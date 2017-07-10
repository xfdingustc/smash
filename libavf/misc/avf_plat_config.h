
#ifndef __AVF_PLAT_CONFIG_H__
#define __AVF_PLAT_CONFIG_H__

// vout id
enum {
	VOUT_0 = 0,		// LCD
	VOUT_1 = 1,		// HDMI
	NUM_VOUT
};

// vout device type
enum {
	VOUT_TYPE_AUTO,
	VOUT_TYPE_CVBS,
	VOUT_TYPE_SVIDEO,
	VOUT_TYPE_YPbPr,
	VOUT_TYPE_HDMI,
	VOUT_TYPE_DIGITAL,
	VOUT_TYPE_MIPI,
};

// vout mode
enum {
	// for Analog and HDMI
	VOUT_MODE_480i,
	VOUT_MODE_576i,
	VOUT_MODE_480p,
	VOUT_MODE_576p,

	VOUT_MODE_720p,
	VOUT_MODE_720p50,
	VOUT_MODE_720p30,
	VOUT_MODE_720p25,
	VOUT_MODE_720p24,

	VOUT_MODE_1080i,
	VOUT_MODE_1080i50,
	VOUT_MODE_1080p24,
	VOUT_MODE_1080p25,
	VOUT_MODE_1080p30,
	VOUT_MODE_1080p,
	VOUT_MODE_1080p50,

	VOUT_MODE_native,

	// for LCD
	VOUT_MODE_D480I,
	VOUT_MODE_D576I,
	VOUT_MODE_D480P,
	VOUT_MODE_D576P,

	VOUT_MODE_D720P,
	VOUT_MODE_D720P50,

	VOUT_MODE_D1080I,
	VOUT_MODE_D1080I50,
	VOUT_MODE_D1080P24,
	VOUT_MODE_D1080P25,
	VOUT_MODE_D1080P30,
	VOUT_MODE_D1080P,
	VOUT_MODE_D1080P50,

	VOUT_MODE_D960x240,
	VOUT_MODE_D320x240,
	VOUT_MODE_OFF,
	VOUT_MODE_D320x288,
	VOUT_MODE_D360x240,
	VOUT_MODE_D360x288,
	VOUT_MODE_D480x640,
	VOUT_MODE_D480x800,

	VOUT_MODE_hvga,
	VOUT_MODE_vga,
	VOUT_MODE_wvga,
	VOUT_MODE_D240x400,
	VOUT_MODE_xga,
	VOUT_MODE_wsvga,
	VOUT_MODE_D960x540,
	VOUT_MODE_D400x400,

	// total number
	VOUT_MODE_NUM
};

// HDMI color space
enum {
	HDMI_CS_AUTO,
	HDMI_CS_RGB,
	HDMI_CS_YCBCR_444,
	HDMI_CS_YCBCR_422,
};

// flip
enum {
	VOUT_FLIP_NORMAL,
	VOUT_FLIP_HV,
	VOUT_FLIP_H,
	VOUT_FLIP_V,
};

// rotate
enum {
	VOUT_ROTATE_NORMAL,
	VOUT_ROTATE_90,
};

typedef struct vout_config_s {
	// need be set

	unsigned	vout_id;	// VOUT_0, VOUT_1
	unsigned	vout_type;	// 
	unsigned	vout_mode;
	unsigned	fps;		// 
	int			fb_id;		// 

	// can be default

	char		disable_csc;
	char		enable_osd_copy;
	char		hdmi_color_space;
	char		enable_video;
	char		video_flip;
	char		video_rotate;

	char		osd_flip;
	char		osd_rotate;

	char		video_size_flag;
	short		video_width;
	short		video_height;

	char		video_offset_flag;
	short		video_offset_x;
	short		video_offset_y;

	char		osd_rescale_flag;
	short		osd_rescale_width;
	short		osd_rescale_height;

	char		osd_offset_flag;
	short		osd_offset_x;
	short		osd_offset_y;
} vout_config_t;

enum {
	MODE_IDLE,
	MODE_DECODE,
//	MODE_ENCODE,
};

typedef struct vout_info_s {
	int	enabled;
	int vout_width;
	int vout_height;
	int video_width;
	int video_height;
	int video_xoff;
	int video_yoff;
} vout_info_t;

typedef struct vout_brightness_s {
	unsigned vout_id;
	unsigned vout_type;
	unsigned brightness;
} vout_brightness_t;

int avf_platform_init(void);
int avf_platform_cleanup(void);

// video and osd
int avf_platform_display_set_mode(int mode);

int avf_platform_display_set_vout(vout_config_t *config, const char *lcd_model);
int avf_platform_display_disable_vout(int vout_id);
int avf_platform_display_set_backlight(int vout_id, const char *lcd_model, int on);
int avf_platform_display_set_brightness(const vout_brightness_t *param);
int avf_platform_display_get_brightness(vout_brightness_t *param);
int avf_platform_display_change_vout_video(int iav_fd, int vout_id, int vout_type,
	int xoff, int yoff, int width, int height, vout_info_t *vout_info);
int avf_platform_display_flip_vout_video(int vout_id, int vout_type, int flip);
int avf_platform_display_flip_vout_osd(int vout_id, int vout_type, int flip);

int avf_platform_display_get_vout_info(int vout_id, vout_info_t *info);
int avf_platform_set_clock_mode(const char *clock_mode);

#endif

