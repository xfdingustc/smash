
#ifndef __AVF_MEDIA_CONTROL_H__
#define __AVF_MEDIA_CONTROL_H__

#include "avf_registry.h"
#include "timer_manager.h"

class CConfig;
class CMediaControl;

//-----------------------------------------------------------------------
//
//  CMediaControl:
//		Base class for engines.
//
//-----------------------------------------------------------------------
class CMediaControl:
	public CObject,
	public IEngine,
	public ITimerManager,
	public IActiveObject,
	public IMediaControl,
	public ITimerReceiver,
	public IDiskNotify
{
	typedef CObject inherited;
	typedef IActiveObject AO;
	DEFINE_INTERFACE;

	enum {
		CMD_FIRST = AO::CMD_LAST,
		CMD_SET_MEDIA_SRC = CMD_FIRST,
		CMD_PLAYER_CMD,
		CMD_TIMER,
		CMD_DUMP_STATE,
		CMD_LAST,
	};

	struct SetMediaSrcStruct {
		const char *pPlayer;
		const char *pMediaSrc;
		const char *pConfig;
		avf_intptr_t tag;
		int conf;
	};

	struct PlayerCmdStruct {
		avf_enum_t cmdId;
		avf_intptr_t cmdArg;
	};

protected:
	// event posted to my thread
	enum {
		EVENT_SET_STATE = 0,
		EVENT_DISK_ERROR = 1,
		EVENT_DISK_SLOW = 2,
		EVENT_LAST,
	};

public:
	static CMediaControl* Create(IRegistry *pGlobalRegistry, MediaControl_MsgProc MsgProc, void *context, bool bRealTime);

protected:
	CMediaControl(IRegistry *pGlobalRegistry, const char *pName, MediaControl_MsgProc MsgProc, void *context, bool bRealTime);
	virtual ~CMediaControl();

// IEngine
public:
	virtual void PostFilterMsg(const avf_msg_t& msg) {
		avf_status_t status = mpFilterMsgQ->PostMsg((void*)&msg, sizeof(avf_msg_t));
		AVF_ASSERT_OK(status);
	}
	virtual void PostPlayerMsg(const avf_msg_t& msg) {
		avf_status_t status = mpPlayerMsgQ->PostMsg((void*)&msg, sizeof(avf_msg_t));
		AVF_ASSERT_OK(status);
	}
	virtual void ClearFilterMsgs() {
		ClearMsgQ(mpFilterMsgQ, NULL);
	}
	virtual void *FindComponent(avf_refiid_t refiid);

	virtual IRegistry *GetRegistry() {
		return mpRegistry;
	}

	virtual avf_s32_t ReadRegInt32(const char *name, avf_u32_t index, avf_s32_t default_value) {
		return mpRegistry->ReadRegInt32(name, index, default_value);
	}
	virtual avf_s64_t ReadRegInt64(const char *name, avf_u32_t index, avf_s64_t default_value) {
		return mpRegistry->ReadRegInt64(name, index, default_value);
	}
	virtual avf_int_t ReadRegBool(const char *name, avf_u32_t index, avf_int_t default_value) {
		return mpRegistry->ReadRegBool(name, index, default_value);
	}
	virtual void ReadRegString(const char *name, avf_u32_t index, char *value, const char *default_value) {
		return mpRegistry->ReadRegString(name, index, value, default_value);
	}
	virtual void ReadRegMem(const char *name, avf_u32_t index, avf_u8_t *mem, avf_size_t size) {
		return mpRegistry->ReadRegMem(name, index, mem, size);
	}

	virtual avf_status_t WriteRegInt32(const char *name, avf_u32_t index, avf_s32_t value) {
		return mpRegistry->WriteRegInt32(name, index, value);
	}
	virtual avf_status_t WriteRegInt64(const char *name, avf_u32_t index, avf_s64_t value) {
		return mpRegistry->WriteRegInt64(name, index, value);
	}
	virtual avf_status_t WriteRegBool(const char *name, avf_u32_t index, avf_int_t value) {
		return mpRegistry->WriteRegBool(name, index, value);
	}
	virtual avf_status_t WriteRegString(const char *name, avf_u32_t index, const char *value) {
		return mpRegistry->WriteRegString(name, index, value);
	}
	virtual avf_status_t WriteRegMem(const char *name, avf_u32_t index, const avf_u8_t *mem, avf_size_t size) {
		return mpRegistry->WriteRegMem(name, index, mem, size);
	}

	virtual avf_status_t LoadConfig(CConfig *pConfig, const char *root) {
		return mpRegistry->LoadConfig(pConfig, root);
	}
	virtual void ClearRegistry() {
		return mpRegistry->ClearRegistry();
	}

	virtual bool IsRealTime() {
		return mbRealTime;
	}

// ITimerManager
public:
	virtual avf_status_t SetTimer(IFilter *pFilter, 
		avf_enum_t id, avf_uint_t ms, bool bPeriodic);
	virtual avf_status_t KillTimer(IFilter *pFilter, avf_enum_t id);

// IActiveObject
public:
	virtual const char *GetAOName() {
		return mName->string();
	}
	virtual void AOLoop();
	virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);

private:
	void ClearMsgQ(CMsgQueue *pMsgQ, avf_queue_remove_msg_proc proc);
	static bool RemoveCmds(void *context, void *pMsg, avf_size_t size);
	void PostAppMsg(avf_uint_t code);
	void PostAppMsgLocked(avf_msg_t &msg);
	void ThreadLoop();

// IMediaControl
public:
	virtual bool RegisterComponent(IInterface *comp);
	virtual bool UnregisterComponent(IInterface *comp);

	virtual avf_status_t SetMediaSource(const char *pPlayer, const char *pMediaSrc, const char *pConfig, avf_intptr_t tag, int conf);
	virtual avf_status_t SetMediaState(avf_enum_t targetState, bool bWait);

	virtual void GetStateInfo(StateInfo& info);
	virtual avf_enum_t WaitState(avf_enum_t state, avf_status_t *result);
	virtual avf_status_t Command(int cmpid, avf_enum_t cmdId, avf_intptr_t cmdArg);
	virtual avf_status_t DumpState();

private:
	avf_status_t SetMediaSourceLocked(SetMediaSrcStruct *sms);

// ITimerReceiver
public:
	virtual void OnTimer(void *param1, avf_enum_t param2);

// IDiskNotify
public:
	virtual void OnDiskError();
	virtual void OnDiskSlow();

protected:
	void ProcessUserEvent(avf_uint_t event);
	void ProcessPlayerEventLocked(const avf_msg_t& msg);

// for derived classes
protected:
	INLINE void SetEventFlag(avf_enum_t event) {
		mpWorkQ->SetEventFlag(event);
	}

	INLINE avf_status_t SendCmd(avf_uint_t code) {
		return mpWorkQ->SendCmd(code, NULL);
	}

	INLINE void ReplyCmd(avf_status_t status) {
		mpWorkQ->ReplyCmd(status);
	}

	INLINE void PostAppMsgLocked(avf_uint_t code) {
		avf_msg_t msg(code);
		PostAppMsgLocked(msg);
	}

	void ChangeStateLocked(avf_enum_t newState, avf_status_t status = E_OK);
	INLINE void EnterErrorStateL(avf_status_t status) {
		ChangeStateLocked(STATE_ERROR, status);
	}

private:
	avf_status_t SetMediaStateLocked(avf_enum_t targetState);
	void RunStateMachineLocked();
	void IdleStateAction();
	void OpenStateAction();
	void PreparedStateAction();
	void RunningStateAction();
	void ErrorStateAction();

private:
	void OpenMediaLocked();
	void CloseMediaLocked();
	void PrepareMediaLocked();
	void RunMediaLocked();
	void StopMediaLocked();
	void ClearCmdMsgs();

private:
	INLINE void DeletePlayer() {
		avf_safe_release(mpPlayer);
		mTag = 0;
	}

protected:
	CConfig *mpConfig;
	IRegistry *mpRegistry;

	ap<string_t> mName;
	CTimerManager *mpTimerManager;

	avf_enum_t mPrevState;
	avf_enum_t mState;
	avf_status_t mStateResult;
	avf_enum_t mTargetState;
	bool mbCancel;

	CCondition mStateChangeCond;

	CMutex mMutex;
	CWorkQueue *mpWorkQ;		// work queue, including a thread and a cmd queue
	CMsgQueue *mpFilterMsgQ;	// from filters
	CMsgQueue *mpPlayerMsgQ;	// from player

	ap<string_t> mPlayerName;
	ap<string_t> mMediaSrc;
	avf_intptr_t mTag;
	int mConf;
	bool mbRealTime;

	IMediaPlayer *mpPlayer;

	CMutex mCompLock;
	TDynamicArray<IInterface*> mComponents;

	MediaControl_MsgProc mAppMsgProc;
	void *mAppMsgContext;
};


#endif

