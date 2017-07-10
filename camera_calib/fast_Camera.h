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

#include <linux/input.h>
#include <linux/fb.h>
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "CLanguage.h"
#include "App.h"
#include "CameraAppl.h"
#include "CKeyEventProcessThread.h"
#include "CalibUI.h"

namespace ui{

class CFastCamera
    : public KeyEventProcessor
{
public:
    CFastCamera();
    ~CFastCamera();

    void Start();
    void Stop();

    virtual bool event_process(struct input_event* ev);

private:

    enum DATA_TYPE
    {
        DATA_TYPE_none        = 0x0,
        DATA_TYPE_x           = 0x1,
        DATA_TYPE_y           = 0x1 << 1,
        DATA_TYPE_press       = 0x1 << 2,
        DATA_TYPE_touch_down  = 0x1 << 3,
        DATA_TYPE_touch_up    = 0x1 << 4,
        DATA_TYPE_flick       = 0x1 << 5,
        DATA_TYPE_double_click= 0x1 << 6,
        DATA_TYPE_tap         = 0x1 << 7,
    };

    Window  *_pCamWnd;

    int   _x;
    int   _y;
    int   _x_rel;
    int   _y_rel;
    int   _x_pressed;
    int   _y_pressed;
    int   _dataReceived;
    char  _flickValue;

    bool  _hasFlickEvent;
    int   _xOnPressed;
    int   _yOnPressed;
    bool  _bMoveTriggered;
    struct timeval last_touch_end_time;
};

}
#endif
