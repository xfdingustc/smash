
#ifndef __YK_CAMERA_SERVICE_
#define __YK_CAMERA_SERVICE_

#include <time.h>

#include "ykcamera.h"


class YKCameraService {
public:
    static YKCameraService* getInstance();
    static int YKCommand_handler(
        int command, handler_parameter *param, void *userdata);
    static void YKPowerOffTimer(void *param);

    void start(char *ip);
    void stop();

private:
    YKCameraService();
    ~YKCameraService();

    void _setupInterface();
    int _openVDBCMDPort();
    void _closeVDBCMDPort(int sock);

    int _getBitrate();
    int _getClipNum();

    int getDeviceStatus(module_value *dev_status, int len);
    int getSettingStatus(module_value *setting_status, int len);
    int getCameraStatus(camera_status *stat_return);
    int setCamMode(int mode);
    int setResolution(int index);
    int videoRecord(bool start);
    int getMediaList();
    int syncTime(time_t utc);
    int setOverlay(bool on);
    int setMicMute(bool mute);
    int factoryReset();
    int formatTF();
    int getDevInfo(devinfo *dev);
    int getThumnail(int vdb_id, thumbnail_info *pic);
    int setAutoPowerOff(int time);

    static YKCameraService* _cam;

    char _ip[32];
    char _video_uri[32];
    char _photo_uri[32];
    char _ssid_AP[16];
    bool _bRunning;

    hfs_info _hfs;
    web_sever_param _wsp;
    camera_status _cam_status;

    ykcamera_interface _cam_interface;

    int  _socket_VDB_CMD;
    char _mediaList[1024 * 3];
};

#endif
