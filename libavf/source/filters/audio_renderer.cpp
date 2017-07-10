
#define LOG_TAG "audio_renderer"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_media_format.h"
#include "filter_if.h"

#include "audio_renderer.h"

//-----------------------------------------------------------------------
//
//  CAudioRenderer
//
//-----------------------------------------------------------------------
avf_status_t CAudioRenderer::RecognizeFormat(CMediaFormat *pFormat)
{
	if (pFormat->mMediaType != MEDIA_TYPE_AUDIO)
		return E_ERROR;
	if (pFormat->mFormatType != MF_RawPcmFormat)
		return E_ERROR;
	return E_OK;
}

IFilter* CAudioRenderer::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CAudioRenderer *result = avf_new CAudioRenderer(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CAudioRenderer::CAudioRenderer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpInput(NULL),
	mp_snd_pcm(NULL),
	bUseBuffer(true),
	mp_snd_buffer(NULL)
{
	mpInput = avf_new CAudioRendererInput(this);
	CHECK_STATUS(mpInput);
	CHECK_OBJ(mpInput);
}

CAudioRenderer::~CAudioRenderer()
{
	avf_safe_release(mpInput);
	avf_safe_free(mp_snd_buffer);
	if (mp_snd_pcm)
		::snd_pcm_close(mp_snd_pcm);
}

avf_status_t CAudioRenderer::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;
	SetRTPriority(CThread::PRIO_RENDERER);
	return E_OK;
}

IPin* CAudioRenderer::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

void CAudioRenderer::FilterLoop()
{
	avf_status_t status;
	unsigned bufferTime;

	if ((status = OpenDevice(&bufferTime)) != E_OK) {
		ReplyCmd(status);
		return;
	}

	if ((status = PrepareBuffer(bufferTime)) != E_OK) {
		ReplyCmd(status);
		return;
	}

	m_total_frames = 0;
	UpdatePlayPosition();
	SetAudioStoppedFlag(false);

	mbPaused = true;
	mbStarting = true;
	ReplyCmd(E_OK);

	while (true) {
		CQueueInputPin *pPin;
		CBuffer *pBuffer;

		if (ReceiveInputBuffer(pPin, pBuffer) != E_OK)
			break;

		if (mbStarting) {
			mbStarting = false;
			PostPreparedMsg();
			if (!WaitForResume(false)) {
				pBuffer->Release();
				goto Done;
			}
		}

		switch (pBuffer->mType) {
		case CBuffer::EOS:
			pBuffer->Release();
			if ((status = WaitEOS()) == E_OK) {
				PostEosMsg();
				SetAudioStoppedFlag(true);
			}
			goto Done;

		case CBuffer::DATA: {
				snd_pcm_uframes_t frames = pBuffer->GetDataSize() / mpInput->FrameSize();
				bool bDone = WriteSndData(pBuffer->GetData(), frames, pBuffer->GetDataSize());

				pBuffer->Release();

				if (!bDone)
					goto Done;

				m_total_frames += frames;
				UpdatePlayPosition();
			}
			break;

		default:
			pBuffer->Release();
			break;
		}
	}

Done:
	if (mp_snd_pcm) {
		::snd_pcm_close(mp_snd_pcm);
		mp_snd_pcm = NULL;
	}
}

bool CAudioRenderer::WriteSndData(avf_u8_t *pdata, snd_pcm_uframes_t frames, avf_u32_t bytes)
{
	if (bUseBuffer) {
		// wait until there's enough room to write the data
		while (true) {
			if (m_snd_avail >= bytes)
				break;

			UpdatePlayPosition();

			if (!ProcessOneCmd(10))
				return false;
		}

		m_snd_avail -= bytes;
		m_snd_filled += bytes;

		// write the data
		if (m_snd_tail >= bytes) {
			::memcpy(mp_snd_wptr, pdata, bytes);
			mp_snd_wptr += bytes;
			m_snd_tail -= bytes;
		} else {
			if (m_snd_tail > 0) {
				::memcpy(mp_snd_wptr, pdata, m_snd_tail);
				pdata += m_snd_tail;
				bytes -= m_snd_tail;
			}
			::memcpy(mp_snd_buffer, pdata, bytes);
			mp_snd_wptr = mp_snd_buffer + bytes;
			m_snd_tail = m_snd_size - bytes;
		}

		// write to pcm device
		m_bytes_delay += m_snd_filled;

		if (mp_snd_rptr + m_snd_filled <= mp_snd_buffer + m_snd_size) {
			WritePCM(mp_snd_rptr, m_snd_filled / mpInput->FrameSize());
			mp_snd_rptr += m_snd_filled;
			m_snd_filled = 0;
		} else {
			avf_size_t tmp = mp_snd_buffer + m_snd_size - mp_snd_rptr;
			WritePCM(mp_snd_rptr, tmp / mpInput->FrameSize());
			tmp = m_snd_filled - tmp;
			WritePCM(mp_snd_buffer, tmp / mpInput->FrameSize());
			mp_snd_rptr = mp_snd_buffer + tmp;
			m_snd_filled = 0;
		}

	} else {
		WritePCM(pdata, frames);
	}

	return true;
}

void CAudioRenderer::WritePCM(avf_u8_t *pdata, snd_pcm_uframes_t frames)
{
	int error = ::snd_pcm_writei(mp_snd_pcm, pdata, frames);
	if (error < 0) {
		//AVF_LOGD("audio underrun");
		if (error == -EPIPE) {
			::snd_pcm_prepare(mp_snd_pcm);
		} else {
			AVF_LOGE("snd_pcm_writei failed %d", error);
			PostErrorMsg(E_DEVICE);
		}
	}
}

void CAudioRenderer::OnStopRenderer()
{
	if (mp_snd_pcm) {
		::snd_pcm_drop(mp_snd_pcm);
	}
}

bool CAudioRenderer::OnPauseRenderer()
{
	if (mp_snd_pcm == NULL) {
		// audio might already stopped
		return true;
	}

	if (bUseBuffer) {
		UpdatePlayPosition();
		::snd_pcm_drop(mp_snd_pcm);
		::snd_pcm_close(mp_snd_pcm);
		mp_snd_pcm = NULL;
	} else {
		::snd_pcm_pause(mp_snd_pcm, 1);
	}

	return WaitForResume(true);
}

avf_status_t CAudioRenderer::Restart()
{
	avf_status_t status = OpenDevice(NULL);
	if (status != E_OK)
		return status;

	if (mp_snd_buffer + m_bytes_delay <= mp_snd_rptr) {
		WritePCM(mp_snd_rptr - m_bytes_delay, m_bytes_delay / mpInput->FrameSize());
	} else {
		int tmp = m_bytes_delay - (mp_snd_rptr - mp_snd_buffer);
		WritePCM(mp_snd_buffer + (m_snd_size - tmp), tmp / mpInput->FrameSize());
		WritePCM(mp_snd_buffer, (m_bytes_delay - tmp) / mpInput->FrameSize());
	}

	return E_OK;
}

bool CAudioRenderer::WaitForResume(bool bForPause)
{
	while (mbPaused) {
		if (!ProcessOneCmd())
			return false;
	}

	if (!SyncWithVideo())
		return false;

	if (bForPause) {
		if (bUseBuffer) {
			avf_status_t status = Restart();
			if (status != E_OK) {
				PostErrorMsg(status);
				return false;
			}
		} else {
			::snd_pcm_pause(mp_snd_pcm, 0);
		}
	}

	return true;
}

void CAudioRenderer::UpdatePlayPosition()
{
	snd_pcm_sframes_t delayp;
	avf_u32_t pos_ms = 0;
	int error;

	error = snd_pcm_delay(mp_snd_pcm, &delayp);
	if (error != 0)
		delayp = 0;

	CAudioFormat *pFormat = (CAudioFormat*)mpInput->GetUsingMediaFormat();
	avf_u32_t pos = (avf_u32_t)((avf_u64_t)(m_total_frames - delayp) * 1000 / pFormat->mTimeScale);
	pos_ms = m_start_ms + pos;
	if (bUseBuffer) {
		avf_u32_t bytes_delay = delayp * mpInput->FrameSize();
		if (bytes_delay > m_bytes_delay) {
			AVF_LOGD("delay: %d, written: %d", bytes_delay, m_bytes_delay);
			bytes_delay = m_bytes_delay;
		}
		avf_u32_t consumed = m_bytes_delay - bytes_delay;
		m_snd_avail += consumed;
		m_bytes_delay = bytes_delay;
	}

	mpEngine->WriteRegInt32(NAME_PB_AUDIO_POS, 0, pos_ms);
}

avf_status_t CAudioRenderer::WaitEOS()
{
	snd_pcm_status_t *status;
	snd_pcm_status_alloca(&status);

	while (true) {
		if (::snd_pcm_status(mp_snd_pcm, status) == 0) {
			snd_pcm_state_t state = snd_pcm_status_get_state(status);
			if (state == SND_PCM_STATE_SUSPENDED || state == SND_PCM_STATE_XRUN) {
				avf_u32_t eos_wait = mpEngine->ReadRegInt32(NAME_PB_EOS_WAIT, 0, VALUE_PB_EOS_WAIT);
				if (eos_wait > 0) {
					AVF_LOGD("Audio: sleep %d ms for EOS", eos_wait);
					if (!ProcessAnyCmds(eos_wait))
						return E_ERROR;
				}
				return E_OK;
			}
		}
		if (!ProcessAnyCmds(AVF_POLL_DELAY)) {
			return E_ERROR;
		}
	}
}

avf_status_t CAudioRenderer::OpenDevice(unsigned *pBufferTime)
{
	CAudioFormat *pFormat = (CAudioFormat*)mpInput->GetUsingMediaFormat();
	snd_pcm_hw_params_t *hw_params = NULL;
	snd_pcm_sw_params_t *sw_params = NULL;
	const char *func = "";
	int error;
	unsigned int rate;
	snd_pcm_uframes_t num_frames;
	snd_pcm_uframes_t buffer_size;
	unsigned int buffer_time = 100*1000;

	error = ::snd_pcm_open(&mp_snd_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (error < 0) {
		AVF_LOGE("snd_pcm_open failed %d", error);
		return E_DEVICE;
	}

	error = ::snd_pcm_hw_params_malloc(&hw_params);
	if (error < 0) {
		func = "snd_pcm_hw_params_malloc";
		goto Error;
	}

	error = ::snd_pcm_hw_params_any(mp_snd_pcm, hw_params);
	if (error < 0) {
		func = "snd_pcm_hw_params_any";
		goto Error;
	}

	error = ::snd_pcm_hw_params_set_access(mp_snd_pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (error < 0) {
		func = "snd_pcm_hw_params_set_access";
		goto Error;
	}

	error = ::snd_pcm_hw_params_set_format(mp_snd_pcm, hw_params, SND_PCM_FORMAT_S16_LE);
	if (error < 0) {
		func = "snd_pcm_hw_params_set_format";
		goto Error;
	}

	error = ::snd_pcm_hw_params_set_channels(mp_snd_pcm, hw_params, pFormat->mNumChannels);
	if (error < 0) {
		func = "snd_pcm_hw_params_set_channels";
		goto Error;
	}

	rate = pFormat->mSampleRate;
//	error = ::snd_pcm_hw_params_set_rate_near(mp_snd_pcm, hw_params, &rate, NULL);
	error = ::snd_pcm_hw_params_set_rate(mp_snd_pcm, hw_params, rate, 0);
	if (error < 0) {
		func = "snd_pcm_hw_params_set_rate_near";
		goto Error;
	}

	num_frames = 1024;	// todo
	error = ::snd_pcm_hw_params_set_period_size_near(mp_snd_pcm, hw_params, &num_frames, NULL);
	if (error < 0) {
		func = "snd_pcm_hw_params_set_period_size_near";
		goto Error;
	}

	error = ::snd_pcm_hw_params_set_buffer_time_near(mp_snd_pcm, hw_params, &buffer_time, 0);
	if (error < 0) {
		func = "snd_pcm_hw_params_set_buffer_time_near";
		goto Error;
	}

	if (pBufferTime)
		*pBufferTime = buffer_time;

	::snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size);

	error = ::snd_pcm_hw_params(mp_snd_pcm, hw_params);
	if (error < 0) {
		func = "snd_pcm_hw_params";
		goto Error;
	}

	::snd_pcm_hw_params_free(hw_params);
	hw_params = NULL;

	// sw
	error = ::snd_pcm_sw_params_malloc(&sw_params);
	if (error < 0) {
		func = "snd_pcm_sw_params_malloc";
		goto Error;
	}

	error = ::snd_pcm_sw_params_current(mp_snd_pcm, sw_params);
	if (error < 0) {
		func = "snd_pcm_sw_params_current";
		goto Error;
	}

	error = ::snd_pcm_sw_params_set_avail_min(mp_snd_pcm, sw_params, num_frames);
	if (error < 0) {
		func = "snd_pcm_sw_params_set_avail_min";
		goto Error;
	}

#if 0
	error = ::snd_pcm_sw_params_set_start_threshold(mp_snd_pcm, sw_params, num_frames);
	if (error < 0) {
		func = "snd_pcm_sw_params_set_start_threshold";
		goto Error;
	}
#endif

#if 0
	error = ::snd_pcm_sw_params_set_stop_threshold(mp_snd_pcm, sw_params, num_frames);
	if (error < 0) {
		func = "snd_pcm_sw_params_set_stop_threshold";
		goto Error;
	}
#endif

	error = ::snd_pcm_sw_params(mp_snd_pcm, sw_params);
	if (error < 0) {
		func = "snd_pcm_sw_params";
		goto Error;
	}

	error = ::snd_pcm_prepare(mp_snd_pcm);
	if (error < 0) {
		func = "snd_pcm_prepare";
		goto Error;
	}

	return E_OK;

Error:
	AVF_LOGE("%s failed %d: %s", func, error, snd_strerror(error));

	if (hw_params)
		::snd_pcm_hw_params_free(hw_params);
	if (sw_params)
		::snd_pcm_sw_params_free(sw_params);

	::snd_pcm_close(mp_snd_pcm);

	mp_snd_pcm = NULL;
	return E_DEVICE;
}

avf_status_t CAudioRenderer::PrepareBuffer(unsigned bufferTime)
{
	// allocate snd buffer
	if (!bUseBuffer)
		return E_OK;

	avf_safe_free(mp_snd_buffer);

	CAudioFormat *pFormat = (CAudioFormat*)mpInput->GetUsingMediaFormat();
	int size = bufferTime/1000 * pFormat->mSampleRate / 1000 * pFormat->FrameSize();

	if ((mp_snd_buffer = avf_malloc_bytes(size)) == NULL)
		return E_NOMEM;

	mp_snd_wptr = mp_snd_buffer;
	mp_snd_rptr = mp_snd_buffer;
	m_snd_size = size;
	m_snd_tail = size;
	m_snd_avail = size;
	m_snd_filled = 0;
	m_bytes_delay = 0;

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CAudioRendererInput
//
//-----------------------------------------------------------------------
CAudioRendererInput::CAudioRendererInput(CAudioRenderer *pFilter):
	inherited(pFilter, 0, "AudioRendererInput")
{
}

CAudioRendererInput::~CAudioRendererInput()
{
}

avf_status_t CAudioRendererInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (CAudioRenderer::RecognizeFormat(pMediaFormat) != E_OK) {
		return E_ERROR;
	}
	return E_OK;
}

