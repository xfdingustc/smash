
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include "libipc.h"

int g_ipc_fd = -1;

int ipc_init(void)
{
	if (g_ipc_fd < 0 && (g_ipc_fd = open("/dev/ipc", O_RDWR, 0)) < 0) {
		perror("/dev/ipc");
		return -1;
	}
	return 0;
}

void ipc_deinit(void)
{
	if (g_ipc_fd >= 0) {
		close(g_ipc_fd);
		g_ipc_fd = -1;
	}
}

void *__ipc_notify_thread(void *param)
{
	ipc_client_t *client = (ipc_client_t*)param;
	unsigned long notifications;

	while (1) {
		if (ipc_read_notification(client->cid, &notifications) < 0) {
			ipc_client_no_disconnected_proc on_disconnected = client->on_disconnected;
			if (on_disconnected) {
				(on_disconnected)(client, errno);
			}
			break;
		}

		if (client->notify_proc) {
			unsigned i;
			for (i = 0; i < sizeof(notifications)*8; i++, notifications >>= 1) {
				if (notifications & 1) {
					client->notify_proc(client, i);
				}
			}
		}
	}

	return NULL;
}

// count (int)
//	str-ptr
//	str-ptr
//	str-ptr
// size;string;EOS
// size;string;EOS
// size;string;EOS
void *__ipc_write_strlist(size_t *size, void *ptr, int count, const char **strlist)
{
	void **pstrlist;
	int i;

	__ipc_write_int(*size, ptr, count);

	pstrlist = (void**)ptr;
	__ipc_inc_pos(*size, ptr, count * sizeof(void*));

	for (i = 0; i < count; i++) {
		pstrlist[i] = ptr;
		__ipc_write_str(*size, ptr, strlist[i]);
	}

	return ptr;
}

void *__ipc_read_strlist(int *status, size_t *size, void *ptr, int *count, char ***pstrlist)
{
	int i, c = 0;

	__ipc_read_int(*status, ptr, *size, c);
	if (*status < 0)
		return ptr;

	*count = c;

	if (*size < c * sizeof(void*)) {
		*status = -1;
		return ptr;
	}
	*pstrlist = (char**)ptr;
	__ipc_inc_pos(*size, ptr, c * sizeof(void*));

	for (i = 0; i < c; i++) {
		char *str = (char*)ptr;
		int len = 0;

		__ipc_read_int(*status, ptr, *size, len);
		if (*status < 0)
			return ptr;

		(*pstrlist)[i] = str;
		if (len <= 0 || (int)*size < len) {
			*status = -1;
			return ptr;
		}

		str[len] = 0;
		__ipc_inc_pos(*size, ptr, len);
	}

	return ptr;
}

void ipc_no_buffer(void)
{
	printf("ipc buffer is not big enough\n");
	*(int*)0=0;	// cause crash
}

void ipc_dump_data(void *buffer, int size)
{
	int i, j, c;
	char *ptr = (char*)buffer;

	c = size / 16;
	for (i = 0; i < c; i++, ptr += 16) {
		for (j = 0; j < 16; j++) {
			printf("%02x ", ptr[j]);
		}
		printf("  ");
		for (j = 0; j < 16; j++) {
			if (isprint(ptr[j])) printf("%c", ptr[j]);
			else printf(".");
		}
		printf("\n");
	}
}

// create an object of 'object_size' on 'cid'
// cid will be closed on failure
ipc_client_t *__ipc_new_client_ex(ipc_cid_t cid, size_t object_size, 
	ipc_client_constructor ctor, void *ctor_args, ipc_client_destructor dtor,
	ipc_client_notify_proc notify_proc, ipc_client_no_disconnected_proc on_disconnected)
{
	ipc_client_t *client;

	if ((client = (ipc_client_t*)malloc(object_size)) == NULL) {
		ipc_close_channel(cid);
		return NULL;
	}

	memset(client, 0, object_size);

	client->cid = cid;
	client->tid_notify = 0;
	client->disconnected = 0;
	client->dtor = dtor;
	client->notify_proc = notify_proc;
	client->on_disconnected = on_disconnected;

	// call child class's constructor
	if (ctor && ctor(client, ctor_args) < 0)
		goto Fail;

	// create a thread to read notification
	if (notify_proc) {
		if (pthread_create(&client->tid_notify, NULL, __ipc_notify_thread, client) < 0)
			goto Fail;
	}

	return client;

Fail:
	__ipc_destroy_client(client);
	return NULL;
}

ipc_client_t *__ipc_create_client(size_t object_size, const char *service_name, 
	int cmd, void *cmd_buffer, size_t buffer_size, 
	ipc_client_constructor ctor, void *ctor_args,
	ipc_client_destructor dtor, ipc_client_notify_proc notify_proc)
{
	ipc_cid_t service_cid;
	ipc_write_read_t iwr;
	ipc_client_t *client = NULL;

	if (ipc_query_service(service_name, &service_cid) < 0)
		return NULL;

	ipc_init_cmd_iwr(&iwr, service_cid, cmd, cmd_buffer, buffer_size, NULL, 0);
	if (ipc_send_cmd(&iwr) < 0) {
		goto Done;
	}

	client = __ipc_new_client(iwr.output_cid[0], object_size, ctor, ctor_args, dtor, notify_proc);

Done:
	ipc_close_channel(service_cid);
	return client;
}

void __ipc_destroy_client(ipc_client_t *client)
{
	void *result;

	if (client == NULL)
		return;

	if (client->dtor) {
		// call the derived first
		client->dtor(client);
	}

	if (client->cid != 0) {
		client->on_disconnected = NULL;	// avoid false alarm
		ipc_close_channel(client->cid);
	}

	if (client->tid_notify != 0) {
		// since the channel is closed, the thread will return failure
		// so wait it done here
		pthread_join(client->tid_notify, &result);
	}

	free(client);
}

int __ipc_call_remote(ipc_client_t *client, int cmd, void *cmd_buffer, size_t buffer_size,
	void *read_buffer, size_t read_size)
{
	ipc_write_read_t iwr;
	int rval;

	ipc_init_cmd_iwr(&iwr, client->cid, cmd, cmd_buffer, buffer_size, read_buffer, read_size);
	if ((rval = ipc_send_cmd(&iwr)) < 0) {
		// todo - check if service is disconnected
		return rval;
	}

	return iwr.read_val;
}

int __ipc_call_remote_read(ipc_client_t *client, int cmd, void *cmd_buffer, size_t buffer_size,
	void *read_buffer, size_t read_size, int *bytes_read)
{
	ipc_write_read_t iwr;
	int rval;

	ipc_init_cmd_iwr(&iwr, client->cid, cmd, cmd_buffer, buffer_size, read_buffer, read_size);
	if ((rval = ipc_send_cmd(&iwr)) < 0) {
		// todo - check if service is disconnected
		return rval;
	}

	*bytes_read = iwr.bytes_read;
	return iwr.read_val;
}

static void *__ipc_server_thread_proc(void *arg)
{
	ipc_server_t *server = (ipc_server_t*)arg;
	ipc_write_read_t iwr;

	//printf("__ipc_server_thread_proc enter\n");

	iwr.input_cid[0] = server->cid;
	iwr.output_cid[0] = 0;
	iwr.rid[0] = 0;

	iwr.write_val = 0;
	iwr.write_ptr = server->write_buffer;
	iwr.write_size = 0;

	iwr.read_val = 0;
	iwr.read_ptr = server->read_buffer;
	iwr.read_size = server->read_buffer_size;

	iwr.flags = IPC_FLAG_READ;
	while (1) {
		int rval;

		if (ipc_write_read(&iwr) < 0) {
			break;
		}

		if ((rval = server->server_proc(server, &iwr) < 0)) {
			if (rval == -1) {
				iwr.write_val = -1;
				iwr.write_size = 0;
			} else {
				// other values - destroy
				break;
			}
		}

		iwr.flags = IPC_FLAG_WRITE | IPC_FLAG_READ;
	}

	__ipc_destroy_server(server);

	//printf("__ipc_server_thread_proc exit\n");

	return NULL;
}

ipc_server_t *__ipc_create_server(size_t object_size, size_t max_read_size, size_t max_write_size,
	ipc_server_constructor ctor, void *ctor_args, ipc_server_destructor dtor, ipc_server_proc server_proc)
{
	ipc_server_t *server;

	if ((server = (ipc_server_t*)malloc(object_size)) == NULL) {
		return NULL;
	}

	memset(server, 0, object_size);

	server->cid = 0;
	server->dtor = dtor;
	server->server_proc = server_proc;

	server->error = 0;
	server->write_buffer = NULL;
	server->read_buffer = NULL;
	server->write_buffer_size = max_write_size; // 1024;
	server->read_buffer_size = max_read_size; // 4096;

	if (ctor && ctor(server, ctor_args) < 0)
		goto Fail;

	if ((server->write_buffer = (char*)malloc(server->write_buffer_size)) == NULL)
		goto Fail;

	if ((server->read_buffer = (char*)malloc(server->read_buffer_size)) == NULL)
		goto Fail;

	return server;

Fail:
	__ipc_destroy_server(server);
	return NULL;
}

int __ipc_destroy_server(ipc_server_t *server)
{
	if (server->cid != 0)
		ipc_close_channel(server->cid);

	if (server->dtor)
		server->dtor(server);

	if (server->write_buffer)
		free(server->write_buffer);

	if (server->read_buffer)
		free(server->read_buffer);

	return 0;
}

int __ipc_run_server(ipc_server_t *server, ipc_write_read_t *iwr)
{
	pthread_attr_t attr;

	if (ipc_setup_channel(iwr) < 0)
		return -1;

	server->cid = iwr->output_cid[0];

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (pthread_create(&server->tid_server, &attr, __ipc_server_thread_proc, server))
		return -1;

	return 0;
}

