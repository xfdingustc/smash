
typedef struct vout_mode_s {
	const char *name;
	int mode;
	short width;
	short height;
	char format;
} vout_mode_t;

#define INTERLACED	AMBA_VIDEO_FORMAT_INTERLACE
#define PROGRESSIVE	AMBA_VIDEO_FORMAT_PROGRESSIVE

static const vout_mode_t g_vout_modes[] = {
	{"480i", AMBA_VIDEO_MODE_480I, 720, 480, INTERLACED},
	{"576i", AMBA_VIDEO_MODE_576I, 720, 576, INTERLACED},
	{"480p", AMBA_VIDEO_MODE_D1_NTSC, 720, 480, PROGRESSIVE},
	{"576p", AMBA_VIDEO_MODE_D1_PAL, 720, 576, PROGRESSIVE},

	{"720p", AMBA_VIDEO_MODE_720P, 1280, 720, PROGRESSIVE},
	{"720p50", AMBA_VIDEO_MODE_720P_PAL, 1280, 720, PROGRESSIVE},
	{"720p30", AMBA_VIDEO_MODE_720P30, 1280, 720, PROGRESSIVE},
	{"720p25", AMBA_VIDEO_MODE_720P25, 1280, 720, PROGRESSIVE},
	{"720p24", AMBA_VIDEO_MODE_720P24, 1280, 720, PROGRESSIVE},

	{"1080i", AMBA_VIDEO_MODE_1080I, 1920, 1080, INTERLACED},
	{"1080i50", AMBA_VIDEO_MODE_1080I_PAL, 1920, 1080, INTERLACED},
	{"1080p24", AMBA_VIDEO_MODE_1080P24, 1920, 1080, PROGRESSIVE},
	{"1080p25", AMBA_VIDEO_MODE_1080P25, 1920, 1080, PROGRESSIVE},
	{"1080p30", AMBA_VIDEO_MODE_1080P30, 1920, 1080, PROGRESSIVE},
	{"1080p", AMBA_VIDEO_MODE_1080P, 1920, 1080, PROGRESSIVE},
	{"1080p50", AMBA_VIDEO_MODE_1080P_PAL, 1920, 1080, PROGRESSIVE},

	{"native", AMBA_VIDEO_MODE_HDMI_NATIVE, 0, 0, PROGRESSIVE},


	{"D480I", AMBA_VIDEO_MODE_480I, 720, 480, INTERLACED},
	{"D576I", AMBA_VIDEO_MODE_576I, 720, 576, INTERLACED},
	{"D480P", AMBA_VIDEO_MODE_D1_NTSC, 720, 480, PROGRESSIVE},
	{"D576P", AMBA_VIDEO_MODE_D1_PAL, 720, 576, PROGRESSIVE},

	{"D720P", AMBA_VIDEO_MODE_720P, 1280, 720, PROGRESSIVE},
	{"D720P50", AMBA_VIDEO_MODE_720P_PAL, 1280, 720, PROGRESSIVE},

	{"D1080I", AMBA_VIDEO_MODE_1080I, 1920, 1080, INTERLACED},
	{"D1080I50", AMBA_VIDEO_MODE_1080I_PAL, 1920, 1080, INTERLACED},
	{"D1080P24", AMBA_VIDEO_MODE_1080P24, 1920, 1080, PROGRESSIVE},
	{"D1080P25", AMBA_VIDEO_MODE_1080P25, 1920, 1080, PROGRESSIVE},
	{"D1080P30", AMBA_VIDEO_MODE_1080P30, 1920, 1080, PROGRESSIVE},
	{"D1080P", AMBA_VIDEO_MODE_1080P, 1920, 1080, PROGRESSIVE},
	{"D1080P50", AMBA_VIDEO_MODE_1080P_PAL, 1920, 1080, PROGRESSIVE},

	{"D960x240", AMBA_VIDEO_MODE_960_240, 960, 240, PROGRESSIVE},	//AUO27
	{"D320x240", AMBA_VIDEO_MODE_320_240, 320, 240, PROGRESSIVE},	//AUO27
	{"D320x240off", AMBA_VIDEO_MODE_OFF, 320, 240, PROGRESSIVE},	// t15p00
	{"D320x288", AMBA_VIDEO_MODE_320_288, 320, 288, PROGRESSIVE},	//AUO27
	{"D360x240", AMBA_VIDEO_MODE_360_240, 360, 240, PROGRESSIVE},	//AUO27
	{"D360x288", AMBA_VIDEO_MODE_360_288, 360, 288, PROGRESSIVE},	//AUO27
	{"D480x640", AMBA_VIDEO_MODE_480_640, 480, 640, PROGRESSIVE},	//P28K
	{"D480x800", AMBA_VIDEO_MODE_480_800, 480, 800, PROGRESSIVE},	//TPO648

	{"hvga", AMBA_VIDEO_MODE_HVGA, 320, 480, PROGRESSIVE},	//TPO489
	{"vga", AMBA_VIDEO_MODE_VGA, 640, 480, PROGRESSIVE},
	{"wvga", AMBA_VIDEO_MODE_WVGA, 800, 480, PROGRESSIVE},	//TD043
	{"D240x400", AMBA_VIDEO_MODE_240_400, 240, 400, PROGRESSIVE},	//WDF2440
	{"xga", AMBA_VIDEO_MODE_XGA, 1024, 768, PROGRESSIVE},	//EJ080NA
	{"wsvga", AMBA_VIDEO_MODE_WSVGA, 1024, 600, PROGRESSIVE},	//AT070TNA2
	{"D960x540", AMBA_VIDEO_MODE_960_540, 960, 540, PROGRESSIVE},	//E330QHD
#ifndef ARCH_A5S
	{"D400x400",	 AMBA_VIDEO_MODE_400_400, 400, 400, PROGRESSIVE},	//
#endif
};

