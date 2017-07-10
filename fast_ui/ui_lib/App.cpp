/*******************************************************************************
**      App.cpp
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Description:
**
**      Revision History:
**      -----------------
**      01a 27- 8-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/

/*******************************************************************************
* <Includes>
*******************************************************************************/
#ifdef WIN32
#include <windows.h>
#include "debug.h"
#include "CWinThread.h"
#include "CEvent.h"
#include "CFile.h"
#include "CBmp.h"
#include "Canvas.h"
#include "Control.h"
#include "Wnd.h"
#include "App.h"
#else
#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
#include "Control.h"
#include "Wnd.h"
#include "App.h"
#endif
namespace ui{


CAppl::CAppl(
	CWndManager* pMain
	) : CThread("app thread", CThread::NORMAL , 0, NULL)
	, _pWndManager(pMain)	
	, _top(0)
	, _bottom(0)
{	
	_pEvent = new CEvent [Event_Queue_Number];
	_pWaitEvent = new CSemaphore(0, 1, "CAppl sem");
	_pLock = new CMutex("CAppl mutex");
}

CAppl::~CAppl()
{
	Stop();	
	delete _pLock;
	delete _pWaitEvent;
	_pWaitEvent = NULL;	
	delete[] _pEvent;	
}

bool CAppl::PushEvent
	(
		UINT16 type,
		UINT16 event,
		UINT32 paload,
		UINT32 paload1
	)
{
#ifdef _DEBUG
	int num = 0;
	if(_bottom >= _top)
		num = _bottom - _top;
	else
		num = _bottom + Event_Queue_Number - _top;
	if(num >= Event_Queue_Number)
	{
		TRACE1("event over flow !!!!\n");
		//return false;
	}
#endif
	_pLock->Take();
	{
		_pEvent[_bottom]._type = type;
		_pEvent[_bottom]._event = event;
		_pEvent[_bottom]._paload = paload;
		_pEvent[_bottom]._paload1 = paload1;
		_pEvent[_bottom]._bProcessed = false;
		_bottom++;
		if(_bottom >= Event_Queue_Number)
			_bottom = 0;
	}
	_pLock->Give();
	
	_pWaitEvent->Give();

	return true;
}

void CAppl::main(void *)
{
	CEvent event;
	while(true)
	{
		_pWaitEvent->Take(-1);
		while(_top != _bottom)
		{
			_pLock->Take();
			{
				event = _pEvent[_top];
				_top++;
				if(_top >= Event_Queue_Number)
					_top = 0;
			}
			_pLock->Give();
			
			if((CWndManager::getInstance()!= NULL)&&(!event._bProcessed))
			{
				event._bProcessed = !CWndManager::getInstance()->OnEvent(&event);
			}
			if(!event._bProcessed)
				this->OnEvent(&event);
			
		}
	}
}


//#ifdef WIN32
};
//#endif
