
#define LOG_TAG "test_src"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_media_format.h"
#include "test_src.h"

//-----------------------------------------------------------------------
//
//  CTestSrc
//
//-----------------------------------------------------------------------

IFilter* CTestSrc::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CTestSrc *result = avf_new CTestSrc(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CTestSrc::CTestSrc(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpPin0(NULL),
	mpPin1(NULL)
{
}

CTestSrc::~CTestSrc()
{
	avf_delete mpPin0;
	avf_delete mpPin1;
}

avf_status_t CTestSrc::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	mpPin0 = avf_new CTestOutput(this, 0);
	CHECK_STATUS(mpPin0);

	mpPin1 = avf_new CTestOutput(this, 1);
	CHECK_STATUS(mpPin1);

	if (mpPin0 == NULL || mpPin1 == NULL)
		return E_NOMEM;

	return E_OK;
}

IPin* CTestSrc::GetOutputPin(avf_uint_t index) {
	if (index == 0)
		return static_cast<IPin*>(mpPin0);
	if (index == 1)
		return static_cast<IPin*>(mpPin1);
	return NULL;
}

void CTestSrc::FilterLoop()
{
	ReplyCmd(E_OK);

	while (true) {
		CBuffer *pBuffer;

		if (mpPin0->Connected()) {

			if (!AllocOutputBuffer(mpPin0, pBuffer))
				break;

			// do something with the buffer

			mpPin0->PostBuffer(pBuffer);
		}

		if (mpPin1->Connected()) {

			if (!AllocOutputBuffer(mpPin1, pBuffer))
				break;

			// do something with the buffer

			mpPin1->PostBuffer(pBuffer);
		}

		sleep(1);
	}
}

//-----------------------------------------------------------------------
//
//  CTestOutput
//
//-----------------------------------------------------------------------
CTestOutput::CTestOutput(CTestSrc *pFilter, avf_uint_t index):
	inherited(static_cast<CFilter*>(pFilter), index, "TestOutput")
{
}

CTestOutput::~CTestOutput()
{
}

CMediaFormat* CTestOutput::QueryMediaFormat()
{
	if (mPinIndex == 0) {
		CTestVideoFormat *pFormat = avf_alloc_media_format(CTestVideoFormat);
		pFormat->mMediaType = MEDIA_TYPE_VIDEO;
		pFormat->mFormatType = MF_TestVideoFormat;
		return pFormat;
	}

	if (mPinIndex == 1) {
		CAudioFormat *pFormat = avf_alloc_media_format(CAudioFormat);
		pFormat->mMediaType = MEDIA_TYPE_AUDIO;
		pFormat->mFormatType = MF_RawPcmFormat;
		return pFormat;
	}

	return NULL;
}

IBufferPool* CTestOutput::QueryBufferPool()
{
	CMemoryBufferPool *pBufferPool = CMemoryBufferPool::Create(
		mName->string(), 5, sizeof(CBuffer), 64);
	return pBufferPool;
}

