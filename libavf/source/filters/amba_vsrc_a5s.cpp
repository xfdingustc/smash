
#define LOG_TAG "amba_vsrc"

#include <fcntl.h>
#include <sys/ioctl.h>

#include <basetypes.h>
#include <ambas_common.h>
#include <ambas_vin.h>
#include <ambas_vout.h>
#include <iav_drv.h>

#include "avf_common.h"
#include "avf_util.h"
#include "avf_new.h"

#define ENABLE_AAA

#include "amba_vsrc.h"
#include "amba_vsrc_priv.h"

#include "avf_plat_config.h"

#include "amba_vsrc_util.cpp"

#include "img/amba_aaa_calib.cpp"
#include "img/amba_aaa_a5s.cpp"

/*===========================================================================*/

typedef struct vin_config_s {
	uint32_t		source_id;
	uint32_t		video_mode;
	uint32_t		fps_q9;	// for iav
	uint8_t			mirror_horizontal;
	uint8_t			mirror_vertical;
	uint8_t			bayer_pattern;
	uint8_t			anti_flicker;
} vin_config_t;

typedef struct vsrc_stream_s {
	int				encode_type;
	uint32_t		encode_width;
	uint32_t		encode_height;

	int				srcbuf_id;
	uint32_t		srcbuf_width;
	uint32_t		srcbuf_height;

	int				jpeg_quality;
	int				encode_bitrate_control;
	uint32_t		encode_avg_bitrate;
	uint32_t		encode_min_bitrate;	// for VBR
	uint32_t		encode_max_bitrate;	// for VBR

	// encoding use
	int8_t			overlay_configured;
	int8_t			encoding_started;
	int8_t			eos_sent;
	uint32_t		index;	// index of this stream: 0, 1, 2
	uint32_t		last_dsp_pts;
	uint64_t		last_stream_pts;
	int				rate_90kHZ;
	int				ar_x;
	int				ar_y;

	// for IAV
	iav_encode_format_ex_t				iav_encode_format;
	iav_h264_config_ex_t				iav_h264_config;
	iav_bitrate_info_ex_t				iav_bitrate_info;
	iav_jpeg_config_ex_t				iav_jpeg_config;
	overlay_insert_ex_t					iav_overlay_config;
	iav_change_framerate_factor_ex_t	iav_framerate_config;
} vsrc_stream_t;

// provide 2 streams
typedef struct vsrc_dev_data_s {
	int8_t					data_in_use;
	int8_t					encoding_stopped;

	int						iav_fd;
	enum vsrc_status_e		state;

	iav_mmap_info_t			bsb_map;
	iav_mmap_info_t			overlay_map;

	uint8_t					*pclut_addr;
	uint32_t				overlay_size;
	uint8_t					*poverlay_addr[MAX_NUM_STREAMS];

	// vin config
	vin_config_t			vin_config;
	int						vin_width;	// -1: read from iav
	int						vin_height;	// -1: read from iav
	uint8_t					fps;			// user config
	uint8_t 				framerate;	// FrameRate_Unknown, for video_source
	uint32_t				vin_rate;	// rate per 90kHZ
	char					*vin_config_str;	// need free

	uint32_t				vin_input_width;		// input from vin
	uint32_t				vin_input_height;	// input from vin
	uint32_t				main_srcbuf_width;	// output to main srcbuf
	uint32_t				main_srcbuf_height;	// output to main srcbuf
	int						stream_of_main;		// which stream uses main srcbuf

	// vin info
	struct amba_video_info			vin_video_info;
	struct amba_vin_source_info 	vin_source_info;

	// vout info
	vout_info_t		lcd_info;
	vout_info_t 	hdmi_info;

	// global configs for iav
	iav_system_setup_info_ex_t			iav_system_setup_info;
	iav_source_buffer_format_all_ex_t	iav_buffer_format;
	iav_source_buffer_type_all_ex_t		iav_buffer_type;
	iav_system_resource_setup_ex_t		iav_system_resource;

	uint32_t			iav_active_stream;

	int8_t				vin_config_changed;	// vin params is changed
	int8_t				stream_size_changed;	// stream size is changed

	vsrc_stream_t		streams[MAX_ENCODE_STREAMS];
	iav_bsb_fullness_t	fullness;
} vsrc_dev_data_t;

/*===========================================================================*/

// stream 0: main h264 stream
// stream 1: mjpeg preview stream
// stream 2: second h264 stream

// srcbuf:
// 3rd - HDMI (VOUT-B, vout1)
// 4th - LCD (VOUT-A, vout0)

static int vsrc_open_iav(vsrc_dev_data_t *pData)
{
	if ((pData->iav_fd = avf_open_file("/dev/iav", O_RDWR, 0)) < 0) {
		AVF_LOGP("/dev/iav");
		return pData->iav_fd;
	}

	return 0;
}

static void vsrc_close_iav(vsrc_dev_data_t *pData)
{
	avf_safe_close(pData->iav_fd);
}

static int vsrc_map_share_mem(vsrc_dev_data_t *pData, iav_mmap_info_t *mmap, int cmd)
{
	int rval;

	if (mmap->addr != NULL)
		return 0;

	if ((rval = ::ioctl(pData->iav_fd, cmd, mmap)) < 0) {
		AVF_LOGP("vsrc_map_share_mem");
		return rval;
	}

	return 0;
}

static int vsrc_unmap_share_mem(vsrc_dev_data_t *pData, iav_mmap_info_t *mmap, int cmd)
{
	int rval;

	if (mmap->addr == NULL || mmap->length == 0)
		return 0;

	if ((rval = ::ioctl(pData->iav_fd, cmd, 0)) < 0) {
		AVF_LOGP("vsrc_unmap_share_mem");
	}

	mmap->addr = NULL;
	mmap->length = 0;

	return 0;
}

static int vsrc_map_bsb(vsrc_dev_data_t *pData)
{
	return vsrc_map_share_mem(pData, &pData->bsb_map, IAV_IOC_MAP_BSB);
}

static void vsrc_unmap_bsb(vsrc_dev_data_t *pData)
{
	vsrc_unmap_share_mem(pData, &pData->bsb_map, IAV_IOC_UNMAP_BSB);
}

static int vsrc_map_overlay(vsrc_dev_data_t *pData)
{
	int rval = vsrc_map_share_mem(pData, &pData->overlay_map, IAV_IOC_MAP_OVERLAY);
	if (rval < 0)
		return rval;

	pData->pclut_addr = pData->overlay_map.addr;
	pData->overlay_size = (pData->overlay_map.length - OVERLAY_OFFSET) / MAX_NUM_STREAMS;

	uint8_t *overlay_addr = pData->pclut_addr + OVERLAY_OFFSET;
	for (int i = 0; i < MAX_NUM_STREAMS; i++) {
		pData->poverlay_addr[i] = overlay_addr + pData->overlay_size * i;
	}

	return 0;
}

static void vsrc_unmap_overlay(vsrc_dev_data_t *pData)
{
	vsrc_unmap_share_mem(pData, &pData->overlay_map, IAV_IOC_UNMAP_OVERLAY);
}

static int vsrc_vin_set_src(vsrc_dev_data_t *pData)
{
	int rval;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SET_CURRENT_SRC,
		&pData->vin_config.source_id);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_VIN_SET_CURRENT_SRC");
		return rval;
	}

	return 0;
}

static int vsrc_vin_set_video_mode(vsrc_dev_data_t *pData)
{
	uint32_t mode;
	int rval;

	mode = pData->vin_config.video_mode;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SRC_SET_VIDEO_MODE, mode);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_VIN_SRC_SET_VIDEO_MODE");
		return rval;
	}

	struct amba_video_info *vin_video_info = &pData->vin_video_info;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SRC_GET_VIDEO_INFO, vin_video_info);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_VIN_SRC_GET_VIDEO_INFO failed!");
		return rval;
	}

	pData->vin_width = vin_video_info->width;
	pData->vin_height = vin_video_info->height;

	AVF_LOGD(C_CYAN "vin size: %d,%d" C_NONE, pData->vin_width, pData->vin_height);

	return 0;
}

static int vsrc_vin_set_framerate(vsrc_dev_data_t *pData)
{
	uint32_t fps_q9;
	int rval;

	fps_q9 = pData->vin_config.fps_q9;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SRC_SET_FRAME_RATE, fps_q9);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_VIN_SRC_SET_FRAME_RATE");
		return rval;
	}

	return 0;
}

static int vsrc_vin_set_mirror_mode(vsrc_dev_data_t *pData)
{
	struct amba_vin_src_mirror_mode mirror_mode;
	int rval;

	if (pData->vin_config.mirror_horizontal) {
		if (pData->vin_config.mirror_vertical)
			mirror_mode.pattern = AMBA_VIN_SRC_MIRROR_HORRIZONTALLY_VERTICALLY;
		else
			mirror_mode.pattern = AMBA_VIN_SRC_MIRROR_HORRIZONTALLY;
	} else {
		if (pData->vin_config.mirror_vertical)
			mirror_mode.pattern = AMBA_VIN_SRC_MIRROR_VERTICALLY;
		else
			mirror_mode.pattern = AMBA_VIN_SRC_MIRROR_NONE;
	}

	// bayer_pattern
	mirror_mode.bayer_pattern = pData->vin_config.bayer_pattern;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SRC_SET_MIRROR_MODE, &mirror_mode);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_VIN_SRC_SET_MIRROR_MODE");
		return rval;
	}

	return 0;
}

static int vsrc_vin_set_anti_flicker(vsrc_dev_data_t *pData)
{
	uint32_t anti_flicker;
	int rval;

	anti_flicker = pData->vin_config.anti_flicker;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SRC_SET_ANTI_FLICKER, anti_flicker);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_VIN_SRC_SET_ANTI_FLICKER");
		return rval;
	}

	return 0;
}

static int vsrc_vin_get_source_info(vsrc_dev_data_t *pData)
{
	int rval;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_VIN_SRC_GET_INFO, &pData->vin_source_info);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_VIN_SRC_GET_INFO");
		return rval;
	}

	return 0;
}

static int vsrc_set_digital_zoom(vsrc_dev_data_t *pData)
{
	struct iav_digital_zoom_ex_s zoom;
	int rval;

	zoom.zoom_factor_x = (pData->main_srcbuf_width << 16) / pData->vin_input_width;
	zoom.zoom_factor_y = (pData->main_srcbuf_height << 16) / pData->vin_input_height;
//	zoom.zoom_factor_x = (1<<16);
//	zoom.zoom_factor_y = (1<<16);
	zoom.center_offset_x = 0;
	zoom.center_offset_y = 0;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_DIGITAL_ZOOM_EX, &zoom);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_SET_DIGITAL_ZOOM_EX");
		return rval;
	}

	return 0;
}

static int vsrc_start_preview(vsrc_dev_data_t *pData)
{
	int rval;

	AVF_LOGD("call vsrc_start_preview");

	rval = ::ioctl(pData->iav_fd, IAV_IOC_ENABLE_PREVIEW, 0);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_ENTER_PREVIEW");
		return rval;
	}

	if ((rval = vsrc_set_digital_zoom(pData)) < 0)
		return rval;

#ifdef ENABLE_AAA

	char name[32];
	CThread::GetThreadName(name);
	CThread::SetThreadName("aaa_thread");

	rval = amba_aaa_start(pData->iav_fd,
			pData->vin_source_info.name,
			pData->vin_config.fps_q9,
			pData->vin_video_info.width,
			pData->vin_video_info.height,
			pData->vin_config.mirror_vertical);
	if (rval < 0) {
		AVF_LOGE("amba_aaa_start() failed %d", rval);
	}

	CThread::SetThreadName(name);

#endif

	return 0;
}

// must be in IDLE state
static int vsrc_config_iav_resource(vsrc_dev_data_t *pData)
{
	int rval;

	iav_system_resource_setup_ex_t *iav_sys_resource = &pData->iav_system_resource;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_GET_SYSTEM_RESOURCE_LIMIT_EX, iav_sys_resource);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_GET_SYSTEM_RESOURCE_LIMIT_EX");
		return rval;
	}

	// note: hardcode these values, otherwise need enter IDLE

	// main buffer for main stream
	iav_sys_resource->main_source_buffer_max_width = 1920;		// 2592
	iav_sys_resource->main_source_buffer_max_height = 1080;		// 2048

	// 2nd buffer for second stream
	iav_sys_resource->second_source_buffer_max_width = 720;		// 720
	iav_sys_resource->second_source_buffer_max_height = 576;	// 576

	// for hdmi (VOUT_1) or preview stream
	iav_sys_resource->third_source_buffer_max_width = 1280;		// 1280
	iav_sys_resource->third_source_buffer_max_height = 720;		// 720

	// for lcd (VOUT_0) or preview stream
	iav_sys_resource->fourth_source_buffer_max_width = 1280;	// 1280
	iav_sys_resource->fourth_source_buffer_max_height = 720;	// 720

	// iav_sys_resource->vout_max_width = 720;
	// iav_sys_resource->vout_max_height = 576;
	// iav_sys_resource->stream_max_GOP_M = {1,1,1,1,0,0,0,0}
	// iav_sys_resource->stream_max_GOP_N = {255,255,255,255,0,0,0,0}
	// iav_sys_resource->stream_max_advanced_quality_model = {6,6,6,6,0,0,0,0}

	iav_sys_resource->stream_max_encode_size[0].width = 1920;
	iav_sys_resource->stream_max_encode_size[0].height = 1080;

	iav_sys_resource->stream_max_encode_size[1].width = 1024;	// **
	iav_sys_resource->stream_max_encode_size[1].height = 720;	// **

	iav_sys_resource->stream_max_encode_size[2].width = 1024;
	iav_sys_resource->stream_max_encode_size[2].height = 720;

	iav_sys_resource->stream_max_encode_size[3].width = 1024;
	iav_sys_resource->stream_max_encode_size[3].height = 720;

	// iav_sys_resource->MCTF_possible = 1;
	iav_sys_resource->max_num_encode_streams = MAX_ENCODE_STREAMS;
	// iav_sys_resource->max_num_cap_sources = 3;
	// iav_sys_resource->total_memory_size = 2;
	// iav_sys_resource->cavlc_max_bitrate = 1500000;
	// iav_sys_resource->encode_mode
	// iav_sys_resource->rotate_possible
	// iav_sys_resource->hdr_exposures_num
	// iav_sys_resource->reserved

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_SYSTEM_RESOURCE_LIMIT_EX, iav_sys_resource);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_SET_SYSTEM_RESOURCE_LIMIT_EX");
		return rval;
	}

	return 0;
}

static int vsrc_get_srcbuf_format(
	vsrc_dev_data_t *pData, int srcbuf_id,
	int buffer_type, vout_info_t *vout_info,
	uint16_t *width, uint16_t *height,
	uint16_t *input_width, uint16_t *input_height,
	uint16_t *deintlc_for_intlc_vin)
{
	*width = 0;
	*height = 0;
	*input_width = pData->main_srcbuf_width;
	*input_height = pData->main_srcbuf_height;
	*deintlc_for_intlc_vin = 0;

	switch (buffer_type) {
	case IAV_SOURCE_BUFFER_TYPE_OFF:
		*input_width = 0;
		*input_height = 0;
		break;

	case IAV_SOURCE_BUFFER_TYPE_ENCODE:
		for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
			vsrc_stream_t *stream = pData->streams + i;
			if (stream->srcbuf_id == srcbuf_id) {
				*width = stream->srcbuf_width;
				*height = stream->srcbuf_height;
				break;
			}
		}
		if (*width == 0 || *height == 0) {
			AVF_LOGE("scrbuf %d is for encode, size: %d, %d", srcbuf_id, *width, *height);
			return -1;
		}
		break;

	case IAV_SOURCE_BUFFER_TYPE_PREVIEW:
		if (vout_info == NULL) {
			AVF_LOGE("unexpected");
			return -1;
		}
		*width = vout_info->video_width;
		*height = vout_info->video_height;
		break;

	default:
		AVF_LOGE("unexpected");
		return -1;
	}

	AVF_LOGD("srcbuf %d fmt: " C_CYAN "size: %d,%d, input size: %d,%d" C_NONE,
		srcbuf_id, *width, *height, *input_width, *input_height);

	return 0;
}

// can be in IDLE/PREVIEW/ENCODING state
// if not in IDLE, main srcbuf will not take effect
static int vsrc_set_srcbuf_format(vsrc_dev_data_t *pData)
{
	int rval;

	AVF_LOGD("vsrc_set_srcbuf_format");

	iav_source_buffer_format_all_ex_t *iav_buffer_format = &pData->iav_buffer_format;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_GET_SOURCE_BUFFER_FORMAT_ALL_EX, iav_buffer_format);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_GET_SOURCE_BUFFER_FORMAT_ALL_EX");
		return rval;
	}

	iav_buffer_format->main_width = pData->main_srcbuf_width;
	iav_buffer_format->main_height = pData->main_srcbuf_height;
	iav_buffer_format->main_deintlc_for_intlc_vin = 0;

	AVF_LOGD("main srcbuf: " C_CYAN "%d,%d" C_NONE,
		pData->main_srcbuf_width, pData->main_srcbuf_height);

	iav_source_buffer_type_all_ex_t *iav_buffer_type = &pData->iav_buffer_type;

	rval = vsrc_get_srcbuf_format(pData, IAV_ENCODE_SOURCE_SECOND_BUFFER,
		iav_buffer_type->second_buffer_type, NULL,
		&iav_buffer_format->second_width,
		&iav_buffer_format->second_height,
		&iav_buffer_format->second_input_width,
		&iav_buffer_format->second_input_height,
		&iav_buffer_format->second_deintlc_for_intlc_vin);
	if (rval < 0)
		return rval;

	rval = vsrc_get_srcbuf_format(pData, IAV_ENCODE_SOURCE_THIRD_BUFFER,
		iav_buffer_type->third_buffer_type, &pData->hdmi_info,
		&iav_buffer_format->third_width,
		&iav_buffer_format->third_height,
		&iav_buffer_format->third_input_width,
		&iav_buffer_format->third_input_height,
		&iav_buffer_format->third_deintlc_for_intlc_vin);
	if (rval < 0)
		return rval;

	rval = vsrc_get_srcbuf_format(pData, IAV_ENCODE_SOURCE_FOURTH_BUFFER,
		iav_buffer_type->fourth_buffer_type, &pData->lcd_info,
		&iav_buffer_format->fourth_width,
		&iav_buffer_format->fourth_height,
		&iav_buffer_format->fourth_input_width,
		&iav_buffer_format->fourth_input_height,
		&iav_buffer_format->fourth_deintlc_for_intlc_vin);
	if (rval < 0)
		return rval;

	iav_buffer_format->intlc_scan = 0;
	iav_buffer_format->second_unwarp = 0;
	iav_buffer_format->third_unwarp = 0;
	iav_buffer_format->fourth_unwarp = 0;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_SOURCE_BUFFER_FORMAT_ALL_EX, iav_buffer_format);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_SET_SOURCE_BUFFER_FORMAT_ALL_EX");
		return rval;
	}

	return 0;
}

// must be in IDLE state
static int vsrc_set_srcbuf_type(vsrc_dev_data_t *pData)
{
	int rval;

	iav_source_buffer_type_all_ex_t *iav_buffer_type = &pData->iav_buffer_type;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_GET_SOURCE_BUFFER_TYPE_ALL_EX, iav_buffer_type);
	if (rval < 0)
		return rval;

	iav_buffer_type->main_buffer_type = IAV_SOURCE_BUFFER_TYPE_ENCODE;
	iav_buffer_type->second_buffer_type = IAV_SOURCE_BUFFER_TYPE_ENCODE;

	if (pData->hdmi_info.enabled) {
		iav_buffer_type->third_buffer_type = IAV_SOURCE_BUFFER_TYPE_PREVIEW; // hdmi
		iav_buffer_type->fourth_buffer_type = IAV_SOURCE_BUFFER_TYPE_ENCODE; // preview stream
	} else if (pData->lcd_info.enabled) {
		iav_buffer_type->third_buffer_type = IAV_SOURCE_BUFFER_TYPE_ENCODE; // preview stream
		iav_buffer_type->fourth_buffer_type = IAV_SOURCE_BUFFER_TYPE_PREVIEW; // lcd
	} else {
		iav_buffer_type->third_buffer_type = IAV_SOURCE_BUFFER_TYPE_ENCODE; // preview stream
		iav_buffer_type->fourth_buffer_type = IAV_SOURCE_BUFFER_TYPE_OFF; // no use
	}

	AVF_LOGD("srcbuf type: " C_CYAN "%d %d %d %d" C_NONE,
		iav_buffer_type->main_buffer_type,
		iav_buffer_type->second_buffer_type,
		iav_buffer_type->third_buffer_type,
		iav_buffer_type->fourth_buffer_type);

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_SOURCE_BUFFER_TYPE_ALL_EX, iav_buffer_type);
	if (rval < 0)
		return rval;

	return 0;
}

// can cover all codeded streams
static int vsrc_calc_main_srcbuf_size(vsrc_dev_data_t *pData, 
	uint32_t *main_srcbuf_width, uint32_t *main_srcbuf_height)
{
	*main_srcbuf_width = 0;
	*main_srcbuf_height = 0;

	for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
		vsrc_stream_t *stream = pData->streams + i;
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

	return 0;
}

// get the largest size for all streams, use it as the main buffer size
static int vsrc_calc_main_srcbuf(vsrc_dev_data_t *pData)
{
	pData->streams[0].srcbuf_id = IAV_ENCODE_SOURCE_MAIN_BUFFER;
	pData->streams[1].srcbuf_id = IAV_ENCODE_SOURCE_THIRD_BUFFER; // use HDMI buffer
	pData->streams[2].srcbuf_id = IAV_ENCODE_SOURCE_SECOND_BUFFER;

	if (pData->hdmi_info.enabled) {
		pData->streams[1].srcbuf_id = IAV_ENCODE_SOURCE_FOURTH_BUFFER; // use LCD buffer
		if (pData->lcd_info.enabled) {
			AVF_LOGE("lcd and hdmi cannot be both enabled");
			return -1;
		}
	}

	uint32_t main_srcbuf_width;
	uint32_t main_srcbuf_height;
	if (vsrc_calc_main_srcbuf_size(pData, &main_srcbuf_width, &main_srcbuf_height) < 0)
		return -1;

	pData->main_srcbuf_width = main_srcbuf_width;
	pData->main_srcbuf_height = main_srcbuf_height;

	AVF_LOGD(C_CYAN "main srcbuf size: %d,%d" C_NONE,
		pData->main_srcbuf_width, pData->main_srcbuf_height);

	// find the largest stream as the main stream
	pData->stream_of_main = 0;
	uint32_t max = 0;
	for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
		vsrc_stream_t *stream = pData->streams + i;
		uint32_t tmp = stream->encode_width * stream->encode_height;
		if (max < tmp) {
			max = tmp;
			pData->stream_of_main = i;
		}
	}

	// exchange the 2 streams: stream 0 no longer uses the main source buffer
	if (pData->stream_of_main != 0) {
		int tmp = pData->streams[pData->stream_of_main].srcbuf_id;
		pData->streams[pData->stream_of_main].srcbuf_id = IAV_ENCODE_SOURCE_MAIN_BUFFER;
		pData->streams[0].srcbuf_id = tmp;
	}

	return 0;
}

// get the max rect of vin input
// the input rect is proportional to main srcbuf, and smaller than vin input
// it is the input of the main buffer. it will be scaled to main buffer size
static int vsrc_calc_vin_input(vsrc_dev_data_t *pData)
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

	AVF_LOGD(C_CYAN "vin input size: %d,%d" C_NONE, pData->vin_input_width, pData->vin_input_height);

	return 0;
}

// calculate srcbuf size according to encode size
// srcbuf size is proportional to the main srcbuf, and bigger than the encode rect
static int vsrc_calc_srcbuf_size(vsrc_dev_data_t *pData, vsrc_stream_t *stream)
{
	int encode_width = stream->encode_width;
	int encode_height = stream->encode_height;

	if (encode_width <= 0 || encode_height <= 0) {
		stream->srcbuf_width = 0;
		stream->srcbuf_height = 0;
		return 0;
	}

	int main_width = pData->main_srcbuf_width;
	int main_height = pData->main_srcbuf_height;

	if (main_width * encode_height > main_height * encode_width) {
		// main is broader
		stream->srcbuf_height = encode_height;
		stream->srcbuf_width = (main_width * stream->srcbuf_height + main_height - 1) / main_height;
		stream->srcbuf_width = ROUND_UP(stream->srcbuf_width, 16);
	} else {
		// main is taller
		stream->srcbuf_width = encode_width;
		stream->srcbuf_height = (main_height * stream->srcbuf_width + main_width - 1) / main_width;
		stream->srcbuf_height = ROUND_UP(stream->srcbuf_height, 8);
	}

	AVF_LOGD(C_CYAN "stream %d srcbuf: %d, size: %d,%d" C_NONE, stream->index,
		stream->srcbuf_id, stream->srcbuf_width, stream->srcbuf_height);

	return 0;
}

static int vsrc_calc_all_srcbufs(vsrc_dev_data_t *pData)
{
	int rval;

	AVF_LOGD("vsrc_calc_srcbuf_size");
	for (int i = 0; i < MAX_ENCODE_STREAMS; i++) {
		if ((rval = vsrc_calc_srcbuf_size(pData, pData->streams + i)) < 0)
			return rval;
	}

	return 0;
}

static int vsrc_config_stream_encode_format(vsrc_dev_data_t *pData, vsrc_stream_t *stream)
{
	int rval;

	iav_encode_format_ex_t *iav_encode_format = &stream->iav_encode_format;
	iav_encode_format->id = 1 << stream->index;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_GET_ENCODE_FORMAT_EX, iav_encode_format);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_GET_ENCODE_FORMAT_EX");
		return rval;
	}

	iav_encode_format->encode_type = stream->encode_type;
	iav_encode_format->source = stream->srcbuf_id;
	iav_encode_format->encode_width = stream->encode_width;
	iav_encode_format->encode_height = stream->encode_height;
	iav_encode_format->encode_x = (stream->srcbuf_width - stream->encode_width) / 2;
	iav_encode_format->encode_y = (stream->srcbuf_height - stream->encode_height) / 2;

	AVF_LOGD("set stream %d format, " C_CYAN "type: %d, source: %d, size: %d,%d, x,y: %d,%d" C_NONE,
		stream->index, iav_encode_format->encode_type, iav_encode_format->source,
		iav_encode_format->encode_width, iav_encode_format->encode_height,
		iav_encode_format->encode_x, iav_encode_format->encode_y);

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_ENCODE_FORMAT_EX, iav_encode_format);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_SET_ENCODE_FORMAT_EX");
		return rval;
	}

	return 0;
}

static int vsrc_config_stream_h264(vsrc_dev_data_t *pData, vsrc_stream_t *stream)
{
	int rval;

	iav_h264_config_ex_t *iav_h264_config = &stream->iav_h264_config;
	iav_h264_config->id = 1 << stream->index;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_GET_H264_CONFIG_EX, iav_h264_config);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_GET_H264_CONFIG_EX");
		return rval;
	}

	iav_h264_config->M = 1;
	iav_h264_config->N = 
		// one IDR per second
		pData->fps * stream->iav_framerate_config.ratio_numerator / 
			stream->iav_framerate_config.ratio_denominator;
	iav_h264_config->gop_model = IAV_GOP_SIMPLE;

	AVF_LOGD("set stream %d format, " C_CYAN "h264 config: M=%d, N=%d, gop=%d" C_NONE,
		stream->index, iav_h264_config->M, iav_h264_config->N, iav_h264_config->gop_model);

	// not used
	// iav_h264_config->pic_info.ar_x = 16;
	// iav_h264_config->pic_info.ar_y = 9;
	// iav_h264_config->pic_info.rate : 3003
	// iav_h264_config->pic_info.scale : 90000

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_H264_CONFIG_EX, iav_h264_config);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_SET_H264_CONFIG_EX");
		return rval;
	}

	return 0;
}

static int vsrc_config_stream_bitrate(vsrc_dev_data_t *pData, vsrc_stream_t *stream)
{
	int rval;

	iav_bitrate_info_ex_t *iav_bitrate_info = &stream->iav_bitrate_info;
	iav_bitrate_info->id = 1 << stream->index;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_GET_BITRATE_EX, iav_bitrate_info);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_GET_BITRATE_EX");
		return rval;
	}

	iav_bitrate_info->rate_control_mode = (iav_rate_control_mode)stream->encode_bitrate_control;
	iav_bitrate_info->cbr_avg_bitrate = stream->encode_avg_bitrate;
	iav_bitrate_info->vbr_min_bitrate = stream->encode_min_bitrate;
	iav_bitrate_info->vbr_max_bitrate = stream->encode_max_bitrate;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_BITRATE_EX, iav_bitrate_info);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_SET_BITRATE_EX");
		return rval;
	}

	return 0;
}

static int vsrc_config_stream_jpeg(vsrc_dev_data_t *pData, vsrc_stream_t *stream)
{
	int rval;

	iav_jpeg_config_ex_t *iav_jpeg_config = &stream->iav_jpeg_config;
	iav_jpeg_config->id = stream->index;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_GET_JPEG_CONFIG_EX, iav_jpeg_config);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_GET_JPEG_CONFIG_EX");
		return rval;
	}

	iav_jpeg_config->chroma_format = IAV_JPEG_CHROMA_FORMAT_YUV420;
	iav_jpeg_config->quality = stream->jpeg_quality;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_JPEG_CONFIG_EX, iav_jpeg_config);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_SET_JPEG_CONFIG_EX");
		return rval;
	}

	return 0;
}

static int vsrc_config_stream_framerate(vsrc_dev_data_t *pData, vsrc_stream_t *stream)
{
	int rval;

	iav_change_framerate_factor_ex_t *iav_framerate_config = &stream->iav_framerate_config;
	iav_framerate_config->id = 1 << stream->index;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_CHANGE_FRAMERATE_FACTOR_EX, iav_framerate_config);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_CHANGE_FRAMERATE_FACTOR_EX");
		return rval;
	}

	AVF_LOGD("set stream %d format, " C_CYAN "framerate: %d:%d" C_NONE,
		stream->index, iav_framerate_config->ratio_numerator, iav_framerate_config->ratio_denominator);

	return 0;
}

static int vsrc_config_one_stream(vsrc_dev_data_t *pData, vsrc_stream_t *stream)
{
	int rval;

	if ((rval = vsrc_config_stream_encode_format(pData, stream)) < 0)
		return rval;

	if (stream->encode_type == IAV_ENCODE_H264) {

		if ((rval = vsrc_config_stream_h264(pData, stream)) < 0)
			return rval;

		if ((rval = vsrc_config_stream_bitrate(pData, stream)) < 0)
			return rval;

	} else if (stream->encode_type == IAV_ENCODE_MJPEG) {

		if ((rval = vsrc_config_stream_jpeg(pData, stream)) < 0)
			return rval;

	}

	stream->rate_90kHZ = amba_calc_rate_90k(pData->framerate,
		stream->iav_framerate_config.ratio_numerator,
		stream->iav_framerate_config.ratio_denominator);

	calc_aspect_ratio(stream->encode_width, stream->encode_height,
		&stream->ar_x, &stream->ar_y);

	if (stream->encode_type != IAV_ENCODE_NONE) {
		if ((rval = vsrc_config_stream_framerate(pData, stream)) < 0)
			return rval;
	}

	stream->encoding_started = 0;
	stream->eos_sent = 0;

	return 0;
}

static int vsrc_config_all_streams(vsrc_dev_data_t *pData)
{
	AVF_LOGD("vsrc_config_all_streams");

	pData->iav_active_stream = 0;
	for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {

		vsrc_stream_t *stream = pData->streams + i;
		if (stream->encode_type != IAV_ENCODE_NONE)
			pData->iav_active_stream |= 1 << stream->index;

		int rval = vsrc_config_one_stream(pData, stream);
		if (rval < 0)
			return rval;
	}

	return 0;
}

#undef GET_DEV_DATA
#define GET_DEV_DATA(_pDev) \
	((vsrc_dev_data_t*)(_pDev)->pData)

// called when the video source filter is destroyed
static void vsrc_Close(vsrc_device_t *pDev)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);

#ifdef ENABLE_AAA
	amba_aaa_deinit();
#endif

	vsrc_unmap_bsb(pData);
	vsrc_unmap_overlay(pData);
	vsrc_close_iav(pData);

	avf_safe_free(pData->vin_config_str);
}

// called when the video source filter is created
static int vsrc_Open(vsrc_device_t *pDev)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	int rval;

	if ((rval = vsrc_open_iav(pData)) < 0)
		return rval;

	if ((rval = vsrc_map_bsb(pData)) < 0) {
		vsrc_Close(pDev);
		return rval;
	}

	if ((rval = vsrc_map_overlay(pData)) < 0) {
		vsrc_Close(pDev);
		return rval;
	}

#ifdef ENABLE_AAA
	if ((rval = amba_aaa_init()) < 0) {
		vsrc_Close(pDev);
		return rval;
	}
#endif

	return 0;
}

static int vsrc_GetMaxNumStream(vsrc_device_t *pDev)
{
	return MAX_ENCODE_STREAMS;
}

static vsrc_stream_t *vsrc_get_stream(vsrc_dev_data_t *pData, int index)
{
	if ((unsigned)index >= ARRAY_SIZE(pData->streams)) {
		AVF_LOGE("no such stream: %d", index);
		return NULL;
	}
	return pData->streams + index;
}

static int vsrc_GetStreamInfo(vsrc_device_t *pDev, int index, vsrc_stream_info_t *pStreamInfo)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_stream_t *stream;

	if ((stream = vsrc_get_stream(pData, index)) == NULL)
		return -1;

	pStreamInfo->fcc = 0;

	switch (stream->encode_type) {
	case IAV_ENCODE_NONE:
		pStreamInfo->format = VSRC_FMT_DISABLED;
		break;

	case IAV_ENCODE_H264:
		pStreamInfo->format = VSRC_FMT_H264;
		pStreamInfo->width = stream->encode_width;
		pStreamInfo->height = stream->encode_height;
		pStreamInfo->framerate = amba_find_framerate(pData->framerate,
			stream->iav_framerate_config.ratio_numerator,
			stream->iav_framerate_config.ratio_denominator);
		pStreamInfo->rate = stream->rate_90kHZ;
		pStreamInfo->scale = _90kHZ;
		pStreamInfo->fcc = MKFCC('A','M','B','A');
		pStreamInfo->extra.ar_x = stream->ar_x;
		pStreamInfo->extra.ar_y = stream->ar_y;
		pStreamInfo->extra.mode = stream->iav_h264_config.pic_info.frame_mode;
		pStreamInfo->extra.M = stream->iav_h264_config.M;
		pStreamInfo->extra.N = stream->iav_h264_config.N;
		pStreamInfo->extra.gop_model = stream->iav_h264_config.gop_model;
		pStreamInfo->extra.idr_interval = stream->iav_h264_config.idr_interval;
		pStreamInfo->extra.rate = pStreamInfo->rate;
		pStreamInfo->extra.scale = pStreamInfo->scale;
		pStreamInfo->extra.bitrate = stream->iav_bitrate_info.cbr_avg_bitrate;
		pStreamInfo->extra.bitrate_min = stream->iav_bitrate_info.vbr_min_bitrate;
		pStreamInfo->extra.idr_size = 1;	// ??
		::memset(pStreamInfo->extra.reserved, 0, sizeof(pStreamInfo->extra.reserved));
		break;

	case IAV_ENCODE_MJPEG:
		pStreamInfo->format = VSRC_FMT_MJPEG;
		pStreamInfo->width = stream->encode_width;
		pStreamInfo->framerate = amba_find_framerate(pData->framerate,
			stream->iav_framerate_config.ratio_numerator,
			stream->iav_framerate_config.ratio_denominator);
		pStreamInfo->height = stream->encode_height;
		pStreamInfo->rate = stream->rate_90kHZ;
		pStreamInfo->scale = _90kHZ;
		break;

	default:
		return -1;
	}

	return 0;
}

// videomode:fps:mirror_horz:mirror_vert:anti_flicker
#define DEFAULT_VIN_CONFIG	"auto:29.97:0:0:0"

static int vsrc_SetVinConfig(vsrc_device_t *pDev, const char *config)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	const char *ptr = config;
	char buffer[16];
	int size;
	uint32_t video_mode;
	int width, height;
	const fps_map_t *fps_map;
	int mirror_horizontal;
	int mirror_vertical;
	int anti_flicker;

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
		AVF_LOGE("invalid fps");
		goto BadConfig;
	}

	// others
	if (::sscanf(ptr, "%d:%d:%d",
			&mirror_horizontal, &mirror_vertical, &anti_flicker) != 3) {
		AVF_LOGE("bad mirror");
		goto BadConfig;
	}

	pData->vin_config.source_id = 0;	// use default
	pData->vin_config.video_mode = video_mode;
	pData->vin_config.fps_q9 = fps_map->fps_q9;
	pData->vin_config.mirror_horizontal = mirror_horizontal;
	pData->vin_config.mirror_vertical = mirror_vertical;
	pData->vin_config.bayer_pattern = AMBA_VIN_SRC_BAYER_PATTERN_AUTO;
	pData->vin_config.anti_flicker = anti_flicker;

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

// "h264:1920x1080:1000000:8000000:4000000"
// "mjpeg:512x288x70"
// "none: 1024x768"
static int vsrc_SetVideoConfig(vsrc_device_t *pDev, int index, const char *config)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	const char *ptr = config;
	char buffer[32];
	int size;

	int encode_type;
	int encode_width;
	int encode_height;
	iav_rate_control_mode bitrate_control = IAV_CBR;
	int min_bitrate = 0;
	int max_bitrate = 0;
	int avg_bitrate = 0;
	int jpeg_quality = 0;

	vsrc_stream_t *stream;

	if ((stream = vsrc_get_stream(pData, index)) == NULL)
		return -1;

	size = avf_get_word(ptr, buffer, sizeof(buffer), ':');
	if (size < 0)
		goto BadConfig;
	ptr += size;

	// encode format
	if (::strcmp(buffer, "h264") == 0) {

		encode_type = IAV_ENCODE_H264;
		if (::sscanf(ptr, "%dx%d:%d:%d:%d", 
			&encode_width, &encode_height,
			&min_bitrate, &max_bitrate, &avg_bitrate) != 5)
			goto BadConfig;

		// force use CBR
		min_bitrate = max_bitrate = avg_bitrate;
		bitrate_control = IAV_CBR;

	} else if (::strcmp(buffer, "mjpeg") == 0) {

		encode_type = IAV_ENCODE_MJPEG;
		if (::sscanf(ptr, "%dx%dx%d",
			&encode_width, &encode_height, &jpeg_quality) != 3)
			goto BadConfig;

	} else if (::strcmp(buffer, "none") == 0) {

		encode_type = IAV_ENCODE_NONE;
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

	if (stream->encode_width != (uint32_t)encode_width ||
			stream->encode_height != (uint32_t)encode_height) {
		pData->stream_size_changed = 1;
	}

	stream->encode_type = encode_type;
	stream->encode_width = encode_width;
	stream->encode_height = encode_height;
	stream->encode_bitrate_control = bitrate_control;
	stream->encode_avg_bitrate = avg_bitrate;
	stream->encode_min_bitrate = min_bitrate;
	stream->encode_max_bitrate = max_bitrate;
	stream->jpeg_quality = jpeg_quality;

	return 0;

BadConfig:
	AVF_LOGE("bad VideoConfig: %s", config);
	return -1;
}

// numerator:denomiator
static int vsrc_SetVideoFrameRateConfig(vsrc_device_t *pDev, int index, const char *config)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_stream_t *stream;
	int ratio_numerator;
	int ratio_denominator;

	if ((stream = vsrc_get_stream(pData, index)) == NULL)
		return -1;

	if (::sscanf(config, "%d:%d", &ratio_numerator, &ratio_denominator) != 2)
		goto BadConfig;

	if (ratio_numerator < 1 || ratio_numerator > 255)
		goto BadConfig;

	if (ratio_denominator < 1 || ratio_denominator > 255)
		goto BadConfig;

	stream->iav_framerate_config.ratio_numerator = ratio_numerator;
	stream->iav_framerate_config.ratio_denominator = ratio_denominator;

	return 0;

BadConfig:
	AVF_LOGE("bad VideoFrameRateConfig: %s", config);
	return -1;
}

static int vsrc_SetOverlayClut(vsrc_device_t *pDev, unsigned int clut_id, const uint32_t *pClut)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	uint8_t *ptr;

	if (pData->overlay_map.addr == NULL) {
		AVF_LOGE("no overlay for clut");
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
static int vsrc_ConfigOverlay(vsrc_device_t *pDev, int index, vsrc_overlay_info_t *pOverlayInfo)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_stream_t *stream;
	overlay_insert_area_ex_t *iav_area;
	vsrc_overlay_area_t *area;
	uint32_t total_size = 0;
	uint32_t area_size;
	uint8_t *data;
	int i;

	if (pData->overlay_map.addr == NULL) {
		AVF_LOGE("no overlay memory");
		return -1;
	}

	if ((stream = vsrc_get_stream(pData, index)) == NULL)
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
		iav_area->clut_id = area->clut_id;
		iav_area->data = area->data;
	}

	stream->overlay_configured = 1;

	return 0;
}

static int vsrc_UpdateOverlay(vsrc_device_t *pDev,
	int index, vsrc_overlay_info_t *pOverlayInfo)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_stream_t *stream;
	overlay_insert_area_ex_t *iav_area;
	vsrc_overlay_area_t *area;
	int rval;
	int i;

	if (pData->state != STATE_PREVIEW && pData->state != STATE_ENCODING) {
		AVF_LOGE("UpdateOverlay: must be in preview or encoding state!");
		return -1;
	}

	if ((stream = vsrc_get_stream(pData, index)) == NULL)
		return -1;

	if (stream->encode_type == IAV_ENCODE_NONE)
		return 0;

	if (!stream->overlay_configured) {
		AVF_LOGE("overlay for stream %d not configured", index);
		return -1;
	}

	overlay_insert_ex_t *iav_overlay_config = &stream->iav_overlay_config;

	iav_overlay_config->id = 1 << index;
	iav_overlay_config->enable = 0;

	area = pOverlayInfo->area + 0;
	iav_area = iav_overlay_config->area + 0;
	for (i = 0; i < NUM_OVERLAY_AREAS; i++, area++, iav_area++) {
		iav_area->enable = area->visible;
		if (iav_area->enable) {
			iav_overlay_config->enable = 1;
		}
	}

	rval = ::ioctl(pData->iav_fd, IAV_IOC_OVERLAY_INSERT_EX, iav_overlay_config);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_OVERLAY_INSERT_EX");
		return rval;
	}

	return 0;
}

static int vsrc_RefreshOverlay(vsrc_device_t *pDev, int index, int area)
{
	// todo - clean cache after the content is modified
	return 0;
}

static int vsrc_SetImageControl(vsrc_device_t *pDev, const char *pName, const char *pValue)
{
#ifdef ENABLE_AAA
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	return amba_aaa_set_image_control(pData->iav_fd, pName, pValue);
#else
	return 0;
#endif
}

static int vsrc_EnterIdle(vsrc_device_t *pDev)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	int rval;

	if (pData->state == STATE_IDLE)
		return 0;

#ifdef ENABLE_AAA
	amba_aaa_stop();
#endif

	if ((rval = ::ioctl(pData->iav_fd, IAV_IOC_ENTER_IDLE, 0)) < 0) {
		AVF_LOGP("IAV_IOC_ENTER_IDLE");
		return rval;
	}

	pData->state = STATE_IDLE;
	return 0;
}

static int vsrc_system_setup(vsrc_dev_data_t *pData)
{
	int rval;

	iav_system_setup_info_ex_t *iav_system_setup_info = &pData->iav_system_setup_info;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_GET_SYSTEM_SETUP_INFO_EX, iav_system_setup_info);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_GET_SYSTEM_SETUP_INFO_EX");
		return rval;
	}

	iav_system_setup_info->voutA_osd_blend_enable = 0;	// **
	iav_system_setup_info->voutB_osd_blend_enable = 1;	// **
	iav_system_setup_info->coded_bits_interrupt_enable = 0;

	iav_system_setup_info->pip_size_enable = 0;
	iav_system_setup_info->low_delay_cap_enable = 0;
	//iav_system_setup_info->audio_clk_freq = 

	rval = ::ioctl(pData->iav_fd, IAV_IOC_SET_SYSTEM_SETUP_INFO_EX, iav_system_setup_info);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_GET_SYSTEM_SETUP_INFO_EX");
		return rval;
	}

	return 0;
}

static int vsrc_EnterPreview(vsrc_device_t *pDev)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	int rval;

	if (pData->state == STATE_PREVIEW)
		return 0;

	if (pData->state != STATE_IDLE) {
		AVF_LOGE("EnterPreview: not in IDLE state: %d!", pData->state);
		return -1;
	}

	//---------------------------------------------------------
	// get vout info
	//---------------------------------------------------------

	vout_info_t *vout_info = &pData->lcd_info;
	avf_platform_display_get_vout_info(VOUT_0, vout_info);
	if (vout_info->enabled) {
		AVF_LOGD(C_GREEN "lcd: %d, %d" C_NONE,vout_info->video_width, vout_info->video_height);
	}

	vout_info = &pData->hdmi_info;
	avf_platform_display_get_vout_info(VOUT_1, vout_info);
	if (vout_info->enabled) {
		AVF_LOGD(C_GREEN "hdmi: %d, %d" C_NONE, vout_info->video_width, vout_info->video_height);
	}

	//---------------------------------------------------------
	// set system setup info
	//---------------------------------------------------------

	if ((rval = vsrc_system_setup(pData)) < 0)
		return rval;

	//---------------------------------------------------------
	// config vin
	//---------------------------------------------------------

	if (pData->vin_config_changed) {

		if ((rval = vsrc_vin_set_src(pData)) < 0)
			return rval;

		if ((rval = vsrc_vin_set_video_mode(pData)) < 0)
			return rval;

		if ((rval = vsrc_vin_set_framerate(pData)) < 0)
			return rval;

		if ((rval = vsrc_vin_set_mirror_mode(pData)) < 0)
			return rval;

		if ((rval = vsrc_vin_set_anti_flicker(pData)) < 0)
			return rval;

		if ((rval = vsrc_vin_get_source_info(pData)) < 0)
			return rval;

		pData->vin_config_changed = 0;

	}

	//---------------------------------------------------------
	// calculate sizes
	//---------------------------------------------------------

	if ((rval = vsrc_calc_main_srcbuf(pData)) < 0)
		return rval;

	if ((rval = vsrc_calc_vin_input(pData)) < 0)
		return rval;

	if ((rval = vsrc_calc_all_srcbufs(pData)) < 0)
		return rval;

	//---------------------------------------------------------
	// config resource
	//---------------------------------------------------------
	if ((rval = vsrc_config_iav_resource(pData)) < 0)
		return rval;

	if ((rval = vsrc_set_srcbuf_type(pData)) < 0)
		return rval;

	if ((rval = vsrc_set_srcbuf_format(pData)) < 0)
		return rval;

	//---------------------------------------------------------
	// preview
	//---------------------------------------------------------
	if ((rval = vsrc_start_preview(pData)) < 0)
		return rval;

	pData->state = STATE_PREVIEW;
	return 0;
}

// after prevew, before encoding
static int vsrc_UpdateVideoConfigs(vsrc_device_t *pDev)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	int rval;

	if (pData->stream_size_changed) {

		if ((rval = vsrc_calc_all_srcbufs(pData)) < 0)
			return rval;

		// srcbuf may changed
		if ((rval = vsrc_set_srcbuf_format(pData)) < 0)
			return rval;

		pData->stream_size_changed = 0;
	}

	// config streams
	if ((rval = vsrc_config_all_streams(pData)) < 0)
		return rval;

	return 0;
}

static int vsrc_NeedReinitPreview(vsrc_device_t *pDev)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);

	if (pData->vin_config_changed)
		return 1;

	uint32_t main_srcbuf_width;
	uint32_t main_srcbuf_height;
	if (vsrc_calc_main_srcbuf_size(pData, &main_srcbuf_width, &main_srcbuf_height) < 0)
		return 1;

	if (main_srcbuf_width != pData->main_srcbuf_width ||
			main_srcbuf_height != pData->main_srcbuf_height) {
		AVF_LOGD("main srcbuf changed, %d, %d => %d, %d",
			pData->main_srcbuf_width, pData->main_srcbuf_height,
			main_srcbuf_width, main_srcbuf_height);
		return 1;
	}

	return 0;
}

static int vsrc_StartEncoding(vsrc_device_t *pDev)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	int rval;

	if (pData->state != STATE_PREVIEW) {
		AVF_LOGE("StartEncoding: not in preview state!");
		return -1;
	}

	AVF_LOGI(C_YELLOW "StartEncoding(0x%x)" C_NONE, pData->iav_active_stream);

	pData->encoding_stopped = 0;
	rval = ::ioctl(pData->iav_fd, IAV_IOC_START_ENCODE_EX, pData->iav_active_stream);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_START_ENCODE_EX");
		return rval;
	}

	pData->fullness.fullness = 0;
	pData->fullness.num_pictures = 0;

	pData->state = STATE_ENCODING;

	return 0;
}

static int vsrc_StopEncoding(vsrc_device_t *pDev)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	int rval;

	if (pData->state == STATE_PREVIEW)
		return 0;

	if (pData->state != STATE_ENCODING) {
		AVF_LOGE("StopEncoding: not in encoding state!");
		return -1;
	}

	AVF_LOGI(C_YELLOW "StopEncoding(0x%x)" C_NONE, pData->iav_active_stream);

	rval = ::ioctl(pData->iav_fd, IAV_IOC_STOP_ENCODE_EX, pData->iav_active_stream);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_STOP_ENCODE_EX");
		return rval;
	}

	pData->encoding_stopped = 1;
	pData->state = STATE_PREVIEW;

	return 0;
}

static int vsrc_SupportStillCapture(vsrc_device_t *pDev)
{
	return 0;
}

static int vsrc_IsStillCaptureMode(vsrc_device_t *pDev)
{
	return 0;
}

static uint64_t fix_pts(vsrc_dev_data_t *pData, vsrc_stream_t *stream, uint32_t dsp_pts)
{
	uint64_t pts;
	uint32_t delta;

	dsp_pts &= PTS_MASK;

	if (!stream->encoding_started) {
		stream->encoding_started = 1;
		stream->last_dsp_pts = dsp_pts;
		stream->last_stream_pts = 0;
		return 0;
	}

	if (dsp_pts >= stream->last_dsp_pts) {
		// PTS increased
		delta = dsp_pts - stream->last_dsp_pts;
		if (delta < PTS_DELTA) {
			pts = stream->last_stream_pts + delta;
		} else {
			// wrap around
			delta = (stream->last_dsp_pts + PTS_WRAP_INC) - dsp_pts;
			pts = stream->last_stream_pts - delta;
		}
	} else {
		// PTS decreased
		delta = stream->last_dsp_pts - dsp_pts;
		if (delta < PTS_DELTA) {
			pts = stream->last_stream_pts - delta;
		} else {
			delta = (dsp_pts + PTS_WRAP_INC) - stream->last_dsp_pts;
			pts = stream->last_stream_pts + delta;
		}
	}

#if 0
	if (stream->encode_type == IAV_ENCODE_H264) {
		//AVF_LOGD("pts: " LLD, pts);
		if (pts != stream->last_stream_pts + stream->rate_90kHZ) {
			AVF_LOGW("stream %d pts not continguous, prev: " LLD ", curr: " LLD ", diff: %d (rate=%d)",
				stream->index, stream->last_stream_pts, pts, (int)(pts - stream->last_stream_pts),
				stream->rate_90kHZ);
		}
	}
#endif

	stream->last_dsp_pts = dsp_pts;
	stream->last_stream_pts = pts;

	return pts;
}

static int vsrc_NewSegment(vsrc_device_t *pDev, uint32_t streamFlags)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	int rval;

	rval = ::ioctl(pData->iav_fd, IAV_IOC_FORCE_IDR_EX, streamFlags);
	if (rval < 0) {
		AVF_LOGP("IOC_FORCE_IDR_EX");
		return rval;
	}

	return 0;
}

static int vsrc_ReadBits(vsrc_device_t *pDev, vsrc_bits_buffer_t *pBitsBuffer)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	vsrc_stream_t *stream;
	bits_info_ex_t bits_info;
	unsigned size;
	int rval;

	if (pData->encoding_stopped) {
		// JPEG stream doesn't return EOS, so generate it here
		for (int i = 0; i < (int)ARRAY_SIZE(pData->streams); i++) {
			stream = pData->streams + i;
			if (stream->encode_type != IAV_ENCODE_H264) {
				if (!stream->eos_sent) {
					pBitsBuffer->stream_id = stream->index;
					pBitsBuffer->is_eos = 1;
					stream->eos_sent = 1;
					return 0;
				}
			}
		}
	}

	rval = ::ioctl(pData->iav_fd, IAV_IOC_READ_BITSTREAM_EX, &bits_info);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_READ_BITSTREAM_EX");
		return rval;
	}

	if ((stream = vsrc_get_stream(pData, bits_info.stream_id)) == NULL)
		return -1;

	pBitsBuffer->stream_id = stream->index;
	pBitsBuffer->is_eos = bits_info.stream_end;

	if (bits_info.stream_end)
		return 0;

	iav_mmap_info_t *bsb_map = &pData->bsb_map;

	// address should be aligned
	if ((bits_info.start_addr & 0x1F) != 0) {
		AVF_LOGE("bad bits start address: 0x%x, size: 0x%x, pic_type: %d",
			bits_info.start_addr, bits_info.pic_size, bits_info.pic_type);
		return -1;
	}

	size = bits_info.pic_size;
	if ((u8*)bits_info.start_addr + size <= bsb_map->addr + bsb_map->length) {
		pBitsBuffer->buffer1 = (uint8_t*)bits_info.start_addr;
		pBitsBuffer->size1 = size;
		pBitsBuffer->buffer2 = NULL;
		pBitsBuffer->size2 = 0;
		if (bits_info.pic_type == IDR_FRAME || bits_info.pic_type == P_FRAME) {
			avf_u32_t start_code = avf_read_be_32(pBitsBuffer->buffer1);
			if (start_code != 0x00000001) {
				AVF_LOGE("video bitstream corrupted: %x", start_code);
				return -1;
			}
		}
	} else {
		pBitsBuffer->buffer1 = (uint8_t*)bits_info.start_addr;
		pBitsBuffer->size1 = (uint8_t*)bsb_map->addr + bsb_map->length - (uint8_t*)pBitsBuffer->buffer1;
		pBitsBuffer->buffer2 = (uint8_t*)bsb_map->addr;
		pBitsBuffer->size2 = size - pBitsBuffer->size1;
	}

	pBitsBuffer->dts64 =
	pBitsBuffer->pts64 = fix_pts(pData, stream, bits_info.PTS);
	pBitsBuffer->length = stream->rate_90kHZ;

	switch (bits_info.pic_type) {
	case IDR_FRAME:
		pBitsBuffer->pic_type = PIC_H264_IDR;
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

	case I_FRAME:
		pBitsBuffer->pic_type = PIC_I;
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

	case P_FRAME:
		pBitsBuffer->pic_type = PIC_P;
		break;

	case B_FRAME:
		pBitsBuffer->pic_type = PIC_B;
		break;

	case JPEG_STREAM:
		pBitsBuffer->pic_type = PIC_JPEG;
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
		AVF_LOGE("unknown pic type");
		return -1;
	}

	pData->fullness.fullness += (size + 31) & ~31;
	pData->fullness.num_pictures ++;

	return 0;
}

static void vsrc_Release(vsrc_device_t *pDev)
{
	vsrc_dev_data_t *pData = GET_DEV_DATA(pDev);
	pData->data_in_use = 0;
	pDev->pData = NULL;
}

int vsrc_Init(vsrc_device_t *pDev)
{
	static vsrc_dev_data_t data;

	vsrc_dev_data_t *pData;

	if (data.data_in_use) {
		AVF_LOGE("can only create 1 vsrc instance");
		return -1;
	}

	data.data_in_use = 1;
	pDev->pData = pData = &data;

	pData->iav_fd = -1;
	pData->state = STATE_UNKNOWN;

	pData->bsb_map.addr = NULL;
	pData->bsb_map.length = 0;
	pData->overlay_map.addr = NULL;
	pData->overlay_map.length = 0;

	// default vin config
	pData->vin_config_str = NULL;
	if (vsrc_SetVinConfig(pDev, DEFAULT_VIN_CONFIG) < 0) {
		return -1;
	}

	pData->vin_config_changed = 1;
	pData->stream_size_changed = 1;

	// streams default config
	for (int i = 0; i < MAX_ENCODE_STREAMS; i++) {
		vsrc_stream_t *stream = pData->streams + i;

		stream->encode_type = IAV_ENCODE_NONE;
		//stream->srcbuf_id = 0;
		stream->index = i;

		stream->overlay_configured = 0;

		stream->iav_framerate_config.ratio_numerator = 1;
		stream->iav_framerate_config.ratio_denominator = 1;
	}

	// device control
	pDev->Release = vsrc_Release;
	pDev->Open = vsrc_Open;
	pDev->Close = vsrc_Close;

	// device info
	pDev->GetMaxNumStream = vsrc_GetMaxNumStream;
	pDev->GetStreamInfo = vsrc_GetStreamInfo;

	// vin
	pDev->SetVinConfig = vsrc_SetVinConfig;
	pDev->SetClockMode = NULL;
	pDev->GetVinConfig = NULL;

	// vout
	pDev->ChangeVoutVideo = NULL;

	// video streams
	pDev->SetVideoConfig = vsrc_SetVideoConfig;
	pDev->SetVideoFrameRateConfig = vsrc_SetVideoFrameRateConfig;
	pDev->UpdateVideoConfigs = vsrc_UpdateVideoConfigs;

	// video overlays
	pDev->SetOverlayClut = vsrc_SetOverlayClut;
	pDev->ConfigOverlay = vsrc_ConfigOverlay;
	pDev->UpdateOverlay = vsrc_UpdateOverlay;
	pDev->RefreshOverlay = vsrc_RefreshOverlay;

	// image control
	pDev->SetImageControl = vsrc_SetImageControl;

	// state control
	pDev->EnterIdle = vsrc_EnterIdle;
	pDev->EnterPreview = vsrc_EnterPreview;
	pDev->StartEncoding = vsrc_StartEncoding;
	pDev->StopEncoding = vsrc_StopEncoding;
	pDev->NeedReinitPreview = vsrc_NeedReinitPreview;
	pDev->SupportStillCapture = vsrc_SupportStillCapture;
	pDev->IsStillCaptureMode = vsrc_IsStillCaptureMode;

	// stream control
	pDev->NewSegment = vsrc_NewSegment;
	pDev->ReadBits = vsrc_ReadBits;

	return 0;
}

