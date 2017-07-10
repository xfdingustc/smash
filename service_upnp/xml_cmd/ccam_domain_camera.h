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
//#include ""

#ifdef isServer
#include "Class_PropsEventThread.h"
#include "class_ipc_rec.h"
#include "Class_Upgrader.h"
class cameraEventGenerator
	: public Domain_EvnetGenerator
	, public IPC_AVF_Delegate 
	, public CPropertyEventHandler
	, public TSUpgradeCB
{
public:
	cameraEventGenerator(SCMD_Domain *domain);
	~cameraEventGenerator(){};
	virtual void onNewFileReady(char* path);
	virtual void onCameraModeChange();
	virtual void onRecodeStateChange(IPC_AVF_Delegate::recordState state);
	virtual void onStartRecordError(IPC_AVF_Delegate::recordStartError error);
	
	virtual void EventPropertyUpdate(changed_property_type state);

	static void SendAudioState(EnumedStringCMD* sender, SCMD_Domain *domain);
	static void SendStorageState(EnumedStringCMD* sender, SCMD_Domain *domain);
	static void SendPowerState(EnumedStringCMD* sender, SCMD_Domain *domain);
	static void SendGPSState(EnumedStringCMD* sender, SCMD_Domain *domain);

	virtual void onUpgradeResult(int ok);
private:

};
#endif

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

SCMD_CMD_CLASS(Poweroff, CMD_Cam_PowerOff)
SCMD_CMD_CLASS(Reboot, CMD_Cam_Reboot)

SCMD_CMD_CLASS(GetWlanMode, CMD_Network_GetWLanMode)
SCMD_CMD_CLASS(GetHostNum, CMD_Network_GetHostNum)
SCMD_CMD_CLASS(GetHostInfor, CMD_Network_GetHostInfor)

SCMD_CMD_CLASS(AddHost, CMD_Network_AddHost)
SCMD_CMD_CLASS(RmvHost, CMD_Network_RmvHost)
SCMD_CMD_CLASS(ConnectHost, CMD_Network_ConnectHost)

SCMD_CMD_CLASS(SyncTime, CMD_Network_Synctime)
SCMD_CMD_CLASS(SetMic, CMD_audio_setMic)
SCMD_CMD_CLASS(GetMicState, CMD_audio_getMicState)

SCMD_CMD_CLASS(GetVersion, CMD_fw_getVersion)
SCMD_CMD_CLASS(NewVersion, CMD_fw_newVersion)
SCMD_CMD_CLASS(DoUpgrade, CMD_fw_doUpgrade)

#ifndef isServer
SCMD_CMD_CLASS(MsgStorageInfor, CMD_Cam_msg_Storage_infor)
SCMD_CMD_CLASS(MsgStorageSpaceInfor, CMD_Cam_msg_StorageSpace_infor)
SCMD_CMD_CLASS(MsgBatteryInfor, CMD_Cam_msg_Battery_infor)
SCMD_CMD_CLASS(MsgPowerInfor, CMD_Cam_msg_power_infor)
SCMD_CMD_CLASS(MsgBTinfor, CMD_Cam_msg_BT_infor)
SCMD_CMD_CLASS(MsgGPSInfor, CMD_Cam_msg_GPS_infor)
SCMD_CMD_CLASS(MsgInternetInfor, CMD_Cam_msg_Internet_infor)
SCMD_CMD_CLASS(MsgMicInfor, CMD_Cam_msg_Mic_infor)
#endif

#endif

