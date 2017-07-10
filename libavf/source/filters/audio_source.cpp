
#define LOG_TAG "audio_source"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "filter_if.h"

#include "avf_config.h"
#include "avf_util.h"

#include "avf_media_format.h"
#include "audio_source.h"

#define FAKE_MONO

//-----------------------------------------------------------------------
//
//  CAudioSource
//
//-----------------------------------------------------------------------
IFilter* CAudioSource::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CAudioSource *result = avf_new CAudioSource(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CAudioSource::CAudioSource(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpOutput(NULL),
	mbEnabled(false),
	m_lag(0)
{
	mp_snd_pcm = NULL;

	m_snd_pcm_format = SND_PCM_FORMAT_S16_LE;
	m_snd_pcm_channels = 2;
	m_snd_pcm_rate = 48000;
	mp_buffer = NULL;

	m_bytes_per_sample = 2;
	m_frames_per_chunk = 1024;	// todo

	if (pFilterConfig) {
		CConfigNode *pAttr;
		if ((pAttr = pFilterConfig->FindChild("lag")) != NULL) {
			m_lag = ::atoi(pAttr->GetValue());
		}
		if ((pAttr = pFilterConfig->FindChild("channels")) != NULL) {
			m_snd_pcm_channels = ::atoi(pAttr->GetValue());
			AVF_LOGD("snd channels: %d", m_snd_pcm_channels);
		}
		if ((pAttr = pFilterConfig->FindChild("samplerate")) != NULL) {
			m_snd_pcm_rate = ::atoi(pAttr->GetValue());
			AVF_LOGD("snd pcm rate: %d", m_snd_pcm_rate);
		}
	}
}

CAudioSource::~CAudioSource()
{
	StopDevice();
	CloseDevice();
	avf_delete mpOutput;
	avf_safe_free(mp_buffer);
}

void* CAudioSource::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_ISourceFilter)
		return static_cast<ISourceFilter*>(this);
	return inherited::GetInterface(refiid);
}

avf_status_t CAudioSource::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	SetRTPriority(CThread::PRIO_SOURCE);

	mpOutput = avf_new CAudioSourceOutput(this);
	CHECK_STATUS(mpOutput);
	if (mpOutput == NULL)
		return E_NOMEM;

	return E_OK;
}

IPin* CAudioSource::GetOutputPin(avf_uint_t index)
{
	return index == 0 ? mpOutput : NULL;
}

void CAudioSource::SetBuffer(CBuffer *pBuffer)
{
	pBuffer->mType = CBuffer::DATA;
	pBuffer->mFlags = CBuffer::F_KEYFRAME;
	//pBuffer->mpData = 
	//pBuffer->mSize = GetBytesPerChunk();
	pBuffer->m_offset = 0;
	pBuffer->m_data_size = pBuffer->mSize;
	pBuffer->m_dts = m_curr_pts;
	pBuffer->m_pts = m_curr_pts;
	pBuffer->m_duration = m_frames_per_chunk;
	
	m_curr_pts += m_frames_per_chunk;
}

void CAudioSource::FilterLoop()
{
	avf_status_t status;
	CAudioFormat *pFormat = (CAudioFormat*)mpOutput->GetUsingMediaFormat();
	avf_size_t chunk_size = pFormat->ChunkSize();
	avf_size_t discard_bytes = 0;
	avf_size_t buffer_size = GetBytesPerChunk();

	ReplyCmd(E_OK);

	if (mbEnabled) {
		if ((status = StartDevice()) != E_OK) {
			PostErrorMsg(E_DEVICE);
			goto Done;
		}
	}

	mbStopped = false;
	m_curr_pts = 0;
	avf_safe_free(mp_buffer);

	CBuffer *pBuffer;

	if (mbEnabled) {
		// handle lag: audio ahead of video => fill empty buffers
		if (m_lag < 0) {
			avf_size_t bytes = pFormat->TimeToBytes(-m_lag);

			while (!mbStopped) {
				if (!AllocOutputBuffer(mpOutput, pBuffer, buffer_size))
					goto Stopped;

				::memset(pBuffer->mpData, 0, pBuffer->m_data_size);

				SetBuffer(pBuffer);
				mpOutput->PostBuffer(pBuffer);

				if (bytes < chunk_size)
					break;

				bytes -= chunk_size;
			}
		} else {
			discard_bytes = pFormat->TimeToBytes(m_lag);
			//AVF_LOGD("lag: %d ms, discard %d bytes", m_lag, discard_bytes);
		}
	}

	tick_info_init(&m_tick_info);

	if (mbEnabled) {
		while (!mbStopped) {

			if (!AllocOutputBuffer(mpOutput, pBuffer, buffer_size))
				break;

			if ((status = ReadDevice(pBuffer->mpData)) != E_OK) {
				pBuffer->Release();
				PostErrorMsg(status);
				break;
			}

			if (discard_bytes < chunk_size) {
				SetBuffer(pBuffer);
				mpOutput->PostBuffer(pBuffer);
			} else {
				discard_bytes -= chunk_size;
				pBuffer->Release();
			}

#ifdef DEBUG_AVSYNC
			tick_info_update(&m_tick_info, 50);
#endif
		}
	} else {
		while (!mbStopped) {
			if (!ProcessAnyCmds(-1))
				break;
		}
	}

Stopped:
	if (mbStopped) {
		GenerateEOS(mpOutput);
	}

	StopDevice();

Done:
	CloseDevice();
}

avf_status_t CAudioSource::OpenDevice()
{
	snd_pcm_hw_params_t *hw_params = NULL;
	snd_pcm_sw_params_t *sw_params = NULL;
	const char *func = NULL;
	int rval;

	uint32_t buffer_time;
	snd_pcm_uframes_t buffer_size;
	snd_pcm_uframes_t num_frames;
	int dir;

	CloseDevice();

	rval = ::snd_pcm_open(&mp_snd_pcm, "default", SND_PCM_STREAM_CAPTURE, 0);
	if (rval < 0) {
		func = "snd_pcm_open";
		goto Done;
	}

	AVF_ASSERT(mp_snd_pcm);

	// allocate a hardware parameters object
	rval = ::snd_pcm_hw_params_malloc(&hw_params);
	if (rval < 0) {
		func = "snc_pcm_hw_params_malloc";
		goto Done;
	}

	// fill it with default values
	rval = ::snd_pcm_hw_params_any(mp_snd_pcm, hw_params);
	if (rval < 0) {
		func = "snd_pcm_hw_params_any";
		goto Done;
	}

	// interleaved mode
	rval = ::snd_pcm_hw_params_set_access(mp_snd_pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (rval < 0) {
		func = "snd_pcm_hw_params_set_access";
		goto Done;
	}

	// format, e.g. 16-bit little-endian
	rval = ::snd_pcm_hw_params_set_format(mp_snd_pcm, hw_params, m_snd_pcm_format);
	if (rval < 0) {
		func = "snd_pcm_hw_params_set_format";
		goto Done;
	}

	// number of channels
#ifdef FAKE_MONO
	if (m_snd_pcm_channels == 1)
		rval = ::snd_pcm_hw_params_set_channels(mp_snd_pcm, hw_params, 2);
	else
#endif
		rval = ::snd_pcm_hw_params_set_channels(mp_snd_pcm, hw_params, m_snd_pcm_channels);
	if (rval < 0) {
		func = "snd_pcm_hw_params_set_channels";
		goto Done;
	}

	// sampling rate, like 44100, 48000
	rval = ::snd_pcm_hw_params_set_rate(mp_snd_pcm, hw_params, m_snd_pcm_rate, 0);
	if (rval < 0) {
		func = "snd_pcm_hw_params_set_rate";
		goto Done;
	}

	rval = ::snd_pcm_hw_params_get_buffer_time_max(hw_params, &buffer_time, &dir);
	if (rval < 0) {
		func = "snd_pcm_hw_params_get_buffer_time_max";
		goto Done;
	}

	rval = ::snd_pcm_hw_params_set_buffer_time_near(mp_snd_pcm, hw_params, &buffer_time, &dir);
	if (rval < 0) {
		func = "snd_pcm_hw_params_set_buffer_time_near";
		goto Done;
	}

	num_frames = m_frames_per_chunk;
	rval = ::snd_pcm_hw_params_set_period_size_near(mp_snd_pcm, hw_params, &num_frames, NULL);
	if (rval < 0) {
		func = "snd_pcm_hw_params_set_period_size_near";
		goto Done;
	}

	// write the parameters to driver
	rval = ::snd_pcm_hw_params(mp_snd_pcm, hw_params);
	if (rval < 0) {
		func = "snd_pcm_hw_params";
		goto Done;
	}

	rval = ::snd_pcm_hw_params_get_buffer_size_max(hw_params, &buffer_size);
	if (rval < 0) {
		func = "snd_pcm_hw_params_get_buffer_size_max";
		goto Done;
	}
	//printf("buffer_size: %d (frames)\n", (int)buffer_size);

	rval = ::snd_pcm_hw_params_get_period_size(hw_params, &num_frames, &dir);
	if (rval < 0) {
		func = "snd_pcm_hw_params_get_period_size";
		goto Done;
	}
	//printf("period_size: %d (frames), dir: %d\n", (int)num_frames, dir);

	// sw

	rval = ::snd_pcm_sw_params_malloc(&sw_params);
	if (rval < 0) {
		func = "snd_pcm_sw_params_malloc";
		goto Done;
	}

	rval = ::snd_pcm_sw_params_current(mp_snd_pcm, sw_params);
	if (rval < 0) {
		func = "snd_pcm_sw_params_current";
		goto Done;
	}

	rval = ::snd_pcm_sw_params_set_avail_min(mp_snd_pcm, sw_params, num_frames);
	if (rval < 0) {
		func = "snd_pcm_sw_params_set_avail_min";
		goto Done;
	}

	rval = ::snd_pcm_sw_params_set_start_threshold(mp_snd_pcm, sw_params, 1);
	if (rval < 0) {
		func = "snd_pcm_sw_params_set_start_threshold";
		goto Done;
	}

	rval = ::snd_pcm_sw_params_set_stop_threshold(mp_snd_pcm, sw_params, buffer_size);
	if (rval < 0) {
		func = "snd_pcm_sw_params_set_stop_threshold";
		goto Done;
	}

	rval = ::snd_pcm_sw_params(mp_snd_pcm, sw_params);
	if (rval < 0) {
		func = "snd_pcm_sw_params";
		goto Done;
	}

	// prepare
	rval = ::snd_pcm_prepare(mp_snd_pcm);
	if (rval < 0) {
		AVF_LOGE("snd_pcm_prepare failed: %s", snd_strerror(rval));
		return E_DEVICE;
	}

	rval = 0;

Done:
	if (hw_params)
		::snd_pcm_hw_params_free(hw_params);
	if (sw_params)
		::snd_pcm_sw_params_free(sw_params);

	if (rval < 0) {
		AVF_LOGE("%s failed: %s", func, snd_strerror(rval));
	}

	return rval < 0 ? E_DEVICE : E_OK;
}

avf_status_t CAudioSource::StartDevice()
{
	int rval;

	rval = ::snd_pcm_start(mp_snd_pcm);
	if (rval < 0) {
		AVF_LOGE("snd_pcm_start failed: %s", snd_strerror(rval));
		return E_DEVICE;
	}

	return E_OK;
}

void CAudioSource::StopDevice()
{
	if (mp_snd_pcm) {
		::snd_pcm_drop(mp_snd_pcm);
	}
}

void CAudioSource::CloseDevice()
{
	if (mp_snd_pcm) {
		::snd_pcm_close(mp_snd_pcm);
		mp_snd_pcm = NULL;
#ifdef MEM_DEBUG
		snd_config_update_free_global();
#endif
	}
}

avf_status_t CAudioSource::ReadDevice(avf_u8_t *buffer)
{
	avf_u32_t frames = m_frames_per_chunk;
	avf_u8_t *ptr;
	int rval;

	ptr = buffer;

#ifdef FAKE_MONO
	if (m_snd_pcm_channels == 1) {
		if (mp_buffer == NULL) {
			mp_buffer = avf_malloc_bytes(GetBytesPerChunk() * 2);
		}
		ptr = mp_buffer;
	}
#endif

	while (true) {

		rval = ::snd_pcm_readi(mp_snd_pcm, ptr, frames);
		if (rval < 0) {
			if (rval == -EPIPE) {
				AVF_LOGW("Audio xrun: %s", snd_strerror(rval));
				if ((rval = snd_pcm_prepare(mp_snd_pcm)) < 0) {
					AVF_LOGE("snd_pcm_prepare failed!");
					return E_DEVICE;
				}
				continue;
			}
			AVF_LOGE("snd_pcm_readi failed: %s", snd_strerror(rval));
			return E_DEVICE;
		}

		frames -= rval;
		if (frames == 0)
			break;

		AVF_LOGI(C_RED "frames %d rval %d" C_NONE, frames, rval);

#ifdef FAKE_MONO
		if (m_snd_pcm_channels == 1)
			ptr += 2 * m_bytes_per_sample * rval;
		else
#endif
			ptr += m_snd_pcm_channels * m_bytes_per_sample * rval;

		AVF_LOGD("There're %d frames", (int)frames);
	}

#ifdef FAKE_MONO
	if (m_snd_pcm_channels == 1) {
		avf_u16_t *p = (avf_u16_t*)buffer;
		avf_u16_t *psrc = (avf_u16_t*)mp_buffer;
		for (avf_size_t i = m_frames_per_chunk/4; i; i--) {
			p[0] = psrc[0];
			p[1] = psrc[2];
			p[2] = psrc[4];
			p[3] = psrc[6];
			p += 4;
			psrc += 8;
		}
		for (avf_size_t i = m_frames_per_chunk&3; i; i--) {
			*p = *psrc;
			p++;
			psrc += 2;
		}
	}
#endif

	return E_OK;
}

bool CAudioSource::AOCmdProc(const CMD& cmd, bool bInAOLoop)
{
	avf_status_t status;

	switch (cmd.code) {
	case CMD_START_SOURCE:
		mbEnabled = mpEngine->ReadRegInt32(NAME_RECORD, 0, 0) != 0;
		*(bool*)cmd.pExtra = mbEnabled ? false : true;	// pbDone
		ReplyCmd(E_OK);

		if (mbEnabled) {
			status = OpenDevice();
			if (status != E_OK) {
				PostErrorMsg(status);
			} else {
				PostStartedMsg();
			}
		}

		return true;

	case CMD_STOP_SOURCE:
		ReplyCmd(E_OK);
		mbStopped = true;
		return true;

	default:
		return inherited::AOCmdProc(cmd, bInAOLoop);
	}
}

//-----------------------------------------------------------------------
//
//  CAudioSourceOutput
//
//-----------------------------------------------------------------------

CAudioSourceOutput::CAudioSourceOutput(CAudioSource *pFilter):
	inherited(pFilter, 0, "AudioSourceOutput"),
	mpSource(pFilter)
{
}

CAudioSourceOutput::~CAudioSourceOutput()
{
}

CMediaFormat* CAudioSourceOutput::QueryMediaFormat()
{
	CAudioFormat *pFormat = avf_alloc_media_format(CAudioFormat);

	if (pFormat) {
		// CMediaFormat
		pFormat->mMediaType = MEDIA_TYPE_AUDIO;
		pFormat->mFormatType = MF_RawPcmFormat;
		pFormat->mFrameDuration = mpSource->m_frames_per_chunk;	// e.g. 1024
		pFormat->mTimeScale = mpSource->m_snd_pcm_rate;	// e.g. 44100, 48000

		// CAudioFormat
		pFormat->mNumChannels = mpSource->m_snd_pcm_channels;
		pFormat->mSampleRate = mpSource->m_snd_pcm_rate;
		pFormat->mBitsPerSample = mpSource->m_bytes_per_sample * 8;
		pFormat->mBitsFormat = 0;
		pFormat->mFramesPerChunk = mpSource->m_frames_per_chunk;
	}

	return pFormat;
}

IBufferPool *CAudioSourceOutput::QueryBufferPool()
{
//	IBufferPool *pBufferPool = CMemoryBufferPool::Create("AudioBP", 128, 
//		sizeof(CBuffer), mpSource->GetBytesPerChunk());
	IBufferPool *pBufferPool = CDynamicBufferPool::CreateNonBlock("AudioBP", NULL, 50);
	return pBufferPool;
}

