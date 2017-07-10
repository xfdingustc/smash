/*******************************************************************************
**       App.h
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a Jun-28-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/

#ifndef __INCApp_h
#define __INCApp_h

#include "WindowManager.h"

namespace ui{

#define Event_Queue_Number 40

class Window;
class WindowManager;

class CAppl : public CThread
{
public:
    CAppl();
    ~CAppl();

    bool PushEvent(
        UINT16 type,
        UINT16 event,
        UINT32 paload,
        UINT32 paload1
    );

    virtual bool OnEvent(CEvent* event) = 0;
    Window* GetWnd(){return _pWndManager->GetCurrentWnd();};
    //void ShowMainWnd(){_pWndManager->Show(true);};

    virtual void Stop();

protected:
    virtual void main(void *);

private:
    WindowManager *_pWndManager;
    CSemaphore    *_pWaitEvent;
    CMutex        *_pLock;

    CEvent *_pEvent;
    int    _top;
    int    _bottom;

    bool   _bRunning;
};

}

#endif

