/*****************************************************************************
* upnp_network.cpp:
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
#include <stdio.h>
#include <string.h>
#include "class_sd_service_network.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <math.h>
#include "class_ipc_rec.h"
#include "class_mail_thread.h"

#include "agbox.h"

#define MAX_AP_NUMBER 256

static int savedNumber = 0;
static agcmd_wpa_list_info_s AP_List[MAX_AP_NUMBER];

//static agcmd_wpa_network_list_s networkList[MAX_AP_NUMBER];

/***************************************************************
CSDSystemTime
*/
CSDSystemTime::CSDSystemTime
	(CUpnpService* pService
	) : inherited("SystemTime", false, 0, 0, 0,false, false, pService)
{

}
CSDSystemTime::~CSDSystemTime()
{

}


/***************************************************************
CSDSystemTimeZone
*/
CSDSystemTimeZone::CSDSystemTimeZone
	(CUpnpService* pService
	) : inherited("SystemTimeZone", false, 0, 0, 0,false, false, pService)
{

}
CSDSystemTimeZone::~CSDSystemTimeZone()
{

}


/***************************************************************
CSDSystemPositionLongitude
*/
CSDPositionLongitude::CSDPositionLongitude
	(CUpnpService* pService
	) : inherited("Longitude", false, 0, 0, 0,false, false, pService)
{

}
CSDPositionLongitude::~CSDPositionLongitude()
{

}



/***************************************************************
CSDPositionLatitude
*/
CSDPositionLatitude::CSDPositionLatitude
	(CUpnpService* pService
	) : inherited("Latitude", false, 0, 0, 0,false, false, pService)
{

}
CSDPositionLatitude::~CSDPositionLatitude()
{

}


/***************************************************************
CSDPositionElavation
*/
CSDPositionElavation::CSDPositionElavation
	(CUpnpService* pService
	) : inherited("Elavation", false, 0, 0, 0,false, false, pService)
{

}
CSDPositionElavation::~CSDPositionElavation()
{

}


 
/***************************************************************
CSDNetworkNum
*/
CSDNetworkNum::CSDNetworkNum
	(CUpnpService* pService
	) : inherited("networkNum", false, 0, 0, 0,false, false, pService)
{

}
CSDNetworkNum::~CSDNetworkNum()
{

}

/***************************************************************
CSDNetworkIndex
*/
CSDNetworkIndex::CSDNetworkIndex
	(CUpnpService* pService
	) : inherited("networkIndex", false, 0, 0, 0,false, false, pService)
{

}

CSDNetworkIndex::~CSDNetworkIndex()
{

}

/***************************************************************
CSDNetworkType
*/
CSDNetworkType::CSDNetworkType
	(CUpnpService* pService
	) : inherited("networkType", false, false, pService)
{
	_pAllowedValue[NetworkType_wifi] = "wifi";
	_pAllowedValue[NetworkType_cable] = "cable";
	_pAllowedValue[NetworkType_3g] = "3g";
	_pAllowedValue[NetworkType_blueTooth] = "bluetooth";
	_pAllowedValue[NetworkType_zigb] = "zigb";
	
	this->SetAllowedList(NetworkType_Num, (char**)_pAllowedValue);

}
		
CSDNetworkType::~CSDNetworkType()
{

}


/***************************************************************
CSDWirelessState
*/
CSDWirelessState::CSDWirelessState
	(CUpnpService* pService
	) : inherited("wifiState", true, true, pService)
{
	_pAllowedValue[WirelessState_AP] = "AP";
	_pAllowedValue[WirelessState_Client] = "Client";
	_pAllowedValue[WirelessState_Off] = "off";	
	this->SetAllowedList(WirelessState_Num, (char**)_pAllowedValue);
	//Update();
}
CSDWirelessState::~CSDWirelessState()
{

}

void CSDWirelessState::Update(bool bSendEvent)
{
	char tmp[AGCMD_PROPERTY_SIZE];	
	agcmd_property_get(WIFI_SWITCH_STATE, tmp, "NA");
	//printf("--CSDWirelessState:%s\n", tmp);
	if(strcasecmp(tmp, "OFF") == 0)
	{
		_wishValueIndex = WirelessState_Off;
	}
	else
	{
		agcmd_property_get(wifiModePropertyName, tmp, "NA");
		//printf("--CSDWirelessState:%s\n", tmp);
		if (strcasecmp(tmp, "AP") == 0) {
			_wishValueIndex = WirelessState_AP;
		} else if(strcasecmp(tmp, "WLAN")==0) {
			_wishValueIndex = WirelessState_Client;
		} else {
			_wishValueIndex = WirelessState_Off;
		}
	}
	this->setWishAsCurrent();
	inherited::Update(bSendEvent);
}

/***************************************************************
CSDIpAddress
*/
CSDIpAddress::CSDIpAddress
	(CUpnpService* pService
	): inherited("ipaddress", false, false, pService)
{

}

CSDIpAddress::~CSDIpAddress()
{

}


/***************************************************************
CSDNetmask
*/
CSDNetmask::CSDNetmask
	(CUpnpService* pService
	): inherited("netmask", false, false, pService)
{

}

CSDNetmask::~CSDNetmask()
{

}

/***************************************************************
CSDNetmask
*/
CSDNetGate::CSDNetGate
	(CUpnpService* pService
	): inherited("netGate", false, false, pService)
{

}

CSDNetGate::~CSDNetGate()
{

}

/***************************************************************
CSDNetDNS
*/
CSDNetDNS::CSDNetDNS
	(CUpnpService* pService
	): inherited("DNS", false, false, pService)
{

}

CSDNetDNS::~CSDNetDNS()
{

}

/***************************************************************
CSDAPName
*/
CSDAPName::CSDAPName
	(CUpnpService* pService
	): inherited("APName", false, false, pService)
{

}

CSDAPName::~CSDAPName()
{

}

/***************************************************************
CSDAPName
*/
CSDAPPassword::CSDAPPassword
	(CUpnpService* pService
	): inherited("APPass", false, false, pService)
{

}

CSDAPPassword::~CSDAPPassword()
{

}

/***************************************************************
CSDClientModeApName
*/
CSDClientModeApName::CSDClientModeApName
	(CUpnpService* pService
	): inherited("ClientApName", false, false, pService)
{

}

CSDClientModeApName::~CSDClientModeApName()
{

}

/***************************************************************
CSDClientModeApType
*/
CSDClientModeApType::CSDClientModeApType
	(CUpnpService* pService
	) : inherited("networkType", false, false, pService)
{
	_pAllowedValue[WirelessType_Open] = "open";
	_pAllowedValue[WirelessType_WEP] = "wep";
	_pAllowedValue[WirelessType_WPA] = "wpa";
	_pAllowedValue[WirelessType_WPA2] = "wpa2";	
	this->SetAllowedList(WirelessType_Num, (char**)_pAllowedValue);
}
		
CSDClientModeApType::~CSDClientModeApType()
{

}

/***************************************************************
CSDClientModeApPassword
*/
CSDClientModeApPassword::CSDClientModeApPassword
	(CUpnpService* pService
	): inherited("APType", false, false, pService)
{

}

CSDClientModeApPassword::~CSDClientModeApPassword()
{

}


/***************************************************************
CSDSavedAPNum
*/
CSDSavedAPNum::CSDSavedAPNum
	(CUpnpService* pService
	) : inherited("savedApNum", false, 0, 0, 0,false, false, pService)
{

}

CSDSavedAPNum::~CSDSavedAPNum()
{

}


/***************************************************************
CSDSavedAPName
*/
CSDSavedAPName::CSDSavedAPName
	(CUpnpService* pService
	): inherited("SavedApName", false, false, pService)
{

}

CSDSavedAPName::~CSDSavedAPName()
{

}


/***************************************************************
CSDNetworkError
*/
CSDNetworkError::CSDNetworkError
	(CUpnpService* pService
	): inherited("networkError", false, false, pService)
{

}

CSDNetworkError::~CSDNetworkError()
{

}

/***************************************************************
CCCActionSetMailAcnt
*/
CCCActionSetMailAcnt::CCCActionSetMailAcnt
	(CUpnpService* pService
	) : inherited("SetMailAcntInfor", pService)
{

}
CCCActionSetMailAcnt::~CCCActionSetMailAcnt(){}
void CCCActionSetMailAcnt::execution()
{
	CMailSendAccount *pAccount = (CMailSendAccount*)_ppInStates[0];
	CMailSendPwd	*pPwd = (CMailSendPwd*)_ppInStates[1];
	CMailSMTPServer	*pMailServer = (CMailSMTPServer*)_ppInStates[2];
	CMailSendTo* pSendTo = (CMailSendTo*)_ppInStates[3];
	for(int i = 0; i < 4; i++)
	{
		_ppInStates[i]->setWishAsCurrent();
	}
	char key[256];
	sprintf(key, "%s.%s", MailPropPreFix, MailPropAccount);
	agcmd_property_set(key, pAccount->getCurrentState());

	sprintf(key, "%s.%s", MailPropPreFix, MailPropPwd);
	agcmd_property_set(key, pPwd->getCurrentState());

	sprintf(key, "%s.%s", MailPropPreFix, MailPropServer);
	agcmd_property_set(key, pMailServer->getCurrentState());

	sprintf(key, "%s.%s", MailPropPreFix, MailPropSendTo);
	agcmd_property_set(key, pSendTo->getCurrentState());
}

/***************************************************************
CCCActionGetMailAcnt
*/
CCCActionGetMailAcnt::CCCActionGetMailAcnt
	(CUpnpService* pService
	) : inherited("GetMailAcntInfor", pService)
{

}
CCCActionGetMailAcnt::~CCCActionGetMailAcnt(){}
void CCCActionGetMailAcnt::execution()
{
	printf("-- get mail account execution\n");
	CMailSendAccount *pAccount = (CMailSendAccount*)_ppOutStates[0];
	CMailSendPwd	*pPwd = (CMailSendPwd*)_ppOutStates[1];
	CMailSMTPServer	*pMailServer = (CMailSMTPServer*)_ppOutStates[2];
	CMailSendTo* pSendTo = (CMailSendTo*)_ppOutStates[3];

	char key[256];
	char tmp[256];
	sprintf(key, "%s.%s", MailPropPreFix, MailPropAccount);
	agcmd_property_get(key, tmp, "");
	pAccount->SetCurrentState(tmp);
	
	sprintf(key, "%s.%s", MailPropPreFix, MailPropPwd);
	agcmd_property_get(key, tmp, "");
	pPwd->SetCurrentState(tmp);

	sprintf(key, "%s.%s", MailPropPreFix, MailPropServer);
	agcmd_property_get(key, tmp, "");
	pMailServer->SetCurrentState(tmp);
		
	sprintf(key, "%s.%s", MailPropPreFix, MailPropSendTo);
	agcmd_property_get(key, tmp, "");
	pSendTo->SetCurrentState(tmp);
}

/***************************************************************
CCCActionSendMail
*/
CCCActionSendMail::CCCActionSendMail
	(CUpnpService* pService
	) : inherited("SendMail", pService)
{

}

CCCActionSendMail::~CCCActionSendMail()
{

}

void CCCActionSendMail::execution()
{

}


/***************************************************************
CCCActionGetNetNum
*/
CCCActionGetNetNum::CCCActionGetNetNum
	(CUpnpService* pService
	) : inherited("GetNetNum", pService)
{

}
CCCActionGetNetNum::~CCCActionGetNetNum()
{

}
void CCCActionGetNetNum::execution()
{

}

/***************************************************************
CCCActionGetNetType
*/
CCCActionGetNetType::CCCActionGetNetType
	(CUpnpService* pService
	) : inherited("GetNetType", pService)
{

}
CCCActionGetNetType::~CCCActionGetNetType()
{

}
void CCCActionGetNetType::execution()
{

}

/***************************************************************
CCCActionGetWifiState
*/
CCCActionGetWifiState::CCCActionGetWifiState
	(CUpnpService* pService
	) : inherited("GetWifState", pService)
{

}
CCCActionGetWifiState::~CCCActionGetWifiState()
{

}
void CCCActionGetWifiState::execution()
{
	//printf("--CCCActionGetWifiState\n");
	//char tmp[AGCMD_PROPERTY_SIZE];
	CSDWirelessState *pWifiState = (CSDWirelessState*)_ppOutStates[0];
	pWifiState->Update(true);
	
}

/***************************************************************
CCCActionSetWifiApMode
*/
CCCActionSetWifiApMode::CCCActionSetWifiApMode
	(CUpnpService* pService
	) : inherited("SetWifiApMode", pService)
{

}
CCCActionSetWifiApMode::~CCCActionSetWifiApMode()
{

}
void CCCActionSetWifiApMode::execution()
{
	agcmd_property_set(WIFI_KEY, WIFI_AP_KEY);
}

/***************************************************************
CCCActionSetWifiClientMode
*/
CCCActionSetWifiClientMode::CCCActionSetWifiClientMode
	(CUpnpService* pService
	) : inherited("SetWifiClientMode", pService)
{

}
CCCActionSetWifiClientMode::~CCCActionSetWifiClientMode()
{

}
void CCCActionSetWifiClientMode::execution()
{
	agcmd_property_set(WIFI_KEY, WIFI_CLIENT_KEY);
}

/***************************************************************
CCCActionAddAP
*/
CCCActionAddAP::CCCActionAddAP
	(CUpnpService* pService
	) : inherited("AddAPPoint", pService)
{

}
CCCActionAddAP::~CCCActionAddAP()
{

}
void CCCActionAddAP::execution()
{
	CSDClientModeApName *pNetAccessToAPName = (CSDClientModeApName*)_ppInStates[0];
	CSDClientModeApType *pNetAccessToAPType = (CSDClientModeApType*)_ppInStates[1];
	CSDClientModeApPassword *pNetAccessToAPPass = (CSDClientModeApPassword*)_ppInStates[2];
	pNetAccessToAPName->setWishAsCurrent();
	pNetAccessToAPType->setWishAsCurrent();
	pNetAccessToAPPass->setWishAsCurrent();
	printf("--add ap: %s, %s, %s\n",pNetAccessToAPName->getCurrentState(),
			pNetAccessToAPType->getCurrentState(),
			pNetAccessToAPPass->getCurrentState());
	AddApNode(pNetAccessToAPName->getCurrentState(),
			pNetAccessToAPType->getCurrentState(),
			pNetAccessToAPPass->getCurrentState());
}

void CCCActionAddAP::AddApNode(char* ssid, char* type, char* pwd)
{
	char wlan_prop[AGCMD_PROPERTY_SIZE];
	char prebuild_prop[AGCMD_PROPERTY_SIZE];

	agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
	agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
	agcmd_wpa_add(wlan_prop, ssid, type, pwd);
	savedNumber = agcmd_wpa_list(wlan_prop, AP_List, MAX_AP_NUMBER);


	//agcmd_wpa_list_networks(wlan_prop, networkList);
}

/***************************************************************
CCCActionDelSavedAP
*/
CCCActionDelSavedAP::CCCActionDelSavedAP
	(CUpnpService* pService
	) : inherited("DelApPoint", pService)
{

}

CCCActionDelSavedAP::~CCCActionDelSavedAP()
{

}

void CCCActionDelSavedAP::execution()
{
	CSDClientModeApName *pNetAccessToAPName = (CSDClientModeApName *)_ppInStates[0];
	char wlan_prop[AGCMD_PROPERTY_SIZE];
	char prebuild_prop[AGCMD_PROPERTY_SIZE];

	pNetAccessToAPName->setWishAsCurrent();

	agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
	agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
	agcmd_wpa_del(wlan_prop, pNetAccessToAPName->getCurrentState());
	savedNumber = agcmd_wpa_list(wlan_prop, AP_List, MAX_AP_NUMBER);
}
	
/***************************************************************
CCCActionGetSavedAPNum
*/
CCCActionGetSavedAPNum::CCCActionGetSavedAPNum
	(CUpnpService* pService
	) : inherited("GetSavedApNum", pService)
{

}
CCCActionGetSavedAPNum::~CCCActionGetSavedAPNum()
{

}
void CCCActionGetSavedAPNum::execution()
{
	char wlan_prop[AGCMD_PROPERTY_SIZE];
	char prebuild_prop[AGCMD_PROPERTY_SIZE];

	agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
	agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
	savedNumber = agcmd_wpa_list(wlan_prop, AP_List, MAX_AP_NUMBER);
	if (savedNumber >= MAX_AP_NUMBER) {
		printf("-- save too much ap info\n");
	}
	//printf("-- saved number:%d\n", savedNumber);
	CSDSavedAPNum *pNetSavedAPNum = (CSDSavedAPNum*)_ppOutStates[0];
	pNetSavedAPNum->SetState(savedNumber);
}

/***************************************************************
CCCActionGetSavedApName
*/
CCCActionGetSavedApName::CCCActionGetSavedApName
	(CUpnpService* pService
	) : inherited("GetSavedApName", pService)
{

}
CCCActionGetSavedApName::~CCCActionGetSavedApName()
{

}
void CCCActionGetSavedApName::execution()
{
	CSDSavedAPNum* pNetworkIndex = (CSDSavedAPNum*)_ppInStates[0];
	int index = (int)pNetworkIndex->getWish();
	//printf("---CCCActionGetSavedApName:%f\n",pNetworkIndex->getState());
	if(index < MAX_AP_NUMBER)
	{
		CSDSavedAPName* pNetSavedAPName = (CSDSavedAPName*)_ppOutStates[0];
		pNetSavedAPName->SetCurrentState(AP_List[index].ssid);
	}
	else
	{
		printf("---enter a number out of MAX_AP_NUMBER");
	}
}

/***************************************************************
CCCActionConnect2SavedAP
*/
CCCActionConnect2SavedAP::CCCActionConnect2SavedAP
	(CUpnpService* pService
	) : inherited("Connect2savedAp", pService)
{

}
CCCActionConnect2SavedAP::~CCCActionConnect2SavedAP()
{

}
void CCCActionConnect2SavedAP::execution()
{
	//CSDSavedAPNum* pNetworkIndex
}

/***************************************************************
CCCActionSetSystemTime
*/

extern int agclkd_set_rtc(time_t utc);
//static char* timezongLink = "/castle/localtime";
//static char* timezongDir = "/usr/share/zoneinfo/TS/";
CCCActionSetSystemTime::CCCActionSetSystemTime
	(CUpnpService* pService
	) : inherited("SetSystemTime", pService)
{

}
CCCActionSetSystemTime::~CCCActionSetSystemTime()
{

}
void CCCActionSetSystemTime::execution()
{
	CSDSystemTime* pSystemTime = (CSDSystemTime*)_ppInStates[0];
	CSDSystemTimeZone* pSystemTimeZone = (CSDSystemTimeZone*)_ppInStates[1];;
	pSystemTime->setWishAsCurrent();
	pSystemTimeZone->setWishAsCurrent();
	
	time_t t, tRemote = 0;
	time(&t);
	tRemote = (time_t)pSystemTime->getState();
	//printf("---system time: %d, remote time:%d \n, timezone : %f \n", t, tRemote, pSystemTimeZone->getWish());
	if(fabs(difftime(t, tRemote)) > 120.0)
	{
		struct timeval tt;
		tt.tv_sec = tRemote;
		tt.tv_usec = 0;
		struct timezone tz;
		tz.tz_minuteswest = (int)(pSystemTimeZone->getState()*60);
		tz.tz_dsttime = 0;
		settimeofday(&tt, &tz);
		agclkd_set_rtc(tRemote);
	}
	{
		char tmp[256];
		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "%sGMT%+d",timezongDir,(int)pSystemTimeZone->getState());
		int rt = remove(timezongLink);
		rt = symlink(tmp, timezongLink);
	}
	// re start upnp service.
	CUpnpThread::getInstance()->RequireUpnpReboot();
}


/***************************************************************
CCCActionSetPositon
*/
#define POSITIONINFO_LONG "upnp.camera.posi.longi"
#define POSITIONINFO_LA "upnp.camera.posi.lati"
#define POSITIONINFO_ELVA "upnp.camera.posi.eleva"
CCCActionSetPositon::CCCActionSetPositon
	(CUpnpService* pService
	) : inherited("SetPostion", pService)
{

}
CCCActionSetPositon::~CCCActionSetPositon()
{

}
void CCCActionSetPositon::execution()
{
	CSDPositionLongitude* pPositionLongitude  = (CSDPositionLongitude*)_ppInStates[0];
	CSDPositionLatitude *pPositionLatitude = (CSDPositionLatitude*)_ppInStates[1];
	CSDPositionElavation *pPositionElavation = (CSDPositionElavation*)_ppInStates[2];
	pPositionLongitude->setWishAsCurrent();
	pPositionLatitude->setWishAsCurrent();
	pPositionElavation->setWishAsCurrent();
	char tmp[256];
	char lt, la;
	if(pPositionLongitude->getState() > 0)
		lt = 'E';
	else
		lt = 'W';
	if(pPositionLongitude->getState() > 0)
		la = 'N';
	else
		la = 'S';
	sprintf(tmp," (R)%c%3.5f %c%3.5f %0.2f"
		,lt,fabs(pPositionLongitude->getState())
		,la,fabs(pPositionLatitude->getState())
		, pPositionElavation->getState());

	printf("--posi from pad :%s\n", tmp);
	IPC_AVF_Client::getObject()->SetCameraLocation(tmp);
}


/***************************************************************
CUpnpSDNetwork
*/
CUpnpSDNetwork::CUpnpSDNetwork
	(CUpnpDevice* pDevice
	) : inherited("network"
			,"urn:schemas-upnp-org:service:SDCCNework:1"
			,"urn:upnp-org:serviceId:SDCCNework"
			,"/description/sd_network.xml"
			,"/upnp/sdnetcontrol"
			,"/upnp/sdnetevent"		
			,pDevice
			,30
			,30)
{
	_pSystemTime = new CSDSystemTime(this);
	_pSystemTimeZone = new CSDSystemTimeZone(this);
	_pPositionLongitude = new CSDPositionLongitude(this);
	_pPositionLatitude = new CSDPositionLatitude(this);
	_pPositionElavation = new CSDPositionElavation(this);
	
	_pMailAccount = new CMailSendAccount(this);
	_pMailPwd = new CMailSendPwd(this);
	_pSMTPServer = new CMailSMTPServer(this);
	_pSendTo = new CMailSendTo(this);
	_pMailContent = new CMailContent(this);
	
	_pNetworkNum = new CSDNetworkNum(this);
	_pNetworkIndex = new CSDNetworkIndex(this);
	_pNetworkType = new CSDNetworkType(this);
	_pNetWifiState = new CSDWirelessState(this);
	_pNetIpaddress = new CSDIpAddress(this);
	_pNetNetmask = new CSDNetmask(this);
	_pNetGate = new CSDNetGate(this);
	_pNetDns = new CSDNetDNS(this);
	_pNetAPName = new CSDAPName(this);	
	_pNetAPPassword = new CSDAPPassword(this);
	_pNetAccessToAPName = new CSDClientModeApName(this);
	_pNetAccessToAPType = new CSDClientModeApType(this);
	_pNetAccessToAPPass = new CSDClientModeApPassword(this);
	_pNetSavedAPNum = new CSDSavedAPNum(this);
	_pNetSavedAPName = new CSDSavedAPName(this);
	_pError = new CSDNetworkError(this);

	_pActionGetNetNum = new CCCActionGetNetNum(this);
	_pStateOut[0] = _pNetworkNum;
	_pActionGetNetNum->SetArguList(0, _pStateIn, 1, _pStateOut);
	
	_pActionGetNetType = new CCCActionGetNetType(this);
	_pStateIn[0] = _pNetworkIndex;
	_pStateOut[0] = _pNetworkType;
	_pActionGetNetType->SetArguList(1, _pStateIn, 1, _pStateOut);
	
	_pActionGetWifiState = new CCCActionGetWifiState(this);
	_pStateOut[0] = _pNetWifiState;
	_pActionGetWifiState->SetArguList(0, _pStateIn, 1, _pStateOut);
	
	_pActionSetWifiApMode = new CCCActionSetWifiApMode(this);
	
	_pActionSetClientMode = new CCCActionSetWifiClientMode(this);
	//_pStateIn[0] = _pNetAccessToAPName;
	//_pStateIn[1] = _pNetAccessToAPType;
	//_pStateIn[2] = _pNetAccessToAPPass;
	_pActionSetClientMode->SetArguList(0, _pStateIn, 0, _pStateOut);
	
	_pActionGetSavedApNum = new CCCActionGetSavedAPNum(this);
	_pStateOut[0] = _pNetSavedAPNum;
	_pActionGetSavedApNum->SetArguList(0, _pStateIn, 1, _pStateOut);
		
	_pActionGetSavedApName = new CCCActionGetSavedApName(this);
	_pStateIn[0] = _pNetSavedAPNum;
	_pStateOut[0] = _pNetSavedAPName;
	_pActionGetSavedApName->SetArguList(1, _pStateIn, 1, _pStateOut);
	
	_pActionConnect2SavedAp = new CCCActionConnect2SavedAP(this);
	_pStateIn[0] = _pNetworkIndex;
	_pActionConnect2SavedAp->SetArguList(1, _pStateIn, 0, _pStateOut);

	_pActionAddAp = new CCCActionAddAP(this);
	_pStateIn[0] = _pNetAccessToAPName;
	_pStateIn[1] = _pNetAccessToAPType;
	_pStateIn[2] = _pNetAccessToAPPass;
	_pActionAddAp->SetArguList(3, _pStateIn, 0, _pStateOut);

	_pActionDelAp = new CCCActionDelSavedAP(this);
	_pStateIn[0] = _pNetAccessToAPName;
	_pActionDelAp->SetArguList(1, _pStateIn, 0, _pStateOut);

	_pSetSystemTime = new CCCActionSetSystemTime(this);
	_pStateIn[0] = _pSystemTime;
	_pStateIn[1] = _pSystemTimeZone;
	_pSetSystemTime->SetArguList(2, _pStateIn, 0, _pStateOut);
	
	_pSetPosition = new CCCActionSetPositon(this);
	_pStateIn[0] = _pPositionLongitude;
	_pStateIn[1] = _pPositionLatitude;
	_pStateIn[2] = _pPositionElavation;
	_pSetPosition->SetArguList(3, _pStateIn, 0, _pStateOut);


	_pActionSetMailInfor = new CCCActionSetMailAcnt(this);
	_pStateIn[0] = _pMailAccount;
	_pStateIn[1] = _pMailPwd;
	_pStateIn[2] = _pSMTPServer;
	_pStateIn[3] = _pSendTo;
	_pActionSetMailInfor->SetArguList(4, _pStateIn, 0, _pStateOut);
	
	_pActionGetMailInfor = new CCCActionGetMailAcnt(this);
	_pStateOut[0] = _pMailAccount;
	_pStateOut[1] = _pMailPwd;
	_pStateOut[2] = _pSMTPServer;
	_pStateOut[3] = _pSendTo;
	_pActionGetMailInfor->SetArguList(0, _pStateIn, 4, _pStateOut);

	_pActionSendMail = new CCCActionSendMail(this);
	_pStateIn[0] = _pMailAccount;
	_pActionSendMail->SetArguList(1, _pStateIn, 0, _pStateOut);

}

CUpnpSDNetwork::~CUpnpSDNetwork()
{
	delete _pNetworkNum;
	delete _pNetworkIndex;
	delete _pNetworkType;
	delete _pNetWifiState;
	delete _pNetIpaddress;
	delete _pNetNetmask;
	delete _pNetGate;
	delete _pNetDns;
	delete _pNetAPName;
	delete _pNetAPPassword;
	delete _pNetAccessToAPName;
	delete _pNetAccessToAPType;
	delete _pNetAccessToAPPass;
	delete _pNetSavedAPNum;	
	delete _pNetSavedAPName;
	delete _pError;

	delete _pSystemTime;
	delete _pSystemTimeZone;
	delete _pPositionLongitude;
	delete _pPositionLatitude;
	delete _pPositionElavation;

	delete _pMailAccount;
	delete _pMailPwd;
	delete _pSMTPServer;
	delete _pSendTo;
	

	delete _pActionGetNetNum;
	delete _pActionGetNetType;
	delete _pActionGetWifiState;
	delete _pActionSetWifiApMode;
	delete _pActionSetClientMode;
	delete _pActionGetSavedApNum;
	delete _pActionGetSavedApName;
	delete _pActionConnect2SavedAp;

	delete _pActionAddAp;
	delete _pActionDelAp;

	delete _pSetSystemTime;
	delete _pSetPosition;

	delete _pActionSetMailInfor;
	delete _pActionGetMailInfor;
	delete _pActionSendMail;
	
}

int CUpnpSDNetwork::GetWifiMode()
{
	//
	_pNetWifiState->Update(true);
	return _pNetWifiState->GetCurrentIndex();
}

int CUpnpSDNetwork::GetWifiState()
{
	//printf("----GetWifiState\n");
	char value[256];
	agcmd_property_get("temp.wifi.status", value, "");
	//printf("----GetWifiState : %s\n", value);
	if(strcmp(value, "on") == 0)
	{
		int rt = CUpnpThread::getInstance()->CheckIpStatus();
		if(rt == 0)
			return 0;
		else
			return 1;
	}
	else if(strcmp(value, "off") == 0)
	{
		return 2;
	}
}

int CUpnpSDNetwork::AddApNode(char* ssid, char* type, char* pwd)
{
	_pActionAddAp->AddApNode(ssid, type, pwd);
}

int CUpnpSDNetwork::SetWifiMode(int mode, char* ssid)
{
	//todo
	return 0;
}

