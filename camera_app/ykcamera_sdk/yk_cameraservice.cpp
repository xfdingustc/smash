
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>

#include "ykcamera_log.h"
#include "ykcamera_common.h"
#include "ykcamera_parameter.h"

#include "class_ipc_rec.h"
#include "vdb_cmd.h"

#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"

#include "yk_cameraservice.h"


extern void getStoragePropertyName(char *prop, int len);
extern void DestroyUpnpService(bool bReboot);
extern int agclkd_set_rtc(time_t utc);

const static char* YK_poweroffTimer = "YK poweroff Timer";

static char CAM_DATE[64] = "";

static module_value ykcamera_device_status[DEVICE_MAX_ITEM] = {
    // 0
    { CAM_TYPE_INTEGER, { 0 } },   // DEVICE_INTERNAL_BATTERY_PRESENT
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 5
    { CAM_TYPE_STRING, { NULL } }, // DEVICE_DATE_TIME
    { CAM_TYPE_INTEGER, { 0 } },   // VIDEO_PROGRESS_COUNTER
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { 0 } },   // STORAGE_SD_STATUS
    { CAM_TYPE_INTEGER, { -1 } },

    // 10
    { CAM_TYPE_INTEGER, { 0 } },   // STORAGE_REMAINING_VIDEO_TIME
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { 0 } },   // STORAGE_NUM_TOTAL_VIDEOS
    { CAM_TYPE_INTEGER, { 0 } },   // STORAGE_REMAINING_SPACE
    { CAM_TYPE_INTEGER, { -1 } },

    // 15
    { CAM_TYPE_INTEGER, { -1 } }
};

static module_value ykcamera_setting_status[SETTING_MAX_ITEM] = {
    // 0
    { CAM_TYPE_INTEGER, { 0 } }, // SETTING_VIDEO_DEFAULT_MODE
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { 0 } }, // SETTING_VIDEO_RESOLUTION_FPS
    { CAM_TYPE_INTEGER, { -1 } },

    // 5
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 10
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 15
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { 0 } }, // SETTING_VIDEO_DATETIME_PR
    { CAM_TYPE_INTEGER, { 0 } }, // SETTING_VIDEO_AUDIO_RECORD
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 20
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 25
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 30
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    //35
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 40
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 45
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } }, // SETTING_COMM_AUTO_POWER_DOWN
    { CAM_TYPE_INTEGER, { 1 } }, // SETTING_COMM_GPS_MODE
    { CAM_TYPE_INTEGER, { -1 } }, // SETTING_COMM_WIRELESS_MODE

    // 50
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 55
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 60
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },

    // 65
    { CAM_TYPE_INTEGER, { -1 } },
    { CAM_TYPE_INTEGER, { -1 } },
};


#define MEDIA_LIST_HEADER \
    "{ \"id\": \"1234567890\", \"media\": [ "

#define VIDEO_LIST_HEADER \
    "{ \"dir\": \"video\", \"pv_dir\": \"video\/.preview\", \"fs\": [ "

#define VIDEO_CLIP_INFO \
    "{ \"mname\": \"%d\", \"pv_name\": \"%s\", \"msize\": %d, \"mtime\": %d , \"url\": \"%s\" }"

#define PHOTO_LIST_HEADER \
    "{ \"dir\": \"photo\", \"fs\": ["


YKCameraService* YKCameraService::_cam = NULL;

YKCameraService* YKCameraService::getInstance() {
    if (_cam == NULL) {
        _cam = new YKCameraService();
    }
    return _cam;
}

void YKCameraService::YKPowerOffTimer(void *param) {
    YKCameraService *cam = (YKCameraService*)param;
    if (cam) {
        DestroyUpnpService(false);
    }
}

int YKCameraService::YKCommand_handler(
    int command, handler_parameter *param, void *userdata)
{
    int ret = 0;
    YKCameraService *cam = (YKCameraService*)userdata;
    if (!cam) {
        CAMERALOG("null userdata");
        return -1;
    }

    CAMERALOG("command = %d", command);
    switch (command) {
        case CAM_REQ_CAMERA_STATUS: {
            // get various camera status
            camera_status stat;
            int dev_len = sizeof(ykcamera_device_status) / sizeof(module_value);
            int set_len = sizeof(ykcamera_setting_status) / sizeof(module_value);
            CAMERALOG("dev_len = %d, set_len = %d, DEVICE_MAX_ITEM = %d, SETTING_MAX_ITEM = %d",
                dev_len, set_len, DEVICE_MAX_ITEM, SETTING_MAX_ITEM);

            // get device status
            cam->getDeviceStatus(ykcamera_device_status, dev_len);

            // get settings status
            cam->getSettingStatus(ykcamera_setting_status, set_len);

            stat.device_status = ykcamera_device_status;
            stat.setting_status = ykcamera_setting_status;
            param->scall_back_return = &stat;
            break;
        }
        case CAM_REQ_VIDEO_DEFAULT_MODE: {
            int *newMode = (int *)param->scall_back_param;
            if (newMode) {
                ret = cam->setCamMode(*newMode);
            }
            break;
        }
        case CAM_REQ_VIDEO_RESOLUTION: {
            ret = -1;
            break;
        }
        case CAM_REQ_VIDEO_FPS: {
            ret = -1;
            break;
        }
        case CAM_REQ_VIDEO_RESOLUTION_FPS: {
            int *index = (int *)param->scall_back_param;
            if (index) {
                ret = cam->setResolution(*index);
            } else {
                ret = -1;
            }
            break;
        }
        case CAM_REQ_VIDEO_DATETIME_PRINT: {
            int *on = (int *)param->scall_back_param;
            if (on) {
                ret = cam->setOverlay(*on != 0);
            } else {
                ret = -1;
            }
            break;
        }
        case CAM_REQ_VIDEO_AUDIO_RECORD: {
            int *mute = (int *)param->scall_back_param;
            if (mute) {
                ret = cam->setMicMute(*mute == 0);
            } else {
                ret = -1;
            }
            break;
        }

        case CAM_REQ_COMM_AUTO_POWER_DOWN: {
            // 0 close, 1 15s, 2 30s, 3 60s
            int *poweroff_time = (int *)param->scall_back_param;
            if (poweroff_time) {
                ret = cam->setAutoPowerOff(*poweroff_time);
            }
            break;
        }
        case CAM_REQ_COMM_RESET_FACTORY: {
            break;
        }
        case CAM_REQ_COMM_FORMAT_SD: {
            ret = cam->formatTF();
            break;
        }
        case CAM_REQ_COMM_SHUTTER: {
            int *start = (int *)param->scall_back_param;
            if (start) {
                ret = cam->videoRecord(*start != 0);
            } else {
                ret = -1;
            }
            break;
        }
        case CAM_REQ_COMM_POWEROFF: {
            DestroyUpnpService(false);
            break;
        }

        case CAM_REQ_COMM_STORAGE_DELETE_ALL: {
            // delete all
            break;
        }
        case CAM_REQ_COMM_STORAGE_DELETE: {
            // delete the one identified by name
            break;
        }
        case CAM_REQ_COMM_STORAGE_DELETE_LAST: {
            // delete the last one
            break;
        }

        case CAM_REQ_COMM_THUMBNAIL: {
            // get thumbnail
            thumbnail_info *pic = (thumbnail_info *)param->scall_back_return;
            char *vdb_id_str = (char*)param->scall_back_param;
            int vdb_id = 0;
            if (vdb_id_str) {
                vdb_id = atoi(vdb_id_str);
                CAMERALOG("vdb_id_str = %s, vdb_id = %d", vdb_id_str, vdb_id);
                if (vdb_id != 0) {
                    ret = cam->getThumnail(vdb_id, pic);
                } else {
                    ret = -1;
                }
            }
            /*
            if (pic->size >= 0) {
                uint8_t *data;
                data = (uint8_t *)pic->addr;
                free(data);
            }
            */
            break;
        }
        case CAM_REQ_COMM_DEVINFO_GET: {
            // get device info
            devinfo *dev = (devinfo *)param->scall_back_return;
            ret = cam->getDevInfo(dev);
            break;
        }
        case CAM_REQ_COMM_MEDIA_LIST: {
            CAMERALOG("CAM_REQ_COMM_MEDIA_LIST");
            ret = cam->getMediaList();
            param->scall_back_return = cam->_mediaList;
#if 0
            struct JPEG_DATA pic;
            cam->getThumnail(385161867, &pic);
            if (pic.jpeg_data) {
                delete [] pic.jpeg_data;
            }
#endif
            break;
        }
        case CAM_REQ_COMM_DATETIME_SYNC: {
            // sync date time
            time_t tRemote = 0;
            tRemote = *((time_t *)param->scall_back_param);
            ret = cam->syncTime(tRemote);
            break;
        }
        default:
            CAMERALOG("command_handler response_function is NULL for command=%d", command);
            ret = -1;
            break;
    }

    return ret;
}

YKCameraService::YKCameraService()
    : _bRunning(false)
    , _socket_VDB_CMD(-1)
{
   memset(_ip, 0x00, 32);
}

YKCameraService::~YKCameraService() {
}

void YKCameraService::_setupInterface() {
    _hfs.hfs_path = "/tmp/mmcblk0p1";
    _hfs.hfs_preview_path = NULL;
    _hfs.hfs_cut_path = NULL;
    _hfs.hfs_port = "8080";
    _hfs.ota_path = "/tmp/mmcblk0p1";

    _wsp.hfs_info = &_hfs;
    _wsp.user_data = (void *)this;
    _wsp.cam_handler = YKCommand_handler;
    _wsp.control_port = "8010";

    _cam_status.device_status = ykcamera_device_status;
    _cam_status.setting_status = ykcamera_setting_status;

    _cam_interface.wsp = &_wsp;
    _cam_interface.cam_status = &_cam_status;
    _cam_interface.sdk_debug_level = CAM_LOG_DEBUG;
}

int YKCameraService::_openVDBCMDPort() {
    struct sockaddr_in addr;
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    //addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_addr.s_addr = inet_addr(_ip);
    addr.sin_port = htons(VDB_CMD_PORT);

    _socket_VDB_CMD = socket(AF_INET, SOCK_STREAM, 0);
    CAMERALOG("_socket_VDB_CMD = %d", _socket_VDB_CMD);
    if (connect(_socket_VDB_CMD, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        CAMERALOG("connect error");
        return -1;
    }

    int bytes_received = 0;

    vdb_ack_t ack;
    int ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
    if (ack.ret_code != 0) {
        return -1;
    }
    bytes_received += ret;

    vdb_ack_GetVersionInfo_s version_ack;
    ret = recv(_socket_VDB_CMD, &version_ack, sizeof(version_ack), 0);
    bytes_received += ret;
    CAMERALOG("version.major = %d, version.minor = %d",
        version_ack.major, version_ack.minor);

    int remaining = 128 + sizeof(ack) + ack.extra_bytes - bytes_received;
    if (remaining > 0) {
        char ackBuffer[160] = {0x00};
        ret = recv(_socket_VDB_CMD, ackBuffer, remaining, 0);
    }

    return _socket_VDB_CMD;
}

void YKCameraService::_closeVDBCMDPort(int sock) {
    if (sock > 0) {
        close(sock);
        sock = -1;
    }
}

int YKCameraService::_getBitrate() {
    int width = 1920, height = 1080, frame = 30;
    char tmp[256];

    memset(tmp, 0x00, sizeof(tmp));
    agcmd_property_get(PropName_rec_resoluton, tmp, "1080p30");
    if (strcmp(tmp, "1080p30") == 0) {
        width = 1920;
        height = 1080;
        frame = 30;
    } else if (strcmp(tmp, "720p60") == 0) {
        width = 1280;
        height = 720;
        frame = 60;
    } else if (strcmp(tmp, "720p30") == 0) {
        width = 1280;
        height = 720;
        frame = 30;
    } else if (strcmp(tmp, "480p30") == 0) {
        width = 720;
        height = 480;
        frame = 30;
    }

    memset(tmp, 0x00, sizeof(tmp));
    agcmd_property_get(PropName_rec_quality, tmp, "HQ");
    int base = BitrateForSoso720p;
    if (strcmp(tmp, "HQ") == 0) {
        base = BitrateForHQ720p;
    } else if (strcmp(tmp, "NORMAL") == 0) {
        base = BitrateForNormal720p;
    } else if (strcmp(tmp, "SOSO") == 0) {
        base = BitrateForSoso720p;
    } else if (strcmp(tmp, "LOW") == 0) {
        base = BitrateForLow720p;
    }

    float b = (float)width * height * ((float)frame / 30.0) / 1000000.0;
    return base * b /* main */ + 880000 /* sub */;
}

int YKCameraService::_getClipNum() {
    int _socket_VDB_CMD = _openVDBCMDPort();
    if (_socket_VDB_CMD <= 0) {
        CAMERALOG("Bad socket %d", _socket_VDB_CMD);
        return 0;
    }

    char cmdBuffer[160] = {0x00};
    char ackBuffer[160] = {0x00};

    // first of all: get all clips
    vdb_cmd_GetClipSetInfoEx_t clipinfo_cmd;
    clipinfo_cmd.inherited.header.cmd_code = VDB_CMD_GetClipSetInfoEx;
    clipinfo_cmd.inherited.header.cmd_flags = 0;
    clipinfo_cmd.inherited.header.cmd_tag = 0;
    clipinfo_cmd.inherited.header.user1 = 0;
    clipinfo_cmd.inherited.header.user2 = 0;
    clipinfo_cmd.inherited.clip_type = CLIP_TYPE_BUFFER;
    clipinfo_cmd.flags = GET_CLIP_EXTRA | GET_CLIP_VDB_ID;
    int sent = send(_socket_VDB_CMD, &clipinfo_cmd, sizeof(clipinfo_cmd), MSG_NOSIGNAL);
    sent = send(_socket_VDB_CMD, cmdBuffer, 160 - sizeof(clipinfo_cmd), MSG_NOSIGNAL);

    int bytes_received = 0;

    vdb_ack_t ack;
    memset(&ack, 0x00, sizeof(ack));
    int ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
    if (ack.ret_code != 0) {
        _closeVDBCMDPort(_socket_VDB_CMD);
        return 0;
    }
    bytes_received += ret;

    vdb_ack_GetClipSetInfoEx_s clipset_ack;
    ret = recv(_socket_VDB_CMD, &clipset_ack, sizeof(clipset_ack), 0);
    bytes_received += ret;
    CAMERALOG("clipset_ack.clip_type = %d, ack.total_clips = %d",
        clipset_ack.clip_type, clipset_ack.total_clips);

    int remaining = 128 + sizeof(ack) + ack.extra_bytes - bytes_received;
    //CAMERALOG("bytes_received = %d, ack.extra_bytes = %d, sizeof(ack) = %d, remaining = %d",
    //    bytes_received, ack.extra_bytes, sizeof(ack), remaining);
    if (remaining > 0) {
        while ((remaining > 0) && (remaining > 160)) {
            ret = recv(_socket_VDB_CMD, ackBuffer, 160, 0);
            remaining -= 160;
        }
        //CAMERALOG("remaining = %d", remaining);
        if (remaining > 0) {
            ret = recv(_socket_VDB_CMD, ackBuffer, remaining, 0);
        }
    }

    _closeVDBCMDPort(_socket_VDB_CMD);
    return clipset_ack.total_clips;
}

void YKCameraService::start(char *ip) {
    if (strcmp(_ip, ip) == 0) {
        CAMERALOG("ip(%s) has not been changed", _ip);
        return;
    }

    if (_bRunning) {
        stop();
    }

    memset(_ip, 0x00, 32);
    sprintf(_ip, "%s", ip);
    _setupInterface();
    youku_camera_init(&_cam_interface);
    youku_camera_start();
    _bRunning = true;

    //_openVDBCMDPort();
}

void YKCameraService::stop() {
    _closeVDBCMDPort(_socket_VDB_CMD);
    youku_camera_deinitialize();

    _bRunning = false;
}

int YKCameraService::setCamMode(int mode)
{
    CAMERALOG("set cam mode to %d", mode);
    int ret = 0;
    switch (mode) {
        case YK_VIDEO_SUBMODE_VIDEO:
            agcmd_property_set(PropName_rec_recMode, "NORMAL");
            break;
        case YK_VIDEO_SUBMODE_VIDEI_PHOTO:
            ret = -1;
            break;
        case YK_VIDEO_SUBMODE_CYCLE_RECARD:
            agcmd_property_set(PropName_rec_recMode, "Circle");
            break;
        case YK_VIDEO_SUBMODE_CARDV:
            agcmd_property_set(PropName_rec_recMode, "CAR");
            break;
        default:
            ret = -1;
            break;
    }
    return ret;
}

int YKCameraService::setResolution(int index) {
    int ret = 0;
    switch (index) {
        case YK_VIDEO_1080P_30FPS: // 1080p30
            agcmd_property_set(PropName_rec_resoluton, "1080p30");
            break;
        case YK_VIDEO_720P_60FPS: // 720p60
            agcmd_property_set(PropName_rec_resoluton, "720p60");
            break;
        case YK_VIDEO_720P_30FPS: // 720p30
            agcmd_property_set(PropName_rec_resoluton, "720p30");
            break;
        case YK_VIDEO_WVGA_30FPS: // 480p30
            agcmd_property_set(PropName_rec_resoluton, "480p30");
            break;
        default:
            ret = -1;
            break;
    }
    return ret;
}

int YKCameraService::videoRecord(bool start) {
    if (start) {
        IPC_AVF_Client::getObject()->StartRecording();
    } else {
        IPC_AVF_Client::getObject()->StopRecording();
    }
    return 0;
}

int YKCameraService::getThumnail(int vdb_id, thumbnail_info *pic) {
    if (!pic) {
        CAMERALOG("Empty buffer");
        return -1;
    }

    if (vdb_id == 0) {
        CAMERALOG("Bad vdb id %d", vdb_id);
        return -1;
    }

    int _socket_VDB_CMD = _openVDBCMDPort();
    if (_socket_VDB_CMD <= 0) {
        CAMERALOG("Bad socket %d", _socket_VDB_CMD);
        return -1;
    }

    char cmdBuffer[160] = {0x00};
    char ackBuffer[160] = {0x00};

    vdb_cmd_GetIndexPicture_t picture_cmd;
    picture_cmd.header.cmd_code = VDB_CMD_GetIndexPicture;
    picture_cmd.header.cmd_flags = 0;
    picture_cmd.header.cmd_tag = 0;
    picture_cmd.header.user1 = 0;
    picture_cmd.header.user2 = 0;
    picture_cmd.clip_type = CLIP_TYPE_BUFFER;
    picture_cmd.clip_id = vdb_id;
    CAMERALOG("picture_cmd.clip_id = %d", picture_cmd.clip_id);
    picture_cmd.clip_time_ms_lo = 0;
    picture_cmd.clip_time_ms_hi = 0;
    int sent = send(_socket_VDB_CMD, &picture_cmd, sizeof(picture_cmd), MSG_NOSIGNAL);
    sent = send(_socket_VDB_CMD, cmdBuffer, 160 - sizeof(picture_cmd), MSG_NOSIGNAL);

    int bytes_received = 0;

    vdb_ack_t ack;
    memset(&ack, 0x00, sizeof(ack));
    int ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
    if (ack.ret_code != 0) {
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }
    bytes_received += ret;

    vdb_ack_GetIndexPicture_s picture_ack;
    ret = recv(_socket_VDB_CMD, &picture_ack, sizeof(picture_ack), 0);
    bytes_received += ret;
    CAMERALOG("picture_ack.clip_id = %d, picture_ack.picture_size = %d",
        picture_ack.clip_id, picture_ack.picture_size);
    time_t tim = picture_ack.clip_date;
    struct tm* clip_tm = gmtime(&tim);
    CAMERALOG("clip date: %s", asctime(clip_tm));

    if (picture_ack.picture_size <= 0) {
        CAMERALOG("incorrect picture_size");
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }
    pic->size = picture_ack.picture_size;
    if (pic->size > 0) {
        uint8_t *data = (uint8_t *)malloc(pic->size);
        pic->addr = (int)data;

        int pic_received = 0;
        while (pic_received < picture_ack.picture_size) {
            ret = recv(_socket_VDB_CMD,
                data + pic_received,
                picture_ack.picture_size - pic_received,
                MSG_NOSIGNAL);
            if (ret < 0) {
                free(data);
                CAMERALOG("recv failed with error %d", ret);
                _closeVDBCMDPort(_socket_VDB_CMD);
                return -1;
            }
            pic_received += ret;
            bytes_received += ret;
        }
    }

    int remaining = 128 + sizeof(ack) + ack.extra_bytes - bytes_received;
    if (remaining > 0) {
        ret = recv(_socket_VDB_CMD, ackBuffer, remaining, 0);
        CAMERALOG("ret = %d, remaining = %d, ack.extra_bytes = %d",
            ret, remaining, ack.extra_bytes);
    }

    _closeVDBCMDPort(_socket_VDB_CMD);
    return 0;
}

int YKCameraService::getMediaList() {
    int _socket_VDB_CMD = _openVDBCMDPort();
    if (_socket_VDB_CMD <= 0) {
        CAMERALOG("Bad socket %d", _socket_VDB_CMD);
        return -1;
    }

    int ret_code = 0;
    char cmdBuffer[160] = {0x00};
    char ackBuffer[160] = {0x00};

    // first of all: get all clips
    vdb_cmd_GetClipSetInfoEx_t clipinfo_cmd;
    clipinfo_cmd.inherited.header.cmd_code = VDB_CMD_GetClipSetInfoEx;
    clipinfo_cmd.inherited.header.cmd_flags = 0;
    clipinfo_cmd.inherited.header.cmd_tag = 0;
    clipinfo_cmd.inherited.header.user1 = 0;
    clipinfo_cmd.inherited.header.user2 = 0;
    clipinfo_cmd.inherited.clip_type = CLIP_TYPE_BUFFER;
    clipinfo_cmd.flags = GET_CLIP_EXTRA | GET_CLIP_VDB_ID;
    int sent = send(_socket_VDB_CMD, &clipinfo_cmd, sizeof(clipinfo_cmd), MSG_NOSIGNAL);
    sent = send(_socket_VDB_CMD, cmdBuffer, 160 - sizeof(clipinfo_cmd), MSG_NOSIGNAL);

    int bytes_received = 0;

    vdb_ack_t ack;
    memset(&ack, 0x00, sizeof(ack));
    int ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
    if (ack.ret_code != 0) {
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }
    bytes_received += ret;

    vdb_ack_GetClipSetInfoEx_s clipset_ack;
    ret = recv(_socket_VDB_CMD, &clipset_ack, sizeof(clipset_ack), 0);
    bytes_received += ret;
    CAMERALOG("ret = %d, sizeof(clipset_ack) = %d", ret, sizeof(clipset_ack));
    CAMERALOG("clipset_ack.clip_type = %d, ack.total_clips = %d",
        clipset_ack.clip_type, clipset_ack.total_clips);
    struct clip_info {
        vdb_clip_info_ex_s clip_ex;
        avf_stream_attr_t stream_attr[2];
    };
    struct clip_info *clips = new struct clip_info[clipset_ack.total_clips];
    for (int i = 0; i < clipset_ack.total_clips; i++) {
        vdb_clip_info_t clip;
        ret = recv(_socket_VDB_CMD, &clip, sizeof(vdb_clip_info_t), 0);
        bytes_received += ret;
        CAMERALOG("ret = %d, sizeof(vdb_clip_info_t) = %d, "
            "clip.clip_id = %d, clip.clip_date = %d, clip.clip_duration_ms = %d, "
            "clip.clip_start_time_ms_lo = %d, clip.clip_start_time_ms_hi = %d, "
            "clip.num_streams = %d, clip.flags = 0x%x",
            ret, sizeof(vdb_clip_info_t),
            clip.clip_id, clip.clip_date, clip.clip_duration_ms,
            clip.clip_start_time_ms_lo, clip.clip_start_time_ms_hi,
            clip.num_streams, clip.flags);
        time_t tim = clip.clip_date;
        struct tm* clip_tm = gmtime(&tim);
        CAMERALOG("clip date: %s", asctime(clip_tm));
        clips[i].clip_ex.inherited.clip_id = clip.clip_id;
        clips[i].clip_ex.inherited.clip_date = clip.clip_date;
        clips[i].clip_ex.inherited.clip_duration_ms = clip.clip_duration_ms;
        clips[i].clip_ex.inherited.clip_start_time_ms_lo = clip.clip_start_time_ms_lo;
        clips[i].clip_ex.inherited.clip_start_time_ms_hi = clip.clip_start_time_ms_hi;
        clips[i].clip_ex.inherited.num_streams = clip.num_streams;
        clips[i].clip_ex.inherited.flags = clip.flags;
        for (int j = 0; j < clip.num_streams; j++) {
            ret = recv(_socket_VDB_CMD, &clips[i].stream_attr[j], sizeof(avf_stream_attr_t), 0);
            bytes_received += ret;
            CAMERALOG("ret = %d, sizeof(avf_stream_attr_t) = %d, "
                "clips[i].stream_attr[j].video_width = %d, "
                "clips[i].stream_attr[j].video_height = %d, "
                "clips[i].stream_attr[j].video_framerate = %d",
                ret, sizeof(avf_stream_attr_t),
                clips[i].stream_attr[j].video_width, clips[i].stream_attr[j].video_height,
                clips[i].stream_attr[j].video_framerate);
        }
        ret = recv(_socket_VDB_CMD, &(clips[i].clip_ex.clip_type), sizeof(clips[i].clip_ex.clip_type), 0);
        bytes_received += ret;
        ret = recv(_socket_VDB_CMD, &(clips[i].clip_ex.extra_size), sizeof(clips[i].clip_ex.extra_size), 0);
        bytes_received += ret;
        if (clips[i].clip_ex.inherited.flags & GET_CLIP_EXTRA) {
            uint8_t uuid[36];
            uint32_t ref_clip_date;	// same with clip_date
            int32_t gmtoff;
            uint32_t real_clip_id;
            ret = recv(_socket_VDB_CMD, uuid, 36, 0);
            bytes_received += ret;
            ret = recv(_socket_VDB_CMD, &ref_clip_date, sizeof(ref_clip_date), 0);
            bytes_received += ret;
            ret = recv(_socket_VDB_CMD, &gmtoff, sizeof(gmtoff), 0);
            bytes_received += ret;
            ret = recv(_socket_VDB_CMD, &real_clip_id, sizeof(real_clip_id), 0);
            bytes_received += ret;
        }
        if (clips[i].clip_ex.inherited.flags & GET_CLIP_VDB_ID) {
            vdb_id_t vdb_id;
            ret = recv(_socket_VDB_CMD, &vdb_id, sizeof(vdb_id), 0);
            bytes_received += ret;
        }
    }

    int remaining = 128 + sizeof(ack) + ack.extra_bytes - bytes_received;
    //CAMERALOG("bytes_received = %d, ack.extra_bytes = %d, sizeof(ack) = %d, remaining = %d",
    //    bytes_received, ack.extra_bytes, sizeof(ack), remaining);
    if (remaining > 0) {
        ret = recv(_socket_VDB_CMD, ackBuffer, remaining, 0);
        CAMERALOG("ret = %d\n\n", ret);
    }
    bytes_received = 0;


#if 1
    memset(_mediaList, 0x00, 1024);
    char *str = _mediaList;
    int len = sprintf(str, "%s%s", MEDIA_LIST_HEADER, VIDEO_LIST_HEADER);
    for (int i = 0; i < clipset_ack.total_clips; i++) {
        // Secondly: get clips' playback url
        vdb_cmd_GetPlaybackUrlEx_s playback_url_cmd;
        playback_url_cmd.inherited.header.cmd_code = VDB_CMD_GetPlaybackUrlEx;
        playback_url_cmd.inherited.header.cmd_flags = 0;
        playback_url_cmd.inherited.header.cmd_tag = 0;
        playback_url_cmd.inherited.header.user1 = 0;
        playback_url_cmd.inherited.header.user2 = 0;
        playback_url_cmd.inherited.clip_type = CLIP_TYPE_BUFFER;//clip.clip_type;
        playback_url_cmd.inherited.clip_id = clips[i].clip_ex.inherited.clip_id;
        playback_url_cmd.inherited.stream = STREAM_MAIN;
        playback_url_cmd.inherited.url_type = URL_TYPE_HLS;
        playback_url_cmd.inherited.clip_time_ms_lo = clips[i].clip_ex.inherited.clip_start_time_ms_lo;
        playback_url_cmd.inherited.clip_time_ms_hi = clips[i].clip_ex.inherited.clip_start_time_ms_hi;
        playback_url_cmd.length_ms = clips[i].clip_ex.inherited.clip_duration_ms;
        sent = send(_socket_VDB_CMD, &playback_url_cmd, sizeof(playback_url_cmd), MSG_NOSIGNAL);
        sent = send(_socket_VDB_CMD, cmdBuffer, 160 - sizeof(playback_url_cmd), MSG_NOSIGNAL);

        memset(&ack, 0x00, sizeof(ack));
        ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
        if (ack.ret_code != 0) {
            ret_code = -1;
            goto EXIT;
        }
        bytes_received += ret;

        vdb_ack_GetPlaybackUrlEx_t playback_url_ack;
        ret = recv(_socket_VDB_CMD, &playback_url_ack, sizeof(playback_url_ack), 0);
        bytes_received += ret;
        CAMERALOG("ret = %d, playback_url_ack.clip_id = %d, playback_url_ack.stream = %d, "
            "playback_url_ack.url_type = %d, playback_url_ack.url_size = %d, "
            "playback_url_ack.length_ms = %d",
            ret, playback_url_ack.clip_id, playback_url_ack.stream,
            playback_url_ack.url_type, playback_url_ack.url_size,
            playback_url_ack.length_ms);

        char url[128] = {0x00};
        ret = recv(_socket_VDB_CMD, url, playback_url_ack.url_size, 0);
        bytes_received += ret;
        CAMERALOG("url = %s", url);

        str = str + len;
        if (i > 0) {
            len = sprintf(str, "%s", ", ");
            str = str + len;
        }
        len = sprintf(str, VIDEO_CLIP_INFO,
            playback_url_ack.clip_id, "NULL", 0, playback_url_ack.length_ms, url);

        remaining = 128 + sizeof(ack) + ack.extra_bytes - bytes_received;
        //CAMERALOG("bytes_received = %d, ack.extra_bytes = %d, sizeof(ack) = %d, remaining = %d",
        //    bytes_received, ack.extra_bytes, sizeof(ack), remaining);
        if (remaining > 0) {
            ret = recv(_socket_VDB_CMD, ackBuffer, remaining, 0);
            CAMERALOG("ret = %d\n\n", ret);
        }
        bytes_received = 0;
    }
    str = str + len;
    len = sprintf(str, " ] }, %s ] } ] }", PHOTO_LIST_HEADER);
#else
    // Secondly: get one clip's download url
    vdb_cmd_GetDownloadUrlEx_s download_url_cmd;
    download_url_cmd.header.cmd_code = VDB_CMD_GetDownloadUrlEx;
    download_url_cmd.header.cmd_flags = 0;
    download_url_cmd.header.cmd_tag = 0;
    download_url_cmd.header.user1 = 0;
    download_url_cmd.header.user2 = 0;
    download_url_cmd.clip_type = CLIP_TYPE_BUFFER;
    download_url_cmd.clip_id = clip.inherited.clip_id;
    CAMERALOG("download_url_cmd.clip_id = %d", download_url_cmd.clip_id);
    download_url_cmd.clip_time_ms_lo = clip.inherited.clip_start_time_ms_lo;
    download_url_cmd.clip_time_ms_hi = clip.inherited.clip_start_time_ms_hi;
    download_url_cmd.clip_length_ms = clip.inherited.clip_duration_ms;
    download_url_cmd.download_opt = DOWNLOAD_OPT_MAIN_STREAM | DOWNLOAD_OPT_SUB_STREAM_1;

    sent = send(_socket_VDB_CMD, &download_url_cmd, sizeof(download_url_cmd), MSG_NOSIGNAL);
    CAMERALOG("sent = %d, sizeof(download_url_cmd) = %d", sent, sizeof(download_url_cmd));
    sent = send(_socket_VDB_CMD, cmdBuffer, 160 - sizeof(download_url_cmd), MSG_NOSIGNAL);
    CAMERALOG("sent = %d", sent);

    memset(&ack, 0x00, sizeof(ack));
    ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
    CAMERALOG("ret = %d, sizeof(ack) = %d, ack.cmd_code = %d, ack.ret_code = %d",
        ret, sizeof(ack), ack.cmd_code, ack.ret_code);

    vdb_ack_GetDownloadUrlEx_s download_url_ack;
    ret = recv(_socket_VDB_CMD, &download_url_ack, sizeof(download_url_ack), 0);
    CAMERALOG("ret = %d, sizeof(download_url_ack) = %d, download_url_ack.clip_id = %d",
        ret, sizeof(download_url_ack), download_url_ack.clip_id);

    vdb_stream_url_t url_t;
    ret = recv(_socket_VDB_CMD, &url_t, sizeof(url_t), 0);
    CAMERALOG("ret = %d, sizeof(url_t) = %d, url_t.url_size = %d",
        ret, sizeof(url_t), url_t.url_size);

    char url[320] = {0x00};
    ret = recv(_socket_VDB_CMD, url, url_t.url_size, 0);
    CAMERALOG("ret = %d, url = %s", ret, url);

    ret = recv(_socket_VDB_CMD, &url_t, sizeof(url_t), 0);
    CAMERALOG("ret = %d, sizeof(url_t) = %d, url_t.url_size = %d",
        ret, sizeof(url_t), url_t.url_size);

    char url_sub[320] = {0x00};
    ret = recv(_socket_VDB_CMD, url_sub, url_t.url_size, 0);
    CAMERALOG("ret = %d, url_sub = %s", ret, url_sub);

    //ret = recv(_socket_VDB_CMD, ackBuffer, 640, 0);
    //CAMERALOG("ret = %d", ret);

    memset(_mediaList, 0x00, 1024);
    char *str = _mediaList;
    int len = sprintf(str, "%s%s", MEDIA_LIST_HEADER, VIDEO_LIST_HEADER);

    str = str + len;
    len = sprintf(str, VIDEO_CLIP_INFO,
        download_url_ack.clip_id, "NULL", 0, clip.inherited.clip_duration_ms, url);

    str = str + len;
    len = sprintf(str, " ] }, %s ] } ] }", PHOTO_LIST_HEADER);
#endif
    CAMERALOG("_mediaList = %s", _mediaList);

EXIT:
    _closeVDBCMDPort(_socket_VDB_CMD);
    delete [] clips;
    return ret_code;
}

int YKCameraService::syncTime(time_t utc) {
    time_t t = 0;
    time(&t);
    if (fabs(difftime(t, utc)) > 1.0) {
        struct timeval tt;
        tt.tv_sec = utc;
        tt.tv_usec = 0;
        int res = settimeofday(&tt, NULL);
        agclkd_set_rtc(utc);
    }
    return 0;
}

int YKCameraService::setOverlay(bool on) {
    if (on) {
        agcmd_property_set("system.ui.namestamp", "On");
        agcmd_property_set("system.ui.timestamp", "On");
        agcmd_property_set("system.ui.coordstamp", "On");
        agcmd_property_set("system.ui.speedstamp", "On");
    } else {
        agcmd_property_set("system.ui.namestamp", "Off");
        agcmd_property_set("system.ui.timestamp", "Off");
        agcmd_property_set("system.ui.coordstamp", "Off");
        agcmd_property_set("system.ui.speedstamp", "Off");
    }
    IPC_AVF_Client::getObject()->UpdateSubtitleConfig();
    return 0;
}

int YKCameraService::setMicMute(bool mute) {
    if (mute) {
        IPC_AVF_Client::getObject()->SetMicVolume(0);
    } else {
        IPC_AVF_Client::getObject()->SetMicVolume(7);
    }
    return 0;
}

int YKCameraService::factoryReset()
{
    agcmd_property_del(PropName_rec_recMode);
    agcmd_property_del(PropName_rec_resoluton);
    agcmd_property_del("system.ui.namestamp");
    agcmd_property_del("system.ui.timestamp");
    agcmd_property_del("system.ui.coordstamp");
    agcmd_property_del("system.ui.speedstamp");
    return 0;
}

int YKCameraService::formatTF() {
    if (IPC_AVF_Client::getObject()) {
        IPC_AVF_Client::getObject()->FormatTF();
    }
    return 0;
}

int YKCameraService::getDevInfo(devinfo *dev) {
    if (!dev) {
        return -1;
    }

    sprintf(_video_uri, "http://%s:8081?format=mjpeg", _ip);
    CAMERALOG("video uri = %s", _video_uri);
    dev->video_ms_url = strdup(_video_uri);
    dev->photo_ms_url = strdup(_video_uri);
    //dev->model_number = 1001;
    //dev->model_name = strdup("Vidit");
    dev->firmware_version = strdup(SERVICE_UPNP_VERSION);
    dev->serial_number = strdup("1001-0002-0003-0004");
    dev->ap_mac = strdup("NULL");
    memset(_ssid_AP, 0x00, 16);
    char ssid_name_prop[64];
    agcmd_property_get("temp.earlyboot.ssid_name", ssid_name_prop, "");
    agcmd_property_get(ssid_name_prop, _ssid_AP, "N/A");
    CAMERALOG("AP ssid: %s", _ssid_AP);
    dev->ap_ssid = strdup(_ssid_AP);
    return 0;
}

int YKCameraService::getDeviceStatus(module_value *dev_status, int len) {
    if (!dev_status) {
        return -1;
    }
    int ret = 0;
    for (int i = 0; i < len; i++) {
        switch (i) {
            case DEVICE_INTERNAL_BATTERY_PRESENT: {
                int rt = 0;
                char tmp[AGCMD_PROPERTY_SIZE];
                agcmd_property_get(propPowerSupplyBCapacity, tmp, "0");
                rt = strtoul(tmp, NULL, 0);
                if (rt > 100) {
                    rt = 100;
                } else if (rt < 0) {
                    rt = 0;
                }
                dev_status[i].ivalue = rt;
                break;
            }
            case DEVICE_DATE_TIME: {
                time_t tim;
                time(&tim);
                struct tm* clip_tm = gmtime(&tim);
                memset(CAM_DATE, 0x00, 64);
                strcpy(CAM_DATE, asctime(clip_tm));
                dev_status[i].status_type = CAM_TYPE_STRING;
                dev_status[i].svalue = CAM_DATE;
                break;
            }
            case VIDEO_PROGRESS_COUNTER: {
                int state, still;
                IPC_AVF_Client::getObject()->getRecordState(&state, &still);
                if (state == 3) {
                    dev_status[i].ivalue = IPC_AVF_Client::getObject()->getRecordingTime();
                } else {
                    dev_status[i].ivalue = 0;
                }
                break;
            }
            case STORAGE_SD_STATUS: {
                char storage_prop[256];
                getStoragePropertyName(storage_prop, sizeof(storage_prop));
                char tmp[256];
                memset(tmp, 0x00, sizeof(tmp));
                agcmd_property_get(storage_prop, tmp, "");
                if (strcmp(tmp, "mount_ok") == 0) {
                    dev_status[i].ivalue = 1;
                } else {
                    dev_status[i].ivalue = 0;
                }
                break;
            }
            case STORAGE_REMAINING_VIDEO_TIME: {
                int bitrateKBS = _getBitrate() / 8 / 1000;
                bool rdy;
                unsigned int free;
                long long Total;
                IPC_AVF_Client::getObject()->GetVDBState(rdy, free, Total);
                dev_status[i].ivalue = free * 1000 / bitrateKBS;
                break;
            }
            case STORAGE_NUM_TOTAL_VIDEOS: {
                dev_status[i].ivalue = _getClipNum();
                break;
            }
            case STORAGE_REMAINING_SPACE: {
                bool rdy;
                unsigned int free;
                long long Total;
                IPC_AVF_Client::getObject()->GetVDBState(rdy, free, Total);
                dev_status[i].ivalue = free;
                break;
            }
            default:
                break;
        }
    }
    return ret;
}

int YKCameraService::getSettingStatus(module_value *setting_status, int len) {
    if (!setting_status) {
        return -1;
    }
    int ret = 0;
    for (int i = 0; i < len; i++) {
        switch (i) {
            case SETTING_VIDEO_DEFAULT_MODE: {
                setting_status[i].ivalue = YK_VIDEO_1080P_30FPS;
                break;
            }
            case SETTING_VIDEO_RESOLUTION_FPS: {
                char resolution[maxMenuStrinLen];
                agcmd_property_get(PropName_rec_resoluton, resolution, "1080p30");
                if (strcmp(resolution, "1080p30") == 0) {
                    setting_status[i].ivalue = YK_VIDEO_1080P_30FPS;
                } else if (strcmp(resolution, "720p60") == 0) {
                    setting_status[i].ivalue = YK_VIDEO_720P_60FPS;
                } else if (strcmp(resolution, "720p30") == 0) {
                    setting_status[i].ivalue = YK_VIDEO_720P_30FPS;
                } else if (strcmp(resolution, "480p30") == 0) {
                    setting_status[i].ivalue = YK_VIDEO_WVGA_30FPS;
                }
                break;
            }
            case SETTING_VIDEO_DATETIME_PR: {
                char tmp[16];
                int c = 0;
#if defined(PLATFORMHACHI)
                const char *defaultVal = "Off";
#else
                const char *defaultVal = "On";
#endif
                agcmd_property_get("system.ui.namestamp", tmp, defaultVal);
                if (strcmp(tmp, "On") == 0) {
                    c = c | 0x01;
                }
                agcmd_property_get("system.ui.timestamp", tmp, defaultVal);
                if (strcmp(tmp, "On") == 0) {
                    c = c | 0x02;
                }
                agcmd_property_get("system.ui.coordstamp", tmp, defaultVal);
                if (strcmp(tmp, "On") == 0) {
                    c = c | 0x04;
                }
                agcmd_property_get("system.ui.speedstamp", tmp, defaultVal);
                if (strcmp(tmp, "On") == 0) {
                    c = c | 0x08;
                }
                setting_status[i].ivalue = (c != 0);
                break;
            }
            case SETTING_VIDEO_AUDIO_RECORD: {
                char tmp[256];
                int volume = 0;
                agcmd_property_get(audioGainPropertyName, tmp, "NA");
                if (strcmp(tmp, "NA")==0) {
                    volume = defaultMicGain;
                } else {
                    volume = atoi(tmp);
                }
                agcmd_property_get(audioStatusPropertyName, tmp, "NA");
                CAMERALOG("volume = %d, tmp = %s", volume, tmp);
                if ((strcmp(tmp, "mute") == 0)
                    || (strcmp(tmp, "Mute") == 0)
                    || (volume == 0))
                {
                    setting_status[i].ivalue = 0;
                } else if ((strcmp(tmp, "normal") == 0)
                    || (strcmp(tmp, "Normal") == 0))
                {
                    setting_status[i].ivalue = 1;
                }
                break;
            }
            default:
                break;
        }
    }
    return ret;
}

int YKCameraService::setAutoPowerOff(int time) {
    if (time < 0) {
        return -1;
    } else if (time == 0) {
        // if already set timer to power off, cancel it
        CTimerThread::GetInstance()->CancelTimer(YK_poweroffTimer);
    } else {
        // set timer to power off
        CTimerThread::GetInstance()->ApplyTimer(
            time * 10, YKCameraService::YKPowerOffTimer, this, false, YK_poweroffTimer);
    }
    return 0;
}

