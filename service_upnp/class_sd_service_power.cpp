/*****************************************************************************
 * upnp_power.cpp:
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
#include "class_sd_service_power.h"
#include "agbox.h"

extern void DestroyUpnpService(bool bReboot);

#define VOTAGE_MAX 4200
#define VOTAGE_MIN 3700

/***************************************************************
CSDHeartBeat
*/
CSDHeartBeat::CSDHeartBeat
	(CUpnpService* pService
	)  : inherited("heartBeat", false, 0, 0, 0,true, true, pService)
{
	this->SetCurrentState("0");
}
CSDHeartBeat::~CSDHeartBeat()
{

}

/***************************************************************
CSDPowerState
*/
CSDPowerState::CSDPowerState
	(CUpnpService* pService
	) : inherited("powerState", true, true, pService)
{
	_pAllowedValue[PowerState_battery] = "battery";
	_pAllowedValue[PowerState_charging] = "charging";
	_pAllowedValue[PowerState_wall_nobattery] = "wall_nobattery";
	
	this->SetAllowedList(PowerState_Num, (char**)_pAllowedValue);		
}

CSDPowerState::~CSDPowerState()
{

}

void CSDPowerState::Update(bool bSendEvent)
{
	char tmp[AGCMD_PROPERTY_SIZE];
	char tmp1[256];

	agcmd_property_get(powerstateName, tmp, "Unknown");
	agcmd_property_get(propPowerSupplyOnline, tmp1, "0");
	if(strcmp(tmp, "Full") == 0) {
		if(strcmp(tmp1, "1")== 0)
			_wishValueIndex = PowerState_wall_nobattery;
		else
			_wishValueIndex = PowerState_battery;
	} else if(strcmp(tmp, "Not charging") == 0) {
		_wishValueIndex = PowerState_battery;
	} else if(strcmp(tmp, "Discharging") == 0) {
		_wishValueIndex = PowerState_battery;
	} else if(strcmp(tmp, "Charging") == 0) {
		_wishValueIndex = PowerState_charging;
	} else {
		_wishValueIndex = PowerState_battery;
	}
	//CSDPowerState* _pStatePowerState
	//printf("--power state update: %s, %s, %d\n", tmp, tmp1, _wishValueIndex);
	this->setWishAsCurrent();
	inherited::Update(bSendEvent);
}

/***************************************************************
CSDPowerNumber
*/
CSDPowerNumber::CSDPowerNumber
	(CUpnpService* pService
	)  : inherited("powerNum", false, 0, 0, 0,false, false, pService)
{
	this->SetCurrentState("1");
}
CSDPowerNumber::~CSDPowerNumber()
{

}


/***************************************************************
CSDBatteryVolume
*/

CSDBatteryVolume::CSDBatteryVolume
	(CUpnpService* pService
	) : inherited("batteryVolume", true, true, pService)
{
	_pAllowedValue[batteryVolume_warnning] = "warnning";
	_pAllowedValue[batteryVolume_10] = "volume10";
	_pAllowedValue[batteryVolume_30] = "volume30";
	_pAllowedValue[batteryVolume_50] = "volume50";
	_pAllowedValue[batteryVolume_70] = "volume70";
	_pAllowedValue[batteryVolume_90] = "volume90";	
	this->SetAllowedList(batteryVolume_Num, (char**)_pAllowedValue);		
}

CSDBatteryVolume::~CSDBatteryVolume()
{

}

void CSDBatteryVolume::Update(bool bSendEvent)
{
	char tmp[AGCMD_PROPERTY_SIZE];

	agcmd_property_get(propPowerSupplyBCapacityLevel, tmp, "Unknown");
	if (strcmp(tmp, "Full") == 0) {
		_wishValueIndex = batteryVolume_90;
	} else if (strcmp(tmp, "High") == 0) {
		_wishValueIndex = batteryVolume_70;
	} else if (strcmp(tmp, "Normal") == 0) {
		_wishValueIndex = batteryVolume_50;
	} else if (strcmp(tmp, "Low") == 0) {
		_wishValueIndex = batteryVolume_30;
	} else if (strcmp(tmp, "Critical") == 0) {
		_wishValueIndex = batteryVolume_10;
	} else {
		_wishValueIndex = batteryVolume_warnning;
	}
	this->setWishAsCurrent();
	inherited::Update(bSendEvent);
}

/***************************************************************
CSDStreamError
*/
CSDPowerError::CSDPowerError
	(CUpnpService* pService
	): inherited("error", true, true, pService)
{

}

CSDPowerError::~CSDPowerError()
{

}

/***************************************************************
CCCActionPowerOff
*/

CCCActionPowerOff::CCCActionPowerOff
	(CUpnpService* pService
	) : inherited("PowerOff", pService)
{

}
CCCActionPowerOff::~CCCActionPowerOff()
{

}
void CCCActionPowerOff::execution()
{
	DestroyUpnpService(false);
}


/***************************************************************
CCCActionReBoot
*/
CCCActionReBoot::CCCActionReBoot
	(CUpnpService* pService
	) : inherited("Reboot", pService)
{

}
CCCActionReBoot::~CCCActionReBoot()
{

}
void CCCActionReBoot::execution()
{
	DestroyUpnpService(true);
}

/***************************************************************
CCCActionGetPowerState
*/

CCCActionGetPowerState::CCCActionGetPowerState
	(CUpnpService* pService
	) : inherited("GetPowerState", pService) 
{
	
}

CCCActionGetPowerState::~CCCActionGetPowerState()
{

}

void CCCActionGetPowerState::execution()
{
	CSDPowerState* pState = (CSDPowerState*)_ppOutStates[0];
	pState->Update(true);	

	CSDPowerNumber* pNmber = (CSDPowerNumber*)_ppOutStates[1];
	pNmber->SetState(1);
}

/***************************************************************
CCCActionGetBatteryVolume
*/

CCCActionGetBatteryVolume::CCCActionGetBatteryVolume
	(CUpnpService* pService
	) : inherited("GetBatteryState", pService) 
{

}

CCCActionGetBatteryVolume::~CCCActionGetBatteryVolume()
{

}

void CCCActionGetBatteryVolume::execution()
{
	//printf("---------get batteryVolume\n");
	CSDBatteryVolume* pVolume = (CSDBatteryVolume*)_ppOutStates[0];
	pVolume->Update(true);	
}

/***************************************************************
CUpnpSDPower
*/
CUpnpSDPower::CUpnpSDPower
	(CUpnpDevice* pDevice
	) : inherited("power"
			,"urn:schemas-upnp-org:service:SDCCPower:1"
			,"urn:upnp-org:serviceId:SDCCPower"
			,"/description/sd_power.xml"
			,"/upnp/sdpowcontrol"
			,"/upnp/sdpowevent"
			,pDevice
			,10
			,10)
{ 
	_pStatePowerState = new CSDPowerState(this);
	_pStateNumber = new CSDPowerNumber(this);
	_pStateBatteryVolume = new CSDBatteryVolume(this);
	_pError = new CSDPowerError(this);
	_pHeartBeat = new CSDHeartBeat(this);

	_pActionPowerOff = new CCCActionPowerOff(this);
	_pActionReboot = new CCCActionReBoot(this);
	_pActionGetState = new CCCActionGetPowerState(this);
	_pStateOut[0] = _pStatePowerState;
	_pStateOut[1] = _pStateNumber;
	_pActionGetState->SetArguList(0, _pStateIn, 2, _pStateOut);
	
	_pActionGetBatteryVolume = new CCCActionGetBatteryVolume(this);
	_pStateOut[0] = _pStateBatteryVolume;
	_pActionGetBatteryVolume->SetArguList(0, _pStateIn, 1, _pStateOut);

	
}

CUpnpSDPower::~CUpnpSDPower()
{
	delete _pStatePowerState;
	delete _pStateNumber;
	delete _pStateBatteryVolume;
	delete _pError;
	delete _pHeartBeat;

	delete _pActionPowerOff;
	delete _pActionReboot;
	delete _pActionGetState;
	delete _pActionGetBatteryVolume;
}

void CUpnpSDPower::SendHeartBeat(UINT32 counter)
{
	char tmp[256];
	memset(tmp, 0, 256);
	sprintf(tmp, "%d", counter);
	_pHeartBeat->SetCurrentState(tmp);
}

int CUpnpSDPower::GetBatteryState()
{
	int rt = 0;
	char tmp[AGCMD_PROPERTY_SIZE];

	agcmd_property_get(propPowerSupplyBCapacity, tmp, "0");
	rt = strtoul(tmp, NULL, 0);

	return rt;
}

