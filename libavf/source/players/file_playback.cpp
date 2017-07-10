
#define LOG_TAG "file_playback"

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
#include "file_io.h"

#include "filter_list.h"
#include "file_demuxer.h"

#include "base_player.h"
#include "file_playback.h"

//-----------------------------------------------------------------------
//
//  CFilePlayback
//
//-----------------------------------------------------------------------
CFilePlayback* CFilePlayback::Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig)
{
	CFilePlayback *result = avf_new CFilePlayback(pEngine, mediaSrc, conf, pConfig);
	CHECK_STATUS(result);
	return result;
}

CFilePlayback::CFilePlayback(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig):
	inherited(CMP_ID_PLAYBACK, pEngine, mediaSrc, conf, pConfig),
	mpMediaSource(NULL),
	mbPaused(false),
	mbStarting(false),
	mStartTime(0)
{
}

CFilePlayback::~CFilePlayback()
{
}

avf_status_t CFilePlayback::OpenMediaSource(IFilter *pFilter, IMediaFileReader *reader, IAVIO *io)
{
	avf_status_t status;

	// init filter
	if ((status = pFilter->InitFilter()) != E_OK) {
		AVF_LOGD("InitFilter failed");
		return status;
	}

	// get media source
	mpMediaSource = IMediaSource::GetInterfaceFrom(pFilter);
	if (mpMediaSource == NULL) {
		AVF_LOGE("IMediaSource not found with %s", pFilter->GetFilterName());
		return E_ERROR;
	}

	mpMediaSource->AddRef();
	AVF_LOGI("found source filter: %s", pFilter->GetFilterName());

	// open media source
	if (reader != NULL) {
		status = mpMediaSource->OpenMediaSource(reader);
	} else if (io != NULL) {
		status = mpMediaSource->OpenMediaSource(io);
	} else {
		status = E_UNKNOWN;
	}

	if (status != E_OK) {
		AVF_LOGE("OpenMediaSource failed");
		return status;
	}

	mpMediaSource->GetMediaInfo(&mMediaInfo);
	AVF_LOGI("length ms: %d", mMediaInfo.length_ms);

	if ((status = mFilterGraph.AddFilter(pFilter)) != E_OK) {
		return status;
	}

	return E_OK;
}

// open the file and recognize it
avf_status_t CFilePlayback::OpenMedia(bool *pbDone)
{
	avf_status_t status;
	IAVIO *io = NULL;
	IMediaFileReader *reader = NULL;
	IFilter *pFilter = NULL;

	AVF_ASSERT(mpMediaSource == NULL);

	// open file
	if ((io = CFileIO::Create()) == NULL)
		return E_NOMEM;

	if ((status = io->OpenRead(mMediaSrc->string())) != E_OK)
		goto Done;

	// create source filter
	if ((reader = avf_create_media_file_reader_by_io(mpEngine, io)) != NULL) {
		if ((pFilter = CFileDemuxer::Create(mpEngine, "FileDemuxer", NULL)) == NULL) {
			AVF_LOGE("Cannot create FileDemuxer");
			status = E_UNKNOWN;
			goto Done;
		}
	} else {
		if ((pFilter = avf_create_filter_by_io(mpEngine, io)) == NULL) {
			AVF_LOGE("File format is unknown: %s", mMediaSrc->string());
			status = E_UNKNOWN;
			goto Done;
		}
	}

	status = OpenMediaSource(pFilter, reader, io);
	pFilter->Release();

	if (status != E_OK) {
		avf_safe_release(mpMediaSource);
	}

Done:
	avf_safe_release(reader);
	avf_safe_release(io);
	return status;
}

// prepare the source filter and create filter graph
avf_status_t CFilePlayback::PrepareMedia(bool *pbDone)
{
	return RenderAllFilters(false);
}

avf_status_t CFilePlayback::RunMedia(bool *pbDone)
{
	avf_status_t status = E_OK;
	int speed = 0;

	mFilterGraph.ClearAllFiltersFlag(CFilterGraph::FF_EOS | CFilterGraph::FF_PREPARED);
	mbStarting = false;

	if (mpMediaSource->CanFastPlay()) {
		speed = GetPlaySpeed();
		//
		if (speed > 16) speed = 16;
		else if (speed < -16) speed = -16;
		//
		mpMediaSource->SetFastPlay(0, speed);
	}

	avf_size_t total = mFilterGraph.GetNumFilters();
	for (avf_size_t i = 0; i < total; i++) {
		IFilter *pFilter = mFilterGraph.__GetFilter(i);
		IMediaRenderer *pMediaRenderer = IMediaRenderer::GetInterfaceFrom(pFilter);

		if (pMediaRenderer) {
			status = pMediaRenderer->InitRenderer(mStartTime);
			if (status != E_OK)
				goto Error;
			mbStarting = true;
		} else {
			mFilterGraph.__SetFilterFlag(i, CFilterGraph::FF_PREPARED);
		}

		if ((status = mFilterGraph.RunFilter(pFilter)) != E_OK)
			goto Error;
	}

	if (speed) {
		mpEngine->WriteRegInt32(NAME_PB_AUDIO_POS, 0, -1);
	}

	*pbDone = true;
	return E_OK;

Error:
	ErrorStop();
	return status;
}

void CFilePlayback::ResumeAllMediaRenderers()
{
	avf_size_t total = mFilterGraph.GetNumFilters();
	for (avf_size_t i = 0; i < total; i++) {
		IFilter *pFilter = mFilterGraph.__GetFilter(i);
		IMediaRenderer *pMediaRenderer = IMediaRenderer::GetInterfaceFrom(pFilter);
		if (pMediaRenderer) {
			AVF_LOGD("Resume %s", pFilter->GetFilterName());
			avf_status_t status = pMediaRenderer->ResumeRenderer(); 
			if (status != E_OK) {
				ErrorStop();
				return;
			}
		}
	}
}

void CFilePlayback::ProcessFilterMsg(const avf_msg_t& msg)
{
	switch (msg.code) {
	case IEngine::MSG_EOS: {
			IFilter *pFilter = (IFilter*)msg.p0;
			AVF_LOGD("Filter %s EOS", pFilter->GetFilterName());
			mFilterGraph.SetFilterFlag(pFilter, CFilterGraph::FF_EOS);
			if (mFilterGraph.TestAllFiltersFlag(CFilterGraph::FF_EOS)) {
				mFilterGraph.StopAllFilters();
				PostPlayerMsg(IEngine::EV_STOP_MEDIA_DONE, 0, 0);
				AVF_LOGI("Playback done");
			}
		}
		return;

	case IEngine::MSG_PREPARED: {
			IFilter *pFilter = (IFilter*)msg.p0;
			AVF_LOGD("READY_TO_RUN from %s", pFilter->GetFilterName());

			if (!mbStarting) {
				AVF_LOGE("Not in starting state!");
				return;
			}

			mFilterGraph.SetFilterFlag(pFilter, CFilterGraph::FF_PREPARED);

			if (mFilterGraph.TestAllFiltersFlag(CFilterGraph::FF_PREPARED) || GetPlaySpeed()) {
				AVF_LOGD("All READY_TO_RUN received");
				mbStarting = false;
				if (!mbPaused) {
					ResumeAllMediaRenderers();
				}
			}
		}
		return;

	default:
		break;
	}

	inherited::ProcessFilterMsg(msg);
}

avf_status_t CFilePlayback::Command(avf_enum_t cmdId, avf_intptr_t arg)
{
	switch (cmdId) {
	case Playback::CMD_GetMediaInfo:
		return GetMediaInfo((Playback::PlaybackMediaInfo_t*)arg);

	case Playback::CMD_GetPlayPos:
		*(avf_u32_t*)arg = GetPlayPos();
		return E_OK;

	case Playback::CMD_PauseResume:
		return PauseResume();

	case Playback::CMD_Seek:
		return Seek((avf_u32_t)arg);

	case Playback::CMD_FastPlay:
		return FastPlay((Playback::FastPlayParam_t*)arg);

	default:
		AVF_LOGE("Unknown playback cmd: %d", cmdId);
		return E_INVAL;
	}
}

avf_status_t CFilePlayback::GetMediaInfo(Playback::PlaybackMediaInfo_t *pMediaInfo)
{
	if (mpMediaSource == NULL) {
		return E_ERROR;
	}

	pMediaInfo->total_length_ms = mMediaInfo.length_ms;
	pMediaInfo->audio_num = mMediaInfo.num_videos;
	pMediaInfo->video_num = mMediaInfo.num_audios;
	pMediaInfo->bPaused = mbPaused;

	return E_OK;
}

avf_u32_t CFilePlayback::GetPlayPosWithSpeed(int speed)
{
	AVF_LOGD("audio: %d", mpEngine->ReadRegInt32(NAME_PB_AUDIO_POS, 0, mStartTime));
	AVF_LOGD("video: %d", mpEngine->ReadRegInt32(NAME_PB_VIDEO_POS, 0, mStartTime));
	if (speed == 0) {
		return mpEngine->ReadRegInt32(NAME_PB_VIDEO_POS, 0, mStartTime);
	} else {
		int curr_ms = mpEngine->ReadRegInt32(NAME_PB_VIDEO_POS, 0, mStartTime);
		if (speed > 0) {
			return curr_ms <= (int)mStartTime ? mStartTime : mStartTime + (curr_ms - mStartTime) * speed;
		} else {
			int delta = (curr_ms - mStartTime) * (-speed);
			return (int)mStartTime <= delta ? 0 : mStartTime - delta;
		}
	}
}

avf_status_t CFilePlayback::PauseResume()
{
	if (mbStarting) {
		// Pause/Resume command issued when the renderers are not ready
		// renderers might be ready, but we have not been notified yet
		AVF_LOGD("PauseResume: playback not started");
	}

	mbPaused = !mbPaused;

	if (GetState() != IMediaControl::STATE_RUNNING) {
		AVF_LOGI("PauseResume: not running");
		return E_OK;
	}

	avf_size_t total = mFilterGraph.GetNumFilters();
	for (avf_size_t i = 0; i < total; i++) {
		IFilter *pFilter = mFilterGraph.__GetFilter(i);
		IMediaRenderer *pMediaRenderer = IMediaRenderer::GetInterfaceFrom(pFilter);
		if (pMediaRenderer) {
			if (mbPaused) {
				pMediaRenderer->PauseRenderer();
			} else {
				pMediaRenderer->ResumeRenderer();
			}
		}
	}
	AVF_LOGI("Playback %s", mbPaused ? "paused" : "resumed");

	return E_OK;
}

avf_status_t CFilePlayback::PerformSeek(avf_u32_t ms)
{
	return mpMediaSource->SeekToTime(ms);
}

avf_status_t CFilePlayback::Seek(avf_u32_t ms)
{
	avf_status_t status = E_OK;
	bool bDone;

	mStartTime = ms;

	if (GetState() != IMediaControl::STATE_RUNNING) {
		AVF_LOGE("Seek: not running");
		return E_OK;
	}

	if (mpMediaSource == NULL) {
		AVF_LOGE("Seek: no media source");
		return E_ERROR;
	}

	if (ms >= mMediaInfo.length_ms) {
		AVF_LOGE("Cannot seek to %d", ms);
		return E_INVAL;
	}

	// set seeking flag
	StartSeeking();

	if ((status = StopMedia(&bDone)) != E_OK)
		goto Error;

	// discard pending msgs
	mpEngine->ClearFilterMsgs();

	mFilterGraph.PurgeAllFilters();

	if ((status = PerformSeek(ms)) != E_OK)
		goto Error;

	if ((status = RunMedia(&bDone)) != E_OK)
		goto Error;

	PostPlayerMsg(IEngine::EV_CHANGE_STATE,
		bDone ? IMediaControl::STATE_RUNNING : IMediaControl::STATE_STARTING,
		IMediaControl::STATE_RUNNING);

	EndSeeking();
	return E_OK;

Error:
	PostPlayerMsg(IEngine::EV_CHANGE_STATE,
		IMediaControl::STATE_ERROR, IMediaControl::STATE_ERROR);
	EndSeeking();
	StopMedia(&bDone);
	return status;
}

avf_status_t CFilePlayback::FastPlay(Playback::FastPlayParam_t *param)
{
	avf_status_t status = E_OK;
	avf_u32_t ms;
	int speed;
	bool bDone;

	speed = GetPlaySpeed();
	mpEngine->WriteRegInt32(NAME_PB_FASTPLAY, 0, param->speed);

	if (GetState() != IMediaControl::STATE_RUNNING) {
		AVF_LOGD("FastPlay: not running");
		return E_OK;
	}

	if (mpMediaSource == NULL) {
		AVF_LOGD("FastPlay: no media source");
		return E_ERROR;
	}

	if (!mpMediaSource->CanFastPlay()) {
		AVF_LOGI("Cannot do fastplay");
		return E_ERROR;
	}

	// set seeking flag
	StartSeeking();

	if ((status = StopMedia(&bDone)) != E_OK)
		goto Error;

	ms = GetPlayPosWithSpeed(speed);
	AVF_LOGD("FastPlay(%d) from %d", param->speed, ms);

	// discard pending msgs
	mpEngine->ClearFilterMsgs();

	mFilterGraph.PurgeAllFilters();

	if ((status = PerformSeek(ms)) != E_OK)
		goto Error;

	mStartTime = ms;

	if ((status = RunMedia(&bDone)) != E_OK)
		goto Error;

	PostPlayerMsg(IEngine::EV_CHANGE_STATE,
		bDone ? IMediaControl::STATE_RUNNING : IMediaControl::STATE_STARTING,
		IMediaControl::STATE_RUNNING);

	EndSeeking();
	return E_OK;

Error:
	PostPlayerMsg(IEngine::EV_CHANGE_STATE,
		IMediaControl::STATE_ERROR, IMediaControl::STATE_ERROR);
	EndSeeking();
	StopMedia(&bDone);
	return status;

	return E_OK;
}

