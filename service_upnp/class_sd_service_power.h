/*****************************************************************************
 * class_power.h
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
#ifndef __H_class_sd_svc_power__
#define __H_class_sd_svc_power__

#include "class_upnp_description.h"

/***************************************************************
CSDPowerState
*/
class CSDPowerState : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
public:	
	typedef enum PowerState
	{
		PowerState_battery = 0,
		PowerState_charging,
		PowerState_wall_nobattery,
		PowerState_Num
	}PowerState;

	CSDPowerState(CUpnpService* pService);
	~CSDPowerState();
	virtual void Update(bool bSendEvent);
private:
	const char*	_pAllowedValue[PowerState_Num];
};

/***************************************************************
CSDHeartBeat
*/
class CSDHeartBeat : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDHeartBeat(CUpnpService* pService);
	~CSDHeartBeat();
private:
};


/***************************************************************
CSDPowerNumber
*/
class CSDPowerNumber : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDPowerNumber(CUpnpService* pService);
	~CSDPowerNumber();
private:
};

/***************************************************************
CSDBatteryVolume
*/
class CSDBatteryVolume : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
public:
	typedef enum batteryVolume
	{
		batteryVolume_10 = 0,
		batteryVolume_30,
		batteryVolume_50,
		batteryVolume_70,
		batteryVolume_90,
		batteryVolume_warnning,
		batteryVolume_Num
	}batteryVolume;

	CSDBatteryVolume(CUpnpService* pService);
	~CSDBatteryVolume();
	virtual void Update(bool bSendEvent);
private:
	const char*	_pAllowedValue[batteryVolume_Num];
};

/***************************************************************
CSDPowerError
*/
class CSDPowerError : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDPowerError(CUpnpService* pService);
	~CSDPowerError();
private:

};


/***************************************************************
CCCActionPowerOff
*/
class CCCActionPowerOff : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionPowerOff(CUpnpService* pService);
	~CCCActionPowerOff();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionPowerOff
*/
class CCCActionReBoot : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionReBoot(CUpnpService* pService);
	~CCCActionReBoot();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetPowerState
*/
class CCCActionGetPowerState : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetPowerState(CUpnpService* pService);
	~CCCActionGetPowerState();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetBatteryVolume
*/
class CCCActionGetBatteryVolume : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetBatteryVolume(CUpnpService* pService);
	~CCCActionGetBatteryVolume();
	virtual void execution();
private:	
};

/***************************************************************
CUpnpSDPower
*/
class CUpnpSDPower : public CUpnpService
{
	typedef CUpnpService inherited;
public:
	CUpnpSDPower(CUpnpDevice*);
	~CUpnpSDPower();	

	void SendHeartBeat(UINT32 counter);
	int GetBatteryState();
	
private:
	CSDPowerState*				_pStatePowerState;
	CSDPowerNumber*				_pStateNumber;
	CSDBatteryVolume*			_pStateBatteryVolume;
	CSDPowerError*				_pError;
	CSDHeartBeat*				_pHeartBeat;

	CCCActionPowerOff* 			_pActionPowerOff;
	CCCActionReBoot*				_pActionReboot;
	CCCActionGetPowerState*		_pActionGetState;
	CCCActionGetBatteryVolume* 	_pActionGetBatteryVolume;
	
	CUpnpState*			_pStateIn[5];
	CUpnpState*			_pStateOut[5];

};

#endif
