/*****************************************************************************
 * class_network.h
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
#ifndef __H_class_sd_svc_network__
#define __H_class_sd_svc_network__

#include "class_upnp_description.h"


/***************************************************************
CSDSystemTime
*/
class CSDSystemTime : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDSystemTime(CUpnpService* pService);
	~CSDSystemTime();
private:
	
};

/***************************************************************
CSDSystemTimeZone
*/
class CSDSystemTimeZone : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDSystemTimeZone(CUpnpService* pService);
	~CSDSystemTimeZone();
private:
	
};

/***************************************************************
CSDSystemPositionLongitude
*/
class CSDPositionLongitude : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDPositionLongitude(CUpnpService* pService);
	~CSDPositionLongitude();
private:
	
};

/***************************************************************
CSDPositionLatitude
*/
class CSDPositionLatitude : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDPositionLatitude(CUpnpService* pService);
	~CSDPositionLatitude();
private:
	
};

/***************************************************************
CSDPositionElavation
*/
class CSDPositionElavation : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDPositionElavation(CUpnpService* pService);
	~CSDPositionElavation();
private:
	
};



/***************************************************************
CSDNetworkNum
*/
class CSDNetworkNum : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDNetworkNum(CUpnpService* pService);
	~CSDNetworkNum();
private:
	
};

/***************************************************************
CSDPowerState
*/
class CSDNetworkIndex : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDNetworkIndex(CUpnpService* pService);
	~CSDNetworkIndex();
private:
	
};

/***************************************************************
CSDNetworkType
*/
class CSDNetworkType : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum NetworkType
	{
		NetworkType_wifi = 0,
		NetworkType_cable,
		NetworkType_3g,
		NetworkType_blueTooth,
		NetworkType_zigb,
		NetworkType_Num
	}NetworkType;
public:
	CSDNetworkType(CUpnpService* pService);
	~CSDNetworkType();
private:
	const char*	_pAllowedValue[NetworkType_Num];
};


/***************************************************************
CSDWirelessState
*/
class CSDWirelessState : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
public:
	typedef enum WirelessState
	{
		WirelessState_AP = 0,
		WirelessState_Client,
		WirelessState_Off,
		WirelessState_Num
	}WirelessState;

	CSDWirelessState(CUpnpService* pService);
	~CSDWirelessState();

	virtual void Update(bool bSendEvent);
private:
	const char*	_pAllowedValue[WirelessState_Num];
};


/***************************************************************
CSDIpAddress
*/
class CSDIpAddress : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDIpAddress(CUpnpService* pService);
	~CSDIpAddress();
private:
};

/***************************************************************
CSDNetmask
*/
class CSDNetmask : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDNetmask(CUpnpService* pService);
	~CSDNetmask();
private:
};

/***************************************************************
CSDNetmask
*/
class CSDNetGate : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDNetGate(CUpnpService* pService);
	~CSDNetGate();
private:
};

/***************************************************************
CSDNetDNS
*/
class CSDNetDNS : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDNetDNS(CUpnpService* pService);
	~CSDNetDNS();
private:
};

/***************************************************************
CSDAPName
*/
class CSDAPName : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDAPName(CUpnpService* pService);
	~CSDAPName();
private:
};

/***************************************************************
CSDAPName
*/
class CSDAPPassword : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDAPPassword(CUpnpService* pService);
	~CSDAPPassword();
private:
};

/***************************************************************
CSDClientModeApName
*/
class CSDClientModeApName : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDClientModeApName(CUpnpService* pService);
	~CSDClientModeApName();
private:
};

/***************************************************************
CSDClientModeApType
*/
class CSDClientModeApType : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
	typedef enum WirelessType
	{
		WirelessType_Open = 0,
		WirelessType_WEP,
		WirelessType_WPA,
		WirelessType_WPA2,
		WirelessType_Num
	}WirelessState;
public:
	CSDClientModeApType(CUpnpService* pService);
	~CSDClientModeApType();
private:
	const char*	_pAllowedValue[WirelessType_Num];
};

/***************************************************************
CSDClientModeApPassword
*/
class CSDClientModeApPassword : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDClientModeApPassword(CUpnpService* pService);
	~CSDClientModeApPassword();
private:
};

/***************************************************************
CSDSavedAPNum
*/
class CSDSavedAPNum : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDSavedAPNum(CUpnpService* pService);
	~CSDSavedAPNum();
private:
	
};

/***************************************************************
CSDClientModeApPassword
*/
class CSDSavedAPName : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDSavedAPName(CUpnpService* pService);
	~CSDSavedAPName();
private:
};

/***************************************************************
CSDNetworkError
*/
class CSDNetworkError : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDNetworkError(CUpnpService* pService);
	~CSDNetworkError();
private:
};

/***************************************************************
CMailSendAccount
*/
class CMailSendAccount : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CMailSendAccount
		(CUpnpService* pService
		): inherited("MailSendAccount", false, false, pService)
	{
	};
	~CMailSendAccount(){};
private:
};

/***************************************************************
CMailSendPwd
*/
class CMailSendPwd : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CMailSendPwd(CUpnpService* pService
		): inherited("MailSendPwd", false, false, pService)
	{
	};
	~CMailSendPwd(){};
private:
};

/***************************************************************
CMailSMTPServer
*/
class CMailSMTPServer : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CMailSMTPServer(CUpnpService* pService
		): inherited("MailXMTPServer", false, false, pService)
	{
	};
	~CMailSMTPServer(){};
private:
};


/***************************************************************
CMailSendTo
*/
class CMailSendTo : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CMailSendTo(CUpnpService* pService
		): inherited("MailSendTo", false, false, pService)
	{
	};
	~CMailSendTo(){};
private:
};


/***************************************************************
CMailEvent
*/
class CMailContent : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CMailContent(CUpnpService* pService
		): inherited("MailContent", false, false, pService)
	{
	};
	~CMailContent(){};
private:
};


/***************************************************************
CCCActionSetMailAcnt
*/
class CCCActionSetMailAcnt : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetMailAcnt(CUpnpService* pService);
	~CCCActionSetMailAcnt();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetMailAcnt
*/
class CCCActionGetMailAcnt : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetMailAcnt(CUpnpService* pService);
	~CCCActionGetMailAcnt();
	virtual void execution();
private:	
};

class CCCActionSendMail : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSendMail(CUpnpService* pService);
	~CCCActionSendMail();
	virtual void execution();
private:	
};


/***************************************************************
CCCActionGetNetNum
*/
class CCCActionGetNetNum : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetNetNum(CUpnpService* pService);
	~CCCActionGetNetNum();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetNetType
*/
class CCCActionGetNetType : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetNetType(CUpnpService* pService);
	~CCCActionGetNetType();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetWifiState
*/
class CCCActionGetWifiState : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetWifiState(CUpnpService* pService);
	~CCCActionGetWifiState();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionSetWifiApMode
*/
class CCCActionSetWifiApMode : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetWifiApMode(CUpnpService* pService);
	~CCCActionSetWifiApMode();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionSetWifiClientMode
*/
class CCCActionSetWifiClientMode : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetWifiClientMode(CUpnpService* pService);
	~CCCActionSetWifiClientMode();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionAddAP
*/
class CCCActionAddAP : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionAddAP(CUpnpService* pService);
	~CCCActionAddAP();
	virtual void execution();
	void AddApNode(char* ssid, char* type, char* pwd);
private:	
};

/***************************************************************
CCCActionAddSavedAP
*/
class CCCActionDelSavedAP : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionDelSavedAP(CUpnpService* pService);
	~CCCActionDelSavedAP();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetSavedAPNum
*/
class CCCActionGetSavedAPNum : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetSavedAPNum(CUpnpService* pService);
	~CCCActionGetSavedAPNum();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetSavedApName
*/
class CCCActionGetSavedApName : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetSavedApName(CUpnpService* pService);
	~CCCActionGetSavedApName();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionConnect2SavedAP
*/
class CCCActionConnect2SavedAP : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionConnect2SavedAP(CUpnpService* pService);
	~CCCActionConnect2SavedAP();
	virtual void execution();
private:	
}; 

/***************************************************************
CCCActionSetSystemTime
*/
class CCCActionSetSystemTime : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetSystemTime(CUpnpService* pService);
	~CCCActionSetSystemTime();
	virtual void execution();
private:	
}; 

/***************************************************************
CCCActionSetPositon
*/
class CCCActionSetPositon : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetPositon(CUpnpService* pService);
	~CCCActionSetPositon();
	virtual void execution();
private:	
};


/***************************************************************
CUpnpSDNetwork
*/
class CUpnpSDNetwork : public CUpnpService
{
	typedef CUpnpService inherited;
public:
	CUpnpSDNetwork(CUpnpDevice*);
	~CUpnpSDNetwork();

	int GetWifiMode();
	int GetWifiState();
	int AddApNode(char* ssid, char* type, char* pwd);
	int SetWifiMode(int mode, char* ssid = NULL); //mode: 0 AP,1, WLAN, 2 OFF.. ssid: for WLAN

private:
	CSDSystemTime 				*_pSystemTime;
	CSDSystemTimeZone 			*_pSystemTimeZone;
	CSDPositionLongitude 		*_pPositionLongitude;
	CSDPositionLatitude 		*_pPositionLatitude;
	CSDPositionElavation 		*_pPositionElavation;

	CMailSendAccount			*_pMailAccount;
	CMailSendPwd				*_pMailPwd;
	CMailSMTPServer				*_pSMTPServer;
	CMailSendTo					*_pSendTo;
	CMailContent				*_pMailContent;
	
	CSDNetworkNum 				*_pNetworkNum;
	CSDNetworkIndex				*_pNetworkIndex;
	CSDNetworkType 				*_pNetworkType;
	CSDWirelessState 			*_pNetWifiState;
	CSDIpAddress				*_pNetIpaddress;
	CSDNetmask 					*_pNetNetmask;
	CSDNetGate 					*_pNetGate;
	CSDNetDNS					*_pNetDns;
	CSDAPName 					*_pNetAPName;	
	CSDAPPassword				*_pNetAPPassword;
	CSDClientModeApName			*_pNetAccessToAPName;
	CSDClientModeApType			*_pNetAccessToAPType;
	CSDClientModeApPassword		*_pNetAccessToAPPass;
	CSDSavedAPNum				*_pNetSavedAPNum;
	CSDSavedAPName 				*_pNetSavedAPName;
	CSDNetworkError 			*_pError;

	CCCActionGetNetNum			*_pActionGetNetNum;
	CCCActionGetNetType			*_pActionGetNetType;
	CCCActionGetWifiState		*_pActionGetWifiState;
	CCCActionSetWifiApMode 		*_pActionSetWifiApMode;
	CCCActionSetWifiClientMode 	*_pActionSetClientMode;
	CCCActionGetSavedAPNum		*_pActionGetSavedApNum;
	CCCActionGetSavedApName 	*_pActionGetSavedApName;
	CCCActionConnect2SavedAP	*_pActionConnect2SavedAp;
	CCCActionAddAP				*_pActionAddAp;
	CCCActionDelSavedAP			*_pActionDelAp;

	CCCActionSetSystemTime		*_pSetSystemTime;
	CCCActionSetPositon  		*_pSetPosition;

	CCCActionSetMailAcnt		*_pActionSetMailInfor;
	CCCActionGetMailAcnt		*_pActionGetMailInfor;
	CCCActionSendMail			*_pActionSendMail;
		
	CUpnpState*			_pStateIn[8];
	CUpnpState*			_pStateOut[8];

};

#endif
