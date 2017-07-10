
/*****************************************************************************
 * ccam_cmd_processor.cpp
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2013, linnsong.
 *
 *
 *****************************************************************************/
#include "ccam_cmd_processor.h" 
#include "ccam_cmds.h"

#include "ccam_domain_camera.h"
#include "ccam_domain_record.h"

/***************************************************************
CloudCameraCMDProcessor::CloudCameraCMDProcessor()
*/
#ifdef isServer
CloudCameraCMDProcessor::CloudCameraCMDProcessor(BroadCaster* pBroadCaster)
{
	_listLength = CMD_Domain_num;
	_addedNum = 0;
	_ppDomainList = new SCMD_Domain*[_listLength];
	for(int i = 0; i < _listLength; i++)
	{
		_ppDomainList[i] = NULL;
	}

	_ppDomainList[CMD_Domain_cam] = SCMD_DOMAIN_NEW(DomainCamera);
	_ppDomainList[CMD_Domain_rec] = SCMD_DOMAIN_NEW(DomainRec);
	for(int i = 0; i < _listLength; i++)
	{
		if(_ppDomainList[i] != NULL)
			_ppDomainList[i]->SetBroadCaster(pBroadCaster);
	}
}
#else
CloudCameraCMDProcessor::CloudCameraCMDProcessor
    (CameraResponder* pResponder
     ):_pResponder(pResponder)
{
	_listLength = CMD_Domain_num;
	_addedNum = 0;
	_ppDomainList = new SCMD_Domain*[_listLength];
	for(int i = 0; i < _listLength; i++)
	{
		_ppDomainList[i] = NULL;
	}
    
	_ppDomainList[CMD_Domain_cam] = SCMD_DOMAIN_NEW(DomainCamera);
    _ppDomainList[CMD_Domain_rec] = SCMD_DOMAIN_NEW(DomainRec);
	for(int i = 0; i < _listLength; i++)
	{
		if(_ppDomainList[i] != NULL)
			_ppDomainList[i]->SetCameraResponder(pResponder);
	}
}
#endif

CloudCameraCMDProcessor::~CloudCameraCMDProcessor()
{
	for(int i = 0; i < _listLength; i++)
	{
		if(_ppDomainList[i])
			delete _ppDomainList[i];
	}
	delete[] _ppDomainList;
}

void CloudCameraCMDProcessor::OnRcvData(char* data, int len, SessionResponseSendBack* sendback)
{
    //printf("-- process data: %s\n", data);
	StringEnvelope strEV(data, len);
	if(strEV.isNotEmpty())
	{
        //printf("-- process : %s\n", data);
		this->ProecessEvelope(&strEV, sendback);
	}
}

void CloudCameraCMDProcessor::OnDisconnect()
{
#ifndef isServer
	_pResponder->OnDisconnect();
#endif
}

void CloudCameraCMDProcessor::onSessionBreak(SessionResponseSendBack* sendback)
{
	_lock->Take();
	removeCmdsInQfor(sendback);
	_lock->Give();
}

void CloudCameraCMDProcessor::OnConnect()
{
#ifndef isServer
    _pResponder->OnConnect();
#endif
}


int CloudCameraCMDProcessor::inProcessCmd(StringCMD* cmd)
{
	int rt = 0;
	EnumedStringCMD* ecmd = (EnumedStringCMD*)cmd;
#ifdef isServer
	int domain = ecmd->getDomain();
	int cmdindex = ecmd->getCMD();
	//printf("---CloudCameraCMDProcessor::inProcessCmd : %d, %d\n", domain, cmdindex);

	if((domain >= 0) && (cmdindex >= 0))
	{
		if(_ppDomainList[domain])
		{
			EnumedStringCMD* executer = _ppDomainList[domain]->searchCMD(cmdindex);
			//printf("---CloudCameraCMDProcessor::inProcessCmd : %s\n", executer->getName());
			if(executer != NULL) {
				rt = executer->execute(ecmd);
				return rt;
			}
		}
	}
	char cmddomain[16];
	sprintf(cmddomain, "%d", ecmd->getDomain());
	char scmdindex[16];
	sprintf(scmdindex, "%d", ecmd->getCMD());
	EnumedStringCMD uscmd(CMD_Domain_cam, CMD_Cam_isAPISupported, cmddomain, scmdindex);
	StringCMD* usp = &uscmd;
	StringEnvelope ev(&usp, 1);
	EnumedStringCMD* executer = _ppDomainList[CMD_Domain_cam]->searchCMD(CMD_Cam_isAPISupported);
	if(executer != NULL) {
		rt = executer->execute(ecmd);
		return rt;
	}
#else
    rt = _pResponder->OnCmd(ecmd);
#endif
    return rt;
}
