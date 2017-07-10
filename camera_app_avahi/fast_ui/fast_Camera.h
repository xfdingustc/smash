/*******************************************************************************
**       fast_Camera.h
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a  7-14-2011 linnsong CREATE AND MODIFY
**
*******************************************************************************/
#ifndef __INFast_Camera_h
#define __INFast_Camera_h

#include "linux.h"
#include <linux/input.h>
#include <linux/fb.h>
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
#include "CLanguage.h"
#include "Control.h"
#include "Style.h"
#include "Wnd.h"
#include "App.h"
#include "CameraAppl.h"
#include "FontLibApi.h"
#include "CKeyEventProcessThread.h"
#include "CLanguage.h"
#include "class_camera_property.h"
#include "class_TCPService.h"
#include "class_gsensor_thread.h"
#include "class_camera_callback.h"

#ifdef  BitUI

#ifdef  NEW_UI
#include "CCameraViditUI.h"
#else
#include "CCameraBitUI.h"
#endif

#endif

#ifdef  RGB656UI
#include "CameraUI.h"
#endif

namespace ui{

#define Time_200ms   200000

class CFastCamera
    : public KeyEventProcessor
      ,public PropertiesObserver
      ,public ShakeObserver
      ,public CameraCallback
{
public:
    static void FB565DrawHandler();
    static void BitCanvasDrawHandler();

    CFastCamera(::CCameraProperties* pp);
    ~CFastCamera();

    void Start();
    void Stop();

    virtual bool event_process(struct input_event* ev);
    virtual void OnPropertyChange(void* p, PropertyChangeType type);
    virtual void OnShake();
#ifdef ENABLE_STILL_CAPTURE
    virtual void onStillCaptureStarted(int one_shot);
    virtual void onStillCaptureDone() {} // TODO:
    virtual void sendStillCaptureInfo(int stillcap_state,
        int num_pictures, int burst_ticks);
#endif
    virtual void onModeChanged();
    virtual void onCameraState(CameraState state, int payload = 0);

    bool SetOLEDBrightness(unsigned char v);

private:
    bool OpenFb();

    static int   _fb;
    static char  *_fbm;
    UINT32       _screensize;
    static struct fb_var_screeninfo _vinfo;
    struct fb_fix_screeninfo        _finfo;

    bool _bShakeProcess;
    bool _bShakeEvent;

#ifdef  RGB656UI
    static void Switcher1Timer(void*);
    static void Switcher2Timer(void*);
    static void powerkeyLongPressTimer(void *);
    static void Timer_key_mute(void *);

    CCanvas   *_pCanvas565;
    CameraWnd *_pCamWnd;

    // For swithers, wait a moment to handle it.
    int  _top_sensor_event;
    int  _bottom_sensor_event;

    struct timeval last_onKeyMute_up_time;
#endif

#ifdef  BitUI
    static void poweroff(void*);
    static void longPressTimer(void *);

    // Filter for (latter_key.onKeyDown.time - former.onKeyUp.time) < 200ms
    static void Timer_key_down(void *);
    static void Timer_key_up(void *);
    static void Timer_key_right(void *);
    static void Timer_key_ok(void *);

    static void Timer_burst_capture(void *);

    bool onKeyUp(struct input_event* ev);
    bool onKeyDown(struct input_event* ev);
    bool onKeyLongPress(int event);

    CBitCanvas *_pCanvasBit;
#ifdef NEW_UI
    CBitCameraWnd *_pBitWnd;
#else
    CBitRecWnd *_pBitWnd;
#endif

    bool _bPoweroffPress;
    int  _poweroffcount;

    bool _bKeyDownPressed;

    // Filter for (latter_key.onKeyDown.time - former.onKeyUp.time) < 200ms
    #define KeyCounts  4
    struct timeval last_onKeyUp_time[KeyCounts];
#endif
};

}
#endif
