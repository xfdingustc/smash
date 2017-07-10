/*****************************************************************************
 * class_PropsEventThread.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2014, linnsong.
 *
 *
 *****************************************************************************/
#ifndef __H_class_PropEventThread__
#define __H_class_PropEventThread__

#include "GobalMacro.h"
#include "ClinuxThread.h"

#define propEventQueueDeepth 10
#define PEHMaxNumber 6
typedef enum changed_property_type
{
	property_type_wifi = 0,
	property_type_file,
	property_type_audio,
	property_type_Bluetooth,
	property_type_storage,
	property_type_power,
	property_type_num,
}changed_property_type;

class CPropertyEventHandler
{
public:
	CPropertyEventHandler(){};
	virtual ~CPropertyEventHandler(){};
	virtual void EventPropertyUpdate(changed_property_type state) = 0;
};

class CPropsEventThread :public CThread
{
public:
	static void Create();
	static void Destroy();
	static CPropsEventThread* getInstance();

	static int WifiPropertyWaitCB(const char *key, const char *value);
	static int FilePropertyWaitCB(const char *key, const char *value);
	static int AudioPropertyWaitCB(const char *key, const char *value);
	static int PowerPropertyWaitCB(const char *key, const char *value);
	static int StoragePropertyWaitCB(const char *key, const char *value);

	void AddPEH(CPropertyEventHandler* pHandle);
	void clearHandle();
	
protected:
	virtual void main(void *);

private:
	CPropsEventThread();
	~CPropsEventThread();

	void propertyChanged(changed_property_type type);

private:
	static CPropsEventThread*	_pInstance;

	bool						_bRun;

	CPropertyEventHandler*		_pPropertyEventHandlers[PEHMaxNumber];
	int							_PEHnumber;

	CMutex 						*_mutex;
	CSemaphore					*_pTmpPropertyChanged;

	changed_property_type		_typeQueue[propEventQueueDeepth];
	int 						_queueNum;
	int 						_queueIndex;

	CMutex 						*_mutexForHandler;
};

#endif
