#ifndef __H_Gobal_macro__
#define __H_Gobal_macro__

#include "aglog.h"

#define IP_MAX_SIZE (40)
#define MaxPathStringLength 256
//#define GENERATE_TOOLFILE 1
#define AUDIO_CHANNEL_MONO  1
//#define MailThreadAvaliable 1


// API are defined in xml_cmd/ccam_cmds.h
#ifdef PLATFORMHACHI
    #define API_VERSION    "1.5"
#else
    #define API_VERSION    "1.2"
#endif

// upgrade release build version each time do release
#if defined(PLATFORMHACHI)
    #define Release_Build_VERSION    "08"
#elif defined(PLATFORMDIABLO)
    #define Release_Build_VERSION    "04"
#elif defined(PLATFORM22A)
    #define Release_Build_VERSION    "06"
#elif defined(PLATFORMHELLFIRE)
    #define Release_Build_VERSION    "01"
#endif

// API version + release build version
#define SERVICE_UPNP_VERSION        (API_VERSION"."Release_Build_VERSION)


#define timezongLink "/castle/localtime"
#define timezongDir "/usr/share/zoneinfo/TS/"
#define defaultMicGain 7


#ifdef PLATFORM22A
#define SW_ROTATE_LOCK_MIRROR 1
#define TRANSFLUENT
#define WifiSwitchForMode 1 // else it is for switch on/off
#define GPS_ACU_DEFAULT_THRESHOLD "0" //"10000"
#define RGB656UI 1
#define SupportNewVDBAVF 1
#ifdef  SupportNewVDBAVF
    #define APPLICATION_VERSION "22a "SERVICE_UPNP_VERSION
#else
    #define APPLICATION_VERSION "22a 1.0.27"
#endif
#define constantBitrate 1
//#define GPSManualFilterOn 1
#define BitrateForHQ720p        6000000
#define BitrateForNormal720p    4000000
#define BitrateForSoso720p      2000000
#define BitrateForLow720p       1000000
//#define noupnp 1
#define Camera_name_default     ""
#endif

#ifdef PLATFORMDIABLO
#define APPLICATION_VERSION "dbl 2.0.27"
#define SW_ROTATE_LOCK_MIRROR 0 // 0 rotate 180, 1, mirror 180, 2 hdr on,off
#define GPS_ACU_DEFAULT_THRESHOLD "0"
//#define XMPPENABLE
#define BitUI 1
#define NEW_UI 1
#define BitUITestServicePort 30004
#define printBootTime 1
#define noupnp 1
#define SupportNewVDBAVF 1
#define constantBitrate 1
#define Support2ndTS 1
#define BitrateForHQ720p        5500000
#define BitrateForNormal720p    4000000
#define BitrateForSoso720p      3000000
#define BitrateForLow720p       2000000
#define DownKeyForGPSTest       1
#define DiabloBoardVersion2     1
#define GSensorTestThread       1
#define Camera_name_default     "Vidit Camera"
#endif

#ifdef PLATFORMHELLFIRE
#define APPLICATION_VERSION "hellfire 2.0.27"
#define SW_ROTATE_LOCK_MIRROR 0 // 0 rotate 180, 1, mirror 180, 2 hdr on,off
#define GPS_ACU_DEFAULT_THRESHOLD "0"
#define BitUI 1
#define NEW_UI 1
#define printBootTime 1
#define noupnp 1
#define SupportNewVDBAVF 1
#define constantBitrate 1
#define Support2ndTS 1
#define GSensorTestThread 1
#define Camera_name_default    "Vidit Camera"
#endif

#ifdef PLATFORMHACHI
#define APPLICATION_VERSION "hachi 2.0.27"
#define SW_ROTATE_LOCK_MIRROR 0 // 0 rotate 180, 1, mirror 180, 2 hdr on,off
#define GPS_ACU_DEFAULT_THRESHOLD "0"
#define CircularUI 1
#define printBootTime 1
#define noupnp 1
#define SupportNewVDBAVF 1
#define constantBitrate 1
#define Support2ndTS 1
#define DownKeyForGPSTest       1
//#define GSensorTestThread       1

#define Brightness_MAX         250
#define Brightness_MIN         100
#define Brightness_Step_Value  15

#define Camera_name_default    "Waylens Camera"
#endif


#define DateFormat_Euro 1

//#define IPInforTest 1
//#define Support2ndTS 1
//
#define GPSManualFilterProp "system.ui.gpsfilter"

#ifdef XMPPENABLE
    #define QRScanEnable 1
    #define NoCheckSDCard 1
#endif

//#define WriteBufferOverFlowEnable 1
//#define support_obd
//#define SupportBlueTooth 1

#define SET_DEFAULTTIME  1

#define FileNumInGroup   15
#define POOL_PATH        "/tmp/mmcblk0p1/"
#define MARKED_PATH      "/tmp/mmcblk0p1/marked/"
#define Internal_PATH    "/backup/"
#define XMPP_DOMAIN_NAME "xmpp.cam2cloud.com"

#define MAXFILE_IN_MARKED_LIST 512
#define MAXFILE_IN_TMP_LIST 512
#define MaxGroupNum 40 //(MAXFILE_IN_MARKED_LIST /FileNumInGroup )
#define FreeSpaceThreshold (512*1024*1024)
#define StorageSizeLimitateion (1024*1024*1024)
#define CircleDirPercentage 75
#define MarkDirPercentage 20
#define NameLength 128
#define MovieFileExtend ".mp4"
#define PosterFileExtend ".jpg"

#define MBPS 1000000

#define SotrageStatusPropertyName "temp.mmcblk0p1.status"
#define USBSotragePropertyName    "temp.usb_device.fs_status"
#define audioStatusPropertyName   "system.boot.audio"
#define audioGainPropertyName     "system.boot.capvol"

#define Prop_Mic_Status           "system.audio.mic_status"
#define Prop_Mic_Volume           "system.audio.mic_volume"
#define Prop_Mic_wnr              "system.audio.mic_wnr"
#define Prop_Spk_Status           "system.audio.spk_status"
#define Prop_Spk_Volume           "system.audio.spk_volume"
#define DEFAULT_SPK_VOLUME        "8"

//status: "Unknown", "Charging", "Discharging", "Not charging", "Full"
#define powerstateName  "temp.power_supply.status"
//online: 1, 0
#define propPowerSupplyOnline  "temp.power_supply.online"
//voltage_now: real battery voltage (mV), eg, 3700
#define propPowerSupplyBV  "temp.power_supply.voltage_now"
//capacity_level: "Unknown", "Critical", "Low", "Normal", "High", "Full"
#define propPowerSupplyBCapacityLevel  "temp.power_supply.capacity_level"
// in percents: x%
#define propPowerSupplyBCapacity  "temp.power_supply.capacity"
#define propPowerSupplyCharge     "temp.power_supply.charge_now"
#define propPowerSupplyCurrent    "temp.power_supply.current_avg"

#define propIsBTSupported      "prebuild.board.support_bt"
#define propBTStatus           "system.bt.status"
#define propBTTempStatus       "temp.bt.status"
#define propOBDMac             "system.bt.obd_mac"
#define propHIDMac             "system.bt.hid_mac"
#define propBTTempOBDStatus    "temp.bt.obd_status"
#define propBTTempHIDStatus    "temp.bt.hid_status"
#define propBTTempOBDMAC       "temp.bt.obd_mac"
#define propBTTempHIDMAC       "temp.bt.hid_mac"
#define Prop_HID_MAC_Backup    "system.bt.hid_mac_backup"
#define Prop_OBD_MAC_Backup    "system.bt.obd_mac_backup"
#define Prop_RC_Battery_level  "temp.bt.hid_batt_level"
#define Prop_Disable_RC_Batt_Warning  "system.bt.rc_batt_warning"
#define RC_Batt_Warning_Level  30

#define PROP_POWER_SUPPLY_CAPACITY_FULL     100
#define PROP_POWER_SUPPLY_CAPACITY_HIGH     80
#define PROP_POWER_SUPPLY_CAPACITY_NORMAL   50
#define PROP_POWER_SUPPLY_CAPACITY_LOW      20
#define PROP_POWER_SUPPLY_CAPACITY_CRITICAL 5

#define WIFI_CLIENT_KEY         "WLAN"
#define WIFI_AP_KEY             "AP"
#define WIFI_MULTIROLE          "MULTIROLE"
#define WIFI_MR_KEY             "AP+WLAN"
#define WIFI_MODE_KEY           "system.boot.wifi_mode"
#define WIFI_SWITCH_STATE       "system.boot.wifi_sw"
#define WIFI_MR_MODE            "system.boot.wifi_mr_mode"
#define wifiStatusPropertyName  "temp.wifi.status"
#define wifiModePropertyName    "temp.wifi.mode"
#define WIFI_MLAN_IP            "temp.mlan0.ip"
#define WIFI_WLAN_IP            "temp.wlan0.ip"
#define WIFI_AP_IP              "temp.uap0.ip"

#define PropName_camera_name   "upnp.camera.name"
#define PropName_rec_resoluton "upnp.record.Resolution"
#define PropName_rec_quality   "upnp.record.quality"
#define PropName_rec_colorMode "upnp.record.colorMode"
#define PropName_rec_recMode   "upnp.record.recordMode"
#define PropName_still_capture "system.caps.stillcapture"

#define PropName_Language      "system.ui.language"

#define prebuild_board_vin0    "prebuild.board.vin0"
#define prebuild_board_version "prebuild.board.version"
#define PropName_prebuild_bsp  "prebuild.system.bsp"

#define PropName_CountDown     "system.photo.countdown"
#define PropName_VideoLapse    "system.video.timelapse"
#define PropName_PhotoLapse    "system.photo.timelapse"

#define PropName_Mark_before   "system.mark.before_s"
#define PropName_Mark_after    "system.mark.after_s"

#define PropName_Rotate_Horizontal  "system.rotate.horizontal"
#define PropName_Rotate_Vertical    "system.rotate.vertical"
#define PropName_Rotate_Mode        "camera.settings.rotate_mode"
#define Rotate_Mode_Auto            "auto"
#define Rotate_Mode_Normal          "normal"
#define Rotate_Mode_180             "180"

#define PropName_Udc_Power_Delay    "camera.settings.udc_power_delay"
#define Udc_Power_Delay_Default     "30s"

#define PropName_Auto_Brightness    "camera.settings.auto_brightness"
#define PropName_Brighness          "camera.screen.brightness"
#define Default_Brighness           "200"
#define PropName_Display_Off_Time   "camera.settings.display_off_time"
#define Default_Display_Off_Time    "60s"
#define PropName_Screen_saver_style "camera.settings.screen_saver_style"
#define Default_Screen_saver_style  "Dot"

#define PropName_Time_Format        "camera.settings.time_format"
#define PropName_Date_Format        "camera.settings.date_format"

#define PropName_Settings_OBD_Demo  "camera.settings.obd_demo"

#define PropName_Board_Temprature     "temp.power_supply.temp"
#define PropName_Board_Temp_warning   "prebuild.board.temp_high_warn"
#define PropName_Board_Temp_critical  "prebuild.board.temp_high_critical"
#define PropName_Board_Temp_high_off  "prebuild.board.temp_high_off"
#define Warning_default               "85"
#define Critical_default              "89"
#define High_off_default              "91"

#define PropName_OBD_VIN              "temp.bt.obd_vin"
#define PropName_VIN_MASK             "camera.settings.vin_mask"
#define Default_VIN_MASK              "xxxxxxxx-xx------"

#define PropName_Units_System         "camera.settings.units_system"
#define Imperial_Units_System         "Imperial Units"
#define Metric_Units_System           "Metric Units"

#define PropName_Auto_Hotspot         "camera.settings.auto_hotspot"
#define Auto_Hotspot_Enabled          "enabled"
#define Auto_Hotspot_Disabled         "disabled"

#define PropName_60MPH_Test_Score           "camera.settings.60mph_scores"
#define PropName_100KMH_Test_Score          "camera.settings.100kmh_scores"
#define PropName_60MPH_Test_Score_Auto      "camera.settings.auto_60mph_scores"
#define PropName_100KMH_Test_Score_Auto     "camera.settings.auto_100kmh_scores"


#ifdef TRANSFLUENT
#define DEVICE_Name "Wifi Car DV"
#define DEVICE_Type "Transfluent"
#define DEVICE_Version "1.0"
#define Device_manufatory "Transfluent"
#define Device_manufatory_url "http://www.transfluent.com.cn"
#else
#define DEVICE_Name "Cloud Camera"
#define DEVICE_Type "Transee"
#define DEVICE_Version "1.0"
#define Device_manufatory "Transee"
#define Device_manufatory_url "http://www.cam2cloud.com"
#endif

// comment it to disable debug log
#define DEBUG_LOG 1
#ifdef DEBUG_LOG
#define CAMERALOG(fmt, args...) \
        AGLOG_WARNING("%s() line %d "fmt"\n", __FUNCTION__, __LINE__, ##args)
#else
#define CAMERALOG(fmt, args...)
#endif

#define maxMenuStrinLen 32
#define screenSaveTime 30

enum CAM_WORKING_MODE {
    CAM_MODE_VIDEO = 0,
    CAM_MODE_SLOW_MOTION,
    CAM_MODE_VIDEO_LAPSE,
    CAM_MODE_PHOTO,
    CAM_MODE_TIMING_SHOT,
    CAM_MODE_PHOTO_LAPSE,
    CAM_MODE_RAW_PICTURE,
    CAM_MODE_NUM,
};

#endif
