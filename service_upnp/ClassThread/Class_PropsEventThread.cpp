#include <stdio.h>
#include "agbox.h"
#include "Class_PropsEventThread.h"

CPropsEventThread* CPropsEventThread::_pInstance = NULL;
void CPropsEventThread::Create()
{
	if(_pInstance == NULL)
	{
		_pInstance = new CPropsEventThread();
		_pInstance->Go();
	}
}

void CPropsEventThread::Destroy()
{
	if(_pInstance)
	{
		printf("CPropsEventThread::Destroy\n");
		_pInstance->clearHandle();
		_pInstance->Stop();
		delete _pInstance;
		_pInstance = NULL;
	}
}

CPropsEventThread* CPropsEventThread::getInstance()
{
	return _pInstance;
}

int CPropsEventThread::WifiPropertyWaitCB(const char *key, const char *value)
{
	if(_pInstance)
		_pInstance->propertyChanged(property_type_wifi);
	return 0;
}

int CPropsEventThread::FilePropertyWaitCB(const char *key, const char *value)
{
	if(_pInstance)
		_pInstance->propertyChanged(property_type_file);
	return 0;
}

int CPropsEventThread::AudioPropertyWaitCB(const char *key, const char *value)
{
	if(_pInstance)
		_pInstance->propertyChanged(property_type_audio);
	return 0;
}

int CPropsEventThread::PowerPropertyWaitCB(const char *key, const char *value)
{
	if(_pInstance)
		_pInstance->propertyChanged(property_type_power);
	return 0;
}
int CPropsEventThread::StoragePropertyWaitCB(const char *key, const char *value)
{
	if(_pInstance)
		_pInstance->propertyChanged(property_type_storage);
	return 0;
}

void CPropsEventThread::propertyChanged(changed_property_type type)
{
	_mutex->Take();
	if(_queueNum > propEventQueueDeepth)
	{
		printf("event queue full\n");
	}
	else
	{
		_typeQueue[(_queueIndex + _queueNum) % propEventQueueDeepth] = type;
		_queueNum++;
	}
	_mutex->Give();
	_pTmpPropertyChanged->Give();
}

CPropsEventThread::CPropsEventThread
	(): CThread("propEventThread", CThread::NORMAL, 0, NULL)
	,_queueNum(0)
	,_queueIndex(0)
	,_PEHnumber(0)
{
	_mutex = new CMutex("CPropsEventThread mutex");
	_mutexForHandler = new CMutex("CPropsEventThread handlerMutex");
	_pTmpPropertyChanged = new CSemaphore(0, 1, "CPropsEventThread sem");
}

CPropsEventThread::~CPropsEventThread()
{
	delete _mutex;
	delete _mutexForHandler;
	delete _pTmpPropertyChanged;
}

void CPropsEventThread::AddPEH(CPropertyEventHandler* pHandle)
{
	_mutexForHandler->Take();
	if(_PEHnumber < PEHMaxNumber)
	{
		_pPropertyEventHandlers[_PEHnumber] = pHandle;
		_PEHnumber++;
	}else
	{
		printf("add too much PEH\n");
	}
	_mutexForHandler->Give();
}

void CPropsEventThread::clearHandle()
{
	_mutexForHandler->Take();
	//for(_pPropertyEventHandlers[_PEHnumber])
	_PEHnumber = 0;
	_mutexForHandler->Give();
}

void CPropsEventThread::main(void *)
{
	struct agcmd_property_wait_info_s *pwait_info;
	struct agcmd_property_wait_info_s *pwait_info1;
	struct agcmd_property_wait_info_s *pwait_info2;
	struct agcmd_property_wait_info_s *pwait_info3;
	struct agcmd_property_wait_info_s *pwait_info4;
	struct agcmd_property_wait_info_s *pwait_info5;

	
	pwait_info = agcmd_property_wait_start(LastFilePropertyName, FilePropertyWaitCB);
	pwait_info1 = agcmd_property_wait_start(audioStatusPropertyName, AudioPropertyWaitCB);
	pwait_info2 = agcmd_property_wait_start(wifiStatusPropertyName, WifiPropertyWaitCB);
	pwait_info3 = agcmd_property_wait_start(WIFI_SWITCH_STATE, WifiPropertyWaitCB);
	pwait_info4 = agcmd_property_wait_start(powerstateName, PowerPropertyWaitCB);
	pwait_info5 = agcmd_property_wait_start(SotrageStatusPropertyName, StoragePropertyWaitCB);
	_bRun = true;
	//printf("--event thread start running\n");
	while(_bRun)
	{
		_pTmpPropertyChanged->Take(-1);	
		//printf("--state change trig\n");
		while(_queueNum > 0)
		{
			_mutexForHandler->Take();
			for(int i = 0; i < _PEHnumber; i++)
			{
				_pPropertyEventHandlers[i]->EventPropertyUpdate(_typeQueue[_queueIndex]);
			}
			_mutexForHandler->Give();
			_mutex->Take();
			_queueIndex++;
			_queueIndex = _queueIndex % propEventQueueDeepth;
			_queueNum --;
			_mutex->Give();
		}
	}
	agcmd_property_wait_stop(pwait_info);
	agcmd_property_wait_stop(pwait_info1);
	agcmd_property_wait_stop(pwait_info2);
	agcmd_property_wait_stop(pwait_info3);
	agcmd_property_wait_stop(pwait_info4);
	agcmd_property_wait_stop(pwait_info5);
}
