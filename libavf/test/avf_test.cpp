
#include <getopt.h>

#include "avf_common.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_plat_config.h"
#include "avf_camera_if.h"
#include "avf_playback_if.h"

#include "mem_stream.h"
#include "avf_util.h"

#include "test_common.cpp"

static int g_repeat = 0;
static IMediaControl *g_MC = NULL;
static IVdbInfo *g_vdb_info = NULL;
static char g_camera_conf[] = "/usr/local/bin/camera.conf";
static char g_playback_conf[] = "/usr/local/bin/playback.conf";

static struct option long_options[] = {
	{"autorun",	NO_ARG,		0,	'a'},
	{"conf",	HAS_ARG,	0,	'c'},
	{"novout",	NO_ARG,		0,	'n'},
	{"vdb",		NO_ARG,		0,	'v'},
	{NULL,		NO_ARG,		0,	0},
};

static const char *short_options = "ac:nv";

static int g_auto_run = 0;
static int g_novout = 0;
static int g_vdb = 0;
static char g_conf[256];

int init_param(int argc, char **argv)
{
	int ch;
	int option_index = 0;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'a':
			g_auto_run = 1;
			break;

		case 'c':
			strcpy(g_conf, optarg);
			break;

		case 'n':
			g_novout = 1;
			break;

		case 'v':
			g_vdb = 1;
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

//-----------------------------------------------------------------------
//
//  common
//
//-----------------------------------------------------------------------

int config_vout(int is_playback)
{
	vout_config_t config;

	if (g_novout)
		return 0;

	// enter idle
	if (avf_platform_display_set_mode(MODE_IDLE) < 0)
		return -1;

#ifdef ARCH_S2L

	memset(&config, 0, sizeof(config));
	config.vout_id = VOUT_1;
	config.vout_type = VOUT_TYPE_HDMI;
	config.vout_mode = VOUT_MODE_1080p;
	config.enable_video = 1;
	config.fb_id = -1;
	if (avf_platform_display_set_vout(&config, NULL) < 0)
		return -1;

#else

	avf_platform_display_set_backlight(VOUT_0, "t15p00", 0);

	// init LCD
	memset(&config, 0, sizeof(config));
	config.vout_id = VOUT_0;
	config.vout_type = VOUT_TYPE_DIGITAL;
	config.vout_mode = VOUT_MODE_D320x240;
	config.enable_video = 1;
	config.fb_id = -1;

	if (avf_platform_display_set_vout(&config, "t15p00") < 0) {
		//return -1;
	}

	avf_platform_display_set_backlight(VOUT_0, "t15p00", 1);

	if (is_playback) {
		// init HDMI
		memset(&config, 0, sizeof(config));
		config.vout_id = VOUT_1;
		config.vout_type = VOUT_TYPE_HDMI;
		config.vout_mode = VOUT_MODE_720p;
		config.enable_video = 1;
		config.fb_id = -1;
		if (avf_platform_display_set_vout(&config, NULL) < 0)
			return -1;
	} else {
		// disable HDMI
		avf_platform_display_disable_vout(VOUT_1);
	}

#endif

	return 0;
}

static void set_media_source(IMediaControl *pControl, 
	const char *pPlayer, const char *pMediaSrc, const char *filename)
{
	char *buffer;

	if (avf_load_text_file(filename, &buffer) < 0) {
		printf("file not found: %s\n", filename);
		exit(-1);
	}

	pControl->SetMediaSource(pPlayer, pMediaSrc, buffer, 0, 0);
	avf_free_text_buffer(buffer);
}

//-----------------------------------------------------------------------
//
//  camara test
//
//-----------------------------------------------------------------------

static void menu_set_config(void);
static void menu_set_state(void);
static void menu_image_control(void);
static void menu_toggle_gps_info(void);
static void menu_test_stream(void);
static void menu_save_current(void);
static void menu_dump_state(void);

static const menu_t g_camera_main_menu[] = {
	{"Set State", menu_set_state},
	{"Save Current", menu_save_current},
	{"Set Config", menu_set_config},
	{"Test Stream", menu_test_stream},
	{"Image Control", menu_image_control},
	{"Toggle GPS Info", menu_toggle_gps_info},
	{"Dump State", menu_dump_state},
};

static void menu_set_config(void)
{
	char key[256];
	char value[256];
	char buffer[256];
	int index;

	// key
	printf("Input key:\n");
	fgets(key, sizeof(key), stdin);
	remove_newline(key);

	if (key[0] == 0)
		return;

	// index
	get_input("Input index: ", buffer, sizeof(buffer));
	index = (buffer[0] == 0) ? 0 : atoi(buffer);

	// value
	get_input("Input value: ", value, sizeof(value));
	if (value[0] == 0)
		return;

	printf("%s[%d] = %s\n", key, index, value);
	g_MC->WriteRegString(key, index, value);
}

static const struct {
	const char *name;
	int state;
} state_desc[] = {
	{"Idle", IMediaControl::STATE_IDLE},
	{"Open", IMediaControl::STATE_OPEN},
	{"Prepared", IMediaControl::STATE_PREPARED},
	{"Running", IMediaControl::STATE_RUNNING},
};
#define STATE_NUM	(sizeof(state_desc) / sizeof(state_desc[0]))

static void menu_set_state(void)
{
	char buffer[32];
	int c;
	int i;

	for (i = 0; i < (int)STATE_NUM; i++)
		printf("  [" C_CYAN "%d" C_NONE "] %s\n", i + 1, state_desc[i].name);

	get_input(C_GREEN "  Enter target state: " C_NONE, buffer, sizeof(buffer));

	c = buffer[0];
	if (c >= '1' && c < '1' + (int)STATE_NUM && buffer[1] == 0) {
		printf("Set state to '%s'\n", state_desc[c - '1'].name);
		g_MC->SetMediaState(state_desc[c - '1'].state);
	}
}

#define OVERLAY_1080p		"1:0,0,0,56:45"
#define OVERLAY_720p		"1:0,0,0,37:30"
#define OVERLAY_1024x576	"1:0,0,0,30:24"
#define OVERLAY_480p		"1:0,0,0,24:20"
#define OVERLAY_512x288		"1:0,0,0,15:12"
#define OVERLAY_640x360		"1:0,0,0,19:15"
#define OVERLAY_NONE		""

static void menu_test_stream(void)
{
	static const struct stream_config_s {
		const char *name;
		const char *video_0_config;
		const char *video_1_config;
		const char *video_2_config;
		const char *overlay_0_config;
		const char *overlay_1_config;
		const char *overlay_2_config;
		int record_config_0;	
		int record_config_1;
	} stream_configs[] = {

#ifdef ARCH_A5S

		{
			"Mjpeg (1024x576) only",	// name
			"none",				// "config.video" 0
			"mjpeg:1024x576x70",	// "config.video" 1
			"",
			OVERLAY_NONE,		// "config.video.overlay" 0
			OVERLAY_1024x576,	// "config.video.overlay" 1
			OVERLAY_NONE,
			0,					// record_config_0
			0,					// record_config_1
		},
		{
			"1080p H.264 + 512x288 Mjpeg",	// name
			"h264:1920x1080:6220800:12441600:10368000", // "config.video" 0
			"mjpeg:512x288x70",	// "config.video" 1
			"",
			OVERLAY_1080p,		// "config.video.overlay" 0
			OVERLAY_512x288,		// "config.video.overlay" 1
			OVERLAY_NONE,
			1,					// record_config_0
			0,					// record_config_1
		},
		{
			"720p H.264 + 512x288 Mjpeg",	// name
			"h264:1280x720:3000000:8000000:6000000", // "config.video" 0
			"mjpeg:512x288x60",	// "config.video" 1
			"",
			OVERLAY_720p,		// "config.video.overlay" 0
			OVERLAY_512x288,		// "config.video.overlay" 1
			OVERLAY_NONE,
			1,					// record_config_0
			0,					// record_config_1
		},
		{
			"480p H.264 + 512x288 Mjpg",	// name
			"h264:720x480:2000000:6000000:4000000", // "config.video" 0
			"mjpeg:512x288x60",	// "config.video" 1
			"",
			OVERLAY_480p,		// "config.video.overlay" 0
			OVERLAY_512x288,		// "config.video.overlay" 1
			OVERLAY_NONE,
			1,					// record_config_0
			0,					// record_config_1
		},
		{
			"1080p H.264 + 512x288 H.264",	// name
			"h264:1920x1080:4000000:12000000:8000000", // "config.video" 0
			"h264:512x288:1000000:5000000:3000000", // "config.video" 1
			"",
			OVERLAY_1080p,		// "config.video.overlay" 0
			OVERLAY_512x288,		// "config.video.overlay" 1
			OVERLAY_NONE,
			1,					// record_config_0
			0,					// record_config_1
		},
		{
			"1080p H.264 + 512x288 MJPEG + 512x288 H.264",	// name
			"h264:1920x1080:4000000:12000000:8000000", // "config.video" 0
			"mjpeg:512x288x60", // "config.video" 1
			"h264:512x288:200000:100000:500000", // "config.video" 2
			OVERLAY_1080p,		// "config.video.overlay" 0
			OVERLAY_512x288,		// "config.video.overlay" 1
			OVERLAY_512x288,
			1,					// record_config_0
			1,					// record_config_1
		},

#else	// hell fire

		// --------------------------------------------------
		// video only
		// --------------------------------------------------
		{
			.name = "1080p H.264 40 Mbps",
			.video_0_config = "h264:1920x1080:40000000:40000000:40000000",
			.video_1_config = "none",
			.video_2_config = "none",
			.overlay_0_config = OVERLAY_1080p,
			.overlay_1_config = OVERLAY_NONE,
			.overlay_2_config = OVERLAY_NONE,
			.record_config_0 = 1,
			.record_config_1 = 0,
		},
		{
			.name = "720p H.264 10 Mbps",
			.video_0_config = "h264:1280x720:10000000:10000000:10000000",
			.video_1_config = "none",
			.video_2_config = "none",
			.overlay_0_config = OVERLAY_720p,
			.overlay_1_config = OVERLAY_NONE,
			.overlay_2_config = OVERLAY_NONE,
			.record_config_0 = 1,
			.record_config_1 = 0,
		},
		{
			.name = "480p H.264",
			.video_0_config = "h264:720x480:4000000:4000000:4000000",
			.video_1_config = "none",
			.video_2_config = "none",
			.overlay_0_config = OVERLAY_480p,
			.overlay_1_config = OVERLAY_NONE,
			.overlay_2_config = OVERLAY_NONE,
			.record_config_0 = 1,
			.record_config_1 = 0,
		},

		// --------------------------------------------------
		// mjpeg only
		// --------------------------------------------------
		{
			.name = "1024x576 Mjpeg 85%",
			.video_0_config = "none",
			.video_1_config = "mjpeg:1024x576x85",
			.video_2_config = "none",
			.overlay_0_config = OVERLAY_NONE,
			.overlay_1_config = OVERLAY_1024x576,
			.overlay_2_config = OVERLAY_NONE,
			.record_config_0 = 0,
			.record_config_1 = 0,
		},
		{
			.name = "512x288 Mjpeg 85%",
			.video_0_config = "none",
			.video_1_config = "mjpeg:512x288x85",
			.video_2_config = "none",
			.overlay_0_config = OVERLAY_NONE,
			.overlay_1_config = OVERLAY_512x288,
			.overlay_2_config = OVERLAY_NONE,
			.record_config_0 = 0,
			.record_config_1 = 0,
		},

		// --------------------------------------------------
		// H264 + Mjpeg
		// --------------------------------------------------
		{
			.name = "1080p H.264 + 1024x576 Mjpeg",
			.video_0_config = "h264:1920x1080:30000000:30000000:30000000",
			.video_1_config = "mjpeg:1024x576x85",
			.video_2_config = "none",
			.overlay_0_config = OVERLAY_1080p,
			.overlay_1_config = OVERLAY_1024x576,
			.overlay_2_config = OVERLAY_NONE,
			.record_config_0 = 1,
			.record_config_1 = 0,
		},
		{
			.name = "720p H.264 + 1024x576 Mjpeg",
			.video_0_config = "h264:1280x720:10000000:10000000:10000000",
			.video_1_config = "mjpeg:1024x576x85",
			.video_2_config = "none",
			.overlay_0_config = OVERLAY_720p,
			.overlay_1_config = OVERLAY_1024x576,
			.overlay_2_config = OVERLAY_NONE,
			.record_config_0 = 1,
			.record_config_1 = 0,
		},
		{
			.name = "480p H.264 + 1024x576 Mjpeg",
			.video_0_config = "h264:720x480:4000000:4000000:4000000",
			.video_1_config = "mjpeg:1024x576x85",
			.video_2_config = "none",
			.overlay_0_config = OVERLAY_480p,
			.overlay_1_config = OVERLAY_1024x576,
			.overlay_2_config = OVERLAY_NONE,
			.record_config_0 = 1,
			.record_config_1 = 0,
		},
		{
			.name = "480p H.264 + 512x288 Mjpeg",
			.video_0_config = "h264:720x480:4000000:4000000:4000000",
			.video_1_config = "mjpeg:512x288x85",
			.video_2_config = "none",
			.overlay_0_config = OVERLAY_480p,
			.overlay_1_config = OVERLAY_512x288,
			.overlay_2_config = OVERLAY_NONE,
			.record_config_0 = 1,
			.record_config_1 = 0,
		},

		// --------------------------------------------------
		// H264 + Mjpeg + H264
		// --------------------------------------------------
		{
			.name = "1080p H.264 + 1024x576 Mjpeg + 640x360 H.264",
			.video_0_config = "h264:1920x1080:30000000:30000000:30000000",
			.video_1_config = "mjpeg:1024x576x85",
			.video_2_config = "h264:640x360:2000000:2000000:2000000",
			.overlay_0_config = OVERLAY_1080p,
			.overlay_1_config = OVERLAY_1024x576,
			.overlay_2_config = OVERLAY_640x360,
			.record_config_0 = 1,
			.record_config_1 = 1,
		},

#endif
	};

	char buffer[64];
	int index;

	for (unsigned i = 0; i < sizeof(stream_configs)/sizeof(stream_configs[0]); i++) {
		printf("  [" C_CYAN "%d" C_NONE "] %s\n", i + 1, stream_configs[i].name);
	}

	get_input(C_GREEN "  Select: " C_NONE, buffer, sizeof(buffer));

	if (buffer[0] == 0)
		return;

	index = atoi(buffer) - 1;
	if (index < 0 || index >= (int)(sizeof(stream_configs)/sizeof(stream_configs[0]))) {
		printf("bad input %d\n", index);
		return;
	}

	g_MC->StopMedia(true);

	g_MC->WriteRegString(NAME_VIDEO, 0, stream_configs[index].video_0_config);
	g_MC->WriteRegString(NAME_VIDEO, 1, stream_configs[index].video_1_config);
	g_MC->WriteRegString(NAME_VIDEO, 2, stream_configs[index].video_2_config);

	g_MC->WriteRegString(NAME_VIDEO_OVERLAY, 0, stream_configs[index].overlay_0_config);
	g_MC->WriteRegString(NAME_VIDEO_OVERLAY, 1, stream_configs[index].overlay_1_config);
	g_MC->WriteRegString(NAME_VIDEO_OVERLAY, 2, stream_configs[index].overlay_2_config);

	g_MC->WriteRegInt32(NAME_RECORD, 0, stream_configs[index].record_config_0);
	g_MC->WriteRegInt32(NAME_RECORD, 1, stream_configs[index].record_config_1);

	g_MC->RunMedia();
}

static void menu_save_current(void)
{
	Camera(g_MC).SaveCurrent(10, 10, "/tmp/mmcblk0p1/test");
}

static void menu_image_control(void)
{
	char key[64];
	char value[64];

	get_input("input key: ", key, sizeof(key));
	if (key[0] == 0)
		return;

	get_input("input value: ", value, sizeof(value));
	if (value[0] == 0)
		return;

	if (Camera(g_MC).SetImageControl(key, value) != E_OK) {
		printf("Set %s to %s failed\n", key, value);
	}
}

static void menu_toggle_gps_info(void)
{
	static int show_gps = 0;
	show_gps = 1 - show_gps;
	g_MC->WriteRegBool(NAME_SUBTITLE_SHOW_GPS, 0, show_gps);
}

static void menu_dump_state(void)
{
	g_MC->DumpState();
}

static int camera_max_files = 0;
static int camera_file_index;
static char **camera_files;

static void handle_new_file(void)
{
	char filename[REG_STR_MAX];
	char *file_to_del;

	g_MC->ReadRegString(NAME_FILE_CURRENT, 0, filename, VALUE_FILE_CURRENT);
	if (filename[0] == 0)
		return;

	if (camera_files == NULL) {
		int size = camera_max_files * sizeof(char*);
		camera_files = (char**)malloc(size);
		memset(camera_files, 0, size);
		camera_file_index = 0;
	}

	if (camera_files[camera_file_index] == NULL) {
		camera_files[camera_file_index] = (char*)malloc(REG_STR_MAX);
	}

	strcpy(camera_files[camera_file_index], filename);

	camera_file_index++;
	camera_file_index %= camera_max_files;

	file_to_del = camera_files[camera_file_index];
	if (file_to_del && file_to_del[0] != 0) {
		char jpeg_name[REG_STR_MAX];
		int len;

		strcpy(jpeg_name, file_to_del);

		printf("remove %s\n", file_to_del);
		remove(file_to_del);

		len = strlen(jpeg_name);
		if (len > 3) {
			jpeg_name[len-3] = 'j';
			jpeg_name[len-2] = 'p';
			jpeg_name[len-1] = 'g';
			printf("remove %s\n", jpeg_name);
			remove(jpeg_name);
		}

		file_to_del[0] = 0;
	}
}

static void free_filnames(void)
{
	int i;

	if (camera_files == NULL)
		return;

	for (i = 0; i < camera_max_files; i++)
		free(camera_files[i]);

	free(camera_files);
}

static void camera_callback(void *context, avf_intptr_t tag, const avf_msg_t& msg)
{
	switch (msg.code) {
	case IMediaControl::APP_MSG_FILE_START: {
			//char buffer[REG_STR_MAX];
			//g_MC->ReadRegString(NAME_FILE_CURRENT, 0, buffer, VALUE_FILE_CURRENT);
			//printf("start %s\n", buffer);
			camera_max_files = g_MC->ReadRegInt32("test.autodel", 0, -1);
			if (camera_max_files > 0) {
				handle_new_file();
			}
		}
		break;

	case IMediaControl::APP_MSG_FILE_END: {
			//char buffer[REG_STR_MAX];
			//g_MC->ReadRegString(NAME_FILE_PREVIOUS, 0, buffer, VALUE_FILE_PREVIOUS);
			//printf("done %s\n", buffer);
		}
		break;

	case IMediaControl::APP_MSG_DONE_SAVING: {
		}
		break;

	default:
		break;
	}
}

static void init_vdb(void)
{
	IEngine *pEngine = IEngine::GetInterfaceFrom(g_MC);
	IVDBControl *vdbc = avf_create_vdb_control(IRegistry::GetInterfaceFrom(pEngine),
		camera_callback, NULL);	// TODO:
	if (vdbc) {
		g_vdb_info = IVdbInfo::GetInterfaceFrom(vdbc);
		g_MC->RegisterComponent(vdbc);
		vdbc->StorageReady(1);	///
		vdbc->Release();
	}
}

int run_camera(const char *conf)
{
	if (config_vout(0) < 0)
		return -1;

	g_MC = avf_create_media_control(NULL, camera_callback, NULL, true);

	if (g_vdb) {
		init_vdb();
	}

	set_media_source(g_MC, "camera", NULL, conf);

	if (g_auto_run) {
		g_MC->RunMedia();
	}

	RUN_MENU("Camera", g_camera_main_menu);

	g_MC->CloseMedia(true);
	g_MC->Release();

	free_filnames();

	return 0;
}

//-----------------------------------------------------------------------
//
//  playback test
//
//-----------------------------------------------------------------------

static void menu_pause_resume(void)
{
	Playback(g_MC).PauseResume();
}

#define FORMAT_TIME(_time) \
	(_time/1000)/3600, (_time/1000)/60%60, (_time/1000)%60, (_time%1000)

static void menu_show_info(void)
{
	Playback::PlaybackMediaInfo_t media_info;
	avf_u32_t pos;
	if (Playback(g_MC).GetMediaInfo(&media_info) == E_OK && Playback(g_MC).GetPlayPos(&pos) == E_OK) {
		printf("Length: %02d:%02d:%02d.%03d - %02d:%02d:%02d.%03d\n",
			FORMAT_TIME(media_info.total_length_ms),
			FORMAT_TIME(pos));
	}
}

static void menu_repeat(void)
{
	g_repeat = !g_repeat;
	printf("repeat: %s\n", g_repeat ? "on" : "off");
}

static void menu_fast_play(void)
{
	char buffer[128];
	int speed;

	get_input("Input speed (-16..16): ", buffer, sizeof(buffer));
	speed = atoi(buffer);

	Playback::FastPlayParam_t param = {0, speed};
	Playback(g_MC).FastPlay(&param);
}

static void menu_seek(void)
{
	char buffer[128];
	avf_u32_t ms;

	get_input("Input pos (ms): ", buffer, sizeof(buffer));
	if (buffer[0] == 0)
		return;

	ms = atoi(buffer);
	Playback(g_MC).Seek(ms);
}

static const menu_t g_playback_main_menu[] = {
	{"Set state", menu_set_state},
	{"Pause/Resume", menu_pause_resume},
	{"Seek", menu_seek},
	{"Show Info", menu_show_info},
	{"Repeat", menu_repeat},
	{"Fast Play", menu_fast_play},
};

static void playback_callback(void *context, avf_intptr_t tag, const avf_msg_t& msg)
{
	if (msg.code == IMediaControl::APP_MSG_STATE_CHANGED) {
		if (g_repeat) {
			IMediaControl::StateInfo info;
			g_MC->GetStateInfo(info);
			if (info.state == IMediaControl::STATE_STOPPED) {
				if (info.prevState == IMediaControl::STATE_STOPPING ||
					info.prevState == IMediaControl::STATE_RUNNING)
					g_MC->RunMedia();
			}
		}
	}
}

#include "vdb_cmd.h"

static int get_clip_name(int index, char *buffer)
{
	IVdbInfo *info = g_vdb_info;
		//IVdbInfo::GetInterfaceFrom(g_MC);
	if (info == NULL) {
		printf("No IVdbInfo!\n");
		return -1;
	}

	avf_u32_t num_clips;
	avf_u32_t sid;

	avf_status_t status = info->GetNumClips(0, &num_clips, &sid);
	if (status != E_OK) {
		printf("GetNumClips failed\n");
		return -1;
	}

	printf("There are %d clips\n", num_clips);
	if (index < 0 || (unsigned)index >= num_clips) {
		printf("No such clip %d\n", index);
		return -1;
	}

	IVdbInfo::clip_set_info_s cs_info;
	avf_u32_t num = 1;
	status = info->GetClipSetInfo(0, &cs_info, index, &num, &sid);
	if (status != E_OK) {
		printf("GetClipSetInfo failed\n");
		return -1;
	}

	// todo
	sprintf(buffer, "vdb:buffer/%x_%llx_%x", cs_info.ref_clip_id, cs_info.clip_time_ms, cs_info.length_ms);

	return 0;
}

int run_playback(const char *filename, const char *conf)
{
	if (config_vout(1) < 0)
		return -1;

	g_MC = avf_create_media_control(NULL, playback_callback, NULL, false);

	if (g_vdb) {
		init_vdb();

		// wait vdb ready - todo
		//g_MC->WaitVdbState(IVdbInfo::STATE_READY);

		char buffer[256];
		if (get_clip_name(atoi(filename), buffer) < 0) {
			g_MC->Release();
			return -1;
		}
		set_media_source(g_MC, "playback", buffer, conf);

	} else {

		if (filename) {
			set_media_source(g_MC, "playback", filename, conf);
		}
	}

	RUN_MENU("Playback", g_playback_main_menu);

	g_MC->CloseMedia();
	avf_status_t status;
	g_MC->WaitState(IMediaControl::STATE_IDLE, &status);
	g_MC->Release();

	return 0;
}

void test(void);

int main(int argc, char **argv)
{
	const char *player = "camera";
	const char *filename = NULL;
	const char *conf;
	int rval;

	test();

	if (argc > 1) {
		player = argv[1];
		if (argc > 2) {
			filename = argv[2];
			if (filename[0] == '-')
				filename = NULL;
		}
		if (init_param(argc, argv) < 0)
			return -1;
	}

	avf_platform_init();

	switch (player[0]) {
	case 'c':
		conf = g_conf[0] ? g_conf : g_camera_conf;
		rval = run_camera(conf);
		break;

	case 'p':
		conf = g_conf[0] ? g_conf : g_playback_conf;
		rval = run_playback(filename, conf);
		break;

	default:
		printf("Unknown player: %s\n", player);
		rval = -1;
		break;
	}

	avf_platform_cleanup();

	avf_mem_info();
	avf_file_info();
	avf_socket_info();

	return rval;
}

void test(void)
{
/*
	time_t time;
	struct tm tm;

	::memset(&tm, 0, sizeof(tm));
	tm.tm_year = (1970-1900);
	printf("1970: %lu\n", mktime(&tm));

	::memset(&tm, 0, sizeof(tm));
	tm.tm_year = (2012-1900);
	printf("2012: %lu\n", mktime(&tm));

	time = 1325376000;
	avf_local_time(&time, &tm);
	printf("=== %d:%d:%d\n"
		"mday: %d, mon: %d, year: %d, wday: %d, yday: %d, isdst: %d\n"
		"gmtoff: %ld, tm_zone: %s\n",
		tm.tm_hour, tm.tm_sec, tm.tm_min,
		tm.tm_mday, tm.tm_mon, tm.tm_year, tm.tm_wday, tm.tm_yday, tm.tm_isdst,
		tm.tm_gmtoff, tm.tm_zone);
*/
/*
	time_t time;
	struct tm tm;

	time = 0;
	avf_local_time(&time, &tm);
	printf("=== %d:%d:%d\n"
		"mday: %d, mon: %d, year: %d, wday: %d, yday: %d, isdst: %d\n"
		"gmtoff: %ld, tm_zone: %s\n",
		tm.tm_hour, tm.tm_sec, tm.tm_min,
		tm.tm_mday, tm.tm_mon, tm.tm_year, tm.tm_wday, tm.tm_yday, tm.tm_isdst,
		tm.tm_gmtoff, tm.tm_zone);
	printf("mktime: %lu\n", mktime(&tm));
//	tm.tm_hour = 16;
//	printf("mktime: %lu\n", mktime(&tm));
	tm.tm_year = (2012-1900);
	printf("mktime: %lu\n", mktime(&tm));
*/
/*
	::memset(&tm, 0, sizeof(tm));
	tm.tm_year = (2039-1900);
	printf("mktime: %lu\n\n", mktime(&tm));
*/

/*
	time_t curr_time = time(NULL);
	printf("sizeof(struct tm): %d, time: %lu\n\n", sizeof(struct tm), curr_time);

	struct tm local_tm;
	avf_local_time(&curr_time, &local_tm);
	printf("=== %d:%d:%d\n"
		"mday: %d, mon: %d, year: %d, wday: %d, yday: %d, isdst: %d\n"
		"gmtoff: %ld, tm_zone: %s\n",
		local_tm.tm_hour, local_tm.tm_sec, local_tm.tm_min,
		local_tm.tm_mday, local_tm.tm_mon, local_tm.tm_year, local_tm.tm_wday, local_tm.tm_yday, local_tm.tm_isdst,
		local_tm.tm_gmtoff, local_tm.tm_zone);
	printf("mktime: %lu\n\n", mktime(&local_tm));
	
	local_tm.tm_gmtoff = 0;
	local_tm.tm_zone = NULL;
	printf("mktime: %lu\n\n", mktime(&local_tm));

	gmtime_r(&curr_time, &local_tm);
	printf("=== %d:%d:%d\n"
		"mday: %d, mon: %d, year: %d, wday: %d, yday: %d, isdst: %d\n"
		"gmtoff: %ld, tm_zone: %s\n",
		local_tm.tm_hour, local_tm.tm_sec, local_tm.tm_min,
		local_tm.tm_mday, local_tm.tm_mon, local_tm.tm_year, local_tm.tm_wday, local_tm.tm_yday, local_tm.tm_isdst,
		local_tm.tm_gmtoff, local_tm.tm_zone);
	printf("mktime: %lu\n\n", mktime(&local_tm));
*/
}

