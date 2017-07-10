
#define LOG_TAG "test_sink"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_media_format.h"
#include "test_sink.h"

//-----------------------------------------------------------------------
//
//  CTestSink
//
//-----------------------------------------------------------------------

IFilter* CTestSink::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CTestSink *result = avf_new CTestSink(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CTestSink::CTestSink(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpPin0(NULL),
	mpPin1(NULL)
{
	mpPin0 = avf_new CTestInput(this, 0);
	CHECK_STATUS(mpPin0);
	CHECK_OBJ(mpPin0);

	mpPin1 = avf_new CTestInput(this, 1);
	CHECK_STATUS(mpPin1);
	CHECK_OBJ(mpPin1);
}

CTestSink::~CTestSink()
{
	avf_delete mpPin0;
	avf_delete mpPin1;
}

IPin* CTestSink::GetInputPin(avf_uint_t index)
{
	if (index == 0)
		return mpPin0;
	if (index == 1)
		return mpPin1;
	return NULL;
}

void CTestSink::FilterLoop()
{
	ReplyCmd(E_OK);

	while (true) {
		CQueueInputPin *pPin;
		CBuffer *pBuffer;

		if (ReceiveInputBuffer(pPin, pBuffer) != E_OK)
			break;

		CTestInput *pInputPin = static_cast<CTestInput*>(pPin);

		AVF_LOGI("Received buffer from pin %d", pInputPin->mPinIndex);
		pBuffer->Release();
	}
}

//-----------------------------------------------------------------------
//
//  CTestInput
//
//-----------------------------------------------------------------------
CTestInput::CTestInput(CTestSink *pFilter, avf_uint_t index):
	inherited(pFilter, index, "TestInputPin")
{
}

CTestInput::~CTestInput()
{
}

avf_status_t CTestInput::AcceptMedia(CMediaFormat *pMediaFormat, IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (mPinIndex == 0) {
		if (pMediaFormat->mMediaType == MEDIA_TYPE_VIDEO &&
			pMediaFormat->mFormatType == MF_TestVideoFormat) {
			return E_OK;
		}
		return E_UNKNOWN;
	}

	if (mPinIndex == 1) {
		if (pMediaFormat->mMediaType == MEDIA_TYPE_AUDIO) {
			return E_OK;
		}
	}

	return E_UNKNOWN;
}

