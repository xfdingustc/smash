/*****************************************************************************
 * class_sd_service_decode.h
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
#ifndef __H_class_upnp_sd_svc_dec__
#define __H_class_upnp_sd_svc_dec__

#include "class_upnp_description.h"


/***************************************************************
CSDHttpReviewAddress
*/
class CSDHttpAddress : public CUpnpStringFreeState
{
	typedef CUpnpStringFreeState inherited;
public:
	CSDHttpAddress(CUpnpService* pService);
	~CSDHttpAddress();
private:
};

/***************************************************************
CCCActionGetReviewAddress
*/
class CCCActionGetReviewAddress : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CCCActionGetReviewAddress(CUpnpService* pService);
	~CCCActionGetReviewAddress();
	virtual void execution();
private:	
};


/***************************************************************
CUpnpSDNetwork
*/
class CUpnpSDDecode : public CUpnpService
{
	typedef CUpnpService inherited;
public:
	CUpnpSDDecode(CUpnpDevice*);
	~CUpnpSDDecode();	

	int StartPlay(char* shortName);
	int StopPlay();
	int Play();
	int Pause();
	int PlayInfor
		(int &length
		, int &currentPosi
		, bool& bPause
		, bool &bStop
		, bool &bError
		,char** file);
	
private:
	CSDHttpAddress 				*_pHttpAddress;

	CCCActionGetReviewAddress		*_pGetReviewAddress;
	
	CUpnpState*			_pStateIn[5];
	CUpnpState*			_pStateOut[5];


};

#endif
