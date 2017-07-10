
#include <basetypes.h>
#include <ambas_common.h>
#include <ambas_imgproc.h>
#include <ambas_vin.h>
#include <iav_drv.h>
#include <iav_drv_ex.h>

extern "C" {
#include <img_struct.h>
#include <img_dsp_interface.h>
#include <img_api.h>

#include "adj_params/imx036_adj_param.c"
#include "adj_params/imx036_aeb_param.c"

#include "adj_params/imx035_adj_param.c"
#include "adj_params/imx035_aeb_param.c"

#include "adj_params/ov2710_adj_param.c"
#include "adj_params/ov2710_aeb_param.c"

#include "adj_params/ov9715_adj_param.c"
#include "adj_params/ov9715_aeb_param.c"

#include "adj_params/ov5653_adj_param.c"
#include "adj_params/ov5653_aeb_param.c"

#include "adj_params/imx072_adj_param.c"
#include "adj_params/imx072_aeb_param.c"

#include "adj_params/imx122_adj_param.c"
#include "adj_params/imx122_aeb_param.c"

#include "adj_params/ov14810_adj_param.c"
#include "adj_params/ov14810_aeb_param.c"

#include "adj_params/mt9m033_adj_param.c"
#include "adj_params/mt9m033_aeb_param.c"

#include "adj_params/mt9t002_adj_param.c"
#include "adj_params/mt9t002_aeb_param.c"

#include "adj_params/s5k5b3gx_adj_param.c"
#include "adj_params/s5k5b3gx_aeb_param.c"

#include "adj_params/mt9p006_adj_param.c"
#include "adj_params/mt9p006_aeb_param.c"

#include "adj_params/ar0331_adj_param.c"
#include "adj_params/ar0331_aeb_param.c"

#include "adj_params/mn34041pl_adj_param.c"
#include "adj_params/mn34041pl_aeb_param.c"

line_t mt9t002_60fps_lines[] = {
	{
	{{SHUTTER_1BY8000, ISO_100, 0}}, {{SHUTTER_1BY100, ISO_100,0}}
	},

	{
	{{SHUTTER_1BY100, ISO_100, 0}}, {{SHUTTER_1BY100, ISO_300, 0}}
	},

	{
	{{SHUTTER_1BY60, ISO_100, 0}}, {{SHUTTER_1BY60, ISO_800,0}}
	},

	{
	{{SHUTTER_1BY60, ISO_100, 0}}, {{SHUTTER_1BY60, ISO_6400,0}}
	}
};

line_t mt9t002_120fps_lines[] = {
	{
	{{SHUTTER_1BY8000, ISO_100, 0}}, {{SHUTTER_1BY240, ISO_100,0}}
	},

	{
	{{SHUTTER_1BY240, ISO_100, 0}}, {{SHUTTER_1BY240, ISO_1600, 0}}
	},

	{
	{{SHUTTER_1BY240, ISO_100, 0}}, {{SHUTTER_1BY240, ISO_1600, 0}}
	},

	{
	{{SHUTTER_1BY240, ISO_100, 0}}, {{SHUTTER_1BY240, ISO_1600, 0}}
	}
};
}

#define IMGPROC_PATH			"/etc/idsp"
#define IMGPROC_REG_FILE		(IMGPROC_PATH"/reg.bin")
#define IMGPROC_3D_FILE		(IMGPROC_PATH"/3D.bin")

#define IMGPROC_REG_SIZE		18752
#define IMGPROC_3D_SIZE		17536
#define IMGPROC_ADJ_SIZE		4

static uint8_t dsp_cc_reg[IMGPROC_REG_SIZE];
static uint8_t dsp_cc_matrix[IMGPROC_3D_SIZE];
static uint8_t dsp_adj_matrix[IMGPROC_ADJ_SIZE][IMGPROC_3D_SIZE];

static int read_file(const char *filename, uint8_t *buffer, uint32_t size)
{
	int fd = avf_open_file(filename, O_RDONLY, 0);
	if (fd < 0) {
		perror(filename);
		return -1;
	}
	if (read(fd, buffer, size) != (int)size) {
		avf_close_file(fd);
		AVF_LOGE("Cannot read %s", filename);
		return -1;
	}
	avf_close_file(fd);
	return 0;
}

static int load_dsp_cc_table(void)
{
	int rval;

	if ((rval = read_file(IMGPROC_REG_FILE, dsp_cc_reg, sizeof(dsp_cc_reg))) < 0)
		return rval;

	if ((rval = read_file(IMGPROC_3D_FILE, dsp_cc_matrix, sizeof(dsp_cc_matrix))) < 0)
		return rval;

	if ((rval = img_dsp_load_color_correction_table(
			(u32)dsp_cc_reg, (u32)dsp_cc_matrix)) < 0)
		return rval;

	return 0;
}

static int load_adj_cc_table(const char *sensor_name)
{
	int rval;
	int i;

	for (i = 0; i < IMGPROC_ADJ_SIZE; i++) {
		char filename[128];
		snprintf(filename, sizeof(filename), "%s/sensors/%s_0%d_3D.bin",
			IMGPROC_PATH, sensor_name, i + 1);

		if ((rval = read_file(filename, &dsp_adj_matrix[i][0], IMGPROC_3D_SIZE)) < 0)
			return rval;

		if ((rval = img_adj_load_cc_table((u32)&dsp_adj_matrix[i][0], i)) < 0)
			return rval;
	}

	return 0;
}

static int dsp_image_sensor_config(
	image_sensor_param_t *param, 
	const char **psensor_name,
	uint32_t fps_q9,
	uint32_t video_width,
	uint32_t video_height
	)
{
	const char *sensor_name = *psensor_name;
	sensor_label sensor_lb;
	int rval;

	memset(param, 0, sizeof(*param));

	do {
		if (strncmp("ov9715", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = OV_9715;
			param->p_adj_param = &ov9715_adj_param;
			param->p_rgb2yuv = ov9715_rgb2yuv;
			param->p_chroma_scale = &ov9715_chroma_scale;
			param->p_awb_param = &ov9715_awb_param;
			param->p_50hz_lines = ov9715_50hz_lines;
			param->p_60hz_lines = ov9715_60hz_lines;
			param->p_tile_config = &ov9715_tile_config;
			param->p_ae_agc_dgain = ov9715_ae_agc_dgain;
			param->p_ae_sht_dgain = ov9715_ae_sht_dgain;
			param->p_dlight_range = ov9715_dlight;
			param->p_manual_LE = ov9715_manual_LE;
			break;
		}

		if (strncmp("ov2710", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = OV_2710;
			param->p_adj_param = &ov2710_adj_param;
			param->p_rgb2yuv = ov2710_rgb2yuv;
			param->p_chroma_scale = &ov2710_chroma_scale;
			param->p_awb_param = &ov2710_awb_param;
			param->p_50hz_lines = ov2710_50hz_lines;
			param->p_60hz_lines = ov2710_60hz_lines;
			param->p_tile_config = &ov2710_tile_config;
			param->p_ae_agc_dgain = ov2710_ae_agc_dgain;
			param->p_ae_sht_dgain = ov2710_ae_sht_dgain;
			param->p_dlight_range = ov2710_dlight;
			param->p_manual_LE = ov2710_manual_LE;
			break;
		}

		if (strncmp("ov5653", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = OV_5653;
			param->p_adj_param = &ov5653_adj_param;
			param->p_rgb2yuv = ov5653_rgb2yuv;
			param->p_chroma_scale = &ov5653_chroma_scale;
			param->p_awb_param = &ov5653_awb_param;
			param->p_50hz_lines = ov5653_50hz_lines;
			param->p_60hz_lines = ov5653_60hz_lines;
			param->p_tile_config = &ov5653_tile_config;
			param->p_ae_agc_dgain = ov5653_ae_agc_dgain;
			//if (fps_q9 == AMBA_VIDEO_FPS_12 && video_mode == AMBA_VIDEO_MODE_QSXGA) {
			if (fps_q9 == AMBA_VIDEO_FPS_12 && video_width == 2592 && video_height == 1944) {
				param->p_ae_sht_dgain = ov5653_ae_sht_dgain_5m12;
			//} else if (fps_q9 == AMBA_VIDEO_FPS_15 && video_mode == AMBA_VIDEO_MODE_QXGA) {
			} else if (fps_q9 == AMBA_VIDEO_FPS_15 && video_width == 2048 && video_height == 1536) {
				param->p_ae_sht_dgain = ov5653_ae_sht_dgain_3m15;
			//} else if (fps_q9 == AMBA_VIDEO_FPS_29_97 && video_mode == AMBA_VIDEO_MODE_1080P) {
			} else if (fps_q9 == AMBA_VIDEO_FPS_29_97 && video_width == 1920 && video_height == 1080) {
				param->p_ae_sht_dgain = ov5653_ae_sht_dgain_1080p;
			} else {
				param->p_ae_sht_dgain = ov5653_ae_sht_dgain;
			}
			param->p_dlight_range = ov5653_dlight;
			param->p_manual_LE = ov5653_manual_LE;
			break;
		}

		if (strncmp("mt9j001", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = MT_9J001;
			break;
		}

		if (strncmp("mt9p006", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = MT_9P031;
			param->p_adj_param = &mt9p006_adj_param;
			param->p_rgb2yuv = mt9p006_rgb2yuv;
			param->p_chroma_scale = &mt9p006_chroma_scale;
			param->p_awb_param = &mt9p006_awb_param;
			param->p_50hz_lines = mt9p006_50hz_lines;
			param->p_60hz_lines = mt9p006_60hz_lines;
			param->p_tile_config = &mt9p006_tile_config;
			param->p_ae_agc_dgain = mt9p006_ae_agc_dgain;
			//if (fps_q9 == AMBA_VIDEO_FPS_12 && video_mode == AMBA_VIDEO_MODE_QSXGA) {
			if (fps_q9 == AMBA_VIDEO_FPS_12 && video_width == 2592 && video_height == 1944) {
				param->p_ae_sht_dgain = mt9p006_ae_sht_dgain_5m12;
			//} else if (fps_q9 == AMBA_VIDEO_FPS_15 && video_mode == AMBA_VIDEO_MODE_QXGA) {
			} else if (fps_q9 == AMBA_VIDEO_FPS_15 && video_width == 2048 && video_height == 1536) {
				param->p_ae_sht_dgain = mt9p006_ae_sht_dgain_3m15;
			//} else if (fps_q9 == AMBA_VIDEO_FPS_29_97 && video_mode == AMBA_VIDEO_MODE_1080P) {
			} else if (fps_q9 == AMBA_VIDEO_FPS_29_97 && video_width == 1920 && video_height == 1080) {
				param->p_ae_sht_dgain = mt9p006_ae_sht_dgain_1080p;
			} else {
				param->p_ae_sht_dgain = mt9p006_ae_sht_dgain;
			}
			param->p_dlight_range = mt9p006_dlight;
			param->p_manual_LE = mt9p006_manual_LE;
			break;
		}

		if (strncmp("mt9m033", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = MT_9M033;
			param->p_adj_param = &mt9m033_adj_param;
			param->p_rgb2yuv = mt9m033_rgb2yuv;
			param->p_chroma_scale = &mt9m033_chroma_scale;
			param->p_awb_param = &mt9m033_awb_param;
			param->p_50hz_lines = mt9m033_50hz_lines;
			param->p_60hz_lines = mt9m033_60hz_lines;
			param->p_tile_config = &mt9m033_tile_config;
			param->p_ae_agc_dgain = mt9m033_ae_agc_dgain;
			param->p_ae_sht_dgain = mt9m033_ae_sht_dgain;
			param->p_dlight_range = mt9m033_dlight;
			param->p_manual_LE = mt9m033_manual_LE;
			break;
		}

		if (strncmp("imx035", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = IMX_035;
			param->p_adj_param = &imx035_adj_param;
			param->p_rgb2yuv = imx035_rgb2yuv;
			param->p_chroma_scale = &imx035_chroma_scale;
			param->p_awb_param = &imx035_awb_param;
			param->p_50hz_lines = imx035_50hz_lines;
			param->p_60hz_lines = imx035_60hz_lines;
			param->p_tile_config = &imx035_tile_config;
			param->p_ae_agc_dgain = imx035_ae_agc_dgain;
			param->p_ae_sht_dgain = imx035_ae_sht_dgain;
			param->p_dlight_range = imx035_dlight;
			param->p_manual_LE = imx035_manual_LE;
			break;
		}

		if (strncmp("imx036", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = IMX_036;
			param->p_adj_param = &imx036_adj_param;
			param->p_rgb2yuv = imx036_rgb2yuv;
			param->p_chroma_scale = &imx036_chroma_scale;
			param->p_awb_param = &imx036_awb_param;
			param->p_50hz_lines = imx036_50hz_lines;
			param->p_60hz_lines = imx036_60hz_lines;
			param->p_tile_config = &imx036_tile_config;
			param->p_ae_agc_dgain = imx036_ae_agc_dgain;
			//if (fps_q9 == AMBA_VIDEO_FPS_15 && video_mode ==AMBA_VIDEO_MODE_QXGA) {
			if (fps_q9 == AMBA_VIDEO_FPS_15 && video_width == 2048 && video_height == 1536) {
				param->p_ae_sht_dgain = imx036_ae_sht_dgain_3m15;
			//} else if (fps_q9 == AMBA_VIDEO_FPS_29_97 && video_mode == AMBA_VIDEO_MODE_1080P) {
			} else if (fps_q9 == AMBA_VIDEO_FPS_29_97 && video_width == 1920 && video_height == 1080) {
				param->p_ae_sht_dgain = imx036_ae_sht_dgain_1080p;
			} else {
				param->p_ae_sht_dgain = imx036_ae_sht_dgain;
			}
			param->p_dlight_range = imx036_dlight;
			param->p_manual_LE = imx036_manual_LE;
			break;
		}

		if (strncmp("imx072", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = IMX_072;
			param->p_adj_param = &imx072_adj_param;
			param->p_rgb2yuv = imx072_rgb2yuv;
			param->p_chroma_scale = &imx072_chroma_scale;
			param->p_awb_param = &imx072_awb_param;
			param->p_50hz_lines = imx072_50hz_lines;
			param->p_60hz_lines = imx072_60hz_lines;
			param->p_tile_config = &imx072_tile_config;
			param->p_ae_agc_dgain = imx072_ae_agc_dgain;
			param->p_ae_sht_dgain = imx072_ae_sht_dgain;
			param->p_dlight_range = imx072_dlight;
			param->p_manual_LE = imx072_manual_LE;
			break;
		}

		if (strncmp("imx122", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = IMX_122;
			param->p_adj_param = &imx122_adj_param;
			param->p_rgb2yuv = imx122_rgb2yuv;
			param->p_chroma_scale = &imx122_chroma_scale;
			param->p_awb_param = &imx122_awb_param;
			param->p_50hz_lines = imx122_50hz_lines;
			param->p_60hz_lines = imx122_60hz_lines;
			param->p_tile_config = &imx122_tile_config;
			param->p_ae_agc_dgain = imx122_ae_agc_dgain;
			param->p_ae_sht_dgain = imx122_ae_sht_dgain;
			param->p_dlight_range = imx122_dlight;
			param->p_manual_LE = imx122_manual_LE;
			break;
		}

		if (strncmp("ov14810", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = OV_14810;
			param->p_adj_param = &ov14810_adj_param;
			param->p_rgb2yuv = ov14810_rgb2yuv;
			param->p_chroma_scale = &ov14810_chroma_scale;
			param->p_awb_param = &ov14810_awb_param;
			param->p_50hz_lines = ov14810_50hz_lines;
			param->p_60hz_lines = ov14810_60hz_lines;
			param->p_tile_config = &ov14810_tile_config;
			param->p_ae_agc_dgain = ov14810_ae_agc_dgain;
			param->p_ae_sht_dgain = ov14810_ae_sht_dgain;
			param->p_dlight_range = ov14810_dlight;
			param->p_manual_LE = ov14810_manual_LE;
			break;
		}

		if (strncmp("mt9t002", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = MT_9T002;
			param->p_adj_param = &mt9t002_adj_param;
			param->p_rgb2yuv = mt9t002_rgb2yuv;
			param->p_chroma_scale = &mt9t002_chroma_scale;
			param->p_awb_param = &mt9t002_awb_param;
			if (fps_q9 <= AMBA_VIDEO_FPS_120) {
				param->p_50hz_lines = mt9t002_120fps_lines;
				param->p_60hz_lines = mt9t002_120fps_lines;
			} else if (fps_q9 <= AMBA_VIDEO_FPS_60) {
				param->p_50hz_lines = mt9t002_60fps_lines;
				param->p_60hz_lines = mt9t002_60fps_lines;
			} else {
				param->p_50hz_lines = mt9t002_50hz_lines;
				param->p_60hz_lines = mt9t002_60hz_lines;
			}
			param->p_tile_config = &mt9t002_tile_config;
			param->p_ae_agc_dgain = mt9t002_ae_agc_dgain;
			//if (fps_q9 == AMBA_VIDEO_FPS_15 && video_mode == AMBA_VIDEO_MODE_QXGA) {
			if (fps_q9 == AMBA_VIDEO_FPS_15 && video_width == 2048 && video_height == 1536) {
				param->p_ae_sht_dgain = mt9t002_ae_sht_dgain_3m15;
			//} else if (fps_q9 == AMBA_VIDEO_FPS_29_97 && video_mode == AMBA_VIDEO_MODE_1080P30) {
			} else if (fps_q9 == AMBA_VIDEO_FPS_29_97 && video_width == 1920 && video_height == 1080) {
				param->p_ae_sht_dgain = mt9t002_ae_sht_dgain_1080p30;
			} else {
				param->p_ae_sht_dgain = mt9t002_ae_sht_dgain_2304x1296;
			}
			param->p_dlight_range = mt9t002_dlight;
			param->p_manual_LE = mt9t002_manual_LE;
			break;
		}

		if (strncmp("ar0331", sensor_name, strlen(sensor_name)) == 0) {
			AR0331_OP_MODE ar0331_mode = img_get_ar0331_mode();
			//AVF_LOGI("ar0331_mode = %d\n", ar0331_mode);

			switch (ar0331_mode) {
			case AR0331_PURE_LINEAR:
			case AR0331_COMBI_LINEAR:
				sensor_lb = MT_9T002;
				param->p_adj_param = &mt9t002_adj_param;
				param->p_rgb2yuv = mt9t002_rgb2yuv;
				param->p_chroma_scale = &mt9t002_chroma_scale;
				param->p_awb_param = &mt9t002_awb_param;
				param->p_50hz_lines = mt9t002_50hz_lines;
				param->p_60hz_lines = mt9t002_60hz_lines;
				param->p_tile_config = &mt9t002_tile_config;
				param->p_ae_agc_dgain = mt9t002_ae_agc_dgain;
				param->p_ae_sht_dgain = mt9t002_ae_sht_dgain_1080p30;
				param->p_dlight_range = mt9t002_dlight;
				param->p_manual_LE = mt9t002_manual_LE;
				*psensor_name = "mt9t002";
				break;
			case AR0331_PURE_HDR:
			case AR0331_COMBI_HDR:
			default:
				sensor_lb = AR_0331;
				param->p_adj_param = &ar0331_adj_param;
				param->p_rgb2yuv = ar0331_rgb2yuv;
				param->p_chroma_scale = &ar0331_chroma_scale;
				param->p_awb_param = &ar0331_awb_param;
				param->p_50hz_lines = ar0331_50hz_lines;
				param->p_60hz_lines = ar0331_60hz_lines;
				param->p_tile_config = &ar0331_tile_config;
				param->p_ae_agc_dgain = ar0331_ae_agc_dgain;
				param->p_ae_sht_dgain = ar0331_ae_sht_dgain;
				param->p_dlight_range = ar0331_dlight;
				param->p_manual_LE = ar0331_manual_LE;
				break;
			}
			break;
		}

		if (strncmp("s5k5b3gx", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = SAM_S5K5B3;
			param->p_adj_param = &s5k5b3gx_adj_param;
			param->p_rgb2yuv = s5k5b3gx_rgb2yuv;
			param->p_chroma_scale = &s5k5b3gx_chroma_scale;
			param->p_awb_param = &s5k5b3gx_awb_param;
			param->p_50hz_lines = s5k5b3gx_50hz_lines;
			param->p_60hz_lines = s5k5b3gx_60hz_lines;
			param->p_tile_config = &s5k5b3gx_tile_config;
			param->p_ae_agc_dgain = s5k5b3gx_ae_agc_dgain;
			param->p_ae_sht_dgain = s5k5b3gx_ae_sht_dgain;
			param->p_dlight_range = s5k5b3gx_dlight;
			param->p_manual_LE = s5k5b3gx_manual_LE;
			break;
		}

		if (strncmp("mn34041pl", sensor_name, strlen(sensor_name)) == 0) {
			sensor_lb = MN_34041PL;
			param->p_adj_param = &mn34041pl_adj_param;
			param->p_rgb2yuv = mn34041pl_rgb2yuv;
			param->p_chroma_scale = &mn34041pl_chroma_scale;
			param->p_awb_param = &mn34041pl_awb_param;
			param->p_50hz_lines = mn34041pl_50hz_lines;
			param->p_60hz_lines = mn34041pl_60hz_lines;
			param->p_tile_config = &mn34041pl_tile_config;
			param->p_ae_agc_dgain = mn34041pl_ae_agc_dgain;
			param->p_ae_sht_dgain = mn34041pl_ae_sht_dgain;
			param->p_dlight_range = mn34041pl_dlight;
			param->p_manual_LE = mn34041pl_manual_LE;
			break;
		}

		AVF_LOGE("unknown sensor: %s", sensor_name);
		return -1;

	} while (0);

	if ((rval = img_config_sensor_info(sensor_lb)) < 0) {
		AVF_LOGE("img_config_sensor_info failed %d", rval);
		return rval;
	}

	return 0;
}

static uint8_t g_aaa_inited = 0;
static uint8_t g_aaa_started = 0;

int amba_aaa_init(void)
{
	if (g_aaa_inited == 0) {
		int rval = img_lib_init(0, 0);
		if (rval < 0) {
			AVF_LOGE("img_lib_init failed %d", rval);
			return rval;
		}
		g_aaa_inited = 1;
	}
	return 0;
}

void amba_aaa_stop(void)
{
	if (g_aaa_started) {
		int rval = img_stop_aaa();
		if (rval < 0)
			AVF_LOGE("image_stop_aaa failed %d", rval);
		g_aaa_started = 0;
	}
}

int amba_aaa_deinit(void)
{
	if (g_aaa_inited) {
		int rval;
		amba_aaa_stop();
		rval = img_lib_deinit();
		if (rval < 0)
			AVF_LOGE("img_lib_deinit failed %d", rval);
		g_aaa_inited = 0;
	}
	return 0;
}

static char aaa_sensor_name[32] = {0};
static uint32_t aaa_fps_q9 = -1;
static uint32_t aaa_video_width = -1;
static uint32_t aaa_video_height = -1;
image_sensor_param_t dsp_image_sensor_param;

extern "C" void ae_set_target_bias(s16 bias);

int amba_aaa_set_image_control(
	int fd_iav,
	const char *name,
	const char *value)
{
	if (!g_aaa_started) {
		AVF_LOGE("img_ctrl(%s:%s): aaa not started", name, value);
		return -1;
	}

	if (::strcmp(name, "antiflicker") == 0) {
		return img_ae_set_antiflicker(atoi(value));
	}

	if (::strcmp(name, "metermode") == 0) {
		return img_ae_set_meter_mode((ae_metering_mode)atoi(value));
	}

	if (::strcmp(name, "backlight") == 0) {
		return img_ae_set_backlight(atoi(value));
	}

	if (::strcmp(name, "ae_set_target_bias") == 0) {
		ae_set_target_bias(atoi(value));
		return 0;
	}

	// printf("set_image_control: %s = %s\n", name, value);
	return -1;
}

int amba_aaa_start(
	int iav_fd, 
	const char *sensor_name, 
	uint32_t fps_q9, 
	uint32_t video_width,
	uint32_t video_height,
	int mirror_vertical)
{
	aaa_api_t aaa_api;
	int rval;

	if (g_aaa_inited) {
		if (g_aaa_started) {
			AVF_LOGD("imglib already started");
			return 0;
		}
	} else {
		AVF_LOGE("Call imglib_init() first!");
		return -1;
	}

//	if (strcmp(aaa_sensor_name, sensor_name) || fps_q9 != aaa_fps_q9 || 
//			video_width != aaa_video_width || video_height != aaa_video_height) {

	do {

		AVF_LOGI("Init sensor");

		if ((rval = img_config_lens_info(LENS_CMOUNT_ID)) < 0) {
			AVF_LOGE("img_config_lens_info failed %d", rval);
			return 0;	//
		}

		if ((rval = img_lens_init()) < 0) {
			AVF_LOGE("img_lens_init failed %d", rval);
			return 0;	//
		}

		if ((rval = dsp_image_sensor_config(&dsp_image_sensor_param, 
				&sensor_name, fps_q9, video_width, video_height)) < 0) {
			return 0;	//
		}

		if ((rval = load_dsp_cc_table()) < 0) {
			return 0;	// 
		}

		if ((rval = load_adj_cc_table(sensor_name)) < 0) {
			return 0;	// 
		}

		if ((rval = img_load_image_sensor_param(dsp_image_sensor_param)) < 0) {
			return 0;	// 
		}

		memset(&aaa_api, 0, sizeof(aaa_api));
		if ((rval = img_register_aaa_algorithm(aaa_api)) < 0) {
			AVF_LOGE("img_register_aaa_algorithm failed %d", rval);
			return -1;
		}

		strncpy(aaa_sensor_name, sensor_name, sizeof(aaa_sensor_name));
		aaa_sensor_name[sizeof(aaa_sensor_name)-1] = 0;
		aaa_fps_q9 = fps_q9;
		aaa_video_width = video_width;
		aaa_video_height = video_height;

	} while (0);
//	else {

//		img_dsp_config_statistics_info(iav_fd, dsp_image_sensor_param.p_tile_config);

//	}

	if ((rval = img_start_aaa(iav_fd)) < 0) {
		AVF_LOGE("img_start_aaa failed %d", rval);
		return -1;
	}

	calib_load_fpn_table(iav_fd, mirror_vertical ? "/calibration/bpc_mirror.bin" : "/calibration/bpc.bin");
	calib_load_vig_table(iav_fd, "/calibration/vignette.bin");
	calib_load_wb_shift("/calibration/wb.bin");

	/// todo
	AVF_LOGI(C_WHITE "wait 1000 ms for aaa ready" C_NONE);
	avf_msleep(1000);

	g_aaa_started = 1;
	return 0;
}

void amba_aaa_restart(int fd)
{
	img_dsp_config_statistics_info(fd, dsp_image_sensor_param.p_tile_config);
}

