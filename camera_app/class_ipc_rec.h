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
#include "xml_cmd/ccam_cmds.h"

#ifdef USE_AVF
#include <stdint.h>
#include "avf_media.h"
#else
#include "ipc_vlc.h"
#endif

class CameraCallback;

#define big_stream_width 1024
#define big_stream_height 576
#define big_stream_quality 85

#define small_stream_width 512
#define small_stream_height 288
#define small_stream_quality 85

#define Storage_least_space (100*1024*1024)


class CHdmiWatchThread : public CThread
{
public:
    static void create();
    static void destroy();
    static CHdmiWatchThread* getInstance(){return pInstance;};
protected:
    virtual void main();
    static void hdmi_hotplug_handler(int num);
    void handler(int num);

private:
    CHdmiWatchThread():CThread("hdmi_watch_Thread")
    {

    };
    ~CHdmiWatchThread(){};
    static CHdmiWatchThread *pInstance;
    int _hdmi_proc;
};

class IPC_AVF_Delegate
{
public:
    IPC_AVF_Delegate() {}
    virtual ~IPC_AVF_Delegate() {}
    virtual void onNewFileReady(char* path) { CAMERALOG("--->"); }
    virtual void onCameraModeChange() = 0;
    virtual void onWorkingModeChange() = 0;
    virtual void onRecordStateChange(State_Rec state) = 0;
    virtual void onStartRecordError(Rec_Error_code error) = 0;
    virtual void onStillCaptureStarted(int one_shot) = 0;
    virtual void onStillCaptureDone() = 0;
    virtual void sendStillCaptureInfo(int stillcap_state,
        int num_pictures, int burst_ticks) = 0;
    virtual void onFormatTFDone(bool success) = 0;
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

    typedef enum RecAR
    {
        AR_16_9,    // 16:9
        AR_3_2,     // 3:2
        AR_4_3,     // 4:3
    } RecAR;

    typedef enum SegLength
    {
        SegLength_1min = 0,
        SegLength_3min,
        SegLength_5min,
        SegLength_10min,
        SegLength_num,
    } SegLength;

    //create
    static void create();
    static IPC_AVF_Client* getObject();
    static void destroy();

    // timer callback
    static void onTimeout(void *);

    //call back
    static void avf_event_callback( int type, void *pdata );
    void AVFCameraCallback(int type);
    void setDelegate(IPC_AVF_Delegate* dele){_pAVFDelegate = dele;};
    //record control
    void switchStream(bool bBig);

    void StartRecording();
    void RecordKeyProcess();
    void GsensorSignalProcess(int before_s, int after_s);
    void StopRecording();
    UINT64 getRecordingTime();
    void getRecordState(int *state, int *is_still);

    //record property
    int getWidth() { return _recording_width; }
    int getHeight() { return _recording_height; }
    int getFps() { return _recording_frame; }
    void getResolution(
        int &width, int &height,
        int &frame, int &ar,
        const char* &clock_mode);
    int getBitrate();
    int getSecondVideoBitrate();
    int getSegSeconds();
    int getColorMode();
    bool isCircleMode();
    bool isAutoStartRec();
    bool isAlertMode();
    bool isAutoStartMode();
    bool isCarMode();
    bool isRotate() { return _bRotate; }
    void SetSegSeconds(int seconds);

    bool setRotateMode(int mode);
    void switchToRec(bool checkAuto);
    void onTFReady(bool b);
    void onOBDReady(bool b);

    int CloseRec();
    void ShowGPSFullInfor(bool b);
    void LCDOnOff(bool b);
#ifdef PLATFORMHACHI
    int  SetDisplayBrightness(int value);
    bool SetFullScreen(bool bFull);
    bool isVideoFullScreen() { return _bVideoFullScreen; }
    bool setOSDRotate(int rotate_type);
#endif
    void SetCameraName(const char* name);
    void SetCameraLocation(char* location);
    void UpdateSubtitleConfig();
    int getGpsState();

    AVF_Mode getAvfMode(){return _avfMode;};
    void EnableDisplay();
    void RestartRecord();

    int SetMicVolume(int capvol);
    int DefaultMicVolume();
    bool SetMicMute(bool mute);
    bool SetMicWindNoise(bool enable);
    int SetSpkVolume(int volume);
    bool SetSpkMute(bool mute);

    virtual void Stop();

    int GetStorageState();
    void GetVDBState(bool &ready, unsigned int &spaceMB, long long &TotalSpaceMB);
    bool GetVDBSpaceInfo(int &totleMB, int &markedMB, int &loopMB);
    int GetStorageFreSpcPcnt();

    bool FormatTF();
    bool EnableUSBStorage();
    bool DisableUSBStorage(bool force);
    bool onClientsNumChanged(bool add);
    int  getClients() { return _numClients; }
    bool markVideo();
    bool markVideo(int before_sec, int after_sec);
    bool markVideo(int before_sec, int after_sec, const uint8_t *data, uint32_t data_size);
    void stopMark();

    bool FactoryReset();

    int  getStorageRecState() { return _storage_rec_state; }

    bool setWorkingMode(CAM_WORKING_MODE mode);
    CAM_WORKING_MODE getWorkingMode();

#ifdef ENABLE_STILL_CAPTURE
    void setStillCaptureMode(bool bStillMode);
    bool isStillCaptureMode();
    void startStillCapture(bool one_shot);
    void stopStillCapture();
    int  getTotalPicNum() { return _totalPicNum; }
    static void onPhotoLapseTimerEvent(void *p);
#endif
    void setCameraCallback(CameraCallback  *callback) { _pCameraCallback = callback; }


protected:
    enum InternalCommand
    {
        InternalCommand_Null = 0,
        InternalCommand_errorProcess,
        InternalCommand_switchToRec,
        InternalCommand_closeRec,
        InternalCommand_record,
        InternalCommand_stopRec,
        InternalCommand_tfReady,
        InternalCommand_tfRemove,
        InternalCommand_setSubtitleConfig,
        InternalCommand_RestartRec,
        InternalCommand_fileBufferWarning,
        InternalCommand_fileBufferError,
        InternalCommand_VdbStateChanged,
        InternalCommand_OBDReady,
        InternalCommand_OBDRemove,
        InternalCommand_FormatTF,
        InternalCommand_SpaceFull,
        InternalCommand_WriteSlow,
        InternalCommand_DiskError,
        InternalCommand_EnableUSBStorage,
        InternalCommand_DisableUSBStorage,
        InternalCommand_DisableUSBStorageForce,
#ifdef ENABLE_STILL_CAPTURE
        InternalCommand_SetStillCapture,
        InternalCommand_DoStillCapture,
        InternalCommand_MSG_StillCaptureStateChanged,
        InternalCommand_MSG_PictureTaken,
        InternalCommand_MSG_PictureListChanged,
        InternalCommand_Start_PhotoLapse,
#endif
        InternalCommand_ChangeWorkingMode,
        InternalCommand_num,
    };
    void main();


private:
    static IPC_AVF_Client* pInstance;
    IPC_AVF_Client();
    ~IPC_AVF_Client();

    void _OBDReady(bool b);

    void _switchToRec(bool forceStartRec);
    void _startAVFRec();
    void _startAVFStream();
    void _ReadConfigs();
    int _configVin();
    int _get_record_flag(bool bRecord);
    int _configCameraParams(bool bRecord);
    int _setOverlay();
    int _CloseRec();

    int _config_vout();
    int _config_lcd_vout(bool bON);
    int _config_hdmi_vout(bool on, bool fbON, bool bsmall);

    void _onAvf_Error_process();

    void _tfReady();
    void _tfRemove();
    void _vdbStateChanged();
    bool _tryReleaseSpace();

    void _updateSubtitleConfig();

    bool _formatTF();
    bool _enableUSBStorage();
    bool _disableUSBStorage(bool force);

    bool _isRotate180();

    bool _changeWorkingMode();

    bool _queueCommand(InternalCommand cmd);
    InternalCommand _dequeueCommand();

    bool _queuePriorityCmd(InternalCommand cmd);
    InternalCommand _dequeuePriorityCmd();
#ifdef ENABLE_STILL_CAPTURE
    bool _setStillCapture();
    bool _doStillCapture();

    void _handleStillCaptureStateChanged();
    void _handlePictureTaken();
    void _updateTotalNumPictures();

    void _startStopPhotoLapse();

    bool _bStillMode;
    int  _action;
    bool _bInPhotoLapse;
    bool _bNeedRTCWakeup;
    int  _timeoutSec;
    int  _totalPicNum;
#endif
    CameraCallback *_pCameraCallback;

private:
    static bool  _bExecuting;

    time_t  _recordingStartTime;

    CSemaphore  *_pOnCommand;
    CMutex      *_pMutex;

    avf_media_t   *_p_avf_media;
    int           _avf_camera_state;
    int           _currentRecOption;
    bool          _restartRecordPara;

    int    _recording_width;
    int    _recording_height;
    int    _recording_frame;
    int    _recording_bitrate;
    int    _recording_AR;
    const char   *_recording_clock_mode;

    int    _seg_iframe_num;
    int    _record_color_mode;
    int    _record_state;
    bool   _bRotate;

    bool           _bBigStream;

    AVF_Mode       _avfMode;

    // 10 commands shoudld be enough
#define CMD_QUE_LEN 10
    InternalCommand     _cmdQueue[CMD_QUE_LEN];
    int                 _queueNum;
    int                 _queueIndex;
    // priority command queue
    InternalCommand     _priCmdQueue[CMD_QUE_LEN];
    int                 _priQueNum;
    int                 _priQueIndex;

    bool                _hdmiOn;

    bool                _bWaitTFReady;
    bool                _bSetOBDReady;

    IPC_AVF_Delegate    *_pAVFDelegate;

    bool                _bCheckAutoRec;

    bool                _lcdOn;

    int                 _numClients;

    CAM_WORKING_MODE    _curMode;
    CAM_WORKING_MODE    _targetMode;

    time_t              _lastWriteSlowTime;
    Storage_Rec_State   _storage_rec_state;

    bool                _bVideoFullScreen;
};

#endif
