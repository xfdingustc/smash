#ifndef __INCameraAppl_h
#define __INCameraAppl_h
//#ifdef WIN32

#include <stdint.h>

#include "qrencode.h"
#include "GobalMacro.h"
#include "agbt.h"
#include "App.h"

class CCameraProperties;

namespace ui{
//#endif
class CBrushBmp;
enum CameraMode
{
    CameraMode_dv = 0,
    CameraMode_still,
    CameraMode_hyper
};

enum DvState
{
    DvState_idel = 0,
    DvState_prepare,
    DvState_recording,
    DvState_stopping,
};

enum StillMode
{
    StillState_single = 0,
    DvState_contiune
};

enum FlashState
{
    FlashState_off = 0,
    FlashState_auto,
    FlashState_on,
    FlashState_redeye,
    FlashState_torch
};

enum GPSState
{
    GPSState_off = 0,
    GPSState_on
};

enum CamAppEvent
{
    start_record = 0,
    stop_record,
};

enum DialogueType
{
    DialogueType_PowerOff = 0,
    DialogueType_USBStorage,
    DialogueType_Car_Mount_Unplug,
    DialogueType_Space_Full,
    DialogueType_Card_Missing,
    DialogueType_Disk_Error,
    DialogueType_UnknownSD_Format,
    DialogueType_Mark,
    DialogueType_Mark_Failed,
    DialogueType_OBD_Connected,
    DialogueType_OBD_Disconnected,
    DialogueType_HID_Connected,
    DialogueType_HID_Disconnected,
    DialogueType_OBD_Not_Binded,
    DialogueType_HID_Not_Binded,
    DialogueType_Overheat,
    DialogueType_num
};

class CameraAppl : public CAppl
{
public:
    typedef CAppl inherited;
    static void Create(CCameraProperties *cp);
    static void Destroy();
    static CameraAppl* getInstance() { return _pInstance; }
    static void SecTimerBtnUpdate(void * p)
    {
        CameraAppl* pp = (CameraAppl*) p;
        pp->TimerUpdate();
    }
    static void TempratureMonitorTimer(void *p);

    virtual bool OnEvent(CEvent* event);

    void StartUpdateTimer();
    void StopUpdateTimer();
    void TimerUpdate()
    {
        this->PushEvent(eventType_internal, InternalEvent_app_timer_update, 0, 0);
    }

    void StartTempratureMonitor();

    DvState GetDvState() { return _dvState; }

    void PopupDialogue(DialogueType dialogue);

protected:
    //virtual void main(void * p);

private:
    CameraAppl(CCameraProperties *cp);
    virtual ~CameraAppl();

    static CameraAppl* _pInstance;

    CCameraProperties *_cp;

    UINT32   _cameraMode;
    DvState  _dvState;
    UINT32   _stillMode;
    UINT32   _flashState;
    UINT32   _gpsState;
    UINT32   _opticalZoom;
    UINT32   _digitalZoom;
    int      _updatetimer;
    bool     _bUpdateTimer;
};


//#ifdef WIN32
}
//#endif

#endif

