
#ifndef __AVF_MEDIA_FORMAT_H__
#define __AVF_MEDIA_FORMAT_H__

#include "avf_std_media.h"

//-----------------------------------------------------------------------
//
//  media type
//
//-----------------------------------------------------------------------

enum {
	MEDIA_TYPE_NULL,
	MEDIA_TYPE_VIDEO,
	MEDIA_TYPE_AUDIO,
	MEDIA_TYPE_SUBTITLE,
	MEDIA_TYPE_AVES,
};

#define MF_NullFormat	MKFCC('n','u','l','l')

//-----------------------------------------------------------------------
//
//  video coding
//
//-----------------------------------------------------------------------

// All video format must inherit this
struct CVideoFormat: public CMediaFormat
{
	avf_u8_t mFrameRate;	// FrameRate_Unknown
	avf_u16_t mWidth;
	avf_u16_t mHeight;
};

// for test
#define MF_TestVideoFormat MKFCC('T','S','T','V')
struct CTestVideoFormat: public CVideoFormat
{
	// todo
};

typedef struct amba_h264_s {
	struct info_t {
		uint16_t	ar_x;
		uint16_t	ar_y;
		uint8_t 	mode;
		uint8_t 	M;
		uint8_t 	N;
		uint8_t 	gop_model;
		uint32_t	idr_interval;	// in TS, high is pic_width
		uint32_t	rate;			// 3003 etc
		uint32_t	scale;			// 90000 or 18000
		uint32_t	bitrate;
		uint32_t	bitrate_min;
		uint32_t	idr_size;		// in TS, high is pic_height
	} info;
	uint32_t	reserved[22];
} amba_h264_t;

// H.264
#define MF_H264VideoFormat MKFCC('a','v','c','1')
struct CH264VideoFormat: public CVideoFormat
{
	enum {
		UNK = 0, I = 1, P = 2, B = 3,
	};
	avf_fcc_t	mDataFourcc;
	// follows data
};

#define FCC_AMBA	MKFCC('A','M','B','A')
#define AMBA_DESCRIPTOR_TAG	0xFE

struct CAmbaAVCFormat: public CH264VideoFormat
{
	amba_h264_t	data;
};

// Motion JPEG
#define MF_MJPEGVideoFormat MKFCC('m','j','p','g')
struct CMJPEGVideoFormat: public CVideoFormat
{
	// todo
};

//-----------------------------------------------------------------------
//
//  audio coding
//
//-----------------------------------------------------------------------

struct CAudioFormat: public CMediaFormat
{
	avf_u32_t	mNumChannels;	// number of channles, e.g. 2
	avf_u32_t	mSampleRate;	// number of samples per second, e.g. 44100, 48000
	avf_u8_t	mBitsPerSample;	// e.g. 16
	avf_u8_t	mBitsFormat;	// little endian/big endian, todo
	avf_u32_t	mFramesPerChunk;	// 1 frame = mNumChannels samples, e.g. 1024
	//
	avf_u32_t	mDecoderDataSize;	// todo
	avf_u8_t	mDecoderData[32];

	INLINE avf_size_t BytesPerSample() {
		return mBitsPerSample / 8;
	}

	INLINE avf_size_t FrameSize() {
		return mNumChannels * BytesPerSample();
	}

	INLINE avf_size_t ChunkSize() {
		return mFramesPerChunk * FrameSize();
	}

	INLINE avf_size_t SamplesPerChunk() {
		return mFramesPerChunk * mNumChannels;
	}

	INLINE avf_size_t TimeToBytes(avf_uint_t ms) {
		return mSampleRate * ms / 1000 * FrameSize();
	}
};

#define MF_RawPcmFormat MKFCC('p','c','m','a')
struct CRawPcmFormat: public CAudioFormat
{
};

#define MF_AACFormat MKFCC('m','p','4','a')
struct CAacFormat: public CAudioFormat
{
};

#define MF_Mp3Format MKFCC('m','p','3','a')
struct CMp3Format: public CAudioFormat
{
};

//-----------------------------------------------------------------------
//
//  A/V ES format
//
//-----------------------------------------------------------------------
struct CESFormat: public CMediaFormat
{
	avf_stream_attr_t stream_attr;
	TDynamicArray<CMediaFormat*> es_formats;
	void (*DoRelease_p)(CMediaFormat *self);
};

#define MF_AVES		MKFCC('A','V','E','S')

//-----------------------------------------------------------------------
//
//  subtitle
//
//-----------------------------------------------------------------------

// Subtitle type

struct CSubtitleFormat: public CMediaFormat
{
};

#define MF_SubtitleFormat MKFCC('s','u','b','t')

#endif

