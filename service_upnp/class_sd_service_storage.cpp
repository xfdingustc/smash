/*****************************************************************************
 * class_sd_service_storage.cpp:
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
#include "class_sd_service_storage.h"
#include <math.h>

#include "agbox.h"

const char* SDStatePropName = "temp.mmcblk0p1.status";
const char* SDSpacePropName = "temp.mmcblk0p1.total_size";
const char* SDFreeSpacePropName = "temp.mmcblk0p1.free_size";
/***************************************************************
CSDStorageState
*/
CSDStorageState::CSDStorageState
	(CUpnpService* pService
	) : inherited("storageState", true, true, pService)
{
	_pAllowedValue[StorageState_Ready] = "ready";
	_pAllowedValue[StorageState_loading] = "loading";
	_pAllowedValue[StorageState_error] = "error";
	_pAllowedValue[StorageState_offline] = "offline";
	_pAllowedValue[StorageState_usbdisk] = "usb";
	this->SetAllowedList(StorageState_Num, (char**)_pAllowedValue);		
}

CSDStorageState::~CSDStorageState()
{

}

void CSDStorageState::Update(bool bSendEvent)
{
	//printf("storage State update\n");
	char tmp[256];
	memset(tmp, 0, sizeof(tmp));
	agcmd_property_get(SDStatePropName, tmp ,"");
	//printf("--tf stat")
	if(strcmp(tmp, "mount_ok") == 0)
	{
		_wishValueIndex= StorageState_Ready;		
	}
	else if(strcmp(tmp, "unknown") == 0)
	{
		_wishValueIndex = StorageState_offline;
	}
	else if(strcmp(tmp, "checking") == 0)
	{
		_wishValueIndex = StorageState_loading;
	}
	else if(strcmp(tmp, "mount_fail") == 0)
	{
		_wishValueIndex = StorageState_error;
	}
	else if(strcmp(tmp, "ready") == 0)
	{
		_wishValueIndex = StorageState_usbdisk;
	}
	this->setWishAsCurrent();
	inherited::Update(bSendEvent);
}

/***************************************************************
CSDStorageNumber
*/
CSDStorageNumber::CSDStorageNumber
	(CUpnpService* pService
	)  : inherited("storageNumber", false, 0, 0, 0,false, false, pService)
{
	this->SetCurrentState("1");
}
CSDStorageNumber::~CSDStorageNumber()
{

}

/***************************************************************
CSDStorageNumber
*/
CSDStorageIndex::CSDStorageIndex
	(CUpnpService* pService
	)  : inherited("storageIndex", false, 0, 0, 0,false, false, pService)
{
	this->SetCurrentState("1");
}
CSDStorageIndex::~CSDStorageIndex()
{

}

/***************************************************************
CSDStorageSpace
*/
CSDStorageSpace::CSDStorageSpace
	(CUpnpService* pService
	)  : inherited("storageSpace", false, 0, 0, 0,false, false, pService)
{
	this->SetCurrentState("1");
}
CSDStorageSpace::~CSDStorageSpace()
{

}

/***************************************************************
CSDStorageFreeSpace
*/
CSDStorageFreeSpace::CSDStorageFreeSpace
	(CUpnpService* pService
	)  : inherited("storageFreeSpace", false, 0, 0, 0,false, false, pService)
{
	this->SetCurrentState("1");
}
CSDStorageFreeSpace::~CSDStorageFreeSpace()
{

}

/***************************************************************
CSDDefaultStorage
*/
CSDDefaultStorage::CSDDefaultStorage
	(CUpnpService* pService
	)  : inherited("defaultStorage", false, 0, 0, 0,false, false, pService)
{
	this->SetCurrentState("1");
}
CSDDefaultStorage::~CSDDefaultStorage()
{

}

/***************************************************************
CSDStorageAddress
*/
CSDStorageAddress::CSDStorageAddress
	(CUpnpService* pService
	): inherited("storageAddr", false, false, pService)
{
}
CSDStorageAddress::~CSDStorageAddress()
{

}



/***************************************************************
CSDStorageError
*/
CSDStorageError::CSDStorageError
	(CUpnpService* pService
	): inherited("error", true, true, pService)
{
}
CSDStorageError::~CSDStorageError()
{

}

/***************************************************************
CCCActionGetStorageNum
*/
CCCActionGetStorageNum::CCCActionGetStorageNum
	(CUpnpService* pService
	) : inherited("GetStorageNum", pService)
{

}
CCCActionGetStorageNum::~CCCActionGetStorageNum()
{

}
void CCCActionGetStorageNum::execution()
{

}


/***************************************************************
CCCActionGetStorageState
*/

	
CCCActionGetStorageState::CCCActionGetStorageState
	(CUpnpService* pService
	) : inherited("GetStorageState", pService)
{

}
CCCActionGetStorageState::~CCCActionGetStorageState()
{

}
void CCCActionGetStorageState::execution()
{
	CSDStorageIndex* pStorageIndex = (CSDStorageIndex*)_ppInStates[0];
	CSDStorageState* pStorageState = (CSDStorageState*)_ppOutStates[0];
	CSDStorageSpace* pStorageSpace = (CSDStorageSpace*)_ppOutStates[1];
	CSDStorageFreeSpace* pStorageFreeSpace = (CSDStorageFreeSpace*)_ppOutStates[2];
	char tmp[256];
	int itmp;
	memset(tmp, 0, sizeof(tmp));
	agcmd_property_get(SDStatePropName, tmp ,"");
	pStorageSpace->SetCurrentState("0");
	pStorageFreeSpace->SetCurrentState("0");
	if(strcmp(tmp, "mount_ok") == 0)
	{
		pStorageState->SetState(CSDStorageState::StorageState_Ready);
		memset(tmp, 0, sizeof(tmp));
		agcmd_property_get(SDSpacePropName, tmp ,"0");
		pStorageSpace->SetCurrentState(tmp);
		memset(tmp, 0, sizeof(tmp));
		agcmd_property_get(SDFreeSpacePropName, tmp ,"0");
		pStorageFreeSpace->SetCurrentState(tmp);		
	}
	else if(strcmp(tmp, "unknown") == 0)
	{
		pStorageState->SetState(CSDStorageState::StorageState_offline);
	}
	else if(strcmp(tmp, "checking") == 0)
	{
		pStorageState->SetState(CSDStorageState::StorageState_loading);
	}
	else if(strcmp(tmp, "mount_fail") == 0)
	{
		pStorageState->SetState(CSDStorageState::StorageState_error);
	}
	else if(strcmp(tmp, "ready") == 0)
	{
		pStorageState->SetState(CSDStorageState::StorageState_usbdisk);
	}

	
	//this->SetCurrentState(tmp);
}

/***************************************************************
CCCActionGetStorageAddress
*/
CCCActionGetStorageAddress::CCCActionGetStorageAddress
	(CUpnpService* pService
	) : inherited("GetStorageAddress", pService)
{

}
CCCActionGetStorageAddress::~CCCActionGetStorageAddress()
{

}
void CCCActionGetStorageAddress::execution()
{
	CSDStorageAddress* paddress = (CSDStorageAddress*) _ppOutStates[0];
	char tmp[256];
	agcmd_property_get("system.boot.enc_code", tmp, "unknow");
	paddress->SetCurrentState(tmp);
	//printf("----CCCActionGetStorageAddress:%s\n", paddress->getCurrentState());
}

/***************************************************************
CCCActionSetDefaultStorage
*/
CCCActionSetDefaultStorage::CCCActionSetDefaultStorage
	(CUpnpService* pService
	) : inherited("SetDefaultStorage", pService)
{

}
CCCActionSetDefaultStorage::~CCCActionSetDefaultStorage()
{

}
void CCCActionSetDefaultStorage::execution()
{

}

/***************************************************************
CCCActionGetDefaultStorage
*/
CCCActionGetDefaultStorage::CCCActionGetDefaultStorage
	(CUpnpService* pService
	) : inherited("GetDefaultStorage", pService)
{

}
CCCActionGetDefaultStorage::~CCCActionGetDefaultStorage()
{

}
void CCCActionGetDefaultStorage::execution()
{

}

/***************************************************************
CUpnpSDStorage
*/
CUpnpSDStorage::CUpnpSDStorage
	(CUpnpDevice* pDevice
	) : inherited("storage"
			,"urn:schemas-upnp-org:service:SDCCStorage:1"
			,"urn:upnp-org:serviceId:SDCCStorage"
			,"/description/sd_storage.xml"
			,"/upnp/sdstorcontrol"
			,"/upnp/sdstorevent"
			,pDevice
			,10
			,10)
{
	_pStorageState = new CSDStorageState(this);
	_pStorageNum = new CSDStorageNumber(this);
	_pStorageIndex= new CSDStorageIndex(this);;
	_pStorageSpace = new CSDStorageSpace(this);
	_pStorageFreeSpace = new CSDStorageFreeSpace(this);
	_pStorageStorageDefault = new CSDDefaultStorage(this);
	_pStorageAddress = new CSDStorageAddress(this);
	_pStorageError = new CSDStorageError(this);

	_pGetStorageNum = new CCCActionGetStorageNum(this);
	_pStateOut[0] = _pStorageNum;
	_pGetStorageNum->SetArguList(0, _pStateIn, 1, _pStateOut);

	
	_pGetStorageState = new CCCActionGetStorageState(this);
	_pStateIn[0] = _pStorageIndex;
	_pStateOut[0] = _pStorageState;
	_pStateOut[1] = _pStorageSpace;
	_pStateOut[2] = _pStorageFreeSpace;
	_pGetStorageState->SetArguList(1, _pStateIn, 3, _pStateOut);
	
	_pGetStorageAddress = new CCCActionGetStorageAddress(this);
	_pStateIn[0] = _pStorageIndex;
	_pStateOut[0] = _pStorageAddress;
	_pGetStorageAddress->SetArguList(1, _pStateIn, 1, _pStateOut);
	
	_pSetStorageAsDefault = new CCCActionSetDefaultStorage(this);
	_pStateIn[0] = _pStorageIndex;
	_pSetStorageAsDefault->SetArguList(1, _pStateIn, 0, _pStateOut);
	
	_pGetStorageDefault = new CCCActionGetDefaultStorage(this);
	_pStateOut[0] = _pStorageIndex;
	_pGetStorageDefault->SetArguList(0, _pStateIn, 1, _pStateOut);
	
}

CUpnpSDStorage::~CUpnpSDStorage()
{
	delete _pStorageState;
	delete _pStorageNum;
	delete _pStorageSpace;
	delete _pStorageFreeSpace;
	delete _pStorageStorageDefault;
	delete _pStorageAddress;
	delete _pStorageError;

	delete _pGetStorageNum;
	delete _pGetStorageState;
	delete _pGetStorageAddress;
	delete _pSetStorageAsDefault;
	delete _pGetStorageDefault;
}	

int CUpnpSDStorage::GetStorageState()
{
	_pStorageState->Update(true);
	return _pStorageState->GetCurrentIndex();
}

int CUpnpSDStorage::GetStorageFreSpc()
{
	char tmp[128];
	memset(tmp, 0, sizeof(tmp));
	agcmd_property_get(SDSpacePropName, tmp ,"0");

	unsigned long long space = atoll(tmp);
	memset(tmp, 0, sizeof(tmp));
	agcmd_property_get(SDFreeSpacePropName, tmp ,"0");
	unsigned long long fs = atoll(tmp);
	
	int per = fs*100/space;
	//printf("---spac: %lld, %lldd, %d,tmp\n",space, fs, per);
	return per;
}

bool CUpnpSDStorage::FormatTF()
{
	bool rt = true;
	SDWatchThread::getInstance()->ToStop();
	_pStorageState->Update(true);
	CSDStorageState::StorageState state = (CSDStorageState::StorageState)_pStorageState->GetCurrentIndex();
	int count = 0;
	const char *tmp_args[8];

	/*while((state != CSDStorageState::StorageState_Ready)&&(count < 5))
	{
		count++;
		sleep(1);
		_pStorageState->Update(true);
		state = (CSDStorageState::StorageState)_pStorageState->GetCurrentIndex();
	}
	count = 0;*/
	if(state == CSDStorageState::StorageState_Ready)
	{
		//unmount

		tmp_args[0] = "agsh";
		tmp_args[1] = "agblock";
		tmp_args[2] = "umount";
		tmp_args[3] = "mmcblk0";
		tmp_args[4] = "mmcblk0p1";
		tmp_args[5] = NULL;
		agbox_sh(5, (const char *const *)tmp_args);
		//system("agsh agblock umount mmcblk0 mmcblk0p1");
		do
		{
			printf("---unmount : %d \n", count);
			sleep(1);
			count ++;
			_pStorageState->Update(true);
			state = (CSDStorageState::StorageState)_pStorageState->GetCurrentIndex();
		}while((state != CSDStorageState::StorageState_usbdisk)&&(count < 5));
		if((state != CSDStorageState::StorageState_usbdisk))
		{
			tmp_args[0] = "agsh";
			tmp_args[1] = "agblock";
			tmp_args[2] = "mount";
			tmp_args[3] = "mmcblk0";
			tmp_args[4] = "mmcblk0p1";
			tmp_args[5] = NULL;
			agbox_sh(5, (const char *const *)tmp_args);
			//system("agsh agblock mount mmcblk0 mmcblk0p1");
			SDWatchThread::getInstance()->ToStart();
			return false;
		}
	}
	
	{
		tmp_args[0] = "agsh";
		tmp_args[1] = "agblock";
		tmp_args[2] = "format";
		tmp_args[3] = "mmcblk0";
		tmp_args[4] = "mmcblk0p1";
		tmp_args[5] = NULL;
		agbox_sh(5, (const char *const *)tmp_args);
		//system("agsh agblock format mmcblk0 mmcblk0p1");

		tmp_args[0] = "agsh";
		tmp_args[1] = "agblock";
		tmp_args[2] = "mount";
		tmp_args[3] = "mmcblk0";
		tmp_args[4] = "mmcblk0p1";
		tmp_args[5] = NULL;
		agbox_sh(5, (const char *const *)tmp_args);
		//system("agsh agblock mount mmcblk0 mmcblk0p1");
		count = 0;
		do
		{
			printf("---in format : %d \n", count);
			sleep(1);
			count ++;
			_pStorageState->Update(true);
			state = (CSDStorageState::StorageState)_pStorageState->GetCurrentIndex();
		}while((state != CSDStorageState::StorageState_Ready)&&(count < 60));

		if((state == CSDStorageState::StorageState_Ready))
		{
			printf("---format ok : %d \n", count);
			rt = true;
		}
		else
		{
			printf("---format error : %d \n", count);
			rt = false;
		}
		SDWatchThread::getInstance()->ToStart();
		return rt;
	}
	
}

