
#define LOG_TAG "av_muxer"

#include <time.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"

#include "avio.h"
#include "file_io.h"

#include "avf_config.h"
#include "avf_media_format.h"
#include "av_muxer.h"

//-----------------------------------------------------------------------
//
//  CAVMuxer
//
//-----------------------------------------------------------------------
IFilter* CAVMuxer::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CAVMuxer *result = avf_new CAVMuxer(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CAVMuxer::CAVMuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mbEnableMuxer(false),
	m_sync_counter(0),
	m_sync_counter2(0),
	m_frame_counter(0),
	mb_avsync_error_sent(false),
	mb_enable_avsync(false)
{
	SET_ARRAY_NULL(mpInput);
	mpOutput = NULL;

	if (pFilterConfig) {
		CConfigNode *pAttr = pFilterConfig->FindChild("index");
		mTypeIndex = pAttr ? ::atoi(pAttr->GetValue()) : 0;
	}

	mb_enable_avsync = mpEngine->ReadRegBool(NAME_AVSYNC, mTypeIndex, VALUE_AVSYNC);
	AVF_LOGD(C_GREEN "%s av sync %d" C_NONE, mb_enable_avsync ? "enable" : "disable", mTypeIndex);
}

CAVMuxer::~CAVMuxer()
{
	SAFE_RELEASE_ARRAY(mpInput);
	avf_safe_release(mpOutput);
}

avf_status_t CAVMuxer::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	SetRTPriority(CThread::PRIO_SINK);

	for (avf_uint_t i = 0; i < ARRAY_SIZE(mpInput); i++) {
		mpInput[i] = avf_new CAVMuxerInput(this, i);
		CHECK_STATUS(mpInput[i]);
		if (mpInput[i] == NULL)
			return E_NOMEM;
	}

	mpOutput = avf_new CAVMuxerOutput(this);
	CHECK_STATUS(mpOutput);
	if (mpOutput == NULL)
		return E_NOMEM;

	return E_OK;
}

IPin* CAVMuxer::GetInputPin(avf_uint_t index)
{
	return index < 2 ? mpInput[index] : NULL;
}

IPin* CAVMuxer::GetOutputPin(avf_uint_t index)
{
	return mpOutput;
}

void CAVMuxer::FilterLoop()
{
	int value = mpEngine->ReadRegInt32(NAME_RECORD, mTypeIndex, -1);
	if (value < 0 && mTypeIndex != 0) {
		// not set, use value of stream 0
		value = mpEngine->ReadRegInt32(NAME_RECORD, 0, 0);
	}
	mbEnableMuxer = (value > 0);

	ReplyCmd(E_OK);

	if (mbEnableMuxer) {

		while (true) {
			CAVMuxerInput *pInput;
			CBuffer *pBuffer;

			while (true) {
				pInput = FindPinToReceive();
				
				if (pInput == NULL) {	// All EOS
					AVF_LOGW("no input");
					return;
				}

				CQueueInputPin *pQP = static_cast<CQueueInputPin*>(pInput);
				if (GetPinWithMsg(pQP) != E_OK) {
					return;
				}

				if (pQP != static_cast<CQueueInputPin*>(pInput)) {
					pInput = GetMaxInputPin();
					if (pInput == NULL) {
						AVF_LOGW("no max input");
						return;
					}
				}

				if ((pBuffer = ReceiveBufferFrom(pInput)) != NULL) {
					if (pBuffer->mType == CBuffer::DATA) {
						pInput->UpdateLength(pBuffer);
					}
					break;
				}

				AVF_LOGW("no buffer received");
			}

			if (PostBuffer(pInput, pBuffer)) {
				PostEosMsg();
				return;
			}

		} 

	} else {

		while (true) {
			CQueueInputPin *pQP;
			CBuffer *pBuffer;

			if (ReceiveInputBuffer(pQP, pBuffer) != E_OK) {
				return;
			}

			if (PostBuffer(static_cast<CAVMuxerInput*>(pQP), pBuffer)) {
				PostEosMsg();
				return;
			}
		}
	}
}

CAVMuxerInput *CAVMuxer::FindPinToReceive()
{
	avf_u64_t length_90k_min = -1ULL;
	CAVMuxerInput *pMinInput = NULL;

#ifdef DEBUG_AVSYNC
	avf_u64_t length_90k_max = 0;
	CAVMuxerInput *pMaxInput = NULL;
#endif

	for (avf_uint_t i = 0; i < ARRAY_SIZE(mpInput); i++) {
		CAVMuxerInput *pPin = mpInput[i];
		if (pPin && pPin->Connected()) {
			if (!pPin->GetEOS()) {
				if (length_90k_min > pPin->m_length_90k) {
					length_90k_min = pPin->m_length_90k;
					pMinInput = pPin;
				}
#ifdef DEBUG_AVSYNC
				if (length_90k_max < pPin->m_length_90k) {
					length_90k_max = pPin->m_length_90k;
					pMaxInput = pPin;
				}
#endif
			}
		}
	}

#ifdef DEBUG_AVSYNC
	if (pMinInput && pMaxInput) {
		if (m_sync_counter2-- > 0) {
			m_sync_counter2 = 120;
			avf_u64_t diff = pMaxInput->m_length_90k - pMinInput->m_length_90k;
			int record_ticks = (int)(avf_get_curr_tick() - mpEngine->ReadRegInt64(NAME_CAMER_START_RECORD_TICKS, 0, 0));
			AVF_LOGD("av sync info: " C_GREEN "%s(%d,%d)-%s(%d,%d) = %d" C_NONE,
				pMaxInput->GetMediaTypeName(), pMaxInput->GetNumInputBuffers(),
				(int)(pMaxInput->m_length_90k/90) - record_ticks,
				pMinInput->GetMediaTypeName(), pMinInput->GetNumInputBuffers(),
				(int)(pMinInput->m_length_90k/90) - record_ticks,
				(int)diff);
		}
	}
#endif

	return pMinInput;
}

CAVMuxerInput *CAVMuxer::GetMaxInputPin()
{
	avf_size_t min_buffers = -1U;
	avf_size_t max_buffers = 0;
	CAVMuxerInput *pMinInput = NULL;
	CAVMuxerInput *pMaxInput = NULL;

	for (avf_uint_t i = 0; i < ARRAY_SIZE(mpInput); i++) {
		CAVMuxerInput *pPin = mpInput[i];
		if (pPin && pPin->Connected() && !pPin->GetEOS()) {
			avf_size_t num_buffers = pPin->GetNumInputBuffers();
			if (min_buffers > num_buffers) {
				min_buffers = num_buffers;
				pMinInput = pPin;
			}
			if (max_buffers < num_buffers) {
				max_buffers = num_buffers;
				pMaxInput = pPin;
			}
		}
	}

	if (max_buffers >= 30 + min_buffers) {
		if (++m_sync_counter >= 100) {
			m_sync_counter = 0;
			AVF_LOGI(C_LIGHT_PURPLE "stream %d av not sync, diff: " LLD C_NONE,
				mTypeIndex, pMaxInput->m_length_90k - pMinInput->m_length_90k);
			AVF_LOGI("%s(%d): %d ms, %s(%d): %d ms, system: %d ms",
				pMaxInput->GetMediaTypeName(), max_buffers, (int)(pMaxInput->m_length_90k/90),
				pMinInput->GetMediaTypeName(), min_buffers, (int)(pMinInput->m_length_90k/90),
				(int)(avf_get_curr_tick() - mpEngine->ReadRegInt64(NAME_CAMER_START_RECORD_TICKS, 0, 0)));
		}
		if (pMaxInput->m_length_90k > pMinInput->m_length_90k + _90kHZ * 5) {
			if (!mb_avsync_error_sent) {
				mb_avsync_error_sent = true;
				AVF_LOGE("av diff too large: " LLD " ms, send E_DEVICE",
					(pMaxInput->m_length_90k - pMinInput->m_length_90k) / 90);
				PostErrorMsg(E_DEVICE);
			}
		}
	}

	return pMaxInput;
}

bool CAVMuxer::PostBuffer(CAVMuxerInput *pInput, CBuffer *pBuffer)
{
	if (pBuffer->IsEOS()) {
		pInput->SetEOS();

		CBuffer *pOutBuffer;
		bool rval = AllocOutputBuffer(mpOutput, pOutBuffer);
		AVF_ASSERT(rval);

		pOutBuffer->SetStreamEOS();
		pOutBuffer->mpFormat = pInput->GetUsingMediaFormat();
		mpOutput->PostBuffer(pOutBuffer);

		if (AllEOSReceived()) {
			mpOutput->PostBuffer(pBuffer);
			return true;
		} else {
			pBuffer->Release();
		}
	} else {
		mpOutput->PostBuffer(pBuffer);
	}

	return false;
}

bool CAVMuxer::AllEOSReceived()
{
	for (avf_uint_t i = 0; i < ARRAY_SIZE(mpInput); i++) {
		CAVMuxerInput *pPin = mpInput[i];
		if (pPin && pPin->Connected()) {
			if (!pPin->GetEOS())
				return false;
		}
	}
	return true;
}

static void free_es_format(CMediaFormat *self)
{
	CESFormat *pMediaFormat = (CESFormat*)self;
	for (avf_uint_t i = 0; i < pMediaFormat->es_formats.count; i++) {
		CMediaFormat *format = *pMediaFormat->es_formats.Item(i);
		format->Release();
	}
	pMediaFormat->es_formats._Release();
	pMediaFormat->DoRelease_p(self);
}

CMediaFormat* CAVMuxer::QueryMediaFormat()
{
	CESFormat *pMediaFormat = avf_alloc_media_format(CESFormat);
	if (pMediaFormat == NULL)
		return NULL;

	// CMediaFormat
	pMediaFormat->mMediaType = MEDIA_TYPE_AVES;
	pMediaFormat->mFormatType = MF_AVES;
	pMediaFormat->mFrameDuration = 0;
	pMediaFormat->mTimeScale = _90kHZ;

	// CESFormat
	::memset(&pMediaFormat->stream_attr, 0, sizeof(pMediaFormat->stream_attr));
	pMediaFormat->stream_attr.stream_version = CURRENT_STREAM_VERSION;
	pMediaFormat->stream_attr.video_version = 0;
	pMediaFormat->stream_attr.audio_version = 0;
	pMediaFormat->stream_attr.extra_size = 0;
	pMediaFormat->es_formats._Init();

	pMediaFormat->DoRelease_p = pMediaFormat->DoRelease;
	pMediaFormat->DoRelease = free_es_format;

	for (avf_uint_t i = 0; i < ARRAY_SIZE(mpInput); i++) {
		CAVMuxerInput *input = mpInput[i];
		if (input && input->Connected()) {
			CMediaFormat *format = input->GetUsingMediaFormat();
			if (format) {
				switch (format->mFormatType) {
				case MF_H264VideoFormat:
					if (pMediaFormat->stream_attr.video_coding != VideoCoding_Unknown) {
						AVF_LOGW("ES already has video");
					} else {
						CH264VideoFormat *pFormat = (CH264VideoFormat*)format;

						pMediaFormat->stream_attr.video_coding = VideoCoding_AVC;
						pMediaFormat->stream_attr.video_framerate = pFormat->mFrameRate;
						pMediaFormat->stream_attr.video_width = pFormat->mWidth;
						pMediaFormat->stream_attr.video_height = pFormat->mHeight;

						format->AddRef();
						pMediaFormat->es_formats._Append(&format);

						AVF_LOGD("video: coding=%d, framerate=%d, width=%d, height=%d",
							pMediaFormat->stream_attr.video_coding,
							pMediaFormat->stream_attr.video_framerate,
							pMediaFormat->stream_attr.video_width,
							pMediaFormat->stream_attr.video_height);
					}
					break;

				case MF_AACFormat:
					if (pMediaFormat->stream_attr.audio_coding != AudioCoding_Unknown) {
						AVF_LOGW("ES already has audio");
					} else {
						CAacFormat *pFormat = (CAacFormat*)format;

						pMediaFormat->stream_attr.audio_coding = AudioCoding_AAC;
						pMediaFormat->stream_attr.audio_num_channels = pFormat->mNumChannels;
						pMediaFormat->stream_attr.audio_sampling_freq = pFormat->mSampleRate;

						format->AddRef();
						pMediaFormat->es_formats._Append(&format);

						AVF_LOGD(C_WHITE "audio: coding=%d, channels=%d, sample_freq=%d" C_NONE,
							pMediaFormat->stream_attr.audio_coding,
							pMediaFormat->stream_attr.audio_num_channels,
							pMediaFormat->stream_attr.audio_sampling_freq);
					}
					break;

				default:
					AVF_LOGD("format_type ignored: " C_LIGHT_CYAN "%c%c%c%c" C_NONE, FCC_CHARS(format->mFormatType));
					break;
				}
			}
		}
	}

	return pMediaFormat;
}

//-----------------------------------------------------------------------
//
//  CAVMuxerInput
//
//-----------------------------------------------------------------------
CAVMuxerInput::CAVMuxerInput(CAVMuxer* pFilter, avf_uint_t index):
	inherited(pFilter, index, "AVMuxerInput", 64)
{
	mbCheckFull = pFilter->mb_enable_avsync;
}

CAVMuxerInput::~CAVMuxerInput()
{
}

avf_status_t CAVMuxerInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (pMediaFormat->mFormatType != MF_H264VideoFormat &&
		pMediaFormat->mFormatType != MF_AACFormat &&
		pMediaFormat->mFormatType != MF_Mp3Format &&
		pMediaFormat->mFormatType != MF_SubtitleFormat &&
		pMediaFormat->mFormatType != MF_NullFormat &&
		pMediaFormat->mFormatType != MF_MJPEGVideoFormat) {
		AVF_LOGE("cannot accept this media '%c%c%c%c'", FCC_CHARS(pMediaFormat->mFormatType));
		return E_INVAL;
	}
	return E_OK;
}

void CAVMuxerInput::UpdateLength(CBuffer *pBuffer)
{
	CMediaFormat *pFormat = pBuffer->mpFormat;
	AVF_ASSERT(pFormat);

	avf_u64_t end_pts_90k;

	if (!mbStarted) {
		mbStarted = true;
		m_start_pts = pBuffer->m_pts;
		end_pts_90k = pBuffer->m_duration;
	} else {
		end_pts_90k = (pBuffer->m_pts - m_start_pts) + pBuffer->m_duration;
	}

	if (pFormat->mTimeScale != _90kHZ) {
		end_pts_90k = end_pts_90k * _90kHZ / pFormat->mTimeScale;
	}

	m_length_90k = end_pts_90k;

	// for debug
	m_buffer_pts = pBuffer->m_pts;
	m_buffer_len = pBuffer->m_duration;
}


//-----------------------------------------------------------------------
//
//  CAVMuxerOutput
//
//-----------------------------------------------------------------------
CAVMuxerOutput::CAVMuxerOutput(CAVMuxer *pFilter):
	inherited(pFilter, 0, "AVMuxerOutput"),
	mpBufferPool(NULL)
{
	CREATE_OBJ(mpBufferPool = CDynamicBufferPool::CreateNonBlock("AVMuxerOutput", NULL, 4));
}

CAVMuxerOutput::~CAVMuxerOutput()
{
	avf_safe_release(mpBufferPool);
}

CMediaFormat* CAVMuxerOutput::QueryMediaFormat()
{
	CAVMuxer *pMuxer = static_cast<CAVMuxer*>(mpFilter);
	return pMuxer->QueryMediaFormat();
}

IBufferPool* CAVMuxerOutput::QueryBufferPool()
{
	mpBufferPool->AddRef();
	return mpBufferPool;
}

