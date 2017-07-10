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

#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
#include "CLanguage.h"
#include "Control.h"
#include "Wnd.h"
#include "App.h"
#include "Style.h"
#include "CCameraViditUI.h"
#include "class_camera_property.h"
#include <stdlib.h>
#include <unistd.h>
#include "CameraAppl.h"
#include "agbox.h"

static const char* ViditResourceDir = "/usr/local/share/ui/diablo/";


/*******************************************************************************
*   CViditLanguageSybl
*
*/
ui::CViditLanguageSybl::CViditLanguageSybl(
    CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    ):inherited(pParent, leftTop, CSize, left)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_language] = new CBmp(WndResourceDir, "icon_brightness.bmp");
    SetSymbles(symble_num, symbles);

    this->SetText((UINT8*)NULL);
    SetTitles(0, NULL);
}

ui::CViditLanguageSybl::~CViditLanguageSybl()
{
}

bool ui::CViditLanguageSybl::OnEvent(CEvent *event)
{
    return false;
}



/*******************************************************************************
*   CViditAudioSybl
*
*/
ui::CViditAudioSybl::CViditAudioSybl(
    CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    , CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_audioOn]  = new CBmp(ViditResourceDir, "mic.bmp");
    symbles[symble_audioOff] = new CBmp(ViditResourceDir, "micmute.bmp");
    SetSymbles(symble_num, symbles);

    this->SetText((UINT8*)NULL);
    SetTitles(0, NULL);
}

ui::CViditAudioSybl::~CViditAudioSybl()
{
}

void ui::CViditAudioSybl::updateSymble() {
    char tmp[256];
    int volume = 0;
    agcmd_property_get(audioGainPropertyName, tmp, "NA");
    if (strcmp(tmp, "NA")==0) {
        volume = defaultMicGain;
    } else {
        volume = atoi(tmp);
    }
    agcmd_property_get(audioStatusPropertyName, tmp, "NA");
    if ((strcmp(tmp, "mute") == 0) || (strcmp(tmp, "Mute") == 0) || (volume == 0)) {
        this->SetCurrentSymble(symble_audioOff);
    } else if ((strcmp(tmp, "normal") == 0) || (strcmp(tmp, "Normal") == 0)) {
        this->SetCurrentSymble(symble_audioOn);
    }
}

bool ui::CViditAudioSybl::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal)
        && (event->_event == InternalEvent_app_property_update)
        && (event->_paload == PropertyChangeType_audio))
    {
        updateSymble();
    }
    return true;
}


/*******************************************************************************
*   CViditWifiSybl
*
*/
ui::CViditWifiSybl::CViditWifiSybl(
    CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    , CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_ap]  = new CBmp(ViditResourceDir, "wifiap.bmp");
    symbles[symble_clt] = new CBmp(ViditResourceDir, "wificlt.bmp");
    symbles[symble_off] = new CBmp(ViditResourceDir, "wifioff.bmp");
    SetSymbles(symble_num, symbles);

    this->SetText((UINT8*)NULL);
    SetTitles(0, NULL);
}

ui::CViditWifiSybl::~CViditWifiSybl()
{
}

void ui::CViditWifiSybl::updateSymble() {
    wifi_mode mode = _cp->getWifiMode();
    switch (mode)
    {
        case wifi_mode_AP:
            this->SetCurrentSymble(symble_ap);
            break;
        case wifi_mode_Client:
            this->SetCurrentSymble(symble_clt);
            break;
        case wifi_mode_Off:
            this->SetCurrentSymble(symble_off);
            break;
        default:
            break;
    }
}

bool ui::CViditWifiSybl::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal)
        && (event->_event == InternalEvent_app_property_update)
        && (event->_paload == PropertyChangeType_Wifi))
    {
        updateSymble();
    }
    return false;
}


/*******************************************************************************
*   CViditBtSybl
*
*/
ui::CViditBtSybl::CViditBtSybl(
    CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    , CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_bt_disconnect] = new CBmp(ViditResourceDir, "btoff.bmp");
    symbles[symble_bt_connect]    = new CBmp(ViditResourceDir, "bton.bmp");
    symbles[symble_bt_hid_paired]     = new CBmp(ViditResourceDir, "bthid.bmp");
    symbles[symble_bt_obd_paired]     = new CBmp(ViditResourceDir, "btobd.bmp");
    symbles[symble_bt_hid_obd_paried] = new CBmp(ViditResourceDir, "bto-h.bmp");
    SetSymbles(symble_num, symbles);

    this->SetText((UINT8*)NULL);
    SetTitles(0, NULL);
}

ui::CViditBtSybl::~CViditBtSybl()
{
}

void ui::CViditBtSybl::updateSymble() {
    bluetooth_state state = _cp->getBluetoothState();
    if (state == bt_state_off) {
        this->SetCurrentSymble(symble_bt_disconnect);
    } else if (state == bt_state_on) {
        this->SetCurrentSymble(symble_bt_connect);
    } else if (state == bt_state_hid_paired) {
        this->SetCurrentSymble(symble_bt_hid_paired);
    } else if (state == bt_state_obd_paired) {
        this->SetCurrentSymble(symble_bt_obd_paired);
    } else if (state == bt_state_hid_obd_paired) {
        this->SetCurrentSymble(symble_bt_hid_obd_paried);
    } else {
        this->SetCurrentSymble(symble_bt_connect);
    }
}

bool ui::CViditBtSybl::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal)
        && (event->_event == InternalEvent_app_property_update)
        && (event->_paload == PropertyChangeType_bluetooth))
    {
        updateSymble();
    }
    return false;
}

/*******************************************************************************
*   CViditBatterySybl
*
*/
ui::CViditBatterySybl::CViditBatterySybl(
    CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    , CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_charge_100] = new CBmp(ViditResourceDir, "battery100.bmp");
    symbles[symble_charge_80]  = new CBmp(ViditResourceDir, "batterycharging80.bmp");
    symbles[symble_charge_60]  = new CBmp(ViditResourceDir, "batterycharging60.bmp");
    symbles[symble_charge_40]  = new CBmp(ViditResourceDir, "batterycharging40.bmp");
    symbles[symble_charge_20]  = new CBmp(ViditResourceDir, "batterycharging20.bmp");
    symbles[symble_charge_0]   = new CBmp(ViditResourceDir, "batterycharging.bmp");
    symbles[symble_100] = new CBmp(ViditResourceDir, "battery100.bmp");
    symbles[symble_80]  = new CBmp(ViditResourceDir, "battery80.bmp");
    symbles[symble_60]  = new CBmp(ViditResourceDir, "battery60.bmp");
    symbles[symble_40]  = new CBmp(ViditResourceDir, "battery40.bmp");
    symbles[symble_20]  = new CBmp(ViditResourceDir, "battery20.bmp");
    symbles[symble_0]   = new CBmp(ViditResourceDir, "battery0.bmp");

    SetSymbles(symble_num, symbles);

    this->SetText((UINT8*)NULL);
    SetTitles(0, NULL);
}

ui::CViditBatterySybl::~CViditBatterySybl()
{
}

void ui::CViditBatterySybl::updateSymble() {
    bool bIncharge;
    int batteryV = _cp->GetBatteryVolume(bIncharge);
    if (bIncharge) {
        if (batteryV >= 100) {
            this->SetCurrentSymble(symble_charge_100);
        } else if (batteryV >= 80) {
            this->SetCurrentSymble(symble_charge_80);
        } else if (batteryV >= 60) {
            this->SetCurrentSymble(symble_charge_60);
        } else if (batteryV >= 40) {
            this->SetCurrentSymble(symble_charge_40);
        } else if (batteryV >= 20) {
            this->SetCurrentSymble(symble_charge_20);
        } else {//PROP_POWER_SUPPLY_CAPACITY_CRITICAL
            this->SetCurrentSymble(symble_charge_0);
        }
    } else {
        if (batteryV >= 100) {
            this->SetCurrentSymble(symble_100);
        } else if(batteryV >= 80) {
            this->SetCurrentSymble(symble_80);
        } else if(batteryV >= 60) {
            this->SetCurrentSymble(symble_60);
        } else if(batteryV >= 40) {
            this->SetCurrentSymble(symble_40);
        } else if(batteryV >= 20) {
            this->SetCurrentSymble(symble_20);
        } else {//PROP_POWER_SUPPLY_CAPACITY_CRITICAL
            this->SetCurrentSymble(symble_0);
        }
    }
}

bool ui::CViditBatterySybl::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal)
        && (event->_event == InternalEvent_app_property_update)
        && (event->_paload == PropertyChangeType_power))
    {
        updateSymble();
    }
    return false;
}


ui::CViditStorageSybl::CViditStorageSybl(
    CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    , CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_storage_100] = new CBmp(ViditResourceDir, "tf_100.bmp");
    symbles[symble_storage_80]  = new CBmp(ViditResourceDir, "tf_80.bmp");
    symbles[symble_storage_60]  = new CBmp(ViditResourceDir, "tf_60.bmp");
    symbles[symble_storage_40]  = new CBmp(ViditResourceDir, "tf_40.bmp");
    symbles[symble_storage_20]  = new CBmp(ViditResourceDir, "tf_20.bmp");
    symbles[symble_storage_0]   = new CBmp(ViditResourceDir, "tf_0.bmp");
    SetSymbles(symble_num, symbles);

    this->SetText((UINT8*)NULL);
    SetTitles(0, NULL);
}

ui::CViditStorageSybl::~CViditStorageSybl()
{
}

void ui::CViditStorageSybl::updateSymble() {
    storage_State st = _cp->getStorageState();
    if (storage_State_ready == st) {
        int space = _cp->getStorageFreSpcByPrcn();
        space = 100 - space; // how much used
        if (space >= 100) {
            this->SetCurrentSymble(symble_storage_100);
        } else if (space >= 80) {
            this->SetCurrentSymble(symble_storage_80);
        } else if (space >= 60) {
            this->SetCurrentSymble(symble_storage_60);
        } else if (space >= 40) {
            this->SetCurrentSymble(symble_storage_40);
        } else if (space >= 20) {
            this->SetCurrentSymble(symble_storage_20);
        } else {
            this->SetCurrentSymble(symble_storage_0);
        }
    } else {
        this->SetCurrentSymble(symble_storage_100);
    }
}

bool ui::CViditStorageSybl::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal)
        && (event->_event == InternalEvent_app_property_update)
        && (event->_paload == PropertyChangeType_storage))
    {
        updateSymble();
    }
    return false;
}

/*******************************************************************************
*   CViditGPSSybl
*
*/
ui::CViditGPSSybl::CViditGPSSybl(
    CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    , CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_gps_offline] = new CBmp(ViditResourceDir, "gpsoff.bmp");
    symbles[symble_gps_ready]   = new CBmp(ViditResourceDir, "gps.bmp");
    SetSymbles(symble_num, symbles);

    this->SetText((UINT8*)NULL);
    SetTitles(0, NULL);
}

ui::CViditGPSSybl::~CViditGPSSybl()
{
}

void ui::CViditGPSSybl::updateSymble() {
    gps_state state = _cp->getGPSState();
    switch (state)
    {
        case gps_state_on:
            this->SetCurrentSymble(symble_gps_offline);
            break;
        case gps_state_ready:
            this->SetCurrentSymble(symble_gps_ready);
            break;
        case gps_state_off:
            this->SetCurrentSymble(symble_gps_offline);
            break;
        default:
            break;
    }
}

bool ui::CViditGPSSybl::OnEvent(CEvent *event)
{
// update status every seconds is not necessary
/*
    if ((event->_type == eventType_internal) &&
        (event->_event == InternalEvent_app_timer_update))
    {
        updateSymble()
    }
*/
    return false;
}

ui::CViditCycleRecSybl::CViditCycleRecSybl(
    CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    , CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_cyclerec] = new CBmp(ViditResourceDir, "looprecording.bmp");
    SetSymbles(symble_num, symbles);

    this->SetText((UINT8*)NULL);
    SetTitles(0, NULL);
}

ui::CViditCycleRecSybl::~CViditCycleRecSybl()
{
}

bool ui::CViditCycleRecSybl::OnEvent(CEvent *event)
{
    return false;
}


ui::CBitCameraWnd::CBitCameraWnd(CWndManager *pMgr, CCameraProperties *cp)
    :inherited("CBitCameraWnd", pMgr)
    ,_cp(cp)
    ,_pMgr(pMgr)
    ,_pCurrentLayout(NULL)
    ,_pTargetLayout(NULL)
    ,_hintShowTime(0)
    ,_pLanguageWnd(NULL) // Load when needed
    ,_bKeyDownLongPressed(false)
    ,_screenSaverCnt(screenSaveTime)
    ,_bChangingWifiMode(false)
    ,_currentWifiMode(0)
    ,_targetWifiMode(0)
    ,_timeoutCnt(0)
    ,_stillcap_state(StillCap_State_Idle)
    ,_bChangingWorkingMode(false)
    ,_countdown(0)
{
    _pStatusLayout = new StatusLayout(this, _cp);
    _pWifiInfoLayout = new WifiInfoLayout(this, _cp);
    _pWifiModeLayout = new WifiModeLayout(this, _cp);
    _pHintLayout = new HintLayout(this, _cp);

    setCurrentLayout(_pStatusLayout, true);

    _pStorageWnd = new CBitStorageWnd(pMgr, cp);
    _pScreenSaveWnd = new CBitScreenSaveWnd(pMgr, cp);
    _pDialogWnd = new CBitDialogWnd(pMgr, cp);
    _pPowerOffWnd = new CBitPowerOffWnd(pMgr, _cp);
}

ui::CBitCameraWnd::~CBitCameraWnd()
{
    delete _pStatusLayout;
    delete _pWifiInfoLayout;
    delete _pWifiModeLayout;
    delete _pHintLayout;
}

void ui::CBitCameraWnd::willHide()
{
}

void ui::CBitCameraWnd::willShow()
{
    // Reset screen save count.
    _screenSaverCnt = screenSaveTime;

    // Show status layout
    _pTargetLayout = _pStatusLayout;
    _hintShowTime = 0;
    setCurrentLayout(_pStatusLayout, true);
}

void ui::CBitCameraWnd::setCurrentLayout(WindowLayout *layout, bool update) {
    layout->onFocus();
    _pCurrentLayout = layout;
    this->SetMainObject(layout->getLayoutRoot());

    if (update) {
        this->GetMainObject()->Draw(true);
    }
}

void ui::CBitCameraWnd::showDefaultWnd() {
    char lang[32];
    memset(lang, 0, 32);
    _cp->GetSystemLanguage(lang);
    if ((strcmp(lang, "NONE") == 0)
        || (strcmp(lang, "") == 0)) {
        if (!_pLanguageWnd) {
            _pLanguageWnd = new CLanguageSelectionWnd(_pMgr, _cp);
        }
        _pLanguageWnd->SetPreWnd(this);
        _pLanguageWnd->Show(false);
    } else {
        this->Show(false);
    }
}

bool ui::CBitCameraWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch(event->_type)
    {
        case eventType_StillCapture:
            if (_pCurrentLayout != _pStatusLayout) {
                setCurrentLayout(_pStatusLayout, true);
            }
            _pStatusLayout->OnEvent(event);
            b = false;
            break;
        case eventType_Cam_mode:
            _bChangingWorkingMode = false;
            setCurrentLayout(_pStatusLayout, true);
            b = false;
            break;
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if ((event->_paload == PropertyChangeType_resolution)
                    || (event->_paload == PropertyChangeType_rec_mode)
                    || (event->_paload == PropertyChangeType_ptl_interval)
                    || (event->_paload == PropertyChangeType_power))
                {
                    _pCurrentLayout->OnEvent(event);
                    b = false;
                } else if (event->_paload == PropertyChangeType_language) {
                    char lang[32];
                    memset(lang, 0, 32);
                    _cp->GetSystemLanguage(lang);
                    CAMERALOG("Language is changed to %s", lang);
                    CLanguageLoader::getInstance()->LoadLanguage(lang);
                    _pDialogWnd->setDialogType(CBitDialogWnd::dialog_language_changed, 3);
                    this->PopWnd(_pDialogWnd);
                    _pCurrentLayout->OnEvent(event);
                    b = false;
                } else if (event->_paload == PropertyChangeType_storage) {
                    storage_State st = _cp->getStorageState();
                    if (st == storage_State_unknown) {
                        this->PopWnd(_pStorageWnd);
                    } else {
                        _pCurrentLayout->OnEvent(event);
                    }
                    b = false;
                }
            } else if (event->_event == InternalEvent_app_state_update) {
                if ((event->_paload == camera_State_starting)
                    || (event->_paload == camera_State_stopping)
                    || (event->_paload == camera_State_Storage_changed))
                {
                    _pCurrentLayout->OnEvent(event);
                    b = false;
                } else if (event->_paload == camera_State_Storage_full) {
                    _pHintLayout->setHintType(HintLayout::hint_TF_full);
                    _pTargetLayout = _pStatusLayout;
                    setCurrentLayout(_pHintLayout, true);
                    _hintShowTime = 2;
                    b = false;
                }
            } else if ((event->_event == InternalEvent_app_timer_update)) {
#ifdef ENABLE_STILL_CAPTURE
                if (_countdown > 0) {
                    _countdown--;
                    _pHintLayout->setHintType(HintLayout::hint_count_down);
                    char txt[2];
                    sprintf(txt, "%d", _countdown);
                    _pHintLayout->setText(txt);
                    setCurrentLayout(_pHintLayout, true);
                    if (_countdown <= 0) {
                        // take a picture
                        _cp->startStillCapture(true);
                        // after take a picture:
                        setCurrentLayout(_pStatusLayout, true);
                    }
                }
#endif

                _pCurrentLayout->OnEvent(event);

                if (_hintShowTime > 0) {
                    _hintShowTime--;
                    if ((_hintShowTime <= 0) && (_pTargetLayout != _pCurrentLayout)) {
                        setCurrentLayout(_pTargetLayout, true);
                    }
                }

                if (!_bKeyDownLongPressed && (_pHintLayout != _pCurrentLayout)) {
                    _screenSaverCnt--;
                    if (_screenSaverCnt <= 0) {
                        this->PopWnd(_pScreenSaveWnd);
                        b = false;
                    }
                } else {
                    // Reset screen save count.
                    _screenSaverCnt = screenSaveTime;
                }

                if (_bKeyDownLongPressed && (_pCurrentLayout == _pWifiModeLayout)) {
                    _pWifiModeLayout->highlightNext();
                }

                if (_bChangingWifiMode) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (mode == _targetWifiMode) {
                        _bChangingWifiMode = false;
                        // Show hint that change wifi mode success
                        _pHintLayout->setHintType(HintLayout::hint_changing_wifi_success);
                        setCurrentLayout(_pHintLayout, true);
                        _pTargetLayout = _pWifiInfoLayout;
                        _hintShowTime = 2;
                    }

                    if (_timeoutCnt >= 0) {
                        _timeoutCnt--;
                        if (_timeoutCnt <= 0) {
                            _bChangingWifiMode = false;
                            // Show hint that change wifi mode timeout
                            _pHintLayout->setHintType(HintLayout::hint_changing_wifi_timeout);
                            setCurrentLayout(_pHintLayout, true);
                            _pTargetLayout = _pWifiInfoLayout;
                            _hintShowTime = 2;
                        }
                    }
                }
            }
            break;
        case eventType_key:
            // Reset screen save count.
            _screenSaverCnt = screenSaveTime;

            // Ignore keys during mode switch
            if (_bChangingWorkingMode) {
                b = false;
                break;
            }

            // during count down to take picture, ignore all keys
            if (_countdown > 0) {
                b = false;
                break;
            }

            switch(event->_event)
            {
                case key_hid: {
                    camera_State state = _cp->getRecState();
                    if (_cp->isCarMode() && (camera_State_record == state)) {
                        // mark video if in car mode
                        _cp->MarkVideo();
                        // Update UI
                        _pHintLayout->setHintType(HintLayout::hint_mark_car_mode);
                        _pTargetLayout = _pStatusLayout;
                        setCurrentLayout(_pHintLayout, true);
                        _hintShowTime = 4;
                    } else {
                        if (_pCurrentLayout != _pStatusLayout) {
                            if (camera_State_record == state) {
                                _pHintLayout->setHintType(HintLayout::hint_stop_recording);
                            } else {
                                _pHintLayout->setHintType(HintLayout::hint_start_recording);
                            }
                            _pTargetLayout = _pStatusLayout;
                            setCurrentLayout(_pHintLayout, true);
                            _hintShowTime = 2;
                        }

                        storage_State st = _cp->getStorageState();
                        if ((storage_State_ready == st) || (storage_State_full == st)) {
#ifdef ENABLE_STILL_CAPTURE
                            CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
                            if ((CAM_MODE_PHOTO == working_mode) ||
                                (CAM_MODE_TIMING_SHOT == working_mode) ||
                                (CAM_MODE_PHOTO_LAPSE == working_mode) ||
                                (CAM_MODE_RAW_PICTURE == working_mode))
                            {
                                // HID only support one shot at the moment.
                                _cp->startStillCapture(true);
                            } else
#endif
                            {
                                _cp->OnRecKeyProcess();
                            }
                        } else if (storage_State_noMedia == st) {
                            _pHintLayout->setHintType(HintLayout::hint_no_TF);
                            _pTargetLayout = _pStatusLayout;
                            setCurrentLayout(_pHintLayout, true);
                            _hintShowTime = 2;
                        } else if ((storage_State_error == st) || (storage_State_unknown == st)) {
                            _pHintLayout->setHintType(HintLayout::hint_TF_Error);
                            _pTargetLayout = _pStatusLayout;
                            setCurrentLayout(_pHintLayout, true);
                            _hintShowTime = 2;
                        }
                    }
                    b = false;
                    break;
                }
                case key_right_onKeyUp: {
                    camera_State state = _cp->getRecState();
                    if (_cp->isCarMode() && (camera_State_record == state)) {
                        // mark video if in car mode
                        _cp->MarkVideo();
                        // Update UI
                        _pHintLayout->setHintType(HintLayout::hint_mark_car_mode);
                        _pTargetLayout = _pStatusLayout;
                        setCurrentLayout(_pHintLayout, true);
                        _hintShowTime = 4;
                    } else {
                        if (_pCurrentLayout != _pStatusLayout) {
                            if (camera_State_record == state) {
                                _pHintLayout->setHintType(HintLayout::hint_stop_recording);
                            } else {
                                _pHintLayout->setHintType(HintLayout::hint_start_recording);
                            }
                            _pTargetLayout = _pStatusLayout;
                            setCurrentLayout(_pHintLayout, true);
                            _hintShowTime = 2;
                        }

                        storage_State st = _cp->getStorageState();
                        if ((storage_State_ready == st) || (storage_State_full == st)) {
#ifdef ENABLE_STILL_CAPTURE
                            CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
                            if (CAM_MODE_PHOTO == working_mode) {
                                if (_stillcap_state == StillCap_State_Burst) {
                                    _cp->stopStillCapture();
                                } else if (_stillcap_state == StillCap_State_Waiting) {
                                    _cp->startStillCapture(true);
                                }
                                _stillcap_state = StillCap_State_Idle;
                            } else if (CAM_MODE_TIMING_SHOT == working_mode) {
                                // count down, then take a picture
                                _countdown = 5; // TODO: read count down from property
                                _pHintLayout->setHintType(HintLayout::hint_count_down);
                                char txt[2];
                                sprintf(txt, "%d", _countdown);
                                _pHintLayout->setText(txt);
                                setCurrentLayout(_pHintLayout, true);
                            } else if (CAM_MODE_RAW_PICTURE == working_mode) {
                                _cp->startStillCapture(true);
                                _stillcap_state = StillCap_State_Idle;
                            } else
                            // Photo lapse is handled in OnRecKeyProcess()
#endif
                            {
                                _cp->OnRecKeyProcess();
                            }
                        } else if (storage_State_noMedia == st) {
                            _pHintLayout->setHintType(HintLayout::hint_no_TF);
                            _pTargetLayout = _pStatusLayout;
                            setCurrentLayout(_pHintLayout, true);
                            _hintShowTime = 2;
                        } else if ((storage_State_error == st) || (storage_State_unknown == st)){
                            _pHintLayout->setHintType(HintLayout::hint_TF_Error);
                            _pTargetLayout = _pStatusLayout;
                            setCurrentLayout(_pHintLayout, true);
                            _hintShowTime = 2;
                        }
                    }
                    b = false;
                    break;
                }
                case key_right_onKeyDown: {
                    CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
                    if (CAM_MODE_PHOTO == working_mode) {
                        _stillcap_state = StillCap_State_Waiting;
                    }
                    b = false;
                    break;
                }
                case key_right_onKeyLongPressed: {
#ifdef ENABLE_STILL_CAPTURE
                    CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
                    if ((CAM_MODE_PHOTO == working_mode) &&
                        (_stillcap_state == StillCap_State_Waiting))
                    {
                        _stillcap_state = StillCap_State_Burst;
                        _cp->startStillCapture(false);
                    }
#endif
                    b = false;
                    break;
                }
                case key_up_onKeyUp:
                    _pHintLayout->setHintType(HintLayout::hint_mark);
                    _pTargetLayout = _pStatusLayout;
                    setCurrentLayout(_pHintLayout, true);
                    _hintShowTime = 4;
                    _cp->MarkVideo();
                    b = false;
                    break;
                case key_down_onKeyUp:
                    {
                        if (_bKeyDownLongPressed && (_pCurrentLayout == _pWifiModeLayout)) {
                            _targetWifiMode = _pWifiModeLayout->getTargetMode();
                            if (_targetWifiMode != _currentWifiMode) {
                                _bChangingWifiMode = true;
                                _timeoutCnt = 20;
                                CAMERALOG("set wifi mode %d", _targetWifiMode);
                                _cp->SetWifiMode(_targetWifiMode);

                                _pHintLayout->setHintType(HintLayout::hint_changing_wifi_mode);
                                setCurrentLayout(_pHintLayout, true);
                            } else {
                                // Show hint that wifi mode is not changed
                                _pHintLayout->setHintType(HintLayout::hint_wifi_no_change);
                                _pTargetLayout = _pWifiInfoLayout;
                                setCurrentLayout(_pHintLayout, true);
                                _hintShowTime = 4;
                            }
                        } else {
                            _pTargetLayout = _pWifiInfoLayout;
                            _hintShowTime = 0;
                            setCurrentLayout(_pWifiInfoLayout, true);
                        }
                        _bKeyDownLongPressed = false;
                        b = false;
                    }
                    break;
                case key_down_onKeyDown:
                    {
                        _currentWifiMode = _cp->getWifiMode();
                        _pHintLayout->setHintType(HintLayout::hint_wifi_long_press);
                        setCurrentLayout(_pHintLayout, true);
                        b = false;
                        break;
                    }
                    break;
                case key_down_onKeyLongPressed:
                    {
                        CAMERALOG("key_down_longpress");
                        _bKeyDownLongPressed = true;
                        if (_pCurrentLayout == _pHintLayout) {
                            setCurrentLayout(_pWifiModeLayout, true);
                        }
                        b = false;
                    }
                    break;
                case key_ok_onKeyUp:
                    if (_pCurrentLayout != _pStatusLayout) {
                        // switch to status layout if not
                        setCurrentLayout(_pStatusLayout, true);
                    } else {
#ifdef ENABLE_STILL_CAPTURE
                        // In S2L which supports still capture,
                        // it works to switch modes.
                        camera_State state = _cp->getRecState();
                        if ((camera_State_record == state) ||
                            (camera_State_PhotoLapse == state) ||
                            (camera_State_PhotoLapse_shutdown == state))
                        {
                            _pHintLayout->setHintType(HintLayout::hint_set_still_in_recording);
                            _pTargetLayout = _pStatusLayout;
                            setCurrentLayout(_pHintLayout, true);
                            _hintShowTime = 2;
                        } else {
                            CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
                            _cp->setWorkingMode((CAM_WORKING_MODE)((working_mode + 1) % CAM_MODE_NUM));
                            _pHintLayout->setHintType(HintLayout::hint_Change_Mode);
                            setCurrentLayout(_pHintLayout, true);
                            _bChangingWorkingMode = true;
                        }
#else
                        _pHintLayout->setHintType(HintLayout::hint_poweroff);
                        _pTargetLayout = _pStatusLayout;
                        setCurrentLayout(_pHintLayout, true);
                        _hintShowTime = 2;
#endif
                    }
                    b = false;
                    break;
                case key_poweroff_prepare:
                    {
                        this->PopWnd(_pPowerOffWnd);
                        b = false;
                    }
                    break;
                default:
                    break;
            }
        break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}


ui::WindowLayout::WindowLayout(CWnd *owner, CCameraProperties* cp)
    : _cp(cp)
{
    _pRoot = new CPanel(NULL, owner, CPoint(0, 0), CSize(128, 64), 15);
    _pRoot->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);
}

ui::WindowLayout::~WindowLayout() {
    delete _pRoot;
}

ui::StatusLayout::StatusLayout(CWnd *owner, CCameraProperties* cp)
    : WindowLayout(owner, cp)
    , _stillcap_mode(StillCap_State_Idle)
    , _cnt_result_showtime(0)
    , _ptl_interval(0.5)
    , _heartBeat(0)
    , _languageType(0)
{
    _pText_Res = new CStaticText(_pRoot, CPoint(0, 0), CSize(75, 16));
    _pText_Res->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny);
    _pText_Res->SetHAlign(LEFT);
    _pText_Res->SetVAlign(MIDDLE);
    memset(_pResolution, 0, maxMenuStrinLen);
    agcmd_property_get(PropName_rec_resoluton, _pResolution, "1080p30");
    _pText_Res->SetText((UINT8 *)_pResolution);

    _pLoopRecSymble = new CViditCycleRecSybl
        (_pRoot
        ,CPoint(75, 0)
        ,CSize(15, 18)
        ,false
        ,cp);
    _pLoopRecSymble->SetHiden();

    _pGpsSymble = new CViditGPSSybl
            (_pRoot
            ,CPoint(92, 2)
            ,CSize(16, 16)
            ,false
            ,cp);

    _pWifiSymble = new CViditWifiSybl
        (_pRoot
        ,CPoint(110, 0)
        ,CSize(18, 18)
        ,false
        ,cp);

    _pBtSymble = new CViditBtSybl
        (_pRoot
        ,CPoint(113, 22)
        ,CSize(12, 20)
        ,false
        ,cp);

    _pAudioSymble = new CViditAudioSybl
        (_pRoot
        ,CPoint(113, 44)
        ,CSize(14, 20)
        ,false
        ,cp
        );

    _pModeSymble = new CStaticText(_pRoot, CPoint(0, 17), CSize(113, 20));
    _pModeSymble->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_normal
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_normal);
    _pModeSymble->SetHAlign(CENTER);
    _pModeSymble->SetVAlign(MIDDLE);
    memset(_pTimeInfo, 0, maxMenuStrinLen);
    CLanguageItem *pItem = LoadTxtItem("VIDEO");
    sprintf(_pTimeInfo, "%s", pItem->getContent());
    _pModeSymble->SetText((UINT8 *)_pTimeInfo);

    _pVRow = new CBlock(_pRoot, CPoint(9, 39), CSize(93, 1), BSL_Bit_White_Pannel);
    _pVRow->SetDisabled();

    _pBatterySymble = new CViditBatterySybl
        (_pRoot
        ,CPoint(4, 45)
        ,CSize(12, 19)
        ,true
        ,cp);

    _pText_Bat = new CStaticText(_pRoot, CPoint(16, 40), CSize(45, 24));
    _pText_Bat->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny);
    _pText_Bat->SetHAlign(LEFT);
    _pText_Bat->SetVAlign(MIDDLE);
    memset(_pBatteryInfo, 0, maxMenuStrinLen);
    sprintf(_pBatteryInfo, "%s", "100%");
    _pText_Bat->SetText((UINT8 *)_pBatteryInfo);

    _pStorageSymble = new CViditStorageSybl
        (_pRoot
        ,CPoint(57, 45)
        ,CSize(15, 18)
        ,true
        ,cp);

    _pText_Storage = new CStaticText(_pRoot, CPoint(71, 40), CSize(43, 24));
    _pText_Storage->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny);
    _pText_Storage->SetHAlign(LEFT);
    _pText_Storage->SetVAlign(MIDDLE);
    memset(_pStorageInfo, 0, maxMenuStrinLen);
    sprintf(_pStorageInfo, "%s", "0.0G");
    _pText_Storage->SetText((UINT8 *)_pStorageInfo);
}

ui::StatusLayout::~StatusLayout() {
    delete _pText_Storage;
    delete _pStorageSymble;
    delete _pText_Bat;
    delete _pBatterySymble;
    delete _pVRow;
    delete _pModeSymble;
    delete _pAudioSymble;
    delete _pBtSymble;
    delete _pWifiSymble;
    delete _pGpsSymble;
    delete _pLoopRecSymble;
    delete _pText_Res;
}

void ui::StatusLayout::updateRecUI() {
    CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
    memset(_pTimeInfo, 0, maxMenuStrinLen);
    CLanguageItem *pItem = NULL;
    switch (working_mode) {
        case CAM_MODE_VIDEO: {
            // update mode / recording time
            camera_State state = _cp->getRecState();
            if (camera_State_record == state) {
                UINT64 tt = _cp->getRecordingTime();
                UINT64 hh =  tt /3600;
                UINT64 mm = (tt % 3600) /60;
                UINT64 ss = (tt % 3600) %60;
                int j = 0;
                j = sprintf(_pTimeInfo, "%02lld:", hh);
                j += sprintf( _pTimeInfo + j, "%02lld:", mm);
                j += sprintf( _pTimeInfo + j, "%02lld", ss);
            } else if (camera_State_starting == state) {
                pItem = LoadTxtItem("StartRec");
                sprintf(_pTimeInfo, "%s...", pItem->getContent());
            } else if (camera_State_stopping == state) {
                pItem = LoadTxtItem("StopRec");
                sprintf(_pTimeInfo, "%s...", pItem->getContent());
            } else if (camera_State_Error == state) {
                pItem = LoadTxtItem("Error");
                sprintf(_pTimeInfo, "%s", pItem->getContent());
            } else {
                pItem = LoadTxtItem("VIDEO");
                sprintf(_pTimeInfo, "%s", pItem->getContent());
            }
            break;
        }
        case CAM_MODE_SLOW_MOTION:
        {
            pItem = LoadTxtItem("SlowMotion");
            sprintf(_pResolution, "%s", pItem->getContent());

            // update mode / recording time
            camera_State state = _cp->getRecState();
            if (camera_State_record == state) {
                UINT64 tt = _cp->getRecordingTime();
                UINT64 hh =  tt /3600;
                UINT64 mm = (tt % 3600) /60;
                UINT64 ss = (tt % 3600) %60;
                int j = 0;
                j = sprintf(_pTimeInfo, "%02lld:", hh);
                j += sprintf( _pTimeInfo + j, "%02lld:", mm);
                j += sprintf( _pTimeInfo + j, "%02lld", ss);
            } else {
                pItem = LoadTxtItem("VIDEO");
                sprintf(_pTimeInfo, "%s", pItem->getContent());
            }
            break;
        }
        case CAM_MODE_VIDEO_LAPSE:
        {
            pItem = LoadTxtItem("VTL 0.3s");
            sprintf(_pResolution, "%s", pItem->getContent());

            // update mode / recording time
            camera_State state = _cp->getRecState();
            if (camera_State_record == state) {
                UINT64 tt = _cp->getRecordingTime();
                UINT64 hh =  tt /3600;
                UINT64 mm = (tt % 3600) /60;
                UINT64 ss = (tt % 3600) %60;
                int j = 0;
                j = sprintf(_pTimeInfo, "%02lld:", hh);
                j += sprintf( _pTimeInfo + j, "%02lld:", mm);
                j += sprintf( _pTimeInfo + j, "%02lld", ss);
            } else {
                pItem = LoadTxtItem("VIDEO");
                sprintf(_pTimeInfo, "%s", pItem->getContent());
            }
            break;
        }
#ifdef ENABLE_STILL_CAPTURE
        case CAM_MODE_PHOTO: {
            // update resolution info
            pItem = LoadTxtItem("PHOTO");
            sprintf(_pResolution, "%s", pItem->getContent());

            storage_State st = _cp->getStorageState();
            if (storage_State_ready == st) {
                // update mode / recording time
                int num_pics = _cp->getTotalPicNum();
                pItem = LoadTxtItem("Taken");
                if (_languageType == 1) { // Chinese
                    sprintf(_pTimeInfo, "%s%02d", pItem->getContent(), num_pics);
                } else {// English
                    sprintf(_pTimeInfo, "%02d%s", num_pics, pItem->getContent());
                }
            } else {
                sprintf(_pTimeInfo, "%s", pItem->getContent());
            }
            break;
        }
        case CAM_MODE_TIMING_SHOT: {
            // update resolution info
            pItem = LoadTxtItem("Countdown");
            sprintf(_pResolution, "%s", pItem->getContent());

            // update mode / recording time
            pItem = LoadTxtItem("5s");
            sprintf(_pTimeInfo, "%s", pItem->getContent());
            break;
        }
        case CAM_MODE_PHOTO_LAPSE: {
            // update time interval
            pItem = LoadTxtItem("PTL");
            if (_ptl_interval < 1.0) {
                sprintf(_pResolution, "%s %.1fs", pItem->getContent(), _ptl_interval);
            } else if (_ptl_interval <= 60.0) {
                sprintf(_pResolution, "%s %ds", pItem->getContent(), (int)_ptl_interval);
            } else if (_ptl_interval < 3600.0) {
                sprintf(_pResolution, "%s %dm", pItem->getContent(), (int)_ptl_interval / 60);
            } else {
                sprintf(_pResolution, "%s %dh", pItem->getContent(), (int)_ptl_interval / 3600);
            }

            camera_State state = _cp->getRecState();
            if ((_ptl_interval >= 5.0) && (camera_State_PhotoLapse == state)) {
                UINT64 tt = _cp->getRecordingTime();
                UINT64 hh =  tt /3600;
                UINT64 mm = (tt % 3600) /60;
                UINT64 ss = (tt % 3600) %60;
                int j = 0;
                if (tt >= 3600) {
                    j += sprintf(_pTimeInfo, "%02lld:", hh);
                    j += sprintf( _pTimeInfo + j, "%02lld:", mm);
                    j += sprintf( _pTimeInfo + j, "%02lld", ss);
                } else if (tt >= 60) {
                    j += sprintf( _pTimeInfo + j, "%02lld:", mm);
                    j += sprintf( _pTimeInfo + j, "%02lld", ss);
                } else {
                    j += sprintf( _pTimeInfo + j, ":%02lld", ss);
                }
            } else if (camera_State_PhotoLapse_shutdown == state) {
                pItem = LoadTxtItem("Shutdown..");
                sprintf(_pTimeInfo, "%s", pItem->getContent());
            } else {
                storage_State st = _cp->getStorageState();
                if (storage_State_ready == st) {
                    // update mode / recording time
                    int num_pics = _cp->getTotalPicNum();
                    pItem = LoadTxtItem("Taken");
                    if (_languageType == 1) { // Chinese
                        sprintf(_pTimeInfo, "%s%02d", pItem->getContent(), num_pics);
                    } else {// English
                        sprintf(_pTimeInfo, "%02d%s", num_pics, pItem->getContent());
                    }
                } else {
                    pItem = LoadTxtItem("PHOTO");
                    sprintf(_pTimeInfo, "%s", pItem->getContent());
                }
            }
            break;
        }
        case CAM_MODE_RAW_PICTURE: {
            // update resolution info
            pItem = LoadTxtItem("RAW");
            sprintf(_pResolution, "%s", pItem->getContent());

            storage_State st = _cp->getStorageState();
            if (storage_State_ready == st) {
                // update mode / recording time
                int num_pics = _cp->getTotalPicNum();
                pItem = LoadTxtItem("Taken");
                if (_languageType == 1) { // Chinese
                    sprintf(_pTimeInfo, "%s%02d", pItem->getContent(), num_pics);
                } else {// English
                    sprintf(_pTimeInfo, "%02d%s", num_pics, pItem->getContent());
                }
            } else {
                sprintf(_pTimeInfo, "%s", pItem->getContent());
            }
            break;
        }
#endif
        default:
            break;
    }
}

void ui::StatusLayout::updateBatteryUI() {
    // update symble
    _pBatterySymble->updateSymble();

    // update battery
    bool bInCharge = false;
    int batteryV = _cp->GetBatteryVolume(bInCharge);
    memset(_pBatteryInfo, 0, maxMenuStrinLen);
    if (!bInCharge) {
        // show remaining time, font size is 14x14
        _pText_Bat->SetStyle(BSL_Bit_Black_Pannel
            ,FSL_Bit_White_Text_tiny
            ,BSL_Bit_Black_Pannel
            ,FSL_Bit_White_Text_tiny);

        // update remaining time
        bool bInDischarge = false;
        int minutes = _cp->GetBatteryRemainingTime(bInDischarge);
        if ((minutes == 0) && (batteryV > 0)) {
            // VT-1, can't calculate current remaining time
            sprintf(_pBatteryInfo, "%02d%%", batteryV);
        } else {
            // VT-2, display remaining time
            if (minutes > 60) {
                sprintf(_pBatteryInfo, "%dh%02d", minutes / 60, minutes % 60);
            } else if (minutes >= 1) {
                sprintf(_pBatteryInfo, "%02dm", minutes % 60);
            } else if (minutes == 0) {
                sprintf(_pBatteryInfo, "%s", "<1m");
            }
        }
    } else {
        if (_languageType == 1) { // Chinese
            _pText_Bat->SetStyle(BSL_Bit_Black_Pannel
                ,FSL_Bit_White_Text_tiny
                ,BSL_Bit_Black_Pannel
                ,FSL_Bit_White_Text_tiny);
        } else { // English
            // show "charge" which has a lot of characters, font size is 12x12
            _pText_Bat->SetStyle(BSL_Bit_Black_Pannel
                ,FSL_Bit_White_Text_tiny_12
                ,BSL_Bit_Black_Pannel
                ,FSL_Bit_White_Text_tiny_12);
        }

        // update charge status
        if (batteryV < 100) {
            CLanguageItem *pItem = LoadTxtItem("Charge");
            sprintf(_pBatteryInfo, "%s", pItem->getContent());
        } else {
            //CLanguageItem *pItem = LoadTxtItem("Charge Finish");
            sprintf(_pBatteryInfo, "%s", "100%");
        }
    }
}

void ui::StatusLayout::updateStorageUI() {
    // update symble
    _pStorageSymble->updateSymble();

    // update storage
    memset(_pStorageInfo, 0, maxMenuStrinLen);
    storage_State st = _cp->getStorageState();
    CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
    if (storage_State_ready == st) {
        int free, total;
        int spaceMB = _cp->getFreeSpaceMB(free, total);
        switch (working_mode) {
            case CAM_MODE_VIDEO:
            case CAM_MODE_SLOW_MOTION:
            case CAM_MODE_VIDEO_LAPSE:
            {
                int bitrateKBS = _cp->getBitrate() / 8 / 1000;
                if (bitrateKBS > 0) {
                    int time_sec = spaceMB * 1000 / bitrateKBS;
                    if (time_sec > 3600) {
                        sprintf(_pStorageInfo, "%dh%02d",
                            time_sec / 3600, (time_sec % 3600) / 60);
                    } else {
                        int minutes = (time_sec % 3600) / 60;
                        if (minutes >= 1) {
                            sprintf(_pStorageInfo, "%02dm", (time_sec % 3600) / 60);
                        } else {
                            sprintf(_pStorageInfo, "%s", "<1m");
                        }
                    }
                } else {
                    if (spaceMB > 1000) {
                        sprintf(_pStorageInfo, "%d.%dG",
                            spaceMB / 1000, (spaceMB % 1000) / 100);
                    } else {
                        if (spaceMB >= 1) {
                            sprintf(_pStorageInfo, "%dM", spaceMB);
                        } else {
                            sprintf(_pStorageInfo, "%s", "<1M");
                        }
                    }
                }
                break;
            }
            case CAM_MODE_PHOTO:
            case CAM_MODE_PHOTO_LAPSE:
            case CAM_MODE_TIMING_SHOT:
            {
                int num_remain = spaceMB / 2.5;
                if (num_remain > 9999) {
                    sprintf(_pStorageInfo, "%s", "9999+");
                } else {
                    sprintf(_pStorageInfo, "%d", num_remain);
                }
                break;
            }
            default: {
                if (spaceMB > 1000) {
                    sprintf(_pStorageInfo, "%d.%dG",
                        spaceMB / 1000, (spaceMB % 1000) / 100);
                } else {
                    if (spaceMB >= 1) {
                        sprintf(_pStorageInfo, "%dM", spaceMB);
                    } else {
                        sprintf(_pStorageInfo, "%s", "<1M");
                    }
                }
                break;
            }
        }
    } else {
        if (storage_State_prepare == st) {
            CLanguageItem *pItem = LoadTxtItem("Check");
            sprintf(_pStorageInfo, "%s", pItem->getContent());
        } else if (storage_State_noMedia == st) {
            CLanguageItem *pItem = LoadTxtItem("No card");
            sprintf(_pStorageInfo, "%s", pItem->getContent());
        } else if (storage_State_full == st) {
            CLanguageItem *pItem = LoadTxtItem("SD Full");
            sprintf(_pStorageInfo, "%s", pItem->getContent());
        } else if (storage_State_error == st) {
            CLanguageItem *pItem = LoadTxtItem("SD Error");
            sprintf(_pStorageInfo, "%s", pItem->getContent());
        } else if (storage_State_unknown == st) {
            CLanguageItem *pItem = LoadTxtItem("Unknown SD");
            sprintf(_pStorageInfo, "%s", pItem->getContent());
        }
    }
}

void ui::StatusLayout::getLanguageType() {
    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_Language, tmp, "English");
    if (strcmp(tmp, "English") == 0) {
        _languageType = 0;
    } else if (strcmp(tmp, "ä¸­æ–‡") == 0) {
        _languageType = 1;
    } else if (strcmp(tmp, "§²§å§ã§ã§Ü§Ú§Û") == 0) {
        _languageType = 2;
    }
}

void ui::StatusLayout::getPTLInterval() {
    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_PhotoLapse, tmp, "0.5");
    _ptl_interval = atof(tmp);
}

void ui::StatusLayout::onFocus() {
    if (_cp->getWorkingMode() == CAM_MODE_VIDEO) {
        // update resolution info
        agcmd_property_get(PropName_rec_resoluton, _pResolution, "1080p30");
    }
    // update cycle rec symble
    if (_cp->isCircleMode()) {
        _pLoopRecSymble->SetShow();
    } else {
        _pLoopRecSymble->SetHiden();
    }
    _pWifiSymble->updateSymble();
    _pBtSymble->updateSymble();
    _pAudioSymble->updateSymble();
    _pGpsSymble->updateSymble();
    getLanguageType();
    getPTLInterval();

    _cnt_result_showtime = 0;
    _heartBeat = 0;
    updateRecUI();
    updateBatteryUI();
    updateStorageUI();
}

bool ui::StatusLayout::OnEvent(CEvent *event) {
    switch (event->_type)
    {
#ifdef ENABLE_STILL_CAPTURE
        case eventType_StillCapture:
            if (event->_event == StillCapture_Start) {
                if (event->_paload == 0) {
                    _cnt_result_showtime = 0;
                    _stillcap_mode = StillCap_State_BURST;
                    memset(_pTimeInfo, 0, maxMenuStrinLen);
                    CLanguageItem *pItem = LoadTxtItem("Bursting");
                    sprintf(_pTimeInfo, "%s", pItem->getContent());
                    _pRoot->Draw(false);
                } else if (event->_paload == 1) {
                    // single shot
                    CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
                    if (CAM_MODE_PHOTO_LAPSE == working_mode) {
                        _cnt_result_showtime = 2;
                        memset(_pTimeInfo, 0, maxMenuStrinLen);
                        // update mode / recording time
                        int num_pics = _cp->getTotalPicNum();
                        CLanguageItem *pItem = LoadTxtItem("Taken");
                        if (_languageType == 1) { // Chinese
                            sprintf(_pTimeInfo, "%s%02d", pItem->getContent(), num_pics);
                        } else {// English
                            sprintf(_pTimeInfo, "%02d%s", num_pics, pItem->getContent());
                        }
                        _pRoot->Draw(false);
                    }
                }
            } else if (event->_event == StillCapture_Info) {
                if ((event->_paload == 0) && (_stillcap_mode == StillCap_State_BURST)) {
                    _stillcap_mode = StillCap_State_Idle;
                    _cnt_result_showtime = 2;
                    memset(_pTimeInfo, 0, maxMenuStrinLen);
                    CLanguageItem *pItem = LoadTxtItem("Burst");
                    sprintf(_pTimeInfo, "%s:%d", pItem->getContent(), event->_paload1);
                    _pRoot->Draw(false);
                }
            }
            break;
#endif
        case eventType_internal:
            if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_language) {
                    getLanguageType();
                } else if (event->_paload == PropertyChangeType_power) {
                    updateBatteryUI();
                } else if (event->_paload == PropertyChangeType_storage) {
                    updateStorageUI();
                    _pRoot->Draw(false);
                } else if (event->_paload == PropertyChangeType_resolution) {
                    if (_cp->getWorkingMode() == CAM_MODE_VIDEO) {
                        // update resolution info
                        agcmd_property_get(PropName_rec_resoluton, _pResolution, "1080p30");
                    }
                } else if (event->_paload == PropertyChangeType_rec_mode) {
                    // update cycle rec symble
                    if (_cp->isCircleMode()) {
                        _pLoopRecSymble->SetShow();
                    } else {
                        _pLoopRecSymble->SetHiden();
                    }
                } else if (event->_paload == PropertyChangeType_ptl_interval) {
                    getPTLInterval();
                }
            } else if ((event->_event == InternalEvent_app_state_update)) {
                if ((event->_paload == camera_State_starting)
                    || (event->_paload == camera_State_stopping))
                {
                    updateRecUI();
                    updateBatteryUI();
                    _pRoot->Draw(false);
                } else if (event->_paload == camera_State_Storage_changed) {
                    updateStorageUI();
                    _pRoot->Draw(false);
                }
            } else if (event->_event == InternalEvent_app_timer_update) {
                _heartBeat++;

#ifdef ENABLE_STILL_CAPTURE
                if (_stillcap_mode != StillCap_State_Idle) {
                    // during still capture bursting
                    break;
                }
                if (_cnt_result_showtime > 0) {
                    // during display burst result
                    _cnt_result_showtime--;
                    break;
                }
#endif

                updateRecUI();

                if (_heartBeat % 10 == 0) {
                    updateBatteryUI();
                    updateStorageUI();
                    _pGpsSymble->updateSymble();
                }

                _pRoot->Draw(false);
            }
            break;
    }

    return true;
}

ui::WifiInfoLayout::WifiInfoLayout(CWnd *owner, CCameraProperties* cp)
    : WindowLayout(owner, cp)
{
    _pWifiSymble = new CViditWifiSybl
        (_pRoot
        ,CPoint(4, 2)
        ,CSize(18, 20)
        ,true
        ,cp);

    _pTitle = new CStaticText(_pRoot, CPoint(23, 2), CSize(106, 20));
    _pTitle->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small);
    _pTitle->SetVAlign(MIDDLE);
    _pTitle->SetHAlign(CENTER);

    _pRow = new CBlock(_pRoot, CPoint(2, 26), CSize(124, 1), BSL_Bit_White_Pannel);

    _textInfor = new CStaticText(_pRoot, CPoint(0, 32), CSize(128, 20));
    _textInfor->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small);
    _textInfor->SetVAlign(MIDDLE);
    _textInfor->SetHAlign(CENTER);
}

ui::WifiInfoLayout::~WifiInfoLayout() {
    delete _pWifiSymble;
    delete _pTitle;
    delete _pRow;
    delete _textInfor;
}

void ui::WifiInfoLayout::updateInfo() {
    wifi_mode mode = _cp->getWifiMode();
    switch(mode)
    {
        case wifi_mode_AP:
            _cp->GetWifiAPinfor(_text_1, _text_2);
            _pTitle->SetText((UINT8*)_text_1);
            _textInfor->SetText((UINT8*)_text_2);
            break;
        case wifi_mode_Client: {
            wifi_State state = _cp->getWifiState();
            switch(state)
            {
                case wifi_State_ready:
                    sprintf(_text_1, "Connected");
                    break;
                case wifi_State_prepare:
                    sprintf(_text_1, "Prepare");
                    break;
                case wifi_State_error:
                    sprintf(_text_1, "No Connect");
                    break;
                default:
                    break;
            }

            _pTitle->SetText((UINT8*)(LoadTxtItem(_text_1))->getContent());

            char ssid[128];
            memset(ssid, 0x00, 128);
            memset(_text_2, 0x00, 128);
            _cp->GetWifiCltinfor(ssid, 128);
            if (strcmp(ssid, "Empty List") == 0) {
                CLanguageItem *pItem = LoadTxtItem("No network added!");
                sprintf(_text_2, "%s", pItem->getContent());
            } else if (strcmp(ssid, "Searching") == 0) {
                CLanguageItem *pItem = LoadTxtItem("Searching");
                sprintf(_text_2, "%s", pItem->getContent());
            } else {
                int len = strlen(ssid);
                // For FSL_Bit_White_Text_small which is 18x18,
                // it can display 10 characters at the most.
                if (len > 10) {
                    strncpy(_text_2, ssid, 9);
                    _text_2[9] = '.';
                    _text_2[10] = '.';
                } else {
                    strcpy(_text_2, ssid);
                }
            }
            _textInfor->SetText((UINT8*)_text_2);
        }
            break;
        case wifi_mode_Off:
            _pTitle->SetText((UINT8*)(LoadTxtItem("Off"))->getContent());
            _textInfor->SetText((UINT8*)(LoadTxtItem("Off"))->getContent());
            break;
        default:
            break;
    }
}

void ui::WifiInfoLayout::onFocus() {
    _pWifiSymble->updateSymble();
    updateInfo();
}

bool ui::WifiInfoLayout::OnEvent(CEvent *event) {
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                updateInfo();
                _pRoot->Draw(true);
            }
            break;
        default:
            break;
    }
    return true;
}

ui::WifiModeLayout::WifiModeLayout(CWnd *owner, CCameraProperties* cp)
    : WindowLayout(owner, cp)
{
    _pSymble = new CViditWifiSybl(_pRoot, CPoint(2, 0), CSize(126, 20), true, cp);
    _pSymble->SetStyle
        (BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small);
    _pSymble->SetHAlign(CENTER);
    _pSymble->SetVAlign(TOP);
    _pSymble->AdjustVertical(0);
    _pSymble->SetDisabled();

    CLanguageItem *pItem = LoadTxtItem("Setup");
    _pSymble->SetLanguageItem(pItem);

    _pRow = new CBlock(_pRoot, CPoint(2, 24), CSize(124, 1), BSL_Bit_White_Pannel);
    _pRow->SetDisabled();

    _pSelector = new CHSelector(_pRoot, NULL, CPoint(0, 26), CSize(128, 38), 2, 5, 42);
    _pSelector->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

    _pAP = new CStaticText(_pSelector, CPoint(0, 0), CSize(40, 24));
    _pAP->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,BSL_Bit_White_Pannel
        ,FSL_Bit_Black_Text_small
        ,0
        ,0
        ,0
        ,0);
    _pAP->SetHAlign(CENTER);
    _pAP->SetVAlign(MIDDLE);
    _pAP->SetText((UINT8 *)"AP");

    _pCLT= new CStaticText(_pSelector, CPoint(0, 0), CSize(40, 24));
    _pCLT->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,BSL_Bit_White_Pannel
        ,FSL_Bit_Black_Text_small
        ,0
        ,0
        ,0
        ,0);
    _pCLT->SetText((UINT8 *)"CLT");
    _pCLT->SetHAlign(CENTER);
    _pCLT->SetVAlign(MIDDLE);

    _pOFF= new CStaticText(_pSelector, CPoint(0, 0), CSize(40, 24));
    _pOFF->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,BSL_Bit_White_Pannel
        ,FSL_Bit_Black_Text_small
        ,0
        ,0
        ,0
        ,0);
    _pOFF->SetText((UINT8 *)"Off");
    _pOFF->SetHAlign(CENTER);
    _pOFF->SetVAlign(MIDDLE);
}

ui::WifiModeLayout::~WifiModeLayout() {
    delete _pSymble;
    delete _pRow;
    delete _pAP;
    delete _pCLT;
    delete _pOFF;
    delete _pSelector;
}

void ui::WifiModeLayout::onFocus() {
    _pSymble->updateSymble();

    _pRoot->SetCurrentHilight(2);
    wifi_mode mode = _cp->getWifiMode();
    CAMERALOG("wifi mode = %d", mode);
    if ((mode >= wifi_mode_AP) && (mode <= wifi_mode_Off)) {
        _pSelector->SetCurrentHilight(mode);
    }
}

void ui::WifiModeLayout::highlightNext() {
    if (_pSelector->GetChildrenCount() > 0) {
        _pSelector->SetCurrentHilight(
            (_pSelector->GetCurrentHilight() + 1) % _pSelector->GetChildrenCount());
    }
    _pSelector->Draw(true);
}

int ui::WifiModeLayout::getTargetMode() {
    return _pSelector->GetCurrentHilight();
}

bool ui::WifiModeLayout::OnEvent(CEvent *event) {
    return true;
}

ui::HintLayout::HintLayout(CWnd *owner, CCameraProperties* cp)
    : WindowLayout(owner, cp)
{
    _pNotice= new CStaticText(_pRoot, CPoint(0, 0), CSize(128, 24));
    _pNotice->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,0
        ,0
        ,0
        ,0);
    _pNotice->SetHAlign(CENTER);
    _pNotice->SetVAlign(MIDDLE);
    memset(_pTitleInfo, 0x00, 64);
    _pNotice->SetText((UINT8 *)_pTitleInfo);

    _pRow = new CBlock(_pRoot, CPoint(2, 24), CSize(124, 1), BSL_Bit_White_Pannel);

    _textInfor_1 = new CStaticText(_pRoot, CPoint(0, 26), CSize(128, 38));
    _textInfor_1->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny);
    _textInfor_1->SetHAlign(CENTER);
    _textInfor_1->SetVAlign(TOP);

    memset(_pDialogInfo, 0x00, 64);
    _textInfor_1->SetText((UINT8 *)_pDialogInfo);
}

ui::HintLayout::~HintLayout() {
    delete _textInfor_1;
    delete _pRow;
    delete _pNotice;
}

void ui::HintLayout::onFocus() {
}

bool ui::HintLayout::OnEvent(CEvent *event) {
    return false;
}

void ui::HintLayout::setHintType(int type) {
    _textInfor_1->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny);

    memset(_pTitleInfo, 0x00, 64);
    memset(_pDialogInfo, 0x00, 64);
    if (hint_wifi_long_press == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Tips");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Long Press to change wifi mode");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_changing_wifi_mode == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Apply new wifi mode");
        sprintf(_pDialogInfo, "%s...", pItem->getContent());
    } else if (hint_changing_wifi_success == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Change wifi mode SUCCESS");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_changing_wifi_timeout == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Change wifi mode TIMEOUT");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_wifi_no_change == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Tips");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Select a different mode please!");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_mark == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        camera_State state = _cp->getRecState();
        if (camera_State_record == state) {
            pItem = LoadTxtItem("Mark video");
            sprintf(_pDialogInfo, "%s %ds!", pItem->getContent(), _cp->getMarkTargetDuration());
        } else {
            pItem = LoadTxtItem("In recording press it to mark video!");
            sprintf(_pDialogInfo, "%s", pItem->getContent());
        }
    } else if (hint_mark_car_mode == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("In car mode, mark video");
        sprintf(_pDialogInfo, "%s %ds!", pItem->getContent(), _cp->getMarkTargetDuration());
    } else if (hint_poweroff == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Tips");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Long Press to Power Off");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_start_recording == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Start recording");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_stop_recording == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Stop recording");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_set_still_in_recording == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Stop rec, then set PHOTO mode");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_no_TF == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("No SD card!");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_TF_full == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("SD card is full!");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_TF_Error == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("SD card in Error!");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (hint_Change_Mode == type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Information");
        sprintf(_pTitleInfo, "%s", pItem->getContent());
        // Information:
        pItem = LoadTxtItem("Changing mode");
        sprintf(_pDialogInfo, "%s...", pItem->getContent());
    } else if (hint_count_down== type) {
        // Title:
        CLanguageItem *pItem = LoadTxtItem("Countdown");
        sprintf(_pTitleInfo, "%s", pItem->getContent());

        // "Countdown" with big font size
        _textInfor_1->SetStyle(BSL_Bit_Black_Pannel
            ,FSL_Bit_White_Text
            ,BSL_Bit_Black_Pannel
            ,FSL_Bit_White_Text);
    }
}

void ui::HintLayout::setText(char *txt) {
    memset(_pDialogInfo, 0x00, 64);
    strcpy(_pDialogInfo, txt);
}

/*******************************************************************************
*   CLanguageSelectionWnd
*
*/
ui::CLanguageSelectionWnd::CLanguageSelectionWnd(CWndManager *pMgr, CCameraProperties* cp)
    :inherited("LangWnd", pMgr)
    ,_cp(cp)
    ,_currentStep(step_language_options)
{
    _pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
    this->SetMainObject(_pPannel);
    _pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

    _pBmp_1 = new CBmpControl(
        _pPannel, CPoint(114, 7), CSize(8, 8), ViditResourceDir, "dot_full.bmp");
    _pBmp_2 = new CBmpControl(
        _pPannel, CPoint(114, 21), CSize(8, 8), ViditResourceDir, "dot_vacuum.bmp");
    _pBmp_3 = new CBmpControl(
        _pPannel, CPoint(114, 35), CSize(8, 8), ViditResourceDir, "dot_vacuum.bmp");
    _pBmp_4 = new CBmpControl(
        _pPannel, CPoint(114, 49), CSize(8, 8), ViditResourceDir, "dot_full.bmp");

    _pBmp_logo = new CBmpControl(
        _pPannel, CPoint(4, 3), CSize(16, 15), ViditResourceDir, "logo_chinese.bmp");
    _pBmp_logo->SetHiden();

    _ppList = CLanguageLoader::getInstance()->GetLanguageList(_numLang);
    if (_numLang > 2) {
        _numLang = 2;
    }
    CLanguageItem *pItem = NULL;
    for (int i = 0; i < _numLang; i++) {
        CLanguageLoader::getInstance()->LoadLanguage(_ppList[i]);
        memset(_pHintConfirm[i], 0x00, maxMenuStrinLen);
        pItem = LoadTxtItem("Confirm");
        sprintf(_pHintConfirm[i], "%s", pItem->getContent());

        memset(_pHintCancel[i], 0x00, maxMenuStrinLen);
        pItem = LoadTxtItem("Cancel");
        sprintf(_pHintCancel[i], "%s", pItem->getContent());
    }
    _pLanguages[0] = new CStaticText(_pPannel, CPoint(40, 0), CSize(70, 22));
    _pLanguages[0]->SetStyle(BSL_Bit_Black_Pannel
            ,FSL_Bit_White_Text_small
            ,BSL_Bit_White_Pannel
            ,FSL_Bit_Black_Text_small
            ,0
            ,0
            ,0
            ,0);
    _pLanguages[0]->SetHAlign(RIGHT);
    _pLanguages[0]->SetVAlign(TOP);
    _pLanguages[0]->SetText((UINT8*)_ppList[0]);

    _pLanguages[1] = new CStaticText(_pPannel, CPoint(40, 42), CSize(70, 20));
    _pLanguages[1]->SetStyle(BSL_Bit_Black_Pannel
            ,FSL_Bit_White_Text_small
            ,BSL_Bit_White_Pannel
            ,FSL_Bit_Black_Text_small
            ,0
            ,0
            ,0
            ,0);
    _pLanguages[1]->SetHAlign(RIGHT);
    _pLanguages[1]->SetVAlign(TOP);
    _pLanguages[1]->SetText((UINT8*)_ppList[1]);
}

ui::CLanguageSelectionWnd::~CLanguageSelectionWnd()
{
    delete _pBmp_logo;
    delete _pBmp_1;
    delete _pBmp_2;
    delete _pBmp_3;
    delete _pBmp_4;
    delete _pLanguages[0];
    delete _pLanguages[1];
    delete _pPannel;
}

void ui::CLanguageSelectionWnd::willHide()
{

}

void ui::CLanguageSelectionWnd::willShow()
{
}

bool ui::CLanguageSelectionWnd::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_key:
            switch (event->_event)
            {
                case key_right_onKeyUp:
                    if (step_language_options == _currentStep) {
                        _pLanguages[0]->SetText((UINT8*)_pHintConfirm[0]);
                        _pLanguages[1]->SetText((UINT8*)_pHintCancel[1]);
                        if (strcmp(_ppList[0], "ä¸­æ–‡") == 0) {
                            _pBmp_logo->setSource(ViditResourceDir, "logo_chinese.bmp");
                        } else if (strcmp(_ppList[0], "English") == 0){
                            _pBmp_logo->setSource(ViditResourceDir, "logo_english.bmp");
                        }
                        _pBmp_logo->SetShow();
                        _currentStep = step_set_language_1;
                        this->GetMainObject()->Draw(false);
                    } else if (step_set_language_1 == _currentStep) {
                        CLanguageLoader::getInstance()->LoadLanguage(_ppList[0]);
                        _cp->SetSystemLanguage(_ppList[0]);
                        this->Close();
                    } else if (step_set_language_2 == _currentStep) {
                        CLanguageLoader::getInstance()->LoadLanguage(_ppList[1]);
                        _cp->SetSystemLanguage(_ppList[1]);
                        this->Close();
                    }
                    b = false;
                    break;
                case key_ok_onKeyUp:
                    if (step_language_options == _currentStep) {
                        _pLanguages[0]->SetText((UINT8*)_pHintConfirm[1]);
                        _pLanguages[1]->SetText((UINT8*)_pHintCancel[0]);
                        if (strcmp(_ppList[1], "English") == 0){
                            _pBmp_logo->setSource(ViditResourceDir, "logo_english.bmp");
                        } else if (strcmp(_ppList[1], "ä¸­æ–‡") == 0) {
                            _pBmp_logo->setSource(ViditResourceDir, "logo_chinese.bmp");
                        }
                        _pBmp_logo->SetShow();
                        _currentStep = step_set_language_2;
                        this->GetMainObject()->Draw(false);
                    } else if ((step_set_language_1 == _currentStep) ||
                        (step_set_language_2 == _currentStep))
                    {
                        _pLanguages[0]->SetText((UINT8*)_ppList[0]);
                        _pLanguages[1]->SetText((UINT8*)_ppList[1]);
                        _pBmp_logo->SetHiden();
                        _currentStep = step_language_options;
                        this->GetMainObject()->Draw(false);
                    }
                    b = false;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if(b)
        b = inherited::OnEvent(event);
    return b;
}

ui::CBitScreenSaveWnd::CBitScreenSaveWnd(CWndManager *pMgr, CCameraProperties* cp)
    : inherited("CBitScreenSaveWnd", pMgr)
    ,_cp(cp)
    ,_screenSaverCount(0)
    ,_info_display_time(0)
    ,_languageType(0)
{
    _pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
    this->SetMainObject(_pPannel);
    _pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

    _pHint_1 = new CStaticText(_pPannel, CPoint(0, 0), CSize(92, 16));
    _pHint_1->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny);
    _pHint_1->SetHAlign(LEFT);
    _pHint_1->SetVAlign(MIDDLE);

    _pHint_2 = new CStaticText(_pPannel, CPoint(0, 21), CSize(92, 16));
    _pHint_2->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny);
    _pHint_2->SetHAlign(LEFT);
    _pHint_2->SetVAlign(MIDDLE);

    memset(_pSpaceInfo, 0, maxMenuStrinLen);
    memset(_pTimeInfo, 0, maxMenuStrinLen);
    _pHint_1->SetText((UINT8 *)_pTimeInfo);
    _pHint_2->SetText((UINT8 *)_pSpaceInfo);

    // Unknown SD inserted when screen saver
    _pStorageWnd = new CBitStorageWnd(pMgr, cp);
    _pDialogWnd = new CBitDialogWnd(pMgr, cp);

    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_Language, tmp, "English");
    if (strcmp(tmp, "English") == 0) {
        _languageType = 0;
    } else if (strcmp(tmp, "ä¸­æ–‡") == 0) {
        _languageType = 1;
    } else if (strcmp(tmp, "§²§å§ã§ã§Ü§Ú§Û") == 0) {
        _languageType = 2;
    }
}

ui::CBitScreenSaveWnd::~CBitScreenSaveWnd() {
    delete _pHint_1;
    delete _pHint_2;
    delete _pPannel;
}

void ui::CBitScreenSaveWnd::willHide() {
    _cp->SetDisplayBrightness(3);
}

void ui::CBitScreenSaveWnd::willShow() {
    _cp->SetDisplayBrightness(-3);
    _screenSaverCount = 0;
    _info_display_time = 0;
    updateUI();
}

bool ui::CBitScreenSaveWnd::OnEvent(CEvent *event) {
    bool b = true;

    switch(event->_type)
    {
        case eventType_Cam_mode:
            updateUI();
            b = false;
            break;
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_info_display_time > 0) {
                    _info_display_time--;
                    if (_info_display_time == 0) {
                        _screenSaverCount = 0;
                        updateUI();
                    }
                } else {
                    camera_State state = _cp->getRecState();
                    if ((camera_State_record == state)
                        || ((_screenSaverCount % 10) == 0))
                    {
                        updateUI();
                    }
                }

                _screenSaverCount++;
                b = false;
            } else if (event->_event == InternalEvent_app_state_update) {
                if (event->_paload == camera_State_stop) {
                    if (_info_display_time == 0) {
                        updateUI();
                    }
                    b = false;
                } else if (event->_paload == camera_State_Storage_full) {
                    // Update UI
                    _info_display_time = 5;
                    _screenSaverCount = 0;
                    _pHint_1->SetRelativePos(CPoint(0, 24));
                    CLanguageItem *pItem = LoadTxtItem("SD card is full!");
                    memset(_pTimeInfo, 0, maxMenuStrinLen);
                    sprintf(_pTimeInfo, "%s", pItem->getContent());
                    memset(_pSpaceInfo, 0, maxMenuStrinLen);
                    _pHint_2->SetRelativePos(CPoint(0, 40));
                    this->GetMainObject()->Draw(false);
                    b = false;
                }
            } else if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    //If insert an unknown SD during screen saver.
                    storage_State st = _cp->getStorageState();
                    if (st == storage_State_unknown) {
                        this->PopWnd(_pStorageWnd);
                        b = false;
                    }
                } else if (event->_paload == PropertyChangeType_language) {
                    char lang[32];
                    memset(lang, 0, 32);
                    _cp->GetSystemLanguage(lang);
                    if (strcmp(lang, "English") == 0) {
                        _languageType = 0;
                    } else if (strcmp(lang, "ä¸­æ–‡") == 0) {
                        _languageType = 1;
                    } else if (strcmp(lang, "§²§å§ã§ã§Ü§Ú§Û") == 0) {
                        _languageType = 2;
                    }
                    CAMERALOG("Language is changed to %s", lang);
                    CLanguageLoader::getInstance()->LoadLanguage(lang);
                    _pDialogWnd->setDialogType(CBitDialogWnd::dialog_language_changed, 3);
                    this->PopWnd(_pDialogWnd);
                    b = false;
                }
            }
            break;
        case eventType_key:
            switch(event->_event) {
                case key_hid: {
                    camera_State state = _cp->getRecState();
                    if (_cp->isCarMode() && (camera_State_record == state)) {
                        // mark video if in car mode
                        _cp->MarkVideo();
                        // Update UI
                        _info_display_time = 5;
                        _screenSaverCount = 0;
                        _pHint_1->SetRelativePos(CPoint(0, 24));
                        CLanguageItem *pItem = LoadTxtItem("Mark video");
                        memset(_pTimeInfo, 0, maxMenuStrinLen);
                        sprintf(_pTimeInfo, "%s %ds!", pItem->getContent(), _cp->getMarkTargetDuration());
                        memset(_pSpaceInfo, 0, maxMenuStrinLen);
                        _pHint_2->SetRelativePos(CPoint(0, 40));
                        this->GetMainObject()->Draw(false);
                    } else {
                        storage_State st = _cp->getStorageState();
                        if ((storage_State_ready == st) || (storage_State_full == st)) {
#ifdef ENABLE_STILL_CAPTURE
                            CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
                            if ((CAM_MODE_PHOTO == working_mode) ||
                                (CAM_MODE_TIMING_SHOT == working_mode) ||
                                (CAM_MODE_PHOTO_LAPSE == working_mode)){
                                // HID only support one shot at the moment.
                                _cp->startStillCapture(true);
                            } else
#endif
                            {
                                _cp->OnRecKeyProcess();
                            }
                        } else if (storage_State_noMedia == st) {
                            // Update UI
                            _info_display_time = 5;
                            _screenSaverCount = 0;
                            _pHint_1->SetRelativePos(CPoint(0, 24));
                            CLanguageItem *pItem = LoadTxtItem("No SD card!");
                            memset(_pTimeInfo, 0, maxMenuStrinLen);
                            sprintf(_pTimeInfo, "%s", pItem->getContent());
                            memset(_pSpaceInfo, 0, maxMenuStrinLen);
                            _pHint_2->SetRelativePos(CPoint(0, 40));
                            this->GetMainObject()->Draw(false);
                        } else if ((storage_State_error == st) || (storage_State_unknown == st)) {
                            // Update UI
                            _info_display_time = 5;
                            _screenSaverCount = 0;
                            _pHint_1->SetRelativePos(CPoint(0, 24));
                            CLanguageItem *pItem = LoadTxtItem("SD card in Error!");
                            memset(_pTimeInfo, 0, maxMenuStrinLen);
                            sprintf(_pTimeInfo, "%s", pItem->getContent());
                            memset(_pSpaceInfo, 0, maxMenuStrinLen);
                            _pHint_2->SetRelativePos(CPoint(0, 40));
                            this->GetMainObject()->Draw(false);
                        }
                    }
                    b = false;
                    break;
                }
                case key_up_onKeyUp: {
                    camera_State state = _cp->getRecState();
                    if (camera_State_record == state) {
                        // mark video
                        _cp->MarkVideo();
                        // Update UI
                        _info_display_time = 5;
                        _screenSaverCount = 0;
                        _pHint_1->SetRelativePos(CPoint(0, 24));
                        CLanguageItem *pItem = LoadTxtItem("Mark video");
                        memset(_pTimeInfo, 0, maxMenuStrinLen);
                        sprintf(_pTimeInfo, "%s %ds!",
                            pItem->getContent(), _cp->getMarkTargetDuration());
                        memset(_pSpaceInfo, 0, maxMenuStrinLen);
                        _pHint_2->SetRelativePos(CPoint(0, 40));
                        this->GetMainObject()->Draw(false);
                    } else {
                        this->Close();
                    }
                    b = false;
                    break;
                }
                case key_right_onKeyUp:
                case key_down_onKeyUp:
                case key_ok_onKeyUp:
                    this->Close();
                    b = false;
                    break;
                default:
                    break;
            }
            b = false;
            break;
        default:
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}

void ui::CBitScreenSaveWnd::updateStorageUI() {
    storage_State st = _cp->getStorageState();
    memset(_pSpaceInfo, 0, maxMenuStrinLen);
    if (storage_State_ready == st) {
        int free, total;
        int freeMB = _cp->getFreeSpaceMB(free, total);
        CLanguageItem *pItem = LoadTxtItem("Free");
        if (freeMB >= 1024) {
            // In industry, 1MB = 1000KB
            double freeGB = (double)freeMB * 1024 * 1024 / 1000.0 / 1000.0 / 1000.0;
            if (freeGB < 0.0) {
                freeGB = 0.0;
            }
            if (freeGB >= 10.0) {
                if (_languageType == 1) { // Chinese
                    sprintf(_pSpaceInfo, "%s%dG", pItem->getContent(), (int)freeGB);
                } else { // English
                    sprintf(_pSpaceInfo, "%dG %s", (int)freeGB, pItem->getContent());
                }
            } else {
                if (_languageType == 1) { // Chinese
                    sprintf(_pSpaceInfo, "%s%.2fG", pItem->getContent(), freeGB);
                } else { // English
                    sprintf(_pSpaceInfo, "%.2fG %s", freeGB, pItem->getContent());
                }
            }
        } else {
            if (_languageType == 1) { // Chinese
                if (freeMB >= 1) {
                    sprintf(_pSpaceInfo, "%s%dM", pItem->getContent(), freeMB);
                } else {
                    sprintf(_pSpaceInfo, "%s<1M", pItem->getContent());
                }
            } else { // English
                if (freeMB >= 1) {
                    sprintf(_pSpaceInfo, "%dM %s", freeMB, pItem->getContent());
                } else {
                    sprintf(_pSpaceInfo, "<1M %s", pItem->getContent());
                }
            }
        }
    } else if (storage_State_noMedia == st) {
        CLanguageItem *pItem = LoadTxtItem("No card");
        sprintf(_pSpaceInfo, "%s", pItem->getContent());
    } else if (storage_State_full == st) {
        CLanguageItem *pItem = LoadTxtItem("SD Full");
        sprintf(_pSpaceInfo, "%s", pItem->getContent());
    } else if (storage_State_error == st) {
        CLanguageItem *pItem = LoadTxtItem("SD Error");
        sprintf(_pSpaceInfo, "%s", pItem->getContent());
    } else if (storage_State_unknown == st) {
        CLanguageItem *pItem = LoadTxtItem("Unknown SD");
        sprintf(_pSpaceInfo, "%s", pItem->getContent());
    }
}

void ui::CBitScreenSaveWnd::updateUI() {
    // update recording time / mode
    CAM_WORKING_MODE working_mode = _cp->getWorkingMode();
    CLanguageItem *pItem = NULL;
    memset(_pTimeInfo, 0, maxMenuStrinLen);

#ifdef ARCH_A5S
    switch (working_mode) {
        case CAM_MODE_VIDEO:
        {
            camera_State state = _cp->getRecState();
            if (camera_State_record == state) {
                UINT64 tt = _cp->getRecordingTime();
                UINT64 hh =  tt /3600;
                UINT64 mm = (tt % 3600) /60;
                UINT64 ss = (tt % 3600) %60;
                int j = 0;
                j += sprintf(_pTimeInfo + j, "%02lld:", hh);
                j += sprintf(_pTimeInfo + j, "%02lld:", mm);
                j += sprintf(_pTimeInfo + j, "%02lld", ss);
            } else {
                sprintf(_pTimeInfo, "%s", "VIDIT");
            }
            break;
        }
        default:
            break;
    }
#else
    switch (working_mode) {
        case CAM_MODE_VIDEO:
        case CAM_MODE_SLOW_MOTION:
        case CAM_MODE_VIDEO_LAPSE:
        {
            camera_State state = _cp->getRecState();
            if (camera_State_record == state) {
                UINT64 tt = _cp->getRecordingTime();
                UINT64 hh =  tt /3600;
                UINT64 mm = (tt % 3600) /60;
                UINT64 ss = (tt % 3600) %60;
                int j = 0;
                j += sprintf(_pTimeInfo + j, "%02lld:", hh);
                j += sprintf(_pTimeInfo + j, "%02lld:", mm);
                j += sprintf(_pTimeInfo + j, "%02lld", ss);
            } else {
                pItem = LoadTxtItem("VIDEO");
                sprintf(_pTimeInfo, "%s", pItem->getContent());
            }
            break;
        }
        case CAM_MODE_PHOTO:
        case CAM_MODE_TIMING_SHOT:
        case CAM_MODE_RAW_PICTURE:
        {
            // update mode
            pItem = LoadTxtItem("PHOTO");
            sprintf(_pTimeInfo, "%s", pItem->getContent());
            break;
        }
        case CAM_MODE_PHOTO_LAPSE:
        {
            char tmp[256];
            memset(tmp, 0x00, 256);
            agcmd_property_get(PropName_PhotoLapse, tmp, "0.5");
            float interval_sec = atof(tmp);

            // update time interval
            pItem = LoadTxtItem("PTL");
            if (interval_sec < 1.0) {
                sprintf(_pTimeInfo, "%s %.1fs", pItem->getContent(), interval_sec);
            } else if (interval_sec <= 60.0) {
                sprintf(_pTimeInfo, "%s %ds", pItem->getContent(), (int)interval_sec);
            } else if (interval_sec < 3600.0) {
                sprintf(_pTimeInfo, "%s %dm", pItem->getContent(), (int)interval_sec / 60);
            } else {
                sprintf(_pTimeInfo, "%s %dh", pItem->getContent(), (int)interval_sec / 3600);
            }
            break;
        }
        default:
            break;
    }
#endif

    // update storage space every 10s
    if (_screenSaverCount % 10 == 0) {
        if ((_screenSaverCount / 10) % 2 == 0) {
            CLanguageItem *pItem = LoadTxtItem("Any key back");
            memset(_pSpaceInfo, 0, maxMenuStrinLen);
            sprintf(_pSpaceInfo, "%s", pItem->getContent());
        } else {
            updateStorageUI();
        }
    }

    // move postion every 20s
    if (_screenSaverCount % 20 == 0) {
        switch ((_screenSaverCount / 20) % 4) {
            case 0:
                _pHint_1->SetRelativePos(CPoint(0, 0));
                _pHint_2->SetRelativePos(CPoint(0, 16));
                break;
            case 1:
                _pHint_1->SetRelativePos(CPoint(36, 32));
                _pHint_2->SetRelativePos(CPoint(36, 48));
                break;
            case 2:
                _pHint_1->SetRelativePos(CPoint(36, 0));
                _pHint_2->SetRelativePos(CPoint(36, 16));
                break;
            case 3:
                _pHint_1->SetRelativePos(CPoint(0, 32));
                _pHint_2->SetRelativePos(CPoint(0, 48));
                break;
        }
    }

    this->GetMainObject()->Draw(false);
}

ui::CBitStorageWnd::CBitStorageWnd(CWndManager *pMgr, CCameraProperties* cp)
    : inherited("CBitStorageWnd", pMgr)
    ,_cp(cp)
    ,_formatStep(formatStep_None)
    ,_displayTime(0)
{
    _pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
    this->SetMainObject(_pPannel);
    _pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

    _pBmp_1 = new CBmpControl(
        _pPannel, CPoint(114, 7), CSize(8, 8), ViditResourceDir, "dot_full.bmp");
    _pBmp_2 = new CBmpControl(
        _pPannel, CPoint(114, 21), CSize(8, 8), ViditResourceDir, "dot_vacuum.bmp");
    _pBmp_3 = new CBmpControl(
        _pPannel, CPoint(114, 35), CSize(8, 8), ViditResourceDir, "dot_vacuum.bmp");
    _pBmp_4 = new CBmpControl(
        _pPannel, CPoint(114, 49), CSize(8, 8), ViditResourceDir, "dot_vacuum.bmp");
    _pBmp_1->SetHiden();
    _pBmp_2->SetHiden();
    _pBmp_3->SetHiden();
    _pBmp_4->SetHiden();

    _textInfor_1 = new CStaticText(_pPannel, CPoint(0, 0), CSize(110, 64));
    _textInfor_1->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny);
    _textInfor_1->SetHAlign(CENTER);
    _textInfor_1->SetVAlign(MIDDLE);

    memset(_pDialogInfo, 0x00, 64);
    _textInfor_1->SetText((UINT8 *)_pDialogInfo);
}

ui::CBitStorageWnd::~CBitStorageWnd() {
    delete _textInfor_1;
    delete _pBmp_1;
    delete _pBmp_2;
    delete _pBmp_3;
    delete _pBmp_4;
    delete _pPannel;
}

void ui::CBitStorageWnd::willHide() {
}

void ui::CBitStorageWnd::willShow() {
    _formatStep = formatStep_None;
    updateUI();
}

void ui::CBitStorageWnd::updateUI() {
    storage_State st = _cp->getStorageState();
    if (storage_State_prepare == st) {
        CLanguageItem *pItem = LoadTxtItem("Preparing SD");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (storage_State_noMedia == st) {
        CLanguageItem *pItem = LoadTxtItem("No card");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (storage_State_unknown == st) {
        CLanguageItem *pItem = LoadTxtItem(
            "Unknown SD Press 1st key to Format");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
        _pBmp_1->setSource(ViditResourceDir, "dot_full.bmp");
        _pBmp_4->setSource(ViditResourceDir, "dot_vacuum.bmp");
        _pBmp_1->SetShow();
        _pBmp_2->SetShow();
        _pBmp_3->SetShow();
        _pBmp_4->SetShow();
    } else if (storage_State_full == st) {
        CLanguageItem *pItem = LoadTxtItem("SD Full");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (storage_State_error == st) {
        CLanguageItem *pItem = LoadTxtItem("SD Error");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    }

    this->GetMainObject()->Draw(false);
}

bool ui::CBitStorageWnd::OnEvent(CEvent *event) {
    bool b = true;

    switch(event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    if (_formatStep != formatStep_2) {
                        updateUI();
                        storage_State st = _cp->getStorageState();
                        if (storage_State_ready == st) {
                            this->Close();
                        } else if (storage_State_noMedia == st) {
                            this->Close();
                        }
                    }
                    b = false;
                }
            } else if (event->_event == InternalEvent_app_state_update) {
                if (event->_paload == camera_State_FormatSD_result) {
                    CLanguageItem *pItem = NULL;
                    if (event->_paload1 != 0) {
                        pItem = LoadTxtItem("Format Successful");
                        _displayTime = 2;
                    } else {
                        pItem = LoadTxtItem("Format Failure");
                    }
                    sprintf(_pDialogInfo, "%s", pItem->getContent());
                    this->GetMainObject()->Draw(false);
                    _formatStep = formatStep_None;
                }
                b = false;
            } else if (event->_event == InternalEvent_app_timer_update) {
                if (_displayTime > 0) {
                    _displayTime--;
                    if (_displayTime == 0) {
                        this->Close();
                    }
                }
            }
            break;
        case eventType_key:
            switch(event->_event)
            {
                case key_right_onKeyUp:
                    if (formatStep_None == _formatStep) {
                        CLanguageItem *pItem = LoadTxtItem(
                            "Press 4th key begin Format other Cancel");
                        sprintf(_pDialogInfo, "%s", pItem->getContent());
                        _pBmp_1->setSource(ViditResourceDir, "dot_vacuum.bmp");
                        _pBmp_4->setSource(ViditResourceDir, "dot_full.bmp");
                        this->GetMainObject()->Draw(false);
                        _formatStep = formatStep_1;
                    } else if (formatStep_1 == _formatStep) {
                        CLanguageItem *pItem = LoadTxtItem(
                            "Unknown SD Press 1st key to Format");
                        sprintf(_pDialogInfo, "%s", pItem->getContent());
                        _pBmp_1->setSource(ViditResourceDir, "dot_full.bmp");
                        _pBmp_4->setSource(ViditResourceDir, "dot_vacuum.bmp");
                        this->GetMainObject()->Draw(false);
                        _formatStep = formatStep_None;
                    }
                    b = false;
                    break;
                case key_ok_onKeyUp:
                    if (formatStep_1 == _formatStep) {
                        CLanguageItem *pItem = LoadTxtItem("Formatting");
                        sprintf(_pDialogInfo, "%s...", pItem->getContent());
                        _pBmp_1->SetHiden();
                        _pBmp_2->SetHiden();
                        _pBmp_3->SetHiden();
                        _pBmp_4->SetHiden();
                        this->GetMainObject()->Draw(false);
                        _cp->FormatStorage();
                        _formatStep = formatStep_2;
                    }
                    b = false;
                    break;
                case key_up_onKeyUp:
                case key_down_onKeyUp:
                    if (_formatStep != formatStep_2){
                        CLanguageItem *pItem = LoadTxtItem(
                            "Unknown SD Press 1st key to Format");
                        sprintf(_pDialogInfo, "%s", pItem->getContent());
                        _pBmp_1->setSource(ViditResourceDir, "dot_full.bmp");
                        _pBmp_4->setSource(ViditResourceDir, "dot_vacuum.bmp");
                        this->GetMainObject()->Draw(false);
                        _formatStep = formatStep_None;
                    }
                    b = false;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}

ui::CBitDialogWnd::CBitDialogWnd(CWndManager *pMgr, CCameraProperties* cp)
    :inherited("DialogWnd", pMgr)
    ,_cp(cp)
    ,_timeoutCnt(2)
{
    _pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
    this->SetMainObject(_pPannel);
    _pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

    _pNotice= new CStaticText(_pPannel, CPoint(0, 0), CSize(128, 24));
    _pNotice->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,0
        ,0
        ,0
        ,0);
    _pNotice->SetLanguageItem(LoadTxtItem("Information"));
    _pNotice->SetHAlign(CENTER);
    _pNotice->SetVAlign(MIDDLE);

    _pRow = new CBlock(_pPannel, CPoint(2, 24), CSize(124, 1), BSL_Bit_White_Pannel);

    _textInfor_1 = new CStaticText(_pPannel, CPoint(0, 26), CSize(128, 38));
    _textInfor_1->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny);
    _textInfor_1->SetHAlign(CENTER);
    _textInfor_1->SetVAlign(TOP);

    memset(_pDialogInfo, 0x00, 64);
    _textInfor_1->SetText((UINT8 *)_pDialogInfo);
}

ui::CBitDialogWnd::~CBitDialogWnd() {
    delete _textInfor_1;
    delete _pRow;
    delete _pNotice;
    delete _pPannel;
}

void ui::CBitDialogWnd::willHide() {

}

void ui::CBitDialogWnd::willShow() {

}

void ui::CBitDialogWnd::setDialogType(int type, int timeShow) {
    _timeoutCnt = timeShow;

    memset(_pDialogInfo, 0x00, 64);
    if (dialog_wifi == type) {
        CLanguageItem *pItem = LoadTxtItem("Long Press change wifi mode");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (dialog_changing_wifi_mode == type) {
        CLanguageItem *pItem = LoadTxtItem("Apply new wifi mode");
        sprintf(_pDialogInfo, "%s...", pItem->getContent());
    } else if (dialog_changing_wifi_success == type) {
        CLanguageItem *pItem = LoadTxtItem("Change wifi mode SUCCESS");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (dialog_changing_wifi_timeout == type) {
        CLanguageItem *pItem = LoadTxtItem("Change wifi mode TIMEOUT");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (dialog_wifi_no_change == type) {
        CLanguageItem *pItem = LoadTxtItem("Select a different mode please!");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (dialog_mark == type) {
        camera_State state = _cp->getRecState();
        if (camera_State_record == state) {
            CLanguageItem *pItem = LoadTxtItem("Mark video");
            sprintf(_pDialogInfo, "%s %ds!", pItem->getContent(), _cp->getMarkTargetDuration());
        } else {
            CLanguageItem *pItem = LoadTxtItem(
                "In recording press it to mark video!");
            sprintf(_pDialogInfo, "%s", pItem->getContent());
        }
    } else if (dialog_poweroff == type) {
        CLanguageItem *pItem = LoadTxtItem("Long press Power Off");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (dialog_start_recording == type) {
        CLanguageItem *pItem = LoadTxtItem("Start recording");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (dialog_stop_recording == type) {
        CLanguageItem *pItem = LoadTxtItem("Stop recording");
        sprintf(_pDialogInfo, "%s", pItem->getContent());
    } else if (dialog_language_changed == type) {
        char lang[32];
        memset(lang, 0, 32);
        _cp->GetSystemLanguage(lang);
        CLanguageItem *pItem = LoadTxtItem("Language is set to");
        sprintf(_pDialogInfo, "%s: %s", pItem->getContent(), lang);
    }
}

bool ui::CBitDialogWnd::OnEvent(CEvent *event) {
    if ((event->_type == eventType_internal)
        && (event->_event == InternalEvent_app_timer_update))
    {
        _timeoutCnt--;
        if (_timeoutCnt <= 0) {
            this->Close();
        }
        return false;
    } else if (event->_type == eventType_key) {
        this->Close();
        // Let this event continues to be processed by others
        return true;
    }

    return true;
}

ui::CBitPowerOffWnd::CBitPowerOffWnd(CWndManager *pMgr, CCameraProperties* cp)
    :inherited("PowerOffWnd", pMgr)
    ,_cp(cp)
{
    _pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
    this->SetMainObject(_pPannel);
    _pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

    _pBmp_1 = new CBmpControl(
        _pPannel, CPoint(114, 7), CSize(8, 8), ViditResourceDir, "dot_full.bmp");
    _pBmp_2 = new CBmpControl(
        _pPannel, CPoint(114, 21), CSize(8, 8), ViditResourceDir, "dot_vacuum.bmp");
    _pBmp_3 = new CBmpControl(
        _pPannel, CPoint(114, 35), CSize(8, 8), ViditResourceDir, "dot_vacuum.bmp");
    _pBmp_4 = new CBmpControl(
        _pPannel, CPoint(114, 49), CSize(8, 8), ViditResourceDir, "dot_full.bmp");

    _pCancel = new CStaticText(_pPannel, CPoint(20, 0), CSize(90, 22));
    _pCancel->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,0
        ,0
        ,0
        ,0);
    _pCancel->SetHAlign(RIGHT);
    _pCancel->SetVAlign(MIDDLE);
    _pCancel->SetLanguageItem(LoadTxtItem("Cancel"));

    _pConfirm = new CStaticText(_pPannel, CPoint(20, 42), CSize(90, 22));
    _pConfirm->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_small
        ,0
        ,0
        ,0
        ,0);
    _pConfirm->SetLanguageItem(LoadTxtItem("Shut down"));
    _pConfirm->SetHAlign(RIGHT);
    _pConfirm->SetVAlign(MIDDLE);

    _pT = new CStaticText(_pPannel, CPoint(0, 0), CSize(128, 64));
    _pT->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,BSL_Bit_Black_Pannel
        ,FSL_Bit_White_Text_tiny
        ,0
        ,0
        ,0
        ,0);
    _pT->SetHAlign(CENTER);
    _pT->SetVAlign(MIDDLE);
    memset(txt, 0x00, 32);
    CLanguageItem *item = LoadTxtItem("Shutting down");
    sprintf(txt, "%s..", item->getContent());
    _pT->SetText((UINT8 *)txt);
    _pT->SetHiden();
}


ui::CBitPowerOffWnd::~CBitPowerOffWnd()
{
    delete _pT;
    delete _pConfirm;
    delete _pCancel;
    delete _pBmp_1;
    delete _pBmp_2;
    delete _pBmp_3;
    delete _pBmp_4;
    delete _pPannel;
}


void ui::CBitPowerOffWnd::willHide()
{

}

void ui::CBitPowerOffWnd::willShow()
{
    _pBmp_1->SetShow();
    _pBmp_2->SetShow();
    _pBmp_3->SetShow();
    _pBmp_4->SetShow();
    _pConfirm->SetShow();
    _pCancel->SetShow();
    _pT->SetHiden();
}

bool ui::CBitPowerOffWnd::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_key:
            switch (event->_event)
            {
                case key_ok_onKeyDown: {
                    CAMERALOG("-- power off\n");
                    _pBmp_1->SetHiden();
                    _pBmp_2->SetHiden();
                    _pBmp_3->SetHiden();
                    _pBmp_4->SetHiden();
                    _pConfirm->SetHiden();
                    _pCancel->SetHiden();
                    _pT->SetShow();
                    this->GetMainObject()->Draw(false);

                    CAMERALOG("Call \"agsh poweroff\" to power off now!");
                    const char *tmp_args[8];
                    tmp_args[0] = "agsh";
                    tmp_args[1] = "poweroff";
                    tmp_args[2] = NULL;
                    agbox_sh(2, (const char *const *)tmp_args);
                    b = false;
                    break;
                }
                case key_right_onKeyUp:
                    CAMERALOG("-- cancel power off");
                    this->Close();
                    break;
                default:
                    b = false;
                    break;
            }
            break;
        case eventType_Gesnsor:
            break;
    }
    if (b) {
        b = inherited::OnEvent(event);
    }
    return b;
}

