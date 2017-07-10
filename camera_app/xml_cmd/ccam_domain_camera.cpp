/*****************************************************************************
 * ccam_domain_camera.cpp
 *****************************************************************************
 * Author: linnsong <llsong@cam2cloud.com>
 *
 * Copyright (C) 1975 - 2014, linnsong.
 *
 *
 *****************************************************************************/
#include "ccam_domain_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CLanguage.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"
#include "agobd.h"
#include <math.h>

static cameraEventGenerator* pCEGobj = NULL;
static CTSUpgrader* pUpgobj = NULL;
static CTSDeviceManager* pDeviceManager = NULL;
extern void getStoragePropertyName(char *prop, int len);
extern void DestroyUpnpService(bool bReboot);

static int savedNumber = 0;
#define MAX_AP_NUMBER 256
#define MAX_VERSION_LENTH 64
static agcmd_wpa_list_info_s AP_List[MAX_AP_NUMBER];
extern int agclkd_set_rtc(time_t utc);

using namespace rapidjson;


SCMD_DOMIAN_CONSTRUCTOR(DomainCamera, CMD_Domain_cam, CMD_Cam_num)
{
    pCEGobj = new cameraEventGenerator(this);
    pDeviceManager = CTSDeviceManager::getInstance();
    if (pDeviceManager) {
        pDeviceManager->AddBTManagerCB(pCEGobj);
    }
    SCMD_CMD_NEW(GetMode, this);
    SCMD_CMD_NEW(GetName, this);
    SCMD_CMD_NEW(SetName, this);

    SCMD_CMD_NEW(GetAPIVersion, this);
    SCMD_CMD_NEW(IsAPISupported, this);

    SCMD_CMD_NEW(GetState, this);
    SCMD_CMD_NEW(StartRecord, this);
    SCMD_CMD_NEW(StopRecord, this);
    SCMD_CMD_NEW(GetTime, this);
    SCMD_CMD_NEW(SetStreamSize, this);

    SCMD_CMD_NEW(GetAllInfor, this);
    SCMD_CMD_NEW(GetStorageInfor, this);
    SCMD_CMD_NEW(MsgBatteryInfor, this);

    SCMD_CMD_NEW(Poweroff, this);
    SCMD_CMD_NEW(Reboot, this);

    SCMD_CMD_NEW(GetWlanMode, this);
    SCMD_CMD_NEW(GetHostNum, this);
    SCMD_CMD_NEW(GetHostInfor, this);
    SCMD_CMD_NEW(AddHost, this);
    SCMD_CMD_NEW(RmvHost, this);
    SCMD_CMD_NEW(ConnectHost, this);

    SCMD_CMD_NEW(SyncTime, this);
    SCMD_CMD_NEW(GetDeviceTime, this);
    SCMD_CMD_NEW(SetMic, this);
    SCMD_CMD_NEW(GetMicState, this);

    SCMD_CMD_NEW(GetVersion, this);
    SCMD_CMD_NEW(NewVersion, this);
    SCMD_CMD_NEW(DoUpgrade, this);

    //1.2
    SCMD_CMD_NEW(isBTSupported, this);
    SCMD_CMD_NEW(isBTEnabled, this);
    SCMD_CMD_NEW(BTEnable, this);
    SCMD_CMD_NEW(GetBTDevStatus, this);
    SCMD_CMD_NEW(GetBTHostNum, this);
    SCMD_CMD_NEW(GetBTHostInfor, this);
    SCMD_CMD_NEW(DoScanBT, this);
    SCMD_CMD_NEW(DoBindBT, this);
    SCMD_CMD_NEW(DoUnBindBT, this);
    SCMD_CMD_NEW(DoSetOBDTypes, this);
    //SCMD_CMD_NEW(BTReserved, this);

    //1.3
    SCMD_CMD_NEW(FormatTF, this);
    SCMD_CMD_NEW(GetLanguageList, this);
    SCMD_CMD_NEW(GetLanguage, this);
    SCMD_CMD_NEW(SetLanguage, this);
    SCMD_CMD_NEW(SetWorkingMode, this);
    SCMD_CMD_NEW(GetWorkingMode, this);
    SCMD_CMD_NEW(SetPhotoLapseInterval, this);
    SCMD_CMD_NEW(GetPhotoLapseInterval, this);
    // end of API 1.3

    // 1.4
    SCMD_CMD_NEW(ScanNetworks, this);
    SCMD_CMD_NEW(ConnectHotSpot, this);
    SCMD_CMD_NEW(SetUdcPowerDelay, this);
    SCMD_CMD_NEW(GetUdcPowerDelay, this);
    SCMD_CMD_NEW(SetSpeakerStatus, this);
    SCMD_CMD_NEW(GetSpeakerStatus, this);
    SCMD_CMD_NEW(SetAutoBrightness, this);
    SCMD_CMD_NEW(GetAutoBrightness, this);
    SCMD_CMD_NEW(SetDisplayBrightness, this);
    SCMD_CMD_NEW(GetDisplayBrightness, this);
    SCMD_CMD_NEW(SetDisplayOffTime, this);
    SCMD_CMD_NEW(GetDisplayOffTime, this);
    SCMD_CMD_NEW(FactoryReset, this);

    SCMD_CMD_NEW(GetClientsInfo, this);
    SCMD_CMD_NEW(AddConnection, this);
    SCMD_CMD_NEW(RemoveConnection, this);
    // end of API 1.4

    // 1.5
    SCMD_CMD_NEW(GetOBDVIN, this);
    SCMD_CMD_NEW(SetVINStyle, this);
    SCMD_CMD_NEW(GetVINStyle, this);
    SCMD_CMD_NEW(SetScreenSaverStyle, this);
    SCMD_CMD_NEW(GetScreenSaverStyle, this);
    SCMD_CMD_NEW(SetUnitsSystem, this);
    SCMD_CMD_NEW(GetUnitsSystem, this);
    // end of API 1.5
}

SCMD_DOMIAN_DISTRUCTOR(DomainCamera)
{
    pDeviceManager->releaseInstance();
    delete pCEGobj;
}

SCMD_CMD_EXECUTE(Reboot)
{
    CAMERALOG("Reboot command from APP!");
    DestroyUpnpService(true);
    return 0;
}
SCMD_CMD_EXECUTE(Poweroff)
{
    CAMERALOG("Poweroff command from APP!");
    DestroyUpnpService(false);
    return 0;
}

SCMD_CMD_EXECUTE(ConnectHost)
{
    EnumedStringCMD* cmd = p;
    int mode = atoi(cmd->getPara1());
    if (pDeviceManager) {
        pDeviceManager->SetWifiMode(mode, cmd->getPara2());
    }
    return 0;
}

SCMD_CMD_EXECUTE(RmvHost)
{
    EnumedStringCMD* cmd = p;

    char wlan_prop[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];

    agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
    agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
    agcmd_wpa_del(wlan_prop, cmd->getPara1());
    savedNumber = agcmd_wpa_list(wlan_prop, AP_List, MAX_AP_NUMBER);

    char tmp1[] = "0";
    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_RmvHost, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(AddHost)
{
    EnumedStringCMD* cmd = p;
    CAMERALOG("add host : %s, pwd: %s", cmd->getPara1(), cmd->getPara2());

    //char *json = (char*)"{\"ssid\":\"aidegongyang\", \"password\":\"dajiba99\", \"is_hide\":1}";
    char *json = (char*)cmd->getPara1();
    bool bAllocated = false;
    const char *pwd = NULL;

    Document d;

    if (!json) {
        //goto parse_error;
    }

    d.Parse(json);
    if(!d.IsObject()) {
        CAMERALOG("Didn't get a JSON object!");
        int len = strlen(cmd->getPara1()) + strlen(cmd->getPara2()) + 40;
        json = new char [len];
        if (json) {
            snprintf(json, len, "{\"ssid\":\"%s\", \"password\":\"%s\", \"is_hide\":0}",
                cmd->getPara1(), cmd->getPara2());
            CAMERALOG("json = %s", json);
            bAllocated = true;
            pwd = cmd->getPara2();
        }
    } else {
        CAMERALOG("%s is a JSON object!", json);
        pwd = d["password"].GetString();
    }

    bool ret = true;
    if ((strlen(pwd) > 0) && (strlen(pwd) < 8)) {
        // password should be longer than 8
        ret = false;
    } else {
        if (CTSDeviceManager::getInstance()) {
            ret = CTSDeviceManager::getInstance()->AddWifiNetwork(json);
        }
    }

    if (bAllocated) {
        delete [] json;
    }

    if (!ret) {
        EnumedStringCMD result(CMD_Domain_cam, CMD_Network_AddHost, (char*)"0", NULL);
        StringCMD *pp = &result;
        StringEnvelope ev(&pp, 1);
        if (cmd != NULL) {
            cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
        }
    }

    return 0;
}

SCMD_CMD_EXECUTE(GetHostNum)
{
    EnumedStringCMD* cmd = p;
    char wlan_prop[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];

    agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
    agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
    savedNumber = agcmd_wpa_list(wlan_prop, AP_List, MAX_AP_NUMBER);
    //CAMERALOG("-- saved number:%d", savedNumber);
    char tmp[64];
    char tmp1[64];
    snprintf(tmp, 64, "%d", savedNumber);
    snprintf(tmp1, 64, "%d", MAX_AP_NUMBER);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_GetHostNum, tmp, tmp1);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetHostInfor)
{
    EnumedStringCMD* cmd = p;
    int index = atoi(cmd->getPara1());
    char* tmp;
    if (index < MAX_AP_NUMBER) {
        tmp = AP_List[index].ssid;
    } else {
        tmp = (char*)"out max access number";
    }
    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_GetHostInfor, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

#ifdef PLATFORMHACHI
SCMD_CMD_EXECUTE(GetWlanMode)
{
    EnumedStringCMD* cmd = p;
    int mode = Wifi_Mode_Off;
    if (CTSDeviceManager::getInstance()) {
        mode = CTSDeviceManager::getInstance()->getWifiMode();
    }

    char tmp[8];
    snprintf(tmp, 8, "%d", mode);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_GetWLanMode, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}
#else
SCMD_CMD_EXECUTE(GetWlanMode)
{
    EnumedStringCMD* cmd = p;
    int mode = 2;
    char tmp[AGCMD_PROPERTY_SIZE];
    agcmd_property_get(WIFI_MODE_KEY, tmp, "");
    if (strcasecmp(tmp, WIFI_AP_KEY) == 0) {
        mode = 0;
    } else if (strcasecmp(tmp, WIFI_CLIENT_KEY) == 0) {
        mode = 1;
    }
    snprintf(tmp, AGCMD_PROPERTY_SIZE, "%d", mode);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_GetWLanMode, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}
#endif

SCMD_CMD_EXECUTE(ScanNetworks)
{
    //EnumedStringCMD* cmd = p;

    if (pDeviceManager) {
        pDeviceManager->ScanNetworks();
    }

    return 0;
}

SCMD_CMD_EXECUTE(ConnectHotSpot)
{
    EnumedStringCMD* cmd = p;

    char *ssid = (char *)cmd->getPara1();
    if (pDeviceManager) {
        pDeviceManager->ConnectHost(ssid);
    }

    return 0;
}

SCMD_CMD_EXECUTE(SetUdcPowerDelay)
{
    EnumedStringCMD* cmd = p;
    agcmd_property_set(PropName_Udc_Power_Delay, cmd->getPara1());

    char tmp1[16];
    snprintf(tmp1, 16, "%d", true);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Set_Auto_Power_Off_Delay, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetUdcPowerDelay)
{
    EnumedStringCMD* cmd = p;

    char delay_sec[AGCMD_PROPERTY_SIZE];
    memset(delay_sec, 0x00, sizeof(delay_sec));
    agcmd_property_get(PropName_Udc_Power_Delay, delay_sec, Udc_Power_Delay_Default);

    EnumedStringCMD result(CMD_Domain_cam, CMD_Get_Auto_Power_Off_Delay, delay_sec, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(SyncTime)
{
    EnumedStringCMD* cmd = p;
    //CSDSystemTime* pSystemTime = (CSDSystemTime*)p[0];
    //CSDSystemTimeZone* pSystemTimeZone = (CSDSystemTimeZone*)_ppInStates[1];;
    //pSystemTime->setWishAsCurrent();
    //pSystemTimeZone->setWishAsCurrent();
    CAMERALOG("-- set time %s, time zone %s", cmd->getPara1(), cmd->getPara2());
    int minuteswest = atof(cmd->getPara2()) * 60;
    time_t t, tRemote = 0;
    time(&t);
    tRemote = (time_t)atoi(cmd->getPara1());
    //CAMERALOG("---system time: %d, remote time:%d \n, timezone : %f",
    //    t, tRemote, pSystemTimeZone->getWish());
    //if (fabs(difftime(t, tRemote)) > 120.0) {
    if (fabs(difftime(t, tRemote)) > 10.0)
    {
        struct timeval tt;
        tt.tv_sec = tRemote;
        tt.tv_usec = 0;
        struct timezone tz;
        tz.tz_minuteswest = minuteswest;
        tz.tz_dsttime = 0;
        settimeofday(&tt, &tz);
        agclkd_set_rtc(tRemote);
    }
    {
        char tmp[256];
        memset(tmp, 0x00, sizeof(tmp));
        if ((minuteswest % 60) != 0) {
            snprintf(tmp, 256, "%sGMT%+d_%02d", timezongDir,
                minuteswest / 60, (minuteswest > 0 ? minuteswest : -minuteswest) % 60);
        } else {
            snprintf(tmp, 256, "%sGMT%+d", timezongDir, minuteswest / 60);
        }
        int rt = remove(timezongLink);
        if (rt < 0) {
            CAMERALOG("remove(timezongLink) failed!");
        }
        rt = symlink(tmp, timezongLink);
        if (rt < 0) {
            CAMERALOG("symlink(tmp, timezongLink) failed!");
        }
    }

    char tmp1[] = "0";
    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_Synctime, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetDeviceTime)
{
    EnumedStringCMD* cmd = p;

    char tmp1[128];
    time_t now_time;
    // 精确到秒的当前距离1970-01-01 00:00:00 +0000 (UTC)的秒数
    time(&now_time);
    snprintf(tmp1, 128, "%ld", now_time);

    char tmp2[128];
    // UTC和本地时间之间的时差,单位为秒
    snprintf(tmp2, 128, "%ld", timezone);

    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_GetDevicetime, tmp1, tmp2);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetMic)
{
    EnumedStringCMD* cmd = p;
    int bMute = atoi(cmd->getPara1());

#ifdef PLATFORMHACHI
    int volume  = atoi(cmd->getPara2());
    if (volume < 0 || volume > 10) {
        volume = atoi(DEFAULT_MIC_VOLUME);
    }

    bool handled = false;
    char tmp[256];
    agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
    if (bMute) {
        if ((strcasecmp(tmp, "Normal") == 0) ||
            (strcasecmp(tmp, "Manual") == 0))
        {
            // if it is unmute, change to mute
            agcmd_property_get(Prop_Mic_Volume, tmp, DEFAULT_MIC_VOLUME);
            if (strcasecmp(tmp, "0") == 0) {
                snprintf(tmp, sizeof(tmp), "%s", DEFAULT_MIC_VOLUME);
            }
            agcmd_property_set(Settings_Mic_Volume, tmp);
            IPC_AVF_Client::getObject()->SetMicVolume(0);
            handled = true;
        }
    } else {
        if (strcasecmp(tmp, "Mute") == 0) {
            // if it is mute, change to unmute
            agcmd_property_get(Settings_Mic_Volume, tmp, DEFAULT_MIC_VOLUME);
            IPC_AVF_Client::getObject()->SetMicVolume(
                (atoi(tmp) == 0) ? atoi(DEFAULT_MIC_VOLUME) : atoi(tmp));
            handled = true;
        }
    }

    // support mic volume change since version 1.6.05
    agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
    if (!handled &&
        ((strcasecmp(tmp, "Normal") == 0) ||
        (strcasecmp(tmp, "Manual") == 0)))
    {
        IPC_AVF_Client::getObject()->SetMicVolume(volume);
        if (volume == 0) {
            snprintf(tmp, sizeof(tmp), "%s", DEFAULT_MIC_VOLUME);
        }
        agcmd_property_set(Settings_Mic_Volume, tmp);
    }
#else
    int gain  = atoi(cmd->getPara2());
    if (bMute) {
        IPC_AVF_Client::getObject()->SetMicVolume(0);
    } else {
        if (gain <=0 || gain >= 9) {
            gain = defaultMicGain;
        }
        IPC_AVF_Client::getObject()->SetMicVolume(gain);
    }
#endif
    char tmp1[] = "1";
    EnumedStringCMD result(CMD_Domain_cam, CMD_audio_setMic, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetMicState)
{
    EnumedStringCMD* cmd = p;
    cameraEventGenerator::SendAudioState(cmd,NULL);
    return 0;
}

SCMD_CMD_EXECUTE(GetVersion)
{
    EnumedStringCMD* cmd = p;

    char HW_Version[AGCMD_PROPERTY_SIZE] = "";
    char Device_SN[AGCMD_PROPERTY_SIZE] = "";
    char FW_Version[AGCMD_PROPERTY_SIZE] = "";
    agcmd_property_get(PropName_prebuild_bsp, HW_Version, "HACHI_V0C");
    agcmd_property_get(PropName_SN_SSID, Device_SN, "NA");
    agcmd_property_get("prebuild.system.version", FW_Version, "NA");
    strcat(Device_SN, "@");
    strcat(Device_SN, HW_Version);
    //CAMERALOG("-- saved number:%d", savedNumber);
    EnumedStringCMD result(CMD_Domain_cam, CMD_fw_getVersion, Device_SN, FW_Version);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(NewVersion)
{
    EnumedStringCMD* cmd = p;
    CAMERALOG("NewVersion: %s\n\t%s", cmd->getPara1(),  cmd->getPara2());
    pUpgobj = CTSUpgrader::getInstance(cmd->getPara1(), pCEGobj);
    return 0;
}

SCMD_CMD_EXECUTE(DoUpgrade)
{
    CAMERALOG("DoUpgrade command from APP, reboot to finish upgrading!");
    DestroyUpnpService(true);
    return 0;
}

//1.2
SCMD_CMD_EXECUTE(isBTSupported)
{
    EnumedStringCMD* cmd = p;
    char tmp1[AGCMD_PROPERTY_SIZE];
    char rt[8] = "0";
    memset(tmp1, 0x00, sizeof(tmp1));
    agcmd_property_get(propIsBTSupported, tmp1, "NA");
    CAMERALOG("get BT support: %s", tmp1);
    if (strcasecmp(tmp1, "1") == 0) {
        snprintf(rt, 8, "%d", 1);
    } else {
        snprintf(rt, 8, "%d", 0);
    }
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_isSupported, rt, NULL);
    StringCMD* pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }

    return 0;
}
SCMD_CMD_EXECUTE(isBTEnabled)
{
    EnumedStringCMD* cmd = p;
    char rt[8] = "0";
    snprintf(rt, 8, "%d", pDeviceManager->getBTStatus());
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_isEnabled, rt, NULL);
    StringCMD* pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }

    return 0;
}
SCMD_CMD_EXECUTE(BTEnable)
{
    if (pDeviceManager == NULL) {
        return -1;
    }
    EnumedStringCMD* cmd = p;
    if (strcmp("1", cmd->getPara1()) == 0) {
        pDeviceManager->doEnableBT(true);
    } else {
        pDeviceManager->doEnableBT(false);
    }
    return 0;
}
SCMD_CMD_EXECUTE(GetBTDevStatus)
{
    if (pDeviceManager == NULL) {
        return -1;
    }
    char status[8] = "";
    char rt[64] = "";
    char tmp1[AGCMD_PROPERTY_SIZE];
    char bdname_str[AGBT_MAX_NAME_SIZE];
    char bdpin_str[AGBT_MAX_PIN_SIZE];
    EnumedStringCMD* cmd = p;
    EBTType eBTType = (EBTType)atoi(cmd->getPara1());
    if (eBTType == BTType_OBD) {
        cameraEventGenerator::SendOBDState(cmd, NULL);
    }
    if (eBTType == BTType_HID) {
        cameraEventGenerator::SendHIDState(cmd, NULL);
        return 0;
    }
    snprintf(status, 8, "%d", (pDeviceManager->getDevStatus(eBTType) + (eBTType << 8)));
    memset(tmp1, 0x00, sizeof(tmp1));
    if (eBTType == BTType_OBD) {
        agcmd_property_get(propOBDMac, tmp1, "NA");
    } else if (eBTType == BTType_HID) {
        agcmd_property_get(propHIDMac, tmp1, "NA");
    }
    if (agbt_pin_get(tmp1, bdname_str, bdpin_str)) {
        if (eBTType == BTType_OBD) {
            snprintf(rt, 64, "%s#OBD-TW01", tmp1);
        } else if (eBTType == BTType_HID) {
            snprintf(rt, 64, "%s#RC-TW01", tmp1);
        }
    } else {
        snprintf(rt, 64, "%s#%s", tmp1, bdname_str);
    }
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_getDEVStatus, status, rt);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}
SCMD_CMD_EXECUTE(GetBTHostNum)
{
    if (pDeviceManager == NULL) {
        return -1;
    }
    EnumedStringCMD* cmd = p;
    char num[8] = "";
    snprintf(num, 8, "%d", pDeviceManager->getHostNum());
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_getHostNum, num, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}
SCMD_CMD_EXECUTE(GetBTHostInfor)
{
    if (pDeviceManager == NULL) {
        return -1;
    }
    EnumedStringCMD* cmd = p;
    int idx = atoi(cmd->getPara1());
    char *name = (char *)malloc(64);
    char *mac = (char *)malloc(64);
    memset(name, 0x00, 64);
    memset(mac, 0x00, 64);
    if (pDeviceManager->getHostInfor(idx, &name, &mac) == 0) {
        EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_getHostInfor, name, mac);
        StringCMD *pp = &result;
        StringEnvelope ev(&pp, 1);
        if (cmd != NULL) {
            cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
        }
        free(name);
        free(mac);
        return 0;
    }
    free(name);
    free(mac);
    return -1;
}
SCMD_CMD_EXECUTE(DoScanBT)
{
    if (pDeviceManager == NULL) {
        return -1;
    }
    pDeviceManager->doScan();
    return 0;
}
SCMD_CMD_EXECUTE(DoBindBT)
{
    if (pDeviceManager == NULL) {
        return -1;
    }
    EnumedStringCMD* cmd = p;
    const char *mac = cmd->getPara2();
    const char *type = cmd->getPara1();
    pDeviceManager->doBind(mac, (EBTType)atoi(type));
    return 0;
}
SCMD_CMD_EXECUTE(DoUnBindBT)
{
    if (pDeviceManager == NULL) {
        return -1;
    }
    EnumedStringCMD* cmd = p;
    const char *type = cmd->getPara1();
    pDeviceManager->doUnBind((EBTType)atoi(type));
    return 0;
}
SCMD_CMD_EXECUTE(DoSetOBDTypes)
{
    return -2;//todo
}
SCMD_CMD_EXECUTE(FormatTF)
{
    CAMERALOG("FormatTF command from App");
    if (IPC_AVF_Client::getObject()) {
        IPC_AVF_Client::getObject()->FormatTF();
    }

// the result will be sent back asynchronously
#if 0
    EnumedStringCMD* cmd = p;
    char tmp1[] = "0";
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_Format_TF, tmp1, NULL);
    StringCMD* pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
#endif
    return 0;
}

SCMD_CMD_EXECUTE(GetLanguageList)
{
    EnumedStringCMD* cmd = p;

    int numLang = 0;
    char** ppList = NULL;
    ppList = ui::CLanguageLoader::getInstance()->GetLanguageList(numLang);

    // Language list support English, Chinese, Russian at the moment
    unsigned long long lang_list = 0;
    for (int i = 0; i < numLang; i++) {
        CAMERALOG("ppList[%d]: %s", i, ppList[i]);
        if (strcmp("English", ppList[i]) == 0) {
            // English, index 0
            lang_list = lang_list | (0x01<<0);
        } else if (strcmp("涓枃", ppList[i]) == 0) {
            // Chinese, index 1
            lang_list = lang_list | (0x01<<1);
        } else if (strcmp("Русский", ppList[i]) == 0) {
            // Russian, index 2
            lang_list = lang_list | (0x01<<2);
        }
    }

    char tmp[64];
    snprintf(tmp, 64, "%lld", lang_list);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_Get_Language_List, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else {// evnet broad cast
        BroadCaster* b = this->getDomainObj()->getBroadCaster();
        if (b) {
            b->BroadCast(&ev);
        }
    }
    return 0;
}


SCMD_CMD_EXECUTE(GetLanguage)
{
    EnumedStringCMD* cmd = p;

    char language[256];
    memset(language, 0x00, sizeof(language));
    agcmd_property_get(PropName_Language, language, "English");
    char selected[8] = "-1";
    if (strcasecmp("English", language) == 0) {
        // English, index 0
        snprintf(selected, 8, "%d", 0);
    } else if (strcasecmp("涓枃", language) == 0) {
        // Chinese, index 1
        snprintf(selected, 8, "%d", 1);
    } else if (strcasecmp("Русский", language) == 0) {
        // Russian, index 2
        snprintf(selected, 8, "%d", 2);
    }

    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_Get_Language, selected, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else {// evnet broad cast
        BroadCaster* b = this->getDomainObj()->getBroadCaster();
        if (b) {
            b->BroadCast(&ev);
        }
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetLanguage)
{
    EnumedStringCMD* cmd = p;
    int idx = atoi(cmd->getPara1());
    int numLang = 0;
    ui::CLanguageLoader::getInstance()->GetLanguageList(numLang);

    if ((idx >= 0) && (idx < numLang)) {
        switch (idx) {
            case 0:
                // English
                agcmd_property_set(PropName_Language, "English");
                break;
            case 1:
                // Chinese
                agcmd_property_set(PropName_Language, "涓枃");
                break;
            case 2:
                // Russian
                agcmd_property_set(PropName_Language, "Русский");
                break;
            default:
                CAMERALOG("Unsupported language index %d", idx);
                break;
        }
    } else {
        CAMERALOG("Unsupported language index %d", idx);
    }

    char tmp1[] = "0";
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_Set_Language, tmp1, NULL);
    StringCMD* pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetWorkingMode) {
    EnumedStringCMD* cmd = p;
    int mode = atoi(cmd->getPara1());
    IPC_AVF_Client::getObject()->setWorkingMode((CAM_WORKING_MODE)mode);
    return 0;
}

SCMD_CMD_EXECUTE(GetWorkingMode) {
    EnumedStringCMD* cmd = p;
    CAM_WORKING_MODE mode = IPC_AVF_Client::getObject()->getWorkingMode();
    if (CAM_MODE_TIMING_SHOT == mode) {
        mode = CAM_MODE_PHOTO;
    }
    char workingmode[8] = "-1";
    snprintf(workingmode, 8, "%d", mode);

    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_Get_WorkingMode, workingmode, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else {// evnet broad cast
        BroadCaster* b = this->getDomainObj()->getBroadCaster();
        if (b) {
            b->BroadCast(&ev);
        }
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetPhotoLapseInterval) {
    EnumedStringCMD* cmd = p;
    agcmd_property_set(PropName_PhotoLapse, cmd->getPara1());

    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_PhotoLapse, tmp, "0.5");
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_Get_PhotoLapse_Interval, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    BroadCaster* b = this->getDomainObj()->getBroadCaster();
    if (b) {
        b->BroadCast(&ev);
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetPhotoLapseInterval) {
    EnumedStringCMD* cmd = p;
    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_PhotoLapse, tmp, "0.5");

    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_Get_PhotoLapse_Interval, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else {// evnet broad cast
        BroadCaster* b = this->getDomainObj()->getBroadCaster();
        if (b) {
            b->BroadCast(&ev);
        }
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetSpeakerStatus)
{
    EnumedStringCMD* cmd = p;

    int bEnable = atoi(cmd->getPara1());
    int volume  = atoi(cmd->getPara2());
    if (volume < 0 || volume > 10) {
        volume = atoi(DEFAULT_SPK_VOLUME);
    }

    bool handled = false;
    char tmp[256];
    agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
    if (bEnable) {
        if (strcasecmp(tmp, "Mute") == 0) {
            // if it is mute, change to unmute
            agcmd_property_get(Settings_Spk_Volume, tmp, DEFAULT_SPK_VOLUME);
            IPC_AVF_Client::getObject()->SetSpkVolume(
                (atoi(tmp) == 0) ? atoi(DEFAULT_SPK_VOLUME) : atoi(tmp));
            handled = true;
        }
    } else {
        if ((strcasecmp(tmp, "Normal") == 0) ||
            (strcasecmp(tmp, "Manual") == 0))
        {
            // if it is unmute, change to mute
            agcmd_property_get(Prop_Spk_Volume, tmp, DEFAULT_SPK_VOLUME);
            if (strcasecmp(tmp, "0") == 0) {
                snprintf(tmp, sizeof(tmp), "%s", DEFAULT_SPK_VOLUME);
            }
            agcmd_property_set(Settings_Spk_Volume, tmp);
            IPC_AVF_Client::getObject()->SetSpkVolume(0);
            handled = true;
        }
    }

    agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
    if (!handled &&
        ((strcasecmp(tmp, "Normal") == 0) ||
        (strcasecmp(tmp, "Manual") == 0)))
    {
        IPC_AVF_Client::getObject()->SetSpkVolume(volume);
        if (volume == 0) {
            snprintf(tmp, sizeof(tmp), "%s", DEFAULT_SPK_VOLUME);
        }
        agcmd_property_set(Settings_Spk_Volume, tmp);
    }

    char feedback[] = "1";
    EnumedStringCMD result(CMD_Domain_cam, CMD_Set_Speaker_Status, feedback, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetSpeakerStatus)
{
    EnumedStringCMD* cmd = p;

    char volume[256];
    agcmd_property_get(Prop_Spk_Volume, volume, DEFAULT_SPK_VOLUME);

    char tmp[256];
    agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
    char *enable = NULL;
    if ((strcasecmp(tmp, "Normal") == 0) ||
        (strcasecmp(tmp, "Manual") == 0))
    {
        enable = (char *)"1";
    } else if (strcasecmp(tmp, "Mute") == 0) {
        enable = (char *)"0";
    }

    //CAMERALOG("GetSpeakerStatus enable = %s, volume = %s", enable, volume);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Get_Speaker_Status, enable, volume);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetAutoBrightness)
{
    EnumedStringCMD* cmd = p;

    // TODO:
    agcmd_property_set(PropName_Auto_Brightness, cmd->getPara1());

    char tmp[256] = {0x00};
    agcmd_property_get(PropName_Auto_Brightness, tmp, "1");
    EnumedStringCMD result(CMD_Domain_cam, CMD_Set_Display_Auto_Brightness, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetAutoBrightness)
{
    EnumedStringCMD* cmd = p;
    // TODO:

    char tmp[256] = {0x00};
    agcmd_property_get(PropName_Auto_Brightness, tmp, "1");
    EnumedStringCMD result(CMD_Domain_cam, CMD_Get_Display_Auto_Brightness, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetDisplayBrightness)
{
    EnumedStringCMD* cmd = p;
    // brightness [1 ... 10], map to [25, 250] which is defined by VOUT:
    int brightness  = atoi(cmd->getPara1());
    if (brightness < 0) {
        brightness = 0;
    } else if (brightness > 10) {
        brightness = 10;
    }
    int value = brightness * Brightness_Step_Value + Brightness_MIN;
    IPC_AVF_Client::getObject()->SetDisplayBrightness(value);
    char tmp[256] = {0x00};
    snprintf(tmp, 256, "%d", value);
    agcmd_property_set(PropName_Brighness, tmp);

    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_Brighness, tmp, Default_Brighness);
    brightness = atoi(tmp);
    snprintf(tmp, 256, "%d", (brightness - Brightness_MIN) / Brightness_Step_Value);

    EnumedStringCMD result(CMD_Domain_cam, CMD_Set_Display_Brightness, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    BroadCaster* b = this->getDomainObj()->getBroadCaster();
    if (b) {
        b->BroadCast(&ev);
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetDisplayBrightness)
{
    EnumedStringCMD* cmd = p;

    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_Brighness, tmp, Default_Brighness);
    int brightness = atoi(tmp);
    snprintf(tmp, 256, "%d",
        ((brightness > Brightness_MIN) ? (brightness - Brightness_MIN) : 0)
        / Brightness_Step_Value);

    EnumedStringCMD result(CMD_Domain_cam, CMD_Get_Display_Brightness, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetDisplayOffTime)
{
    EnumedStringCMD* cmd = p;

    agcmd_property_set(PropName_Display_Off_Time, cmd->getPara1());

    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);

    EnumedStringCMD result(CMD_Domain_cam, CMD_Set_Display_Auto_Off_Time, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    BroadCaster* b = this->getDomainObj()->getBroadCaster();
    if (b) {
        b->BroadCast(&ev);
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetDisplayOffTime)
{
    EnumedStringCMD* cmd = p;

    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);

    EnumedStringCMD result(CMD_Domain_cam, CMD_Get_Display_Auto_Off_Time, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(FactoryReset)
{
    EnumedStringCMD* cmd = p;
    bool res = true;

    // TODO:

    char tmp[16];
    snprintf(tmp, 16, "%d", res);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Factory_Reset, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetClientsInfo)
{
    EnumedStringCMD* cmd = p;

    int clients_num = 0;
    if (IPC_AVF_Client::getObject()) {
        clients_num = IPC_AVF_Client::getObject()->getClients();
    }

    char tmp[16];
    snprintf(tmp, 16, "%d", clients_num);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Get_Connected_Clients_Info, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(AddConnection)
{
    CAMERALOG("new connection from App");
    if (IPC_AVF_Client::getObject()) {
        IPC_AVF_Client::getObject()->onClientsNumChanged(true);
    }
    return 0;
}

SCMD_CMD_EXECUTE(RemoveConnection)
{
    CAMERALOG("connection closed from App");
    if (IPC_AVF_Client::getObject()) {
        IPC_AVF_Client::getObject()->onClientsNumChanged(false);
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetAPIVersion)
{
    EnumedStringCMD* cmd = p;
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_getAPIVersion,
        (char*)SERVICE_UPNP_VERSION, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(IsAPISupported)
{
    EnumedStringCMD* cmd = p;
    bool issupported = false;
    int apidom = atoi(cmd->getPara1());
    int apiidx = atoi(cmd->getPara2());
    if ((apidom == CMD_Domain_cam) && (apiidx < CMD_Cam_num)) {
        issupported = true;
    }
    if ((apidom == CMD_Domain_rec) && (apiidx < CMD_Rec_num)) {
        issupported = true;
    }
    char apiwithdomain[16];
    snprintf(apiwithdomain, 16, "%d", (apidom << 16) + apiidx);
    //CAMERALOG("IsAPISupported: %s, %d", apiwithdomain, issupported);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_isAPISupported,
        apiwithdomain, issupported ? (char*)"1" : (char*)"0");
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetName)
{
    EnumedStringCMD* cmd = p;
    char nam[AGCMD_PROPERTY_SIZE];
    memset(nam, 0x00, sizeof(nam));
    agcmd_property_get(PropName_camera_name, nam, Camera_name_default);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_get_Name_result, nam, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else {// evnet broad cast
        BroadCaster* b = this->getDomainObj()->getBroadCaster();
        if (b) {
            b->BroadCast(&ev);
        }
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetNameResult)
{
    return 0;
}

SCMD_CMD_EXECUTE(SetName)
{
    EnumedStringCMD* cmd = p;
    CAMERALOG("on set name : %s", cmd->getPara1());
    agcmd_property_set(PropName_camera_name, cmd->getPara1());
    IPC_AVF_Client::getObject()->SetCameraName(cmd->getPara1());

    char tmp1[] = "0";
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_set_Name, tmp1, NULL);
    StringCMD* pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }

    char nam[AGCMD_PROPERTY_SIZE];
    memset(nam, 0x00, sizeof(nam));
    agcmd_property_get(PropName_camera_name, nam, Camera_name_default);
    EnumedStringCMD result_bdct(CMD_Domain_cam, CMD_Cam_get_Name_result, nam, NULL);
    StringCMD *pp_bdct = &result_bdct;
    StringEnvelope ev_bdct(&pp_bdct, 1);
    BroadCaster* b = this->getDomainObj()->getBroadCaster();
    if (b) {
        b->BroadCast(&ev_bdct);
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetNameResult)
{
    return 0;
}

SCMD_CMD_EXECUTE(GetMode)
{
    EnumedStringCMD* cmd = p;

    IPC_AVF_Client::AVF_Mode mode = IPC_AVF_Client::getObject()->getAvfMode();
    State_Cam_Mode cam_mode = State_Cam_Mode_idle;
    switch (mode)
    {
        case IPC_AVF_Client::AVF_Mode_NULL:
            cam_mode = State_Cam_Mode_idle;
            break;
        case IPC_AVF_Client::AVF_Mode_Encode:
            cam_mode = State_Cam_Mode_Record;
            break;
        case IPC_AVF_Client::AVF_Mode_Decode:
            cam_mode = State_Cam_Mode_Decode;
            break;
    };
    char tmp[64];
    snprintf(tmp, 64, "%d", cam_mode);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_getMode_result, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }else {// evnet broad cast
        BroadCaster* b = this->getDomainObj()->getBroadCaster();
        if (b) {
            b->BroadCast(&ev);
        }
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetModeResult)
{
    return 0;
}

SCMD_CMD_EXECUTE(GetState)
{
    EnumedStringCMD* cmd = p;

    int state = 0;
    int is_still = 0;
    IPC_AVF_Client::getObject()->getRecordState(&state, &is_still);
    char buf1[64] = "";
    char buf2[64] = "";
    snprintf(buf1, 64, "%d", state);
    snprintf(buf2, 64, "%d", is_still);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_get_State_result, buf1, buf2);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL)
    {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else {
        BroadCaster* b = this->getDomainObj()->getBroadCaster();
        if (b) {
            b->BroadCast(&ev);
        }
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetStateResult)
{
    return 0;
}

SCMD_CMD_EXECUTE(StartRecord)
{
    IPC_AVF_Client::getObject()->StartRecording();

    EnumedStringCMD* cmd = p;
    char tmp1[] = "0";
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_start_rec, tmp1, NULL);
    StringCMD* pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(StopRecord)
{
    IPC_AVF_Client::getObject()->StopRecording();

    EnumedStringCMD* cmd = p;
    char tmp1[] = "0";
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_stop_rec, tmp1, NULL);
    StringCMD* pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }

    return 0;
}

SCMD_CMD_EXECUTE(SetStreamSize)
{
    EnumedStringCMD* cmd = p;
    CAMERALOG("on set stream : %s", cmd->getPara1());
    int bBig = atoi(cmd->getPara1());
    IPC_AVF_Client::getObject()->switchStream(bBig);

    char tmp1[] = "0";
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_set_StreamSize, tmp1, NULL);
    StringCMD* pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetTime)
{
    EnumedStringCMD* cmd = p;
    UINT64 time = IPC_AVF_Client::getObject()->getRecordingTime();
    char tmp[64];
    snprintf(tmp, 64, "%lld",time);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_get_time_result, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else {
        BroadCaster* b = this->getDomainObj()->getBroadCaster();
        if (b) {
            b->BroadCast(&ev);
        }
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetTimeResult)
{
    return 0;
}

SCMD_CMD_EXECUTE(GetAllInfor)
{
    EnumedStringCMD* cmd = p;
    cameraEventGenerator::SendAudioState(cmd, NULL);
    cameraEventGenerator::SendPowerState(cmd, NULL);
    cameraEventGenerator::SendStorageState(cmd, NULL);
    cameraEventGenerator::SendGPSState(cmd, NULL);
    return 0;
}

SCMD_CMD_EXECUTE(GetStorageInfor)
{
    EnumedStringCMD* cmd = p;
    cameraEventGenerator::SendStorageState(cmd, NULL);
    return 0;
}

SCMD_CMD_EXECUTE(MsgBatteryInfor)
{
// This is time cost. Should avoid calling it.
#if 0
    EnumedStringCMD* cmd = p;

    char *power_status_json = NULL;
    cameraEventGenerator::getPowerInfoJson(&power_status_json);

    char tmp4[AGCMD_PROPERTY_SIZE];
    agcmd_property_get(propPowerSupplyBCapacity, tmp4, "0");

    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_Battery_infor,
        power_status_json, tmp4);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }

    delete [] power_status_json;
#endif
    return 0;
}

SCMD_CMD_EXECUTE(GetOBDVIN)
{
    EnumedStringCMD* cmd = p;

    char vin[32];
    memset(vin, 0x00, 32);

    struct agobd_client_info_s *obd_client = agobd_open_client();
    if (obd_client) {
        int ret_val = agobd_client_get_info(obd_client);
        if (ret_val == 0) {
            struct agobd_info_s *info = obd_client->pobd_info;
            const char *pvin = (const char*)info + info->vin_offset;
            if (*pvin != 0x00) {
                int size = sizeof(info->vin);
                if (size > 32) {
                    size = 32;
                }
                memcpy(vin, pvin, size);
                vin[size - 1] = 0x00;

                char mask[AGCMD_PROPERTY_SIZE];
                memset(mask, 0x00, AGCMD_PROPERTY_SIZE);
                agcmd_property_get(PropName_VIN_MASK, mask, Default_VIN_MASK);
                for (unsigned int i = 0; (i < strlen(mask)) && (i < strlen(vin)); i++) {
                    if (mask[i] == '-') {
                        vin[i] = '-';
                    }
                }
            }
        }
    }

    //CAMERALOG("#### vin = %s", vin);
    EnumedStringCMD result(CMD_Domain_cam, CMD_GET_OBD_VIN, vin, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetVINStyle)
{
    EnumedStringCMD* cmd = p;
    //CAMERALOG("##### SetVINStyle: %s %s", cmd->getPara1(), cmd->getPara2());
    int enable = atoi(cmd->getPara1());
    if (enable != 0) {
        agcmd_property_set(PropName_VIN_MASK, cmd->getPara2());
    } else {
        agcmd_property_set(PropName_VIN_MASK, "");
    }

    char *ret = (char*)"1";
    EnumedStringCMD result(CMD_Domain_cam, CMD_SET_CLIP_VIN_STYLE, ret, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetVINStyle)
{
    EnumedStringCMD* cmd = p;
    char *enabled = (char*)"1";
    char mask[AGCMD_PROPERTY_SIZE];
    memset(mask, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(PropName_VIN_MASK, mask, Default_VIN_MASK);
    if (strlen(mask) > 0) {
        enabled = (char*)"1";
    } else {
        enabled = (char*)"0";
    }

    EnumedStringCMD result(CMD_Domain_cam, CMD_GET_CLIP_VIN_STYLE, enabled, mask);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetScreenSaverStyle)
{
    EnumedStringCMD* cmd = p;
    const char *new_style = cmd->getPara1();
    char *ret = (char*)"1";
    if ((strcasecmp(new_style, "All Black") == 0)
        || (strcasecmp(new_style, "Dot") == 0))
    {
        agcmd_property_set(PropName_Screen_saver_style, new_style);
    } else {
        ret = (char*)"0";
    }

    EnumedStringCMD result(CMD_Domain_cam, CMD_SET_Screen_Saver_Style, ret, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetScreenSaverStyle)
{
    EnumedStringCMD* cmd = p;
    char tmp[256];
    agcmd_property_get(PropName_Screen_saver_style, tmp, Default_Screen_saver_style);

    EnumedStringCMD result(CMD_Domain_cam, CMD_GET_Screen_Saver_Style, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(SetUnitsSystem)
{
    EnumedStringCMD* cmd = p;
    const char *new_units = cmd->getPara1();
    char *ret = (char*)"1";
    if ((strcasecmp(new_units, Imperial_Units_System) == 0)
        || (strcasecmp(new_units, Metric_Units_System) == 0))
    {
        agcmd_property_set(PropName_Units_System, new_units);
    } else {
        ret = (char*)"0";
    }

    EnumedStringCMD result(CMD_Domain_cam, CMD_SET_Units_System, ret, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetUnitsSystem)
{
    EnumedStringCMD* cmd = p;
    char tmp[256];
    agcmd_property_get(PropName_Units_System, tmp, Imperial_Units_System);

    EnumedStringCMD result(CMD_Domain_cam, CMD_GET_Units_System, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    }
    return 0;
}


cameraEventGenerator::cameraEventGenerator(SCMD_Domain *domain)
    : Domain_EvnetGenerator(domain)
{
    IPC_AVF_Client::getObject()->setDelegate(this);
    CPropsEventThread::getInstance()->AddPEH(this);
}

void cameraEventGenerator::onNewFileReady(char* path)
{
    CAMERALOG("--->cameraEventGenerator--->onNewFileReady");
}

void cameraEventGenerator::onCameraModeChange()
{
    this->Event(CMD_Cam_getMode);
}

void cameraEventGenerator::onWorkingModeChange() {
    CAM_WORKING_MODE mode = IPC_AVF_Client::getObject()->getWorkingMode();
    if (CAM_MODE_TIMING_SHOT == mode) {
        mode = CAM_MODE_PHOTO;
    }
    char workingmode[8] = "-1";
    snprintf(workingmode, 8, "%d", mode);

    EnumedStringCMD result_1(CMD_Domain_cam, CMD_CAM_Get_WorkingMode, workingmode, NULL);
    StringCMD *pp_1 = &result_1;
    StringEnvelope ev_1(&pp_1, 1);
    this->GetDomainObj()->BroadCast(&ev_1);
}

void cameraEventGenerator::onRecordStateChange(State_Rec state)
{
    char tmp[64];
    memset(tmp, 0x00, sizeof(tmp));
    snprintf(tmp, 64, "%d", state);

    int rec_state = 0;
    int is_still = 0;
    IPC_AVF_Client::getObject()->getRecordState(&rec_state, &is_still);
    char buf2[64] = "";
    snprintf(buf2, 64, "%d", is_still);

    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_get_State_result, tmp, buf2);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    //CAMERALOG("broadcast -- record state: %d", state);
    this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::onStartRecordError(Rec_Error_code state)
{
    char tmp[64];
    memset(tmp, 0x00, sizeof(tmp));
    snprintf(tmp, 64, "%d", state);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Rec_error, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    //CAMERALOG("broadcast -- record state: %d", state);
    this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::onStillCaptureStarted(int one_shot) {
    char buf1[64];
    memset(buf1, 0x00, sizeof(buf1));
    snprintf(buf1, 64, "%d", one_shot);
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_StartStillCapture, buf1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::onStillCaptureDone() {
    EnumedStringCMD result(CMD_Domain_rec, MSG_Rec_StillCaptureDone, NULL, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::sendStillCaptureInfo(
    int stillcap_state, int num_pictures, int burst_ticks)
{
    char buf1[64];
    char buf2[64];
    memset(buf1, 0x00, sizeof(buf1));
    memset(buf2, 0x00, sizeof(buf2));
    snprintf(buf1, 64, "%d", (burst_ticks << 4) | stillcap_state);
    snprintf(buf2, 64, "%d", num_pictures);
    EnumedStringCMD result(CMD_Domain_rec, MSG_Rec_StillPictureInfo, buf1, buf2);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::EventPropertyUpdate(changed_property_type state)
{
    //CAMERALOG("+++++--cameraEventGenerator");
    switch (state)
    {
        case property_type_wifi:

            break;

        case property_type_file:

            break;

        case property_type_audio:
            SendAudioState(NULL, this->GetDomainObj());
            break;
        case property_type_storage:
            SendStorageState(NULL, this->GetDomainObj());
            break;

        case property_type_power:
            SendPowerState(NULL, this->GetDomainObj());
            break;

        case property_type_Bluetooth:
            SendBTState(NULL, this->GetDomainObj());
            break;
        case property_type_obd:
            SendOBDState(NULL, this->GetDomainObj());
            break;
        case property_type_hid:
            SendHIDState(NULL, this->GetDomainObj());
            break;

        case property_type_resolution:
            SendResolution(NULL, this->GetDomainObj());
            break;
        case property_type_settings:
            SendSettingsState(NULL, this->GetDomainObj());
            break;
        case property_type_rotation:
        case property_type_rotation_mode:
            SendRotation(NULL, this->GetDomainObj());
            break;

        default:
            break;
    }
}

void cameraEventGenerator::SendAudioState(EnumedStringCMD* sender, SCMD_Domain *domain)
{
    char tmp[AGCMD_PROPERTY_SIZE];
    memset(tmp, 0x00, sizeof(tmp));
    char tmp2[AGCMD_PROPERTY_SIZE];
    memset(tmp2, 0x00, sizeof(tmp2));

#ifdef PLATFORMHACHI
    agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
    if ((strcasecmp(tmp, "Normal") == 0) ||
        (strcasecmp(tmp, "Manual") == 0))
    {
        snprintf(tmp, AGCMD_PROPERTY_SIZE, "%d", State_Mic_ON);
    } else if (strcasecmp(tmp, "Mute") == 0) {
        snprintf(tmp, AGCMD_PROPERTY_SIZE, "%d", State_Mic_MUTE);
    }
    snprintf(tmp2, AGCMD_PROPERTY_SIZE, "%d", defaultMicGain);
#else
    int rt;
    agcmd_property_get(audioGainPropertyName, tmp2, "0");
    //CAMERALOG("camrea event mic : %d, %s", rt, tmp2);
    int vol = atoi(tmp2);
    rt = (vol == 0) ? State_Mic_MUTE : State_Mic_ON;
    snprintf(tmp, AGCMD_PROPERTY_SIZE, "%d", rt);
    snprintf(tmp2, AGCMD_PROPERTY_SIZE, "%d", vol);
#endif

    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_Mic_infor, tmp, tmp2);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    //CAMERALOG("--send out : %s", ev.getBuffer());
    if (sender) {
        sender->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else if(domain) {
        domain->BroadCast(&ev);
    }

#ifdef PLATFORMHACHI
    char volume[256];
    agcmd_property_get(Prop_Spk_Volume, volume, DEFAULT_SPK_VOLUME);

    agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
    char *enable = NULL;
    if ((strcasecmp(tmp, "Normal") == 0) ||
        (strcasecmp(tmp, "Manual") == 0))
    {
        enable = (char *)"1";
    } else if (strcasecmp(tmp, "Mute") == 0) {
        enable = (char *)"0";
    }

    EnumedStringCMD spk_result(CMD_Domain_cam, CMD_Get_Speaker_Status, enable, volume);
    StringCMD *spk_pp = &spk_result;
    StringEnvelope spk_ev(&spk_pp, 1);
    if (sender) {
        sender->SendBack(spk_ev.getBuffer(), spk_ev.getBufferLen());
    } else if(domain) {
        domain->BroadCast(&spk_ev);
    }
#endif
}

void cameraEventGenerator::SendPowerState(EnumedStringCMD* sender, SCMD_Domain *domain)
{
    char tmp1[AGCMD_PROPERTY_SIZE];
    char tmp2[AGCMD_PROPERTY_SIZE];
    char tmp3[AGCMD_PROPERTY_SIZE];
    char tmp4[AGCMD_PROPERTY_SIZE];
    //int rt;
    memset(tmp1, 0x00, sizeof(tmp1));
    memset(tmp2, 0x00, sizeof(tmp2));
    memset(tmp3, 0x00, sizeof(tmp3));
    memset(tmp4, 0x00, sizeof(tmp4));

    agcmd_property_get(powerstateName, tmp1, "Unknown");
    agcmd_property_get(propPowerSupplyOnline, tmp2, "0");
    agcmd_property_get(propPowerSupplyBV, tmp3, "3700");
    agcmd_property_get(propPowerSupplyBCapacity, tmp4, "0");

    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_power_infor, tmp1, tmp2);
#if defined(PLATFORMHACHI)
    char *power_status_json = NULL;
    getPowerInfoJson(&power_status_json);
    EnumedStringCMD result1(CMD_Domain_cam, CMD_Cam_msg_Battery_infor,
        power_status_json, tmp4);
#else
    EnumedStringCMD result1(CMD_Domain_cam, CMD_Cam_msg_Battery_infor, tmp3, tmp4);
#endif
    StringCMD *pp[2];
    pp[0] = &result;
    pp[1] = &result1;
    StringEnvelope ev(pp, 2);
    if (sender) {
        sender->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else if(domain) {
        domain->BroadCast(&ev);
    }

#if defined(PLATFORMHACHI)
    delete [] power_status_json;
#endif
}

void cameraEventGenerator::SendStorageState
    (EnumedStringCMD* sender
    , SCMD_Domain *domain)
{
    char tmp1[AGCMD_PROPERTY_SIZE];
    int rt = State_storage_ready;;
    char storage_prop[256];
    getStoragePropertyName(storage_prop, sizeof(storage_prop));
    memset(tmp1, 0x00, sizeof(tmp1));
    agcmd_property_get(storage_prop, tmp1, "");
    if (strcasecmp(tmp1, "mount_ok") == 0) {
        rt = State_storage_ready;
    } else if((strcasecmp(tmp1, "unknown") == 0)
        || (strcasecmp(tmp1, "mount_fail") == 0)
        || (strcasecmp(tmp1, "NA") == 0))
    {
        rt = State_storage_noStorage;
    } else if((strcasecmp(tmp1, "checking") == 0)
        || (strcasecmp(tmp1, "formating") == 0))
    {
        rt = State_storage_loading;
    } else if(strcasecmp(tmp1, "mount_fail") == 0) {
        rt = State_storage_error;
    } else if(strcasecmp(tmp1, "ready") == 0) {
        rt = State_storage_usbdisc;
    }
    snprintf(tmp1, AGCMD_PROPERTY_SIZE, "%d", rt);

#if defined(PLATFORMHACHI)

    //CAMERALOG("camrea event storage : %s\n", tmp1);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_Storage_infor, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (sender) {
        sender->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else if(domain) {
        domain->BroadCast(&ev);
    }

#else

    char tmp2[AGCMD_PROPERTY_SIZE];
    char tmp3[AGCMD_PROPERTY_SIZE];
    memset(tmp2, 0x00, sizeof(tmp2));
    memset(tmp3, 0x00, sizeof(tmp3));

    bool rdy = false;
    unsigned int free = 0;
    long long Total = 0;
    IPC_AVF_Client::getObject()->GetVDBState(rdy, free, Total);
    //CAMERALOG("free = %dMB, Total = %lldMB", free, Total);
    snprintf(tmp2, AGCMD_PROPERTY_SIZE, "%llu", Total*1024*1024);
    snprintf(tmp3, AGCMD_PROPERTY_SIZE, "%llu", (unsigned long long)free*1024*1024);

    //CAMERALOG("camrea event storage : %s, %s, %s\n", tmp1, tmp2, tmp3);

    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_Storage_infor, tmp1, NULL);
    EnumedStringCMD result1(CMD_Domain_cam, CMD_Cam_msg_StorageSpace_infor, tmp2, tmp3);
    StringCMD *pp[2];
    pp[0] = &result;
    pp[1] = &result1;
    StringEnvelope ev(pp, 2);
    //CAMERALOG("--send out : %s", ev.getBuffer());
    if (sender) {
        sender->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else if(domain) {
        domain->BroadCast(&ev);
    }

#endif
}

void cameraEventGenerator::SendGPSState
    (EnumedStringCMD* sender
    , SCMD_Domain *domain)
{
    int rt = IPC_AVF_Client::getObject()->getGpsState();
    State_GPS state = State_GPS_off;
    switch (rt)
    {
        case 0:
            state = State_GPS_ready;
            break;
        case -1:
            state = State_GPS_on;
            break;
        case -2:
            state = State_GPS_off;
            break;
        default:
            state = State_GPS_off;
            break;
    }
    char tmp1[64];
    snprintf(tmp1, 64, "%d", state);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_GPS_infor, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (sender) {
        sender->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else if(domain) {
        domain->BroadCast(&ev);
    }
}

void cameraEventGenerator::SendResolution(EnumedStringCMD* sender, SCMD_Domain *domain)
{
    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_rec_resoluton, tmp, "1080p30");
    int index = 0;
    if (strcasecmp(tmp, "1080p30") == 0) {
        index = Video_Resolution_1080p30;
    } else if (strcasecmp(tmp, "720p60") == 0) {
        index = Video_Resolution_720p60;
    } else if (strcasecmp(tmp, "720p30") == 0) {
        index = Video_Resolution_720p30;
    } else if (strcasecmp(tmp, "QXGAp30") == 0) {
        index = Video_Resolution_QXGAp30;
    } else if (strcasecmp(tmp, "1080p60") == 0) {
        index = Video_Resolution_1080p60;
    } else if (strcasecmp(tmp, "stillCapture") == 0) {
        index = Video_Resolution_Still;
    } else if (strcasecmp(tmp, "720p120") == 0) {
        index = Video_Resolution_720p120;
    }
    snprintf(tmp, 256, "%d", index);
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_get_Resolution, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (domain) {
        domain->BroadCast(&ev);
    }
}

void cameraEventGenerator::SendRotation(EnumedStringCMD* sender, SCMD_Domain *domain)
{
    // current mode
    char rotate_mode[256];
    memset(rotate_mode, 0x00, 256);
    agcmd_property_get(PropName_Rotate_Mode, rotate_mode, Rotate_Mode_Normal);

    // current rotation
    char *rotation = NULL;
    char horizontal[256];
    char vertical[256];
    agcmd_property_get(PropName_Rotate_Horizontal, horizontal, "0");
    agcmd_property_get(PropName_Rotate_Vertical, vertical, "0");
    if ((strcasecmp(horizontal, "0") == 0)
        && (strcasecmp(vertical, "0") == 0))
    {
        rotation = (char*)Rotate_Mode_Normal;
    } else if ((strcasecmp(horizontal, "1") == 0)
         && (strcasecmp(vertical, "1") == 0))
    {
        rotation = (char*)Rotate_Mode_180;
    }

    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Get_Rotate_Mode, rotate_mode, rotation);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (domain) {
        domain->BroadCast(&ev);
    }
}

void cameraEventGenerator::SendSettingsState(EnumedStringCMD* sender, SCMD_Domain *domain)
{
    char tmp[256];
    memset(tmp, 0x00, 256);
    agcmd_property_get(PropName_Brighness, tmp, Default_Brighness);
    int brightness = atoi(tmp);
    snprintf(tmp, 256, "%d", (brightness - Brightness_MIN) / Brightness_Step_Value);
    EnumedStringCMD brightness_result(CMD_Domain_cam, CMD_Get_Display_Brightness, tmp, NULL);
    StringCMD *brightness_pp = &brightness_result;
    StringEnvelope brightness_ev(&brightness_pp, 1);
    if (domain) {
        domain->BroadCast(&brightness_ev);
    }

    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);
    EnumedStringCMD displayoff_result(CMD_Domain_cam, CMD_Get_Display_Auto_Off_Time, tmp, NULL);
    StringCMD *displayoff_pp = &displayoff_result;
    StringEnvelope displayoff_ev(&displayoff_pp, 1);
    if (domain) {
        domain->BroadCast(&displayoff_ev);
    }

    agcmd_property_get(PropName_Screen_saver_style, tmp, Default_Screen_saver_style);
    EnumedStringCMD screen_saver_result(CMD_Domain_cam, CMD_GET_Screen_Saver_Style, tmp, NULL);
    StringCMD *screen_saver_pp = &screen_saver_result;
    StringEnvelope screen_saver_ev(&screen_saver_pp, 1);
    if (domain) {
        domain->BroadCast(&screen_saver_ev);
    }

    agcmd_property_get(PropName_Udc_Power_Delay, tmp, Udc_Power_Delay_Default);
    EnumedStringCMD poweroff_result(CMD_Domain_cam, CMD_Get_Auto_Power_Off_Delay, tmp, NULL);
    StringCMD *poweroff_pp = &poweroff_result;
    StringEnvelope poweroff_ev(&poweroff_pp, 1);
    if (domain) {
        domain->BroadCast(&poweroff_ev);
    }
}

void cameraEventGenerator::onUpgradeResult(int stat)
{
    char tmp1[64];
    snprintf(tmp1, 64, "%d", stat);
    EnumedStringCMD result(CMD_Domain_cam, CMD_fw_newVersion, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::SendBTState(EnumedStringCMD* sender, SCMD_Domain *domain)
{
    char tmp1[AGCMD_PROPERTY_SIZE];
    int rt = 0;
    char rts[8];
    memset(tmp1, 0x00, sizeof(tmp1));
    agcmd_property_get(propBTTempStatus, tmp1, "NA");
    if (strcasecmp(tmp1, "on") == 0) {
        rt = 1;
    }
    if (CTSDeviceManager::getInstance() != NULL) {
        CTSDeviceManager::getInstance()->onBTStatus(rt);
    }
    snprintf(rts, 8, "%d", rt);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_isEnabled, rts, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (sender) {
        sender->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else if (domain) {
        domain->BroadCast(&ev);
    }
}
void cameraEventGenerator::SendOBDState(EnumedStringCMD* sender, SCMD_Domain *domain)
{
    char tmp1[AGCMD_PROPERTY_SIZE];
    char bdname_str[AGBT_MAX_NAME_SIZE];
    char bdpin_str[AGBT_MAX_PIN_SIZE];
    int rt = 0;
    char rt1[8];
    char rt2[64];
    memset(tmp1, 0x00, sizeof(tmp1));
    agcmd_property_get(propBTTempOBDStatus, tmp1, "NA");
    if (strcasecmp(tmp1, "on") == 0) {
        rt = 1;
    }
    if (CTSDeviceManager::getInstance() != NULL) {
        CTSDeviceManager::getInstance()->onOBDStatus(rt);
    }
    IPC_AVF_Client::getObject()->onOBDReady(rt==1);
    snprintf(rt1, 8, "%d", (pDeviceManager->getDevStatus(BTType_OBD) + (BTType_OBD<< 8)));
    agcmd_property_get(propOBDMac, tmp1, "NA");
    if (agbt_pin_get(tmp1, bdname_str, bdpin_str)) {
        snprintf(rt2, 64, "%s#OBD-TW01", tmp1);
    } else {
        snprintf(rt2, 64, "%s#%s", tmp1, bdname_str);
    }
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_getDEVStatus, rt1, rt2);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (sender) {
        sender->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else if (domain) {
        domain->BroadCast(&ev);
    }
}
void cameraEventGenerator::SendHIDState(EnumedStringCMD* sender, SCMD_Domain *domain)
{
    char tmp1[AGCMD_PROPERTY_SIZE];
    char bdname_str[AGBT_MAX_NAME_SIZE];
    char bdpin_str[AGBT_MAX_PIN_SIZE];
    int rt = 0;
    char rt1[8];
    char rt2[64];
    memset(tmp1, 0x00, sizeof(tmp1));
    agcmd_property_get(propBTTempHIDStatus, tmp1, "NA");
    if (strcasecmp(tmp1, "on") == 0) {
        rt = 1;
    }
    if (CTSDeviceManager::getInstance() != NULL) {
        CTSDeviceManager::getInstance()->onHIDStatus(rt);
    }
    snprintf(rt1, 8, "%d", (pDeviceManager->getDevStatus(BTType_HID) + (BTType_HID<< 8)));
    agcmd_property_get(propHIDMac, tmp1, "NA");
    if (agbt_pin_get(tmp1, bdname_str, bdpin_str)) {
        snprintf(rt2, 64, "%s#RC-TW01", tmp1);
    } else {
        snprintf(rt2, 64, "%s#%s", tmp1, bdname_str);
    }
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_getDEVStatus, rt1, rt2);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    if (sender) {
        sender->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else if (domain) {
        domain->BroadCast(&ev);
    }
}

void cameraEventGenerator::onBTEnabled(EBTErr done)
{
    char tmp1[64];
    snprintf(tmp1, 64, "%d", (done==BTErr_OK)?1:0);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_isEnabled , tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}
void cameraEventGenerator::onBTDisabled(EBTErr done)
{
    char tmp1[64];
    snprintf(tmp1, 64, "%d", (done==BTErr_OK)?0:1);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_isEnabled, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}
void cameraEventGenerator::onBTScanDone(EBTErr done, const char *list)
{
    char tmp1[64];
    snprintf(tmp1, 64, "%d", done);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_doScan, tmp1, (char*)list);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}
void cameraEventGenerator::onBTOBDBinded(EBTErr done)
{
    char tmp1[64];
    char rt1[8];
    snprintf(rt1, 8, "%d", BTType_OBD);
    snprintf(tmp1, 64, "%d", done);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_doBind, rt1, tmp1);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}
void cameraEventGenerator::onBTOBDUnBinded(EBTErr done)
{
    char tmp1[64];
    char rt1[8];
    snprintf(rt1, 8, "%d", BTType_OBD);
    snprintf(tmp1, 64, "%d", done);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_doUnBind, rt1, tmp1);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}
void cameraEventGenerator::onBTHIDBinded(EBTErr done)
{
    char tmp1[64];
    char rt1[8];
    snprintf(rt1, 8, "%d", BTType_HID);
    snprintf(tmp1, 64, "%d", done);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_doBind, rt1, tmp1);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}
void cameraEventGenerator::onBTHIDUnBinded(EBTErr done)
{
    char tmp1[64];
    char rt1[8];
    snprintf(rt1, 8, "%d", BTType_HID);
    snprintf(tmp1, 64, "%d", done);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_BT_doUnBind, rt1, tmp1);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::onFormatTFDone(bool success)
{
    CAMERALOG("format TF is %s success", success ? "" : "not");
    char rt1[8];
    snprintf(rt1, 8, "%d", success);
    EnumedStringCMD result(CMD_Domain_cam, CMD_CAM_Format_TF, rt1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::sendStorageState() {
    SendStorageState(NULL, this->GetDomainObj());
}

void cameraEventGenerator::sendClientsInfo()
{
    int clients_num = 0;
    if (IPC_AVF_Client::getObject()) {
        clients_num = IPC_AVF_Client::getObject()->getClients();
    }

    char tmp[16];
    snprintf(tmp, 16, "%d", clients_num);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Get_Connected_Clients_Info, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    BroadCaster* b = this->GetDomainObj()->getBroadCaster();
    if (b) {
        b->BroadCast(&ev);
    }
}

void cameraEventGenerator::onWifiScanDone(const char *list)
{
    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_ScanHost, (char*)list, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    BroadCaster* b = this->GetDomainObj()->getBroadCaster();
    if (b) {
        b->BroadCast(&ev);
    }
}

void cameraEventGenerator::onHotspotConnected(bool bConnected)
{
    char tmp1[16];
    snprintf(tmp1, 16, "%d", bConnected);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_ConnectHotSpot, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    BroadCaster* b = this->GetDomainObj()->getBroadCaster();
    if (b) {
        b->BroadCast(&ev);
    }
}

void cameraEventGenerator::onAddWifiResult(bool bAdded)
{
    char tmp1[16];
    snprintf(tmp1, 16, "%d", bAdded);
    EnumedStringCMD result(CMD_Domain_cam, CMD_Network_AddHost, tmp1, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    BroadCaster* b = this->GetDomainObj()->getBroadCaster();
    if (b) {
        b->BroadCast(&ev);
    }
}


void cameraEventGenerator::getPowerInfoJson(char **json_str)
{
    const char *power_status[][2] = {
        {"temp.power_supply.status",             "Not charging"},
        {"temp.power_supply.charge_type",        "Fast"},
        {"temp.power_supply.health",             "Cold"},
        {"temp.power_supply.present",            "0"},
        {"temp.power_supply.online",             "1"},
        {"temp.power_supply.voltage_now",        "0"},
        {"temp.power_supply.current_max",        "0"},
        {"temp.power_supply.current_avg",        "0"},
        {"temp.power_supply.capacity_level",     "Unknown"},
        {"temp.power_supply.capacity",           "0"},
        {"temp.power_supply.charge_full",        "0"},
        {"temp.power_supply.charge_now",         "0"},
        {"temp.power_supply.charge_full_design", "0"},
        {"temp.power_supply.temp",               "0"},
        {"prebuild.board.temp_high_warn",        Warning_default},
        {"prebuild.board.temp_high_critical",    Critical_default},
        {"prebuild.board.temp_high_off",         High_off_default}
    };
    int len = sizeof(power_status) / sizeof(power_status[0]);

    Document d1;
    Document::AllocatorType& allocator = d1.GetAllocator();
    d1.SetObject();

    for (int i = 0; i < len; i++) {
        Value name(kStringType);
        name.SetString(power_status[i][0], allocator);

        char tmp1[AGCMD_PROPERTY_SIZE];
        memset(tmp1, 0x00, sizeof(tmp1));
        agcmd_property_get(power_status[i][0], tmp1, power_status[i][1]);
        Value status(kStringType);
        status.SetString(tmp1, allocator);

        d1.AddMember(name, status, allocator);
    }

    StringBuffer buffer;
    Writer<StringBuffer> write(buffer);
    d1.Accept(write);
    const char *str = buffer.GetString();

    if (json_str) {
        *json_str = new char [strlen(str) + 1];
        strcpy(*json_str, str);
        //CAMERALOG("strlen %d: %s", strlen(*json_str), *json_str);
    }
}

