
#define LOG_TAG "timer_manager"

#include <signal.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_if.h"
#include "avf_base_if.h"
#include "timer_manager.h"

static CMutex g_timer_lock;
static CTimerManager *g_timer_manager = NULL;
static int g_timer_refcount = 0;

//-----------------------------------------------------------------------
//
//  CTimerManager
//
//-----------------------------------------------------------------------
CTimerManager* CTimerManager::Create()
{
	AUTO_LOCK(g_timer_lock);
	if (g_timer_refcount == 0) {
		g_timer_manager = avf_new CTimerManager();
		if (g_timer_manager != NULL)
			g_timer_refcount = 1;
	} else {
		g_timer_refcount++;
	}
	return g_timer_manager;
}

void CTimerManager::Release()
{
	AUTO_LOCK(g_timer_lock);
	if (--g_timer_refcount == 0) {
		avf_delete g_timer_manager;
		g_timer_manager = NULL;
	}
}

CTimerManager::CTimerManager()
{
	mpTimerThread = NULL;
	mbQuitThread = false;
	mTimers._Init();
}

CTimerManager::~CTimerManager()
{
	if (mpTimerThread) {
		mbQuitThread = true;
		mTimerCond.Signal();
		mpTimerThread->Release();
	}
	mTimers._Release();
}

int CTimerManager::FindTimerLocked(ITimerReceiver *pReceiver, void *param1, avf_enum_t param2)
{
	Timer *timer = mTimers.elems;
	for (avf_uint_t i = 0; i < mTimers.count; i++, timer++) {
		if (timer->pReceiver == pReceiver && timer->param1 == param1 && timer->param2 == param2)
			return i;
	}
	return -1;
}

CTimerManager::Timer* CTimerManager::AllocTimerLocked()
{
	return mTimers._Append(NULL);
}

void CTimerManager::RemoveTimerLocked(avf_uint_t index)
{
	mTimers._Remove(index);
}

// return next target tick delta
avf_u32_t CTimerManager::CheckTimersLocked()
{
	avf_u64_t curr_tick = avf_get_curr_tick();
	avf_u64_t next_tick = curr_tick + 10*60*1000;	// 10 minutes

	Timer *timer = mTimers.elems;
	for (avf_uint_t i = 0; i < mTimers.count; i++, timer++) {
		if (timer->targetTick > curr_tick) {
			if (next_tick > timer->targetTick)
				next_tick = timer->targetTick;
		} else {
			// fire the timer
			if (!timer->bDispatched) {
				timer->pReceiver->OnTimer(timer->param1, timer->param2);
			}
			// remove the timer
			if (!timer->bPeriodic) {
				RemoveTimerLocked(i);
				i--; timer--;
			} else {
				timer->targetTick += timer->ms;
				timer->bDispatched = true;
				if (next_tick > timer->targetTick)
					next_tick = timer->targetTick;
			}
		}
	}

	if (next_tick < curr_tick)
		return 0;

	return (avf_u32_t)(next_tick - curr_tick);
}

void CTimerManager::ThreadEntry(void *p)
{
	((CTimerManager*)p)->ThreadLoop();
}

void CTimerManager::ThreadLoop()
{
	AUTO_LOCK(mMutex);
	while (!mbQuitThread) {
		if (mTimers.count > 0) {
			avf_u32_t ms_sleep = CheckTimersLocked();
			if (mTimers.count > 0)
				mTimerCond.TimedWait(mMutex, ms_sleep);
			else
				mTimerCond.Wait(mMutex);
		} else {
			mTimerCond.Wait(mMutex);
		}
	}
}

void CTimerManager::RescheduleTimerLocked()
{
	if (mTimers.count > 0) {
		if (mpTimerThread == NULL) {
			mpTimerThread = CThread::Create("TimerThread", ThreadEntry, this);
			if (mpTimerThread) {
				mpTimerThread->SetThreadPriority(CThread::PRIO_MANAGER);
			}
		} else {
			mTimerCond.Signal();
		}
	}
}

avf_status_t CTimerManager::SetTimer(ITimerReceiver *pReceiver,
	void *param1, avf_enum_t param2, 
	avf_uint_t ms, bool bPeriodic)
{
	AUTO_LOCK(mMutex);

	if (ms == 0) {
		AVF_LOGE("bad timer");
		return E_INVAL;
	}

	Timer *timer;

	int index = FindTimerLocked(pReceiver, param1, param2);
	if (index >= 0) {
		timer = mTimers.elems + index;
	} else {
		timer = AllocTimerLocked();
		if (timer == NULL) {
			AVF_LOGE("no empty timer slot!");
			return E_ERROR;
		}
		timer->pReceiver = pReceiver;
		timer->param1 = param1;
		timer->param2 = param2;
	}

	timer->ms = ms;
	timer->bPeriodic = bPeriodic;
	timer->bDispatched = false;
	timer->targetTick = avf_get_curr_tick() + ms;

	AVF_LOGD("SetTimer %p %p %d", pReceiver, param1, param2);

	RescheduleTimerLocked();

	return E_OK;
}

avf_status_t CTimerManager::KillTimer(ITimerReceiver *pReceiver, void *param1, avf_enum_t param2)
{
	AUTO_LOCK(mMutex);

	int index = FindTimerLocked(pReceiver, param1, param2);
	if (index < 0) {
		AVF_LOGE("KillTimer, no such timer: %p %p %d", pReceiver, param1, param2);
		return E_ERROR;
	}

	RemoveTimerLocked(index);

	AVF_LOGD("KillTimer %p %p %d", pReceiver, param1, param2);

	RescheduleTimerLocked();

	return E_OK;
}

avf_status_t CTimerManager::ResetTimer(ITimerReceiver *pReceiver, void *param1, avf_enum_t param2)
{
	AUTO_LOCK(mMutex);

	int index = FindTimerLocked(pReceiver, param1, param2);
	if (index < 0) {
		AVF_LOGW("ResetTimer, no such timer: %p %p %d", pReceiver, param1, param2);
		return E_ERROR;
	}

	Timer *timer = mTimers.elems + index;
	if (timer->bDispatched) {
		timer->bDispatched = false;
	} else {
		AVF_LOGW("timer already reset");
	}

	return E_OK;
}

avf_uint_t CTimerManager::ClearTimers(ITimerReceiver *pReceiver)
{
	AUTO_LOCK(mMutex);

	avf_uint_t n = 0;
	Timer *timer = mTimers.elems;
	for (avf_uint_t i = 0; i < mTimers.count; i++, timer++) {
		if (timer->pReceiver == pReceiver) {
			RemoveTimerLocked(i);
			i--; timer--;
			n++;
		}
	}

	return n;
}

