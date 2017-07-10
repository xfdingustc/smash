/*****************************************************************************
 * upnp_stream.cpp:
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
#include "class_sd_service_stream.h"

#ifdef USE_AVF
	typedef  class IPC_AVF_Client IPC_Obj;
#else
	typedef  class IPC_VLC_Object IPC_Obj;
#endif

/***************************************************************
CSDStreamProtocol
*/
CSDStreamProtocol::CSDStreamProtocol
	(CUpnpService* pService
	) : inherited("Protocol", false, false, pService)
{
	_pAllowedValue[StreamProtol_http] = "http";
	_pAllowedValue[StreamProtol_rtsp] = "rtsp";
	_pAllowedValue[StreamProtol_hlv] = "hlv";
	
	this->SetAllowedList(StreamProtol_Num, (char**)_pAllowedValue);		
}

CSDStreamProtocol::~CSDStreamProtocol()
{

}

/***************************************************************
CSDStreamVCodec
*/

CSDStreamVCodec::CSDStreamVCodec
	(CUpnpService* pService
	) : inherited("VCodec", false, false, pService)
{
	_pAllowedValue[StreamVCodec_mjpeg] = "mjpeg";
	_pAllowedValue[StreamVCodec_h264] = "h264";
	
	this->SetAllowedList(StreamVCodec_Num, (char**)_pAllowedValue);
}

CSDStreamVCodec::~CSDStreamVCodec()
{

}

/***************************************************************
CSDStreamACodec
*/
CSDStreamACodec::CSDStreamACodec
	(CUpnpService* pService
	) : inherited("ACodec", false, false, pService)
{
	_pAllowedValue[StreamACodec_AAC] = "AAC";
	_pAllowedValue[StreamACodec_PCM] = "PCM";
	_pAllowedValue[StreamACodec_MP3] = "MP3";
	
	this->SetAllowedList(StreamACodec_Num, (char**)_pAllowedValue);
}

CSDStreamACodec::~CSDStreamACodec()
{

}

/***************************************************************
CSDStreamVSize
*/
CSDStreamVSize::CSDStreamVSize
	(CUpnpService* pService
	) : inherited("VSize", false, false, pService)
{
	_pAllowedValue[StreamVSize_1080p] = "1080p";
	_pAllowedValue[StreamVSize_720p] = "720p";
	_pAllowedValue[StreamVSize_576p] = "576p";
	_pAllowedValue[StreamVSize_288p] = "288p";
	
	this->SetAllowedList(StreamVSize_Num, (char**)_pAllowedValue);
}

CSDStreamVSize::~CSDStreamVSize()
{

}

bool CSDStreamVSize::BigStream()
{
	return !(_currentValueIndex == StreamVSize_288p);
}

/***************************************************************
CSDStreamVFPS
*/
CSDStreamVFPS::CSDStreamVFPS
	(CUpnpService* pService
	) : inherited("VFPS", false, false, pService)
{
	_pAllowedValue[StreamVFPS_60] = "60";
	_pAllowedValue[StreamVFPS_30] = "30";
	_pAllowedValue[StreamVFPS_15] = "15";
	_pAllowedValue[StreamVFPS_8] = "8";
	_pAllowedValue[StreamVFPS_4] = "4";
	
	this->SetAllowedList(StreamVFPS_Num, (char**)_pAllowedValue);
}

CSDStreamVFPS::~CSDStreamVFPS()
{

}

/***************************************************************
CSDStreamVQuality
*/
CSDStreamVQuality::CSDStreamVQuality
	(CUpnpService* pService
	) : inherited("VQuality", false, false, pService)
{
	_pAllowedValue[StreamVQuality_HQ] = "HQ";
	_pAllowedValue[StreamVQuality_NORMAL] = "NORMAL";
	_pAllowedValue[StreamVQuality_SOSO] = "SOSO";
	_pAllowedValue[StreamVQuality_LONG] = "LONG";
	
	this->SetAllowedList(StreamVQuality_Num, (char**)_pAllowedValue);
}

CSDStreamVQuality::~CSDStreamVQuality()
{

}

/***************************************************************
CSDStreamBitrate
*/

CSDStreamBitrate::CSDStreamBitrate
	(CUpnpService* pService
	) : inherited("Bitrate", false, 0, 0, 0,false, false, pService)
{
}

CSDStreamBitrate::~CSDStreamBitrate()
{

}

/***************************************************************
CSDStreamState
*/
CSDStreamState::CSDStreamState
	(CUpnpService* pService
	): inherited("State", false, false, pService)
{
	_pAllowedValue[StreamState_ON] = "ON";
	_pAllowedValue[StreamState_OFF] = "OFF";

	
	this->SetAllowedList(StreamState_Num, (char**)_pAllowedValue);
}

CSDStreamState::~CSDStreamState()
{

}


/***************************************************************
CSDStreamAddress
*/

CSDStreamAddress::CSDStreamAddress
	(CUpnpService* pService
	): inherited("Address", false, false, pService)
{

}

CSDStreamAddress::~CSDStreamAddress()
{

}


/***************************************************************
CSDStreamError
*/
CSDStreamError::CSDStreamError
	(CUpnpService* pService
	): inherited("error", true, true, pService)
{

}

CSDStreamError::~CSDStreamError()
{

}
	

/***************************************************************
CCCActionStopStream
*/

CCCActionStopStream::CCCActionStopStream
	(CUpnpService* pService
	) : inherited("StopStream", pService)
{

}
CCCActionStopStream::~CCCActionStopStream()
{

}
void CCCActionStopStream::execution()
{

}

/***************************************************************
CCCActionStartStream
*/

CCCActionStartStream::CCCActionStartStream
	(CUpnpService* pService
	) : inherited("StartStream", pService)
{

}
CCCActionStartStream::~CCCActionStartStream()
{

}
void CCCActionStartStream::execution()
{

}

/***************************************************************
CCCActionRecStartRec
*/

CCCActionSetStreamConfig::CCCActionSetStreamConfig
	(CUpnpService* pService
	) : inherited("SetStreamConfig", pService)
{

}
CCCActionSetStreamConfig::~CCCActionSetStreamConfig()
{

}
void CCCActionSetStreamConfig::execution()
{
	//printf("--- CCCActionSetStreamConfig\n");
	CSDStreamVCodec *pStateVCodec = (CSDStreamVCodec*)_ppInStates[0];
	CSDStreamACodec *pStateACodec = (CSDStreamACodec*)_ppInStates[1];
	CSDStreamVSize *pStateVSize = (CSDStreamVSize*)_ppInStates[2];
	CSDStreamVQuality *pStateVQuality = (CSDStreamVQuality*)_ppInStates[3];
	CSDStreamBitrate *pStateBitrate = (CSDStreamBitrate*)_ppInStates[4];
	
	pStateVSize->setWishAsCurrent();
	if(pStateVSize->BigStream())
	{
		//printf("--- set big stream");
		IPC_Obj::getObject()->switchStream(true);
	}
	else
	{
		//printf("--- set small stream");
		IPC_Obj::getObject()->switchStream(false);
	}
}

/***************************************************************
CCCActionRecStartRec
*/

CCCActionGetStreamConfig::CCCActionGetStreamConfig
	(CUpnpService* pService
	) : inherited("GetStreamConfig", pService)
{

}
CCCActionGetStreamConfig::~CCCActionGetStreamConfig()
{

}

void CCCActionGetStreamConfig::execution()
{

}

/***************************************************************
CCCActionRecStartRec
*/

CCCActionGetAddress::CCCActionGetAddress
	(CUpnpService* pService
	) : inherited("GetStreamAddress", pService)	
{

}

CCCActionGetAddress::~CCCActionGetAddress()
{

}

void CCCActionGetAddress::execution()
{
	char tmp[256];
	memset(tmp, 0, 256);
	sprintf(tmp,"http://%s:8081", UpnpGetServerIpAddress());
	if(_ppOutStates != NULL)
	{
		CSDStreamAddress* pAddressState = (CSDStreamAddress*)_ppOutStates[0];
		pAddressState->SetCurrentState(tmp);
	}
	else
	{
		printf("argument for get address not inited\n");
	}

}

/***************************************************************
CUpnpSDLiveStream
*/
CUpnpSDLiveStream::CUpnpSDLiveStream
	(CUpnpDevice* pDevice
	):inherited("stream"
			,"urn:schemas-upnp-org:service:SDCCPreview:1"
			,"urn:upnp-org:serviceId:SDCCPreview"
			,"/description/sd_livestream.xml"
			,"/upnp/sdprevcontrol"
			,"/upnp/sdprevevent"
			,pDevice
			,10
			,10)
{
	_pStateProtocol = new CSDStreamProtocol(this);
	_pStateVCodec = new CSDStreamVCodec(this);
	_pStateACodec = new CSDStreamACodec(this);
	_pStateVSize = new CSDStreamVSize(this);
	_pStateVQuality = new CSDStreamVQuality(this);
	_pStateBitrate = new CSDStreamBitrate(this);
	_pStateState = new CSDStreamState(this);
	_pStateAddress = new CSDStreamAddress(this);
	_pError = new CSDStreamError(this);


	_pActionStopStream = new CCCActionStopStream(this);	
	
	_pActionStartStream = new CCCActionStartStream(this);
	
	_pActionSetStreamConfig = new CCCActionSetStreamConfig(this);
	_pStateIn[0] = _pStateVCodec;
	_pStateIn[1] = _pStateACodec;
	_pStateIn[2] = _pStateVSize;
	_pStateIn[3] = _pStateVQuality;
	_pStateIn[4] = _pStateBitrate;
	_pActionSetStreamConfig->SetArguList(5, _pStateIn, 0, _pStateOut);
	
	_pActionGetStreamConfig = new CCCActionGetStreamConfig(this);
	_pStateOut[0] = _pStateVCodec;
	_pStateOut[1] = _pStateACodec;
	_pStateOut[2] = _pStateVSize;
	_pStateOut[3] = _pStateVQuality;
	_pStateOut[4] = _pStateBitrate;
	_pActionGetStreamConfig->SetArguList(0, _pStateIn, 5, _pStateOut);
	
	_pActionGetAddress = new CCCActionGetAddress(this);
	_pStateOut[0] = _pStateAddress;
	_pActionGetAddress->SetArguList(0, _pStateIn, 1, _pStateOut);

}

 CUpnpSDLiveStream::~CUpnpSDLiveStream()
{
	delete _pStateProtocol;
	delete _pStateVCodec;
	delete _pStateACodec;
	delete _pStateVSize;
	delete _pStateVQuality;
	delete _pStateBitrate;
	delete _pStateState;
	delete _pStateAddress;
	delete _pError;

	delete _pActionStopStream;
	delete _pActionStartStream;
	delete _pActionSetStreamConfig;
	delete _pActionGetStreamConfig;
	delete _pActionGetAddress;
	
}

