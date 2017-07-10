
#ifndef __AMBA_VIDEO_SOURCE_H__
#define __AMBA_VIDEO_SOURCE_H__

#define NUM_VSRC_STREAMS		4
#define NUM_OVERLAY_AREAS		3

#define VSRC_CLUT_ID(_stream, _area) \
	((_stream) * NUM_OVERLAY_AREAS + (_area))

// encoding format
enum {
	VSRC_FMT_DISABLED,
	VSRC_FMT_H264,
	VSRC_FMT_MJPEG,
};

typedef struct vsrc_stream_info_s {
	int	format;
	int	width;
	int	height;
	int framerate;		// FrameRate_Unknown
	uint32_t rate;		// e.g. 3003
	uint32_t scale;		// e.g. 90000
	// only valid for H.264
	uint32_t	fcc;
	struct {
		uint16_t	ar_x;
		uint16_t	ar_y;
		uint8_t		mode;
		uint8_t		M;
		uint8_t		N;
		uint8_t		gop_model;
		uint32_t	idr_interval;
		uint32_t	rate;
		uint32_t	scale;
		uint32_t	bitrate;
		uint32_t	bitrate_min;
		uint32_t	idr_size;
		uint32_t	reserved[5];
	} extra;
} vsrc_stream_info_t;

typedef struct vsrc_overlay_area_s {
	int8_t		valid;		// in - is this area used
	int8_t		visible;		// in - is this area visible
	uint8_t		clut_id;		// in
	int16_t		width;		// in, out
	int16_t		height;		// in, out
	int16_t		pitch;		// out, bytes per line
	int16_t		x_off;		// in
	int16_t		y_off;		// in
	int16_t		y_off_inner;
	uint8_t		*data;		// out
	uint32_t	total_size;	// out
} vsrc_overlay_area_t;

typedef struct vsrc_overlay_info_s {
	vsrc_overlay_area_t area[NUM_OVERLAY_AREAS];
} vsrc_overlay_info_t;

#define CLUT_ENTRY(_y, _u, _v, _a) \
	(((uint32_t)(_a) << 24) | ((uint32_t)(_y) << 16) | ((uint32_t)(_u) << 8) | (uint32_t)(_v))

enum {
	PIC_UNKNOWN = 0,
	PIC_H264_IDR = 1,
	PIC_I = 2,
	PIC_P = 3,
	PIC_B = 4,
	PIC_JPEG = 5,
};

typedef struct vsrc_bits_buffer_s {
	uint8_t *buffer1;
	uint32_t size1;

	uint8_t *buffer2;
	uint32_t size2;

	uint64_t pts64;
	uint64_t dts64;
	uint32_t length;

	uint32_t stream_id;
	uint32_t frame_counter;

	uint8_t is_eos;
	uint8_t pic_type;
} vsrc_bits_buffer_t;

typedef struct vsrc_device_s vsrc_device_t;

struct vsrc_device_s {
	void *pData;

	// device control
	void (*Release)(vsrc_device_t *pDev);
	int (*Open)(vsrc_device_t *pDev);
	void (*Close)(vsrc_device_t *pDev);

	// device info
	int (*GetMaxNumStream)(vsrc_device_t *pDev);
	int (*GetStreamInfo)(vsrc_device_t *pDev, int index, vsrc_stream_info_t *pStreamInfo);

	// vin
	int (*SetVinConfig)(vsrc_device_t *pDev, const char *config);
	int (*SetClockMode)(vsrc_device_t *pDev, const char *mode);
	int (*GetVinConfig)(vsrc_device_t *pDev, vin_config_info_t *info);

	// vout
	int (*ChangeVoutVideo)(vsrc_device_t *pDev, const char *config);

	// video streams
	int (*SetVideoConfig)(vsrc_device_t *pDev, int index, const char *config);
	int (*SetVideoFrameRateConfig)(vsrc_device_t *pDev, int index, const char *config);
	int (*UpdateVideoConfigs)(vsrc_device_t *pDev);

	// video overlays
	int (*SetOverlayClut)(vsrc_device_t *pDev, unsigned int clut_id, const uint32_t *pClut);
	int (*ConfigOverlay)(vsrc_device_t *pDev, int index, vsrc_overlay_info_t *pOverlayInfo);
	int (*UpdateOverlay)(vsrc_device_t *pDev, int index, vsrc_overlay_info_t *pOverlayInfo);
	int (*RefreshOverlay)(vsrc_device_t *pDev, int index, int area);

	// image control
	int (*SetImageControl)(vsrc_device_t *pDev, const char *pName, const char *pValue);

	// state control
	int (*EnterIdle)(vsrc_device_t *pDev);
	int (*EnterPreview)(vsrc_device_t *pDev);
	int (*StartEncoding)(vsrc_device_t *pDev);
	int (*StopEncoding)(vsrc_device_t *pDev);
	int (*NeedReinitPreview)(vsrc_device_t *pDev);
	int (*SupportStillCapture)(vsrc_device_t *pDev);
	int (*IsStillCaptureMode)(vsrc_device_t *pDev);

	// stream control
	int (*NewSegment)(vsrc_device_t *pDev, uint32_t streamFlags);
	int (*ReadBits)(vsrc_device_t *pDev, vsrc_bits_buffer_t *pBitsBuffer);
};

int vsrc_Init(vsrc_device_t *pDev);

#endif

