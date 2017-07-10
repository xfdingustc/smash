/*****************************************************************************
 * class_storage.h
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
#ifndef __H_class_sd_svc_storage__
#define __H_class_sd_svc_storage__
#include "class_upnp_description.h"


/***************************************************************
CSDStorageState
*/
class CSDStorageState : public CUpnpStringLimitedState
{
	typedef CUpnpStringLimitedState inherited;
public:
	typedef enum StorageState
	{
		StorageState_Ready = 0,
		StorageState_loading,
		StorageState_error,
		StorageState_offline,
		StorageState_usbdisk,
		StorageState_Num
	}StorageState;

	CSDStorageState(CUpnpService* pService);
	~CSDStorageState();
	virtual void Update(bool bSendEvent);
private:
	const char*	_pAllowedValue[StorageState_Num];
};

/***************************************************************
CSDStorageNumber
*/
class CSDStorageNumber : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDStorageNumber(CUpnpService* pService);
	~CSDStorageNumber();
private:
};

/***************************************************************
CSDStorageIndex
*/
class CSDStorageIndex : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDStorageIndex(CUpnpService* pService);
	~CSDStorageIndex();
private:
};

/***************************************************************
CSDStorageNumber
*/
class CSDStorageSpace : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDStorageSpace(CUpnpService* pService);
	~CSDStorageSpace();
private:
};

/***************************************************************
CSDStorageNumber
*/
class CSDStorageFreeSpace : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDStorageFreeSpace(CUpnpService* pService);
	~CSDStorageFreeSpace();
private:
};

/***************************************************************
CSDStorageNumber
*/
class CSDDefaultStorage : public CUpnpNumberState
{
	typedef CUpnpNumberState inherited;
public:
	CSDDefaultStorage(CUpnpService* pService);
	~CSDDefaultStorage();
private:
};

/***************************************************************
CSDStorageAddress
*/
class CSDStorageAddress : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDStorageAddress(CUpnpService* pService);
	~CSDStorageAddress();
private:
};



/***************************************************************
CSDStorageError
*/
class CSDStorageError : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDStorageError(CUpnpService* pService);
	~CSDStorageError();
private:

};

/***************************************************************
CCCActionGetStorageNum
*/
class CCCActionGetStorageNum : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetStorageNum(CUpnpService* pService);
	~CCCActionGetStorageNum();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetStorageState
*/
class CCCActionGetStorageState : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetStorageState(CUpnpService* pService);
	~CCCActionGetStorageState();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetStorageAddress
*/
class CCCActionGetStorageAddress : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetStorageAddress(CUpnpService* pService);
	~CCCActionGetStorageAddress();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionSetDefaultStorage
*/
class CCCActionSetDefaultStorage : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionSetDefaultStorage(CUpnpService* pService);
	~CCCActionSetDefaultStorage();
	virtual void execution();
private:	
};

/***************************************************************
CCCActionGetDefaultStorage
*/
class CCCActionGetDefaultStorage : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetDefaultStorage(CUpnpService* pService);
	~CCCActionGetDefaultStorage();
	virtual void execution();
private:	
};


/***************************************************************
CUpnpSDStorage
*/
class CUpnpSDStorage : public CUpnpService
{
	typedef CUpnpService inherited;
public:
	CUpnpSDStorage(CUpnpDevice*);
	~CUpnpSDStorage();	

	int GetStorageState();
	bool FormatTF();
	int GetStorageFreSpc();
	
private:
	CSDStorageState				*_pStorageState;
	CSDStorageNumber 			*_pStorageNum;
	CSDStorageIndex				*_pStorageIndex;
	CSDStorageSpace 				*_pStorageSpace;
	CSDStorageFreeSpace 			*_pStorageFreeSpace;
	CSDDefaultStorage				*_pStorageStorageDefault;
	CSDStorageAddress 			*_pStorageAddress;
	CSDStorageError 				*_pStorageError;

	CCCActionGetStorageNum		*_pGetStorageNum;
	CCCActionGetStorageState		*_pGetStorageState;
	CCCActionGetStorageAddress 	*_pGetStorageAddress;
	CCCActionSetDefaultStorage 		*_pSetStorageAsDefault;
	CCCActionGetDefaultStorage		*_pGetStorageDefault;
	
	CUpnpState*			_pStateIn[5];
	CUpnpState*			_pStateOut[5];

};

#endif
