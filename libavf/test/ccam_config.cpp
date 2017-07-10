

//============================================================

enum {
	AR_16_9,	// 16:9
	AR_3_2,		// 3:2
	AR_4_3,		// 4:3
	AR_1_1,		// 1:1
};

typedef struct video_config_s {
	const char *name;
	int width;
	int height;
	int framerate;
	int aspect_ratio;
	const char *overlay;
	enum Video_Resolution std_resolution;
	int bitrate;
	const char *clock_mode;
} video_config_t;

// valid:xoff,yoff,width,height:fontsize
#define OVERLAY_QXGA		"1:0,0,0,64:52"		// 24
#define OVERLAY_1080p		"1:0,0,0,60:48"		// 24
#define OVERLAY_720p		"1:0,0,0,40:32"		// 16
#define OVERLAY_1024x576	"1:0,0,0,32:26"		// 12.8
#define OVERLAY_960x720		"1:0,0,0,30:24"		// 
#define OVERLAY_480p		"1:0,0,0,22:18"		// 9
#define OVERLAY_512x288		"1:0,0,0,16:13"		// 6.4
#define OVERLAY_960x640		"1:0,0,0,30:24"		// 12
#define OVERLAY_640x360		"1:0,0,0,20:16"		// 8
#define OVERLAY_480x320		"1:0,0,0,15:12"		// 6
#define OVERLAY_NONE		""

// cat /proc/ambarella/mode
// 720p30 1080p30 3Mp30 1080p60

static const video_config_t g_video_config[] =
{
#ifdef ARCH_S2L
	// ----------------------------------------------------------------------------
	{
		"1080p60",
		1920,1080,60,AR_16_9,		// resolution, fps, aspect_ratio
		OVERLAY_1080p,				// overlay
		Video_Resolution_1080p60,	// std_resolution
		30*MBPS,					// bitrate
		"1080p60",					// clock_mode
	},

#if 0
	// ----------------------------------------------------------------------------
	{
		"QXGAp30 (2048x1536)",
		2048,1536,30,AR_4_3,			// resolution, fps, aspect_ratio
		OVERLAY_QXGA,				// overlay
		Video_Resolution_QXGAp30,	// std_resolution
		30*MBPS,					// bitrate
		"3Mp30",						// clock_mode
	},
#endif
#endif

	// ----------------------------------------------------------------------------
	{
		"1080p30",
		1920,1080,30,AR_16_9,		// resolution, fps, aspect_ratio
		OVERLAY_1080p,				// overlay
		Video_Resolution_1080p30,	// std_resolution
#ifdef ARCH_S2L
		20*MBPS,					// bitrate
#else
		12*MBPS,					// bitrate
#endif
		"1080p30",					// clock_mode
	},

#ifdef ARCH_S2L
	// ----------------------------------------------------------------------------
	{
		"720p120",
		1280,720,120,AR_16_9,		// resolution, fps, aspect_ratio
		OVERLAY_720p,				// overlay
		Video_Resolution_720p120,	// std_resolution
		30*MBPS,					// bitrate
		"720p120",					// clock_mode
	},
#endif

	// ----------------------------------------------------------------------------
	{
		"720p60",
		1280,720,60,AR_16_9,			// resolution, fps, aspect_ratio
		OVERLAY_720p,				// overlay
		Video_Resolution_720p60,	// std_resolution
#ifdef ARCH_S2L
		20*MBPS,					// bitrate
#else
		12*MBPS,					// bitrate
#endif
		"720p60",					// clock_mode
	},

	// ----------------------------------------------------------------------------
	{
		"720p30",
		1280,720,30,AR_16_9,			// resolution, fps, aspect_ratio
		OVERLAY_720p,				// overlay
		Video_Resolution_720p30,	// std_resolution
#ifdef ARCH_S2L
		15*MBPS,					// bitrate
#else
		8*MBPS,						// bitrate
#endif
		"720p30",					// clock_mode
	},

	// ----------------------------------------------------------------------------
	{
		"IMX178Calib",
		2688,1512,25,AR_16_9,
		OVERLAY_1080p,
		Video_Resolution_2688x1512p25,
		20*MBPS,
		"still",	// for test
	},

#if 0
	// ----------------------------------------------------------------------------
	{
		"480p30",
		720,480,30,AR_3_2,			// resolution, fps, aspect_ratio
		OVERLAY_480p,				// overlay
		Video_Resolution_480p30,	// std_resolution
		5*MBPS,						// bitrate
		"720p30",					// clock_mode
	},

	// ----------------------------------------------------------------------------
	{
		"1440x1080",
		1440,1080,30,AR_4_3,			// resolution, fps, aspect_ratio
		OVERLAY_1080p,				// overlay
		Video_Resolution_1440x1080,	// std_resolution
#ifdef ARCH_S2L
		20*MBPS, 					// bitrate
#else
		12*MBPS,					// bitrate
#endif
		"1080p30",					// clock_mode
	},
#endif

	// ----------------------------------------------------------------------------
	{
		"1504p30",
		1504,1504,30,AR_1_1,
		OVERLAY_1024x576,
		Video_Resolution_1504x1504p30,
		10*MBPS,
		"1080p60",	// for test
	},

	// ----------------------------------------------------------------------------
	{
		"4Mp30",
		2048,2048,30,AR_1_1,
		OVERLAY_1080p,
		Video_Resolution_2048x2048p30,
		30*MBPS,
		"4Mp30",	// for test
	},

	// ----------------------------------------------------------------------------
	{
		"1536p30",
		1536,1536,30,AR_1_1,
		OVERLAY_1024x576,
		Video_Resolution_1536x1536p30,
		10*MBPS,
		"1080p60",	// for test
	},
};

static int find_video_config(const char *name)
{
	for (avf_uint_t i = 0; i < ARRAY_SIZE(g_video_config); i++) {
		if (::strcmp(name, g_video_config[i].name) == 0) {
			return i;
		}
	}
	AVF_LOGE("video config '%s' not found", name);
	return -1;
}

//============================================================

typedef struct preview_size_info_s {
	int width;
	int height;
	const char *overlay;
} preview_size_info_t;

typedef struct preview_config_s {
	const char *name;
	int quality;
	const preview_size_info_t size_16_9;
	const preview_size_info_t size_3_2;
	const preview_size_info_t size_4_3;
	const preview_size_info_t size_1_1;
} preview_config_t;

// normal, small, large, off
static const preview_config_t g_preview_config[] =
{
	// ----------------------------------------------------------------------------
	{
		"normal", 85,

		{ 1024,576, OVERLAY_1024x576 },		// size_16_9
		{ 960,640, OVERLAY_960x640 },		// size_3_2

#ifdef ARCH_S2L
		{ 1024,768, OVERLAY_1024x576 },		// size_4_3
#else
		{ 960,720, OVERLAY_960x720 },		// size_4_3
#endif

#ifdef ARCH_S2L
		{ 1024,1024, OVERLAY_1024x576 },	// size_1_1
#else
		{ 960,960, OVERLAY_960x720 },		// size_1_1
#endif
	},

	// ----------------------------------------------------------------------------
	{
		"small", 85,
		{ 512,288, OVERLAY_512x288 },		// size_16_9
		{ 480,320, OVERLAY_480x320 },		// size_3_2
		{ 640,480, OVERLAY_512x288 },		// size_4_3
		{ 288,288, OVERLAY_480x320 },		// size_1_1
	},

	// ----------------------------------------------------------------------------
	{
		"large", 90,
		{ 1920,1080, OVERLAY_1080p },		// size_16_9
		{ 1920,1280, OVERLAY_1080p },		// size_3_2
		{ 1920,1440, OVERLAY_1080p },		// size_4_3
		{ 1920,1920, OVERLAY_1080p },		// size_1_1
	},

	// ----------------------------------------------------------------------------
	{
		"off", 85,
		{ 0,0, OVERLAY_NONE },			// size_16_9
		{ 0,0, OVERLAY_NONE },			// size_3_2
		{ 0,0, OVERLAY_NONE },			// size_4_3
		{ 0,0, OVERLAY_NONE },			// size_1_1
	},
};

static const preview_config_t g_preview_config_v1[] =
{
	// ----------------------------------------------------------------------------
	{
		"normal", 85,

		{ 720,416, OVERLAY_1024x576 },		// size_16_9
		{ 720,480, OVERLAY_960x640 },		// size_3_2
		{ 720,540, OVERLAY_1024x576 },		// size_4_3
		{ 720,720, OVERLAY_1024x576 },	// size_1_1
	},

	// ----------------------------------------------------------------------------
	{
		"small", 85,	// same with normal
		{ 720,416, OVERLAY_1024x576 },		// size_16_9
		{ 720,480, OVERLAY_960x640 },		// size_3_2
		{ 720,540, OVERLAY_1024x576 },		// size_4_3
		{ 720,720, OVERLAY_1024x576 },	// size_1_1
	},

	// ----------------------------------------------------------------------------
	{
		"large", 90,
		{ 1920,1080, OVERLAY_1080p },		// size_16_9
		{ 1920,1280, OVERLAY_1080p },		// size_3_2
		{ 1920,1440, OVERLAY_1080p },		// size_4_3
		{ 1920,1920, OVERLAY_1080p },		// size_1_1
	},

	// ----------------------------------------------------------------------------
	{
		"off", 85,
		{ 0,0, OVERLAY_NONE },			// size_16_9
		{ 0,0, OVERLAY_NONE },			// size_3_2
		{ 0,0, OVERLAY_NONE },			// size_4_3
		{ 0,0, OVERLAY_NONE },			// size_1_1
	},
};

static const preview_config_t *get_preview_config_table(int version, unsigned *count)
{
	if (version == 0) {
		*count = ARRAY_SIZE(g_preview_config);
		return g_preview_config;
	} else {
		*count = ARRAY_SIZE(g_preview_config_v1);
		return g_preview_config_v1;
	}
}

static const preview_config_t *get_preview_config_i(int version, unsigned index)
{
	unsigned count;
	const preview_config_t *table = get_preview_config_table(version, &count);
	if (index >= count) {
		AVF_LOGE("get_preview_config_i, index: %d, count: %d", index, count);
		index = 0;
	}
	return table + index;
}

static int find_preview_config(const char *name, int version)
{
	unsigned count;
	const preview_config_t *table = get_preview_config_table(version, &count);

	for (avf_uint_t i = 0; i < count; i++) {
		if (::strcmp(name, table[i].name) == 0) {
			return i;
		}
	}

	AVF_LOGE("preview config '%s' not found", name);
	return -1;
}

//============================================================

typedef struct sec_video_config_s {
	const char *name;
	int width;
	int height;
	const char *overlay;
	int bitrate;
} sec_video_config_t;

static const sec_video_config_t g_sec_video_config[] =
{
	// ----------------------------------------------------------------------------
	// for S2L
	{
		"640x360",
		640,360,			// resolution
		OVERLAY_640x360,	// overlay
		2*MBPS,				// bitrate
	},

	// ----------------------------------------------------------------------------
	// for A5S
	{
		"512x288",
		512,288,			// resolution
		OVERLAY_512x288,	// overlay
		2*MBPS,				// bitrate
	},

	// ----------------------------------------------------------------------------
	// for S2L
	{
		"640x640",
		640,640,			// resolution
		OVERLAY_640x360,	// overlay
		4*MBPS, 			// bitrate
	},

	// ----------------------------------------------------------------------------
	{
		"off",
		0,0,				// resolution
		OVERLAY_NONE,		// overlay
		0,					// bitrate
	},
};

static const sec_video_config_t g_sec_video_config_v1[] =
{
	// ----------------------------------------------------------------------------
	// for S2L
	{
		"1024x576",
		1024,576,			// resolution
		OVERLAY_1024x576,	// overlay
		4*MBPS,				// bitrate
	},

	// ----------------------------------------------------------------------------
	// for A5S
	{
		"512x288",
		512,288,			// resolution
		OVERLAY_512x288,	// overlay
		2*MBPS,				// bitrate
	},

	// ----------------------------------------------------------------------------
	// for S2L
	{
		"640x640",
		640,640,			// resolution
		OVERLAY_640x360,	// overlay
		4*MBPS, 			// bitrate
	},

	// ----------------------------------------------------------------------------
	{
		"off",
		0,0,				// resolution
		OVERLAY_NONE,		// overlay
		0,					// bitrate
	},
};

static const sec_video_config_t *get_sec_video_config_table(int version, unsigned *count)
{
	if (version == 0) {
		*count = ARRAY_SIZE(g_sec_video_config);
		return g_sec_video_config;
	} else {
		*count = ARRAY_SIZE(g_sec_video_config_v1);
		return g_sec_video_config_v1;
	}
}

static const sec_video_config_t *get_sec_video_config_i(int version, unsigned index)
{
	unsigned count;
	const sec_video_config_t *table = get_sec_video_config_table(version, &count);
	if (index >= count) {
		AVF_LOGE("get_sec_video_config_i, bad index: %d, count: %d", index, count);
		index = 0;
	}
	return table + index;
}

static int find_sec_video_config(const char *name, int version)
{
	unsigned count;
	const sec_video_config_t *table = get_sec_video_config_table(version, &count);

	for (avf_uint_t i = 0; i < count; i++) {
		if (::strcmp(name, table[i].name) == 0) {
			return i;
		}
	}

	AVF_LOGE("second video config '%s' not found", name);
	return -1;
}

//============================================================

typedef struct vout_video_config_s {
	short video_off_x;
	short video_off_y;
	short video_width;
	short video_height;
} vout_video_config_t;

typedef struct vout_table_s {
	const char *name;
	int mode;
	const vout_video_config_t *video_config_ps;	// pan and scan mode
	const vout_video_config_t *video_config_lb;	// letterbox mode
} vout_table_t;

static vout_video_config_t g_d400_video_config_ps =
{
	0,0, 400,400
};

static vout_video_config_t g_d400_video_config_lb[] = {
	// AR_16_9
	{0,88, 400,224},
	// AR_3_2
	{0,66, 400,266},
	// AR_4_3
	{0,50, 400,300}
};

static const vout_table_t g_vout_table[] = {
	{"off", -1},
	{"480p", VOUT_MODE_480p},
	{"720p", VOUT_MODE_720p},
	{"1080p", VOUT_MODE_1080p},
	{"Dqvga", VOUT_MODE_D320x240},
	{"D400", VOUT_MODE_D400x400, &g_d400_video_config_ps, g_d400_video_config_lb},
};

static int find_vout_mode(const char *name)
{
	for (int i = 0; i < (int)ARRAY_SIZE(g_vout_table); i++) {
		if (::strcmp(name, g_vout_table[i].name) == 0)
			return g_vout_table[i].mode;
	}
	AVF_LOGE("vout mode %s not found", name);
	return -2;
}

static const vout_table_t *find_vout_table_item(int mode)
{
	for (int i = 0; i < (int)ARRAY_SIZE(g_vout_table); i++) {
		if (g_vout_table[i].mode == mode)
			return g_vout_table + i;
	}
	return NULL;
}


