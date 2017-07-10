/*****************************************************************************
 * class_gsensor_thread.cpp:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 -, linnsong.
 *
 *
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/select.h">
#include <sys/inotify.h>
#include <sys/time.h>

#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"
#include "agsys_iio.h"
//#include "agsys_iio_private.h"
#include "class_ipc_rec.h"
#include "class_gsensor_thread.h"
#include "math.h"


extern void getStoragePropertyName(char *prop, int len);

#define MAKE_FOURCC(a,b,c,d) \
    ( ((uint32_t)d) | ( ((uint32_t)c) << 8 ) | ( ((uint32_t)b) << 16 ) | ( ((uint32_t)a) << 24 ) )

static struct agsys_iio_accel_event_setting_s agsys_iio_accel_event_setting = {
    .sampling_frequency = 100,
    .accel_scale = 0.001,
    .thresh_value = 0.500,
    .thresh_duration = 0.1,
    .accel_x_thresh_falling_en = 0,
    .accel_x_thresh_rising_en = 1,
    .accel_y_thresh_falling_en = 0,
    .accel_y_thresh_rising_en = 1,
    .accel_z_thresh_falling_en = 0,
    .accel_z_thresh_rising_en = 1,
};


CGsensorThread* CGsensorThread::_pInstance = NULL;

void CGsensorThread::destroy()
{
    if (_pInstance != NULL) {
        _pInstance->Stop();
        delete _pInstance;
        _pInstance = NULL;
    }
}

void CGsensorThread::create()
{
    if (_pInstance == NULL) {
        _pInstance = new CGsensorThread();
        _pInstance->Go();
    }
}

CGsensorThread::CGsensorThread()
    : CThread("GsensorThread")
    , _piio_info(NULL)
    , _bAutoMarkEnabled(true)
{
    _piio_info = agsys_iio_accel_open();
    if (_piio_info == NULL) {
        CAMERALOG("--- open gsensor device failed");
        return;
    }

    char tmp[256];
    agcmd_property_get(PropName_Auto_Mark_Sensitivity, tmp, Sensitivity_Normal);
    if (strcasecmp(tmp, Sensitivity_High) == 0) {
        agsys_iio_accel_event_setting.thresh_value = 0.75; // 0.75g
        agsys_iio_accel_event_setting.thresh_duration = 0.09; // 90ms
    } else if (strcasecmp(tmp, Sensitivity_Normal) == 0) {
        agsys_iio_accel_event_setting.thresh_value = 1.0; // 1.0g
        agsys_iio_accel_event_setting.thresh_duration = 0.1; // 100ms
    } else if (strcasecmp(tmp, Sensitivity_Low) == 0) {
        agsys_iio_accel_event_setting.thresh_value = 1.5; // 1.5g
        agsys_iio_accel_event_setting.thresh_duration = 0.15; // 150ms
    }

    int ret_val;
    ret_val = agsys_iio_accel_set_event(_piio_info,
        &agsys_iio_accel_event_setting);
    if (ret_val) {
        CAMERALOG("--- set gsensor device config failed");
        return;
    }
    CAMERALOG("auto mark thread created!");
}

CGsensorThread::~CGsensorThread()
{
    if (_piio_info != NULL) {
        agsys_iio_accel_event_setting.accel_x_thresh_falling_en = 0;
        agsys_iio_accel_event_setting.accel_x_thresh_rising_en = 0;
        agsys_iio_accel_event_setting.accel_y_thresh_falling_en = 0;
        agsys_iio_accel_event_setting.accel_y_thresh_rising_en = 0;
        agsys_iio_accel_event_setting.accel_z_thresh_falling_en = 0;
        agsys_iio_accel_event_setting.accel_z_thresh_rising_en = 0;
        agsys_iio_accel_set_event(_piio_info, &agsys_iio_accel_event_setting);
        agsys_iio_accel_close(_piio_info);
        _piio_info = NULL;
    }
    CAMERALOG("%s() destroyed", __FUNCTION__);
}

void CGsensorThread::setSensitivity(CGsensorThread::Gsensor_Sensitivity sens)
{
    switch (sens) {
        case Gsensor_Sensitivity_High:
            agsys_iio_accel_event_setting.thresh_value = 0.75; // 0.75g
            agsys_iio_accel_event_setting.thresh_duration = 0.09; // 90ms
            break;
        case Gsensor_Sensitivity_Normal:
            agsys_iio_accel_event_setting.thresh_value = 1.0; // 1.0g
            agsys_iio_accel_event_setting.thresh_duration = 0.1; // 100ms
            break;
        case Gsensor_Sensitivity_Low:
            agsys_iio_accel_event_setting.thresh_value = 1.5; // 1.5g
            agsys_iio_accel_event_setting.thresh_duration = 0.15; // 150ms
            break;
        default:
            break;
    }
    agsys_iio_accel_set_event(_piio_info, &agsys_iio_accel_event_setting);
}

bool CGsensorThread::Read(float &x, float &y, float &z)
{
    int ret_val;
    struct agsys_iio_accel_xyz_s iio_accel_xyz;

    ret_val = agsys_iio_accel_read_xyz(_piio_info, &iio_accel_xyz);
    if (ret_val < 0) {
        CAMERALOG("--- read accel infor failed.");
        return false;
    }

    x = (iio_accel_xyz.accel_x * _piio_info->accel_scale);
    y = (iio_accel_xyz.accel_y * _piio_info->accel_scale);
    z = (iio_accel_xyz.accel_z * _piio_info->accel_scale);
    //CAMERALOG("acceleration x = %.2f, y = %.2f, z = %.2f", x, y, z);
    return true;
}

float CGsensorThread::getThreshold()
{
    float gforce = 1.0;

    char tmp[256];
    agcmd_property_get(PropName_Auto_Mark_Sensitivity, tmp, Sensitivity_Normal);
    if (strcasecmp(tmp, Sensitivity_High) == 0) {
        gforce = 0.75;
    } else if (strcasecmp(tmp, Sensitivity_Normal) == 0) {
        gforce = 1.0;
    } else if (strcasecmp(tmp, Sensitivity_Low) == 0) {
        gforce = 1.5;
    }

    return gforce;
}

bool CGsensorThread::checkMarkable()
{
    bool markable = false;
    char auto_mark[256];
    agcmd_property_get(PropName_Auto_Mark, auto_mark, Auto_Mark_Disabled);
    if (strcasecmp(auto_mark, Auto_Mark_Enabled) == 0) {
        markable = true;
    }
    return markable;
}

void CGsensorThread::main()
{
    int ret_val;
    float x = 0.0, y = 0.0, z = 0.0, g_force;

    while (_piio_info)
    {
        struct iio_event_data iio_ed;
        ret_val = agsys_iio_accel_wait_event(_piio_info, &iio_ed);
        if (ret_val < 0) {
            //CAMERALOG("--- wait failed");
            sleep(1);
            continue;
        }

        Read(x, y, z);
        // ignore the g-force on vertical direction
        g_force = sqrt(x * x  + z * z);
        float threshold = getThreshold();
        if (g_force > threshold) {
            CAMERALOG("acceleration x = %.2f, y = %.2f, z = %.2f, "
                "g-force = sqrt(x * x  + z * z) = %.2fg is big than "
                "threshold(%.2fg), something happens",
                x, y, z, g_force, threshold);
            if (_bAutoMarkEnabled && checkMarkable() && IPC_AVF_Client::getObject()) {
                // firstly check whether recording, if not, try start recording
                bool bSDReady = false;
                char storage_prop[256];
                getStoragePropertyName(storage_prop, sizeof(storage_prop));
                char tmp[256];
                memset(tmp, 0x00, sizeof(tmp));
                agcmd_property_get(storage_prop, tmp, "");
                if (strcasecmp(tmp, "mount_ok") == 0) {
                    bSDReady = true;
                }

                int avf_state = 0;
                int is_still = 0;
                IPC_AVF_Client::getObject()->getRecordState(&avf_state, &is_still);
                if ((avf_state != State_Rec_recording) &&
                    (avf_state != State_Rec_starting) &&
                    bSDReady)
                {
                    IPC_AVF_Client::getObject()->StartRecording();
                    int counter = 0;
                    do {
                        usleep(200 * 1000);
                        IPC_AVF_Client::getObject()->getRecordState(&avf_state, &is_still);
                        counter++;
                    } while ((avf_state != State_Rec_recording) && (counter < 20));
                }

                // at last, mark the video
                CAMERALOG("automatically add a highlight[-10, 20]");
                struct Auto_Mark_Data {
                    uint32_t fourcc;
                };
                struct Auto_Mark_Data scene_data;
                scene_data.fourcc = MAKE_FOURCC('A', 'U', 'T', 'M');
                IPC_AVF_Client::getObject()->markVideo(10, 20,
                    (const uint8_t*)&scene_data, sizeof(scene_data));
            }
        }
    }
}

