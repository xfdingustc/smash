
#define LOG_TAG "media_control"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_config.h"
#include "avio.h"

//#include "libipc.h"
//#include "avf_media.h"

#include "avf_media_control.h"

extern "C" IMediaControl* avf_create_media_control(IRegistry *pGlobalRegistry, 
	MediaControl_MsgProc MsgProc, void *context, bool bRealTime)
{
	return CMediaControl::Create(pGlobalRegistry, MsgProc, context, bRealTime);
}

extern "C" void avf_default_media_control_cb(void*, avf_intptr_t, const avf_msg_t& msg)
{
	AVF_LOGE("no app msg handler for msg code %d", msg.code);
}

//-----------------------------------------------------------------------
//
//  CMediaControl
//
//-----------------------------------------------------------------------
CMediaControl* CMediaControl::Create(IRegistry *pGlobalRegistry, MediaControl_MsgProc MsgProc, void *context, bool bRealTime)
{
	if (MsgProc == NULL) MsgProc = avf_default_media_control_cb;
	CMediaControl* result = avf_new CMediaControl(pGlobalRegistry, "MediaControl", MsgProc, context, bRealTime);
	CHECK_STATUS(result);
	return result;
}

CMediaControl::CMediaControl(IRegistry *pGlobalRegistry, const char *pName, MediaControl_MsgProc MsgProc, void *context, bool bRealTime):

	mpConfig(NULL),
	mpRegistry(NULL),

	mName(pName),
	mpTimerManager(NULL),

	mPrevState(STATE_IDLE),
	mState(STATE_IDLE),
	mTargetState(STATE_IDLE),
	mbCancel(false),

	mpWorkQ(NULL),
	mpFilterMsgQ(NULL),
	mpPlayerMsgQ(NULL),

	mPlayerName(),
	mMediaSrc(),
	mTag(0),	// mTag(AVF_MEDIA_NONE),
	mbRealTime(bRealTime),

	mpPlayer(NULL),

	mAppMsgProc(MsgProc),
	mAppMsgContext(context)
{
	mComponents._Init();

	CREATE_OBJ(mpConfig = CConfig::Create());

	CREATE_OBJ(mpRegistry = CRegistry2::Create(pGlobalRegistry));

	CREATE_OBJ(mpTimerManager = CTimerManager::Create());

	CREATE_OBJ(mpWorkQ = CWorkQueue::Create(static_cast<IActiveObject*>(this)));

	CREATE_OBJ(mpFilterMsgQ = CMsgQueue::Create("FilterMsgQ", sizeof(avf_msg_t), 4));
	mpWorkQ->AttachMsgQueue(mpFilterMsgQ, CMsgQueue::Q_INPUT);

	CREATE_OBJ(mpPlayerMsgQ = CMsgQueue::Create("PlayerMsgQ", sizeof(avf_msg_t), 4));
	mpWorkQ->AttachMsgQueue(mpPlayerMsgQ, CMsgQueue::Q_INPUT);

	if (mbRealTime) {
		mpWorkQ->SetRTPriority(CThread::PRIO_MANAGER);
	}

	mpWorkQ->Run();

	CIOLock::SetDiskNotify(this);
}

CMediaControl::~CMediaControl()
{
	SetMediaState(STATE_IDLE, true);
	DeletePlayer();

	if (mpWorkQ) {
		mpWorkQ->Stop();
		if (mpFilterMsgQ)
			mpWorkQ->DetachMsgQueue(mpFilterMsgQ);
		if (mpPlayerMsgQ)
			mpWorkQ->DetachMsgQueue(mpPlayerMsgQ);
	}

	avf_safe_release(mpFilterMsgQ);
	avf_safe_release(mpPlayerMsgQ);
	avf_safe_release(mpWorkQ);

	avf_safe_release(mpTimerManager);
	avf_safe_release(mpRegistry);
	avf_safe_release(mpConfig);

	for (avf_size_t i = 0; i < mComponents.count; i++) {
		avf_safe_release(mComponents.elems[i]);
	}
	mComponents._Release();

	CIOLock::SetDiskNotify(NULL);
}

void *CMediaControl::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IEngine)
		return static_cast<IEngine*>(this);
	if (refiid == IID_IActiveObject)
		return static_cast<IActiveObject*>(this);
	if (refiid == IID_IMediaControl)
		return static_cast<IMediaControl*>(this);
	if (refiid == IID_ITimerManager)
		return static_cast<ITimerManager*>(this);
	if (refiid == IID_IRegistry)
		return static_cast<IRegistry*>(this);
	if (refiid == IID_ITimerReceiver)
		return static_cast<ITimerReceiver*>(this);
	return inherited::GetInterface(refiid);
}

void CMediaControl::ClearCmdMsgs()
{
	mpTimerManager->ClearTimers(this);

	ClearMsgQ(mpFilterMsgQ, NULL);
	ClearMsgQ(mpPlayerMsgQ, NULL);

	// cmdQ (set_media_src, player_cmd, timer)
	mpWorkQ->RemoveAllCmds(RemoveCmds, (void*)this);

	// EVENT_SET_STATE, EVENT_DISK_ERROR, EVENT_DISK_SLOW
	mpWorkQ->ClearAllEvents();
}

avf_status_t CMediaControl::SetTimer(IFilter *pFilter, 
	avf_enum_t id, avf_uint_t ms, bool bPeriodic)
{
	return mpTimerManager->SetTimer(this, pFilter, id, ms, bPeriodic);
}

avf_status_t CMediaControl::KillTimer(IFilter *pFilter, avf_enum_t id)
{
	return mpTimerManager->KillTimer(this, pFilter, id);
}

void *CMediaControl::FindComponent(avf_refiid_t refiid)
{
	AUTO_LOCK(mCompLock);
	for (avf_size_t i = 0; i < mComponents.count; i++) {
		IInterface *intf = mComponents.elems[i];
		void *result = intf->GetInterface(refiid);
		if (result) {
			return result;
		}
	}
	return NULL;
}

bool CMediaControl::RemoveCmds(void *context, void *pMsg, avf_size_t size)
{
	// only remove my commands
	AO::CMD *cmd = (AO::CMD*)pMsg;
	bool result = cmd->code >= CMD_FIRST;
	return result;
}

void CMediaControl::ClearMsgQ(CMsgQueue *pMsgQ, avf_queue_remove_msg_proc proc)
{
	int n = pMsgQ->ClearMsgQ(proc, (void*)this);
	AVF_UNUSED(n);
#ifdef AVF_DEBUG
	// TODO:
#endif
}

void CMediaControl::PostAppMsg(avf_uint_t code)
{
	avf_msg_t msg(code);
	(mAppMsgProc)(mAppMsgContext, mTag, msg);
}

void CMediaControl::PostAppMsgLocked(avf_msg_t &msg)
{
	mMutex.Unlock();
	(mAppMsgProc)(mAppMsgContext, mTag, msg);
	mMutex.Lock();
}

void CMediaControl::AOLoop()
{
	ReplyCmd(E_OK);
	PostAppMsg(APP_MSG_ATTACH_THREAD);
	ThreadLoop();
	PostAppMsg(APP_MSG_DETACH_THREAD);
}

void CMediaControl::ThreadLoop()
{
	while (true) {
		CMsgHub::MsgResult result;
		mpWorkQ->WaitMsg(result, CMsgQueue::Q_CMD | CMsgQueue::Q_INPUT);

		if (result.flag >= 0) {
			switch (result.flag) {
			case EVENT_SET_STATE:
				// events trigged by app to run state machine
				ProcessUserEvent(result.flag);
				break;

			case EVENT_DISK_ERROR:
				AVF_LOGW("send app EVENT_DISK_ERROR");
				PostAppMsg(APP_MSG_DISK_ERROR);
				break;

			case EVENT_DISK_SLOW:
				AVF_LOGW("send app EVENT_DISK_SLOW");
				PostAppMsg(APP_MSG_DISK_SLOW);
				break;

			default:
				break;
			}

			continue;
		}

		if (result.pMsgQ) {

			if (mpWorkQ->IsCmdQ(result.pMsgQ)) {

				AO::CMD cmd;
				if (mpWorkQ->PeekCmd(cmd)) {
					if (!AOCmdProc(cmd, true)) {
						// returning false means stop AOLoop
						return;
					}
				}

			} else if (result.pMsgQ == mpFilterMsgQ) {

				avf_msg_t msg;
				if (mpFilterMsgQ->PeekMsg((void*)&msg, sizeof(avf_msg_t))) {
					if (mpPlayer) {
						mpPlayer->ProcessFilterMsg(msg);
					} else {
						// TODO:
					}
					msg.Release();
				}

			} else if (result.pMsgQ == mpPlayerMsgQ) {

				avf_msg_t msg;
				if (mpPlayerMsgQ->PeekMsg((void*)&msg, sizeof(avf_msg_t))) {
					AUTO_LOCK(mMutex);

					if (msg.code == EV_APP_EVENT) { // from player/vdb to app
						avf_msg_t app_msg(msg.p0);
						app_msg.p0 = 0;
						app_msg.p1 = msg.p1;
						PostAppMsgLocked(app_msg);
					} else { // to us
						ProcessPlayerEventLocked(msg);
					}

					msg.Release();
				}

			} else {
				AVF_LOGE("MediaControl: Unknown msg queue");
			}
		}
	}
}

bool CMediaControl::RegisterComponent(IInterface *comp)
{
	AUTO_LOCK(mCompLock);
	IInterface** pcomp = mComponents._Append(NULL);
	if (pcomp) {
		comp->AddRef();
		*pcomp = comp;
		return true;
	} else {
		return false;
	}
}

bool CMediaControl::UnregisterComponent(IInterface *comp)
{
	AUTO_LOCK(mCompLock);
	if (mComponents._Remove(comp)) {
		comp->Release();
		return true;
	} else {
		AVF_LOGW("UnregisterComponent: not found");
		return false;
	}
}

// state transtion matrix
struct mcontrol_state_transition_t {
	avf_u8_t allowed;	// state transition is allowed
	avf_u8_t activate;	// need to activate the thread
	avf_u8_t cancel;		// need to setup cancel flag
};

#define BAD_TRANS \
	{0,0,0}

//
// the state transition path:
//		idle -> OpenMedia() -> opening/open
//		opening -> open
//		open -> PrepareMedia() -> prepared
//		prepared -> RunMedia() -> starting/running
//		starting -> running
//		running -> StopMedia() -> open
//		open -> CloseMedia() -> idle
//
static const mcontrol_state_transition_t
g_mcontrol_state_transition[IMediaControl::STATE_NUM][IMediaControl::STATE_NUM] = {
		// idle		[opening]	open		[preparing]	prepared	[starting]	running		[stopping]	error
// from idle
		{{1,0,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	BAD_TRANS},
// from opening
		{{1,0,1},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	BAD_TRANS},
// from open
		{{1,1,0},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	BAD_TRANS},
// from preparing
		{{1,0,1},	BAD_TRANS,	{1,0,1},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	BAD_TRANS},
// from prepared
		{{1,1,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	BAD_TRANS},
// from starting
		{{1,1,1},	BAD_TRANS,	{1,0,1},	BAD_TRANS,	{1,0,1},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	BAD_TRANS},
// from running
		{{1,1,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	BAD_TRANS},
// from stopping
		{{1,0,0},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	{1,0,0},	BAD_TRANS,	BAD_TRANS},
// from error
		{{1,1,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	{1,1,0},	BAD_TRANS,	BAD_TRANS},
};

static avf_sbool_t g_trigger_next_state[IMediaControl::STATE_NUM] = {
	1,	// STATE_IDLE
	0,	// STATE_OPENING

	1,	// STATE_OPEN
	0,	// STATE_PREPARING

	1,	// STATE_PREPARED
	0,	// STATE_STARTING

	1,	// STATE_RUNNING
	0,	// STATE_STOPPING

	1,	// STATE_ERROR
};

extern "C" const char *avf_get_media_state_name(avf_enum_t media_state)
{
	switch (media_state) {
	case IMediaControl::STATE_IDLE: return "IDLE";
	case IMediaControl::STATE_OPENING: return "OPENING";
	case IMediaControl::STATE_OPEN: return "OPEN/STOPPED";
	case IMediaControl::STATE_PREPARING: return "PREPARING";
	case IMediaControl::STATE_PREPARED: return "PREPARED";
	case IMediaControl::STATE_STARTING: return "STARTING";
	case IMediaControl::STATE_RUNNING: return "RUNNING";
	case IMediaControl::STATE_STOPPING: return "STOPPING";
	case IMediaControl::STATE_ERROR: return "ERROR";
	default: return "UNKNOWN";
	}
}

extern "C" const char *avf_get_control_event_name(avf_enum_t event)
{
	switch (event) {
	case IEngine::EV_NULL: return "evNull";
	case IEngine::EV_PLAYER_ERROR: return "evPlayerError";
	case IEngine::EV_OPEN_MEDIA_DONE: return "evOpenMediaDone";
	case IEngine::EV_PREPARE_MEDIA_DONE: return "evPrepareMediaDone";
	case IEngine::EV_RUN_MEDIA_DONE: return "evRunMediaDone";
	case IEngine::EV_STOP_MEDIA_DONE: return "evStopMediaDone";
	case IEngine::EV_CHANGE_STATE: return "evChangeState";
	case IEngine::EV_APP_EVENT: return "evAppEvent";
	default: return "Unknown";
	}
}

// runs in user thread
avf_status_t CMediaControl::SetMediaStateLocked(avf_enum_t targetState)
{
	mcontrol_state_transition_t transition;

	if (targetState < 0 || targetState >= STATE_NUM) {
		AVF_LOGE("bad target state %d", targetState);
		return E_INVAL;
	}

	transition = g_mcontrol_state_transition[mState][targetState];

	if (!transition.allowed) {
		AVF_LOGE("transition from state %s to %s is not allowed",
			avf_get_media_state_name(mState), avf_get_media_state_name(targetState));
		return E_PERM;
	}

	mStateResult = E_OK;
	mTargetState = targetState;
	mbCancel = transition.cancel;

	if (transition.activate) {
		// activate my thread
		SetEventFlag(EVENT_SET_STATE);
	}

	if (mbCancel && mpPlayer) {
		AVF_LOGD("player is busy, try to cancel");
		mpPlayer->CancelActionAsync();
	}

	return E_OK;
}

// called by user thread
avf_status_t CMediaControl::SetMediaState(avf_enum_t targetState, bool bWait)
{
	AVF_LOGD(C_GREEN "--- SetMediaState %s ---" C_NONE, avf_get_media_state_name(targetState));

	avf_status_t status;
	{
		AUTO_LOCK(mMutex);
		status = SetMediaStateLocked(targetState);
		if (status != E_OK)
			return status;
	}

	if (bWait) {
		WaitState(targetState, &status);
		return status;
	}

	return E_OK;
}

// called by user thread
void CMediaControl::GetStateInfo(StateInfo& info)
{
	AUTO_LOCK(mMutex);
	info.prevState = mPrevState;
	info.state = mState;
	info.targetState = mTargetState;
	info.error = mStateResult;
}

// user wait until our state is changed to 'state' or ERROR
avf_enum_t CMediaControl::WaitState(avf_enum_t state, avf_status_t *result)
{
	AUTO_LOCK(mMutex);

	while (true) {
		AVF_LOGD("wait state "
			C_YELLOW "%s" C_NONE ", current: " C_YELLOW "%s" C_NONE, 
			avf_get_media_state_name(state),
			avf_get_media_state_name(mState));

		if (mState == state) {
			*result = mStateResult;
			return mState;
		}

		if (mState == STATE_ERROR) {
			// if user wants to be IDLE, then wait;
			// otherwise return
			if (state != STATE_IDLE) {
				*result = mStateResult;
				return STATE_ERROR;
			}
			SetMediaStateLocked(STATE_IDLE);
		}

		mStateChangeCond.Wait(mMutex);
	}
}

// called by user thread
avf_status_t CMediaControl::SetMediaSource(const char *pPlayer, 
	const char *pMediaSrc, const char *pConfig, avf_intptr_t tag, int conf)
{
	SetMediaSrcStruct sms = {pPlayer, pMediaSrc, pConfig, tag, conf};
	return mpWorkQ->SendCmd(CMD_SET_MEDIA_SRC, (void*)&sms);
}

avf_status_t CMediaControl::SetMediaSourceLocked(SetMediaSrcStruct *sms)
{
	mpConfig->Clear();

//	int autodel = mpRegistry->ReadRegBool(NAME_VDB_AUTODEL, 0, VALUE_VDB_AUTODEL);
//	int nodelete = mpRegistry->ReadRegBool(NAME_VDB_NO_DELETE, 0, VALUE_VDB_NO_DELETE);

	mpRegistry->ClearRegistry();

//	mpRegistry->WriteRegBool(NAME_VDB_AUTODEL, 0, autodel);
//	mpRegistry->WriteRegBool(NAME_VDB_NO_DELETE, 0, nodelete);

	if (sms->pPlayer == NULL) {
		mPlayerName.clear();
		mMediaSrc.clear();
		mTag = 0; //mTag = AVF_MEDIA_NONE;
		return E_OK;
	}

	mPlayerName = sms->pPlayer;
	if (sms->pMediaSrc)
		mMediaSrc = sms->pMediaSrc;
	else
		mMediaSrc.clear();

	if (sms->pConfig) {
		if (!mpConfig->Parse(sms->pConfig))
			return E_ERROR;
		mpRegistry->LoadConfig(mpConfig, "configs");
	}

	mTag = sms->tag;
	mConf = sms->conf;

	return E_OK;
}

avf_status_t CMediaControl::Command(int cmpid, avf_enum_t cmdId, avf_intptr_t cmdArg)
{
	PlayerCmdStruct pcs = {cmdId, cmdArg};
	return mpWorkQ->SendCmd(cmpid, CMD_PLAYER_CMD, (void*)&pcs);
}

avf_status_t CMediaControl::DumpState()
{
	return mpWorkQ->SendCmd(CMD_DUMP_STATE, NULL);
}

// received from the timer thread, then post it to our thread
void CMediaControl::OnTimer(void *param1, avf_enum_t param2)
{
	AO::CMD cmd(CMD_TIMER, param1, param2);
	mpWorkQ->PostCmd(cmd);
}

void CMediaControl::OnDiskError()
{
	SetEventFlag(EVENT_DISK_ERROR);
}

void CMediaControl::OnDiskSlow()
{
	SetEventFlag(EVENT_DISK_SLOW);
}

// return false to exit AOLoop
bool CMediaControl::AOCmdProc(const CMD& cmd, bool bInAOLoop)
{
	switch (cmd.code) {
	case AO::CMD_STOP:
		ReplyCmd(E_OK);
		return false;

	case CMD_SET_MEDIA_SRC: {
			AUTO_LOCK(mMutex);
			if (mState != STATE_IDLE && mState != STATE_ERROR) {
				AVF_LOGE("SetMediaSource: not in idle/error state");
				ReplyCmd(E_BADCALL);
			} else {
				SetMediaSrcStruct *sms = (SetMediaSrcStruct*)cmd.pExtra;
				ReplyCmd(SetMediaSourceLocked(sms));
			}
		}
		return true;

	case CMD_PLAYER_CMD:
		if (mpPlayer == NULL) {
			AVF_LOGE("no player when calling Command()");
			ReplyCmd(E_PERM);
		} else if (mpPlayer->GetID() != cmd.sid) {
			AVF_LOGE("player id %d is not equal cmd sid %d", mpPlayer->GetID(), (int)cmd.sid);
			ReplyCmd(E_PERM);
		} else {
			PlayerCmdStruct *pcs = (PlayerCmdStruct*)cmd.pExtra;
			avf_status_t status = mpPlayer->Command(pcs->cmdId, pcs->cmdArg);
			ReplyCmd(status);
		}
		return true;

	case CMD_TIMER:
		if (mpPlayer == NULL) {
			AVF_LOGW("timer not handled!");
		} else {
			if (mpPlayer->DispatchTimer((IFilter*)cmd.pExtra, cmd.param) == E_OK) {
				mpTimerManager->ResetTimer(this, cmd.pExtra, cmd.param);
			}
		}
		return true;

	case CMD_DUMP_STATE:
		// TODO : dump MediaControl state
		if (mpPlayer == NULL) {
			AVF_LOGW("no player");
		} else {
			mpPlayer->DumpState();
		}
		ReplyCmd(E_OK);
		return true;

	default:
		ReplyCmd(E_UNKNOWN);
		return true;
	}
}

// change state to newState
void CMediaControl::ChangeStateLocked(avf_enum_t newState, avf_status_t status)
{
	if (mState == newState)
		return;

	AVF_LOGD("state changed from "
		C_YELLOW "%s" C_NONE " to " C_YELLOW "%s" C_NONE ", target: " C_YELLOW "%s" C_NONE,
		avf_get_media_state_name(mState),
		avf_get_media_state_name(newState),
		avf_get_media_state_name(mTargetState));

	mPrevState = mState;
	mState = newState;

	if (mpPlayer) {
		mpPlayer->SetState(mState);
	}

	mStateResult = status;
	if (mState == STATE_IDLE) {
		ClearCmdMsgs();
	} else if (mState == STATE_ERROR) {
		// if we're changed to ERROR state, also set target state as ERROR
		AVF_LOGD("new state: ERROR, status: %d", status);
		mTargetState = mState;
	}

	// wakeup any one is waiting our state change
	mStateChangeCond.WakeupAll();

	// self-trigger next state
	if (g_trigger_next_state[mState] && mTargetState != mState) {
		SetEventFlag(EVENT_SET_STATE);
	}

	// notify user
	PostAppMsgLocked(APP_MSG_STATE_CHANGED);
}

// currently, user event only comes from that user changes our target state
void CMediaControl::ProcessUserEvent(avf_uint_t event)
{
	AUTO_LOCK(mMutex);

	AVF_LOGD("state: " C_YELLOW "%s" C_NONE ", target: " C_YELLOW "%s" C_NONE,
		avf_get_media_state_name(mState),
		avf_get_media_state_name(mTargetState));

	if (mState == mTargetState)
		return;

	if (mState == STATE_ERROR && mTargetState != STATE_IDLE) {
		AVF_LOGE("cannot change from ERROR to %s", avf_get_media_state_name(mTargetState));
		mTargetState = STATE_ERROR;
		return;
	}

	RunStateMachineLocked();
}

// player just sent us an event
// p0: error code
void CMediaControl::ProcessPlayerEventLocked(const avf_msg_t& msg)
{
	if (msg.code == EV_PLAYER_ERROR) {
		AVF_LOGI("media control got EV_PLAYER_ERROR");
		/*
		if (mState >= STATE_OPEN) {
			ChangeStateLocked(STATE_OPEN, (avf_status_t)msg.p0);
		} else {
			ChangeStateLocked(STATE_IDLE, (avf_status_t)msg.p0);
		}
		*/
		EnterErrorStateL((avf_status_t)msg.p0);
		return;
	}

	if (msg.code == EV_CHANGE_STATE) {
		ChangeStateLocked(msg.p0);
		mTargetState = msg.p1;
		return;
	}

	switch (mState) {
	case STATE_OPENING:
		if (msg.code == EV_OPEN_MEDIA_DONE) {
			ChangeStateLocked(STATE_OPEN);
			return;
		}
		break;

	case STATE_PREPARING:
		if (msg.code == EV_PREPARE_MEDIA_DONE) {
			ChangeStateLocked(STATE_PREPARED);
			return;
		}
		break;

	case STATE_STARTING:
		if (msg.code == EV_RUN_MEDIA_DONE) {
			ChangeStateLocked(STATE_RUNNING);
			return;
		}
		break;

	// TODO
	case STATE_RUNNING:
	case STATE_STOPPING:
		if (msg.code == EV_STOP_MEDIA_DONE) {
			if (mState == STATE_RUNNING) {
				mTargetState = STATE_OPEN;
			}
			ChangeStateLocked(STATE_OPEN);
			return;
		}
		break;

	default:
		break;
	}

	AVF_LOGE("wrong event %s received, state: %s",
		avf_get_control_event_name(msg.code),
		avf_get_media_state_name(mState));
}

void CMediaControl::RunStateMachineLocked()
{
	switch (mState) {
	case STATE_IDLE:
		IdleStateAction();
		break;

	case STATE_OPEN:
		OpenStateAction();
		break;

	case STATE_PREPARED:
		PreparedStateAction();
		break;

	case STATE_STARTING:
	case STATE_RUNNING:
		RunningStateAction();
		break;

	case STATE_ERROR:
		ErrorStateAction();
		break;

	default:
		break;
	}
}

// idle -> open -> prepared -> running
void CMediaControl::IdleStateAction()
{
	switch (mTargetState) {
	case STATE_IDLE:
		break;

	case STATE_OPEN:
	case STATE_PREPARED:
	case STATE_RUNNING:
		OpenMediaLocked();
		break;

	default:
		AVF_LOGE("IdleStateAction: bad target state %d", mTargetState);
		break;
	}
}

// open -> idle
// open -> prepared -> running
void CMediaControl::OpenStateAction()
{
	switch (mTargetState) {
	case STATE_IDLE:
		CloseMediaLocked();
		break;

	case STATE_PREPARED:
	case STATE_RUNNING:
		PrepareMediaLocked();
		break;

	default:
		break;
	}
}

// prepared -> open -> idle
// prepared -> running
void CMediaControl::PreparedStateAction()
{
	switch (mTargetState) {
	case STATE_IDLE:
	case STATE_OPEN:
		ChangeStateLocked(STATE_OPEN);
		break;

	case STATE_RUNNING:
		RunMediaLocked();
		break;

	default:
		break;
	}
}

// runing -> prepared -> open ->idle
void CMediaControl::RunningStateAction()
{
	switch (mTargetState) {
	case STATE_IDLE:
	case STATE_OPEN:
	case STATE_PREPARED:
		StopMediaLocked();
		break;

	default:
		break;
	}
}

// error -> idle -> open -> prepared -> running
void CMediaControl::ErrorStateAction()
{
	switch (mTargetState) {
	case STATE_IDLE:
	case STATE_OPEN:
	case STATE_PREPARED:
	case STATE_RUNNING:
		CloseMediaLocked();
		break;
	}
}

// called when open/error -> idle
void CMediaControl::CloseMediaLocked()
{
	DeletePlayer();
	ChangeStateLocked(STATE_IDLE);
}

// called when idle -> open
void CMediaControl::OpenMediaLocked()
{
	AVF_ASSERT(mpPlayer == NULL);
	mpPlayer = avf_create_player(mPlayerName, mMediaSrc, mConf, mpConfig, static_cast<IEngine*>(this));
	if (mpPlayer == NULL) {
		AVF_LOGE("create player failed");
		EnterErrorStateL(E_FATAL);
		return;
	}

	ChangeStateLocked(STATE_OPENING);
	mbCancel = false;

	mMutex.Unlock();
	//====================================================
	bool bDone = true;
	avf_status_t status = mpPlayer->OpenMedia(&bDone);
	//====================================================
	mMutex.Lock();

	if (status == E_OK) {
		// open is successful
		if (bDone) {
			ChangeStateLocked(STATE_OPEN);
		} else {
			// keep STATE_OPENING state
			// wait for EV_OPEN_MEDIA_DONE
		}
	} else {
		// open failed/cancelled
		if (mbCancel) {
			ChangeStateLocked(STATE_IDLE);
		} else {
			AVF_LOGE("OpenMediaLocked failed %d", status);
			EnterErrorStateL(status);
		}
	}

	mbCancel = false;
}

// called when doing open -> prepared
void CMediaControl::PrepareMediaLocked()
{
	ChangeStateLocked(STATE_PREPARING);
	mbCancel = false;

	mMutex.Unlock();
	//====================================================
	AVF_ASSERT(mpPlayer);
	bool bDone = true;
	avf_status_t status = mpPlayer->PrepareMedia(&bDone);
	//====================================================
	mMutex.Lock();

	if (status == E_OK) {
		if (bDone) {
			ChangeStateLocked(STATE_PREPARED);
		} else {
			// keep PREPARING state
			// wait for EV_PREPARE_MEDIA_DONE
		}
	} else {
		if (mbCancel) {
			ChangeStateLocked(STATE_IDLE);
		} else {
			AVF_LOGE("PrepareMedia failed %d", status);
			EnterErrorStateL(status);
		}
	}

	mbCancel = false;
}

// called when doing prepared -> running
void CMediaControl::RunMediaLocked()
{
	ChangeStateLocked(STATE_STARTING);
	mbCancel = false;

	mMutex.Unlock();
	//====================================================
	AVF_ASSERT(mpPlayer);
	bool bDone = true;
	avf_status_t status = mpPlayer->RunMedia(&bDone);
	//====================================================
	mMutex.Lock();

	if (status == E_OK) {
		if (bDone) {
			ChangeStateLocked(STATE_RUNNING);
		} else {
			// keep STARTING state
			// wait for EV_RUN_MEDIA_DONE
		}
	} else {
		if (mbCancel) {
			ChangeStateLocked(STATE_PREPARED);
		} else {
			AVF_LOGE("RunMedia failed");
			EnterErrorStateL(status);
		}
	}

	mbCancel = false;
}

// called when doing running -> prepared
// cannot be cancelled
void CMediaControl::StopMediaLocked()
{
	ChangeStateLocked(STATE_STOPPING);

	mMutex.Unlock();
	//====================================================
	AVF_ASSERT(mpPlayer);
	bool bDone = true;
	avf_status_t status = mpPlayer->StopMedia(&bDone);
	//====================================================
	mMutex.Lock();

	if (status == E_OK) {
		if (bDone) {
			ChangeStateLocked(STATE_PREPARED);
		} else {
			// keep STOPPING state
			// wait for EV_STOP_MEDIA_DONE
		}
	} else {
		AVF_LOGE("StopMedia failed %d", status);
		EnterErrorStateL(status);
	}
}

