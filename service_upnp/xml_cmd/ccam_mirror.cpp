
/*****************************************************************************
 * ccam_mirror.cpp
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
 * 
 *****************************************************************************/
#include "ccam_mirror.h"
#include "string.h"
#include "unistd.h"

CCAM_mirror::CCAM_mirror
    (char* address
     , int16_t port)
    :_port(port)
{
#ifdef isServer
	_ccamService = new TCPSessionService
		(_port
		, 10240
		, 24
		, 10240);
	_processor = new CloudCameraCMDProcessor(this);
	_ccamService->SetDelegate(_processor);
	_processor->Go();
	_ccamService->Go();
#else
	_pTCPSessionClient = new TCPSessionClient(10240, 10240, (char*)"tcp session client");
    _processor = new CloudCameraCMDProcessor(this);
    _pTCPSessionClient->SetDelegate(_processor);
    _processor->Go();
#endif
    memset(_address, 0, sizeof(_address));
    if(address)
        memcpy(_address, address, strlen(address));
}

CCAM_mirror::~CCAM_mirror()
{
#ifdef isServer
	_ccamService->Stop();
	_processor->Stop();
	delete _processor;
	delete _ccamService;
#else
    _processor->Stop();
	delete _pTCPSessionClient;
    delete _processor;
#endif
}

#ifdef isServer
void CCAM_mirror::BroadCast(StringEnvelope* ev)
{
	if((_ccamService)&&(ev))
	{
		_ccamService->BroadCast(ev->getBuffer(), strlen(ev->getBuffer()));
	}
}
#else
bool CCAM_mirror::Connect()
{
    bool rt = 0;
    int count = 1;
    while(count > 0)
    {
        rt = _pTCPSessionClient->Connect(_address, _port);
        //printf("--- connect result : %d\n", rt);
        if(rt)
            break;
        else
        {
            sleep(1);
            count --;
        }
    }
    return rt;
}

void CCAM_mirror::Disconnect()
{
    _pTCPSessionClient->Disconnect();
    _pTCPSessionClient->Stop();
    //_processor->Stop();
}

void CCAM_mirror::SendCmdTest()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_set_Name, (char*)"TEST NAME", NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::getCameraMode()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_getMode, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::getCameraState()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_get_State, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::StartRecord()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_start_rec, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::StopRecord()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_stop_rec, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::RecGetTimeInfo()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_get_time, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::GetCameraName()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_get_Name, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::SetCameraName(const char*name)
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_set_Name, (char*)name, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::setStream(bool bBig)
{
    char tmp[5];
    sprintf(tmp, "%d", bBig);
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_set_StreamSize, (char*)tmp, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
    
}

void CCAM_mirror::getAllInfor()
{
    //char tmp[5];
    //sprintf(tmp, "%d", bBig);
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_get_getAllInfor, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
    
}


void CCAM_mirror::getResolutionList()
{
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_List_Resolutions, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::getCurrentResolution()
{
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_get_Resolution, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::setResolution(int resolution)
{
    char tmp[5];
    sprintf(tmp, "%d", resolution);
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_Set_Resolution, tmp, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());

}

void CCAM_mirror::getQualityList()
{
    //char tmp[5];
    //sprintf(tmp, "%d", resolution);
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_List_Qualities, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::getCurrentQuality()
{
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_get_Quality, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::setQuality(int quality)
{
    char tmp[5];
    sprintf(tmp, "%d", quality);
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_Set_Quality, tmp, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}


void CCAM_mirror::getRecModeList()
{
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_List_RecModes, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::getCurrentRecMode()
{
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_get_RecMode, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::setRecMode(int mode)
{
    char tmp[5];
    sprintf(tmp, "%d", mode);
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_Set_RecMode, tmp, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::getColorModeList()
{
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_List_ColorModes, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::getColorMode()
{
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_get_ColorMode, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::setColorMode(int mode)
{
    char tmp[5];
    sprintf(tmp, "%d", mode);
    EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_Set_ColorMode, tmp, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::PowerOff()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_PowerOff, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::Reboot()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Cam_Reboot, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}


void CCAM_mirror::GetHostNumber()
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Network_GetHostNum, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::GetHostInfor(int index)
{
    char tmp[32];
    sprintf(tmp, "%d", index);
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Network_GetHostInfor, tmp, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::addHost(const char* name, const char* pwd)
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Network_AddHost, (char*)name, (char*)pwd);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}
void CCAM_mirror::rmvHost(const char* name)
{
    EnumedStringCMD cmd(CMD_Domain_cam, CMD_Network_RmvHost, (char*)name, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::SyncTime(unsigned int time, unsigned int timezone)
{
	char tmp[16];
	char tmp2[16];
	sprintf(tmp, "%d", time);
	sprintf(tmp2, "%d", timezone);
	EnumedStringCMD cmd(CMD_Domain_cam, CMD_Network_Synctime, tmp, tmp2);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::setMic(bool bMute, int gain)
{
	char tmp[8];
	sprintf(tmp, "%d", bMute);
	char tmp1[8];
	sprintf(tmp1, "%d", gain);
 	EnumedStringCMD cmd(CMD_Domain_cam, CMD_audio_setMic, tmp, tmp1);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());

}

void CCAM_mirror::getMicState()
{
 	EnumedStringCMD cmd(CMD_Domain_cam, CMD_audio_getMicState, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());

}

void CCAM_mirror::SetOverlay(bool bNameOn, bool bTimeOn, bool bGPSOn, bool bSpeed)
{
	char tmp[8];
	int  m = 0;
	if(bNameOn)
		m|=0x01;
	if(bTimeOn)
		m|=0x02;
	if(bGPSOn)
		m|=0x04;
	if(bSpeed)
		m|=0x08;
	sprintf(tmp, "%d", m);
	EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_setOverlay, tmp, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

void CCAM_mirror::GetOverlayState()
{
	EnumedStringCMD cmd(CMD_Domain_rec, CMD_Rec_getOverlayState, NULL, NULL);
    StringCMD* p = &cmd;
    StringEnvelope ev(&p, 1);
    _pTCPSessionClient->Send(ev.getBuffer(), (int)ev.getBufferLen());
}

int CCAM_mirror::OnCmd(EnumedStringCMD* cmd)
{
    if (!_delegate) {
        printf("---error----no call back object set\n");
        return 0;
    }
    switch (cmd->getDomain()) {
        case CMD_Domain_cam:
        {
            switch (cmd->getCMD()) {
                case CMD_Cam_get_Name_result:
                    _delegate->OnGetCameraName((char*)cmd->getPara1());
                    break;
                    
                case CMD_Cam_get_State_result:
                    printf("--- get state update: %s\n", cmd->getPara1());
                    _delegate->OnRecordState(atoi(cmd->getPara1()));
                    break;
                    
                case CMD_Cam_getMode_result:
                    //printf("--- get mode update: %s\n", cmd->getPara1());
                    _delegate->OnModeSwitch(atoi(cmd->getPara1()));
                    break;
                    
                case CMD_Cam_get_time_result:
                     _delegate->OnRecordTimeInfor(atoi(cmd->getPara1()));
                    break;
                    
                case CMD_Cam_msg_Storage_infor:
                    printf("-- storage infor : %s, %s\n", cmd->getPara1(), cmd->getPara2());
                   
                    break;
                    
                case CMD_Cam_msg_StorageSpace_infor:
                    //printf("-- storage space infor : %s, %s\n", cmd->getPara1(), cmd->getPara2());
                     _delegate->OnSpaceUpdate(atoll(cmd->getPara1()), atoll(cmd->getPara2()));
                    break;
                    
                case CMD_Cam_msg_Battery_infor:
                    //printf("-- battery infor : %s, %s\n", cmd->getPara1(), cmd->getPara2());
                    _delegate->OnBatteryVolume(atoi(cmd->getPara1()), atoi(cmd->getPara2()));
                    break;
                    
                case CMD_Cam_msg_power_infor:
                    printf("-- power infor : %s, %s\n", cmd->getPara1(), cmd->getPara2());
                    break;
                    
                case CMD_Cam_msg_BT_infor:
                    printf("-- bt infor : %s, %s\n", cmd->getPara1(), cmd->getPara2());
                    //_delegate->OnBTSate(atoi(cmd->getPara1()), cmd->getPara2());
                    break;
                    
                case CMD_Cam_msg_GPS_infor:
                    printf("-- gps infor : %s, %s\n", cmd->getPara1(), cmd->getPara2());
                    _delegate->OnGPSSate(atoi(cmd->getPara1()), (char*)cmd->getPara2());
                    break;
                    
                case CMD_Cam_msg_Mic_infor:
                    //printf("-- mic infor : %s, %s\n", cmd->getPara1(), cmd->getPara2());
                    _delegate->OnMicState(atoi(cmd->getPara1()), atoi(cmd->getPara1()));
                    break;
                    
                case CMD_Cam_msg_Internet_infor:
                    _delegate->OnInternetServerState(atoi(cmd->getPara1()));
                    break;
                    
                case CMD_Network_GetHostNum:
                    _delegate->OnHostNumber(atoi(cmd->getPara1()));
                    break;
                
                case CMD_Network_GetHostInfor:
                    _delegate->OnHostInfor((char*)cmd->getPara1());
                    break;

				case CMD_audio_getMicState:
					_delegate->OnMicState(atoi(cmd->getPara1()), atoi(cmd->getPara1()));
                    break;
                default:
                    //printf("--cmd  : %s\n", )
                    break;
            }
        }
            break;
        case CMD_Domain_rec:
        {
            switch (cmd->getCMD()) {
                case CMD_Rec_List_ColorModes:
                    _delegate->OnSupportedColorModeList(atoll(cmd->getPara1()));
                    break;
                case CMD_Rec_List_RecModes:
                    _delegate->OnSupportedRecModeList(atoll(cmd->getPara1()));
                    break;
                case CMD_Rec_List_Resolutions:
                    //printf("--- list resolution:%lld\n", atoll(cmd->getPara1()));
                    _delegate->OnSupportedResolutionList(atoll(cmd->getPara1()));
                    break;
                case CMD_Rec_List_Qualities:
                    _delegate->OnSupportedQualityList(atoll(cmd->getPara1()));
                    break;
                    
                case CMD_Rec_get_ColorMode:
                    _delegate->OnCurrentColorMod(atoi(cmd->getPara1()));
                    break;
                case CMD_Rec_get_RecMode:
                    _delegate->OnCurrentRecMode(atoi(cmd->getPara1()));
                    break;
                case CMD_Rec_get_Quality:
                    _delegate->OnCurrentQuality(atoi(cmd->getPara1()));
                    break;
                case CMD_Rec_get_Resolution:
                    _delegate->OnCurrentResolution(atoi(cmd->getPara1()));
                    break;
				case CMD_Rec_getOverlayState:
					{
						int m = atoi(cmd->getPara1());
						bool bName = m & 0x01;
						bool bTime = m & 0x02;
						bool bPosi = m & 0x04;
						bool bSpeed =  m & 0x08;
					_delegate->OnOverlayInfor(bName, bTime, bPosi, bSpeed);
					}
					break;
				default:
					break;
            }
        }
            break;
        default:
            break;
    }
    return 0;
}

int CCAM_mirror::OnConnect()
{
    ///printf("--- CCAM_mirror::OnConnect()\n");
    if (_delegate) {
        _delegate->onConnect();
    }
    return 0;
}

int CCAM_mirror::OnDisconnect()
{
    printf("--- CCAM_mirror::OnDisconnect()\n");
    if (_delegate) {
        _delegate->onDisconnect();
    }
    return 0;
}
//void CCAM_mirror::OnState(int state, int StateValue)
//{

//}

#endif
