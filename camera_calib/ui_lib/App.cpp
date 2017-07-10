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
#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "WindowManager.h"
#include "App.h"


namespace ui{

CAppl::CAppl()
    : CThread("app thread", CThread::NORMAL , 0, NULL)
    , _top(0)
    , _bottom(0)
    , _bRunning(false)
{
    _pEvent = new CEvent [Event_Queue_Number];
    _pWaitEvent = new CSemaphore(0, 1, "CAppl sem");
    _pLock = new CMutex("CAppl mutex");

    _pWndManager = WindowManager::getInstance();
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
    if (_bottom >= _top) {
        num = _bottom - _top;
    } else {
        num = _bottom + Event_Queue_Number - _top;
    }
    if (num >= Event_Queue_Number) {
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
        if (_bottom >= Event_Queue_Number) {
            _bottom = 0;
        }
    }
    _pLock->Give();

    _pWaitEvent->Give();

    return true;
}

void CAppl::Stop()
{
    _bRunning = false;
    _top = _bottom;
    _pWaitEvent->Give();
    CThread::Stop();
}

void CAppl::main(void *)
{
    _bRunning = true;

    CEvent event;
    while (_bRunning) {
        _pWaitEvent->Take(-1);
        while (_top != _bottom) {
            _pLock->Take();
            {
                event = _pEvent[_top];
                _top++;
                if (_top >= Event_Queue_Number) {
                    _top = 0;
                }
            }
            _pLock->Give();

            if ((WindowManager::getInstance()!= NULL) && (!event._bProcessed)) {
                event._bProcessed = !WindowManager::getInstance()->OnEvent(&event);
            }
            if (!event._bProcessed) {
                this->OnEvent(&event);
            }
        }
    }
}

};
