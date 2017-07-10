/*****************************************************************************
 * upnp_sdcc.h
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 - 2014, linnsong.
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

#ifndef __H_class_upnp_sdcc__
#define __H_class_upnp_sdcc__

#include "class_upnp_description.h"
#include "class_sd_service_rec.h"
#include "class_sd_service_stream.h"
#include "class_sd_service_power.h"
#include "class_sd_service_network.h"
#include "class_sd_service_storage.h"
#include "class_sd_service_decode.h"

//for ui
#include "class_camera_property.h"

#include "qrCodeScan.h"
/***************************************************************
CUpnpCloudCamera
*/
class CUpnpCloudCamera 
	: public CUpnpDevice
	, public CCameraProperties
#ifdef QRScanEnable
	, public Code_Scan_Delegate
#endif
{
	typedef CUpnpDevice inherited;
public:
	CUpnpCloudCamera
		(const char* versionMajor
		,const char* versionMinor
		,const char* udn
		,const char* id
		,const char* sn);
	~CUpnpCloudCamera();

	void SendHeartBeat();
	virtual void EventPropertyUpdate(changed_property_type state);
	void EventWriteWarnning();
	void EventWriteError();
	
	// ui interface for rec
	virtual camera_State getRecState();
	virtual UINT64 getRecordingTime();
	virtual storage_State getStorageState();
	virtual int getStorageFreSpcByPrcn();
	virtual bool FormatStorage();
	virtual wifi_mode getWifiMode();
	virtual wifi_State getWifiState();
	virtual audio_State getAudioState();
	virtual rotate_State getRotationState();
	virtual gps_state getGPSState();
	virtual UINT64 getPlayingTime();

	//ui interface  for play
	virtual int getMovieDirNum();
	virtual void getMovieDirList(char** _ppList, int strLen, int strnum);
	virtual int getGroupNum(char* name);	
	virtual void getDirGroupList(char* dir, char** _ppList, int strLen, int strnum);
	virtual  int getCurrentDirType(char* dir);
	virtual int getFileNum(char* currentDir, int currentGroup);
	virtual void getFileList(char* currentDir, int currentGroup, char** _ppList, int strLen, int strnum);
	virtual camera_mode getRecMode();
	virtual void setRecMode(camera_mode m);
	virtual void setRecSegLength(int seconds);
	virtual int startRecMode();	
	virtual int endRecMode();
#ifdef QRScanEnable
	virtual int startScanMode();
	virtual void onScanFinish(bool result);
	virtual char* GetScanResult();
	virtual int stopScanMode();
#endif
	virtual int startPlay(char* shortName);
	virtual int removeMovie(char* shortName);
	virtual int markMovie(char* shortName);
	virtual int unmarkMovie(char* shortName);
	virtual int cpyToInternal(char* shortName);
	virtual UINT64 MovieSize(char* shortName);
	virtual UINT64 InternalStorageFreeSize();
	virtual bool InternalHasFile(char* shortName);
	
	virtual int StopPlay();
	virtual void Play();
	virtual void Pause();
	virtual void PlayInfor(int &length, int &currentPosi, camera_State &, char** file);
	virtual int PlayNext();
	virtual int PlayPre();
	virtual int Step(bool forward, int mseconds);
	//virtual camera_State getPlayState();
	virtual void OnSystemTimeChange();
	virtual void ShowGPSFullInfor(bool b);
	virtual void LCDOnOff(bool b);
	virtual void OnRecKeyProcess();
	virtual void OnSensorSwitchKey();
	virtual void SetGSensorOutputBuf(char* buf, int len, int lineNum);
	virtual void UpdateSubtitleConfig();
	virtual void SetGSensorSensitivity(int s);
	virtual void SetGSensorSensitivity(int x, int y, int z);
	virtual int GetGSensorSensitivity(char* buf, int len);
	virtual int GetGSensorSensitivity(int& x, int& y, int& z);
	virtual void EnableDisplayManually();
	virtual void SetMicVolume(int volume);
	virtual int GetBatteryVolume(bool& bIncharge);
	virtual int SetDisplayBrightness(int vaule);
	virtual int GetBrightness();
	virtual int GetWifiAPinfor(char* pName, char* pPwd);
	virtual int GetWifiCltinfor(char* pName);
	virtual int SetWifiMode(int m);
	virtual int GetSystemLanugage(char* language);
	virtual int SetSystemLanguage(char* language);
private:

	CUpnpSDRecord* 		_pRecordService;
	CUpnpSDLiveStream* 	_pStreamService;
	CUpnpSDPower*		_pPower;
	CUpnpSDNetwork*		_pNetwork;
	CUpnpSDStorage*		_pStorage;
	CUpnpSDDecode*		_pDecord;
	UINT32 				_heartBeat;

	

private:
	FileQueueControl*	_currentqueue;
	movieFileNode* 		_nodeList;
	int				 	_fielNumInDir;
	int				 	_GroupNum;
	int					_currentGroup;
	int					_fileNumInGroup;

private:
	//int GetFullPath(char* shortName, char* path, int len);

	Code_Scan_thread 	*_pscanThread;
	char				_scanResultString[512];
};


class ScanResultProcess :public Code_Scan_Delegate
{
public:
	ScanResultProcess();
	~ScanResultProcess();

private:
	
};
#endif
