
#ifndef __TIMER_MANAGER_H__
#define __TIMER_MANAGER_H__

#include "avf_util.h"

//-----------------------------------------------------------------------
//
//  CTimerManager
//
//-----------------------------------------------------------------------
class CTimerManager
{
public:
	static CTimerManager* Create();
	void Release();

private:
	CTimerManager();
	~CTimerManager();

public:
	avf_status_t SetTimer(ITimerReceiver *pReceiver, void *param1, avf_enum_t param2, 
		avf_uint_t ms, bool bPeriodic);
	avf_status_t KillTimer(ITimerReceiver *pReceiver, void *param1, avf_enum_t param2);
	avf_status_t ResetTimer(ITimerReceiver *pReceiver, void *param1, avf_enum_t param2);
	avf_uint_t ClearTimers(ITimerReceiver *pReceiver);

private:
	struct Timer {
		ITimerReceiver *pReceiver;
		void *param1;
		avf_enum_t param2;
		avf_uint_t ms;
		bool bPeriodic;
		bool bDispatched;
		avf_u64_t targetTick;
	};

	Timer* AllocTimerLocked();
	void RemoveTimerLocked(avf_uint_t index);

	int FindTimerLocked(ITimerReceiver *pReceiver, void *param1, avf_enum_t param2);
	avf_u32_t CheckTimersLocked();
	void RescheduleTimerLocked();

	static void ThreadEntry(void *p);
	void ThreadLoop();

private:
	CMutex mMutex;
	CConditionBase mTimerCond;
	CThread *mpTimerThread;
	bool mbQuitThread;
	TDynamicArray<Timer> mTimers;
};

#endif

