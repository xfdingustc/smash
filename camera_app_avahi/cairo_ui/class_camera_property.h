
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
    camera_State_stopping,
    camera_State_starting,
    camera_State_record,
    camera_State_marking,
    camera_State_PhotoLapse,
    camera_State_PhotoLapse_shutdown,
    camera_State_play,
    camera_State_Pause,
    camera_State_Error,
    camera_State_WriteSlow,
    camera_State_Storage_changed,
    camera_State_Storage_full,
    camera_State_FormatSD_result,
    camera_State_enable_usbstorage_result,
    camera_State_disable_usbstorage_result,
    camera_State_scanwifi_result,
    camera_State_connectwifi_result,
    camera_State_scanBT_result,
    camera_State_bindBT_result,
    camera_State_Num,
}camera_State;

// Keep sync with enum Wifi_Mode in xml_cmd/ccam_cmds.h
typedef enum wifi_mode
{
    wifi_mode_AP = 0,
    wifi_mode_Client,
    wifi_mode_Off,
    wifi_mode_Multirole,
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
    storage_State_unknown,
    storage_State_usb,
}storage_State;

typedef enum storage_rec_state
{
    storage_rec_state_Normal = 0,
    storage_rec_state_WriteSlow,
    storage_rec_state_DiskError,
    storage_rec_state_num
} storage_rec_state;

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

typedef enum bluetooth_state
{
    bt_state_off = 0,
    bt_state_on,
    bt_state_connected,
    bt_state_hid_paired,
    bt_state_obd_paired,
    bt_state_hid_obd_paired,
}bluetooth_state;

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
    PropertyChangeType_Wifi,
    PropertyChangeType_storage,
    PropertyChangeType_language,
    PropertyChangeType_audio,
    PropertyChangeType_bluetooth,
    PropertyChangeType_power,
    PropertyChangeType_resolution,
    PropertyChangeType_rec_mode,
    PropertyChangeType_ptl_interval,
    PropertyChangeType_usb_storage,
    PropertyChangeType_settings,
    PropertyChangeType_rotation,
    PropertyChangeType_rotation_mode,
}PropertyChangeType;

class PropertiesObserver
{
public:
    PropertiesObserver(){};
    virtual ~PropertiesObserver(){};
    virtual void OnPropertyChange(void* p, PropertyChangeType type) = 0;
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
    virtual ~CCameraProperties(){};

    virtual camera_State getRecState() = 0;
    virtual UINT64 getRecordingTime()= 0;
    virtual int getMarkTargetDuration()= 0;
    virtual storage_rec_state getStorageRecState() = 0;
    virtual storage_State getStorageState() = 0;
    virtual int getFreeSpaceMB(int& free, int& total) = 0;
    virtual int getSpaceInfoMB(int &totleMB, int &markedMB, int &loopMB) = 0;
    virtual int getStorageFreSpcByPrcn() = 0;
    virtual bool FormatStorage() = 0;
    virtual bool EnableUSBStorage() = 0;
    virtual bool DisableUSBStorage(bool force) = 0;
    virtual wifi_mode getWifiMode() = 0;
    virtual wifi_State getWifiState() = 0;
    virtual rotate_State getRotationState() = 0;
    virtual gps_state getGPSState() = 0;
    virtual bluetooth_state getBluetoothState() = 0;
    virtual camera_mode getRecMode() = 0;
    virtual void setRecMode(camera_mode m) = 0;
    virtual void setRecSegLength(int seconds) = 0;
    virtual int getBitrate() = 0;

    virtual int startRecMode() = 0;
    virtual int endRecMode() = 0;
    virtual bool isCarMode() = 0;
    virtual bool isCircleMode() = 0;

    virtual bool setWorkingMode(CAM_WORKING_MODE mode) = 0;
    virtual CAM_WORKING_MODE getWorkingMode() = 0;

#ifdef ENABLE_STILL_CAPTURE
    virtual void setStillCaptureMode(bool bStillMode) = 0;
    virtual bool isStillCaptureMode() = 0;
    virtual void startStillCapture(bool one_shot) = 0;
    virtual void stopStillCapture() = 0;
    virtual int  getTotalPicNum() = 0;
#endif

    virtual void OnSystemTimeChange() = 0;
    virtual void ShowGPSFullInfor(bool b) = 0;
    virtual void LCDOnOff(bool b) = 0;

    virtual void OnRecKeyProcess() = 0;
    virtual void OnSensorSwitchKey() = 0;
    virtual void UpdateSubtitleConfig() = 0;

    void SetObserver(PropertiesObserver* p){_ppObeservers = p;};

    virtual void EnableDisplayManually() = 0;

    virtual void SetMicVolume(int volume) = 0;
    virtual void SetMicMute(bool mute) = 0;
    virtual void SetMicWindNoise(bool enable) = 0;
    virtual void SetSpkVolume(int volume) = 0;
    virtual void SetSpkMute(bool mute) = 0;

    virtual int GetBatteryVolume(bool& bCharge) = 0;
    virtual int GetBatteryRemainingTime(bool& bCharge) = 0;

    virtual int SetDisplayBrightness(int vaule)=0;
    virtual int GetBrightness() = 0;
    virtual bool isDisplayOff() = 0;
    virtual bool SetFullScreen(bool bFull) = 0;
    virtual bool isVideoFullScreen() = 0;
    virtual bool setOSDRotate(int rotate_type) = 0;

    virtual int GetWifiAPinfor(char* pName, char* pPwd) = 0;
    virtual int GetWifiCltinfor(char* pName, int len) = 0;
    virtual int GetWifiCltSignal() = 0;
    virtual int SetWifiMode(int m) = 0;

    virtual int GetSystemLanguage(char* language) = 0;
    virtual int SetSystemLanguage(char* language) = 0;
    virtual bool MarkVideo() = 0;
    virtual bool MarkVideo(int before_sec, int after_sec) = 0;
    virtual bool MarkVideo(int before_sec, int after_sec,
        const char *data, int data_size) = 0;
    virtual bool FactoryReset() = 0;

    virtual int getClientsNum() = 0;

#ifdef PLATFORMHACHI
    virtual void ScanNetworks() = 0;
    virtual bool ConnectHost(char *ssid) = 0;

    virtual int EnableBluetooth(bool enable) = 0;
    virtual void ScanBTDevices() = 0;
    virtual int BindBTDevice(const char* mac, int type) = 0;
    virtual int unBindBTDevice(int type) = 0;

    virtual bool getOrientation(
        float &euler_heading,
        float &euler_roll,
        float &euler_pitch) = 0;

    virtual bool getAccelXYZ(
        float &accel_x,
        float &accel_y,
        float &accel_z) = 0;

    virtual bool getSpeedKmh(int &speed) = 0;
    virtual bool getBoostKpa(int &pressure) = 0;
    virtual bool getRPM(int &rpm) = 0;
    virtual bool getAirFlowRate(int &volume) = 0;
    virtual bool getEngineTemp(int &temperature) = 0;
#endif

    virtual bool setFlightMode(bool enable) = 0;
    virtual bool isFlightMode() = 0;

protected:
    PropertiesObserver* _ppObeservers;
};

#endif
