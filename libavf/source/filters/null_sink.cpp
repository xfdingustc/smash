
#define LOG_TAG "null_sink"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_media_format.h"
#include "null_sink.h"

//-----------------------------------------------------------------------
//
//  CNullSink
//
//-----------------------------------------------------------------------
IFilter* CNullSink::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CNullSink *result = avf_new CNullSink(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CNullSink::CNullSink(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpInput(NULL)
{
}

CNullSink::~CNullSink()
{
	avf_safe_release(mpInput);
}

avf_status_t CNullSink::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;
	mpInput = avf_new CNullSinkInput(this);
	CHECK_STATUS(mpInput);
	if (mpInput == NULL)
		return E_NOMEM;
	return E_OK;
}

IPin *CNullSink::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

//-----------------------------------------------------------------------
//
//  CNullSinkInput
//
//-----------------------------------------------------------------------

CNullSinkInput::CNullSinkInput(CNullSink *pFilter):
	inherited(pFilter, 0, "NullInput")
{
}

CNullSinkInput::~CNullSinkInput()
{
}

void CNullSinkInput::ReceiveBuffer(CBuffer *pBuffer)
{
	IncBuffersReceived();
	if (pBuffer->mType == CBuffer::EOS) {
		mpFilter->PostEosMsg();
	}
}

