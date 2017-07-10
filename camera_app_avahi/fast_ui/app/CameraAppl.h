#ifndef __INCameraAppl_h
#define __INCameraAppl_h
//#ifdef WIN32

#include <stdint.h>

#include "qrencode.h"
#include "GobalMacro.h"
#include "agbt.h"

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


class CameraAppl : public CAppl
{
public:
    typedef CAppl inherited;
    static void Create(CWndManager *pWndM);
    static void Destroy();
    static CameraAppl* getInstance(){return _pInstance;};
    virtual bool OnEvent(CEvent* event);

    static void SecTimerBtnUpdate(void * p)
    {
        CameraAppl* pp = (CameraAppl*) p;
        pp->TimerUpdate();
    }

    void StartUpdateTimer();
    void StopUpdateTimer();
    void TimerUpdate()
    {
        this->PushEvent(eventType_internal, InternalEvent_app_timer_update, 0, 0);
    }

    DvState GetDvState(){return _dvState;};
protected:
    //virtual void main(void * p);
private:
    static CameraAppl* _pInstance;
    CameraAppl(CWndManager *pWndM = NULL);
    ~CameraAppl();
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

