
#define LOG_TAG "raw_muxer"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avio.h"
#include "buffer_io.h"

#include "avf_media_format.h"
#include "raw_muxer.h"

//-----------------------------------------------------------------------
//
//  CRawMuxer
//
//-----------------------------------------------------------------------
IFilter* CRawMuxer::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CRawMuxer *result = avf_new CRawMuxer(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CRawMuxer::CRawMuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpInput(NULL),
	mpIO(NULL),
	mFileName()
{
}

CRawMuxer::~CRawMuxer()
{
	avf_safe_release(mpIO);
	avf_safe_release(mpInput);
}

avf_status_t CRawMuxer::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	mpInput = avf_new CRawMuxerInput(this);
	CHECK_STATUS(mpInput);
	if (mpInput == NULL)
		return E_NOMEM;

	if ((mpIO = CBufferIO::Create()) == NULL)
		return E_NOMEM;

	char buffer[REG_STR_MAX];
	mpEngine->ReadRegString(NAME_MUXER_FILE, 0, buffer, VALUE_MUXER_FILE);
	mFileName = buffer;

	return E_OK;
}

IPin* CRawMuxer::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

void CRawMuxer::FilterLoop()
{
	avf_status_t status;

	status = mpIO->CreateFile(mFileName->string());
	if (status != E_OK) {
		ReplyCmd(status);
		return;
	}

	ReplyCmd(E_OK);

	while (true) {
		CQueueInputPin *pPin;
		CBuffer *pBuffer;

		if (ReceiveInputBuffer(pPin, pBuffer) != E_OK)
			break;

		switch (pBuffer->mType) {
		case CBuffer::DATA:
			mpIO->Write(pBuffer->GetData(), pBuffer->mSize);
			break;

		case CBuffer::EOS:
			pBuffer->Release();
			PostEosMsg();
			goto Done;

		default:
			break;
		}

		pBuffer->Release();
	}

Done:
	AVF_LOGI("File saved to %s", mFileName->string());
	mpIO->Close();
}

//-----------------------------------------------------------------------
//
//  CRawMuxerInput
//
//-----------------------------------------------------------------------
CRawMuxerInput::CRawMuxerInput(CRawMuxer *pFilter):
	inherited(pFilter, 0, "FileMuxerInput")
{
}

CRawMuxerInput::~CRawMuxerInput()
{
}

avf_status_t CRawMuxerInput::AcceptMedia(CMediaFormat *pMediaFormat, IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	return E_OK;
}


