
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <getopt.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "class_camera_callback.h"

#include "Class_DeviceManager.h"
#include "class_ipc_rec.h"

#ifdef AVAHI_ENABLE
#include "class_avahi_thread.h"
#endif

extern void CameraDelayms(int time_ms);

using namespace rapidjson;

//-----------------------------------------------------------------------
//
//  CTSDeviceManager
//
//-----------------------------------------------------------------------

CTSDeviceManager* CTSDeviceManager::pInstance = NULL;
CTSDeviceManager::CTSDeviceManager()
    : CThread("CTSDeviceManager")
    , bRun(true)
    , mCmd(BTCmdNull)
    , pCB(NULL)
    , mBTStatus(BTStatus_OFF)
    , mOBDStatus(BTStatus_OFF)
    , mHIDStatus(BTStatus_OFF)
    , mScanStatus(BTScanStatus_IDLE)
    , mFormatResult(false)
    , _pCameraCallback(NULL)
    , _pWifiList(NULL)
    , _pBTList(NULL)
    , _last_wifi_signal(-100)
    , _bWaitingAddWifi(false)
    , _pWifiJson(NULL)
    , _ssidToForget(NULL)
{
    memset(&mCurDev, 0x00, sizeof(mCurDev));
    memset(&mBTScanList, 0x00, sizeof(mBTScanList));
    pCmdSem = new CSemaphore(0, 1, "BT Manager");
    pMutex = new CMutex("BT mutex");
}

CTSDeviceManager::~CTSDeviceManager()
{
    if (_pWifiJson) {
        delete [] _pWifiJson;
        _pWifiJson = NULL;
    }

    pCmdSem->Give();
    delete pMutex;
    delete pCmdSem;
    if (_pWifiList) {
        delete [] _pWifiList;
    }
    if (_pBTList) {
        delete [] _pBTList;
    }
    CAMERALOG("destroyed");
}

void CTSDeviceManager::Create(CameraCallback* pr) {
    if (CTSDeviceManager::pInstance == NULL) {
        CTSDeviceManager::pInstance = new CTSDeviceManager();
        CTSDeviceManager::pInstance->SetCameraCallback(pr);
        CTSDeviceManager::pInstance->Go();
    }
}

CTSDeviceManager* CTSDeviceManager::getInstance()
{
    return CTSDeviceManager::pInstance;
}

void CTSDeviceManager::releaseInstance()
{
    if (CTSDeviceManager::pInstance != NULL) {
        CTSDeviceManager::pInstance->Stop();
        delete CTSDeviceManager::pInstance;
        CTSDeviceManager::pInstance = NULL;
    }
}

EBTStatus CTSDeviceManager::getDevStatus(EBTType type)
{
    EBTStatus status = BTStatus_OFF;
    if (type == BTType_OBD) {
        status = mOBDStatus;
    }
    if (type == BTType_HID) {
        status = mHIDStatus;
    }
    //CAMERALOG("CTSDeviceManager::getDevStatus type: %d, state: %d", type, status);
    return status;
}
int CTSDeviceManager::doEnableBT(bool enable)
{
    if (isBusy()) {
        if (pCB && enable) {
            pCB->onBTEnabled(BTErr_TryLater);
        }
        if (pCB && !enable) {
            pCB->onBTDisabled(BTErr_TryLater);
        }
        CAMERALOG("doEnableBT later");
        return -1;
    }
    if ((mBTStatus == BTStatus_OFF) && !enable) {
        if (pCB) {
            pCB->onBTDisabled(BTErr_OK);
        }
        CAMERALOG("onBTDisabled no need");
        return 0;
    }
    if ((mBTStatus == BTStatus_ON) && enable) {
        if (pCB) {
            pCB->onBTEnabled(BTErr_OK);
        }
        CAMERALOG("onBTEnabled no need");
        return 0;
    }
    pMutex->Take();
    mCmd = enable ? BTCmdEnable : BTCmdDisable;
    pCmdSem->Give();
    pMutex->Give();
    return 0;
}

int CTSDeviceManager::getHostNum()
{
    if (mScanStatus != BTScanStatus_GotList) {
        return 0;
    }
    return mBTScanList.total_num;
}
int CTSDeviceManager::getHostInfor(int index, char** name, char** mac)
{
    if (mScanStatus != BTScanStatus_GotList) {
        return 0;
    }
    if (index >= mBTScanList.total_num) {
        return 0;
    }
    strcpy(*name, mBTScanList.result[index].name);
    strcpy(*mac, mBTScanList.result[index].mac);
    return 0;
}
void CTSDeviceManager::getBTList(char** list)
{
        Document d1;
        Document::AllocatorType& allocator = d1.GetAllocator();
        d1.SetObject();

        Value array(kArrayType);
        for (int i = 0; i < mBTScanList.total_num; i++) {
            Value object(kObjectType);

            Value jName(kStringType);
            jName.SetString(mBTScanList.result[i].name, allocator);
            object.AddMember("name", jName, allocator);

            Value jMac(kStringType);
            jMac.SetString(mBTScanList.result[i].mac, allocator);
            object.AddMember("mac", jMac, allocator);

            array.PushBack(object, allocator);
        }
        d1.AddMember("Devices", array, allocator);

        StringBuffer buffer;
        Writer<StringBuffer> write(buffer);
        d1.Accept(write);
        const char *str = buffer.GetString();
        CAMERALOG("strlen %d: %s", strlen(str), str);

        if (list != NULL) {
            *list = new char[strlen(str) + 1];
            strcpy(*list, str);
            //CAMERALOG("json len %d: %s", strlen(*json), *json);
        }
}
int CTSDeviceManager::doScan()
{
    if (mScanStatus == BTScanStatus_Scaning) {
        //if (pCB) pCB->onBTScanDone(BTErr_OK);
        if (_pCameraCallback) {
            _pCameraCallback->onCameraState(
                CameraCallback::CameraState_scanBT_result,
                (int)NULL);
        }
        return 0;
    }
    pMutex->Take();
    mCmd = BTCmdScan;
    pCmdSem->Give();
    pMutex->Give();
    return 0;
}
int CTSDeviceManager::doBind(const char* mac, EBTType type)
{
    CAMERALOG("doBind, %d %d %d, mac: %s, %d", mBTStatus, mOBDStatus, mHIDStatus, mac, type);
    if (isBusy() || (mBTStatus == BTStatus_OFF)) {
        if (pCB && (type == BTType_OBD)) {
            pCB->onBTOBDBinded(BTErr_TryLater);
        }
        if (pCB && (type == BTType_HID)) {
            pCB->onBTHIDBinded(BTErr_TryLater);
        }
        CAMERALOG("doBind, ret 1");
        return -1;
    }
    if ((type == BTType_OBD) && mOBDStatus != BTStatus_OFF) {
        if (pCB) {
            pCB->onBTOBDBinded(BTErr_TryLater);
        }
        CAMERALOG("doBind, ret 2");
        return -1;
    }
    if ((type == BTType_HID) && mHIDStatus != BTStatus_OFF) {
        if (pCB) {
            pCB->onBTHIDBinded(BTErr_TryLater);
        }
        CAMERALOG("doBind, ret 3");
        return -1;
    }
    pMutex->Take();
    strcpy(mCurDev.mac, mac);
    mCurDev.type = type;
    mCmd = BTCmdBind;
    pCmdSem->Give();
    pMutex->Give();
    return 0;
}
int CTSDeviceManager::doUnBind(EBTType type)
{
    CAMERALOG("doUnBind, %d %d %d, %d", mBTStatus, mOBDStatus, mHIDStatus, type);
    if (isBusy()) {
        if (pCB && (type == BTType_OBD)) {
            pCB->onBTOBDUnBinded(BTErr_TryLater);
        }
        if (pCB && (type == BTType_HID)) {
            pCB->onBTHIDUnBinded(BTErr_TryLater);
        }
        return -1;
    }
    if (type == BTType_OBD) {
        if (mOBDStatus == BTStatus_OFF) {
            agcmd_property_del(propOBDMac);
            agcmd_property_del(Prop_OBD_MAC_Backup);
            if (pCB) {
                pCB->onBTOBDUnBinded(BTErr_OK);
            }
            CAMERALOG("BTCmdUnBind: 1");
            return 0;
        }
        if (mOBDStatus == BTStatus_Busy) {
            if (pCB) {
                pCB->onBTOBDUnBinded(BTErr_TryLater);
            }
            CAMERALOG("BTCmdUnBind: 2");
            return -1;
        }
    }
    if (type == BTType_HID) {
        if (mHIDStatus == BTStatus_OFF) {
            agcmd_property_del(propHIDMac);
            agcmd_property_del(Prop_HID_MAC_Backup);
            agcmd_property_del(Prop_Disable_RC_Batt_Warning);
            if (pCB) {
                pCB->onBTHIDBinded(BTErr_OK);
            }
            CAMERALOG("BTCmdUnBind: 3");
            return 0;
        }
        if (mHIDStatus == BTStatus_Busy) {
            if (pCB) {
                pCB->onBTHIDBinded(BTErr_TryLater);
            }
            CAMERALOG("BTCmdUnBind: 4");
            return -1;
        }
    }
    mCurDev.type = type;
    pMutex->Take();
    mCmd = BTCmdUnBind;
    pCmdSem->Give();
    pMutex->Give();
    return 0;
}

void CTSDeviceManager::broadcastStorageState()
{
    pMutex->Take();
    mCmd = DeviceStorageState;
    pCmdSem->Give();
    pMutex->Give();
}

void CTSDeviceManager::broadcastClientsChanged()
{
    pMutex->Take();
    mCmd = DeviceClientsInfo;
    pCmdSem->Give();
    pMutex->Give();
}

int CTSDeviceManager::SetWifiMode(int mode, const char* ssid)
{
    const char *tmp_args[8];
    tmp_args[0] = "agsh";
    tmp_args[1] = "castle";

    char key[AGCMD_PROPERTY_SIZE];
    memset(key, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(WIFI_MODE_KEY, key, "NA");
    if ((strcasecmp(key, WIFI_MULTIROLE) != 0)) {
        // Firstly, change to multirole mode
        CAMERALOG("-- stop wifi");
        tmp_args[2] = "wifi_stop";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
        CameraDelayms(1000);

        CAMERALOG("-- set multirole mode and start wifi");
        agcmd_property_set(WIFI_MODE_KEY, WIFI_MULTIROLE);
        tmp_args[2] = "wifi_start";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
        CameraDelayms(1000);
    }

    char tmp[AGCMD_PROPERTY_SIZE];
    memset(tmp, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(WIFI_MR_MODE, tmp, "0");

    char status[AGCMD_PROPERTY_SIZE];
    memset(status, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(WIFI_SWITCH_STATE, status, "NA");

    //CAMERALOG("change wifi mode to %d", mode);
    switch (mode) {
        case Wifi_Mode_Off: // Off
            // CAMERALOG("-- stop wifi");
            tmp_args[2] = "wifi_stop";
            tmp_args[3] = NULL;
            agbox_sh(3, (const char *const *)tmp_args);
            break;
        case Wifi_Mode_AP:// AP
            if (strcasecmp(status, "ON") != 0) {
                tmp_args[2] = "wifi_start";
                tmp_args[3] = NULL;
                agbox_sh(3, (const char *const *)tmp_args);
            }
            if (strcasecmp(tmp, WIFI_AP_KEY) != 0) {
                tmp_args[2] = "wifi_multirole";
                tmp_args[3] = "AP";
                tmp_args[4] = NULL;
                agbox_sh(4, (const char *const *)tmp_args);
            }
            break;
        case Wifi_Mode_Client: // WLAN
            if (strcasecmp(status, "ON") != 0) {
                tmp_args[2] = "wifi_start";
                tmp_args[3] = NULL;
                agbox_sh(3, (const char *const *)tmp_args);
            }
            if (strcasecmp(tmp, WIFI_CLIENT_KEY) != 0) {
                tmp_args[2] = "wifi_multirole";
                tmp_args[3] = "WLAN";
                tmp_args[4] = NULL;
                agbox_sh(4, (const char *const *)tmp_args);
            }
            break;
        case Wifi_Mode_MultiRole: // AP + WLAN
            if (strcasecmp(status, "ON") != 0) {
                tmp_args[2] = "wifi_start";
                tmp_args[3] = NULL;
                agbox_sh(3, (const char *const *)tmp_args);
                CameraDelayms(1000);
            }
            if (strcasecmp(tmp, WIFI_MR_KEY) != 0) {
                tmp_args[2] = "wifi_multirole";
                tmp_args[3] = "AP+WLAN";
                tmp_args[4] = NULL;
                agbox_sh(4, (const char *const *)tmp_args);
            }
            break;
    }

    if (mode != Wifi_Mode_Client) {
        _last_wifi_signal = -100;
    }

    return 0;
}

int CTSDeviceManager::getWifiMode()
{
    int state = Wifi_Mode_Off;;
    char tmp[AGCMD_PROPERTY_SIZE];
    memset(tmp, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(WIFI_SWITCH_STATE, tmp, "NA");
    if(strcasecmp(tmp, "OFF") == 0) {
        state = Wifi_Mode_Off;
    } else {
        agcmd_property_get(wifiModePropertyName, tmp, "NA");
        if (strcasecmp(tmp, WIFI_AP_KEY) == 0) {
            state = Wifi_Mode_AP;
        } else if (strcasecmp(tmp, WIFI_CLIENT_KEY) == 0) {
            state = Wifi_Mode_Client;
        } else if (strcasecmp(tmp, WIFI_MULTIROLE) == 0) {
            // under multirole mode, 4 cases:
            //    AP+WLAN
            //    AP
            //    WLAN
            //    0 (no AP or WLAN)
            agcmd_property_get(WIFI_MR_MODE, tmp, "AP");
            if (strcasecmp(tmp, WIFI_AP_KEY) == 0) {
                state = Wifi_Mode_AP;
            } else if (strcasecmp(tmp, WIFI_CLIENT_KEY) == 0) {
                state = Wifi_Mode_Client;
            } else if (strcasecmp(tmp, WIFI_MR_KEY) == 0) {
                state = Wifi_Mode_MultiRole;
            } else {
                state = Wifi_Mode_Off;
            }
        } else {
            state = Wifi_Mode_Off;
        }
    }

    return state;
}

void CTSDeviceManager::ScanNetworks()
{
    pMutex->Take();
    mCmd = WifiCmdScan;
    //CAMERALOG("add WifiCmdScan");
    pCmdSem->Give();
    pMutex->Give();
}

void CTSDeviceManager::_scanWifiHotSpots()
{
#if defined(PLATFORMHACHI)
    //CAMERALOG("Enter");
    // firstly get wifi mode, if not in WLAN nor AP+WLAN, change to AP+WLAN
    int mode = Wifi_Mode_Off, counter = 0;
    int previous_mode = getWifiMode();
    if ((previous_mode != Wifi_Mode_Client)
        && (previous_mode != Wifi_Mode_MultiRole))
    {
        SetWifiMode(Wifi_Mode_MultiRole);
        do {
            CameraDelayms(1000);
            mode = getWifiMode();
            counter++;
        } while ((mode != Wifi_Mode_MultiRole) && (counter < 10));
    }

    //CAMERALOG("Begin scan hot spots...");

    char wlan_prop[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];

    agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
    agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);

    char *current_ssid = NULL;
    agcmd_wpa_network_list_s currentList;
    if ((previous_mode == Wifi_Mode_Client)
        || (previous_mode == Wifi_Mode_MultiRole))
    {
        memset(&currentList, 0x00, sizeof(currentList));
        agcmd_wpa_list_networks(wlan_prop, &currentList);
        if ((currentList.valid_num > 0) && (currentList.valid_num < 32)) {
            for (int i = 0; i < currentList.valid_num; i++) {
                if (strcmp(currentList.info[i].flags, "[CURRENT]") == 0) {
                    current_ssid = currentList.info[i].ssid;
                    //CAMERALOG("current_ssid = %s", current_ssid);
                    break;
                }
            }
        }
    }

    agcmd_wpa_list_info_s addedlist[256];
    int savedNumber = agcmd_wpa_list(wlan_prop, addedlist, 256);

    agcmd_wpa_network_scan_s networkList;
    memset(&networkList, 0x00, sizeof(networkList));
    agcmd_wpa_scan_networks(wlan_prop, &networkList);
    //CAMERALOG("networkList.valid_num = %d", networkList.valid_num);
    if (networkList.valid_num <= 0) {
        //CAMERALOG("Empty List");
    } else if (networkList.valid_num > 32) {
        CAMERALOG("Obviously incorrect valid_num %d", networkList.valid_num);
    } else {
        Document d1;
        Document::AllocatorType& allocator = d1.GetAllocator();
        d1.SetObject();

        Value array(kArrayType);
        for (int i = 0; i < networkList.valid_num; i++) {
            /*CAMERALOG("%d  ssid: %s  bssid: %s  flags: %s  frequence %d signal_level %d",
                i, networkList.info[i].ssid, networkList.info[i].bssid,
                networkList.info[i].flags, networkList.info[i].frequency,
                networkList.info[i].signal_level);*/
            if (strlen(networkList.info[i].ssid) <= 0) {
                continue;
            }
            Value object(kObjectType);

            Value jSsid(kStringType);
            jSsid.SetString(networkList.info[i].ssid, allocator);
            object.AddMember("ssid", jSsid, allocator);

            Value jBSsid(kStringType);
            jBSsid.SetString(networkList.info[i].bssid, allocator);
            object.AddMember("bssid", jBSsid, allocator);

            Value jFlags(kStringType);
            jFlags.SetString(networkList.info[i].flags, allocator);
            object.AddMember("flags", jFlags, allocator);

            object.AddMember("frequency", networkList.info[i].frequency, allocator);
            object.AddMember("signal_level", networkList.info[i].signal_level, allocator);

            bool added = false;
            for (int idx = 0; idx < savedNumber; idx++) {
                if (strcmp(networkList.info[i].ssid, addedlist[idx].ssid) == 0) {
                    added = true;
                    break;
                }
            }
            object.AddMember("added", added, allocator);

            bool current = false;
            if ((current_ssid != NULL) &&
                (strcmp(networkList.info[i].ssid, current_ssid) == 0))
            {
                current = true;
            }
            object.AddMember("current", current, allocator);

            array.PushBack(object, allocator);
        }
        d1.AddMember("networks", array, allocator);

        StringBuffer buffer;
        Writer<StringBuffer> write(buffer);
        d1.Accept(write);
        const char *str = buffer.GetString();
        //CAMERALOG("strlen %d: %s", strlen(str), str);

        if (pCB) {
            pCB->onWifiScanDone(str);
        }

        if (_pCameraCallback) {
            if (_pWifiList) {
                delete [] _pWifiList;
            }
            _pWifiList = new char [strlen(str) + 1];
            if (_pWifiList) {
                memset(_pWifiList, 0x00, strlen(str) + 1);
                strcpy(_pWifiList, str);
                _pCameraCallback->onCameraState(
                    CameraCallback::CameraState_scanWifi_result,
                    (int)_pWifiList);
            } else {
                CAMERALOG("Out of memory!");
            }
        }
    }

    // lastly, recover previous wifi mode
    mode = getWifiMode();
    if (mode != previous_mode) {
        SetWifiMode(previous_mode);
        counter = 0;
        do {
            CameraDelayms(1000);
            mode = getWifiMode();
            counter++;
        } while ((mode != previous_mode) && (counter < 10));
    }

    //CAMERALOG("Exit");
#endif
}

bool CTSDeviceManager::AddWifiNetwork(const char *json)
{
    if (!json) {
        return false;
    }

    bool ret = true;

    _pWifiJson = new char[strlen(json) + 1];
    if (_pWifiJson) {
        memset(_pWifiJson, 0x00, strlen(json) + 1);
        snprintf(_pWifiJson, strlen(json) + 1, "%s", json);
        _bWaitingAddWifi = true;
        pMutex->Take();
        mCmd = WifiCmdAddHost;
        pCmdSem->Give();
        pMutex->Give();
        ret = true;
    } else {
        CAMERALOG("Out of memory!");
        ret = false;
    }

    return ret;
}

bool CTSDeviceManager::_checkAndAddHost()
{
    bool ret = true;

    const char *ssid = NULL;
    const char *pwd = NULL;
    int is_hide = 0;

    Document d;

    if (!_pWifiJson) {
        ret = false;
        goto parse_error;
    }

    d.Parse(_pWifiJson);
    if(!d.IsObject()) {
        CAMERALOG("Didn't get a JSON object!");
        ret = false;
        goto parse_error;
    }

    ssid = d["ssid"].GetString();
    pwd = d["password"].GetString();
    is_hide = d["is_hide"].GetInt();
    CAMERALOG("ssid = %s, pwd = %s, is_hide = %d", ssid, pwd, is_hide);
    if ((strlen(pwd) > 0) && (strlen(pwd) < 8)) {
        ret = false;
        goto parse_error;
    }

    // Check whether the hidden ssid is correct:
    if (is_hide) {
        int previous_mode = getWifiMode();
        int mode = Wifi_Mode_Off, counter = 0;
        if ((previous_mode != Wifi_Mode_Client)
            && (previous_mode != Wifi_Mode_MultiRole))
        {
            SetWifiMode(Wifi_Mode_MultiRole);
            do {
                CameraDelayms(1000);
                mode = getWifiMode();
                counter++;
            } while ((mode != Wifi_Mode_MultiRole) && (counter < 10));
        }

        // Temperarily add to list:
        char wlan_prop[AGCMD_PROPERTY_SIZE];
        char prebuild_prop[AGCMD_PROPERTY_SIZE];
        agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
        agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
        //---------------
        // Note that after add to temp list, and at client mode, wifi maybe switched already!
        //---------------
        agcmd_wpa_add_network(wlan_prop, ssid, "hide", pwd);

        // If the hide ssid can be scan, it is a correct ssid, otherwise maybe spell error
        agcmd_wpa_network_scan_s networkList;
        bool found = false;
        counter = 0;
        do {
            memset(&networkList, 0x00, sizeof(networkList));
            agcmd_wpa_scan_networks(wlan_prop, &networkList);
            if (networkList.valid_num <= 0) {
                //CAMERALOG("Empty List");
            } else if (networkList.valid_num > 32) {
                CAMERALOG("Obviously incorrect valid_num %d", networkList.valid_num);
            } else {
                for (int i = 0; i < networkList.valid_num; i++) {
                    if (strcmp(networkList.info[i].ssid, ssid) == 0) {
                        found = true;
                    }
                }
            }
        } while (!found && (counter < 3));

        if (!found) {
            ret = false;

            // Recover wifi mode on failure, else keep multirole/client to finish next steps.
            SetWifiMode(previous_mode);
            counter = 0;
            do {
                CameraDelayms(1000);
                mode = getWifiMode();
                counter++;
            } while ((mode != previous_mode) && (counter < 10));
        } else {
            // If the hidden wifi is already connected, pop up a dialogue!
            if ((previous_mode == Wifi_Mode_Client) && _pCameraCallback) {
                _pCameraCallback->onCameraState(
                    CameraCallback::CameraState_Wifi_pwd_error,
                    1);
            }
        }
    }

parse_error:

    if (!ret) {
        delete [] _pWifiJson;
        _pWifiJson = NULL;
        _bWaitingAddWifi = false;
    }
    if (pCB) {
        pCB->onAddWifiResult(ret);
    }
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(
            CameraCallback::CameraState_Wifi_add_result,
            ret);
    }
    return ret;
}

bool CTSDeviceManager::_addWifiToTempWpa()
{
    const char *ssid = NULL;
    const char *pwd = NULL;
    int is_hide = 0;

    Document d;

    if (!_pWifiJson) {
        return false;
    }

    d.Parse(_pWifiJson);
    if(!d.IsObject()) {
        CAMERALOG("Didn't get a JSON object!");
        return false;
    }

    ssid = d["ssid"].GetString();
    pwd = d["password"].GetString();
    is_hide = d["is_hide"].GetInt();
    if ((strlen(pwd) > 0) && (strlen(pwd) < 8)) {
        return false;
    }

    char wlan_prop[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];
    agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
    agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);

    agcmd_wpa_network_list_s addedList;
    agcmd_wpa_list_networks(wlan_prop, &addedList);
    for (int i = 0; i < addedList.valid_num; i++) {
        if (strcmp(addedList.info[i].ssid, ssid) == 0) {
            return true;
        }
    }

    // Temperarily add to list:
    if ((pwd == NULL) ||
        (strcmp(pwd, "") == 0))
    {
        // add to buffered list, so that it can be connected immediately:
        CAMERALOG("Open SSID");
        agcmd_wpa_add_network(wlan_prop, ssid, "open", NULL);
    } else {
        if (strlen(pwd) < 8) {
            // password should be longer than 8
            return false;
        } else {
            // add to buffered list, so that it can be connected immediately:
            if (is_hide) {
                agcmd_wpa_add_network(wlan_prop, ssid, "hide", pwd);
            } else {
                agcmd_wpa_add_network(wlan_prop, ssid, "auto", pwd);
            }
        }
    }

    return true;
}

bool CTSDeviceManager::_addWifiToWpa()
{
    const char *ssid = NULL;
    const char *pwd = NULL;
    int is_hide = 0;

    Document d;

    if (!_pWifiJson) {
        return false;
    }

    d.Parse(_pWifiJson);
    if(!d.IsObject()) {
        CAMERALOG("Didn't get a JSON object!");
        return false;
    }

    ssid = d["ssid"].GetString();
    pwd = d["password"].GetString();
    is_hide = d["is_hide"].GetInt();
    CAMERALOG("ssid = %s, pwd = %s, is_hide = %d", ssid, pwd, is_hide);

    // Add to wpa list:
    char wlan_prop[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];
    agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
    agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
    if ((pwd == NULL) ||
        (strcmp(pwd, "") == 0))
    {
        CAMERALOG("Open SSID");
        agcmd_wpa_add(wlan_prop, ssid, "open", NULL);
    } else {
        if (strlen(pwd) >= 8) {
            if (is_hide) {
                agcmd_wpa_add(wlan_prop, ssid, "hide", pwd);
            } else {
                agcmd_wpa_add(wlan_prop, ssid, "auto", pwd);
            }
        }
    }

    return true;
}

bool CTSDeviceManager::ForgetHost(char *ssid)
{
    if (!ssid) {
        return false;
    }

    bool ret = true;

    if (_ssidToForget) {
        delete[] _ssidToForget;
    }

    _ssidToForget = new char[strlen(ssid) + 1];
    if (_ssidToForget) {
        memset(_ssidToForget, 0x00, strlen(ssid) + 1);
        snprintf(_ssidToForget, strlen(ssid) + 1, "%s", ssid);
        pMutex->Take();
        mCmd = WifiCmdForgetHost;
        CAMERALOG("add WifiCmdForgetHost");
        pCmdSem->Give();
        pMutex->Give();
        ret = true;
    } else {
        CAMERALOG("Out of memory!");
        ret = false;
    }

    return ret;
}

bool CTSDeviceManager::_forgetHost()
{
    int cur_mode = getWifiMode();
    if ((cur_mode != Wifi_Mode_Client)
        && (cur_mode != Wifi_Mode_MultiRole))
    {
        return false;
    }

    char wlan_prop[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];
    agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
    agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);

    bool ret = false;
    CAMERALOG("To remove network: %s", _ssidToForget);

    // firstly remove from temporary list
    agcmd_wpa_network_list_s addedList;
    agcmd_wpa_list_networks(wlan_prop, &addedList);
    int index = -1;
    for (int i = 0; i < addedList.valid_num; i++) {
        if (strcmp(addedList.info[i].ssid, _ssidToForget) == 0) {
            index = i;
            break;
        }
    }
    if (index >= 0) {
        //CAMERALOG("to remove %s with index %d from temporary list",
        //    _ssidToForget, addedList.info[index].id);
        int result = agcmd_wpa_remove_network(wlan_prop, addedList.info[index].id);
        if (result) {
            CAMERALOG("agcmd_wpa_remove_network with ret = %d", result);
        }
    }

    // secondly remove from wpa list
    int result = agcmd_wpa_del(wlan_prop, _ssidToForget);
    if (result) {
        CAMERALOG("agcmd_wpa_del with ret = %d", result);
        ret = false;
    } else {
        ret = true;
    }

    return ret;
}

bool CTSDeviceManager::ConnectHost(char *ssid)
{
    bool ret = true;

    if (strlen(ssid) < 128) {
        pMutex->Take();
        mCmd = WifiCmdConnect;
        strcpy(_ssid, ssid);
        pCmdSem->Give();
        pMutex->Give();
        ret = true;
    } else {
        CAMERALOG("too long ssid name(%d), not supported!", strlen(ssid));
        ret = false;
    }

    return ret;
}

bool CTSDeviceManager::_connectHotSpot()
{
    int previous_mode = getWifiMode();
    int mode = Wifi_Mode_Off, counter = 0;
    if ((previous_mode != Wifi_Mode_Client)
        && (previous_mode != Wifi_Mode_MultiRole))
    {
        SetWifiMode(Wifi_Mode_MultiRole);
        do {
            CameraDelayms(1000);
            mode = getWifiMode();
            counter++;
        } while ((mode != Wifi_Mode_MultiRole) && (counter < 10));
    }

    char wlan_prop[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];
    agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
    agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);

    char prop_ip[128];
    snprintf(prop_ip, 128, "temp.%s.ip", prebuild_prop);
    CAMERALOG("prop_ip = %s", prop_ip);
    char prev_ip[128];
    agcmd_property_get(prop_ip, prev_ip, "0.0.0.0");
    CAMERALOG("IP at the moment is %s", prev_ip);

    // Firstly get all added wifi and available at the moment
    agcmd_wpa_network_list_s addedList;
    agcmd_wpa_list_networks(wlan_prop, &addedList);

    // Secondly, find index of ssid
    bool ret = false;
    int index = -1;
    int previous_index = -1;
    CAMERALOG("hot spots found: addedList.valid_num = %d", addedList.valid_num);
    for (int i = 0; i < addedList.valid_num; i++) {
        //CAMERALOG("%dth found: id %d, ssid %s, bssid %s, flags %s",
        //    i, addedList.info[i].id, addedList.info[i].ssid,
        //    addedList.info[i].bssid, addedList.info[i].flags);
        if (strcmp(addedList.info[i].ssid, _ssid) == 0) {
            index = i;
            break;
        }
    }
    // Check whether it is the current wifi
    ret = false;
    if ((index >= 0) &&
        (strcmp(addedList.info[index].flags, "[CURRENT]") == 0))
    {
        if (strcmp(prev_ip, "0.0.0.0") != 0) {
            // already connected to the wifi:
            ret = true;
            if (_bWaitingAddWifi) {
                delete [] _pWifiJson;
                _pWifiJson = NULL;
                _bWaitingAddWifi = false;
            }
            goto connect_result;
        }
    }

    if (_bWaitingAddWifi) {
        _addWifiToTempWpa();
    }

    memset(&addedList, 0x00, sizeof(addedList));
    agcmd_wpa_list_networks(wlan_prop, &addedList);
    index = -1;
    previous_index = -1;
    for (int i = 0; i < addedList.valid_num; i++) {
        CAMERALOG("%dth found: id %d, ssid %s, bssid %s, flags %s",
            i, addedList.info[i].id, addedList.info[i].ssid,
            addedList.info[i].bssid, addedList.info[i].flags);
        if (strcmp(addedList.info[i].ssid, _ssid) == 0) {
            index = i;
        }
        if (strcmp(addedList.info[i].flags, "[CURRENT]") == 0) {
            // get the current ssid, if under client mode and switch wifi failed, recover to it
            previous_index = i;
            CAMERALOG("previous_ssid = %s", addedList.info[i].ssid);
        }
    }

    if (index >= 0) {
        CAMERALOG("Connect %dth: %s", addedList.info[index].id, _ssid);
        agcmd_wpa_select_network(wlan_prop, addedList.info[index].id);

        int num = 0;
        do {
            CameraDelayms(1000);
            num++;
            agcmd_wpa_network_list_s currentList;
            memset(&currentList, 0x00, sizeof(currentList));
            agcmd_wpa_list_networks(wlan_prop, &currentList);
            int i = 0;
            for (i = 0; i < currentList.valid_num; i++) {
                CAMERALOG("--ssid %dth: %s, status: %s",
                    i, currentList.info[i].ssid, currentList.info[i].flags);
                if (strcmp(_ssid, currentList.info[i].ssid) == 0) {
                    if ((strcmp(currentList.info[i].flags, "[CURRENT]") == 0) ||
                        (strcmp(currentList.info[i].flags, "[TEMP-DISABLED]") == 0))
                    {
                        break;
                    }
                }
            }
            if (i < currentList.valid_num) {
                if (strcmp(currentList.info[i].flags, "[CURRENT]") == 0) {
                    char ip[128];
                    agcmd_property_get(prop_ip, ip, "0.0.0.0");
                    CAMERALOG("ip = %s", ip);
                    if (strcmp(ip, "0.0.0.0") != 0) {
                        if (previous_mode == Wifi_Mode_Client) {
                            if (strcmp(ip, prev_ip) != 0)
                            {
                                ret = true;
                                break;
                            }
                        } else {
                            ret = true;
                            break;
                        }
                    }
                } else if (strcmp(currentList.info[i].flags, "[TEMP-DISABLED]") == 0) {
                    CAMERALOG("incorrect password for %s", _ssid);
                    ret = false;

                    if (_pCameraCallback) {
                        _pCameraCallback->onCameraState(
                            CameraCallback::CameraState_Wifi_pwd_error,
                            0);
                    }
                    break;
                }
            }
        } while (num <= 30);
    } else {
       CAMERALOG("%s is not found!", _ssid);
    }

connect_result:

    CAMERALOG("ret = %d", ret);

    if (pCB) {
        pCB->onHotspotConnected(ret);
    }
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(
            CameraCallback::CameraState_connectWifi_result,
            (int)ret);
    }
    // pop up add wifi success dialogue as soon as possible
    if (_bWaitingAddWifi && ret) {
        if (_pCameraCallback) {
            _pCameraCallback->onCameraState(
                CameraCallback::CameraState_Wifi_pwd_error,
                1);
        }
    }

    CameraDelayms(1000);

    // delay to change wpa list, in order to show password error in dialogue
    if (_bWaitingAddWifi) {
        if (ret) {
            _addWifiToWpa();
        } else {
            agcmd_wpa_remove_network(wlan_prop, addedList.info[index].id);
        }
        delete [] _pWifiJson;
        _pWifiJson = NULL;
        _bWaitingAddWifi = false;
    }

    int newmode = Wifi_Mode_Off;
    bool bRestartAvahi = false;
    if (ret){
        // change to wlan mode on success
        newmode = Wifi_Mode_Client;
        if (previous_mode != Wifi_Mode_Client) {
            bRestartAvahi = true;
        }
    } else {
        // recover previous mode on failure
        newmode = previous_mode;
    }

    SetWifiMode(newmode);
    counter = 0;
    do {
        CameraDelayms(1000);
        mode = getWifiMode();
        counter++;
    } while ((mode != newmode) && (counter < 10));

    // restart avahi after switching to client mode and connect successfully
    if (bRestartAvahi) {
#ifdef AVAHI_ENABLE
        if (CAvahiServerThread::getInstance()) {
            CAvahiServerThread::getInstance()->RestartAvahi();
        }
#endif
    }

    if (!ret && (previous_mode == Wifi_Mode_Client) && (previous_index >= 0)) {
        // under client mode and switch wifi failed, recover to previous wifi
        agcmd_wpa_select_network(wlan_prop, addedList.info[previous_index].id);
    }

    return ret;
}

int CTSDeviceManager::GetWifiCltSignal()
{
    pMutex->Take();
    mCmd = WifiGetCltSignal;
    pCmdSem->Give();
    pMutex->Give();

    //CAMERALOG("####### _last_wifi_signal = %d", _last_wifi_signal);
    return _last_wifi_signal;
}

bool CTSDeviceManager::_getWifiCltSigal()
{
    int mode = getWifiMode();
    if ((mode == Wifi_Mode_Client)
        || (mode == Wifi_Mode_MultiRole))
    {
        char wlan_prop[AGCMD_PROPERTY_SIZE];
        char prebuild_prop[AGCMD_PROPERTY_SIZE];

        agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
        agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);

        char *current_ssid = NULL;
        agcmd_wpa_network_list_s currentList;
        memset(&currentList, 0x00, sizeof(currentList));
        agcmd_wpa_list_networks(wlan_prop, &currentList);
        bool bConnected = false;
        if ((currentList.valid_num > 0) && (currentList.valid_num < 32)) {
            for (int i = 0; i < currentList.valid_num; i++) {
                if (strcmp(currentList.info[i].flags, "[CURRENT]") == 0) {
                    bConnected = true;
                    current_ssid = currentList.info[i].ssid;
                    //CAMERALOG("current_ssid = %s", current_ssid);
                    break;
                }
            }
        }
        if (!bConnected) {
            return false;
        }

        agcmd_wpa_network_scan_s networkList;
        memset(&networkList, 0x00, sizeof(networkList));
        agcmd_wpa_scan_networks(wlan_prop, &networkList);
        if (networkList.valid_num > 0) {
            for (int i = 0; i < networkList.valid_num; i++) {
                if (strcmp(networkList.info[i].ssid, current_ssid) == 0) {
                    _last_wifi_signal = networkList.info[i].signal_level;
                    //CAMERALOG("####### signal for %s = %d", current_ssid, _last_wifi_signal);
                }
            }
        }
    }

    return true;
}

int CTSDeviceManager::onBTStatus(int isOn)
{
    CAMERALOG("bt status change from %d to %d", mBTStatus, isOn);
    if ((mBTStatus == BTStatus_OFF) && (isOn == BTStatus_ON)) {
        char tmp[256];
        agcmd_property_get(Prop_HID_MAC_Backup, tmp, "NA");
        if (strcasecmp(tmp, "NA") != 0) {
            CAMERALOG("Recover HID device %s", tmp);
            agcmd_property_set(propHIDMac, tmp);
            agcmd_property_del(Prop_HID_MAC_Backup);
            mHIDStatus = BTStatus_Wait;
        }
        agcmd_property_get(Prop_OBD_MAC_Backup, tmp, "NA");
        if (strcasecmp(tmp, "NA") != 0) {
            CAMERALOG("Recover OBD device %s", tmp);
            agcmd_property_set(propOBDMac, tmp);
            agcmd_property_del(Prop_OBD_MAC_Backup);
            mOBDStatus = BTStatus_Wait;
        }
    }
    mBTStatus = isOn ? BTStatus_ON : BTStatus_OFF;
    return 0;
}
int CTSDeviceManager::onOBDStatus(int isOn)
{
    //CAMERALOG("OBD status change from %d to %d", mOBDStatus, isOn);
    if (isOn == 0) {
        switch (mOBDStatus) {
            case BTStatus_OFF:
                mOBDStatus = BTStatus_OFF;
                break;
            default: {
                char tmpMac[64];
                memset(tmpMac, 0x00, sizeof(tmpMac));
                agcmd_property_get(propOBDMac, tmpMac, "NA");
                if (strcasecmp(tmpMac, "NA") == 0) {
                    mOBDStatus = BTStatus_OFF;
                } else {
                    mOBDStatus = BTStatus_Wait;
                }
            }
                break;
        }
    } else {
        mOBDStatus = BTStatus_ON;
    }
    return 0;
}
int CTSDeviceManager::onHIDStatus(int isOn)
{
    //CAMERALOG("HID status change from %d to %d", mHIDStatus, isOn);
    if (isOn == 0) {
        switch (mHIDStatus) {
            case BTStatus_OFF:
                mHIDStatus = BTStatus_OFF;
                break;
            default: {
                char tmpMac[64];
                memset(tmpMac, 0x00, sizeof(tmpMac));
                agcmd_property_get(propHIDMac, tmpMac, "NA");
                if (strcasecmp(tmpMac, "NA") == 0) {
                    mHIDStatus = BTStatus_OFF;
                } else {
                    mHIDStatus = BTStatus_Wait;
                }
            }
                break;
        }
    } else {
        mHIDStatus = BTStatus_ON;
    }
    return 0;
}
void CTSDeviceManager::Stop()
{
    bRun = false;
    pCmdSem->Give();
    CThread::Stop();
}
void CTSDeviceManager::doGetBTStatus()
{
    char tmpStatus[64];
    memset(tmpStatus, 0x00, sizeof(tmpStatus));
    agcmd_property_get(propBTTempStatus, tmpStatus, "NA");
    if (strcasecmp(tmpStatus, "on") == 0) {
        mBTStatus = BTStatus_ON;
    } else {
        mBTStatus = BTStatus_OFF;
    }
}
void CTSDeviceManager::doGetOBDStatus()
{
    char tmpMac[64];
    memset(tmpMac, 0x00, sizeof(tmpMac));
    agcmd_property_get(propOBDMac, tmpMac, "NA");
    char tmpStatus[64];
    memset(tmpStatus, 0x00, sizeof(tmpStatus));
    agcmd_property_get(propBTTempOBDStatus, tmpStatus, "NA");
    if (strcasecmp(tmpStatus, "on") == 0) {
        mOBDStatus = BTStatus_ON;
    } else if (strcasecmp(tmpMac, "NA") == 0) {
        mOBDStatus = BTStatus_OFF;
    } else {
        mOBDStatus = BTStatus_Wait;
    }
}
void CTSDeviceManager::doGetHIDStatus()
{
    char tmpMac[64];
    memset(tmpMac, 0x00, sizeof(tmpMac));
    agcmd_property_get(propHIDMac, tmpMac, "NA");
    char tmpStatus[64];
    memset(tmpStatus, 0x00, sizeof(tmpStatus));
    agcmd_property_get(propBTTempHIDStatus, tmpStatus, "NA");
    if (strcasecmp(tmpStatus, "on") == 0) {
        mHIDStatus = BTStatus_ON;
        //pHIDListener = CTSBTHIDListener::getHIDListenerObject(_keyEventProcessor);
    } else if (strcasecmp(tmpMac, "NA") == 0) {
        mHIDStatus = BTStatus_OFF;
    } else {
        mHIDStatus = BTStatus_Wait;
    }
}
void CTSDeviceManager::main()
{
    doGetBTStatus();
    doGetOBDStatus();
    doGetHIDStatus();

    // initiate OBD status
    IPC_AVF_Client::getObject()->onOBDReady(mOBDStatus == BTStatus_ON);

    while(bRun) {
        DevMngCmd cmd;
        pCmdSem->Take(-1);
        pMutex->Take();
        cmd = mCmd;
        mCmd = BTCmdNull;
        pMutex->Give();
        switch (cmd) {
            case BTCmdEnable: {
                agcmd_property_set(propBTStatus, "on");
                CAMERALOG("BlueTooth Enable");
            }
            break;
            case BTCmdDisable: {
                CAMERALOG("BlueTooth disable:");
                // Firstly temporarily remember the device
                char tmp[256];
                agcmd_property_get(propHIDMac, tmp, "NA");
                if (strcasecmp(tmp, "NA") != 0) {
                    agcmd_property_set(Prop_HID_MAC_Backup, tmp);
                }
                agcmd_property_get(propOBDMac, tmp, "NA");
                if (strcasecmp(tmp, "NA") != 0) {
                    agcmd_property_set(Prop_OBD_MAC_Backup, tmp);
                }
                // Then change state
                agcmd_property_del(propOBDMac);
                agcmd_property_del(propHIDMac);
                int i = 0;
                do {
                    CameraDelayms(1000);
                    i++;
                } while (bRun && 
                    ((mOBDStatus != BTStatus_OFF) || (mHIDStatus != BTStatus_OFF)) && (i < 5));
                agcmd_property_set(propBTStatus, "off");
                CAMERALOG("BlueTooth disable 2");
            }
            break;
            case BTCmdScan: {
                mScanStatus = BTScanStatus_Scaning;
                struct agbt_scan_list_s bt_scan_list;
                int ret = agbt_scan_devices(&bt_scan_list, 10);
                CAMERALOG("BlueTooth Scan, Find: %d", bt_scan_list.total_num);
                mBTScanList.total_num = 0;
                for (int i = 0; i < bt_scan_list.total_num; i++) {
                    CAMERALOG("\t%s [%s]\n", bt_scan_list.result[i].name,
                        bt_scan_list.result[i].mac);
                    /* If do not need remember all devices name,
                    Better move to bind */
                    if (strlen (bt_scan_list.result[i].name) != 0) {
                        memcpy(&mBTScanList.result[mBTScanList.total_num], 
                            &bt_scan_list.result[i], sizeof(agbt_scan_result_s));
                        mBTScanList.total_num ++;
                        agbt_pin_set(bt_scan_list.result[i].mac,
                            bt_scan_list.result[i].name,
                            "0000");
                    }
                }
                mScanStatus = BTScanStatus_GotList;
                if (_pBTList) {
                    delete [] _pBTList;
                }
                getBTList(&_pBTList);
                if (pCB) {
                    pCB->onBTScanDone(ret ? BTErr_Err : BTErr_OK, _pBTList);
                }
                if (_pCameraCallback) {
                    _pCameraCallback->onCameraState(
                        CameraCallback::CameraState_scanBT_result,
                        (int)_pBTList);
                }
            }
            break;
            case BTCmdBind: {
                if (mCurDev.type == BTType_OBD) {
                    if (mOBDStatus != BTStatus_OFF) {
                        if (pCB) {
                            pCB->onBTOBDBinded(BTErr_TryLater);
                        }
                        if (_pCameraCallback) {
                            int result = (BTType_OBD << 16) | BTErr_TryLater;
                            _pCameraCallback->onCameraState(
                                CameraCallback::CameraState_bindBT_result,
                                result);
                        }
                        break;
                    }
                    agcmd_property_set(propOBDMac, mCurDev.mac);
                    CAMERALOG("bind OBD: %s\n", mCurDev.mac);
                    mOBDStatus = BTStatus_Wait;
                    if (pCB) {
                        pCB->onBTOBDBinded(BTErr_OK);
                    }
                    if (_pCameraCallback) {
                        int result = (BTType_OBD << 16) | BTErr_OK;
                        _pCameraCallback->onCameraState(
                            CameraCallback::CameraState_bindBT_result,
                            result);
                    }
                }
                if (mCurDev.type == BTType_HID) {
                    if (mHIDStatus != BTStatus_OFF) {
                        if (pCB) {
                            pCB->onBTHIDBinded(BTErr_TryLater);
                        }
                        if (_pCameraCallback) {
                            int result = (BTType_HID << 16) | BTErr_TryLater;
                            _pCameraCallback->onCameraState(
                                CameraCallback::CameraState_bindBT_result,
                                result);
                        }
                        break;
                    }
                    agcmd_property_set(propHIDMac, mCurDev.mac);
                    CAMERALOG("bind HID: %s\n", mCurDev.mac);
                    mHIDStatus = BTStatus_Wait;
                    if (pCB) {
                        pCB->onBTHIDBinded(BTErr_OK);
                    }
                    if (_pCameraCallback) {
                        int result = (BTType_HID << 16) | BTErr_OK;
                        _pCameraCallback->onCameraState(
                            CameraCallback::CameraState_bindBT_result,
                            result);
                    }
                }
            }
            break;
            case BTCmdUnBind: {
                CAMERALOG("BTCmdUnBind:");
                if (mCurDev.type == BTType_OBD) {
                    if ((mOBDStatus != BTStatus_ON) && (mOBDStatus != BTStatus_Wait)) {
                        if (pCB) {
                            pCB->onBTOBDUnBinded(BTErr_OK);
                        }
                        break;
                    }
                    agcmd_property_del(propOBDMac);
                    agcmd_property_del(Prop_OBD_MAC_Backup);
                    CAMERALOG("unbind OBD: %s", mCurDev.mac);
                    if (mOBDStatus == BTStatus_ON) {
                        mOBDStatus = BTStatus_Busy;
                    } else {
                        mOBDStatus = BTStatus_OFF;
                    }
                    if (pCB) {
                        pCB->onBTOBDUnBinded(BTErr_OK);
                    }
                }
                if (mCurDev.type == BTType_HID) {
                    if ((mHIDStatus != BTStatus_ON) && (mHIDStatus != BTStatus_Wait)) {
                        if (pCB) {
                            pCB->onBTHIDUnBinded(BTErr_OK);
                        }
                        break;
                    }
                    agcmd_property_del(propHIDMac);
                    agcmd_property_del(Prop_HID_MAC_Backup);
                    agcmd_property_del(Prop_Disable_RC_Batt_Warning);
                    CAMERALOG("unbind HID: %s", mCurDev.mac);
                    if (mHIDStatus == BTStatus_ON) {
                        mHIDStatus = BTStatus_Busy;
                    } else {
                        mHIDStatus = BTStatus_OFF;
                    }
                    if (pCB) {
                        pCB->onBTHIDUnBinded(BTErr_OK);
                    }
                }
            }
            break;
            case DeviceStorageState: {
                if (pCB) {
                    pCB->sendStorageState();
                }
            }
            break;
            case DeviceClientsInfo:
                if (pCB) {
                    pCB->sendClientsInfo();
                }
            break;
            case WifiCmdScan:
                _scanWifiHotSpots();
                break;
            case WifiCmdAddHost:
                _checkAndAddHost();
                break;
            case WifiCmdForgetHost:
                _forgetHost();
                break;
            case WifiCmdConnect:
                _connectHotSpot();
                break;
            case WifiGetCltSignal:
                _getWifiCltSigal();
                break;
            default:
                break;
        }
    }
}

int CTSDeviceManager::findToBind()
{
    int ret = -1;
    for (int i = 0; i < mBTScanList.total_num; i++) {
        if (strcmp(mBTScanList.result[i].mac, mCurDev.mac) == 0) {
            ret = 0;
            break;
        }
    }
    return ret;
}
bool CTSDeviceManager::isBusy()
{
    return (mBTStatus == BTStatus_Busy) || (mOBDStatus == BTStatus_Busy) || (mHIDStatus == BTStatus_Busy);
}

