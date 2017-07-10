
/*****************************************************************************
 * ccam_mirror.h
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
 *
 *****************************************************************************/
#ifndef __H_ccam_mirror__
#define __H_ccam_mirror__
#include "ccam_cmds.h"
#include "sd_general_description.h"
#include "class_TCPService.h"
#include "ccam_cmd_processor.h"

class CCAM_mirrorDelegate
{
public:
    CCAM_mirrorDelegate() {}
    virtual ~CCAM_mirrorDelegate() {}
    virtual void onStateChange(int domain) = 0;
    virtual void onCmdReturn(int domain) = 0;
    virtual void onDisconnect() = 0;
    virtual void onConnect() = 0;
    virtual void OnGetCameraName(char* name) = 0;

    virtual void OnModeSwitch(int mode) = 0;
    virtual void OnRecordTimeInfor(unsigned int Seconds) = 0;
    virtual void OnRecordState(int state) = 0;
    virtual void OnStorageState(int stage) = 0;
    virtual void OnSpaceUpdate(unsigned long long whole, unsigned long long free) = 0;
    virtual void OnBatteryVolume(int mV, int percentage) = 0;
    //virtual void OnAudioState(int state, int volume) = 0;
    virtual void OnBTSate(int state, char* name) = 0;
    virtual void OnGPSSate(int state, char* posi) = 0;
    virtual void OnInternetServerState(int state) = 0;
    virtual void OnNameUpdate(char *name) = 0;
    virtual void OnMicState(int state, int gain) = 0;


    virtual void OnSupportedResolutionList(unsigned long long supported) = 0;
    virtual void OnSupportedQualityList(unsigned long long supported) = 0;
    virtual void OnSupportedColorModeList(unsigned long long supported) = 0;
    virtual void OnSupportedRecModeList(unsigned long long supported) = 0;

    virtual void OnCurrentResolution(int current) = 0;
    virtual void OnCurrentQuality(int current) = 0;
    virtual void OnCurrentColorMod(int current) = 0;
    virtual void OnCurrentRecMode(int current) = 0;

    virtual void OnOverlayInfor(bool bName, bool bTime, bool bPosi, bool bSpeed);

    virtual void OnHostNumber(int num) = 0;
    virtual void OnHostInfor(char*name) = 0;

private:

};

class CCAM_mirror
    : public BroadCaster
{
public:
    CCAM_mirror(char* address, int16_t port);
    virtual ~CCAM_mirror();
    void setDelegate(CCAM_mirrorDelegate* dele) { _delegate = dele; }
    virtual void BroadCast(StringEnvelope* ev);

private:
    CCAM_mirrorDelegate*      _delegate;
    char                      _address[40];
    unsigned short            _port;

    TCPSessionService*        _ccamService;
    CloudCameraCMDProcessor*  _processor;
};

#endif
