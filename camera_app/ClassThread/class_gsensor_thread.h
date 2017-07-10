/*****************************************************************************
 * class_gsensor_thread.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 -, linnsong.
 *
 *
 *****************************************************************************/
#ifndef __IN_gsensor_thread_h
#define __IN_gsensor_thread_h

#include "class_TCPService.h"

class CGsensorThread : public CThread
{
public:
    enum Gsensor_Sensitivity {
        Gsensor_Sensitivity_High = 0,
        Gsensor_Sensitivity_Normal,
        Gsensor_Sensitivity_Low,
    };

    static CGsensorThread* getInstance() { return _pInstance; }
    static void destroy();
    static void create();

    void enableAutoMark(bool enable) { _bAutoMarkEnabled = enable; }
    void setSensitivity(Gsensor_Sensitivity sens);

protected:
    virtual void main();

private:
    CGsensorThread();
    virtual ~CGsensorThread();

    bool Read(float &x, float &y, float &z);
    float getThreshold();
    bool checkMarkable();

    static CGsensorThread         *_pInstance;
    struct agsys_iio_accel_info_s *_piio_info;

    bool    _bAutoMarkEnabled;
};

#endif
