
#define LOG_TAG "remuxer"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "filter_if.h"
#include "avf_config.h"
#include "mem_stream.h"

#include "avio.h"
#include "http_io.h"
#include "file_io.h"
#include "clip_reader_io.h"
#include "filter_list.h"

#include "base_player.h"
#include "file_playback.h"
#include "remuxer.h"
#include "avf_remuxer_if.h"

#define AAC_FOR_MP4

//-----------------------------------------------------------------------
//
//  CRemuxer
//
//-----------------------------------------------------------------------
CRemuxer* CRemuxer::Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig)
{
	CRemuxer *result = avf_new CRemuxer(pEngine, mediaSrc, conf, pConfig);
	CHECK_STATUS(result);
	return result;
}

CRemuxer::CRemuxer(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig):
	inherited(CMP_ID_REMUXER, pEngine, mediaSrc, conf, pConfig),

	mpSourceFilter(NULL),
	mpAudioSourceFilter(NULL),

	mpReadProgress(NULL),
	mpMediaSource(NULL),
	mpAudioMediaSource(NULL),

	mpSinkFilter(NULL),

	mSourceList(),
	mpSource(NULL)
{
}

CRemuxer::~CRemuxer()
{
}

//	source{
//		filter=FileDemuxer{format=%s;}
//		audio=FileDemuxer{format=%s;}
//	}
//	sink{
//		filter=FileMuxer{format=%s;filename=%s;}
//	}
avf_status_t CRemuxer::LoadFilter(const char *type, const char *name, IFilter **ppFilter, ap<string_t>& format)
{
	avf_status_t status;
	CConfigNode *node;
	CConfigNode *pAttr;
	IFilter *pFilter;

	if ((node = mpConfig->FindChild(type)) == NULL) {
		//AVF_LOGE("no such elem: %s", type);
		return E_ERROR;
	}

	if ((pAttr = node->FindChild(name)) == NULL) {
		//AVF_LOGE("no %s under %s", name, type);
		return E_ERROR;
	}

	if ((pFilter = avf_create_filter_by_name(pAttr->GetValue(), pAttr->GetValue(), mpEngine, pAttr)) == NULL) {
		AVF_LOGE("cannot create filter %s", pAttr->GetValue());
		return E_ERROR;
	}

	if ((status = pFilter->InitFilter()) != E_OK) {
		pFilter->Release();
		return status;
	}

	if ((status = mFilterGraph.AddFilter(pFilter)) != E_OK) {
		pFilter->Release();
		return status;
	}

	pAttr = pAttr->FindChild("format");
	if (pAttr) {
		format = pAttr->GetValue();
	}

	*ppFilter = pFilter;
	return E_OK;
}

avf_status_t CRemuxer::Connect()
{
	int num = mpSourceFilter->GetNumOutput();
	for (int i = 0; i < num; i++) {
		avf_status_t status = mFilterGraph.Connect(mpSourceFilter, i, mpSinkFilter, i);
		if (status != E_OK)
			return status;
	}
	return E_OK;
}

IFilter *CRemuxer::CreateFilterByName(const char *name, CConfigNode *config)
{
	IFilter *pFilter = CreateFilter(name, name, config);
	config->Clear();
	return pFilter;
}

avf_status_t CRemuxer::Connect2()
{
	avf_status_t status;

	status = mFilterGraph.Connect(mpSourceFilter, 0, mpSinkFilter, 0);
	if (status != E_OK)
		return status;

#ifdef AAC_FOR_MP4
	if (mAudioSourceFormat == "mp3" && mSinkFormat == "mp4") {
		AVF_LOGD("should convert mp3 to aac");

		CConfigNode config;
		config.Init();

		// load mp3 decoder filter
		IFilter *pMp3Decoder = CreateFilterByName("Mp3Decoder", &config);
		if (pMp3Decoder == NULL)
			return E_ERROR;

		// connect
		status = mFilterGraph.Connect(mpAudioSourceFilter, 0, pMp3Decoder, 0);
		if (status != E_OK)
			return status;

		// load aac encoder filter
		config.AddChild("afterburner", 0, "1");

		int duration = mpEngine->ReadRegInt32(NAME_REMUXER_DURATION, 0, VALUE_REMUXER_DURATION);
		char buffer[32];
		sprintf(buffer, "%d", duration);
		config.AddChild("duration", 0, buffer);

		int audio_fade_ms = mpEngine->ReadRegInt32(NAME_REMUXER_FADE_MS, 0, VALUE_REMUXER_FADE_MS);
		sprintf(buffer, "%d", audio_fade_ms);
		config.AddChild("fade-ms", 0, buffer);

		IFilter *pAacEncoder = CreateFilterByName("AACEncoder", &config);
		if (pAacEncoder == NULL)
			return E_ERROR;

		// connect
		status = mFilterGraph.Connect(pMp3Decoder, 0, pAacEncoder, 0);
		if (status != E_OK)
			return status;

		// connect to sink
		status = mFilterGraph.Connect(pAacEncoder, 0, mpSinkFilter, 1);
		if (status != E_OK)
			return status;

	} else {
#endif

		status = mFilterGraph.Connect(mpAudioSourceFilter, 0, mpSinkFilter, 1);
		if (status != E_OK)
			return status;

#ifdef AAC_FOR_MP4
	}
#endif

	return E_OK;
}

avf_status_t CRemuxer::OpenMedia(bool *pbDone)
{
	avf_status_t status;

	AVF_ASSERT(mpSourceFilter == NULL);
	AVF_ASSERT(mpAudioSourceFilter == NULL);
	AVF_ASSERT(mpSinkFilter == NULL);

	mSourceList = mMediaSrc;
	mpSource = mSourceList->string();
	AVF_LOGD("source: %s", mpSource);

	///
	//if ((status = inherited::OpenMedia(pbDone)) != E_OK)
	//	return status;

	// source filter
	if ((status = LoadFilter("source", "filter", &mpSourceFilter, mSourceFormat)) != E_OK)
		return status;

	if ((mpMediaSource = IMediaSource::GetInterfaceFrom(mpSourceFilter)) == NULL) {
		AVF_LOGE("source: no interface");
		return E_ERROR;
	}

	// audio source
	LoadFilter("source", "audio", &mpAudioSourceFilter, mAudioSourceFormat);
	if (mpAudioSourceFilter) {
		mpAudioMediaSource = IMediaSource::GetInterfaceFrom(mpAudioSourceFilter);
		if (mpAudioMediaSource == NULL) {
			AVF_LOGE("audio source: no interface");
			return E_ERROR;
		}
	}

	// sink filter
	if ((status = LoadFilter("sink", "filter", &mpSinkFilter, mSinkFormat)) != E_OK)
		return status;

	AVF_LOGD(C_YELLOW "source: %s, audio: %s, sink: %s" C_NONE,
		mSourceFormat->string(), mAudioSourceFormat->string(), mSinkFormat->string());

	return E_OK;
}

avf_status_t CRemuxer::GetInput(const char **ppStart, char buffer[], int buffer_size, int ch)
{
	const char *pStart = *ppStart;
	const char *pEnd = ::strchr(pStart, ch);
	int size;

	if (pEnd == NULL)
		return E_ERROR;

	size = pEnd - pStart;
	if (size >= buffer_size) {
		AVF_LOGE("name too long");
		return E_ERROR;
	}

	::memcpy(buffer, pStart, size);
	buffer[size] = 0;

	*ppStart = pEnd + 1;
	return E_OK;
}

// filename,start_time,length;
// filename,start_time,length;

avf_status_t CRemuxer::GetNextFile(char filename[], int filename_size, int *start_time, int *length)
{
	const char *pStart = mpSource;
	char buffer[32];

	// filename
	if (GetInput(&pStart, filename, filename_size, ',') != E_OK)
		return S_END;

	// start time
	if (GetInput(&pStart, buffer, ARRAY_SIZE(buffer), ',') != E_OK) {
		AVF_LOGE("',' expected");
		goto InputError;
	}

	*start_time = ::atoi(buffer);
	if (*start_time < 0) {
		AVF_LOGE("bad start_time: %d", *start_time);
		goto InputError;
	}

	// length
	if (GetInput(&pStart, buffer, ARRAY_SIZE(buffer), ';') != E_OK) {
		AVF_LOGE("';' expected");
		goto InputError;
	}

	*length = ::atoi(buffer);

	mpSource = pStart;
	return E_OK;

InputError:
	return E_ERROR;
}

IAVIO *CRemuxer::OpenFile(const char *filename)
{
	IAVIO *io;

#if !defined(WIN32_OS) && !defined(MAC_OS)
	if (::strncasecmp(filename, "http://", 7) == 0) {
		io = CHttpIO::Create();
	} else
#endif

#ifndef ANDROID_OS
	if (::strncasecmp(filename, VDB_CLIPS_PROT, sizeof(VDB_CLIPS_PROT)-1) == 0) {
		IVdbInfo *info = (IVdbInfo*)mpEngine->FindComponent(IID_IVdbInfo);
		if (info == NULL) {
			AVF_LOGE("no IVdbInfo component");
			return NULL;
		}
		io = CClipReaderIO::Create(info);
	} else
#endif

	{
		io = CFileIO::Create();
	}

	if (io) {
		avf_status_t status = io->OpenRead(filename);
		if (status != E_OK) {
			AVF_LOGE("cannot open %s", filename);
			io->Release();
			io = NULL;
		}
	}

	return io;
}

IMediaFileReader *CRemuxer::CreateFileReader(const char *filename, const ap<string_t>& format)
{
	IMediaFileReader *reader;
	IAVIO *io;

	if ((io = OpenFile(filename)) == NULL)
		return NULL;

	if (format->size() > 0) {
		reader = avf_create_media_file_reader_by_format(mpEngine, format->string(), io);
		if (reader == NULL) {
			AVF_LOGE("unknown input format: %s", format->string());
		}
	} else {
		reader = avf_create_media_file_reader_by_io(mpEngine, io);
		if (reader == NULL) {
			AVF_LOGE("unknown file format: %s", filename);
		}
	}

	io->Release();
	return reader;
}

// file1,start,end;file2,start,end;
// S_END: no more input
avf_status_t CRemuxer::MuxNextFile(bool bFirst)
{
	char filename[512];
	int start_time_ms;
	int length_ms;

	IMediaFileReader *reader = NULL;
	IMediaFileReader *a_reader = NULL;

	avf_status_t status;

	status = GetNextFile(with_size(filename), &start_time_ms, &length_ms);
	if (status != E_OK)
		return status;

	// open source
	if ((reader = CreateFileReader(filename, mSourceFormat)) == NULL) {
		status = E_UNKNOWN;
		goto Done;
	}

	reader->SetReportProgress(true);

	// audio
	if (mpAudioMediaSource) {
		char a_filename[REG_STR_MAX];
		mpEngine->ReadRegString(NAME_EXTRA_AUDIO, 0, a_filename, "");

		if ((a_reader = CreateFileReader(a_filename, mAudioSourceFormat)) == NULL) {
			status = E_UNKNOWN;
			goto Done;
		}
	}

	if ((status = mpMediaSource->OpenMediaSource(reader)) != E_OK) {
		AVF_LOGE("OpenMediaSource failed");
		goto Done;
	}

	if (mpAudioMediaSource) {
		if ((status = mpAudioMediaSource->OpenMediaSource(a_reader)) != E_OK) {
			AVF_LOGE("OpenMediaSource(audio) failed");
			goto Done;
		}
	}

	if ((status = mpMediaSource->SetRange(start_time_ms, length_ms, mpSource[0] == 0)) != E_OK)
		goto Done;

	if (bFirst) {
		if (mpAudioSourceFilter == NULL) {
			// video only
			if ((status = Connect()) != E_OK)
				goto Done;
		} else {
			// video + audio
			if ((status = Connect2()) != E_OK)
				goto Done;
		}
	} else {
		if ((status = mpSourceFilter->RunFilter()) != E_OK)
			goto Done;
	}

	// done

	if (status == E_OK) {
		// for report detailed progress
		mpReadProgress = IReadProgress::GetInterfaceFrom(mpSourceFilter);
	} else {
		mpReadProgress = NULL;
	}

Done:
	avf_safe_release(reader);
	avf_safe_release(a_reader);
	return status;
}

avf_status_t CRemuxer::RunMedia(bool *pbDone)
{
	avf_status_t status;

	mFilterGraph.ClearAllFiltersFlag(CFilterGraph::FF_EOS | CFilterGraph::FF_PREPARED);

	if ((status = MuxNextFile(true)) != E_OK) {
		if (status == S_END) {
			PostPlayerMsg(IEngine::EV_STOP_MEDIA_DONE, 0, 0);
			AVF_LOGD("no file to mux");
			return E_OK;
		} else {
			// error
			AVF_LOGD("RunMedia failed");
			return status;
		}
	}

	AVF_LOGD("RunAllFilters");
	if ((status = mFilterGraph.RunAllFilters()) != E_OK)
		return status;

	return E_OK;
}

void CRemuxer::ProcessFilterMsg(const avf_msg_t& msg)
{
	switch (msg.code) {
	case IEngine::MSG_EOS: {
			IFilter *pFilter = (IFilter*)msg.p0;
			avf_status_t status = S_END;

			AVF_LOGD("[%d] %s EOS", pFilter->GetFilterIndex() + 1, pFilter->GetFilterName());

			if (pFilter == mpSourceFilter) {
				status = MuxNextFile(false);
			}

			switch (status) {
			case E_OK:
				// do nothing
				break;

			case S_END:
				// ended
				mFilterGraph.SetFilterFlag(pFilter, CFilterGraph::FF_EOS);
				if (mFilterGraph.TestAllFiltersFlag(CFilterGraph::FF_EOS)) {
					mFilterGraph.StopAllFilters();
					PostPlayerMsg(IEngine::EV_STOP_MEDIA_DONE, 0, 0);
					AVF_LOGI("remuxer done");
				}
				break;

			default:
				// error
				PostPlayerMsg(IEngine::EV_PLAYER_ERROR, 0, 0);
				break;
			}
		}
		return;

	case IEngine::MSG_EARLY_EOS:
		if (mpAudioSourceFilter) {
			mpAudioSourceFilter->ForceEOS();
		}
		return;

	default:
		break;
	}

	inherited::ProcessFilterMsg(msg);
}

avf_status_t CRemuxer::Command(avf_enum_t cmdId, avf_intptr_t arg)
{
	if (cmdId == Remuxer::CMD_GetRemuxProgress) {
		if (mpReadProgress == NULL) {
			AVF_LOGW("no IReadProgress found");
			return E_UNIMP;
		}
		return mpReadProgress->GetReadProgress((IReadProgress::progress_t*)arg);
	}
	return E_UNKNOWN;
}

