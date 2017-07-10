#ifndef __H_Gobal_macro__
#define __H_Gobal_macro__

#include "aglog.h"


// API are defined in xml_cmd/ccam_cmds.h
#define API_VERSION    "1.6"

// upgrade release build version each time do release
#define Release_Build_VERSION    "05"

// API version + release build version
#define SERVICE_UPNP_VERSION        (API_VERSION"."Release_Build_VERSION)


#define timezongLink "/castle/localtime"
#define timezongDir "/usr/share/zoneinfo/TS/"
#define defaultMicGain 7


#define SW_ROTATE_LOCK_MIRROR       0 // 0 rotate 180, 1, mirror 180, 2 hdr on,off
#define GPS_ACU_DEFAULT_THRESHOLD   "0"
#define CircularUI                  1
#define printBootTime               1
#define noupnp                      1
#define SupportNewVDBAVF            1
#define constantBitrate             1
#define Support2ndTS                1
#define DownKeyForGPSTest           1
#define GSensorTestThread           1

#define Brightness_MAX         250
#define Brightness_MIN         100
#define Brightness_Step_Value  15

#define Camera_name_default    "Waylens Camera"


#define DateFormat_Euro 1

#define GPSManualFilterProp "system.ui.gpsfilter"

#define SET_DEFAULTTIME  1

#define MBPS 1000000

#define PropName_Disk             "temp.board.disk"
#define PropName_Pattition        "temp.board.partition"
#define PropName_Media_dir        "temp.board.media_dir"

#define USBSotragePropertyName    "temp.usb_device.fs_status"
#define audioStatusPropertyName   "system.boot.audio"
#define audioGainPropertyName     "system.boot.capvol"

#define Prop_Mic_Status           "system.audio.mic_status"
#define Prop_Mic_Volume           "system.audio.mic_volume"
#define DEFAULT_MIC_VOLUME        "8"
#define Settings_Mic_Volume       "camera.settings.mic_volume"

#define Prop_Mic_wnr              "system.audio.mic_wnr"
#define Prop_Spk_Status           "system.audio.spk_status"
#define Prop_Spk_Volume           "system.audio.spk_volume"
#define DEFAULT_SPK_VOLUME        "8"
#define Settings_Spk_Volume       "camera.settings.spk_volume"

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

#define Prop_OBD_Defect         "temp.bt.obd_dtc"

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
#define Quality_Normal         "Standard"
#define Quality_High           "Super High"

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
#define Mark_before_time       "8"
#define Mark_after_time        "7"

#define PropName_Rotate_Horizontal  "system.rotate.horizontal"
#define PropName_Rotate_Vertical    "system.rotate.vertical"
#define PropName_Rotate_Mode        "camera.settings.rotate_mode"
#define Rotate_Mode_Auto            "auto"
#define Rotate_Mode_Normal          "normal"
#define Rotate_Mode_180             "180"

#define PropName_Udc_Power_Delay    "camera.settings.udc_power_delay"
#define Udc_Power_Delay_Default     "10s"

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

#define PropName_Settings_Temp_Gauge    "camera.settings.temp_gauge"
#define PropName_Settings_Boost_Gauge   "camera.settings.boost_gauge"
#define PropName_Settings_MAF_Gauge     "camera.settings.maf_gauge"
#define PropName_Settings_RPM_Gauge     "camera.settings.rpm_gauge"
#define PropName_Settings_Speed_Gauge   "camera.settings.speed_gauge"
#define SHOW_GAUGE                      "show"
#define HIDE_GAUGE                      "hide"

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
#define Imperial_Units_System         "Imperial"
#define Metric_Units_System           "Metric"

#define PropName_Auto_Hotspot         "camera.settings.auto_hotspot"
#define Auto_Hotspot_Enabled          "On"
#define Auto_Hotspot_Disabled         "Off"

#define PropName_60MPH_Test_Score           "camera.settings.60mph_scores"
#define PropName_100KMH_Test_Score          "camera.settings.100kmh_scores"
#define PropName_60MPH_Test_Score_Auto      "camera.settings.auto_60mph_scores"
#define PropName_100KMH_Test_Score_Auto     "camera.settings.auto_100kmh_scores"

// if set to 1, OBD will connect only when it is on a car mount
#define PropName_OBD_Check_Switch       "system.bt.obd_check_switch"
#define Check_Switch_Enabled            "1"
#define Check_Switch_Disabled           "0"

#define PropName_Auto_Mark              "camera.settings.auto_mark"
#define Auto_Mark_Enabled               "On"
#define Auto_Mark_Disabled              "Off"

#define PropName_Auto_Mark_Sensitivity  "camera.settings.auto_sensitivity"
#define Sensitivity_High                "High"
#define Sensitivity_Normal              "Normal"
#define Sensitivity_Low                 "Low"

#define PropName_Auto_Record            "camera.settings.auto_record"
#define Auto_Record_Enabled             "On"
#define Auto_Record_Disabled            "Off"

#define PropName_060_Length_After       "camera.settings.060_after_s"

#define PropName_support_2397_fps       "camera.settings.2397_fps"
#define Enabled_2397_fps                "enabled"
#define Disabled_2397_fps               "disabled"

#define PropName_No_Write_Record        "temp.record.no_write"
#define No_Write_Record_Enalbed         "enabled"
#define No_Write_Record_Disabled        "disabled"

#define PropName_SN_SSID                "calib.finalize.ssid_name"


#include <sys/time.h>

// comment it to disable debug log
#define DEBUG_LOG 1
#ifdef DEBUG_LOG
#define CAMERALOG(fmt, args...) \
    do { \
        struct tm gtime; \
        struct timeval now_val; \
        gettimeofday(&now_val, NULL); \
        gtime=(*localtime(&now_val.tv_sec)); \
        AGLOG_WARNING("%04d-%02d-%02d %02d:%02d:%02d.%02d %s() line %d "fmt"\n", \
            1900 + gtime.tm_year, 1 + gtime.tm_mon, gtime.tm_mday, \
            gtime.tm_hour, gtime.tm_min, gtime.tm_sec, (int)(now_val.tv_usec / 10000), \
            __FUNCTION__, __LINE__, ##args); \
    } while (0);
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
