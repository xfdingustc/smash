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
#include <math.h>

#include "fast_Camera.h"
#include "agbox.h"
#include "agsys_input.h"
#include "aglib.h"
#include "GobalMacro.h"
#include "CameraWndFactory.h"
#include "WindowRegistration.h"
#include "RotateMonitor.h"

#define ui_standalong 0

extern void getStoragePropertyName(char *prop, int len);

namespace ui{

#define TOUCH_R   250
#define SCREEN_R  200
#define T_S_DIFF  (TOUCH_R - SCREEN_R)
const static char* languageTxtDir = "/usr/local/share/ui/";
const static char* keyrightlongpressTimer = "key right Timer";
const static char* keyoklongpressTimer = "key ok Timer";


CFastCamera::CFastCamera(CCameraProperties* pp)
    : KeyEventProcessor(NULL)
    , _cp(pp)
    , _pCamWnd(NULL)
    , _dataReceived(0x00)
    , _flickValue(0)
    , _hasFlickEvent(false)
    , _bMoveTriggered(false)
    , _bKeyRightLongPressed(false)
    , _bKeyOKLongPressed(false)
    , _bRotate180(false)
    , _mountStatus(Mount_DEVICE_Status_NULL)
    , _bOBDBinded(false)
    , _bHIDBinded(false)
    , _bt_state(bt_state_off)
{
    WindowFactory *factory = new CameraWndFactory(pp);
    WindowManager::Create(factory);
    CameraAppl::Create(pp);
    {
        pp->SetObserver(this);
        char tmp[256];
        memset(tmp, 0x00, sizeof(tmp));
        agcmd_property_get(PropName_Language, tmp, "English");
        CAMERALOG("system.ui.language = %s", tmp);

        CLanguageLoader::Create(languageTxtDir, tmp);

#ifdef CircularUI
        if (WindowManager::getInstance()) {
            int width = 0, height = 0, bitdepth = 0;
            WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
            _pCamWnd = new GaugesWnd(pp, WINDOW_gauges, Size(width, height), Point(0, 0));
            _pCamWnd->SetApp(CameraAppl::getInstance());
        }
        // Init key filters:
        memset(&last_touch_end_time, 0x00, sizeof(last_touch_end_time));
#endif

        EventProcessThread::getEPTObject()->AddProcessor(this);

        agcmd_property_set("system.input.key_mute_diff", "10000");
    }

    bluetooth_state bt = _cp->getBluetoothState();
    _bt_state = bt;
    if (bt == bt_state_hid_obd_paired) {
        _bOBDBinded = true;
        _bHIDBinded = true;
    } else if (bt == bt_state_hid_paired) {
        _bOBDBinded = false;
        _bHIDBinded = true;
    } else if (bt == bt_state_obd_paired) {
        _bOBDBinded = true;
        _bHIDBinded = false;
    } else {
        _bOBDBinded = false;
        _bHIDBinded = false;
    }

    RotateMonitor::Create(pp);
    _bRotate180 = _isRotate180();
}

CFastCamera::~CFastCamera()
{
    RotateMonitor::releaseInstance();
    EventProcessThread::getEPTObject()->RemoveProcessor(this);
    CameraAppl::getInstance()->StopUpdateTimer();
    CameraAppl::Destroy();
    CLanguageLoader::Destroy();
    WindowManager::Destroy();
}

void CFastCamera::Start()
{
#ifdef CircularUI
    //_pCamWnd->Show(false);
    _pCamWnd->StartWnd(WINDOW_gauges);

    int brightness = _cp->GetBrightness();
    // For the first time, never set brightness to 0
    _cp->SetDisplayBrightness(brightness < 30 ? 30 : brightness);

    // start timer
    CameraAppl::getInstance()->StartUpdateTimer();
    CameraAppl::getInstance()->StartTempratureMonitor();

    if (_bOBDBinded) {
        char tmp[256];
        agcmd_property_get(Prop_OBD_Defect, tmp, "");
        CAMERALOG("OBD defect code: %s", tmp);
        if (strcasecmp(tmp, "") != 0) {
            sleep(1);
            CameraAppl::getInstance()->PopupDialogue(DialogueType_OBD_Defect);
        }
    }
#endif
}

void CFastCamera::Stop()
{

}

void CFastCamera::CheckStatusOnBootup()
{
    // check whether mount on any device
    //int ret_val = agsys_input_read_switch(SW_DOCK);
    _getMountStatus();
    char tmp[256];
    if ((Mount_DEVICE_Status_Car_Mount_With_Power == _mountStatus)
        ||(Mount_DEVICE_Status_Car_Mount_No_Power == _mountStatus))
    {
        memset(tmp, 0x00, sizeof(tmp));
        agcmd_property_get(PropName_Auto_Hotspot, tmp, Auto_Hotspot_Enabled);
        if (strcasecmp(tmp, Auto_Hotspot_Enabled) == 0) {
            _cp->SetWifiMode(wifi_mode_AP);
        }
    }
    if (Mount_DEVICE_Status_Car_Mount_With_Power == _mountStatus) {
        memset(tmp, 0x00, sizeof(tmp));
        agcmd_property_get(PropName_Auto_Record, tmp, Auto_Record_Enabled);
        if (strcasecmp(tmp, Auto_Record_Enabled) == 0) {
            CameraAppl::getInstance()->PushEvent(
                eventType_external,
                MountEvent_Plug_To_Car_Mount,
                1, 0);
        }
    } else if ((Mount_DEVICE_Status_USB_Mount == _mountStatus)
        && _USBStorageReady())
    {
        storage_State st = storage_State_noMedia;
        int counter = 0;
        do {
            sleep(1);
            st = _cp->getStorageState();
            counter++;
        } while ((st != storage_State_ready) && (counter < 5));
        if (st == storage_State_ready) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_USBStorage);
        }
    } else {
        storage_State st = _cp->getStorageState();
        if (storage_State_usb == st) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_USBStorage);
        }
    }
}

bool CFastCamera::_USBStorageReady()
{
    bool rt = true;

    char tmp[256];
    agcmd_property_get(USBSotragePropertyName, tmp, "");
    if (strcasecmp(tmp, "Ready") != 0) {
        //CAMERALOG("It is in status %s, not ready for enable USB storage.", tmp);
        rt = false;
        return rt;
    }

    memset(tmp, 0x00, sizeof(tmp));
    char storage_prop[256];
    getStoragePropertyName(storage_prop, sizeof(storage_prop));
    agcmd_property_get(storage_prop, tmp, "");
    //CAMERALOG("TF status is %s before try to enable USB Storage", tmp);
    if (strcasecmp(tmp, "mount_ok") != 0) {
        rt = false;
        return rt;
    }

    return rt;
}

void CFastCamera::_getMountStatus()
{
    int ret_val = -1;
    char usbphy0_data[1024];
    char *sw_str;

    ret_val = aglib_read_file_buffer("/proc/ambarella/usbphy0",
        usbphy0_data, (sizeof(usbphy0_data) - 1));
    if (ret_val < 0) {
        CAMERALOG("read /proc/ambarella/usbphy0 failed with error %d", ret_val);
        return;
    }
    usbphy0_data[ret_val] = 0;
    //CAMERALOG("%s", usbphy0_data);

    // MODE & STATUS
    char status[16];
    memset(status, 0x00, 16);
    char *begin = NULL;
    char *end = NULL;
    begin = strstr(usbphy0_data, "STATUS[");
    if (begin) {
        begin += strlen("STATUS[");
        end = strstr(begin, "]");
        memcpy(status, begin, (end - begin));
    }

    // VBUS
    int vbus = -1;
    sw_str = strstr(usbphy0_data, "VBUS[");
    if (sw_str) {
        vbus = strtoul((sw_str + 5), NULL, 0);
    }

    // GPIO
    int gpio = -1;
    sw_str = strstr(usbphy0_data, "GPIO[");
    if (sw_str) {
        gpio = strtoul((sw_str + 5), NULL, 0);
    }

    if (strcmp(status, "DEVICE") == 0) {
        if ((vbus == 0) && (gpio == 0)) {
            _mountStatus = Mount_DEVICE_Status_NULL;
        } else if ((vbus == 1) && (gpio == 0)) {
            _mountStatus = Mount_DEVICE_Status_USB_Mount;
        }
    } else if (strcmp(status, "HOST") == 0) {
        if ((vbus == 0) && (gpio == 1)) {
            _mountStatus = Mount_DEVICE_Status_Car_Mount_No_Power;
        } else if ((vbus == 1) && (gpio == 1)) {
            _mountStatus = Mount_DEVICE_Status_Car_Mount_With_Power;
        }
    }
    CAMERALOG("status = %s, vbus = %d, gpio = %d, _mountStatus = %d",
       status, vbus, gpio, _mountStatus);
}

bool CFastCamera::_isUnknownSDPluged()
{
    // delay in case unplug SD card
    usleep(100 * 1000);

    bool ret = false;

    char storage_prop[256];
    getStoragePropertyName(storage_prop, sizeof(storage_prop));
    char tmp[256];
    memset(tmp, 0x00, sizeof(tmp));
    agcmd_property_get(storage_prop, tmp, "");
    if (strcasecmp(tmp, "unknown") == 0) {
        ret = true;
    }

    return ret;
}

bool CFastCamera::_isAutoOffEnabled()
{
    bool ret = true;

    char tmp[256];
    agcmd_property_get(PropName_Udc_Power_Delay, tmp, Udc_Power_Delay_Default);
    if (strcasecmp(tmp, "Never") == 0) {
        ret = false;
    }

    return ret;
}

void CFastCamera::_updateBTStatus()
{
    bluetooth_state bt = _cp->getBluetoothState();
    if (bt == bt_state_hid_obd_paired) {
        if (!_bOBDBinded) {
            char tmp[256];
            agcmd_property_get(Prop_OBD_Defect, tmp, "");
            CAMERALOG("OBD defect code: %s", tmp);
            if (strcasecmp(tmp, "") != 0) {
                CameraAppl::getInstance()->PopupDialogue(DialogueType_OBD_Defect);
            }

            CameraAppl::getInstance()->PopupDialogue(DialogueType_OBD_Connected);
            _bOBDBinded = true;
        }
        if (!_bHIDBinded) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_HID_Connected);
            _bHIDBinded = true;
        }
    } else if (bt == bt_state_hid_paired) {
        if (!_bHIDBinded) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_HID_Connected);
            _bHIDBinded = true;
        }
        if (_bOBDBinded) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_OBD_Disconnected);
            _bOBDBinded = false;
        }
    } else if (bt == bt_state_obd_paired) {
        if (_bHIDBinded) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_HID_Disconnected);
            _bHIDBinded = false;
        }
        if (!_bOBDBinded) {
            char tmp[256];
            agcmd_property_get(Prop_OBD_Defect, tmp, "");
            CAMERALOG("OBD defect code: %s", tmp);
            if (strcasecmp(tmp, "") != 0) {
                CameraAppl::getInstance()->PopupDialogue(DialogueType_OBD_Defect);
            }

            CameraAppl::getInstance()->PopupDialogue(DialogueType_OBD_Connected);
            _bOBDBinded = true;
        }
    } else {
        if (_bOBDBinded) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_OBD_Disconnected);
            _bOBDBinded = false;
        }
        if (_bHIDBinded) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_HID_Disconnected);
            _bHIDBinded = false;
        }
#if 0
        if ((bt == bt_state_on) && (_bt_state == bt_state_off)) {
            char hid_mac[256];
            agcmd_property_get(propHIDMac, hid_mac, "NA");
            if (strcasecmp(hid_mac, "NA") == 0)
            {
                agcmd_property_get(Prop_HID_MAC_Backup, hid_mac, "NA");
            }
            if (strcasecmp(hid_mac, "NA") == 0)
            {
                CameraAppl::getInstance()->PopupDialogue(DialogueType_HID_Not_Binded);
            }

            if (Mount_DEVICE_Status_Car_Mount_With_Power == _mountStatus) {
                char obd_mac[256];
                agcmd_property_get(propOBDMac, obd_mac, "NA");
                if (strcasecmp(obd_mac, "NA") == 0)
                {
                    agcmd_property_get(Prop_OBD_MAC_Backup, obd_mac, "NA");
                }
                if (strcasecmp(obd_mac, "NA") == 0)
                {
                    CameraAppl::getInstance()->PopupDialogue(DialogueType_OBD_Not_Binded);
                }
            }
        }
#endif
    }

    _bt_state = bt;

    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        agcmd_property_get(propBTTempOBDStatus, tmp, "");
        if (strcasecmp(tmp, "upgrading") == 0) {
            CAMERALOG("OBD upgrading");
            CameraAppl::getInstance()->PopupDialogue(DialogueType_OBD_Upgrading);
        }

        agcmd_property_get(propBTTempHIDStatus, tmp, "");
        if (strcasecmp(tmp, "upgrading") == 0) {
            CAMERALOG("HIDq upgrading");
            CameraAppl::getInstance()->PopupDialogue(DialogueType_HID_Upgrading);
        }
    }
}

bool CFastCamera::_isRotate180()
{
    bool bRotate = false;
    char horizontal[256];
    char vertical[256];
    memset(horizontal, 0x00, 256);
    memset(vertical, 0x00, 256);
    agcmd_property_get(PropName_Rotate_Horizontal, horizontal, "0");
    agcmd_property_get(PropName_Rotate_Vertical, vertical, "0");
    if ((strcasecmp(horizontal, "0") == 0)
        && (strcasecmp(vertical, "0") == 0))
    {
        bRotate = false;
    } else if ((strcasecmp(horizontal, "1") == 0)
        && (strcasecmp(vertical, "1") == 0))
    {
        bRotate = true;
    }

    return bRotate;
}

void CFastCamera::OnPropertyChange(void* p, PropertyChangeType type)
{
    if (CameraAppl::getInstance() != NULL) {
        if ((type == PropertyChangeType_usb_storage) && _USBStorageReady())
        {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_USBStorage);
        } else if ((type == PropertyChangeType_storage) && _isUnknownSDPluged()) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_UnknownSD_Format);
        } else {
            if (type == PropertyChangeType_bluetooth) {
                _updateBTStatus();
            } else if (type == PropertyChangeType_rotation) {
                _bRotate180 = _isRotate180();
            }
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_property_update, type, 0);
        }
    }
}

#ifdef ENABLE_STILL_CAPTURE
void CFastCamera::onStillCaptureStarted(int one_shot) {
    if (CameraAppl::getInstance() != NULL) {
        CameraAppl::getInstance()->PushEvent(
            eventType_StillCapture,
            StillCapture_Start,
            one_shot,
            0);
    }
}

void CFastCamera::sendStillCaptureInfo(
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

void CFastCamera::onModeChanged() {
    if (CameraAppl::getInstance() != NULL) {
        CameraAppl::getInstance()->PushEvent(
            eventType_Cam_mode,
            0,
            0,
            0);
    }
}

void CFastCamera::onCameraState(CameraState state, int payload) {
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
        } else if (state == CameraState_mark) {
            if (payload) {
                CameraAppl::getInstance()->PopupDialogue(
                    DialogueType_Mark_Failed);
            } else {
                CameraAppl::getInstance()->PopupDialogue(
                    DialogueType_Mark);
            }
        } else if (state == CameraState_writeslow) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_WriteSlow, 0);
        } else if (state == CameraState_write_diskerror) {
            // delay 500ms
            storage_State st = _cp->getStorageState();
            if (st != storage_State_noMedia) {
                CameraAppl::getInstance()->PopupDialogue(
                    DialogueType_Disk_Error);
            }
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
            //CameraAppl::getInstance()->PushEvent(
            //    eventType_internal, InternalEvent_app_state_update,
            //    camera_State_Storage_full, 0);
            CameraAppl::getInstance()->PopupDialogue(
                    DialogueType_Space_Full);
        } else if (state == CameraState_storage_cardmissing) {
            CameraAppl::getInstance()->PopupDialogue(
                    DialogueType_Card_Missing);
        } else if (state == CameraState_FormatSD_Result) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_FormatSD_result, payload);
        } else if (state == CameraState_enable_usbstrorage) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_enable_usbstorage_result, payload);
        } else if (state == CameraState_disable_usbstrorage) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_disable_usbstorage_result, payload);
        } else if (state == CameraState_scanWifi_result) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_scanwifi_result, payload);
        } else if (state == CameraState_connectWifi_result) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_connectwifi_result, payload);
        } else if (state == CameraState_scanBT_result) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_scanBT_result, payload);
        } else if (state == CameraState_bindBT_result) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_bindBT_result, payload);
        } else if (state == CameraState_Wifi_pwd_error) {
            Window *wnd = NULL;
            wnd = WindowManager::getInstance()->GetCurrentWnd();
            if ((!wnd) || (strcmp(wnd->getWndName(), WINDOW_wifi_detail) != 0))
            {
                if (payload) {
                    CameraAppl::getInstance()->PopupDialogue(
                        DialogueType_Wifi_add_success);
                } else {
                    CameraAppl::getInstance()->PopupDialogue(
                        DialogueType_Wifi_pwd_wrong);
                }
            } else {
                CameraAppl::getInstance()->PushEvent(
                    eventType_internal, InternalEvent_app_state_update,
                    camera_State_Wifi_pwd_error, payload);
            }
        } else if (state == CameraState_Wifi_add_result) {
            CameraAppl::getInstance()->PushEvent(
                eventType_internal, InternalEvent_app_state_update,
                camera_State_addwifi_result, payload);
        }
    }
}

void CFastCamera::KeyRightLongPressTimer(void *para) {
    CFastCamera *cam = (CFastCamera *)para;
    if (cam) {
        CAMERALOG("OnKeyLongPress --- key_right_onKeyLongPressed");
        CameraAppl::getInstance()->PushEvent(
            eventType_key, key_right_onKeyLongPressed, 0, 0);
        cam->_bKeyRightLongPressed = true;
    }
}

void CFastCamera::KeyOKLongPressTimer(void *para) {
    CFastCamera *cam = (CFastCamera *)para;
    if (cam) {
        CAMERALOG("OnKeyLongPress --- key_ok_onKeyLongPressed");
        CameraAppl::getInstance()->PushEvent(
            eventType_key, key_ok_onKeyLongPressed, 0, 0);
        CameraAppl::getInstance()->PopupDialogue(DialogueType_PowerOff);
        cam->_bKeyOKLongPressed = true;
    }
}

int getRotateX(bool isRotate, int x)
{
    if (isRotate) {
        x = 400 - x;
    }

    return x;
}

int getRotateGesture(bool isRotate, int flickvalue)
{
    if (isRotate) {
        switch (flickvalue) {
            case TouchFlick_UP:
                flickvalue = TouchFlick_Down;
                break;
            case TouchFlick_Down:
                flickvalue = TouchFlick_UP;
                break;
            case TouchFlick_Right:
                flickvalue = TouchFlick_Left;
                break;
            case TouchFlick_Left:
                flickvalue = TouchFlick_Right;
                break;
            default:
                break;
        }
    }

    return flickvalue;
}

bool CFastCamera::event_process(struct input_event* ev)
{
    //CAMERALOG("key event: type = %d code = %d value = %d",
    //    ev->type, ev->code, ev->value);
    if ((ev->type == EV_SYN) && (ev->code == 0) && (ev->value == 0)) {
        bool bRotate = _bRotate180;

        // get coordinate
        if ((_dataReceived & DATA_TYPE_x) || (_dataReceived & DATA_TYPE_y)) {
            bool bOnScreen = (abs(_x - TOUCH_R) <= SCREEN_R)
                && (abs(_y - TOUCH_R) <= SCREEN_R);
            if (bOnScreen) {
                CameraAppl::getInstance()->PushEvent(
                        eventType_touch,
                        TouchEvent_coordinate,
                        getRotateX(bRotate, _x - T_S_DIFF),
                        getRotateX(bRotate, _y - T_S_DIFF));
            } else {
                //CAMERALOG("point(%d, %d) is not on screen", _x, _y);
            }
            if (!_bMoveTriggered && (((_x - _xOnPressed) * (_x - _xOnPressed)
                + (_y - _yOnPressed) * (_y - _yOnPressed)) > 8 * 8))
            {
                _bMoveTriggered = true;
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_Move_Began,
                    getRotateX(bRotate, _xOnPressed - T_S_DIFF),
                    getRotateX(bRotate, _yOnPressed - T_S_DIFF));
            }
            if (_bMoveTriggered && ((_x != _xOnPressed) || (_y != _yOnPressed))) {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_Move_Changed,
                    (bRotate ? -1 : 1) * (_x - _xOnPressed),
                    (bRotate ? -1 : 1) * (_y - _yOnPressed));
            }
            _dataReceived &= ~(DATA_TYPE_x | DATA_TYPE_y);

            if (_dataReceived & DATA_TYPE_press) {
                if ((abs(_x_pressed - _x) > 15)
                    || (abs(_y_pressed - _y) > 15))
                {
                    //CAMERALOG("#### now at (%d, %d) while pressed on (%d, %d), "
                    //    "press is invalid", _x, _y, _x_pressed, _y_pressed);
                    _dataReceived &= ~DATA_TYPE_press;
                }
            }
        }

        if (_dataReceived & DATA_TYPE_touch_up) {
            bool bOnScreen = (abs(_x - TOUCH_R) <= SCREEN_R)
                && (abs(_y - TOUCH_R) <= SCREEN_R);
            if (bOnScreen)
            {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_OnReleased,
                    getRotateX(bRotate, _x - T_S_DIFF),
                    getRotateX(bRotate, _y - T_S_DIFF));
            } else {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_OnReleased,
                    400,
                    400);
                //CAMERALOG("point(%d, %d) is not on screen", _x, _y);
            }
            // send move ended, if there is flick event, then it will end with flick
            CameraAppl::getInstance()->PushEvent(
                eventType_touch,
                TouchEvent_Move_Ended,
                getRotateGesture(bRotate, _flickValue),
                0/* TODO: speed instead */);

            if (_dataReceived & DATA_TYPE_flick) {
                // send flick event if any
                int x_pos = _x - T_S_DIFF;
                if (x_pos < 0) {
                    x_pos = 0;
                } else if (x_pos > 400) {
                    x_pos = 400;
                }
                int y_pos = _y - T_S_DIFF;
                if (y_pos < 0) {
                    y_pos = 0;
                } else if (y_pos > 400) {
                    y_pos = 400;
                }
                UINT32 position = 0x00;
                position = (getRotateX(bRotate, x_pos) << 16) | getRotateX(bRotate, y_pos);
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_flick,
                    getRotateGesture(bRotate, _flickValue),
                    position);
            }
            if ((_dataReceived & DATA_TYPE_tap) && bOnScreen)
            {
                // send touch click event if any
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_SingleClick,
                    getRotateX(bRotate, _x - T_S_DIFF),
                    getRotateX(bRotate, _y - T_S_DIFF));
            }
            if ((_dataReceived & DATA_TYPE_press) && bOnScreen)
            {
                if (((_dataReceived & DATA_TYPE_tap) == 0x00)
                    && ((_dataReceived & DATA_TYPE_flick) == 0x00)
                    && ((_dataReceived & DATA_TYPE_double_click) == 0x00))
                {
                    // treat press as a kind of single click
                    //CAMERALOG("#### Only press event happens, treat it as a single click.");
                    CameraAppl::getInstance()->PushEvent(
                        eventType_touch,
                        TouchEvent_SingleClick,
                        getRotateX(bRotate, _x_pressed - T_S_DIFF),
                        getRotateX(bRotate, _y_pressed - T_S_DIFF));
                }
            }
            // reset values
            _flickValue = 0;
            _dataReceived = 0x00;
        }

        if (_dataReceived & DATA_TYPE_double_click) {
            bool bOnScreen = (abs(_x - TOUCH_R) <= SCREEN_R)
                && (abs(_y - TOUCH_R) <= SCREEN_R);
            if (bOnScreen)
            {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_DoubleClick,
                    getRotateX(bRotate, _x - T_S_DIFF),
                    getRotateX(bRotate, _y - T_S_DIFF));
            }
        }

        if (_dataReceived & DATA_TYPE_touch_down) {
            bool bOnScreen = (abs(_x - TOUCH_R) <= SCREEN_R)
                && (abs(_y - TOUCH_R) <= SCREEN_R);
            if (bOnScreen)
            {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_OnPressed,
                    getRotateX(bRotate, _x - T_S_DIFF),
                    getRotateX(bRotate, _y - T_S_DIFF));
            }
            // reset record at the beginning of each operation
            _hasFlickEvent = false;
            _dataReceived &= ~DATA_TYPE_touch_down;
        }
    } else if ((ev->type == EV_KEY) && (ev->code == 330)) {
        // Touch pressed/released event
        if (ev->value == 1)
        {
            _xOnPressed = _x;
            _yOnPressed = _y;
            //CAMERALOG("_xOnPressed = %d, _yOnPressed = %d", _xOnPressed, _yOnPressed);
            _dataReceived |= DATA_TYPE_touch_down;
        } else if (ev->value == 0) {
            _dataReceived |= DATA_TYPE_touch_up;
            _bMoveTriggered = false;
            last_touch_end_time.tv_sec = ev->time.tv_sec;
            last_touch_end_time.tv_usec = ev->time.tv_usec;
        }
    } else if ((ev->type == EV_KEY) && (ev->code == 212)) {
        if (ev->value == 1) {
            // key down event from remote controller, start a timer to check whether long pressed:
            //     If key is pressed more than 2s, it is a long press event.
            CTimerThread::GetInstance()->ApplyTimer(
                20, CFastCamera::KeyRightLongPressTimer, this, false, keyrightlongpressTimer);
            _bKeyRightLongPressed = false;
        } else if (ev->value == 0) {
            // key up event from remote controller
            CTimerThread::GetInstance()->CancelTimer(keyrightlongpressTimer);
            if (!_bKeyRightLongPressed) {
                CameraAppl::getInstance()->PushEvent(
                    eventType_key,
                    key_right_onKeyUp,
                    0, 0);
            }
            _bKeyRightLongPressed = false;
        }
    } else if ((ev->type == EV_KEY) && (ev->code == 116)) {
        if (ev->value == 1) {
            // key down event from power key, start a timer to check whether long pressed:
            //     If key is pressed more than 2s, it is a long press event.
            CTimerThread::GetInstance()->ApplyTimer(
                20, CFastCamera::KeyOKLongPressTimer, this, true, keyoklongpressTimer);
            _bKeyOKLongPressed = false;

            // send event to exit screen saver
            CameraAppl::getInstance()->PushEvent(
                eventType_key, key_ok_onKeyDown, 0, 0);
        } else if (ev->value == 0) {
            // key up event from power key
            CTimerThread::GetInstance()->CancelTimer(keyoklongpressTimer);
            /*if (!_bKeyRightLongPressed) {
                CameraAppl::getInstance()->PushEvent(
                    eventType_key,
                    key_ok_onKeyUp,
                    0, 0);
            }*/
            _bKeyOKLongPressed = false;
        }
    } else if ((ev->type == EV_KEY) && (ev->code == 226)) {
        if (ev->value == 0) {
            //CAMERALOG("Car mount without power unpluged!");
            _mountStatus = Mount_DEVICE_Status_NULL;
        } else if (ev->value == 1) {
            //CAMERALOG("Car mount without power pluged!");
            _mountStatus = Mount_DEVICE_Status_Car_Mount_No_Power;
        }
    } else if ((ev->type == EV_SW) && (ev->code == 5)) {
        if (ev->value == 0) {
            if (Mount_DEVICE_Status_Car_Mount_With_Power == _mountStatus) {
                //CAMERALOG("Car mount with power unpluged!");
                CameraAppl::getInstance()->PushEvent(
                    eventType_external,
                    MountEvent_UnPlug_From_Car_Mount,
                    1, 0);
                // TODO: to ensure exit screen saver firstly
                usleep(300 * 1000);

                if (_isAutoOffEnabled()) {
                    CameraAppl::getInstance()->PopupDialogue(
                        DialogueType_Car_Mount_Unplug);
                }
            } else if (Mount_DEVICE_Status_USB_Mount == _mountStatus) {
                //CAMERALOG("USB mount unpluged!");
            }
        } else if (ev->value == 1) {
            _getMountStatus();
            if (Mount_DEVICE_Status_Car_Mount_With_Power == _mountStatus) {
                //CAMERALOG("Car mount with power plugged!");
                char tmp[256];
                memset(tmp, 0x00, sizeof(tmp));
                agcmd_property_get(PropName_Auto_Hotspot, tmp, Auto_Hotspot_Enabled);
                if (strcasecmp(tmp, Auto_Hotspot_Enabled) == 0) {
                    _cp->SetWifiMode(wifi_mode_AP);
                }

                memset(tmp, 0x00, sizeof(tmp));
                agcmd_property_get(PropName_Auto_Record, tmp, Auto_Record_Enabled);
                if (strcasecmp(tmp, Auto_Record_Enabled) == 0) {
                    CameraAppl::getInstance()->PushEvent(
                        eventType_external,
                        MountEvent_Plug_To_Car_Mount,
                        1, 0);
                }
            } else if (Mount_DEVICE_Status_USB_Mount == _mountStatus) {
                //CAMERALOG("USB mount plugged!");
                CameraAppl::getInstance()->PushEvent(
                    eventType_external,
                    MountEvent_Plug_To_USB,
                    1, 0);
            }
        }
    } else if ((ev->type == EV_REL) && (ev->code == REL_X)) {
        _x_rel = ev->value;
    } else if ((ev->type == EV_REL) && (ev->code == REL_Y)) {
        _y_rel = ev->value;
    } else if ((ev->type == EV_ABS) && (ev->code == ABS_X)) {
        _x = ev->value;
        _dataReceived |= DATA_TYPE_x;
    } else if ((ev->type == EV_ABS) && (ev->code == ABS_Y)) {
        _y = ev->value;
        _dataReceived |= DATA_TYPE_y;
    } else if (ev->type == EV_MSC) {
        //CAMERALOG("#### ev->value = 0x%x", ev->value);
        int eventType = (ev->value >> 24) & 0xFF;
        if (eventType == 0x20) {
            //CAMERALOG("#### tap on (%d, %d)", _x, _y);
            _dataReceived |= DATA_TYPE_tap;
        } else if (eventType == 0x21) {
            //CAMERALOG("#### press on (%d, %d)", _x, _y);
            _x_pressed = _x;
            _y_pressed = _y;
            _dataReceived |= DATA_TYPE_press;
        } else if (eventType == 0x22) {
            _flickValue = ev->value & 0xFF;
            if (_flickValue != TouchFlick_disabled) {
                int distance = _x_rel * _x_rel + _y_rel * _y_rel;
                const int threshold = 30;
                if (distance >= threshold * threshold) {
                    //CAMERALOG("#### flick = 0x%x, from (%d, %d) with movement (%d, %d)",
                    //    _flickValue, _x, _y, _x_rel, _y_rel);
                    _dataReceived |= DATA_TYPE_flick;
                    _hasFlickEvent = true;
                } else {
                    //CAMERALOG("#### flick = 0x%x, from (%d, %d) with movement "
                    //    "(%d, %d) which is smaller than %d pixels",
                    //    _flickValue, _x, _y, _x_rel, _y_rel, threshold);
                    _flickValue = 0;
                }
            }
        } else if (eventType == 0x23) {
            //CAMERALOG("#### double click on (%d, %d)", _x, _y);
            _dataReceived |= DATA_TYPE_double_click;
            _hasFlickEvent = true;
        }
    }
    return false;
}

}
