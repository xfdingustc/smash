#ifndef _CAM_DEVICE_PARAMETER_H_
#define _CAM_DEVICE_PARAMETER_H_

typedef enum _YK_COMMON_STATUS_ITEM {
    YK_STATUS_OFF = 0,
    YK_STATUS_ON
} YK_COMMON_STATUS_ITEM;

typedef enum _YK_VIDEO_SUBMODE_ITEM {
    YK_VIDEO_SUBMODE_VIDEO = 0,
    YK_VIDEO_SUBMODE_VIDEI_PHOTO,
    YK_VIDEO_SUBMODE_CYCLE_RECARD,
    YK_VIDEO_SUBMODE_CARDV,
    YK_VIDEO_SUBMODE_MAX
} YK_VIDEO_SUBMODE_ITEM;

typedef enum _YK_VIDEO_RESOLUTION_ITEM {
    YK_VIDEO_RESOLUTION_QVGA = 0,
    YK_VIDEO_RESOLUTION_VGA,
    YK_VIDEO_RESOLUTION_WVGA,
    YK_VIDEO_RESOLUTION_720P,
    YK_VIDEO_RESOLUTION_1080P,
    YK_VIDEO_RESOLUTION_3MHD,
    YK_VIDEO_RESOLUTION_QHD,
    YK_VIDEO_RESOLUTION_UHD,
    YK_VIDEO_RESOLUTION_4K,
    YK_VIDEO_RESOLUTION_MAX
} YK_VIDEO_RESOLUTION_ITEM;

typedef enum _YK_VIDEO_FPS_ITEM {
    YK_VIDEO_FPS_15 = 0,
    YK_VIDEO_FPS_24,
    YK_VIDEO_FPS_30,
    YK_VIDEO_FPS_60,
    YK_VIDEO_FPS_120,
    YK_VIDEO_FPS_240,
    YK_VIDEO_FPS_MAX
} YK_VIDEO_FPS_ITEM;

typedef enum _YK_VIDEO_RESOLUTION_FPS_ITEM {
    YK_VIDEO_QVGA_30FPS = 0,
    YK_VIDEO_VGA_30FPS,
    YK_VIDEO_VGA_240FPS,
    YK_VIDEO_WVGA_30FPS,
    YK_VIDEO_720P_30FPS,
    YK_VIDEO_720P_60FPS,
    YK_VIDEO_720P_120FPS,
    YK_VIDEO_720P_240FPS,
    YK_VIDEO_1080P_30FPS,
    YK_VIDEO_1080P_60FPS,
    YK_VIDEO_1080P_120FPS,
    YK_VIDEO_3MHD_30FPS,
    YK_VIDEO_QHD_30FPS,
    YK_VIDEO_27K_30FPS,
    YK_VIDEO_UHD_24FPS,
    YK_VIDEO_4K_15FPS,
    YK_VIDEO_4K_30FPS,
    YK_VIDEO_4K_60FPS,
    YK_VIDEO_MAX
} YK_VIDEO_RESOLUTION_FPS_ITEM;

typedef enum _YK_FOV_ITEM {
    YK_FOV_WIDE = 0,
    YK_FOV_MEDIUM,
    YK_FOV_NARROW,
    YK_FOV_CROP,
    YK_FOV_MAX
} YK_FOV_ITEM;

typedef enum _YK_VIDEO_TIMELAPSE_RATE_ITEM {
    YK_VIDEO_TIMELAPSE_CLOSE = 0,
    YK_VIDEO_TIMELAPSE_100MS,
    YK_VIDEO_TIMELAPSE_200MS,
    YK_VIDEO_TIMELAPSE_500MS,
    YK_VIDEO_TIMELAPSE_1S,
    YK_VIDEO_TIMELAPSE_5S,
    YK_VIDEO_TIMELAPSE_10S,
    YK_VIDEO_TIMELAPSE_30S,
    YK_VIDEO_TIMELAPSE_1M,
    YK_VIDEO_TIMELAPSE_5M,
    YK_VIDEO_TIMELAPSE_10M,
    YK_VIDEO_TIMELAPSE_30MIN,
    YK_VIDEO_TIMELAPSE_1H,
    YK_VIDEO_TIMELAPSE_2H,
    YK_VIDEO_TIMELAPSE_3H,
    YK_VIDEO_TIMELAPSE_1D,
    YK_VIDEO_TIMELAPSE_MAX
} YK_VIDEO_TIMELAPSE_RATE_ITEM;

typedef enum _YK_VIDEO_LOOPING_ITEM {
    YK_VIDEO_LOOPING_OFF = 0,
    YK_VIDEO_LOOPING_3MIN,
    YK_VIDEO_LOOPING_5MIN,
    YK_VIDEO_LOOPING_10MIN,
    YK_VIDEO_LOOPING_MAX
} YK_VIDEO_LOOPING_ITEM;

typedef enum _YK_VIDEO_PIV_ITEM {
    YK_VIDEO_PIV_5S = 0,
    YK_VIDEO_PIV_10S,
    YK_VIDEO_PIV_30S,
    YK_VIDEO_PIV_MAX
} YK_VIDEO_PIV_ITEM;

typedef enum _YK_SPOT_METER_ITEM {
    YK_METERING_CENTER = 0,
    YK_METERING_MULTI,
    YK_METERING_SPOT,
    YK_METERING_AIAE,
    YK_METERING_MAX
} YK_SPOT_METER_ITEM;

typedef enum _YK_EFFECT_ENHANCE_ITEM {
    YK_EFFECT_NONE = 0,
    YK_EFFECT_SHADING,
    YK_EFFECT_FANTASY,
    YK_EFFECT_DISTORTION,
    YK_EFFECT_MIRROR,
    YK_EFFECT_ROCK,
    YK_EFFECT_SKETCH,
    YK_EFFECT_MINIATURE,
    YK_EFFECT_SPARKLE,
    YK_EFFECT_FISHEYE,
    YK_EFFECT_COLORPENCIL,
    YK_EFFECT_MAX
} YK_EFFECT_ENHANCE_ITEM;

typedef enum _YK_WHITE_BALANCE_ITEM {
    YK_WB_AUTO = 0,
    YK_WB_DAYLIGHT,
    YK_WB_CLOUDY,
    YK_WB_TUNGSTEN,
    YK_WB_FLUORESCENT,
    YK_WB_MAX
} YK_WHITE_BALANCE_ITEM;

typedef enum _YK_COLOR_ITEM {
    YK_COLOR_STANDARD = 0,
    YK_COLOR_MONOCHROME,
    YK_COLOR_SEPIR,
    YK_COLOR_MAX
} YK_COLOR_ITEM;

typedef enum _YK_ISO_ITEM {
    YK_ISO_AUTO = 0,
    YK_ISO_100,
    YK_ISO_200,
    YK_ISO_400,
    YK_ISO_800,
    YK_ISO_1600,
    YK_ISO_3200,
    YK_ISO_6400,
    YK_ISO_12800,
    YK_ISO_25600,
    YK_ISO_51200,
    YK_ISO_MAX
} YK_ISO_ITEM;

typedef enum _YK_SHARPNESS_ITEM {
    YK_SHARPNESS_HIGH = 0,
    YK_SHARPNESS_MEDIUM,
    YK_SHARPNESS_LOW,
    YK_SHARPNESS_MAX
} YK_SHARPNESS_ITEM;

typedef enum _YK_EV_ITEM {
    YK_EV_N30 = 0,
    YK_EV_N26,
    YK_EV_N23,
    YK_EV_N20,
    YK_EV_N16,
    YK_EV_N13,
    YK_EV_N10,
    YK_EV_N06,
    YK_EV_N03,
    YK_EV_0,
    YK_EV_P03,
    YK_EV_P06,
    YK_EV_P10,
    YK_EV_P13,
    YK_EV_P16,
    YK_EV_P20,
    YK_EV_P23,
    YK_EV_P26,
    YK_EV_P30,
    YK_EV_MAX
} YK_EV_ITEM;

typedef enum _YK_PHOTO_SUBMODE_ITEM {
    YK_PHOTO_SUBMODE_SINGLE = 0,
    YK_PHOTO_SUBMODE_MULTI,
    YK_PHOTO_SUBMODE_MAX
} YK_PHOTO_SUBMODE_ITEM;

typedef enum _YK_PHOTO_RESOLUTION_ITEM {
    YK_PHOTO_RESOLUTION_1M = 0,
    YK_PHOTO_RESOLUTION_VGA,
    YK_PHOTO_RESOLUTION_2M,
    YK_PHOTO_RESOLUTION_3M,
    YK_PHOTO_RESOLUTION_5M,
    YK_PHOTO_RESOLUTION_8M,
    YK_PHOTO_RESOLUTION_10M,
    YK_PHOTO_RESOLUTION_12M,
    YK_PHOTO_RESOLUTION_16M,
    YK_PHOTO_RESOLUTION_20M,
    YK_PHOTO_RESOLUTION_MAX
} YK_PHOTO_RESOLUTION_ITEM;

typedef enum _YK_PHOTO_CONTINUOUS_RATE_ITEM {
    YK_PHOTO_CONTINUOUS_OFF = 0,
    YK_PHOTO_CONTINUOUS_3P,
    YK_PHOTO_CONTINUOUS_5P,
    YK_PHOTO_CONTINUOUS_10P,
    YK_PHOTO_CONTINUOUS_MAX
} YK_PHOTO_CONTINUOUS_RATE_ITEM;

typedef enum _YK_PHOTO_EXPOSURE_TIME_ITEM {
    YK_PHOTO_EXPOSURE_AUTO = 0,
    YK_PHOTO_EXPOSURE_2S,
    YK_PHOTO_EXPOSURE_5S,
    YK_PHOTO_EXPOSURE_10S,
    YK_PHOTO_EXPOSURE_MAX
} YK_PHOTO_EXPOSURE_TIME_ITEM;

typedef enum _YK_PHOTO_COUNTDOWN_INTERVEL_ITEM {
    YK_PHOTO_INTERVEL_CLOSE = 0,
    YK_PHOTO_INTERVEL_3S,
    YK_PHOTO_INTERVEL_5S,
    YK_PHOTO_INTERVEL_10S,
    YK_PHOTO_INTERVEL_20S,
    YK_PHOTO_INTERVEL_MAX
} YK_PHOTO_COUNTDOWN_INTERVEL_ITEM;

typedef enum _YK_PHOTO_CORDINATE_ITEM {
    YK_PHOTO_CORDINATE_FORMAL = 0,
    YK_PHOTO_CORDINATE_FISH,
    YK_PHOTO_CORDINATE_MAX
} _YK_PHOTO_CORDINATE_ITEM;

typedef enum _YK_PHOTO_DATETIME_ITEM {
    YK_PHOTO_DATETIME_OFF = 0,
    YK_PHOTO_DATETIME_DATE_ONLY,
    YK_PHOTO_DATETIME_DATE_TIME,
    YK_PHOTO_DATETIME_MAX
} YK_PHOTO_DATETIME_ITEM;

typedef enum _YK_STREAM_SIZE_ITEM {
    YK_STREAM_FHD = 0,
    YK_STREAM_HD,
    YK_STREAM_WVGA,
    YK_STREAM_VGA_4_3,
    YK_STREAM_VGA_16_9,
    YK_STREAM_QVGA,
    YK_STREAM_MAX
} YK_STREAM_SIZE_ITEM;

typedef enum _YK_STREAM_WINDOW_SIZE_ITEM {
    YK_STREAM_WINDOW_480X270_16_9 = 0,
    YK_STREAM_WINDOW_640X360_16_9,
    YK_STREAM_WINDOW_MAX
} YK_STREAM_WINDOW_SIZE_ITEM;

typedef enum _YK_STREAM_BIT_RATE_ITEM {
    YK_STREAM_BIT_RATE_800K = 0,
    YK_STREAM_BIT_RATE_1000K,
    YK_STREAM_BIT_RATE_1600K,
    YK_STREAM_BIT_RATE_2000K,
    YK_STREAM_BIT_RATE_2400K,
    YK_STREAM_BIT_RATE_3200K,
    YK_STREAM_BIT_RATE_6400K,
    YK_STREAM_BIT_RATE_9600K,
    YK_STREAM_BIT_RATE_MAX
} YK_STREAM_BIT_RATE_ITEM;

typedef enum _YK_LCD_BRIGHTNESS_ITEM {
    YK_LCD_BRIGHTNESS_HIGH = 0,
    YK_LCD_BRIGHTNESS_MEDIUM,
    YK_LCD_BRIGHTNESS_LOW,
    YK_LCD_BRIGHTNESS_MAX
} YK_LCD_BRIGHTNESS_ITEM;

typedef enum _YK_LCD_SLEEP_ITEM {
    YK_LCD_SLEEP_CLOSE = 0,
    YK_LCD_SLEEP_10S,
    YK_LCD_SLEEP_20S,
    YK_LCD_SLEEP_30S,
    YK_LCD_SLEEP_1M,
    YK_LCD_SLEEP_2M,
    YK_LCD_SLEEP_MAX
} YK_LCD_SLEEP_ITEM;

typedef enum _YK_ORIENTATION_ITEM {
    YK_ORIENTATION_UP = 0,
    YK_ORIENTATION_DOWN,
    YK_ORIENTATION_MAX
} YK_ORIENTATION_ITEM;

typedef enum _YK_BEEP_VOLUME_ITEM {
    YK_BEEP_VOLUME_OFF = 0,
    YK_BEEP_VOLUME_MEDIUM,  // 50% percent
    YK_BEEP_VOLUME_HIGH,     //100 percent
    YK_BEEP_VOLUME_MAX
} YK_BEEP_VOLUME_ITEM;

typedef enum _YK_AUTO_POWERDOWN_ITEM {
    YK_AUTO_POWERDOWN_OFF = 0,
    YK_AUTO_POWERDOWN_30S,
    YK_AUTO_POWERDOWN_1M,
    YK_AUTO_POWERDOWN_2M,
    YK_AUTO_POWERDOWN_3M,
    YK_AUTO_POWERDOWN_5M,
    YK_AUTO_POWERDOWN_10M,
    YK_AUTO_POWERDOWN_MAX
} YK_AUTO_POWERDOWN_ITEM;

typedef enum _YK_WIRELESS_MODE_ITEM {
    YK_WIRELESS_OFF = 0,
    YK_WIRELESS_APP,
    YK_WIRELESS_RC,
    YK_WIRELESS_SMART,
    YK_WIRELESS_MAX
} YK_WIRELESS_MODE_ITEM;

typedef enum _YK_CAMERA_MODE_ITEM {
    YK_CAMERA_MODE_PHOTO = 0,
    YK_CAMERA_MODE_VIDEO,
    YK_CAMERA_MODE_PLAYBACK,
    YK_CAMERA_MODE_MULTISHOT,
    YK_CAMERA_MODE_MAX
} YK_CAMERA_MODE_ITEM;


#endif
