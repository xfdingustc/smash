
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
	CCAM_mirrorDelegate(){};
	virtual ~CCAM_mirrorDelegate(){};
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
#ifdef isServer
: public BroadCaster
#else
: public CameraResponder
#endif
{
public:
	CCAM_mirror(char* address, int16_t port);
	~CCAM_mirror();
	void setDelegate(CCAM_mirrorDelegate* dele){_delegate = dele;};
#ifdef isServer
	virtual void BroadCast(StringEnvelope* ev);
#else
    virtual int OnCmd(EnumedStringCMD* cmd);
    virtual int OnConnect();
    virtual int OnDisconnect();
    //virtual void OnState(int stateName, int StateValue);
	bool Connect();
    void Disconnect();
    void SendCmdTest();
    void getCameraMode();
    void getCameraState();
    void StartRecord();
    void StopRecord();
    void RecGetTimeInfo();
    void GetCameraName();
    void SetCameraName(const char*name);
    void setStream(bool bBig);
    void getAllInfor();
    
    void getResolutionList();
    void getCurrentResolution();
    void setResolution(int resolution);
    
    void getQualityList();
    void getCurrentQuality();
    void setQuality(int resolution);
    
    void getRecModeList();
    void getCurrentRecMode();
    void setRecMode(int resolution);
    
    void getColorModeList();
    void getColorMode();
    void setColorMode(int resolution);
    
    void PowerOff();
    void Reboot();
    
    void GetHostNumber();
    void GetHostInfor(int index);
    void addHost(const char* name, const char* pwd);
    void rmvHost(const char* name);

	void SyncTime(unsigned int time, unsigned int timezone);
	
	void setMic(bool bMute, int gain);
	void getMicState();

	void SetOverlay(bool bNameOn, bool bTimeOn, bool bGPSOn, bool bSpeed);
	void GetOverlayState();
    
#endif
    
private:
	CCAM_mirrorDelegate* _delegate;
    char _address[40];
    unsigned short _port;
    
#ifdef isServer
	TCPSessionService		*_ccamService;
#else
    TCPSessionClient*   _pTCPSessionClient;
#endif
    CloudCameraCMDProcessor* _processor;
};

#endif
