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
    static CGsensorThread* getInstance(){return _pInstance;};
    static void destroy();
    static void create();

protected:
    virtual void main(void * p);

private:
    CGsensorThread();
    virtual ~CGsensorThread();

    bool Read(float &x, float &y, float &z);

    static CGsensorThread         *_pInstance;
    struct agsys_iio_accel_info_s *_piio_info;
};

#endif
