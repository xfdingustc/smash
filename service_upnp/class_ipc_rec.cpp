/*****************************************************************************
 * class_ipc_rec.cpp
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
 *
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <basetypes.h>
#include <ambas_iav.h>
#include <ambas_vout.h>
#include <ambas_event.h>

//#include "class_ipc_rec.h"
#include "class_sd_service_rec.h"
#include <linux/input.h>
#include "class_mail_thread.h"
#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"

#define media_recording	0 //"recordWithStream"
#define media_streaming	1 //"streamOnly"
#define media_NULL -1

#ifdef USE_AVF
static const char* AVF_Check_Timer_Name = "check free space timer";

typedef enum CameraHardWareVersion
{
	CameraHardWareVersion_agb_castle = 0,
	CameraHardWareVersion_hd22a,
	CameraHardWareVersion_hd22d,
	CameraHardWareVersion_hd23a,
}CameraHardWareVersion;
extern bool withLCD()
{
	struct agsys_platform_info_s agboard_helper_platform_info;
	agsys_platform_get_info(&agboard_helper_platform_info);
	switch (agboard_helper_platform_info.ambarella_info.board_rev) {
	case 70:	//Fog + 8940
	case 326:	//Fog + 8974
	case 65:	//Castle
		return FALSE;
	case 84:	//TSF_HD22A + 8940
	case 340:	//TSF_HD22A + 8974
	case 68:	//TSF_HD22D
		return TRUE;
	case 66:	//TSF_HD23A
		return FALSE;
	default:
		return FALSE;
		break;
	}
};

extern bool withInternalStorage()
{
	struct agsys_platform_info_s agboard_helper_platform_info;
	agsys_platform_get_info(&agboard_helper_platform_info);
	switch (agboard_helper_platform_info.ambarella_info.board_rev) {
	case 70:	//Fog + 8940
	case 326:	//Fog + 8974
	case 65:	//Castle
	case 84:	//TSF_HD22A + 8940
	case 340:	//TSF_HD22A + 8974
		return FALSE;
	case 68:	//TSF_HD22D
		return TRUE;
	case 66:	//TSF_HD23A
		return FALSE;
	default:
		return FALSE;
		break;
	}
};

UINT64 GetCurrentTick()
{	
	struct timespec ts;	
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000;
}

/***************************************************************
static agboard_switch_ar0331_module
*/
static int agboard_switch_ar0331_module(int hdr_mode)
{
	int ret_val;
	char *ar0331_mod_args[16];
	char hdr_mode_str[16];

	ar0331_mod_args[0] = (char*)"rmmod";
	ar0331_mod_args[1] = (char*)"ar0331";
	ar0331_mod_args[2] = NULL;
	ret_val = agcmd_rmmod(2, (const char * const*)ar0331_mod_args);
	if (ret_val < 0) {
		//AGLOG_ERR("%s: agcmd_rmmod(%s) = %d\n", __func__, ar0331_mod_args[1], ret_val);
	}

	if (hdr_mode) {
		hdr_mode = 1;
	}
	snprintf(hdr_mode_str, sizeof(hdr_mode_str), "hdr_mode=%d", hdr_mode);
	ar0331_mod_args[0] = (char*)"insmod";
	ar0331_mod_args[1] = (char*)"extra/ar0331.ko";
	ar0331_mod_args[2] = hdr_mode_str;
	ar0331_mod_args[3] = NULL;
	ret_val = agcmd_insmod(3, (const char *const *)ar0331_mod_args);
	if (ret_val < 0) {
		//AGLOG_ERR("%s: agcmd_insmod(%s, %s) = %d\n", __func__, ar0331_mod_args[1], ar0331_mod_args[2], ret_val);
	}
	return ret_val;
}


/***************************************************************
static IPC_AVF_Client::create
*/
IPC_AVF_Client* IPC_AVF_Client::pInstance = NULL;
bool IPC_AVF_Client::_bExecuting = false;

void IPC_AVF_Client::create(CUpnpSDRecord* service)
{
	if(pInstance == NULL)
	{
		//printf("--IPC_AVF_Client::initialize\n");
		//CHdmiWatchThread::create();
		pInstance = new IPC_AVF_Client(service);
		pInstance->Go();
	}
}

/***************************************************************
static IPC_AVF_Client::getObject
*/
IPC_AVF_Client* IPC_AVF_Client::getObject()
{
	if(pInstance == NULL)
	{
		printf("--ipc avf object not initialized\n");
	}
	return pInstance;
}

/***************************************************************
static IPC_AVF_Client::destroy
*/
void IPC_AVF_Client::destroy()
{	
	if(pInstance != NULL)
	{
		//printf("--++--IPC_AVF_Client::destroy\n");
		//pInstance->EndStreamOrRecord();
		//CHdmiWatchThread::destroy();
		pInstance->Stop();
		delete pInstance;
		pInstance = NULL;
		printf("--++--IPC_AVF_Client::destroy1\n");
	}
}

/***************************************************************
static IPC_AVF_Client::AutoStopTimerCallBack
*/
void IPC_AVF_Client::AutoStopTimerCallBack(void * p)
{	
	printf("auto stop timer call back\n");
	IPC_AVF_Client* avf = (IPC_AVF_Client*)p;
	avf->timerCallBack();
}

/***************************************************************
static IPC_AVF_Client::timerCallBack
*/
void IPC_AVF_Client::timerCallBack()
{
	_checkTimer = -1;
	_pMutex->Take();
	_command0 = InternalCommand_timerChcek;
	_pOnCommand->Give();
	_pMutex->Give();		
}

void IPC_AVF_Client::_timerCheck()
{
	if((!isCircleMode())&&(_currentRecOption == media_recording))
	{
		int state = _CheckVDBSpace();
		//printf("---timer check remain: %lld\n", sec);
		if(state <= 1)
		{
			//stop
			_pMutex->Take();
			_command0 = InternalCommand_stopRec;
			_pOnCommand->Give();
			_pMutex->Give();
		}
		else
		{
			_checkTimer = CTimerThread::GetInstance()->ApplyTimer(100 , AutoStopTimerCallBack, this, false, AVF_Check_Timer_Name);
		}
	}
}

/***************************************************************
static IPC_AVF_Client::_CheckVDBSpace
*/
int IPC_AVF_Client::_CheckVDBSpace()
{

#ifdef NoCheckSDCard
	return 100;
#else
	//_ReadConfigs();
#ifdef SupportNewVDBAVF
	unsigned int freesize;
	long long totalspace;
	bool b = false;
	//printf("++++IPC_AVF_Client::_CheckVDBSpace\n");
	GetVDBState(b, freesize, totalspace);
	int state = 0;
	if(b)
	{
		state = 2;
	} else {
		if (totalspace == 0)
			state = 0;
		else {
			if (_record_state == CSDRSRecordState::RecordState_recording) state = 2;
			else state = 1;
		}
	}
	return state;
#else
	int bytesPerSeconds = _recording_bitrate / 8;
	UINT64 total, freesize;
	SDWatchThread::getInstance()->GetSpaceInfor(&total, &freesize);
	int remainTime = (int)(freesize / bytesPerSeconds);
	//printf("-- remain seconds: %lld\n", remainTime);
	return remainTime;
#endif

#endif
}

bool IPC_AVF_Client::_tryReleaseSpace()
{
#ifdef SupportNewVDBAVF
	//unsigned int freesize;
	avf_vdb_info_t infor;
	avf_media_free_vdb_space(_p_avf_media, &infor);
	return true;
#else
	return false;
#endif
}
/***************************************************************
static IPC_AVF_Client::avf_event_callback
*/
void IPC_AVF_Client::avf_event_callback( int type, void *pdata )
{
	//printf("--avf_event_callback:%d, %d\n", type, _bExecuting);
	if(!_bExecuting)
		return;
	if(IPC_AVF_Client::getObject())
		IPC_AVF_Client::getObject()->AVFCameraCallback(type);
}

/***************************************************************
private IPC_AVF_Client::AVFCameraCallback
*/
void IPC_AVF_Client::AVFCameraCallback(int type)
{
	if (type == AVF_CAMERA_EventStatusChanged) {
		int state = avf_media_get_state(_p_avf_media);
		if (state == _avf_camera_state)
			return;

		switch (state) {
		case AVF_MEDIA_STATE_IDLE:
			// pause;
			break;
		case AVF_MEDIA_STATE_STOPPED:
			_pRecordStopWait->Give();
			break;

		case AVF_MEDIA_STATE_RUNNING:
			_pPlayingWait->Give();
			break;

		case AVF_MEDIA_STATE_ERROR:
			// reset command
			_pMutex->Take();
			_command0 = InternalCommand_errorProcess;
			_pOnCommand->Give();
			_pMutex->Give();			
			break;
		default:
			break;
		}
		_avf_camera_state = state;
	}
	else if(type == AVF_CAMERA_EventFileEnd)
	{
		printf("--file end\n");
#ifdef XMPPENABLE
		char fileName[256];
		avf_media_get_string(_p_avf_media, "state.file.previous", 1, fileName,256 ,"");
		//if(strstr(fileName, "/xmpp/") > 0)
		//{
			if(_pAVFDelegate)
			{
				//printf("stream file ready : %s\n", fileName);
				_pAVFDelegate->onNewFileReady(fileName);
			}
			else
			{
				//printf("remove : %s\n", fileName);
				remove(fileName);
			}
		//}
#endif		
	}
	else if(type == AVF_CAMERA_EventFileStart)
	{
		printf("--file new\n");
	}
#ifdef QRScanEnable
	else if(type == AVF_CAMERA_EventDoneSaveNextPicture)
	{
		printf("--snap new jpeg\n");
		_pCaptureJpegWait->Give();
	}
#endif
#ifdef WriteBufferOverFlowEnable
	else if(type == AVF_CAMERA_EventBufferOverRun)
	{
		_pMutex->Take();
		_command0 = InternalCommand_fileBufferWarning;
		_pOnCommand->Give();
		_pMutex->Give();

	}

	else if(type == AVF_CAMERA_EventBufferOverflow)
	{
		_pMutex->Take();
		_command0 = InternalCommand_fileBufferError;
		_pOnCommand->Give();
		_pMutex->Give();
	}
#endif
	else if(type == AVF_CAMERA_EventDiskError)
	{
		printf("---Disk Error event, stop record.\n");
		_pMutex->Take();
		_command0 = InternalCommand_stopRec;
		_pOnCommand->Give();
		_pMutex->Give();
	}
	else if(type == AVF_CAMERA_EventBufferSpaceFull)
	{
		printf("---space full event, stop record.\n");
		_pMutex->Take();
		_command0 = InternalCommand_stopRec;
		_pOnCommand->Give();
		_pMutex->Give();
	}
	else if(type == AVF_CAMERA_EventVdbStateChanged)
	{
		printf("---vdb state changed event.\n");
		_pMutex->Take();
		_command0 = InternalCommand_VdbStateChanged;
		_pOnCommand->Give();
		_pMutex->Give();
	}
}

/***************************************************************
private IPC_AVF_Client::IPC_AVF_Client
*/
IPC_AVF_Client::IPC_AVF_Client
	(
		CUpnpSDRecord* service
	) : CThread("ipc AVF client thread", CThread::NORMAL, 0, NULL)
	, _p_avf_media(NULL)
	,_recording_frame(-1)
	,_bChangeVin(false)
	,_pUpnp_rec_service(service)
	,_bBigStream(TRUE)
	,_avfMode(AVF_Mode_NULL)
	, _command(InternalCommand_Null)
	, _command0(InternalCommand_Null)
	, _command1(InternalCommand_Null)
	, _hdmiOn(false)
	, _checkTimer(-1)
	, _bWaitTFReady(false)
	, _play_state(PLAY_STATE_unknow)
	, _pAVFDelegate(NULL)
	, _bHDROn(-1)
	,_lcdOn(FALSE)
{
	_pRecordStopWait = new CSemaphore(0, 1, "ipc avf stop sem");
	_pPlayingWait = new CSemaphore(0, 1, "ipc avf play sem");
	_pCaptureJpegWait = new CSemaphore(0, 1, "jpeg capture sem");
	if (ipc_init() < 0)
	{
		printf("---ipc init failed\n");
		return;
	}
	_avf_camera_state = AVF_MEDIA_STATE_STOPPED;
	_p_avf_media = avf_media_create(IPC_AVF_Client::avf_event_callback, this);
	//LCDOnOff();
	avf_display_set_backlight(_p_avf_media, VOUT_0, "t15p00", 0);
	avf_media_close(_p_avf_media, 1);

	/*int ret = agsys_input_read_switch(SW_ROTATE_LOCK);
	if(ret)*/
	{
		_config_lcd_vout(true);
		_lcdOn = true;
	}
	/*else
	{
		_config_hdmi_vout(true, true, true);
		_lcdOn = false;
	}*/
	
	_pOnCommand = new CSemaphore(0,1, "ipc avf comand sem");
	_command = InternalCommand_Null;
	_pMutex = new CMutex("avf mutex");
	_currentRecOption = media_NULL;
}

/***************************************************************
private IPC_AVF_Client::~IPC_AVF_Client
*/
IPC_AVF_Client::~IPC_AVF_Client()
{
	_bExecuting =false;
	_pUpnp_rec_service = NULL;
	_command = InternalCommand_Null;
	_pOnCommand->Give();
	if(_avfMode == AVF_Mode_Decode)
	{

	}
	else if(_avfMode == AVF_Mode_Encode)
	{

	}
	delete _pMutex;
	delete _pOnCommand;
	
	delete _pCaptureJpegWait;
	delete _pRecordStopWait;
	delete _pPlayingWait;
	avf_media_destroy(_p_avf_media);
}


int IPC_AVF_Client::_config_lcd_vout(bool bON)
{
	vout_config_t config;
	//printf("avf_display_set_mode set mode idle\n");
	if (avf_display_set_mode(_p_avf_media, MODE_IDLE) < 0)
		return -1;

	// init LCD
	if(bON)
	{	
		if(_lcdOn)
			return 0;
		memset(&config, 0, sizeof(config));
		config.vout_id = VOUT_0;
		config.vout_type = VOUT_TYPE_DIGITAL;
		config.vout_mode = VOUT_MODE_D320x240;
		config.enable_video = 1;
		config.fb_id = 0;
		if (avf_display_set_vout(_p_avf_media, &config, "t15p00") < 0)
			return -1;

		//printf("avf_display_set_mode set mode MODE_DECODE\n");
		//if (avf_display_set_mode(_p_avf_media, MODE_DECODE) < 0)
		//	return -1;
		_lcdOn = true;
		return 0;
	}
	else
	{
		if(!_lcdOn)
			return 0;
		avf_display_disable_vout(_p_avf_media, VOUT_0);
		_lcdOn = false;
	}
	return 0;
}

int IPC_AVF_Client::_config_hdmi_vout(bool on, bool fb, bool small)
{
	vout_config_t config;
	//printf("_config_hdmi_vout avf_display_set_mode set mode MODE_IDLE\n");
	//if (avf_display_set_mode(_p_avf_media, MODE_IDLE) < 0)
	//	return -1;
	
	if (on) {
		if(_hdmiOn)
			return 0;
		memset(&config, 0, sizeof(config));
		config.vout_id = VOUT_1;
		config.vout_type = VOUT_TYPE_HDMI;
		if(small)
			config.vout_mode = VOUT_MODE_480p;
		else
			config.vout_mode = VOUT_MODE_1080i;
		config.enable_video = 1;
		if(fb)
			config.fb_id = 0;
		else
			config.fb_id = -1;
		if (avf_display_set_vout(_p_avf_media, &config, "") < 0)
		{
			return -1;
		}
		else
			_hdmiOn = true;
	} else {
		if(!_hdmiOn)
			return 0;
		avf_display_disable_vout(_p_avf_media, VOUT_1);
		_hdmiOn = false;
	}
	return 0;
}

/***************************************************************
private IPC_AVF_Client::_PrepareVinSetting
*/
void IPC_AVF_Client::_PrepareVinSetting(char* tmp, int length)
{
	int ret;
	ret = agsys_input_read_switch(SW_ROTATE_LOCK);
	_bRotate = (ret == 1);
	
	int h =0,v = 0;
	char* vinResolution = (char*)"2304x1296";
	char* vinFrequency= (char*)"29.97";
	struct agsys_platform_info_s agboard_helper_platform_info;
	ret = agsys_platform_get_info(&agboard_helper_platform_info);
	int switchOption = SW_ROTATE_LOCK_MIRROR;
	printf("--board :%d\n",agboard_helper_platform_info.ambarella_info.board_rev);
	switch (agboard_helper_platform_info.ambarella_info.board_rev)
		{
		case 70:	//Fog + 8940
			vinResolution=(char*)"720p";
			break;
		case 326:	//Fog + 8974
			break;
		case 65:	//Castle
			
			break;
		case 67:    //diablo
		case 323:   //diablo--2
	printf("--vin :%dx%dx%d\n",_recording_width, _recording_height, _recording_frame);
			if (_recording_width == 1280 &&
				_recording_height == 720 &&
				_recording_frame == 60) {
				vinResolution = (char*)"1920x1080";
				vinFrequency = (char*)"60";
			} else if (_recording_width == 720 &&
				_recording_height == 480 &&
				_recording_frame == 60) {
				vinResolution = (char*)"1920x1080";
				vinFrequency = (char*)"60";
			}
			if(_bRotate)
			{
				h = 0;
				v = 0;
			}
			else
			{
				h = 1;
				v = 1;
			}
			break;
		case 84:	//TSF_HD22A + 8940
		case 340:	//TSF_HD22A + 8974
			h = _bRotate ? 1 : 0;
			if(switchOption == 0)
				v = _bRotate ? 1 : 0;
			else
				v = 1;
			break;
		case 68:	//TSF_HD22D
			vinResolution=(char*)"1080p";
			h = 1;
			v = 1;
			//set hdr on off here
			if(_bHDROn == _bRotate)
				break;
			agboard_switch_ar0331_module(_bRotate);
			_bHDROn = _bRotate;
			break;
		case 66:	//TSF_HD23A
			if(_bRotate)
			{
				h = 0;
				v = 0;
			}
			else
			{
				h = 1;
				v = 1;
			}
			break;
		default:
			
			break;
	}
	sprintf(tmp, "%s:%s:%d:%d:1", vinResolution, vinFrequency, h, v);
}
/***************************************************************
private IPC_AVF_Client::_PrepareRecord
*/
void IPC_AVF_Client::_PrepareRecord()
{
	if (_bChangeVin) {
		return _rePrepareRecord();
	}
	char tmp[256];
	int ret = 0;
	if (_p_avf_media == NULL) {
		printf("avf_camera_create failed\n");
	} else {
		avf_camera_set_media_source(_p_avf_media, "", "/usr/local/bin/camera.conf");
		if(_pAVFDelegate){
			_pAVFDelegate->onRecodeStateChange(IPC_AVF_Delegate::recordState_starting);
		}
		avf_camera_stop(_p_avf_media, 1);
		_PrepareVinSetting(tmp, 256);
		printf("----set vin: %s\n", tmp);
		ret = avf_media_set_config_string(_p_avf_media,"config.vin", 0, tmp);
		_updateSubtitleConfig();
		_avfMode = AVF_Mode_Encode;
		if(_pAVFDelegate) {
			_pAVFDelegate->onCameraModeChange();
		}
	}
}

void IPC_AVF_Client::_rePrepareRecord()
{
	char tmp[256];
	int ret = 0;
	if (_p_avf_media == NULL) {
		printf("avf_camera_create failed\n");
	}else
	{
		if(_pAVFDelegate){
			_pAVFDelegate->onRecodeStateChange(IPC_AVF_Delegate::recordState_starting);
		}
		avf_camera_close(_p_avf_media, 1);
		avf_camera_set_media_source(_p_avf_media, "", "/usr/local/bin/camera.conf");
		_PrepareVinSetting(tmp, 256);
		printf("--re--set vin: %s\n", tmp);
		ret = avf_media_set_config_string(_p_avf_media,"config.vin", 0, tmp);
		_updateSubtitleConfig();
		_avfMode = AVF_Mode_Encode;
		avf_camera_open(_p_avf_media, 1);
		_bChangeVin = false;
		if(_pAVFDelegate) {
			_pAVFDelegate->onCameraModeChange();
		}
	}	
}

/***************************************************************
private IPC_AVF_Client::_PrepareDecode
*/
void IPC_AVF_Client::_PrepareDecode(char* path)
{	
	printf("prepare decode for : %s\n", path);
	avf_playback_set_media_source(_p_avf_media, path, "/usr/local/bin/playback.conf");
	if (_p_avf_media == NULL) {
		printf("avf_camera_create failed\n");
	}else
	{
		avf_playback_open(_p_avf_media, 1);
		_avfMode = AVF_Mode_Decode;
		_play_state = PLAY_STATE_ready;
		if(_pAVFDelegate)
		{
			_pAVFDelegate->onCameraModeChange();
		}
	}	
}

/***************************************************************
private  _switchToRec
*/
void IPC_AVF_Client::_switchToRec(bool forceStartRec)
{	
	if(_avfMode == AVF_Mode_NULL)
	{
		if(_lcdOn)
			_config_hdmi_vout(false, false, false);
	}
	else if(_avfMode == AVF_Mode_Decode)
	{
		_ClosePlay();
		if(_lcdOn)
			_config_hdmi_vout(false, false, false);
	}
	else
	{
		// do nothing
	}

	_ReadConfigs();
	_PrepareRecord();
	_bWaitTFReady = false;
	if(_pUpnp_rec_service != NULL)
	{
		if((isAutoStartRec() || forceStartRec)&&(_bCheckAutoRec))
		{
			if((_CheckVDBSpace() == 1))
			{
				if (isCircleMode())
				{
					printf("--- try release space\n");
					_tryReleaseSpace();
				}
			}

			if(_CheckVDBSpace() > 1)
			{
				printf("--- start rec\n");
				_startAVFRec();
			}
			else
			{		
				printf("--- start stream, wait tf ready\n");
				_startAVFStream();
				_bWaitTFReady = true;
			}
		}
		else
		{
			printf("--- start stream\n");
			_startAVFStream();
		}
	}
}

void IPC_AVF_Client::_tfReady()
{
#ifdef SupportNewVDBAVF
	printf("tf ready enable avf\n");
	avf_media_storage_ready(_p_avf_media, 1);
#else
	if((_bWaitTFReady)&&(_currentRecOption != media_recording))
	{
		_startAVFRec();
		_bWaitTFReady = false;
	}
#endif
}
void IPC_AVF_Client::_vdbStateChanged()
{
#ifdef SupportNewVDBAVF
	unsigned int freesize;
	long long totalspace;
	bool b = false;
	//printf("++++IPC_AVF_Client::_CheckRemainSpace\n");
	GetVDBState(b, freesize, totalspace);
	printf("_vdbStateChanged(), return %d\n", b);
	if(b) {
		_bWaitTFReady = false;
		_switchToRec(false);
	}
#endif
}

void IPC_AVF_Client::_tfRemove()
{
#ifdef SupportNewVDBAVF
	printf("tf remove disable avf\n");
	avf_media_storage_ready(_p_avf_media, 0);
#endif
	if((_currentRecOption == media_recording)&&(isCircleMode()))
	{
		_startAVFStream();
		_bWaitTFReady = true;
	}
}

/***************************************************************
private  _switchToPlay
*/
void IPC_AVF_Client::_switchToPlay()
{
	if(_avfMode == AVF_Mode_NULL)
	{	
		if(_lcdOn)
			_config_hdmi_vout(true, false, false);
	}
	else if(_avfMode == AVF_Mode_Encode)
	{
		_CloseRec();
		if(_lcdOn)
			_config_hdmi_vout(true, false, false);
	}
	else if(_avfMode == AVF_Mode_Decode)
	{	
		// stop?
	}
	//_CheckVoutConfig(AVF_Mode_Decode);
	//printf("--switch to play\n");
	_playCurrentFile();
}

void IPC_AVF_Client::_playCurrentFile()
{
	_PrepareDecode(_currentMovie);
	_AVFPlay();
}

void IPC_AVF_Client::_onAvf_Error_process()
{
	if(_avfMode == AVF_Mode_Decode)
	{
		printf("reset decode \n");
	}
	else if(_avfMode == AVF_Mode_Encode)
	{
		printf("reset record \n");
	}
}

/***************************************************************
private IPC_AVF_Client::AVFReleaseMedia no use

void IPC_AVF_Client::AVFReleaseMedia()
{
	// todo
}*/

/***************************************************************
private IPC_AVF_Client::ReadConfigs
*/
void IPC_AVF_Client::_ReadConfigs()
{
	if(_pUpnp_rec_service)
	{
		//printf("_ReadConfigs --bitrate read trace\n");
		int oldframerate = _recording_frame;
		_pUpnp_rec_service->getResolution(_recording_width, _recording_height, _recording_frame);
		_recording_bitrate = _pUpnp_rec_service->getBitrate();
		_seg_iframe_num = _pUpnp_rec_service->getSegSeconds();
		_record_color_mode = _pUpnp_rec_service->getColorMode();
		if ((oldframerate > 0) && (oldframerate != _recording_frame)) {
			printf("---> Vin Framerate Changed: %d-> %d\n", oldframerate, _recording_frame);
			_bChangeVin = true;
		}
	}
	//printf("---set resolution: %dx%dx%d, quality:%d,\n", _recording_width, _recording_height,_recording_frame,_recording_bitrate);
}

/***************************************************************
private IPC_AVF_Client::_Update_Recording_Config
*/
void IPC_AVF_Client::_Update_Recording_Config()
{
	//printf("-- _Update_Recording_Config\n");
	//int r = 0;
	//DefaultMicVolume();
#ifdef XMPPENABLE
		_ReadConfigs();
		char input[256];
		memset(input, 0, sizeof(input));
	#ifdef constantBitrate
		sprintf(input, "h264:%dx%d:%d:%d:%d", _recording_width, _recording_height, _recording_bitrate, _recording_bitrate, _recording_bitrate);
	#else
		sprintf(input, "h264:%dx%d:%d:%d:%d", _recording_width, _recording_height, _recording_bitrate, _recording_bitrate*10/12 , _recording_bitrate / 2);
	#endif
		avf_media_set_config_string(_p_avf_media,"config.video", 0, input);
		avf_media_set_config_int32(_p_avf_media,"config.record", 0, 1);
		//avf_media_set_config_int32(_p_avf_media,"config.video.segment", 0, 1);
		//avf_media_set_config_string(_p_avf_media,"config.muxer.path", 0, POOL_PATH);
		//avf_media_set_config_string(_p_avf_media,"config.muxer.type", 0, "ts");

		memset(input, 0, sizeof(input));
		sprintf(input, "h264:%dx%d:%d:%d:%d", 512, 288, 512000, 460000, 256000);
		avf_media_set_config_string(_p_avf_media,"config.video", 2, input);
		avf_media_set_config_int32(_p_avf_media,"config.record", 2, 1);
		avf_media_set_config_int32(_p_avf_media,"config.video.segment", 2, 1);
		avf_media_set_config_string(_p_avf_media,"config.muxer.path", 2, "/tmp/xmpp/");
		avf_media_set_config_string(_p_avf_media,"config.muxer.type", 2, "ts");
		
		memset(input, 0, sizeof(input));
		sprintf(input, "mjpeg:%dx%dx%d", small_stream_width, small_stream_height, small_stream_quality);
		avf_media_set_config_string(_p_avf_media,"config.video", 1, input);
		if (120 <= _recording_frame) {
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:120");
		} else if  (90 <= _recording_frame) {
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:90");
		} else if  (60 <= _recording_frame) {
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:60");
		} else {
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:30");
		}
#else

		_ReadConfigs();
		char input[256];
		memset(input, 0, sizeof(input));
	#ifdef constantBitrate
		sprintf(input, "h264:%dx%d:%d:%d:%d", _recording_width, _recording_height, _recording_bitrate, _recording_bitrate, _recording_bitrate);
	#else
		sprintf(input, "h264:%dx%d:%d:%d:%d", _recording_width, _recording_height, _recording_bitrate / 2, _recording_bitrate, _recording_bitrate*10/12);
	#endif
		avf_media_set_config_string(_p_avf_media,"config.video", 0, input);
		avf_media_set_config_int32(_p_avf_media,"config.record", 0, 1);
		avf_media_set_config_int32(_p_avf_media,"config.video.segment", 0, _seg_iframe_num);
		avf_media_set_config_string(_p_avf_media,"config.muxer.path", 0, POOL_PATH);

	#ifdef Support2ndTS
		memset(input, 0, sizeof(input));
		sprintf(input, "h264:%dx%d:%d:%d:%d", 512, 288, 880000, 880000, 880000);
		avf_media_set_config_string(_p_avf_media,"config.video", 2, input);
		avf_media_set_config_int32(_p_avf_media,"config.record", 2, 1);
		//avf_media_set_config_int32(_p_avf_media,"config.video.segment", 2, 120);
		//avf_media_set_config_string(_p_avf_media,"config.muxer.path", 2, "/tmp/xmpp/");
		avf_media_set_config_string(_p_avf_media,"config.muxer.type", 2, "ts");
	#endif
		memset(input, 0, sizeof(input));
		sprintf(input, "mjpeg:%dx%dx%d", small_stream_width, small_stream_height, small_stream_quality);
		avf_media_set_config_string(_p_avf_media,"config.video", 1, input);
		if (120 <= _recording_frame) {
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:120");
		} else if  (90 <= _recording_frame) {
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:90");
		} else if  (60 <= _recording_frame) {
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:60");
		} else {
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:30");
		}
#endif
	// gps infor size
	int font_height = 36;
	int row_height = 44;
	font_height = font_height*_recording_width/1920;
	row_height = row_height*_recording_width/1920;
	memset(input, 0, sizeof(input));
	sprintf(input, "1:0,0,0,%d:%d", row_height, font_height);
	avf_media_set_config_string(_p_avf_media,"config.video.overlay", 0, input);
	char name[256];
	agcmd_property_get("upnp.camera.name", name , "");
	SetCameraName(name);

	font_height = 36;
	row_height = 44;
	font_height = font_height*small_stream_width/1920;
	row_height = row_height*small_stream_width/1920;
	memset(input, 0, sizeof(input));
	sprintf(input, "1:0,0,0,%d:%d", row_height, font_height);
	avf_media_set_config_string(_p_avf_media,"config.video.overlay", 1, input);

#ifdef SupportNewVDBAVF
	//auto delete or not,
	if(isCircleMode())
	{
		printf("--set circle mode \n");
		avf_media_set_config_int32(_p_avf_media,"config.vdb.autodel", 0, 1);
	}
	else
	{
		avf_media_set_config_int32(_p_avf_media,"config.vdb.autodel", 0, 0);
	}
#endif

}

/***************************************************************
private IPC_AVF_Client::_Update_Streaming_Config
*/
void IPC_AVF_Client::_Update_Streaming_Config()
{
	//printf("-- _Update_Streaming_Config\n");
	_ReadConfigs();
	char input[256];
	// resolution for encode
	avf_media_set_config_string(_p_avf_media,"config.video", 0, "none");
	memset(input, 0, sizeof(input));
	if(_bBigStream)
		sprintf(input, "mjpeg:%dx%dx%d", big_stream_width, big_stream_height, big_stream_quality);
	else
		sprintf(input, "mjpeg:%dx%dx%d", small_stream_width, small_stream_height, small_stream_quality);
	avf_media_set_config_string(_p_avf_media,"config.video", 1, input);
	avf_media_set_config_int32(_p_avf_media,"config.record", 0, 0);
	
	// gps infor size
	int streamWidth = _bBigStream ?  big_stream_width : small_stream_width;
		
	int font_height = 36;
	int row_height = 44;
	font_height = font_height*streamWidth/1920;
	row_height = row_height*streamWidth/1920;
	memset(input, 0, sizeof(input));
	sprintf(input, "1:0,0,0,%d:%d", row_height, font_height);
	avf_media_set_config_string(_p_avf_media,"config.video.overlay", 1, input);
	if (120 <= _recording_frame) {
		if(_bBigStream)
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"30:120");
		else
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:120");
	} else if  (90 <= _recording_frame) {
		if(_bBigStream)
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"30:90");
		else
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:90");
	} else if  (60 <= _recording_frame) {
		if(_bBigStream)
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"30:60");
		else
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:60");
	} else {
		if(_bBigStream)
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"30:30");
		else
			avf_media_set_config_string(_p_avf_media,"config.mjpeg.fps",0,"15:30");
	}
}

void IPC_AVF_Client::_Update_aaa_Config()
{
	//printf("+++++_Update_aaa_Config\n");
	char input[64];
	memset(input, 0, sizeof(input));
	sprintf(input, "%d", _record_color_mode);
	avf_camera_image_control(_p_avf_media, "metermode", input);

	char tt[16];
	memset(tt, 0, sizeof(tt));
	int acu;
#ifdef GPSManualFilterOn
	agcmd_property_get(GPSManualFilterProp, tt, GPS_ACU_DEFAULT_THRESHOLD);
	acu = atoi(tt);
	printf("set gps filter: %dcm, %s\n", acu,  tt);
#else
	acu = atoi(GPS_ACU_DEFAULT_THRESHOLD);
	printf("set default gps filter: %dcm\n", acu);
#endif
	//sprintf(input,"3,%s",tt);
	 
	//avf_media_set_config_string(_p_avf_media,"config.gps.satsnr", 0, input);
	avf_media_set_config_int32(_p_avf_media, "config.gps.accuracy", 0, acu);
}

/***************************************************************
_startAVFRec
*/ // add timer watch

//FileQueueControl::GetStorageFreeSpace()
//_recording_bitrate
// if normal mode
// estimate space and current bit stream
// set half length timer.
// if half remain time < 30 secons,
// auto stop norml record
// 
void IPC_AVF_Client::_startAVFRec()
{
	// nomal mode
	//printf("_startAVFRec1\n");
	SDWatchThread::getInstance()->CancelMarkState();
	if(_pUpnp_rec_service != NULL)
	{
		_record_state = CSDRSRecordState::RecordState_starting;
		_pUpnp_rec_service->SetRecordState(_record_state);
		if(_pAVFDelegate)
		{
			_pAVFDelegate->onRecodeStateChange(IPC_AVF_Delegate::recordState_starting);
		}
	}
	if(_currentRecOption != media_NULL)
	{
		if (avf_camera_stop(_p_avf_media, 0) < 0)
		{
			printf("avf_camera_stop failed\n");
		}
		_pRecordStopWait->Take(-1);
		_currentRecOption = media_NULL;
	}
	_Update_Recording_Config();
	//printf("_startAVFRec2\n");
	if (avf_camera_run(_p_avf_media, 0) < 0) {
		printf("avf_camera_run failed\n");
		return;
	}
	
	_currentRecOption = media_recording;
	_pPlayingWait->Take(-1);
	_Update_aaa_Config();
	_recordingStartTime = GetCurrentTick() / 1000;

	//	
	if(_pUpnp_rec_service != NULL)
	{
		_record_state = CSDRSRecordState::RecordState_recording;
		_pUpnp_rec_service->SetRecordState(_record_state);
		if(_pAVFDelegate)
		{
			_pAVFDelegate->onRecodeStateChange(IPC_AVF_Delegate::recordState_recording);
		}
	}
	//_timerCheck();
	//printf("_startAVFRec3\n");
}

/***************************************************************
_startAVFRec
*/
void IPC_AVF_Client::_startAVFStream()
{
	//printf("-- start avf stream\n");
	if(_pUpnp_rec_service != NULL)
	{
		_record_state = CSDRSRecordState::RecordState_stopping;
		_pUpnp_rec_service->SetRecordState(_record_state);
		if(_pAVFDelegate)
		{
			_pAVFDelegate->onRecodeStateChange(IPC_AVF_Delegate::recordState_stopping);
		}
	}
	if(_currentRecOption != media_NULL)
	{
		//printf("-- avf_camera_stop 0\n");
		if (avf_camera_stop(_p_avf_media, 0) < 0)
		{
			printf("avf_camera_stop failed\n");
		}
		//printf("-- avf_camera_stop 1\n");
		_pRecordStopWait->Take(-1);
		_currentRecOption = media_NULL;
	}
	
	_Update_Streaming_Config();
	//printf("-- avf_camera_run 0\n");
	if (avf_camera_run(_p_avf_media, 0) < 0) {
		//printf("avf_camera_run failed\n");
		return;
	}
	_currentRecOption = media_streaming;
	_pPlayingWait->Take(-1);
	_Update_aaa_Config();

	if(_pUpnp_rec_service != NULL)
	{
		_record_state = CSDRSRecordState::RecordState_stop;
		_pUpnp_rec_service->SetRecordState(_record_state);
		if(_pAVFDelegate)
		{
			_pAVFDelegate->onRecodeStateChange(IPC_AVF_Delegate::recordState_stoped);
		}
	}
}

void IPC_AVF_Client::_AVFPlay()
{
	avf_playback_run(_p_avf_media, 0);
	_play_state = PLAY_STATE_playing;
}

void IPC_AVF_Client::_AVFPause()
{
	avf_playback_pause_resume(_p_avf_media);
}

int IPC_AVF_Client::_ClosePlay()
{
	if(avf_playback_stop(_p_avf_media, 1) < 0)
	{
		printf("avf_playback_stop failed\n");
	}
	avf_playback_close(_p_avf_media, 1);	
	_avfMode = AVF_Mode_NULL;
	if(_pAVFDelegate)
	{
		_pAVFDelegate->onCameraModeChange();
	}
	//_play_state = PLAY_STATE_unknow;
	return 0;
}

int IPC_AVF_Client::_CloseRec()
{
	_bWaitTFReady = false;
	//printf("---avf_camera_stop \n");
	if(_pUpnp_rec_service != NULL)
	{
		_record_state = CSDRSRecordState::RecordState_close;
		_pUpnp_rec_service->SetRecordState(_record_state);
	}
	if (avf_camera_stop(_p_avf_media, 0) < 0)
	{
		printf("avf_camera_stop failed\n");
	}
	else
	{
		//printf("---_pRecordStopWait \n");
		_pRecordStopWait->Take(-1);
		_currentRecOption = media_NULL;
	}	
	//printf("---avf_camera_close \n");
	avf_camera_close(_p_avf_media, 1);
	_avfMode = AVF_Mode_NULL;
	if(_pAVFDelegate)
	{
		_pAVFDelegate->onCameraModeChange();
	}
	return 0;
}

void IPC_AVF_Client::_updateSubtitleConfig()
{
	char tmp[256];
	int c = 0;
	agcmd_property_get("system.ui.namestamp", tmp, "On");
	if(strcmp(tmp, "On") == 0)
	{
		c = c | 0x01;
		char name[256];
		agcmd_property_get("upnp.camera.name", name , "");
		IPC_AVF_Client::getObject()->SetCameraName(name);
		printf("set name %s\n", name);
	}
	agcmd_property_get("system.ui.timestamp", tmp, "On");
	if(strcmp(tmp, "On") == 0)
	{
		c = c | 0x02;
	}
	agcmd_property_get("system.ui.coordstamp", tmp, "On");
	if(strcmp(tmp, "On") == 0)
	{
		c = c | 0x04;
	}
	agcmd_property_get("system.ui.speedstamp", tmp, "On");
	if(strcmp(tmp, "On") == 0)
	{
		c = c | 0x08;
	}
	//speed | position | time | prefix
	avf_media_set_config_int32(_p_avf_media,"config.subtitle.flags", 0, c);

	agcmd_property_get("system.ui.overspeed", tmp, "Off");
	if(strcmp(tmp, "On") == 0)
	{
		agcmd_property_get("system.ui.speedroof", tmp, "120");
		int speed = atoi(tmp);
		avf_media_set_config_int32(_p_avf_media,"config.subtitle.slimit", 0, speed);
		avf_media_set_config_string(_p_avf_media,"config.subtitle.slimit-str", 0, "");
	}else
	{
		avf_media_set_config_int32(_p_avf_media,"config.subtitle.slimit", 0, -1);
	}
	agcmd_property_get("system.ui.language", tmp, "English");
	if(strcmp(tmp, "中文") == 0) //chinese
	{
		avf_media_set_config_string(_p_avf_media, "config.subtitle.timefmt", 0, "%04Y/%02m/%02d %02H:%02M:%02S"); 
	}
	else
		avf_media_set_config_string(_p_avf_media, "config.subtitle.timefmt", 0, "%02d/%02m/%04Y %02H:%02M:%02S"); 
}

/***************************************************************
IPC_AVF_Client::main
*/
void IPC_AVF_Client::Stop()
{
	_bExecuting = false;
	_command0 = InternalCommand_Null;
	_command1 = InternalCommand_Null;
	_command = InternalCommand_Null;
	_pOnCommand->Give();
	CThread::Stop();
}

void IPC_AVF_Client::main(void *)
{
	_bExecuting = true;
		
	int cmd;
	while(_bExecuting)
	{
		_pOnCommand->Take(-1);
		_pMutex->Take();
		if(_command0 != InternalCommand_Null)
		{
			cmd = _command0;
			_command0 = InternalCommand_Null;
		}
		else if(_command1 != InternalCommand_Null)
		{
			cmd = _command1;
			_command1 = InternalCommand_Null;
		}
		else 
		{
			cmd = _command;
			_command = InternalCommand_Null;
		}
		_pMutex->Give();
		switch(cmd)
		{
			case InternalCommand_Null:
				break;

			case InternalCommand_errorProcess:
				_onAvf_Error_process();
				break;

			case InternalCommand_record:
				if(_avfMode == AVF_Mode_Encode)
				{
					_startAVFRec();
				}
				break;

			case InternalCommand_stopRec:
				if(_avfMode == AVF_Mode_Encode)
				{	
					_startAVFStream();
				}
				break;
			case InternalCommand_closeRec:
				if(_avfMode == AVF_Mode_Encode)
				{
					_CloseRec();
				}
				break;
			case InternalCommand_switchToRec:
				_switchToRec(false);				
				break;
			case InternalCommand_switchToPlay:
				_switchToPlay();
				break;
			case InternalCommand_Play:
				if(_avfMode == AVF_Mode_Decode)
				{	
					_AVFPlay();
				}
				break;
			case InternalCommand_StopPlay:

				break;
			case InternalCommand_PauseResume:
				if(_avfMode == AVF_Mode_Decode)
				{
					_AVFPause();
				}
				break;
			case InternalCommand_ClosePlay:
				if(_avfMode == AVF_Mode_Decode)
				{
					_ClosePlay();
				}
				break;
			case InternalCommand_Seek:

				break;
			case InternalCommand_timerChcek:
				_timerCheck();
				break;
			case InternalCommand_tfReady:
				_tfReady();
				break;
			case InternalCommand_tfRemove:
				_tfRemove();
				break;
			case InternalCommand_playCurrent:
				_playCurrentFile();
				break;
			case InternalCommand_setSubtitleConfig:
				_updateSubtitleConfig();
				break;
			case InternalCommand_RestartRec:
				if(_avfMode == AVF_Mode_Encode)
				{
					_CloseRec();
					_switchToRec(_restartRecordPara);	
				}
				break;
			case InternalCommand_fileBufferWarning:
				{
					_pUpnp_rec_service->EventWriteWarnning();
				}
				break;
			case InternalCommand_fileBufferError:
				{
					if(_avfMode == AVF_Mode_Encode)
					{	
						_startAVFStream();
					}
					_pUpnp_rec_service->EventWriteError();
				}
				break;
			case InternalCommand_VdbStateChanged:
				{
					_vdbStateChanged();
				}
				break;
			default:
				break;
		}

		_command = InternalCommand_Null;
		
	}

	
}


/***************************************************************
getRecordingTime
*/
UINT64 IPC_AVF_Client::getRecordingTime()
{
	if(_currentRecOption == media_recording)
	{
		//time_t currenttime; 
		//time(&currenttime);
		long long time = GetCurrentTick() / 1000 - _recordingStartTime;
		if(time > 0)
			return  time;
		else
			return 0;
	}
	else
		return 0;
}

int	IPC_AVF_Client::getRecordState()
{
	printf(">>>getRecordState() -- : %d\n", _record_state);
	return _record_state;
}

/***************************************************************
RecordKeyProcess
*/
void IPC_AVF_Client::RecordKeyProcess()
{
	//printf("--car mode : %d, state : %d\n",_pUpnp_rec_service->_pStateRecMode->CarMode(),  _record_state);
	if((_pUpnp_rec_service->isCarMode())&&(_record_state != CSDRSRecordState::RecordState_stop))
	{
#if (defined PLATFORM23)  || (defined PLATFORM22A) || (defined PLATFORM22D)
		SDWatchThread::getInstance()->OnMarkEvent(true);
#else
		StartRecording(NULL, 0);
#endif
	}
	else if((_pUpnp_rec_service->isAlertMode()&&(_record_state != CSDRSRecordState::RecordState_stop)))
	{
		printf("+++will send mail\n");
		// prepare clip
		//send to a thread wait file prepared.
		//during prepare process , skip more key event.
		//signal mail
#ifdef MailThreadAvaliable
		//CMailServerThread::getInstance()->TrigerMail( CMail::TrigerMailEvent_Alert, char * movieClipPath, char * posterPath);
#endif
	}
	else
	{
		StartRecording(NULL, 0);
	}
}

void IPC_AVF_Client::GsensorSignalProcess()
{
	if((_pUpnp_rec_service->isCarMode())&&(_record_state != CSDRSRecordState::RecordState_stop))
	{
		SDWatchThread::getInstance()->OnMarkEvent(false);
	}
}

/***************************************************************
CheckFreeAutoStop
*/
void IPC_AVF_Client::CheckFreeAutoStop()
{
	long long space = FileQueueControl::GetStorageFreeSpace();
	//long long oneFileSpaceNeeded;
	printf("-- CheckFreeAutoStop\n ");
	if((!isCircleMode())&&(space < _pUpnp_rec_service->getOneFileSpaceNeeded()))
	{
		printf("----no enough space :%lld\n",space);
		_pMutex->Take();		
		if(_currentRecOption == media_recording)
		{
			_command = InternalCommand_stopRec;
			_pOnCommand->Give();
		}
		_pMutex->Give();
	}
	
}

/***************************************************************
isCarMode
*/
bool IPC_AVF_Client::isCircleMode()
{
	bool  rt = _pUpnp_rec_service->isCarMode() || _pUpnp_rec_service->isAlertMode();
	return rt;
}
bool IPC_AVF_Client::isAutoStartRec()
{
	return _pUpnp_rec_service->isAutoStartMode();
}

bool IPC_AVF_Client::isAlertMode()
{
	return _pUpnp_rec_service->isAlertMode();
}

/***************************************************************
switchToRec
*/
void IPC_AVF_Client::switchToRec(bool checkAuto)
{
	_pMutex->Take();
	_bCheckAutoRec = checkAuto;
	_command1 = InternalCommand_switchToRec;
	_pOnCommand->Give();
	_pMutex->Give();
}

void IPC_AVF_Client::onTFReady(bool b)
{
	printf("+++ onTFReady: %d \n", b);
	_pMutex->Take();
	if(b)
		_command1 = InternalCommand_tfReady;
	else
		_command1 = InternalCommand_tfRemove;
	_pOnCommand->Give();
	_pMutex->Give();

}
/***************************************************************
RestartRecord
*/
void IPC_AVF_Client::RestartRecord()
{
	_pMutex->Take();
	_restartRecordPara = (_currentRecOption == media_recording);
	{
		printf("------on restart cmd!!\n");
		_command = InternalCommand_RestartRec;
		_pOnCommand->Give();
		//if(length > 128)
		//	sprintf(error, "OK");
	}
	_pMutex->Give();
}

/***************************************************************
StartRecording
*/
void IPC_AVF_Client::StartRecording(char* error, int length)
{
	_pMutex->Take();	
	if(_currentRecOption == media_streaming)
	{
		_ReadConfigs();
		//_PrepareRecord();
		if (isCircleMode()) {
			if(_CheckVDBSpace() == 1)
			{
				_tryReleaseSpace();
			}
		}
		int state = _CheckVDBSpace();
		if(state <= 1)
		{
			//printf("----no enough space for recording!!, %lld\n", t);
			//printf("----no enough space for recording!!, %lld\n", t);
			// or UI show error here.r
			if(length > 128)
				sprintf(error, "No space for recording!");
			_pMutex->Give();

			if(_pAVFDelegate)
			{
				_pAVFDelegate->onStartRecordError(state==1?
					IPC_AVF_Delegate::recordStartError_tfFull:
					IPC_AVF_Delegate::recordStartError_notf);
			}
			return;
		}
		_PrepareRecord();
		printf("----on start cmd!!\n");
		_command = InternalCommand_record;
		_pOnCommand->Give();
		if(length > 128)
			sprintf(error, "OK"); 
	}
	else if(_currentRecOption == media_recording)
	{
		printf("------on stop cmd!!\n");
		_command = InternalCommand_stopRec;
		_pOnCommand->Give();
		if(length > 128)
			sprintf(error, "OK");
	}	
	_pMutex->Give();
}

/***************************************************************
switchStream
*/
void IPC_AVF_Client::switchStream(bool bBig)
{
	_pMutex->Take();
	if(_bBigStream != bBig)
	{
		_bBigStream = bBig;
		if(_currentRecOption == media_streaming)
		{
			_command1 = InternalCommand_stopRec;
			_pOnCommand->Give();
		}
	}
	_pMutex->Give();
}
/***************************************************************
StopRecording
*/
void IPC_AVF_Client::StopRecording()
{
	_pMutex->Take();
	if(_currentRecOption == media_recording)
	{
		printf("--on stop cmd!!\n");
		_command = InternalCommand_stopRec;
		_pOnCommand->Give();
		
	}
	else if(_currentRecOption == media_streaming)
	{

	}
	_pMutex->Give();
}

/***************************************************************
switchToPlay
*/
void IPC_AVF_Client::switchToPlay(char *Name)
{
	_pMutex->Take();
	_command1 = InternalCommand_switchToPlay;
	memset(_currentMovie, 0, sizeof(_currentMovie));
	memcpy(_currentMovie, Name, strlen(Name));
	_pOnCommand->Give();
	_pMutex->Give();
}


/***************************************************************
CloseRec
*/
int IPC_AVF_Client::CloseRec()
{
	//printf("---close record\n");
	_pMutex->Take();
	_command1 = InternalCommand_closeRec;
	_pOnCommand->Give();
	_pMutex->Give();
	return 0;
}

/***************************************************************
ClosePlay
*/
int IPC_AVF_Client::ClosePlay()
{
	_pMutex->Take();
	_command1 = InternalCommand_ClosePlay;
	_pOnCommand->Give();
	_pMutex->Give();
	return 0;
}

/***************************************************************
Play
*/
int IPC_AVF_Client::Play()
{
	_pMutex->Take();
	_command = InternalCommand_Play;
	_pOnCommand->Give();
	_pMutex->Give();
	return 0;
}

/***************************************************************
PausePlay
*/
int IPC_AVF_Client::PausePlay()
{
	_pMutex->Take();
	_command = InternalCommand_PauseResume;
	_pOnCommand->Give();
	_pMutex->Give();
	return 0;
}


/***************************************************************
StopRecording
*/
int IPC_AVF_Client::getPlayInfor
	(int &length
	,int &currentPosi
	, bool &bPause
	, bool &bStop
	, bool &bError
	, char** file)
{
	avf_playback_media_info_t st;
	memset(&st, 0, sizeof(avf_playback_media_info_t));
	avf_playback_get_media_info(_p_avf_media, &st);
	
	length = st.total_length_ms;
	bPause = st.paused;
	avf_playback_get_play_pos(_p_avf_media, &currentPosi);

	bStop = (_avf_camera_state == AVF_MEDIA_STATE_STOPPED);
	bError = (_avf_camera_state == AVF_MEDIA_STATE_ERROR);

	//_shortName = _currentMovie;
	movieFileNode::getShortName(_shortName, 30, _currentMovie);
	*file = _shortName;
	if((_play_state == PLAY_STATE_playing)&&(bStop || bError))
	{
		_play_state = PLAY_STATE_finish;
		_pMutex->Take();
		_command1 = InternalCommand_ClosePlay;
		_pOnCommand->Give();
		_pMutex->Give();
	} else if((_play_state == PLAY_STATE_finish))
	{
		// get next file
		char* nextfile = SDWatchThread::getInstance()->GetNextFile(_currentMovie);
		if(nextfile != NULL)
		{
			_pMutex->Take();
			_command1 = InternalCommand_playCurrent;
			memset(_currentMovie, 0, sizeof(_currentMovie));
			memcpy(_currentMovie, nextfile, strlen(nextfile));
			_pOnCommand->Give();
			_pMutex->Give();
		}
	}
	return 0;
}

int IPC_AVF_Client::getGpsState()
{
	int result = -2;
	avf_media_get_int32(_p_avf_media, "status.gps", 0, &result, -2);			
	//-2: cannot read; -1: no position; 0: has position
	//printf("---gps state: %d\n", result);

	return result;
}


int IPC_AVF_Client::SyncCaptureNextJpeg(char* path)
{
#ifdef QRScanEnable
	printf("--start new jpeg snap\n");
	avf_camera_save_next_picture(_p_avf_media, path);
	_pCaptureJpegWait->Take(-1);
#endif
	return 0;
}

void IPC_AVF_Client::ShowGPSFullInfor(bool b)
{
	//printf("-- show gps full infor \n");
	int r = b? 1 : 0;
	r = avf_media_set_config_int32(_p_avf_media,"config.subtitle.show-gps", 0, r);
	//printf("-- show gps full infor : %d \n", r);
}

void IPC_AVF_Client::UpdateSubtitleConfig()
{
	//printf("-- update subtitle config\n");
	//"system.ui.timestamp"
	_pMutex->Take();
	_command = InternalCommand_setSubtitleConfig;
	_pOnCommand->Give();
	_pMutex->Give();
	//return 0;
}

void IPC_AVF_Client::EnableDisplay()
{
	printf("EnableDisplay set mode MODE_DECODE\n");
	if (avf_display_set_mode(_p_avf_media, MODE_DECODE) < 0)
	{
		printf("enable display error\n");
	}
}

void IPC_AVF_Client::LCDOnOff(bool b)
{
	int r = b? 1 : 0;
	//printf("---set lcd backlight : %d\n", r);
	avf_display_set_backlight(_p_avf_media, VOUT_0, "t15p00", r);
}

void IPC_AVF_Client::SetCameraName(const char* name)
{
	if(_p_avf_media)
	{
		avf_media_set_config_string(_p_avf_media,"config.subtitle.prefix", 0, name);
	}
}

void IPC_AVF_Client::SetCameraLocation(char* location)
{
	if(_p_avf_media)
	{
		avf_media_set_config_string(_p_avf_media,"config.subtitle.gpsinfo", 0, location);
	}
}

int IPC_AVF_Client::SetMicVolume(int capvol)
{
	int ret_val = -1;
	char *capvol_args[8];
	char capvol_str[AGCMD_PROPERTY_SIZE];

	snprintf(capvol_str, sizeof(capvol_str), "%d", capvol);
	capvol_args[0] = "agboard_helper";
	capvol_args[1] = "--capvol";
	capvol_args[2] = capvol_str;
	capvol_args[3] = NULL;
	ret_val = agcmd_agexecvp(capvol_args[0], capvol_args);
	if (ret_val) {
		AGLOG_ERR("%s: agboard_helper(capvol) = %d.\n",
			__func__, ret_val);
	}
	return ret_val;
}

int IPC_AVF_Client::DefaultMicVolume()
{
	char tmp[256];
	agcmd_property_get(audioGainPropertyName, tmp, "NA");
	if(strcmp(tmp, "NA")==0)
	{
		printf("set mic volume : %d:\n", defaultMicGain);
		SetMicVolume(defaultMicGain);
	}
	return 0;
}

#ifdef SupportNewVDBAVF
void IPC_AVF_Client::GetVDBState
	(bool &ready
	, unsigned int &spaceMB, long long &TotalSpaceMB)
{
	avf_vdb_info_t infor;
	avf_media_get_vdb_info(_p_avf_media, &infor);
	if (isCircleMode())
		ready = infor.vdb_ready&&infor.enought_space_autodel;
	else
		ready = infor.vdb_ready&&infor.enough_space;
	if(ready)
		spaceMB = infor.file_space_mb + infor.disk_space_mb;
	else
		spaceMB = 0;
	TotalSpaceMB = infor.total_space_mb;
	//printf("--total ready : %d, free: %d space : %d\n", ready, spaceMB, TotalSpaceMB);
}

int IPC_AVF_Client::GetStorageFreSpcPcnt()
{
	avf_vdb_info_t infor;
	avf_media_get_vdb_info(_p_avf_media, &infor);
	int space = 0;
	bool ready = infor.vdb_ready&&infor.enough_space;
	if(ready)
	{
		space = (infor.file_space_mb + infor.disk_space_mb)*100 / infor.total_space_mb;
	}
	//printf("---spac: %d, %d, %d,tmp\n",space, infor.file_space_mb + infor.disk_space_mb, infor.total_space_mb);
	return space;
}


#endif

int IPC_AVF_Client::SetWifiMode(int mode, const char* ssid)
{
	char tmp[16];
	agcmd_property_get(WIFI_KEY, tmp, "NA");
	bool bOn = false;
	bool bOff = false;
	switch (mode) {
		case 0://ap
			if(strcmp (tmp, WIFI_AP_KEY) != 0) {
				printf("-- set ap mode\n");
				agcmd_property_set(WIFI_KEY, WIFI_AP_KEY);
				bOn = true;
				bOff = true;
			}
			break;
		case 1://Client
			if(strcmp (tmp, WIFI_CLIENT_KEY) != 0) {
				printf("-- set client mode\n");
				agcmd_property_set(WIFI_KEY, WIFI_CLIENT_KEY);
				bOn = true;
				bOff = true;
			}
			break;
		case 2://OFF
			bOff = true;
			break;
		default:
			break;
	}
	const char *tmp_args[8];
	tmp_args[0] = "agsh";
	tmp_args[1] = "castle";
	// close then open
	if(bOff) {
		//printf("-- stop wifi\n");
		tmp_args[2] = "wifi_stop";
		tmp_args[3] = NULL;
		agbox_sh(3, (const char *const *)tmp_args);
		sleep(1);
	}
	if(bOn) {
		//printf("-- start wifi\n");
		tmp_args[2] = "wifi_start";
		tmp_args[3] = NULL;
		agbox_sh(3, (const char *const *)tmp_args);
	}
	return 0;
}
/***************************************************************
CHdmiWatchThread
*/
CHdmiWatchThread* CHdmiWatchThread::pInstance = NULL;

void CHdmiWatchThread::create()
{
	if(pInstance == NULL)
	{	
		pInstance = new CHdmiWatchThread();
		pInstance->Go();
	}
}

void CHdmiWatchThread::destroy()
{
	if(pInstance != NULL)
	{
		pInstance->Stop();
		delete pInstance;
	}
}

void CHdmiWatchThread::main(void *p)
{
	CHdmiWatchThread::_hdmi_proc = open("/proc/ambarella/vout1_event", O_RDONLY);
	if (_hdmi_proc < 0) {
		printf("--Unable to open vout1_event!\n");
	} else {
		//signal(SIGIO, (sig_t)hdmi_hotplug_handler);
		//fcntl(_hdmi_proc, F_SETOWN, getpid());
		//int flags = fcntl(_hdmi_proc, F_GETFL);
		//fcntl(_hdmi_proc, F_SETFL, flags | FASYNC);
		hdmi_hotplug_handler(0);
	}
}

void CHdmiWatchThread::hdmi_hotplug_handler(int num)
{
	pInstance->handler(num);
}

void CHdmiWatchThread::handler(int num)
{
	struct amb_event			events[256];
	unsigned int				sno;
	int							i, event_num;
	//struct iav_command_message	cmd;
	enum amb_event_type			last_event;
	//int							wait_client;

	while(1)
	{
		event_num = read(_hdmi_proc, events, sizeof(events)) / sizeof(struct amb_event);
		if (event_num > 0)
		{
			sno	= 0;
			last_event	= AMB_EV_NONE;
			for(i = 0; i < event_num; i++ )	{
				if(events[i].sno <= sno) {
					continue;
				} else {
					sno = events[i].sno;
				}

				switch (events[i].type) {
				case AMB_EV_VOUT_HDMI_PLUG:
					printf("---------------hdmid plug in\n");
					break;
				case AMB_EV_VOUT_HDMI_REMOVE:
					printf("---------------hdmid remove\n");
					//last_event = events[i].type;
					break;

				default:
					break;
				}
			}
		}
	}
}



#else
#if 0
/***************************************************************
IPC_VLC_Object
*/

IPC_VLC_Object* IPC_VLC_Object::pInstance = NULL;
void IPC_VLC_Object::vlc_event_callback( int type, void *pdata )
{
	IPC_VLC_Object* vlcObj = (IPC_VLC_Object*)pdata;
	vlcObj->VLCCallBack(type);
}
void IPC_VLC_Object::create(CUpnpSDRecord* service)
{
	if(pInstance == NULL)
	{
		pInstance = new IPC_VLC_Object(service);
		pInstance->Go();
	}
	//return pInstance;
}

IPC_VLC_Object* IPC_VLC_Object::getObject()
{
	if(pInstance == NULL)
	{
		printf("--ipc vlc object not initialized\n");
	}
	return pInstance;
}

void IPC_VLC_Object::destroy()
{	
	if(pInstance != NULL)
	{
		
		pInstance->EndStreamOrRecord();
		pInstance->Stop();
		delete pInstance;
	}
}

IPC_VLC_Object::IPC_VLC_Object
	(
		CUpnpSDRecord* service
	) : CThread("ipc vlc client thread", CThread::NORMAL, 0, NULL)
	,_pUpnp_rec_service(service)
{
	_pRecordStopWait = new CSemaphore(0, 1);
	_pPlayingWait = new CSemaphore(0, 1);
	_pVLCWait = new CSemaphore(0, 1);
	
	if (ipc_init() < 0)
		return;

	_p_vlc_instance = ipc_libvlc_new(0, NULL);
	if (_p_vlc_instance == NULL) {
		printf("ipc_libvlc_new failed\n");
		return;
	}

	printf("call ipc_libvlc_vlm_set_event_callback\n");
	if (ipc_libvlc_vlm_set_event_callback(_p_vlc_instance,
			vlc_event_callback, this) < 0) {
		printf("ipc_libvlc_vlm_set_event_callback failed\n");
		return;
	}	

	ReadConfigs();
}
IPC_VLC_Object::~IPC_VLC_Object()
{
	_bExecuting =false;
	_pOnCommand->Give();

	if (ipc_libvlc_vlm_stop_media(_p_vlc_instance,
			_currentRecOption) < 0) {
		printf("ipc_libvlc_vlm_stop_media failed\n");
	}

	
	if (ipc_libvlc_vlm_set_enabled(_p_vlc_instance,
			_currentRecOption, false) < 0) {
		printf("ipc_libvlc_vlm_set_enabled failed\n");
	}

	IPC_VLC_release_medias();
	

	printf("call ipc_libvlc_vlm_release\n");
	ipc_libvlc_vlm_release(_p_vlc_instance);

	printf("call ipc_libvlc_release\n");
	ipc_libvlc_release(_p_vlc_instance);	

	delete _pRecordStopWait;
	delete _pPlayingWait;
	delete _pVLCWait;

}


void IPC_VLC_Object::VLCCallBack(int type)
{
	switch (type) {
	case VLC_VLM_EventMediaAdded: printf(">>> VLC_VLM_EventMediaAdded\n"); break;
	case VLC_VLM_EventMediaRemoved:
		//printf(">>> VLC_VLM_EventMediaRemoved\n"); 
		_pVLCWait->Give();
		break;
	case VLC_VLM_EventMediaChanged: printf(">>> VLC_VLM_EventMediaChanged\n"); break;
	case VLC_VLM_EventMediaInstanceStarted:
		printf(">>> VLC_VLM_EventMediaInstanceStarted\n");
		
		break;		
	case VLC_VLM_EventMediaInstanceStopped:
		//printf(">>> VLC_VLM_EventMediaInstanceStopped\n");
		//StopedCallBack(); 
		_pRecordStopWait->Give();
		break;
	case VLC_VLM_EventMediaInstanceStatusInit: printf(">>> VLC_VLM_EventMediaInstanceStatusInit\n"); break;
	case VLC_VLM_EventMediaInstanceStatusOpening: printf(">>> VLC_VLM_EventMediaInstanceStatusOpening\n"); break;
	case VLC_VLM_EventMediaInstanceStatusPlaying:
		//printf(">>> VLC_VLM_EventMediaInstanceStatusPlaying\n");
		//PlayingCallBack();
		_pPlayingWait->Give();
		break;

	case VLC_VLM_EventMediaInstanceStatusPause: printf(">>> VLC_VLM_EventMediaInstanceStatusPause\n"); break;
	case VLC_VLM_EventMediaInstanceStatusEnd: printf(">>> VLC_VLM_EventMediaInstanceStatusEnd\n"); break;
	case VLC_VLM_EventMediaInstanceStatusError: printf(">>> VLC_VLM_EventMediaInstanceStatusError\n"); break;

	default: printf("Unknwon event %d\n", type); break;
	}


}


void IPC_VLC_Object::IPC_VLC_release_medias()
{
	if (ipc_libvlc_vlm_del_media(_p_vlc_instance,
			media_streaming) < 0) {
		printf("ipc_libvlc_vlm_del_media failed\n");
	}


	if (ipc_libvlc_vlm_del_media(_p_vlc_instance,
			media_recording) < 0) {
		printf("ipc_libvlc_vlm_del_media failed\n");
	}
}

void IPC_VLC_Object::ReadConfigs()
{
	if(_pUpnp_rec_service)
	{
		_pUpnp_rec_service->_pStateResolution->getResolution
			(_recording_width, _recording_height, _recording_frame);
		
		_pUpnp_rec_service->_pStateQuality->getBitRateAccrodingSize
			(_recording_bitrate, _recording_width*_recording_height, _recording_frame);
		
		_pUpnp_rec_service->_pStateColorMode->SetColorMode();
		_pUpnp_rec_service->_pRecSegLength->GetIframeLen(_seg_iframe_num);
		_pUpnp_rec_service->_pStateColorMode->GetColorMode(_record_color_mode);
	}
	printf("---set resolution: %dx%dx%d, quality:%d,\n", _recording_width, _recording_height,_recording_frame,_recording_bitrate);
}

void IPC_VLC_Object::Update_Recording_Config()
{

	if (ipc_libvlc_vlm_set_enabled(_p_vlc_instance, media_recording, false) < 0)
	{
		printf("ipc_libvlc_vlm_set_enabled failed\n");
	}
	
	if (ipc_libvlc_vlm_del_media(_p_vlc_instance,
			media_recording) < 0) {
		printf("ipc_libvlc_vlm_del_media failed\n");
	}
	
	char input[1024];
	char output[1024];

	this->ReadConfigs();
	
	memset(input, 0, sizeof(input));
	sprintf(input, 
		"ambcam://enc_stream0=%dx%dx%d:enc_h2640=%dx%dx%dx%d"
		":enc_stream1=%dx%dx%d:enc_mjpeg1=%d"
		":audio=%dx%dx%d:misc=%dx%dx%d"
		":vin=%dx%dx%dx%.02fx%d:vout=%dx%dx%dx%.02fx%d",
		1, _recording_width, _recording_height, 0, 1000000, 16000000,_recording_bitrate,
		2, 512, 288, 70,
		2, 44100, 128000,
		3, 67000, _record_color_mode,
		0, 2304, 1296, 29.97, 2,
		3, 720, 480, 59.94, 2);
	
	memset(output, 0, sizeof(input));
	sprintf(output,
		"#agcar{lfs=512,lif=%d,"
		"mux=mp4{moovhead=524288},"
		"dst=/tmp/mmcblk0p1/cc%%d.mp4,access=agfile{wbuffer=512,sync=1},"
		"skip1=1,mux1=mpjpeg,dst1=:%d,access1=http{"
		"mime='multipart/x-mixed-replace;boundary=7b3cc56e5f51db803f790dad720ed50a'},"
		"}",
		_seg_iframe_num,8081);

	if (ipc_libvlc_vlm_add_broadcast(_p_vlc_instance,
			media_recording, input, output, 0, NULL, false, false) < 0)
	{
		printf("ipc_libvlc_vlm_add_broadcast failed\n");
		return;
	}

	if (ipc_libvlc_vlm_set_enabled(_p_vlc_instance, media_recording, true) < 0)
	{
		printf("ipc_libvlc_vlm_set_enabled failed\n");
		return;
	}

}

void IPC_VLC_Object::Update_Streaming_Config()
{

	if (ipc_libvlc_vlm_set_enabled(_p_vlc_instance, media_streaming, false) < 0)
		{
			printf("ipc_libvlc_vlm_set_enabled failed\n");
		}
	
	if (ipc_libvlc_vlm_del_media(_p_vlc_instance,
			media_streaming) < 0) {
		printf("ipc_libvlc_vlm_del_media failed\n");
	}

	int stream_width = 1024;
	int stream_height = 576;
	int stream_quality = 80;
	//int stream_frame = 15;
	this->ReadConfigs();
	char input[1024];
	char output[1024];

	memset(input, 0, sizeof(input));
	sprintf(input, 
		"ambcam://enc_stream0=%dx%dx%d:enc_mjpeg0=%d"
		":audio=%dx%dx%d:misc=%dx%dx%d"
		":vin=%dx%dx%dx%.02fx%d:vout=%dx%dx%dx%.02fx%d",
		2, stream_width, stream_height, stream_quality,
		0, 44100, 128000,
		3, 67000, _record_color_mode,
		0, 2304, 1296, 29.97, 2,
		3, 720, 480, 59.94, 2);
	memset(output, 0, sizeof(input));
	sprintf(output,
		"#agcar{lfs=512,lif=300,"
		"skip=1,mux=mpjpeg,dst=:%d,access=http{"
		"mime='multipart/x-mixed-replace;boundary=7b3cc56e5f51db803f790dad720ed50a'},"
		"}",
		8081);

	
	if (ipc_libvlc_vlm_add_broadcast(_p_vlc_instance,
			media_streaming, input, output, 0, NULL, false, false) < 0) {
		printf("ipc_libvlc_vlm_add_broadcast failed\n");
		return;
	}

	if (ipc_libvlc_vlm_set_enabled(_p_vlc_instance, media_streaming, true) < 0)
	{
		printf("ipc_libvlc_vlm_set_enabled failed\n");
		return;
	}
	
}


void IPC_VLC_Object::main(void *)
{
	_bExecuting = true;
	_pOnCommand = new CSemaphore(0,1);
	_command = InternalCommand_Null;
	_pMutex = new CMutex();
	_currentRecOption = NULL;	

	if(_pUpnp_rec_service != NULL)
	{
		
		if(_pUpnp_rec_service->_pStateRecMode->CarMode())
		{
			startVlcRec();
		}
		else
		{
			startVlcStream();
		}
	}
	
	while(_bExecuting)
	{
		_pOnCommand->Take(-1);
		switch(_command)
		{
			case InternalCommand_Null:
				break;

			case InternalCommand_record:
				{
					_recordingStartTime = time((time_t*)NULL);
					startVlcRec();
				}
				break;

			case InternalCommand_stopRec:
				{	
					startVlcStream();
				}
				break;

			default:
				break;
		}

		_command = InternalCommand_Null;
		
	}
	_pUpnp_rec_service = NULL;
	delete _pMutex;
	delete _pOnCommand;
}

void IPC_VLC_Object::startVlcRec()
{
	printf("---startVlcRec\n");
	_pMutex->Take();
				
	if(_pUpnp_rec_service != NULL)
	{	
		_record_state = CSDRSRecordState::RecordState_starting;
		_pUpnp_rec_service->_pStateRecord->SetState(_record_state);	
	}
	if(_currentRecOption != NULL)
	{
		if (ipc_libvlc_vlm_stop_media(_p_vlc_instance, _currentRecOption) < 0)
		{
			printf("ipc_libvlc_vlm_stop_media failed\n");
		}			
		_pRecordStopWait->Take(-1);
	}
	Update_Recording_Config();					

	if (ipc_libvlc_vlm_play_media(_p_vlc_instance, media_recording) < 0) {
		printf("ipc_libvlc_vlm_play_media failed\n");
		return;
	}
	_currentRecOption = media_recording;	
	_pPlayingWait->Take(-1);
	_recordingStartTime = time((time_t*)NULL);
	//sleep(0);
	if(_pUpnp_rec_service != NULL)
	{
		_record_state = CSDRSRecordState::RecordState_recording;
		_pUpnp_rec_service->_pStateRecord->SetState(_record_state);
	}
	_pMutex->Give();

}


void IPC_VLC_Object::EndStreamOrRecord()
{
	_pMutex->Take();
	if (ipc_libvlc_vlm_stop_media(_p_vlc_instance, _currentRecOption) < 0)
	{
		printf("ipc_libvlc_vlm_stop_media failed\n");
	}
	_pRecordStopWait->Take(-1);
	_pMutex->Give();
	return;
}

void IPC_VLC_Object::startVlcStream()
{
	printf("---startVlcStream\n");
	_pMutex->Take();
	
	if(_pUpnp_rec_service != NULL)
	{
		//printf("---vlc stopping record\n");
		_record_state = CSDRSRecordState::RecordState_stopping;
		_pUpnp_rec_service->_pStateRecord->SetState(_record_state);
	}
	if(_currentRecOption != NULL)
	{
		if (ipc_libvlc_vlm_stop_media(_p_vlc_instance, _currentRecOption) < 0)
		{
			printf("ipc_libvlc_vlm_stop_media failed\n");
		}		
		_pRecordStopWait->Take(-1);
	}
	
	Update_Streaming_Config();

	if (ipc_libvlc_vlm_play_media(_p_vlc_instance, media_streaming) < 0) {
		printf("ipc_libvlc_vlm_play_media failed\n");
		return;
	}
	_currentRecOption = media_streaming;		
	_pPlayingWait->Take(-1);
	//sleep(1);
	if(_pUpnp_rec_service != NULL)
	{
		//printf("---vlc stopped record\n");
		_record_state = CSDRSRecordState::RecordState_stop;
		_pUpnp_rec_service->_pStateRecord->SetState(_record_state);

	}
	_pMutex->Give();
}

UINT64 IPC_VLC_Object::getRecordingTime()
{
	if(_currentRecOption == media_recording)
	{
		time_t currenttime = time(NULL);
		return currenttime - _recordingStartTime;
	}
	else
		return 0;
}
/***************************************************************
GetRecrodState

void IPC_VLC_Object::GetRecrodState	(
	CSDRSRecordState* pState
	)
{
	pState->setWishState(_record_state);
}*/
/***************************************************************
RecordKeyProcess
*/
void IPC_VLC_Object::RecordKeyProcess()
{
	//printf("--car mode : %d, state : %d\n",_pUpnp_rec_service->_pStateRecMode->CarMode(),  _record_state);
	if((_pUpnp_rec_service->_pStateRecMode->CarMode())&&(_record_state != CSDRSRecordState::RecordState_stop))
	{
		SDWatchThread::getInstance()->OnMarkEvent();
	}
	else
	{
		StartRecording();
	}
}


/***************************************************************
StartRecording
*/
void IPC_VLC_Object::StartRecording()
{
	_pMutex->Take();
	if(_currentRecOption == media_streaming)
	{
		printf("----on start cmd!!\n");
		_command = InternalCommand_record;
		_pOnCommand->Give();
		 
	}
	else if(_currentRecOption == media_recording)
	{
		printf("----on stop cmd!!\n");
		_command = InternalCommand_stopRec;
		_pOnCommand->Give();
	}
	_pMutex->Give();
}
/***************************************************************
StopRecording
*/
void IPC_VLC_Object::StopRecording()
{
	if(_currentRecOption == media_recording)
	{
		printf("----on stop cmd!!\n");
		//_pUpnp_rec_service = service;
		_command = InternalCommand_stopRec;
		_pOnCommand->Give();
		
	}
	else if(_currentRecOption == media_recording)
	{

	}
}

void IPC_VLC_Object::switchStream(bool bBig)
{
	
}

#endif
#endif
