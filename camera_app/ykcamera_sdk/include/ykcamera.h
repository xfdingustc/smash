/*
 ==========================================================
 Name        : youku_camera.h
 Author      : huangyeqing@youku.com
 Version     :
 Copyright   : Your copyright notice youku original
 Description : interface for camera
 ==========================================================
 */

#ifndef _YOUKU_CAMERA_H_
#define _YOUKU_CAMERA_H_

#include "ykcamera_common.h"

CXX_EXTERN_BEGIN


/*
sdk 接口参数的结构体
wsp 是webserver的参数
cam_status是相机状态结构
sdk_debug_level debug版本的日志开关
*/
	typedef struct _ykcamera_interface ykcamera_interface;
	struct _ykcamera_interface {
		web_sever_param *wsp;
		camera_status *cam_status;
		int sdk_debug_level	;
	};

/*
sdk初始化接口，初始化ｓｄｋ的相关结构
cam_iface：	初始化相关的参数，参见youku_camera_interface定义
*/

	YKCAMSDK_API	int youku_camera_init(ykcamera_interface * cam_iface);
/*
sdk释放接口，主要是释放ｓｄｋ的资源
*/
	YKCAMSDK_API	int youku_camera_deinitialize(void);


/*
sdk运行接口
*/
	YKCAMSDK_API	int youku_camera_start(void);

CXX_EXTERN_END

#endif
