/*****************************************************************************
 * upnp_decode.cpp:
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
#include "class_sd_service_decode.h"
#include "class_ipc_rec.h"

/***************************************************************
CSDNetmask
*/
CSDHttpAddress::CSDHttpAddress
	(CUpnpService* pService
	): inherited("httpAddress", false, false, pService)
{

}

CSDHttpAddress::~CSDHttpAddress()
{

}

/***************************************************************
CCCActionGetAddress
*/

CCCActionGetReviewAddress::CCCActionGetReviewAddress
	(CUpnpService* pService
	) : inherited("GetReviewAddress", pService)	
{
 	//printf("--CCCActionGetAddress: %x ", this);
}

CCCActionGetReviewAddress::~CCCActionGetReviewAddress()
{

}

void CCCActionGetReviewAddress::execution()
{
	//printf("get address called\n");
	char tmp[256];
	memset(tmp, 0, 256);
	sprintf(tmp,"http://%s", UpnpGetServerIpAddress());
	if(_ppOutStates != NULL)
	{
		CSDHttpAddress* pAddressState = (CSDHttpAddress*)_ppOutStates[0];
		pAddressState->SetCurrentState(tmp);
	}
	else
	{
		printf("argument for get address not inited\n");
	}
	//pAddressState->SetCurrentState(tmp);	*/
}


/***************************************************************
CUpnpSDDecode
<serviceType>urn:schemas-upnp-org:service:SDCCDecode:1</serviceType>
				<serviceId>urn:upnp-org:serviceId:SDCCDecode</serviceId>
				<SCPDURL>/description/sd_decode.xml</SCPDURL>
				<controlURL>/upnp/sddeccontrol</controlURL>
				<eventSubURL>/upnp/sddecevent</eventSubURL>
*/
CUpnpSDDecode::CUpnpSDDecode
	(CUpnpDevice* pDevice
	):inherited("decoder"
			,"urn:schemas-upnp-org:service:SDCCDecode:1"
			,"urn:upnp-org:serviceId:SDCCDecode"
			,"/description/sd_decode.xml"
			,"/upnp/sddeccontrol"
			,"/upnp/sddecevent"
			,pDevice
			,5
			,5)
{
	_pHttpAddress = new CSDHttpAddress(this);

	_pGetReviewAddress = new CCCActionGetReviewAddress(this);
	_pStateOut[0] = _pHttpAddress;
	_pGetReviewAddress->SetArguList(0, _pStateIn, 1, _pStateOut);
}	


CUpnpSDDecode::~CUpnpSDDecode()
{
	delete _pHttpAddress;
	
	delete _pGetReviewAddress;	
}


int CUpnpSDDecode::StartPlay(char* shortName)
{
	IPC_AVF_Client::getObject()->switchToPlay(shortName);
	return 0;
}

int CUpnpSDDecode::StopPlay()
{
	IPC_AVF_Client::getObject()->ClosePlay();
	return 0;
}

int CUpnpSDDecode::Play()
{
	IPC_AVF_Client::getObject()->Play();
	return 0;
}
int CUpnpSDDecode::Pause()
{
	IPC_AVF_Client::getObject()->PausePlay();
	return 0;
}

int CUpnpSDDecode::PlayInfor
	(int &length
	, int &currentPosi
	, bool &bPause
	, bool &bStop
	, bool &bError
	, char **file)
{
	IPC_AVF_Client::getObject()->getPlayInfor(length, currentPosi, bPause, bStop, bError, file);
	return 0;
}