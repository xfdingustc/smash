
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
    , _dvState(DvState_idel)
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
        (20 * 60, CameraAppl::TempratureMonitorTimer, this, true, tempratureMonitorTimerName);
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
        default:
            break;
    }
}

}
