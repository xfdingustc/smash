#ifndef __H_Gobal_macro__
#define __H_Gobal_macro__

#define IP_MAX_SIZE (40)
#define MaxPathStringLength 256
//#define GENERATE_TOOLFILE 1
#define AUDIO_CHANNEL_MONO	1
//#define MailThreadAvaliable 1

#define SERVICE_UPNP_VERSION	"1.1.01"

#define timezongLink "/castle/localtime"
#define timezongDir "/usr/share/zoneinfo/TS/"
#define defaultMicGain 5

#ifdef PLATFORM23
#define APPLICATION_VERSION "23b 1.0.24"
#define SW_ROTATE_LOCK_MIRROR 0 // 0 rotate 180, 1, mirror 180, 2 hdr on,off
#define support_motorControl 1  
#define TRANSFLUENT
#define GPS_ACU_DEFAULT_THRESHOLD "0"
#define RGB656UI 1
#define BitrateForHQ720p		6000000
#define BitrateForNormal720p	4000000
#define BitrateForSoso720p		2000000
#define BitrateForLong720p		1000000
#define support_steper_motor 1
#endif 

#ifdef PLATFORM22D
#define APPLICATION_VERSION "22d 1.0.22"
#define SW_ROTATE_LOCK_MIRROR 2 
#define TRANSFLUENT
#define GPS_ACU_DEFAULT_THRESHOLD "0"
#define RGB656UI 1 
#define BitrateForHQ720p		6000000
#define BitrateForNormal720p	3000000
#define BitrateForSoso720p		2000000
#define BitrateForLong720p		1000000
#endif 

#ifdef PLATFORM22A
#define APPLICATION_VERSION "22a 2.0.1"
#define SW_ROTATE_LOCK_MIRROR 1 
#define TRANSFLUENT
#define WifiSwitchForMode 1 // else it is for switch on/off
#define GPS_ACU_DEFAULT_THRESHOLD "0" //"10000"
#define RGB656UI 1 
#define SupportNewVDBAVF 1
#ifdef  SupportNewVDBAVF
	#define APPLICATION_VERSION "22a 2.0.1"
#else
	#define APPLICATION_VERSION "22a 1.0.27"
#endif
#define constantBitrate 1
//#define GPSManualFilterOn 1
#define BitrateForHQ720p		6000000
#define BitrateForNormal720p	4000000
#define BitrateForSoso720p		2000000
#define BitrateForLong720p		1000000
//#define noupnp 1
#endif 

#ifdef PLATFORMDIABLO
#define APPLICATION_VERSION "dbl 2.0.27"
#define SW_ROTATE_LOCK_MIRROR 0 // 0 rotate 180, 1, mirror 180, 2 hdr on,off
#define GPS_ACU_DEFAULT_THRESHOLD "0"
//#define XMPPENABLE
#define BitUI 1 
#define BitUITestServicePort 30004
#define printBootTime 1
#define noupnp 1
#define SupportNewVDBAVF 1
#define constantBitrate 1
//#define Support2ndTS 1
#define BitrateForHQ720p 		5500000
#define BitrateForNormal720p	4000000
#define BitrateForSoso720p		3000000
#define BitrateForLong720p		2000000
#define DownKeyForGPSTest			1
#define DiabloBoardVersion2			1
#endif

#define GSensorTestThread 1
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

#define SET_DEFAULTTIME_2013 1

#define FileNumInGroup 15
#define POOL_PATH  "/tmp/mmcblk0p1/"
#define MARKED_PATH  "/tmp/mmcblk0p1/marked/"
#define Internal_PATH "/backup/"
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



#define SotrageSizePropertyName "temp.mmcblk0p1.total_size"
#define SotrageStatusPropertyName "temp.mmcblk0p1.status"
#define SotrageFreePropertyName  "temp.mmcblk0p1.free_size"
#define LastFilePropertyName "temp.mmcblk0p1.last_file"
#define audioStatusPropertyName  "temp.audio.status"
#define audioGainPropertyName "system.boot.capvol"
#define wifiStatusPropertyName  "temp.wifi.status"
#define wifiModePropertyName  "temp.wifi.mode"
//status: "Unknown", "Charging", "Discharging", "Not charging", "Full"
#define powerstateName "temp.power_supply.status"
//online: 1, 0
#define propPowerSupplyOnline "temp.power_supply.online"
//voltage_now: real battery voltage (mV), eg, 3700
#define propPowerSupplyBV "temp.power_supply.voltage_now"
//capacity_level: "Unknown", "Critical", "Low", "Normal", "High", "Full"
#define propPowerSupplyBCapacityLevel "temp.power_supply.capacity_level"
// in percents: x%
#define propPowerSupplyBCapacity "temp.power_supply.capacity"
#define PROP_POWER_SUPPLY_CAPACITY_FULL		100
#define PROP_POWER_SUPPLY_CAPACITY_HIGH		80
#define PROP_POWER_SUPPLY_CAPACITY_NORMAL	50
#define PROP_POWER_SUPPLY_CAPACITY_LOW		20
#define PROP_POWER_SUPPLY_CAPACITY_CRITICAL	5

#define WIFI_CLIENT_KEY "WLAN"
#define WIFI_AP_KEY "AP"
#define WIFI_KEY "system.boot.wifi_mode"
//#define WIFI_MODE_KEY "system.boot.wifi_mode"
#define WIFI_SWITCH_STATE "system.boot.wifi_sw"


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


#endif
