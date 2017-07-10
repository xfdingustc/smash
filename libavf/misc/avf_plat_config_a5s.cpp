
#define LOG_TAG "avf_plat_config"

#include <fcntl.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <alsa/asoundlib.h>

#include "types.h"
#include "ambas_common.h"
#include "ambas_vout.h"
#include "iav_drv.h"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_util.h"
#include "avf_plat_config.h"

#include "avf_plat_config_vout.cpp"

static vout_info_t g_vout_info[NUM_VOUT] = {
	// VOUT_0, LCD
	{0,	},

	// VOUT_1, HDMI
	{0,	},
};

static CMutex g_plat_lock;

#include "lcd/lcd_api.h"
#include "lcd/lcd_digital.c"
#include "lcd/lcd_digital601.c"
#include "lcd/lcd_auo27.c"
#include "lcd/lcd_p28kls03.c"
#include "lcd/lcd_tpo489.c"
#include "lcd/lcd_tpo648.c"
#include "lcd/lcd_td043.c"
#include "lcd/lcd_ppga3.c"
#include "lcd/lcd_wdf2440.c"
//#include "lcd/lcd_1p3831.c"
#include "lcd/lcd_1p3828.c"
#include "lcd/lcd_ej080na.c"
#include "lcd/lcd_at070tna2.c"
//#include "lcd/lcd_e330qhd.c"
#include "lcd/lcd_t15p00.c"

typedef struct lcd_model_s {
	const char *model;
	LCD_SETMODE_FUNC lcd_setmode;
	LCD_POST_SETMODE_FUNC lcd_post_setmode;
	LCD_SET_BACKLIGHT_FUNC lcd_set_backlight;
} lcd_model_t;

static const lcd_model_t g_lcd_model_list[] = {
	{"digital", lcd_digital_setmode, NULL},
	{"digital601", lcd_digital601_setmode, NULL},
	{"auo27", lcd_auo27_setmode, NULL},
	{"p28k", lcd_p28kls03_setmode, NULL},
	{"tpo489", lcd_tpo489_setmode, NULL},
	{"tpo648", lcd_tpo648_setmode, NULL},
	{"td043", lcd_td043_setmode, NULL},
	{"ppga3", lcd_ppga3_setmode, NULL},
	{"wdf2440", lcd_wdf2440_setmode, lcd_wdf2440_post_setmode},
//	{"1p3831", lcd_1p3831_setmode, NULL},
	{"1p3828", lcd_1p3828_setmode, NULL},
	{"ej080na", lcd_ej080na_setmode, lcd_ej080na_post_setmode},
	{"at070tna2", lcd_at070tna2_setmode, NULL},
//	{"e330qhd", lcos_e330qhd_setmode, NULL},
	{"t15p00", lcd_t15p00_setmode, NULL, lcd_t15p00_set_backlight},
};

int open_iav(void)
{
	int iav_fd = avf_open_file("/dev/iav", O_RDWR, 0);
	if (iav_fd < 0) {
		AVF_LOGP("/dev/iav");
	}
	return iav_fd;
}

int vout_get_sink_id(int iav_fd, int sink_type)
{
	int num;
	int i;

	if (::ioctl(iav_fd, IAV_IOC_VOUT_GET_SINK_NUM, &num) < 0) {
		AVF_LOGP("IAV_IOC_VOUT_GET_SINK_NUM");
		return -1;
	}

	if (num < 1) {
		AVF_LOGE("No vout. Please load vout driver!");
		return -1;
	}

	for (i = 0; i < num; i++) {
		struct amba_vout_sink_info sink_info;
		sink_info.id = i;
		if (::ioctl(iav_fd, IAV_IOC_VOUT_GET_SINK_INFO, &sink_info) < 0) {
			AVF_LOGP("IAV_IOC_VOUT_GET_SINK_INFO");
			return -1;
		}

		if (sink_info.sink_type == sink_type)
			return sink_info.id;
	}

	AVF_LOGE("No such vout sink, sink_type=%d", sink_type);
	return -1;
}

int get_lcd_model_index(const char *lcd_model)
{
	unsigned i;

	for (i = 0; i < sizeof(g_lcd_model_list)/sizeof(g_lcd_model_list[0]); i++) {
		if (strcmp(g_lcd_model_list[i].model, lcd_model) == 0)
			return i;
	}

	return -1;
}

int get_hdmi_native_size(int iav_fd, int sink_id, int *width, int *height)
{
	struct amba_vout_sink_info sink_info;

	sink_info.id = sink_id;
	if (::ioctl(iav_fd, IAV_IOC_VOUT_GET_SINK_INFO, &sink_info) < 0) {
		AVF_LOGP("IAV_IOC_VOUT_GET_SINK_INFO");
		return -1;
	}

	if (sink_info.hdmi_native_mode != AMBA_VIDEO_MODE_HDMI_NATIVE) {
		AVF_LOGP("Not native mode");
		return -1;
	}

	*width = sink_info.hdmi_native_width;
	*height = sink_info.hdmi_native_height;

	return 0;
}

int get_flip(int avf_flip)
{
	switch (avf_flip) {
	case VOUT_FLIP_NORMAL: return AMBA_VOUT_FLIP_NORMAL;
	case VOUT_FLIP_HV: return AMBA_VOUT_FLIP_HV;
	case VOUT_FLIP_H: return AMBA_VOUT_FLIP_HORIZONTAL;
	case VOUT_FLIP_V: return AMBA_VOUT_FLIP_VERTICAL;
	default: return -1;
	}
}

int get_rotate(int avf_rotate)
{
	switch (avf_rotate) {
	case VOUT_ROTATE_NORMAL: return AMBA_VOUT_ROTATE_NORMAL;
	case VOUT_ROTATE_90: return AMBA_VOUT_ROTATE_90;
	default: return -1;
	}
}

int config_vout(int iav_fd, vout_config_t *config, const char *lcd_model)
{
	int vout_mode_index = config->vout_mode;
	int lcd_model_index = -1;
	int sink_type;
	int sink_id;
	int hdmi_cs;
	int video_flip;
	int video_rotate;
	int osd_flip;
	int osd_rotate;

	struct amba_video_sink_mode sink_cfg;

	if (config->vout_id >= NUM_VOUT) {
		AVF_LOGE("bad vout_id %d", config->vout_id);
		return -1;
	}

	// dev type
	switch (config->vout_type) {
	case VOUT_TYPE_AUTO: sink_type = AMBA_VOUT_SINK_TYPE_AUTO; break;
	case VOUT_TYPE_CVBS: sink_type = AMBA_VOUT_SINK_TYPE_CVBS; break;
	case VOUT_TYPE_SVIDEO: sink_type = AMBA_VOUT_SINK_TYPE_SVIDEO; break;
	case VOUT_TYPE_YPbPr: sink_type = AMBA_VOUT_SINK_TYPE_YPBPR; break;
	case VOUT_TYPE_HDMI: sink_type = AMBA_VOUT_SINK_TYPE_HDMI; break;
	case VOUT_TYPE_DIGITAL: sink_type = AMBA_VOUT_SINK_TYPE_DIGITAL; break;
	case VOUT_TYPE_MIPI: sink_type = AMBA_VOUT_SINK_TYPE_MIPI; break;
	default:
		AVF_LOGE("bad dev_type %d", config->vout_type);
		return -1;
	}

	if (sink_type == AMBA_VOUT_SINK_TYPE_DIGITAL) {
		lcd_model_index = get_lcd_model_index(lcd_model);
		if (lcd_model_index < 0)
			return -1;
	}

	// sink id
	sink_id = vout_get_sink_id(iav_fd, sink_type);
	if (sink_id < 0)
		return -1;

	if (::ioctl(iav_fd, IAV_IOC_VOUT_SELECT_DEV, sink_id) < 0) {
		AVF_LOGP("IAV_IOC_VOUT_SELECT_DEV");
		return -1;
	}

	// vout mode
	if ((unsigned)vout_mode_index >= sizeof(g_vout_modes)/sizeof(g_vout_modes[0])) {
		AVF_LOGE("invalid vout_mode: %d", vout_mode_index);
		return -1;
	}

	// config vout
	::memset(&sink_cfg, 0, sizeof(sink_cfg));
	if (sink_type == AMBA_VOUT_SINK_TYPE_DIGITAL) {
		if (g_lcd_model_list[lcd_model_index].lcd_setmode(
			g_vout_modes[vout_mode_index].mode, &sink_cfg) < 0) {
			AVF_LOGE("lcd_setmode failed");
			return -1;
		}
	} else {
		sink_cfg.mode = g_vout_modes[vout_mode_index].mode;
		sink_cfg.ratio = AMBA_VIDEO_RATIO_AUTO;
		sink_cfg.bits = AMBA_VIDEO_BITS_AUTO;
		sink_cfg.type = AMBA_VIDEO_TYPE_AUTO;
		sink_cfg.format = g_vout_modes[vout_mode_index].format;
		sink_cfg.sink_type = AMBA_VOUT_SINK_TYPE_AUTO;
		sink_cfg.bg_color.y = 0x10;
		sink_cfg.bg_color.cb = 0x80;
		sink_cfg.bg_color.cr = 0x80;
		sink_cfg.lcd_cfg.mode = AMBA_VOUT_LCD_MODE_DISABLE;
	}

	if (config->disable_csc) {
		sink_cfg.bg_color.y = 0;
		sink_cfg.bg_color.cb = 0;
		sink_cfg.bg_color.cr = 0;
	}

	if (config->enable_osd_copy) {
		sink_cfg.osd_tailor = (enum amba_vout_tailored_info)
			(AMBA_VOUT_OSD_NO_CSC | AMBA_VOUT_OSD_AUTO_COPY);
	}

	// HDMI color space
	switch (config->hdmi_color_space) {
	case HDMI_CS_AUTO: hdmi_cs = AMBA_VOUT_HDMI_CS_AUTO; break;
	case HDMI_CS_RGB: hdmi_cs = AMBA_VOUT_HDMI_CS_RGB; break;
	case HDMI_CS_YCBCR_444: hdmi_cs = AMBA_VOUT_HDMI_CS_YCBCR_444; break;
	case HDMI_CS_YCBCR_422: hdmi_cs = AMBA_VOUT_HDMI_CS_YCBCR_422; break;
	default:
		AVF_LOGE("invalid hdmi_color_space: %d", config->hdmi_color_space);
		return -1;
	}

	// video flip
	video_flip = get_flip(config->video_flip);
	if (video_flip < 0) {
		AVF_LOGE("invalid video_flip: %d", config->video_flip);
		return -1;
	}

	// video rotate
	video_rotate = get_rotate(config->video_rotate);
	if (video_rotate < 0) {
		AVF_LOGE("invalid video_rotate: %d", config->video_rotate);
		return -1;
	}

	sink_cfg.id = sink_id;
	sink_cfg.frame_rate = config->fps ? AMBA_VIDEO_FPS(config->fps) : AMBA_VIDEO_FPS_AUTO;
	sink_cfg.csc_en = !config->disable_csc;
	sink_cfg.hdmi_color_space = (enum amba_vout_hdmi_color_space)hdmi_cs;
	sink_cfg.hdmi_3d_structure = DDD_RESERVED;	// todo
	sink_cfg.hdmi_overscan = AMBA_VOUT_HDMI_OVERSCAN_AUTO;	// todo
	sink_cfg.video_en = config->enable_video;
	sink_cfg.video_flip = (enum amba_vout_flip_info)video_flip;
	sink_cfg.video_rotate = (enum amba_vout_rotate_info)video_rotate;
	sink_cfg.fb_id = config->fb_id;

	if (vout_mode_index == VOUT_MODE_native) {
		int width, height;
		if (get_hdmi_native_size(iav_fd, sink_id, &width, &height) < 0)
			return -1;
		sink_cfg.video_size.vout_width = width;
		sink_cfg.video_size.vout_height = height;
	} else {
		sink_cfg.video_size.vout_width = g_vout_modes[vout_mode_index].width;
		sink_cfg.video_size.vout_height = g_vout_modes[vout_mode_index].height;
	}

	sink_cfg.video_size.specified = config->video_size_flag;
	if (sink_cfg.video_size.specified) {
		sink_cfg.video_size.video_width = config->video_width;
		sink_cfg.video_size.video_height = config->video_height;
	} else {
		sink_cfg.video_size.video_width = sink_cfg.video_size.vout_width;
		sink_cfg.video_size.video_height = sink_cfg.video_size.vout_height;
	}

	sink_cfg.video_offset.specified = config->video_offset_flag;
	sink_cfg.video_offset.offset_x = config->video_offset_flag ? config->video_offset_x : 0;
	sink_cfg.video_offset.offset_y = config->video_offset_flag ? config->video_offset_y : 0;

	osd_flip = get_flip(config->osd_flip);
	if (osd_flip < 0) {
		AVF_LOGE("invalid osd_flip: %d", config->osd_flip);
		return -1;
	}

	osd_rotate = get_rotate(config->osd_rotate);
	if (osd_rotate < 0) {
		AVF_LOGE("invalid osd_rotate: %d", config->osd_rotate);
		return -1;
	}

	sink_cfg.osd_flip = (enum amba_vout_flip_info)osd_flip;
	sink_cfg.osd_rotate = (enum amba_vout_rotate_info)osd_rotate;

	sink_cfg.osd_rescale.enable = config->osd_rescale_flag;
	sink_cfg.osd_rescale.width = config->osd_rescale_width;
	sink_cfg.osd_rescale.height = config->osd_rescale_height;

	sink_cfg.osd_offset.specified = config->osd_offset_flag;
	sink_cfg.osd_offset.offset_x = config->osd_offset_flag ? config->osd_offset_x : 0;
	sink_cfg.osd_offset.offset_y = config->osd_offset_flag ? config->osd_offset_y : 0;

	sink_cfg.display_input = AMBA_VOUT_INPUT_FROM_MIXER; // todo
	sink_cfg.direct_to_dsp = 0;	// todo

	if (::ioctl(iav_fd, IAV_IOC_VOUT_CONFIGURE_SINK, &sink_cfg) < 0) {
		AVF_LOGP("IAV_IOC_VOUT_CONFIGURE_SINK");
		return -1;
	}

	g_vout_info[config->vout_id].enabled = 1;
	g_vout_info[config->vout_id].vout_width = sink_cfg.video_size.vout_width;
	g_vout_info[config->vout_id].vout_height = sink_cfg.video_size.vout_height;
	g_vout_info[config->vout_id].video_width = sink_cfg.video_size.video_width;
	g_vout_info[config->vout_id].video_height = sink_cfg.video_size.video_height;
	g_vout_info[config->vout_id].video_xoff = sink_cfg.video_offset.offset_x;
	g_vout_info[config->vout_id].video_yoff = sink_cfg.video_offset.offset_y;

	// lcd post setmode
	if (sink_type == AMBA_VOUT_SINK_TYPE_DIGITAL) {
		if (g_lcd_model_list[lcd_model_index].lcd_post_setmode) {
			if (g_lcd_model_list[lcd_model_index].lcd_post_setmode(
				g_vout_modes[vout_mode_index].mode) < 0) {
				AVF_LOGE("lcd_post_setmode failed");
				//return -1;
			}
		}
	}

	return 0;
}

int avf_platform_display_set_mode(int mode)
{
	AUTO_LOCK(g_plat_lock);

	int iav_fd = open_iav();
	int rval;

	if (iav_fd < 0)
		return -1;

	switch (mode) {
	case MODE_IDLE:
		rval = ::ioctl(iav_fd, IAV_IOC_ENTER_IDLE, 0);
		if (rval < 0) {
			AVF_LOGP("IAV_IOC_ENTER_IDLE");
		}
		break;

	case MODE_DECODE:
		rval = ::ioctl(iav_fd, IAV_IOC_START_DECODE, 0);
		if (rval < 0) {
			AVF_LOGP("IAV_IOC_START_DECODE");
		}
		break;

	default:
		AVF_LOGE("bad mode %d", mode);
		rval = -1;
		break;
	}

	avf_close_file(iav_fd);
	return rval;
}

int avf_platform_display_set_vout(vout_config_t *config, const char *lcd_model)
{
	AUTO_LOCK(g_plat_lock);

	int iav_fd = open_iav();
	int rval;

	if (iav_fd < 0)
		return -1;

	AVF_LOGD("config vout %d", config->vout_id);
	rval = config_vout(iav_fd, config, lcd_model);

	avf_close_file(iav_fd);
	return rval;
}

int avf_platform_display_disable_vout(int vout_id)
{
	AUTO_LOCK(g_plat_lock);

	int iav_fd = open_iav();
	int rval;

	if (iav_fd < 0 || vout_id >= NUM_VOUT)
		return -1;

	AVF_LOGD("disable vout %d", vout_id);
	rval = ioctl(iav_fd, IAV_IOC_VOUT_HALT, vout_id);

	g_vout_info[vout_id].enabled = 0;

	avf_close_file(iav_fd);
	return rval;
}

int avf_platform_display_set_backlight(int vout_id, const char *lcd_model, int on)
{
	int rval = -1;

	AUTO_LOCK(g_plat_lock);

	// vout_id not used

	int lcd_model_index = get_lcd_model_index(lcd_model);
	if (lcd_model_index < 0)
		return -1;

	if (g_lcd_model_list[lcd_model_index].lcd_set_backlight) {
		rval = g_lcd_model_list[lcd_model_index].lcd_set_backlight(on);
	}

	return rval;
}

int avf_platform_display_set_brightness(const vout_brightness_t *param)
{
	AVF_LOGW("set_brightness not supported on A5s");
	return -1;
}

int avf_platform_display_get_brightness(vout_brightness_t *param)
{
	AVF_LOGW("get_brightness not supported on A5s");
	return -1;
}

int avf_platform_display_change_vout_video(int iav_fd, int vout_id, int vout_type,
	int xoff, int yoff, int width, int height, vout_info_t *vout_info)
{
	AVF_LOGW("change_vout_video not supported on A5s");
	return -1;
}

int avf_platform_display_get_vout_info(int vout_id, vout_info_t *info)
{
	AUTO_LOCK(g_plat_lock);

	if (vout_id < 0 || vout_id >= NUM_VOUT)
		return -1;

	*info = g_vout_info[vout_id];
	return 0;
}

int avf_platform_set_clock_mode(const char *clock_mode)
{
	return -1;
}

int avf_platform_init(void)
{
	return 0;
}

int avf_platform_cleanup(void)
{
	return 0;
}

