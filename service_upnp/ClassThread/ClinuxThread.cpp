/*******************************************************************************
**       CLinuxThread.cpp
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
#include <unistd.h>
#include <fcntl.h>
#include "assert.h"
#include <string.h>
#include <cstdio>
#include "ClinuxThread.h"
#include <signal.h>
#ifdef PLATFORM_BUILD
#include <sys/prctl.h>
//#include "am_new.h"
#endif

#ifdef IOS_OS
using namespace SC;
#endif

static void segfault_handler(int signo)
{
	//AVF_LOGE("thread: %d", (int)pthread_self());
	//AVF_LOGE("stack trace");
	//AVF_LOGE("-----------");
	//dump_stack_trace();
	//AVF_LOGE("-----------");
	while(1)
	{
		sleep(1);
		printf("segfault_handler\n");
		sleep(1);
	}
	//exit(-1);
}


static int cthreadCounter = 0;

CThread::CThread
	(
		const char *pName,
		CThread::PRIORITY priority,
		unsigned int statckSize,
		void* pParam
	) : _pMainFunc(NULL),
	_bRuning(false)
{
	//printf("-- pName : %s\n", pName);
	cthreadCounter++;
	sprintf(_name, "%s", pName);
	_pPara = new ThreadFuncPara(this,pParam);
	//printf("-- _name : %s\n", _name);
}

CThread::~CThread()
{
	if(_bRuning)
		pthread_cancel(_hThread);
	delete _pPara;	
}

void CThread::Go()
{
	if(!_bRuning)
	{
		//printf("-- start %s CThread\n", _name);
		if(0 == pthread_create(&_hThread, NULL, ThreadFunc, (void*)_pPara))
		{
			_bRuning = true;
		}
		else
		{
			printf("-- start %s CThread failed\n", _name);
		}
	}
}

void CThread::TestCancel()
{
	pthread_testcancel();
}
void CThread::Pause()
{
	
}

void CThread::Stop()
{
	if(_bRuning)
	{
		//printf("CThread::Stop--%p\n", _name);
		printf("cancel thread : %s\n", _name);
		pthread_cancel(_hThread);
	}
	_bRuning = false;
}

void CThread::Finish()
{
	//pthread_join(_hThread, 0);
}

void CThread::main(void* pParam)
{
	
	if(_pMainFunc != NULL)
	{
		_pMainFunc((void*)pParam);
	}
	else
	{
		TRACE("main function should be overload in sub class or set call setmain function!!\n");
	}
}

void* CThread::ThreadFunc(void* lpParam )
{
	ThreadFuncPara para(((ThreadFuncPara*)lpParam)->self, ((ThreadFuncPara*)lpParam)->para);	
	if(para.self != NULL)
	{
#ifdef PLATFORM_BUILD
		//printf("--set thread name\n");
		::prctl(PR_SET_NAME, para.self->getName());
#endif
		//signal(SIGSEGV, segfault_handler);
		para.self->main(para.para);
	}
	return 0;
}


///////////////////////////////////////////////////////////////////////////
static int SemphoreCount = 0;
CSemaphore::CSemaphore
	(int counter
	,int maxcounter
	,const char* name
	) : _maxcounter(maxcounter)
	,_name(name)
    ,_count(0)
{
    {
    //sem_init(&_hSemaphore, 0, counter);
    sprintf(_semName, "Sem_%s_%d", _name, SemphoreCount);
    /*SemphoreCount++;
    printf("create sem: %s\n", _semName);
    sem_unlink(_semName);
    _pSemaphore = sem_open(_semName, O_CREAT, S_IRWXU|S_IRWXG, counter);
    if(_pSemaphore == SEM_FAILED)
        printf("--sem create failed");*/
    }
    pthread_cond_init(&_cond, NULL);
    pthread_mutex_init(&_mutex,NULL);
    _bTimeout = false;
    //_timerName = new char[40];
    //memset(_timerName, 0, sizeof(_timerName));
    
}

CSemaphore::~CSemaphore()
{
	//delete[] _timerName;
	//sem_destroy(&_hSemaphore);
    //sem_unlink(_semName);
    //if(_pSemaphore != SEM_FAILED)
    //    sem_close(_pSemaphore);
    pthread_cond_destroy(&_cond);
	pthread_mutex_destroy(&_mutex);	
}

int CSemaphore::Take(int usec)
{
    /*
	_bTimeout = false;
	int rt = 0;
	int timer_h = -1;
	if(usec > 0)
	{
		timer_h = CTimerThread::GetInstance()->ApplyTimer(usec /MIN_TIMER_UNIT, TimerGive, this , false, _semName);
	}
	//sem_wait(&_hSemaphore);
    sem_wait(_pSemaphore);
	pthread_mutex_lock(&_mutex);
	if(_bTimeout)
	{
		rt =  -1;
	}
	if( timer_h != -1)
	{
		CTimerThread::GetInstance()->CancelTimer(_semName);
		timer_h = -1;
	}
	pthread_mutex_unlock(&_mutex);*/
    if (_count > 0) {
        _count --;
        return _count;
    }
    else
    {
        int rt = 0;
        if(usec > 0)
        {
            struct timespec timeToWait;
            struct timeval now;
            gettimeofday(&now,NULL);
            timeToWait.tv_sec = now.tv_sec + usec / 1000000;
            timeToWait.tv_nsec = (now.tv_usec + (usec % 1000000))*1000;
            pthread_mutex_lock(&_mutex);
            rt = pthread_cond_timedwait(&_cond, &_mutex, &timeToWait);
            pthread_mutex_unlock(&_mutex);
        }
        else
        {
            pthread_mutex_lock(&_mutex);
            //while (!_count)
            rt = pthread_cond_wait(&_cond, &_mutex);
            pthread_mutex_unlock(&_mutex);
        }
        if(rt !=0)
        {
            //printf("--pthread_cond_timedwait : %d\n", rt);
            return -1;
        }
        else
        {
            _count --;
            return _count;
        }
    }
	//return  rt;
}
void CSemaphore::SetTimeOut(bool b)
{
	_bTimeout = b;
}

void CSemaphore::PrivateGive()
{
	//sem_post(&_hSemaphore);
    //sem_post(_pSemaphore);
    _count++;
    pthread_cond_signal(&_cond);
}

void CSemaphore::SetLock(bool block)
{
	if(block)
		pthread_mutex_lock(&_mutex);
	else
		pthread_mutex_unlock(&_mutex);
}
void CSemaphore::TimerGive(void* pSem)
{
	//TRACE("timer_give\n");
	((CSemaphore*)pSem)->SetLock(true);
	((CSemaphore*)pSem)->SetTimeOut(true);
	((CSemaphore*)pSem)->PrivateGive();
	((CSemaphore*)pSem)->SetLock(false);
}

void CSemaphore::Give()
{
	//TRACE("give\n");
	pthread_mutex_lock(&_mutex);
	//if(!_bTimeout)
		PrivateGive();
	pthread_mutex_unlock(&_mutex);
}

///////////////////////////////////////////////////////////////////////////
CTimerThread* CTimerThread::_pInstance = NULL;
void CTimerThread::Create()
{
	if(_pInstance == NULL)
	{
		_pInstance = new CTimerThread();
		_pInstance->Go();
	}		
}

void CTimerThread::Destroy()
{
	if(_pInstance)
	{	
		_pInstance->Stop();
		delete _pInstance;
	}
}

CTimerThread::CTimerThread
	() : CThread("timer thread", CThread::NORMAL, 0, NULL),
	_addedNumber(0)
{
	//if(0 != sem_init(&_hSemaphore, 0, 0))
	sem_unlink("timerThread");
	if(!(0 < (_pSemaphore = sem_open("timerThread", O_CREAT, 0, 0))))
	{
		printf("---CTimerThread sem init failed \n");
	}
	if(0 != pthread_mutex_init(&_mutex,NULL))
	{
		printf("---CTimerThread pthread_mutex_init failed \n");
	}	
	_timelace = MIN_TIMER_UNIT - TIMER_LACEADJUST;
}

CTimerThread::~CTimerThread()
{
	//sem_destroy(&_hSemaphore); 
	sem_close(_pSemaphore);
	pthread_mutex_destroy(&_mutex);	
}

int CTimerThread::ApplyTimer(int timeout, TIMER_CALL_BACK p, void* para, bool bloop, const char* name)
{
	int rt = -1;
	int i;
	for(i = 0; i < _addedNumber; i++)
	{
		if(strcmp(_timers[i].name, name) == 0)
			break;
	}	
	if(i < _addedNumber)
	{
		printf("timer exist\n");
		return rt;
	}
		
	pthread_mutex_lock(&_mutex);
	if(_addedNumber < MAX_TIMER_NUMBER)
	{
		_timers[_addedNumber].bLoop = bloop;
		_timers[_addedNumber].pCallback = p;
		_timers[_addedNumber].timerCounter = timeout;
		_timers[_addedNumber].timerIinitialCounter= timeout;
		_timers[_addedNumber].pPara = para;
		_timers[_addedNumber].name = name;
		_addedNumber++;
		
		rt =  _addedNumber -1;
	}
	pthread_mutex_unlock(&_mutex);

	//int tmp, tt;
		//sem_getvalue(&_hSemaphore, &tmp) ;
	//if((tt = sem_getvalue(&_hSemaphore, &tmp)) == 0)
	//{
	//printf("-- applytimer sem give\n");
	sem_post(_pSemaphore);
	//}
	//else
	//{
	//	TRACE("sem_getvalue return value:%d\n", tt);
	//}
	return rt;
}

int CTimerThread::CancelTimer(const char* name)
{	
	int i;
	for(i = 0; i < _addedNumber; i++)
	{
		if(strcmp(_timers[i].name, name) == 0)
			break;
	}	
	if(i < _addedNumber)	
	{	
		printf("cancel timer : %d,%s\n", i, name);
		pthread_mutex_lock(&_mutex);
		_timers[i].bLoop = false;
		_timers[i].pCallback = NULL;
		pthread_mutex_unlock(&_mutex);
	}
	return 0;
}

void CTimerThread::main(void *)
{
	while(1)
	{
		sem_wait(_pSemaphore);
		while(_addedNumber > 0)
		{
			_tt.tv_sec = 0;
			_tt.tv_usec = _timelace;
			select(0, NULL, NULL, NULL, &_tt);
			pthread_mutex_lock(&_mutex);
			for(int i = 0; i < _addedNumber; i++ )
			{
				_timers[i].timerCounter--;
				if(_timers[i].timerCounter <= 0)
				{
					if(_timers[i].pCallback != NULL)
						_timers[i].pCallback(_timers[i].pPara);
					if(!_timers[i].bLoop)
					{					
						DelTimer(i);					
						i--;
					}
					else
					{
						//printf("--t:%d \n", _addedNumber);
						_timers[i].timerCounter = _timers[i].timerIinitialCounter;
					}
				}		
			}		
			pthread_mutex_unlock(&_mutex);
		}

	}
}

int CTimerThread::DelTimer(int i)
{
	printf("---del timer %d \n", i);
	for(int j = i; j < _addedNumber-1 ; j ++)
	{
		_timers[j] = _timers[j + 1];
	}
	_addedNumber --;
	return _addedNumber;
}

void CTimerThread::Delay(int usec)
{
	timeval tt;
	tt.tv_sec = usec / 1000000;
	tt.tv_usec = usec%1000000;
	select(0, NULL, NULL, NULL, &tt);
}


CFile::CFile(char * path, File_Open_Mode mode)
{
	int mod;
	switch(mode)
	{
		case FILE_READ:
			mod = O_RDONLY;
			break;
		case FILE_WRITE:
			mod = O_RDWR|O_CREAT;
			break;
	}	
    _handle = open(path, mod, 0);
	if(_handle < 0)
	{
		printf("--open file failed: %s, :%d\n",path,_handle);
	}
}
CFile::~CFile()
{
    close(_handle);
}


int CFile::readfile(char* buffer, int length)
{
	return ::read(_handle, buffer, length); 
}

int CFile::writefile(char*buffer, int length)
{
	return ::write(_handle, buffer, length);
}

int CFile::write(BYTE*buffer, int length)
{
	return ::write(_handle, buffer, length);
}

int CFile::seek(int posi)
{
    lseek(_handle, posi, SEEK_SET);
	return 0;
}

int CFile::Append(BYTE*buffer, int length)
{
	lseek(_handle, 0, SEEK_END);
	return ::write(_handle, buffer, length);
}

LONGLONG CFile::read(LONGLONG length, BYTE* pbf, LONGLONG bfLength)
{
	ssize_t rt;
	if(bfLength >= length)
	{
		rt = ::read(_handle, pbf, length);
	}
	else
	{
		rt = ::read(_handle, pbf, bfLength);
	}
	
	return rt;
}

UINT64 CFile::GetLength()
{
	int c = lseek(_handle, 0, SEEK_CUR);
	UINT64 len = lseek(_handle, 0, SEEK_END);
	lseek(_handle, c, SEEK_SET);
	return len;
}

CMutex::CMutex(const char* name)
{
	_name = name;
	pthread_mutex_init(&_handle,NULL);
	//ASSERT(_handle != NULL);
}

CMutex::~CMutex()
{
	//ASSERT(_handle != NULL);
	pthread_mutex_destroy(&_handle);	
}

void CMutex::Take()
{
	//printf("---+--%s take, 0x%x\n", _name, &_handle);
	pthread_mutex_lock(&_handle);
}

void CMutex::Give()
{
	pthread_mutex_unlock(&_handle);
	//ASSERT(rt);
}

void  CSystemTime::GetSystemTime(UINT64 *t)
{
	*t = time(NULL);
}

void CSystemTime::GetSystemTime(UINT32 *sec_l, UINT32 *sec_h)
{
	*sec_l = time(NULL); 
	*sec_h = 0;
}

