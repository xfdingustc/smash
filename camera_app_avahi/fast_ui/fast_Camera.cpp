/*******************************************************************************
**       fast_Camera.cpp
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

#include <netinet/in.h>

#include "fast_Camera.h"
#include "agbox.h"
#include "GobalMacro.h"

#define ui_standalong 0
//namespace ui{

const static char* fontFile = "/usr/local/share/ui/arialuni.ttf";
//static char* fontFile = "/usr/local/bin/ui/SIMYOU.TTF";
#if defined(PLATFORMDIABLO) || defined(PLATFORMHELLFIRE)
const static char* poweroffTimer = "poweroff Timer";
const static char* longpressTimer = "long press Timer";
const static char* languageTxtDir = "/usr/local/share/ui/diablo";
const static char* key_down_Timer = "key down Timer";
const static char* key_up_Timer = "key up Timer";
const static char* key_ok_Timer = "key ok Timer";
const static char* key_right_Timer = "key right Timer";
const static char* burst_capture_Timer = "burst capture Timer";
#endif

#ifdef PLATFORM22A
const static char* topswitcherTimer = "switcher1 Timer";
const static char* bottomswitcherTimer = "switcher2 Timer";
const static char* powerkeyLongTimer = "power key long Timer";
const static char* key_mute_Timer = "key mute Timer";
const static char* languageTxtDir = "/usr/local/share/ui/";
#endif
const static char* ControlResourceDir = "/usr/local/share/ui/";

static char *_fbbbb;//[1024];

int ui::CFastCamera::_fb= -1;
char* ui::CFastCamera::_fbm = 0;
fb_var_screeninfo ui::CFastCamera::_vinfo;

void ui::CFastCamera::FB565DrawHandler()
{
    // notice FB to update here
    ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
    //CAMERALOG("--fb infor after pan : \n\tYoffset:%d\n\tXoffset:%d",  _vinfo.yoffset, _vinfo.xoffset);
}

void ui::CFastCamera::BitCanvasDrawHandler()
{
#if 1
    if (_fb >0) {
        unsigned char mmm[8];
        unsigned char ppp;
        unsigned char* tt = (unsigned char*)_fbm;
        unsigned char *p;
        unsigned char ttt;
        //m_rcvLocker.Take();
        //memcpy(_pBitTransBuffer,_pBitBuffer,1024);
        //m_rcvLocker.Give();

        for (int i = 0; i < 64 ; i ++) {
            for (int j = 0; j < 16; j++) {
                tt[i*16 + j] = _fbbbb[((i / 8) * 8 + (j%8))*16 + ((i%8)*2 + j/8)];
            }
        }

        for (int i = 0; i < 1024; i += 8) {
            p = tt + i;
            memcpy(mmm, p, 8);
            ttt = 0x80;
            for (int j = 0; j < 8; j++) {
                ppp = 0x80;
                for (int k = 7; k >= 0; k--) {
                    if (mmm[k]&ttt) {
                        *(p + j) |= ppp;
                    } else {
                        *(p + j) &= ~ppp;
                    }
                    ppp = ppp >> 1;
                }
                ttt = ttt >> 1;
            }
        }
        //unsigned short tmp = 0x1234;
        //_serialObj.SendData((const char*)&tmp, 2);
        //_serialObj.SendData((const char*)tt,1024);

    }
    //CAMERALOG("--- fb pan");
#endif
    if (_fb >0) {
        ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
    }
}

ui::CFastCamera::CFastCamera(::CCameraProperties* pp)
    :KeyEventProcessor(NULL)
    ,_bShakeProcess(false)
    ,_bShakeEvent(false)
#ifdef  RGB656UI
    ,_top_sensor_event(0)
    ,_bottom_sensor_event(0)
#endif
#ifdef  BitUI
    ,_bPoweroffPress(false)
    ,_poweroffcount(5)
    ,_bKeyDownPressed(false)
#endif
{
    //if(OpenFb())
    OpenFb();
    {
        pp->SetObserver(this);
        CFontLibApi::Create(fontFile);
        CStyle::Create();
        char tmp[256];
        memset(tmp, 0, sizeof(tmp));
        agcmd_property_get(PropName_Language, tmp, "English");
        CAMERALOG("system.ui.language = %s", tmp);
        CLanguageLoader::Create(languageTxtDir, tmp);
        CMenu::LoadSymbles(ControlResourceDir);
        CPanel::SetStyle(BSL_PANEL_NORMAL, BSL_PANEL_NORMAL);
        CBtn::SetBtnStyle(BSL_BUTTON_NORMAL, FSL_BUTTON_NORMAL,
            BSL_BUTTON_HILIGHT, FSL_BUTTON_HILIGHT);

#ifdef RGB656UI
        _pCanvas565 = new CCanvas
            ((COLOR*)_fbm + _vinfo.yoffset * _finfo.line_length /sizeof(COLOR)
            , FB565DrawHandler
            ,_vinfo.xres
            ,_vinfo.yres
            ,false);
        CWndManager::Create(_pCanvas565);
        _pCamWnd = new CameraWnd(pp);
#endif

#ifdef BitUI
        _fbbbb = new char[1024];

        _pCanvasBit = new CBitCanvas((UINT8*)_fbbbb, BitCanvasDrawHandler, 128, 64);

        CWndManager::Create(_pCanvasBit);
#ifdef NEW_UI
        _pBitWnd = new CBitCameraWnd(CWndManager::getInstance(), pp);
#else
        _pBitWnd = new CBitRecWnd(CWndManager::getInstance(), pp);
#endif
        _pCanvasBit->FillAll(0);

        CTimerThread::GetInstance()->ApplyTimer
            (20, CFastCamera::poweroff, this, true, poweroffTimer);

        // Init key filters:
        memset(last_onKeyUp_time, 0x00, sizeof(last_onKeyUp_time));
#endif

        CameraAppl::Create(CWndManager::getInstance());
        CameraAppl::getInstance()->StartUpdateTimer();
        CameraAppl::getInstance()->PushEvent(eventType_internal,
            InternalEvent_app_state_update, 0, 0);

        EventProcessThread::getEPTObject()->AddProcessor(this);

        agcmd_property_set("system.input.key_mute_diff", "10000");
    }
    //else
    //{
    //	CAMERALOG("_fb open failed!! %d", _fb);
    //}
}


ui::CFastCamera::~CFastCamera()
{
    CAMERALOG("destroy CFastCamera");

    //if(_fb > 0)
    {
        //CAMERALOG("--RemoveProcessor");
        EventProcessThread::getEPTObject()->RemoveProcessor(this);
        CameraAppl::getInstance()->StopUpdateTimer();
        CameraAppl::Destroy();

        CMenu::DeleteSymbles();
        CLanguageLoader::Destroy();
        CFontLibApi::Destroy();
        CStyle::Destroy();

#ifdef  BitUI
        CTimerThread::GetInstance()->CancelTimer(poweroffTimer);
        CWndManager::Destroy();
        delete _pCanvasBit;
        delete[] _fbbbb;

#endif

#ifdef  RGB656UI
        CWndManager::Destroy();
        delete _pCanvas565;
#endif

        munmap(_fbm, _screensize);
        if (_fb > 0) {
            close(_fb);
        }
    }

    CAMERALOG("CFastCamera destroyed");
}

bool ui::CFastCamera::OpenFb()
{
    _fb = open("/dev/fb0", O_RDWR);
    if (!_fb) {
        CAMERALOG("Error: cannot open framebuffer device.");
    } else {
        CAMERALOG("The framebuffer device was opened successfully.");
    }
    if (_fb > 0) {
        // Get fixed screen information
        if (ioctl(_fb, FBIOGET_FSCREENINFO, &_finfo)) {
            CAMERALOG("Error reading fixed information.");
        }

        if (ioctl(_fb, FBIOGET_VSCREENINFO, &_vinfo)) {
            CAMERALOG("Error reading variable information.");
        }
        //CAMERALOG("vinfo.xoffset : %d,  vinfo.yoffset : %d", _vinfo.xoffset,  _vinfo.yoffset);
        _screensize = _vinfo.xres * _vinfo.yres * _vinfo.bits_per_pixel / 8;
        _fbm = (char *)mmap(0, _finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, _fb, 0);
        memset(_fbm, 0xff, _screensize);
        ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
        CAMERALOG("open fb : %d, %d, %d", _vinfo.xres, _vinfo.yres, _screensize);
        return true;
    } else {
        return false;
    }
}

bool ui::CFastCamera::SetOLEDBrightness(unsigned char v)
{
    if (_fb) {
//		ioctl(_fb, AGB_SSD1308_IOCTL_SET_CONTRAST, &v);
    }
    return true;
}

void ui::CFastCamera::Start()
{
#ifdef  RGB656UI
    _pCamWnd->GetManager()->GetCanvas()->FillAll(0);
    _pCamWnd->showDefaultWnd();
#endif
    //CAMERALOG("---_pBitWnd: 0x%x", _pBitWnd);

#ifdef  BitUI

#ifdef NEW_UI
    _pBitWnd->showDefaultWnd();
#else
    _pBitWnd->Show(false);
#endif

#endif

}

void ui::CFastCamera::Stop()
{
#ifdef  RGB656UI
    _pCamWnd->GetManager()->GetCanvas()->FillAll(0);
#endif

#ifdef  BitUI
    if (_fb > 0) {
        //memset(_fbm, 0, 1024);
        //_pCanvasBit->FillAll(1);
        //ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
    }
#endif
}

void ui::CFastCamera::OnPropertyChange(void* p, PropertyChangeType type)
{
    if (CameraAppl::getInstance() != NULL) {
        CameraAppl::getInstance()->PushEvent(
            eventType_internal, InternalEvent_app_property_update, type, 0);
    }
}

void ui::CFastCamera::OnShake()
{
    if (_bShakeProcess) {
        CAMERALOG("CFastCamera::OnShake()");
        if (CameraAppl::getInstance() != NULL) {
            CameraAppl::getInstance()->PushEvent(eventType_Gesnsor, GsensorShake, 0, 0);
            _bShakeEvent = true;
        }
    }
}

#ifdef ENABLE_STILL_CAPTURE
void ui::CFastCamera::onStillCaptureStarted(int one_shot) {
    if (CameraAppl::getInstance() != NULL) {
        CameraAppl::getInstance()->PushEvent(
            eventType_StillCapture,
            StillCapture_Start,
            one_shot,
            0);
    }
}

void ui::CFastCamera::sendStillCaptureInfo(
    int stillcap_state, int num_pictures, int burst_ticks)
{
    if (CameraAppl::getInstance() != NULL) {
        if (stillcap_state == 0/*idle*/) {
            CameraAppl::getInstance()->PushEvent(
                eventType_StillCapture,
                StillCapture_Info,
                stillcap_state,
                num_pictures);
        }
    }
}

#endif

void ui::CFastCamera::onModeChanged() {
    if (CameraAppl::getInstance() != NULL) {
        CameraAppl::getInstance()->PushEvent(
            eventType_Cam_mode,
            0,
            0,
            0);
    }
}

void ui::CFastCamera::onCameraState(CameraState state, int payload) {
    if (CameraAppl::getInstance() != NULL) {
        if (state == CameraState_starting) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_starting, 0);
        } else if (state == CameraState_stopping) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_stopping, 0);
        } else if (state == CameraState_error) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_Error, 0);
        } else if (state == CameraState_recording) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_record, 0);
        } else if (state == CameraState_stopped) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_stop, 0);
        } else if (state == CameraState_storage_changed) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_Storage_changed, 0);
        } else if (state == CameraState_storage_full) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_Storage_full, 0);
        } else if (state == CameraState_FormatSD_Result) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_FormatSD_result, payload);
        }
    }
}

#ifdef  BitUI

void ui::CFastCamera::poweroff(void *para)
{
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam && cam->_bPoweroffPress) {
        cam->_poweroffcount --;
        if (cam->_poweroffcount <= 0) {
            CAMERALOG("-- power off");
            const char *tmp_args[8];
            tmp_args[0] = "agsh";
            tmp_args[1] = "poweroff";
            tmp_args[2] = NULL;
            agbox_sh(2, (const char *const *)tmp_args);
        } else if (cam->_poweroffcount <= 3) {
            //push power off msg
            CameraAppl::getInstance()->PushEvent(eventType_key, key_poweroff_prepare, 0, 0);
        }
    }
}

// Because it is not a loop timer, it will be deleted automatically after this callback.
void ui::CFastCamera::longPressTimer(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam && cam->_bKeyDownPressed) {
        //CAMERALOG("OnKeyLongPress --- key_down_onKeyLongPressed");
        CameraAppl::getInstance()->PushEvent(eventType_key, key_down_onKeyLongPressed, 0, 0);
    }
}

// Because it is not a loop timer, it will be deleted automatically after this callback.
void ui::CFastCamera::Timer_key_down(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam) {
        //CAMERALOG("OnKeyUp --- code 108, key_down_onKeyUp");

        // It is in TimerThread, call CancelTimer() will dead lock, so just change the flag:
        cam->_bKeyDownPressed = false;

        CameraAppl::getInstance()->PushEvent(eventType_key, key_down_onKeyUp, 0, 0);
    }
}

// Because it is not a loop timer, it will be deleted automatically after this callback.
void ui::CFastCamera::Timer_key_up(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam) {
        //CAMERALOG("OnKeyUp --- code 103, key_up_onKeyUp");
        CameraAppl::getInstance()->PushEvent(eventType_key, key_up_onKeyUp, 0, 0);
    }
}

// Because it is not a loop timer, it will be deleted automatically after this callback.
void ui::CFastCamera::Timer_key_right(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam) {
        //CAMERALOG("OnKeyUp --- code 212, key_right_onKeyUp");
        CameraAppl::getInstance()->PushEvent(eventType_key, key_right_onKeyUp, 0, 0);
    }
}

// Because it is not a loop timer, it will be deleted automatically after this callback.
void ui::CFastCamera::Timer_key_ok(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam) {
        cam->_bPoweroffPress = false;
        cam->_poweroffcount = 5;

        if (cam->_poweroffcount > 3) {
            //CAMERALOG("OnKeyUp --- code 116, key_ok_onKeyUp");
            CameraAppl::getInstance()->PushEvent(eventType_key, key_ok_onKeyUp, 0, 0);
        }
    }
}

void ui::CFastCamera::Timer_burst_capture(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam) {
        CameraAppl::getInstance()->PushEvent(eventType_key, key_right_onKeyLongPressed, 0, 0);
    }
}

bool ui::CFastCamera::event_process(struct input_event* ev)
{
    //CAMERALOG("key event: type = %d code = %d value = %d", ev->type, ev->code, ev->value);
    if ((ev->type == 1) && (ev->value == 0)) {
        // onKeyUp
        bool waiting = true;
        if (ev->code == 108) {
            // key_down
            CTimerThread::GetInstance()->ApplyTimer(
                2, CFastCamera::Timer_key_down, this, false, key_down_Timer);
            last_onKeyUp_time[0].tv_sec = ev->time.tv_sec;
            last_onKeyUp_time[0].tv_usec = ev->time.tv_usec;
        } else if (ev->code == 212) {
            // key_right
            CTimerThread::GetInstance()->ApplyTimer(
                2, CFastCamera::Timer_key_right, this, false, key_right_Timer);
            last_onKeyUp_time[1].tv_sec = ev->time.tv_sec;
            last_onKeyUp_time[1].tv_usec = ev->time.tv_usec;
            // cancel burst capture timer
            CTimerThread::GetInstance()->CancelTimer(burst_capture_Timer);
        } else if (ev->code == 116) {
            // key_ok
            CTimerThread::GetInstance()->ApplyTimer(
                2, CFastCamera::Timer_key_ok, this, false, key_ok_Timer);
            last_onKeyUp_time[2].tv_sec = ev->time.tv_sec;
            last_onKeyUp_time[2].tv_usec = ev->time.tv_usec;
        } else if (ev->code == 103) {
            // key_up
            CTimerThread::GetInstance()->ApplyTimer(
                2, CFastCamera::Timer_key_up, this, false, key_up_Timer);
            last_onKeyUp_time[3].tv_sec = ev->time.tv_sec;
            last_onKeyUp_time[3].tv_usec = ev->time.tv_usec;
        } else {
            // other key, no need waiting
            waiting = false;
        }

        if (waiting) {
            return false;
        }
    }

    if ((ev->type == 1) && (ev->value == 1)) {
        // onKeyDown
        struct timeval *tm = NULL;
        if (ev->code == 108) {
            // key_down
            CTimerThread::GetInstance()->CancelTimer(key_down_Timer);
            tm = &last_onKeyUp_time[0];
        } else if (ev->code == 212) {
            // key_right
            CTimerThread::GetInstance()->CancelTimer(key_right_Timer);
            tm = &last_onKeyUp_time[1];
            // Start burst capture timer, which will be triggered after 300ms.
            CTimerThread::GetInstance()->ApplyTimer(
                3, CFastCamera::Timer_burst_capture, this, false, burst_capture_Timer);
        } else if (ev->code == 116) {
            // key_ok
            CTimerThread::GetInstance()->CancelTimer(key_ok_Timer);
            tm = &last_onKeyUp_time[2];
        } else if (ev->code == 103) {
            // key_up
            CTimerThread::GetInstance()->CancelTimer(key_up_Timer);
            tm = &last_onKeyUp_time[3];
        }

        if (tm && (tm->tv_sec != 0) && (tm->tv_usec != 0) ) {
            unsigned int diff_us = (ev->time.tv_sec - tm->tv_sec) * 1000000
                + (ev->time.tv_usec - tm->tv_usec);
            if (diff_us < Time_200ms) {
                //CAMERALOG("Two keys continuously: "
                //    "(latter_key.onKeyDown.time - former.onKeyUp.time) = %dus", diff_us);
                return false;
            }
        }
    }

    int event;
    if ((ev->type == 1)) {//key
        if (ev->value == 0) { // release
            //CAMERALOG("OnKeyUp --- ev->code : %d", ev->code);
            event = key_unknow;
            switch ( ev->code)
            {
                case 28:
                    event = key_hid;
                    break;
                case 108:
                    _bKeyDownPressed = false;
                    CTimerThread::GetInstance()->CancelTimer(longpressTimer);
                    event= key_down_onKeyUp;
                    break;
                case 212:
                    _bShakeProcess = false;
                    if (!_bShakeEvent) {
                        event = key_right_onKeyUp;
                    }
                    _bShakeEvent = false;
                    break;
                case 200:
                case 201:
                case 167:
                case 116:
#ifdef DiabloBoardVersion2
                    _bPoweroffPress = false;
                    _poweroffcount = 5;

                    if (_poweroffcount > 3) {
                        event = key_ok_onKeyUp;
                    }
#else
                    event = key_ok_onKeyUp;
#endif
                    break;
                case 128:
                    event = key_test;
                    break;
                case 113:
                    _bShakeProcess = true;
                    break;
                case 103:
                    event= key_up_onKeyUp;
                    break;
                default:
                    break;
            }
            if ((CameraAppl::getInstance() != NULL)&&(event != key_unknow)) {
                CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
            }
        } else {
            //CAMERALOG("OnKeyDown --- ev->code : %d", ev->code);
            event = key_unknow;
            switch ( ev->code)
            {
                case 108:
                    _bShakeProcess = true;
                    _bKeyDownPressed = true;
                    // Cancel the timer if it exists:
                    CTimerThread::GetInstance()->CancelTimer(longpressTimer);
                    // If key is pressed more than 2s, it is a long press event.
                    CTimerThread::GetInstance()->ApplyTimer(
                        20, CFastCamera::longPressTimer, this, false, longpressTimer);
                    event= key_down_onKeyDown;
                    break;
                case 212:
                    _bShakeProcess = false;
                    if (!_bShakeEvent) {
                        event = key_right_onKeyDown;
                    }
                    _bShakeEvent = false;
                    break;
                case 200:
                case 201:
                case 167:
                case 116:
#ifdef DiabloBoardVersion2
                    _bPoweroffPress = true;
#endif
                    event = key_ok_onKeyDown;
                    break;
                case 103:
                    event= key_up_onKeyDown;
                    break;
                default:
                    break;
            }
            if ((CameraAppl::getInstance() != NULL)&&(event != key_unknow)) {
                CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
            }
        }
    }
    return false;
}

#endif

#ifdef  RGB656UI

// Because it is not a loop timer, it will be deleted automatically after this callback.
void ui::CFastCamera::Switcher1Timer(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam && cam->_top_sensor_event != 0) {
        cam->_top_sensor_event = 0;
        CAMERALOG("key_switch_top");
        int event = key_switch_top;
        CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
    }
}

// Because it is not a loop timer, it will be deleted automatically after this callback.
void ui::CFastCamera::Switcher2Timer(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam && cam->_bottom_sensor_event != 0) {
        cam->_bottom_sensor_event = 0;
        CAMERALOG("key_switch_bottom");
        int event = key_switch_bottom;
        CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
    }
}

void ui::CFastCamera::powerkeyLongPressTimer(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam) {
        int event = key_poweroff;
        CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
    }
}

void ui::CFastCamera::Timer_key_mute(void *para) {
    ui::CFastCamera *cam = (ui::CFastCamera *)para;
    if (cam) {
        //CAMERALOG("OnKeyUp --- code 108, key_down_onKeyUp");
        CameraAppl::getInstance()->PushEvent(eventType_key, key_mute, 0, 0);
    }
}

bool ui::CFastCamera::event_process(struct input_event* ev)
{
    //CAMERALOG("--CFastCamera key, type: %d code: %d, value : %d \n",
    //    ev->type, ev->code, ev->value);
    int event = key_unknow;
    if ((ev->type == 1)) {//key
        if (ev->value == 1) { // press
            //CAMERALOG("--- ev->code : %d", ev->code);
            switch( ev->code)
            {
                case 28:
                    event = key_hid;
                    break;
                case 103:
                    event= key_up;
                    break;
                case 108:
                    event = key_down;
                    break;
                case 105:
                    event = key_left;
                    break;
                case 106:
                    event = key_right;
                    break;
                case 200:
                case 201:
                case 167:
                    event = key_ok;
                    break;
                case 128:
                    event = key_test;
                    break;
                case 113:
                    {
                        struct timeval *tm = NULL;
                        CTimerThread::GetInstance()->CancelTimer(key_mute_Timer);
                        tm = &last_onKeyMute_up_time;
                        if (tm && (tm->tv_sec != 0) && (tm->tv_usec != 0) ) {
                            unsigned int diff_us = (ev->time.tv_sec - tm->tv_sec) * 1000000
                                + (ev->time.tv_usec - tm->tv_usec);
                            if (diff_us < Time_200ms) {
                                //CAMERALOG("Two keys continuously: "
                                //    "(latter_key.onKeyDown.time - former.onKeyUp.time) = %dus", diff_us);
                                return false;
                            }
                        }

                        _bShakeProcess = true;
                    }
                    break;
                case 116:
                    CTimerThread::GetInstance()->ApplyTimer(
                        20 /* 2s */,
                        CFastCamera::powerkeyLongPressTimer,
                        this,
                        false,
                        powerkeyLongTimer);
                    break;
                default:
                    event = key_unknow;
                    break;
            }
            if (CameraAppl::getInstance() != NULL) {
                CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
            }
        } else {
            if (ev->code == 113) {
                _bShakeProcess = false;
                // key_up
                CTimerThread::GetInstance()->ApplyTimer(
                    2, CFastCamera::Timer_key_mute, this, false, key_mute_Timer);
                last_onKeyMute_up_time.tv_sec = ev->time.tv_sec;
                last_onKeyMute_up_time.tv_usec = ev->time.tv_usec;
            } else if (ev->code == 116) {
                CTimerThread::GetInstance()->CancelTimer(powerkeyLongTimer);
            }
        }
    } else if ((ev->type == 5)) {
        switch( ev->code)
        {
            case 3:
                if (!_top_sensor_event) {
                    CAMERALOG("swither1 event comes");
                    // Wait 3s to process the event.
                    _top_sensor_event = 1;
                    CTimerThread::GetInstance()->ApplyTimer
                        (30, CFastCamera::Switcher1Timer, this, false, topswitcherTimer);
                } else {
                    CAMERALOG("skip unstable events");
                }
                //event = key_switch_top;
                //CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
                break;
            case 12:
                if (!_bottom_sensor_event) {
                    CAMERALOG("swither2 event comes");
                    // Wait 3s to process the event.
                    _bottom_sensor_event = 1;
                    CTimerThread::GetInstance()->ApplyTimer
                        (30, CFastCamera::Switcher2Timer, this, false, bottomswitcherTimer);
                } else {
                    CAMERALOG("skip unstable events");
                }
                //event = key_switch_bottom;
                //CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
                break;
        }
    }
    return false;
}

#endif

