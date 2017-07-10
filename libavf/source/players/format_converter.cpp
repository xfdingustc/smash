
#define LOG_TAG "converter"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "filter_if.h"
#include "avf_config.h"
#include "mem_stream.h"

#include "avio.h"
#include "http_io.h"
#include "file_io.h"
#include "filter_list.h"

#include "base_player.h"
#include "file_playback.h"
#include "format_converter.h"

//-----------------------------------------------------------------------
//
//  CFormatConverter
//
//-----------------------------------------------------------------------
CFormatConverter* CFormatConverter::Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig)
{
	CFormatConverter *result = avf_new CFormatConverter(pEngine, mediaSrc, conf, pConfig);
	CHECK_STATUS(result);
	return result;
}

CFormatConverter::CFormatConverter(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig):
	inherited(CMP_ID_FMT_CONVERTER, pEngine, mediaSrc, conf, pConfig),
	mpMediaSource(NULL)
{
}

CFormatConverter::~CFormatConverter()
{
}

avf_status_t CFormatConverter::OpenMedia(bool *pbDone)
{
	avf_status_t status;

	AVF_ASSERT(mpMediaSource == NULL); 

	if ((status = LoadAllFilters()) != E_OK)
		return status;

	CConfigNode *node = mpConfig->FindChild("source");
	if (node == NULL) {
		AVF_LOGE("source not specified");
		return E_ERROR;
	}

	const char *name = node->GetValue();
	IFilter *pFilter = mFilterGraph.FindFilterByName(name);
	if (pFilter == NULL) {
		AVF_LOGE("source filter '%s' not found", name);
		return E_ERROR;
	}

	mpMediaSource = IMediaSource::GetInterfaceFrom(pFilter);
	if (mpMediaSource == NULL) {
		AVF_LOGE("'%s' is not a source filter", name);
		return E_ERROR;
	}

	IMediaFileReader *reader = NULL;
	IAVIO *io = CFileIO::Create();

	status = io->OpenRead(mMediaSrc->string());
	if (status != E_OK) {
		AVF_LOGE("cannot open %s", mMediaSrc->string());
		goto Done;
	}

	reader = avf_create_media_file_reader_by_io(mpEngine, io);
	if (reader == NULL) {
		status = E_UNKNOWN;
		goto Done;
	}

	if ((status = mpMediaSource->OpenMediaSource(reader)) != E_OK) {
		AVF_LOGE("OpenMediaSource failed");
		goto Done;
	}

	reader->SetReportProgress(true);

Done:
	avf_safe_release(reader);
	avf_safe_release(io);
	return status;
}

avf_status_t CFormatConverter::PrepareMedia(bool *pbDone)
{
	return RenderAllFilters(true);
}

avf_status_t CFormatConverter::RunMedia(bool *pbDone)
{
	avf_status_t status;

	//mFilterGraph.ClearAllFiltersFlag(CFilterGraph::FF_EOS | CFilterGraph::FF_PREPARED);

	if ((status = mFilterGraph.RunAllFilters()) != E_OK) {
		goto Error;
	}

	*pbDone = true;
	return E_OK;

Error:
	ErrorStop();
	return status;
}

void CFormatConverter::ProcessFilterMsg(const avf_msg_t& msg)
{
	switch (msg.code) {
	case IEngine::MSG_EOS: {
			IFilter *pFilter = (IFilter*)msg.p0;
			mFilterGraph.SetFilterFlag(pFilter, CFilterGraph::FF_EOS);
			if (mFilterGraph.TestAllFiltersFlag(CFilterGraph::FF_EOS)) {
				mFilterGraph.StopAllFilters();
				PostPlayerMsg(IEngine::EV_STOP_MEDIA_DONE, 0, 0);
				AVF_LOGI("format converter done");
			}
		}
		break;

	default:
		return inherited::ProcessFilterMsg(msg);
	}
}


