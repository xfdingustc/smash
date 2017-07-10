
#define LOG_TAG "avf_plat_config_s2l"

#include <fcntl.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <alsa/asoundlib.h>

#include <basetypes.h>
#include <iav_ioctl.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_util.h"
#include "avf_plat_config.h"

#include "avf_plat_config_vout.cpp"

static CMutex g_plat_lock;
static vout_info_t g_vout_info[NUM_VOUT] = {
	// VOUT_0, LCD
	{0,	},

	// VOUT_1, HDMI
	{0,	},
};

static int s2l_open_iav(void)
{
	int iav_fd;

	iav_fd = avf_open_file("/dev/iav", O_RDWR, 0);
	if (iav_fd < 0) {
		AVF_LOGP("/dev/iav");
	}

	return iav_fd;
}

static int s2l_vout_get_sink_id(int iav_fd, int sink_type, int source_id)
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

	for (i = num - 1; i >= 0; i--) {
		struct amba_vout_sink_info sink_info;

		::memset(&sink_info, 0, sizeof(sink_info));
		sink_info.id = i;

		if (::ioctl(iav_fd, IAV_IOC_VOUT_GET_SINK_INFO, &sink_info) < 0) {
			AVF_LOGP("IAV_IOC_VOUT_GET_SINK_INFO");
			return -1;
		}

		if (sink_info.sink_type == sink_type && sink_info.source_id == source_id) {
			//printf("sink_type: %d, source_id: %d, id: %d\n", sink_type, source_id, sink_info.id);
			return sink_info.id;
		}
	}

	AVF_LOGE("No such vout sink, sink_type=%d, source_id=%d",
		sink_type, source_id);
	return -1;
}

static int s2l_get_hdmi_native_size(int iav_fd, int sink_id, int *width, int *height)
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

static int s2l_get_flip(int avf_flip)
{
	switch (avf_flip) {
	case VOUT_FLIP_NORMAL: return AMBA_VOUT_FLIP_NORMAL;
	case VOUT_FLIP_HV: return AMBA_VOUT_FLIP_HV;
	case VOUT_FLIP_H: return AMBA_VOUT_FLIP_HORIZONTAL;
	case VOUT_FLIP_V: return AMBA_VOUT_FLIP_VERTICAL;
	default: return -1;
	}
}

static int s2l_get_rotate(int avf_rotate)
{
	switch (avf_rotate) {
	case VOUT_ROTATE_NORMAL: return AMBA_VOUT_ROTATE_NORMAL;
	case VOUT_ROTATE_90: return AMBA_VOUT_ROTATE_90;
	default: return -1;
	}
}

static int s2l_get_sink_type(int vout_type)
{
	switch (vout_type) {
	case VOUT_TYPE_AUTO: return AMBA_VOUT_SINK_TYPE_AUTO;
	case VOUT_TYPE_CVBS: return AMBA_VOUT_SINK_TYPE_CVBS;
	case VOUT_TYPE_SVIDEO: return AMBA_VOUT_SINK_TYPE_SVIDEO;
	case VOUT_TYPE_YPbPr: return AMBA_VOUT_SINK_TYPE_YPBPR;
	case VOUT_TYPE_HDMI: return AMBA_VOUT_SINK_TYPE_HDMI;
	case VOUT_TYPE_DIGITAL: return AMBA_VOUT_SINK_TYPE_DIGITAL;
	case VOUT_TYPE_MIPI: return AMBA_VOUT_SINK_TYPE_MIPI;
	default:
		AVF_LOGE("bad dev_type %d", vout_type);
		return -1;
	}
}

static int s2l_config_vout(int iav_fd,
	vout_config_t *config, const char *lcd_model)
{
	int vout_mode_index = config->vout_mode;
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
	if ((sink_type = s2l_get_sink_type(config->vout_type)) < 0)
		return -1;

	// sink id
	sink_id = s2l_vout_get_sink_id(iav_fd, sink_type, config->vout_id);
	if (sink_id < 0)
		return -1;

	if (::ioctl(iav_fd, IAV_IOC_VOUT_SELECT_DEV, sink_id) < 0) {
		AVF_LOGP("IAV_IOC_VOUT_SELECT_DEV");
		return -1;
	}

	// vout mode
	if ((unsigned)vout_mode_index >= ARRAY_SIZE(g_vout_modes)) {
		AVF_LOGE("invalid vout_mode: %d", vout_mode_index);
		return -1;
	}

	::memset(&sink_cfg, 0, sizeof(sink_cfg));

	//---------------------------------------------------------------
	// sink
	//---------------------------------------------------------------
	sink_cfg.id = sink_id;
	sink_cfg.mode = g_vout_modes[vout_mode_index].mode;
	sink_cfg.ratio = AMBA_VIDEO_RATIO_AUTO;
	sink_cfg.bits = AMBA_VIDEO_BITS_AUTO;
	sink_cfg.type = AMBA_VIDEO_TYPE_AUTO;
	sink_cfg.format = g_vout_modes[vout_mode_index].format;
	sink_cfg.frame_rate = config->fps ? AMBA_VIDEO_FPS(config->fps) : AMBA_VIDEO_FPS_AUTO;
	sink_cfg.csc_en = !config->disable_csc;
	sink_cfg.bg_color.y = 0x10;
	sink_cfg.bg_color.cb = 0x80;
	sink_cfg.bg_color.cr = 0x80;
	sink_cfg.display_input = AMBA_VOUT_INPUT_FROM_MIXER; // todo
	sink_cfg.sink_type = AMBA_VOUT_SINK_TYPE_AUTO;

	//---------------------------------------------------------------
	// video
	//---------------------------------------------------------------
	sink_cfg.video_en = config->enable_video;

	sink_cfg.video_size.specified = config->video_size_flag;
	if (vout_mode_index == VOUT_MODE_native) {
		int width, height;
		if (s2l_get_hdmi_native_size(iav_fd, sink_id, &width, &height) < 0)
			return -1;
		sink_cfg.video_size.vout_width = width;
		sink_cfg.video_size.vout_height = height;
	} else {
		sink_cfg.video_size.vout_width = g_vout_modes[vout_mode_index].width;
		sink_cfg.video_size.vout_height = g_vout_modes[vout_mode_index].height;
	}
	if (config->video_size_flag) {
		sink_cfg.video_size.video_width = config->video_width;
		sink_cfg.video_size.video_height = config->video_height;
	} else {
		sink_cfg.video_size.video_width = sink_cfg.video_size.vout_width;
		sink_cfg.video_size.video_height = sink_cfg.video_size.vout_height;
	}

	sink_cfg.video_offset.specified = config->video_offset_flag;
	sink_cfg.video_offset.offset_x = config->video_offset_flag ? config->video_offset_x : 0;
	sink_cfg.video_offset.offset_y = config->video_offset_flag ? config->video_offset_y : 0;

	video_flip = s2l_get_flip(config->video_flip);
	if (video_flip < 0) {
		AVF_LOGE("invalid video_flip: %d", config->video_flip);
		return -1;
	}
	sink_cfg.video_flip = (enum amba_vout_flip_info)video_flip;

	video_rotate = s2l_get_rotate(config->video_rotate);
	if (video_rotate < 0) {
		AVF_LOGE("invalid video_rotate: %d", config->video_rotate);
		return -1;
	}
	sink_cfg.video_rotate = (enum amba_vout_rotate_info)video_rotate;

	//---------------------------------------------------------------
	// osd
	//---------------------------------------------------------------
	sink_cfg.fb_id = config->fb_id;

	//sink_cfg.osd_size

	sink_cfg.osd_rescale.enable = config->osd_rescale_flag;
	sink_cfg.osd_rescale.width = config->osd_rescale_width;
	sink_cfg.osd_rescale.height = config->osd_rescale_height;

	sink_cfg.osd_offset.specified = config->osd_offset_flag;
	sink_cfg.osd_offset.offset_x = config->osd_offset_flag ? config->osd_offset_x : 0;
	sink_cfg.osd_offset.offset_y = config->osd_offset_flag ? config->osd_offset_y : 0;

	osd_flip = s2l_get_flip(config->osd_flip);
	if (osd_flip < 0) {
		AVF_LOGE("invalid osd_flip: %d", config->osd_flip);
		return -1;
	}
	sink_cfg.osd_flip = (enum amba_vout_flip_info)osd_flip;

	osd_rotate = s2l_get_rotate(config->osd_rotate);
	if (osd_rotate < 0) {
		AVF_LOGE("invalid osd_rotate: %d", config->osd_rotate);
		return -1;
	}
	sink_cfg.osd_rotate = (enum amba_vout_rotate_info)osd_rotate;

	if (config->enable_osd_copy) {
		sink_cfg.osd_tailor = (enum amba_vout_tailored_info)
			(AMBA_VOUT_OSD_NO_CSC | AMBA_VOUT_OSD_AUTO_COPY);
	}

	//---------------------------------------------------------------
	// misc
	//---------------------------------------------------------------

	sink_cfg.direct_to_dsp = 0;	// todo
	//sink_cfg.lcd_cfg

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
	sink_cfg.hdmi_color_space = (enum amba_vout_hdmi_color_space)hdmi_cs;

	sink_cfg.hdmi_3d_structure = DDD_RESERVED;	// todo
	sink_cfg.hdmi_overscan = AMBA_VOUT_HDMI_OVERSCAN_AUTO;	// todo

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

	return 0;
}

int avf_platform_display_set_mode(int mode)
{
	int iav_fd;
	int rval;

	AUTO_LOCK(g_plat_lock);

	iav_fd = s2l_open_iav();
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
		//rval = ::ioctl(iav_fd, IAV_IOC_START_DECODE, 0);
		//if (rval < 0) {
		//	AVF_LOGP("IAV_IOC_START_DECODE");
		//}
		rval = -1;
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
	int iav_fd;
	int rval;

	AUTO_LOCK(g_plat_lock);

	iav_fd = s2l_open_iav();
	if (iav_fd < 0)
		return -1;

	AVF_LOGD("config vout %d", config->vout_id);
	rval = s2l_config_vout(iav_fd, config, lcd_model);

	avf_close_file(iav_fd);
	return rval;
}

int avf_platform_display_disable_vout(int vout_id)
{
	AUTO_LOCK(g_plat_lock);

	int iav_fd = s2l_open_iav();
	int rval;

	if (iav_fd < 0 || vout_id >= NUM_VOUT)
		return -1;

	AVF_LOGD("disable vout %d", vout_id);
	rval = ::ioctl(iav_fd, IAV_IOC_VOUT_HALT, vout_id);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_VOUT_HALT");
	}

	g_vout_info[vout_id].enabled = 0;

	avf_close_file(iav_fd);
	return rval;
}

int avf_platform_display_set_backlight(int vout_id, const char *lcd_model, int on)
{
	return 0;
}

int s2l_display_prepare_sink_brightness(int iav_fd, const vout_brightness_t *param,
	struct iav_vout_sink_brightness_s *sink_brightness)
{
	int sink_type;
	int sink_id;

	if (param->vout_id >= NUM_VOUT) {
		AVF_LOGE("bad vout_id: %d", param->vout_id);
		return -1;
	}

	if ((sink_type = s2l_get_sink_type(param->vout_type)) < 0)
		return -1;

	sink_id = s2l_vout_get_sink_id(iav_fd, sink_type, param->vout_id);
	if (sink_id < 0)
		return -1;

	sink_brightness->id = sink_id;
	sink_brightness->brightness = param->brightness;

	return 0;
}

int s2l_display_set_brightness(int iav_fd, const vout_brightness_t *param)
{
	struct iav_vout_sink_brightness_s sink_brightness;
	int ret_val;

	ret_val = s2l_display_prepare_sink_brightness(iav_fd, param, &sink_brightness);
	if (ret_val < 0) {
		return ret_val;
	}

	AVF_LOGD("set brightness: %d", sink_brightness.brightness);

	ret_val = ::ioctl(iav_fd, IAV_IOC_VOUT_SET_SINK_BRIGHTNESS, &sink_brightness);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_VOUT_SET_SINK_BRIGHTNESS");
		return ret_val;
	}

	return ret_val;
}

int s2l_display_get_brightness(int iav_fd, vout_brightness_t *param)
{
	struct iav_vout_sink_brightness_s sink_brightness;
	int ret_val;

	ret_val = s2l_display_prepare_sink_brightness(iav_fd, param, &sink_brightness);
	if (ret_val < 0) {
		return ret_val;
	}

	ret_val = ::ioctl(iav_fd, IAV_IOC_VOUT_GET_SINK_BRIGHTNESS, &sink_brightness);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_VOUT_GET_SINK_BRIGHTNESS");
		return ret_val;
	}

	param->brightness = sink_brightness.brightness;

	return 0;
}

int s2l_flip_vout_video(int iav_fd, int vout_id, int vout_type, int flip)
{
	int sink_type;
//	int sink_id;
	int ret_val;

	// dev type
	if ((sink_type = s2l_get_sink_type(vout_type)) < 0)
		return -1;

	// sink id
//	sink_id = s2l_vout_get_sink_id(iav_fd, sink_type, vout_id);
//	if (sink_id < 0)
//		return -1;

	struct iav_vout_flip_video_s flip_video;
	flip_video.vout_id = vout_id;
	flip_video.flip = s2l_get_flip(flip);

	ret_val = ::ioctl(iav_fd, IAV_IOC_VOUT_FLIP_VIDEO, &flip_video);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_VOUT_FLIP_VIDEO");
		return ret_val;
	}

	return 0;
}

int s2l_flip_vout_osd(int iav_fd, int vout_id, int vout_type, int flip)
{
	int sink_type;
//	int sink_id;
	int ret_val;

	// dev type
	if ((sink_type = s2l_get_sink_type(vout_type)) < 0)
		return -1;

	// sink id
//	sink_id = s2l_vout_get_sink_id(iav_fd, sink_type, vout_id);
//	if (sink_id < 0)
//		return -1;

	struct iav_vout_flip_osd_s flip_osd;
	flip_osd.vout_id = vout_id;
	flip_osd.flip = s2l_get_flip(flip);

	ret_val = ::ioctl(iav_fd, IAV_IOC_VOUT_FLIP_OSD, &flip_osd);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_VOUT_FLIP_OSD");
		return ret_val;
	}

	return 0;
}

int avf_platform_display_set_brightness(const vout_brightness_t *param)
{
	int iav_fd = s2l_open_iav();
	int rval;

	if (iav_fd < 0)
		return -1;

	rval = s2l_display_set_brightness(iav_fd, param);
	avf_close_file(iav_fd);

	return rval;
}

int avf_platform_display_get_brightness(vout_brightness_t *param)
{
	int iav_fd = s2l_open_iav();
	int rval;

	if (iav_fd < 0)
		return -1;

	rval = s2l_display_get_brightness(iav_fd, param);
	avf_close_file(iav_fd);

	return rval < 0 ? rval : param->brightness;
}

int avf_platform_display_change_vout_video(int iav_fd, int vout_id, int vout_type,
	int xoff, int yoff, int width, int height, vout_info_t *vout_info)
{
	if (vout_id < 0 || vout_id >= NUM_VOUT)
		return -1;

	AUTO_LOCK(g_plat_lock);

	iav_vout_change_preview_video_t cpv;
	int rval;

	::memset(&cpv, 0, sizeof(cpv));

	cpv.vout_id = vout_id;
	cpv.video_x = width;
	cpv.video_y = height;
	cpv.video_offset_x = xoff;
	cpv.video_offset_y = yoff;

	rval = ::ioctl(iav_fd, IAV_IOC_VOUT_CHANGE_PREVIEW_VIDEO, &cpv);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_VOUT_CHANGE_PREVIEW_VIDEO");
		return rval;
	}

	// update
	vout_info_t *info = g_vout_info + vout_id;
	info->video_width = width;
	info->video_height = height;
	info->video_xoff = xoff;
	info->video_yoff = yoff;

	*vout_info = *info;

	return 0;
}

int avf_platform_display_flip_vout_video(int vout_id, int vout_type, int flip)
{
	int iav_fd = s2l_open_iav();
	int rval;

	if (iav_fd < 0)
		return -1;

	rval = s2l_flip_vout_video(iav_fd, vout_id, vout_type, flip);
	avf_close_file(iav_fd);

//	if (rval == 0) {
//		g_vout_info[vout_id].enabled = 0;
//	}

	return rval;
}

int avf_platform_display_flip_vout_osd(int vout_id, int vout_type, int flip)
{
	int iav_fd = s2l_open_iav();
	int rval;

	if (iav_fd < 0)
		return -1;

	rval = s2l_flip_vout_osd(iav_fd, vout_id, vout_type, flip);
	avf_close_file(iav_fd);

//	if (rval == 0) {
//		g_vout_info[vout_id].enabled = 0;
//	}

	return rval;
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
	AVF_LOGD("set clock mode to [" C_LIGHT_PURPLE "%s" C_NONE "]", clock_mode);

	int fd = avf_open_file("/proc/ambarella/mode", O_WRONLY, 0);
	if (fd < 0) {
		AVF_LOGP("/proc/ambarella/mode");
		return -1;
	}

	int ret_val = ::write(fd, clock_mode, ::strlen(clock_mode));
	if (ret_val < 0) {
		AVF_LOGP("write");
	}

	avf_close_file(fd);

	return ret_val;
}

int avf_platform_init(void)
{
	return 0;
}

int avf_platform_cleanup(void)
{
	return 0;
}

