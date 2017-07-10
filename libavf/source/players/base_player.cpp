
#define LOG_TAG "base_player"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_config.h"
#include "mem_stream.h"

#include "filter_list.h"
#include "base_player.h"

//-----------------------------------------------------------------------
//
//  CBasePlayer
//
//-----------------------------------------------------------------------
CBasePlayer* CBasePlayer::Create(int id, IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig)
{
	CBasePlayer *result = avf_new CBasePlayer(id, pEngine, mediaSrc, conf, pConfig);
	CHECK_STATUS(result);
	return result;
}

CBasePlayer::CBasePlayer(int id, IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig):
	m_id(id),
	mpEngine(pEngine),
	mMediaSrc(mediaSrc),
	mpConfig(NULL),
	mState(IMediaControl::STATE_IDLE),
	mConf(conf)
{
	pConfig->AddRef();
	mpConfig = pConfig;
}

CBasePlayer::~CBasePlayer()
{
	avf_safe_release(mpConfig);
}

void *CBasePlayer::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMediaPlayer)
		return static_cast<IMediaPlayer*>(this);
	return inherited::GetInterface(refiid);
}

avf_status_t CBasePlayer::OpenMedia(bool *pbDone)
{
	avf_status_t status;

	if ((status = CreateGraph()) != E_OK) {
		mFilterGraph.ClearGraph();
		return status;
	}

	return E_OK;
}

IFilter *CBasePlayer::CreateFilter(const char *pName, const char *pType, CConfigNode *pFilterConfig)
{
	avf_status_t status;

	// AudioSource; - create AudioSource
	// FileMuxer1{type=FileMuxer; index=0; } - create FileMuxer named FileMuxer1

	if (pType == NULL) {
		CConfigNode *pTypeAttr = pFilterConfig->FindChild("type");
		pType = pTypeAttr ? pTypeAttr->GetValue() : pFilterConfig->GetName();
	}

	if (pName == NULL) {
		pName = pFilterConfig->GetName();
	}

	// create filter
	IFilter *pFilter = avf_create_filter_by_name(pType, pName, mpEngine, pFilterConfig);
	if (pFilter == NULL) {
		AVF_LOGE("cannot create filter: %s", pType);
		return NULL;
	}

	status = mFilterGraph.AddFilter(pFilter);
	if (status != E_OK) {
		pFilter->Release();
		return NULL;
	}

	// init the filter
	status = pFilter->InitFilter();
	if (status != E_OK)
		return NULL;

	OnFilterCreated(pFilter);

	return pFilter;
}

avf_status_t CBasePlayer::CreateConnection(CConfigNode *pConnection)
{
	avf_status_t status;

	IFilter *pSrcFilter = mFilterGraph.FindFilterByName(pConnection->GetName());
	if (pSrcFilter == NULL) {
		AVF_LOGE("connection src %s not found", pConnection->GetName());
		return E_ERROR;
	}

	int outIndex = pConnection->GetIndex();

	CConfigNode *pPeerAttr = pConnection->FindChild("peer");
	if (pPeerAttr == NULL) {
		AVF_LOGE("no peer specified");
		return E_ERROR;
	}

	IFilter *pPeerFilter = mFilterGraph.FindFilterByName(pPeerAttr->GetValue());
	if (pPeerFilter == NULL) {
		AVF_LOGE("filter %s not found", pPeerAttr->GetValue());
		return E_ERROR;
	}

	int inIndex = 0;
	CConfigNode *pInAttr = pConnection->FindChild("in");
	if (pInAttr) {
		inIndex = ::atoi(pInAttr->GetValue());
	}

	// connect
	status = mFilterGraph.Connect(pSrcFilter, outIndex, pPeerFilter, inIndex);
	if (status != E_OK) {
		//return status;
	}

	return E_OK;
}

IFilter *CBasePlayer::CreateDownStreamFilter(CConfigNode *connections, bool bUseExisting,
	IFilter *pFilter, IPin *pPin, avf_size_t outIndex, avf_size_t &inIndex, bool& bNew)
{
	const char *pFilterName = pFilter->GetFilterName();

	// check if a connection is specified in configs
	if (connections) {
		CConfigNode *pc = connections->GetFirst();
		for (; pc; pc = pc->GetNext()) {
			if (::strcmp(pFilterName, pc->GetName()) == 0) {
				avf_size_t oi = ::atoi(pc->GetValue());
				if (oi != outIndex)
					continue;

				CConfigNode *pPeerAttr = pc->FindChild("peer");
				if (pPeerAttr == NULL) {
					AVF_LOGE("no peer specified");
					return NULL;
				}

				CConfigNode *pInAttr = pc->FindChild("in");
				if (pInAttr == NULL) {
					AVF_LOGE("no input specified");
					return NULL;
				}
				inIndex = ::atoi(pInAttr->GetValue());

				IFilter *pPeerFilter = mFilterGraph.FindFilterByName(pPeerAttr->GetValue());
				if (pPeerFilter)
					return pPeerFilter;

				pPeerFilter = avf_create_filter_by_name(pPeerAttr->GetValue(), pPeerAttr->GetValue(), mpEngine, NULL);
				if (pPeerFilter == NULL) {
					AVF_LOGE("cannot create filter %s", pPeerAttr->GetValue());
					return NULL;
				}

				bNew = true;
				return pPeerFilter;
			}
		}
	}

	if (bUseExisting) {
		AVF_LOGD(C_YELLOW "no downstream filter for %s" C_NONE, pFilterName);
		return NULL;
	}

	// get media format
	CMediaFormat *pFormat = pPin->QueryMediaFormat();
	if (pFormat == NULL) {
		AVF_LOGD("No media on pin %d of %s", outIndex, pFilter->GetFilterName());
		return NULL;
	}

	// get new filter
	IFilter *pNewFilter = avf_create_filter_by_media_format(pFormat, mpEngine);
	pFormat->Release();
	if (pNewFilter == NULL) {
		AVF_LOGE("cannot render pin %d of %s", outIndex, pFilter->GetFilterName());
		return NULL;
	}

	bNew = true;
	return pNewFilter;
}

void CBasePlayer::ErrorStop()
{
	AVF_LOGE("player ErrorStop");
	mFilterGraph.StopAllFilters();
	PostPlayerMsg(IEngine::EV_PLAYER_ERROR, E_ERROR, 0);
}

avf_status_t CBasePlayer::RenderAllFilters(bool bUseExisting)
{
	CConfigNode *connections = mpConfig->FindChild("connections", mConf);

	avf_size_t total = mFilterGraph.GetNumFilters();
	for (avf_size_t i = 0; i < total; i++) {
		IFilter *pFilter = mFilterGraph.__GetFilter(i);
		avf_status_t status = RenderFilter(connections, bUseExisting, pFilter);
		if (status != E_OK)
			return status;
	}

	return E_OK;
}

avf_status_t CBasePlayer::RenderFilter(CConfigNode *connections, bool bUseExisting, IFilter *pFilter)
{
	avf_size_t num_outputs = pFilter->GetNumOutput();
	for (avf_size_t i = 0; i < num_outputs; i++) {
		IPin *pPin = pFilter->GetOutputPin(i);
		if (pPin == NULL) {
			AVF_LOGE("cannot get pin %d of %s", i, pFilter->GetFilterName());
			return E_ERROR;
		}
		if (pPin->CanConnect()) {

			avf_size_t outIndex = 0;

			// create the filter
			bool bNew = false;
			IFilter *pNewFilter = CreateDownStreamFilter(connections, bUseExisting, pFilter, pPin, i, outIndex, bNew);
			if (pNewFilter == NULL) {
				continue;	// todo - check if it is optional
				//return E_ERROR;
			}

			avf_status_t status;

			if (bNew) {
				// add the filter
				AVF_LOGD("add filter %s", pNewFilter->GetFilterName());

				status = mFilterGraph.AddFilter(pNewFilter);
				if (status != E_OK) {
					pNewFilter->Release();
					return status;
				}

				// init the filter
				status = pNewFilter->InitFilter();
				if (status != E_OK) {
					pNewFilter->Release();
					return status;
				}
			}

			// connect
			status = mFilterGraph.Connect(pFilter, i, pNewFilter, outIndex);
			if (status != E_OK) {
				AVF_LOGE("cannot connect %s[%d] -> %s[%d]",	
					pFilter->GetFilterName(), i, pNewFilter->GetFilterName(), 0);
				return status;
			}
		}
	}
	return E_OK;
}

avf_status_t CBasePlayer::LoadAllFilters()
{
	AVF_LOGD("LoadAllFilters %d", mConf);
	CConfigNode* pFilters = mpConfig->FindChild("filters", mConf);
	if (pFilters == NULL) {
		AVF_LOGW("No filters");
		return E_OK;
	}

	CConfigNode *pFilterConfig = pFilters->GetFirst();
	for (; pFilterConfig; pFilterConfig = pFilterConfig->GetNext()) {
		IFilter *pFilter = CreateFilter(NULL, NULL, pFilterConfig);
		if (pFilter == NULL)
			return E_ERROR;
	}

	return E_OK;
}

avf_status_t CBasePlayer::CreateAllConnections()
{
	CConfigNode* pConnections = mpConfig->FindChild("connections", mConf);
	if (pConnections == NULL)
		return E_OK;

	CConfigNode *pConnection = pConnections->GetFirst();
	for (; pConnection; pConnection = pConnection->GetNext()) {
		avf_status_t status = CreateConnection(pConnection);
		if (status != E_OK)
			return E_ERROR;
	}

	return E_OK;
}

avf_status_t CBasePlayer::CreateGraph()
{
	avf_status_t status;

	if ((status = LoadAllFilters()) != E_OK)
		return status;

	if ((status = CreateAllConnections()) != E_OK)
		return status;

	return E_OK;
}

avf_status_t CBasePlayer::RunMedia(bool *pbDone)
{
	avf_status_t status;

	if ((status = mFilterGraph.PreRunAllFilters()) != E_OK) {
		mFilterGraph.StopAllFilters();
		return status;
	}

	if ((status = mFilterGraph.RunAllFilters()) != E_OK) {
		mFilterGraph.StopAllFilters();
		return status;
	}

	return E_OK;
}

avf_status_t CBasePlayer::StopMedia(bool *pbDone)
{
	mFilterGraph.StopAllFilters();
	mFilterGraph.PurgeAllFilters();
	return E_OK;
}

// this is called from media controller's thread without mutex
void CBasePlayer::ProcessFilterMsg(const avf_msg_t& msg)
{
	switch (msg.code) {
	case IEngine::MSG_ERROR:
		PostPlayerMsg(IEngine::EV_PLAYER_ERROR, msg.p1, 0);
		break;

	case IEngine::MSG_FILE_START:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_FILE_START, 0);
		break;

	case IEngine::MSG_FILE_END:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_FILE_END, 0);
		break;

	case IEngine::MSG_DONE_SAVING:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_DONE_SAVING, 0);
		break;

	case IEngine::MSG_DONE_WRITE_NEXT_PICTURE:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_DONE_WRITE_NEXT_PICTURE, 0);
		break;

	case IEngine::MSG_MEDIA_SOURCE_PROGRESS:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_MEDIA_SOURCE_PROGRESS, msg.p1);
		break;

	case IEngine::MSG_UPLOAD_VIDEO:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_UPLOAD_VIDEO, msg.p1);
		break;

	case IEngine::MSG_UPLOAD_PICTURE:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_UPLOAD_PICTURE, msg.p1);
		break;

	case IEngine::MSG_UPLOAD_RAW:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_UPLOAD_RAW, msg.p1);
		break;

	case IEngine::MSG_MMS_MODE1:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_MMS_MODE1, msg.p1);
		break;

	case IEngine::MSG_MMS_MODE2:
		PostPlayerMsg(IEngine::EV_APP_EVENT, IMediaControl::APP_MSG_MMS_MODE2, msg.p1);
		break;

	default:
		break;
	}
}

avf_status_t CBasePlayer::DispatchTimer(IFilter *pTargetFilter, avf_enum_t id)
{
	// lookup the filter, maybe it was destroyed
	avf_size_t total = mFilterGraph.GetNumFilters();
	for (avf_size_t i = 0; i < total; i++) {
		IFilter *pFilter = mFilterGraph.__GetFilter(i);
		if (pFilter == pTargetFilter) {
			ITimerTarget *pTarget = ITimerTarget::GetInterfaceFrom(pFilter);
			if (pTarget == NULL) {
				AVF_LOGE("filter %s is not a ITimerTarget", pFilter->GetFilterName());
				return E_ERROR;
			}
			return pTarget->TimerTriggered(id);
		}
	}
	AVF_LOGE("timer target not found.");
	return E_UNKNOWN;
}

const char *CBasePlayer::GetPinMediaTypeName(IPin *pPin)
{
	CMediaFormat *pFormat = pPin->GetUsingMediaFormat();
	if (pFormat == NULL)
		return "null";
	switch (pFormat->mMediaType) {
	case MEDIA_TYPE_VIDEO: return "video";
	case MEDIA_TYPE_AUDIO: return "audio";
	case MEDIA_TYPE_SUBTITLE: return "subtitle";
	default: return "unknown";
	}
}

avf_status_t CBasePlayer::DumpState()
{
	AVF_LOGI("================ dump state ================");
	avf_size_t total = mFilterGraph.GetNumFilters();
	for (avf_size_t i = 0; i < total; i++) {
		IFilter *pFilter = mFilterGraph.__GetFilter(i);
		AVF_LOGI(C_BROWN "filter[%d]: %s" C_NONE, i, pFilter->GetFilterName());
		// input
		int n = pFilter->GetNumInput();
		for (int j = 0; j < n; j++) {
			IPin *pPin = pFilter->GetInputPin(j);
			if (pPin == NULL) {
				AVF_LOGI("  input[%d]: NULL", j);
			} else if (pPin->Connected() > 0) {
				AVF_LOGI(C_BROWN "  input[%d]: %s [%s]" C_NONE, j,
					pPin->GetPinName(), GetPinMediaTypeName(pPin));
				AVF_LOGI(C_CYAN "    %s, received: %d (%d), stored: %d (%d)" C_NONE,
					pPin->GetEOS() ? "    EOS" : "running",
					pPin->GetBuffersReceived(), pPin->GetBuffersReceivedDiff(),
					pPin->GetBuffersStored(), pPin->GetBuffersStoredDiff());
			} else {
				AVF_LOGI("  input[%d]: %s, not connected", j, pPin->GetPinName());
			}
		}
		n = pFilter->GetNumOutput();
		for (int j = 0; j < n; j++) {
			IPin *pPin = pFilter->GetOutputPin(j);
			if (pPin == NULL) {
				AVF_LOGI("  output[%d]: NULL", j);
			} else if (pPin->Connected() > 0) {
				AVF_LOGI(C_BROWN "  output[%d]: %s [%s]" C_NONE, j,
					pPin->GetPinName(), GetPinMediaTypeName(pPin));
				AVF_LOGI(C_CYAN "    %s, sent: %d (%d)" C_NONE,
					pPin->GetEOS() ? "    EOS" : "running",
					pPin->GetBuffersSent(), pPin->GetBuffersSentDiff());
			} else {
				AVF_LOGI("  output[%d]: %s, not connected", j, pPin->GetPinName());
			}
		}
		AVF_LOGI("============================================");
	}
	return E_OK;
}

