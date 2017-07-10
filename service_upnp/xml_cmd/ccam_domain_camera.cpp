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


#ifdef isServer
#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"
#include <math.h>

static cameraEventGenerator* pCEGobj = NULL;
static CTSUpgrader* pUpgobj = NULL;
extern void DestroyUpnpService(bool bReboot);

static int savedNumber = 0;
#define MAX_AP_NUMBER 256
#define MAX_VERSION_LENTH 64
static agcmd_wpa_list_info_s AP_List[MAX_AP_NUMBER];
extern int agclkd_set_rtc(time_t utc);
#endif

SCMD_DOMIAN_CONSTRUCTOR(DomainCamera, CMD_Domain_cam, CMD_Cam_num)
{
#ifdef isServer
	pCEGobj = new cameraEventGenerator(this);
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

	SCMD_CMD_NEW(Poweroff, this);
	SCMD_CMD_NEW(Reboot, this);

	SCMD_CMD_NEW(GetWlanMode, this);
	SCMD_CMD_NEW(GetHostNum, this);
	SCMD_CMD_NEW(GetHostInfor, this);
	SCMD_CMD_NEW(AddHost, this);
	SCMD_CMD_NEW(RmvHost, this);
	SCMD_CMD_NEW(ConnectHost, this);
	
	SCMD_CMD_NEW(SyncTime, this);
	SCMD_CMD_NEW(SetMic, this);
	SCMD_CMD_NEW(GetMicState, this);

	SCMD_CMD_NEW(GetVersion, this);
	SCMD_CMD_NEW(NewVersion, this);
	SCMD_CMD_NEW(DoUpgrade, this);
	
#else
	SCMD_CMD_NEW(GetModeResult, this);
	SCMD_CMD_NEW(GetNameResult, this);
	SCMD_CMD_NEW(SetNameResult, this);
	
	SCMD_CMD_NEW(GetStateResult, this);
	SCMD_CMD_NEW(GetTimeResult, this);

	SCMD_CMD_NEW(MsgStorageInfor, this);
	SCMD_CMD_NEW(MsgStorageSpaceInfor, this);
	SCMD_CMD_NEW(MsgBatteryInfor, this);
	SCMD_CMD_NEW(MsgPowerInfor, this);
	SCMD_CMD_NEW(MsgBTinfor, this);
	SCMD_CMD_NEW(MsgGPSInfor, this);
	SCMD_CMD_NEW(MsgInternetInfor, this);
	SCMD_CMD_NEW(MsgMicInfor, this);

	SCMD_CMD_NEW(GetWlanMode, this);
	SCMD_CMD_NEW(GetHostNum, this);
	SCMD_CMD_NEW(GetHostInfor, this);
	//SCMD_CMD_NEW(AddHost, this);
	//SCMD_CMD_NEW(RmvHost, this);
	//SCMD_CMD_NEW(ConnectHost, this);
#endif
}

SCMD_DOMIAN_DISTRUCTOR(DomainCamera)
{
#ifdef isServer
	delete pCEGobj;
#endif
}

#ifdef isServer
SCMD_CMD_EXECUTE(Reboot)
{
	DestroyUpnpService(true);
	return 0;
}
SCMD_CMD_EXECUTE(Poweroff)
{
	DestroyUpnpService(false);
	return 0;
}

SCMD_CMD_EXECUTE(ConnectHost)
{
#ifdef isServer
	EnumedStringCMD* cmd = p;
	int mode = atoi(cmd->getPara1());
	IPC_AVF_Client::getObject()->SetWifiMode(mode, cmd->getPara2());
#endif
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
	return 0;
}

SCMD_CMD_EXECUTE(AddHost)
{
	EnumedStringCMD* cmd = p;
	printf("add host : %s, pwd: %s\n", cmd->getPara1(), cmd->getPara2());

	char wlan_prop[AGCMD_PROPERTY_SIZE];
	char prebuild_prop[AGCMD_PROPERTY_SIZE];

	agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
	agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
	if (cmd->getPara2() == NULL ||
		strcmp(cmd->getPara2(), "")==0){
		agcmd_wpa_add(wlan_prop, cmd->getPara1(), "open", NULL);
		printf("Open SSID\n");
	}else{
		agcmd_wpa_add(wlan_prop, cmd->getPara1(), "auto", cmd->getPara2());
	}
	savedNumber = agcmd_wpa_list(wlan_prop, AP_List, MAX_AP_NUMBER);
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
	//printf("-- saved number:%d\n", savedNumber);
	char tmp[64];
	char tmp1[64];
	sprintf(tmp, "%d", savedNumber);
	sprintf(tmp1, "%d", MAX_AP_NUMBER);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Network_GetHostNum, tmp, tmp1);
    StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
	return 0;

}

SCMD_CMD_EXECUTE(GetHostInfor)
{
	EnumedStringCMD* cmd = p;
	int index = atoi(cmd->getPara1());
	//char tmp[256];
	char* tmp;
	if(index < MAX_AP_NUMBER)
	{
		 tmp = AP_List[index].ssid;
	}
	else
	{
		tmp = (char*)"out max access number";
	}
	EnumedStringCMD result(CMD_Domain_cam, CMD_Network_GetHostInfor, tmp, NULL);
    StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
	return 0;
}

SCMD_CMD_EXECUTE(GetWlanMode)
{
#ifdef isServer
	EnumedStringCMD* cmd = p;
	int mode = 2;
	char tmp[256];
	agcmd_property_get(WIFI_KEY, tmp, "");
	if(strcmp(tmp, WIFI_AP_KEY) == 0) {
		mode = 0;
	} else if(strcmp(tmp, WIFI_CLIENT_KEY) == 0) {
		mode = 1;
	}
	sprintf(tmp, "%d", mode);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Network_GetWLanMode, tmp, NULL);
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
#endif
	return 0;
}

SCMD_CMD_EXECUTE(SyncTime)
{
	EnumedStringCMD* cmd = p;
	//CSDSystemTime* pSystemTime = (CSDSystemTime*)p[0];
	//CSDSystemTimeZone* pSystemTimeZone = (CSDSystemTimeZone*)_ppInStates[1];;
	//pSystemTime->setWishAsCurrent();
	//pSystemTimeZone->setWishAsCurrent();
	printf("-- set time %s, %s\n", cmd->getPara1(), cmd->getPara2());
	time_t t, tRemote = 0;
	time(&t);
	tRemote = (time_t)atoi(cmd->getPara1());
	//printf("---system time: %d, remote time:%d \n, timezone : %f \n", t, tRemote, pSystemTimeZone->getWish());
	if(fabs(difftime(t, tRemote)) > 120.0)
	{
		struct timeval tt;
		tt.tv_sec = tRemote;
		tt.tv_usec = 0;
		struct timezone tz;
		tz.tz_minuteswest = (int)(atoi(cmd->getPara2())*60);
		tz.tz_dsttime = 0;
		settimeofday(&tt, &tz);
		agclkd_set_rtc(tRemote);
	}
	{
		char tmp[256];
		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "%sGMT%+d",timezongDir, atoi(cmd->getPara2()));
		int rt = remove(timezongLink);
		rt = symlink(tmp, timezongLink);
	}
	// re start upnp service.
	//CUpnpThread::getInstance()->RequireUpnpReboot();
	return 0;
}

SCMD_CMD_EXECUTE(SetMic)
{
	EnumedStringCMD* cmd = p;
	int bMute = atoi(cmd->getPara1());
	int gain  = atoi(cmd->getPara2());
	if (bMute) IPC_AVF_Client::getObject()->SetMicVolume(0);
	else
	{
		if (gain <=0 || gain >= 9) gain = 5;
		IPC_AVF_Client::getObject()->SetMicVolume(gain);
	}
	return 0;
}

SCMD_CMD_EXECUTE(GetMicState)
{
	EnumedStringCMD* cmd = p;
	cameraEventGenerator::SendAudioState(cmd,NULL);
	return 0;
}

//
SCMD_CMD_EXECUTE(GetVersion)
{
	EnumedStringCMD* cmd = p;

	char HW_Version[MAX_VERSION_LENTH] = "";
	char Device_SN[MAX_VERSION_LENTH] = "";
	char FW_Version[MAX_VERSION_LENTH] = "";
	agcmd_property_get("prebuild.system.bsp", HW_Version, "NA");
	agcmd_property_get("system.device.sn", Device_SN, "NA");
	agcmd_property_get("prebuild.system.version", FW_Version, "NA");
	strcat(Device_SN, "@");
	strcat(Device_SN, HW_Version);
	//printf("-- saved number:%d\n", savedNumber);
	EnumedStringCMD result(CMD_Domain_cam, CMD_fw_getVersion, Device_SN, FW_Version);
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
	return 0;
}

SCMD_CMD_EXECUTE(NewVersion)
{
	EnumedStringCMD* cmd = p;
	printf("NewVersion: %s\n\t%s\n", cmd->getPara1(),  cmd->getPara2());
	pUpgobj = CTSUpgrader::getInstance(cmd->getPara1(), pCEGobj);
	return 0;
}

SCMD_CMD_EXECUTE(DoUpgrade)
{
	DestroyUpnpService(true);
	return 0;
}

#else
SCMD_CMD_EXECUTE(GetHostNum)
{
	return 0;
}

SCMD_CMD_EXECUTE(GetWlanMode)
{
	return 0;
}

SCMD_CMD_EXECUTE(GetHostInfor)
{
	return 0;
}

#endif

SCMD_CMD_EXECUTE(GetAPIVersion)
{
#ifdef isServer
	EnumedStringCMD* cmd = p;
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_getAPIVersion, SERVICE_UPNP_VERSION, NULL);
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(IsAPISupported)
{
#ifdef isServer
	EnumedStringCMD* cmd = p;
	bool issupported = false;
	int apidom = atoi(cmd->getPara1());
	int apiidx = atoi(cmd->getPara2());
	if ( (apidom == CMD_Domain_cam) && (apiidx < CMD_Cam_num)) issupported = true;
	if ( (apidom == CMD_Domain_rec) && (apiidx < CMD_Rec_num)) issupported = true;
	char apiwithdomain[16];
	sprintf(apiwithdomain, "%d", (apidom << 16) + apiidx);
	printf("IsAPISupported: %s, %d\n", apiwithdomain, issupported);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_isAPISupported, apiwithdomain, issupported?(char*)"1":(char*)"0");
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
#endif
	return 0;
}
SCMD_CMD_EXECUTE(GetName)
{
#ifdef isServer
	EnumedStringCMD* cmd = p;
	char nam[256];
	memset (nam, 0, sizeof(nam));
	agcmd_property_get("upnp.camera.name", nam , "No Named");
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_get_Name_result, nam,NULL);
    StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
	else // evnet broad cast
	{
		BroadCaster* b = this->getDomainObj()->getBroadCaster();
		if(b)
			b->BroadCast(&ev);
	}
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(GetNameResult)
{
#ifndef isServer	
	EnumedStringCMD* cmd = p;
	//cam_mode = atoi(cmd->getPara1());
	printf("--camera name:%s\n", cmd->getPara1());
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(SetName)
{
#ifdef isServer
	EnumedStringCMD* cmd = p;
	printf("on set name : %s\n", cmd->getPara1());
	//char nam[256];
	//memset (nam, 0, sizeof(nam));
	agcmd_property_set("upnp.camera.name", cmd->getPara1());
	IPC_AVF_Client::getObject()->SetCameraName(cmd->getPara1());
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(SetNameResult)
{
#ifndef isServer	
	//EnumedStringCMD* cmd = p;
	//cam_mode = atoi(cmd->getPara1());
	//printf("--camera name:%s\n", cmd->getPara1());
    //this->getDomainObj()->getResponder()->OnState(this->getDomain(), cmd->getCMD(), cmd->getPara1());
#endif
	return 0;
}

SCMD_CMD_EXECUTE(GetMode)
{
#ifdef isServer
	EnumedStringCMD* cmd = p;

	IPC_AVF_Client::AVF_Mode mode = IPC_AVF_Client::getObject()->getAvfMode();
	State_Cam_Mode cam_mode;
	switch(mode)
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
	sprintf(tmp, "%d",cam_mode);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_getMode_result, tmp,NULL);
    StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
	else // evnet broad cast
	{
		BroadCaster* b = this->getDomainObj()->getBroadCaster();
		if(b)
			b->BroadCast(&ev);
	}
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(GetModeResult)
{
#ifndef isServer	
	EnumedStringCMD* cmd = p;
	int cam_mode;
	cam_mode = atoi(cmd->getPara1());
	printf("--camera mode: %d, %s\n", cam_mode, cmd->getPara1());
#endif
	return 0;
}

SCMD_CMD_EXECUTE(GetState)
{
#ifdef isServer	
	EnumedStringCMD* cmd = p;
	
	int state = IPC_AVF_Client::getObject()->getRecordState();
	char tmp[64];
	sprintf(tmp, "%d",state);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_get_State_result, tmp,NULL);
    StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
	{
		printf("SCMD_CMD_EXECUTE: 1\n");
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
	}
	else
	{
		printf("SCMD_CMD_EXECUTE: 2\n");
		BroadCaster* b = this->getDomainObj()->getBroadCaster();
		if(b)
			b->BroadCast(&ev);
	}
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(GetStateResult)
{
#ifndef isServer	
	EnumedStringCMD* cmd = p;
	printf("--camera state:%s\n", cmd->getPara1());
#endif
	return 0;
}

SCMD_CMD_EXECUTE(StartRecord)
{
#ifdef isServer	
	IPC_AVF_Client::getObject()->StartRecording(NULL, 0);
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(StopRecord)
{
#ifdef isServer	
	IPC_AVF_Client::getObject()->StopRecording();
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(SetStreamSize)
{
#ifdef isServer
	EnumedStringCMD* cmd = p;
	printf("on set stream : %s\n", cmd->getPara1());
	int bBig = atoi(cmd->getPara1());
	IPC_AVF_Client::getObject()->switchStream(bBig);
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(GetTime)
{
#ifdef isServer	
	EnumedStringCMD* cmd = p;
	UINT64 time = IPC_AVF_Client::getObject()->getRecordingTime();
	char tmp[64];
	sprintf(tmp, "%lld",time);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_get_time_result, tmp,NULL);
    StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(cmd != NULL)
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
	else
	{
		BroadCaster* b = this->getDomainObj()->getBroadCaster();
		if(b)
			b->BroadCast(&ev);
	}
	return 0;
#endif
	return 0;
}

SCMD_CMD_EXECUTE(GetTimeResult)
{
#ifndef isServer	
	EnumedStringCMD* cmd = p;
	printf("--recording time:%s\n", cmd->getPara1());
	return 0;
#endif
	return 0;
}

#ifdef isServer
SCMD_CMD_EXECUTE(GetAllInfor)
{
	EnumedStringCMD* cmd = p;
	cameraEventGenerator::SendAudioState(cmd,NULL);
	cameraEventGenerator::SendPowerState(cmd,NULL);
	cameraEventGenerator::SendStorageState(cmd,NULL);
	cameraEventGenerator::SendGPSState(cmd,NULL);
	return 0;
}

SCMD_CMD_EXECUTE(GetStorageInfor)
{
	EnumedStringCMD* cmd = p;
	cameraEventGenerator::SendStorageState(cmd,NULL);
	return 0;
}

cameraEventGenerator::cameraEventGenerator
	(SCMD_Domain *domain
	) : Domain_EvnetGenerator(domain)
{
	IPC_AVF_Client::getObject()->setDelegate(this);
	CPropsEventThread::getInstance()->AddPEH(this);
}

void cameraEventGenerator::onNewFileReady(char* path)
{
	printf("--->cameraEventGenerator--->onNewFileReady\n");
}

void cameraEventGenerator::onCameraModeChange()
{
	this->Event(CMD_Cam_getMode);
}

void cameraEventGenerator::onRecodeStateChange(IPC_AVF_Delegate::recordState state)
{
	char tmp[64];
	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%d",state);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_get_State_result, tmp,NULL);
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	//printf("broadcast -- record state: %d\n", state);
	this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::onStartRecordError(IPC_AVF_Delegate::recordStartError state)
{
	char tmp[64];
	memset(tmp, 0, sizeof(tmp));
	Rec_Error_code error = Error_code_none;
	switch(state)
	{
		case IPC_AVF_Delegate::recordStartError_notf:
			error = Error_code_notfcard;
			break;

		case IPC_AVF_Delegate::recordStartError_tfFull:
			error = Error_code_tfFull;
			break;
	}
	sprintf(tmp, "%d",error);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Rec_error, tmp,NULL);
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	//printf("broadcast -- record state: %d\n", state);
	this->GetDomainObj()->BroadCast(&ev);
}

void cameraEventGenerator::EventPropertyUpdate(changed_property_type state)
{
	//printf("+++++--cameraEventGenerator\n");
	switch(state)
	{
		case property_type_wifi:
			
			break;
			
		case property_type_file:
			
			break;
			
		case property_type_audio:
			SendAudioState(NULL, this->GetDomainObj());
			break;
			
		case property_type_Bluetooth:
			
			break;
			
		case property_type_storage:
			SendStorageState(NULL, this->GetDomainObj());
			break;
			
		case property_type_power:
			SendPowerState(NULL, this->GetDomainObj());
			break;

		default:
				break;
	}
}

void cameraEventGenerator::SendAudioState(EnumedStringCMD* sender, SCMD_Domain *domain)
{
	char tmp[256];
	memset(tmp, 0, sizeof(tmp));
	char tmp2[56];
	memset(tmp2, 0, sizeof(tmp2));
	int rt;
	agcmd_property_get(audioGainPropertyName, tmp2, "0");
	//printf("camrea event mic : %d, %s\n", rt, tmp2);
	int vol = atoi(tmp2);
	rt = (vol == 0)? State_Mic_MUTE : State_Mic_ON;
	sprintf(tmp,"%d", rt);
	sprintf(tmp2,"%d", vol);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_Mic_infor, tmp,tmp2);
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	//printf("--send out : %s,\n", ev.getBuffer());
	if(sender)
	{
		sender->SendBack(ev.getBuffer(), ev.getBufferLen());
	}
	else if(domain)
	{
		domain->BroadCast(&ev);
	}
}

void cameraEventGenerator::SendPowerState(EnumedStringCMD* sender, SCMD_Domain *domain)
{
	char tmp1[64];
	char tmp2[64];
	char tmp3[64];
	char tmp4[64];
	int rt;
	memset(tmp1, 0, sizeof(tmp1));
	memset(tmp2, 0, sizeof(tmp2));
	memset(tmp3, 0, sizeof(tmp3));
	memset(tmp4, 0, sizeof(tmp4));

	agcmd_property_get(powerstateName, tmp1, "Unknown");
	agcmd_property_get(propPowerSupplyOnline, tmp2, "0");
	agcmd_property_get(propPowerSupplyBV, tmp3, "3700");
	agcmd_property_get(propPowerSupplyBCapacity, tmp4, "0");

	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_power_infor, tmp1, tmp2);
	EnumedStringCMD result1(CMD_Domain_cam, CMD_Cam_msg_Battery_infor, tmp3, tmp4);
	StringCMD *pp[2];
	pp[0] = &result;
	pp[1] = &result1;
	StringEnvelope ev(pp, 2);
	if (sender) {
		sender->SendBack(ev.getBuffer(), ev.getBufferLen());
	} else if(domain) {
		domain->BroadCast(&ev);
	}
}

void cameraEventGenerator::SendStorageState
	(EnumedStringCMD* sender
	, SCMD_Domain *domain)
{
	char tmp1[64];
	char tmp2[64];
	char tmp3[64];
	int rt;
	memset(tmp1, 0, sizeof(tmp1));
	memset(tmp2, 0, sizeof(tmp2));
	memset(tmp3, 0, sizeof(tmp3));
	//char tmp4[64];
	agcmd_property_get(SotrageStatusPropertyName, tmp1, "");
	agcmd_property_get(SotrageSizePropertyName, tmp2, "");
#ifdef SupportNewVDBAVF
	bool rdy;
	unsigned int free;
	long long Total;
	IPC_AVF_Client::getObject()->GetVDBState(rdy, free, Total);
	sprintf(tmp3, "%llu", (unsigned long long)free*1024*1024);
#else
	agcmd_property_get(SotrageFreePropertyName, tmp3, "");
#endif
	//printf("camrea event storage : %s, %s, %s\n", tmp1, tmp2, tmp3);
	if(strcmp(tmp1, "mount_ok") == 0)
	{
		rt = State_storage_ready;		
	}
	else if(strcmp(tmp1, "unknown") == 0)
	{
		rt = State_storage_noStorage;
		IPC_AVF_Client::getObject()->onTFReady(false);
	}
	else if(strcmp(tmp1, "checking") == 0)
	{
		rt = State_storage_loading;
	}
	else if(strcmp(tmp1, "mount_fail") == 0)
	{
		rt = State_storage_error;
	}
	else if(strcmp(tmp1, "ready") == 0)
	{
		rt = State_storage_usbdisc;
	}
	sprintf(tmp1, "%d", rt);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_Storage_infor, tmp1, NULL);
	EnumedStringCMD result1(CMD_Domain_cam, CMD_Cam_msg_StorageSpace_infor, tmp2,tmp3);
	StringCMD *pp[2];
	pp[0] = &result;
	pp[1] = &result1;
	StringEnvelope ev(pp, 2);
	//printf("--send out : %s,\n", ev.getBuffer());
	if(sender)
	{
		sender->SendBack(ev.getBuffer(), ev.getBufferLen());
	}
	else if(domain)
	{
		domain->BroadCast(&ev);
	}
}

void cameraEventGenerator::SendGPSState
	(EnumedStringCMD* sender
	, SCMD_Domain *domain)
{

	int rt = IPC_AVF_Client::getObject()->getGpsState();
	State_GPS state = State_GPS_off;
	switch(rt)
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
	sprintf(tmp1, "%d", state);
	EnumedStringCMD result(CMD_Domain_cam, CMD_Cam_msg_GPS_infor, tmp1, NULL);
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	if(sender)
	{
		sender->SendBack(ev.getBuffer(), ev.getBufferLen());
	}
	else if(domain)
	{
		domain->BroadCast(&ev);
	}
}

void cameraEventGenerator::onUpgradeResult(int stat)
{
	char tmp1[64];
	sprintf(tmp1, "%d", stat);
	EnumedStringCMD result(CMD_Domain_cam, CMD_fw_newVersion, tmp1, NULL);
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	this->GetDomainObj()->BroadCast(&ev);
}
#else

SCMD_CMD_EXECUTE(MsgStorageInfor)
{
	EnumedStringCMD* cmd = p;
	printf("---storage: %s, %s\n", cmd->getPara1(), cmd->getPara2());
    return 0;
}
SCMD_CMD_EXECUTE(MsgStorageSpaceInfor)
{
	EnumedStringCMD* cmd = p;
	printf("---storage Space: %s, %s\n", cmd->getPara1(), cmd->getPara2());
     return 0;
}

SCMD_CMD_EXECUTE(MsgBatteryInfor)
{
	EnumedStringCMD* cmd = p;
	printf("---Battery: %s, %s\n", cmd->getPara1(), cmd->getPara2());
     return 0;
}

SCMD_CMD_EXECUTE(MsgPowerInfor)
{
	EnumedStringCMD* cmd = p;
	printf("---Power: %s, %s\n", cmd->getPara1(), cmd->getPara2());
     return 0;
}

SCMD_CMD_EXECUTE(MsgBTinfor)
{
	EnumedStringCMD* cmd = p;
	printf("---BT: %s, %s\n", cmd->getPara1(), cmd->getPara2());
     return 0;
}

SCMD_CMD_EXECUTE(MsgGPSInfor)
{
	EnumedStringCMD* cmd = p;
	printf("---GPS: %s, %s\n", cmd->getPara1(), cmd->getPara2());
     return 0;
}

SCMD_CMD_EXECUTE(MsgInternetInfor)
{
	EnumedStringCMD* cmd = p;
	printf("---Internet: %s, %s\n", cmd->getPara1(), cmd->getPara2());
     return 0;
}

SCMD_CMD_EXECUTE(MsgMicInfor)
{
	EnumedStringCMD* cmd = p;
	printf("---Mic audio: %s, %s\n", cmd->getPara1(), cmd->getPara2());
     return 0;
}
#endif

