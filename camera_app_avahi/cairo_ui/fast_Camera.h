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
#include "class_camera_property.h"
#include "class_TCPService.h"
#include "class_gsensor_thread.h"
#include "class_camera_callback.h"

#include "CCameraCircularUI.h"


namespace ui{

class CFastCamera
    : public KeyEventProcessor
      ,public PropertiesObserver
      ,public CameraCallback
{
public:
    CFastCamera(::CCameraProperties* pp);
    ~CFastCamera();

    void Start();
    void Stop();
    void CheckStatusOnBootup();

    virtual bool event_process(struct input_event* ev);
    virtual void OnPropertyChange(void* p, PropertyChangeType type);
#ifdef ENABLE_STILL_CAPTURE
    virtual void onStillCaptureStarted(int one_shot);
    virtual void onStillCaptureDone() {} // TODO:
    virtual void sendStillCaptureInfo(int stillcap_state,
        int num_pictures, int burst_ticks);
#endif
    virtual void onModeChanged();
    virtual void onCameraState(CameraState state, int payload = 0);

private:
    static void KeyRightLongPressTimer(void *);
    static void KeyOKLongPressTimer(void *);

    bool _USBStorageReady();
    void _getMountStatus();
    bool _isUnknownSDPluged();
    bool _isAutoOffEnabled();
    void _updateBTStatus();
    bool _isRotate180();

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


    typedef enum Mount_Device_Status
    {
        Mount_DEVICE_Status_NULL = 0,
        Mount_DEVICE_Status_USB_Mount,
        Mount_DEVICE_Status_Car_Mount_No_Power,
        Mount_DEVICE_Status_Car_Mount_With_Power,
    } Mount_Device_Status;

    CCameraProperties *_cp;
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

    bool  _bKeyRightLongPressed;
    bool  _bKeyOKLongPressed;
    bool  _bRotate180;

    Mount_Device_Status _mountStatus;
    bool  _bOBDBinded;
    bool  _bHIDBinded;
    int   _bt_state;
};

}
#endif
