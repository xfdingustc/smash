#ifndef __INCEvent_h
#define __INCEvent_h
//#ifdef WIN32
namespace ui{
//#endif

enum evenType
{
    eventType_key=0,
    eventType_touch,
    eventType_internal,
    eventType_external,
    eventType_Gesnsor,
    eventType_StillCapture,
    eventType_Cam_mode,
};

enum TouchEvent
{
    TouchEvent_SingleClick = 0,
    TouchEvent_DoubleClick,
    TouchEvent_LongPress,
    TouchEvent_OnPressed,
    TouchEvent_OnReleased,
    TouchEvent_coordinate,
    TouchEvent_flick,
    TouchEvent_Move_Began,
    TouchEvent_Move_Changed,
    TouchEvent_Move_Ended,
    TouchEvent_Ended
};

enum TouchFlick
{
    TouchFlick_disabled   = 0x00,
    TouchFlick_UP         = 0x08,
    TouchFlick_UpperRight = 0x09,
    TouchFlick_Right      = 0x0A,
    TouchFlick_LowerRight = 0x0B,
    TouchFlick_Down       = 0x0C,
    TouchFlick_LowerLeft  = 0x0D,
    TouchFlick_Left       = 0x0E,
    TouchFlick_UpperLeft  = 0x0F,
    TouchFlick_DownAndUp  = 0x0C,
};

enum InternalEvent
{
    InternalEvent_wnd_button_click = 0,
    InternalEvent_app_timer_update,
    InternalEvent_app_state_update,
    InternalEvent_wnd_force_close,
    InternalEvent_app_property_update,
};

enum KeyEvent
{
    key_unknow = 0,

    key_up,
    key_up_onKeyDown,
    key_up_onKeyUp,
    key_up_onKeyLongPressed,

    key_down,
    key_down_onKeyDown,
    key_down_onKeyUp,
    key_down_onKeyLongPressed,

    key_left,
    key_left_onKeyDown,
    key_left_onKeyUp,
    key_left_onKeyLongPressed,

    key_right,
    key_right_onKeyDown,
    key_right_onKeyUp,
    key_right_onKeyLongPressed,

    key_ok,
    key_ok_onKeyDown,
    key_ok_onKeyUp,
    key_ok_onKeyLongPressed,

    key_sensorModeSwitch,
    key_test,
    key_poweroff_prepare,
    key_poweroff_cancel,
    key_poweroff,
    key_switch_top,
    key_switch_bottom,
    key_hid,
    key_number,
};

enum GsensorEvent
{
    GsensorShake = 0,
};

enum StillCaptureEvent
{
    StillCapture_Start,
    StillCapture_Info,
    StillCapture_Done,
};

enum MountEvent
{
    MountEvent_Plug_To_Car_Mount = 0,
    MountEvent_UnPlug_From_Car_Mount,
    MountEvent_Plug_To_USB, // including destop, and charger
};

class CEvent
{
public:
    CEvent() {}
    ~CEvent() {}

public:
    UINT16  _type;
    UINT16  _event;
    UINT32  _paload;
    UINT32  _paload1;
    bool    _bProcessed;
};

//#ifdef WIN32
}
//#endif

#endif