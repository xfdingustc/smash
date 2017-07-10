
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "libipc.h"
#define IPC_CLIENT
#include "ipc_vlc.h"


ipc_cid_t query_service(int fd, const char *name)
{
	ipc_service_t service;

	strcpy(service.name, name);
	if (ioctl(fd, IPC_QUERY_SERVICE, &service) < 0) {
		perror("client: IPC_REGISTER_SERVICE");
		return 0;
	}

	return service.service_cid;
}

void *new_channel_100(void *arg);

int main(int argc, char **argv)
{
	int fd, i;
	ipc_cid_t cid;
	ipc_write_read_t iwr;
	char write_buffer[2048];
	char read_buffer[2048];

	if ((fd = open("/dev/ipc", O_RDWR, 0)) < 0) {
		perror("client: /dev/ipc");
		return -1;
	}

	// query service
	cid = query_service(fd, "BadServiceName");
	printf("client: cid = %d\n", cid);
	sleep(1);

	// query service
	cid = query_service(fd, "TestService");
	printf("client: cid = %d\n", cid);
	sleep(1);

	iwr.input_cid[0] = cid;
	iwr.output_cid[0] = 0;
	iwr.rid[0] = 0;

	iwr.write_ptr = write_buffer;
	iwr.write_size = 0;

	iwr.read_ptr = read_buffer;
	iwr.read_size = sizeof(read_buffer);
	iwr.read_val = 0;

	// send cmd 1
	iwr.write_val = 1;
	iwr.write_size = 0;

	if (ioctl(fd, IPC_SEND_CMD, &iwr) < 0) {
		perror("client: IPC_SEND_CMD");
		//return -1;
	}

	printf("client: cmd 1 returns %d\n", iwr.read_val);
	sleep(1);

	// send cmd 2
	iwr.write_val = 2;
	iwr.write_size = 0;

	if (ioctl(fd, IPC_SEND_CMD, &iwr) < 0) {
		perror("client: IPC_SEND_CMD");
		//return -1;
	}

	printf("client: cmd 2 returns %d\n", iwr.read_val);
	sleep(1);

	// send cmd 3
	iwr.write_val = 3;
	iwr.write_size = 0;

	if (ioctl(fd, IPC_SEND_CMD, &iwr) < 0) {
		perror("client: IPC_SEND_CMD");
		//return -1;
	}

	printf("client: cmd 3 returns %d\n", iwr.read_val);
	sleep(1);

	// test new channel
	iwr.write_val = 100;
	iwr.write_size = 0;

	if (ioctl(fd, IPC_SEND_CMD, &iwr) < 0) {
		perror("client: IPC_SEND_CMD");
		//return -1;
	}

	printf("client: new channel id = %d\n", iwr.output_cid[0]);
	{
			pthread_t thread;
			//pthread_attr_t thread_attr;
			//pthread_attr_init(&thread_attr);
			//pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
			//pthread_create(&thread, &thread_attr, new_channel_100, (void*)iwr.output_cid[0]);
			pthread_create(&thread, NULL, new_channel_100, (void*)iwr.output_cid[0]);
			pthread_join(thread, NULL);
	}

	return 0;
}

void *new_channel_100(void *arg)
{
	// todo
	return NULL;
}

