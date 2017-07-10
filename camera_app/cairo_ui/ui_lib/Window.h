/*******************************************************************************
**       Canvas.h
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Use of Transee Design code is governed by terms and conditions
**      stated in the accompanying licensing statement.
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a  8- 9-2011 linn song CREATE AND MODIFY
**
*******************************************************************************/

#ifndef __IN_Window_h__
#define __IN_Window_h__

#include "CairoCanvas.h"
#include "CEvent.h"
#include "Widgets.h"

namespace ui {

class Window;
class CAppl;

enum AnimationType
{
    Anim_Null = 0,
    Anim_Right2LeftEnter,
    Anim_Right2LeftExit,
    Anim_Left2RightEnter,
    Anim_Left2RightExit,
    Anim_Top2BottomEnter,
    Anim_Top2BottomCoverEnter,
    Anim_Bottom2TopExit,
    Anim_Bottom2TopEnter,
    Anim_Bottom2TopCoverEnter,
    Anim_Top2BottomExit,
    Anim_Num,
};

class Window
{
public:
    Window(const char *name, Size wndSize, Point leftTop);
    virtual ~Window();

    virtual void onStart();
    virtual void onResume();
    virtual void onPause();
    virtual void onStop();

    virtual bool OnEvent(CEvent* event) = 0;
    virtual void onTimerEvent() {}

    bool processEvent(CEvent* event);

    void SetApp(CAppl* p) { _pParentApp = p; }
    CAppl* GetApp() { return _pParentApp; }

    void SetMainObject(Container* p) { _pMainObject = p; }
    Container* GetMainObject() { return _pMainObject; }

    void StartWnd(const char *name, int animType = Anim_Right2LeftEnter);
    void Close(int animType = Anim_Null);
    void Close(int animType, bool bDestroy);

    const char* getWndName() { return _name; }

    CairoCanvas* GetCanvas() { return _pCanvas; }

    Size& getWindowSize() { return _wndSize; }

    void onSurfaceChanged();

    bool isWindowVisible() { return _bVisible; }
    void setVisibleArea(const Point& leftTop, const Size& size);
    void getVisibleArea(Point &leftTop, Size &size);
    bool isToDelete() { return _toDelete; }

    bool startTimer(int t_ms, bool bLoop);
    bool cancelTimer();

private:
    static void TimerCB(void*);

    Container      *_pMainObject;
    CAppl          *_pParentApp;
    const char     *_name;
    CairoCanvas    *_pCanvas;
    Size           _wndSize;

    bool           _bVisible;
    Point          _leftTop;
    Size           _visibleSize;

    bool           _toDelete;
};

};

#endif
