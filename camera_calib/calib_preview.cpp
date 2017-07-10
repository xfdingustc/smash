#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <assert.h>
#include <basetypes.h>
#include <iav_ioctl.h>
#include <iav_vout_ioctl.h>
#include <signal.h>

#include <agbox.h>

#include "lcd/lcd_api.h"
#include "lcd/lcd_digital.c"
#include "lcd/lcd_digital601.c"
#include "lcd/lcd_td043.c"


//#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define DIV_ROUND(divident, divider)    ( ( (divident)+((divider)>>1)) / (divider) )
#define ROUND_UP(size, align) (((size) + ((align) - 1)) & ~((align) - 1))

typedef struct{
	int sensor_id;
	int video_mode;
	int hdr_mode;
	int width;
	int height;
	int fps;
	int bit_resolution;
	int mirror;
	int bayer_pattern;
	char sensor_name[64];
}vin_t;

typedef struct{
	int enable;
	int input;
	int mode;
	int width;
	int height;
	int device;
	int lcd_model_index;
}vout_t;

typedef struct{
	int from_mixer_a;
	int from_mixer_b;
}osd_t;

typedef struct{
	int enc_mode;
	int raw2enc;
	int raw_cap;
	int raw_max_width;
	int raw_max_height;
	int exposure_num;
	int duration;
	char next_cfg_file[64];
}pipeline_t;

typedef struct{
	int input_width;
	int input_height;
	int input_offset_x;
	int input_offset_y;
	int max_width;
	int max_height;
	int width;
	int height;
	int type;
}buffer_t;

typedef struct{
	int width;
	int height;
	int max_width;
	int max_height;
	int m;
	int n;
	int bit_rate;
	int quality;
	int h_flip;
	int v_flip;
	int rotate;
	int fps_ratio;
	int source;
	int type;
}stream_t;

enum {
	FLIP_NORMAL = 0,
	FLIP_HV = 1,
	FLIP_H = 2,
	FLIP_V = 3,
};

typedef struct lcd_model {
	const char			*model;
	const LCD_SETMODE_FUNC		lcd_setmode;
	const LCD_POST_SETMODE_FUNC	lcd_post_setmode;
} lcd_model_t;


static int source = -1;
//static int hdr_mode	= AMBA_VIDEO_LINEAR_MODE;
//static int mirror_pattern	= VINDEV_MIRROR_AUTO;
//static int bayer_pattern	= VINDEV_BAYER_PATTERN_AUTO;
static struct vindev_mirror mirror_mode;
static pipeline_t pipeline_cfg;
static vin_t vin_cfg;
static vout_t vout0_cfg;
static vout_t vout1_cfg;
static buffer_t main_buf_cfg;
static buffer_t second_buf_cfg;
static buffer_t third_buf_cfg;
static buffer_t fourth_buf_cfg;
static stream_t stream_a_cfg;
static stream_t stream_b_cfg;
static stream_t stream_c_cfg;
static stream_t stream_d_cfg;
static osd_t osd_cfg;

static lcd_model_t lcd_model_list[] = {
	{"digital",	lcd_digital_setmode,	NULL},
	{"digital601",	lcd_digital601_setmode,	NULL},
	{"td043",	lcd_td043_setmode,	NULL},
	{"td043_16",	lcd_td043_16_setmode,	NULL},
};

static int _fps2hz(u32 fps_q9, u32 *fps_hz, char *fps)
{
	u32				hz = 0;

	switch(fps_q9) {
	case AMBA_VIDEO_FPS_AUTO:
		snprintf(fps, 32, "%s", "AUTO");
		break;
	case AMBA_VIDEO_FPS_29_97:
		snprintf(fps, 32, "%s", "29.97");
		break;
	case AMBA_VIDEO_FPS_59_94:
		snprintf(fps, 32, "%s", "59.94");
		break;
	default:
		hz = DIV_ROUND(512000000, fps_q9);
		snprintf(fps, 32, "%d", hz);
		break;
	}

	if (fps_hz) {
		*fps_hz = hz;
	}

	return 0;
}

static inline enum amba_video_mode cfg_video_mode_index2mode(int index)
{
	enum amba_video_mode mode = AMBA_VIDEO_MODE_MAX;

	if (index < 0)
		goto amba_video_mode_index2mode_exit;


	if ((index >= 0) && (index < AMBA_VIDEO_MODE_MISC_NUM)) {
		mode = (enum amba_video_mode)index;
		goto amba_video_mode_index2mode_exit;
	}

       if ((index >= AMBA_VIDEO_MODE_MISC_NUM) && (index < AMBA_VIDEO_MODE_NUM)) {
               mode = (enum amba_video_mode)(AMBA_VIDEO_MODE_480I + (index -
                       AMBA_VIDEO_MODE_MISC_NUM));
               goto amba_video_mode_index2mode_exit;
       }

amba_video_mode_index2mode_exit:
	return mode;
}


/*==============================================================================
  Get Sink ID from sink type
==============================================================================*/
int _vout_get_sink_id(int fd_iav, int chan, int sink_type)
{
	int					num;
	int					i;
	struct amba_vout_sink_info		sink_info;
	u32					sink_id = -1;

	num = 0;
	if (ioctl(fd_iav, IAV_IOC_VOUT_GET_SINK_NUM, &num) < 0) {
		perror("IAV_IOC_VOUT_GET_SINK_NUM");
		return -1;
	}
	if (num < 1) {
		printf("Please load vout driver!\n");
		return -1;
	}

	for (i = num - 1; i >= 0; i--) {
		sink_info.id = i;
		if (ioctl(fd_iav, IAV_IOC_VOUT_GET_SINK_INFO, &sink_info) < 0) {
			perror("IAV_IOC_VOUT_GET_SINK_INFO");
			return -1;
		}

		printf("sink %d is %s, src %d\n", sink_info.id, sink_info.name, sink_info.source_id);

		if ((sink_info.sink_type == sink_type) &&
			(sink_info.source_id == chan))
			sink_id = sink_info.id;
	}

	printf("%s: %d %d, return %d\n", __func__, chan, sink_type, sink_id);

	return sink_id;
}

/*==============================================================================
  Initialize vout:
  	vout_id: 0-LCD 1-HDMI
  	direct_to_dsp: 0-dsp cmds will be cached in vout and sent to dsp by iav
  		       1-dsp cmds will be sent to dsp directly,
  			usually used to restart vout
==============================================================================*/
int cfg_init_vout(int fd_iav, int vout_id, u32 direct_to_dsp, vout_t* voutp)
{
	//external devices
	int device_selected, sink_type, lcd_model;

	//Options related to the whole vout
	int mode, csc_en, fps, qt;
	enum amba_vout_hdmi_color_space hdmi_cs;
	enum ddd_structure hdmi_3d_structure;
	enum amba_vout_hdmi_overscan hdmi_overscan;
	enum amba_vout_display_input display_input;

	//Options related to Video only
	int video_en, video_flip, video_rotate;
	struct amba_vout_video_size video_size;
	struct amba_vout_video_offset video_offset;

	//Options related to OSD only
	int fb_id, osd_flip;
	struct amba_vout_osd_rescale osd_rescale;
	struct amba_vout_osd_offset osd_offset;

	//For sink config
	int sink_id = -1;
	struct amba_video_sink_mode sink_cfg;

//	struct iav_system_resource resource;

	device_selected = voutp->enable;
	sink_type = voutp->device;
	lcd_model = voutp->lcd_model_index;//vout_lcd_model_index[vout_id];
	mode = voutp->mode;
	csc_en = 1;//vout_csc_en[vout_id];
	fps = 0;//vout_fps[vout_id];
	qt = 0;//vout_qt_osd[vout_id];
	hdmi_cs = AMBA_VOUT_HDMI_CS_AUTO;// vout_hdmi_cs[vout_id];
	hdmi_3d_structure = DDD_RESERVED;//vout_hdmi_3d_structure[vout_id];
	hdmi_overscan = AMBA_VOUT_HDMI_OVERSCAN_AUTO;//vout_hdmi_overscan[vout_id];
	video_en = 1;//vout_video_en[vout_id];
	video_flip = 0;//vout_video_flip[vout_id];
	video_rotate = 0;//vout_video_rotate[vout_id];

	video_size.specified = 1;
	video_size.video_width = video_size.vout_width = voutp->width;
	video_size.video_height = video_size.vout_height = voutp->height;

	video_offset.specified = 1;
	video_offset.offset_x = video_offset.offset_y = 0;
	fb_id = 0;//vout_fb_id[vout_id];
	osd_flip = 0;//vout_osd_flip[vout_id];
//	osd_rescale = vout_osd_rescale[vout_id];
	osd_rescale.enable = 0;
//	osd_offset = vout_osd_offset[vout_id];
	osd_offset.specified = 0;

	//Specify external device type for vout
	if (device_selected) {
		sink_id = _vout_get_sink_id(fd_iav, vout_id, sink_type);
		if (ioctl(fd_iav, IAV_IOC_VOUT_SELECT_DEV, sink_id) < 0) {
			perror("IAV_IOC_VOUT_SELECT_DEV");
			switch (sink_type) {
			case AMBA_VOUT_SINK_TYPE_CVBS:
				printf("No CVBS sink: ");
				printf("Driver not loaded!\n");
				break;

			case AMBA_VOUT_SINK_TYPE_YPBPR:
				printf("No YPBPR sink: ");
				printf("Driver not loaded ");
				printf("or ypbpr output not supported!\n");
				break;

			case AMBA_VOUT_SINK_TYPE_DIGITAL:
				printf("No DIGITAL sink: ");
				printf("Driver not loaded ");
				printf("or digital output not supported!\n");
				break;

			case AMBA_VOUT_SINK_TYPE_HDMI:
				printf("No HDMI sink: ");
				printf("Hdmi cable not plugged, ");
				printf("driver not loaded, ");
				printf("or hdmi output not supported!\n");
				break;

			default:
				break;
			}
			return -1;
		}
	}

	// update display input
/*	if (configure_mixer_flag[vout_id]) {
		memset(&resource, 0, sizeof(resource));
		resource.encode_mode = DSP_CURRENT_MODE;
		if (ioctl(fd_iav, IAV_IOC_GET_SYSTEM_RESOURCE, &resource) < 0) {
			perror("IAV_IOC_GET_SYSTEM_RESOURCE");
			return -1;
		}
		if (vout_id == 0) {
			display_input = resource.mixer_a_enable ?
				AMBA_VOUT_INPUT_FROM_MIXER : AMBA_VOUT_INPUT_FROM_SMEM;
		} else {
			display_input = resource.mixer_b_enable ?
				AMBA_VOUT_INPUT_FROM_MIXER : AMBA_VOUT_INPUT_FROM_SMEM;
		}
	}*/

	display_input = (enum amba_vout_display_input)voutp->input;

	//Configure vout
	memset(&sink_cfg, 0, sizeof(sink_cfg));
	if (sink_type == AMBA_VOUT_SINK_TYPE_DIGITAL) {
		if (lcd_model_list[lcd_model].lcd_setmode(mode, &sink_cfg) < 0) {
			perror("lcd_setmode fail.");
			return -1;
		}
	} else {
		sink_cfg.mode = mode;
		sink_cfg.ratio = AMBA_VIDEO_RATIO_AUTO;
		sink_cfg.bits = AMBA_VIDEO_BITS_AUTO;
		sink_cfg.type = AMBA_VIDEO_TYPE_AUTO;
		if (mode == AMBA_VIDEO_MODE_480I || mode == AMBA_VIDEO_MODE_576I
			|| mode == AMBA_VIDEO_MODE_1080I
			|| mode == AMBA_VIDEO_MODE_1080I50)
			sink_cfg.format = AMBA_VIDEO_FORMAT_INTERLACE;
		else
			sink_cfg.format = AMBA_VIDEO_FORMAT_PROGRESSIVE;
		sink_cfg.sink_type = AMBA_VOUT_SINK_TYPE_AUTO;
		sink_cfg.bg_color.y = 0x10;
		sink_cfg.bg_color.cb = 0x80;
		sink_cfg.bg_color.cr = 0x80;
		sink_cfg.lcd_cfg.mode = AMBA_VOUT_LCD_MODE_DISABLE;
	}
	if (csc_en) {
		sink_cfg.bg_color.y	= 0;
		sink_cfg.bg_color.cb	= 0;
		sink_cfg.bg_color.cr	= 0;
	}
	if (qt) {
		sink_cfg.osd_tailor = (enum amba_vout_tailored_info)(AMBA_VOUT_OSD_NO_CSC | AMBA_VOUT_OSD_AUTO_COPY);
	}
	sink_cfg.id = sink_id;
	sink_cfg.frame_rate = fps;
	sink_cfg.csc_en = csc_en;
	sink_cfg.hdmi_color_space = hdmi_cs;
	sink_cfg.hdmi_3d_structure = hdmi_3d_structure;
	sink_cfg.hdmi_overscan = hdmi_overscan;
	sink_cfg.video_en = video_en;

	switch (video_flip) {
	case FLIP_NORMAL:
		sink_cfg.video_flip = AMBA_VOUT_FLIP_NORMAL;
		break;
	case FLIP_HV:
		sink_cfg.video_flip = AMBA_VOUT_FLIP_HV;
		break;
	case FLIP_H:
		sink_cfg.video_flip = AMBA_VOUT_FLIP_HORIZONTAL;
		break;
	case FLIP_V:
		sink_cfg.video_flip = AMBA_VOUT_FLIP_VERTICAL;
		break;
	default:
		sink_cfg.video_flip = AMBA_VOUT_FLIP_NORMAL;
		break;
	}

	if (video_rotate) {
		sink_cfg.video_rotate = AMBA_VOUT_ROTATE_90;
	} else {
		sink_cfg.video_rotate = AMBA_VOUT_ROTATE_NORMAL;
	}

	sink_cfg.video_size = video_size;
	sink_cfg.video_offset = video_offset;
	sink_cfg.fb_id = fb_id;
	switch (osd_flip) {
	case FLIP_NORMAL:
		sink_cfg.osd_flip = AMBA_VOUT_FLIP_NORMAL;
		break;
	case FLIP_HV:
		sink_cfg.osd_flip = AMBA_VOUT_FLIP_HV;
		break;
	case FLIP_H:
		sink_cfg.osd_flip = AMBA_VOUT_FLIP_HORIZONTAL;
		break;
	case FLIP_V:
		sink_cfg.osd_flip = AMBA_VOUT_FLIP_VERTICAL;
		break;
	default:
		sink_cfg.osd_flip = AMBA_VOUT_FLIP_NORMAL;
		break;
	}
	sink_cfg.osd_rotate = AMBA_VOUT_ROTATE_NORMAL;
	sink_cfg.osd_rescale = osd_rescale;
	sink_cfg.osd_offset = osd_offset;
	sink_cfg.display_input = display_input;
	sink_cfg.direct_to_dsp = direct_to_dsp;

	if (ioctl(fd_iav, IAV_IOC_VOUT_CONFIGURE_SINK, &sink_cfg) < 0) {
		perror("IAV_IOC_VOUT_CONFIGURE_SINK");
		return -1;
	}

	printf("init_vout%d done\n", vout_id);
	return 0;
}

void cfg_vout_brightness(int fd_iav, int vout_id, vout_t *voutp, int brightness)
{
	int ret_val;
	struct iav_vout_sink_brightness_s sink_brightness;

	ret_val = _vout_get_sink_id(fd_iav, vout_id, voutp->device);
	if (ret_val < 0) {
		return;
	}

	sink_brightness.id = ret_val;
	sink_brightness.brightness = brightness;
	ret_val = ioctl(fd_iav, IAV_IOC_VOUT_SET_SINK_BRIGHTNESS,
		&sink_brightness);
	if (ret_val < 0) {
		perror("IAV_IOC_VOUT_SET_SINK_BRIGHTNESS");
	}
}

void _test_print_video_info( int fd_iav, const struct amba_video_info *pvinfo, int active_flag)
{
	char				format[32];
	char				hdrmode[32];
	char				fps[32];
	char				type[32];
	char				bits[32];
	char				ratio[32];
	char				system[32];
	u32				fps_q9 = 0;
	struct vindev_fps active_fps;

	switch(pvinfo->format) {

	case AMBA_VIDEO_FORMAT_PROGRESSIVE:
		snprintf(format, 32, "%s", "P");
		break;
	case AMBA_VIDEO_FORMAT_INTERLACE:
		snprintf(format, 32, "%s", "I");
		break;
	case AMBA_VIDEO_FORMAT_AUTO:
		snprintf(format, 32, "%s", "Auto");
		break;
	default:
		snprintf(format, 32, "format?%d", pvinfo->format);
		break;
	}

	switch(pvinfo->hdr_mode) {

	case AMBA_VIDEO_2X_HDR_MODE:
		snprintf(hdrmode, 32, "%s", "(HDR 2x)");
		break;
	case AMBA_VIDEO_3X_HDR_MODE:
		snprintf(hdrmode, 32, "%s", "(HDR 3x)");
		break;
	case AMBA_VIDEO_4X_HDR_MODE:
		snprintf(hdrmode, 32, "%s", "(HDR 4x)");
		break;
	case AMBA_VIDEO_INT_HDR_MODE:
		snprintf(hdrmode, 32, "%s", "(WDR In)");
		break;
	case AMBA_VIDEO_LINEAR_MODE:
	default:
		snprintf(hdrmode, 32, "%s", "");
		break;
	}

	if (active_flag) {
		memset(&active_fps, 0, sizeof(active_fps));
		active_fps.vsrc_id = 0;
		if (ioctl(fd_iav, IAV_IOC_VIN_GET_FPS, &active_fps) < 0) {
			perror("IAV_IOC_VIN_GET_FPS");
		}
		fps_q9 = active_fps.fps;
	} else {
		fps_q9 = pvinfo->max_fps;
	}
	_fps2hz(fps_q9, NULL, fps);

	switch(pvinfo->type) {
	case AMBA_VIDEO_TYPE_RGB_RAW:
		snprintf(type, 32, "%s", "RGB");
		break;
	case AMBA_VIDEO_TYPE_YUV_601:
		snprintf(type, 32, "%s", "YUV BT601");
		break;
	case AMBA_VIDEO_TYPE_YUV_656:
		snprintf(type, 32, "%s", "YUV BT656");
		break;
	case AMBA_VIDEO_TYPE_YUV_BT1120:
		snprintf(type, 32, "%s", "YUV BT1120");
		break;
	case AMBA_VIDEO_TYPE_RGB_601:
		snprintf(type, 32, "%s", "RGB BT601");
		break;
	case AMBA_VIDEO_TYPE_RGB_656:
		snprintf(type, 32, "%s", "RGB BT656");
		break;
	case AMBA_VIDEO_TYPE_RGB_BT1120:
		snprintf(type, 32, "%s", "RGB BT1120");
		break;
	default:
		snprintf(type, 32, "type?%d", pvinfo->type);
		break;
	}

	switch(pvinfo->bits) {
	case AMBA_VIDEO_BITS_AUTO:
		snprintf(bits, 32, "%s", "Bits Not Availiable");
		break;
	default:
		snprintf(bits, 32, "%dbits", pvinfo->bits);
		break;
	}

	switch(pvinfo->ratio) {
	case AMBA_VIDEO_RATIO_AUTO:
		snprintf(ratio, 32, "%s", "AUTO");
		break;
	case AMBA_VIDEO_RATIO_4_3:
		snprintf(ratio, 32, "%s", "4:3");
		break;
	case AMBA_VIDEO_RATIO_16_9:
		snprintf(ratio, 32, "%s", "16:9");
		break;
	default:
		snprintf(ratio, 32, "ratio?%d", pvinfo->ratio);
		break;
	}

	switch(pvinfo->system) {
	case AMBA_VIDEO_SYSTEM_AUTO:
		snprintf(system, 32, "%s", "AUTO");
		break;
	case AMBA_VIDEO_SYSTEM_NTSC:
		snprintf(system, 32, "%s", "NTSC");
		break;
	case AMBA_VIDEO_SYSTEM_PAL:
		snprintf(system, 32, "%s", "PAL");
		break;
	case AMBA_VIDEO_SYSTEM_SECAM:
		snprintf(system, 32, "%s", "SECAM");
		break;
	case AMBA_VIDEO_SYSTEM_ALL:
		snprintf(system, 32, "%s", "ALL");
		break;
	default:
		snprintf(system, 32, "system?%d", pvinfo->system);
		break;
	}

	printf("\t%dx%d%s%s\t%s\t%s\t%s\t%s\t%s\trev[%d]\n",
		pvinfo->width, pvinfo->height,
		format, hdrmode, fps, type, bits, ratio, system, pvinfo->rev);
}

void _check_source_info(int fd_iav)
{
	char					format[32];
	u32					i, j;
	struct vindev_devinfo			vsrc_info;
	struct vindev_video_info		video_info;

	vsrc_info.vsrc_id = 0;
	if (ioctl(fd_iav, IAV_IOC_VIN_GET_DEVINFO, &vsrc_info) < 0) {
		perror("IAV_IOC_VIN_GET_DEVINFO");
		return;
	}

	if (source < 0)
		source = vsrc_info.vsrc_id;

	printf("\nFind Vin Source %s ", vsrc_info.name);

	if (vsrc_info.dev_type == VINDEV_TYPE_SENSOR) {
		printf("it supports:\n");

		for (i = 0; i < AMBA_VIDEO_MODE_NUM; i++) {
			for (j = AMBA_VIDEO_MODE_FIRST; j < AMBA_VIDEO_MODE_LAST; j++) {
				video_info.info.hdr_mode = j;
				video_info.info.mode = cfg_video_mode_index2mode(i);
				video_info.vsrc_id = 0;

				if (ioctl(fd_iav, IAV_IOC_VIN_GET_VIDEOINFO, &video_info) < 0) {
					if (errno == EINVAL)
						continue;

					perror("IAV_IOC_VIN_GET_VIDEOINFO");
					break;
				}

				_test_print_video_info(fd_iav, &video_info.info, 0);
				if (i == AMBA_VIDEO_MODE_AUTO)
					break;
			}
		}
	} else {
		switch(vsrc_info.sub_type) {
		case VINDEV_SUBTYPE_CVBS:
			snprintf(format, 32, "%s", "CVBS");
			break;
		case VINDEV_SUBTYPE_SVIDEO:
			snprintf(format, 32, "%s", "S-Video");
			break;
		case VINDEV_SUBTYPE_YPBPR:
			snprintf(format, 32, "%s", "YPbPr");
			break;
		case VINDEV_SUBTYPE_HDMI:
			snprintf(format, 32, "%s", "HDMI");
			break;
		case VINDEV_SUBTYPE_VGA:
			snprintf(format, 32, "%s", "VGA");
			break;
		default:
			snprintf(format, 32, "format?%d", vsrc_info.sub_type);
			break;
		}
		printf("Channel[%s]'s type is %s\n", vsrc_info.name, format);

		video_info.info.mode = AMBA_VIDEO_MODE_AUTO;
		video_info.vsrc_id = 0;
		if (ioctl(fd_iav, IAV_IOC_VIN_GET_VIDEOINFO, &video_info) < 0) {
			perror("IAV_IOC_VIN_GET_VIDEOINFO");
			return;
		}

		if (video_info.info.height == 0) {
			printf("No signal yet.\n");
		} else {
			printf("The signal is:\n");
			_test_print_video_info(fd_iav, &video_info.info, 0);
		}
	}
}

static int cfg_set_vin_param(int fd_iav, int db, int fps)
{
	struct vindev_shutter vsrc_shutter;
	struct vindev_agc vsrc_agc;
	int vin_eshutter_time = fps;	// 1/fps
	int vin_agc_db = db;	// 0dB

	u32 shutter_time_q9;
	s32 agc_db;

	shutter_time_q9 = 512000000/vin_eshutter_time;
	agc_db = vin_agc_db<<24;

	vsrc_shutter.vsrc_id = 0;
	vsrc_shutter.shutter = shutter_time_q9;
	if (ioctl(fd_iav, IAV_IOC_VIN_SET_SHUTTER, &vsrc_shutter) < 0) {
		perror("IAV_IOC_VIN_SET_SHUTTER");
		return -1;
	}

	vsrc_agc.vsrc_id = 0;
	vsrc_agc.agc = agc_db;
	if (ioctl(fd_iav, IAV_IOC_VIN_SET_AGC, &vsrc_agc) < 0) {
		perror("IAV_IOC_VIN_SET_AGC");
		return -1;
	}

	return 0;
}

static int cfg_set_vin_frame_rate(int fd_iav, int fps)
{
	struct vindev_fps vsrc_fps;

	vsrc_fps.vsrc_id = 0;
	vsrc_fps.fps = DIV_ROUND(512000000, fps);
	//printf("set framerate %d\n", framerate);
	if (ioctl(fd_iav, IAV_IOC_VIN_SET_FPS, &vsrc_fps) < 0) {
		perror("IAV_IOC_VIN_SET_FPS");
		return -1;
	}
	return 0;
}

int cfg_init_vin(int fd_iav, vin_t* vinp)
{
	struct vindev_mode vsrc_mode;
	struct vindev_devinfo vsrc_info;
	struct vindev_video_info video_info;

	//IAV_IOC_VIN_GET_SOURCE_NUM

	_check_source_info(fd_iav);

	vsrc_info.vsrc_id = 0;
	if (ioctl(fd_iav, IAV_IOC_VIN_GET_DEVINFO, &vsrc_info) < 0) {
		perror("IAV_IOC_VIN_GET_DEVINFO");
		return -1;
	}
	if(vsrc_info.sensor_id != vinp->sensor_id){
		printf("sensor_id in cfg is %d, hardware is %d", vinp->sensor_id, vsrc_info.sensor_id);
		return -1;
	}

    memset(&vsrc_mode, 0x00, sizeof(vsrc_mode));
	vsrc_mode.vsrc_id = 0;
	vsrc_mode.video_mode = vinp->video_mode;
	vsrc_mode.hdr_mode = vinp->hdr_mode;
	if (ioctl(fd_iav, IAV_IOC_VIN_SET_MODE, &vsrc_mode) < 0) {
		perror("IAV_IOC_VIN_SET_MODE 1111");
		return -1;
	}

	if (vinp->mirror != VINDEV_MIRROR_AUTO || vinp->bayer_pattern != VINDEV_BAYER_PATTERN_AUTO) {
		mirror_mode.vsrc_id = 0;
		if (ioctl(fd_iav, IAV_IOC_VIN_SET_MIRROR, &mirror_mode) < 0) {
			perror("IAV_IOC_VIN_SET_MIRROR");
			return -1;
		}
	}


	if (vinp->video_mode == AMBA_VIDEO_MODE_OFF) {
		printf("VIN is power off.\n");
	} else {
		if (vsrc_info.dev_type == VINDEV_TYPE_SENSOR) {
			if (cfg_set_vin_param(fd_iav, 0, vinp->fps) < 0)	//set aaa here
				return -1;
			if (cfg_set_vin_frame_rate(fd_iav, vinp->fps)< 0)
				return -1;
		}

		/* get current video info */
		video_info.vsrc_id = 0;
		video_info.info.mode = AMBA_VIDEO_MODE_CURRENT;
		if (ioctl(fd_iav, IAV_IOC_VIN_GET_VIDEOINFO, &video_info) < 0) {
			perror("IAV_IOC_VIN_GET_VIDEOINFO");
			return -1;
		}

		printf("Active src %d's mode is:\n", source);
		_test_print_video_info(fd_iav,&video_info.info, 1);
		printf("init_vin done\n");
	}

	return 0;
}


int cfg_setup_resource_limit(int fd_iav, pipeline_t* pipe_cfg, vout_t* vout0_cfg, vout_t* vout1_cfg,
				osd_t* osd_cfg,buffer_t** buf_cfg, stream_t** stream_cfg)
{
	int i;
	struct iav_system_resource resource;

	memset(&resource, 0, sizeof(resource));
	resource.encode_mode = pipe_cfg->enc_mode;
	printf("enc mode %d\n", pipe_cfg->enc_mode);

	if(ioctl(fd_iav, IAV_IOC_GET_SYSTEM_RESOURCE, &resource)<0){
		perror("IAV_IOC_GET_SYSTEM_RESOURCE");
		return -1;
	}

	// set source buffer max size
	for (i = 0; i < 4; i++) {
		resource.buf_max_size[i].width = buf_cfg[i]->max_width;
		resource.buf_max_size[i].height = buf_cfg[i]->max_height;
	}

	// set stream max size
	for (i = 0; i < 4; ++i) {
		resource.stream_max_size[i].width = stream_cfg[i]->max_width;
		resource.stream_max_size[i].height = stream_cfg[i]->max_height;
		resource.stream_max_M[i] = stream_cfg[i]->m;
		resource.stream_max_N[i] = stream_cfg[i]->n;
	}

//	TBD
//	resource.rotate_enable = system_resource_setup.rotate_possible;

	resource.raw_capture_enable = pipe_cfg->raw_cap;

	resource.exposure_num = pipe_cfg->exposure_num;

//	resource.lens_warp_enable = system_resource_setup.lens_warp;

	resource.enc_raw_rgb = pipe_cfg->raw2enc;

	resource.raw_size.width = pipe_cfg->raw_max_width;
	resource.raw_size.height = pipe_cfg->raw_max_height;

//	if (system_resource_setup.max_warp_input_width_flag) {
//		resource.max_warp_input_width = system_resource_setup.max_warp_input_width;
//	}
//	if (system_resource_setup.max_warp_output_width_flag) {
//		resource.max_warp_output_width = system_resource_setup.max_warp_output_width;
//	}

//	if (system_resource_setup.max_padding_width_flag) {
//		resource.max_padding_width = system_resource_setup.max_padding_width;
//	}

	if (vout0_cfg->enable) {
		resource.mixer_a_enable =
			(vout0_cfg->input == AMBA_VOUT_INPUT_FROM_MIXER);
	} else {
		resource.mixer_a_enable = 0;
	}
	if (vout1_cfg->enable) {
		resource.mixer_b_enable =
			(vout1_cfg->input == AMBA_VOUT_INPUT_FROM_MIXER);
	} else {
		resource.mixer_b_enable = 0;
	}

	resource.osd_from_mixer_a = osd_cfg->from_mixer_a;
	resource.osd_from_mixer_b = osd_cfg->from_mixer_b;


	if(ioctl(fd_iav, IAV_IOC_SET_SYSTEM_RESOURCE, &resource)<0){
		perror("IAV_IOC_SET_SYSTEM_RESOURCE");
		return -1;
	}
	return 0;
}


int cfg_setup_source_buffer(int fd_iav, buffer_t** buf)
{
	struct iav_srcbuf_setup buf_setup;
	int i;

	memset(&buf_setup, 0, sizeof(buf_setup));
	if(ioctl(fd_iav, IAV_IOC_GET_SOURCE_BUFFER_SETUP, &buf_setup)<0){
		perror("IAV_IOC_GET_SOURCE_BUFFER_SETUP");
		return -1;
	}

	for (i = 0; i < 4; i++) {
		buf_setup.type[i] = (enum iav_srcbuf_type)buf[i]->type;
		buf_setup.size[i].width = buf[i]->width;
		buf_setup.size[i].height = buf[i]->height;


		buf_setup.input[i].width = buf[i]->input_width;
		buf_setup.input[i].height = buf[i]->input_height;
		buf_setup.input[i].x = buf[i]->input_offset_x;
		buf_setup.input[i].y = buf[i]->input_offset_y;
	}
	printf("--------mb %d %d %d %d\n", buf_setup.input[0].width, buf_setup.input[0].height, buf[0]->input_width, buf[0]->input_height);
	if(ioctl(fd_iav, IAV_IOC_SET_SOURCE_BUFFER_SETUP, &buf_setup)<0){
		perror("IAV_IOC_SET_SOURCE_BUFFER_SETUP");
		return -1;
	}
	return 0;
}

int _init_cfg(int fd_iav)
{
	const char prebuild_board_vin0[] = "prebuild.board.vin0";
	char tmp_prop[AGCMD_PROPERTY_SIZE];
	struct vindev_aaa_info vin_aaa_info;

	agcmd_property_get(prebuild_board_vin0, tmp_prop, "imx178");
	if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
		vin_aaa_info.sensor_id = SENSOR_IMX178;
	} else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
		vin_aaa_info.sensor_id = SENSOR_IMX124;
	} else {
		// vin aaa info
		memset(&vin_aaa_info, 0, sizeof(vin_aaa_info));
		vin_aaa_info.vsrc_id = 0;
		if (ioctl(fd_iav, IAV_IOC_VIN_GET_AAAINFO, &vin_aaa_info) < 0) {
			perror("IAV_IOC_VIN_GET_AAAINFO error\n");
			return -1;
		}
	}

	switch(vin_aaa_info.sensor_id){
		case SENSOR_IMX178:
			{
				pipeline_cfg.enc_mode = 0;
				pipeline_cfg.raw2enc = 0;
				pipeline_cfg.raw_cap = 1;
				pipeline_cfg.raw_max_width = 3072;
				pipeline_cfg.raw_max_height = 2048;
				pipeline_cfg.exposure_num = 1;
				pipeline_cfg.duration = 0;

				//SENSOR_IMX178 = 0x00003009
				strcpy(vin_cfg.sensor_name, "imx178");
				vin_cfg.sensor_id = SENSOR_IMX178;//12297
				//video_mode=0 means auto
				vin_cfg.video_mode = 0;
				vin_cfg.hdr_mode = 0;
				vin_cfg.width	= 3072;
				vin_cfg.height = 2048;
				vin_cfg.fps = 30;
				vin_cfg.bit_resolution = 14;
				//255 means auto
				vin_cfg.mirror = 255;
				vin_cfg.bayer_pattern = 255;

				main_buf_cfg.input_width = 3072;
				main_buf_cfg.input_height = 1728;
				main_buf_cfg.input_offset_x = 0;
				main_buf_cfg.input_offset_y = 160;
				main_buf_cfg.max_width = 3072;
				main_buf_cfg.max_height = 1728;
				main_buf_cfg.width = 3072;
				main_buf_cfg.height = 1728;
				//OFF=0 ENC=1 PREVIEW=2
				main_buf_cfg.type = 1;
			}
			break;
		case SENSOR_IMX124:
			{
				pipeline_cfg.enc_mode = 0;
				pipeline_cfg.raw2enc = 0;
				pipeline_cfg.raw_cap = 1;
				pipeline_cfg.raw_max_width = 2048;
				pipeline_cfg.raw_max_height = 1536;
				pipeline_cfg.exposure_num = 1;
				pipeline_cfg.duration = 0;

				//SENSOR_IMX178 = 0x00003009
				strcpy(vin_cfg.sensor_name, "imx124");
				vin_cfg.sensor_id = SENSOR_IMX124;//12297
				//video_mode=0 means auto
				vin_cfg.video_mode = 0;
				vin_cfg.hdr_mode = 0;
				vin_cfg.width	= 2048;
				vin_cfg.height = 1536;
				vin_cfg.fps = 30;
				vin_cfg.bit_resolution = 12;
				//255 means auto
				vin_cfg.mirror = 255;
				vin_cfg.bayer_pattern = 255;

				main_buf_cfg.input_width = 2048;
				main_buf_cfg.input_height = 1536;
				main_buf_cfg.input_offset_x = 0;
				main_buf_cfg.input_offset_y = 0;
				main_buf_cfg.max_width = 2048;
				main_buf_cfg.max_height = 1536;
				main_buf_cfg.width = 2048;
				main_buf_cfg.height = 1536;
				//OFF=0 ENC=1 PREVIEW=2
				main_buf_cfg.type = 1;
			}
			break;
		default:
			printf("unsupported sensor type\n");
			return -1;
	}



	vout0_cfg.enable = 1;
	vout0_cfg.input = AMBA_VOUT_INPUT_FROM_MIXER;
	vout0_cfg.width = 400;
	vout0_cfg.height = 400;
	vout0_cfg.mode = AMBA_VIDEO_MODE_400_400;
	vout0_cfg.device = AMBA_VOUT_SINK_TYPE_MIPI;
	vout0_cfg.lcd_model_index = -1;

	vout1_cfg.enable = 0;
	vout1_cfg.input = AMBA_VOUT_INPUT_FROM_SMEM;
	vout1_cfg.mode = AMBA_VIDEO_MODE_1080P;
	vout1_cfg.width = 1920;
	vout1_cfg.height = 1080;
	vout1_cfg.device = AMBA_VOUT_SINK_TYPE_HDMI;
	vout1_cfg.lcd_model_index = -1;

	osd_cfg.from_mixer_a = 0;
	osd_cfg.from_mixer_b = 1;

	second_buf_cfg.input_width = 1920;
	second_buf_cfg.input_height = 1080;
	second_buf_cfg.input_offset_x = 0;
	second_buf_cfg.input_offset_y = 0;
	second_buf_cfg.max_width = 720;
	second_buf_cfg.max_height = 480;
	second_buf_cfg.width = 720;
	second_buf_cfg.height = 480;
	second_buf_cfg.type = 1;

	third_buf_cfg.input_width = 1920;
	third_buf_cfg.input_height = 1080;
	third_buf_cfg.input_offset_x = 0;
	third_buf_cfg.input_offset_y = 0;
	third_buf_cfg.max_width = 1920;
	third_buf_cfg.max_height = 1080;
	third_buf_cfg.width = 1920;
	third_buf_cfg.height = 1080;
	third_buf_cfg.type = IAV_SRCBUF_TYPE_OFF;

	fourth_buf_cfg.input_width = 1080;
	fourth_buf_cfg.input_height = 1080;
	fourth_buf_cfg.input_offset_x = 420;
	fourth_buf_cfg.input_offset_y = 0;
	fourth_buf_cfg.max_width = 400;
	fourth_buf_cfg.max_height = 400;
	fourth_buf_cfg.width = 400;
	fourth_buf_cfg.height = 400;
	fourth_buf_cfg.type = IAV_SRCBUF_TYPE_PREVIEW;

	stream_a_cfg.width = 1920;
	stream_a_cfg.height = 1080;
	stream_a_cfg.max_width = 1920;
	stream_a_cfg.max_height = 1080;
	stream_a_cfg.m = 1;
	stream_a_cfg.n = 30;
	stream_a_cfg.bit_rate = 20000000;
	stream_a_cfg.quality = 95;
	stream_a_cfg.h_flip = 0;
	stream_a_cfg.v_flip = 0;
	stream_a_cfg.rotate = 0;
	stream_a_cfg.fps_ratio = 1;
	stream_a_cfg.source = 0;
	//h264 = 1 mjpeg = 2
	stream_a_cfg.type = 1;

	stream_b_cfg.width = 1920;
	stream_b_cfg.height = 1080;
	stream_b_cfg.max_width = 1920;
	stream_b_cfg.max_height = 1080;
	stream_b_cfg.m = 1;
	stream_b_cfg.n = 30;
	stream_b_cfg.bit_rate = 10000000;
	stream_b_cfg.quality = 90;
	stream_b_cfg.h_flip = 0;
	stream_b_cfg.v_flip = 0;
	stream_b_cfg.rotate = 0;
	stream_b_cfg.fps_ratio = 1;
	stream_b_cfg.source = 0;
	stream_b_cfg.type = 1;

	stream_c_cfg.width = 1920;
	stream_c_cfg.height = 1080;
	stream_c_cfg.max_width = 1920;
	stream_c_cfg.max_height = 1080;
	stream_c_cfg.m = 1;
	stream_c_cfg.n = 30;
	stream_c_cfg.bit_rate = 10000000;
	stream_c_cfg.quality = 90;
	stream_c_cfg.h_flip = 0;
	stream_c_cfg.v_flip = 0;
	stream_c_cfg.rotate = 0;
	stream_c_cfg.fps_ratio = 1;
	stream_c_cfg.source = 0;
	stream_c_cfg.type = 1;

	stream_d_cfg.width = 1920;
	stream_d_cfg.height = 1080;
	stream_d_cfg.max_width = 1920;
	stream_d_cfg.max_height = 1080;
	stream_d_cfg.m = 1;
	stream_d_cfg.n = 30;
	stream_d_cfg.bit_rate = 10000000;
	stream_d_cfg.quality = 90;
	stream_d_cfg.h_flip = 0;
	stream_d_cfg.v_flip = 0;
	stream_d_cfg.rotate = 0;
	stream_d_cfg.fps_ratio = 1;
	stream_d_cfg.source = 0;
	stream_d_cfg.type = 1;

	return 0;
}

int calib_enter_preview(int fd_iav)
{

	if(_init_cfg(fd_iav)<0){
		printf("init cfg fail\n");
		return -1;
	}

	if(ioctl(fd_iav, IAV_IOC_ENTER_IDLE)<0){
		perror("IAV_IOC_ENTER_IDLE");
	}
	printf("go to idle done\n");

	if(vout0_cfg.enable) {
		cfg_init_vout(fd_iav, 0, 0, &vout0_cfg);
	}
	if(vout1_cfg.enable) {
		cfg_init_vout(fd_iav, 1, 0, &vout1_cfg);
	}

	cfg_init_vin(fd_iav, &vin_cfg);

	buffer_t* bufcfg[4];
	bufcfg[0] = &main_buf_cfg;
	bufcfg[1] = &second_buf_cfg;
	bufcfg[2] = &third_buf_cfg;
	bufcfg[3] = &fourth_buf_cfg;

	stream_t* streamcfg[4];
	streamcfg[0] = &stream_a_cfg;
	streamcfg[1] = &stream_b_cfg;
	streamcfg[2] = &stream_c_cfg;
	streamcfg[3] = &stream_d_cfg;

	if(cfg_setup_resource_limit(fd_iav, &pipeline_cfg,
		&vout0_cfg, &vout1_cfg, &osd_cfg, bufcfg, streamcfg) < 0){
		printf("setup resource failed\n");
		return -1;
	}
	cfg_setup_source_buffer(fd_iav, bufcfg);

	ioctl(fd_iav, IAV_IOC_ENABLE_PREVIEW, 31);

	if(vout0_cfg.enable) {
		cfg_vout_brightness(fd_iav, 0, &vout0_cfg, 255);
	}

	return 0;
}

