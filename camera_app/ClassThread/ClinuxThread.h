/*******************************************************************************
**      ClinuxThread.h
**
**      
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a Jun-28-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/

#ifndef __H_CLinuxThread__
#define __H_CLinuxThread__

#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/types.h>

typedef unsigned char       UINT8;  /**< UNSIGNED 8-bit data type */
typedef unsigned short      UINT16; /**< UNSIGNED 16-bit data type */
typedef unsigned int        UINT32; /**< UNSIGNED 32-bit data type */
typedef unsigned long long  UINT64; /**< UNSIGNED 64-bit data type */
typedef unsigned short      WORD;   /**< UNSIGNED 64-bit data type */
typedef unsigned int        DWORD;  /**< UNSIGNED 64-bit data type */
typedef unsigned char       BYTE;   /**< UNSIGNED 8-bit data type */
typedef long                LONG;
typedef long long           LONGLONG; /**< UNSIGNED 64-bit data type */
typedef long                INT32;

//#define PLATFORM_BUILD
typedef void (*RUN_FUN_PTR)(void*);
#define TRACE  printf
//#define TRACE(format, args...)
//#define TRACE1  printf
#define TRACE1(format, args...)

#ifdef IOS_OS
namespace SC {
#endif

class CThread
{
public:
    enum PRIORITY
    {
        HIGH   = 100,
        NORMAL = 150,
        LOW    = 200
    };
    CThread(const char *pName = "no name thread");
    virtual ~CThread();
    void Go();
    void Pause();
    virtual void Stop();
    void Finish();
    static void TestCancel();
    char* getName(){return _name;};

protected:
    virtual void main() = 0;

private:
    static void* ThreadFunc(void* lpParam );
    pthread_t           _hThread;
    int                 _bRuning;
    char                _name[128];
};

class CSemaphore
{
public:
    CSemaphore(int counter, int maxcounter, const char* name);
    ~CSemaphore();
    int  Take(int timeout);
    void Give();
private:
    void SetTimeOut(bool b);
    void PrivateGive();
    static void TimerGive(void* pSem);
    void SetLock(bool block);

private:
    pthread_cond_t   _cond;
    sem_t            *_pSemaphore;
    pthread_mutex_t  _mutex;
    bool             _bTimeout;
    int              _maxcounter;
    char             _semName[128];
    const char       *_name;
    int              _count;
};


typedef void (*TIMER_CALL_BACK) (void*);
#define MAX_TIMER_NUMBER        20
#define MIN_TIMER_UNIT          100000  // Usecond
#define TIMER_LACEADJUST        900
class CTimerThread : public CThread
{
public:
    static void Create();
    static void Destroy();
    static CTimerThread* GetInstance(){return _pInstance;};
    virtual ~CTimerThread();
    int ApplyTimer
    (int timeout1PercentageSec
     , TIMER_CALL_BACK p
     , void* para
     , bool bloop
     , const char* name);
    int CancelTimer(const char* name);

    static void Delay(int usec);

protected:
    virtual void main();
private:
    int DelTimer(int hadle);
    CTimerThread();

private:
    static CTimerThread* _pInstance;
    struct Timer_s
    {
        int     timerHandle;
        int     timerIinitialCounter;
        TIMER_CALL_BACK pCallback;
        void    *pPara;
        bool    bLoop;
        int     timerCounter;
        const char  *name;
    };
    Timer_s             _timers[MAX_TIMER_NUMBER];
    int                 _addedNumber;
    sem_t               _hSemaphore;
    sem_t               *_pSemaphore;
    pthread_mutex_t     _mutex;
    int                 _timelace;
    timeval             _tt;
};

class CFile
{
public:
    enum File_Open_Mode
    {
        FILE_READ = 0,
        FILE_WRITE,
    };
    CFile(const char* path, File_Open_Mode mode);
    ~CFile();

    int readfile(char* buffer, int length);
    LONGLONG read(LONGLONG length, BYTE* pBuffer, LONGLONG bfLength);

    UINT64 GetLength();

    int writefile(char *buffer, int length);
    int write(BYTE *buffer, int length);
    int Append(BYTE*buffer, int length);
    int seek(int posi);
    bool IsValid() { return (_handle > 0); }

private:
    int  _handle;
};


class CMutex
{
public:
    CMutex(const char* name);
    ~CMutex();
    void Take();
    void Give();
private:
    pthread_mutex_t _handle;
    const char* _name;
};

class AutoLock
{
public:
    inline AutoLock(CMutex& mutex) : mMutex(mutex) {
        mMutex.Take();
    }

    inline AutoLock(CMutex* pMutex) : mMutex(*pMutex) {
        mMutex.Take();
    }

    inline ~AutoLock() {
        mMutex.Give();
    }

private:
    CMutex& mMutex;
};

class CSystemTime
{
public:
    CSystemTime(){};
    ~CSystemTime(){};
    static void  GetSystemTime(UINT64 *t);
    static void GetSystemTime(UINT32 *sec_l, UINT32 *sec_h);
private:

};

#ifdef IOS_OS
}
#endif

#endif
