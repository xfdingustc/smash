
/*****************************************************************************
 * ccam_cmd_processor.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2013, linnsong.
 *
 *
 *****************************************************************************/


#ifndef __H_ccam_cmd_processor__
#define __H_ccam_cmd_processor__

#include "ClinuxThread.h"
#include "sd_general_description.h"
#include "class_TCPService.h"
#include "ccam_cmds.h"


class CCAM_CameraDomain : public SCMD_Domain
{
public:
    typedef SCMD_Domain inherited;
    CCAM_CameraDomain
        ():inherited(CMD_Domain_cam, CMD_Cam_num)
    {
    }
    ~CCAM_CameraDomain() {}
private:

};

class CloudCameraCMDProcessor : public SessionRcvDelegate, public StringCMDProcessor1
{
public:
    CloudCameraCMDProcessor(BroadCaster* pBroadCaster);
    ~CloudCameraCMDProcessor();

protected:
    virtual int inProcessCmd(StringCMD* cmd);
    virtual void OnRcvData(char* data, int len, SessionResponseSendBack* sendback);
    virtual void OnDisconnect(SessionResponseSendBack* sendback);
    virtual void OnConnect(SessionResponseSendBack* sendback);
private:
    SCMD_Domain      **_ppDomainList;
    int              _listLength;
    int              _addedNum;
    CameraResponder  *_pResponder;
};

#endif
