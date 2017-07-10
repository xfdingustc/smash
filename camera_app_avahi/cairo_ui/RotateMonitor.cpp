
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <string.h>

#include "agbox.h"

#include "RotateMonitor.h"

extern void CameraDelayms(int time_ms);

namespace ui {

RotateMonitor* RotateMonitor::_pInstance = NULL;

void RotateMonitor::Create(CCameraProperties *cp)
{
    if (RotateMonitor::_pInstance == NULL) {
        RotateMonitor::_pInstance = new RotateMonitor(cp);
    }
}

RotateMonitor* RotateMonitor::getInstance()
{
    return RotateMonitor::_pInstance;
}

void RotateMonitor::releaseInstance()
{
    if (RotateMonitor::_pInstance != NULL) {
        delete RotateMonitor::_pInstance;
        RotateMonitor::_pInstance = NULL;
    }
}

void* RotateMonitor::ThreadFunc(void* lpParam)
{
    RotateMonitor *pSelf = (RotateMonitor *)lpParam;
    if (pSelf) {
        prctl(PR_SET_NAME, "RotateMonitorThread");
        pSelf->main();
    }
    return NULL;
}

RotateMonitor::RotateMonitor(CCameraProperties *cp)
  : _cp(cp)
  , _hThread(0)
  , _rotate(RotateStatus_None)
  , _bExit(false)
{
    memset(_historyPitchRoll, 0x00, sizeof(_historyPitchRoll));
    pthread_create(&_hThread, NULL, ThreadFunc, (void*)this);
}

RotateMonitor::~RotateMonitor()
{
    if (_hThread) {
        _bExit = true;
        pthread_join(_hThread, NULL);
        _hThread = 0;
        CAMERALOG("RotateMonitorThread destroyed");
    }
}

bool RotateMonitor::_recordPitchRoll()
{
    float roll, pitch, heading;
    bool got = _cp->getOrientation(heading, roll, pitch);
    if (!got) {
        return false;
    }
    for (int i = HISTORY_LEN - 1; i > 0; i--) {
        _historyPitchRoll[i] = _historyPitchRoll[i - 1];
    }
    _historyPitchRoll[0] = roll;

    return true;
}

RotateMonitor::RotateStatus RotateMonitor::_checkRotate()
{
    RotateStatus rotate = RotateStatus_None;
    int i = 0;
    for (i = 0; i < HISTORY_LEN; i++) {
        if ((_historyPitchRoll[i] >= 135.0) &&
            (_historyPitchRoll[i] <= 225.0))
        {
            continue;
        } else {
            break;
        }
    }
    if (i == HISTORY_LEN) {
        rotate = RotateStatus_HV;
        return rotate;
    }

    for (i = 0; i < HISTORY_LEN; i++) {
        if ((_historyPitchRoll[i] <= 45.0) ||
            (_historyPitchRoll[i] >= 315.0))
        {
            continue;
        } else {
            break;
        }
    }
    if (i == HISTORY_LEN) {
        rotate = RotateStatus_Normal;
        return rotate;
    }

    return rotate;
}

bool RotateMonitor::_isRecording()
{
    bool bRecording = false;
    camera_State state = _cp->getRecState();
    if ((camera_State_record == state)
        || (camera_State_marking == state)
        || (camera_State_starting == state)
        || (camera_State_stopping == state))
    {
        bRecording = true;
    }
    return bRecording;
}

bool RotateMonitor::_isAutoRotateEnabled()
{
    bool bEnabled = false;
    char rotate_mode[256];
    agcmd_property_get(PropName_Rotate_Mode, rotate_mode, Rotate_Mode_Normal);
    if (strcasecmp(rotate_mode, Rotate_Mode_Auto) == 0) {
        bEnabled = true;
    }
    return bEnabled;
}

void RotateMonitor::main()
{
    while (!_bExit) {
        if (_isAutoRotateEnabled() && _recordPitchRoll()) {
            RotateStatus rotate = _checkRotate();
            if (_rotate != rotate) {
                if (!_isRecording()) {
                    if (RotateStatus_Normal == rotate) {
                        if (_cp->setOSDRotate(0)) {
                            _rotate = rotate;
                        }
                    } else if (RotateStatus_HV == rotate) {
                        if (_cp->setOSDRotate(1)) {
                            _rotate = rotate;
                        }
                    }
                }
            }
        }
        CameraDelayms(200);
    }
}



};
