/*
 ==========================================================
 Name        : youku_camera.h
 Author      : huangyeqing@youku.com
 Version     :
 Copyright   : Your copyright notice youku original
 Description : interface for camera
 ==========================================================
 */

#ifndef _CAM_SDK_COMMON_H_
#define _CAM_SDK_COMMON_H_

#define YKCAMSDK_API                     __attribute__((visibility("default")))

#ifndef __cplusplus
#define bool                     _Bool
#define CXX_EXTERN_BEGIN
#define CXX_EXTERN_END
#define C_EXTERN                 extern
#else
#define _Bool                    bool
#define CXX_EXTERN_BEGIN         extern "C" {
#define CXX_EXTERN_END           }
#define C_EXTERN
#endif

CXX_EXTERN_BEGIN

#define 	FUNC_ENTRY	clogd("%s entry", __func__)
#define 	FUNC_EXIT	clogd("%s exit", __func__)

#define 	HANDLER_PARAM_NONE	-1
#define 	STATUS_NUMBER_NONE	-1
#define 	STATUS_STRING_NONE	NULL

#define 	PREVIEW_STREAM_PORT	8020

typedef struct _devinfo {
	char *video_ms_url;		//视频模式直播流
	char *photo_ms_url;		//图片模式直播流
	char *firmware_version;	//固件版本号
	char *serial_number;	//序列号
	char *ap_mac;			//ap mac
	char *ap_ssid;			//ap ssid
	char *yksdk_version;	//yk sdk git ci-id
} devinfo;

typedef struct _handler_parameter handler_parameter;

struct _handler_parameter {
	void * scall_back_param;	//回调处理函数的参数，根据信令做类型匹配转换
	void * scall_back_return;	//回调处理函数的返回值，根据信令返回数据
};


/**
function：sdk和相机主服务通信的回调函数，包括状态，信令
command: 是信令id
req_value: 信令值
userdata:	用户数据，sdk初始化的时候赋值，用于回调函数参数
 */
typedef int (*youku_camera_request_handler) (int command, handler_parameter *hp,
											 void *userdata);


typedef struct _stream_enable_param stream_enable_param;
 
struct _stream_enable_param {
	int enable;	//1: start, 0: stop
	char *remote_ip; //app ip
	int	stream_port; //app port

};


//http文件服务相关参数的结构体
typedef struct _hfs_info hfs_info;
struct _hfs_info {
	char *hfs_path;				//已经录制的数据存放的路径，这里用于http 文件服务
    char *hfs_preview_path;
    char *hfs_cut_path;
	char *ota_path;
	char *hfs_port;				//http 文件服务的端口号
};
//web服务的相关参数
typedef struct _web_sever_param {
	hfs_info *hfs_info;			//http文件服务相关参数的结构体
	char *control_port;			//http 控制信令服务的端口号
	youku_camera_request_handler cam_handler; //http 控制信令服务的回调处理函数
	void *user_data;			//用户数据，sdk初始化的时候赋值，用于回调函数参数,参见youku_camera_request_handler
} web_sever_param;
/*
//状态数据的类型
CAM_TYPE_INTEGER:				//整型
CAM_TYPE_FLOAT:					//浮点型
CAM_TYPE_STRING:				//字符串型
*/
typedef enum _camera_status_type {
	CAM_TYPE_INTEGER =0,
	CAM_TYPE_FLOAT,
	CAM_TYPE_STRING
} camera_status_type;

typedef struct _thumbnail_info
{
	unsigned long addr;
	unsigned long size;
}thumbnail_info;

//状态数据项的结构体
/*
status_type: 是状态数据项的类型
ivalue：	联合体的整型值
svalue：	联合体的字符串值
fvalue：	联合体的浮点型值
*/
typedef struct _module_value {
	camera_status_type status_type;
	union {
		int ivalue;
		char *svalue;
		double fvalue;
	};
} module_value;
/*
camera_status: 是状态数据，包括设备的状态和设置的状态
device_status	设备的状态数组
setting_status	设备的设置数组
*/
typedef struct _camera_status camera_status;
struct _camera_status {
	module_value *device_status;
	module_value *setting_status;

};
/*
DEVICE_STATUS_STORAGE_SD: 是sd卡状态类型
*/
typedef enum _DEVICE_STATUS_STORAGE_SD {	
	DEVICE_STATUS_STORAGE_SD_REMOVED=0 ,	//sd卡卸载
	DEVICE_STATUS_STORAGE_SD_INSERTED,		//sd卡插入
	DEVICE_STATUS_STORAGE_SD_LOCKED,		//sd卡锁住
	DEVICE_STATUS_STORAGE_SD_BROKEN			//sd卡已坏
		} DEVICE_STATUS_STORAGE_SD;
/*
设备状态项目定义，详细含义参见设备认证数据项定义
*/
typedef enum _cam_device_status_item {
	DEVICE_INTERNAL_BATTERY_PRESENT,      // 0
	DEVICE_EXTERNAL_BATTERY_PRESENT,
	DEVICE_HOT	,
	DEVICE_BUSY	,
	DEVICE_QUICK_CAPTURE_ACTIVE	,
	DEVICE_DATE_TIME	,				// 5
	VIDEO_PROGRESS_COUNTER,
	SETTING_COMM_STREAM_SUPPORT ,
	STORAGE_SD_STATUS	,
	STORAGE_REMAINING_PHOTOS	,
	STORAGE_REMAINING_VIDEO_TIME	,// 10
	STORAGE_NUM_TOTAL_PHOTOS	,
	STORAGE_NUM_TOTAL_VIDEOS	,
	STORAGE_REMAINING_SPACE	,
	OTA_STATUS	,
	OTA_DOWNLOAD_CANCEL_REQUEST_PENDING	,// 15
	VIDEO_NEW_UPLOAD_VIDEO,
	DEVICE_MAX_ITEM					//设备状态项目数，方便循环使用
} cam_device_status_item;
/*
信令项目定义，详细含义参见设备认证数据项定义
*/
typedef enum _cam_setting_status_item {
	SETTING_VIDEO_DEFAULT_MODE = 0,						 
	SETTING_VIDEO_RESOLUTION	,
	SETTING_VIDEO_FPS	,
	SETTING_VIDEO_RESOLUTION_FPS	,
	SETTING_VIDEO_FOV	,
	SETTING_VIDEO_TIMELAPSE_RATE	,      // 5
	SETTING_VIDEO_LOOPING	,
	SETTING_VIDEO_PIV	,
	SETTING_VIDEO_LOW_LIGHT	,
	SETTING_VIDEO_SPOT_METER	,
	SETTING_VIDEO_EFFECT	,              // 10
	SETTING_VIDEO_WHITE_BALANCE	,
	SETTING_VIDEO_COLOR	,
	SETTING_VIDEO_ISO	,
	SETTING_VIDEO_SHARPNESS	,
	SETTING_VIDEO_EV	,                  // 15
	SETTING_VIDEO_DATETIME_PR	,
	SETTING_VIDEO_AUDIO_RECORD	,
	SETTING_VIDEO_HDR	,
	SETTING_VIDEO_WDR	,
	SETTING_PHOTO_DEFAULT_MODE	,      // 20
	SETTING_PHOTO_RESOLUTION	,
	SETTING_PHOTO_CONTINUOUS_RATE	,
	SETTING_PHOTO_EXPOSURE_TIME	,
	SETTING_PHOTO_SPOT_METER	,
	SETTING_PHOTO_EFFECT	,              // 25
	SETTING_PHOTO_WHITE_BALANCE	,
	SETTING_PHOTO_COLOR	,
	SETTING_PHOTO_ISO	,
	SETTING_PHOTO_SHARPNESS	,
	SETTING_PHOTO_HDR	,                  // 30
	SETTING_PHOTO_WDR	,
	SETTING_PHOTO_EV	,
	SETTING_PHOTO_FOV	,
	SETTING_PHOTO_COUNTDOWN_ERVEL	,
	SETTING_PHOTO_CORDINATE	,          // 35
	SETTING_PHOTO_DATE_PRINT	,
	SETTING_COMM_STREAM_SIZE	,
	SETTING_COMM_STREAM_WINDOW_SIZE	,
	SETTING_COMM_STREAM_BIT_RATE	,
	SETTING_COMM_LCD_BRIGHTNESS	,	// 40
	SETTING_COMM_LCD_LOCK	,
	SETTING_COMM_LCD_SLEEP	,
	SETTING_COMM_ORIENTATION	,
	SETTING_COMM_LED_BLINK	,
	SETTING_COMM_QUICK_CAPTURE	, // 45
	SETTING_COMM_BEEP_VOLUME	,
	SETTING_COMM_AUTO_POWER_DOWN	,
	SETTING_COMM_GPS_MODE	,
	SETTING_COMM_WIRELESS_MODE	,
	SETTING_COMM_RESET	,//50
	SETTING_COMM_FORMAT	,
	SETTING_COMM_WIFI_RESET	,
	SETTING_COMM_SHUTTER	,
	SETTING_COMM_CAMERA_MODE	,
	SETTING_COMM_POWEROFF	,//55
	SETTING_COMM_UPDATE_DOWNLOAD_START	,
	SETTING_COMM_UPDATE_DOWNLOAD_DONE	,
	SETTING_COMM_UPDATE_DOWNLOAD_CANCEL	,
	SETTING_COMM_STORAGE_DELETE_ALL	,
	SETTING_COMM_STORAGE_DELETE	,//60
	SETTING_COMM_STORAGE_DELETE_LAST	,
	SETTING_COMM_STREAM_ENABLE	,
	SETTING_COMM_THUMBNAIL	,
	SETTING_COMM_DEVINFO_GET	,
	SETTING_COMM_MEDIA_LIST	,//65
	SETTING_COMM_DATETIME_SYNC	,
	SETTING_COMM_CLEAN_NEW_VIDEO_FLAG,
	SETTING_MAX_ITEM
} cam_setting_status_item;

/*
信令项目定义，详细含义参见设备认证数据项定义
*/
typedef enum _cam_commands {
	CAM_REQ_CAMERA_STATUS =0	,
	CAM_REQ_VIDEO_DEFAULT_MODE	,
	CAM_REQ_VIDEO_RESOLUTION	,
	CAM_REQ_VIDEO_FPS	,
	CAM_REQ_VIDEO_RESOLUTION_FPS	,
	CAM_REQ_VIDEO_FOV	,                // 5
	CAM_REQ_VIDEO_TIMELAPSE_RATE	,
	CAM_REQ_VIDEO_LOOPING	,
	CAM_REQ_VIDEO_PIV	,
	CAM_REQ_VIDEO_LOW_LIGHT	,
	CAM_REQ_VIDEO_SPOT_METER	,        // 10
	CAM_REQ_VIDEO_EFFECT	,
	CAM_REQ_VIDEO_WHITE_BALANCE	,
	CAM_REQ_VIDEO_COLOR	,
	CAM_REQ_VIDEO_ISO	,
	CAM_REQ_VIDEO_SHARPNESS	,        // 15
	CAM_REQ_VIDEO_EV	,
	CAM_REQ_VIDEO_DATETIME_PRINT	,
	CAM_REQ_VIDEO_AUDIO_RECORD	,
	CAM_REQ_VIDEO_HDR	,
	CAM_REQ_VIDEO_WDR	,                // 20
	CAM_REQ_PHOTO_DEFAULT_MODE	,
	CAM_REQ_PHOTO_RESOLUTION	,
	CAM_REQ_PHOTO_CONTINUOUS_RATE	,
	CAM_REQ_PHOTO_EXPOSURE_TIME	,
	CAM_REQ_PHOTO_SPOT_METER	,        // 25
	CAM_REQ_PHOTO_EFFECT	,
	CAM_REQ_PHOTO_WHITE_BALANCE	,
	CAM_REQ_PHOTO_COLOR	,
	CAM_REQ_PHOTO_ISO	,
	CAM_REQ_PHOTO_SHARPNESS	,        // 30
	CAM_REQ_PHOTO_HDR	,
	CAM_REQ_PHOTO_WDR	,
	CAM_REQ_PHOTO_EV	,
	CAM_REQ_PHOTO_FOV	,
	CAM_REQ_PHOTO_TIMELAPSE_RATE	,   // 35
	CAM_REQ_PHOTO_CORDINATE	,
	CAM_REQ_PHOTO_DATETIME_PRINT	,
	CAM_REQ_COMM_STREAM_SIZE	,
	CAM_REQ_COMM_STREAM_WINDOW_SIZE	,
	CAM_REQ_COMM_STREAM_BIT_RATE	,    // 40
	CAM_REQ_COMM_LCD_BRIGHTNESS	,
	CAM_REQ_COMM_LCD_LOCK	,
	CAM_REQ_COMM_LCD_SLEEP	,
	CAM_REQ_COMM_ORIENTATION	,
	CAM_REQ_COMM_LED_BLINK	,        // 45
	CAM_REQ_COMM_QUICK_CAPTURE	,
	CAM_REQ_COMM_BEEP_VOLUME	,
	CAM_REQ_COMM_AUTO_POWER_DOWN	,
	CAM_REQ_COMM_LOCATE_MODE	,
	CAM_REQ_COMM_WIRELESS_MODE	,    // 50
	CAM_REQ_COMM_RESET_FACTORY	,
	CAM_REQ_COMM_FORMAT_SD	,
	CAM_REQ_COMM_RESET_SSID	,
	CAM_REQ_COMM_SHUTTER	,
	CAM_REQ_COMM_CHANGE_CAMERA_MODE	, // 55
	CAM_REQ_COMM_POWEROFF	,
	CAM_REQ_COMM_UPDATE_DOWNLOAD_START	,
	CAM_REQ_COMM_UPDATE_DOWNLOAD_DONE	,
	CAM_REQ_COMM_UPDATE_DOWNLOAD_CANCEL	,
	CAM_REQ_COMM_STORAGE_DELETE_ALL	,	//60
	CAM_REQ_COMM_STORAGE_DELETE	,
	CAM_REQ_COMM_STORAGE_DELETE_LAST	,
	CAM_REQ_COMM_STREAM_ENABLE	,
	CAM_REQ_COMM_THUMBNAIL	,
	CAM_REQ_COMM_DEVINFO_GET, 			//65
	CAM_REQ_COMM_MEDIA_LIST,
	CAM_REQ_COMM_DATETIME_SYNC,
	CAM_REQ_COMM_CLEAN_NEW_VIDEO_FLAG
} cam_commands;


CXX_EXTERN_END
#endif
