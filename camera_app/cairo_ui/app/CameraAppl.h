#ifndef __INCameraAppl_h
#define __INCameraAppl_h
//#ifdef WIN32

#include <stdint.h>

#include "qrencode.h"
#include "GobalMacro.h"
#include "agbt.h"
#include "App.h"

#include "IMEWnd.h"

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
    DialogueType_Wifi_pwd_wrong,
    DialogueType_Wifi_add_success,
    DialogueType_Loop_Space_Warning,
    DialogueType_Space_Warning_guide,
    DialogueType_OBD_Defect,
    DialogueType_OBD_Upgrading,
    DialogueType_HID_Upgrading,
    DialogueType_num
};

class CameraAppl : public CAppl
{
public:
    typedef CAppl inherited;
    static void Create(CCameraProperties *cp);
    static void Destroy();
    static CameraAppl* getInstance() { return _pInstance; }
    static void SecTimerBtnUpdate(void * p);
    static void TempratureMonitorTimer(void *p);

    virtual bool OnEvent(CEvent* event);

    void StartUpdateTimer();
    void StopUpdateTimer();
    void StartTempratureMonitor();

    void PopupDialogue(DialogueType dialogue);

    void StartIME(char *title, bool bPassword);
    void updateTextInputed(char *txt);
    char* GetInputedText();

protected:
    //virtual void main(void * p);

private:
    CameraAppl(CCameraProperties *cp);
    virtual ~CameraAppl();

    void _timerUpdate();
    void _checkSpaceWarning();

    static CameraAppl   *_pInstance;

    CCameraProperties   *_cp;

    IMEWnd              *_pIMEWnd;
    char                *_txtInputed;

    int                 _counter;
    bool                _bSpaceWarning;
};


//#ifdef WIN32
}
//#endif

#endif

