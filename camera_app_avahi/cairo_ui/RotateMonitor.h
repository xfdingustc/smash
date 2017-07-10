#ifndef __OSD_Rotate_Monitor_h__
#define __OSD_Rotate_Monitor_h__

#include <pthread.h>

#include "linux.h"
#include "GobalMacro.h"
#include "class_camera_property.h"

namespace ui {

class RotateMonitor
{
public:
    static void Create(CCameraProperties *cp);
    static RotateMonitor* getInstance();
    static void releaseInstance();

private:

#define HISTORY_LEN     3

    enum RotateStatus {
        RotateStatus_None = 0,
        RotateStatus_Normal,
        RotateStatus_HV,
    };

    RotateMonitor(CCameraProperties *cp);
    ~RotateMonitor();

    static void* ThreadFunc(void* lpParam);

    void main();
    bool _recordPitchRoll();
    bool _isRecording();
    bool _isAutoRotateEnabled();
    RotateStatus _checkRotate();


    static RotateMonitor    *_pInstance;

    CCameraProperties   *_cp;

    pthread_t           _hThread;
    float               _historyPitchRoll[HISTORY_LEN];
    RotateStatus        _rotate;
    bool                _bExit;
};

};
#endif
