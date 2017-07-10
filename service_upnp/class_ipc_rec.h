/*****************************************************************************
 * class_ipc_rec.h
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
 *
 *****************************************************************************/
#ifndef __H_class_ipc_avf__
#define __H_class_ipc_avf__

#include <unistd.h>
#include "libipc.h"
#include "ClinuxThread.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include "GobalMacro.h"

#ifdef USE_AVF
//#include "avf_camera.h"
#include <stdint.h>
#include "avf_media.h"
#else
#include "ipc_vlc.h"
#endif
class CUpnpSDRecord;


#define big_stream_width 1024
#define big_stream_height 576
#define big_stream_quality 85

#define small_stream_width 512
#define small_stream_height 288
#define small_stream_quality 85

#define Storage_least_space (100*1024*1024)
//#include "avf_camera.h"

#ifdef USE_AVF

class CHdmiWatchThread : public CThread
{
public:
	static void create();
	static void destroy();
	static CHdmiWatchThread* getInstance(){return pInstance;};
protected:
	virtual void main(void *p);
	static void hdmi_hotplug_handler(int num);
	void handler(int num);
	
private:
	CHdmiWatchThread():CThread("hdmi watch Thread", CThread::NORMAL, 0, NULL)
	{

	};
	~CHdmiWatchThread(){};	
	static CHdmiWatchThread* pInstance;
	int _hdmi_proc;
};

class IPC_AVF_Delegate
{
public:
	enum recordState
	{
		recordState_stoped = 0,
		recordState_stopping,
		recordState_starting,
		recordState_recording,
	};

	enum recordStartError
	{
		recordStartError_notf = 0,
		recordStartError_tfFull,
	};
public:
	IPC_AVF_Delegate(){};
	~IPC_AVF_Delegate(){};
	virtual void onNewFileReady(char* path){printf("--->\n");};
	virtual void onCameraModeChange(){};
	virtual void onRecodeStateChange(recordState state){};
	virtual void onStartRecordError(recordStartError error){};
private:
	
};

class IPC_AVF_Client : public CThread
{
public:
	typedef enum AVF_Mode
	{
		AVF_Mode_NULL = 0,
		AVF_Mode_Encode,
		AVF_Mode_Decode,
	}AVF_Mode;

	typedef enum PLAY_STATE
	{
		PLAY_STATE_ready = 0,
		PLAY_STATE_playing,
		PLAY_STATE_finish,
		PLAY_STATE_unknow,
	}PLAY_STATE;

	//create 
	static void create(CUpnpSDRecord* service);
	static IPC_AVF_Client* getObject();
	static void destroy();

	//call back
	static void avf_event_callback( int type, void *pdata );
	void AVFCameraCallback(int type);
	void setDelegate(IPC_AVF_Delegate* dele){_pAVFDelegate = dele;};
	//record control
	void switchStream(bool bBig);
	
	void StartRecording(char* tmp, int len);	
	void RecordKeyProcess();
	void GsensorSignalProcess();
	void StopRecording();
	//void EndStreamOrRecord();	
	UINT64 getRecordingTime();
	int	getRecordState();

	//record property
	int getWidth(){return _recording_width;};
	int getHeight(){return _recording_height;};
	int getFps(){return _recording_frame;};
	//int getBitrate();
	void CheckFreeAutoStop();
	bool isCircleMode();
	bool isAutoStartRec();
	bool isAlertMode();
	bool isRotate(){return _bRotate;};
	void switchToRec(bool checkAuto);
	void onTFReady(bool b);
	void switchToPlay(char* path);
	
	int getPlayInfor
		(int &length
		, int &currentPosi
		, bool &b
		, bool &bStop
		, bool &bError
		, char** file);

	int ClosePlay();
	int PausePlay();
	int Play();

	int	CloseRec();
	void ShowGPSFullInfor(bool b);
	void LCDOnOff(bool b);
	static void AutoStopTimerCallBack(void * p);
	void timerCallBack();
	int GetStorageState()
	{
		return _CheckVDBSpace();
	}
	void SetCameraName(const char* name);
	void SetCameraLocation(char* location);
	void UpdateSubtitleConfig();
	int getGpsState();
	int SyncCaptureNextJpeg(char* path); // should call in a dedicat thread

	AVF_Mode getAvfMode(){return _avfMode;};
	void EnableDisplay();
	void RestartRecord();
	int SetMicVolume(int capvol);
	int DefaultMicVolume();
	virtual void Stop();
#ifdef SupportNewVDBAVF
	void GetVDBState(bool &ready, unsigned int &spaceMB, long long &TotalSpaceMB);
	int GetStorageFreSpcPcnt();
#endif
	int SetWifiMode(int mode, const char* ssid = NULL);
protected:
	enum InternalCommand
	{
		InternalCommand_Null = 0,
		InternalCommand_errorProcess,
		InternalCommand_switchToRec,
		InternalCommand_switchToPlay,
		InternalCommand_closeRec,
		InternalCommand_ClosePlay,
		InternalCommand_record,
		InternalCommand_stopRec,
		InternalCommand_Play,
		InternalCommand_StopPlay,
		InternalCommand_PauseResume,
		InternalCommand_Seek,
		InternalCommand_timerChcek,
		InternalCommand_tfReady,
		InternalCommand_tfRemove,
		InternalCommand_playCurrent,
		InternalCommand_setSubtitleConfig,
		InternalCommand_RestartRec,
		InternalCommand_fileBufferWarning,
		InternalCommand_fileBufferError,
		InternalCommand_VdbStateChanged,
		InternalCommand_num,
	};
	void main(void *);
	
	
private:
	static IPC_AVF_Client* pInstance;
	IPC_AVF_Client(CUpnpSDRecord* service);
	~IPC_AVF_Client();

	// internal rec config
	//void AVFReleaseMedia();
	void _switchToRec(bool forceStartRec);
	void _tfReady();
	void _tfRemove();
	void _vdbStateChanged();
	void _startAVFRec();
	void _startAVFStream();
	void _ReadConfigs();
	void _Update_Recording_Config();
	void _Update_Streaming_Config();
	void _Update_aaa_Config();
	int _CloseRec();

	// internal mode switch	
	void _PrepareRecord();
	void _rePrepareRecord();//if vin changed
	void _PrepareDecode(char* path);

	// internal play commands
	void _switchToPlay();
	void _AVFPlay();
	void _AVFPause();
	int _ClosePlay();	

	int _config_vout(bool record);	
	int _config_lcd_vout(bool bON);
	int _config_hdmi_vout(bool on, bool fbON, bool bsmall);

	void _onAvf_Error_process();
	int _CheckVDBSpace(); //0. not ready, 1. full, >1  ok
	void _timerCheck();

	void _playCurrentFile();
	void _PrepareVinSetting(char* tmp, int length);
	void _updateSubtitleConfig();

	bool _tryReleaseSpace();
	
private:	
	time_t 	_recordingStartTime;
	
	CSemaphore	*_pRecordStopWait;
	CSemaphore	*_pPlayingWait;
	CSemaphore	*_pOnCommand;
	CMutex		*_pMutex;

	avf_media_t* 		_p_avf_media;	
	int 				_avf_camera_state;
	int			_currentRecOption;
	bool		_restartRecordPara;


	int		_recording_width;
	int		_recording_height;
	int		_recording_frame;
	int		_recording_bitrate;
	int		_seg_iframe_num;
	int		_record_color_mode;
	int		_record_state;
	bool	_bRotate;
	bool	_bChangeVin;


	CUpnpSDRecord* 		_pUpnp_rec_service;
	bool 				_bBigStream;

	AVF_Mode			_avfMode;

	static bool			_bExecuting;
	InternalCommand		_command; //third
	InternalCommand		_command0; // firspriority
	InternalCommand		_command1; // second

	char				_currentMovie[256];
	bool				_hdmiOn;

	int 				_checkTimer;
	bool 				_bWaitTFReady;

	PLAY_STATE			_play_state;
	char				_shortName[30];

	IPC_AVF_Delegate* _pAVFDelegate;
	CSemaphore			*_pCaptureJpegWait;

	bool				_bCheckAutoRec;

	int					_bHDROn;
	bool 				_lcdOn;
};

#else
#if 0
class IPC_VLC_Object : public CThread
{
public:
	static void create(CUpnpSDRecord* service);
	static IPC_VLC_Object* getObject();
	static void destroy();
	static void vlc_event_callback( int type, void *pdata );
	
	void StartRecording();
	void switchStream(bool bBig);
	void RecordKeyProcess();
	
	void StopRecording();
	void EndStreamOrRecord();
	void VLCCallBack(int type);
	//void GetRecrodState(CSDRSRecordState* pState);
	UINT64 getRecordingTime();

	int getWidth(){return _recording_width;};
	int getHeight(){return _recording_height;};
	int getFps(){return _recording_frame;};
	int getBitrate(){return _recording_bitrate;};
	
protected:
	enum InternalCommand
	{
		InternalCommand_Null = 0,
		InternalCommand_record,
		InternalCommand_stopRec,
		InternalCommand_num,
	};
	void main(void *);
	
	
private:
	IPC_VLC_Object(CUpnpSDRecord * callbackObj);
	~IPC_VLC_Object();
	void IPC_VLC_release_medias();

	void Update_Recording_Config();
	void Update_Streaming_Config();
	void startVlcRec();
	void startVlcStream();

	void ReadConfigs();
	
	time_t 	_recordingStartTime;
	
	CSemaphore	*_pRecordStopWait;
	CSemaphore	*_pPlayingWait;
	CSemaphore	*_pOnCommand;
	CMutex		*_pMutex;

	CSemaphore	*_pVLCWait;

	bool				_bExecuting;
	InternalCommand		_command;
	CUpnpSDRecord		*_pUpnp_rec_service;
		
	static IPC_VLC_Object* pInstance;
	ipc_libvlc_instance_t*	_p_vlc_instance;
	const char*				_currentRecOption;

	void* 				_pCallBackObject;

	int _recording_width;
	int _recording_height;
	int _recording_frame;
	int _recording_bitrate;
	int _seg_iframe_num;
	int _record_color_mode;
	int _record_state;

	
};
#endif
#endif

#endif


//for ui

/*
class IPC_AVF_Delegate
{
public:
	IPC_AVF_Delegate(){};
	~IPC_AVF_Delegate(){};
	virtual void onRecStateChanged(avf_State state){};
};*/