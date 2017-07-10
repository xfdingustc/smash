
#define LOG_TAG "still_cap"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_queue.h"
#include "avf_util.h"

#include "avf_if.h"
#include "filter_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_socket.h"

#include "libipc.h"
#include "avf_media.h"

#include "still_capture.h"

// On Linux, a message queue descriptor is actually a file descriptor, 
// and can be monitored using select(2), poll(2), or epoll(7). 
// This is not portable.

#define AAA_STILL_CAP_CMD_Q 	"/still_cap_cmd"
#define AAA_STILL_CAP_STATUS_Q	"/still_cap_status"

#define INVALID_MSGQ		((mqd_t)-1)

enum {
	SRV_CAP_BURST_START = 1,
	SRV_CAP_BURST_STOP = 2,
	SRV_CAP_STATUS = 3,
	SRV_CAP_EXIT = 4,
	SRV_CAP_SINGLE_SHOT = 5,
	SRV_CAP_WITH_RAW = 6,
	SRV_CAP_JPEG_QUAL = 7,
	SRV_CAP_SD_CHANGE = 8,	// 0: no sd; 1: ready
};

enum {
	SRV_CAP_STATUS_SINGLE_DONE = 1,
	SRV_CAP_STATUS_BURST_STOP = 2,
	SRV_CAP_STATUS_CAP_FAIL = 3,//cap yuv err
	SRV_CAP_STATUS_WR_FAIL = 4,//write err
	SRV_CAP_STATUS_ENC_FAIL = 5,//soft jpeg enc error
	SRV_CAP_STATUS_SD_FAIL = 6,//cannot create file
};

//-----------------------------------------------------------------------
//
//  CStillCapture
//
//-----------------------------------------------------------------------

CStillCapture *CStillCapture::Create(IEngine *pEngine)
{
	CStillCapture *result = avf_new CStillCapture(pEngine);
	CHECK_STATUS(result);
	return result;
}

CStillCapture::CStillCapture(IEngine *pEngine):
	mpEngine(pEngine),
	mpEvent(NULL),
	mbIsRaw(false),
	mbStorageReady(false),
	mNumPictures(0),
	mpThread(NULL),
	mState(STATE_IDLE),
	mCapState(STILLCAP_STATE_IDLE),
	m_sendq(INVALID_MSGQ),
	m_recvq(INVALID_MSGQ)
{
	// for select()
	CREATE_OBJ(mpEvent = CSocketEvent::Create("StillCap"));
	CSocketEvent::SetBlocking(m_recvq, false);

	// send to aaa
	m_sendq = ::mq_open(AAA_STILL_CAP_CMD_Q, O_WRONLY);

	// recv from aaa
	m_recvq = ::mq_open(AAA_STILL_CAP_STATUS_Q, O_RDONLY);

	if (m_sendq == INVALID_MSGQ || m_recvq == INVALID_MSGQ) {
		AVF_LOGE("cannot open message queue");
		mStatus = E_ERROR;
		return;
	}
}

CStillCapture::~CStillCapture()
{
	Stop();
	if (m_sendq != INVALID_MSGQ) {
		::mq_close(m_sendq);
	}
	if (m_recvq != INVALID_MSGQ) {
		::mq_close(m_recvq);
	}
	avf_safe_release(mpEvent);
}

void *CStillCapture::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IStorageListener)
		return static_cast<IStorageListener*>(this);
	return inherited::GetInterface(refiid);
}

// API called from user thread
// start still capture thread
avf_status_t CStillCapture::Start()
{
	AUTO_LOCK(mMutex);

	if (mState != STATE_IDLE) {
		//AVF_LOGW("Start: not idle");
		return E_OK;
	}

	mpThread = CThread::Create("stillcap", ThreadEntry, this);
	if (mpThread == NULL) {
		AVF_LOGE("cannot create thread");
		return E_NOMEM;
	}

	mState = STATE_RUNNING;

	return E_OK;
}

// API called from user thread
// stop still capture thread
avf_status_t CStillCapture::Stop()
{
	// check state
	{
		AUTO_LOCK(mMutex);

		if (mState != STATE_RUNNING) {
			AVF_LOGD("Stop: not running (%d)", mState);
			return E_OK;
		}

		mState = STATE_STOPPING;
	}

	// interrupt and finish the thread
	mpEvent->Interrupt();
	avf_safe_release(mpThread);

	{
		AUTO_LOCK(mMutex);
		mpEvent->Reset();
		mState = STATE_IDLE;
	}

	return E_OK;
}

void CStillCapture::ProcessOneMsg()
{
	cap_msg_t msg;

	if (::mq_receive(m_recvq, (char*)&msg, sizeof(msg), NULL) < 0) {
		AVF_LOGP("mq_receive failed unexpectedly");
		avf_msleep(10);
		return;
	}

	if (msg.msg_id == SRV_CAP_STATUS) {
		HandleStatusMsg(msg.payload);
	} else {
		AVF_LOGW("unknown msg: msg_id=%d, payload=%d", msg.msg_id, msg.payload);
	}
}

IVDBControl *CStillCapture::GetVdbControl()
{
	IVDBControl *pVdbControl = (IVDBControl*)mpEngine->FindComponent(IID_IVDBControl);
	if (pVdbControl == NULL) {
		AVF_LOGE("no vdb control");
	}
	return pVdbControl;
}

// register CStillCapture so NotifyStorageState() will be called
void CStillCapture::RegisterStorageListener(bool bRegister)
{
	IVDBControl *pVdbControl = GetVdbControl();
	if (pVdbControl == NULL)
		return;

	if (bRegister) {
		AVF_LOGD("register storage listener");
		bool bReady = false;
		pVdbControl->RegisterStorageListener(this, &bReady);
		SetStorageState(bReady);
	} else {
		AVF_LOGD("unregister storage listener");
		pVdbControl->UnregisterStorageListener(this);
	}
}

// called when storage state is changed
void CStillCapture::SetStorageState(bool bReady)
{
	AUTO_LOCK(mMutex);
	if (bReady != mbStorageReady) {
		mbStorageReady = bReady;
		SendSDChangedMsg(bReady);
	}
}

avf_status_t CStillCapture::SendSDChangedMsg(bool bReady)
{
	cap_msg_t msg;
	msg.msg_id = SRV_CAP_SD_CHANGE;
	msg.payload = bReady ? 1 : 0;
	AVF_LOGD(C_GREEN "SRV_CAP_SD_CHANGE: %d" C_NONE, msg.payload);
	SendMsg(msg);
	return E_OK;
}

// check if sd is ready and there's enough space for capture
avf_status_t CStillCapture::CheckStorageSpaceLocked(IVDBControl *pVdbControl, bool bSetError)
{
	AVF_LOGD("CheckStorageSpaceLocked");

	// if there is sd card
	if (!mbStorageReady) {
		AVF_LOGD("storage is not ready");
		if (bSetError) {
			m_last_error = STILLCAP_ERR_NO_SD;
			StillCaptureStateChanged();
		}
		return E_ERROR;
	}

	if (pVdbControl == NULL) {
		if ((pVdbControl = GetVdbControl()) == NULL)
			return E_ERROR;
	}

	// if sd card has space
	if (!pVdbControl->CheckStorageSpace()) {
		AVF_LOGD("CheckStorageSpace returns false, no tf or no space");
		if (bSetError) {
			m_last_error = STILLCAP_ERR_NO_SPACE;
			StillCaptureStateChanged();
		}
		return E_ERROR;
	}

	return E_OK;
}

// called by vdb manager thread
void CStillCapture::NotifyStorageState(int is_ready)
{
	AVF_LOGD("NotifyStorageState: %d", is_ready);
	SetStorageState(is_ready);
}

void CStillCapture::ThreadLoop()
{
	AVF_LOGD(C_YELLOW "still capture thread running" C_NONE);

	RegisterStorageListener(true);

	while (true) {
		avf_status_t status = mpEvent->WaitFileInput(m_recvq, -1);
		if (status == E_OK) {
			ProcessOneMsg();
		} else if (status == E_CANCEL) {
			// interrupted
			break;
		} else {
			AVF_LOGW("WaitFileInput returns %d", status);
		}
	}

	while (true) {
		// keep on read & process msgs until capturing is done
		{
			AUTO_LOCK(mMutex);
			if (mCapState == STILLCAP_STATE_IDLE)
				break;
		}

		AVF_LOGD("cap state %d, wait for idle", mCapState);

		avf_status_t status = CSocketEvent::WaitSingleFileInput(m_recvq, -1);
		if (status == E_OK) {
			ProcessOneMsg();
		}
	}

	RegisterStorageListener(false);

	// force aaa to close files
	SendSDChangedMsg(false);

	AVF_LOGD(C_YELLOW "still capture thread stopped" C_NONE);
}

void CStillCapture::BadStatusMsg(int status)
{
	AVF_LOGW("bad status msg %d received when state is %d", status, mCapState);
}

void CStillCapture::HandleStatusMsg(int status)
{
	AUTO_LOCK(mMutex);

	int prev_num_pictures = mNumPictures;
	int prev_cap_state = mCapState;

	// last 6 bits is msg code
	switch (status & 0x3F) {
	case SRV_CAP_STATUS_SINGLE_DONE:
		AVF_LOGD("SRV_CAP_STATUS_SINGLE_DONE");
		if (mCapState == STILLCAP_STATE_SINGLE) {
			mNumPictures++;
			mCapState = STILLCAP_STATE_IDLE;
		} else if (mCapState == STILLCAP_STATE_BURST || mCapState == STILLCAP_STATE_STOPPING) {
			mNumPictures++;
		} else {
			BadStatusMsg(status);
			return;
		}
		break;

	case SRV_CAP_STATUS_BURST_STOP:
		AVF_LOGD("SRV_CAP_STATUS_BURST_STOP");
		if (mCapState != STILLCAP_STATE_BURST && mCapState != STILLCAP_STATE_STOPPING) {
			BadStatusMsg(status);
			return;
		}
		mCapState = STILLCAP_STATE_IDLE;
		break;

	case SRV_CAP_STATUS_CAP_FAIL:	// capture yuv error
		AVF_LOGD("SRV_CAP_STATUS_CAP_FAIL");
		m_last_error = STILLCAP_ERR_CAPTURE;
		mCapState = STILLCAP_STATE_IDLE;
		break;

	case SRV_CAP_STATUS_WR_FAIL:	// write error
		AVF_LOGD("SRV_CAP_STATUS_WR_FAIL");
		m_last_error = STILLCAP_ERR_WRITE;
		mCapState = STILLCAP_STATE_IDLE;
		break;

	case SRV_CAP_STATUS_ENC_FAIL:	// soft jpeg enc error
		AVF_LOGD("SRV_CAP_STATUS_ENC_FAIL");
		m_last_error = STILLCAP_ERR_SOFT_JPG;
		mCapState = STILLCAP_STATE_IDLE;
		break;

	case SRV_CAP_STATUS_SD_FAIL:	// no filename
		AVF_LOGD("SRV_CAP_STATUS_SD_FAIL");
		m_last_error = STILLCAP_ERR_CREATE_FILE;
		mCapState = STILLCAP_STATE_IDLE;
		break;

	default:
		BadStatusMsg(status);
		return;
	}

	if (mNumPictures > prev_num_pictures) {
		PictureTaken(status);
	}

	if (mCapState != prev_cap_state) {
		StillCaptureStateChanged();
	}
}

void CStillCapture::StillCaptureStateChanged()
{
	avf_msg_t msg;
	msg.sid = CMP_ID_CAMERA;
	msg.code = IEngine::EV_APP_EVENT;
	msg.p0 = IMediaControl::APP_MSG_STILL_CAPTURE_STATE_CHANGED;
	msg.p1 = 0;
	mpEngine->PostPlayerMsg(msg);
}

void CStillCapture::PictureTaken(int code)
{
	avf_msg_t msg;
	msg.sid = CMP_ID_CAMERA;
	msg.code = IEngine::EV_APP_EVENT;
	msg.p0 = IMediaControl::APP_MSG_PICTURE_TAKEN;
	msg.p1 = code;
	mpEngine->PostPlayerMsg(msg);
}

avf_status_t CStillCapture::SendMsg(cap_msg_t& msg)
{
	AUTO_LOCK(mWriteMutex);
	if (::mq_send(m_sendq, (char*)&msg, sizeof(msg), 0) < 0) {
		AVF_LOGP("mq_send");
		return E_ERROR;
	}
	return E_OK;
}

// API called from user thread
avf_status_t CStillCapture::StillCapture(int action, int flags)
{
	AUTO_LOCK(mMutex);

	if (mState != STATE_RUNNING) {
		AVF_LOGE("still capture is not running");
		return E_ERROR;
	}

	int new_state = mCapState;

	avf_status_t status;
	cap_msg_t msg;

	switch (action) {
	case STILLCAP_ACTION_SINGLE:
		AVF_LOGD("STILLCAP_ACTION_SINGLE");
		if (mCapState != STILLCAP_STATE_IDLE) {
			AVF_LOGE("is capturing");
			return E_PERM;
		}

		// todo - do it after still cap is done
		if ((status = CheckStorageSpaceLocked(NULL, true)) != E_OK)
			return status;

		msg.msg_id = SRV_CAP_SINGLE_SHOT;
		msg.payload = 0;

		new_state = STILLCAP_STATE_SINGLE;
		mNumPictures = 0;
		m_last_error = 0;

		break;

	case STILLCAP_ACTION_BURST:
		AVF_LOGD("STILLCAP_ACTION_BURST");
		if (mCapState != STILLCAP_STATE_IDLE) {
			AVF_LOGE("is capturing");
			return E_PERM;
		}

		// todo - do it after still cap is done
		if ((status = CheckStorageSpaceLocked(NULL, true)) != E_OK)
			return status;

		msg.msg_id = SRV_CAP_BURST_START;
		msg.payload = 0;

		mNumPictures = 0;
		m_last_error = 0;

		m_burst_start_tick = avf_get_curr_tick();
		new_state = STILLCAP_STATE_BURST;

		break;

	case STILLCAP_ACTION_STOP:
		AVF_LOGD("STILLCAP_ACTION_STOP");
		if (mCapState != STILLCAP_STATE_BURST) {
			AVF_LOGE("not bursting");
			return E_PERM;
		}

		msg.msg_id = SRV_CAP_BURST_STOP;
		msg.payload = 100;	// TODO

		new_state = STILLCAP_STATE_STOPPING;

		break;

	case STILLCAP_ACTION_SET_RAW:
		AVF_LOGD("STILLCAP_ACTION_SET_RAW %d", flags);
		if (mCapState != STILLCAP_STATE_IDLE) {
			AVF_LOGE("not idle");
			return E_PERM;
		}

		mbIsRaw = flags != 0;

		msg.msg_id = SRV_CAP_WITH_RAW;
		msg.payload = flags;

		break;

	default:
		AVF_LOGE("unknown still capture action %d", action);
		return E_ERROR;
	}

	if ((status = SendMsg(msg)) != E_OK)
		return status;

	if (mCapState != new_state) {
		mCapState = new_state;
		StillCaptureStateChanged();
	}

	return E_OK;
}

// API called from user thread
avf_status_t CStillCapture::GetStillCaptureInfo(IVideoControl::stillcap_info_s *info)
{
	AUTO_LOCK(mMutex);

	info->b_running = mState == STATE_RUNNING;
	info->stillcap_state = mCapState;
	info->b_raw = mbIsRaw;
	info->last_error = m_last_error;
	info->num_pictures = mNumPictures;
	if (mCapState == STILLCAP_STATE_BURST || mCapState == STILLCAP_STATE_STOPPING) {
		info->burst_ticks = (int)(avf_get_curr_tick() - m_burst_start_tick);
	} else {
		info->burst_ticks = 0;
	}

	return E_OK;
}

