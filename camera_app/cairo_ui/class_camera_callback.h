
#ifndef __H_class_camera_callback__
#define __H_class_camera_callback__

class CameraCallback
{
public:
    typedef enum RecordState
    {
        CameraState_stopped = 0,
        CameraState_stopping,
        CameraState_starting,
        CameraState_recording,
        CameraState_close,
        CameraState_open,
        CameraState_mark,
        CameraState_storage_changed,
        CameraState_storage_full,
        CameraState_storage_cardmissing,
        CameraState_FormatSD_Result,
        CameraState_error,
        CameraState_writeslow,
        CameraState_write_diskerror,
        CameraState_PhotoLapse,
        CameraState_enable_usbstrorage,
        CameraState_disable_usbstrorage,
        CameraState_scanWifi_result,
        CameraState_connectWifi_result,
        CameraState_scanBT_result,
        CameraState_bindBT_result,
        CameraState_Wifi_pwd_error,
        CameraState_Wifi_add_result,
        RecordState_Num
    } CameraState;

    CameraCallback(){}
    virtual ~CameraCallback(){}
#ifdef ENABLE_STILL_CAPTURE
    virtual void onStillCaptureStarted(int one_shot) = 0;
    virtual void onStillCaptureDone() = 0;
    virtual void sendStillCaptureInfo(int stillcap_state,
        int num_pictures, int burst_ticks) = 0;
#endif
    virtual void onModeChanged() = 0;
    virtual void onCameraState(CameraState state, int payload = 0) = 0;
};

#endif
