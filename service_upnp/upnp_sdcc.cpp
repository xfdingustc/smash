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
#include "agbox.h"

#include "upnp_sdcc.h"
#include "GobalMacro.h"
#include "class_gsensor_thread.h"
#ifdef QRScanEnable
#include "sd_general_description.h"
#endif
static const char* CircleDir = "Circle Dir";
static const char* MarkDir = "Mark Dir";
static const char* InternalDir = "Internal Dir";

#define AGB_SSD1308_IOCTL_SET_CONTRAST _IOW('a', 0, int)
extern int TestWavePlay(char* fileName);

/***************************************************************
CUpnpCloudCamera
*/
CUpnpCloudCamera::CUpnpCloudCamera
	(const char* versionMajor
	,const char* versionMinor
	,const char* udn
	,const char* id
	,const char* sn
	):inherited(udn//"uuid:0022B09E-2012-1979-0110-000000000003"
			,id //"uuid:0022B09E-2012-1979-0110-000000000003"
			,"urn:schemas-upnp-org:device:SDCloudCamera:1"
			,DEVICE_Name
			,DEVICE_Type
			,DEVICE_Version
			,sn
			,1
			,6
			,versionMajor
			,versionMinor)
	,_heartBeat(0)
{
	_pRecordService = new CUpnpSDRecord(this);	
	this->AddService(_pRecordService);

	_pStreamService = new CUpnpSDLiveStream(this);
	this->AddService(_pStreamService);	

	_pPower = new CUpnpSDPower(this);
	this->AddService(_pPower);

	_pNetwork = new  CUpnpSDNetwork(this);
	this->AddService(_pNetwork);

	_pStorage = new CUpnpSDStorage(this);
	this->AddService(_pStorage);

	_pDecord =  new CUpnpSDDecode(this);
	this->AddService(_pDecord);

	CBackupDirManager::Create();
	SetDisplayBrightness(0);
}


CUpnpCloudCamera::~CUpnpCloudCamera() 
{
	CBackupDirManager::Destroy();
	printf("delete camera\n");
	delete _pDecord;
	delete _pStreamService;
	delete _pPower;
	delete _pNetwork;
	delete _pStorage;
	delete _pRecordService;
}

void CUpnpCloudCamera::SendHeartBeat()
{
	_pPower->SendHeartBeat(_heartBeat);
	_heartBeat++;
}

void CUpnpCloudCamera::EventPropertyUpdate(changed_property_type state)
{
	//printf("--EventPropertyUpdate\n");
	switch(state)
	{
		case property_type_wifi:
			_pNetwork->EventPropertyUpdate();	
				break;
		case property_type_power:
			_pPower->EventPropertyUpdate(); 
				break;
		case property_type_Bluetooth:
				break;
		case property_type_audio:
			_pRecordService->EventPropertyUpdate();
				break;
		case property_type_storage:
			_pStorage->EventPropertyUpdate();
				break;
	}
	//printf("--EventPropertyUpdate 1\n");
	if(_ppObeservers != NULL)
	{
		_ppObeservers->OnPropertieChange(0, PropertyChangeType_unknow);
	}
}

void CUpnpCloudCamera::EventWriteWarnning()
{
	if(_ppObeservers != NULL)
	{
		_ppObeservers->OnPropertieChange(0, PropertyChangeType_FileWriteWarnning);
	}
	//TestWavePlay("/usr/local/share/ui/beep.wav");
}

void CUpnpCloudCamera::EventWriteError()
{
	if(_ppObeservers != NULL)
	{
		_ppObeservers->OnPropertieChange(0, PropertyChangeType_FileWriteError);
	}
	TestWavePlay("/usr/local/share/ui/beep.wav");
}

// ui interface
camera_State CUpnpCloudCamera::getRecState()
{
	int rt = this->_pRecordService->GetRecordState();
	camera_State state;
	switch(rt)
	{
		case CSDRSRecordState::RecordState_stop:
		case CSDRSRecordState::RecordState_stopping:
			state = camera_State_stop;
			break;
		case CSDRSRecordState::RecordState_recording:
		case CSDRSRecordState::RecordState_starting:
			{				
				if(SDWatchThread::getInstance()->inMarking())
					state = camera_State_marking;
				else
					state = camera_State_record;
			}
			break;
	}
	return state;	
}

UINT64 CUpnpCloudCamera::getRecordingTime()
{
	return this->_pRecordService->GetElapsedTime();
}
storage_State CUpnpCloudCamera::getStorageState()
{
	int rt = this->_pStorage->GetStorageState();
	storage_State state;
	int vdbSpaceState;
	//printf("--storage state : %d\n", rt);
	switch(rt)
	{
		case CSDStorageState::StorageState_Ready:
			vdbSpaceState = this->_pRecordService->GetStorageState();
			if(vdbSpaceState < 0)
			{
				state = storage_State_error;
			}
			else if(vdbSpaceState == 0)
			{
				state = storage_State_prepare;
			}
			else if(vdbSpaceState == 1)
			{
				state = storage_State_full;
			}
			else
				state = storage_State_ready;
			break;
		case CSDStorageState::StorageState_error:
		case CSDStorageState::StorageState_usbdisk:
			state = storage_State_error;
			break;
		case CSDStorageState::StorageState_loading:
			state = storage_State_prepare;
			break;
		case CSDStorageState::StorageState_offline:
			{				
				state = storage_State_noMedia;
			}
			break;
	}
	return state;	
}

int CUpnpCloudCamera::getStorageFreSpcByPrcn()
{
#ifdef SupportNewVDBAVF
	//IPC_AVF_Client::getObject()->switchToPlay(shortName);
	return _pRecordService->GetStorageFreeSpaceInPcnt();
#else
	return _pStorage->GetStorageFreSpc();
#endif
}

bool CUpnpCloudCamera::FormatStorage()
{
	return _pStorage->FormatTF();
}

wifi_mode CUpnpCloudCamera::getWifiMode()
{
	int rt = this->_pNetwork->GetWifiMode();
	wifi_mode state;
	switch(rt)
	{
		case CSDWirelessState::WirelessState_AP:
			state = wifi_mode_AP;
			break;
		case CSDWirelessState::WirelessState_Client:
			state = wifi_mode_Client;
			break; 
		case CSDWirelessState::WirelessState_Off:
			state = wifi_mode_Off;
			break;
	}
	return state;	
}
wifi_State CUpnpCloudCamera::getWifiState()
{
	int rt = this->_pNetwork->GetWifiState();
	//printf("--get wifi state: %d\n", rt);
	wifi_State state;
	switch(rt)
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
audio_State CUpnpCloudCamera::getAudioState()
{
	int rt = this->_pRecordService->GetAudioState();
	audio_State state;
	switch(rt)
	{
		case CSDRSAudioRecState::AudioRecState_mute:
			state = audio_State_off;
			break;
		case CSDRSAudioRecState::AudioRecState_on:
			state = audio_State_on;
			break; 
		default:
			state = audio_State_on;
			break;
	}
	return state;
}
rotate_State CUpnpCloudCamera::getRotationState()
{
	int switchOption = SW_ROTATE_LOCK_MIRROR;
	int rt = this->_pRecordService->GetRotate();
	if(rt)
	{
		if(switchOption == 1)
			return rotate_State_mirror_0;
		else if(switchOption == 0)
			return rotate_State_0;
		else
			return rotate_State_hdr_on; // sould be hdr on off
	}
	else
	{
		if(switchOption == 1)
			return rotate_State_mirror_180;
		else if(switchOption == 0)
			return rotate_State_180;
		else
			return rotate_State_hdr_off; // sould be hdr on off
	}
}

gps_state CUpnpCloudCamera::getGPSState()
{
	int rt = this->_pRecordService->GetGpsState();
	gps_state state;
	switch(rt)
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

UINT64 CUpnpCloudCamera::getPlayingTime()
{
	return 0;	
}


int CUpnpCloudCamera::getMovieDirNum()
{
#ifdef GSensorTestThread
	return 3;
#else
	return 2;	
#endif
}


void CUpnpCloudCamera::getMovieDirList
	(char** _ppList
	, int strLen
	, int strnum)
{
	memset(_ppList[0], 0, strLen);
	sprintf(_ppList[0], CircleDir);
	memset(_ppList[1], 0, strLen);
	sprintf(_ppList[1], MarkDir);
#ifdef GSensorTestThread
	memset(_ppList[2], 0, strLen);
	sprintf(_ppList[2], InternalDir);
#endif
}

int CUpnpCloudCamera::getCurrentDirType(char* dir)
{
	printf("get current dir type: %s\n", dir);
	if(!dir)
		return -1;
	if(strcmp(dir, CircleDir) == 0)
		return MovieDirType_Circle;
	else if(strcmp(dir, MarkDir) == 0)
		return MovieDirType_Marked;
	else if(strcmp(dir, InternalDir) == 0)
		return MovieDirType_Internal;
	else
		return -1;
}

int CUpnpCloudCamera::getGroupNum
	(char* dir)
{	
	if(SDWatchThread::getInstance() == NULL)
		return 0;
	
	if(strcmp(dir, MarkDir) == 0)
	{
		_currentqueue = SDWatchThread::getInstance()->GetMarkedQueue();
		
	}
	else if(strcmp(dir, CircleDir) == 0)
	{
		_currentqueue = SDWatchThread::getInstance()->GetCircleQueue();
	}
	else if(strcmp(dir, InternalDir) == 0)
	{
		_currentqueue = CBackupDirManager::getInstance()->getQueue();
	}
	if(_currentqueue != NULL)
	{
		_fielNumInDir = _currentqueue->getFileNumberInQueue();
		_GroupNum = ( _fielNumInDir / FileNumInGroup) + 
			((_fielNumInDir % FileNumInGroup) > 0 ? 1 : 0);	
	}
	else
	{
		_GroupNum = 0;
	}
	return _GroupNum;
}
	
void CUpnpCloudCamera::getDirGroupList
	(char* dir
	, char** _ppList
	, int strLen
	, int strnum)
{
	
	int head, deapth, index;

	_nodeList = _currentqueue->getFileNodeList(head, deapth);
	for(int i = 0; i < _GroupNum; i++)
	{
		index = (i * FileNumInGroup + head) % deapth;
		memset(_ppList[i], 0, strLen);
		if(i != strnum - 1)
		{			
			snprintf(_ppList[i]
				, strLen - 1
				, "%d~%d"
				, i * FileNumInGroup + 1
				, (i + 1) * FileNumInGroup); //, _nodeList[index].name + 5
		}
		else
		{
			memset(_ppList[i], 0, strLen);
			snprintf(_ppList[i]
				, strLen - 1
				, "%d~%d"
				, i * FileNumInGroup + 1
				, _fielNumInDir);

		}
	}
}

int CUpnpCloudCamera::getFileNum
	(char* currentDir
	, int currentGroup)
{	
	getGroupNum(currentDir);
	_currentGroup = currentGroup;
	if(_currentGroup < _GroupNum - 1)
		_fileNumInGroup = FileNumInGroup;
	else if(_currentGroup == _GroupNum - 1)
		_fileNumInGroup = _fielNumInDir - (_GroupNum - 1) * FileNumInGroup;	
	else
		_fileNumInGroup = 0;
	return _fileNumInGroup;
}

void CUpnpCloudCamera::getFileList
	(char* currentDir
	, int currentGroup
	, char** _ppList
	, int strLen
	, int strnum)
{
	printf("get File list\n");
	int index;
	int head, deapth;
	_nodeList = _currentqueue->getFileNodeList(head, deapth);
	for(int i = 0; i < _fileNumInGroup; i++)
	{
		index = (head + _currentGroup*FileNumInGroup + i) % deapth;
		_nodeList[index].getShortName(_ppList[i], strLen);		
	}	
}

camera_mode CUpnpCloudCamera::getRecMode()
{
	if(_pRecordService->isCarMode())
		return camera_mode_car;
	else
		return camera_mode_normal;
}
void CUpnpCloudCamera::setRecMode(camera_mode m)
{
	if(m == camera_mode_car)
		_pRecordService->SetCarMode(true);
	else
		_pRecordService->SetCarMode(false);
}

void CUpnpCloudCamera::setRecSegLength(int seconds)
{
	_pRecordService->SetSegSeconds(seconds);
}

int CUpnpCloudCamera::startRecMode()
{
	_pRecordService->StartRecMode();
	return 0;
}

#ifdef QRScanEnable
int CUpnpCloudCamera::startScanMode()
{
	_pRecordService->startScanMode();
	_pscanThread = new Code_Scan_thread(this);
	_pscanThread->Go();
	return 0;
}

void CUpnpCloudCamera::onScanFinish(bool result)
{
	TestWavePlay("/usr/local/share/ui/beep.wav");
	if(result)
	{
		// process cmd 
		StringEnvelope* pEnv = this->getEnvelope();
		for(int i = 0; i < pEnv->getNum(); i++)
		{
			StringCMD* p = pEnv->GetCmd(i);
			printf("cmd: %s, para1:%s, para2:%s\n", p->getName(), p->getPara1(), p->getPara2());
			if(strcmp(p->getName(), "addAP") == 0)
			{
				_pNetwork->AddApNode(p->getPara1(), "auto", p->getPara2());
				snprintf(_scanResultString, sizeof(_scanResultString), "Got:\\n ssid:%s \\n pwd:%s", p->getPara1(), p->getPara2());
			}
		}
	}
	else
	{
		snprintf(_scanResultString, sizeof(_scanResultString), "Got unknow msg:\\n %s", this->getResult());
	}
	_ppObeservers->OnPropertieChange(0, PropertyChangeType_scanOk);
	// signal ui to finish.
}

char* CUpnpCloudCamera::GetScanResult()
{
	return _scanResultString;
}

int CUpnpCloudCamera::stopScanMode()
{
	_pscanThread->Stop();
	delete _pscanThread;
	_pRecordService->endRecMode();
	return 0;
}
#endif

int CUpnpCloudCamera::endRecMode()
{
	_pRecordService->endRecMode();
	return 0;
}

int CUpnpCloudCamera::StopPlay()
{
	_pDecord->StopPlay();
	return 0;
}
/*
int CUpnpCloudCamera::GetFullPath(char* shortName, char* path, int len)
{	
	int index;
	int head, deapth;
	_nodeList = _currentqueue->getFileNodeList(head, deapth);
	if(_nodeList <= 0)
		return -1;
	for(int i = 0; i < _fileNumInGroup; i++)
	{
		index = (head + _currentGroup*FileNumInGroup + i) % deapth;
		if(_nodeList[index].checkShortName(shortName, strlen(shortName)))
		{
			//printf("---fileNOde : %d \n", index);
			break;
		}		
	}
	memset(path, 0, len);
	sprintf(path,"%s%s%s",_currentqueue->getBasePath(), _nodeList[index].name,MovieFileExtend);
	return 0;
}*/

int CUpnpCloudCamera::startPlay
	(char* shortName)
{
	char path[256];	
	int rt = _currentqueue->GetFullPath(shortName, _currentGroup*FileNumInGroup, _fileNumInGroup, path, sizeof(path));
	printf("---try to play file : %s\n", path);
	_pDecord->StartPlay(path);
	return 0;
}

int CUpnpCloudCamera::removeMovie(char* shortName)
{
	printf("CUpnpCloudCamera::removeMovie %d, %d, %s\n", _currentGroup*FileNumInGroup, _fileNumInGroup, shortName);
	int rt = _currentqueue->RemoveMovie(shortName, _currentGroup*FileNumInGroup,  _fileNumInGroup);
	return rt;
}

int CUpnpCloudCamera::markMovie(char* shortName)
{

	int rt = _currentqueue->mvToMarkDir(shortName, _currentGroup*FileNumInGroup,  _fileNumInGroup);
	return rt;
}

int CUpnpCloudCamera::unmarkMovie(char* shortName)
{
	int rt = _currentqueue->mvToCircleDir(shortName, _currentGroup*FileNumInGroup,  _fileNumInGroup);
	return rt;
}

int CUpnpCloudCamera::cpyToInternal(char* shortName)
{
	int rt = _currentqueue->cpyToInternalDir(shortName, _currentGroup*FileNumInGroup,  _fileNumInGroup);
	return rt;
}

UINT64 CUpnpCloudCamera::MovieSize(char* shortName)
{
	return _currentqueue->fileSize(shortName, _currentGroup*FileNumInGroup,  _fileNumInGroup);
}

bool CUpnpCloudCamera::InternalHasFile(char* shortName)
{
	CBackupDirManager::getInstance()->getQueue()->HasFile(shortName);
}

UINT64 CUpnpCloudCamera::InternalStorageFreeSize()
{
	CBackupDirManager::getInstance()->getFreeSpace();
}

void CUpnpCloudCamera::Play()
{
	_pDecord->Play();
}

void CUpnpCloudCamera::Pause()
{
	_pDecord->Pause();
}

void CUpnpCloudCamera::PlayInfor
	(int &length, int &currentPosi, camera_State &s, char** file)
{
	bool bPause;
	bool bStop;
	bool bError;
	_pDecord->PlayInfor(length, currentPosi, bPause, bStop, bError, file);
	if(bError)
	{
		s = camera_State_Error;
	}
	else if(bStop)
		s = camera_State_stop;
	else
	{
		if(bPause)
			s = camera_State_Pause;
		else
			s = camera_State_play;
	}
}

int CUpnpCloudCamera::PlayNext()
{
	return 0;
}	

int CUpnpCloudCamera::PlayPre()
{
	return 0;
}

int CUpnpCloudCamera::Step
	(bool forward, int mseconds)
{
	return 0;
}

void CUpnpCloudCamera::OnSystemTimeChange()
{
	CUpnpThread::getInstance()->RequireUpnpReboot();
}	

void CUpnpCloudCamera::ShowGPSFullInfor(bool b)
{
	_pRecordService->ShowGPSFullInfor(b);
}

void CUpnpCloudCamera::UpdateSubtitleConfig()
{
	_pRecordService->UpdateSubtitleConfig();
}

void CUpnpCloudCamera::LCDOnOff(bool b)
{
	_pRecordService->LCDOnOff(b);
}

void CUpnpCloudCamera::OnRecKeyProcess()
{
	_pRecordService->OnRecKeyProcess();
}

void CUpnpCloudCamera::OnSensorSwitchKey()
{
	_pRecordService->OnSensorSwitchKey();
}

void CUpnpCloudCamera::SetGSensorOutputBuf(char* buf, int len, int lineNum)
{
	_pRecordService->SetGSensorOutputBuf(buf, len,lineNum);
}

void CUpnpCloudCamera::SetGSensorSensitivity(int s)
{
	//if(s > 0)
	//	s = s * 100;
	CGsensorThread::getInstance()->setSensitivity(s);
}

void CUpnpCloudCamera::SetGSensorSensitivity(int x, int y, int z)
{
	//if(s > 0)
	//	s = s * 100;
	CGsensorThread::getInstance()->setSensitivity(x, y, z);
}

int CUpnpCloudCamera::GetGSensorSensitivity(char* buf, int len)
{
	int x, y, z;
	int st = CGsensorThread::getInstance()->getSensitivity(x, y, z);
	if((x == y)&&(y == z))
	{
		switch(x)
		{
			case CGsensorThread::Sensitivity_off:
				snprintf(buf, len, "Off");
				break;
			case CGsensorThread::Sensitivity_low:
				snprintf(buf, len, "Low");
				break;
			case CGsensorThread::Sensitivity_Hi:
				snprintf(buf, len, "High");
				break;
			case CGsensorThread::Sensitivity_middle:
				snprintf(buf, len, "Middle");
				break;
			default:
				snprintf(buf, len, "%d", x);
				break;
		}
	}
	else
	{
		snprintf(buf, len, "Manual", st);
	}
	return st;
}

int CUpnpCloudCamera::GetGSensorSensitivity(int& x, int& y, int& z)
{
	int st = CGsensorThread::getInstance()->getSensitivity(x, y, z);
	//snprintf(buf, len, "x:%d y: z:%d", x, y, z);
	return st;
}
void CUpnpCloudCamera::EnableDisplayManually()
{
	_pRecordService->EnableDisplayManually();
}

void CUpnpCloudCamera::SetMicVolume(int volume)
{
	_pRecordService->SetMicVolume(volume);
}

int CUpnpCloudCamera::GetBatteryVolume(bool& bIncharge)
{
	int rt = 0;
	char tmp[AGCMD_PROPERTY_SIZE];

	agcmd_property_get(propPowerSupplyBCapacity, tmp, "0");
	rt = strtoul(tmp, NULL, 0);
	agcmd_property_get(powerstateName, tmp, "Discharging");
	bIncharge = !!(strcmp(tmp, "Charging") == 0);

	return rt;
}

int CUpnpCloudCamera::SetDisplayBrightness(int value)
{
	int brightness = GetBrightness();
	int _fb = open("/dev/fb0", O_RDWR);
	if (!_fb) 
	{
		printf("Error: cannot open framebuffer device.\n");
	}
	else
	{
		if(value > 0)
		{
			brightness += 28;
			if(brightness > 252)
			{
				brightness = 252;
			}
		}else if (value < 0)
		{
			brightness -= 28;
			if(brightness < 0)
			{
				brightness = 0;
			}
		}
		ioctl(_fb, AGB_SSD1308_IOCTL_SET_CONTRAST, &brightness);
		char tmp[256];
		sprintf(tmp, "%d", brightness);
		agcmd_property_set("camera.screen.brightness", tmp);
		close(_fb);	
	}
	return brightness;
}

int CUpnpCloudCamera::GetBrightness()
{
	char tmp[256];
	agcmd_property_get("camera.screen.brightness", tmp, "128");
	printf("screen brightness : %s\n", tmp);
	int brightness = atoi(tmp);
	return brightness;
}

int CUpnpCloudCamera::GetWifiAPinfor(char* pName, char* pPwd)
{
	char ssid_name_prop[AGCMD_PROPERTY_SIZE];
	char ssid_key_prop[AGCMD_PROPERTY_SIZE];

	agcmd_property_get("temp.earlyboot.ssid_name", ssid_name_prop, "");
	agcmd_property_get("temp.earlyboot.ssid_key", ssid_key_prop, "");
	agcmd_property_get(ssid_name_prop, pName, "N/A");
	agcmd_property_get(ssid_key_prop, pPwd, "N/A");
}

int CUpnpCloudCamera::GetWifiCltinfor(char* pName)
{
	char wlan_prop[AGCMD_PROPERTY_SIZE];
	char prebuild_prop[AGCMD_PROPERTY_SIZE];

	agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
	agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
	//agcmd_wpa_add(wlan_prop, ssid, type, pwd);
	//savedNumber = agcmd_wpa_list(wlan_prop, AP_List, MAX_AP_NUMBER);
	
	agcmd_wpa_network_list_s networkList;
	int num = agcmd_wpa_list_networks(wlan_prop, &networkList);
	//printf("--- %d, %d,\n", num , networkList.valid_num);
	if(networkList.valid_num <= 0)
	{
		sprintf(pName, "Empty List");
	}
	else
	{
		int i = 0;
		for(i = 0; i < networkList.valid_num; i++)
		{
			//printf("--ssid : %s, status: %s\n", networkList.info[i].ssid, networkList.info[i].flags);
			if(strcmp(networkList.info[i].flags,"[CURRENT]") == 0)
			{
				sprintf(pName, "%s", networkList.info[i].ssid);
				break;
			}
		}
		if(!(i < networkList.valid_num))
		{
			sprintf(pName, "Searching");
		}
	}
}

int CUpnpCloudCamera::SetWifiMode(int m)
{
	return _pRecordService->SetWifiMode(m);
}

int CUpnpCloudCamera::GetSystemLanugage
	(char* language)
{
	//LoadTxtItem("Language")->getContent();
	
	//char* language =  (char*)pItem->GetText();
	//	CLanguageLoader::getInstance()->LoadLanguage(language);
	//	//save property TBD
	agcmd_property_get("system.ui.language", language, "Err");	
}

int CUpnpCloudCamera::SetSystemLanguage(char* language)
{
	agcmd_property_set("system.ui.language", language);
}
