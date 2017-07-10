
/*******************************************************************************
**      Camera.cpp
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Description:
**
**      Revision History:
**      -----------------
**      01a 27- 8-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/

/*******************************************************************************
* <Includes>
*******************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <linux/input.h>

#include "agbox.h"
#include "agcmd.h"

#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "App.h"
#include "CameraAppl.h"
#include "class_camera_property.h"
#include "WindowRegistration.h"

extern bool bReleaseVersion;

namespace ui{

#define screen_saver_prop_name "system.ui.screensaver"
#define screen_saver_prop_default "15"
/*******************************************************************************
*   CameraAppl
*/

const static char* uiUpdateTimerName = "ui update Timer";
const static char* tempratureMonitorTimerName = "temprature monitor Timer";


CameraAppl* CameraAppl::_pInstance = NULL;
void CameraAppl::Create(CCameraProperties *cp)
{
    if (_pInstance == NULL) {
        _pInstance = new CameraAppl(cp);
        _pInstance->Go();
    }
}

void CameraAppl::Destroy()
{
    if (_pInstance != NULL) {
        _pInstance->Stop();
        delete _pInstance;
        _pInstance = NULL;
    }
}

void CameraAppl::TempratureMonitorTimer(void *p)
{
    CameraAppl* pp = (CameraAppl*) p;
    if (pp) {
        int temp_critical = 0, temp_high_off = 0;
        char tmp[256];
        agcmd_property_get(PropName_Board_Temp_critical, tmp, Critical_default);
        temp_critical = atoi(tmp);

        agcmd_property_get(PropName_Board_Temp_high_off, tmp, High_off_default);
        temp_high_off = atoi(tmp);

        agcmd_property_get(PropName_Board_Temprature, tmp, "450");
        int board_temprature = atoi(tmp);

        //CAMERALOG("temp_critical = %d, temp_high_off = %d, board_temprature = %d",
        //    temp_critical, temp_high_off, board_temprature);
        if (board_temprature > temp_high_off) {
            CAMERALOG("board_temprature = %d, temp_high_off = %d, overheat! "
                "Call \"agsh poweroff\" to power off now!",
                board_temprature, temp_high_off);
            const char *tmp_args[4];
            tmp_args[0] = "agsh";
            tmp_args[1] = "poweroff";
            tmp_args[2] = NULL;
            agbox_sh(2, (const char *const *)tmp_args);
        } else if (board_temprature > temp_critical) {
            pp->PopupDialogue(DialogueType_Overheat);
        }
    }
}

CameraAppl::CameraAppl(CCameraProperties *cp)
    : CAppl()
    , _cp(cp)
    , _pIMEWnd(NULL)
    , _txtInputed(NULL)
    , _counter(0)
    , _bSpaceWarning(false)
{

}

CameraAppl::~CameraAppl()
{
    CAMERALOG("destroyed");
}

bool CameraAppl::OnEvent(CEvent* event)
{
    //inherited::OnEvent(event);
    if (!event->_bProcessed) {
        //CAMERALOG("on event: %d, %d", event->_type, event->_event);
    }
    switch (event->_type)
    {
        case eventType_internal:
            switch (event->_event)
            {
                default:
                break;
            }
            break;
        case eventType_key:
            {
                switch (event->_event)
                {
                    case key_up:

                    break;
                    case key_down:

                    break;
                }
            }
            break;
        default:
            break;
    }

    return true;
}

void CameraAppl::_checkSpaceWarning()
{
    bool origin = _bSpaceWarning;

    // check whether there is enough space for loop recording:
    _bSpaceWarning = false;
    storage_State st = _cp->getStorageState();
    if (storage_State_full == st) {
        _bSpaceWarning = true;
    } else if (storage_State_ready == st) {
        int totle = 0, marked = 0, loop = 0;
        _cp->getSpaceInfoMB(totle, marked, loop);
        if ((totle > 10 * 1024) && (loop <= 8 * 1024)) {
            _bSpaceWarning = true;
        } else if ((totle > 0) && (totle < 10 * 1024) && (loop <= 6 * 1024)) {
            _bSpaceWarning = true;
        }
    }

    if (!origin && _bSpaceWarning) {
        CameraAppl::getInstance()->PopupDialogue(DialogueType_Loop_Space_Warning);
    }
}

void CameraAppl::_timerUpdate()
{
    if ((++_counter) % 100 == 0) {
        _checkSpaceWarning();
    }
    this->PushEvent(eventType_internal, InternalEvent_app_timer_update, 0, 0);
}

void CameraAppl::SecTimerBtnUpdate(void * p)
{
    CameraAppl* pp = (CameraAppl*) p;
    if (pp) {
        pp->_timerUpdate();
    }
}

void CameraAppl::StartUpdateTimer()
{
    // Time out every 100ms
    CTimerThread::GetInstance()->ApplyTimer
        (2, CameraAppl::SecTimerBtnUpdate, this, true, uiUpdateTimerName);
}

void CameraAppl::StopUpdateTimer()
{
    CTimerThread::GetInstance()->CancelTimer(uiUpdateTimerName);
}

void CameraAppl::StartTempratureMonitor()
{
    // Time out every 1minute
    CTimerThread::GetInstance()->ApplyTimer
        (10 * 60, CameraAppl::TempratureMonitorTimer, this, true, tempratureMonitorTimerName);
}

void CameraAppl::PopupDialogue(DialogueType dialogue)
{
    //if (dialogue != DialogueType_Mark) {
        if (_cp->isDisplayOff()) {
            char tmp[256];
            agcmd_property_get(PropName_Brighness, tmp, Default_Brighness);
            int brightness = atoi(tmp);
            _cp->SetDisplayBrightness(brightness);
        }
    //}

    Window *wnd = NULL;
    wnd = WindowManager::getInstance()->GetCurrentWnd();

    if (wnd && (strcmp(wnd->getWndName(), WINDOW_usb_storage) == 0)
        && ((DialogueType_OBD_Connected == dialogue) ||
            (DialogueType_OBD_Disconnected == dialogue) ||
            (DialogueType_HID_Connected == dialogue) ||
            (DialogueType_HID_Disconnected == dialogue) ||
            (DialogueType_OBD_Not_Binded == dialogue) ||
            (DialogueType_HID_Not_Binded == dialogue)))
    {
        return;
    }

    switch (dialogue)
    {
        case DialogueType_PowerOff:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_poweroff) != 0))
            {
                startDialogue(WINDOW_poweroff, Anim_Bottom2TopCoverEnter);
            }
            break;
        case DialogueType_USBStorage:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_usb_storage) != 0))
            {
                startDialogue(WINDOW_usb_storage, Anim_Bottom2TopCoverEnter);
            }
            break;
        case DialogueType_Car_Mount_Unplug:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_carmountunplug) != 0))
            {
                startDialogue(WINDOW_carmountunplug, Anim_Null);
            }
            break;
        case DialogueType_Space_Full:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_spacefull) != 0))
            {
                startDialogue(WINDOW_spacefull, Anim_Bottom2TopCoverEnter);
            }
            break;
        case DialogueType_Card_Missing:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_cardmissing) != 0))
            {
                startDialogue(WINDOW_cardmissing, Anim_Bottom2TopCoverEnter);
            }
            break;
        case DialogueType_Disk_Error:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_diskerror) != 0))
            {
                startDialogue(WINDOW_diskerror, Anim_Bottom2TopCoverEnter);
            }
            break;
        case DialogueType_UnknownSD_Format:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_UnknownFormat) != 0))
            {
                startDialogue(WINDOW_UnknownFormat, Anim_Bottom2TopCoverEnter);
            }
            break;
        case DialogueType_Mark:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_highlight) != 0))
            {
                startDialogue(WINDOW_highlight, Anim_Null);
            }
            break;
        case DialogueType_Mark_Failed:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_highlight_failed) != 0))
            {
                startDialogue(WINDOW_highlight_failed, Anim_Null);
            }
            break;
        case DialogueType_OBD_Connected:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_obd_connected) != 0))
            {
                startDialogue(WINDOW_obd_connected, Anim_Null);
            }
            break;
        case DialogueType_OBD_Disconnected:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_obd_disconnected) != 0))
            {
                startDialogue(WINDOW_obd_disconnected, Anim_Null);
            }
            break;
        case DialogueType_HID_Connected:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_hid_connected) != 0))
            {
                startDialogue(WINDOW_hid_connected, Anim_Null);
            }
            break;
        case DialogueType_HID_Disconnected:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_hid_disconnected) != 0))
            {
                startDialogue(WINDOW_hid_disconnected, Anim_Null);
            }
            break;
        case DialogueType_OBD_Not_Binded:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_OBD_not_binded) != 0))
            {
                startDialogue(WINDOW_OBD_not_binded, Anim_Null);
            }
            break;
        case DialogueType_HID_Not_Binded:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_HID_not_binded) != 0))
            {
                startDialogue(WINDOW_HID_not_binded, Anim_Null);
            }
            break;
        case DialogueType_Overheat:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_overheat) != 0))
            {
                startDialogue(WINDOW_overheat, Anim_Null);
            }
            break;
        case DialogueType_Wifi_pwd_wrong:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_wrong_wifi_pwd) != 0))
            {
                startDialogue(WINDOW_wrong_wifi_pwd, Anim_Null);
            }
            break;
        case DialogueType_Wifi_add_success:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_add_wifi_success) != 0))
            {
                startDialogue(WINDOW_add_wifi_success, Anim_Null);
            }
            break;
        case DialogueType_Loop_Space_Warning:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_loop_space_warning) != 0))
            {
                startDialogue(WINDOW_loop_space_warning, Anim_Null);
            }
            break;
        case DialogueType_Space_Warning_guide:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_space_warning_guide) != 0))
            {
                startDialogue(WINDOW_space_warning_guide, Anim_Null);
            }
            break;
        case DialogueType_OBD_Defect:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_obd_defect) != 0))
            {
                if (!bReleaseVersion) {
                    startDialogue(WINDOW_obd_defect, Anim_Null);
                }
            }
            break;
        case DialogueType_OBD_Upgrading:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_obd_upgrading) != 0))
            {
                startDialogue(WINDOW_obd_upgrading, Anim_Null);
            }
            break;
        case DialogueType_HID_Upgrading:
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_hid_upgrading) != 0))
            {
                startDialogue(WINDOW_hid_upgrading, Anim_Null);
            }
            break;
        default:
            break;
    }
}

void CameraAppl::StartIME(char *title, bool bPassword)
{
    if (!_pIMEWnd) {
        int width = 0, height = 0, bitdepth = 0;
        WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
        _pIMEWnd = new IMEWnd(_cp, WINDOW_IME, Size(width, height), Point(0, 0));
    }

    if (_txtInputed) {
        delete[] _txtInputed;
        _txtInputed = NULL;
    }
    _pIMEWnd->setInitValue(title, bPassword);

    WindowManager::getInstance()->StartWnd(WINDOW_IME, Anim_Bottom2TopCoverEnter);
}

void CameraAppl::updateTextInputed(char *txt)
{
    if (_txtInputed) {
        delete[] _txtInputed;
        _txtInputed = NULL;
    }

    if (txt && (strlen(txt) > 0)) {
        _txtInputed = new char [strlen(txt) + 1];
        if (_txtInputed) {
            snprintf(_txtInputed, strlen(txt) + 1, "%s", txt);
        }
    }
}

char* CameraAppl::GetInputedText()
{
    return _txtInputed;
}

}
