


#define PTS_MASK		0x3FFFFFFF
#define PTS_DELTA		0x10000000
#define PTS_WRAP_INC	0x40000000

// hw pts: 12.288 Mhz
#define PTS_HW_TO_90k(_pts) \
	((_pts) * 15 / 2048)
//	((_pts) * 90000 / 12288000)

#define PTS_90k_TO_HW(_90k) \
	((_90k) * 2048 / 15)

/*===========================================================================*/

typedef struct fps_map_s {
	const char *name;
	uint32_t fps;		// frames per second, for calc IDR
	uint32_t fps_q9;
	uint8_t framerate;	// FrameRate_Unknown
	uint8_t framerate_div2;
	uint8_t framerate_div4;
	uint8_t framerate_div8;
} fps_map_t;

static const fps_map_t amba_fps_map[] = {
	{"120",		120,	AMBA_VIDEO_FPS_120,		FrameRate_120,		FrameRate_60,		FrameRate_30,		FrameRate_15},
	{"60",		60,	AMBA_VIDEO_FPS_60,		FrameRate_60,		FrameRate_30,		FrameRate_15,		FrameRate_Unknown},
	{"59.94",	60,	AMBA_VIDEO_FPS_59_94,	FrameRate_59_94,	FrameRate_29_97,	FrameRate_14_985,	FrameRate_Unknown},
	{"50",		50,	AMBA_VIDEO_FPS_50,		FrameRate_50,		FrameRate_25,		FrameRate_Unknown,	FrameRate_Unknown},
	{"30",		30,	AMBA_VIDEO_FPS_30,		FrameRate_30,		FrameRate_15,		FrameRate_Unknown,	FrameRate_Unknown},
	{"29.97",	30,	AMBA_VIDEO_FPS_29_97,	FrameRate_29_97,	FrameRate_14_985,	FrameRate_Unknown,	FrameRate_Unknown},
	{"25",		25,	AMBA_VIDEO_FPS_25,		FrameRate_25,		FrameRate_Unknown,	FrameRate_Unknown,	FrameRate_Unknown},
	{"24",		24,	AMBA_VIDEO_FPS_24,		FrameRate_24,		FrameRate_Unknown,	FrameRate_Unknown,	FrameRate_Unknown},
	{"23.976",	24,	AMBA_VIDEO_FPS_23_976,	FrameRate_23_976,	FrameRate_Unknown,	FrameRate_Unknown,	FrameRate_Unknown},
	{"20",		20,	AMBA_VIDEO_FPS_20,		FrameRate_20,		FrameRate_Unknown,	FrameRate_Unknown,	FrameRate_Unknown},
};

const fps_map_t *amba_find_fps_by_name(const char *name)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(amba_fps_map); i++) {
		if (strcmp(name, amba_fps_map[i].name) == 0)
			return amba_fps_map + i;
	}

	return NULL;
}

// 1:1, 1:2, 1:4, or 1:8
int amba_find_framerate(uint32_t framerate, int fps_multi, int fps_div)
{
	if (fps_multi == fps_div)
		return framerate;

	int new_framerate = FrameRate_Unknown;
	for (unsigned i = 0; i < ARRAY_SIZE(amba_fps_map); i++) {
		if (amba_fps_map[i].framerate == framerate) {
			if (fps_multi * 2 == fps_div) {
				new_framerate = amba_fps_map[i].framerate_div2;
			} else if (fps_multi * 4 == fps_div) {
				new_framerate = amba_fps_map[i].framerate_div4;
			} else if (fps_multi * 8 == fps_div) {
				new_framerate = amba_fps_map[i].framerate_div8;
			} else {
				switch (framerate) {
				case FrameRate_29_97:
				case FrameRate_30:
					if (fps_multi * 30 == fps_div) {
						new_framerate = FrameRate_1;
					}
					break;

				case FrameRate_59_94:
				case FrameRate_60:
					if (fps_multi * 60 == fps_div) {
						new_framerate = FrameRate_1;
					}
					break;

				case FrameRate_120:
					if (fps_multi * 120 == fps_div) {
						new_framerate = FrameRate_1;
					}
					break;

				case FrameRate_240:
					if (fps_multi * 240 == fps_div) {
						new_framerate = FrameRate_1;
					}
					break;

				default:
					break;
				}
			}
			break;
		}
	}

	if (new_framerate == FrameRate_Unknown) {
		AVF_LOGE("framerate is unknown, %d %d:%d", framerate, fps_multi, fps_div);
	}

	return new_framerate;
}

int amba_calc_rate_90k(uint32_t framerate, int fps_multi, int fps_div)
{
	int new_framerate = amba_find_framerate(framerate, fps_multi, fps_div);

	switch (new_framerate) {
	default:
	case FrameRate_Unknown:
		AVF_LOGE("framerate %d %d:%d not defined", framerate, fps_multi, fps_div);
		return 0;

	case FrameRate_12_5: return 7200;
	case FrameRate_6_25: return 14400;
	case FrameRate_23_976: return 3753;
	case FrameRate_24: return 3750;
	case FrameRate_25: return 3600;
	case FrameRate_29_97: return 3003;
	case FrameRate_30: return 3000;
	case FrameRate_50: return 1800;
	case FrameRate_59_94: return 1501;
	case FrameRate_60: return 1500;
	case FrameRate_120: return 750;
	case FrameRate_240: return 375;
	case FrameRate_20: return 4500;
	case FrameRate_15: return 6000;
	case FrameRate_14_985: return 6006;
	case FrameRate_1: return 90000;
	}
}

/*===========================================================================*/

typedef struct amba_video_mode_map_s {
	const char *name;
	uint32_t mode;
	int width;
	int height;
} amba_video_mode_map_t;

static const amba_video_mode_map_t amba_video_mode_map[] = {
	{"auto",		AMBA_VIDEO_MODE_AUTO, -1,-1},

	{"720p",		AMBA_VIDEO_MODE_720P, 1280,720},
	{"1280x720",		AMBA_VIDEO_MODE_720P, 1280,720},

	{"1080p",		AMBA_VIDEO_MODE_1080P, 1920,1080},
	{"1920x1080",		AMBA_VIDEO_MODE_1080P, 1920,1080},

	{"2048x1152",		AMBA_VIDEO_MODE_2048_1152, 2048,1152},
	{"1920x1440",		AMBA_VIDEO_MODE_1920_1440, 1920,1440,},

	{"3072x2048",		AMBA_VIDEO_MODE_3072_2048, 3072,2048},
#ifdef ARCH_S2L
	{"3072x1728",		AMBA_VIDEO_MODE_3072_1728, 3072,1728},
#endif
	{"2048x1536",		AMBA_VIDEO_MODE_QXGA, 2048,1536},
	{"2592x1944",		AMBA_VIDEO_MODE_QSXGA, 2592,1944},
	{"2688x1512",		AMBA_VIDEO_MODE_2688_1512, 2688,1512},
	{"2688x1520",		AMBA_VIDEO_MODE_2688_1520, 2688,1520},
	{"2048x2048",		AMBA_VIDEO_MODE_2048_2048, 2048,2048},
};

int amba_find_video_mode_by_name(const char *name,
	uint32_t *pvideo_mode, int *pwidth, int *pheight)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(amba_video_mode_map); i++) {
		if (strcmp(name, amba_video_mode_map[i].name) == 0) {
			*pvideo_mode = amba_video_mode_map[i].mode;
			*pwidth = amba_video_mode_map[i].width;
			*pheight = amba_video_mode_map[i].height;
			return i;
		}
	}

	return -1;
}

void calc_aspect_ratio(int width, int height, int *ar_x, int *ar_y)
{
	*ar_x = 1;
	*ar_y = 1;

	if (height == 0)
		return;

	int numerator = width * 18 / height;
	switch (numerator) {
	case 24:
		*ar_x = 4;
		*ar_y = 3;
		break;

	case 26:	// 352x288
	case 27:
		*ar_x = 3;
		*ar_y = 2;
		break;

	case 31:
	case 32:
		*ar_x = 16;
		*ar_y = 9;
		break;

	case 18:
		*ar_x = 1;
		*ar_y = 1;
		break;

	default:
		AVF_LOGW("unknown aspect ratio: %d,%d", width, height);
		break;
	}
}

