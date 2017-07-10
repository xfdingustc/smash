
#define LOG_TAG "mp3decoder"

#include <stdlib.h>
#include <stdio.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_media_format.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "mp3_decoder.h"

//-----------------------------------------------------------------------
//
//  CMp3Decoder
//
//-----------------------------------------------------------------------

avf_status_t CMp3Decoder::RecognizeFormat(CMediaFormat *pFormat)
{
	if (pFormat->mFormatType != MF_Mp3Format)
		return E_ERROR;

	// todo - more checks

	return E_OK;
}

IFilter* CMp3Decoder::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CMp3Decoder *result = avf_new CMp3Decoder(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CMp3Decoder::CMp3Decoder(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpInput(NULL),
	mpOutput(NULL)
{
	mpInput = avf_new CMp3DecoderInput(this);
	CHECK_STATUS(mpInput);
	CHECK_OBJ(mpInput);

	mpOutput = avf_new CMp3DecoderOutput(this);
	CHECK_STATUS(mpOutput);
	CHECK_OBJ(mpOutput);
}

CMp3Decoder::~CMp3Decoder()
{
	__avf_safe_release(mpInput);
	__avf_safe_release(mpOutput);
}

IPin* CMp3Decoder::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

IPin* CMp3Decoder::GetOutputPin(avf_uint_t index)
{
	return index == 0 ? mpOutput : NULL;
}

struct ffmpeg_decoder_s {
	AVCodec *codec;
	AVCodecContext *cc;
	avf_u8_t *inbuf;
	AVPacket packet;
	AVFrame *decoded_frame;

	ffmpeg_decoder_s() {
		codec = NULL;
		cc = NULL;
		inbuf = NULL;
		decoded_frame = NULL;
	}

	bool Init();
	void Release();
};

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

bool ffmpeg_decoder_s::Init()
{
	// moved to avf_module_init
	// avcodec_register_all();

	av_init_packet(&packet);

	codec = avcodec_find_decoder(AV_CODEC_ID_MP3);
	if (codec == NULL) {
		AVF_LOGE("could not find mp3 decoder");
		return false;
	}

	cc = avcodec_alloc_context3(codec);
	if (cc == NULL) {
		AVF_LOGE("could not alloc mp3 decoder context");
		return false;
	}
	cc->sample_fmt = AV_SAMPLE_FMT_S16;

	decoded_frame = av_frame_alloc();
	if (decoded_frame == NULL) {
		AVF_LOGE("could not alloc frame");
		return false;
	}

	if (avcodec_open2(cc, codec, NULL) < 0) {
		AVF_LOGE("could not open mp3 decoder");
		return false;
	}

	inbuf = avf_malloc_bytes(AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
	if (inbuf == NULL) {
		return false;
	}
	::memset(inbuf + AUDIO_INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);

	return true;
}

void ffmpeg_decoder_s::Release()
{
	if (cc) {
		avcodec_close(cc);
		av_free(cc);
	}
	av_frame_free(&decoded_frame);
	avf_safe_free(inbuf);
}

// return false: cancelled
bool CMp3Decoder::GetInput(ffmpeg_decoder_s *decoder)
{
	while (true) {
		CQueueInputPin *pInPin;
		if (ReceiveInputBuffer(pInPin, mpInBuffer) != E_OK) {
			return false;
		}

		switch (mpInBuffer->mType) {
		case CBuffer::DATA:
			mpInDataPtr = mpInBuffer->GetData();
			mInDataRemain = mpInBuffer->GetDataSize();
			return true;

		case CBuffer::EOS:
			avf_safe_release(mpInBuffer);
			mpInDataPtr = NULL;
			mInDataRemain = 0;
			mbEOS = true;
			return true;

		default:
			avf_safe_release(mpInBuffer);
			break;
		}
	}
}

// return false: cancelled
bool CMp3Decoder::RefillData(ffmpeg_decoder_s *decoder)
{
	AVPacket *packet = &decoder->packet;

	if (packet->size >= AUDIO_REFILL_THRESH)
		return true;

	if (packet->size > 0) {
		::memmove(decoder->inbuf, packet->data, packet->size);
		packet->data = decoder->inbuf;
	}

	while (true) {
		if (mbEOS)
			return true;

		if (mInDataRemain == 0) {
			if (!GetInput(decoder))
				return false;
			if (mbEOS)
				return true;
		}

		int toread = mInDataRemain;
		int room = AUDIO_INBUF_SIZE - packet->size;
		if (toread > room)
			toread = room;

		::memcpy(packet->data + packet->size, mpInDataPtr, toread);
		packet->size += toread;

		mpInDataPtr += toread;
		mInDataRemain -= toread;
		if (mInDataRemain == 0) {
			mpInDataPtr = NULL;
			avf_safe_release(mpInBuffer);
		}

		if (packet->size >= AUDIO_REFILL_THRESH)
			break;
	}

	return true;
}

// return true: got frame
bool CMp3Decoder::DecodeData(ffmpeg_decoder_s *decoder, int *error)
{
	AVPacket *packet = &decoder->packet;

	if (packet->size == 0)
		return false;

	int got_frame = 0;
	int len = avcodec_decode_audio4(decoder->cc, decoder->decoded_frame, &got_frame, packet);
	if (len < 0) {
		AVF_LOGE("avcodec_decode_audio4 returns %d", len);
		*error = 1;
		return true;
	}

	packet->data += len;
	packet->size -= len;
	packet->dts = AV_NOPTS_VALUE;
	packet->pts = AV_NOPTS_VALUE;

	return got_frame != 0;
}

void CMp3Decoder::FilterLoop()
{
	ffmpeg_decoder_s decoder;

	if (!decoder.Init()) {
		decoder.Release();
		ReplyCmd(E_ERROR);
		return;
	}

	ReplyCmd(E_OK);

	mpInBuffer = NULL;
	mpInDataPtr = 0;
	mInDataRemain = 0;
	mbEOS = false;

	decoder.packet.data = decoder.inbuf;
	decoder.packet.size = 0;

	while (true) {
		if (!RefillData(&decoder))
			break;

		int error = 0;
		if (DecodeData(&decoder, &error)) {
			if (error) {
				AVF_LOGE("mp3 decoder error");
				PostErrorMsg(E_ERROR);
				break;
			}

			int data_size = av_samples_get_buffer_size(NULL,
				decoder.cc->channels, decoder.decoded_frame->nb_samples,
				decoder.cc->sample_fmt, 1);
			//printf("sample_fmt: %d\n", decoder.cc->sample_fmt);

			if (decoder.cc->sample_fmt != AV_SAMPLE_FMT_S16P) {
				AVF_LOGE("sample format is unexpected: %d", decoder.cc->sample_fmt);
				PostErrorMsg(E_ERROR);
				break;
			}

			CBuffer *pOutBuffer;
			if (!AllocOutputBuffer(mpOutput, pOutBuffer))
				break;

			if (pOutBuffer->AllocMem(data_size) != E_OK) {
				pOutBuffer->Release();
				AVF_LOGE("mp3 decoder cannot alloc %d bytes", data_size);
				PostErrorMsg(E_ERROR);
				break;
			}

			int n = data_size / 4;
			avf_u16_t *p0 = (avf_u16_t*)decoder.decoded_frame->data[0];
			avf_u16_t *p1 = (avf_u16_t*)decoder.decoded_frame->data[1];
			avf_u16_t *p = (avf_u16_t*)pOutBuffer->GetMem();
			for (; n > 0; n--) {
				p[0] = *p0++;
				p[1] = *p1++;
				p += 2;
			}

			//::memcpy(pOutBuffer->GetMem(), decoder.decoded_frame->data[0], data_size);

			pOutBuffer->mType = CBuffer::DATA;
			pOutBuffer->mFlags = 0;
			pOutBuffer->m_offset = 0;
			pOutBuffer->m_data_size = data_size;
			pOutBuffer->m_dts = 0;
			pOutBuffer->m_pts = 0;
			pOutBuffer->m_duration = 0;
			pOutBuffer->m_timescale = 0;

			CMediaFormat *pFormat = mpOutput->GetMediaFormat();
			if (pFormat) {
				pOutBuffer->m_duration = pFormat->mFrameDuration;
				pOutBuffer->m_timescale = pFormat->mTimeScale;
			}

			mpOutput->PostBuffer(pOutBuffer);

		} else {

			if (mbEOS) {
				GenerateEOS(mpOutput);
				break;
			}
		}
	}

	decoder.Release();
	avf_safe_release(mpInBuffer);
}

//-----------------------------------------------------------------------
//
//  CMp3DecoderInput
//
//-----------------------------------------------------------------------
CMp3DecoderInput::CMp3DecoderInput(CMp3Decoder *pFilter):
	inherited(pFilter, 0, "mp3_decoder_input"),
	mpDecoder(pFilter)
{
}

CMp3DecoderInput::~CMp3DecoderInput()
{
}

avf_status_t CMp3DecoderInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (CMp3Decoder::RecognizeFormat(pMediaFormat) != E_OK) {
		return E_ERROR;
	}
	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CMp3DecoderOutput
//
//-----------------------------------------------------------------------
CMp3DecoderOutput::CMp3DecoderOutput(CMp3Decoder *pFilter):
	inherited(pFilter, 0, "mp3_decoder_output"),
	mpDecoder(pFilter)
{
}

CMp3DecoderOutput::~CMp3DecoderOutput()
{
}

CAudioFormat *CMp3DecoderOutput::GetInputMediaFormat()
{
	if (!mpDecoder->mpInput->Connected()) {
		return NULL;
	}
	return static_cast<CAudioFormat*>(mpDecoder->mpInput->GetMediaFormat());
}

CMediaFormat* CMp3DecoderOutput::QueryMediaFormat()
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

IBufferPool *CMp3DecoderOutput::QueryBufferPool()
{
//	CAudioFormat *pInFormat = GetInputMediaFormat();
//	if (pInFormat == NULL)
//		return NULL;

//	mSamplesPerChunk = pInFormat->SamplesPerChunk();
//	mBytesPerChunk = pInFormat->ChunkSize();
//	mNumChannels = pInFormat->mNumChannels;

//	IBufferPool *pBufferPool = CMemoryBufferPool::Create("Mp3DecBP", 
//		8, sizeof(CBuffer), mBytesPerChunk);

	IBufferPool *pBufferPool = CDynamicBufferPool::CreateBlock("Mp3DecBP", NULL, 8);
	return pBufferPool;
}

