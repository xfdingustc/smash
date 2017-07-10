/*****************************************************************************
 * class_sd_service_rec.h
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
#ifndef __H_class_upnp_sd_svc_rec__
#define __H_class_upnp_sd_svc_rec__

#include "class_upnp_description.h"
#include <unistd.h>
#include "libipc.h"

#include "ClinuxThread.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include "class_ipc_rec.h"
#include "CKeyEventProcessThread.h"

#ifdef USE_AVF
	typedef  class IPC_AVF_Client IPC_Obj;
#else
	typedef  class IPC_VLC_Object IPC_Obj;
#endif

/***************************************************************
CSDRecSvTimer
*/
class CSDRecSvTimer : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDRecSvTimer(CUpnpService* pService);
	~CSDRecSvTimer();

private:

};

/***************************************************************
CSDRecSvElapsedTime
*/
class CSDRecSvElapsedTime : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDRecSvElapsedTime(CUpnpService* pService);
	~CSDRecSvElapsedTime();

private:

};

/***************************************************************
CSDRSError
*/
class CSDRSError : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDRSError(CUpnpService* pService);
	~CSDRSError();

private:

};

/***************************************************************
CSDRSError
*/
class CSDCameraName : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDCameraName(CUpnpService* pService);
	~CSDCameraName();

private:

};


/***************************************************************
CSDRSColorState
*/
class CSDRSColorState : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum RecColorState
	{
		RecColorState_NORMAL = 0,
		RecColorState_SPORT,
		RecColorState_CARDV,
		RecColorState_SCENE,
		RecColorState_num,
	}RecordState;
public:
	CSDRSColorState(CUpnpService* pService);
	~CSDRSColorState();
	void SetColorMode();
	void GetColorMode(int &colorMode);
	virtual void Update(bool bSendEvent);
private:
	const char*	_pAllowedValue[RecColorState_num];
};

/***************************************************************
CSDRSSegLength
*/
class CSDRSSegLength : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum SegLength
	{
		SegLength_1min = 0,
		SegLength_3min,
		SegLength_5min,
		SegLength_10min,
		SegLength_num,
	}SegLength;
public:
	CSDRSSegLength(CUpnpService* pService);
	~CSDRSSegLength();
	void GetIframeLen(int& seg_iframe_num);
	//void SetColorMode();
	void SetIframeNum(int seg_iframe_num);
	virtual void Update(bool bSendEvent);
private:
	const char*	_pAllowedValue[SegLength_num];
};
/***************************************************************
CSDRSRecModeState
*/
class CSDRSRecModeState : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum RecModeState
	{
		RecModeState_NORMAL = 0,
		RecModeState_CIRCLE,
		RecModeState_ALERT,
		RecModeState_num,
	}RecordState;
public:
	CSDRSRecModeState(CUpnpService* pService);
	~CSDRSRecModeState();
	bool CarMode();
	bool SetCarMode(bool b);
	bool AlertMode(){return (_currentValueIndex == RecModeState_ALERT);};
	virtual void Update(bool bSendEvent);
private:
	const char*	_pAllowedValue[RecModeState_num];
};

/***************************************************************
CSDRSQualityState
*/
class CSDRSQualityState : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum QualityState
	{
		QualityState_HQ = 0,
		QualityState_NORMAL,
		QualityState_SOSO,
		QualityState_LONG,
		QualityState_num,
	}RecordState;
public:
	CSDRSQualityState(CUpnpService* pService);
	~CSDRSQualityState();
	void getBitRateAccrodingSize
			(int &recording_bitrate, int pixelperFrame, int rate);
	virtual void Update(bool bSendEvent);
private:
	const  char*	_pAllowedValue[QualityState_num];
};


/***************************************************************
CSDRSResolution
*/
class CSDRSResolution : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum Resolution
	{
		//Resolution_1080p60,
		Resolution_1080p30 = 0,
		Resolution_720p60,
		Resolution_720p30,
		//Resolution_480p120,
		Resolution_480p60,
		Resolution_480p30,
		Resolution_number,
	}RecordState;
public:
	CSDRSResolution(CUpnpService* pService);
	~CSDRSResolution();
	void getResolution(int& width, int& height, int& fps);
	//static void CurrentResolution(int& width, int& height, int& fps);
	//bool CheckFramerate();
	virtual void Update(bool bSendEvent);
private:
	const  char*	_pAllowedValue[Resolution_number];
};

/***************************************************************
CSDRecResWidth
*/
class CSDRecResWidth : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDRecResWidth(CUpnpService* pService);
	~CSDRecResWidth();
private:

};
/***************************************************************
CSDRecResHeight
*/
class CSDRecResHeight : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDRecResHeight(CUpnpService* pService);
	~CSDRecResHeight();
private:

};
/***************************************************************
CSDRecResFPS
*/
class CSDRecResFPS : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDRecResFPS(CUpnpService* pService);
	~CSDRecResFPS();
private:

};
/***************************************************************
CSDRecQuaBitrate
*/
class CSDRecQuaBitrate : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDRecQuaBitrate(CUpnpService* pService);
	~CSDRecQuaBitrate();
private:

};

/***************************************************************
CSDRSRecordState
*/
class CSDRSRecordState : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
public:
	typedef enum RecordState
	{
		RecordState_stop = 0,
		RecordState_stopping,
		RecordState_starting,
		RecordState_recording,
		RecordState_close,
		RecordState_open,
		RecordState_Num
	}RecordState;
	CSDRSRecordState(CUpnpService* pService);
	~CSDRSRecordState();
	void Update(bool bSendEvent);
	int GetRecState(){return _currentValueIndex;};
private:
	const char*	_pAllowedValue[RecordState_Num];
};

/***************************************************************
CSDRSRecordState
*/
class CSDRSAudioRecState : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
public:
	typedef enum AudioRecState
	{
		AudioRecState_mute = 0,
		AudioRecState_on,
		AudioRecState_Num
	}AudioRecState;
	CSDRSAudioRecState(CUpnpService* pService);
	~CSDRSAudioRecState();
	void Update(bool bSendEvent);
private:
	const char*	_pAllowedValue[AudioRecState_Num];
};

/***************************************************************
CCCActionSetAudioState
*/
class CCCActionSetAudioState : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetAudioState(CUpnpService* pService);
	~CCCActionSetAudioState();

	virtual void execution();
private:
	
};

/***************************************************************
CCCActionGetAudioState
*/
class CCCActionGetAudioState : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetAudioState(CUpnpService* pService);
	~CCCActionGetAudioState();

	virtual void execution();
private:
	
};

/***************************************************************
CCCActionSetRecTimer
*/
class CCCActionSetRecTimer : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetRecTimer(CUpnpService* pService);
	~CCCActionSetRecTimer();

	virtual void execution();
private:
	
};


/***************************************************************
CCCActionRecStartRec
*/
class CCCActionSetRecConfig : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetRecConfig(CUpnpService* pService);
	~CCCActionSetRecConfig();
	virtual void execution();
private:
	
};

/***************************************************************
CCCActionRecStartRec
*/
class CCCActionGetRecConfig : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetRecConfig(CUpnpService* pService);
	~CCCActionGetRecConfig();
	virtual void execution();
private:
	
};


/***************************************************************
CCCActionRecStartRec
*/
class CCCActionRecStartRec : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionRecStartRec(CUpnpService* pService);
	~CCCActionRecStartRec();
	virtual void execution();
private:
	
};

/***************************************************************
CCCActionGetRecState
*/
class CCCActionGetRecState : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetRecState(CUpnpService* pService);
	~CCCActionGetRecState();
	virtual void execution();
private:
	
};

/***************************************************************
CCCActionGetTimeInfor GetTimeInfor
*/
class CCCActionGetTimeInfor : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetTimeInfor(CUpnpService* pService);
	~CCCActionGetTimeInfor();
	virtual void execution();
private:
	
};

/***************************************************************
CCCActionStopRec
*/
class CCCActionStopRec : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionStopRec(CUpnpService* pService);
	~CCCActionStopRec();
	virtual void execution();
private:
	
};

/***************************************************************
CCCSetCameraName
*/
class CCCSetCameraName : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCSetCameraName(CUpnpService* pService);
	~CCCSetCameraName();
	virtual void execution();
private:
	
};

/***************************************************************
CCCSetCameraName
*/
class CCCGetCameraName : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCGetCameraName(CUpnpService* pService);
	~CCCGetCameraName();
	virtual void execution();
private:
	
};

/***************************************************************
CCCSetSegLength
*/
class CCCSetSegLength : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCSetSegLength(CUpnpService* pService);
	~CCCSetSegLength();
	virtual void execution();
private:
	
};

/***************************************************************
CCCGetSegLength
*/
class CCCGetSegLength : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCGetSegLength(CUpnpService* pService);
	~CCCGetSegLength();
	virtual void execution();
private:
	
};

/***************************************************************
CCCGetResInfo
*/
class CCCGetResInfo : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCGetResInfo(CUpnpService* pService);
	~CCCGetResInfo();
	virtual void execution();
private:
	
};

/***************************************************************
CCCGetBitrateInfo
*/
class CCCGetBitrateInfo : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCGetBitrateInfo(CUpnpService* pService);
	~CCCGetBitrateInfo();
	virtual void execution();
private:
	
};

/***************************************************************
RecKeyEventProcessor
*/
class RecKeyEventProcessor : public KeyEventProcessor
{
public:
	RecKeyEventProcessor(void* obj);
	~RecKeyEventProcessor();
	virtual bool event_process(struct input_event* ev);
private:
	struct timeval _last_camera_key_time;
};

/***************************************************************
CUpnpSDRecord
*/
//class IPC_Obj;
//class IPC_Obj;
class CUpnpSDRecord : public CUpnpService
{
	typedef CUpnpService inherited;
public:
	CUpnpSDRecord(CUpnpDevice*);
	~CUpnpSDRecord();

	void getResolution(int &width, int &height, int &frame);
	int getBitrate();
	int getSegSeconds();
	void SetSegSeconds(int seconds);
	int getColorMode();
	bool isCarMode();
	bool isAlertMode();
	bool isAutoStartMode();
	void SetCarMode(bool b);
	void SetRecordState(int State);
	int GetRecordState();
	long long getOneFileSpaceNeeded();
	int GetAudioState();
	UINT64 GetElapsedTime();
	int GetRotate();
	void StartRecMode();
	void startScanMode();
	void endRecMode();
	char* getCameraName();
	void ShowGPSFullInfor(bool b);
	void LCDOnOff(bool b);
	int GetStorageState();
	int GetStorageFreeSpaceInPcnt();
	int GetGpsState();
	void OnRecKeyProcess();
	void OnSensorSwitchKey();
	void SetGSensorOutputBuf(char* buf, int len, int lineNum);
	void UpdateSubtitleConfig();
	void EnableDisplayManually();
	void SetMicVolume(int volume);

	void EventWriteWarnning();
	void EventWriteError();

	int SetWifiMode(int mode);
private:
#ifdef USE_AVF
	//friend class IPC_AVF_Client;
#else
	friend class IPC_VLC_Object;
#endif

	
	CSDRSRecordState* 		_pStateRecord;
	CSDRSResolution*		_pStateResolution;
	CSDRSQualityState* 		_pStateQuality;
	CSDRSRecModeState*		_pStateRecMode;
	CSDRSColorState*		_pStateColorMode;
	CSDRecSvElapsedTime*	_pElapsedTime;
	CSDRecSvTimer*			_pTimer;
	CSDRSError*				_pError;
	CSDCameraName*			_pCameraName;
	
	CSDRSSegLength*			_pRecSegLength;
	CSDRecResWidth*			_pRecResWidth;
	CSDRecResHeight*		_pRecResHeight;
	CSDRecResFPS*			_pRecResFPS;
	CSDRecQuaBitrate*		_pRecQuaBitrate;

	CSDRSAudioRecState*		_pRecAudioState;
		
	CCCActionRecStartRec*	_pActionStartRec;
	CCCActionSetRecConfig*	_pActionSetRecConfig;
	CCCActionSetRecTimer*	_pActionSetTimer;
	CCCActionGetRecConfig*	_pActionGetRecConfig;
	CCCActionGetRecState*	_pActionGetRecState;
	CCCActionGetTimeInfor*	_pActionGetTimeInfo;
	CCCActionStopRec*		_pStopRecord;	
	CCCGetCameraName*		_pGetName;
	CCCSetCameraName*		_pSetName;


	CCCSetSegLength*		_pActionSetSegLen;
	CCCGetSegLength*		_pActionGetSegLen;
	CCCGetResInfo*			_pActionGetResInfo;
	CCCGetBitrateInfo*		_pActionGetBitrateInfo;

	CCCActionSetAudioState* _pActionSetAudioState;
	CCCActionGetAudioState* _pActionGetAudioState;

	CUpnpState*			_pStateIn[8];
	CUpnpState*			_pStateOut[8];	

	RecKeyEventProcessor *_keyProcessor;
};

#endif
