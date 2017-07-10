
#define LOG_TAG "osal"

#include <signal.h>
#include <pthread.h>

#ifdef MINGW
#include <_timeval.h>
#include <pthread_time.h>
#endif

#if defined(IOS_OS) || defined(WIN32_OS) || defined(MAC_OS)
#undef THREAD_HAS_NAME
#else
#define THREAD_HAS_NAME
#endif

#ifdef THREAD_HAS_NAME
#include <sys/prctl.h>
#endif

#if defined(IOS_OS) || defined(MAC_OS)
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#include "avf_common.h"
#include "avf_osal.h"
#include "avf_new.h"
#include "avf_util.h"

#ifdef LINUX_OS
#include <execinfo.h>
#endif

#if defined(AVF_DEBUG) || defined(LINUX_OS)
#define DEBUG_THREAD
#endif


void avf_die(const char *file, int line);
void avf_die_info();

#ifdef DEBUG_THREAD

static void segfault_handler(int signo)
{
	char name[32];
	CThread::GetThreadName(name);
	AVF_LOGE("thread: %s(%x), signal %d", name, (int)pthread_self(), signo);
	avf_die_info();
}

#endif

//-----------------------------------------------------------------------
//
// CConditionBase
//
//-----------------------------------------------------------------------

CConditionBase::CConditionBase()
{
#if defined(ANDROID_OS) || defined(IOS_OS) || defined(WIN32_OS) || defined(MAC_OS)
	pthread_cond_init(&mCondition, NULL);
#else
	pthread_condattr_t attr;
	pthread_condattr_init(&attr);
	pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	pthread_cond_init(&mCondition, &attr);
	pthread_condattr_destroy(&attr);
#endif
}

// return false: timed out
bool CConditionBase::TimedWait(CMutex& mutex, avf_uint_t ms)
{

#if defined(IOS_OS) || defined(MAC_OS)
    struct timespec ts;
    avf_uint_t sec;
	avf_uint_t nsec;
    static mach_timebase_info_data_t    sTimebaseInfo;
    uint64_t        startNano;
    uint64_t start = mach_absolute_time();
    if ( sTimebaseInfo.denom == 0 ) {
        (void) mach_timebase_info(&sTimebaseInfo);
    }
    startNano = start * sTimebaseInfo.numer / sTimebaseInfo.denom;
    
    ts.tv_sec = startNano / 1000000000;
	ts.tv_nsec = startNano % 1000000000;
    
    sec = ms / 1000;
	nsec = (ms % 1000) * 1000000;
    
    ts.tv_sec += sec;
	ts.tv_nsec += nsec;
    
    if (ts.tv_nsec >= 1000000000) {
		ts.tv_nsec -= 1000000000;
		ts.tv_sec++;
	}
    
    return pthread_cond_timedwait(&mCondition, &mutex.mMutex, &ts) == 0;

#else

	struct timespec ts;
	avf_uint_t sec;
	avf_uint_t nsec;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	sec = ms / 1000;
	nsec = (ms % 1000) * 1000000;

	ts.tv_sec += sec;
	ts.tv_nsec += nsec;

	if (ts.tv_nsec >= 1000000000) {
		ts.tv_nsec -= 1000000000;
		ts.tv_sec++;
	}

	return pthread_cond_timedwait(&mCondition, &mutex.mMutex, &ts) == 0;

#endif
}

//-----------------------------------------------------------------------
//
// CThread
//
//-----------------------------------------------------------------------

CThread* CThread::Create(const char *name, avf_thread_entry_t entry, void *pParam, int b_detached)
{
	CThread *result = avf_new CThread(name, entry, pParam, b_detached);
	CHECK_STATUS(result);
	return result;
}

void CThread::Release()
{
	avf_delete this;
}

CThread::CThread(const char *name, avf_thread_entry_t entry, void *pParam, int b_detached)
{
	mpName = name;
	mEntry = entry;
	mpParam = pParam;
	mStatus = E_OK;
	mb_detached = b_detached;

	pthread_attr_t attr;
	pthread_attr_t *pAttr = NULL;

	if (b_detached) {
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pAttr = &attr;
	}

	int status = pthread_create(&mThread, pAttr, StaticEntry, (void*)this);

	if (pAttr) {
		pthread_attr_destroy(pAttr);
	}

	if (status != 0) {
		AVF_LOGP("create thread %s failed, error = %d", name, status);
		mStatus = E_NOMEM;
	}
}

CThread::~CThread()
{
	if (mStatus == E_OK && !mb_detached) {
		pthread_join(mThread, NULL);
	}
}

void* CThread::StaticEntry(void *pParam)
{
	CThread *pthis = reinterpret_cast<CThread*>(pParam);

#ifdef THREAD_HAS_NAME
	::prctl(PR_SET_NAME, pthis->mpName);
#endif

#ifdef MAC_OS
	::signal(SIGPIPE, SIG_IGN);
#endif

#ifdef DEBUG_THREAD
	::signal(SIGBUS, segfault_handler);
	::signal(SIGSEGV, segfault_handler);
	::signal(SIGFPE, segfault_handler);
	::signal(SIGABRT, segfault_handler);
#endif

	pthis->mEntry(pthis->mpParam);
	return NULL;
}

void CThread::GetThreadName(char name[32])
{
#ifdef THREAD_HAS_NAME
	::prctl(PR_GET_NAME, name);
#else
	name[0] = 0;
#endif
}

void CThread::SetThreadName(const char *name)
{
#ifdef THREAD_HAS_NAME
	::prctl(PR_SET_NAME, name);
#endif
}

void CThread::SetThreadPriority(avf_enum_t priority)
{
#ifdef LINUX_OS
	if (mStatus == E_OK) {
		struct sched_param sp = {0};
		sp.sched_priority = priority;
		int rval = pthread_setschedparam(mThread, SCHED_RR, &sp);
		if (rval != 0) {
			AVF_LOGW("pthread_setschedparam returns %d", rval);
		} else {
			AVF_LOGD(C_WHITE "set thread '%s' priority to %d" C_NONE, mpName, priority);
		}
	}
#endif
}

void avf_die(const char *file, int line)
{
	if (file) {
		AVF_LOGE("fatal error at %s, line %d", file, line);
	} else {
		AVF_LOGE("fatal error at line %d", line);
	}
	avf_die_info();
}

void avf_die_info()
{
#ifdef THREAD_HAS_NAME
	char name[32];
	::prctl(PR_GET_NAME, name);
	AVF_LOGI(C_YELLOW "offending thread: %s" C_NONE, name);
#endif

#ifdef AVF_DEBUG
	AVF_LOGI(C_YELLOW "waiting gdb, pid: %d" C_NONE, getpid());
	while (1) {
		avf_msleep(100);
	}
#else

#ifdef DEBUG_THREAD
#define N_PTR	64
	{
		void *buffer[N_PTR];
		int nptrs = backtrace(buffer, N_PTR);
		AVF_LOGI("backtrace %d", nptrs);
		for (int i = 0; i < nptrs; i++) {
			AVF_LOGI("%d : %p", i, buffer[i]);
		}
	}
#endif
	*(int*)0 = 0;
#endif
}

