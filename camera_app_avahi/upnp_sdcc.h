/*****************************************************************************
 * upnp_sdcc.h
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 - 2014, linnsong.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef __H_class_upnp_sdcc__
#define __H_class_upnp_sdcc__

#include "Class_PropsEventThread.h"

//for ui
#include "class_camera_property.h"
#include "class_camera_callback.h"

#include "Class_DeviceManager.h"
#include "class_ipc_rec.h"

#ifdef PLATFORMHACHI
#include "SensorManager.h"
#endif

/***************************************************************
CUpnpCloudCamera
*/
class CUpnpCloudCamera 
    : public CCameraProperties
{
public:
    CUpnpCloudCamera();
    virtual ~CUpnpCloudCamera();

    void EventWriteWarnning();
    void EventWriteError();

    // ui interface for rec
    virtual camera_State getRecState();
    virtual UINT64 getRecordingTime();
    virtual int getMarkTargetDuration();
    virtual storage_rec_state getStorageRecState();
    virtual storage_State getStorageState();
    virtual int getFreeSpaceMB(int& free, int& total);
    virtual int getSpaceInfoMB(int &totleMB, int &markedMB, int &loopMB);
    virtual int getStorageFreSpcByPrcn();
    virtual bool FormatStorage();
    virtual bool EnableUSBStorage();
    virtual bool DisableUSBStorage(bool force);
    virtual wifi_mode getWifiMode();
    virtual wifi_State getWifiState();
    virtual rotate_State getRotationState();
    virtual gps_state getGPSState();
    virtual bluetooth_state getBluetoothState();

    virtual camera_mode getRecMode();
    virtual void setRecMode(camera_mode m);
    virtual void setRecSegLength(int seconds);
    virtual int getBitrate();
    virtual int startRecMode();
    virtual int endRecMode();
    virtual bool isCarMode();
    virtual bool isCircleMode();

    virtual bool setWorkingMode(CAM_WORKING_MODE mode);
    virtual CAM_WORKING_MODE getWorkingMode();

#ifdef ENABLE_STILL_CAPTURE
    virtual void setStillCaptureMode(bool bStillMode);
    virtual bool isStillCaptureMode();
    virtual void startStillCapture(bool one_shot);
    virtual void stopStillCapture();
    virtual int  getTotalPicNum();
#endif
    void setCameraCallback(CameraCallback *callback);

    virtual void OnSystemTimeChange();
    virtual void ShowGPSFullInfor(bool b);
    virtual void LCDOnOff(bool b);
    virtual void OnRecKeyProcess();
    virtual void OnSensorSwitchKey();
    virtual void UpdateSubtitleConfig();
    virtual void EnableDisplayManually();

    virtual void SetMicVolume(int volume);
    virtual void SetMicMute(bool mute);
    virtual void SetMicWindNoise(bool enable);
    virtual void SetSpkVolume(int volume);
    virtual void SetSpkMute(bool mute);

    virtual int GetBatteryVolume(bool& bIncharge);
    virtual int GetBatteryRemainingTime(bool& bDischarging);
    virtual int SetDisplayBrightness(int vaule);
    virtual bool SetFullScreen(bool bFull);
    virtual bool isVideoFullScreen();
    virtual bool setOSDRotate(int rotate_type);
    virtual int GetBrightness();
    virtual bool isDisplayOff() { return _bDisplayOff; }

    virtual int GetWifiAPinfor(char* pName, char* pPwd);
    virtual int GetWifiCltinfor(char* pName, int len);
    virtual int GetWifiCltSignal();
    virtual int SetWifiMode(int m);

    virtual int GetSystemLanguage(char* language);
    virtual int SetSystemLanguage(char* language);
    virtual bool MarkVideo();
    virtual bool MarkVideo(int before_sec, int after_sec);
    virtual bool MarkVideo(int before_sec, int after_sec,
        const char *data, int data_size);
    virtual bool FactoryReset();
    virtual int getClientsNum();

#ifdef PLATFORMHACHI
    virtual void ScanNetworks();
    virtual bool ConnectHost(char *ssid);

    virtual int EnableBluetooth(bool enable);
    virtual void ScanBTDevices();
    virtual int BindBTDevice(const char* mac, int type);
    virtual int unBindBTDevice(int type);

    virtual bool getOrientation(
        float &euler_heading,
        float &euler_roll,
        float &euler_pitch);

    virtual bool getAccelXYZ(
        float &accel_x,
        float &accel_y,
        float &accel_z);

    virtual bool getSpeedKmh(int &speed);
    virtual bool getBoostKpa(int &pressure);
    virtual bool getRPM(int &rpm);
    virtual bool getAirFlowRate(int &volume);
    virtual bool getEngineTemp(int &temperature);
#endif

    virtual bool setFlightMode(bool enable);
    virtual bool isFlightMode();

    static void markTimerCallback(void *para);

private:
    int _getWifiState();
    int _checkIpStatus();

    void _setBrightness(int value);

    int           _CurrentIP;
    bool          _bMarking;

    bool          _bDisplayOff;

#ifdef PLATFORMHACHI
    SensorManager *_sensors;
#endif
};

#endif
