
#include <fcntl.h>
#include <linux/rtc.h>

#include <signal.h>
#include <setjmp.h>

#include <getopt.h>
#include <agbox.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"

#include "libipc.h"
#include "avf_media.h"

#include "test_common.cpp"
#include "avahi.cpp"

#include "ccam_cmd.h"

#undef LOG_TAG
#define LOG_TAG "ccam-state"

#include "ccam_state.cpp"

#undef LOG_TAG
#define LOG_TAG "ccam-server"
#include "ccam_server.cpp"

#undef LOG_TAG
#define LOG_TAG "main"

#define MBPS	1000000
#define KBPS	1000

#undef LOG_TAG
#define LOG_TAG	"ccam-config"
#include "ccam_config.cpp"

//============================================================

static const char prebuild_board_vin0[] = "prebuild.board.vin0";

//============================================================

static int opt_setclock = 1;
static int opt_video_config = -1;
static int opt_sec_video_config = -1;
static int opt_preview_config = -1;
static int opt_still_mode = 0;
static int opt_idle_state = 0;
static int opt_auto_record = 0;
static int opt_auto_delete = 1;
static int opt_no_delete = 0;
static int opt_raw = 0;
static int opt_hdmi_mode = -2; // -2: do nothing; -1: disable
static int opt_lcd_mode = -2;	// -2: do nothing; -1: disable
static int opt_lcd_mode_set = 0;
static int opt_fb_id = -1;	// -1: disable; 0: fb0; 1: fb1
static int opt_mirror_horz = 0;
#ifdef ARCH_A5S
static int opt_mirror_vert = 1;
#else
static int opt_mirror_vert = 0;
#endif
static int opt_anti_flicker = 0;
static int opt_video_letterboxed = 0;
static int opt_flip_horz = 0;
static int opt_flip_vert = 0;
static int opt_osd_flip_horz = 0;
static int opt_osd_flip_vert = 0;

static int opt_force_enc_mode = 0;
static int opt_enc_mode = 0;
static int opt_exposure = 0;	// default
static int opt_bits = 0;	// not specified
static int opt_hdr = 0;
static int opt_force_hdr = 0;	// opt_hdr not specified
static char opt_vin_video_mode[32] = {0};

static int opt_daemon = 0;
static int opt_nologo = 0;
static int opt_overlay_flags = 0x0f;
static int opt_nowrite = 0;
static int opt_rtsp = 0;
static int opt_config = 0;

static char opt_format[32];
static char opt_vdbfs[128] = VALUE_VDB_FS;
static char opt_conf[256] = "/usr/local/bin/camera.conf";

pthread_t main_thread_id;

#include "ccam_control.cpp"

//============================================================

class MainCameraStateListener: public CObject, public ICameraStateListener
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

public:
	static MainCameraStateListener* Create(CCameraState *pCameraState);

private:
	MainCameraStateListener(CCameraState *pCameraState):
		mpCameraState(pCameraState), mbStateChanged(false) {}
	virtual ~MainCameraStateListener() {}

// ICameraStateListener
public:
	virtual void OnStateChanged(int which);
	virtual void OnRecordError(enum Rec_Error_code code) {}
	virtual void OnStillCaptureStarted(int one_shot) {}

public:
	int WaitForVideoMode(int index);

private:
	CCameraState *mpCameraState;
	CMutex mMutex;
	CCondition mStateCondition;
	bool mbStateChanged;
};

MainCameraStateListener* MainCameraStateListener::Create(CCameraState *pCameraState)
{
	MainCameraStateListener *result = avf_new MainCameraStateListener(pCameraState);
	CHECK_STATUS(result);
	return result;
}

void *MainCameraStateListener::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_ICameraStateListener)
		return static_cast<ICameraStateListener*>(this);
	return inherited::GetInterface(refiid);
}

void MainCameraStateListener::OnStateChanged(int which)
{
	switch (which) {
	case RecState:
	case VideoResolution: {
			AUTO_LOCK(mMutex);
			mbStateChanged = true;
			mStateCondition.WakeupAll();
		}
		break;
	}
}

// return 0 on OK, -1 on error state
int MainCameraStateListener::WaitForVideoMode(int index)
{
	while (true) {
		enum State_Rec state;
		int is_still;
		mpCameraState->GetRecState(&state, &is_still);

		if (state == State_Rec_error)
			return -1;

		if (state == State_Rec_stopped) {
			enum Video_Resolution resolution;
			resolution = mpCameraState->GetVideoResolution();
			if (g_video_config[index].std_resolution == resolution) {
				break;
			}
		}

		mMutex.Lock();
		if (!mbStateChanged) {
			mStateCondition.Wait(mMutex);
		}
		mbStateChanged = false;
		mMutex.Unlock();
	}

	return 0;
}

//============================================================

CCameraControl *g_ccam_control = NULL;
CAvahi *g_avahi = NULL;
CCameraState *g_ccam_state = NULL;
CCamServer *g_ccam_server = NULL;
MainCameraStateListener *g_ccam_state_listener;

//============================================================

static void menu_start_record(void)
{
	g_ccam_control->StartRecord();
}

static void menu_stop_record(void)
{
	g_ccam_control->StopRecord();
}

static void menu_enter_idle(void)
{
	g_ccam_control->EnterIdle();
}

static void menu_set_video_config(void)
{
	char buffer[64];
	int index;

	for (unsigned i = 0; i < ARRAY_SIZE(g_video_config); i++) {
		printf("  [" C_CYAN "%d" C_NONE "] %s\n", i + 1, g_video_config[i].name);
	}
	get_input(C_GREEN "  Select: " C_NONE, buffer, sizeof(buffer));

	if (buffer[0] == 0)
		return;

	index = ::atoi(buffer) - 1;
	if (index < 0 || index >= (int)ARRAY_SIZE(g_video_config)) {
		AVF_LOGE("bad input %d\n", index);
		return;
	}

	g_ccam_control->CmdChangeVideoConfig(index);
}

static void menu_set_preview_config(void)
{
	char buffer[64];
	int index;

	unsigned count;
	const preview_config_t *table = get_preview_config_table(opt_config, &count);

	for (unsigned i = 0; i < count; i++) {
		printf("  [" C_CYAN "%d" C_NONE "] %s\n", i + 1, table[i].name);
	}
	get_input(C_GREEN "  Select: " C_NONE, buffer, sizeof(buffer));

	if (buffer[0] == 0)
		return;

	index = ::atoi(buffer) - 1;
	if (index < 0 || index >= (int)count) {
		AVF_LOGE("bad input %d\n", index);
		return;
	}

	g_ccam_control->CmdChangePreviewConfig(index);
}

static void menu_set_second_video_config(void)
{
	char buffer[64];
	int index;

	unsigned count;
	const sec_video_config_t *table = get_sec_video_config_table(opt_config, &count);

	for (unsigned i = 0; i < count; i++) {
		printf("  [" C_CYAN "%d" C_NONE "] %s\n", i + 1, table[i].name);
	}
	get_input(C_GREEN "  Select: " C_NONE, buffer, sizeof(buffer));

	if (buffer[0] == 0)
		return;

	index = ::atoi(buffer) - 1;
	if (index < 0 || index >= (int)count) {
		AVF_LOGE("bad input %d\n", index);
		return;
	}

	g_ccam_control->CmdChangeSecondVideoConfig(index);
}

static void menu_set_video_mode(void)
{
	g_ccam_control->CmdSetStillCaptureMode(false);
}

static void menu_set_still_capture_mode(void)
{
	g_ccam_control->CmdSetStillCaptureMode(true);
}

static void menu_still_capture(void)
{
	char buffer[64];

	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 1, "Single");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 2, "Burst");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 3, "Stop");
	get_input(C_GREEN "  Select: " C_NONE, buffer, sizeof(buffer));

	if (buffer[0] == 0)
		return;

	int action = ::atoi(buffer);
	if (action <= 0 || action > 4) {
		AVF_LOGE("bad action %d\n", action);
		return;
	}

	g_ccam_control->CmdStillCapture(action);
}

static void menu_capture_raw(void)
{
	int raw_mode = g_ccam_state->GetRawMode();
	if (raw_mode) {
		printf(C_YELLOW "Set to JPEG mode\n" C_NONE);
	} else {
		printf(C_YELLOW "Set to RAW mode\n" C_NONE);
	}
	g_ccam_control->CmdSetCaptureRaw(!raw_mode);
}

static void menu_unmount_tf(void)
{
	g_ccam_control->CmdUnmountTF();
}

static void menu_mount_tf(void)
{
	g_ccam_control->CmdMountTF();
}

static void menu_mark_live_clip(void)
{
	// 5s + 10s
	g_ccam_control->CmdMarkLiveClip(5*1000, 10*1000);
}

static void menu_manual_mark_live_clip(void)
{
	g_ccam_control->CmdMarkLiveClip(0, -1);	// start

	char buffer[64];
	get_input(C_GREEN "  Press any key to stop... " C_NONE, buffer, sizeof(buffer));

	raw_data_t *raw = avf_alloc_raw_data(128, 0);
	::memset(raw->GetData(), 0, raw->GetSize());

	sprintf((char*)raw->GetData(), "manual marked live clip");
	g_ccam_control->CmdSetSceneData(raw);
	raw->Release();

	g_ccam_control->CmdStopMarkLiveClip();
}

static int set_to_video_mode(int curr_mode, int new_mode)
{
	g_ccam_control->CmdChangeVideoConfig(new_mode);
	if (g_ccam_state_listener->WaitForVideoMode(new_mode) < 0) {
		printf(C_PURPLE "switch to %s failed" C_NONE "\n", g_video_config[new_mode].name);
		return -1;
	} else {
		printf("set to %s done\n", g_video_config[new_mode].name);
		return new_mode;
	}
}

static void menu_test_switch_video_config(void)
{
	int loop_counter = 0;
	int curr_mode = -1;

	while (true) {
		printf(C_CYAN "loop %d start" C_NONE "\n", ++loop_counter);
		for (int i = 0; i < (int)ARRAY_SIZE(g_video_config); i++) {

			printf(C_YELLOW "=== switch from %s ===" C_NONE "\n", g_video_config[i].name);

			for (int j = 0; j < (int)ARRAY_SIZE(g_video_config); j++) {

				// switch to i first
				if (curr_mode == i) {
					printf("skip %s\n", g_video_config[curr_mode].name);
				} else {
					if ((curr_mode = set_to_video_mode(curr_mode, i)) < 0)
						return;
					avf_msleep(3*1000);
				}

				// switch to j
				if (curr_mode == j) {
					printf("skip %s\n", g_video_config[curr_mode].name);
				} else {
					if ((curr_mode = set_to_video_mode(curr_mode, j)) < 0)
						return;
					avf_msleep(3*1000);
				}
			}
		}
	}
}

static void menu_toggle_mirror_horizontal(void)
{
	g_ccam_control->CmdSetMirrorHorz(true, false);
}

static void menu_toggle_mirror_vertical(void)
{
	g_ccam_control->CmdSetMirrorVert(true, false);
}

static void menu_change_lcd_video(void)
{
	char buffer[64];

	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 1, "pan and scan");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 2, "letterbox");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 3, "input parameters...");

	get_input(C_GREEN "  Select: " C_NONE, buffer, sizeof(buffer));

	if (buffer[0] == 0)
		return;

	switch (buffer[0]) {
	case '1':
		g_ccam_control->CmdChangeLCDVideo(0);
		break;

	case '2':
		g_ccam_control->CmdChangeLCDVideo(1);
		break;

	case '3': {
			int xoff;
			int yoff;
			int video_width;
			int video_height;

			get_input("format: " C_YELLOW "xoff,yoff,width,height  " C_NONE,
				buffer, sizeof(buffer));
			if (::sscanf(buffer, "%d,%d,%d,%d", &xoff, &yoff, &video_width, &video_height) != 4) {
				printf("invalid");
			} else {
				g_ccam_control->CmdChangeLCDVideo(xoff, yoff, video_width, video_height);
			}
		}
		break;
	}
}

static void menu_set_lcd_brightness(void)
{
	int brightness = g_ccam_control->GetLCDBrightness();
	char msg[256];
	char buffer[64];

	sprintf(msg, "brightness: " C_GREEN "%d" C_NONE ", input new (0-255): ", brightness);
	get_input(msg, buffer, sizeof(buffer));

	if (buffer[0] == 0)
		return;

	int value = ::atoi(buffer);
	g_ccam_control->ChangeLCDBrightness(value);
}

static void menu_flip_lcd_video(void)
{
	char buffer[64];

	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 1, "normal");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 2, "horizontal & vertical");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 3, "horizontal");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 4, "vertical");

	get_input(C_GREEN "  Select: " C_NONE, buffer, sizeof(buffer));

	if (buffer[0] == 0)
		return;

	int flip;
	switch (buffer[0]) {
	case '1': flip = VOUT_FLIP_NORMAL; break;
	case '2': flip = VOUT_FLIP_HV; break;
	case '3': flip = VOUT_FLIP_H; break;
	case '4': flip = VOUT_FLIP_V; break;
	default:
		return;
	}

	g_ccam_control->FlipLCDVideo(flip);
}

static void menu_flip_lcd_osd(void)
{
	char buffer[64];

	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 1, "normal");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 2, "horizontal & vertical");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 3, "horizontal");
	printf("  [" C_CYAN "%d" C_NONE "] %s\n", 4, "vertical");

	get_input(C_GREEN "  Select: " C_NONE, buffer, sizeof(buffer));

	if (buffer[0] == 0)
		return;

	int flip;
	switch (buffer[0]) {
	case '1': flip = VOUT_FLIP_NORMAL; break;
	case '2': flip = VOUT_FLIP_HV; break;
	case '3': flip = VOUT_FLIP_H; break;
	case '4': flip = VOUT_FLIP_V; break;
	default:
		return;
	}

	g_ccam_control->FlipLCDOSD(flip);
}

static const menu_t g_main_menu[] = {
	{"Start Record", menu_start_record},
	{"Enter Preview (Stop Record)", menu_stop_record},
	{"Enter Idle", menu_enter_idle},
	{"Set Video Config...", menu_set_video_config},
	{"Set Preview Config...", menu_set_preview_config},
	{"Set Second Video Config...", menu_set_second_video_config},
	{"Switch to Video Mode", menu_set_video_mode},
	{"Switch to Still Capture Mode", menu_set_still_capture_mode},
	{"Still Capture...", menu_still_capture},
	{"Toggle Capture Raw", menu_capture_raw},
	{"Unmout TF card", menu_unmount_tf},
	{"Mount TF card", menu_mount_tf},
	{"Mark Live Clip", menu_mark_live_clip},
	{"Manual Mark Live Clip", menu_manual_mark_live_clip},
	{"Test Switch Video Config", menu_test_switch_video_config},
	{"Toggle Mirror Horizontal", menu_toggle_mirror_horizontal},
	{"Toggle Mirror Vertical", menu_toggle_mirror_vertical},
	{"Change LCD Video...", menu_change_lcd_video},
	{"Set LCD Brightness...", menu_set_lcd_brightness},
	{"Flip LCD Video...", menu_flip_lcd_video},
	{"Flip LCD OSD...", menu_flip_lcd_osd},
};

//============================================================

static struct option long_options[] = {
	{"video", HAS_ARG, 0, 'v'},
	{"Video", HAS_ARG, 0, 'V'},
	{"preview", HAS_ARG, 0,'p'},
	{"still", NO_ARG, 0, 's'},
	{"idle", NO_ARG, 0, 'i'},
	{"auto-record", HAS_ARG,	0, 'a'},
	{"auto-delete", HAS_ARG, 0, 'd'},
	{"no-delete", NO_ARG, 0, 'n'},
	{"raw", NO_ARG, 0, 'r'},
	{"setclock", HAS_ARG, 0, 'c'},
	{"hdmi", HAS_ARG, 0, 'h'},
	{"lcd", HAS_ARG, 0, 'l'},
	{"letterbox", NO_ARG, 0, 'b'},
	{"fb", HAS_ARG, 0, 'f'},
	{"mirror-horz", HAS_ARG, 0, 0x80},
	{"mirror-vert", HAS_ARG, 0, 0x81},
	{"flip-horz", HAS_ARG, 0, 0x82},
	{"flip-vert", HAS_ARG, 0, 0x83},
	{"enc-mode", HAS_ARG, 0, 0x84},
	{"daemon", NO_ARG, 0, 0x86},
	{"vdbfs", HAS_ARG, 0, 0x87},
	{"exposure", HAS_ARG, 0, 0x88},
	{"bits", HAS_ARG, 0, 0x89},
	{"hdr", HAS_ARG, 0, 0x90},
	{"vin", HAS_ARG, 0, 0x91},
	{"nologo", NO_ARG, 0, 0x92},
	{"overlay", HAS_ARG, 0, 0x93},
	{"conf", HAS_ARG, 0, 0x94},
	{"osd-flip-horz", HAS_ARG, 0, 0x95},
	{"osd-flip-vert", HAS_ARG, 0, 0x96},
	{"nowrite", NO_ARG, 0, 0x97},
	{"rtsp", NO_ARG, 0, 0x98},
	{"format", HAS_ARG, 0, 0x99},
	{"config", HAS_ARG, 0, 0x100},
	{NULL, NO_ARG, 0, 0}
};

static const char *short_options = "V:v:sia:d:nrc:h:l:fp";

#define OPT_SUB_STR(_name, _arg, _desc) \
	"  --" C_CYAN _name C_NONE " " _arg " : " C_GREEN _desc C_NONE "\n"

#define OPT_STR \
	OPT_SUB_STR("video", "index", "set video config as index") \
	OPT_SUB_STR("Video", "index", "set second video config as index") \
	OPT_SUB_STR("preview", "index", "set preview video config as index") \
	OPT_SUB_STR("still", "", "if enter still mode") \
	OPT_SUB_STR("idle", "", "if stay in idle state") \
	OPT_SUB_STR("auto-record", "0/1", "auto start record when TF is ready") \
	OPT_SUB_STR("auto-delete", "0/1", "auto delete when TF space is low") \
	OPT_SUB_STR("no-delete", "", "prohibit delete clips") \
	OPT_SUB_STR("raw", "", "raw format for still capture") \
	OPT_SUB_STR("setclock", "0/1", "if set clock before entering preview") \
	OPT_SUB_STR("hdmi", "resolution", "enable HDMI") \
	OPT_SUB_STR("lcd", "resolution", "enable LCD") \
	OPT_SUB_STR("letterbox", "", "letterbox video on vout") \
	OPT_SUB_STR("fb", "id", "Select Frame Buffer") \
	OPT_SUB_STR("mirror-horz", "0/1", "mirror VIN horizontal") \
	OPT_SUB_STR("mirror-vert", "0/1", "mirror VIN vertical") \
	OPT_SUB_STR("flip-horz", "0/1", "mirror Video horizontal") \
	OPT_SUB_STR("flip-vert", "0/1", "mirror Video vertical") \
	OPT_SUB_STR("enc-mode", "[mode]", "force using enc mode") \
	OPT_SUB_STR("daemon", "", "run daemon in the background (no input)") \
	OPT_SUB_STR("vdbfs", "(default is " VALUE_VDB_FS ")", "set root dir for VDB") \
	OPT_SUB_STR("exposure", "[num]", "specify dsp sysres->exposure_num") \
	OPT_SUB_STR("bits", "[bits]", "specify vin bits") \
	OPT_SUB_STR("hdr", "[hdr_mode]", "specify hdr mode") \
	OPT_SUB_STR("vin", "[vin video mode]", "specify vin video mode") \
	OPT_SUB_STR("nologo", "", "do not display TS logo") \
	OPT_SUB_STR("overlay", "[flags]", "set video overlay flags") \
	OPT_SUB_STR("conf", "[path]", "set conf file") \
	OPT_SUB_STR("osd-flip-horz", "0/1", "mirror OSD horizontal") \
	OPT_SUB_STR("osd-flip-vert", "0/1", "mirror OSD vertical") \
	OPT_SUB_STR("nowrite", "", "do not write disk") \
	OPT_SUB_STR("rtsp", "", "enable RTSP on second stream") \
	OPT_SUB_STR("format", "ts|mp4", "specify file format") \
	OPT_SUB_STR("config", "0,1", "specify video/mjpeg/2nd-video config version")

int init_param(int argc, char **argv)
{
	int option_index = 0;
	int ch;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'v':
			if ((opt_video_config = find_video_config(optarg)) < 0) {
				AVF_LOGE("bad video config %s", optarg);
				return -1;
			}
			break;

		case 'V':
			if ((opt_sec_video_config = find_sec_video_config(optarg, opt_config)) < 0) {
				AVF_LOGE("bad second video config %s", optarg);
				return -1;
			}
			break;

		case 'p':
			if ((opt_preview_config = find_preview_config(optarg, opt_config)) < 0) {
				AVF_LOGE("bad preview config %s", optarg);
				return -1;
			}
			break;

		case 's': opt_still_mode = 1; break;
		case 'i': opt_idle_state = 1; break;
		case 'a': opt_auto_record = ::atoi(optarg); break;
		case 'd': opt_auto_delete = ::atoi(optarg); break;
		case 'n': opt_no_delete = 1; break;
		case 'r': opt_raw = 1; break;
		case 'c': opt_setclock = ::atoi(optarg); break;
		case 'h': opt_hdmi_mode = find_vout_mode(optarg); break;
		case 'l': opt_lcd_mode = find_vout_mode(optarg); opt_lcd_mode_set = 1; break;
		case 'b': opt_video_letterboxed = 1; break;
		case 'f': opt_fb_id = ::atoi(optarg); break;

		case 0x80: opt_mirror_horz = ::atoi(optarg) ? 1 : 0; break;
		case 0x81: opt_mirror_vert = ::atoi(optarg) ? 1 : 0; break;
		case 0x82: opt_flip_horz = ::atoi(optarg) ? 1 : 0; break;
		case 0x83: opt_flip_vert = ::atoi(optarg) ? 1 : 0; break;
		case 0x84: opt_force_enc_mode = 1; opt_enc_mode = ::atoi(optarg); break;
		case 0x86: opt_daemon = 1; break;
		case 0x87: ::strcpy(opt_vdbfs, optarg); break;
		case 0x88: opt_exposure = ::atoi(optarg); break;
		case 0x89: opt_bits = ::atoi(optarg); break;

		case 0x90: opt_hdr = ::atoi(optarg); opt_force_hdr = 1; break;
		case 0x91: ::strcpy(opt_vin_video_mode, optarg); break;
		case 0x92: opt_nologo = 1; break;
		case 0x93: opt_overlay_flags = ::atoi(optarg); break;
		case 0x94: ::strcpy(opt_conf, optarg); break;
		case 0x95: opt_osd_flip_horz = ::atoi(optarg) ? 1 : 0; break;
		case 0x96: opt_osd_flip_vert = ::atoi(optarg) ? 1 : 0; break;
		case 0x97: opt_nowrite = 1; break;
		case 0x98: opt_rtsp = 1; break;
		case 0x99: ::strcpy(opt_format, optarg); break;

		case 0x100: opt_config = ::atoi(optarg); break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

#define CCAM_SERVER_PORT	10086

static int app_init(void)
{
	// ccam state
	g_ccam_state = CCameraState::Create();
	if (g_ccam_state == NULL)
		return -1;

#ifdef ARCH_A5S
	if (opt_still_mode) {
		AVF_LOGW("a5s does not have still mode");
		opt_still_mode = 0;
	}
#endif

	// ccam control
	g_ccam_control = CCameraControl::Create(g_ccam_state);
	if (g_ccam_control == NULL)
		return -1;

	// ccam server
	g_ccam_server = CCamServer::Create(g_ccam_state, g_ccam_control);
	if (g_ccam_server == NULL) {
		AVF_LOGW("failed to create cam server");
		return -1;
	}
	g_ccam_server->Run(CCAM_SERVER_PORT);

	// ccam state listener
	g_ccam_state_listener = MainCameraStateListener::Create(g_ccam_state);
	if (g_ccam_state_listener == NULL)
		return -1;

	g_ccam_state->RegisterOnStateChangedListener(g_ccam_state_listener);

	// avahi
	g_avahi = CAvahi::Create("Vidit Camera", "_ccam._tcp", "local", NULL, CCAM_SERVER_PORT);
	if (g_avahi == NULL) {
		AVF_LOGW("failed to create avahi server");
		return -1;
	}

	return 0;
}

static void app_deinit(void)
{
	avf_safe_release(g_avahi);
	if (g_ccam_state && g_ccam_state_listener) {
		g_ccam_state->UnRegisterOnStateChangedListener(g_ccam_state_listener);
	}
	avf_safe_release(g_ccam_state_listener);
	avf_safe_release(g_ccam_server);
	avf_safe_release(g_ccam_control);
	avf_safe_release(g_ccam_state);
}

int main(int argc, char **argv)
{
	printf("test_media_server\n" OPT_STR);

	main_thread_id = pthread_self();

	if (init_param(argc, argv) < 0)
		return -1;

#if defined(BOARD_HELLFIRE) || defined(BOARD_HACHI)
	if (!opt_lcd_mode_set) {
		opt_lcd_mode = find_vout_mode("D400");
	}
#endif

	ipc_init();

	if (app_init() == 0) {
		if (opt_daemon) {
			while (1) {
				sleep(1);
			}
		} else {
			RUN_MENU("Main", g_main_menu);
		}
	}

	app_deinit();

	ipc_deinit();

	// for debug
	avf_mem_info(1);
	avf_file_info();
	avf_socket_info();

	return 0;
}

