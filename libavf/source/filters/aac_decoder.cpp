
#define LOG_TAG "aac_decoder"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_media_format.h"
#include "aac_decoder.h"

#ifdef USE_FDK_AAC
#else
#include "aac_lib/aac_audio_dec.h"
#endif

#include "mpeg_utils.h"

//-----------------------------------------------------------------------
//
//  CAacDecoder
//
//-----------------------------------------------------------------------
avf_status_t CAacDecoder::RecognizeFormat(CMediaFormat *pFormat)
{
	if (pFormat->mFormatType != MF_AACFormat)
		return E_ERROR;

	// todo - more checks

	return E_OK;
}

IFilter* CAacDecoder::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CAacDecoder *result = avf_new CAacDecoder(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CAacDecoder::CAacDecoder(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpInput(NULL),
	mpOutput(NULL),
#ifdef USE_FDK_AAC
	m_dec_handle(NULL),
	mbConfigured(false)
#else
	mbDecoderOpen(false),
	mp_decoder_config(NULL),
	mp_dec_buffer(NULL)
#endif
{
	mpInput = avf_new CAacDecoderInput(this);
	CHECK_STATUS(mpInput);
	CHECK_OBJ(mpInput);

	mpOutput = avf_new CAacDecoderOutput(this);
	CHECK_STATUS(mpOutput);
	CHECK_OBJ(mpOutput);

#ifdef USE_FDK_AAC
#else
	CREATE_OBJ(mp_decoder_config = avf_malloc_type(struct au_aacdec_config_s);
	::memset(mp_decoder_config, 0, sizeof(*mp_decoder_config));
#endif
}

CAacDecoder::~CAacDecoder()
{
#ifdef USE_FDK_AAC
#else
	__avf_safe_free(mp_decoder_config);
	__avf_safe_free(mp_dec_buffer);
#endif

	__avf_safe_release(mpInput);
	__avf_safe_release(mpOutput);
}

IPin* CAacDecoder::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

IPin* CAacDecoder::GetOutputPin(avf_uint_t index)
{
	return index == 0 ? mpOutput : NULL;
}

#ifndef USE_FDK_AAC
static void convert_i32_to_i16(const avf_u32_t *from, avf_u16_t *to, avf_size_t size)
{
	for (avf_size_t i = size/4; i; i--) {
		to[0] = from[0] >> 16;
		to[1] = from[1] >> 16;
		to[2] = from[2] >> 16;
		to[3] = from[3] >> 16;
		to += 4;
		from += 4;
	}

	for (avf_size_t i = size&3; i; i--) {
		*to = *from >> 16;
		to++;
		from++;
	}
}

static void convert_i32_to_i16_mono(const avf_u32_t *from, avf_u16_t *to, avf_size_t size)
{
	for (avf_size_t i = size/4; i; i--) {
		to[0] = from[0] >> 16;
		to[1] = from[2] >> 16;
		to[2] = from[4] >> 16;
		to[3] = from[6] >> 16;
		to += 4;
		from += 8;
	}

	for (avf_size_t i = size&3; i; i--) {
		*to = *from >> 16;
		to++;
		from += 2;
	}
}
#endif

void CAacDecoder::FilterLoop()
{
	avf_status_t status;
	avf_size_t buffer_size;
	avf_pts_t start_dts = 0;
	int frame_count = 0;

	status = OpenDecoder(&buffer_size);
	if (status != E_OK) {
		ReplyCmd(status);
		return;
	}

#ifdef USE_FDK_AAC
#else
	avf_safe_free(mp_dec_buffer);
	if ((mp_dec_buffer = (uint8_t*)avf_malloc(buffer_size * 2)) == NULL) {
		CloseDecoder();
		PostErrorMsg(E_NOMEM);
		return;
	}
#endif

	ReplyCmd(E_OK);

	while (true) {
		CQueueInputPin *pInPin;
		CBuffer *pInBuffer;

		if (ReceiveInputBuffer(pInPin, pInBuffer) != E_OK)
			break;

		CAacDecoderOutput *pOutPin = mpOutput;
		CBuffer *pOutBuffer;

		if (!AllocOutputBuffer(pOutPin, pOutBuffer)) {
			pInBuffer->Release();
			break;
		}

		switch (pInBuffer->mType) {
		case CBuffer::EOS:
			pInBuffer->Release();

			SetEOS(pOutPin, pOutBuffer);
			pOutPin->PostBuffer(pOutBuffer);

			PostEosMsg();
			goto Done;

		case CBuffer::START_DTS:
			start_dts = pInBuffer->m_dts;
			pInBuffer->Release();
			pOutBuffer->Release();
			break;

		case CBuffer::DATA:
			frame_count++;

			if (pInBuffer->m_pts < start_dts) {
				pInBuffer->Release();
				pOutBuffer->Release();

			} else {

#ifdef USE_FDK_AAC

				avf_u8_t *in_buf = pInBuffer->GetData();
				UINT buf_size = pInBuffer->GetDataSize();
				UINT valid = pInBuffer->GetDataSize();

				if (aacDecoder_Fill(m_dec_handle, &in_buf, &buf_size, &valid) != AAC_DEC_OK) {
					AVF_LOGE("aac dec fill failed");
					//
				}

				if (aacDecoder_DecodeFrame(m_dec_handle, (INT_PCM*)pOutBuffer->mpData,
					pOutBuffer->mSize, 0) != AAC_DEC_OK) {
					AVF_LOGE("aac dec failed");
					//
				}

				pInBuffer->Release();

#else
				mp_decoder_config->dec_rptr = pInBuffer->GetData();
				mp_decoder_config->interBufSize = pInBuffer->GetDataSize();
				mp_decoder_config->dec_wptr = (s32*)mp_dec_buffer;
				aacdec_set_bitstream_rp(mp_decoder_config);
				aacdec_decode(mp_decoder_config);
				//printf("feed: %d %02x %02x %02x %02x\n", pInBuffer->GetDataSize(),
				//	pInBuffer->GetData()[0], pInBuffer->GetData()[1], 
				//	pInBuffer->GetData()[2], pInBuffer->GetData()[3]);
				if (mp_decoder_config->ErrorStatus) {
					AVF_LOGE("Decode failed %d, consumedByte: %d", 
						mp_decoder_config->ErrorStatus, mp_decoder_config->consumedByte);
					PostErrorMsg(E_DEVICE);
					pInBuffer->Release();
					pOutBuffer->Release();
					break;
				}
				//printf("decode OK\n");
				pInBuffer->Release();

				if (mpOutput->mNumChannels == 1) {
					convert_i32_to_i16_mono((avf_u32_t*)mp_dec_buffer, (avf_u16_t*)pOutBuffer->mpData, 
						mpOutput->mSamplesPerChunk);
				} else {
					convert_i32_to_i16((avf_u32_t*)mp_dec_buffer, (avf_u16_t*)pOutBuffer->mpData, 
						mpOutput->mSamplesPerChunk);
				}
#endif
				pOutBuffer->mType = CBuffer::DATA;
				pOutBuffer->mFlags = 0;	// todo
				//pOutBuffer->mSize = mpOutput->mBytesPerChunk;
				pOutBuffer->m_offset = 0;
				pOutBuffer->m_data_size = mpOutput->mBytesPerChunk;
				pOutBuffer->m_dts = 0;
				pOutBuffer->m_pts = 0;	// todo
				pOutBuffer->m_duration = 0;	// todo

				pOutPin->PostBuffer(pOutBuffer);
			}
			break;

		default:
			pInBuffer->Release();
			pOutBuffer->Release();
			break;
		}
	}

Done:
	CloseDecoder();
}

avf_status_t CAacDecoder::OpenDecoder(avf_size_t *buffer_size)
{
#ifdef USE_FDK_AAC

	mbConfigured = false;

	m_dec_handle = aacDecoder_Open(TT_MP4_ADTS, 1);
	if (m_dec_handle == NULL) {
		AVF_LOGE("Cannot open aac dec");
		return E_ERROR;
	}

	if (aacDecoder_SetParam(m_dec_handle, AAC_PCM_OUTPUT_INTERLEAVED, 1) != AAC_DEC_OK) {
		AVF_LOGE("set AAC_PCM_OUTPUT_INTERLEAVED failed");
		return E_ERROR;
	}

	return E_OK;

#else

	if (mbDecoderOpen)
		return E_OK;

	CAudioFormat *pInFormat = static_cast<CAudioFormat*>(mpInput->GetUsingMediaFormat());
	AVF_ASSERT(pInFormat);

	*buffer_size = pInFormat->ChunkSize() * 2;

	mp_decoder_config->externalSamplingRate = pInFormat->mSampleRate;
	mp_decoder_config->bsFormat = ADTS_BSFORMAT;
	mp_decoder_config->srcNumCh = pInFormat->mNumChannels;
	mp_decoder_config->outNumCh = pInFormat->mNumChannels == 1 ? 2 : pInFormat->mNumChannels;
	mp_decoder_config->externalBitrate = 0;
	mp_decoder_config->frameCounter = 0;
	mp_decoder_config->errorCounter = 0;
	mp_decoder_config->interBufSize = 0;
	mp_decoder_config->codec_lib_mem_addr = m_share_mem;
	mp_decoder_config->codec_lib_backup_addr = m_share_mem1;
	mp_decoder_config->codec_lib_backup_size = 0;//sizeof(m_share_mem1);
	mp_decoder_config->consumedByte = 0;

	aacdec_setup(mp_decoder_config);
	aacdec_open(mp_decoder_config);
	if (mp_decoder_config->ErrorStatus) {
		AVF_LOGE("AAC dec init failed %d", mp_decoder_config->ErrorStatus);
		return E_ERROR;
	}

	return E_OK;

#endif
}

void CAacDecoder::CloseDecoder()
{
#ifdef USE_FDK_AAC

	if (m_dec_handle) {
		aacDecoder_Close(m_dec_handle);
		m_dec_handle = NULL;
	}

#else

	if (mbDecoderOpen) {
		aacdec_close();
		mbDecoderOpen = false;
	}

#endif
}

//-----------------------------------------------------------------------
//
//  CAacDecoderInput
//
//-----------------------------------------------------------------------

CAacDecoderInput::CAacDecoderInput(CAacDecoder *pFilter):
	inherited(pFilter, 0, "AAC_decoder_input"),
	mpDecoder(pFilter)
{
}

CAacDecoderInput::~CAacDecoderInput()
{
}

avf_status_t CAacDecoderInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (CAacDecoder::RecognizeFormat(pMediaFormat) != E_OK) {
		return E_ERROR;
	}
	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CAacDecoderOutput
//
//-----------------------------------------------------------------------

CAacDecoderOutput::CAacDecoderOutput(CAacDecoder *pFilter):
	inherited(pFilter, 0, "AAC_decoder_output"),
	mpDecoder(pFilter)
{
}

CAacDecoderOutput::~CAacDecoderOutput()
{
}

CAudioFormat *CAacDecoderOutput::GetInputMediaFormat()
{
	if (!mpDecoder->mpInput->Connected()) {
		return NULL;
	}
	return static_cast<CAudioFormat*>(mpDecoder->mpInput->GetUsingMediaFormat());
}

CMediaFormat* CAacDecoderOutput::QueryMediaFormat()
{
	CAudioFormat *pInFormat = GetInputMediaFormat();
	if (pInFormat == NULL)
		return NULL;

	CAudioFormat *pOutFormat = avf_alloc_media_format(CAudioFormat);
	if (pOutFormat) {
		// CMediaFormat
		pOutFormat->mMediaType = MEDIA_TYPE_AUDIO;
		pOutFormat->mFormatType = MF_RawPcmFormat;
		pOutFormat->mFrameDuration = pInFormat->mFrameDuration;
		pOutFormat->mTimeScale = pInFormat->mTimeScale;

		// CAudioFormat
		pOutFormat->mNumChannels = pInFormat->mNumChannels;
		pOutFormat->mSampleRate = pInFormat->mSampleRate;
		pOutFormat->mBitsPerSample = pInFormat->mBitsPerSample;
		pOutFormat->mBitsFormat = 0;
		pOutFormat->mFramesPerChunk = pInFormat->mFramesPerChunk;
	}

	return pOutFormat;
}

IBufferPool *CAacDecoderOutput::QueryBufferPool()
{
	CAudioFormat *pInFormat = GetInputMediaFormat();
	if (pInFormat == NULL)
		return NULL;

	mSamplesPerChunk = pInFormat->SamplesPerChunk();
	mBytesPerChunk = pInFormat->ChunkSize();
	mNumChannels = pInFormat->mNumChannels;

	IBufferPool *pBufferPool = CMemoryBufferPool::Create("AACDecBP", 
		8, sizeof(CBuffer), mBytesPerChunk);

	return pBufferPool;
}

