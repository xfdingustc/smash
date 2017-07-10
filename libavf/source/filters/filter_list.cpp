
#define LOG_TAG "filter_list"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_media_format.h"
#include "avio.h"
#include "avf_util.h"
#include "avf_config.h"

#include "filter_if.h"
#include "filter_list.h"

#ifndef REMUXER
#include "test_src.h"
#include "test_sink.h"
#include "null_sink.h"
#endif

//-----------------------------------------------------------------------
//
//  Filters
//
//-----------------------------------------------------------------------

#ifndef REMUXER

#ifndef VDBCOPY
#include "video_source.h"
#include "audio_source.h"
#include "subtitle.h"
#include "http_mjpeg.h"
#endif

#include "raw_muxer.h"
#include "wave_muxer.h"

#endif

#include "file_muxer.h"
#include "file_demuxer.h"

#include "av_muxer.h"
#include "media_writer.h"
#include "mm_server.h"

#ifdef AAC_ENCODER
#include "aac_encoder.h"
#endif

#ifndef REMUXER
#ifndef VDBCOPY
#include "audio_renderer.h"
#include "video_renderer.h"
#include "aac_decoder.h"
#endif
#endif

#ifdef SUPPORT_MP3
#include "mp3_reader.h"
#endif

#ifdef USE_FFMPEG
#include "mp3_decoder.h"
#endif

#include "mp4_reader.h"
#include "ts_reader.h"

struct CConfigNode;

typedef IFilter* (*avf_create_filter_proc)(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
typedef IMediaFileReader* (*avf_create_mfr_proc)(IEngine *pEngine, IAVIO *io);
typedef avf_int_t (*avf_recognize_io_proc)(IAVIO *io);
typedef avf_status_t (*recognize_format_proc)(CMediaFormat *pFormat);


typedef struct filter_create_struct_s {
	const char *pName;
	avf_create_filter_proc create_filter;
	avf_recognize_io_proc recognize_io;
	recognize_format_proc recognize_format;
} filter_create_struct_t;

static const filter_create_struct_t g_filter_create_struct[] =
{
#ifndef REMUXER
#ifndef VDBCOPY
	{"VideoSource", CVideoSource::Create, NULL, NULL},
	{"AudioSource", CAudioSource::Create, NULL, NULL},
	{"SubtitleSource", CSubtitleFilter::Create, NULL, NULL},
	{"HttpMjpegServer", CHttpMjpegFilter::Create, NULL, NULL},
#endif
	{"RawMuxer", CRawMuxer::Create, NULL, NULL},
	{"WaveMuxer", CWaveMuxer::Create, NULL, NULL},
#endif

#ifdef AAC_ENCODER
	{"AACEncoder", CAacEncoder::Create, NULL, NULL},
#endif

	{"FileMuxer", CFileMuxer::Create, NULL, NULL},
	{"FileDemuxer", CFileDemuxer::Create, NULL, NULL},

#if !defined(WIN32_OS) && !defined(MAC_OS)
	{"AVMuxer", CAVMuxer::Create, NULL, NULL},
	{"MediaWriter", CMediaWriter::Create, NULL, NULL},
	{"MMServer", CMMServer::Create, NULL, NULL},
#endif

#ifndef REMUXER
	{"TestSrc", CTestSrc::Create, NULL, NULL},
	{"TestSink", CTestSink::Create, NULL, NULL},
	{"NullSink", CNullSink::Create, NULL, NULL},

#ifndef VDBCOPY
	{"AudioRenderer", CAudioRenderer::Create, NULL, CAudioRenderer::RecognizeFormat},
	{"VideoRenderer", CVideoRenderer::Create, NULL, CVideoRenderer::RecognizeFormat},
	{"AACDecoder", CAacDecoder::Create, NULL, CAacDecoder::RecognizeFormat},
#endif
#endif

#ifdef USE_FFMPEG
	{"Mp3Decoder", CMp3Decoder::Create, NULL, CMp3Decoder::RecognizeFormat},
#endif
};

typedef struct mfr_create_struct_s {
	const char *pFormat;	// format
	const char *pFileExt;	// file extension
	const char *pDescription;
	avf_create_mfr_proc create_reader;	// AVIO -> Media File Reader
	avf_recognize_io_proc recognize_io;
} mfr_create_struct_t;

static const mfr_create_struct_t g_mfr_create_struct[] = {
	{"mp4", "*.mp4", "", CMP4Reader::Create, CMP4Reader::Recognize},
	{"ts", "*.ts", "", CTSReader::Create, CTSReader::Recognize},
#ifdef SUPPORT_MP3
	{"mp3", "*.mp3", "", CMp3Reader::Create, CMp3Reader::Recognize},
#endif
};

//-----------------------------------------------------------------------
//
//  Create
//
//-----------------------------------------------------------------------

static IFilter *avf_create_filter(IEngine *pEngine, const char *pFilterName, CConfigNode *pFilterConfig, int i)
{
	IFilter *pFilter = g_filter_create_struct[i].create_filter(pEngine, pFilterName, pFilterConfig);
	if (pFilterConfig && pFilter) {
		/*
		CConfigNode *pAttr = pFilterConfig->FindChild("index");
		if (pAttr) {
			pFilter->SetTypeIndex(::atoi(pAttr->GetValue()));
		}
		*/
	}
	return pFilter;
}

IFilter *avf_create_filter_by_name(const char *pFilterType, const char *pFilterName,
	IEngine *pEngine, CConfigNode *pFilterConfig)
{
	for (avf_size_t i = 0; i < ARRAY_SIZE(g_filter_create_struct); i++) {
		if (::strcmp(pFilterType, g_filter_create_struct[i].pName) == 0)
			return avf_create_filter(pEngine, pFilterName, pFilterConfig, i);
	}
	AVF_LOGE("unknown filter: '%s'", pFilterName);
	return NULL;
}

IFilter *avf_create_filter_by_io(IEngine *pEngine, IAVIO *io)
{
	avf_int_t value;
	for (avf_size_t i = 0; i < ARRAY_SIZE(g_filter_create_struct); i++) {
		avf_recognize_io_proc recognize_io = g_filter_create_struct[i].recognize_io;
		if (recognize_io) {
			io->Seek(0);
			value = recognize_io(io);
			if (value > 0) {
				io->Seek(0);
				return avf_create_filter(pEngine, g_filter_create_struct[i].pName, NULL, i);
			}
		}
	}
	AVF_LOGE("create filter by io failed");
	return NULL;
}

IFilter *avf_create_filter_by_media_format(CMediaFormat *pFormat, IEngine *pEngine)
{
	for (avf_size_t i = 0; i < ARRAY_SIZE(g_filter_create_struct); i++) {
		recognize_format_proc recognize_format = g_filter_create_struct[i].recognize_format;
		if (recognize_format) {
			if (recognize_format(pFormat) == E_OK) {
				return avf_create_filter(pEngine, g_filter_create_struct[i].pName, NULL, i);
			}
		}
	}
	AVF_LOGE("create filter by media format failed");
	return NULL;
}

IMediaFileReader *avf_create_media_file_reader_by_io(IEngine *pEngine, IAVIO *io)
{
	avf_int_t value;
	for (avf_size_t i = 0; i < ARRAY_SIZE(g_mfr_create_struct); i++) {
		avf_recognize_io_proc recognize_io = g_mfr_create_struct[i].recognize_io;
		if (recognize_io) {
			io->Seek(0);
			value = recognize_io(io);
			if (value > 0) {
				io->Seek(0);
				IMediaFileReader *reader = g_mfr_create_struct[i].create_reader(pEngine, io);
				return reader;
			}
		}
	}
	AVF_LOGE("create mfr by io failed");
	return NULL;
}

IMediaFileReader *avf_create_media_file_reader_by_format(IEngine *pEngine, 
	const char *format, IAVIO *io)
{
	for (avf_size_t i = 0; i < ARRAY_SIZE(g_mfr_create_struct); i++) {
		if (::strcmp(g_mfr_create_struct[i].pFormat, format) == 0) {
			IMediaFileReader *reader = g_mfr_create_struct[i].create_reader(pEngine, io);
			return reader;
		}
	}
	AVF_LOGE("create mfr by format failed");
	return NULL;
}

