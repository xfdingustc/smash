/*****************************************************************************
 * upnp_sdcc.cpp:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/fb.h>
#include <arpa/inet.h> //struct in_addr

#include "agbox.h"
#include "GobalMacro.h"
#include "class_gsensor_thread.h"
#include "AudioPlayer.h"

#include "upnp_sdcc.h"


extern void getStoragePropertyName(char *prop, int len);

#define AGB_SSD1308_IOCTL_SET_CONTRAST _IOW('a', 0, int)
#if defined(PLATFORM22A) || defined(PLATFORMHACHI)
const static char* markTimer = "mark Timer";
#endif

/***************************************************************
CUpnpCloudCamera
*/
CUpnpCloudCamera::CUpnpCloudCamera()
    : _CurrentIP(0)
    , _bMarking(false)
    , _bDisplayOff(false)
{
    // Firstly create avf client
    IPC_AVF_Client::create();

#ifdef PLATFORMHACHI
    _sensors = new SensorManager();
#else
    _setBrightness(128);
#endif
}


CUpnpCloudCamera::~CUpnpCloudCamera() 
{
    CAMERALOG("delete camera");
    IPC_AVF_Client::destroy();

#ifdef PLATFORMHACHI
    delete _sensors;
#endif
    CAMERALOG("destroyed");
}

void CUpnpCloudCamera::EventWriteWarnning()
{
    if (_ppObeservers != NULL) {
        _ppObeservers->OnPropertyChange(0, PropertyChangeType_FileWriteWarnning);
    }

#ifdef ENABLE_LOUDSPEAKER
    if (CAudioPlayer::getInstance()) {
        CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_StopRecording);
    }
#endif
}

void CUpnpCloudCamera::EventWriteError()
{
    if (_ppObeservers != NULL) {
        _ppObeservers->OnPropertyChange(0, PropertyChangeType_FileWriteError);
    }

#ifdef ENABLE_LOUDSPEAKER
    if (CAudioPlayer::getInstance()) {
        CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_StopRecording);
    }
#endif
}

// ui interface
camera_State CUpnpCloudCamera::getRecState()
{
    int avf_state = 0;
    int is_still = 0;
    IPC_AVF_Client::getObject()->getRecordState(&avf_state, &is_still);
    camera_State state = camera_State_stop;
    switch(avf_state)
    {
        case State_Rec_stoped:
        case State_Rec_close:
            state = camera_State_stop;
            break;
        case State_Rec_stopping:
            state = camera_State_stopping;
            break;
        case State_Rec_recording:
            if (_bMarking) {
                state = camera_State_marking;
            } else {
                state = camera_State_record;
            }
            break;
        case State_Rec_starting:
            state = camera_State_starting;
            break;
        case State_Rec_PhotoLapse:
            state = camera_State_PhotoLapse;
            break;
        case State_Rec_PhotoLapse_shutdown:
            state = camera_State_PhotoLapse_shutdown;
            break;
        case State_Rec_error:
            state = camera_State_Error;
            break;
        default:
            state = camera_State_stop;
            break;
    }
    return state;
}

UINT64 CUpnpCloudCamera::getRecordingTime()
{
    return IPC_AVF_Client::getObject()->getRecordingTime();
}

int CUpnpCloudCamera::getMarkTargetDuration() {
    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_Mark_before, tmp, Mark_before_time);
    int before_s = atoi(tmp);
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_Mark_after, tmp, Mark_after_time);
    int after_s = atoi(tmp);
    return before_s + after_s;
}

storage_rec_state CUpnpCloudCamera::getStorageRecState()
{
    return (storage_rec_state)IPC_AVF_Client::getObject()->getStorageRecState();
}

storage_State CUpnpCloudCamera::getStorageState()
{
    storage_State state = storage_State_full;
    int vdbSpaceState;
    char tmp[256];

    char storage_prop[256];
    getStoragePropertyName(storage_prop, sizeof(storage_prop));
    memset(tmp, 0x00, sizeof(tmp));
    agcmd_property_get(storage_prop, tmp, "");
    if (strcasecmp(tmp, "mount_ok") == 0) {
        vdbSpaceState = IPC_AVF_Client::getObject()->GetStorageState();
        if (vdbSpaceState < 0) {
            state = storage_State_error;
        } else if (vdbSpaceState == 0) {
            state = storage_State_prepare;
            // because SD is mounted, try enable vdb again.
            IPC_AVF_Client::getObject()->onTFReady(true);
        } else if (vdbSpaceState == 1) {
            state = storage_State_full;
        } else {
            state = storage_State_ready;
        }
    } else if((strcasecmp(tmp, "mount_fail") == 0)
        || (strcasecmp(tmp, "NA") == 0)) {
        state = storage_State_noMedia;
    } else if ((strcasecmp(tmp, "unknown") == 0)) {
        state = storage_State_unknown;
    } else if ((strcasecmp(tmp, "checking") == 0)
        || (strcasecmp(tmp, "formating") == 0)) {
        state = storage_State_prepare;
    } else if (strcasecmp(tmp, "ready") == 0) {
        state = storage_State_usb;
    }

    return state;
}

int CUpnpCloudCamera::getFreeSpaceMB(int& freeMB, int& totalMB) {
    bool rdy = false;
    unsigned int free = 0;
    long long Total = 0;
    IPC_AVF_Client::getObject()->GetVDBState(rdy, free, Total);

    freeMB = free * 1024 * 1024 / 1000 / 1000;
    totalMB = Total * 1024 * 1024 / 1000 / 1000;
    return free;
}

int CUpnpCloudCamera::getSpaceInfoMB(int &totleMB, int &markedMB, int &loopMB)
{
    bool ret = IPC_AVF_Client::getObject()->GetVDBSpaceInfo(totleMB, markedMB, loopMB);
    return ret;
}

int CUpnpCloudCamera::getStorageFreSpcByPrcn()
{
    return IPC_AVF_Client::getObject()->GetStorageFreSpcPcnt();
}

bool CUpnpCloudCamera::FormatStorage()
{
    return IPC_AVF_Client::getObject()->FormatTF();
}

bool CUpnpCloudCamera::EnableUSBStorage()
{
    return IPC_AVF_Client::getObject()->EnableUSBStorage();
}

bool CUpnpCloudCamera::DisableUSBStorage(bool force)
{
    return IPC_AVF_Client::getObject()->DisableUSBStorage(force);
}

rotate_State CUpnpCloudCamera::getRotationState()
{
    int switchOption = SW_ROTATE_LOCK_MIRROR;
    int rt = IPC_AVF_Client::getObject()->isRotate();
    if (rt) {
        if (switchOption == 1) {
            return rotate_State_mirror_0;
        } else if (switchOption == 0) {
            return rotate_State_0;
        } else {
            return rotate_State_hdr_on; // sould be hdr on off
        }
    } else {
        if (switchOption == 1) {
            return rotate_State_mirror_180;
        } else if (switchOption == 0) {
            return rotate_State_180;
        } else {
            return rotate_State_hdr_off; // sould be hdr on off
        }
    }
}

gps_state CUpnpCloudCamera::getGPSState()
{
    int rt = IPC_AVF_Client::getObject()->getGpsState();
    gps_state state = gps_state_off;
    switch (rt)
    {
        case 0:
            state = gps_state_ready;
            break;
        case -1:
            state = gps_state_on;
            break;
        case -2:
            state = gps_state_off;
            break;
        default:
            state = gps_state_off;
            break;
    }
    return state;
}

bluetooth_state CUpnpCloudCamera::getBluetoothState() {
    bluetooth_state state = bt_state_off;
    char tmp[256];

    memset(tmp, 0x00, sizeof(tmp));
    agcmd_property_get(propBTTempStatus, tmp ,"");
    if (strcasecmp(tmp, "on") == 0) {
        state = bt_state_on;

        bool hid = false;
        bool obd = false;
        memset(tmp, 0x00, sizeof(tmp));
        agcmd_property_get(propBTTempHIDStatus, tmp ,"");
        if (strcasecmp(tmp, "on") == 0) {
            hid = true;
        }

        memset(tmp, 0x00, sizeof(tmp));
        agcmd_property_get(propBTTempOBDStatus, tmp ,"");
        if (strcasecmp(tmp, "on") == 0) {
            obd = true;
        }

        if (hid && obd) {
            state = bt_state_hid_obd_paired;
        } else if (hid) {
            state = bt_state_hid_paired;
        } else if (obd) {
            state = bt_state_obd_paired;
        }
    } else if (strcasecmp(tmp, "off") == 0) {
        state = bt_state_off;
    }

    return state;
}

camera_mode CUpnpCloudCamera::getRecMode()
{
    if (IPC_AVF_Client::getObject()->isCarMode()) {
        return camera_mode_car;
    } else {
        return camera_mode_normal;
    }
}
void CUpnpCloudCamera::setRecMode(camera_mode m)
{
    if (m == camera_mode_car) {
        agcmd_property_set(PropName_rec_recMode, "CAR");
    } else {
        agcmd_property_set(PropName_rec_recMode, "NORMAL");
    }
}

void CUpnpCloudCamera::setRecSegLength(int seconds)
{
    IPC_AVF_Client::getObject()->SetSegSeconds(seconds);
}

int CUpnpCloudCamera::getBitrate() {
    // primary video + second video
    return IPC_AVF_Client::getObject()->getBitrate() +
        IPC_AVF_Client::getObject()->getSecondVideoBitrate();
}

int CUpnpCloudCamera::startRecMode()
{
    IPC_AVF_Client::getObject()->switchToRec(true);
    return 0;
}

int CUpnpCloudCamera::endRecMode()
{
    IPC_AVF_Client::getObject()->CloseRec();
    return 0;
}

bool CUpnpCloudCamera::isCarMode() {
    return IPC_AVF_Client::getObject()->isCarMode();
}

bool CUpnpCloudCamera::isCircleMode() {
    return IPC_AVF_Client::getObject()->isCircleMode();
}

bool CUpnpCloudCamera::setWorkingMode(CAM_WORKING_MODE mode) {
    return IPC_AVF_Client::getObject()->setWorkingMode(mode);
}

CAM_WORKING_MODE CUpnpCloudCamera::getWorkingMode() {
    return IPC_AVF_Client::getObject()->getWorkingMode();
}

#ifdef ENABLE_STILL_CAPTURE
void CUpnpCloudCamera::setStillCaptureMode(bool bStillMode) {
    return IPC_AVF_Client::getObject()->setStillCaptureMode(bStillMode);
}

bool CUpnpCloudCamera::isStillCaptureMode() {
    return IPC_AVF_Client::getObject()->isStillCaptureMode();
}

void CUpnpCloudCamera::startStillCapture(bool one_shot) {
    return IPC_AVF_Client::getObject()->startStillCapture(one_shot);
}

int CUpnpCloudCamera::getTotalPicNum() {
    return IPC_AVF_Client::getObject()->getTotalPicNum();
}

void CUpnpCloudCamera::stopStillCapture() {
    return IPC_AVF_Client::getObject()->stopStillCapture();
}

#endif

void CUpnpCloudCamera::setCameraCallback(CameraCallback  *callback) {
    return IPC_AVF_Client::getObject()->setCameraCallback(callback);
}

void CUpnpCloudCamera::OnSystemTimeChange()
{
    //CUpnpThread::getInstance()->RequireUpnpReboot();
}

void CUpnpCloudCamera::ShowGPSFullInfor(bool b)
{
    IPC_AVF_Client::getObject()->ShowGPSFullInfor(b);
}

void CUpnpCloudCamera::UpdateSubtitleConfig()
{
    IPC_AVF_Client::getObject()->UpdateSubtitleConfig();
}

void CUpnpCloudCamera::LCDOnOff(bool b)
{
    IPC_AVF_Client::getObject()->LCDOnOff(b);
}

void CUpnpCloudCamera::OnRecKeyProcess()
{
    IPC_AVF_Client::getObject()->RecordKeyProcess();
}

void CUpnpCloudCamera::EnableDisplayManually()
{
    IPC_AVF_Client::getObject()->EnableDisplay();
}

void CUpnpCloudCamera::SetMicVolume(int volume)
{
    IPC_AVF_Client::getObject()->SetMicVolume(volume);
}

void CUpnpCloudCamera::SetMicMute(bool mute)
{
    IPC_AVF_Client::getObject()->SetMicMute(mute);
}

void CUpnpCloudCamera::SetMicWindNoise(bool enable)
{
    IPC_AVF_Client::getObject()->SetMicWindNoise(enable);
}

void CUpnpCloudCamera::SetSpkVolume(int volume)
{
    IPC_AVF_Client::getObject()->SetSpkVolume(volume);
}

void CUpnpCloudCamera::SetSpkMute(bool mute)
{
    IPC_AVF_Client::getObject()->SetSpkMute(mute);
}

int CUpnpCloudCamera::GetBatteryVolume(bool& bIncharge)
{
    int rt = 0;
    char tmp[AGCMD_PROPERTY_SIZE];

    agcmd_property_get(propPowerSupplyBCapacity, tmp, "0");
    rt = strtoul(tmp, NULL, 0);
    agcmd_property_get(powerstateName, tmp, "Discharging");
    bIncharge = !!(strcasecmp(tmp, "Charging") == 0);

    return rt;
}

int CUpnpCloudCamera::GetBatteryRemainingTime(bool& bDischarging) {
    int charge_now = 0;
    int current_avg = 0;
    int rt_min = 0;

    char tmp[AGCMD_PROPERTY_SIZE];
    agcmd_property_get(powerstateName, tmp, "Discharging");
    bDischarging = !!(strcasecmp(tmp, "Discharging") == 0);
    if (bDischarging) {
        agcmd_property_get(propPowerSupplyCharge, tmp, "0");
        charge_now = strtoul(tmp, NULL, 0);

        agcmd_property_get(propPowerSupplyCurrent, tmp, "0");
        current_avg = strtoul(tmp, NULL, 0) * (-1);

        if (current_avg != 0) {
            rt_min = (charge_now / (float)current_avg) * 60;
        }
    }

    return rt_min;
}

int CUpnpCloudCamera::SetDisplayBrightness(int value)
{
#ifdef PLATFORMHACHI
    IPC_AVF_Client::getObject()->SetDisplayBrightness(value);

    // brightness 0 is only for auto off screen
    if (value == 0) {
        _bDisplayOff = true;
    } else {
        int brightness = (value - Brightness_MIN) / Brightness_Step_Value;
        _bDisplayOff = false;
        char tmp[256] = {0x00};
        snprintf(tmp, 256, "%d", brightness * Brightness_Step_Value + Brightness_MIN);
        //CAMERALOG("value = %d, brightness = %d, tmp = %s", value, brightness, tmp);
        agcmd_property_set(PropName_Brighness, tmp);
    }
    return value;
#else
    int brightness = GetBrightness();
    brightness += 28 * value;
    if (brightness > 252) {
        brightness = 252;
    } else if (brightness < 0) {
        brightness = 0;
    }
    _setBrightness(brightness);
    return brightness;
#endif
}

int CUpnpCloudCamera::GetBrightness()
{
    char tmp[256];
    agcmd_property_get(PropName_Brighness, tmp, Default_Brighness);

    int brightness = atoi(tmp);
    return brightness;
}

bool CUpnpCloudCamera::SetFullScreen(bool bFull)
{
    bool ret = true;
#ifdef PLATFORMHACHI
    ret = IPC_AVF_Client::getObject()->SetFullScreen(bFull);
#endif
    return ret;
}

bool CUpnpCloudCamera::isVideoFullScreen()
{
    bool ret = true;
#ifdef PLATFORMHACHI
    ret = IPC_AVF_Client::getObject()->isVideoFullScreen();
#endif
    return ret;
}

bool CUpnpCloudCamera::setOSDRotate(int rotate_type)
{
    bool ret = true;
#ifdef PLATFORMHACHI
    ret = IPC_AVF_Client::getObject()->setOSDRotate(rotate_type);
#endif
    return ret;
}

#ifdef PLATFORMHACHI

wifi_mode CUpnpCloudCamera::getWifiMode()
{
    int state = wifi_mode_Off;
    if (CTSDeviceManager::getInstance()) {
        state = CTSDeviceManager::getInstance()->getWifiMode();
    }
    return (wifi_mode)state;
}

#else

wifi_mode CUpnpCloudCamera::getWifiMode()
{
    wifi_mode state;
    char tmp[AGCMD_PROPERTY_SIZE];
    memset(tmp, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(WIFI_SWITCH_STATE, tmp, "NA");
    if(strcasecmp(tmp, "OFF") == 0) {
        state = wifi_mode_Off;
    } else {
        agcmd_property_get(wifiModePropertyName, tmp, "NA");
        //CAMERALOG("--CSDWirelessState:%s", tmp);
        if (strcasecmp(tmp, WIFI_AP_KEY) == 0) {
            state = wifi_mode_AP;
        } else if(strcasecmp(tmp, WIFI_CLIENT_KEY) == 0) {
            state = wifi_mode_Client;
        } else {
            state = wifi_mode_Off;
        }
    }

    return state;
}

#endif

wifi_State CUpnpCloudCamera::getWifiState()
{
    int rt = _getWifiState();
    wifi_State state;
    switch (rt)
    {
        case 0:
            state = wifi_State_prepare;
            break;
        case 1:
            state = wifi_State_ready;
            break;
        default:
            state = wifi_State_error;
            break;
    }
    return state;
}

int CUpnpCloudCamera::GetWifiAPinfor(char* pName, char* pPwd)
{
    char ssid_name_prop[AGCMD_PROPERTY_SIZE];
    char ssid_key_prop[AGCMD_PROPERTY_SIZE];

    agcmd_property_get("temp.earlyboot.ssid_name", ssid_name_prop, "");
    agcmd_property_get("temp.earlyboot.ssid_key", ssid_key_prop, "");

#if defined(PLATFORM22A)
    agcmd_property_get(ssid_name_prop, pName, "tsf_hd22a");
#elif defined(PLATFORMDIABLO)
    agcmd_property_get(ssid_name_prop, pName, "agb_diablo");
#elif defined(PLATFORMHELLFIRE)
    agcmd_property_get(ssid_name_prop, pName, "agb_hellfire");
#elif defined(PLATFORMHACHI)
    char bsp[32];
    agcmd_property_get(PropName_prebuild_bsp, bsp, "HACHI_V0C");
    // default wifi name is bsp name
    agcmd_property_get(ssid_name_prop, pName, bsp);
#else
    agcmd_property_get(ssid_name_prop, pName, "N/A");
#endif

    agcmd_property_get(ssid_key_prop, pPwd, "testAGDMC");

    return 0;
}

bool CUpnpCloudCamera::GetWifiCltInfor(char* pName, int len)
{
    bool ret = true;
    char wlan_prop[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];

    agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
    agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);

    agcmd_wpa_network_list_s networkList;
    memset(&networkList, 0x00, sizeof(networkList));
    agcmd_wpa_list_networks(wlan_prop, &networkList);
    if (networkList.valid_num <= 0) {
        snprintf(pName, len, "Empty List");
    } else if (networkList.valid_num > 32) {
        CAMERALOG("Obviously incorrect valid_num %d", networkList.valid_num);
        snprintf(pName, len, "Empty List");
    } else {
        int i = 0;
        for (i = 0; i < networkList.valid_num; i++) {
            //CAMERALOG("--ssid : %s, status: %s\n",
            //    networkList.info[i].ssid, networkList.info[i].flags);
            if (strcmp(networkList.info[i].flags, "[CURRENT]") == 0) {
                char prop_ip[128];
                snprintf(prop_ip, 128, "temp.%s.ip", prebuild_prop);
                //CAMERALOG("prop_ip = %s", prop_ip);
                char ip[128];
                agcmd_property_get(prop_ip, ip, "0.0.0.0");
                //CAMERALOG("ip = %s", ip);
                if (strcmp(ip, "0.0.0.0") != 0) {
                    snprintf(pName, len, "%s", networkList.info[i].ssid);
                } else {
                    snprintf(pName, len, "%s", "Connecting");
                }
                break;
            } else if (strcmp(networkList.info[i].flags, "[TEMP-DISABLED]") == 0) {
                snprintf(pName, len, "%s", networkList.info[i].ssid);
                ret = false;
                break;
            }
        }
        if (!(i < networkList.valid_num)) {
            snprintf(pName, len, "Searching");
        }
    }

    return ret;
}

int CUpnpCloudCamera::SetWifiMode(int m)
{
    int ret = 0;
    if (CTSDeviceManager::getInstance()) {
        ret = CTSDeviceManager::getInstance()->SetWifiMode(m);
    }

    return ret;
}

int CUpnpCloudCamera::GetWifiCltSignal()
{
    int ret = -100;
    if (CTSDeviceManager::getInstance()) {
        ret = CTSDeviceManager::getInstance()->GetWifiCltSignal();
    }
    return ret;
}

int CUpnpCloudCamera::GetSystemLanguage(char* language)
{
    agcmd_property_get(PropName_Language, language, "NONE");

    return 0;
}

int CUpnpCloudCamera::SetSystemLanguage(char* language)
{
    agcmd_property_set(PropName_Language, language);

    return 0;
}

bool CUpnpCloudCamera::MarkVideo() {
#if 0
#if defined(PLATFORM22A) || defined(PLATFORMHACHI)
    // A simple way to tell UI it is during Mark.
    //if (_pRecordService->isCarMode()) {
        int avf_state = 0;
        int is_still = 0;
        IPC_AVF_Client::getObject()->getRecordState(&avf_state, &is_still);
        if (State_Rec_recording == avf_state) {
            if (_bMarking) {
                if (CTimerThread::GetInstance()) {
                    CTimerThread::GetInstance()->CancelTimer(markTimer);
                }
            }
            _bMarking = true;
            if (CTimerThread::GetInstance()) {
                CTimerThread::GetInstance()->ApplyTimer
                    (100, CUpnpCloudCamera::markTimerCallback, this, false, markTimer);
            }
        }
    //}
#endif
#endif
    return IPC_AVF_Client::getObject()->markVideo();
}

bool CUpnpCloudCamera::MarkVideo(int before_sec, int after_sec)
{
    return IPC_AVF_Client::getObject()->markVideo(before_sec, after_sec);
}

bool CUpnpCloudCamera::MarkVideo(int before_sec, int after_sec,
    const char *data, int data_size)
{
    return IPC_AVF_Client::getObject()->markVideo(before_sec, after_sec,
        (const uint8_t *)data, data_size);
}

void CUpnpCloudCamera::stopMark()
{
    if (IPC_AVF_Client::getObject()) {
        IPC_AVF_Client::getObject()->stopMark();
    }
}

bool CUpnpCloudCamera::FactoryReset()
{
    return IPC_AVF_Client::getObject()->FactoryReset();
}

int CUpnpCloudCamera::getClientsNum()
{
    return IPC_AVF_Client::getObject()->getClients();
}

#ifdef PLATFORMHACHI

void CUpnpCloudCamera::ScanNetworks()
{
    if (CTSDeviceManager::getInstance()) {
        CTSDeviceManager::getInstance()->ScanNetworks();
    }
}

int CUpnpCloudCamera::EnableBluetooth(bool enable)
{
    int ret = 0;
    if (CTSDeviceManager::getInstance()) {
        ret = CTSDeviceManager::getInstance()->doEnableBT(enable);
    }
    return ret;
}

bool CUpnpCloudCamera::AddWifiNetwork(const char *json)
{
    bool ret = false;
    if (CTSDeviceManager::getInstance()) {
        ret = CTSDeviceManager::getInstance()->AddWifiNetwork(json);
    }
    return ret;
}

bool CUpnpCloudCamera::ConnectHost(char *ssid)
{
    bool ret = false;
    if (CTSDeviceManager::getInstance()) {
        ret = CTSDeviceManager::getInstance()->ConnectHost(ssid);
    }
    return ret;
}

bool CUpnpCloudCamera::ForgetHost(char *ssid)
{
    bool ret = false;
    if (CTSDeviceManager::getInstance()) {
        ret = CTSDeviceManager::getInstance()->ForgetHost(ssid);
    }
    return ret;
}

void CUpnpCloudCamera::ScanBTDevices()
{
    if (CTSDeviceManager::getInstance()) {
        CTSDeviceManager::getInstance()->doScan();
    }
}

int CUpnpCloudCamera::BindBTDevice(const char* mac, int type)
{
    int ret = 0;
    if (CTSDeviceManager::getInstance()) {
        ret = CTSDeviceManager::getInstance()->doBind(mac, (EBTType)type);
    }
    return ret;
}

int CUpnpCloudCamera::unBindBTDevice(int type)
{
    int ret = 0;
    if (CTSDeviceManager::getInstance()) {
        ret = CTSDeviceManager::getInstance()->doUnBind((EBTType)type);
    }
    return ret;
}

bool CUpnpCloudCamera::getOrientation(
    float &euler_heading,
    float &euler_roll,
    float &euler_pitch)
{
    bool ret = false;
    if (_sensors) {
        ret = _sensors->getOrientation(euler_heading, euler_roll, euler_pitch);
    }
    return ret;
}

bool CUpnpCloudCamera::getAccelXYZ(
    float &accel_x,
    float &accel_y,
    float &accel_z)
{
    bool ret = false;
    if (_sensors) {
        ret = _sensors->getAccelXYZ(accel_x, accel_y, accel_z);
    }
    return ret;
}

bool CUpnpCloudCamera::getSpeedKmh(int &speed)
{
    bool ret = false;
    if (_sensors) {
        ret = _sensors->getSpeedKmh(speed);
    }
    return ret;
}

bool CUpnpCloudCamera::getBoostKpa(int &pressure)
{
    bool ret = false;
    if (_sensors) {
        ret = _sensors->getBoostKpa(pressure);
    }
    return ret;
}

bool CUpnpCloudCamera::getRPM(int &rpm)
{
    bool ret = false;
    if (_sensors) {
        ret = _sensors->getRPM(rpm);
    }
    return ret;
}

bool CUpnpCloudCamera::getAirFlowRate(int &volume)
{
    bool ret = false;
    if (_sensors) {
        ret = _sensors->getAirFlowRate(volume);
    }
    return ret;
}

bool CUpnpCloudCamera::getEngineTemp(int &temperature)
{
    bool ret = false;
    if (_sensors) {
        ret = _sensors->getEngineTemp(temperature);
    }
    return ret;
}
#endif

void CUpnpCloudCamera::enableAutoMark(bool enable)
{
    if (CGsensorThread::getInstance()) {
        CGsensorThread::getInstance()->enableAutoMark(enable);
    }
}

void CUpnpCloudCamera::setSensitivity(gsensor_sensitive sens)
{
    if (CGsensorThread::getInstance()) {
        switch (sens) {
            case gsensor_sensitive_low:
                CGsensorThread::getInstance()->setSensitivity(
                    CGsensorThread::Gsensor_Sensitivity_Low);
                break;
            case gsensor_sensitive_middle:
                CGsensorThread::getInstance()->setSensitivity(
                    CGsensorThread::Gsensor_Sensitivity_Normal);
                break;
            case gsensor_sensitive_high:
                CGsensorThread::getInstance()->setSensitivity(
                    CGsensorThread::Gsensor_Sensitivity_High);
                break;
            default:
                break;
        }
    }
}


int CUpnpCloudCamera::_getWifiState()
{
    char value[AGCMD_PROPERTY_SIZE];
    memset(value, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get("temp.wifi.status", value, "");
    if (strcasecmp(value, "on") == 0) {
        int rt = _checkIpStatus();
        if  (rt == 0) {
            return 0;
        } else {
            return 1;
        }
    } else if (strcasecmp(value, "off") == 0) {
        return 2;
    } else {
        return 2;
    }
}

int CUpnpCloudCamera::_checkIpStatus()
{
    char wifi_prop[AGCMD_PROPERTY_SIZE];
    char ip_prop_name[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];

    memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(wifiStatusPropertyName, wifi_prop, "NA");
    if (strcasecmp(wifi_prop, "on") == 0) {
        // wifi is on, continue
    } else {
        return 0;
    }

    memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(wifiModePropertyName, wifi_prop, "NA");
    if (strcasecmp(wifi_prop, "AP") == 0) {
        memset(prebuild_prop, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get("prebuild.board.if_ap", prebuild_prop, "NA");

        memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get("temp.earlyboot.if_ap", wifi_prop, prebuild_prop);
    } else {
        memset(prebuild_prop, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");

        memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get("temp.earlyboot.if_wlan", wifi_prop, prebuild_prop);
    }

    snprintf(ip_prop_name, AGCMD_PROPERTY_SIZE, "temp.%s.ip", wifi_prop);
    memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(ip_prop_name, wifi_prop, "NA");

    UINT32 rt = inet_addr(wifi_prop);
    //CAMERALOG("%s = %s, ip = %d", ip_prop_name, wifi_prop, rt);
    if ((rt != (UINT32)-1) && (rt != 0)) {
        if(_CurrentIP != (int)rt) {
            _CurrentIP = rt;
            return 2;
        } else {
            return 1;
        }
    }

    // IP is not ready.
    return 0;
}

void CUpnpCloudCamera::_setBrightness(int value) {
    int _fb = open("/dev/fb0", O_RDWR);
    if (!_fb) {
        CAMERALOG("Error: cannot open framebuffer device.\n");
    } else {
        if (value > 252) {
            value = 252;
        } else if (value < 0) {
            value = 0;
        }
        ioctl(_fb, AGB_SSD1308_IOCTL_SET_CONTRAST, &value);
        char tmp[256] = {0x00};
        snprintf(tmp, 256, "%d", value);
        agcmd_property_set(PropName_Brighness, tmp);
        close(_fb);
    }
}

/* static */
void CUpnpCloudCamera::markTimerCallback(void *para) {
    CUpnpCloudCamera *cam = (CUpnpCloudCamera *)para;
    if (cam) {
        CAMERALOG("Mark finished");
        cam->_bMarking = false;
    }
}

