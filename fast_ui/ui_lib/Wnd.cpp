/*******************************************************************************
**      Wnd.cpp
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Use of Transee Design code is governed by terms and conditions 
**      stated in the accompanying licensing statement.
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
#include "CEvent.h"
#include "CFile.h"
#include "CBmp.h"
#include "Canvas.h"
#include "Control.h"
#include "Wnd.h"

#else
#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
#include "Control.h"
#include "Wnd.h"
#endif
namespace ui{
/*******************************************************************************
* <Macros>
*******************************************************************************/

/*******************************************************************************
* <Local Functions>
*******************************************************************************/

/*******************************************************************************
* <Local variable>
*******************************************************************************/
//CWndManager* CWndManager::_pInstance = NULL;

/*******************************************************************************
* <Function elements>
*******************************************************************************/

/*******************************************************************************
*       CWnd::CWnd(UINT16 id)
*   
*/ 
CWnd::CWnd
	(const char *name
	 ,CWndManager* pManag
	) :_pMainObject(NULL) 
	,_name(name)
	, _bShown(false)
	,_preWnd(NULL)
	,_pWndManager(pManag)
{
	if(_pWndManager == NULL)
   		_pWndManager = CWndManager::getInstance();
    if(_pWndManager != NULL)
    {
		_pWndManager->AddWnd(this);
    }
}

/*******************************************************************************
*       CWnd::~CWnd()
*   
*/ 
CWnd::~CWnd()
{

}

bool CWnd::OnEvent(CEvent* event)
{
	bool rt = true;
	if(_pMainObject)
	{
		rt = _pMainObject->OnEvent(event);
	}
	if(rt)
	{
		if(event->_type == eventType_key)
		{
			switch(event->_event)
			{
				case key_right:
					//this->Close();
					//rt = false;
					break;
			}
		}
		if(event->_type == eventType_internal)
		{
			switch(event->_event)
			{
				case InternalEvent_wnd_force_close:
					if((UINT32)this == event->_paload)
					{
						this->Close();
						rt = false;
					}
					break;

			}
		}
	}
	//printf("--OnEvent of %s end\n", _name);
	return rt;
}

void CWnd::willShow()
{

}

void CWnd::willHide()
{

}

void CWnd::Show(bool bResetFocus)
{
	if(!_bShown)
	{	
		this->willShow();
		_bShown = true;
		//printf("CWnd::Show 0x%x\n", this->GetMainObject());
		if(this->GetMainObject())
		{
			this->GetMainObject()->Draw(true);
		}
		this->GetManager()->setCurrentWnd(this);
	}
}

void CWnd::Hide(bool bResetFocus)
{
	if(_bShown)
	{
		this->willHide();
		_bShown = false;
		
		if(this->GetMainObject())
			this->GetMainObject()->Clear();
		this->GetManager()->setCurrentWnd(NULL);
	}
}

void CWnd::PopWnd(CWnd* pWnd)
{
	pWnd->SetPreWnd(this);
	this->Hide(false);
	pWnd->Show(true);
}

void CWnd::Close()
{
	this->Hide(true);
	if(_preWnd != NULL)
	{
		_preWnd->Show(false);
		_preWnd = NULL;
	}
}

CWndManager* CWndManager::_pInstance = NULL;
void CWndManager::Create(CCanvas* canvas)
{
	if(_pInstance == NULL)
	{
		_pInstance = new CWndManager(canvas);
	}
}

void CWndManager::Destroy()
{
	if(_pInstance != NULL)
	{
		printf("delete ui CWndManager 0\n");
		delete _pInstance;
	}
}

int CWndManager::AddWnd(CWnd* pWnd)
{
	if(_number < maxWndNum)
	{
		_ppWndList[_number] = pWnd;
		_number++;
	}
	else
	{
		printf("-- too much wnd over maxWndNum(%d) defined\n", maxWndNum);
	}
	return _number;
}

bool CWndManager::OnEvent(CEvent* event)
{	
	bool rt = true;
	_lock->Take();
	if(_currentWnd != NULL)
	{	
	_lock->Give();
		rt = _currentWnd->OnEvent(event);
		if(rt)
		{
			if(event->_type == eventType_key)
			{
				switch(event->_event)
				{
					case key_left:
						_currentWnd->Close();
						rt = false;
						break;
				}
			}
		}
	_lock->Take();
	}
	_lock->Give();
	return rt;
}

void CWndManager::setCurrentWnd(CWnd *pWnd)
{
	_lock->Take();
	_currentWnd = pWnd;
	_lock->Give();
}

CWndManager::CWndManager
	(CCanvas* canvas
	):_number(0)
	,_pCanvas(canvas)
{
	_currentWnd = 0;
	_lock = new CMutex("CWndManager mutex");	
}

CWndManager::~CWndManager()
{
	printf("--~CWndManager : %d \n", _currentWnd);
	if(_currentWnd != 0)
	{
		_currentWnd->Hide(true);
	}
	printf("--~CWndManager 1\n");
	for(int i = 0; i < _number; i++)
	{
		if(_ppWndList[i] != 0)
		{
			printf("-- delete wnd: %s\n", _ppWndList[i]->getName());
			delete _ppWndList[i];
		}
	}	
	delete _lock;
}


//#ifdef WIN32
};
//#endif

