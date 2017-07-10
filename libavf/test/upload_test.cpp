
#include <getopt.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_socket.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "vdb_api.h"

#include "libipc.h"
#include "avf_media.h"

#include "test_common.cpp"

static struct option long_options[] = {
	{"picture",	NO_ARG,		0,	'p'},
	{"video",	NO_ARG,		0,	'v'},
	{"ip",		HAS_ARG,	0,	'i'},
	{NULL,		NO_ARG,		0,	0},
};
static const char *short_options = "pvi:";

static int opt_picture = 0;
static int opt_video = 0;

CMutex g_lock;
CSocket *g_socket;
CSocketEvent *g_socket_event; // TODO
char g_ip_addr[32] = "192.168.0.103";

int init_param(int argc, char **argv)
{
	int ch;
	int option_index = 0;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'p':
			opt_picture = 1;
			break;

		case 'v':
			opt_video = 1;
			break;

		case 'i':
			strcpy(g_ip_addr, optarg);
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

static avf_media_t *pmedia;
static void avf_media_cb(int type, void *pdata);

static int upload_Connect(void)
{
	AUTO_LOCK(g_lock);

	if (g_socket != NULL)
		return 0;

	g_socket_event = CSocketEvent::Create("upload_socket_event");
	if (g_socket_event == NULL) {
		return -1;
	}

	g_socket = CSocket::CreateTCPClientSocket("upload_socket");
	if (g_socket == NULL) {
		avf_safe_release(g_socket_event);
		return -1;
	}

	struct sockaddr_in sockaddr;

	::memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(VDB_SERVER_PORT);
	inet_aton(g_ip_addr, &sockaddr.sin_addr);

	printf("connnecting to %s:%d...\n", g_ip_addr, VDB_SERVER_PORT);
	avf_status_t status = g_socket_event->Connect(g_socket, &sockaddr, 30*1000);
	if (status != E_OK) {
		printf("cannot connect to server\n");
	} else {
		printf("connected\n");
	}

	return status;
}

static void upload_Disconnect(void)
{
	AUTO_LOCK(g_lock);
	avf_safe_release(g_socket);
}

static int upload_StartRecord(void)
{
	AUTO_LOCK(g_lock);

	server_cmd_t cmd;
	cmd.cmd_code = SERVER_CMD_StartRecord;
	cmd.u.StartRecord.has_video = opt_video;
	cmd.u.StartRecord.has_picture = opt_picture;
	avf_status_t status = g_socket_event->TCPSend(g_socket, 10*1000,
		(const avf_u8_t*)&cmd, sizeof(cmd));
	return status == E_OK ? 0 : -1;
}

static int upload_StopRecord(void)
{
	AUTO_LOCK(g_lock);

	server_cmd_t cmd;
	cmd.cmd_code = SERVER_CMD_StopRecord;
	avf_status_t status = g_socket_event->TCPSend(g_socket, 10*1000,
		(const avf_u8_t*)&cmd, sizeof(cmd));
	return status == E_OK ? 0 : -1;
}

static int upload_AddData(void *data, int size)
{
	AUTO_LOCK(g_lock);

	server_cmd_t cmd;
	cmd.cmd_code = SERVER_CMD_AddData;
	cmd.u.AddData.data_size = size;
	avf_status_t status = g_socket_event->TCPSend(g_socket, 10*1000,
		(const avf_u8_t*)&cmd, sizeof(cmd));
	if (status != E_OK) {
		return -1;
	}
	status = g_socket_event->TCPSend(g_socket, 10*1000,
		(const avf_u8_t*)data, size);
	return status == E_OK ? 0 : -1;
}

#define UPLOAD_PATH	"/tmp/upload/"
#define UPLOAD_V_STREAM	1	// the smaller one

static void menu_upload_start(void)
{
	// re-create the dir
	avf_remove_dir_recursive(UPLOAD_PATH, false);
	avf_safe_mkdir(UPLOAD_PATH);

	// set path
	avf_media_set_config_string(pmedia, "config.upload.path.video", UPLOAD_V_STREAM, UPLOAD_PATH);
	avf_media_set_config_string(pmedia, "config.upload.path.picture", 0, UPLOAD_PATH);
	avf_media_set_config_string(pmedia, "config.upload.path.raw", 0, UPLOAD_PATH);

	// enable upload
	avf_media_set_config_bool(pmedia, "config.upload.video", UPLOAD_V_STREAM, true);
	avf_media_set_config_bool(pmedia, "config.upload.picture", 0, true);
	avf_media_set_config_bool(pmedia, "config.upload.gps", 0, true);
	avf_media_set_config_bool(pmedia, "config.upload.acc", 0, true);
	avf_media_set_config_bool(pmedia, "config.upload.obd", 0, true);
	avf_media_set_config_bool(pmedia, "config.upload.raw", 0, true);

	upload_StartRecord();
}

static void menu_upload_stop(void)
{
	// disable upload
	avf_media_set_config_bool(pmedia, "config.upload.video", UPLOAD_V_STREAM, false);
	avf_media_set_config_bool(pmedia, "config.upload.picture", 0, false);
	avf_media_set_config_bool(pmedia, "config.upload.gps", 0, false);
	avf_media_set_config_bool(pmedia, "config.upload.acc", 0, false);
	avf_media_set_config_bool(pmedia, "config.upload.obd", 0, false);
	avf_media_set_config_bool(pmedia, "config.upload.raw", 0, false);

	upload_StopRecord();
}

static const menu_t g_upload_menu[] = {
	{"Start upload", menu_upload_start},
	{"Stop upload", menu_upload_stop},
};

int main(int argc, char **argv)
{
	if (init_param(argc, argv) < 0)
		return -1;

	// init ipc (we're client)
	if (ipc_init() < 0)
		return -1;

	// create avf media client
	if ((pmedia = avf_media_create(avf_media_cb, NULL)) == NULL) {
		printf("avf_media_create failed\n");
		return -1;
	}

	// print avf version
	uint32_t version = 0;
	avf_get_version(pmedia, &version);
	printf("avf version: %d\n", version);

	if (upload_Connect() < 0)
		return -1;

	RUN_MENU("Upload", g_upload_menu);

	menu_upload_stop();

	upload_Disconnect();

	avf_media_destroy(pmedia);

	avf_mem_info();
	avf_file_info();
	avf_socket_info();

	return 0;
}

typedef struct file_data_s {
	void *data;
	int size;
} file_data_t;

static bool read_file_data(const char *key, int index, file_data_t *fdata)
{
	char filename[256];
	if (avf_media_get_string(pmedia, key, index, with_size(filename), "") == 0 && filename[0]) {

		printf("%s\n", filename);

		int fd = avf_open_file(filename, O_RDONLY, 0);
		if (fd < 0) {
			printf("cannot open %s\n", filename);
			return false;
		}

		fdata->size = avf_get_filesize(fd);
		fdata->data = avf_malloc_bytes(fdata->size);

		if (::read(fd, fdata->data, fdata->size) != fdata->size) {
			printf("read file failed: %s\n", filename);
			avf_free(fdata->data);
			avf_close_file(fd);
			return false;
		}

		avf_close_file(fd);
		return true;
	}
	return false;
}

// this should be run in another thread, not in the callback!
static void avf_media_cb(int type, void *pdata)
{
	file_data_t fdata;

	switch (type) {
	case AVF_CAMERA_EventUploadVideo:
		if (opt_video && read_file_data(NAME_UPLOAD_VIDEO_PREV, UPLOAD_V_STREAM, &fdata)) {
			upload_AddData(fdata.data, fdata.size);
			avf_free(fdata.data);
		}
		break;

	case AVF_CAMERA_EventUploadPicture:
		if (opt_picture && read_file_data(NAME_UPLOAD_PICTURE_PREV, 0, &fdata)) {
			upload_AddData(fdata.data, fdata.size);
			avf_free(fdata.data);
		}
		break;

	case AVF_CAMERA_EventUploadRaw:
		if (read_file_data(NAME_UPLOAD_RAW_PREV, 0, &fdata)) {
			upload_AddData(fdata.data, fdata.size);
			avf_free(fdata.data);
		}
		break;

	default:
		break;
	}
}

