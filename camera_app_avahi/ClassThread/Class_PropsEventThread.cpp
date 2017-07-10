#include <stdio.h>
#include "agbox.h"
#include "Class_PropsEventThread.h"

CPropsEventThread* CPropsEventThread::_pInstance = NULL;
void CPropsEventThread::Create()
{
    if (_pInstance == NULL) {
        _pInstance = new CPropsEventThread();
        _pInstance->Go();
    }
}

void CPropsEventThread::Destroy()
{
    if (_pInstance) {
        CAMERALOG("CPropsEventThread::Destroy");
        _pInstance->clearHandle();
        _pInstance->Stop();
        delete _pInstance;
        _pInstance = NULL;
    }
}

CPropsEventThread* CPropsEventThread::getInstance()
{
    return _pInstance;
}

int CPropsEventThread::WifiPropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_wifi);
    }
    return 0;
}

int CPropsEventThread::FilePropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_file);
    }
    return 0;
}

int CPropsEventThread::AudioPropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_audio);
    }
    return 0;
}

int CPropsEventThread::PowerPropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_power);
    }
    return 0;
}
int CPropsEventThread::StoragePropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_storage);
    }
    return 0;
}
int CPropsEventThread::BTPropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_Bluetooth);
    }
    return 0;
}
int CPropsEventThread::OBDPropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_obd);
    }
    return 0;
}
int CPropsEventThread::HIDPropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_hid);
    }
    return 0;
}
int CPropsEventThread::LanguagePropertyWaitCB(const char *key, const char *value) {
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_language);
    }
    return 0;
}

int CPropsEventThread::ResolutionPropertyWaitCB(const char *key, const char *value) {
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_resolution);
    }
    return 0;
}

int CPropsEventThread::RecModePropertyWaitCB(const char *key, const char *value) {
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_rec_mode);
    }
    return 0;
}

int CPropsEventThread::PTLIntervalPropertyWaitCB(const char *key, const char *value) {
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_ptl_interval);
    }
    return 0;
}

int CPropsEventThread::USBStoragePropertyWaitCB(const char *key, const char *value) {
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_usb_storage);
    }
    return 0;
}

int CPropsEventThread::SettingsPropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_settings);
    }
    return 0;
}

int CPropsEventThread::RotationPropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_rotation);
    }
    return 0;
}

int CPropsEventThread::RotationModePropertyWaitCB(const char *key, const char *value)
{
    if (_pInstance) {
        _pInstance->propertyChanged(property_type_rotation_mode);
    }
    return 0;
}

void CPropsEventThread::propertyChanged(changed_property_type type)
{
    _mutex->Take();
    if (_queueNum > propEventQueueDeepth) {
        CAMERALOG("event queue full, type = %d", type);
    } else {
        _typeQueue[(_queueIndex + _queueNum) % propEventQueueDeepth] = type;
        _queueNum++;
    }
    _mutex->Give();
    _pTmpPropertyChanged->Give();
}

CPropsEventThread::CPropsEventThread()
    : CThread("propEventThread", CThread::NORMAL, 0, NULL)
    , _PEHnumber(0)
    , _queueNum(0)
    , _queueIndex(0)
{
    _mutex = new CMutex("CPropsEventThread mutex");
    _mutexForHandler = new CMutex("CPropsEventThread handlerMutex");
    _pTmpPropertyChanged = new CSemaphore(0, 1, "CPropsEventThread sem");
}

CPropsEventThread::~CPropsEventThread()
{
    delete _mutex;
    delete _mutexForHandler;
    delete _pTmpPropertyChanged;
    CAMERALOG("destroyed");
}

void CPropsEventThread::AddPEH(CPropertyEventHandler* pHandle)
{
    _mutexForHandler->Take();
    if (_PEHnumber < PEHMaxNumber) {
        _pPropertyEventHandlers[_PEHnumber] = pHandle;
        _PEHnumber++;
    } else {
        CAMERALOG("add too much PEH");
    }
    _mutexForHandler->Give();
}

void CPropsEventThread::clearHandle()
{
    _mutexForHandler->Take();
    //for(_pPropertyEventHandlers[_PEHnumber])
    _PEHnumber = 0;
    _mutexForHandler->Give();
}

void CPropsEventThread::main(void *)
{
    struct agcmd_property_wait_info_s *pwait_info1;
    struct agcmd_property_wait_info_s *pwait_info_volume;
#ifdef PLATFORMHACHI
    struct agcmd_property_wait_info_s *pwait_info_spk_mute;
    struct agcmd_property_wait_info_s *pwait_info_spk_volume;
    struct agcmd_property_wait_info_s *pwait_info_brightness;
    struct agcmd_property_wait_info_s *pwait_info_display_off;
    struct agcmd_property_wait_info_s *pwait_info_power_off;
    struct agcmd_property_wait_info_s *pwait_info_screen_saver_style;
    struct agcmd_property_wait_info_s *pwait_info_rotation;
    struct agcmd_property_wait_info_s *pwait_info_rotation_mode;
#endif
    struct agcmd_property_wait_info_s *pwait_info2;
    struct agcmd_property_wait_info_s *pwait_info3;
    struct agcmd_property_wait_info_s *pwait_info4;
    struct agcmd_property_wait_info_s *pwait_info5;
    struct agcmd_property_wait_info_s *pwait_info6;
    struct agcmd_property_wait_info_s *pwait_info7;
    struct agcmd_property_wait_info_s *pwait_info8;
    struct agcmd_property_wait_info_s *pwait_info_rc_mac;
    struct agcmd_property_wait_info_s *pwait_info_obd_mac;
    struct agcmd_property_wait_info_s *pwait_info_wifimode;
    struct agcmd_property_wait_info_s *pwait_info_mlan_ip;
    struct agcmd_property_wait_info_s *pwait_info_wlan_ip;
    struct agcmd_property_wait_info_s *pwait_info_ap_ip;
    struct agcmd_property_wait_info_s *pwait_info_language;
    struct agcmd_property_wait_info_s *pwait_info_resolution;
    struct agcmd_property_wait_info_s *pwait_info_rec_mode;
    struct agcmd_property_wait_info_s *pwait_info_ptl_interval;
    struct agcmd_property_wait_info_s *pwait_info_usb;

#ifdef PLATFORMHACHI
    pwait_info1 = agcmd_property_wait_start(Prop_Mic_Status, AudioPropertyWaitCB);
    pwait_info_volume = agcmd_property_wait_start(Prop_Mic_Volume, AudioPropertyWaitCB);
    pwait_info_spk_mute = agcmd_property_wait_start(Prop_Spk_Status, AudioPropertyWaitCB);
    pwait_info_spk_volume = agcmd_property_wait_start(Prop_Spk_Volume, AudioPropertyWaitCB);
    pwait_info_brightness = agcmd_property_wait_start(
        PropName_Brighness, SettingsPropertyWaitCB);
    pwait_info_display_off = agcmd_property_wait_start(
        PropName_Display_Off_Time, SettingsPropertyWaitCB);
    pwait_info_power_off = agcmd_property_wait_start(
        PropName_Udc_Power_Delay, SettingsPropertyWaitCB);
    pwait_info_screen_saver_style = agcmd_property_wait_start(
        PropName_Screen_saver_style, SettingsPropertyWaitCB);
    pwait_info_rotation = agcmd_property_wait_start(
        PropName_Rotate_Vertical, RotationPropertyWaitCB);
    pwait_info_rotation_mode = agcmd_property_wait_start(
        PropName_Rotate_Mode, RotationModePropertyWaitCB);
#else
    pwait_info1 = agcmd_property_wait_start(audioStatusPropertyName, AudioPropertyWaitCB);
    pwait_info_volume = agcmd_property_wait_start(audioGainPropertyName, AudioPropertyWaitCB);
#endif
    pwait_info2 = agcmd_property_wait_start(wifiStatusPropertyName, WifiPropertyWaitCB);
    pwait_info3 = agcmd_property_wait_start(WIFI_SWITCH_STATE, WifiPropertyWaitCB);
    pwait_info4 = agcmd_property_wait_start(powerstateName, PowerPropertyWaitCB);
    pwait_info5 = agcmd_property_wait_start(SotrageStatusPropertyName, StoragePropertyWaitCB);
    pwait_info6 = agcmd_property_wait_start(propBTTempStatus, BTPropertyWaitCB);
    pwait_info7 = agcmd_property_wait_start(propBTTempOBDStatus, OBDPropertyWaitCB);
    pwait_info8 = agcmd_property_wait_start(propBTTempHIDStatus, HIDPropertyWaitCB);
    pwait_info_rc_mac = agcmd_property_wait_start(propHIDMac, BTPropertyWaitCB);
    pwait_info_obd_mac = agcmd_property_wait_start(propOBDMac, BTPropertyWaitCB);
    pwait_info_wifimode = agcmd_property_wait_start(wifiModePropertyName, WifiPropertyWaitCB);
    pwait_info_mlan_ip = agcmd_property_wait_start(WIFI_MLAN_IP, WifiPropertyWaitCB);
    pwait_info_wlan_ip = agcmd_property_wait_start(WIFI_WLAN_IP, WifiPropertyWaitCB);
    pwait_info_ap_ip = agcmd_property_wait_start(WIFI_AP_IP, WifiPropertyWaitCB);
    pwait_info_language = agcmd_property_wait_start(PropName_Language, LanguagePropertyWaitCB);
    pwait_info_resolution = agcmd_property_wait_start(PropName_rec_resoluton, ResolutionPropertyWaitCB);
    pwait_info_rec_mode = agcmd_property_wait_start(PropName_rec_recMode, RecModePropertyWaitCB);
    pwait_info_ptl_interval = agcmd_property_wait_start(PropName_PhotoLapse, PTLIntervalPropertyWaitCB);
    pwait_info_usb = agcmd_property_wait_start(USBSotragePropertyName, USBStoragePropertyWaitCB);

    //propertyChanged(property_type_obd);
    _bRun = true;
    //CAMERALOG("--event thread start running");
    while(_bRun)
    {
        _pTmpPropertyChanged->Take(-1);
        //CAMERALOG("--state change trig");
        while (_queueNum > 0)
        {
            _mutexForHandler->Take();
            for (int i = 0; i < _PEHnumber; i++) {
                _pPropertyEventHandlers[i]->EventPropertyUpdate(_typeQueue[_queueIndex]);
            }
            _mutexForHandler->Give();
            _mutex->Take();
            _queueIndex++;
            _queueIndex = _queueIndex % propEventQueueDeepth;
            _queueNum --;
            _mutex->Give();
        }
    }
    agcmd_property_wait_stop(pwait_info1);
    agcmd_property_wait_stop(pwait_info_volume);
#ifdef PLATFORMHACHI
    agcmd_property_wait_stop(pwait_info_spk_mute);
    agcmd_property_wait_stop(pwait_info_spk_volume);
    agcmd_property_wait_stop(pwait_info_brightness);
    agcmd_property_wait_stop(pwait_info_display_off);
    agcmd_property_wait_stop(pwait_info_power_off);
    agcmd_property_wait_stop(pwait_info_screen_saver_style);
    agcmd_property_wait_stop(pwait_info_rotation);
    agcmd_property_wait_stop(pwait_info_rotation_mode);
#endif
    agcmd_property_wait_stop(pwait_info2);
    agcmd_property_wait_stop(pwait_info3);
    agcmd_property_wait_stop(pwait_info4);
    agcmd_property_wait_stop(pwait_info5);
    agcmd_property_wait_stop(pwait_info6);
    agcmd_property_wait_stop(pwait_info7);
    agcmd_property_wait_stop(pwait_info8);
    agcmd_property_wait_stop(pwait_info_rc_mac);
    agcmd_property_wait_stop(pwait_info_obd_mac);
    agcmd_property_wait_stop(pwait_info_wifimode);
    agcmd_property_wait_stop(pwait_info_mlan_ip);
    agcmd_property_wait_stop(pwait_info_wlan_ip);
    agcmd_property_wait_stop(pwait_info_ap_ip);
    agcmd_property_wait_stop(pwait_info_language);
    agcmd_property_wait_stop(pwait_info_resolution);
    agcmd_property_wait_stop(pwait_info_rec_mode);
    agcmd_property_wait_stop(pwait_info_ptl_interval);
    agcmd_property_wait_stop(pwait_info_usb);
}
