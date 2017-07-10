
#define LOG_TAG "amba_vsrc"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <basetypes.h>
#include <iav_ioctl.h>

#include "avf_common.h"
#include "avf_util.h"
#include "avf_new.h"

#include "amba_vsrc.h"
#include "amba_vsrc_priv.h"

#include "avf_plat_config.h"

#include "amba_vsrc_util.cpp"

#define USE_HW_PTS

/*===========================================================================*/

typedef struct vin_s2l_config_s {
	uint32_t				source_id;
	uint32_t				video_mode;
	uint32_t				fps_q9;
	uint32_t				bits;
	uint32_t				mirror_horizontal;
	uint32_t				mirror_vertical;
	uint32_t				hdr_mode;
	uint32_t				enc_mode;
	uint8_t					anti_flicker;
	int						exposure;
} vin_s2l_config_t;

typedef struct pts_item_s {
	uint8_t					started;
	uint32_t				last_pts;
	uint64_t				last_stream_pts;
} pts_item_t;

typedef struct srcbuf_info_s {
	uint16_t				xoff;
	uint16_t				yoff;
	uint16_t				width;
	uint16_t				height;
} srcbuf_info_t;

typedef struct vsrc_s2l_stream_s {
	enum iav_stream_type	encode_type;
	uint32_t				encode_width;
	uint32_t				encode_height;

	enum iav_srcbuf_id		srcbuf_id;
	srcbuf_info_t			srcbuf_info;

	int						jpeg_quality;
	int						encode_bitrate_control;
	uint32_t				encode_avg_bitrate;
	uint32_t				encode_min_bitrate;	// for VBR
	uint32_t				encode_max_bitrate;	// for VBR

	// encoding use
	int8_t					overlay_configured;
	int8_t					eos_sent;
	int8_t					is_still;
	int						index;	// index of this stream: 0, 1, 2
	int						framerate;
	int						rate_90kHZ;
	int						ar_x;
	int						ar_y;

	int						M;
	int						N;

	pts_item_t				dsp_pts;
	pts_item_t				hw_pts;
	uint64_t				last_dts64;
	uint32_t				frame_counter;

	// for IAV
	struct iav_stream_format	stream_format;
	struct iav_h264_cfg			h264_cfg;
	struct iav_mjpeg_cfg		mjpeg_cfg;
	struct iav_stream_fps		stream_fps;
	struct iav_bitrate			bitrate_cfg;
	struct iav_overlay_insert	iav_overlay_config;
} vsrc_s2l_stream_t;

typedef struct vsrc_s2l_dev_data_s {
	int8_t					data_in_use;
	int8_t					encoding_stopped;

	int						iav_fd;
	enum vsrc_status_e		state;

	uint32_t				iav_bsb_size;
	uint8_t					*piav_bsb;

	uint32_t				iav_overlay_size;
	uint8_t					*piav_overlay;

	uint8_t					*pclut_addr;
	uint32_t				overlay_size;
	uint8_t					*poverlay_addr[MAX_NUM_STREAMS];

	// vin config
	vin_s2l_config_t			vin_config;
	struct vindev_devinfo		vin_info;
	struct vindev_video_info	vin_video_info;

	int						vin_width;	// -1: read from iav
	int						vin_height;	// -1: read from iav
	uint8_t					fps;			// user config
	uint8_t					framerate;	// FrameRate_Unknown
	uint32_t				vin_rate;	// rate per 90kHZ
	char					*vin_config_str;	// need free

	uint32_t				vin_input_width;
	uint32_t				vin_input_height;
	uint32_t				main_srcbuf_width;
	uint32_t				main_srcbuf_height;
	int						stream_of_preview;	// valid in still mode

	// vout info
	vout_info_t		lcd_info;
	srcbuf_info_t	lcd_srcbuf_info;

	vout_info_t 	hdmi_info;
	srcbuf_info_t	hdmi_srcbuf_info;

	// global configs for iav
	unsigned int				iav_active_stream;
	struct iav_system_resource	system_resource;
	struct iav_srcbuf_setup		srcbuf_setup;

	char						*clock_mode_str;	// need free

	// any of these changes needs to enter Idle first, and then restart
	int8_t						vin_config_changed;	// vin params is changed
	int8_t						stream_size_changed;	// stream size is changed
	int8_t						clock_mode_changed;	// clock mode

	// stream configs
	vsrc_s2l_stream_t			streams[MAX_ENCODE_STREAMS];
} vsrc_s2l_dev_data_t;

#define VIDEO_PROFILE		1	// main profile
#define VIDEO_AU_TYPE		1	// Add AUD before SPS, PPS, with SEI in the end
#define VIDEO_GOP_MODEL		0	// simple GOP
#define VIDEO_IDR_INTERVAL	1	// closed GOP
#define VIDEO_M				1	// 

/*===========================================================================*/

// stream 0: main h264 stream
// stream 1: mjpeg preview stream
// stream 2: second h264 stream

/*===========================================================================*/

static int vsrc_s2l_open_iav(vsrc_s2l_dev_data_t *pData)
{
	if ((pData->iav_fd = avf_open_file("/dev/iav", O_RDWR, 0)) < 0) {
		AVF_LOGP("/dev/iav");
		return pData->iav_fd;
	}

/*
	// disable iav check
	struct iav_debug_cfg debug;
	::memset(&debug, 0, sizeof(debug));

	if (::ioctl(pData->iav_fd, IAV_IOC_GET_DEBUG_CONFIG, &debug) == 0) {
		debug.enable = 1;
		debug.flags |= IAV_DEBUG_CHECK_DISABLE;
		if (::ioctl(pData->iav_fd, IAV_IOC_SET_DEBUG_CONFIG, &debug) == 0) {
			AVF_LOGD("check disable");
		}
	}
*/

	return 0;
}

static void vsrc_s2l_close_iav(vsrc_s2l_dev_data_t *pData)
{
	avf_safe_close(pData->iav_fd);
}

static int vsrc_s2l_map_bsb(vsrc_s2l_dev_data_t *pData)
{
	struct iav_querybuf querybuf;
	int ret_val;

	if (pData->piav_bsb != NULL)
		return 0;

	// querybuf: {buf = 0x1, length = 0x600000, offset = 0x20a00000}
	querybuf.buf = IAV_BUFFER_BSB;
	querybuf.length = 0;
	querybuf.offset = 0;

	if ((ret_val = ::ioctl(pData->iav_fd, IAV_IOC_QUERY_BUF, &querybuf)) < 0) {
		AVF_LOGP("IAV_IOC_QUERY_BUF:IAV_BUFFER_BSB");
		return ret_val;
	}

	pData->iav_bsb_size = querybuf.length;
	pData->piav_bsb = (uint8_t *)::mmap(NULL, pData->iav_bsb_size * 2,
		PROT_READ, MAP_SHARED, pData->iav_fd, querybuf.offset);

	if (pData->piav_bsb == MAP_FAILED) {
		AVF_LOGP("mmap:IAV_BUFFER_BSB");
		pData->piav_bsb = NULL;
		pData->iav_bsb_size = 0;
		return -1;
	}

	return 0;
}

static void vsrc_s2l_unmap_bsb(vsrc_s2l_dev_data_t *pData)
{
	if (pData->piav_bsb && pData->iav_bsb_size) {
		::munmap(pData->piav_bsb, pData->iav_bsb_size * 2);
		pData->piav_bsb = NULL;
		pData->iav_bsb_size = 0;
	}
}

static int vsrc_s2l_map_overlay(vsrc_s2l_dev_data_t *pData)
{
	struct iav_querybuf querybuf;
	int ret_val;

	if (pData->piav_overlay != NULL)
		return 0;

	// querybuf: {buf = 0x4, length = 0x200000, offset = 0x21000000}
	querybuf.buf = IAV_BUFFER_OVERLAY;
	querybuf.length = 0;
	querybuf.offset = 0;

	if ((ret_val = ::ioctl(pData->iav_fd, IAV_IOC_QUERY_BUF, &querybuf)) < 0) {
		AVF_LOGP("IAV_IOC_QUERY_BUF:IAV_BUFFER_OVERLAY");
		return ret_val;
	}

	pData->iav_overlay_size = querybuf.length;
	pData->piav_overlay = (uint8_t *)::mmap(NULL, pData->iav_overlay_size,
		PROT_WRITE, MAP_SHARED, pData->iav_fd, querybuf.offset);

	if (pData->piav_overlay == MAP_FAILED) {
		AVF_LOGP("mmap:IAV_BUFFER_BSB");
		pData->piav_overlay = NULL;
		pData->iav_overlay_size = 0;
		return -1;
	}

	::memset(pData->piav_overlay, 0, pData->iav_overlay_size);

	pData->pclut_addr = pData->piav_overlay;
	pData->overlay_size = (pData->iav_overlay_size - OVERLAY_OFFSET) / MAX_NUM_STREAMS;

	uint8_t *overlay_addr = pData->pclut_addr + OVERLAY_OFFSET;
	for (int i = 0; i < MAX_NUM_STREAMS; i++) {
		pData->poverlay_addr[i] = overlay_addr + pData->overlay_size * i;
	}

	return 0;
}

static void vsrc_s2l_unmap_overlay(vsrc_s2l_dev_data_t *pData)
{
	if (pData->piav_overlay && pData->iav_overlay_size) {
		::munmap(pData->piav_overlay, pData->iav_overlay_size);
		pData->piav_overlay = NULL;
		pData->iav_overlay_size = 0;
	}
}

static int vsrc_s2l_set_video_mode(vsrc_s2l_dev_data_t *pData)
{
	struct vindev_mode vin_mode;
	int ret_val;

	vin_mode.vsrc_id = pData->vin_config.source_id;
	vin_mode.video_mode = pData->vin_config.video_mode;
	vin_mode.bits = pData->vin_config.bits;
	vin_mode.hdr_mode = pData->vin_config.hdr_mode;

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SET_MODE, &vin_mode);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_VIN_SET_MODE");
		return ret_val;
	}

	return 0;
}

static int vsrc_s2l_set_framerate(vsrc_s2l_dev_data_t *pData)
{
	struct vindev_fps vin_fps;
	int ret_val;

	vin_fps.vsrc_id = pData->vin_config.source_id;
	vin_fps.fps = pData->vin_config.fps_q9;

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SET_FPS, &vin_fps);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_VIN_SET_FPS");
		return ret_val;
	}

	return 0;
}

static int vsrc_s2l_set_mirror_mode(vsrc_s2l_dev_data_t *pData)
{
	struct vindev_mirror vin_mirror;
	int ret_val;

	// vsrc_id
	vin_mirror.vsrc_id = pData->vin_config.source_id;

	// pattern
	if (pData->vin_config.mirror_horizontal) {
		if (pData->vin_config.mirror_vertical) {
			vin_mirror.pattern = VINDEV_MIRROR_HORRIZONTALLY_VERTICALLY;
		} else {
			vin_mirror.pattern = VINDEV_MIRROR_HORRIZONTALLY;
		}
	} else {
		if (pData->vin_config.mirror_vertical) {
			vin_mirror.pattern = VINDEV_MIRROR_VERTICALLY;
		} else {
			vin_mirror.pattern = VINDEV_MIRROR_NONE;
		}
	}

	// bayer_pattern
	vin_mirror.bayer_pattern = VINDEV_BAYER_PATTERN_AUTO;

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SET_MIRROR, &vin_mirror);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_VIN_SET_MIRROR");
		return ret_val;
	}

	return 0;
}

static int vsrc_s2l_get_vin_info(vsrc_s2l_dev_data_t *pData)
{
	int ret_val;

	pData->vin_info.vsrc_id = pData->vin_config.source_id;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_VIN_GET_DEVINFO, &pData->vin_info);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_VIN_GET_DEVINFO");
		return ret_val;
	}

	pData->vin_video_info.vsrc_id = pData->vin_config.source_id;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_VIN_GET_VIDEOINFO, &pData->vin_video_info);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_VIN_GET_VIDEOINFO");
		return ret_val;
	}

	// if not specified, set to vin size
	if (pData->vin_width <= 0 || pData->vin_height <= 0) {
		pData->vin_width = pData->vin_video_info.info.width;
		pData->vin_height = pData->vin_video_info.info.height;
	}

	AVF_LOGD("vin%d:%d[%s]: size:%d,%d; dev:%d,%d",
		pData->vin_info.vsrc_id,
		pData->vin_info.intf_id,
		pData->vin_info.name,
		pData->vin_width, pData->vin_height,
		pData->vin_video_info.info.width,
		pData->vin_video_info.info.height);

	return 0;
}

static int vsrc_s2l_get_stream_configs(vsrc_s2l_dev_data_t *pData,
	vsrc_s2l_stream_t *stream)
{
	struct iav_stream_cfg stream_cfg;
	int ret_val;

	stream->stream_format.id = stream->index;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_GET_STREAM_FORMAT, &stream->stream_format);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_GET_STREAM_FORMAT");
		return ret_val;
	}

	stream->h264_cfg.id = stream->index;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_GET_H264_CONFIG, &stream->h264_cfg);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_GET_H264_CONFIG");
		return ret_val;
	}

	stream->mjpeg_cfg.id = stream->index;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_GET_MJPEG_CONFIG, &stream->mjpeg_cfg);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_GET_MJPEG_CONFIG");
		return ret_val;
	}

/*
	// framerate
	stream_cfg.id = stream->index;
	stream_cfg.cid = IAV_STMCFG_FPS;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_GET_STREAM_CONFIG, &stream_cfg);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_GET_STREAM_CONFIG:IAV_STMCFG_FPS");
		return ret_val;
	}
	stream->stream_fps = stream_cfg.arg.fps;
*/

	// bitrate
	stream_cfg.id = stream->index;
	stream_cfg.cid = IAV_H264_CFG_BITRATE;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_GET_STREAM_CONFIG, &stream_cfg);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_GET_STREAM_CONFIG:IAV_H264_CFG_BITRATE");
		return ret_val;
	}
	stream->bitrate_cfg = stream_cfg.arg.h264_rc;

	return 0;
}

// preview / encode state
static int vsrc_s2l_set_stream_configs(vsrc_s2l_dev_data_t *pData,
	vsrc_s2l_stream_t *stream)
{
	struct iav_stream_cfg stream_cfg;
	int ret_val;

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_SET_STREAM_FORMAT, &stream->stream_format);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_SET_STREAM_FORMAT");
		return ret_val;
	}

	if (stream->stream_format.type == IAV_STREAM_TYPE_H264) {
		ret_val = ::ioctl(pData->iav_fd, IAV_IOC_SET_H264_CONFIG, &stream->h264_cfg);
		if (ret_val < 0) {
			AVF_LOGP("IAV_IOC_SET_H264_CONFIG");
			return ret_val;
		}
	} else if (stream->stream_format.type == IAV_STREAM_TYPE_MJPEG) {
		ret_val = ::ioctl(pData->iav_fd, IAV_IOC_SET_MJPEG_CONFIG, &stream->mjpeg_cfg);
		if (ret_val < 0) {
			AVF_LOGP("IAV_IOC_SET_MJPEG_CONFIG");
			return ret_val;
		}
	} else {
		return 0;
	}

	// framerate
	stream_cfg.id = stream->index;
	stream_cfg.cid = IAV_STMCFG_FPS;
	stream_cfg.arg.fps = stream->stream_fps;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_SET_STREAM_CONFIG, &stream_cfg);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_SET_STREAM_CONFIG:IAV_STMCFG_FPS");
		return ret_val;
	}

	// bitrate
	stream_cfg.id = stream->index;
	stream_cfg.cid = IAV_H264_CFG_BITRATE;
	stream_cfg.arg.h264_rc = stream->bitrate_cfg;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_SET_STREAM_CONFIG, &stream_cfg);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_SET_STREAM_CONFIG:IAV_H264_CFG_BITRATE");
		return ret_val;
	}

	return 0;
}

static int vsrc_s2l_start_preview(vsrc_s2l_dev_data_t *pData)
{
	int ret_val;

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_ENABLE_PREVIEW, 0);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_ENABLE_PREVIEW");
		return ret_val;
	}

	// wait preview stable
	avf_msleep(1000);

	return 0;
}

static INLINE int vsrc_s2l_get_hdmi_srcbuf(vsrc_s2l_dev_data_t *pData)
{
	return IAV_SRCBUF_3;
}

static INLINE int vsrc_s2l_get_lcd_srcbuf(vsrc_s2l_dev_data_t *pData)
{
	return IAV_SRCBUF_4;
}

// use IAV_SRCBUF_PA or IAV_SRCBUF_PB for second video stream
static int vsrc_s2l_get_extra_srcbuf(vsrc_s2l_dev_data_t *pData)
{
	if (pData->hdmi_info.enabled) {
		// only one can be enabled, see vsrc_s2l_EnterPreview()
		return pData->lcd_info.enabled ? -1 : vsrc_s2l_get_lcd_srcbuf(pData);
	} else {
		return vsrc_s2l_get_hdmi_srcbuf(pData);
	}
}

// must be in IDLE state
static int vsrc_s2l_config_resource(vsrc_s2l_dev_data_t *pData)
{
	int ret_val;

	struct iav_system_resource *sysres = &pData->system_resource;

	::memset(sysres, 0, sizeof(*sysres));
	sysres->encode_mode = pData->vin_config.enc_mode;

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_GET_SYSTEM_RESOURCE, sysres);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_GET_SYSTEM_RESOURCE");
		return ret_val;
	}

	// sysres->encode_mode
	// sysres->max_num_encode = 4;
	// sysres->max_num_cap_sources = 2;
	// sysres->exposure_num = 2;

	if (pData->vin_config.exposure > 0) {
		sysres->exposure_num = pData->vin_config.exposure;
	}

	::memset(&sysres->buf_max_size, 0, sizeof(sysres->buf_max_size));

	// main srcbuf
	sysres->buf_max_size[IAV_SRCBUF_1].width = pData->main_srcbuf_width;
	sysres->buf_max_size[IAV_SRCBUF_1].height = pData->main_srcbuf_height;

	// other srcbufs
	for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
		vsrc_s2l_stream_t *stream = pData->streams + i;
		if (stream->srcbuf_id != IAV_SRCBUF_1) {
			sysres->buf_max_size[stream->srcbuf_id].width = stream->encode_width;
			sysres->buf_max_size[stream->srcbuf_id].height = stream->encode_height;
		}
	}

	if (pData->hdmi_info.enabled) {
		int srcbuf = vsrc_s2l_get_hdmi_srcbuf(pData);
		sysres->buf_max_size[srcbuf].width = ROUND_UP(pData->hdmi_info.vout_width, 16);
		sysres->buf_max_size[srcbuf].height = ROUND_UP(pData->hdmi_info.vout_height, 8);
	}

	if (pData->lcd_info.enabled) {
		int srcbuf = vsrc_s2l_get_lcd_srcbuf(pData);
		sysres->buf_max_size[srcbuf].width = ROUND_UP(pData->lcd_info.vout_width, 16);
		sysres->buf_max_size[srcbuf].height = ROUND_UP(pData->lcd_info.vout_height, 8);
	}

	::memset(&sysres->stream_max_size, 0, sizeof(sysres->stream_max_size));

	for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
		sysres->stream_max_size[i].width = pData->streams[i].encode_width;
		sysres->stream_max_size[i].height = pData->streams[i].encode_height;
	}

	// sysres->stream_max_M = {1,1,1,1};
	// sysres->stream_max_N = {255,255,255,255};
	// sysres->stream_max_advanced_quality_model = {8,8,8,8};
	// sysres->stream_long_ref_enable = {0,0,0};

	// sysres->raw_pitch_in_bytes = 0;
	// sysres->total_memory_size = 8;
	// sysres->hdr_type = 0;
	// sysres->iso_type = 2;
	// sysres->is_stitched = 1;
	// sysres->reserved1 = 0;

	// sysres->rotate_enable = 1;
	sysres->raw_capture_enable = 1;	// enable raw capture
	// sysres->vout_swap_enable = 0;
	// sysres->lens_warp_enable = 0;
	// sysres->enc_from_raw_enable = 0;

	if (pData->hdmi_info.enabled) {
		sysres->mixer_a_enable = 0;
		sysres->mixer_b_enable = 1;
		sysres->osd_from_mixer_a = 1;
		sysres->osd_from_mixer_b = 0;
	} else if (pData->lcd_info.enabled) {
		sysres->mixer_a_enable = 1;
		sysres->mixer_b_enable = 0;
		sysres->osd_from_mixer_a = 0;
		sysres->osd_from_mixer_b = 1;
	} else {
		sysres->mixer_a_enable = 0;
		sysres->mixer_b_enable = 0;
		sysres->osd_from_mixer_a = 1;
		sysres->osd_from_mixer_b = 0;
	}

	// sysres->idsp_upsample_type = 0;
	// sysres->reserved2 = 0;
	// sysres->dsp_partition_map = 0;

	// sysres->raw_max_size = {1920,1088};
	// sysres->extra_dram_buf = {0,0,0,0,0};
	// sysres->reserved3 = 0;
	// sysres->max_warp_input_width = 0;
	// sysres->max_warp_input_height = 0;
	// sysres->max_warp_output_width = 0;
	// sysres->max_padding_width = 256;
	// sysres->v_warped_main_max_width = 0;
	// sysres->v_warped_main_max_height = 0;
	// sysres->enc_dummy_latency = 0;

//#ifdef AVF_DEBUG
	AVF_LOGD(C_BROWN "system resource:" C_NONE);
	AVF_LOGD("    encode_mode: " C_CYAN "%d" C_NONE
			", max_num_encode: " C_CYAN "%d" C_NONE
			", max_num_cap_sources: " C_CYAN "%d" C_NONE
			", exposure_num: " C_CYAN "%d" C_NONE,
		sysres->encode_mode,
		sysres->max_num_encode,
		sysres->max_num_cap_sources,
		sysres->exposure_num);
	for (int i = 0; i < (int)ARRAY_SIZE(sysres->buf_max_size); i++) {
		AVF_LOGD("    buf_max_size[%d]: " C_CYAN "%d,%d" C_NONE,
			i, sysres->buf_max_size[i].width, sysres->buf_max_size[i].height);
	}
//#endif

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_SET_SYSTEM_RESOURCE, sysres);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_SET_SYSTEM_RESOURCE");
		return ret_val;
	}

	return 0;
}

static int vsrc_s2l_calc_still_size(vsrc_s2l_dev_data_t *pData)
{
	int still_stream = -1;

	pData->stream_of_preview = -1;

	// find the still stream index
	for (int i = 0; i < MAX_ENCODE_STREAMS; i++) {
		if (pData->streams[i].is_still) {
			still_stream = i;
			break;
		}
	}

	if (still_stream < 0) {
		// no still stream
		return 0;
	}

	// if still size is not specified, use full vin size
	vsrc_s2l_stream_t *stream = pData->streams + still_stream;
	if (stream->encode_width == 0 || stream->encode_height == 0) {
		stream->encode_width = pData->vin_width;
		stream->encode_height = pData->vin_height;
	}

	int still_width = stream->encode_width;
	int still_height = stream->encode_height;

	// find other largest JPEG stream as the preview stream, and adjust the size
	// use case: still + preview

	uint32_t total = 0;
	for (int i = 0; i < MAX_ENCODE_STREAMS; i++) {
		stream = pData->streams + i;
		if (i != still_stream && stream->encode_type == IAV_STREAM_TYPE_MJPEG) {
			// if height not specified, make the jpeg stream proportional to still
			if (stream->encode_height == 0) {
				stream->encode_height = stream->encode_width * still_height / still_width;
				stream->encode_height = ROUND_DOWN(stream->encode_height, 8);
			}
			uint32_t tmp = stream->encode_width * stream->encode_height;
			if (total < tmp) {
				total = tmp;
				pData->stream_of_preview = i;
			}
		}
	}

	return 0;
}

// can cover all coded streams
static int vsrc_calc_main_srcbuf_size(vsrc_s2l_dev_data_t *pData, 
	uint32_t *main_srcbuf_width, uint32_t *main_srcbuf_height)
{
	*main_srcbuf_width = 0;
	*main_srcbuf_height = 0;

	// width = max{all widths}
	// height = max{all heights}
	for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
		vsrc_s2l_stream_t *stream = pData->streams + i;
		if (*main_srcbuf_width < stream->encode_width) {
			*main_srcbuf_width = stream->encode_width;
		}
		if (*main_srcbuf_height < stream->encode_height) {
			*main_srcbuf_height = stream->encode_height;
		}
	}

	if (*main_srcbuf_width == 0 || *main_srcbuf_height == 0) {
		AVF_LOGE("main srcbuf size is 0");
		return -1;
	}

	// width be 16x and height be 8x
	*main_srcbuf_width = ROUND_UP(*main_srcbuf_width, 16);
	*main_srcbuf_height = ROUND_UP(*main_srcbuf_height, 8);

	// main srcbuf should be bigger than hdmi
	if (pData->hdmi_info.enabled) {
		if ((int)*main_srcbuf_width < pData->hdmi_info.video_width ||
			(int)*main_srcbuf_height < pData->hdmi_info.video_height) {
			AVF_LOGW("main srcbuf size %d,%d is smaller than hdmi size %d,%d",
				*main_srcbuf_width, *main_srcbuf_height,
				pData->hdmi_info.video_width, pData->hdmi_info.video_height);
		}
	}

	// main srcbuf should be bigger than lcd
	if (pData->lcd_info.enabled) {
		if ((int)*main_srcbuf_width < pData->lcd_info.video_width ||
			(int)*main_srcbuf_height < pData->lcd_info.video_height) {
			AVF_LOGW("main srcbuf size %d,%d is smaller than hdmi size %d,%d",
				*main_srcbuf_width, *main_srcbuf_height,
				pData->lcd_info.video_width, pData->lcd_info.video_height);
		}
	}

	return 0;
}

// get the largest size for all streams, use it as the main srcbuf size
static int vsrc_s2l_calc_main_srcbuf(vsrc_s2l_dev_data_t *pData)
{
	vsrc_s2l_stream_t *streams = pData->streams;

	streams[0].srcbuf_id = IAV_SRCBUF_1;	// main

	iav_srcbuf_id id1 = (iav_srcbuf_id)vsrc_s2l_get_extra_srcbuf(pData); // larger
	iav_srcbuf_id id2 = IAV_SRCBUF_2;		// smaller

	if (streams[1].encode_width * streams[1].encode_height > streams[2].encode_width * streams[2].encode_height) {
		streams[1].srcbuf_id = id1;
		streams[2].srcbuf_id = id2;
	} else {
		streams[1].srcbuf_id = id2;
		streams[2].srcbuf_id = id1;
	}

	uint32_t main_srcbuf_width;
	uint32_t main_srcbuf_height;

	if (vsrc_calc_main_srcbuf_size(pData, &main_srcbuf_width, &main_srcbuf_height) < 0)
		return -1;

	pData->main_srcbuf_width = main_srcbuf_width;
	pData->main_srcbuf_height = main_srcbuf_height;

	AVF_LOGD("main srcbuf size: " C_CYAN "%d,%d" C_NONE,
		pData->main_srcbuf_width, pData->main_srcbuf_height);

	// find the largest stream to use the main srcbuf
	int stream_of_main = 0;
	uint32_t max = 0;
	for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
		vsrc_s2l_stream_t *stream = pData->streams + i;
		uint32_t tmp = stream->encode_width * stream->encode_height;
		if (max < tmp) {
			max = tmp;
			stream_of_main = i;
		}
	}

	// exchange the 2 streams: stream 0 no longer uses the main source buffer
	if (stream_of_main != 0) {
		iav_srcbuf_id tmp = pData->streams[stream_of_main].srcbuf_id;
		pData->streams[stream_of_main].srcbuf_id = pData->streams[0].srcbuf_id;
		pData->streams[0].srcbuf_id = tmp;
	}

	return 0;
}

// get the max rect of vin input
// the rect is proportional to main srcbuf, and smaller than vin input
// it is the input of the main buffer. it will be scaled to main buffer size
static int vsrc_s2l_calc_vin_input(vsrc_s2l_dev_data_t *pData)
{
	int main_width = pData->main_srcbuf_width;
	int main_height = pData->main_srcbuf_height;
	int vin_width = pData->vin_width;
	int vin_height = pData->vin_height;

	if (vin_width * main_height > vin_height * main_width) {
		// vin is broader
		pData->vin_input_height = vin_height;
		pData->vin_input_width = (main_width * vin_height + main_height - 1) / main_height;
		pData->vin_input_width = ROUND_UP(pData->vin_input_width, 16);
	} else {
		// vin is taller
		pData->vin_input_width = vin_width;
		pData->vin_input_height = (main_height * vin_width + main_width - 1) / main_width;
		pData->vin_input_height = ROUND_UP(pData->vin_input_height, 8);
	}

	return 0;
}

static int vsrc_s2l_calc_srcbuf_size(vsrc_s2l_dev_data_t *pData,
	uint32_t dest_width, uint32_t dest_height, srcbuf_info_t *srcbuf_info, int width_align)
{
	if (dest_width == 0 || dest_height == 0) {
		srcbuf_info->xoff = 0;
		srcbuf_info->yoff = 0;
		srcbuf_info->width = 0;
		srcbuf_info->height = 0;
		return 0;
	}

	int main_width = pData->main_srcbuf_width;
	int main_height = pData->main_srcbuf_height;

	if (main_width * dest_height > main_height * dest_width) {
		// main is wider
		srcbuf_info->height = main_height;
		srcbuf_info->width = (dest_width * main_height + dest_height - 1) / dest_height;
		srcbuf_info->width = ROUND_UP(srcbuf_info->width, width_align);
	} else {
		// main is taller
		srcbuf_info->width = main_width;
		srcbuf_info->height = (dest_height * main_width + dest_width - 1) / dest_width;
		srcbuf_info->height = ROUND_UP(srcbuf_info->height, 8);
	}

	srcbuf_info->xoff = (main_width - srcbuf_info->width) / 2;
	srcbuf_info->yoff = (main_height - srcbuf_info->height) / 2;

	return 0;
}

static int vsrc_s2l_calc_all_srcbufs(vsrc_s2l_dev_data_t *pData)
{
	int ret_val;

	for (int i = 0; i < MAX_ENCODE_STREAMS; i++) {
		vsrc_s2l_stream_t *stream = pData->streams + i;
		ret_val = vsrc_s2l_calc_srcbuf_size(pData,
			stream->encode_width, stream->encode_height,
			&stream->srcbuf_info, 16);
		if (ret_val < 0)
			return ret_val;
	}

	if (pData->hdmi_info.enabled) {
		ret_val = vsrc_s2l_calc_srcbuf_size(pData,
			pData->hdmi_info.video_width, pData->hdmi_info.video_height,
			&pData->hdmi_srcbuf_info, 8);
		if (ret_val < 0)
			return ret_val;
	}

	if (pData->lcd_info.enabled) {
		ret_val = vsrc_s2l_calc_srcbuf_size(pData,
			pData->lcd_info.video_width, pData->lcd_info.video_height,
			&pData->lcd_srcbuf_info, 8);
		if (ret_val < 0)
			return ret_val;
	}

	return 0;
}

#ifdef AVF_DEBUG
static const char *vsrc_s2l_get_srcbuf_typename(iav_srcbuf_type type)
{
	switch (type) {
	case IAV_SRCBUF_TYPE_OFF: return "off";
	case IAV_SRCBUF_TYPE_ENCODE: return "encode";
	case IAV_SRCBUF_TYPE_PREVIEW: return "preview";
	default: return "unknown";
	}
}
#endif

// must be in IDLE state
static int vsrc_s2l_config_srcbuf(vsrc_s2l_dev_data_t *pData)
{
	int ret_val;

	struct iav_srcbuf_setup *srcbuf_setup = &pData->srcbuf_setup;
	::memset(srcbuf_setup, 0, sizeof(*srcbuf_setup));

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_GET_SOURCE_BUFFER_SETUP, srcbuf_setup);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_GET_SOURCE_BUFFER_SETUP");
		return ret_val;
	}

	//---------------------------------------------------------
	// srcbuf size
	//---------------------------------------------------------
	::memset(&srcbuf_setup->size, 0, sizeof(srcbuf_setup->size));

	for (int i = 0; i < MAX_ENCODE_STREAMS; i++) {
		vsrc_s2l_stream_t *stream = pData->streams + i;
		struct iav_window *size = srcbuf_setup->size + stream->srcbuf_id;
		size->width = stream->encode_width;
		size->height = stream->encode_height;
	}

	if (pData->hdmi_info.enabled) {
		struct iav_window *size = srcbuf_setup->size + vsrc_s2l_get_hdmi_srcbuf(pData);
		size->width = pData->hdmi_info.video_width;
		size->height = pData->hdmi_info.video_height;
	}

	if (pData->lcd_info.enabled) {
		struct iav_window *size = srcbuf_setup->size + vsrc_s2l_get_lcd_srcbuf(pData);
		size->width = pData->lcd_info.video_width;
		size->height = pData->lcd_info.video_height;
	}

	//---------------------------------------------------------
	// srcbuf input
	//---------------------------------------------------------
	::memset(&srcbuf_setup->input, 0, sizeof(srcbuf_setup->input));

	for (int i = 0; i < MAX_ENCODE_STREAMS; i++) {
		vsrc_s2l_stream_t *stream = pData->streams + i;
		struct iav_rect *input = srcbuf_setup->input + stream->srcbuf_id;
		if (stream->srcbuf_id == IAV_SRCBUF_1) { // main
			input->width = pData->vin_input_width;
			input->height = pData->vin_input_height;
			input->x = (pData->vin_width - pData->vin_input_width) / 2;
			input->y = (pData->vin_height - pData->vin_input_height) / 2;
		} else {
			input->width = stream->srcbuf_info.width;
			input->height = stream->srcbuf_info.height;
			input->x = stream->srcbuf_info.xoff;
			input->y = stream->srcbuf_info.yoff;
		}
	}

	if (pData->hdmi_info.enabled) {
		struct iav_rect *input = srcbuf_setup->input + vsrc_s2l_get_hdmi_srcbuf(pData);
		input->width = pData->hdmi_srcbuf_info.width;
		input->height = pData->hdmi_srcbuf_info.height;
		input->x = pData->hdmi_srcbuf_info.xoff;
		input->y = pData->hdmi_srcbuf_info.yoff;
	}

	if (pData->lcd_info.enabled) {
		struct iav_rect *input = srcbuf_setup->input + vsrc_s2l_get_lcd_srcbuf(pData);
		input->width = pData->lcd_srcbuf_info.width;
		input->height = pData->lcd_srcbuf_info.height;
		input->x = pData->lcd_srcbuf_info.xoff;
		input->y = pData->lcd_srcbuf_info.yoff;
	}

	//---------------------------------------------------------
	// srcbuf type
	//---------------------------------------------------------
	for (int i = 0; i < (int)ARRAY_SIZE(srcbuf_setup->type); i++) {
		srcbuf_setup->type[i] = IAV_SRCBUF_TYPE_OFF;
	}

	for (int i = 0; i < MAX_ENCODE_STREAMS; i++) {
		vsrc_s2l_stream_t *stream = pData->streams + i;
		if (stream->encode_width > 0 && stream->encode_height > 0) {
			srcbuf_setup->type[stream->srcbuf_id] = IAV_SRCBUF_TYPE_ENCODE;
		}
	}

	if (pData->hdmi_info.enabled) {
		srcbuf_setup->type[vsrc_s2l_get_hdmi_srcbuf(pData)] = IAV_SRCBUF_TYPE_PREVIEW;
	}

	if (pData->lcd_info.enabled) {
		srcbuf_setup->type[vsrc_s2l_get_lcd_srcbuf(pData)] = IAV_SRCBUF_TYPE_PREVIEW;
	}

#ifdef AVF_DEBUG
	AVF_LOGD(C_BROWN "srcbuf info:" C_NONE);
	for (int i = 0; i < IAV_SRCBUF_NUM; i++) {
		AVF_LOGD("    srcbuf[%d]"
			": input=" C_CYAN "%d,%d" C_NONE
			", off=" C_CYAN "%d,%d" C_NONE
			", size=" C_CYAN "%d,%d" C_NONE
			", type=" C_CYAN "%s" C_NONE
			,
		i,
		srcbuf_setup->input[i].width,
		srcbuf_setup->input[i].height,
		srcbuf_setup->input[i].x,
		srcbuf_setup->input[i].y,
		srcbuf_setup->size[i].width,
		srcbuf_setup->size[i].height,
		vsrc_s2l_get_srcbuf_typename(srcbuf_setup->type[i]));
	}
#endif

	// srcbuf_setup->unwarp = {0,0,0,0,0};

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_SET_SOURCE_BUFFER_SETUP, srcbuf_setup);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_SET_SOURCE_BUFFER_SETUP");
		return ret_val;
	}

	return 0;
}

#ifdef AVF_DEBUG
static const char *vsrc_s2l_get_enc_typename(iav_stream_type type)
{
	switch (type) {
	case IAV_STREAM_TYPE_NONE: return "none";
	case IAV_STREAM_TYPE_H264: return "h264";
	case IAV_STREAM_TYPE_MJPEG: return "mjpeg";
	default: return "unknown";
	}
}
#endif

static int vsrc_s2l_config_one_stream(vsrc_s2l_dev_data_t *pData, vsrc_s2l_stream_t *stream)
{
	int ret_val;

	ret_val = vsrc_s2l_get_stream_configs(pData, stream);
	if (ret_val < 0)
		return ret_val;

	// stream format
	stream->stream_format.id = stream->index;
	stream->stream_format.type = stream->encode_type;
	stream->stream_format.buf_id = stream->srcbuf_id;
	stream->stream_format.enc_win.width = stream->encode_width;
	stream->stream_format.enc_win.height = stream->encode_height;
	stream->stream_format.enc_win.x = 0;
	stream->stream_format.enc_win.y = 0;
	stream->stream_format.hflip = 0;
	stream->stream_format.vflip = 0;
	stream->stream_format.rotate_cw = 0;
	stream->stream_format.duration = 0;
	stream->stream_format.snapshot_enable = 0;
	stream->stream_format.reserved = 0;

	// still mode, enable other streams to take snapshot
	if (pData->stream_of_preview >= 0 && stream->index != pData->stream_of_preview) {
		stream->stream_format.snapshot_enable = 1;
	}

#ifdef AVF_DEBUG
	if (stream->index == 0) {
		AVF_LOGD(C_BROWN "stream format:" C_NONE);
	}
	AVF_LOGD("    stream[%d]"
		": enc_type=" C_CYAN "%s" C_NONE
		", srcbuf=" C_CYAN "%d" C_NONE
		", size=" C_CYAN "%d,%d" C_NONE
		", off=" C_CYAN "%d,%d" C_NONE
		", fps=" C_CYAN "%d:%d" C_NONE
		,
		stream->stream_format.id,
		vsrc_s2l_get_enc_typename(stream->stream_format.type),
		stream->stream_format.buf_id,
		stream->stream_format.enc_win.width,
		stream->stream_format.enc_win.height,
		stream->stream_format.enc_win.x,
		stream->stream_format.enc_win.y,
		stream->stream_fps.fps_multi,
		stream->stream_fps.fps_div);
#endif

	if (stream->stream_format.type == IAV_STREAM_TYPE_H264) {

		// h264 config
		stream->h264_cfg.id = stream->index;
		stream->h264_cfg.gop_structure = VIDEO_GOP_MODEL;
		stream->h264_cfg.M = stream->M;
		stream->h264_cfg.N = stream->N;
		stream->h264_cfg.idr_interval = VIDEO_IDR_INTERVAL;
		stream->h264_cfg.profile = VIDEO_PROFILE;
		stream->h264_cfg.au_type = VIDEO_AU_TYPE;
		stream->h264_cfg.chroma_format = 0;
		stream->h264_cfg.cpb_buf_idc = 0;
		stream->h264_cfg.reserved0 = 0;
		stream->h264_cfg.cpb_user_size = 0;
		stream->h264_cfg.en_panic_rc = 0;
		stream->h264_cfg.cpb_cmp_idc = 0;
		stream->h264_cfg.fast_rc_idc = 0;
		stream->h264_cfg.mv_threshold = 0;
		stream->h264_cfg.enc_improve = 0;
		stream->h264_cfg.multi_ref_p = 0;
		stream->h264_cfg.reserved1 = 0;
		stream->h264_cfg.long_term_intvl = 0;

		//stream->h264_cfg.pic_info = {
		//	rate = 6006,
		//	scale = 180000,
		//	width = 1920,
		//	height = 1080
		//};

		// bitrate config
		stream->bitrate_cfg.vbr_setting = IAV_BRC_SCBR;
		stream->bitrate_cfg.average_bitrate = stream->encode_avg_bitrate;
		stream->bitrate_cfg.qp_min_on_I = 1;
		stream->bitrate_cfg.qp_max_on_I = 51;
		stream->bitrate_cfg.qp_min_on_P = 1;
		stream->bitrate_cfg.qp_max_on_P = 51;
		stream->bitrate_cfg.qp_min_on_B = 1;
		stream->bitrate_cfg.qp_max_on_B = 51;
		stream->bitrate_cfg.skip_flag = 0;

	} else if (stream->stream_format.type == IAV_STREAM_TYPE_MJPEG) {

		// mjpeg config
		stream->mjpeg_cfg.chroma_format = JPEG_CHROMA_YUV420;
		stream->mjpeg_cfg.quality = stream->jpeg_quality;
		stream->mjpeg_cfg.reserved1 = 0;

	}

	if ((ret_val = vsrc_s2l_set_stream_configs(pData, stream)) < 0)
		return ret_val;

//	if ((ret_val = vsrc_s2l_get_stream_configs(pData, stream)) < 0)
//		return ret_val;

	stream->dsp_pts.started = 0;
	stream->hw_pts.started = 0;
	stream->last_dts64 = 0;
	stream->frame_counter = 0;

	stream->eos_sent = 0;

	return 0;
}

static int vsrc_s2l_config_all_streams(vsrc_s2l_dev_data_t *pData)
{
	pData->iav_active_stream = 0;

	for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
		vsrc_s2l_stream_t *stream = pData->streams + i;

		int ret_val = vsrc_s2l_config_one_stream(pData, stream);
		if (ret_val < 0)
			return ret_val;

		if (stream->stream_format.type != IAV_STREAM_TYPE_NONE) {
			pData->iav_active_stream |= 1 << stream->index;
		}
	}

	pData->stream_size_changed = 0;

	return 0;
}

#undef GET_DEV_DATA
#define GET_DEV_DATA(_pDev) \
	((vsrc_s2l_dev_data_t*)(_pDev)->pData)

// called when the video source filter is destroyed
static void vsrc_s2l_Close(vsrc_device_t *pDev)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);

	vsrc_s2l_unmap_bsb(pData);
	vsrc_s2l_unmap_overlay(pData);
	vsrc_s2l_close_iav(pData);

	avf_safe_free(pData->vin_config_str);
	avf_safe_free(pData->clock_mode_str);
}

// called when the video source filter is created
static int vsrc_s2l_Open(vsrc_device_t *pDev)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	int ret_val;

	if ((ret_val = vsrc_s2l_open_iav(pData)) < 0)
		return ret_val;

	if ((ret_val = vsrc_s2l_map_bsb(pData)) < 0) {
		vsrc_s2l_Close(pDev);
		return ret_val;
	}

	if ((ret_val = vsrc_s2l_map_overlay(pData)) < 0) {
		vsrc_s2l_Close(pDev);
		return ret_val;
	}

	return 0;
}

static int vsrc_s2l_GetMaxNumStream(vsrc_device_t *pDev)
{
	return MAX_ENCODE_STREAMS;
}

static vsrc_s2l_stream_t *vsrc_s2l_get_stream(vsrc_s2l_dev_data_t *pData, int index)
{
	if ((unsigned)index >= ARRAY_SIZE(pData->streams)) {
		AVF_LOGE("no such stream: %d", index);
		return NULL;
	}
	return pData->streams + index;
}

static int vsrc_s2l_GetStreamInfo(vsrc_device_t *pDev,
	int index, vsrc_stream_info_t *pStreamInfo)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_s2l_stream_t *stream;

	if ((stream = vsrc_s2l_get_stream(pData, index)) == NULL)
		return -1;

	pStreamInfo->fcc = 0;

	switch (stream->encode_type) {
	case IAV_STREAM_TYPE_NONE:
		::memset(pStreamInfo, 0, sizeof(*pStreamInfo));
		pStreamInfo->format = VSRC_FMT_DISABLED;
		break;

	case IAV_STREAM_TYPE_H264:
		pStreamInfo->format = VSRC_FMT_H264;

		pStreamInfo->width = stream->encode_width;
		pStreamInfo->height = stream->encode_height;

		pStreamInfo->framerate = stream->framerate;
		pStreamInfo->rate = stream->rate_90kHZ;
		pStreamInfo->scale = _90kHZ;

		pStreamInfo->fcc = MKFCC('A','M','B','A');

		pStreamInfo->extra.ar_x = stream->ar_x;
		pStreamInfo->extra.ar_y = stream->ar_y;

		pStreamInfo->extra.mode = 1;	// frame mode
		pStreamInfo->extra.M = stream->M;
		pStreamInfo->extra.N = stream->N;
		AVF_LOGI(C_CYAN "stream %d N = %d, ar %d:%d" C_NONE, index, 
			pStreamInfo->extra.N, pStreamInfo->extra.ar_x, pStreamInfo->extra.ar_y);

		pStreamInfo->extra.gop_model = VIDEO_GOP_MODEL;
		pStreamInfo->extra.idr_interval = VIDEO_IDR_INTERVAL;
		pStreamInfo->extra.rate = pStreamInfo->rate;
		pStreamInfo->extra.scale = pStreamInfo->scale;
		pStreamInfo->extra.bitrate = stream->encode_avg_bitrate;
		pStreamInfo->extra.bitrate_min = stream->encode_avg_bitrate;

		pStreamInfo->extra.idr_size = VIDEO_IDR_INTERVAL;
		::memset(pStreamInfo->extra.reserved, 0, sizeof(pStreamInfo->extra.reserved));

		break;

	case IAV_STREAM_TYPE_MJPEG:
		pStreamInfo->format = VSRC_FMT_MJPEG;

		pStreamInfo->width = stream->encode_width;
		pStreamInfo->height = stream->encode_height;

		pStreamInfo->framerate = stream->framerate;
		pStreamInfo->rate = stream->rate_90kHZ;
		pStreamInfo->scale = _90kHZ;

		break;

	default:
		AVF_LOGE("unknown encode_type %d", stream->encode_type);
		return -1;
	}

//	AVF_LOGD(C_GREEN "stream %d format %d, %dx%d, framerate: %d, rate: %d" C_NONE,
//		index, pStreamInfo->format, pStreamInfo->width, pStreamInfo->height,
//		pStreamInfo->framerate, pStreamInfo->rate);

	return 0;
}

// video_mode:fps:mirror_horz:mirror_vert:anti_flicker;
// bits:hdr_mode:enc_mode;
// exposure
#define DEFAULT_VIN_CONFIG	"auto:30:0:0:0;14:0:4"

#define CHECK_STRTOUL(_ptr, _pend, _c, _lbl) \
	do { \
		if (_pend == _ptr) { \
			AVF_LOGW("error at line %d", __LINE__); \
			goto _lbl; \
		} \
		_ptr = _pend; \
		if (*_ptr == _c) \
			_ptr++; \
	} while (0)

static int vsrc_s2l_SetVinConfig(vsrc_device_t *pDev, const char *config)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	const char *ptr = config;
	char *pend;
	char buffer[16];
	int size;
	uint32_t video_mode;
	int width, height;
	const fps_map_t *fps_map;
	int mirror_horizontal;
	int mirror_vertical;
	int anti_flicker;
	int bits;
	int hdr_mode;
	int enc_mode;
	int exposure;

	if (pData->vin_config_str && ::strcmp(pData->vin_config_str, config) == 0) {
		// not changed
		return 0;
	}

	// video_mode
	size = avf_get_word(ptr, buffer, sizeof(buffer), ':');
	if (size < 0) {
		AVF_LOGE("no video mode");
		goto BadConfig;
	}
	ptr += size;
	if (amba_find_video_mode_by_name(buffer, &video_mode, &width, &height) < 0) {
		AVF_LOGE("invalid video mode %s", buffer);
		goto BadConfig;
	}

	// fps
	size = avf_get_word(ptr, buffer, sizeof(buffer), ':');
	if (size < 0) {
		AVF_LOGE("no fps");
		goto BadConfig;
	}
	ptr += size;

	if ((fps_map = amba_find_fps_by_name(buffer)) == NULL) {
		AVF_LOGE("invalid fps %s", buffer);
		goto BadConfig;
	}

	mirror_horizontal = 0;
	mirror_vertical = 0;
	anti_flicker = 0;

	bits = 0;
	hdr_mode = AMBA_VIDEO_LINEAR_MODE;
	enc_mode = DSP_CURRENT_MODE;

	exposure = 0;	// use default

	mirror_horizontal = ::strtoul(ptr, &pend, 10);
	CHECK_STRTOUL(ptr, pend, ':', UseDefault);

	mirror_vertical = ::strtoul(ptr, &pend, 10);
	CHECK_STRTOUL(ptr, pend, ':', UseDefault);

	anti_flicker = ::strtoul(ptr, &pend, 10);
	CHECK_STRTOUL(ptr, pend, ';', UseDefault);

	bits = ::strtoul(ptr, &pend, 10);
	CHECK_STRTOUL(ptr, pend, ':', UseDefault);

	hdr_mode = ::strtoul(ptr, &pend, 10);
	CHECK_STRTOUL(ptr, pend, ':', UseDefault);

	enc_mode = ::strtoul(ptr, &pend, 10);
	CHECK_STRTOUL(ptr, pend, ';', UseDefault);

	exposure = ::strtoul(ptr, &pend, 10);
	// no more

UseDefault:
	pData->vin_config.source_id = 0;	// use default
	pData->vin_config.video_mode = video_mode;
	pData->vin_config.fps_q9 = fps_map->fps_q9;
	pData->vin_config.bits = bits;
	pData->vin_config.mirror_horizontal = mirror_horizontal;
	pData->vin_config.mirror_vertical = mirror_vertical;
	pData->vin_config.hdr_mode = hdr_mode;
	pData->vin_config.enc_mode = enc_mode;
	pData->vin_config.anti_flicker = anti_flicker;
	pData->vin_config.exposure = exposure;

	pData->vin_width = width;
	pData->vin_height = height;
	pData->fps = fps_map->fps;
	pData->framerate = fps_map->framerate;
	pData->vin_rate = amba_calc_rate_90k(fps_map->framerate, 1, 1);

	avf_safe_free(pData->vin_config_str);
	pData->vin_config_str = avf_strdup(config);

	pData->vin_config_changed = 1;

	return 0;

BadConfig:
	AVF_LOGE("bad VinConfig: %s", config);
	return -1;
}

static int vsrc_s2l_SetClockMode(vsrc_device_t *pDev, const char *mode)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);

	if (pData->clock_mode_str && ::strcmp(pData->clock_mode_str, mode) == 0) {
		// not changed
		return 0;
	}

	avf_safe_free(pData->clock_mode_str);
	pData->clock_mode_changed = 1;
	pData->clock_mode_str = avf_strdup(mode);

	return 0;
}

static int vsrc_s2l_GetVinConfig(vsrc_device_t *pDev, vin_config_info_t *info)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);

	memset(info, 0, sizeof(*info));

	info->bits = pData->vin_config.bits;
	info->mirror_horizontal = pData->vin_config.mirror_horizontal;
	info->mirror_vertical = pData->vin_config.mirror_vertical;
	info->hdr_mode = pData->vin_config.hdr_mode;
	info->enc_mode = pData->vin_config.enc_mode;
	info->anti_flicker = pData->vin_config.anti_flicker;
	info->exposure = pData->vin_config.exposure;

	strncpy((char*)info->dev_name, pData->vin_info.name, sizeof(info->dev_name));
	info->dev_name[sizeof(info->dev_name) - 1] = 0;

	info->source_id = pData->vin_config.source_id;
	info->video_mode = pData->vin_config.video_mode;
	info->fps_q9 = pData->vin_config.fps_q9;

	return 0;
}

// lcd:x_off,y_off,width,height
static int vsrc_s2l_ChangeVoutVideo(vsrc_device_t *pDev, const char *config)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	AVF_LOGD("ChangeVoutVideo: " C_YELLOW "%s" C_NONE, config);

	const char *ptr = config;
	char buffer[32];
	int size;

	int x_off;
	int y_off;
	int width;
	int height;

	vout_info_t *vout_info;
	int vout_id;
	iav_srcbuf_id srcbuf_id;
	srcbuf_info_t *p_srcbuf_info;
	srcbuf_info_t srcbuf_info;

	struct iav_srcbuf_format format;
	int rval;

	if (pData->state != STATE_PREVIEW && pData->state != STATE_ENCODING) {
		AVF_LOGE("ChangeVoutVideo must be called in preview/encoding state");
		return -1;
	}

	size = avf_get_word(ptr, buffer, sizeof(buffer), ':');
	if (size < 0)
		goto BadConfig;
	ptr += size;

	size = ::sscanf(ptr, "%d,%d,%d,%d", &x_off, &y_off, &width, &height);
	if (size != 4)
		goto BadConfig;

	if (::strcmp(buffer, "lcd") == 0) {
		vout_info = &pData->lcd_info;
		p_srcbuf_info = &pData->lcd_srcbuf_info;
		vout_id = VOUT_0;
		srcbuf_id = (iav_srcbuf_id)vsrc_s2l_get_lcd_srcbuf(pData);
	} else if (::strcmp(buffer, "hdmi") == 0) {
		vout_info = &pData->hdmi_info;
		p_srcbuf_info = &pData->hdmi_srcbuf_info;
		vout_id = VOUT_1;
		srcbuf_id = (iav_srcbuf_id)vsrc_s2l_get_hdmi_srcbuf(pData);
	} else {
		goto BadConfig;
	}

	if (!vout_info->enabled) {
		AVF_LOGE("%s not enabled", buffer);
		goto BadConfig;
	}

	vsrc_s2l_calc_srcbuf_size(pData, width, height, &srcbuf_info, 8);

	// if changed
	if (srcbuf_info.width == p_srcbuf_info->width &&
			srcbuf_info.height == p_srcbuf_info->height &&
			srcbuf_info.xoff == p_srcbuf_info->xoff &&
			srcbuf_info.yoff == p_srcbuf_info->yoff &&
			width == vout_info->video_width &&
			height == vout_info->video_height &&
			x_off == vout_info->video_xoff &&
			y_off == vout_info->video_yoff) {
		AVF_LOGI(C_GREEN "vout video not changed" C_NONE);
		return 0;
	}

	::memset(&format, 0, sizeof(format));

	format.buf_id = srcbuf_id;
	format.size.width = width;
	format.size.height = height;

	format.input.width = srcbuf_info.width;
	format.input.height = srcbuf_info.height;
	format.input.x = srcbuf_info.xoff;
	format.input.y = srcbuf_info.yoff;

	AVF_LOGD("change srcbuf %d", srcbuf_id);
	AVF_LOGD("    from x,y: " C_CYAN "%d,%d" C_NONE
		", width,height: " C_CYAN "%d,%d" C_NONE ", size: " C_CYAN "%d,%d" C_NONE,
		p_srcbuf_info->xoff, p_srcbuf_info->yoff,
		p_srcbuf_info->width, p_srcbuf_info->height,
		vout_info->video_width, vout_info->video_height);
	AVF_LOGD("    to x,y: " C_CYAN "%d,%d" C_NONE
		", width,height: " C_CYAN "%d,%d" C_NONE ", size: " C_CYAN "%d,%d" C_NONE,
		srcbuf_info.xoff, srcbuf_info.yoff,
		srcbuf_info.width, srcbuf_info.height,
		width, height);

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_SOURCE_BUFFER_FORMAT, &format);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_SET_SOURCE_BUFFER_FORMAT");
		return rval;
	}

	rval = avf_platform_display_change_vout_video(pData->iav_fd, vout_id, 0,
		x_off, y_off, width, height, vout_info);
	if (rval < 0)
		return rval;

	*p_srcbuf_info = srcbuf_info;

	return 0;

BadConfig:
	AVF_LOGE("bad VoutVideo: %s", config);
	return -1;
}

// "h264:1920x1080:1000000:8000000:4000000"
// "mjpeg:512x288x70"
// "still:0x0x95"
// "none:1024x768"
static int vsrc_s2l_SetVideoConfig(vsrc_device_t *pDev,
	int index, const char *config)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	const char *ptr = config;
	char buffer[32];
	int size;

	int is_still = 0;
	int encode_type;
	int encode_width;
	int encode_height;
	int bitrate_control = IAV_BRC_CBR;
	int min_bitrate = 0;
	int max_bitrate = 0;
	int avg_bitrate = 0;
	int jpeg_quality = 0;

	vsrc_s2l_stream_t *stream;

	if ((stream = vsrc_s2l_get_stream(pData, index)) == NULL)
		return -1;

	size = avf_get_word(ptr, buffer, sizeof(buffer), ':');
	if (size < 0)
		goto BadConfig;
	ptr += size;

	// encode format
	if (::strcmp(buffer, "h264") == 0) {

		encode_type = IAV_STREAM_TYPE_H264;
		if (::sscanf(ptr, "%dx%d:%d:%d:%d", 
			&encode_width, &encode_height,
			&min_bitrate, &max_bitrate, &avg_bitrate) != 5)
			goto BadConfig;

		// force use CBR
		min_bitrate = max_bitrate = avg_bitrate;
		bitrate_control = IAV_BRC_CBR;

	} else if (::strcmp(buffer, "mjpeg") == 0) {

		encode_type = IAV_STREAM_TYPE_MJPEG;
		if (::sscanf(ptr, "%dx%dx%d",
			&encode_width, &encode_height, &jpeg_quality) != 3)
			goto BadConfig;

	} else if (::strcmp(buffer, "still") == 0) {

		is_still = 1;
		encode_type = IAV_STREAM_TYPE_MJPEG;
		if (::sscanf(ptr, "%dx%dx%d",
			&encode_width, &encode_height, &jpeg_quality) != 3)
			goto BadConfig;

	} else if (::strcmp(buffer, "none") == 0) {

		encode_type = IAV_STREAM_TYPE_NONE;
		if (*ptr == 0) {
			encode_width = 0;
			encode_height = 0;
		} else {
			if (::sscanf(ptr, "%dx%d",
				&encode_width, &encode_height) != 2)
				goto BadConfig;
		}

	} else {
		goto BadConfig;
	}

	if (stream->is_still != is_still ||
			stream->encode_width != (uint32_t)encode_width || 
			stream->encode_height != (uint32_t)encode_height) {
		pData->stream_size_changed = 1;
	}

	stream->is_still = is_still;
	stream->encode_type = (enum iav_stream_type)encode_type;
	stream->encode_width = encode_width;
	stream->encode_height = encode_height;
	stream->encode_bitrate_control = bitrate_control;
	stream->encode_avg_bitrate = avg_bitrate;
	stream->encode_min_bitrate = min_bitrate;
	stream->encode_max_bitrate = max_bitrate;
	stream->jpeg_quality = jpeg_quality;

	calc_aspect_ratio(stream->encode_width, stream->encode_height,
		&stream->ar_x, &stream->ar_y);

	return 0;

BadConfig:
	AVF_LOGE("bad VideoConfig: %s", config);
	return -1;
}

// numerator:denomiator
static int vsrc_s2l_SetVideoFrameRateConfig(vsrc_device_t *pDev,
	int index, const char *config)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_s2l_stream_t *stream;
	int ratio_numerator;
	int ratio_denominator;

	if ((stream = vsrc_s2l_get_stream(pData, index)) == NULL)
		return -1;

	if (::sscanf(config, "%d:%d", &ratio_numerator, &ratio_denominator) != 2)
		goto BadConfig;

	if (ratio_numerator < 1 || ratio_numerator > 255)
		goto BadConfig;

	if (ratio_denominator < 1 || ratio_denominator > 255)
		goto BadConfig;

	stream->stream_fps.fps_multi = ratio_numerator;
	stream->stream_fps.fps_div = ratio_denominator;

	stream->framerate = amba_find_framerate(pData->framerate, 
		stream->stream_fps.fps_multi, stream->stream_fps.fps_div);

	stream->rate_90kHZ = amba_calc_rate_90k(pData->framerate,
		stream->stream_fps.fps_multi, stream->stream_fps.fps_div);

	stream->M = VIDEO_M;
	stream->N = pData->fps * stream->stream_fps.fps_multi / stream->stream_fps.fps_div;

	return 0;

BadConfig:
	AVF_LOGE("bad VideoFrameRateConfig: %s", config);
	return -1;
}

static int vsrc_s2l_UpdateVideoConfigs(vsrc_device_t *pDev)
{
	return 0;
}

static int vsrc_s2l_SetOverlayClut(vsrc_device_t *pDev,
	unsigned int clut_id, const uint32_t *pClut)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	uint8_t *ptr;

	if (pData->piav_overlay == NULL) {
		AVF_LOGE("no overlay for clut %d", clut_id);
		return -1;
	}

	if (clut_id >= NUM_CLUTS) {
		AVF_LOGE("no such clut %d", clut_id);
		return -1;
	}

	ptr = pData->pclut_addr + CLUT_ENTRY_SIZE * clut_id;
	::memcpy(ptr, pClut, CLUT_ENTRY_SIZE);

	return 0;
}

// exchange overlay info
static int vsrc_s2l_ConfigOverlay(vsrc_device_t *pDev,
	int index, vsrc_overlay_info_t *pOverlayInfo)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_s2l_stream_t *stream;
	iav_overlay_area *iav_area;
	vsrc_overlay_area_t *area;
	uint32_t total_size = 0;
	uint32_t area_size;
	uint8_t *data;
	int i;

	if (pData->piav_overlay == NULL) {
		AVF_LOGE("no overlay memory");
		return -1;
	}

	if ((stream = vsrc_s2l_get_stream(pData, index)) == NULL)
		return -1;

	data = pData->poverlay_addr[index];
	area = pOverlayInfo->area + 0;
	iav_area = stream->iav_overlay_config.area + 0;

	for (i = 0; i < NUM_OVERLAY_AREAS; i++, area++, iav_area++) {
		if (!area->valid) {
			// this area is not in use
			::memset(iav_area, 0, sizeof(*iav_area));
			continue;
		}

		if (area->clut_id >= NUM_CLUTS) {
			AVF_LOGE("bad clut_id: %d", area->clut_id);
			return -1;
		}

		// area->width = 
//		area->height = (area->height + 31) & ~31;
//		area->pitch = (area->width + 31) & ~31;

		if (area->width == 0) {
			area->width = stream->encode_width - area->x_off;
			if (area->width > 1920) {
				area->width = 1920;
			}
		}

		area->width = ROUND_DOWN(area->width, 32);
		area->pitch = ROUND_UP(area->width, 32);
		area->height = ROUND_UP(area->height, 4);
		// area->x_off =
		// area->y_off =

		area_size = area->height * area->pitch;
		if (total_size + area_size > pData->overlay_size) {
			AVF_LOGE("overlay area too large: %d %d: %d", index, i, area_size);
			::memset(iav_area, 0, sizeof(*iav_area));
			continue;
		}

		area->data = data;
		area->total_size = area_size;

		data += area_size;
		total_size += area_size;

		// set iav area
		iav_area->enable = 0;	// fixed in UpdateOverlay
		iav_area->width = area->width;
		iav_area->pitch = area->pitch;
		iav_area->height = area->height;
		iav_area->total_size = area->total_size;
		iav_area->start_x = area->x_off;
		iav_area->start_y = area->y_off;
		iav_area->clut_addr_offset = CLUT_ENTRY_SIZE * area->clut_id;
		iav_area->data_addr_offset = (u32)(area->data - pData->piav_overlay);
	}

	stream->overlay_configured = 1;

	return 0;
}

static int vsrc_s2l_UpdateOverlay(vsrc_device_t *pDev,
	int index, vsrc_overlay_info_t *pOverlayInfo)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_s2l_stream_t *stream;
	iav_overlay_area *iav_area;
	vsrc_overlay_area_t *area;
	int ret_val;
	int i;

	if (pData->state != STATE_PREVIEW && pData->state != STATE_ENCODING) {
		AVF_LOGE("UpdateOverlay: must be in preview or encoding state!");
		return -1;
	}

	if ((stream = vsrc_s2l_get_stream(pData, index)) == NULL)
		return -1;

	if (stream->encode_type == IAV_STREAM_TYPE_NONE)
		return 0;

	if (!stream->overlay_configured) {
		AVF_LOGE("overlay for stream %d not configured", index);
		return -1;
	}

	stream->iav_overlay_config.id = stream->index;
	stream->iav_overlay_config.enable = 0;

	area = pOverlayInfo->area + 0;
	iav_area = stream->iav_overlay_config.area + 0;
	for (i = 0; i < NUM_OVERLAY_AREAS; i++, area++, iav_area++) {
		iav_area->enable = area->visible;
		if (iav_area->enable) {
			stream->iav_overlay_config.enable = 1;
		}
	}

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_SET_OVERLAY_INSERT,
		&stream->iav_overlay_config);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_SET_OVERLAY_INSERT:%d", index);
		return ret_val;
	}

	return 0;
}

static int vsrc_s2l_RefreshOverlay(vsrc_device_t *pDev, int index, int area)
{
	// todo - clean cache after the content is modified
	return 0;
}

static int vsrc_s2l_SetImageControl(vsrc_device_t *pDev,
	const char *pName, const char *pValue)
{
	AVF_LOGE("SetImageControl");
	return 0;
}

static int vsrc_s2l_EnterIdle(vsrc_device_t *pDev)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	struct vindev_mode vin_mode;
	int ret_val;

	if (pData->state == STATE_IDLE)
		return 0;

	AVF_LOGI(C_YELLOW "EnterIdle" C_NONE);

	if ((ret_val = ::ioctl(pData->iav_fd, IAV_IOC_ENTER_IDLE, 0)) < 0) {
		AVF_LOGP("IAV_IOC_ENTER_IDLE");
		return ret_val;
	}

	vin_mode.vsrc_id = pData->vin_config.source_id;
	vin_mode.video_mode = AMBA_VIDEO_MODE_OFF;
	vin_mode.bits = pData->vin_config.bits;
	vin_mode.hdr_mode = pData->vin_config.hdr_mode;
	if ((ret_val = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SET_MODE, &vin_mode)) < 0) {
		AVF_LOGP("IAV_IOC_VIN_SET_MODE");
		// return ret_val;
	}

	pData->state = STATE_IDLE;
	return 0;
}

static int vsrc_s2l_set_clock(vsrc_s2l_dev_data_t *pData)
{
	pData->clock_mode_changed = 0;

	if (pData->clock_mode_str == NULL || pData->clock_mode_str[0] == 0) {
		AVF_LOGD("clock mode not set");
		return 0;
	}

	int ret_val = avf_platform_set_clock_mode(pData->clock_mode_str);

	return ret_val;
}

static int vsrc_s2l_EnterPreview(vsrc_device_t *pDev)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	int ret_val;

	if (pData->state == STATE_PREVIEW)
		return 0;

	if (pData->state != STATE_IDLE) {
		AVF_LOGE("EnterPreview: not in IDLE state: %d!", pData->state);
		return -1;
	}

	AVF_LOGI(C_YELLOW "EnterPreview" C_NONE);

	//---------------------------------------------------------
	// get vout info
	//---------------------------------------------------------

	vout_info_t *vout_info = &pData->lcd_info;
	avf_platform_display_get_vout_info(VOUT_0, vout_info);
	if (vout_info->enabled) {
		AVF_LOGD("lcd: " C_GREEN "%d,%d" C_NONE ", video: " C_GREEN "%d,%d" C_NONE,
			vout_info->vout_width, vout_info->vout_height,
			vout_info->video_width, vout_info->video_height);
	}

	vout_info = &pData->hdmi_info;
	avf_platform_display_get_vout_info(VOUT_1, vout_info);
	if (vout_info->enabled) {
		AVF_LOGD("hdmi: " C_GREEN "%d,%d" C_NONE ", video: " C_GREEN "%d,%d" C_NONE,
			vout_info->vout_width, vout_info->vout_height,
			vout_info->video_width, vout_info->video_height);
	}

	if (pData->hdmi_info.enabled && pData->lcd_info.enabled) {
		AVF_LOGE("lcd and hdmi cannot be enabled at the same time");
		return -1;
	}

	//---------------------------------------------------------
	// setup system clock
	//---------------------------------------------------------
	if ((ret_val = vsrc_s2l_set_clock(pData)) < 0)
		return ret_val;

	//---------------------------------------------------------
	// config vin
	//---------------------------------------------------------
	if (pData->vin_config_changed) {

		if ((ret_val = vsrc_s2l_set_video_mode(pData)) < 0)
			return ret_val;

		if ((ret_val = vsrc_s2l_set_framerate(pData)) < 0)
			return ret_val;

		if ((ret_val = vsrc_s2l_set_mirror_mode(pData)) < 0)
			return ret_val;

		if ((ret_val = vsrc_s2l_get_vin_info(pData)) < 0)
			return ret_val;

		pData->vin_config_changed = 0;
	}

	//---------------------------------------------------------
	// calculate sizes
	//---------------------------------------------------------
	if ((ret_val = vsrc_s2l_calc_still_size(pData)) < 0)
		return ret_val;

	if ((ret_val = vsrc_s2l_calc_main_srcbuf(pData)) < 0)
		return ret_val;

	if ((ret_val = vsrc_s2l_calc_vin_input(pData)) < 0)
		return ret_val;

	if ((ret_val = vsrc_s2l_calc_all_srcbufs(pData)) < 0)
		return ret_val;

	//---------------------------------------------------------
	// config resource
	//---------------------------------------------------------
	if ((ret_val = vsrc_s2l_config_resource(pData)) < 0)
		return ret_val;

	if ((ret_val = vsrc_s2l_config_srcbuf(pData)) < 0)
		return ret_val;

	//---------------------------------------------------------
	// config streams
	//---------------------------------------------------------
	if ((ret_val = vsrc_s2l_config_all_streams(pData)) < 0)
		return ret_val;

	//---------------------------------------------------------
	// enter preview
	//---------------------------------------------------------
	if ((ret_val = vsrc_s2l_start_preview(pData)) < 0)
		return ret_val;

	pData->state = STATE_PREVIEW;
	return 0;
}

// if vin config was changed, or any stream size/framerate was changed,
// we should enter idle and then preview to re-config the parameters
static int vsrc_s2l_NeedReinitPreview(vsrc_device_t *pDev)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);

	if (pData->vin_config_changed) {
		AVF_LOGD(C_YELLOW "vin_config_changed" C_NONE);
		return 1;
	}

	if (pData->stream_size_changed) {
		AVF_LOGD(C_YELLOW "stream_size_changed" C_NONE);
		return 1;
	}

	if (pData->clock_mode_changed) {
		AVF_LOGD(C_YELLOW "clock_mode_changed" C_NONE);
		return 1;
	}

	return 0;
}

static int vsrc_s2l_SupportStillCapture(vsrc_device_t *pDev)
{
	return 1;
}

static int vsrc_s2l_IsStillCaptureMode(vsrc_device_t *pDev)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	return pData->stream_of_preview >= 0;
}

static int vsrc_s2l_StartEncoding(vsrc_device_t *pDev)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	int ret_val;

	if (pData->state != STATE_PREVIEW) {
		AVF_LOGE("StartEncoding: not in preview state!");
		return -1;
	}

	// stream configs may be changed
	if ((ret_val = vsrc_s2l_config_all_streams(pData)) < 0)
		return ret_val;

	AVF_LOGI(C_YELLOW "StartEncoding(0x%x)" C_NONE, pData->iav_active_stream);

	pData->encoding_stopped = 0;
	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_START_ENCODE, pData->iav_active_stream);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_START_ENCODE");
		return ret_val;
	}

	pData->state = STATE_ENCODING;
	return 0;
}

static int vsrc_s2l_StopEncoding(vsrc_device_t *pDev)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	int ret_val;

	if (pData->state == STATE_PREVIEW)
		return 0;

	if (pData->state != STATE_ENCODING) {
		AVF_LOGE("StopEncoding: not in encoding state!");
		return -1;
	}

	AVF_LOGI(C_YELLOW "StopEncoding(0x%x)" C_NONE, pData->iav_active_stream);

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_STOP_ENCODE, pData->iav_active_stream);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_STOP_ENCODE");
		return ret_val;
	}

	pData->encoding_stopped = 1;
	pData->state = STATE_PREVIEW;

	return 0;
}

static void vsrc_s2l_update_pts(pts_item_t *item, uint32_t curr_pts)
{
	// use last 30 bits
	curr_pts &= PTS_MASK;

	if (!item->started) {
		// the first call after StartEncode()
		item->started = 1;
		item->last_pts = curr_pts;
		item->last_stream_pts = 0;
		return;
	}

	uint32_t delta;
	uint64_t pts64;

	if (curr_pts >= item->last_pts) {
		// PTS increased
		delta = curr_pts - item->last_pts;
		if (delta < PTS_DELTA) {
			// real increase
			pts64 = item->last_stream_pts + delta;
		} else {
			// wrap around, descreased
			delta = (item->last_pts + PTS_WRAP_INC) - curr_pts;
			pts64 = item->last_stream_pts - delta;
		}
	} else {
		// PTS decreased
		delta = item->last_pts - curr_pts;
		if (delta < PTS_DELTA) {
			// real decrease
			pts64 = item->last_stream_pts - delta;
		} else {
			// wrap around, increased
			delta = (curr_pts + PTS_WRAP_INC) - item->last_pts;
			pts64 = item->last_stream_pts + delta;
		}
	}

	item->last_pts = curr_pts;
	item->last_stream_pts = pts64;
}

static INLINE uint64_t vsrc_round_pts(uint64_t pts, uint32_t rate)
{
	pts += rate / 2;
	return pts - (pts % rate);
}

static uint64_t vsrc_s2l_fix_pts_idr(
	vsrc_s2l_dev_data_t *pData,
	vsrc_s2l_stream_t *stream,
	struct iav_framedesc *framedesc)
{
	vsrc_s2l_update_pts(&stream->dsp_pts, framedesc->dsp_pts);

#ifdef USE_HW_PTS
	// adjust pts to hw pts

	vsrc_s2l_update_pts(&stream->hw_pts, framedesc->hw_pts);
	uint64_t hw_pts64 = PTS_HW_TO_90k(stream->hw_pts.last_stream_pts);

#ifdef DEBUG_AVSYNC
	if (stream->encode_type == IAV_STREAM_TYPE_H264) {
		uint32_t delta = (uint32_t)(hw_pts64 - stream->dsp_pts.last_stream_pts);
		AVF_LOGD("stream %d dsp_pts: " LLD ", hw_pts: " LLD ", delta: %d",
			stream->index, stream->dsp_pts.last_stream_pts, hw_pts64, delta);
	}
#endif

	stream->dsp_pts.last_stream_pts = hw_pts64;

#endif

	return vsrc_round_pts(stream->dsp_pts.last_stream_pts, stream->rate_90kHZ);
}

static INLINE uint64_t vsrc_s2l_fix_pts(vsrc_s2l_stream_t *stream,
	struct iav_framedesc *framedesc)
{
	vsrc_s2l_update_pts(&stream->dsp_pts, framedesc->dsp_pts);
	return vsrc_round_pts(stream->dsp_pts.last_stream_pts, stream->rate_90kHZ);
}

static int vsrc_s2l_NewSegment(vsrc_device_t *pDev, uint32_t streamFlags)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);

	struct iav_stream_cfg stream_cfg;
	int ret_val;

	stream_cfg.id = streamFlags;
	stream_cfg.cid = IAV_H264_CFG_FORCE_IDR;

	ret_val = ::ioctl(pData->iav_fd, IAV_IOC_SET_STREAM_CONFIG, &stream_cfg);
	if (ret_val < 0) {
		AVF_LOGP("IAV_IOC_SET_STREAM_CONFIG:IAV_H264_CFG_FORCE_IDR");
		return ret_val;
	}

	return 0;
}

static int vsrc_s2l_ReadBits(vsrc_device_t *pDev,
	vsrc_bits_buffer_t *pBitsBuffer)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_s2l_stream_t *stream;
	struct iav_querydesc query_desc;
	struct iav_framedesc *frame_desc;
	int ret_val = 0;

	if (pData->encoding_stopped) {
		// JPEG stream doesn't return EOS, so generate it here
		for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
			stream = pData->streams + i;
			if (stream->encode_type != IAV_STREAM_TYPE_H264) {
				if (!stream->eos_sent) {
					pBitsBuffer->stream_id = stream->index;
					pBitsBuffer->is_eos = 1;
					stream->eos_sent = 1;
					return 0;
				}
			}
		}
	}

	while (true) {
		memset(&query_desc, 0, sizeof(query_desc));
		frame_desc = &query_desc.arg.frame;
		query_desc.qid = IAV_DESC_FRAME;
		frame_desc->id = pData->stream_of_preview;	// -1 if not still; else it is the preview stream
		frame_desc->data_addr_offset = (uint32_t)pData->piav_bsb;
		ret_val = ::ioctl(pData->iav_fd, IAV_IOC_QUERY_DESC, &query_desc);
		if (ret_val < 0) {
			if (errno == EAGAIN) {
				// when run by gdb, and chrome connects to mjpeg preview,
				// gdb will send us a signal - figure out why
				AVF_LOGW("IAV_IOC_QUERY_DESC error");
				continue;
			}
			AVF_LOGP("IAV_IOC_QUERY_DESC");
			return ret_val;
		}
		break;
	}

	if ((stream = vsrc_s2l_get_stream(pData, frame_desc->id)) == NULL) {
		AVF_LOGE("bad stream id: %d", frame_desc->id);
		return -1;
	}

	pBitsBuffer->stream_id = stream->index;
	pBitsBuffer->is_eos = frame_desc->stream_end;

	if (frame_desc->stream_end)
		return 0;

	pBitsBuffer->buffer1 = &pData->piav_bsb[frame_desc->data_addr_offset];
	pBitsBuffer->size1 = frame_desc->size;
	pBitsBuffer->buffer2 = NULL;
	pBitsBuffer->size2 = 0;
//	pBitsBuffer->pts64 = vsrc_s2l_fix_pts_idr(pData, stream, frame_desc);
	pBitsBuffer->length = stream->rate_90kHZ;
	pBitsBuffer->frame_counter = ++stream->frame_counter;

	switch (frame_desc->pic_type) {
	case IAV_PIC_TYPE_IDR_FRAME:
		pBitsBuffer->pic_type = PIC_H264_IDR;
		pBitsBuffer->dts64 = // dts == pts for IDR
		pBitsBuffer->pts64 = vsrc_s2l_fix_pts_idr(pData, stream, frame_desc);
		stream->last_dts64 = pBitsBuffer->dts64;
#ifdef AVF_DEBUG
		{
			avf_u32_t start_code = avf_read_be_32(pBitsBuffer->buffer1);
			if (start_code != 0x00000001) {
				AVF_LOGE("IDR corrupted: %x", start_code);
				return -1;
			}
		}
#endif
		break;

	case IAV_PIC_TYPE_I_FRAME:
		pBitsBuffer->pic_type = PIC_I;
		pBitsBuffer->dts64 = stream->last_dts64 + stream->rate_90kHZ;
		pBitsBuffer->pts64 = (stream->M == 1) ? pBitsBuffer->dts64 : vsrc_s2l_fix_pts(stream, frame_desc);
		stream->last_dts64 = pBitsBuffer->dts64;
		AVF_LOGW("I frame");
		break;

	case IAV_PIC_TYPE_P_FRAME:
		pBitsBuffer->pic_type = PIC_P;
		pBitsBuffer->dts64 = stream->last_dts64 + stream->rate_90kHZ;
		pBitsBuffer->pts64 = (stream->M == 1) ? pBitsBuffer->dts64 : vsrc_s2l_fix_pts(stream, frame_desc);
		stream->last_dts64 = pBitsBuffer->dts64;
#ifdef AVF_DEBUG
		{
			avf_u32_t start_code = avf_read_be_32(pBitsBuffer->buffer1);
			if (start_code != 0x00000001) {
				AVF_LOGE("p-frame corrupted: %x", start_code);
				return -1;
			}
		}
#endif
		break;

	case IAV_PIC_TYPE_B_FRAME:
		pBitsBuffer->pic_type = PIC_B;
		pBitsBuffer->dts64 = stream->last_dts64 + stream->rate_90kHZ;
		pBitsBuffer->pts64 = (stream->M == 1) ? pBitsBuffer->dts64 : vsrc_s2l_fix_pts(stream, frame_desc);
		stream->last_dts64 = pBitsBuffer->dts64;
		break;

	case IAV_PIC_TYPE_MJPEG_FRAME:
		pBitsBuffer->pic_type = PIC_JPEG;
		pBitsBuffer->dts64 = // same
		pBitsBuffer->pts64 = vsrc_s2l_fix_pts_idr(pData, stream, frame_desc);
#ifdef AVF_DEBUG
		{
			uint8_t *p = pBitsBuffer->buffer1;
			uint32_t size = pBitsBuffer->size1;
			if (avf_read_be_16(p) != 0xffd8 || avf_read_be_16(p + size - 2) != 0xffd9) {
				AVF_LOGE("bad jpeg frame");
			}
		}
#endif
		break;

	default:
		pBitsBuffer->pic_type = PIC_UNKNOWN;
		pBitsBuffer->dts64 = 
		pBitsBuffer->pts64 = vsrc_s2l_fix_pts_idr(pData, stream, frame_desc);
		AVF_LOGE("unknown pic type");
		return -1;
	}

	return 0;
}

static void vsrc_s2l_Release(vsrc_device_t *pDev)
{
	vsrc_s2l_dev_data_t *pData = GET_DEV_DATA(pDev);
	pData->data_in_use = 0;
	pDev->pData = NULL;
}

int vsrc_Init(vsrc_device_t *pDev)
{
	static vsrc_s2l_dev_data_t data;

	vsrc_s2l_dev_data_t *pData;

	if (data.data_in_use) {
		AVF_LOGE("can only create 1 vsrc instance");
		return -1;
	}

	data.data_in_use = 1;
	pDev->pData = pData = &data;

	pData->iav_fd = -1;
	pData->state = STATE_UNKNOWN;

	pData->piav_bsb = NULL;
	pData->iav_bsb_size = 0;
	pData->piav_overlay = NULL;
	pData->iav_overlay_size = 0;

	// default vin config
	pData->vin_config_str = NULL;
	if (vsrc_s2l_SetVinConfig(pDev, DEFAULT_VIN_CONFIG) < 0) {
		return -1;
	}

	pData->clock_mode_str = NULL;

	pData->vin_config_changed = 1;
	pData->stream_size_changed = 1;
	pData->clock_mode_changed = 1;

	// streams default config
	for (int i = 0; i < MAX_ENCODE_STREAMS; i++) {
		vsrc_s2l_stream_t *stream = pData->streams + i;

		stream->encode_type = IAV_STREAM_TYPE_NONE;
		//stream->srcbuf_id = 0; // fixed later
		stream->index = i;

		stream->is_still = 0;
		stream->overlay_configured = 0;

		stream->stream_fps.fps_multi = 1;
		stream->stream_fps.fps_div = 1;
	}

	// device control
	pDev->Release = vsrc_s2l_Release;
	pDev->Open = vsrc_s2l_Open;
	pDev->Close = vsrc_s2l_Close;

	// device info
	pDev->GetMaxNumStream = vsrc_s2l_GetMaxNumStream;
	pDev->GetStreamInfo = vsrc_s2l_GetStreamInfo;

	// vin
	pDev->SetVinConfig = vsrc_s2l_SetVinConfig;
	pDev->SetClockMode = vsrc_s2l_SetClockMode;
	pDev->GetVinConfig = vsrc_s2l_GetVinConfig;

	// vout
	pDev->ChangeVoutVideo = vsrc_s2l_ChangeVoutVideo;

	// video streams
	pDev->SetVideoConfig = vsrc_s2l_SetVideoConfig;
	pDev->SetVideoFrameRateConfig = vsrc_s2l_SetVideoFrameRateConfig;
	pDev->UpdateVideoConfigs = vsrc_s2l_UpdateVideoConfigs;

	// video overlays
	pDev->SetOverlayClut = vsrc_s2l_SetOverlayClut;
	pDev->ConfigOverlay = vsrc_s2l_ConfigOverlay;
	pDev->UpdateOverlay = vsrc_s2l_UpdateOverlay;
	pDev->RefreshOverlay = vsrc_s2l_RefreshOverlay;

	// image control
	pDev->SetImageControl = vsrc_s2l_SetImageControl;

	// state control
	pDev->EnterIdle = vsrc_s2l_EnterIdle;
	pDev->EnterPreview = vsrc_s2l_EnterPreview;
	pDev->StartEncoding = vsrc_s2l_StartEncoding;
	pDev->StopEncoding = vsrc_s2l_StopEncoding;
	pDev->NeedReinitPreview = vsrc_s2l_NeedReinitPreview;
	pDev->SupportStillCapture = vsrc_s2l_SupportStillCapture;
	pDev->IsStillCaptureMode = vsrc_s2l_IsStillCaptureMode;

	// stream control
	pDev->NewSegment = vsrc_s2l_NewSegment;
	pDev->ReadBits = vsrc_s2l_ReadBits;

	return 0;
}

