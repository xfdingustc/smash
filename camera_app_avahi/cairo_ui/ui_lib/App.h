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
#define Wnd_Q_Num          10


class Window;
class WindowManager;

class DialogueInfo
{
public:
    DialogueInfo() : _name(NULL), _enterAnim(Anim_Null) {}
    DialogueInfo(const char *name, int anim) : _name(name), _enterAnim(anim) {}
    ~DialogueInfo() {}
    void setName(const char *name) { _name = name; }
    const char* getName() { return _name; }
    void setEnterAnim(int anim) { _enterAnim = anim; }
    int getEnterAnim() { return _enterAnim; }

private:
    const char *_name;
    int        _enterAnim;
};

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
    virtual void Stop();

    Window* GetWnd() { return _pWndManager->GetCurrentWnd(); }

    bool startDialogue(const char *name, int anim);

protected:
    virtual void main(void *);

private:
    WindowManager *_pWndManager;
    CSemaphore    *_pWaitEvent;
    CMutex        *_pLock;

    CEvent        *_pEvent;
    int           _top;
    int           _bottom;

    DialogueInfo  _pDialogueList[Wnd_Q_Num];
    int           _dialogueHead;
    int           _dialogueTail;

    bool          _bRunning;
};

}

#endif

