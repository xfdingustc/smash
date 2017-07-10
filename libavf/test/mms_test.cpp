
#define LOG_TAG	"mms"

#include <getopt.h>
#include <fcntl.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_socket.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "filemap.h"

#include "libipc.h"
#include "avf_media.h"

#include "mms_if.h"

#include "test_common.cpp"

//-----------------------------------------------------------------------

static avf_media_t *g_avf_media = NULL;

#ifdef RTSP
static IMMSource *g_rtsp_source = NULL;
static IMMRtspServer *g_rtsp_server = NULL;
#endif

#ifdef RTMP
static IMMSource *g_rtmp_source = NULL;
static IMMRtmpStream *g_rtmp_stream = NULL;
#endif

static CMutex g_mms_mutex;
static IMMSource *g_mms_source[2];

void SetSource(int index, IMMSource *pSource)
{
	AUTO_LOCK(g_mms_mutex);
	g_mms_source[index] = pSource;
}

IMMSource* GetSource(int index)
{
	AUTO_LOCK(g_mms_mutex);
	IMMSource *pSource = g_mms_source[index];
	if (pSource) pSource->AddRef();
	return pSource;
}

static int g_mms_stream = 1;
static char g_server_url[256] = "rtmp://90.0.0.31/live";

#define MMS_PATH_MODE_0		"/tmp/"
#define MMS_PATH_MODE_1		"/tmp/mms-1/"
#define MMS_PATH_MODE_2		"/tmp/mms-2/"

static void avf_media_on_event_cb(int type, void *pdata);
static void avf_media_on_disconnected_cb(int error, void *pdata);

static void recreate_dir(const char *dir)
{
	avf_remove_dir_recursive(dir, false);
	avf_safe_mkdir(dir);
}

/*
static void menu_mms_start()
{
	// re-create the dir
	recreate_dir(MMS_PATH_MODE_0);

	// set path
	avf_media_set_config_string(g_avf_media, "config.upload.path.video", g_mms_stream, MMS_PATH_MODE_0);
	avf_media_set_config_string(g_avf_media, "config.upload.path.picture", 0, MMS_PATH_MODE_0);
	avf_media_set_config_string(g_avf_media, "config.upload.path.raw", 0, MMS_PATH_MODE_0);

	// enable upload
	avf_media_set_config_bool(g_avf_media, "config.upload.video", g_mms_stream, true);
	avf_media_set_config_bool(g_avf_media, "config.upload.picture", 0, true);
	avf_media_set_config_bool(g_avf_media, "config.upload.gps", 0, true);
	avf_media_set_config_bool(g_avf_media, "config.upload.acc", 0, true);
	avf_media_set_config_bool(g_avf_media, "config.upload.obd", 0, true);
	avf_media_set_config_bool(g_avf_media, "config.upload.raw", 0, true);
}

static void menu_mms_stop()
{
	// disable upload
	avf_media_set_config_bool(g_avf_media, "config.upload.video", g_mms_stream, false);
	avf_media_set_config_bool(g_avf_media, "config.upload.picture", 0, false);
	avf_media_set_config_bool(g_avf_media, "config.upload.gps", 0, false);
	avf_media_set_config_bool(g_avf_media, "config.upload.acc", 0, false);
	avf_media_set_config_bool(g_avf_media, "config.upload.obd", 0, false);
	avf_media_set_config_bool(g_avf_media, "config.upload.raw", 0, false);
}
*/

static void menu_mms_start_mode1()
{
	recreate_dir(MMS_PATH_MODE_1);

	avf_media_set_config_string(g_avf_media, "config.mms.path.mode1", g_mms_stream, MMS_PATH_MODE_1);
	avf_media_set_config_int32(g_avf_media, "config.mms.opt.mode1", g_mms_stream, UPLOAD_OPT_ALL);
}

static void menu_mms_stop_mode1()
{
	avf_media_set_config_int32(g_avf_media, "config.mms.opt.mode1", g_mms_stream, 0);
}

static void menu_mms_start_mode2()
{
	recreate_dir(MMS_PATH_MODE_2);

	avf_media_set_config_string(g_avf_media, "config.mms.path.mode2", g_mms_stream, MMS_PATH_MODE_2);
	avf_media_set_config_int32(g_avf_media, "config.mms.opt.mode2", g_mms_stream, UPLOAD_OPT_ALL);

#ifdef RTMP
	g_rtmp_stream->Start();
#endif
}

static void menu_mms_stop_mode2()
{
	avf_media_set_config_int32(g_avf_media, "config.mms.opt.mode2", g_mms_stream, 0);
}

static const menu_t g_mms_menu[] =
{
//	{"Start MMS (mode 0)", menu_mms_start},
//	{"Stop MMS (mode 0)", menu_mms_stop},
	{"Start RTSP", menu_mms_start_mode1},
	{"Stop RTSP", menu_mms_stop_mode1},
	{"Start RTMP", menu_mms_start_mode2},
	{"Stop RTMP", menu_mms_stop_mode2},
};

static struct option long_options[] = {
	{"stream", HAS_ARG, 0, 's'},
	{"rtmp", HAS_ARG, 0, 'r'},
	{NULL,	NO_ARG, 0,	0}
};
static const char *short_options = "r:s:";

static int init_param(int argc, char **argv)
{
	int ch;
	int option_index = 0;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'r':
			strcpy(g_server_url, optarg);
			break;

		case 's':
			g_mms_stream = ::atoi(optarg);
			if (g_mms_stream < 0 || g_mms_stream > 1) {
				printf("stream can only be 0 or 1\n");
				return -1;
			}
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

static int init_rtsp()
{
#ifdef RTSP
	g_rtsp_source = mms_create_source(MMS_PATH_MODE_1, IMMSource::MODE_TS);
	if (g_rtsp_source == NULL)
		return -1;

	SetSource(0, g_rtsp_source);

	if ((g_rtsp_server = mms_create_rtsp_server(g_rtsp_source)) == NULL)
		return -1;

	if (g_rtsp_server->Start() != E_OK)
		return -1;
#else
	AVF_LOGI("no RTSP");
#endif

	return 0;
}

static void deinit_rtsp()
{
#ifdef RTSP
	if (g_rtsp_source) {
		SetSource(0, NULL);
		g_rtsp_source->Close();
	}
	if (g_rtsp_server) {
		g_rtsp_server->Stop();
		g_rtsp_server->Release();
	}
	if (g_rtsp_source) {
		g_rtsp_source->Release();
	}
#endif
}

static int init_rtmp()
{
#ifdef RTMP
	g_rtmp_source = mms_create_source(MMS_PATH_MODE_2, IMMSource::MODE_FRAME);
	if (g_rtmp_source == NULL)
		return -1;

	SetSource(1, g_rtmp_source);

	if ((g_rtmp_stream = mms_create_rtmp_stream(g_rtmp_source, g_server_url)) == NULL)
		return -1;

#else
	AVF_LOGI("no RTMP");
#endif

	return 0;
}

#ifdef RTMP
extern "C" void CONF_modules_free(void);
extern "C" void ENGINE_cleanup(void);
extern "C" void CONF_modules_unload(int all);
extern "C" void ERR_free_strings(void);
extern "C" void EVP_cleanup(void);
extern "C" void CRYPTO_cleanup_all_ex_data(void);
extern "C" void ERR_remove_state(unsigned long pid);
#endif

static void deinit_rtmp()
{
#ifdef RTMP
	if (g_rtmp_source) {
		SetSource(1, NULL);
		g_rtmp_source->Close();
	}
	if (g_rtmp_stream) {
		g_rtmp_stream->Release();
	}
	if (g_rtmp_source) {
		g_rtmp_source->Release();
	}

	CONF_modules_free();
	ERR_remove_state(0);
	ENGINE_cleanup();
	CONF_modules_unload(1);
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();

#endif
}

pthread_t main_thread_id;

int main(int argc, char **argv)
{
	main_thread_id = pthread_self();

	if (init_param(argc, argv) < 0)
		return -1;

#ifdef RTMP
	AVF_LOGI("rtmp server addr: " C_CYAN "%s" C_NONE, g_server_url);
#endif

	// init ipc (we're client)
	if (ipc_init() < 0)
		return -1;

	// create avf media client
	if ((g_avf_media = avf_media_create_ex(avf_media_on_event_cb, avf_media_on_disconnected_cb, NULL)) == NULL) {
		printf("avf_media_create failed\n");
		return -1;
	}

	// init rtsp
	if (init_rtsp() < 0) {
		printf("init_rtsp failed\n");
		return -1;
	}

	// init rtmp
	if (init_rtmp() < 0) {
		printf("init_rtmp failed\n");
		return -1;
	}

	// -------------------------------
	RUN_MENU("MMS test", g_mms_menu);
	// -------------------------------

	deinit_rtsp();
	deinit_rtmp();

	avf_media_destroy(g_avf_media);

	ipc_deinit();

	// for debug
//	avf_mem_info(1);
	avf_file_info();
	avf_socket_info();

	return 0;
}

static void avf_media_on_event_cb(int type, void *pdata)
{
	switch (type) {
	case AVF_CAMERA_EventMmsMode1: {
			IMMSource *source = GetSource(0);
			if (source) {
				uint32_t buf[3];
				int rval = avf_media_get_mem(g_avf_media, "state.mms.mode1", g_mms_stream, (uint8_t*)buf, sizeof(buf));
				if (rval == 0) {
					// fileindex, filesize, framecount
					source->UpdateWrite(buf[0], buf[1], buf[2]);
				}
				source->Release();
			}
		}
		break;

	case AVF_CAMERA_EventMmsMode2: {
			IMMSource *source = GetSource(1);
			if (source) {
				uint32_t buf[3];
				int rval = avf_media_get_mem(g_avf_media, "state.mms.mode2", g_mms_stream, (uint8_t*)buf, sizeof(buf));
				if (rval == 0) {
					// fileindex, filesize, framecount
					source->UpdateWrite(buf[0], buf[1], buf[2]);
				}
				source->Release();
			}
		}
		break;

	default:
		break;
	}
}

static void avf_media_on_disconnected_cb(int error, void *pdata)
{
	pthread_kill(main_thread_id, SIGQUIT);
}

