
#ifndef __AVF_OSAL_LINUX_H__
#define __AVF_OSAL_LINUX_H__

#include <semaphore.h>
#include <pthread.h>

INLINE void avf_atomic_set(avf_atomic_t *p, int value)
{
	*p = value;
}

INLINE int avf_atomic_get(avf_atomic_t *p)
{
	return *p;
}

INLINE int avf_atomic_inc(avf_atomic_t *p)
{
	return __sync_add_and_fetch(p, 1);
}

INLINE int avf_atomic_dec(avf_atomic_t *p)
{
	return __sync_fetch_and_sub(p, 1);
}

INLINE int avf_atomic_add(avf_atomic_t *p, int size)
{
	return __sync_add_and_fetch(p, size);
}

INLINE int avf_atomic_sub(avf_atomic_t *p, int size)
{
	return __sync_sub_and_fetch(p, size);
}

// Returns the number of trailing 0-bits in x, 
// starting at the least significant bit position. 
// If x is 0, the result is undefined.
INLINE int avf_ctz(unsigned int x)
{
	return __builtin_ctz(x);
}


//-----------------------------------------------------------------------
//
// CMutex
//
//-----------------------------------------------------------------------
class CMutex
{
	friend class CConditionBase;

public:
	INLINE explicit CMutex() {
		pthread_mutex_init(&mMutex, NULL);
	}

	INLINE ~CMutex() {
		pthread_mutex_destroy(&mMutex);
	}

	INLINE void Lock() {
		pthread_mutex_lock(&mMutex);
	}

	INLINE void Unlock() {
		pthread_mutex_unlock(&mMutex);
	}

private:
	pthread_mutex_t mMutex;
};

//-----------------------------------------------------------------------
//
// CConditionBase
//
//-----------------------------------------------------------------------
class CConditionBase
{
public:
	CConditionBase();

	INLINE ~CConditionBase() {
		pthread_cond_destroy(&mCondition);
	}

	INLINE void Wait(CMutex& mutex) {
		avf_status_t status = (avf_status_t)pthread_cond_wait(&mCondition, &mutex.mMutex);
		AVF_ASSERT_OK(status);
	}

	// for CSemaphore
	bool TimedWait(CMutex& mutex, avf_uint_t ms);

	INLINE void Signal() {
		avf_status_t status = (avf_status_t)pthread_cond_signal(&mCondition);
		AVF_ASSERT_OK(status);
	}

	INLINE void Broadcast() {
		avf_status_t status = (avf_status_t)pthread_cond_broadcast(&mCondition);
		AVF_ASSERT_OK(status);
	}

private:
	pthread_cond_t mCondition;
};

//-----------------------------------------------------------------------
//
// CCondition
//
//-----------------------------------------------------------------------
class CCondition
{
public:
	CCondition() {}
	~CCondition() {}

public:
	INLINE void Wait(CMutex& mutex) {
		mCondition.Wait(mutex);
	}

	INLINE void Wakeup() {
		mCondition.Signal();
	}

	INLINE void WakeupAll() {
		mCondition.Broadcast();
	}

private:
	CConditionBase mCondition;
};

//-----------------------------------------------------------------------
//
//  CSemaphore
//
//-----------------------------------------------------------------------
class CSemaphore
{
public:
	CSemaphore(): mbTimedWaiters(false) {}
	~CSemaphore() {}

public:
	INLINE void Wait(CMutex& mutex) {
		mCondition.Wait(mutex);
	}

	INLINE bool TimedWait(CMutex& mutex, int timeout) {
		mbTimedWaiters = true;
		return mTimedContion.TimedWait(mutex, timeout);
	}

	// wakeup one waiter, or all timed waiters
	INLINE void Wakeup() {
		if (mbTimedWaiters) {
			mbTimedWaiters = false;
			mTimedContion.Broadcast();
		} else {
			mCondition.Signal();
		}
	}

private:
	bool mbTimedWaiters;
	CConditionBase mCondition;
	CConditionBase mTimedContion;
};

//-----------------------------------------------------------------------
//
// CThread
//
//-----------------------------------------------------------------------

class CThread
{
public:
	enum {
		PRIO_LOWER = 1,
		PRIO_LOW = 5,
		PRIO_MEDIUM = 10,
		PRIO_HIGH = 15,
		PRIO_HIGHER = 20,
		PRIO_MANAGER = 25,
	};

	enum {
		PRIO_SOURCE = PRIO_HIGHER, //PRIO_MEDIUM,
		PRIO_CODEC = PRIO_HIGH,
		PRIO_SINK = PRIO_MEDIUM,	//PRIO_HIGHER,
		PRIO_WRITER = PRIO_LOW,
		PRIO_RENDERER = PRIO_LOW,
	};

	enum {
		DETACHED = 1,
	};

public:
	static CThread* Create(const char *name, avf_thread_entry_t entry, void *pParam, int b_detached = 0);
	void Release();

public:
	static void GetThreadName(char name[32]);
	static void SetThreadName(const char *name);
	pthread_t GetThreadId() {
		return mThread;
	}

private:
	CThread(const char *name, avf_thread_entry_t entry, void *pParam, int b_detached);
	~CThread();
	static void* StaticEntry(void *); 

public:
	INLINE avf_status_t GetStatus() { return mStatus; }
	INLINE const char *GetName() { return mpName; }
	void SetThreadPriority(avf_enum_t priority);

private:
	avf_status_t Status() { return mStatus; }
	avf_status_t mStatus;
	const char *mpName;
	avf_thread_entry_t mEntry;
	void *mpParam;
	pthread_t mThread;
	int mb_detached;
};

#endif

