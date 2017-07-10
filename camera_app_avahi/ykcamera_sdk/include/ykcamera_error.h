#ifndef __YK_CAM_ERROR_H__
#define __YK_CAM_ERROR_H__
#include "ykcamera_common.h"

CXX_EXTERN_BEGIN
//全局错误码表
enum error_code {
	YOUKU_CMD_SUCCESS = 0,
    YOUKU_CMD_NOT_SUPPORTED,             //not support this command
    YOUKU_CMD_MODE_ERROR,                //not support in this mode
    YOUKU_CMD_PARAM_OVERFLOW,            //parameter overflow
    YOUKU_CMD_INVALID_DURING_RECORD,     //invalid during recording
    YOUKU_CMD_CARD_REMOVED,              //card removed
    YOUKU_CMD_CARD_LOCKED,               //card locked
    YOUKU_CMD_STORAGE_FULL,              //storage full
    YOUKU_CMD_STREAM_DISABLED,           //stream is disabled
    YOUKU_CMD_VIDEO_TOO_SHORT,           //video file should be recorded more than 1 sec
    YOUKU_CMD_NO_THUMBNAIL,              //no thumbnanil in this file
    YOUKU_CMD_FILE_LOCKED,               //file is read only
    YOUKU_CMD_FILE_ERROR,                //file error
    YOUKU_CMD_FILE_NOT_EXIST,            //file doesn't exist
    YOUKU_CMD_CMD_NOT_EXIST,             //the cmd from app not exist
    YOUKU_CMD_MEM_INSUFFICIENT,          //memory is not enough
    YOUKU_CMD_ERROR_UNKNOWN,
};

CXX_EXTERN_END
#endif // __YK_CAMSDK_COMMON_H__
