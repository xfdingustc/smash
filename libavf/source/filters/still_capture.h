
#ifndef __STILL_CAPTURE_H__
#define __STILL_CAPTURE_H__

#include <mqueue.h>

class IVDBControl;
class CSocketEvent;
class CStillCapture;

//-----------------------------------------------------------------------
//
//  CStillCapture
//
//-----------------------------------------------------------------------

class CStillCapture: public CObject, public IStorageListener
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

	typedef struct cap_msg_s {
		int msg_id;
		int payload;
	} cap_msg_t;

public:
	static CStillCapture* Create(IEngine *pEngine);

private:
	CStillCapture(IEngine *pEngine);
	virtual ~CStillCapture();

public:
	avf_status_t Start();
	avf_status_t Stop();
	avf_status_t StillCapture(int action, int flags);
	avf_status_t GetStillCaptureInfo(IVideoControl::stillcap_info_s *info);

// IStorageListener
public:
	virtual void NotifyStorageState(int is_ready);

private:
	static void ThreadEntry(void *p) {
		((CStillCapture*)p)->ThreadLoop();
	}
	void ThreadLoop();
	void RegisterStorageListener(bool bRegister);
	IVDBControl *GetVdbControl();
	void SetStorageState(bool bReady);
	avf_status_t CheckStorageSpaceLocked(IVDBControl *pVdbControl, bool bSetError);

private:
	void ProcessOneMsg();
	void BadStatusMsg(int status);
	void HandleStatusMsg(int status);
	void StillCaptureStateChanged();
	void PictureTaken(int code);
	avf_status_t SendMsg(cap_msg_t& msg);
	avf_status_t SendSDChangedMsg(bool bReady);

private:
	enum State {
		STATE_IDLE = 0,
		STATE_RUNNING = 1,
		STATE_STOPPING = 2,
	};

private:
	CMutex mMutex;
	CMutex mWriteMutex;
	IEngine *mpEngine;
	CSocketEvent *mpEvent;
	avf_u64_t m_burst_start_tick;
	bool mbIsRaw;
	bool mbStorageReady;
	int m_last_error;
	int mNumPictures;
	CThread *mpThread;
	State mState;
	int mCapState;
	mqd_t m_sendq;
	mqd_t m_recvq;
};

#endif

