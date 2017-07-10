
#define LOG_TAG "file_demuxer"

#include <limits.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avio.h"
#include "avf_media_format.h"
#include "avf_config.h"

#include "filter_if.h"
#include "file_demuxer.h"

//-----------------------------------------------------------------------
//
//  CFileDemuxer
//
//-----------------------------------------------------------------------
IFilter* CFileDemuxer::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CFileDemuxer *result = avf_new CFileDemuxer(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CFileDemuxer::CFileDemuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mNumOutput(0),
	mpVideoOutput(NULL),
	mpAudioOutput(NULL),
	mStartTimeMs(0),
	mLengthMs(-1),
	mbLastFile(true),
	mFastPlayMode(0),
	mFastPlaySpeed(0),
	mbIframeOnly(false)
{
	SET_ARRAY_NULL(mpOutput);
	if (pFilterConfig) {
		CConfigNode *node = pFilterConfig->FindChild("iframeonly", 0);
		mbIframeOnly = node ? ::atoi(node->GetValue()) : 0;
	}
}

CFileDemuxer::~CFileDemuxer()
{
	SAFE_RELEASE_ARRAY(mpOutput);
}

void* CFileDemuxer::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMediaSource)
		return static_cast<IMediaSource*>(this);
	if (refiid == IID_IReadProgress && mReader.get() != NULL) {
		void *result = mReader->GetInterface(refiid);
		if (result != NULL)
			return result;
	}
	return inherited::GetInterface(refiid);
}

IPin* CFileDemuxer::GetOutputPin(avf_uint_t index)
{
	return index >= mNumOutput ? NULL : mpOutput[index];
}

void CFileDemuxer::FilterLoop()
{
	avf_status_t status;

	if ((status = SeekToCurrent()) != E_OK) {
		ReplyCmd(E_ERROR);
		return;
	}

	ReplyCmd(E_OK);

	if ((status = EnableAllTracks()) != E_OK) {
		PostErrorMsg(status);
		return;
	}

	if ((status = StartAllPins()) != E_OK) {
		PostErrorMsg(status);
		return;
	}

	if (mFastPlaySpeed == 0) {
		AVF_LOGD("normal playback");
		NormalPlayLoop();
	} else {
		AVF_LOGD("fast playback");
		FastPlayLoop(mFastPlaySpeed);
	}
}

bool CFileDemuxer::ReadInputSample(bool bStream, COutputPin *&pPin, CBuffer *&pBuffer, bool &bEOS)
{
	avf_status_t status;

	if (!bStream) {

		// can demux each stream independently
		if (!AllocAnyOutput(pPin, pBuffer))
			return false;

	} else {

		// read any sample that's ready
		avf_size_t index = 1024;
		if ((status = mReader->FetchSample(&index)) != E_OK) {
			PostErrorMsg(status);
			return false;
		}

		// get the pin
		if (index >= MAX_OUTPUT || (pPin = mpOutput[index]) == NULL) {
			AVF_LOGE("bad stream index %d", index);
			PostErrorMsg(E_ERROR);
			return false;
		}

		// alloc buffer
		if (!AllocOutputBuffer(pPin, pBuffer)) {
			//AVF_LOGD("AllocOutputBuffer false");
			return false;
		}
	}

	// read the sample
	CFileDemuxerOutput *pOutput = (CFileDemuxerOutput*)pPin;
	if ((status = pOutput->ReadSample(pBuffer, bEOS)) != E_OK) {
		AVF_LOGE("ReadSample() failed, status=%d", status);
		pBuffer->Release();
		PostErrorMsg(E_IO);
		return false;
	}

	return true;
}

bool CFileDemuxer::FastModeSendEOS()
{
	CBuffer *pBuffer;

	for (avf_size_t i = 0; i < mNumOutput; i++) {
		CFileDemuxerOutput *pOutput = mpOutput[i];
		if (pOutput->IsVideo()) {
			if (!AllocOutputBuffer(pOutput, pBuffer))
				return false;

			SetEOS(pOutput, pBuffer);
			pOutput->PostBuffer(pBuffer);
		}
	}

	PostEosMsg();

	return true;
}

void CFileDemuxer::SendAllEOS()
{
	AVF_LOGD("SendAllEOS()");
	for (avf_size_t i = 0; i < MAX_OUTPUT; i++) {
		CFileDemuxerOutput *pPin = mpOutput[i];
		if (pPin && pPin->Connected()) {
			if (!GenerateEOS(pPin, false))
				return;
		}
	}
	PostEosMsg();
}

void CFileDemuxer::NormalPlayLoop()
{
	bool bStream = mReader->IsStreamFormat();
	avf_int_t num_eos = mNumOutput;

	while (true) {
		COutputPin *pPin;
		CBuffer *pBuffer;
		bool bEOS = false;

		if (!ReadInputSample(bStream, pPin, pBuffer, bEOS))
			break;

		if (!bEOS) {
			bool is_idr = (pBuffer->mFlags & CBuffer::F_AVC_IDR) != 0;
			pPin->PostBuffer(pBuffer);

			if (is_idr && mbIframeOnly) {
				AVF_LOGD("iframeonly, SendAllEOS");
				SendAllEOS();
				break;
			}
		} else {
			if (mbLastFile) {
				SetEOS(pPin, pBuffer);
				pPin->PostBuffer(pBuffer);
			} else {
				pBuffer->Release();
			}

			AVF_LOGD("%s[%d] EOS", GetFilterName(), pPin->GetPinIndex());

			if (--num_eos == 0) {
				PostEosMsg();
				break;
			}
		}
	}
}

void CFileDemuxer::FastPlayLoop(int speed)
{
	bool bStream = mReader->IsStreamFormat();
	avf_u32_t time_ms = mStartTimeMs;
	avf_status_t status;

	while (true) {
		COutputPin *pPin;
		CBuffer *pBuffer;
		bool bEOS;

		if ((status = mReader->SeekToTime(time_ms, &time_ms)) != E_OK) {
			FastModeSendEOS();
			break;
		}

		if (!ReadInputSample(bStream, pPin, pBuffer, bEOS))
			break;

		if (bEOS) {
			pBuffer->SetEOS();
		} else {
			avf_pts_t start_pts = pPin->ms_to_pts(mStartTimeMs);
			if (speed > 0) {
				if (pBuffer->m_pts <= start_pts) {
					pBuffer->m_pts = start_pts;
				} else {
					pBuffer->m_pts = start_pts + (pBuffer->m_pts - start_pts) / speed;
				}
			} else {
				if (pBuffer->m_pts >= start_pts) {
					pBuffer->m_pts = start_pts;
				} else {
					pBuffer->m_pts = start_pts + (start_pts - pBuffer->m_pts) / (-speed);
				}
			}
		}

		pPin->PostBuffer(pBuffer);

		if (bEOS) {
			PostEosMsg();
			//AttachOutputPin(pPin, false);
			break;
		}

		if (speed > 0) {
			time_ms += 1002;
		} else {
			if (time_ms < 1000) {
				FastModeSendEOS();
				break;
			} else {
				time_ms -= 1000;
			}
		}
	}
}

IPin *CFileDemuxer::GetOutputMaster()
{
	return mpAudioOutput ? mpAudioOutput : mpVideoOutput;
}

avf_status_t CFileDemuxer::SeekToCurrent()
{
	avf_status_t status;

	if (mReader->IsStreamFormat()) {
		return mReader->SeekToTime(mStartTimeMs, &mStartTimeMs);
	}

	for (avf_size_t i = 0; i < mNumOutput; i++) {
		if ((status = mpOutput[i]->SeekToTime(mStartTimeMs)) != E_OK)
			return status;
	}

	return E_OK;
}

avf_status_t CFileDemuxer::EnableAllTracks()
{
	avf_status_t status;

	for (avf_size_t i = 0; i < mNumOutput; i++) {
		bool bEnable = (mFastPlaySpeed == 0) || (mpOutput[i]->IsVideo());
		if ((status = mpOutput[i]->EnableTrack(bEnable)) != E_OK)
			return status;
	}

	return E_OK;
}

avf_status_t CFileDemuxer::StartAllPins()
{
	avf_status_t status;

	for (avf_size_t i = 0; i < mNumOutput; i++) {
		if ((status = StartPin(mpOutput[i])) != E_OK)
			return status;
	}

	return E_OK;
}

avf_status_t CFileDemuxer::StartPin(CFileDemuxerOutput *pPin)
{
	CBuffer *pBuffer;

	if (!AllocOutputBuffer(pPin, pBuffer, 0))
		return E_ERROR;

	pBuffer->SetStartDts();
	pBuffer->m_dts = pPin->ms_to_pts(mStartTimeMs);
	pPin->PostBuffer(pBuffer);

	return E_OK;
}

// may be called many times
avf_status_t CFileDemuxer::OpenMediaSource(IMediaFileReader *pReader)
{
	avf_status_t status;

	if ((status = pReader->OpenMediaSource()) != E_OK) {
		return status;
	}

	avf_media_info_t info;
	if ((status = pReader->GetMediaInfo(&info)) != E_OK) {
		return status;
	}

	if (info.num_videos == 0 && info.num_audios == 0) {
		AVF_LOGE("no video nor audio");
		return E_ERROR;
	}

	avf_size_t nTracks = pReader->GetNumTracks();
	if (nTracks != mNumOutput && mNumOutput > 0) {
		AVF_LOGE("track not match: %d %d", nTracks, mNumOutput);
		return E_ERROR;
	}

	for (avf_size_t i = 0; i < nTracks; i++) {
		IMediaTrack *track = pReader->GetTrack(i);
		CMediaFormat *format = track->QueryMediaFormat();

		if (format == NULL) {
			AVF_LOGE("no media format %d (total: %d)", i, nTracks);
			return E_ERROR;
		}

		if (format->IsVideo()) {
			if (mpVideoOutput) {
				// todo - check media format
				track->SetDtsOffset(mpVideoOutput->GetTrack()->GetDtsOffset());
				mpVideoOutput->SetTrack(track);
			} else {
				mpVideoOutput = CFileDemuxerOutput::Create(this, i, track);
				if (mpVideoOutput != NULL) {
					mpOutput[mNumOutput++] = mpVideoOutput;
				}
			}
		}

		if (format->IsAudio()) {
			if (mpAudioOutput) {
				// todo - check media format
				track->SetDtsOffset(mpAudioOutput->GetTrack()->GetDtsOffset());
				mpAudioOutput->SetTrack(track);
			} else {
				mpAudioOutput = CFileDemuxerOutput::Create(this, i, track);
				if (mpAudioOutput != NULL) {
					mpOutput[mNumOutput++] = mpAudioOutput;
				}
			}
		}

		format->Release();
	}

	mReader = pReader;
	mStartTimeMs = 0;

	return E_OK;
}

avf_status_t CFileDemuxer::GetMediaInfo(avf_media_info_t *info)
{
	return mReader.get() ? mReader->GetMediaInfo(info) : E_ERROR;
}

avf_status_t CFileDemuxer::GetVideoInfo(avf_uint_t index, avf_video_info_t *info)
{
	return mReader.get() ? mReader->GetVideoInfo(index, info) : E_ERROR;
}

avf_status_t CFileDemuxer::GetAudioInfo(avf_uint_t index, avf_audio_info_t *info)
{
	return mReader.get() ? mReader->GetAudioInfo(index, info) : E_ERROR;
}

avf_status_t CFileDemuxer::GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info)
{
	return mReader.get() ? mReader->GetSubtitleInfo(index, info) : E_ERROR;
}

avf_status_t CFileDemuxer::SetRange(avf_u32_t start_time_ms, avf_s32_t length_ms, bool bLastFile)
{
	mStartTimeMs = start_time_ms;
	mLengthMs = length_ms;
	mbLastFile = bLastFile;
	// todo
	return E_OK;
}

avf_status_t CFileDemuxer::SeekToTime(avf_u32_t ms)
{
	mStartTimeMs = ms;
	return E_OK;
}

bool CFileDemuxer::CanFastPlay()
{
	return mReader->CanFastPlay();
}

avf_status_t CFileDemuxer::SetFastPlay(int mode, int speed)
{
	mFastPlayMode = mode;
	mFastPlaySpeed = speed;
	AVF_LOGD("SetFastPlay speed %d", speed);
	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CFileDemuxerOutput
//
//-----------------------------------------------------------------------

CFileDemuxerOutput* CFileDemuxerOutput::Create(CFileDemuxer *pFilter, int index, IMediaTrack *track)
{
	CFileDemuxerOutput *result = avf_new CFileDemuxerOutput(pFilter, index, track);
	CHECK_STATUS(result);
	return result;
}

CFileDemuxerOutput::CFileDemuxerOutput(CFileDemuxer *pFilter, int index, IMediaTrack *track):
	inherited(pFilter, index, "FileDemuxerOutput"),
	mpTrack(track)
{
}

CFileDemuxerOutput::~CFileDemuxerOutput()
{
}

IBufferPool *CFileDemuxerOutput::QueryBufferPool()
{
	IBufferPool *pBufferPool = CDynamicBufferPool::CreateBlock("FileDemuxerBP", NULL, 32);
	return pBufferPool;
}

