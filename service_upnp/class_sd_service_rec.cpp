/*****************************************************************************
 * upnp_.cpp:
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
#include "class_ipc_rec.h"
#include "class_sd_service_rec.h"
#include "class_gsensor_thread.h"
#include <math.h>

#include "agbox.h"
#include "upnp_sdcc.h"

/***************************************************************
CCCActionSetAudioState
*/
CCCActionSetAudioState::CCCActionSetAudioState
	(CUpnpService* pService
	) : inherited("setAudioState", pService)	
{
}
CCCActionSetAudioState::~CCCActionSetAudioState()	
{
}

void CCCActionSetAudioState::execution()	
{
	CSDRSAudioRecState* pAudioState = (CSDRSAudioRecState*)_ppInStates[0];
	pAudioState->setWishAsCurrent();
	pAudioState->saveProperty();
}

/***************************************************************
CCCActionGetAudioState
*/

CCCActionGetAudioState::CCCActionGetAudioState
	(CUpnpService* pService
	) : inherited("getAudioState", pService)	
{
}
CCCActionGetAudioState::~CCCActionGetAudioState()	
{
}

void CCCActionGetAudioState::execution()	
{
	CSDRSAudioRecState* pAudioState = (CSDRSAudioRecState*)_ppOutStates[0];
	char value[256];
	agcmd_property_get("temp.audio.status", value, "");
	if(strcmp(value, "mute") == 0)
	{
		pAudioState->SetState(CSDRSAudioRecState::AudioRecState_mute);
	}
	else if(strcmp(value, "normal") == 0)
	{
		pAudioState->SetState(CSDRSAudioRecState::AudioRecState_on);
	}
}


/***************************************************************
CCCSetSegLength
*/

CCCSetSegLength::CCCSetSegLength
	(CUpnpService* pService
	) : inherited("setSegLen", pService)	
{
}
CCCSetSegLength::~CCCSetSegLength()	
{
}
void CCCSetSegLength::execution()	
{
	CSDRSSegLength* pSegLength = (CSDRSSegLength*)_ppInStates[0];
	pSegLength->setWishAsCurrent();
	pSegLength->saveProperty();
	pSegLength->sendUpnpEvent();
}

/***************************************************************
CCCGetSegLength
*/

CCCGetSegLength::CCCGetSegLength
	(CUpnpService* pService
	) : inherited("getSegLen", pService)	
{
}
CCCGetSegLength::~CCCGetSegLength()	
{
}
void CCCGetSegLength::execution()	
{
	CSDRSSegLength* pSegLength = (CSDRSSegLength*)_ppOutStates[0];
	pSegLength->loadProperty();
}


/***************************************************************
CCCGetResInfo
*/
CCCGetResInfo::CCCGetResInfo
	(CUpnpService* pService
	) : inherited("getResInfo", pService)	
{
}
CCCGetResInfo::~CCCGetResInfo()	
{
}
void CCCGetResInfo::execution()	
{
	CSDRecResWidth* pRecResWidth = (CSDRecResWidth*)_ppOutStates[0];
	CSDRecResHeight* pRecResHeight = (CSDRecResHeight*)_ppOutStates[1];
	CSDRecResFPS* pRecResFPS = (CSDRecResFPS*)_ppOutStates[2];
	//_pRecQuaBitrate = new CSDRecQuaBitrate(this);
	pRecResWidth->SetState((double)IPC_Obj::getObject()->getWidth());
	pRecResHeight->SetState((double)IPC_Obj::getObject()->getHeight());
	pRecResFPS->SetState((double)IPC_Obj::getObject()->getFps());
}


/***************************************************************
CCCGetBitrateInfo
*/

CCCGetBitrateInfo::CCCGetBitrateInfo
	(CUpnpService* pService
	) : inherited("getBitrateInfo", pService)	
{
}
CCCGetBitrateInfo::~CCCGetBitrateInfo()	
{
}
void CCCGetBitrateInfo::execution()	
{
	//printf("CCCGetBitrateInfo --bitrate read trace\n");
	CSDRecQuaBitrate* pRecQuaBitrate = (CSDRecQuaBitrate*)_ppOutStates[0];
	CUpnpSDRecord* pUpnp_rec_service = (CUpnpSDRecord*)this->_pOwnerService;
	pRecQuaBitrate->SetState((double)pUpnp_rec_service->getBitrate());
}



/***************************************************************
CCCSetCameraName 
*/
//#define CAMERA_NAME_KEY "upnp.camera.name"
CCCSetCameraName::CCCSetCameraName
	(CUpnpService* pService
	) : inherited("setCameraName", pService)	
{
}
CCCSetCameraName::~CCCSetCameraName()
{
}

void CCCSetCameraName::execution()
{
	CSDCameraName* pCameraName = (CSDCameraName*) _ppInStates[0];
	pCameraName->setWishAsCurrent();
	pCameraName->saveProperty();
	pCameraName->sendUpnpEvent();
	IPC_Obj::getObject()->SetCameraName(pCameraName->getCurrentState());
}

/***************************************************************
CCCSetCameraName 
*/
CCCGetCameraName::CCCGetCameraName
	(CUpnpService* pService
	) : inherited("getCameraName", pService)	
{
}

CCCGetCameraName::~CCCGetCameraName()
{
}

void CCCGetCameraName::execution()
{
	CSDCameraName* pCameraName = (CSDCameraName*) _ppOutStates[0];
	pCameraName->loadProperty();
}


/***************************************************************
CCCActionStopRec 
*/
CCCActionStopRec::CCCActionStopRec
	(CUpnpService* pService
	) : inherited("StopRecord", pService)	
{
}

CCCActionStopRec::~CCCActionStopRec()
{
}

void CCCActionStopRec::execution()
{
	IPC_Obj::getObject()->StopRecording();
}

/***************************************************************
CCCActionGetTimeInfor 
*/
CCCActionGetTimeInfor::CCCActionGetTimeInfor
	(CUpnpService* pService
	) : inherited("GetTimeInfor", pService)	
{
}

CCCActionGetTimeInfor::~CCCActionGetTimeInfor()
{
}

void CCCActionGetTimeInfor::execution()
{
	//get rec current state from IPC

	// set to upnp state
	CSDRecSvElapsedTime *pElapsedTime = (CSDRecSvElapsedTime *)_ppOutStates[0];
//	CSDRecSvTimer *pTimer = (CSDRecSvTimer *)_ppOutStates[1];

	UINT64 elapsed = IPC_Obj::getObject()->getRecordingTime();
	pElapsedTime->SetState((double)elapsed);
	
}



/***************************************************************
CCCActionGetRecState 
*/
CCCActionGetRecState::CCCActionGetRecState
	(CUpnpService* pService
	) : inherited("GetRecordState", pService)	
{
}

CCCActionGetRecState::~CCCActionGetRecState()
{
}

void CCCActionGetRecState::execution()
{
	//printf("---CCCActionGetRecState\n");
	CSDRSRecordState* pRecordState = (CSDRSRecordState*) _ppOutStates[0];
	pRecordState->sendUpnpEvent();
}


/***************************************************************
CCCActionGetRecConfig
*/
CCCActionGetRecConfig::CCCActionGetRecConfig
	(CUpnpService* pService
	) : inherited("GetConfig", pService)	
{
}

CCCActionGetRecConfig::~CCCActionGetRecConfig()
{
}

void CCCActionGetRecConfig::execution()
{
	//get rec current state from IPC

	// set to upnp state
}

/***************************************************************
CCCActionSetRecConfig
*/
CCCActionSetRecConfig::CCCActionSetRecConfig
	(CUpnpService* pService
	):inherited("SetRecConfig", pService)	
{
	
}

CCCActionSetRecConfig::~CCCActionSetRecConfig()
{

}

void CCCActionSetRecConfig::execution()
{
	//printf("--set rec config\n");
	char tmp[256];
	memset(tmp, 0, sizeof(tmp));
	CSDRSError* perror = (CSDRSError*)_ppOutStates[0];
	
	CSDRSResolution* _pStateResolution = (CSDRSResolution*)_ppInStates[0];
	sprintf(tmp, "success");
	perror->SetState(tmp);
	
	for(int i = 0; i < this->_inNum; i++)
	{
		_ppInStates[i]->setWishAsCurrent();
		_ppInStates[i]->saveProperty();
		_ppInStates[i]->sendUpnpEvent();
	}
}

/***************************************************************
CCCActionSetRecTimer
*/
CCCActionSetRecTimer::CCCActionSetRecTimer
	(CUpnpService* pService
	) :inherited("SetAutoStopTimer", pService)	
{

}

CCCActionSetRecTimer::~CCCActionSetRecTimer()
{

}

void CCCActionSetRecTimer::execution()
{
	
}

/***************************************************************
CCCActionRecStartRec
*/
CCCActionRecStartRec::CCCActionRecStartRec
	(CUpnpService* pService
	):inherited("StartRecord", pService)					
{	
}
CCCActionRecStartRec::~CCCActionRecStartRec()
{

}

void CCCActionRecStartRec::execution()
{
	char tmp[256];
	memset(tmp, 0, sizeof(tmp));
	CSDRSError* perror = (CSDRSError*)_ppOutStates[0];
	IPC_Obj::getObject()->StartRecording(tmp, sizeof(tmp));
	perror->SetCurrentState(tmp);
}

/***************************************************************
CSDRecSvTimer
*/
CSDRecSvTimer::CSDRecSvTimer
	(CUpnpService* pService
	):inherited("timer", false, 0, 0, 0,false, false, pService)
{
	
}

CSDRecSvTimer::~CSDRecSvTimer()
{

}


/***************************************************************
CSDRecSvElapsedTime
*/

CSDRecSvElapsedTime::CSDRecSvElapsedTime
	(CUpnpService* pService
	):inherited("elapsedtime", false, 0, 0, 0,true, true, pService)
{
	_currentValue = 0;
}

CSDRecSvElapsedTime::~CSDRecSvElapsedTime()
{

}


/***************************************************************
CSDRSColorState
*/
CSDRSColorState::CSDRSColorState
	(CUpnpService* pService
	): inherited("colorMode", true, true, pService)
{
	_pAllowedValue[RecColorState_NORMAL] = "NORMAL";
	_pAllowedValue[RecColorState_SPORT] = "SPORT";
	_pAllowedValue[RecColorState_CARDV] = "CARDV";
	_pAllowedValue[RecColorState_SCENE] = "SCENE";
	this->SetAllowedList(RecColorState_num, (char**)_pAllowedValue);
	loadProperty();
}

CSDRSColorState::~CSDRSColorState()
{
	//saveProperty();
}

void CSDRSColorState::SetColorMode()
{
	printf("--set color mode to: %s", getValue());

	switch(_currentValueIndex)
	{
		case RecColorState_NORMAL:
			break;
		case RecColorState_SPORT:
			break;
		case RecColorState_CARDV:
			break;
		case RecColorState_SCENE:
			break;
		default:
			break;
	}
}

void CSDRSColorState::GetColorMode(int &colorMode)
{
	loadProperty();
	switch(_currentValueIndex)
	{
		case RecColorState_NORMAL:
			colorMode = 1;
			break;
		case RecColorState_SPORT:
			colorMode = 0;
			break;
		case RecColorState_CARDV:
			colorMode = 3;
			break;
		case RecColorState_SCENE:
			colorMode = 2;
			break;
		default:
			colorMode = 1;
			break;
	}
}

void CSDRSColorState::Update(bool b)
{
	//this->sendUpnpEvent();
}

/***************************************************************
CSDRSSegLength
*/
CSDRSSegLength::CSDRSSegLength
	(CUpnpService* pService
	) : inherited("segLength", true, true, pService)
{
	_pAllowedValue[SegLength_1min] = "1min";
	_pAllowedValue[SegLength_3min] = "3min";
	_pAllowedValue[SegLength_5min] = "5min";
	_pAllowedValue[SegLength_10min] = "10min";
	this->SetAllowedList(SegLength_num, (char**)_pAllowedValue);
	loadProperty();	
}
CSDRSSegLength::~CSDRSSegLength()
{
	//saveProperty();
}

void CSDRSSegLength::GetIframeLen(int& seg_iframe_num)
{
	switch(_currentValueIndex)
	{
		case SegLength_1min:
			seg_iframe_num = 60;
			break;
		case SegLength_3min:
			seg_iframe_num = 180;
			break;
		case SegLength_5min:
			seg_iframe_num = 300;
			break;
		case SegLength_10min:
			seg_iframe_num = 600;
			break;
		default:
			seg_iframe_num = 60;
			break;
	}
}

void CSDRSSegLength::SetIframeNum(int seg_iframe_num)
{
	switch(seg_iframe_num)
	{
		case 60:
			_currentValueIndex = SegLength_1min;
			break;
		case 180:
			_currentValueIndex = SegLength_3min;
			break;
		case 300:
			_currentValueIndex = SegLength_5min;
			break;
		case 600:
			_currentValueIndex = SegLength_10min;
			break;
		default:
			_currentValueIndex = SegLength_1min;
			break;
	}
	saveProperty();
}

void CSDRSSegLength::Update(bool b)
{
	//this->sendUpnpEvent();
}

/***************************************************************
CSDRSRecModeState
*/
CSDRSRecModeState::CSDRSRecModeState
	(CUpnpService* pService
	) : inherited("recordMode", true, true, pService)
{
	_pAllowedValue[RecModeState_NORMAL] = "NORMAL";
	_pAllowedValue[RecModeState_CIRCLE] = "CAR";
	_pAllowedValue[RecModeState_ALERT] = "ALERT";
	this->SetAllowedList(RecModeState_num, (char**)_pAllowedValue);
#ifndef PLATFORMDIABLO
	loadProperty("CAR");
#else
	loadProperty("NORMAL");
#endif

}

CSDRSRecModeState::~CSDRSRecModeState()
{
	//saveProperty();
}

bool CSDRSRecModeState::CarMode()
{
	return (_currentValueIndex == RecModeState_CIRCLE);
}

bool CSDRSRecModeState::SetCarMode(bool b)
{
	if(b)
		this->SetState(RecModeState_CIRCLE);
	else
		this->SetState(RecModeState_NORMAL);
	saveProperty();
}

void CSDRSRecModeState::Update(bool b)
{
	//this->sendUpnpEvent();
}

/***************************************************************
CSDRSQualityState
*/
CSDRSQualityState::CSDRSQualityState
	(CUpnpService* pService
	): inherited("quality", true, true, pService)
{
	_pAllowedValue[QualityState_HQ] = "HQ";
	_pAllowedValue[QualityState_NORMAL] = "NORMAL";
	_pAllowedValue[QualityState_SOSO] = "SOSO";
	_pAllowedValue[QualityState_LONG] = "LONG";
	this->SetAllowedList(QualityState_num, (char**)_pAllowedValue);

	loadProperty();
}

CSDRSQualityState::~CSDRSQualityState()
{
	//saveProperty();
}

void CSDRSQualityState::getBitRateAccrodingSize
	(int &recording_bitrate
	,int pixelperFrame
	,int Framerate)
{
	loadProperty();
	int base = BitrateForSoso720p;
	switch(_currentValueIndex)
	{
		case QualityState_HQ:
			base = BitrateForHQ720p;
			break;
		case QualityState_NORMAL:
			base = BitrateForNormal720p;
			break;
		case QualityState_SOSO:
			base = BitrateForSoso720p;
			break;
		case QualityState_LONG:
			base = BitrateForLong720p;
			break;
		default: 
			break;
	}
	float b = (float)pixelperFrame * ((float)Framerate/30.0) / 1000000.0;
	recording_bitrate = base * b;
	//printf("-- bitrate : %d\n", recording_bitrate);
}

void CSDRSQualityState::Update(bool b)
{
	//this->sendUpnpEvent();
}
/***************************************************************
CSDRSResolution
*/
CSDRSResolution::CSDRSResolution
	(CUpnpService* pService
	) : inherited("Resolution", true, true, pService)
{
	//_pAllowedValue[Resolution_1080p60] = "1080p60";
	_pAllowedValue[Resolution_1080p30] = "1080p30";
	_pAllowedValue[Resolution_720p60] = "720p60";
	_pAllowedValue[Resolution_720p30] = "720p30";
	//_pAllowedValue[Resolution_480p120] = "480p120";
	_pAllowedValue[Resolution_480p60] = "480p60";
	_pAllowedValue[Resolution_480p30] = "480p30";

	this->SetAllowedList(Resolution_number, (char**)_pAllowedValue);
	loadProperty();
}

CSDRSResolution::~CSDRSResolution()
{
	//saveProperty();
}

void CSDRSResolution::getResolution
	(int& width
	,int& height
	,int& fps)
{
	loadProperty();
	switch(_currentValueIndex)
	{
		/*case Resolution_1080p60:
			width = 1920;
			height = 1024;
			fps = 60;
			break;*/
		case Resolution_1080p30:
			width = 1920;
			height = 1080;
			fps = 30;
			break;
		case Resolution_720p60:
			width = 1280;
			height = 720;
			fps = 60;
			break;
		case Resolution_720p30:
			width = 1280;
			height = 720;
			fps = 30;
			break;
		/*case Resolution_480p120:
			width = 640;
			height = 480;
			fps = 120;
			break;*/
		case Resolution_480p60:
			width = 720;
			height = 480;
			fps = 60;
			break;
		case Resolution_480p30:
			width = 720;
			height = 480;
			fps = 30;
			break;
		default:
			width = 1920;
			height = 1080;
			fps = 30;
			break;
	
	}
}

void CSDRSResolution::Update(bool b)
{
	//this->sendUpnpEvent();
}

/***************************************************************
CSDRecResWidth
*/
CSDRecResWidth::CSDRecResWidth
	(CUpnpService* pService
	):inherited("resWidth", false, 0, 0, 0,false, false, pService)
{
	
}
CSDRecResWidth::~CSDRecResWidth()
{

}
/***************************************************************
CSDRecResHeight
*/

CSDRecResHeight::CSDRecResHeight
	(CUpnpService* pService
	):inherited("resHeight", false, 0, 0, 0,false, false, pService)
{
	
}
CSDRecResHeight::~CSDRecResHeight()
{

}

/***************************************************************
CSDRecResFPS
*/

CSDRecResFPS::CSDRecResFPS
	(CUpnpService* pService
	):inherited("resFPS", false, 0, 0, 0,false, false, pService)
{
	
}
CSDRecResFPS::~CSDRecResFPS()
{

}

/***************************************************************
CSDRecQuaBitrate
*/

CSDRecQuaBitrate::CSDRecQuaBitrate
	(CUpnpService* pService
	):inherited("quaBitrate", false, 0, 0, 0,false, false, pService)
{
	
}
CSDRecQuaBitrate::~CSDRecQuaBitrate()
{

}

/***************************************************************
CSDRSError
*/
CSDRSError::CSDRSError
	(CUpnpService* pService
	) : inherited("error", true, true, pService)
{

}

CSDRSError::~CSDRSError()
{

}

/***************************************************************
CSDCameraName
*/
CSDCameraName::CSDCameraName
	(CUpnpService* pService
	) : inherited("cameraName", true, true, pService)
{
	loadProperty();
}

CSDCameraName::~CSDCameraName()
{
	//saveProperty();
}

/***************************************************************
CSDRSRecordState
*/
CSDRSRecordState::CSDRSRecordState
	(CUpnpService* pService
	) : inherited("RecordState", true, true, pService)
{
	_pAllowedValue[RecordState_stop] = "stop";
	_pAllowedValue[RecordState_recording] = "recording";
	_pAllowedValue[RecordState_stopping] = "stoping";
	_pAllowedValue[RecordState_starting] = "starting";
	_pAllowedValue[RecordState_close] = "close";
	_pAllowedValue[RecordState_open] = "open";
	this->SetAllowedList(RecordState_Num, (char**)_pAllowedValue);
}
CSDRSRecordState::~CSDRSRecordState()
{

}

void CSDRSRecordState::Update(bool b)
{	
	//IPC_Obj::getObject()->GetRecrodState(this);
	//this->setWishAsCurrent();
	//printf("record state update : %s\n", _pAllowedValue[_currentValueIndex]);
	this->sendUpnpEvent();
}


/***************************************************************
CUpnpCloudCamera
*/
CSDRSAudioRecState::CSDRSAudioRecState
	(CUpnpService* pService
	) : inherited("RecAudioState", true, true, pService)
{
	_pAllowedValue[AudioRecState_mute] = "mute";
	_pAllowedValue[AudioRecState_on] = "audio on";

	this->SetAllowedList(AudioRecState_Num, (char**)_pAllowedValue);
	this->loadProperty();
}
CSDRSAudioRecState::~CSDRSAudioRecState()
{
	//this->saveProperty();
}

void CSDRSAudioRecState::Update(bool b)
{
	//printf("------- audio state update\n");
	char value[256];
	agcmd_property_get("temp.audio.status", value, "");
	if(strcmp(value, "mute") == 0)
	{
		this->SetState(AudioRecState_mute);
	}
	else if(strcmp(value, "normal") == 0)
	{
		this->SetState(AudioRecState_on);
	}
	else
		this->SetState(AudioRecState_on);
}



RecKeyEventProcessor::RecKeyEventProcessor
	(void* obj):KeyEventProcessor(obj)
{
	_last_camera_key_time.tv_sec = 0;
	_last_camera_key_time.tv_usec = 0;
};
RecKeyEventProcessor::~RecKeyEventProcessor(){};
bool RecKeyEventProcessor::event_process(struct input_event* pev)
{	
	bool rt = true;
	if (((pev->code == KEY_CAMERA)||(pev->code == KEY_RECORD))&& (pev->value == 0)) {
		if ((pev->time.tv_sec - _last_camera_key_time.tv_sec) < 1) {
			printf("Skip Camera op by %d\n", (pev->time.tv_sec - _last_camera_key_time.tv_sec));
		} else {
			_last_camera_key_time = pev->time;
			IPC_Obj::getObject()->RecordKeyProcess();
		}
		rt = false;
	}
	return rt;
}


/***************************************************************
CUpnpSDRecord
*/
CUpnpSDRecord::CUpnpSDRecord
	(CUpnpDevice* pDevice
	):inherited("record"
			,"urn:schemas-upnp-org:service:SDCCRecord:1"
			,"urn:upnp-org:serviceId:SDCCRecord"
			,"/description/sd_record.xml"
			,"/upnp/sdreccontrol"
			,"/upnp/sdrecevent"
			,pDevice
			,20
			,20)
{
	_pStateRecord = new CSDRSRecordState(this);
	_pStateResolution = new CSDRSResolution(this)	;
	_pStateQuality = new CSDRSQualityState(this);
	_pStateRecMode = new CSDRSRecModeState(this);
	_pStateColorMode = new CSDRSColorState(this);
	_pElapsedTime = new CSDRecSvElapsedTime(this);
	_pTimer = new CSDRecSvTimer(this);
	_pError = new CSDRSError(this);
	_pCameraName =  new CSDCameraName(this);

	_pRecSegLength = new CSDRSSegLength(this);
	_pRecResWidth = new CSDRecResWidth(this);
	_pRecResHeight = new CSDRecResHeight(this);
	_pRecResFPS = new CSDRecResFPS(this);
	_pRecQuaBitrate = new CSDRecQuaBitrate(this);
	_pRecAudioState = new CSDRSAudioRecState(this);


	_pActionStartRec = new CCCActionRecStartRec(this);
	_pStateOut[0] = _pError;
	_pActionStartRec->SetArguList(0, _pStateIn, 1, _pStateOut);

	_pActionSetRecConfig= new CCCActionSetRecConfig(this);
	_pStateIn[0] = _pStateResolution;
	_pStateIn[1] = _pStateQuality;
	_pStateIn[2] = _pStateRecMode;
	_pStateIn[3] = _pStateColorMode;
	_pActionSetRecConfig->SetArguList(4, _pStateIn, 1, _pStateOut);
	
	_pActionSetTimer = new CCCActionSetRecTimer(this);
	_pStateIn[0] = _pTimer;
	_pActionSetTimer->SetArguList(1, _pStateIn, 0, _pStateOut);
	
	_pActionGetRecConfig = new CCCActionGetRecConfig(this);
	_pStateOut[0] = _pStateResolution;
	_pStateOut[1] = _pStateQuality;
	_pStateOut[2] = _pStateRecMode;
	_pStateOut[3] = _pStateColorMode;
	_pActionGetRecConfig->SetArguList(0, _pStateIn, 4, _pStateOut);

	_pActionGetRecState = new CCCActionGetRecState(this);
	_pStateOut[0] = _pStateRecord;
	_pActionGetRecState->SetArguList(0, _pStateIn, 1, _pStateOut);

	_pActionGetTimeInfo = new CCCActionGetTimeInfor(this);
	_pStateOut[0] = _pElapsedTime;
	_pStateOut[1] = _pTimer;
	_pActionGetTimeInfo->SetArguList(0, _pStateIn, 2, _pStateOut);	

	_pStopRecord = new CCCActionStopRec(this);
	_pStopRecord->SetArguList(0, _pStateIn, 0, _pStateOut);	

	_pGetName = new CCCGetCameraName(this);
	_pStateOut[0] = _pCameraName;
	_pGetName->SetArguList(0, _pStateIn, 1, _pStateOut);
	
	_pSetName = new CCCSetCameraName(this);
	_pStateIn[0] = _pCameraName;
	_pSetName->SetArguList(1, _pStateIn, 0, _pStateOut);


	_pActionSetSegLen = new CCCSetSegLength(this);
	_pStateIn[0] = _pRecSegLength;
	_pActionSetSegLen->SetArguList(1, _pStateIn, 0, _pStateOut);

	_pActionGetSegLen = new CCCGetSegLength(this);
	_pStateOut[0] = _pRecSegLength;
	_pActionGetSegLen->SetArguList(0, _pStateIn, 1, _pStateOut);

	_pActionGetResInfo = new CCCGetResInfo(this);
	_pStateOut[0] = _pRecResWidth;
	_pStateOut[1] = _pRecResHeight;
	_pStateOut[2] = _pRecResFPS;
	_pActionGetResInfo->SetArguList(0, _pStateIn, 3, _pStateOut);

	_pActionGetBitrateInfo = new CCCGetBitrateInfo(this);
	_pStateOut[0] = _pRecQuaBitrate;
	_pActionGetBitrateInfo->SetArguList(0, _pStateIn, 1, _pStateOut);

	_pActionSetAudioState = new CCCActionSetAudioState(this);
	_pStateIn[0] = _pRecAudioState;
	_pActionSetAudioState->SetArguList(1, _pStateIn, 0, _pStateOut);
	
	_pActionGetAudioState = new CCCActionGetAudioState(this);
	_pStateOut[0] = _pRecAudioState;
	_pActionGetAudioState->SetArguList(0, _pStateIn, 1, _pStateOut);
		
	IPC_Obj::create(this);
	//_keyProcessor = new RecKeyEventProcessor(this);
	//EventProcessThread::getEPTObject()->AddProcessor(_keyProcessor);
	
}
/***************************************************************
~CUpnpSDRecord
*/
CUpnpSDRecord::~CUpnpSDRecord()
{
	/*printf("--saveProperty 0\n");
	_pStateResolution->saveProperty();
	printf("--saveProperty 1\n");
	_pStateQuality->saveProperty();
	printf("--saveProperty 2\n");
	_pStateRecMode->saveProperty();
	printf("--saveProperty 3\n");
	//_pStateColorMode->saveProperty();
	printf("--saveProperty 4\n");
	//_pRecSegLength->saveProperty();*/
	

	printf("--delete watch thread\n");
	
	printf("--_keyProcessor thread\n");
	//delete _keyProcessor;

	printf("--IPC_Obj::destroy\n");
	IPC_Obj::destroy();

	
	printf("--delete states\n");
	delete _pStateRecord;
	delete _pStateResolution;
	delete _pStateQuality;
	delete _pStateRecMode;
	delete _pStateColorMode;
	delete _pElapsedTime;
	delete _pTimer;
	delete _pError;
	delete _pCameraName;

	delete _pRecSegLength;
	delete _pRecResWidth;
	delete _pRecResHeight;
	delete _pRecResFPS;
	delete _pRecQuaBitrate;
	delete _pRecAudioState;

	delete _pActionStartRec;
	delete _pActionSetRecConfig;
	delete _pActionSetTimer;
	delete _pActionGetRecConfig;
	delete _pActionGetRecState;
	delete _pActionGetTimeInfo;
	delete _pStopRecord;	
	delete _pSetName;
	delete _pGetName;

	delete _pActionSetSegLen;
	delete _pActionGetSegLen;
	delete _pActionGetResInfo;
	delete _pActionGetBitrateInfo;
	delete _pActionSetAudioState;
	delete _pActionGetAudioState;
	
}


void CUpnpSDRecord::getResolution(int &width, int &height, int &frame)
{
	_pStateResolution->getResolution(width, height, frame);
}

int CUpnpSDRecord::getBitrate()
{	
	int width, height, frame, bitrate;
	_pStateResolution->getResolution(width, height, frame);
	_pStateQuality->getBitRateAccrodingSize(bitrate, width*height, frame);
	return bitrate;
}

int CUpnpSDRecord::getSegSeconds()
{
	int frameLength;
	_pRecSegLength->GetIframeLen(frameLength);
	return frameLength;
}

void CUpnpSDRecord::SetSegSeconds(int seconds)
{
	_pRecSegLength->SetIframeNum(seconds);
}

int CUpnpSDRecord::getColorMode()
{
	int colormode;
	_pStateColorMode->GetColorMode(colormode);
	return colormode;
}

bool CUpnpSDRecord::isCarMode()
{
	bool bReturn = false;
	char val[256];
	memset (val, 0, sizeof(val));
	agcmd_property_get("upnp.record.recordMode", val , "");
	if(strcmp(val, "Circle")==0)
	{
		bReturn = true;
	}
	else if(strcmp(val, "CAR")==0)
	{
		bReturn = true;
	}
	return bReturn;
}

bool CUpnpSDRecord::isAutoStartMode()
{
	bool bReturn = false;
	char val[256];
	memset (val, 0, sizeof(val));
	agcmd_property_get("upnp.record.recordMode", val , "");
	if(strcmp(val, "AutoStart")==0)
	{
		bReturn = true;
	}
	else if(strcmp(val, "CAR")==0)
	{
		bReturn = true;
	}
	return bReturn;
}

bool CUpnpSDRecord::isAlertMode()
{
	return _pStateRecMode->AlertMode();
}

void CUpnpSDRecord::SetCarMode(bool b)
{
	_pStateRecMode->SetCarMode(b);
}

int CUpnpSDRecord::GetRecordState()
{
	return _pStateRecord->GetRecState();
}

int CUpnpSDRecord::GetGpsState()
{
	int rt = IPC_AVF_Client::getObject()->getGpsState();
	return rt;
}

void CUpnpSDRecord::SetRecordState(int State)
{
	_pStateRecord->SetState(State);
}

UINT64 CUpnpSDRecord::GetElapsedTime()
{
	return IPC_Obj::getObject()->getRecordingTime();
}

int CUpnpSDRecord::GetAudioState()
{
	_pRecAudioState->Update(true);
	return _pRecAudioState->GetCurrentIndex();
}

long long CUpnpSDRecord::getOneFileSpaceNeeded()
{
	//printf("--getOneFileSpaceNeeded : --bitrate\n");
	int bitrate = this->getBitrate();
	int segInum = this->getSegSeconds();
	long long spaceNeed =  segInum * bitrate / 8;
}

int CUpnpSDRecord::GetRotate()
{
	return IPC_AVF_Client::getObject()->isRotate();
}

void CUpnpSDRecord::endRecMode()
{
	IPC_AVF_Client::getObject()->CloseRec();
}

void CUpnpSDRecord::StartRecMode()
{
	IPC_AVF_Client::getObject()->switchToRec(true);
}

void CUpnpSDRecord::startScanMode()
{
	IPC_AVF_Client::getObject()->switchToRec(false);
}

void CUpnpSDRecord::ShowGPSFullInfor(bool b)
{
	IPC_AVF_Client::getObject()->ShowGPSFullInfor(b);
}

void CUpnpSDRecord::UpdateSubtitleConfig()
{
	IPC_AVF_Client::getObject()->UpdateSubtitleConfig();
}

void CUpnpSDRecord::EnableDisplayManually()
{
	IPC_AVF_Client::getObject()->EnableDisplay();
}

void CUpnpSDRecord::LCDOnOff(bool b)
{
	IPC_AVF_Client::getObject()->LCDOnOff(b);
}
char* CUpnpSDRecord::getCameraName()
{
	if(strcmp(_pCameraName->getCurrentState(), "NA") == 0)
		return "";
	else
		return _pCameraName->getCurrentState();
}

int CUpnpSDRecord::GetStorageState()
{
	return IPC_AVF_Client::getObject()->GetStorageState();
}

int CUpnpSDRecord::GetStorageFreeSpaceInPcnt()
{
#ifdef SupportNewVDBAVF
	return IPC_AVF_Client::getObject()->GetStorageFreSpcPcnt();
#endif
}


void CUpnpSDRecord::OnRecKeyProcess()
{
	IPC_Obj::getObject()->RecordKeyProcess();
}

void CUpnpSDRecord::OnSensorSwitchKey()
{
	IPC_Obj::getObject()->RestartRecord();
}

void CUpnpSDRecord::SetGSensorOutputBuf(char* buf, int len, int lineNum)
{
	CGsensorThread::getInstance()->setTestBuffer(buf, len, lineNum);
}

void CUpnpSDRecord::SetMicVolume(int volume)
{
	IPC_Obj::getObject()->SetMicVolume(volume);
}

void CUpnpSDRecord::EventWriteWarnning()
{
	((CUpnpCloudCamera*)this->getDevice())->EventWriteWarnning();
}

void CUpnpSDRecord::EventWriteError()
{
	((CUpnpCloudCamera*)this->getDevice())->EventWriteError();
}
int CUpnpSDRecord::SetWifiMode(int mode)
{
	return IPC_AVF_Client::getObject()->SetWifiMode(mode);
}
#if 0
"/dev/input/event0"
event_fd = open(event_name, O_RDONLY);
ret_val = ioctl(phif_info->event_fd,
		EVIOCGVERSION,
		&phif_info->event_version);
ret_val = ioctl(phif_info->event_fd,
		EVIOCGID,
		&phif_info->event_id);
ret_val = ioctl(phif_info->event_fd,
		EVIOCGNAME(sizeof(phif_info->event_name) - 1),
		&phif_info->event_name);
ret_val = ioctl(phif_info->event_fd,
		EVIOCGPHYS(sizeof(phif_info->event_location) - 1),
		&phif_info->event_location);
ret_val = read(phif_info->event_fd, ev, sizeof(ev));
if (ret_val < (int)sizeof(struct input_event)) {
			AG_LOG_ERR("short read %d\n", ret_val);
			continue;
		}

		ret_val /= sizeof(struct input_event);
		for (i = 0; i < ret_val; i++) {
			dmc_hif_event_process(phif_info, &ev[i]);
		}
#endif



