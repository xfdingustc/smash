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
#include <stdint.h>
//#include <ambas_iav.h>
//#include <ambas_vout.h>
//#include <ambas_event.h>
#include <linux/input.h>
#include <linux/rtc.h>

#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"
#include "Class_DeviceManager.h"
#include "class_camera_callback.h"
#ifdef ENABLE_LED
#include "LedIndicator.h"
#endif
#ifdef ENABLE_LOUDSPEAKER
#include "AudioPlayer.h"
#endif

#include "class_ipc_rec.h"

extern void getStoragePropertyName(char *prop, int len);

#define media_recording 0 //"recordWithStream"
#define media_streaming 1 //"streamOnly"
#define media_NULL -1

// used to recovery state if service_upnp crashes
#define PROPERTY_RecStartTime "temp.recording.start_time"
#define PROPERTY_RecState     "temp.recording.state"

#if defined(PLATFORMHELLFIRE)
#define MIRROR_ORIGIN "0:0"
#define MIRROR_180    "1:1"
#elif defined(PLATFORMDIABLO) || defined(PLATFORMHACHI)
#define MIRROR_ORIGIN "1:1"
#define MIRROR_180    "0:0"
#endif

#define VALUE_DISK_MONITOR      "0:60000,1000,5000" // method:period,timeout,total_timeout

#define VIDEO_VIN_IMX178        "3072x1728"
#define VIDEO_VIN_IMX178_4_3    "2592x1944"
#define VIDEO_VIN_IMX124        "2048x1536"

#define kEnoughSpace       (300*1000*1000)
#define kEnoughSpace_1G    (1000*1000*1000)
#define kEnoughSpace_2G    (2000*1000*1000)
#define kMinSpace          (150*1000*1000)
#define kQuotaSpace        (300*1000*1000)
#define RecordThreshold    300
#define kEnoughSpace_Mark  (4LL*1000*1000*1000)

static const char *WriteSlowTimer = "WriteSlowTimer";
static const char *PhotoLapse_Timer_Name = "photo lapse timer";

bool is_no_write_record()
{
    char tmp[256] = {0x00};
    agcmd_property_get(PropName_No_Write_Record, tmp, No_Write_Record_Disabled);
    if (strcasecmp(tmp, No_Write_Record_Enalbed) == 0) {
        return true;
    } else {
        return false;
    }
}

UINT64 GetCurrentTick()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000;
}

static int agboard_helper_open_rtc(int flags)
{
    int ret_val;

    ret_val = open("/dev/rtc", flags);
    if (ret_val >= 0) {
        goto agboard_helper_open_rtc_exit;
    }
    ret_val = open("/dev/rtc0", flags);
    if (ret_val >= 0) {
        goto agboard_helper_open_rtc_exit;
    }
    ret_val = open("/dev/misc/rtc", flags);
    if (ret_val >= 0) {
        goto agboard_helper_open_rtc_exit;
    }

agboard_helper_open_rtc_exit:
    return ret_val;
}

static int agboard_helper_set_alarm(uint32_t alarm_delay)
{
    int ret_val = 0;
    int rtc_fd;
    struct rtc_time rtc_tm;
    time_t rtc_time;
    time_t alarm_time;
    struct rtc_wkalrm rtc_alarm;
    char tm_buf[256];

    rtc_fd = agboard_helper_open_rtc(O_RDONLY);
    if (rtc_fd < 0) {
        ret_val = rtc_fd;
        AGLOG_ERR("%s: agboard_helper_open_rtc() = %d\n",
            __func__, ret_val);
        goto agboard_helper_set_alarm_exit;
    }

    memset(&rtc_tm, 0x00, sizeof(rtc_tm));
    ret_val = ioctl(rtc_fd, RTC_RD_TIME, &rtc_tm);
    if (ret_val < 0) {
        AGLOG_ERR("%s: RTC_RD_TIME = %d\n", __func__, ret_val);
        goto agboard_helper_set_alarm_close;
    }
    rtc_time = timegm((struct tm *)&rtc_tm);
    alarm_time = (rtc_time + alarm_delay);

    rtc_alarm.enabled = 1;
    rtc_alarm.pending = 0;
    gmtime_r(&alarm_time, (struct tm *)&rtc_alarm.time);
    ret_val = ioctl(rtc_fd, RTC_WKALM_SET, &rtc_alarm);
    if (ret_val < 0) {
        AGLOG_ERR("%s: RTC_WKALM_SET = %d\n", __func__, ret_val);
        //goto agboard_helper_set_alarm_close;
    }

    ret_val = ioctl(rtc_fd, RTC_WKALM_RD, &rtc_alarm);
    if (ret_val < 0) {
        AGLOG_ERR("%s: RTC_WKALM_RD = %d\n", __func__, ret_val);
        goto agboard_helper_set_alarm_close;
    }
    strftime(tm_buf, sizeof(tm_buf), "%a %b %e %H:%M:%S %Z %Y",
        (struct tm *)&rtc_alarm.time);
    AGLOG_NOTICE("%s(%u) = %s\n", __func__, alarm_delay, tm_buf);

agboard_helper_set_alarm_close:
    close(rtc_fd);
agboard_helper_set_alarm_exit:
    return ret_val;
}


/***************************************************************
static IPC_AVF_Client::create
*/
IPC_AVF_Client* IPC_AVF_Client::pInstance = NULL;
bool IPC_AVF_Client::_bExecuting = false;

void IPC_AVF_Client::create()
{
    if (pInstance == NULL) {
        //CAMERALOG("--IPC_AVF_Client::initialize");
        //CHdmiWatchThread::create();
        pInstance = new IPC_AVF_Client();
        pInstance->Go();
    }
}

/***************************************************************
static IPC_AVF_Client::getObject
*/
IPC_AVF_Client* IPC_AVF_Client::getObject()
{
    if (pInstance == NULL) {
        CAMERALOG("--ipc avf object not initialized");
    }
    return pInstance;
}

/***************************************************************
static IPC_AVF_Client::destroy
*/
void IPC_AVF_Client::destroy()
{
    if (pInstance != NULL) {
        CAMERALOG("--++--IPC_AVF_Client::destroy");
        //CHdmiWatchThread::destroy();
        pInstance->Stop();
        delete pInstance;
        pInstance = NULL;
        CAMERALOG("--++--IPC_AVF_Client::destroyed");
    }
}

// timer callback
void IPC_AVF_Client::onTimeout(void *para)
{
    IPC_AVF_Client *client = (IPC_AVF_Client *)para;
    if (client) {
        if ((State_Rec_recording == client->_record_state) &&
            (Storage_Rec_State_DiskError != client->_storage_rec_state)) {
            client->_storage_rec_state = Storage_Rec_State_Normal;
        }
        if (client->_pCameraCallback) {
            client->_pCameraCallback->onCameraState(
                CameraCallback::CameraState_recording);
        }
    }
}


/***************************************************************
static IPC_AVF_Client::avf_event_callback
*/
void IPC_AVF_Client::avf_event_callback( int type, void *pdata )
{
    //CAMERALOG("--avf_event_callback:%d, %d", type, _bExecuting);
    if (!_bExecuting) {
        return;
    }
    if (IPC_AVF_Client::getObject()) {
        IPC_AVF_Client::getObject()->AVFCameraCallback(type);
    }
}

/***************************************************************
private IPC_AVF_Client::AVFCameraCallback
*/
void IPC_AVF_Client::AVFCameraCallback(int type)
{
    if (type == AVF_CAMERA_EventStatusChanged) {
        int state = avf_media_get_state(_p_avf_media);
        if (state == _avf_camera_state) {
            return;
        }

        switch (state) {
            case AVF_MEDIA_STATE_IDLE:
                // pause;
                break;
            case AVF_MEDIA_STATE_STOPPED:
                break;

            case AVF_MEDIA_STATE_RUNNING:
                break;

            case AVF_MEDIA_STATE_ERROR:
                CAMERALOG("AVF_MEDIA_STATE_ERROR!!!");
                // reset command
                _pMutex->Take();
                _queuePriorityCmd(InternalCommand_errorProcess);
                _pOnCommand->Give();
                _pMutex->Give();
                break;
            default:
                break;
        }
        _avf_camera_state = state;
    } else if (type == AVF_CAMERA_EventFileEnd) {
        CAMERALOG("--file end");
#ifdef XMPPENABLE
        char fileName[256];
        avf_media_get_string(_p_avf_media, "state.file.previous", 1, fileName,256 ,"");
        //if(strstr(fileName, "/xmpp/") > 0)
        //{
            if (_pAVFDelegate) {
                //CAMERALOG("stream file ready : %s", fileName);
                _pAVFDelegate->onNewFileReady(fileName);
            } else {
                //CAMERALOG("remove : %s", fileName);
                remove(fileName);
            }
        //}
#endif
    } else if (type == AVF_CAMERA_EventFileStart) {
        CAMERALOG("--file new");
    }
#ifdef WriteBufferOverFlowEnable
    else if (type == AVF_CAMERA_EventBufferOverRun) {
        _pMutex->Take();
        _queuePriorityCmd(InternalCommand_fileBufferWarning);
        _pOnCommand->Give();
        _pMutex->Give();
    } else if (type == AVF_CAMERA_EventBufferOverflow) {
        _pMutex->Take();
        _queuePriorityCmd(InternalCommand_fileBufferError);
        _pOnCommand->Give();
        _pMutex->Give();
    }
#endif
    else if (type == AVF_CAMERA_EventDiskError) {
        CAMERALOG("---Disk Error event, stop record.");
        _pMutex->Take();
        _queuePriorityCmd(InternalCommand_DiskError);
        _pOnCommand->Give();
        _pMutex->Give();
#ifdef PLATFORMHACHI
    } else if (type == AVF_CAMERA_EventDiskSlow) {
        time_t now;
        time(&now);
        CAMERALOG("---Disk slow event happens on %s", ctime(&now));
        _pMutex->Take();
        _queuePriorityCmd(InternalCommand_WriteSlow);
        _pOnCommand->Give();
        _pMutex->Give();
#endif
    } else if(type == AVF_CAMERA_EventBufferSpaceFull) {
        CAMERALOG("---space full event, stop record.");
        _pMutex->Take();
        _queuePriorityCmd(InternalCommand_SpaceFull);
        _pOnCommand->Give();
        _pMutex->Give();
    } else if(type == AVF_CAMERA_EventVdbStateChanged) {
        CAMERALOG("---vdb state changed event.");
        _pMutex->Take();
        _queuePriorityCmd(InternalCommand_VdbStateChanged);
        _pOnCommand->Give();
        _pMutex->Give();
    }
#ifdef ENABLE_STILL_CAPTURE
    else if (type == AVF_CAMERA_EventStillCaptureStateChanged) {
        _pMutex->Take();
        _queuePriorityCmd(InternalCommand_MSG_StillCaptureStateChanged);
        _pOnCommand->Give();
        _pMutex->Give();
    } else if (type == AVF_CAMERA_EventPictureTaken) {
        _pMutex->Take();
        _queuePriorityCmd(InternalCommand_MSG_PictureTaken);
        _pOnCommand->Give();
        _pMutex->Give();
    } else if (type == AVF_CAMERA_EventPictureListChanged) {
        _pMutex->Take();
        _queuePriorityCmd(InternalCommand_MSG_PictureListChanged);
        _pOnCommand->Give();
        _pMutex->Give();
    }
#endif
}

/***************************************************************
private IPC_AVF_Client::IPC_AVF_Client
*/
IPC_AVF_Client::IPC_AVF_Client()
    : CThread("ipc_AVF_client_thread")
#ifdef ENABLE_STILL_CAPTURE
    , _bStillMode(false)
    , _bInPhotoLapse(false)
    , _bNeedRTCWakeup(false)
    , _timeoutSec(0)
    , _totalPicNum(0)
#endif
    , _pCameraCallback(NULL)
    , _p_avf_media(NULL)
    , _recording_frame(-1)
    , _record_state(State_Rec_close)
    , _bBigStream(true)
    , _avfMode(AVF_Mode_NULL)
    , _queueNum(0)
    , _queueIndex(0)
    , _priQueNum(0)
    , _priQueIndex(0)
    , _hdmiOn(false)
    , _bWaitTFReady(false)
    , _bSetOBDReady(false)
    , _pAVFDelegate(NULL)
    , _lcdOn(false)
    , _numClients(0)
    , _curMode(CAM_MODE_VIDEO)
    , _targetMode(CAM_MODE_VIDEO)
    , _lastWriteSlowTime(0)
    , _storage_rec_state(Storage_Rec_State_Normal)
    , _bVideoFullScreen(false)
{
    _pOnCommand = new CSemaphore(0, 1, "ipc avf comand sem");
    _pMutex = new CMutex("avf mutex");

    if (ipc_init() < 0) {
        CAMERALOG("---ipc init failed");
        return;
    }

    _avf_camera_state = AVF_MEDIA_STATE_STOPPED;
    _p_avf_media = avf_media_create(IPC_AVF_Client::avf_event_callback, this);
    _currentRecOption = media_NULL;

    avf_media_set_config_string(_p_avf_media, "config.diskmonitor.params",
        AVF_GLOBAL_CONFIG, VALUE_DISK_MONITOR);

#if defined(PLATFORMHACHI)
    _config_vout();
#endif

    // init picture list service
    char tmp[256];
    agcmd_property_get(PropName_Media_dir, tmp, "");
    char dirname[256];
    snprintf(dirname, sizeof(dirname), "%s/DCIM/", tmp);
    avf_media_enable_picture_service(_p_avf_media, dirname);

    _avf_camera_state = avf_media_get_state(_p_avf_media);
    CAMERALOG("mediaserver state is %d when service_upnp boot up", _avf_camera_state);

    memset (tmp, 0x00, sizeof(tmp));
    agcmd_property_get(PROPERTY_RecState, tmp, "");
#ifdef ENABLE_STILL_CAPTURE
    if (strcasecmp(tmp, "photo_timelapse") == 0) {
        _curMode =  _targetMode = CAM_MODE_PHOTO_LAPSE;
        memset(tmp, 0x00, sizeof(tmp));
        agcmd_property_get(PROPERTY_RecStartTime, tmp, "0");
        struct timeval tv;
        gettimeofday(&tv, NULL);
        _timeoutSec = (int)(atoll(tmp) - tv.tv_sec);
        if (_timeoutSec < 10) {
            _timeoutSec = 10;
        }
        CAMERALOG("%ds left for next photo timelapse shot", _timeoutSec);
        setWorkingMode(CAM_MODE_PHOTO_LAPSE);
        _bInPhotoLapse = true;
        _record_state = State_Rec_PhotoLapse;
        _bNeedRTCWakeup = true;
        CTimerThread::GetInstance()->ApplyTimer(
            // 1s
            10, onPhotoLapseTimerEvent, this, true, PhotoLapse_Timer_Name);
    } else
#endif
    if (_avf_camera_state == AVF_MEDIA_STATE_RUNNING) {
        if (strcasecmp(tmp, "media_recording") == 0) {
            _currentRecOption = media_recording;
            _avfMode = AVF_Mode_Encode;
        } else if (strcasecmp(tmp, "media_streaming") == 0) {
            _record_state = State_Rec_stoped;
            _currentRecOption = media_streaming;
            _avfMode = AVF_Mode_Encode;
        }

        if (_currentRecOption == media_recording) {
            // if service_upnp is restarted after crash and was recording,
            // recovery previous status
            CAMERALOG("service_upnp is restarted after crash, recovery previous status");
            _currentRecOption = media_recording;
            _avfMode = AVF_Mode_Encode;
            _record_state = State_Rec_recording;
            memset(tmp, 0x00, sizeof(tmp));
            agcmd_property_get(PROPERTY_RecStartTime, tmp, "0");
            _recordingStartTime = atoll(tmp);
        }
    } else {
#if defined(PLATFORMHACHI)
        // for waylens hachi, check mount type to start recording on boot up
        switchToRec(false);
#else
        // auto record in car mode after boot
        switchToRec(true);
#endif
    }
}

/***************************************************************
private IPC_AVF_Client::~IPC_AVF_Client
*/
IPC_AVF_Client::~IPC_AVF_Client()
{
    _bExecuting =false;
    _queueIndex = 0;
    _queueNum = 0;
    _priQueIndex = 0;
    _priQueNum = 0;
    _pOnCommand->Give();
    if (_avfMode == AVF_Mode_Decode) {

    } else if(_avfMode == AVF_Mode_Encode) {

    }
    delete _pMutex;
    delete _pOnCommand;

    int rval;
    if ((rval = avf_camera_close(_p_avf_media, true)) < 0) {
        CAMERALOG("avf_camera_close() failed %d", rval);
        // TODO: error handling
    }
    avf_media_destroy(_p_avf_media);

    CAMERALOG("IPC_AVF_Client is destroyed");
}

void IPC_AVF_Client::_onAvf_Error_process()
{
    _record_state = State_Rec_error;
    if (_avfMode == AVF_Mode_Decode) {
        CAMERALOG("error in decode mode, should reset decode");
    } else if(_avfMode == AVF_Mode_Encode) {
        CAMERALOG("error in encode mode, should reset record");
    } else {
        CAMERALOG("error from avf");
    }
    if (_pAVFDelegate) {
        _pAVFDelegate->onRecordStateChange(State_Rec_error);
    }
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(CameraCallback::CameraState_error);
    }
}

int IPC_AVF_Client::_config_vout()
{
#if defined(PLATFORMHACHI)

    avf_vout_info_t info;
    memset(&info, 0x00, sizeof(info));
    int result = avf_display_get_vout_info(_p_avf_media, VOUT_0, VOUT_TYPE_MIPI, &info);
    CAMERALOG("result = %d, vout info: enabled = %d, vout_width = %d, vout_height = %d, "
        "video_width = %d, video_height = %d, video_xoff = %d, video_yoff = %d",
        result, info.enabled, info.vout_width, info.vout_height,
        info.video_width, info.video_height, info.video_xoff, info.video_yoff);
    bool renewVout = false;
    if (result != 0) {
        renewVout = true;
    } else {
        if (!info.enabled) {
            renewVout = true;
        } else {
            if ((info.video_width == 400)
                && (info.video_height == 400)
                && (info.video_xoff == 0)
                && (info.video_yoff == 0))
            {
                _bVideoFullScreen = true;
            }
        }
    }
    if (renewVout) {
        // TODO: only for waylens
        vout_config_t config;
        memset(&config, 0x00, sizeof(config));
        config.vout_id = VOUT_0;
        config.vout_type = VOUT_TYPE_MIPI;
        config.vout_mode = VOUT_MODE_D400x400;
        config.enable_video = 1;
        config.fb_id = 0;
        config.disable_csc = 0;
        config.enable_osd_copy = 0;

        config.video_size_flag = 1;
        config.video_width = 400;
        config.video_height = 224;
        config.video_offset_flag = 1;
        config.video_offset_x = 0;
        config.video_offset_y = 88;

        int ret = avf_display_set_vout(_p_avf_media, &config, NULL);
        CAMERALOG("avf display set vout with ret = %d", ret);
    }

    int ret_val = 0;
    if (_isRotate180()) {
        ret_val = avf_display_flip_vout_osd(_p_avf_media,
            VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_HV);
        if (ret_val == 0) {
            ret_val = avf_display_flip_vout_video(_p_avf_media,
                VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_HV);
        }
    } else {
        ret_val = avf_display_flip_vout_osd(_p_avf_media,
            VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_NORMAL);
        if (ret_val == 0) {
            ret_val = avf_display_flip_vout_video(_p_avf_media,
                VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_NORMAL);
        }
    }
#endif

    return 0;
}

int IPC_AVF_Client::_config_lcd_vout(bool bON)
{
    vout_config_t config;
    //CAMERALOG("avf_display_set_mode set mode idle");
    if (avf_display_set_mode(_p_avf_media, MODE_IDLE) < 0) {
        return -1;
    }

    // init LCD
    if (bON) {
        if(_lcdOn) {
            return 0;
        }
            memset(&config, 0x00, sizeof(config));
            config.vout_id = VOUT_0;
            config.vout_type = VOUT_TYPE_DIGITAL;
            config.vout_mode = VOUT_MODE_D320x240;
            config.enable_video = 1;
            config.fb_id = 0;
        if (avf_display_set_vout(_p_avf_media, &config, "t15p00") < 0) {
            return -1;
        }

        //CAMERALOG("avf_display_set_mode set mode MODE_DECODE");
        //if (avf_display_set_mode(_p_avf_media, MODE_DECODE) < 0)
        //	return -1;
        _lcdOn = true;
        return 0;
    } else {
        if (!_lcdOn) {
            return 0;
        }
        avf_display_disable_vout(_p_avf_media, VOUT_0);
        _lcdOn = false;
    }
    return 0;
}

int IPC_AVF_Client::_config_hdmi_vout(bool on, bool fb, bool small)
{
    vout_config_t config;
    //CAMERALOG("_config_hdmi_vout avf_display_set_mode set mode MODE_IDLE");
    //if (avf_display_set_mode(_p_avf_media, MODE_IDLE) < 0)
    //	return -1;

    if (on) {
        if (_hdmiOn) {
            return 0;
        }
        memset(&config, 0x00, sizeof(config));
        config.vout_id = VOUT_1;
        config.vout_type = VOUT_TYPE_HDMI;
        if (small) {
            config.vout_mode = VOUT_MODE_480p;
        } else {
            config.vout_mode = VOUT_MODE_1080i;
        }
        config.enable_video = 1;
        if (fb) {
            config.fb_id = 0;
        } else {
         config.fb_id = -1;
        }
        if (avf_display_set_vout(_p_avf_media, &config, "") < 0) {
            return -1;
        } else {
            _hdmiOn = true;
        }
    } else {
        if (!_hdmiOn) {
            return 0;
        }
        avf_display_disable_vout(_p_avf_media, VOUT_1);
        _hdmiOn = false;
    }
    return 0;
}

/***************************************************************
private  _switchToRec
*/
void IPC_AVF_Client::_switchToRec(bool forceStartRec)
{
    CAMERALOG("_switchToRec enter");
    if (_avfMode == AVF_Mode_NULL) {
        if (_lcdOn) {
            _config_hdmi_vout(false, false, false);
        }
    } else if (_avfMode == AVF_Mode_Decode) {
        if (_lcdOn) {
            _config_hdmi_vout(false, false, false);
        }
    } else {
        // do nothing
    }

    if (is_no_write_record()) {
        _bWaitTFReady = false;
        CAMERALOG("--- start rec");
        _startAVFRec();
        CAMERALOG("_switchToRec exit");
        return;
    }

    _bWaitTFReady = false;
    //CAMERALOG("isAutoStartRec() = %d, forceStartRec = %d, _bCheckAutoRec = %d",
    //    isAutoStartRec(), forceStartRec, _bCheckAutoRec);
    if ((isAutoStartRec() || forceStartRec) && (_bCheckAutoRec)) {
        unsigned int freesize;
        long long totalspace;
        bool bReady = false;
        GetVDBState(bReady, freesize, totalspace);
#if !defined(PLATFORMHACHI)
        // Try release space firstly
        if (bReady && (freesize <= RecordThreshold) && isCircleMode()) {
            _tryReleaseSpace();
            GetVDBState(bReady, freesize, totalspace);
        }
#endif
#ifdef PLATFORMHACHI
        if (bReady) {
#else
        if (bReady && (freesize > RecordThreshold)) {
#endif
            CAMERALOG("--- start rec");
            _startAVFRec();
        } else {
            CAMERALOG("--- start stream, wait tf ready");
            _startAVFStream();
            _bWaitTFReady = true;
        }
    } else {
        CAMERALOG("--- start stream");
        _startAVFStream();
    }
    CAMERALOG("_switchToRec exit");
}

bool IPC_AVF_Client::_tryReleaseSpace()
{
    avf_vdb_info_t infor;
    avf_media_free_vdb_space(_p_avf_media, &infor);
    return true;
}

void IPC_AVF_Client::_vdbStateChanged()
{
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(CameraCallback::CameraState_storage_changed);
    }

    unsigned int freesize;
    long long totalspace;
    bool b = false;
    GetVDBState(b, freesize, totalspace);
    CAMERALOG("_vdbStateChanged(), return %d", b);
    if (b) {
        if (_bWaitTFReady) {
            _bWaitTFReady = false;
            _switchToRec(false);
        }
        if (CTSDeviceManager::getInstance()) {
            CTSDeviceManager::getInstance()->broadcastStorageState();
        }
    }
}

void IPC_AVF_Client::_tfReady()
{
    CAMERALOG("tf ready enable avf");
    avf_media_storage_ready(_p_avf_media, 1);
}

void IPC_AVF_Client::_tfRemove()
{
    if (_record_state != State_Rec_close) {
        CAMERALOG("tf remove disable avf");
        avf_media_storage_ready(_p_avf_media, 0);
        if (_currentRecOption == media_recording){
            // restore recording when TF re-plugged
            _startAVFStream();
            _bWaitTFReady = true;
        }
    }
}

void IPC_AVF_Client::_OBDReady(bool b)
{
    if (b == _bSetOBDReady) {
        return;
    }
    _bSetOBDReady = b;
    avf_media_set_config_bool(_p_avf_media,"config.obd.enable", 0, b?1:0);
}

/***************************************************************
private IPC_AVF_Client::ReadConfigs
*/
void IPC_AVF_Client::_ReadConfigs()
{
    if (CAM_MODE_SLOW_MOTION == _targetMode) {
        char tmp_prop[AGCMD_PROPERTY_SIZE];
        agcmd_property_get(prebuild_board_vin0, tmp_prop, "imx178");
        if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
            _recording_width = 1280;
            _recording_height = 720;
            _recording_frame = 120;
            _recording_AR = AR_16_9;
            _recording_clock_mode = "720p120";
        } else {
            _recording_width = 1280;
            _recording_height = 720;
            _recording_frame = 60;
            _recording_AR = AR_16_9;
            _recording_clock_mode = "720p60";
        }
        CAMERALOG("---resolution: %dx%dx%d, quality:%d",
            _recording_width, _recording_height,
            _recording_frame, _recording_bitrate);
        return;
    }

    //CAMERALOG("_ReadConfigs --bitrate read trace");
    int oldframerate = _recording_frame;
    getResolution(
        _recording_width, _recording_height, _recording_frame,
        _recording_AR, _recording_clock_mode);
    _recording_bitrate = getBitrate();
    _seg_iframe_num = getSegSeconds();
    _record_color_mode = getColorMode();
    if ((oldframerate > 0) && (oldframerate != _recording_frame)) {
        //CAMERALOG("---> Vin Framerate Changed: %d-> %d", oldframerate, _recording_frame);
    }
    //CAMERALOG("---resolution: %dx%dx%d, quality:%d", _recording_width, _recording_height,
    //    _recording_frame, _recording_bitrate);
}

int IPC_AVF_Client::_configVin() {
    char tmp_prop[AGCMD_PROPERTY_SIZE];
    agcmd_property_get("temp.earlyboot.insane", tmp_prop, "0");
    int insane_mode = strtoul(tmp_prop, NULL, 0);
#ifdef ARCH_S2L
    agcmd_property_get(prebuild_board_vin0, tmp_prop, "imx178");
    int enc_mode = 4;
#else//ARCH_A5S
    agcmd_property_get(prebuild_board_vin0, tmp_prop, "mt9t002");
    int enc_mode = 0;
#endif
    //CAMERALOG("prop = %s", tmp_prop);
    CAMERALOG("insane_mode = %d", insane_mode);

    if (_avfMode == AVF_Mode_NULL) {
        int rval = avf_camera_set_media_source(_p_avf_media, "", "/usr/local/bin/camera.conf");
        if (rval < 0) {
            CAMERALOG("avf_camera_set_media_source() failed %d", rval);
            //return -1;
        }
        _avfMode = AVF_Mode_Encode;
        if (_pAVFDelegate) {
            _pAVFDelegate->onCameraModeChange();
        }
    }

    // video_mode:fps:mirror_horz:mirror_vert:anti_flicker;bits:hdr_mode:enc_mode
#ifdef ARCH_S2L
    if (_bStillMode) {
        if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
            avf_media_set_config_string(_p_avf_media, "config.vin", 0,
                "3072x2048:20:0:0:0;14:0:4");
        } else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
            avf_media_set_config_string(_p_avf_media, "config.vin", 0,
                "2048x1536:30:0:0:0;12:0:4");
        } else {
            CAMERALOG("Do not support sensor: %s for still capture", tmp_prop);
            return -1;
        }
    } else
#endif
    {
        bool bSupport = true;
        char video_mode_fps[64] = {0x00};
        int bits = 0;

        if (_recording_frame == 120) {
            if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
                snprintf(video_mode_fps, 64, "%s", "720p:120");
                bits = 12;
                enc_mode = 0;
            } else {
                CAMERALOG("Do not support sensor: %s for 120fps", tmp_prop);
                bSupport = false;
            }
        } else if (_recording_frame == 60) {

// a5s: 60 is actually 59.94
#ifdef ARCH_S2L
            if (_recording_AR == AR_4_3) {
                if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
                    snprintf(video_mode_fps, 64, "%s", VIDEO_VIN_IMX178_4_3":60");
                    bits = 12;
                } else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
                    snprintf(video_mode_fps, 64, "%s", VIDEO_VIN_IMX124":60");
                    bits = 12;
                } else {
                    CAMERALOG("Do not support sensor: %s for 60fps", tmp_prop);
                    bSupport = false;
                }
            } else {
                if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
                    if (_recording_width == 1280 && _recording_height == 720) {
                        snprintf(video_mode_fps, 64, "%s", "720p:60");
                    } else {
                        snprintf(video_mode_fps, 64, "%s", VIDEO_VIN_IMX178":60");
                        if (insane_mode == 1) {
                            enc_mode = 4;
                        } else {
                            enc_mode = 0;
                        }
                    }
                    bits = 12;
                } else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
                    snprintf(video_mode_fps, 64, "%s", "1920x1080:60");
                    bits = 12;
                } else {
                    CAMERALOG("Do not support sensor: %s for 60fps", tmp_prop);
                    bSupport = false;
                }
            }
#else
            snprintf(video_mode_fps, 64, "%s", "1920x1080:59.94");
            bits = 10;
#endif

        } else if (_recording_frame == 30) {

// a5s: 30 is actually 29.97
#ifdef ARCH_S2L
            if (_recording_AR == AR_4_3) {
                if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
                    snprintf(video_mode_fps, 64, "%s", VIDEO_VIN_IMX178_4_3":30");
                    bits = 14;
                } else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
                    snprintf(video_mode_fps, 64, "%s", VIDEO_VIN_IMX124":30");
                    bits = 12;
                } else {
                    CAMERALOG("Do not support sensor: %s for 30fps", tmp_prop);
                    bSupport = false;
                }
            } else {
                if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
                    if (_recording_width == 1280 && _recording_height == 720) {
                        snprintf(video_mode_fps, 64, "%s", "720p:30");
                    } else {
                        snprintf(video_mode_fps, 64, "%s", VIDEO_VIN_IMX178":30");
                    }
                    bits = 12;
                } else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
                    snprintf(video_mode_fps, 64, "%s", "1920x1080:30");
                    bits = 12;
                } else {
                    CAMERALOG("Do not support sensor: %s for 30fps", tmp_prop);
                    bSupport = false;
                }
            }
#else
            snprintf(video_mode_fps, 64, "%s", "auto:29.97");
            bits = 14;

#endif

        } else if (_recording_frame == 24) {
            snprintf(video_mode_fps, sizeof(video_mode_fps), "%s", "3072x1728:23.976");
            bits = 12;
        } else {
            CAMERALOG("unknown framerate %d", _recording_frame);
            bSupport = false;
        }

        if (bSupport) {
            char horizontal[256];
            char vertical[256];
            memset(horizontal, 0x00, 256);
            memset(vertical, 0x00, 256);
            agcmd_property_get(PropName_Rotate_Horizontal, horizontal, "0");
            agcmd_property_get(PropName_Rotate_Vertical, vertical, "0");

            const char *mirror = NULL;
            char vin_config[128] = {0x00};
            if ((strcasecmp(horizontal, "0") == 0)
                && (strcasecmp(vertical, "0") == 0))
            {
                mirror = MIRROR_ORIGIN;
#ifdef PLATFORMHACHI
                char vBoard[32];
                agcmd_property_get(prebuild_board_version, vBoard, "V0B");
                //CAMERALOG("board version %s", vBoard);
                if (strcasecmp(vBoard, "V0B") == 0) {
                    mirror = MIRROR_180;
                }
#endif
            } else if ((strcasecmp(horizontal, "1") == 0)
                && (strcasecmp(vertical, "1") == 0))
            {
                mirror = MIRROR_180;
#ifdef PLATFORMHACHI
                char vBoard[32];
                agcmd_property_get(prebuild_board_version, vBoard, "V0B");
                CAMERALOG("board version %s", vBoard);
                if (strcasecmp(vBoard, "V0B") == 0) {
                    mirror = MIRROR_ORIGIN;
                }
#endif
            }
            // video_mode:fps:mirror_horz:mirror_vert:anti_flicker;bits:hdr_mode:enc_mode
            snprintf(vin_config, 128, "%s:%s:0;%d:0:%d",
                video_mode_fps, mirror, bits, enc_mode);
            CAMERALOG("config.vin = %s", vin_config);
            avf_media_set_config_string(_p_avf_media, "config.vin", 0, vin_config);
            return 0;
        } else {
            return -1;
        }
    }

    return 0;
}

int IPC_AVF_Client::_setOverlay() {
#ifdef ENABLE_STILL_CAPTURE
    if (_bStillMode) {
        avf_media_set_config_string(_p_avf_media, "config.video.overlay", 0, "");
        avf_media_set_config_string(_p_avf_media, "config.video.overlay", 1, "");
        avf_media_set_config_string(_p_avf_media, "config.video.overlay", 2, "");
    } else
#endif
    {
        const int default_font_height = 36;
        const int default_row_height = 44;
        char input[256];
        int font_height = default_font_height * _recording_width / 1920;
        int row_height = default_row_height * _recording_width/ 1920;
        memset(input, 0x00, sizeof(input));
        snprintf(input, 256, "1:0,0,0,%d:%d", row_height, font_height);
        avf_media_set_config_string(_p_avf_media, "config.video.overlay", 0, input);

#ifdef ARCH_S2L
        int preview_width = big_stream_width;
        //int preview_height = big_stream_height;
#else
        int preview_width = small_stream_width;
        //int preview_height = small_stream_height;
#endif
        font_height = default_font_height * preview_width / 1920;
        row_height = default_row_height * preview_width / 1920;
        memset(input, 0x00, sizeof(input));
        snprintf(input, 256, "1:0,0,0,%d:%d", row_height, font_height);
        avf_media_set_config_string(_p_avf_media, "config.video.overlay", 1, input);

#ifdef ARCH_S2L
        int second_width = 640;
        //int second_height = 360;
#else
        int second_width = 512;
        //int second_height = 288;
#endif
        font_height = default_font_height * second_width / 1920;
        row_height = default_row_height * second_width / 1920;
        memset(input, 0x00, sizeof(input));
        snprintf(input, 256, "1:0,0,0,%d:%d", row_height, font_height);
        avf_media_set_config_string(_p_avf_media, "config.video.overlay", 2, input);

        _updateSubtitleConfig();
    }
    return 0;
}

int IPC_AVF_Client::_get_record_flag(bool bRecord)
{
    if (bRecord) {
        char tmp[256] = {0x00};
        agcmd_property_get(PropName_No_Write_Record, tmp, No_Write_Record_Disabled);
        if (strcasecmp(tmp, No_Write_Record_Enalbed) == 0) {
            return 2;
        } else {
            return 1;
        }
    } else {
        return 0;
    }
}

int IPC_AVF_Client::_configCameraParams(bool bRecord) {
    char buffer[256];

    //---------------------------------------------------------
    // set clock
    //---------------------------------------------------------
#ifdef ARCH_S2L
    if (_bStillMode) {
        _recording_clock_mode = "still";
    }
#endif
    avf_media_set_config_string(_p_avf_media, "config.clockmode", 0, _recording_clock_mode);

    //---------------------------------------------------------
    // video config
    //---------------------------------------------------------
#ifdef ARCH_S2L
    if (_bStillMode) {
        if (bRecord) {
            CAMERALOG("cannot record in still mode");
            return -1;
        }
        // set width and height as 0: use full vin size
        // set jpeg quality to 95
        snprintf(buffer, sizeof(buffer), "still:0x0x95");
    } else
#endif
    {
        if (bRecord) {
            snprintf(buffer, sizeof(buffer), "h264:%dx%d:%d:%d:%d",
                _recording_width, _recording_height,
                _recording_bitrate, _recording_bitrate, _recording_bitrate);
        } else {
            // just preview, video stream is disabled
            // width, height must be set anyway
            snprintf(buffer, sizeof(buffer), "none:%dx%d",
                _recording_width, _recording_height);
        }
    }

    CAMERALOG("config first video as %s", buffer);
    avf_media_set_config_string(_p_avf_media, "config.video", 0, buffer);
    avf_media_set_config_bool(_p_avf_media, "config.record", 0, _get_record_flag(bRecord));

    //---------------------------------------------------------
    // preview config
    //---------------------------------------------------------
    int preview_width;
    int preview_height;
#ifdef ARCH_S2L
    if (_bStillMode) {
        // in still mode, set preview height to 0 thus vin size will be used
        preview_width = 1024;
        preview_height = 0;
    } else
#endif
    {
        // TODO: select preview size according to video aspect ratio
#ifdef ARCH_S2L
        preview_width = big_stream_width;
        preview_height = big_stream_height;
#else
        if (bRecord) {
            preview_width = small_stream_width;
            preview_height = small_stream_height;
        } else {
            preview_width = big_stream_width;
            preview_height = big_stream_height;
        }
#endif
    }
    snprintf(buffer, sizeof(buffer), "mjpeg:%dx%dx%d", preview_width, preview_height, 85);
    avf_media_set_config_string(_p_avf_media, "config.video", 1, buffer);

    //---------------------------------------------------------
    // second video config
    //---------------------------------------------------------
#ifdef ARCH_S2L
    if (_bStillMode || _recording_width <= 720) {
        if (_bStillMode) {
            avf_media_set_config_string(_p_avf_media, "config.video", 2, "mjpeg:352x240x85");
        } else {
            avf_media_set_config_string(_p_avf_media, "config.video", 2, "none");
        }
        avf_media_set_config_bool(_p_avf_media, "config.record", 1, 0);
    } else
#endif
    {
        if (bRecord) {
            int bitrate_sec = getSecondVideoBitrate();
            if (_recording_frame == 120) {
                bitrate_sec = bitrate_sec * 4;
            } else if (_recording_frame == 60) {
                bitrate_sec = bitrate_sec * 2;
            }
#ifdef ARCH_S2L
            snprintf(buffer, 256, "h264:%dx%d:%d:%d:%d",
                640, 360, bitrate_sec, bitrate_sec, bitrate_sec);
#else
            snprintf(buffer, 256, "h264:%dx%d:%d:%d:%d",
                512, 288, bitrate_sec, bitrate_sec, bitrate_sec);
#endif
        } else {
            // just preview. width, height must be set anyway
#ifdef ARCH_S2L
            snprintf(buffer, sizeof(buffer), "none:%dx%d", 640, 360);
#else
            snprintf(buffer, sizeof(buffer), "none:%dx%d", 512, 288);
#endif
        }

        avf_media_set_config_string(_p_avf_media, "config.video", 2, buffer);
        avf_media_set_config_bool(_p_avf_media, "config.record", 1, _get_record_flag(bRecord));
    }

    //---------------------------------------------------------
    // frame rate config
    //---------------------------------------------------------
#ifdef ARCH_S2L
    if (_bStillMode) {
        avf_media_set_config_string(_p_avf_media, "config.video.framerate", 0, "1:1");
        avf_media_set_config_string(_p_avf_media, "config.video.framerate", 1, "1:1");
        avf_media_set_config_string(_p_avf_media, "config.video.framerate", 2, "1:1");
    } else
#endif
    {
        if (_recording_frame == 120) {
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 0, "1:1");// video: 120
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 1, "1:8");// preview: 15
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 2, "1:4");// 2nd video: 30
        } else if (_recording_frame == 60) {
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 0, "1:1");// video: 60
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 1, "1:4");// preview: 15
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 2, "1:2");// 2nd video: 30
        } else if (_recording_frame == 30) {
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 0, "1:1");// video: 30
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 1, "1:2");// preview: 15
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 2, "1:1");// 2nd video: 30
        } else if (_recording_frame == 24) {
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 0, "1:1");// video: 23.976
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 1, "1:1");// preview: 23.976
            avf_media_set_config_string(_p_avf_media, "config.video.framerate", 2, "1:1");// 2nd video: 23.976
        } else {
            CAMERALOG("unknown framerate %d", _recording_frame);
            return -1;
        }
    }

    _setOverlay();

#ifdef PLATFORMHACHI

    if (bRecord) {
        bool ready = false;
        unsigned int spaceMB = 0;
        long long TotalSpaceMB = 0;
        GetVDBState(ready, spaceMB, TotalSpaceMB);
        // auto delete for both mannual and continous modes
        if (TotalSpaceMB > 100 * 1024) {
            avf_media_set_config_int64(_p_avf_media, "config.vdb.autodel.free",
                AVF_GLOBAL_CONFIG, kEnoughSpace_2G);
        } else if (TotalSpaceMB > 50 * 1024) {
            avf_media_set_config_int64(_p_avf_media, "config.vdb.autodel.free",
                AVF_GLOBAL_CONFIG, kEnoughSpace_1G);
        } else {
            avf_media_set_config_int64(_p_avf_media, "config.vdb.autodel.free",
                AVF_GLOBAL_CONFIG, kEnoughSpace);
        }
        //avf_media_set_config_int64(_p_avf_media, "config.vdb.autodel.quota",
        //    AVF_GLOBAL_CONFIG, kQuotaSpace);
        avf_media_set_config_bool(_p_avf_media, "config.vdb.autodel", AVF_GLOBAL_CONFIG, 1);

        if (!isCarMode()) {
            // mannual mode recording, set attr: mannual && no auto delete
            CAMERALOG("mannual mode recording, set attr: mannual && no auto delete");
            avf_camera_set_record_attr(_p_avf_media, "mn");
        } else {
            // continous mode recording, set attr: empty
            avf_camera_set_record_attr(_p_avf_media, "");
        }

        // save OBD VIN in clip or not
        char mask[AGCMD_PROPERTY_SIZE];
        memset(mask, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get(PropName_VIN_MASK, mask, Default_VIN_MASK);
        if (strlen(mask) > 0) {
            //CAMERALOG("#### set config.vin.mask to %s", mask);
            avf_media_set_config_string(_p_avf_media, "config.vin.mask", 0, mask);
        }
    }

#else

    if (bRecord) {
        //---------------------------------------------------------
        // auto delete or not
        //---------------------------------------------------------
        if (isCircleMode()) {
            CAMERALOG("--set circle mode");
            avf_media_set_config_bool(_p_avf_media, "config.vdb.autodel", 0, 1);
#ifdef ARCH_S2L
            avf_media_set_config_bool(_p_avf_media, "config.vdb.autodel", AVF_GLOBAL_CONFIG, 1);
#else
            avf_media_set_config_bool(_p_avf_media, "config.vdb.autodel", 0x80000000, 1);
#endif
        } else {
            avf_media_set_config_bool(_p_avf_media, "config.vdb.autodel", 0, 0);
#ifdef ARCH_S2L
            avf_media_set_config_bool(_p_avf_media, "config.vdb.autodel", AVF_GLOBAL_CONFIG, 0);
#else
            avf_media_set_config_bool(_p_avf_media, "config.vdb.autodel", 0x80000000, 0);
#endif
        }
    }

#endif
    return 0;
}

void IPC_AVF_Client::_startAVFRec()
{
    avf_space_info_s infor;
    memset(&infor, 0x00, sizeof(infor));
    avf_media_get_space_info(_p_avf_media, &infor);
    //CAMERALOG("infor.total_space = %llu, infor.used_space = %llu, "
    //    "infor.marked_space = %llu, infor.clip_space = %llu, other = %llu, erasable = %llu",
    //    infor.total_space, infor.used_space, infor.marked_space, infor.clip_space,
    //    infor.used_space - infor.clip_space,
    //    infor.total_space - (infor.used_space - infor.clip_space) - infor.marked_space);
    if (isCarMode() &&
        (infor.total_space - (infor.used_space - infor.clip_space) - infor.marked_space < kEnoughSpace * 2))
    {
        CAMERALOG("No enough space(%llu, kEnoughSpace * 2 = %d) to start recording!",
            infor.total_space - (infor.used_space - infor.clip_space) - infor.marked_space,
            kEnoughSpace * 2);
        if (_pCameraCallback) {
            _pCameraCallback->onCameraState(CameraCallback::CameraState_storage_full);
        }
        if (_pAVFDelegate) {
            _pAVFDelegate->onStartRecordError(Error_code_tfFull);
        }
        return;
    }

    // nomal mode
    CAMERALOG("_startAVFRec enter");
    _record_state = State_Rec_starting;
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(CameraCallback::CameraState_starting);
    }
    if (_pAVFDelegate) {
        _pAVFDelegate->onRecordStateChange(State_Rec_starting);
    }
#ifdef ENABLE_LOUDSPEAKER
    CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_StartRecording);
#endif

    if (_currentRecOption != media_NULL) {
        if (avf_camera_stop(_p_avf_media, true) < 0) {
            CAMERALOG("avf_camera_stop failed");
        }
        _currentRecOption = media_NULL;
        agcmd_property_set(PROPERTY_RecState, "media_NULL");
    }

    _ReadConfigs();
    _configVin();
    _configCameraParams(true);

    //CAMERALOG("_startAVFRec2");
    if (avf_camera_run(_p_avf_media, true) < 0) {
        CAMERALOG("avf_camera_run failed");
        return;
    }
    _currentRecOption = media_recording;
    agcmd_property_set(PROPERTY_RecState, "media_recording");
    _recordingStartTime = GetCurrentTick() / 1000;
    _lastWriteSlowTime = 0;
    char start_time[64] = "";
    snprintf(start_time, 64, "%lld", (long long)_recordingStartTime);
    agcmd_property_set(PROPERTY_RecStartTime, start_time);

    _record_state = State_Rec_recording;
    if (_pAVFDelegate) {
        _pAVFDelegate->onRecordStateChange(State_Rec_recording);
    }
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(CameraCallback::CameraState_recording);
    }
#ifdef ENABLE_LED
    if (CLedIndicator::getInstance()) {
        CLedIndicator::getInstance()->recStatusChanged(true);
    }
#endif

    CAMERALOG("_startAVFRec exit");
}

void IPC_AVF_Client::_startAVFStream()
{
    CAMERALOG("-- start avf stream, enter");
    if (_currentRecOption == media_recording) {
        _record_state = State_Rec_stopping;
        if (_pCameraCallback) {
            _pCameraCallback->onCameraState(CameraCallback::CameraState_stopping);
        }
        if (_pAVFDelegate) {
            _pAVFDelegate->onRecordStateChange(State_Rec_stopping);
        }
#ifdef ENABLE_LED
        if (CLedIndicator::getInstance()) {
            CLedIndicator::getInstance()->recStatusChanged(false);
        }
#endif
#ifdef ENABLE_LOUDSPEAKER
        CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_StopRecording);
#endif
    }
    if (_currentRecOption != media_NULL) {
        //CAMERALOG("-- avf_camera_stop 0");
        if (avf_camera_stop(_p_avf_media, true) < 0) {
            CAMERALOG("avf_camera_stop failed");
        }
        //CAMERALOG("-- avf_camera_stop 1");
        //CAMERALOG("-- avf_camera_stop 2");
        _currentRecOption = media_NULL;
        agcmd_property_set(PROPERTY_RecState, "media_NULL");
    }

    _ReadConfigs();
    _configVin();
    _configCameraParams(false);

    //CAMERALOG("-- avf_camera_run 0");
    if (avf_camera_run(_p_avf_media, true) < 0) {
        CAMERALOG("avf_camera_run() failed");
        return;
    }
    //CAMERALOG("-- avf_camera_run 1");
    //CAMERALOG("-- avf_camera_run 2");
    _currentRecOption = media_streaming;
    agcmd_property_set(PROPERTY_RecState, "media_streaming");

    _record_state = State_Rec_stoped;
    if (_pAVFDelegate) {
        _pAVFDelegate->onRecordStateChange(State_Rec_stoped);
    }
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(CameraCallback::CameraState_stopped);
    }
    CAMERALOG("-- start avf stream, exit");
}

int IPC_AVF_Client::_CloseRec()
{
    CAMERALOG("-- close avf, enter");
    _bWaitTFReady = false;
    if (_currentRecOption == media_recording) {
        if (_pCameraCallback) {
            _pCameraCallback->onCameraState(CameraCallback::CameraState_stopping);
        }
        if (_pAVFDelegate) {
            _pAVFDelegate->onRecordStateChange(State_Rec_stopping);
        }
    }
    _record_state = State_Rec_close;
    if (avf_camera_stop(_p_avf_media, true) < 0) {
        CAMERALOG("avf_camera_stop failed");
    }
    _currentRecOption = media_NULL;
    agcmd_property_set(PROPERTY_RecState, "media_NULL");

    avf_camera_close(_p_avf_media, true);
    if (_pAVFDelegate) {
        _pAVFDelegate->onRecordStateChange(State_Rec_close);
    }
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(CameraCallback::CameraState_close);
    }
    _avfMode = AVF_Mode_NULL;
    if (_pAVFDelegate) {
        _pAVFDelegate->onCameraModeChange();
    }
    CAMERALOG("-- close avf, exit");
    return 0;
}

void IPC_AVF_Client::_updateSubtitleConfig()
{
    char tmp[256];
    int c = 0;

#if defined(PLATFORMHACHI)
    const char *defaultVal = "Off";
#else
    const char *defaultVal = "On";
#endif

    agcmd_property_get("system.ui.timestamp", tmp, defaultVal);
    if (strcasecmp(tmp, "On") == 0) {
        // namestamp
        c = c | 0x01;
        SetCameraName(NULL);

        // timestamp
        c = c | 0x02;
    }
    agcmd_property_get("system.ui.coordstamp", tmp, defaultVal);
    if (strcasecmp(tmp, "On") == 0) {
        c = c | 0x04;
    }
    agcmd_property_get("system.ui.speedstamp", tmp, defaultVal);
    if (strcasecmp(tmp, "On") == 0) {
        c = c | 0x08;
    }
    //speed | position | time | prefix
    avf_media_set_config_int32(_p_avf_media, "config.subtitle.flags", 0, c);

    agcmd_property_get("system.ui.overspeed", tmp, "Off");
    if (strcasecmp(tmp, "On") == 0) {
        agcmd_property_get("system.ui.speedroof", tmp, "120");
        int speed = atoi(tmp);
        avf_media_set_config_int32(_p_avf_media, "config.subtitle.slimit", 0, speed);
        avf_media_set_config_string(_p_avf_media, "config.subtitle.slimit-str", 0, "");
    } else {
        avf_media_set_config_int32(_p_avf_media, "config.subtitle.slimit", 0, -1);
    }

    char style[128] = {0x00};
    int len = 0;

    char format[32];
    agcmd_property_get(PropName_Date_Format, format, "mm/dd/yy");
    if (strcasecmp(format, "mm/dd/yy") == 0) {
        len += snprintf(style, sizeof(style), "%s", "%b %d, %Y ");
    } else if (strcasecmp(format, "dd/mm/yy") == 0) {
        len += snprintf(style, sizeof(style), "%s", "%d %b %Y ");
    } else if (strcasecmp(format, "yy/mm/dd") == 0) {
        len += snprintf(style, sizeof(style), "%s", "%Y-%m-%d ");
    }

    agcmd_property_get(PropName_Time_Format, format, "12h");
    if (strcasecmp(format, "12h") == 0) {
        len += snprintf(style + len, sizeof(style) - len, "%s", "%l:%M::%S %p");
    } else {
        len += snprintf(style + len, sizeof(style) - len, "%s", "%H:%M:%S");
    }

    avf_media_set_config_string(_p_avf_media, "config.subtitle.timefmt", 0, style);
}

bool IPC_AVF_Client::_formatTF() {
    char tmp[256];
    memset(tmp, 0x00, sizeof(tmp));

    bool rt = true;
    int count = 0;
    const char *tmp_args[8];

    // firstly, check it is during recording /playing
    if ((_record_state == State_Rec_starting) ||
        (_record_state == State_Rec_recording))
    {
        CAMERALOG("It is during recording, can't format!");
        rt = false;
        goto RET;
    }

    // secondly, notify avf to remove vdb
    avf_media_storage_ready(_p_avf_media, 0);

    // thirdly, umount TF, format it, mount it again
    char storage_prop[256];
    getStoragePropertyName(storage_prop, sizeof(storage_prop));
    memset(tmp, 0x00, sizeof(tmp));
    agcmd_property_get(storage_prop, tmp, "");
    CAMERALOG("TF status is %s before umount", tmp);

    if (strcasecmp(tmp, "NA") == 0) {
        CAMERALOG("TF status %s is not ready for format", tmp);
        rt = false;
        goto RET;
    } else {
        // Format the TF to FAT32 - AGTSV1.0_vfat
        if (strcasecmp(tmp, "mount_ok") == 0) {
            // Firstly, umount
            tmp_args[0] = "agsh";
            tmp_args[1] = "castle";
            tmp_args[2] = "umount";
            tmp_args[3] = NULL;
            agbox_sh(3, (const char *const *)tmp_args);
            count = 0;
            do {
                sleep(1);
                count ++;
                agcmd_property_get(storage_prop, tmp, "");
                CAMERALOG("during unmount : %d, status = %s \n", count, tmp);
            } while ((strcasecmp(tmp, "ready") != 0) && (count < 10));
            if(strcasecmp(tmp, "ready") != 0) {
                // Format failed, mount the TF
                tmp_args[0] = "agsh";
                tmp_args[1] = "castle";
                tmp_args[2] = "mount";
                tmp_args[3] = NULL;
                agbox_sh(3, (const char *const *)tmp_args);
                CAMERALOG("unmount failed!");
                rt = false;
                goto RET;
            }
        }

        // Secondly, format TF, it may take some time
        tmp_args[0] = "agsh";
        tmp_args[1] = "castle";
        tmp_args[2] = "format";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
        count = 0;
        do {
            sleep(1);
            count ++;
            agcmd_property_get(storage_prop, tmp, "");
            CAMERALOG("during format : %d, status = %s \n", count, tmp);
        } while((strcasecmp(tmp, "ready") != 0) && (count < 60));
        if(strcasecmp(tmp, "ready") != 0) {
            // Format failed, mount the TF
            tmp_args[0] = "agsh";
            tmp_args[1] = "castle";
            tmp_args[2] = "mount";
            tmp_args[3] = NULL;
            agbox_sh(3, (const char *const *)tmp_args);
            CAMERALOG("format failed!");
            rt = false;
            goto RET;
        }

        // Lastly, mount th already formatted SD card
        tmp_args[0] = "agsh";
        tmp_args[1] = "castle";
        tmp_args[2] = "mount";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
        count = 0;
        do {
            sleep(1);
            count ++;
            agcmd_property_get(storage_prop, tmp, "");
            CAMERALOG("during mount after format : %d, status = %s \n", count, tmp);
        } while((strcasecmp(tmp, "mount_ok") != 0) && (count < 60));

        if(strcasecmp(tmp, "mount_ok") == 0) {
            CAMERALOG("format ok : %d \n", count);
            rt = true;
        } else {
            CAMERALOG("format error : %d, status = %s \n", count, tmp);
            rt = false;
        }
    }

RET:
    CAMERALOG("format result: %d", rt);
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(CameraCallback::CameraState_FormatSD_Result, rt);
    }
    if (_pAVFDelegate) {
        _pAVFDelegate->onFormatTFDone(rt);
    }
    return rt;
}

bool IPC_AVF_Client::_enableUSBStorage()
{
    bool rt = true;

    // Firstly check whether SD card is mounted.
    char tmp[256];
    agcmd_property_get(USBSotragePropertyName, tmp ,"");
    if (strcasecmp(tmp, "Ready") != 0) {
        CAMERALOG("It is in status %s, not ready for enable USB storage.", tmp);
        rt = false;
        goto Enable_USB_EXIT;
    }

    char storage_prop[256];
    getStoragePropertyName(storage_prop, sizeof(storage_prop));
    memset(tmp, 0x00, sizeof(tmp));
    agcmd_property_get(storage_prop, tmp, "");
    //CAMERALOG("TF status is %s before try to enable USB Storage", tmp);
    if (strcasecmp(tmp, "mount_ok") == 0) {
        // Secondly, set vout to not full screen, then close avf.
        SetFullScreen(false);

        // Thirdly, notify avf to remove vdb, then close avf
        avf_media_storage_ready(_p_avf_media, 0);
        _CloseRec();

        // Fourthly, unmount SD card.
        int count = 0;
        const char *tmp_args[8];
        tmp_args[0] = "agsh";
        tmp_args[1] = "castle";
        tmp_args[2] = "umount";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
        count = 0;
        do {
            sleep(1);
            count ++;
            agcmd_property_get(storage_prop, tmp, "");
            //CAMERALOG("during unmount : %d, storage status = %s\n", count, tmp);
        } while ((strcasecmp(tmp, "ready") != 0) && (count < 10));
        if(strcasecmp(tmp, "ready") != 0) {
            // umount failed, mount the TF
            tmp_args[0] = "agsh";
            tmp_args[1] = "castle";
            tmp_args[2] = "mount";
            tmp_args[3] = NULL;
            agbox_sh(3, (const char *const *)tmp_args);
            CAMERALOG("unmount failed!");
            rt = false;
            goto Enable_USB_EXIT;
        }

        // Lastly, enable USB storage
        tmp_args[0] = "agsh";
        tmp_args[1] = "castle";
        tmp_args[2] = "usb_fs_on";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
        do {
            sleep(1);
            count ++;
            agcmd_property_get(USBSotragePropertyName, tmp ,"");
            //CAMERALOG("during enable usb storage: %d, "
            //    "usb storage status = %s\n", count, tmp);
        } while ((strcasecmp(tmp, "Mount") != 0) && (count < 10));
        if(strcasecmp(tmp, "Mount") != 0) {
            // enable usb storage failed, mount the TF
            tmp_args[0] = "agsh";
            tmp_args[1] = "castle";
            tmp_args[2] = "mount";
            tmp_args[3] = NULL;
            agbox_sh(3, (const char *const *)tmp_args);
            CAMERALOG("enable usb storage failed!");
            rt = false;
            goto Enable_USB_EXIT;
        }
    }

Enable_USB_EXIT:
    if (rt) {
        CAMERALOG("enable USB storage successful");
    } else {
        CAMERALOG("failed to enable USB storage, resume liveview");
        _startAVFStream();
    }
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(CameraCallback::CameraState_enable_usbstrorage, rt);
    }
    return rt;
}

bool IPC_AVF_Client::_disableUSBStorage(bool force)
{
    bool rt = true;

    int count = 0;
    const char *tmp_args[8];

    // Firstly check whether SD card is USB storage.
    char tmp[256];
    agcmd_property_get(USBSotragePropertyName, tmp ,"");
    //CAMERALOG("USB storage status is %s", tmp);
    if (!((strcasecmp(tmp, "Mount") != 0) ||
        (strcasecmp(tmp, "Ready") != 0))) {
        CAMERALOG("It is in status %s, not in USB storage mode.", tmp);
        rt = false;
        goto Disable_USB_EXIT;
    }

    // Secondly disable USB storage.
    tmp_args[0] = "agsh";
    tmp_args[1] = "castle";
    tmp_args[2] = "usb_fs_off";
    if (force) {
        tmp_args[3] = "force";
        tmp_args[4] = NULL;
        CAMERALOG("Force exit usb storage!");
        agbox_sh(4, (const char *const *)tmp_args);
    } else {
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
    }
    do {
        sleep(1);
        count ++;
        agcmd_property_get(USBSotragePropertyName, tmp ,"");
        //CAMERALOG("during disable usb storage: %d, "
        //    "usb storage status = %s\n", count, tmp);
    } while ((strcasecmp(tmp, "NA") != 0) &&
        (strcasecmp(tmp, "Ready") != 0) &&
        (count < 10));
    if ((strcasecmp(tmp, "NA") != 0) && (strcasecmp(tmp, "Ready") != 0)) {
        // enable usb storage failed, mount the TF
        tmp_args[0] = "agsh";
        tmp_args[1] = "castle";
        tmp_args[2] = "mount";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
        CAMERALOG("disable usb storage failed!");
        rt = false;
        goto Disable_USB_EXIT;
    }
    CAMERALOG("after usb_fs_off, %s = %s, mount microSD again",
        USBSotragePropertyName, tmp);

    char storage_prop[256];
    getStoragePropertyName(storage_prop, sizeof(storage_prop));

    // Lastly, mount SD card.
    tmp_args[0] = "agsh";
    tmp_args[1] = "castle";
    tmp_args[2] = "mount";
    tmp_args[3] = NULL;
    agbox_sh(3, (const char *const *)tmp_args);
    count = 0;
    do {
        sleep(1);
        count ++;
        agcmd_property_get(storage_prop, tmp, "");
        //CAMERALOG("during mount after disable USB storage: %d, "
        //    "status = %s \n", count, tmp);
    } while((strcasecmp(tmp, "mount_ok") != 0)
        && (strcasecmp(tmp, "NA") != 0)
        && (count < 60));

    if ((strcasecmp(tmp, "mount_ok") == 0)
        || (strcasecmp(tmp, "NA") == 0))
    {
        //CAMERALOG("mount ok : %d \n", count);
        rt = true;
    } else {
        CAMERALOG("mount error : %d, status = %s \n", count, tmp);
        rt = false;
    }

Disable_USB_EXIT:
    if (rt) {
        // TODO:
        _startAVFStream();
        CAMERALOG("disable USB storage successful");
    }
    if (_pCameraCallback) {
        _pCameraCallback->onCameraState(CameraCallback::CameraState_disable_usbstrorage, rt);
    }
    return rt;
}

bool IPC_AVF_Client::_isRotate180()
{
    bool bRotate = false;
    char horizontal[256];
    char vertical[256];
    memset(horizontal, 0x00, 256);
    memset(vertical, 0x00, 256);
    agcmd_property_get(PropName_Rotate_Horizontal, horizontal, "0");
    agcmd_property_get(PropName_Rotate_Vertical, vertical, "0");
    if ((strcasecmp(horizontal, "0") == 0)
        && (strcasecmp(vertical, "0") == 0))
    {
        bRotate = false;
    } else if ((strcasecmp(horizontal, "1") == 0)
        && (strcasecmp(vertical, "1") == 0))
    {
        bRotate = true;
    }

    return bRotate;
}

bool IPC_AVF_Client::_queueCommand(InternalCommand cmd) {
    _cmdQueue[(_queueIndex + _queueNum) % CMD_QUE_LEN] = cmd;
    _queueNum++;
    return true;
}

IPC_AVF_Client::InternalCommand IPC_AVF_Client::_dequeueCommand() {
    int cmd = InternalCommand_Null;
    if (_queueNum > 0) {
        cmd = _cmdQueue[_queueIndex];
        _queueIndex++;
        _queueIndex = _queueIndex % CMD_QUE_LEN;
        _queueNum --;
    }
    return (InternalCommand)cmd;
}

bool IPC_AVF_Client::_queuePriorityCmd(InternalCommand cmd) {
    _priCmdQueue[(_priQueIndex + _priQueNum) % CMD_QUE_LEN] = cmd;
    _priQueNum++;
    return true;
}

IPC_AVF_Client::InternalCommand IPC_AVF_Client::_dequeuePriorityCmd() {
    int cmd = InternalCommand_Null;
    if (_priQueNum > 0) {
        cmd = _priCmdQueue[_priQueIndex];
        _priQueIndex++;
        _priQueIndex = _priQueIndex % CMD_QUE_LEN;
        _priQueNum--;
    }
    return (InternalCommand)cmd;
}

bool IPC_AVF_Client::setWorkingMode(CAM_WORKING_MODE mode) {
    _pMutex->Take();
    _targetMode = mode;
    _queueCommand(InternalCommand_ChangeWorkingMode);
    _pOnCommand->Give();
    _pMutex->Give();
    return true;
}

CAM_WORKING_MODE IPC_AVF_Client::getWorkingMode() {
    return _curMode;
}

// working mode loop:
// -> video -> slow motion -> video time lapse -> photo -> countdown photo -> photo time lapse -> raw ->
bool IPC_AVF_Client::_changeWorkingMode() {
#ifdef ENABLE_STILL_CAPTURE
    if ((CAM_MODE_VIDEO == _targetMode) ||
        (CAM_MODE_SLOW_MOTION == _targetMode) ||
        (CAM_MODE_VIDEO_LAPSE == _targetMode))
    {
        //if (_bStillMode) {
            _bStillMode = false;
            _ReadConfigs();
            _configVin();
            _configCameraParams(false);
            _startAVFStream();
        //}
    } else if ((CAM_MODE_PHOTO == _targetMode) ||
        (CAM_MODE_TIMING_SHOT == _targetMode) ||
        (CAM_MODE_PHOTO_LAPSE == _targetMode) ||
        (CAM_MODE_RAW_PICTURE == _targetMode))
    {
        if (!_bStillMode) {
            _bStillMode = true;
            _setStillCapture();
        }
        avf_camera_still_capture(_p_avf_media,
            STILLCAP_ACTION_SET_RAW, CAM_MODE_RAW_PICTURE == _targetMode);
    }
#endif
    _curMode = _targetMode;
    if (_pAVFDelegate) {
        _pAVFDelegate->onWorkingModeChange();
    }
#ifdef ENABLE_STILL_CAPTURE
    if (_pCameraCallback) {
        _pCameraCallback->onModeChanged();
    }
#endif
    return true;
}

#ifdef ENABLE_STILL_CAPTURE
bool IPC_AVF_Client::_setStillCapture() {
    CAMERALOG("-- config still capture, begin");

    if (_currentRecOption == media_recording) {
        CAMERALOG("It is during recording, no action!");
        return true;
    }

    int rval;
    if (_currentRecOption != media_NULL) {
        rval = avf_camera_stop(_p_avf_media, true);
        if (rval < 0) {
            CAMERALOG("avf_camera_stop failed %d", rval);
            return false;
        }
        _currentRecOption = media_NULL;
        agcmd_property_set(PROPERTY_RecState, "media_NULL");
    }

    if (_avfMode == AVF_Mode_NULL) {
        rval = avf_camera_set_media_source(_p_avf_media, "", "/usr/local/bin/camera.conf");
        if (rval < 0) {
            CAMERALOG("avf_camera_set_media_source() failed %d", rval);
            return false;
        }
        _avfMode = AVF_Mode_Encode;
    }

    if (_pAVFDelegate) {
        _pAVFDelegate->onRecordStateChange(State_Rec_switching);
    }

    _ReadConfigs();
    _configVin();
    _configCameraParams(false);

    rval = avf_camera_run(_p_avf_media, true);
    if (rval < 0) {
        CAMERALOG("avf_camera_run() failed %d", rval);
        return false;
    }
    _currentRecOption = media_streaming;
    agcmd_property_set(PROPERTY_RecState, "media_streaming");

    if (_pAVFDelegate) {
        _pAVFDelegate->onRecordStateChange(State_Rec_stoped);
    }

    CAMERALOG("-- config still capture, exit");
    return true;
}

bool IPC_AVF_Client::_doStillCapture() {
    CAMERALOG("-- still capture, action = %d", _action);
    int rval = avf_camera_still_capture(_p_avf_media, _action, 0);
    if (rval < 0) {
        CAMERALOG("avf_camera_still_capture() failed, rval = %d", rval);
#ifdef ENABLE_LOUDSPEAKER
        if (_action == STILLCAP_ACTION_STOP) {
            CAudioPlayer::getInstance()->stopSoundEffect();
        }
#endif
        return false;
    }
    if (_action == STILLCAP_ACTION_SINGLE || _action == STILLCAP_ACTION_BURST) {
        if (_pCameraCallback) {
            _pCameraCallback->onStillCaptureStarted(_action == STILLCAP_ACTION_SINGLE);
        }
        if (_pAVFDelegate) {
            _pAVFDelegate->onStillCaptureStarted(_action == STILLCAP_ACTION_SINGLE);
        }
    }

    return true;
}

void IPC_AVF_Client::_handleStillCaptureStateChanged() {
    still_capture_info_t info;
    if (avf_camera_get_still_capture_info(_p_avf_media, &info) < 0) {
        CAMERALOG("avf_camera_get_still_capture_info failed");
        return;
    }

    if (info.stillcap_state == STILLCAP_STATE_BURST) {
        CAudioPlayer::getInstance()->playSoundEffect(
            CAudioPlayer::SE_ContinousShot);
    } else if (info.stillcap_state == STILLCAP_STATE_STOPPING) {
        CAudioPlayer::getInstance()->stopSoundEffect();
        if (_pAVFDelegate) {
            _pAVFDelegate->onStillCaptureDone();
        }
    }
}

void IPC_AVF_Client::_handlePictureTaken() {
    still_capture_info_t info;
    if (avf_camera_get_still_capture_info(_p_avf_media, &info) < 0) {
        CAMERALOG("avf_camera_get_still_capture_info failed");
        return;
    }

    if ((info.stillcap_state == STILLCAP_STATE_IDLE) &&
        (info.num_pictures == 1) &&
        (info.burst_ticks == 0))
    {
        // blue LED oneshot to show that single shot success
#ifdef ENABLE_LED
        if (CLedIndicator::getInstance()) {
            CLedIndicator::getInstance()->blueOneshot();
        }
#endif
        // play sound to show that single shot success
#ifdef ENABLE_LOUDSPEAKER
        if (CAudioPlayer::getInstance()) {
            CAudioPlayer::getInstance()->playSoundEffect(
                CAudioPlayer::SE_SingleShot);
        }
#endif
    } else if (info.stillcap_state == STILLCAP_STATE_STOPPING) {
        if (_pCameraCallback) {
            _pCameraCallback->sendStillCaptureInfo(
                STILLCAP_STATE_IDLE, info.num_pictures, info.burst_ticks);
        }
        if (_pAVFDelegate) {
            _pAVFDelegate->sendStillCaptureInfo(
                STILLCAP_STATE_IDLE, info.num_pictures, info.burst_ticks);
        }
    } else {
        if (_pCameraCallback) {
            _pCameraCallback->sendStillCaptureInfo(
                info.stillcap_state, info.num_pictures, info.burst_ticks);
        }
        if (_pAVFDelegate) {
            _pAVFDelegate->sendStillCaptureInfo(
                info.stillcap_state, info.num_pictures, info.burst_ticks);
        }
    }
    return;
}

void IPC_AVF_Client::_updateTotalNumPictures() {
    _totalPicNum = avf_camera_get_num_pictures(_p_avf_media);
}

void IPC_AVF_Client::_startStopPhotoLapse() {
    if (CAM_MODE_PHOTO_LAPSE == _curMode) {
        if (_bInPhotoLapse) {
            // stop photo lapse
            CTimerThread::GetInstance()->CancelTimer(PhotoLapse_Timer_Name);
#ifdef ENABLE_LED
            if (CLedIndicator::getInstance()) {
                CLedIndicator::getInstance()->blueOneshot();
            }
#endif
            _record_state = State_Rec_stoped;
            if (_pAVFDelegate) {
                _pAVFDelegate->onRecordStateChange(State_Rec_stoped);
            }
            _bInPhotoLapse = false;
            _timeoutSec = 0;
        } else {
            // start photo lapse
            startStillCapture(true);
            if (_timeoutSec <= 0) {
                char tmp[256];
                memset(tmp, 0x00, 256);
                agcmd_property_get(PropName_PhotoLapse, tmp, "0.5");
                float interval_sec = atof(tmp);
                if (interval_sec >= 60.0) {
                    _bNeedRTCWakeup = true;
                } else {
                    _bNeedRTCWakeup = false;
                }
                if (interval_sec <= 1.0) {
                    _timeoutSec = 1;
                } else {
                    _timeoutSec = (int)interval_sec;
                }
            }
            if (_timeoutSec <= 1.0) {
                CTimerThread::GetInstance()->ApplyTimer(
                    // 0.5s
                    5, onPhotoLapseTimerEvent, this, true, PhotoLapse_Timer_Name);
            } else {
                CTimerThread::GetInstance()->ApplyTimer(
                    // 1s
                    10, onPhotoLapseTimerEvent, this, true, PhotoLapse_Timer_Name);
            }
            _record_state = State_Rec_PhotoLapse;
            if (_pAVFDelegate) {
                _pAVFDelegate->onRecordStateChange(State_Rec_PhotoLapse);
            }
            _bInPhotoLapse = true;
        }
    }
}

void IPC_AVF_Client::setStillCaptureMode(bool bStillMode) {
    _pMutex->Take();
    if (_bStillMode != bStillMode) {
        _bStillMode = bStillMode;
        CAMERALOG("------ set still capture mode = %d", _bStillMode);
        if (_bStillMode) {
            agcmd_property_set(PropName_still_capture, "stillCapture");
        } else {
            agcmd_property_set(PropName_still_capture, "NONE");
        }
        _queueCommand(InternalCommand_SetStillCapture);
        _pOnCommand->Give();
    }
    _pMutex->Give();
}

bool IPC_AVF_Client::isStillCaptureMode() {
    return _bStillMode;
}

void IPC_AVF_Client::startStillCapture(bool one_shot) {
    _pMutex->Take();
    if (_bStillMode) {
        if (one_shot) {
            _action = STILLCAP_ACTION_SINGLE;
        } else {
            _action = STILLCAP_ACTION_BURST;
        }
        _queueCommand(InternalCommand_DoStillCapture);
        _pOnCommand->Give();
    }
    _pMutex->Give();
}

void IPC_AVF_Client::stopStillCapture(){
    _pMutex->Take();
    if (_bStillMode) {
        _action = STILLCAP_ACTION_STOP;
        _queueCommand(InternalCommand_DoStillCapture);
        _pOnCommand->Give();
    }
    _pMutex->Give();
}

/*static*/
void IPC_AVF_Client::onPhotoLapseTimerEvent(void *para) {
    IPC_AVF_Client *cam = (IPC_AVF_Client *)para;
    if (cam && cam->_bInPhotoLapse) {
        if (cam->_timeoutSec > 0) {
            cam->_timeoutSec--;

            char tmp[256];
            memset(tmp, 0x00, 256);
            agcmd_property_get(PropName_PhotoLapse, tmp, "0.5");
            float interval_sec = atof(tmp);
            if (cam->_bNeedRTCWakeup && (interval_sec >= 60.0)) {
                if (interval_sec - cam->_timeoutSec == 8) {
                    // TODO: use callback instead
                    cam->_record_state = State_Rec_PhotoLapse_shutdown;
                } else if (interval_sec - cam->_timeoutSec == 10) {
                    // step 1: record context
                    agcmd_property_set(PROPERTY_RecState, "photo_timelapse");
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    time_t nextShotTime = tv.tv_sec + cam->_timeoutSec;
                    char start_time[32] = "";
                    snprintf(start_time, 32, "%lld", (long long)nextShotTime);
                    agcmd_property_set(PROPERTY_RecStartTime, start_time);

                    // step 2: set rtc time to wake up
                    agboard_helper_set_alarm(cam->_timeoutSec - 20);

                    // step 3: power off
                    CAMERALOG("Call \"agsh poweroff\" to power off now!");
                    const char *tmp_args[8];
                    tmp_args[0] = "agsh";
                    tmp_args[1] = "poweroff";
                    tmp_args[2] = NULL;
                    agbox_sh(2, (const char *const *)tmp_args);
                }
            }

            if (cam->_timeoutSec <= 0) {
                cam->startStillCapture(true);
                // reset counter
                if (interval_sec <= 1.0) {
                    cam->_timeoutSec = 1;
                } else {
                    cam->_timeoutSec = (int)interval_sec;
                }
            }
        }
    }
}

#endif

/***************************************************************
IPC_AVF_Client::main
*/
void IPC_AVF_Client::Stop()
{
    _bExecuting = false;
    _queueIndex = 0;
    _queueNum = 0;
    _priQueIndex = 0;
    _priQueNum = 0;
    _pOnCommand->Give();
    CThread::Stop();
}

void IPC_AVF_Client::main()
{
    _bExecuting = true;

    int cmd;
    while (_bExecuting) {
        _pOnCommand->Take(-1);
        _pMutex->Take();
        cmd = _dequeuePriorityCmd();
        if (InternalCommand_Null == cmd) {
            cmd = _dequeueCommand();
        }
        _pMutex->Give();
        switch (cmd)
        {
            case InternalCommand_Null:
                break;
            case InternalCommand_errorProcess:
                _onAvf_Error_process();
                break;
            case InternalCommand_record:
                if (_avfMode == AVF_Mode_Encode) {
                    if (_record_state != State_Rec_recording) {
                        _startAVFRec();
                    }
                }
                break;
            case InternalCommand_stopRec:
#if 0
                if (_numClients <= 0) {
                    CAMERALOG("close preview");
                    _CloseRec();
                    CAMERALOG("preview is closed");
                } else
#endif
                {
                    if (_avfMode == AVF_Mode_Encode) {
                        _startAVFStream();
                    }
                }
                break;
            case InternalCommand_closeRec:
                if (_avfMode == AVF_Mode_Encode) {
                    _CloseRec();
                }
                break;
            case InternalCommand_switchToRec:
                _switchToRec(false);
                break;
            case InternalCommand_tfReady:
                _tfReady();
                break;
            case InternalCommand_tfRemove:
                _tfRemove();
                break;
            case InternalCommand_OBDReady:
                _OBDReady(true);
                break;
            case InternalCommand_OBDRemove:
                _OBDReady(false);
                break;
            case InternalCommand_setSubtitleConfig:
                _updateSubtitleConfig();
                break;
            case InternalCommand_RestartRec:
                if (_avfMode == AVF_Mode_Encode) {
                    _CloseRec();
                    _switchToRec(_restartRecordPara);
                }
                break;
            case InternalCommand_fileBufferWarning:
                {
                    //_pUpnp_rec_service->EventWriteWarnning();
                }
                break;
            case InternalCommand_fileBufferError:
                {
                    if (_avfMode == AVF_Mode_Encode) {
                        _startAVFStream();
                    }
                        //_pUpnp_rec_service->EventWriteError();
                    }
                break;
            case InternalCommand_VdbStateChanged:
                {
                    _vdbStateChanged();
                }
                break;
            case InternalCommand_FormatTF:
                {
                    _formatTF();
                }
                break;
            case InternalCommand_SpaceFull:
                {
                    // notify UI
                    if (_pCameraCallback) {
                        _pCameraCallback->onCameraState(
                            CameraCallback::CameraState_storage_full);
                    }
                    // notify avf to stop
                    if (_avfMode == AVF_Mode_Encode) {
                        if (_record_state != State_Rec_stoped) {
                            _startAVFStream();
                        }
                    }
                }
                break;
            case InternalCommand_WriteSlow:
                {
                    time_t writeSlowTime = GetCurrentTick() / 1000;
                    // if two events happen within 10mins, show warning
                    const int duration = 10 * 60;
                    if ((_lastWriteSlowTime > 0) &&
                        (writeSlowTime - _lastWriteSlowTime < duration))
                    {
                        if (Storage_Rec_State_DiskError != _storage_rec_state) {
                            _storage_rec_state = Storage_Rec_State_WriteSlow;
                        }
                        CTimerThread::GetInstance()->CancelTimer(WriteSlowTimer);
                        // set a timer which time out after 10min
                        CTimerThread::GetInstance()->ApplyTimer(
                            20 * 60 * 10, IPC_AVF_Client::onTimeout,
                            this, false, WriteSlowTimer);
                        if (_pCameraCallback) {
                            _pCameraCallback->onCameraState(
                                CameraCallback::CameraState_writeslow);
                        }
                    }
                    _lastWriteSlowTime = writeSlowTime;
                }
                break;
            case InternalCommand_DiskError:
                {
                    // delay 1s, in case unplug SD during recording
                    sleep(1);

                    char storage_prop[256];
                    getStoragePropertyName(storage_prop, sizeof(storage_prop));
                    char tmp[256];
                    memset(tmp, 0x00, sizeof(tmp));
                    agcmd_property_get(storage_prop, tmp, "");
                    if((strcasecmp(tmp, "mount_fail") == 0)
                        || (strcasecmp(tmp, "NA") == 0))
                    {
                        _storage_rec_state = Storage_Rec_State_Normal;
                    } else {
                        // notify UI
                        _storage_rec_state = Storage_Rec_State_DiskError;
                        _pCameraCallback->onCameraState(
                            CameraCallback::CameraState_write_diskerror);
                    }

                    // notify avf to stop
                    if (_avfMode == AVF_Mode_Encode) {
                        if (_record_state != State_Rec_stoped) {
                            _startAVFStream();
                        }
                    }
                }
                break;
            case InternalCommand_EnableUSBStorage:
                {
                    _enableUSBStorage();
                }
                break;
            case InternalCommand_DisableUSBStorage:
                {
                    _disableUSBStorage(false);
                }
                break;
            case InternalCommand_DisableUSBStorageForce:
                {
                    _disableUSBStorage(true);
                }
                break;
#ifdef ENABLE_STILL_CAPTURE
            case InternalCommand_SetStillCapture:
                {
                    _setStillCapture();
                }
                break;
            case InternalCommand_DoStillCapture:
                {
                    _doStillCapture();
                }
                break;
            case InternalCommand_MSG_StillCaptureStateChanged:
                {
                    _handleStillCaptureStateChanged();
                }
                break;
            case InternalCommand_MSG_PictureTaken:
                {
                    _handlePictureTaken();
                }
                break;
            case InternalCommand_MSG_PictureListChanged:
                {
                    _updateTotalNumPictures();
                }
                break;
            case InternalCommand_ChangeWorkingMode:
                {
                    _changeWorkingMode();
                }
                break;
            case InternalCommand_Start_PhotoLapse:
                {
                    _startStopPhotoLapse();
                }
                break;
#endif
            default:
                break;
        }

        cmd = InternalCommand_Null;
    }
}

void IPC_AVF_Client::getResolution(int &width, int &height, int &frame,
    int &ar, const char* &clock_mode)
{
    char tmp[256];
    memset (tmp, 0x00, sizeof(tmp));
    agcmd_property_get(PropName_rec_resoluton, tmp, "1080p30");

    // default value
    width = 1920;
    height = 1080;
    frame = 30;
    ar = AR_16_9;
    clock_mode = "1080p30";

    if (strcasecmp(tmp, "1080p30") == 0) {
        width = 1920;
        height = 1080;
        frame = 30;
        ar = AR_16_9;
        clock_mode = "1080p30";
    } else if (strcasecmp(tmp, "720p60") == 0) {
        width = 1280;
        height = 720;
        frame = 60;
        ar = AR_16_9;
        clock_mode = "720p60";
    } else if (strcasecmp(tmp, "720p30") == 0) {
        width = 1280;
        height = 720;
        frame = 30;
        ar = AR_16_9;
        clock_mode = "720p30";
    } else if (strcasecmp(tmp, "480p60") == 0) {
        width = 720;
        height = 480;
        frame = 60;
        ar = AR_3_2;
        clock_mode = "720p30";
    } else if (strcasecmp(tmp, "480p30") == 0) {
        width = 720;
        height = 480;
        frame = 30;
        ar = AR_3_2;
        clock_mode = "720p30";
    }
#ifdef ARCH_S2L
    else if (strcasecmp(tmp, "QXGAp30") == 0) {
        width = 2048;
        height = 1536;
        frame = 30;
        ar = AR_4_3;
        clock_mode = "3Mp30";
    } else if (strcasecmp(tmp, "1080p60") == 0) {
        width = 1920;
        height = 1080;
        frame = 60;
        ar = AR_16_9;
        clock_mode = "1080p60";
    } else if (strcasecmp(tmp, "720p120") == 0) {
        width = 1280;
        height = 720;
        frame = 120;
        ar = AR_16_9;
        clock_mode = "720p120";
    } else if (strcasecmp(tmp, "1080p23976") == 0) {
        width = 1920;
        height = 1080;
        frame = 24;
        ar = AR_16_9;
        clock_mode = "1080p30";
    }
#endif

    return;
}

#ifdef ARCH_S2L
int IPC_AVF_Client::getBitrate()
{
    int bitrate = 2 * MBPS;
    char tmp[256];
    memset(tmp, 0x00, sizeof(tmp));
    agcmd_property_get(PropName_rec_resoluton, tmp, "1080p30");

    char quality[256];
    memset(quality, 0x00, sizeof(quality));
    agcmd_property_get(PropName_rec_quality, quality, Quality_Normal);
    bool bHighQuality = true;
    if (strcasecmp(quality, Quality_Normal) == 0) {
        bHighQuality = false;
    }

    if (bHighQuality) {
        if (strcasecmp(tmp, "QXGAp30") == 0) {
            bitrate = 36 * MBPS;
        } else if (strcasecmp(tmp, "1080p60") == 0) {
            bitrate = 36 * MBPS;
        } else if (strcasecmp(tmp, "1080p30") == 0) {
            bitrate = 20 * MBPS;
        } else if (strcasecmp(tmp, "1080p23976") == 0) {
            bitrate = 20 * MBPS;
        } else if (strcasecmp(tmp, "720p120") == 0) {
            bitrate = 36 * MBPS;
        } else if (strcasecmp(tmp, "720p60") == 0) {
            bitrate = 20 * MBPS;
        } else if (strcasecmp(tmp, "720p30") == 0) {
            bitrate = 12 * MBPS;
        }
    } else {
        if (strcasecmp(tmp, "QXGAp30") == 0) {
            bitrate = 26 * MBPS;
        } else if (strcasecmp(tmp, "1080p60") == 0) {
            bitrate = 26 * MBPS;
        } else if (strcasecmp(tmp, "1080p30") == 0) {
            bitrate = 16 * MBPS;
        } else if (strcasecmp(tmp, "1080p23976") == 0) {
            bitrate = 16 * MBPS;
        } else if (strcasecmp(tmp, "720p120") == 0) {
            bitrate = 26 * MBPS;
        } else if (strcasecmp(tmp, "720p60") == 0) {
            bitrate = 16 * MBPS;
        } else if (strcasecmp(tmp, "720p30") == 0) {
            bitrate = 10 * MBPS;
        }
    }

    return bitrate;
}
#else
int IPC_AVF_Client::getBitrate()
{
    int width, height, frame, ar;
    const char *clock_mode;
    this->getResolution(width, height, frame, ar, clock_mode);

    char tmp[256];
    memset (tmp, 0x00, sizeof(tmp));
    agcmd_property_get(PropName_rec_quality, tmp, "HQ");

    int base = BitrateForSoso720p;
    if (strcasecmp(tmp, "HQ") == 0) {
        base = BitrateForHQ720p;
    } else if (strcasecmp(tmp, "NORMAL") == 0) {
        base = BitrateForNormal720p;
    } else if (strcasecmp(tmp, "SOSO") == 0) {
        base = BitrateForSoso720p;
    } else if (strcasecmp(tmp, "LOW") == 0) {
        base = BitrateForLow720p;
    }

    float b = (float)width * height * ((float)frame / 30.0) / 1000000.0;
    return base * b;
}
#endif

int IPC_AVF_Client::getSecondVideoBitrate()
{
#ifdef ARCH_S2L
    return 2 * MBPS;
#else
    return 880000;
#endif
}

int IPC_AVF_Client::getColorMode()
{
    char tmp[256];
    memset (tmp, 0x00, sizeof(tmp));
    agcmd_property_get(PropName_rec_colorMode, tmp, "NORMAL");

    int colormode = Color_Mode_NORMAL;
    if (strcasecmp(tmp, "NORMAL") == 0) {
        colormode = Color_Mode_NORMAL;
    } else if (strcasecmp(tmp, "SPORT") == 0) {
        colormode = Color_Mode_SPORT;
    } else if (strcasecmp(tmp, "CARDV") == 0) {
        colormode = Color_Mode_CARDV;
    } else if (strcasecmp(tmp, "SCENE") == 0) {
        colormode = Color_Mode_SCENE;
    }

    return colormode;
}

// Car mode is the combination of AutoStart and Circle mode.
bool IPC_AVF_Client::isCarMode()
{
    bool bReturn = false;
    char val[256];
    memset (val, 0x00, sizeof(val));
#ifdef PLATFORMHACHI
    agcmd_property_get(PropName_rec_recMode, val , "CAR");
#else
    agcmd_property_get(PropName_rec_recMode, val , "");
#endif
    if (strcasecmp(val, "CAR") == 0) {
        bReturn = true;
    }
    return bReturn;
}

bool IPC_AVF_Client::isAutoStartMode()
{
    bool bReturn = false;
    char val[256];
    memset (val, 0x00, sizeof(val));
#ifdef PLATFORMHACHI
    agcmd_property_get(PropName_rec_recMode, val , "CAR");
#else
    agcmd_property_get(PropName_rec_recMode, val , "");
#endif
    if (strcasecmp(val, "AutoStart") == 0) {
        bReturn = true;
    } else if (strcasecmp(val, "CAR") == 0) {
        bReturn = true;
    }
    return bReturn;
}

int IPC_AVF_Client::getSegSeconds()
{
    // TODO: implement when needed
#if 0
    int frameLength;
    _pRecSegLength->GetIframeLen(frameLength);
    return frameLength;
#endif
    return SegLength_1min;
}

void IPC_AVF_Client::SetSegSeconds(int seconds)
{
// TODO: implement when needed
    //_pRecSegLength->SetIframeNum(seconds);
}

/***************************************************************
getRecordingTime
*/
UINT64 IPC_AVF_Client::getRecordingTime()
{
#ifdef ENABLE_STILL_CAPTURE
    if (_bInPhotoLapse) {
        return _timeoutSec;
    } else
#endif
    if (_currentRecOption == media_recording) {
        //time_t currenttime;
        //time(&currenttime);
        long long time = GetCurrentTick() / 1000 - _recordingStartTime;
        if (time > 0) {
            return  time;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

void IPC_AVF_Client::getRecordState(int *state, int *is_still)
{
    *state = _record_state;
#ifdef ENABLE_STILL_CAPTURE
    *is_still = _bStillMode;
#else
    *is_still = 0;
#endif
}

/***************************************************************
RecordKeyProcess
*/
void IPC_AVF_Client::RecordKeyProcess()
{
    //CAMERALOG("--car mode : %d, state : %d", isCarMode(), _record_state);
    if (0/*isCarMode() && (_record_state != State_Rec_stoped)*/) {
        // in car mode, key event means mark the video
        markVideo();
    } else {
        StartRecording();
    }
}

void IPC_AVF_Client::GsensorSignalProcess(int before_s, int after_s)
{
    if (isCarMode() && (_record_state != State_Rec_stoped)) {
        avf_camera_mark_live_clip_v2(_p_avf_media, 0, before_s * 1000, after_s * 1000, true, "");
        CAMERALOG("in car mode, gsensor event comes, mark the video");
    }
}

bool IPC_AVF_Client::isCircleMode()
{
    bool bReturn = false;
    char val[256];
    memset (val, 0x00, sizeof(val));
#ifdef PLATFORMHACHI
    agcmd_property_get(PropName_rec_recMode, val , "CAR");
#else
    agcmd_property_get(PropName_rec_recMode, val , "");
#endif
    if ((strcasecmp(val, "CAR") == 0) || (strcasecmp(val, "Circle") == 0)) {
        bReturn = true;
    } else {
        bReturn = false;
    }
    return bReturn;
}

bool IPC_AVF_Client::isAutoStartRec()
{
    return isAutoStartMode();
}

bool IPC_AVF_Client::isAlertMode()
{
    bool bReturn = false;
    char val[256];
    memset (val, 0x00, sizeof(val));
#ifdef PLATFORMHACHI
    agcmd_property_get(PropName_rec_recMode, val , "CAR");
#else
    agcmd_property_get(PropName_rec_recMode, val , "");
#endif

    if (strcasecmp(val, "ALERT") == 0) {
        bReturn = true;
    } else {
        bReturn = false;
    }
    return bReturn;
}

/***************************************************************
switchToRec
*/
void IPC_AVF_Client::switchToRec(bool checkAuto)
{
    _pMutex->Take();
    _bCheckAutoRec = checkAuto;
    _queueCommand(InternalCommand_switchToRec);
    _pOnCommand->Give();
    _pMutex->Give();
}

void IPC_AVF_Client::onTFReady(bool b)
{
    CAMERALOG("+++ onTFReady: %d", b);
    _pMutex->Take();
    if (b) {
        _queueCommand(InternalCommand_tfReady);
    } else {
        _queueCommand(InternalCommand_tfRemove);
    }
    // reset state on plug/unplug sd card
    _storage_rec_state = Storage_Rec_State_Normal;
    _pOnCommand->Give();
    _pMutex->Give();
}
void IPC_AVF_Client::onOBDReady(bool b)
{
    //CAMERALOG("+++ onOBDReady: %d", b);
    _pMutex->Take();
    if (b) {
        _queueCommand(InternalCommand_OBDReady);
    } else {
        _queueCommand(InternalCommand_OBDRemove);
    }
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
        CAMERALOG("------on restart cmd!!");
        _queueCommand(InternalCommand_RestartRec);
        _pOnCommand->Give();
    }
    _pMutex->Give();
}

/***************************************************************
StartRecording
*/
void IPC_AVF_Client::StartRecording()
{
    if (is_no_write_record()) {
        _pMutex->Take();
        if ((_currentRecOption == media_streaming) || (_currentRecOption == media_NULL)) {
            CAMERALOG("----on start cmd!!");
            _queueCommand(InternalCommand_record);
            _pOnCommand->Give();
        } else if(_currentRecOption == media_recording) {
            CAMERALOG("------on stop cmd!!");
            _queueCommand(InternalCommand_stopRec);
            _pOnCommand->Give();
        }
        _pMutex->Give();
        return;
    }

    char storage_prop[256];
    getStoragePropertyName(storage_prop, sizeof(storage_prop));
    char tmp[256];
    memset(tmp, 0x00, sizeof(tmp));
    agcmd_property_get(storage_prop, tmp, "");
    if ((strcasecmp(tmp, "mount_fail") == 0)
        || (strcasecmp(tmp, "NA") == 0))
    {
        if (_pCameraCallback) {
            _pCameraCallback->onCameraState(
                CameraCallback::CameraState_storage_cardmissing);
        }
        if (_pAVFDelegate) {
            _pAVFDelegate->onStartRecordError(Error_code_notfcard);
        }
        return;
    }

#ifdef ENABLE_STILL_CAPTURE
    if (CAM_MODE_PHOTO_LAPSE == _curMode) {
        _startStopPhotoLapse();
        return;
    }
#endif

    _pMutex->Take();

    if (_record_state == State_Rec_error) {
#if 0
        CAMERALOG("avf is under error state, try close");
        int rval = avf_camera_close(_p_avf_media, true);
        if (rval < 0) {
            CAMERALOG("avf_camera_close() failed %d", rval);
            _pMutex->Give();
            return;
        }
        CAMERALOG("avf is closed from error state, try run again");
        rval = avf_camera_run(_p_avf_media, true);
        if (rval < 0) {
            CAMERALOG("avf_camera_run() failed %d", rval);
            _pMutex->Give();
            return;
        }
        CAMERALOG("avf recovered from error state successfully");
#else
        CAMERALOG("Can't start recording because in error state!");
        _pMutex->Give();
        return;
#endif
    }

    if ((_currentRecOption == media_streaming) || (_currentRecOption == media_NULL)) {
        unsigned int freesize;
        long long totalspace;
        bool bReady = false;
        GetVDBState(bReady, freesize, totalspace);
#if !defined(PLATFORMHACHI)
        // Try release space firstly
        if (bReady && (freesize < RecordThreshold) && isCircleMode()) {
            _tryReleaseSpace();
            GetVDBState(bReady, freesize, totalspace);
        }
#endif
#ifdef PLATFORMHACHI
        if (!bReady) {
#else
        if (!bReady || (freesize < RecordThreshold)) {
#endif
            _pMutex->Give();
            if (_pAVFDelegate) {
                _pAVFDelegate->onStartRecordError(
                    bReady ? Error_code_tfFull : Error_code_notfcard);
            }
            if (_pCameraCallback) {
                _pCameraCallback->onCameraState(CameraCallback::CameraState_storage_full);
            }
            return;
        }

        CAMERALOG("----on start cmd!!");
        _queueCommand(InternalCommand_record);
        _pOnCommand->Give();
    } else if(_currentRecOption == media_recording) {
        CAMERALOG("------on stop cmd!!");
        _queueCommand(InternalCommand_stopRec);
        _pOnCommand->Give();
    }
    _pMutex->Give();
}

/***************************************************************
switchStream
*/
void IPC_AVF_Client::switchStream(bool bBig)
{
    _pMutex->Take();
    if (_bBigStream != bBig) {
        _bBigStream = bBig;
        if (_currentRecOption == media_streaming) {
            _queueCommand(InternalCommand_stopRec);
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
#ifdef ENABLE_STILL_CAPTURE
    if (CAM_MODE_PHOTO_LAPSE == _curMode) {
        _startStopPhotoLapse();
        return;
    }
#endif

    _pMutex->Take();
    if (_currentRecOption == media_recording) {
        CAMERALOG("--on stop cmd!!");
        _queueCommand(InternalCommand_stopRec);
        _pOnCommand->Give();
    } else if(_currentRecOption == media_streaming) {

    }
    _pMutex->Give();
}

/***************************************************************
CloseRec
*/
int IPC_AVF_Client::CloseRec()
{
    //CAMERALOG("---close record");
    _pMutex->Take();
    _queueCommand(InternalCommand_closeRec);
    _pOnCommand->Give();
    _pMutex->Give();
    return 0;
}

int IPC_AVF_Client::getGpsState()
{
    int result = -2;
    avf_media_get_int32(_p_avf_media, "status.gps", 0, &result, -2);
    //-2: cannot read; -1: no position; 0: has position
    //CAMERALOG("---gps state: %d", result);

    return result;
}

void IPC_AVF_Client::ShowGPSFullInfor(bool b)
{
    //CAMERALOG("-- show gps full infor");
    int r = b? 1 : 0;
    r = avf_media_set_config_int32(_p_avf_media,"config.subtitle.show-gps", 0, r);
    //CAMERALOG("-- show gps full infor : %d", r);
}

void IPC_AVF_Client::UpdateSubtitleConfig()
{
    //CAMERALOG("-- update subtitle config");
    //"system.ui.timestamp"
    _pMutex->Take();
    _queueCommand(InternalCommand_setSubtitleConfig);
    _pOnCommand->Give();
    _pMutex->Give();
    //return 0;
}

void IPC_AVF_Client::EnableDisplay()
{
    CAMERALOG("EnableDisplay set mode MODE_DECODE");
    if (avf_display_set_mode(_p_avf_media, MODE_DECODE) < 0) {
        CAMERALOG("enable display error");
    }
}

void IPC_AVF_Client::LCDOnOff(bool b)
{
    int r = b? 1 : 0;
    //CAMERALOG("---set lcd backlight : %d", r);
    avf_display_set_backlight(_p_avf_media, VOUT_0, "t15p00", r);
}

#ifdef PLATFORMHACHI
int IPC_AVF_Client::SetDisplayBrightness(int value)
{
    if (value < 0) {
        value = 0;
    } else if (value > 255) {
        value = 255;
    }
    //CAMERALOG("set brightness to %d", value);
    avf_display_set_brightness(_p_avf_media, VOUT_0, VOUT_TYPE_MIPI, value);
    return value;
}

bool IPC_AVF_Client::SetFullScreen(bool bFull)
{
    const char *fullscreen = "lcd:0,0,400,400";
    const char *aspectratio16_9 = "lcd:0,88,400,224";
    char *config = (char *)aspectratio16_9;
    if (bFull) {
        config = (char *)fullscreen;
    } else {
        config = (char *)aspectratio16_9;
    }

    int ret = avf_camera_change_vout_video(_p_avf_media, config);
    if (ret == 0) {
        _bVideoFullScreen = bFull;
        return true;
    } else {
        CAMERALOG("change vout video failed with error %d", ret);
        return false;
    }
}

bool IPC_AVF_Client::setOSDRotate(int rotate_type)
{
    bool ret = false;
    int ret_val = 0;
    switch (rotate_type) {
        case 0:
            ret_val = avf_display_flip_vout_osd(_p_avf_media,
                VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_NORMAL);
            if (ret_val == 0) {
                ret_val = avf_display_flip_vout_video(_p_avf_media,
                    VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_NORMAL);
                if (ret_val == 0) {
                    _pMutex->Take();
                    _queueCommand(InternalCommand_stopRec);
                    _pOnCommand->Give();
                    _pMutex->Give();

                    agcmd_property_set(PropName_Rotate_Horizontal, "0");
                    agcmd_property_set(PropName_Rotate_Vertical, "0");
                    ret = true;
                } else {
                    avf_display_flip_vout_osd(_p_avf_media,
                        VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_HV);
                }
            }
            break;
        case 1:
            ret_val = avf_display_flip_vout_osd(_p_avf_media,
                VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_HV);
            if (ret_val == 0) {
                ret_val = avf_display_flip_vout_video(_p_avf_media,
                    VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_HV);
                if (ret_val == 0) {
                    _pMutex->Take();
                    _queueCommand(InternalCommand_stopRec);
                    _pOnCommand->Give();
                    _pMutex->Give();

                    agcmd_property_set(PropName_Rotate_Horizontal, "1");
                    agcmd_property_set(PropName_Rotate_Vertical, "1");
                    ret = true;
                } else {
                    avf_display_flip_vout_osd(_p_avf_media,
                        VOUT_0, VOUT_TYPE_MIPI, VOUT_FLIP_NORMAL);
                }
            }
            break;
        default:
            break;
    }
    return ret;
}

#endif

void IPC_AVF_Client::SetCameraName(const char* name)
{
    if (_p_avf_media) {
        char logo_name[256];
        snprintf(logo_name, sizeof(logo_name), " \xEE\xC0\xC1 Waylens");
        CAMERALOG("set name %s", logo_name);
        avf_media_set_config_string(_p_avf_media, "config.subtitle.prefix", 0, logo_name);
    }
}

void IPC_AVF_Client::SetCameraLocation(char* location)
{
    if (_p_avf_media) {
        avf_media_set_config_string(_p_avf_media, "config.subtitle.gpsinfo", 0, location);
    }
}

int IPC_AVF_Client::SetMicVolume(int capvol)
{
    int ret_val = -1;
    const char *capvol_args[8];
    char capvol_str[AGCMD_PROPERTY_SIZE];

    snprintf(capvol_str, sizeof(capvol_str), "%d", capvol);
    capvol_args[0] = "agboard_helper";
    capvol_args[1] = "--mic_volume";
    capvol_args[2] = capvol_str;
    capvol_args[3] = NULL;
    ret_val = agcmd_agexecvp(capvol_args[0], (char * const *)capvol_args);
    if (ret_val) {
        CAMERALOG("agboard_helper(capvol) = %d.", ret_val);
    }
    return ret_val;
}

int IPC_AVF_Client::DefaultMicVolume()
{
    char tmp[256];
    agcmd_property_get(audioGainPropertyName, tmp, "NA");
    if (strcasecmp(tmp, "NA")==0) {
        CAMERALOG("set mic volume : %d:", defaultMicGain);
        SetMicVolume(defaultMicGain);
    }
    return 0;
}

bool IPC_AVF_Client::SetMicMute(bool mute)
{
    int ret_val = -1;
    const char *volume_args[8];

    volume_args[0] = "agboard_helper";
    if (mute) {
        volume_args[1] = "--mic_mute";
        volume_args[2] = "MIC Mute";
    } else {
        volume_args[1] = "--mic_normal";
        volume_args[2] = "MIC Normal";
    }
    volume_args[3] = NULL;
    ret_val = agcmd_agexecvp(volume_args[0], (char * const *)volume_args);
    if (ret_val) {
        CAMERALOG("agboard_helper(mic mute) = %d.", ret_val);
    }
    return true;
}

bool IPC_AVF_Client::SetMicWindNoise(bool enable)
{
    int ret_val = -1;
    const char *volume_args[8];
    char wnr_str[8];

    snprintf(wnr_str, sizeof(wnr_str), "%d", enable);

    volume_args[0] = "agboard_helper";
    volume_args[1] = "--mic_wnr";
    volume_args[2] = wnr_str;
    volume_args[3] = NULL;
    ret_val = agcmd_agexecvp(volume_args[0], (char * const *)volume_args);
    if (ret_val) {
        CAMERALOG("agboard_helper(mic mute) = %d.", ret_val);
    }
    return true;
}

int IPC_AVF_Client::SetSpkVolume(int volume)
{
    if (volume < 0){
        volume = 0;
    } else if (volume > 10) {
        volume = 10;
    }

    int ret_val = -1;
    const char *volume_args[8];
    char volume_str[AGCMD_PROPERTY_SIZE];

    snprintf(volume_str, sizeof(volume_str), "%d", volume);
    volume_args[0] = "agboard_helper";
    volume_args[1] = "--spk_volume";
    volume_args[2] = volume_str;
    volume_args[3] = NULL;
    ret_val = agcmd_agexecvp(volume_args[0], (char * const *)volume_args);
    if (ret_val) {
        CAMERALOG("agboard_helper(spk_volume) = %d.", ret_val);
    }

    return ret_val;
}

bool IPC_AVF_Client::SetSpkMute(bool mute)
{
    int ret_val = -1;
    const char *volume_args[8];

    volume_args[0] = "agboard_helper";
    if (mute) {
        volume_args[1] = "--spk_mute";
        volume_args[2] = "Speaker Mute";
    } else {
        volume_args[1] = "--spk_normal";
        volume_args[2] = "Speaker Normal";
    }
    volume_args[3] = NULL;
    ret_val = agcmd_agexecvp(volume_args[0], (char * const *)volume_args);
    if (ret_val) {
        CAMERALOG("agboard_helper(spk_mute) = %d.", ret_val);
    }

    return true;
}

// return value:
//   0: vdb not ready, maybe during prepare
//   1: sd card full
//   2: sd card and vdb are ready
int IPC_AVF_Client::GetStorageState() {
    if (_record_state == State_Rec_recording) {
        return 2;
    }

    avf_vdb_info_t infor;
    memset(&infor, 0x00, sizeof(infor));
    avf_media_get_vdb_info(_p_avf_media, &infor);
    if (infor.vdb_ready) {
        unsigned int freesize = 0;
        long long totalspace = 0;
        bool b = false;
        GetVDBState(b, freesize, totalspace);
        if (freesize == 0) {
            return 1;
        } else {
            return 2;
        }
    } else {
        return 0;
    }
}

void IPC_AVF_Client::GetVDBState
    (bool &ready
    , unsigned int &spaceMB, long long &TotalSpaceMB)
{
    avf_vdb_info_t infor;
    memset(&infor, 0x00, sizeof(infor));
    avf_media_get_vdb_info(_p_avf_media, &infor);
    if (isCircleMode()) {
        ready = infor.vdb_ready && infor.enough_space_autodel;
    } else {
        ready = infor.vdb_ready && infor.enough_space;
    }

    if (ready) {
        spaceMB = infor.file_space_mb + infor.disk_space_mb;
    } else {
        spaceMB = 0;
    }
    TotalSpaceMB = infor.total_space_mb;
    //CAMERALOG("--infor.vdb_ready: %d, ready : %d, free: %d space : %lld\n",
    //    infor.vdb_ready, ready, spaceMB, TotalSpaceMB);
}

bool IPC_AVF_Client::GetVDBSpaceInfo(int &totleMB, int &markedMB, int &loopMB)
{
    avf_space_info_s infor;
    memset(&infor, 0x00, sizeof(infor));
    avf_media_get_space_info(_p_avf_media, &infor);
    //CAMERALOG("infor.total_space = %llu, infor.used_space = %llu, "
    //    "infor.marked_space = %llu, infor.clip_space = %llu",
    //    infor.total_space, infor.used_space, infor.marked_space, infor.clip_space);

    totleMB = infor.total_space / 1000 / 1000;
    loopMB = ((infor.clip_space - infor.marked_space) +
        (infor.total_space - infor.used_space))
        / 1000 / 1000;
    markedMB = infor.marked_space / 1000 / 1000;
    //CAMERALOG("totle = %dMB, marked = %dMB, loop = %dMB",
    //    totleMB, markedMB, loopMB);
    return true;
}

int IPC_AVF_Client::GetStorageFreSpcPcnt()
{
    avf_vdb_info_t infor;
    avf_media_get_vdb_info(_p_avf_media, &infor);
    int space = 0;
    bool ready = infor.vdb_ready && infor.enough_space;
    if (ready) {
        space = (infor.file_space_mb + infor.disk_space_mb) * 100 / infor.total_space_mb;
    }
    //CAMERALOG("---spac: %d, %d, %d, %d\n", space, infor.file_space_mb, infor.disk_space_mb, infor.total_space_mb);
    return space;
}

bool IPC_AVF_Client::FormatTF() {
    _pMutex->Take();
    CAMERALOG("------on FormatTF cmd!!");
    _queueCommand(InternalCommand_FormatTF);
    _pOnCommand->Give();
    _pMutex->Give();
    return true;
}

bool IPC_AVF_Client::EnableUSBStorage()
{
    _pMutex->Take();
    //CAMERALOG("------on enable usb storage cmd!!");
    _queueCommand(InternalCommand_EnableUSBStorage);
    _pOnCommand->Give();
    _pMutex->Give();
    return true;
}

bool IPC_AVF_Client::DisableUSBStorage(bool force)
{
    _pMutex->Take();
    //CAMERALOG("------on disable usb storage cmd!!");
    if (force) {
        _queueCommand(InternalCommand_DisableUSBStorageForce);
    } else {
        _queueCommand(InternalCommand_DisableUSBStorage);
    }
    _pOnCommand->Give();
    _pMutex->Give();
    return true;
}

bool IPC_AVF_Client::setRotateMode(int mode) {
    CAMERALOG("------setRotateMode %d", mode);
    if (mode == 1) {
        _bRotate = true;
    } else {
        _bRotate = false;
    }
    RestartRecord();
    return _bRotate;
}

bool IPC_AVF_Client::onClientsNumChanged(bool add) {
    _pMutex->Take();
    if (add) {
        _numClients++;
    } else {
        if (_numClients > 0) {
            _numClients--;
        }
    }
    CAMERALOG("clients num %d", _numClients);
#if 0
    if (!add && (_numClients <= 0) && (_currentRecOption != media_recording)) {
        CAMERALOG("close preview");
        _CloseRec();
        CAMERALOG("preview is closed");
    }
    if (add && (_numClients == 1) && (_currentRecOption == media_NULL)) {
        CAMERALOG("start preview");
        CAMERALOG("preview started");
    }
#endif
    if (CTSDeviceManager::getInstance())
    {
        CTSDeviceManager::getInstance()->broadcastClientsChanged();
    }
    _pMutex->Give();
    return true;
}

bool IPC_AVF_Client::markVideo() {
    if (_p_avf_media && (_record_state != State_Rec_stoped)) {
        // Firstly check whether enough space to add a tag
        bool bMarkable = false;
        avf_space_info_s infor;
        memset(&infor, 0x00, sizeof(infor));
        avf_media_get_space_info(_p_avf_media, &infor);
        //CAMERALOG("infor.total_space = %llu, infor.used_space = %llu, "
        //    "infor.marked_space = %llu, infor.clip_space = %llu",
        //    infor.total_space, infor.used_space, infor.marked_space, infor.clip_space);

        // check whether there is enough space to add a tag
        if ((!isCarMode()) ||
            (infor.total_space - (infor.used_space - infor.clip_space) - infor.marked_space > kEnoughSpace_Mark))
        {
            bMarkable = true;
        }

        if (bMarkable) {
            char tmp[256];
            memset(tmp, 0x00, 256);
            agcmd_property_get(PropName_Mark_before, tmp, Mark_before_time);
            int before_s = atoi(tmp);
            memset(tmp, 0x00, 256);
            agcmd_property_get(PropName_Mark_after, tmp, Mark_after_time);
            int after_s = atoi(tmp);

            int ret = avf_camera_mark_live_clip_v2(_p_avf_media, 0,
                before_s * 1000, after_s * 1000, true, "");
            //CAMERALOG("mark the video for [-%ds, %ds] with ret %d",
            //    before_s, after_s, ret);
        } else {
            CAMERALOG("No enough space(%llu, kEnoughSpace_Mark = %llu) to add a tag!",
                infor.total_space - (infor.used_space - infor.clip_space) - infor.marked_space,
                kEnoughSpace_Mark);
        }

        if (_pCameraCallback) {
            _pCameraCallback->onCameraState(
                CameraCallback::CameraState_mark, bMarkable ? 0 : 1);
        }

        return bMarkable;
    }

    return false;
}

bool IPC_AVF_Client::markVideo(int before_sec, int after_sec)
{
    if (_p_avf_media && (_record_state != State_Rec_stoped)) {
        // Firstly check whether enough space to add a tag
        bool bMarkable = false;
        avf_space_info_s infor;
        memset(&infor, 0x00, sizeof(infor));
        avf_media_get_space_info(_p_avf_media, &infor);
        //CAMERALOG("infor.total_space = %llu, infor.used_space = %llu, "
        //    "infor.marked_space = %llu, infor.clip_space = %llu",
        //    infor.total_space, infor.used_space, infor.marked_space, infor.clip_space);

        // check whether there is enough space to add a tag
        if ((!isCarMode()) ||
            (infor.total_space - (infor.used_space - infor.clip_space) - infor.marked_space > kEnoughSpace_Mark))
        {
            bMarkable = true;
        }

        if (bMarkable) {
            int ret = avf_camera_mark_live_clip_v2(_p_avf_media, 0,
                before_sec * 1000, after_sec * 1000, true, "");
            //CAMERALOG("mark the video for [-%ds, %ds] with ret %d",
            //    before_s, after_s, ret);
        } else {
            CAMERALOG("No enough space(%llu, kEnoughSpace_Mark = %llu) to add a tag!",
                infor.total_space - (infor.used_space - infor.clip_space) - infor.marked_space,
                kEnoughSpace_Mark);
            if (_pCameraCallback) {
                _pCameraCallback->onCameraState(
                    CameraCallback::CameraState_mark, 1);
            }
        }

        return bMarkable;
    }

    return false;
}

bool IPC_AVF_Client::markVideo(int before_sec, int after_sec,
    const uint8_t *data, uint32_t data_size)
{
    if (!data || (data_size <= 0)) {
        return false;
    }

    if (_p_avf_media && (_record_state != State_Rec_stoped)) {
        // Firstly check whether enough space to add a tag
        bool bMarkable = false;
        avf_space_info_s infor;
        memset(&infor, 0x00, sizeof(infor));
        avf_media_get_space_info(_p_avf_media, &infor);
        //CAMERALOG("infor.total_space = %llu, infor.used_space = %llu, "
        //    "infor.marked_space = %llu, infor.clip_space = %llu",
        //    infor.total_space, infor.used_space, infor.marked_space, infor.clip_space);

        // check whether there is enough space to add a tag
        if ((!isCarMode()) ||
            (infor.total_space - (infor.used_space - infor.clip_space) - infor.marked_space > kEnoughSpace_Mark))
        {
            bMarkable = true;
        }

        if (bMarkable) {
            int ret = avf_camera_mark_live_clip_v2(_p_avf_media, 0,
                before_sec * 1000, after_sec * 1000, true, "");
            //CAMERALOG("mark the video for [-%ds, %ds] with ret %d",
            //    before_sec, after_sec, ret);
            avf_mark_live_info_t mark_info;
            memset(&mark_info, 0x00, sizeof(mark_info));
            ret = avf_camera_get_mark_live_clip_info(_p_avf_media, &mark_info);

            ret = avf_camera_set_clip_scene_data(_p_avf_media,
                CLIP_TYPE_MARKED, mark_info.clip_id,
                data, data_size);
            if (ret != 0) {
                CAMERALOG("set clip scene data failed, cancel mark");
                avf_camera_cancel_mark_live_clip(_p_avf_media);
            }
        } else {
            CAMERALOG("No enough space(%llu, kEnoughSpace_Mark = %llu) to add a tag!",
                infor.total_space - (infor.used_space - infor.clip_space) - infor.marked_space,
                kEnoughSpace_Mark);
            if (_pCameraCallback) {
                _pCameraCallback->onCameraState(
                    CameraCallback::CameraState_mark, 1);
            }
        }

        return bMarkable;
    }

    return false;
}

void IPC_AVF_Client::stopMark()
{
    avf_camera_cancel_mark_live_clip(_p_avf_media);
}

bool IPC_AVF_Client::FactoryReset()
{
    bool ret = true;
    int ret_val = -1;
    const char *capvol_args[8];

    capvol_args[0] = "agboard_helper";
    capvol_args[1] = "--factory_reset";
    capvol_args[2] = NULL;
    ret_val = agcmd_agexecvp(capvol_args[0], (char * const *)capvol_args);
    if (ret_val) {
        CAMERALOG("agboard_helper(factory_reset) = %d.", ret_val);
        ret = false;
    }
    return ret;
}


/***************************************************************
CHdmiWatchThread
*/
CHdmiWatchThread* CHdmiWatchThread::pInstance = NULL;

void CHdmiWatchThread::create()
{
    if (pInstance == NULL) {
        pInstance = new CHdmiWatchThread();
        pInstance->Go();
    }
}

void CHdmiWatchThread::destroy()
{
    if (pInstance != NULL) {
        pInstance->Stop();
        delete pInstance;
    }
}

void CHdmiWatchThread::main()
{
    CHdmiWatchThread::_hdmi_proc = open("/proc/ambarella/vout1_event", O_RDONLY);
    if (_hdmi_proc < 0) {
        CAMERALOG("--Unable to open vout1_event!");
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
#if 0
    struct amb_event            events[256];
    unsigned int                sno;
    int                         i, event_num;
    //struct iav_command_message  cmd;
    //enum amb_event_type         last_event;
    //int                         wait_client;

    while (1) {
        event_num = read(_hdmi_proc, events, sizeof(events)) / sizeof(struct amb_event);
        if (event_num > 0) {
            sno = 0;
            //last_event = AMB_EV_NONE;
            for (i = 0; i < event_num; i++) {
                if (events[i].sno <= sno) {
                    continue;
                } else {
                    sno = events[i].sno;
                }

                switch (events[i].type) {
                    case AMB_EV_VOUT_HDMI_PLUG:
                        CAMERALOG("---------------hdmid plug in");
                        break;
                    case AMB_EV_VOUT_HDMI_REMOVE:
                        CAMERALOG("---------------hdmid remove");
                        //last_event = events[i].type;
                        break;

                    default:
                        break;
                }
            }
        }
    }
#endif
}

