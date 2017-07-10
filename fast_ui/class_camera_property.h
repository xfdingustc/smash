
/*****************************************************************************
 * class_camera_property.h
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
 * 
 *****************************************************************************/
#ifndef __H_class_camera_property__
#define __H_class_camera_property__

typedef enum camera_mode
{
	camera_mode_car = 0,
	camera_mode_normal,
	camera_mode_Num,
}camera_mode;

typedef enum camera_State
{
	camera_State_stop = 0,
	camera_State_record,
	camera_State_marking,
	camera_State_play,
	camera_State_Pause,
	camera_State_Error,
	camera_State_Num,
}camera_State;

typedef enum wifi_mode
{
	wifi_mode_AP = 0,
	wifi_mode_Client,
	wifi_mode_Off,
}wifi_mode;

typedef enum wifi_State
{
	wifi_State_ready = 0,
	wifi_State_error,
	wifi_State_prepare,
}wifi_State;

typedef enum storage_State
{
	storage_State_ready = 0,
	storage_State_error,
	storage_State_prepare,
	storage_State_noMedia,
	storage_State_full,
}storage_State;

typedef enum audio_State
{
	audio_State_on = 0,
	audio_State_off,
}audio_State;

typedef enum hdr_State
{
	hdr_State_on = 0,
	hdr_State_off,
}hdr_State;

typedef enum rotate_State
{
	rotate_State_0 = 0,
	rotate_State_180,
	rotate_State_mirror_0,
	rotate_State_mirror_180,
	rotate_State_hdr_off,
	rotate_State_hdr_on,
}rotate_State;

typedef enum gps_state
{
	gps_state_on = 0,
	gps_state_ready,
	gps_state_off,
}gps_state;

typedef enum gsensor_sensitive
{
	gsensor_sensitive_off = 0,
	gsensor_sensitive_low,
	gsensor_sensitive_middle,
	gsensor_sensitive_high,
}gsensor_sensitive;

typedef enum PropertyChangeType
{
	PropertyChangeType_unknow = 0,
	PropertyChangeType_scanOk,
	PropertyChangeType_scanFail,
	PropertyChangeType_FileWriteWarnning,
	PropertyChangeType_FileWriteError,
}PropertyChangeType;

class PropertiesObserver
{
public:
	PropertiesObserver(){};
	virtual ~PropertiesObserver(){};
	virtual void OnPropertieChange(void* p, PropertyChangeType type) = 0;
	//virtual void OnInternalError(void* p, PropertyChangeType type) = 0;
};

class CCameraProperties
{
public:
	enum MovieDirType
	{
		MovieDirType_Circle = 0,
		MovieDirType_Marked,
		MovieDirType_Internal,
		MovieDirType_num,
	};

	CCameraProperties(){_ppObeservers = 0;};
	~CCameraProperties(){};

	virtual camera_State getRecState() = 0;
	virtual UINT64 getRecordingTime()= 0;
	virtual storage_State getStorageState() = 0;
	virtual int getStorageFreSpcByPrcn() = 0;
	virtual bool FormatStorage() = 0;
	virtual wifi_mode getWifiMode() = 0;
	virtual wifi_State getWifiState() = 0;
	virtual audio_State getAudioState() = 0;
	virtual rotate_State getRotationState() = 0;
	virtual gps_state getGPSState() = 0;
	virtual UINT64 getPlayingTime() = 0;
	virtual camera_mode getRecMode() = 0;
	virtual void setRecMode(camera_mode m) = 0;
	virtual void setRecSegLength(int seconds) = 0;
	virtual int getMovieDirNum() = 0;
	virtual void getMovieDirList(char** _ppList, int strLen, int strnum) = 0;
	virtual int getGroupNum(char* name) = 0;
	virtual int getCurrentDirType(char* dir) = 0;
	virtual void getDirGroupList(char* dir, char** _ppList, int strLen, int strnum) = 0;
	virtual int getFileNum(char* currentDir, int currentGroup) = 0;
	virtual void getFileList(char* currentDir, int currentGroup, char** _ppList, int strLen, int strnum) = 0;

	virtual int startRecMode() = 0;
	virtual int endRecMode() = 0;
#ifdef QRScanEnable
	virtual int startScanMode() = 0;
	virtual char* GetScanResult() = 0;
	virtual int stopScanMode() = 0;
#endif
	virtual int startPlay(char* shortName) = 0;
	virtual int removeMovie(char* shortName) = 0;
	virtual int markMovie(char* shortName) = 0;
	virtual int unmarkMovie(char* shortName) = 0;
	virtual int cpyToInternal(char* shortName) = 0;
	virtual UINT64 MovieSize(char* shortName) = 0;
	virtual UINT64 InternalStorageFreeSize() = 0;
	virtual bool InternalHasFile(char* shortName) = 0;
	
	virtual int StopPlay() = 0;
	virtual void Play() = 0;
	virtual void Pause() = 0;
	virtual void PlayInfor(int &length, int &currentPosi, camera_State& s, char** file) = 0;
	virtual int PlayNext() = 0;
	virtual int PlayPre() = 0;
	virtual int Step(bool forward, int mseconds) = 0;
	virtual void OnSystemTimeChange() = 0;
	virtual void ShowGPSFullInfor(bool b) = 0;
	virtual void LCDOnOff(bool b) = 0;
	//virtual camera_State getPlayState() = 0;
	virtual void OnRecKeyProcess() = 0;
	virtual void OnSensorSwitchKey() = 0;
	virtual void SetGSensorOutputBuf(char* buf, int len, int lineNum) = 0;
	virtual void UpdateSubtitleConfig() = 0;
	
	void SetObserver(PropertiesObserver* p){_ppObeservers = p;};

	virtual void SetGSensorSensitivity(int s) = 0;
	virtual void SetGSensorSensitivity(int x, int y, int z) = 0;
	virtual int GetGSensorSensitivity(char* buf, int len) = 0;
	virtual int GetGSensorSensitivity(int& x, int& y, int& z) = 0;
	virtual void EnableDisplayManually() = 0;
	virtual void SetMicVolume(int volume) = 0;
	virtual int GetBatteryVolume(bool& bCharge) = 0;

	virtual int SetDisplayBrightness(int vaule)=0;
	virtual int GetBrightness() = 0;

	virtual int GetWifiAPinfor(char* pName, char* pPwd) = 0;
	virtual int GetWifiCltinfor(char* pName) = 0;
	virtual int SetWifiMode(int m) = 0;

	virtual int GetSystemLanugage(char* language) = 0;
	virtual int SetSystemLanguage(char* language) = 0;
protected:
	PropertiesObserver* _ppObeservers;

	//char* _currentDir;
	
};

#endif
