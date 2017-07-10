
#ifndef __AVF_OSAL_H__
#define __AVF_OSAL_H__

class CMutex;
class CConditionBase;
class CCondition;
class CSemaphore;
class CThread;
class CAutoLock;

typedef void (*avf_thread_entry_t)(void*);

extern "C" void avf_socket_info(void);

#include "avf_osal_linux.h"

//-----------------------------------------------------------------------
//
// CAutoLock
//
//-----------------------------------------------------------------------
class CAutoLock
{
public:
	INLINE CAutoLock(CMutex& mutex) : mMutex(mutex) {
		mMutex.Lock();
	}

	INLINE CAutoLock(CMutex* pMutex) : mMutex(*pMutex) {
		mMutex.Lock();
	}

	INLINE ~CAutoLock() {
		mMutex.Unlock();
	}

private:
	CMutex& mMutex;
};

#define AUTO_LOCK(_mutex) \
	CAutoLock __lock(_mutex)

#endif

