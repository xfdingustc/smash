/*****************************************************************************
 * ccam_domain_camera.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2013, linnsong.
 *
 *
 *****************************************************************************/

#ifndef __H_ccam_domain_camera__
#define __H_ccam_domain_camera__

#include "ClinuxThread.h"
#include "sd_general_description.h"
#include "class_TCPService.h"
#include "ccam_cmds.h"

#include "Class_PropsEventThread.h"
#include "class_ipc_rec.h"
#include "Class_Upgrader.h"
#include "Class_DeviceManager.h"
class cameraEventGenerator
    : public Domain_EvnetGenerator
    , public IPC_AVF_Delegate
    , public CPropertyEventHandler
    , public TSUpgradeCB
    , public TSBTManagerCB
{
public:
    cameraEventGenerator(SCMD_Domain *domain);
    virtual ~cameraEventGenerator(){}
    virtual void onNewFileReady(char* path);
    virtual void onCameraModeChange();
    virtual void onWorkingModeChange();
    virtual void onRecordStateChange(State_Rec state);
    virtual void onStartRecordError(Rec_Error_code error);
    virtual void onStillCaptureStarted(int one_shot);
    virtual void onStillCaptureDone();
    virtual void sendStillCaptureInfo(int stillcap_state,
        int num_pictures, int burst_ticks);

    virtual void EventPropertyUpdate(changed_property_type state);

    static void SendAudioState(EnumedStringCMD* sender, SCMD_Domain *domain);
    static void SendStorageState(EnumedStringCMD* sender, SCMD_Domain *domain);
    static void SendPowerState(EnumedStringCMD* sender, SCMD_Domain *domain);
    static void SendGPSState(EnumedStringCMD* sender, SCMD_Domain *domain);
    static void SendSettingsState(EnumedStringCMD* sender, SCMD_Domain *domain);
    static void SendResolution(EnumedStringCMD* sender, SCMD_Domain *domain);
    static void SendRotation(EnumedStringCMD* sender, SCMD_Domain *domain);

    virtual void onUpgradeResult(int ok);

    static void SendBTState(EnumedStringCMD* sender, SCMD_Domain *domain);
    static void SendOBDState(EnumedStringCMD* sender, SCMD_Domain *domain);
    static void SendHIDState(EnumedStringCMD* sender, SCMD_Domain *domain);

    virtual void onBTEnabled(EBTErr done);
    virtual void onBTDisabled(EBTErr done);
    virtual void onBTScanDone(EBTErr done, const char *list);
    virtual void onBTOBDBinded(EBTErr done);
    virtual void onBTOBDUnBinded(EBTErr done);
    virtual void onBTHIDBinded(EBTErr done);
    virtual void onBTHIDUnBinded(EBTErr done);
    virtual void onFormatTFDone(bool success);
    virtual void sendStorageState();
    virtual void sendClientsInfo();
    virtual void onWifiScanDone(const char *list);
    virtual void onHotspotConnected(bool bConnected);
    virtual void onAddWifiResult(bool bAdded);

    static void getPowerInfoJson(char **str);

private:

};


SCMD_DOMIAN_CLASS(DomainCamera)
SCMD_CMD_CLASS(GetMode, CMD_Cam_getMode)
SCMD_CMD_CLASS(GetModeResult, CMD_Cam_getMode_result)
SCMD_CMD_CLASS(GetAPIVersion, CMD_Cam_getAPIVersion)
SCMD_CMD_CLASS(IsAPISupported, CMD_Cam_isAPISupported)
SCMD_CMD_CLASS(GetName, CMD_Cam_get_Name)
SCMD_CMD_CLASS(GetNameResult, CMD_Cam_get_Name_result)
SCMD_CMD_CLASS(SetName, CMD_Cam_set_Name)
SCMD_CMD_CLASS(SetNameResult, CMD_Cam_set_Name_result)

SCMD_CMD_CLASS(GetState, CMD_Cam_get_State)
SCMD_CMD_CLASS(GetStateResult, CMD_Cam_get_State_result)
SCMD_CMD_CLASS(StartRecord, CMD_Cam_start_rec)
SCMD_CMD_CLASS(StopRecord, CMD_Cam_stop_rec)
SCMD_CMD_CLASS(GetTime, CMD_Cam_get_time)
SCMD_CMD_CLASS(GetTimeResult, CMD_Cam_get_time_result)

SCMD_CMD_CLASS(SetStreamSize, CMD_Cam_set_StreamSize)

SCMD_CMD_CLASS(GetAllInfor, CMD_Cam_get_getAllInfor)
SCMD_CMD_CLASS(GetStorageInfor, CMD_Cam_get_getStorageInfor)
SCMD_CMD_CLASS(MsgBatteryInfor, CMD_Cam_msg_Battery_infor)

SCMD_CMD_CLASS(Poweroff, CMD_Cam_PowerOff)
SCMD_CMD_CLASS(Reboot, CMD_Cam_Reboot)

SCMD_CMD_CLASS(GetWlanMode, CMD_Network_GetWLanMode)
SCMD_CMD_CLASS(GetHostNum, CMD_Network_GetHostNum)
SCMD_CMD_CLASS(GetHostInfor, CMD_Network_GetHostInfor)

SCMD_CMD_CLASS(AddHost, CMD_Network_AddHost)
SCMD_CMD_CLASS(RmvHost, CMD_Network_RmvHost)
SCMD_CMD_CLASS(ConnectHost, CMD_Network_ConnectHost)

SCMD_CMD_CLASS(SyncTime, CMD_Network_Synctime)
SCMD_CMD_CLASS(GetDeviceTime, CMD_Network_GetDevicetime)
SCMD_CMD_CLASS(SetMic, CMD_audio_setMic)
SCMD_CMD_CLASS(GetMicState, CMD_audio_getMicState)

SCMD_CMD_CLASS(GetVersion, CMD_fw_getVersion)
SCMD_CMD_CLASS(NewVersion, CMD_fw_newVersion)
SCMD_CMD_CLASS(DoUpgrade, CMD_fw_doUpgrade)

//1.2
SCMD_CMD_CLASS(isBTSupported, CMD_CAM_BT_isSupported)
SCMD_CMD_CLASS(isBTEnabled, CMD_CAM_BT_isEnabled)
SCMD_CMD_CLASS(BTEnable, CMD_CAM_BT_Enable)
SCMD_CMD_CLASS(GetBTDevStatus, CMD_CAM_BT_getDEVStatus)
SCMD_CMD_CLASS(GetBTHostNum, CMD_CAM_BT_getHostNum)
SCMD_CMD_CLASS(GetBTHostInfor, CMD_CAM_BT_getHostInfor)
SCMD_CMD_CLASS(DoScanBT, CMD_CAM_BT_doScan)
SCMD_CMD_CLASS(DoBindBT, CMD_CAM_BT_doBind)
SCMD_CMD_CLASS(DoUnBindBT, CMD_CAM_BT_doUnBind)
SCMD_CMD_CLASS(DoSetOBDTypes, CMD_CAM_BT_setOBDTypes)
//SCMD_CMD_CLASS(BTReserved, CMD_CAM_BT_RESERVED)
//1.2 done

//1.3
SCMD_CMD_CLASS(FormatTF, CMD_CAM_Format_TF)
SCMD_CMD_CLASS(GetLanguageList, CMD_CAM_Get_Language_List)
SCMD_CMD_CLASS(GetLanguage, CMD_CAM_Get_Language)
SCMD_CMD_CLASS(SetLanguage, CMD_CAM_Set_Language)
SCMD_CMD_CLASS(SetWorkingMode, CMD_CAM_Set_WorkingMode)
SCMD_CMD_CLASS(GetWorkingMode, CMD_CAM_Get_WorkingMode)
SCMD_CMD_CLASS(SetPhotoLapseInterval, CMD_CAM_Set_PhotoLapse_Interval)
SCMD_CMD_CLASS(GetPhotoLapseInterval, CMD_CAM_Get_PhotoLapse_Interval)
// end of API v1.3

// 1.4
SCMD_CMD_CLASS(ScanNetworks, CMD_Network_ScanHost)
SCMD_CMD_CLASS(ConnectHotSpot, CMD_Network_ConnectHotSpot)
SCMD_CMD_CLASS(SetUdcPowerDelay, CMD_Set_Auto_Power_Off_Delay)
SCMD_CMD_CLASS(GetUdcPowerDelay, CMD_Get_Auto_Power_Off_Delay)
SCMD_CMD_CLASS(SetSpeakerStatus, CMD_Set_Speaker_Status)
SCMD_CMD_CLASS(GetSpeakerStatus, CMD_Get_Speaker_Status)
SCMD_CMD_CLASS(SetAutoBrightness, CMD_Set_Display_Auto_Brightness)
SCMD_CMD_CLASS(GetAutoBrightness, CMD_Get_Display_Auto_Brightness)
SCMD_CMD_CLASS(SetDisplayBrightness, CMD_Set_Display_Brightness)
SCMD_CMD_CLASS(GetDisplayBrightness, CMD_Get_Display_Brightness)
SCMD_CMD_CLASS(SetDisplayOffTime, CMD_Set_Display_Auto_Off_Time)
SCMD_CMD_CLASS(GetDisplayOffTime, CMD_Get_Display_Auto_Off_Time)
SCMD_CMD_CLASS(FactoryReset, CMD_Factory_Reset)

SCMD_CMD_CLASS(GetClientsInfo, CMD_Get_Connected_Clients_Info)
SCMD_CMD_CLASS(AddConnection, CMD_CAM_NEW_CONNECTION)
SCMD_CMD_CLASS(RemoveConnection, CMD_CAM_CLOSE_CONNECTION)
// end of API v1.4

// 1.5
SCMD_CMD_CLASS(GetOBDVIN, CMD_GET_OBD_VIN)
SCMD_CMD_CLASS(SetVINStyle, CMD_SET_CLIP_VIN_STYLE)
SCMD_CMD_CLASS(GetVINStyle, CMD_GET_CLIP_VIN_STYLE)
SCMD_CMD_CLASS(SetScreenSaverStyle, CMD_SET_Screen_Saver_Style)
SCMD_CMD_CLASS(GetScreenSaverStyle, CMD_GET_Screen_Saver_Style)
SCMD_CMD_CLASS(SetUnitsSystem, CMD_SET_Units_System)
SCMD_CMD_CLASS(GetUnitsSystem, CMD_GET_Units_System)
// end of API v1.5
#endif

