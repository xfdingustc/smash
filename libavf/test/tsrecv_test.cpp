
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>  
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h> 

#include "test_common.cpp"
#include "avf_util.h"

static struct option long_options[] = {
	{"ip",		HAS_ARG,	0,	'i'},
	{"port",	HAS_ARG,	0,	'p'},
	{"file",	HAS_ARG,	0,	'f'},
	{NULL,		NO_ARG,		0,	0},
};

static const char *short_options = "f:i:p:";
static char g_server_ip[64];
static int g_server_port = 8087;
static char g_filename[256] = "/tmp/recv1.ts";

int init_param(int argc, char **argv)
{
	int ch;
	int option_index = 0;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'i':
			strcpy(g_server_ip, optarg);
			break;

		case 'p':
			g_server_port = atoi(optarg);
			break;

		case 'f':
			strcpy(g_filename, optarg);
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

#define COUNTER_MASK (1 << 20)

int run_recv(void)
{
	int socket_fd;
	int fd;
	struct sockaddr_in remote_addr;
	int last_total = 0;
	int total = 0;

	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_addr.s_addr = inet_addr(g_server_ip);
	remote_addr.sin_port = htons(g_server_port);

	if ((socket_fd = ::socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	if (::connect(socket_fd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0) {
		perror("connect");
		return -1;
	}

	if ((fd = avf_open_file(g_filename, O_CREAT|O_TRUNC|O_RDWR, 0)) < 0) {
		perror(g_filename);
		return -1;
	}

	while (1) {
		char buf[16*1024];
		int len;

		len = ::recv(socket_fd, buf, sizeof(buf), 0);
		if (len < 0) {
			perror("recv");
			break;
		}

		if (::write(fd, buf, len) != len) {
			perror("write");
			break;
		}

		total += len;
		if ((total & ~COUNTER_MASK) != (last_total & ~COUNTER_MASK)) {
			printf("received: %d\n", total);
			last_total = total;
		}
	}

	avf_close_file(fd);
	::close(socket_fd);

	return 0;
}

int main(int argc, char **argv)
{
	if (init_param(argc, argv) < 0)
		return -1;

	if (run_recv() < 0)
		return -1;

	return 0;
}

