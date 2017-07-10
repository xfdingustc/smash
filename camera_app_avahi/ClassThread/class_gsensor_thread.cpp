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


static struct agsys_iio_accel_event_setting_s agsys_iio_accel_event_setting = {
    .sampling_frequency = 100,
    .accel_scale = 0.001,
    .thresh_value = 0.100,
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
    :CThread("GsensorThread", CThread::NORMAL, 0, NULL)
    ,_piio_info(NULL)
{
    _piio_info = agsys_iio_accel_open();
    if (_piio_info == NULL) {
        CAMERALOG("--- open gsensor device failed");
        return;
    }

    int ret_val;
    ret_val = agsys_iio_accel_set_event(_piio_info,
        &agsys_iio_accel_event_setting);
    if (ret_val) {
        CAMERALOG("--- set gsensor device config failed");
        return;
    }
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
    return true;
}

void CGsensorThread::main(void * p)
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
        if (g_force > 1.0) {
            CAMERALOG("acceleration x = %.2f, y = %.2f, z = %.2f, "
                "g-force = sqrt(x * x  + z * z) = %.2f is big, "
                "something happens, automatically add a highlight[-10, 20]",
                x, y, z, g_force);
            IPC_AVF_Client::getObject()->markVideo(10, 20);
        }
    }
}

