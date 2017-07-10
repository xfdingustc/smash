
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include "../ipc_drv/ipc_drv.h"

int fd;

int run_service(const char *name);
void *new_channel_100(void *arg);

int main(int argc, char **argv)
{
	if ((fd = open("/dev/ipc", O_RDWR, 0)) < 0) {
		perror("/dev/ipc");
		return -1;
	}

	run_service("TestService");

	return 0;
}

ipc_cid_t register_service(const char *name)
{
	ipc_service_t service;

	strcpy(service.name, name);
	if (ioctl(fd, IPC_REGISTER_SERVICE, &service) < 0) {
		perror("server: IPC_REGISTER_SERVICE");
		return 0;
	}

	return service.service_cid;
}

int run_service(const char *name)
{
	ipc_cid_t cid;
	ipc_write_read_t iwr;
	char read_buffer[2048];
	char write_buffer[2048];

	if ((cid = register_service(name)) == 0) {
		return 0;
	}

	iwr.input_cid[0] = cid;
	iwr.output_cid[0] = 0;
	iwr.rid[0] = 0;

	iwr.flags = IPC_FLAG_READ;
	iwr.write_val = 0;
	iwr.write_ptr = write_buffer;
	iwr.write_size = 0;

	iwr.read_val = 0;
	iwr.read_ptr = read_buffer;
	iwr.read_size = sizeof(read_buffer);

	while (1) {

		if (ioctl(fd, IPC_WRITE_READ, &iwr) < 0) {
			perror("server: IPC_WRITE_READ");
			iwr.flags = IPC_FLAG_READ;
			continue;
		}

		switch (iwr.read_val) {
		case 1:
			printf("server: cmd 1\n");
			iwr.write_val = 10;
			iwr.write_size = 0;
			iwr.flags = IPC_FLAG_WRITE | IPC_FLAG_READ;
			break;

		case 2:
			printf("server: cmd 2\n");
			iwr.write_val = 20;
			iwr.write_size = 0;
			iwr.flags = IPC_FLAG_WRITE | IPC_FLAG_READ;
			break;

		case 100:
			printf("server: start new channel\n");
			iwr.write_val = 0;
			iwr.write_size = 0;
			if (ioctl(fd, IPC_SETUP_CHANNEL, &iwr) < 0) {
				perror("server: IPC_SETUP_CHANNEL");
			} else {
				pthread_t thread;
				pthread_attr_t thread_attr;
				pthread_attr_init(&thread_attr);
				pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
				pthread_create(&thread, &thread_attr, new_channel_100, (void*)iwr.output_cid[0]);
			}
			iwr.flags = IPC_FLAG_READ;
			break;

		default:
			printf("server: cmd %d - unknown cmd\n", iwr.read_val);
			iwr.write_val = -1;
			iwr.write_size = 0;
			iwr.flags = IPC_FLAG_WRITE | IPC_FLAG_READ;
			break;
		}
	}

	return 0;
}

void *new_channel_100(void *arg)
{
	ipc_cid_t cid = (ipc_cid_t)arg;

	// todo
	printf("server: new channel id = %d\n", cid);
	ioctl(fd, IPC_CLOSE_CHANNEL, cid);

	return NULL;
}

