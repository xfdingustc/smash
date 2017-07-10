
#define LOG_TAG "camera"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "filter_if.h"
#include "avf_config.h"
#include "mem_stream.h"
#include "avio.h"

#include "base_player.h"
#include "avf_camera_if.h"
#include "camera.h"

//-----------------------------------------------------------------------
//
//  CCamera
//
//-----------------------------------------------------------------------
CCamera* CCamera::Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig)
{
	CCamera *result = avf_new CCamera(pEngine, mediaSrc, conf, pConfig);
	CHECK_STATUS(result);
	return result;
}

CCamera::CCamera(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig):
	inherited(CMP_ID_CAMERA, pEngine, mediaSrc, conf, pConfig)
{
	mbStarting = false;
	mbStopping = false;
	mbSaving = false;
	mbVdbRecording = false;
	mbDiskMonitor = false;

	mpImageControl = NULL;
	mpVideoControl = NULL;
	mpSubtitleInput = NULL;
	mpSubtitleFilter = NULL;
	mpSegmentSaver = NULL;

	int method;
	unsigned period, timeout, value;
	char buffer[REG_STR_MAX];
	pEngine->ReadRegString(NAME_DISK_MONITOR, IRegistry::GLOBAL, buffer, VALUE_DISK_MONITOR);
	if (buffer[0]) {
		if (::sscanf(buffer, "%d:%d,%d,%d", &method, &period, &timeout, &value) != 4) {
			AVF_LOGW("bad disk monitor param: %s", buffer);
		} else {
			AVF_LOGD("disk monitor: " C_CYAN "%s" C_NONE, buffer);
			CDiskMonitor::Init(method, period, timeout, value);
			mbDiskMonitor = true;
		}
	}
}

CCamera::~CCamera()
{
	StopRecordVdb();
	if (mbDiskMonitor) {
		CDiskMonitor::Release();
	}
}

avf_status_t CCamera::StartRecordVdb()
{
	IRecordObserver *observer = GetRecordObserver();
	if (observer == NULL)
		return E_OK;

	bool bEnableRecord = mpEngine->ReadRegInt32(NAME_RECORD, 0, 0) == 1;

	if (bEnableRecord) {
		int seg_len = observer->GetSegmentLength();
		if (seg_len < 0)
			return (avf_status_t)seg_len;

		AVF_LOGI(C_CYAN "set segment length to " C_YELLOW "%d" C_CYAN " seconds" C_NONE, seg_len);
		for (int i = 0; i < 3; i++) {
			mpEngine->WriteRegInt32(NAME_VIDEO_SEGMENT, i, seg_len);
		}
	}

	avf_status_t status = observer->StartRecord(bEnableRecord);
	if (status == E_OK) {
		mbVdbRecording = true;
	}

	return status;
}

void CCamera::StopRecordVdb()
{
	if (mbVdbRecording) {
		mbVdbRecording = false;
		IRecordObserver *observer = GetRecordObserver();
		if (observer) {
			observer->StopRecord();
		} else {
			AVF_LOGW("StopRecordVdb: no observer");
		}
	}
}

avf_status_t CCamera::RunMedia(bool *pbDone)
{
	avf_status_t status;

	if ((status = StartRecordVdb()) != E_OK) {
		mFilterGraph.StopAllFilters();
		return status;
	}

	mbStarting = false;
	mFilterGraph.ClearAllFiltersFlag(CFilterGraph::FF_STARTED);

	bool bNeedWait = false;
	avf_size_t total = mFilterGraph.GetNumFilters();
	for (avf_size_t i = 0; i < total; i++) {
		IFilter *pFilter = mFilterGraph.__GetFilter(i);
		ISourceFilter *pSource = ISourceFilter::GetInterfaceFrom(pFilter);

		if (pSource) {
			bool bDone = true;
			AVF_LOGD("[%d] start %s", i + 1, pFilter->GetFilterName());
			if ((status = pSource->StartSourceFilter(&bDone)) != E_OK) {
				AVF_LOGE("start %s failed: %d", pFilter->GetFilterName(), status);
				goto Error;
			}
			if (!bDone) {
				AVF_LOGD("StartSourceFilter(%s) need wait", pFilter->GetFilterName());
				bNeedWait = true;
			} else {
				mFilterGraph.__SetFilterFlag(i, CFilterGraph::FF_STARTED);
			}
		} else {
			mFilterGraph.__SetFilterFlag(i, CFilterGraph::FF_STARTED);
		}
	}

	if (bNeedWait) {
		*pbDone = false;
		mbStarting = true;
	} else {
		if ((status = RunAllFilters()) != E_OK) {
			goto Error;
		}
	}

	return E_OK;

Error:
	mFilterGraph.StopAllFilters();
	StopRecordVdb();
	return status;
}

// we stop all source filters, so they generate EOS to other filters
// when all filters have reported EOS, the filter graph is stopped
avf_status_t CCamera::StopMedia(bool *pbDone)
{
	mFilterGraph.ClearAllFiltersFlag(CFilterGraph::FF_EOS);

	avf_size_t total = mFilterGraph.GetNumFilters();
	if (total == 0) {
		mbStopping = false;
		*pbDone = true;
		return E_OK;
	}

	for (avf_size_t i = 0; i < total; i++) {
		IFilter *pFilter = mFilterGraph.__GetFilter(i);
		ISourceFilter *pSource = ISourceFilter::GetInterfaceFrom(pFilter);
		if (pSource) {
			AVF_LOGD(C_GREEN "stop source filter %s" C_NONE, pFilter->GetFilterName());
			avf_status_t status = pSource->StopSourceFilter();
			if (status != E_OK) {
				AVF_LOGE("stop source filter %s failed", pFilter->GetFilterName());
				return status;
			}
			//AVF_LOGI("Source filter %s stopped", pFilter->GetFilterName());
		}
	}

	mbStopping = true;
	*pbDone = false;

	return E_OK;
}

avf_status_t CCamera::RunAllFilters()
{
	avf_status_t status;

	if ((status = mFilterGraph.PreRunAllFilters()) != E_OK) {
		mFilterGraph.StopAllFilters();
		return status;
	}

	if ((status = mFilterGraph.RunAllFilters()) != E_OK)
		return status;

	avf_u64_t record_tick = avf_get_curr_tick();
	mpEngine->WriteRegInt64(NAME_CAMER_START_RECORD_TICKS, 0, record_tick);

	return E_OK;
}

void CCamera::ProcessFilterMsg(const avf_msg_t& msg)
{
	avf_status_t status;

	switch (msg.code) {
	case IEngine::MSG_STARTED: {
			IFilter *pFilter = (IFilter*)msg.p0;
			AVF_LOGD("%s started", pFilter->GetFilterName());

			if (!mbStarting) {
				AVF_LOGE("not in starting state!");
				return;
			}

			mFilterGraph.SetFilterFlag(pFilter, CFilterGraph::FF_STARTED);

			if (mFilterGraph.TestAllFiltersFlag(CFilterGraph::FF_STARTED)) {

				AVF_LOGD("all filters started");
				mbStarting = false;

				status = mFilterGraph.UpdateAllConnections();
				if (status != E_OK) {
					PostPlayerErrorMsg(status);
					return;
				}

				status = RunAllFilters();
				if (status != E_OK) {
					PostPlayerErrorMsg(status);
					return;
				}

				AVF_LOGD(C_GREEN "--- EV_RUN_MEDIA_DONE ---" C_NONE);
				PostPlayerMsg(IEngine::EV_RUN_MEDIA_DONE, 0, 0);
			}
		}
		return;

	case IEngine::MSG_EOS: {
			IFilter *pFilter = (IFilter*)msg.p0;
			AVF_LOGD(C_CYAN "[%d]" C_NONE " EOS from " C_CYAN "%s" C_NONE,
				pFilter->GetFilterIndex(), pFilter->GetFilterName());

			if (!mbStopping) {
				AVF_LOGE("not in stopping state!");
				return;
			}

			mFilterGraph.SetFilterFlag(pFilter, CFilterGraph::FF_EOS);

			if (mFilterGraph.TestAllEOS()) {
				AVF_LOGD(C_GREEN "--- all EOS received ---" C_NONE);
				mbStopping = false;

				mFilterGraph.StopAllFilters();
				PostPlayerMsg(IEngine::EV_STOP_MEDIA_DONE, 0, 0);

				StopRecordVdb();
			}
		}
		return;

	case IEngine::MSG_DONE_SAVING:
		mbSaving = false;
		return inherited::ProcessFilterMsg(msg);

	case IEngine::MSG_ERROR:
		//StopRecordVdb();
		return inherited::ProcessFilterMsg(msg);

	default:
		return inherited::ProcessFilterMsg(msg);
	}
}

avf_status_t CCamera::OnFilterCreated(IFilter *pFilter)
{
	if (mpImageControl == NULL) {
		mpImageControl = IImageControl::GetInterfaceFrom(pFilter);
	}
	if (mpVideoControl == NULL) {
		mpVideoControl = IVideoControl::GetInterfaceFrom(pFilter);
	}
	if (mpSubtitleInput == NULL) {
		if ((mpSubtitleInput = ISubtitleInput::GetInterfaceFrom(pFilter)) != NULL) {
			mpSubtitleFilter = pFilter;
		}
	}
	if (mpSegmentSaver == NULL) {
		mpSegmentSaver = ISegmentSaver::GetInterfaceFrom(pFilter);
	}
	return E_OK;
}

// this is called from the media control thread
avf_status_t CCamera::Command(avf_enum_t cmdId, avf_intptr_t arg)
{
	switch (cmdId) {
	case Camera::CMD_SetImageControl:
		if (mpImageControl == NULL) {
			AVF_LOGE("no image control interface");
			return E_INVAL;
		} else {
			name_value_t *param = (name_value_t*)arg;
			return mpImageControl->SetImageConfig(param);
		}

	case Camera::CMD_SaveCurrent:
		if (mbSaving) {
			AVF_LOGE("in saving state, cannot start again");
			return E_BUSY;
		}
		if (mpSegmentSaver == NULL) {
			AVF_LOGE("no saver");
			return E_ERROR;
		}
		if (!mpSegmentSaver->CanSave()) {
			AVF_LOGE("cannot save");
			return E_ERROR;
		} else {
			Camera::save_current_t *param = (Camera::save_current_t*)arg;
			return mpSegmentSaver->SaveCurrent(param->before, param->after, param->filename);
		}

	case Camera::CMD_SaveNextPicture:
		if (mpVideoControl == NULL) {
			AVF_LOGE("no IVideoControl");
			return E_ERROR;
		}
		return mpVideoControl->SaveNextPicture((const char*)arg);

	case Camera::CMD_StillCapture:
		if (mpVideoControl == NULL) {
			AVF_LOGE("no IVideoControl");
			return E_ERROR;
		} else {
			Camera::still_capture_s *param = (Camera::still_capture_s*)arg;
			return mpVideoControl->StillCapture(param->action, param->flags);
		}

	case Camera::CMD_GetStillCaptureInfo:
		if (mpVideoControl == NULL) {
			AVF_LOGE("no IVideoControl");
			return E_ERROR;
		} else {
			IVideoControl::stillcap_info_s *info = (IVideoControl::stillcap_info_s*)arg;
			return mpVideoControl->GetStillCaptureInfo(info);
		}

	case Camera::CMD_ChangeVoutVideo:
		if (mpVideoControl == NULL) {
			AVF_LOGE("no IVideoControl");
			return E_ERROR;
		} else {
			return mpVideoControl->ChangeVoutVideo((const char*)arg);
		}

	default:
		return E_INVAL;
	}
}

