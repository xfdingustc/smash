
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
CloudCameraCMDProcessor::CloudCameraCMDProcessor(BroadCaster* pBroadCaster)
    : _ppDomainList(NULL)
    , _listLength(0)
    , _addedNum(0)
    , _pResponder(NULL)
{
    _listLength = CMD_Domain_num;
    _addedNum = 0;
    _ppDomainList = new SCMD_Domain*[_listLength];
    for (int i = 0; i < _listLength; i++)
    {
        _ppDomainList[i] = NULL;
    }

    _ppDomainList[CMD_Domain_cam] = SCMD_DOMAIN_NEW(DomainCamera);
    _ppDomainList[CMD_Domain_rec] = SCMD_DOMAIN_NEW(DomainRec);
    for (int i = 0; i < _listLength; i++)
    {
        if (_ppDomainList[i] != NULL) {
            _ppDomainList[i]->SetBroadCaster(pBroadCaster);
        }
    }
}

CloudCameraCMDProcessor::~CloudCameraCMDProcessor()
{
    for (int i = 0; i < _listLength; i++)
    {
        if (_ppDomainList[i]) {
            delete _ppDomainList[i];
        }
    }
    delete[] _ppDomainList;
}

void CloudCameraCMDProcessor::OnRcvData(char* data, int len, SessionResponseSendBack* sendback)
{
    if (!data || (len <= 0)) {
        return;
    }

    //CAMERALOG("-- process data: %s", data);
    StringEnvelope strEV(data, len);
    if (strEV.isNotEmpty()) {
        //CAMERALOG("-- process : %s", data);
        this->ProecessEnvelope(&strEV, sendback);
    }
}

void CloudCameraCMDProcessor::OnDisconnect(SessionResponseSendBack* sendback)
{
    char removeConnect[16] = "";
    snprintf(removeConnect, 16, "ECMD0.%d", CMD_CAM_CLOSE_CONNECTION);
    StringCMD cmd(removeConnect);
    cmd.setSendBack(sendback);
    _lock->Take();
    // Synchronously notify disconnect, then remove all commands from this client.
    inProcessCmd(&cmd);
    removeCmdsInQfor(sendback);
    _lock->Give();
}

void CloudCameraCMDProcessor::OnConnect(SessionResponseSendBack* sendback)
{
    char addConnect[256] = "";
    snprintf(addConnect, 256, "<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>"
        "<ccev><cmd act=\"ECMD0.%d\" p1=\"\" p2=\"\" /></ccev>", CMD_CAM_NEW_CONNECTION);
    OnRcvData(addConnect, strlen(addConnect), sendback);
}


int CloudCameraCMDProcessor::inProcessCmd(StringCMD* cmd)
{
    if (!cmd) {
        return 0;
    }

    int rt = 0;
    EnumedStringCMD* ecmd = (EnumedStringCMD*)cmd;
    int domain = ecmd->getDomain();
    int cmdindex = ecmd->getCMD();
    //CAMERALOG("---CloudCameraCMDProcessor::inProcessCmd : %d, %d", domain, cmdindex);

    if ((domain >= 0) && (domain < CMD_Domain_num)
        && (cmdindex >= 0))
    {
        if (_ppDomainList[domain]) {
            EnumedStringCMD* executer = _ppDomainList[domain]->searchCMD(cmdindex);
            //CAMERALOG("---CloudCameraCMDProcessor::inProcessCmd : %s", executer->getName());
            if (executer != NULL) {
#if 1
                struct timeval time_before;
                gettimeofday(&time_before, NULL);
#endif
                rt = executer->execute(ecmd);
#if 1
                struct timeval time_after;
                gettimeofday(&time_after, NULL);
                float time_consumed_us = (time_after.tv_sec - time_before.tv_sec) * 1E6 +
                    (time_after.tv_usec - time_before.tv_usec);
                //CAMERALOG("############# execute command(domain %d, index %d) cost %.2fms",
                //    domain, cmdindex, time_consumed_us / 1E3);
                if (time_consumed_us > 50 * 1E3) {
                    CAMERALOG("#### execute command(domain %d, index %d) cost %.2fms, "
                        "more than 50ms!!! ####",
                        domain, cmdindex, time_consumed_us / 1E3);
                }
#endif
                return rt;
            }
        }
    } else {
        CAMERALOG("Invalid command domain = %d, cmdindex = %d", domain, cmdindex);
    }

    return rt;
}
