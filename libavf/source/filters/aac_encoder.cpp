
#define LOG_TAG "aac_encoder"

// to avoid link error
#if defined(ANDROID_OS) && defined(USE_FDK_AAC)
int stdout = -1;
int stdin = -1;
int stderr = -1;
extern "C" void _IO_getc(void) {}
#endif

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_config.h"
#include "avf_util.h"

#include "avf_media_format.h"
#include "aac_encoder.h"

#ifdef USE_FDK_AAC
#else
#include "aac_lib/aac_audio_enc.h"
#endif

#include "mpeg_utils.h"

//-----------------------------------------------------------------------
//
//  CAacEncoder
//
//-----------------------------------------------------------------------
IFilter* CAacEncoder::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CAacEncoder *result = avf_new CAacEncoder(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CAacEncoder::CAacEncoder(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpBufferPool(NULL),
	mpInput(NULL),
	mpOutput(NULL),
	mbEnabled(false),

#ifdef USE_FDK_AAC
	m_enc_handle(NULL),
	mp_input_buffer(NULL),
#else
	mp_encode_buffer(NULL),
	mp_encoder_config(NULL),
	mbEncoderOpen(false),
#endif

	m_bitrate(128000),
	m_afterburner(0),
	m_enc_duration(0),
	m_fade_ms(0)
{
	CConfigNode *pAttr;

	if (pFilterConfig) {
		if ((pAttr = pFilterConfig->FindChild("bitrate")) != NULL)
			m_bitrate = ::atoi(pAttr->GetValue());

		if ((pAttr = pFilterConfig->FindChild("afterburner")) != NULL) {
			m_afterburner = ::atoi(pAttr->GetValue()) != 0;
			if (m_afterburner) {
				AVF_LOGD("afterburner");
			}
		}

		if ((pAttr = pFilterConfig->FindChild("duration")) != NULL) {
			m_enc_duration = ::atoi(pAttr->GetValue());
			AVF_LOGD("duration: %d", m_enc_duration);
		}

		if ((pAttr = pFilterConfig->FindChild("fade-ms")) != NULL) {
			m_fade_ms = ::atoi(pAttr->GetValue());
			AVF_LOGD("fade_ms: %d", m_fade_ms);
		}
	}

	CREATE_OBJ(mpBufferPool = CDynamicBufferPool::CreateNonBlock("AACEncOutput", NULL, 50));

	// do not attach the output pins
	// We'll share a dynamic buffer pool on all outputs
	mbAttachOutput = false;
}

CAacEncoder::~CAacEncoder()
{
	CloseEncoder();

#ifdef USE_FDK_AAC
	//
#else
	avf_safe_free(mp_encode_buffer);
	avf_safe_free(mp_encoder_config);
#endif

	avf_safe_release(mpInput);
	avf_safe_release(mpOutput);

	avf_safe_release(mpBufferPool);
}

void CAacEncoder::OnAttachAllPins(bool bAttach)
{
	AttachBufferPool(mpBufferPool, bAttach, CMsgQueue::Q_OUTPUT);
}

avf_status_t CAacEncoder::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	SetRTPriority(CThread::PRIO_CODEC);

#ifdef USE_FDK_AAC
	//
#else
	// todo - is the buffer large enough?
	if ((mp_encode_buffer = avf_malloc_bytes(4*1024)) == NULL)
		return E_NOMEM;

	if ((mp_encoder_config = avf_malloc_type(struct au_aacenc_config_s)) == NULL)
		return E_NOMEM;

	::memset(mp_encoder_config, 0, sizeof(*mp_encoder_config));
#endif

	mpInput = avf_new CAacEncoderInput(this);
	CHECK_STATUS(mpInput);
	if (mpInput == NULL)
		return E_NOMEM;

	mpOutput = avf_new CAacEncoderOutput(this);
	CHECK_STATUS(mpOutput);
	if (mpOutput == NULL)
		return E_NOMEM;

	return E_OK;
}

IPin* CAacEncoder::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

IPin* CAacEncoder::GetOutputPin(avf_uint_t index)
{
	return index == 0 ? mpOutput : NULL;
}

void CAacEncoder::FilterLoop()
{
	avf_status_t status;

	mbEnabled = mpEngine->ReadRegInt32(NAME_RECORD, 0, 0) != 0;
	m_input_length = 0;

	CMediaFormat *pFormat = mpInput->GetUsingMediaFormat();
	m_duration_eos = (avf_u32_t)((avf_u64_t)(m_enc_duration + 50)
		* pFormat->mTimeScale / 1000);
	m_start_fade_pts = (m_enc_duration <= m_fade_ms) ? 0 :
		(avf_u32_t)((avf_u64_t)(m_enc_duration - m_fade_ms)
		* pFormat->mTimeScale / 1000);
	m_end_pts = (avf_u32_t)((avf_u64_t)m_enc_duration * pFormat->mTimeScale / 1000);

	if (mbEnabled) {
		status = OpenEncoder();
		if (status != E_OK) {
			CloseEncoder();
			ReplyCmd(status);
			return;
		}
	}

	ReplyCmd(E_OK);

	tick_info_init(&m_tick_info);

	while (true) {
		CQueueInputPin *pInPin;
		CBuffer *pInBuffer;

		if (ReceiveInputBuffer(pInPin, pInBuffer) != E_OK)
			break;

		if (!ProcessInputBuffer(pInBuffer)) {
			pInBuffer->Release();
			break;
		}

		pInBuffer->Release();
	}

	CloseEncoder();
}

#ifdef USE_FDK_AAC
bool CAacEncoder::FDK_FeedData(avf_u8_t *p, avf_size_t size)
{
	while (size > 0) {
		avf_size_t tocopy = m_input_frame_size - m_input_bytes;
		if (tocopy > size) tocopy = size;

		::memcpy(mp_input_buffer + m_input_bytes, p, tocopy);

		p += tocopy;
		size -= tocopy;

		m_input_bytes += tocopy;

		if (m_input_bytes == m_input_frame_size) {
			if (!FDK_Encode(false)) {
				return false;
			}
		}
	}
	return true;
}

bool CAacEncoder::FDK_Encode(bool bEOS)
{
	while (true) {
		AACENC_BufDesc in_buf = {0};
		AACENC_BufDesc out_buf = {0};
		AACENC_InArgs in_args = {0};
		AACENC_OutArgs out_args = {0};

		int in_identifier = IN_AUDIO_DATA;
		void *in_ptr = (void*)mp_input_buffer;
		int in_size = m_input_bytes;
		int in_elem_size = 2;
		
		int out_identifier = OUT_BITSTREAM_DATA;
		void *out_ptr = (void*)m_enc_buffer;
		int out_size = ARRAY_SIZE(m_enc_buffer);
		int out_elem_size = 1;
		avf_u32_t size = 0;

		int numInSamples = m_input_bytes / 2;
		in_args.numInSamples = m_input_bytes == 0 ? -1 : numInSamples;
		m_input_bytes = 0;

		in_buf.numBufs = 1;
		in_buf.bufs = &in_ptr;
		in_buf.bufferIdentifiers = &in_identifier;
		in_buf.bufSizes = &in_size;
		in_buf.bufElSizes = &in_elem_size;
		
		out_buf.numBufs = 1;
		out_buf.bufs = &out_ptr;
		out_buf.bufferIdentifiers = &out_identifier;
		out_buf.bufSizes = &out_size;
		out_buf.bufElSizes = &out_elem_size;

		AACENC_ERROR error = aacEncEncode(m_enc_handle, &in_buf, &out_buf, &in_args, &out_args);
		if (error != AACENC_OK) {
			if (error == AACENC_ENCODE_EOF && bEOS) {
				SendAllEOS();
			} else {
				AVF_LOGE("aacEncEncode failed: %d", error);
				PostErrorMsg(E_ERROR);
			}
			return false;
		}

		size = out_args.numOutBytes;
		if (size > 0) {
			CBuffer *pOutBuffer;
			if (!AllocOutputBuffer(mpBufferPool, mpOutput, pOutBuffer, size))
				return false;

			::memcpy(pOutBuffer->mpData, m_enc_buffer, size);
			avf_size_t duration = numInSamples / m_num_channels;

			pOutBuffer->mType = CBuffer::DATA;
			pOutBuffer->mFlags = CBuffer::F_KEYFRAME;
			//pBuffer->mpData = 
			//pOutBuffer->mSize = size;
			pOutBuffer->m_offset = 0;
			pOutBuffer->m_data_size = size;
			pOutBuffer->m_dts = m_pts;
			pOutBuffer->m_pts = m_pts;
			pOutBuffer->m_duration = duration;
			m_pts += duration;

			SendToAllPins(pOutBuffer);
		}

		if (!bEOS)
			break;
	}

	return true;
}
#endif

// 16 bits le
void CAacEncoder::zoomSamples(CBuffer *pInBuffer, int zoom100)
{
	int n = pInBuffer->GetDataSize() / 2;
	avf_s16_t *ptr = (avf_s16_t*)pInBuffer->GetData();
	for (int i = 0; i < n; i++, ptr++) {
		*ptr = (*ptr) * zoom100 / 100;
	}
}

bool CAacEncoder::ProcessInputBuffer(CBuffer *pInBuffer)
{
	switch (pInBuffer->mType) {
	case CBuffer::EOS:
		SendAllEOS();
		return false;

	case CBuffer::DATA:
		if (mbEnabled) {

			// fade out
			if (m_enc_duration > 0) {
				if (m_input_length > m_start_fade_pts) {
					if (m_input_length > m_duration_eos) {
						AVF_LOGD("local_duration: %d, timescale: %d", m_duration_eos, pInBuffer->GetTimeScale());
						SendAllEOS();
						PostFilterMsg(IEngine::MSG_EARLY_EOS, 0);
						return false;
					}
					if (m_fade_ms > 0) {
						int zoom100;
						if (m_input_length >= m_end_pts) {
							zoom100 = 0;
						} else {
							zoom100 = 100 * (m_end_pts - m_input_length) / (m_end_pts - m_start_fade_pts);
						}
						//AVF_LOGD("zoom100: %d", zoom100);
						zoomSamples(pInBuffer, zoom100);
					}
				}
				m_input_length += pInBuffer->m_duration;
			}

#ifdef USE_FDK_AAC

			if (!FDK_FeedData(pInBuffer->GetData(), pInBuffer->GetDataSize()))
				return false;

#else
			mp_encoder_config->enc_rptr = (s32*)pInBuffer->GetData();
			mp_encoder_config->enc_wptr = (u8*)mp_encode_buffer;
			aacenc_encode(mp_encoder_config);
			int size = (mp_encoder_config->nBitsInRawDataBlock + 7) >> 3;

			CBuffer *pOutBuffer;
			if (!AllocOutputBuffer(mpBufferPool, mpOutput, pOutBuffer, size))
				return false;

			::memcpy(pOutBuffer->mpData, mp_encode_buffer, size);

			pOutBuffer->mType = CBuffer::DATA;
			pOutBuffer->mFlags = CBuffer::F_KEYFRAME;
			//pBuffer->mpData = 
			//pOutBuffer->mSize = size;
			pOutBuffer->m_offset = 0;
			pOutBuffer->m_data_size = size;
			pOutBuffer->m_dts = pInBuffer->m_dts;
			pOutBuffer->m_pts = pInBuffer->m_pts;
			pOutBuffer->m_duration = pInBuffer->m_duration;

			SendToAllPins(pOutBuffer);
#endif

		}
		break;

	default:
		break;
	}

	return true;
}

void CAacEncoder::SendToAllPins(CBuffer *pOutBuffer)
{
	mpOutput->PostBuffer(pOutBuffer);
	if (tick_info_update(&m_tick_info, 50)) {
		AVF_LOGD("num input: %d", mpInput->GetNumInputBuffers());
	}
}

void CAacEncoder::SendAllEOS()
{
	CBuffer *pOutBuffer;
	if (AllocOutputBuffer(mpBufferPool, mpOutput, pOutBuffer, 0)) {
		SetEOS(mpOutput, pOutBuffer);
		SendToAllPins(pOutBuffer);
		PostEosMsg();
	}
}

avf_status_t CAacEncoder::OpenEncoder()
{
#ifdef USE_FDK_AAC

	CHANNEL_MODE mode;

	if (m_enc_handle != NULL)
		return E_OK;

	CAudioFormat *pInFormat = static_cast<CAudioFormat*>(mpInput->GetUsingMediaFormat());
	AVF_ASSERT(pInFormat);

	if (aacEncOpen(&m_enc_handle, 0, pInFormat->mNumChannels) != AACENC_OK) {
		AVF_LOGE("aacEncOpen failed");
		return E_ERROR;
	}

	// 2: MPEG-4 AAC Low Complexity
	// 129: MPEG-2 AAC Low Complexity
	if (aacEncoder_SetParam(m_enc_handle, AACENC_AOT, 129) != AACENC_OK) {
		AVF_LOGE("set AACENC_AOT failed");
		return E_ERROR;
	}

	if (aacEncoder_SetParam(m_enc_handle, AACENC_SAMPLERATE, pInFormat->mSampleRate) != AACENC_OK) {
		AVF_LOGE("set AACENC_SAMPLERATE failed");
		return E_ERROR;
	}

	switch (pInFormat->mNumChannels) {
	case 1: mode = MODE_1; break;
	case 2: mode = MODE_2; break;
	case 3: mode = MODE_1_2; break;
	case 4: mode = MODE_1_2_1; break;
	case 5: mode = MODE_1_2_2; break;
	case 6: mode = MODE_1_2_2_1; break;
	default:
		AVF_LOGE("Bad channels: %d", pInFormat->mNumChannels);
		return E_ERROR;
	}

	if (aacEncoder_SetParam(m_enc_handle, AACENC_CHANNELMODE, mode) != AACENC_OK) {
		AVF_LOGE("set AACENC_CHANNELMODE failed");
		return E_ERROR;
	}

	// 1 - WAVE file format channel ordering (e. g. 5.1: L, R, C, LFE, SL, SR)
	if (aacEncoder_SetParam(m_enc_handle, AACENC_CHANNELORDER, 1) != AACENC_OK) {
		AVF_LOGE("set AACENC_CHANNELORDER failed");
		return E_ERROR;
	}

	if (aacEncoder_SetParam(m_enc_handle, AACENC_BITRATE, m_bitrate) != AACENC_OK) {
		AVF_LOGE("set AACENC_BITRATE failed");
		return E_INVAL;
	}

	// 2: ADTS bitstream format
	if (aacEncoder_SetParam(m_enc_handle, AACENC_TRANSMUX, 2) != AACENC_OK) {
		AVF_LOGE("set AACENC_TRANSMUX failed");
		return E_INVAL;
	}

	if (aacEncoder_SetParam(m_enc_handle, AACENC_AFTERBURNER, m_afterburner) != AACENC_OK) {
		AVF_LOGE("set AACENC_AFTERBURNER failed");
		return E_ERROR;
	}

	if (aacEncEncode(m_enc_handle, NULL, NULL, NULL, NULL) != AACENC_OK) {
		AVF_LOGE("aacEncEncode failed");
		return E_ERROR;
	}

	if (aacEncInfo(m_enc_handle, &m_enc_info) != AACENC_OK) {
		AVF_LOGE("aacEncInfo failed");
		return E_ERROR;
	}

#if 0
	AVF_LOGD("aac maxOutBufBytes: %d", m_enc_info.maxOutBufBytes);
	AVF_LOGD("aac maxAncBytes: %d", m_enc_info.maxAncBytes);
	AVF_LOGD("aac inBufFillLevel: %d", m_enc_info.inBufFillLevel);
	AVF_LOGD("aac inputChannels: %d", m_enc_info.inputChannels);
	AVF_LOGD("aac frameLength: %d", m_enc_info.frameLength);
	AVF_LOGD("aac encoderDelay: %d", m_enc_info.encoderDelay);
#endif

	m_input_frame_size = m_enc_info.inputChannels * 2 * m_enc_info.frameLength;
	mp_input_buffer = avf_malloc_bytes(m_input_frame_size);
	if (mp_input_buffer == NULL) {
		AVF_LOGE("aac failed to alloc mem %d", m_input_frame_size);
		return E_ERROR;
	}
	m_input_bytes = 0;

	m_pts = 0;

	return E_OK;

#else

	if (mbEncoderOpen)
		return E_OK;

	if ((mp_encoder_config->codec_lib_mem_adr = (u32*)avf_malloc(106000)) == NULL)
		return E_NOMEM;

	CAudioFormat *pInFormat = static_cast<CAudioFormat*>(mpInput->GetUsingMediaFormat());
	AVF_ASSERT(pInFormat);

	mp_encoder_config->sample_freq = pInFormat->mSampleRate;
	mp_encoder_config->coreSampleRate = pInFormat->mSampleRate;
	mp_encoder_config->Src_numCh = pInFormat->mNumChannels;
	mp_encoder_config->Out_numCh = pInFormat->mNumChannels;
	mp_encoder_config->bitRate = m_bitrate;
	mp_encoder_config->quantizerQuality = 2;
	mp_encoder_config->tns = 1;
	mp_encoder_config->ffType = 't';
	mp_encoder_config->enc_mode = 0;	// 0: AAC; 1: AAC+; 2: AAC++
	mp_encoder_config->ErrorStatus = 0;

	aacenc_setup(mp_encoder_config);
	if (mp_encoder_config->ErrorStatus != ENCODE_OK) {
		AVF_LOGE("aacenc_setup returns error %d", mp_encoder_config->ErrorStatus);
		return E_DEVICE;
	}

	aacenc_open(mp_encoder_config);
	mbEncoderOpen = true;

	return E_OK;
#endif
}

void CAacEncoder::CloseEncoder()
{
#ifdef USE_FDK_AAC

	if (m_enc_handle) {
		aacEncClose(&m_enc_handle);
		m_enc_handle = NULL;
	}

	avf_safe_free(mp_input_buffer);

#else

	if (mbEncoderOpen) {
		mbEncoderOpen = false;
		aacenc_close(0);
	}
	avf_safe_free(mp_encoder_config->codec_lib_mem_adr);

#endif
}

//-----------------------------------------------------------------------
//
//  CAacEncoderInput
//
//-----------------------------------------------------------------------

CAacEncoderInput::CAacEncoderInput(CAacEncoder *pFilter):
	inherited(pFilter, 0, "AAC_encoder_input")
{
	mpEncoder = pFilter;
}

CAacEncoderInput::~CAacEncoderInput()
{
}

avf_status_t CAacEncoderInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (pMediaFormat->mMediaType != MEDIA_TYPE_AUDIO)
		return E_ERROR;
	if (pMediaFormat->mFormatType != MF_RawPcmFormat)
		return E_ERROR;

	CRawPcmFormat *pFormat = static_cast<CRawPcmFormat*>(pMediaFormat);

	mpEncoder->m_num_channels = pFormat->mNumChannels;
	mpEncoder->m_sample_rate = pFormat->mSampleRate;
	mpEncoder->m_sample_rate_index = aac_get_sample_rate_index(pFormat->mSampleRate);

	if (mpEncoder->m_sample_rate_index < 0) {
		AVF_LOGE("Sample rate %d is not accepted by aac encoder", (int)pFormat->mSampleRate);
		return E_ERROR;
	}

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CAacEncoderOutput
//
//-----------------------------------------------------------------------
CAacEncoderOutput::CAacEncoderOutput(CAacEncoder *pFilter):
	inherited(pFilter, 0, "AAC_encoder_output")
{
	mpEncoder = pFilter;
}

CAacEncoderOutput::~CAacEncoderOutput()
{
}

CAudioFormat *CAacEncoderOutput::GetInputMediaFormat()
{
	if (!mpEncoder->mpInput->Connected()) {
		AVF_LOGD("AAC encoder input not connected");
		return NULL;
	}

	return static_cast<CAudioFormat*>(mpEncoder->mpInput->GetUsingMediaFormat());
}

CMediaFormat* CAacEncoderOutput::QueryMediaFormat()
{
	CAudioFormat *pInFormat = GetInputMediaFormat();
	if (pInFormat == NULL)
		return NULL;

	CAacFormat *pOutFormat = avf_alloc_media_format(CAacFormat);
	if (pOutFormat) {
		// CMediaFormat
		pOutFormat->mMediaType = MEDIA_TYPE_AUDIO;
		pOutFormat->mFormatType = MF_AACFormat;
		pOutFormat->mFrameDuration = pInFormat->mFrameDuration;
#ifdef USE_FDK_AAC
		pOutFormat->mTimeScale = mpEncoder->m_sample_rate;
#else
		pOutFormat->mTimeScale = pInFormat->mTimeScale;
#endif

		// CAudioFormat
		pOutFormat->mNumChannels = pInFormat->mNumChannels;
		pOutFormat->mSampleRate = pInFormat->mSampleRate;
		pOutFormat->mBitsPerSample = pInFormat->mBitsPerSample;
		pOutFormat->mBitsFormat = 0;
		pOutFormat->mFramesPerChunk = pInFormat->mFramesPerChunk;

		pOutFormat->mFlags |= CMediaFormat::F_HAS_EXTRA_DATA;
		pOutFormat->mDecoderDataSize = 2;
		avf_u32_t data = aac_gen_specific_config(pInFormat->mSampleRate, pInFormat->mNumChannels, 0);
		pOutFormat->mDecoderData[0] = data >> 8;
		pOutFormat->mDecoderData[1] = data;
	}

	return pOutFormat;
}

IBufferPool *CAacEncoderOutput::QueryBufferPool()
{
	CAudioFormat *pInFormat = GetInputMediaFormat();
	if (pInFormat == NULL)
		return NULL;

	return mpEncoder->mpBufferPool->acquire();
}

