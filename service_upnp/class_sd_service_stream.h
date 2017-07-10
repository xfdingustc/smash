/*****************************************************************************
 * class_.h
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
#ifndef __H_class_sd_svc_stream__
#define __H_class_sd_svc_stream__

#include "class_upnp_description.h"

/***************************************************************
CSDStreamProtocol
*/
class CSDStreamProtocol : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum StreamProtol
	{
		StreamProtol_http = 0,
		StreamProtol_rtsp,
		StreamProtol_hlv,
		StreamProtol_Num
	}StreamProtol;
public:
	CSDStreamProtocol(CUpnpService* pService);
	~CSDStreamProtocol();
private:
	const char*	_pAllowedValue[StreamProtol_Num];
};

/***************************************************************
CSDStreamVCodec
*/
class CSDStreamVCodec : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum StreamVCodec
	{
		StreamVCodec_mjpeg = 0,
		StreamVCodec_h264,
		//StreamCodec_vp6,
		StreamVCodec_Num
	}StreamVCodec;
public:
	CSDStreamVCodec(CUpnpService* pService);
	~CSDStreamVCodec();
private:
	const char*	_pAllowedValue[StreamVCodec_Num];
};

/***************************************************************
CSDStreamACodec
*/
class CSDStreamACodec : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum StreamACodec
	{
		StreamACodec_AAC = 0,
		StreamACodec_PCM,
		StreamACodec_MP3,
		StreamACodec_Num
	}StreamACodec;
public:
	CSDStreamACodec(CUpnpService* pService);
	~CSDStreamACodec();
private:
	const char*	_pAllowedValue[StreamACodec_Num];
};

/***************************************************************
CSDStreamVSize
*/
class CSDStreamVSize : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
public:	
	typedef enum StreamVSize
	{
		StreamVSize_1080p = 0,
		StreamVSize_720p,
		StreamVSize_576p,
		StreamVSize_288p,
		StreamVSize_Num
	}StreamVSize;

	CSDStreamVSize(CUpnpService* pService);
	~CSDStreamVSize();
	bool BigStream();
private:
	const char*	_pAllowedValue[StreamVSize_Num];
};

/***************************************************************
CSDStreamVFPS
*/
class CSDStreamVFPS : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum StreamVFPS
	{
		StreamVFPS_60 = 0,
		StreamVFPS_30,
		StreamVFPS_15,
		StreamVFPS_8,
		StreamVFPS_4,
		StreamVFPS_Num
	}StreamVFPS;
public:
	CSDStreamVFPS(CUpnpService* pService);
	~CSDStreamVFPS();
private:
	const char*	_pAllowedValue[StreamVFPS_Num];
};

/***************************************************************
CSDStreamVQuality
*/
class CSDStreamVQuality : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum StreamVQuality
	{
		StreamVQuality_HQ = 0,
		StreamVQuality_NORMAL,
		StreamVQuality_SOSO,
		StreamVQuality_LONG,
		StreamVQuality_Num
	}StreamVQuality;
public:
	CSDStreamVQuality(CUpnpService* pService);
	~CSDStreamVQuality();
private:
	const char*	_pAllowedValue[StreamVQuality_Num];
};

/***************************************************************
CSDStreamBitrate
*/
class CSDStreamBitrate : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDStreamBitrate(CUpnpService* pService);
	~CSDStreamBitrate();
private:
};

/***************************************************************
CSDStreamState
*/
class CSDStreamState : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum StreamState
	{
		StreamState_ON = 0,
		StreamState_OFF,
		StreamState_Num,
	}StreamState;
public:
	
	CSDStreamState(CUpnpService* pService);
	~CSDStreamState();
private:
	const char*	_pAllowedValue[StreamState_Num];
};

/***************************************************************
CSDStreamAddress
*/
class CSDStreamAddress : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDStreamAddress(CUpnpService* pService);
	~CSDStreamAddress();
private:

};

/***************************************************************
CSDStreamError
*/
class CSDStreamError : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDStreamError(CUpnpService* pService);
	~CSDStreamError();
private:

};

/***************************************************************
CCCActionStopStream
*/
class CCCActionStopStream : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionStopStream(CUpnpService* pService);
	~CCCActionStopStream();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionStartStream
*/
class CCCActionStartStream : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionStartStream(CUpnpService* pService);
	~CCCActionStartStream();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionRecStartRec
*/
class CCCActionSetStreamConfig : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetStreamConfig(CUpnpService* pService);
	~CCCActionSetStreamConfig();
	virtual void execution();
private:
	
};

/***************************************************************
CCCActionRecStartRec
*/
class CCCActionGetStreamConfig : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetStreamConfig(CUpnpService* pService);
	~CCCActionGetStreamConfig();
	virtual void execution();
private:
	
};

/***************************************************************
CCCActionRecStartRec
*/
class CCCActionGetAddress : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetAddress(CUpnpService* pService);
	~CCCActionGetAddress();
	virtual void execution();
private:
	
};


/***************************************************************
CUpnpSDLiveStream
*/
class CUpnpSDLiveStream : public CUpnpService
{
	typedef CUpnpService inherited;
public:
	CUpnpSDLiveStream(CUpnpDevice*);
	~CUpnpSDLiveStream();	
	
private:
	CSDStreamProtocol*		_pStateProtocol;
	CSDStreamVCodec*		_pStateVCodec;
	CSDStreamACodec*		_pStateACodec;
	CSDStreamVSize*			_pStateVSize;
	CSDStreamVQuality*		_pStateVQuality;
	CSDStreamBitrate*			_pStateBitrate;
	CSDStreamState*			_pStateState;
	CSDStreamAddress*		_pStateAddress;
	CSDStreamError*			_pError;

	CCCActionStopStream* 		_pActionStopStream;
	CCCActionStartStream*		_pActionStartStream;
	CCCActionSetStreamConfig* 	_pActionSetStreamConfig;
	CCCActionGetStreamConfig* 	_pActionGetStreamConfig;
	CCCActionGetAddress*		_pActionGetAddress;
	
	CUpnpState*			_pStateIn[8];
	CUpnpState*			_pStateOut[8];

};



#endif
