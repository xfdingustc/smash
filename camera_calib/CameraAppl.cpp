
/*******************************************************************************
**      Camera.cpp
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Description:
**
**      Revision History:
**      -----------------
**      01a 27- 8-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/

/*******************************************************************************
* <Includes>
*******************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <linux/input.h>

#include "agbox.h"
#include "agcmd.h"

#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "App.h"
#include "CameraAppl.h"


#define screen_saver_prop_name "system.ui.screensaver"
#define screen_saver_prop_default "15"
/*******************************************************************************
*   CameraAppl
*/

const static char* uiUpdateTimerName = "ui update Timer";

ui::CameraAppl* ui::CameraAppl::_pInstance = NULL;
void ui::CameraAppl::Create()
{
    if (_pInstance == NULL) {
        _pInstance = new CameraAppl();
        _pInstance->Go();
    }
}

void ui::CameraAppl::Destroy()
{
    if (_pInstance != NULL) {
        _pInstance->Stop();
        delete _pInstance;
        _pInstance = NULL;
    }
}
ui::CameraAppl::CameraAppl(	)
    : CAppl()
    , _dvState(DvState_idel)
{

}

ui::CameraAppl::~CameraAppl()
{
    printf("%s() destroyed\n", __FUNCTION__);
}
bool ui::CameraAppl::OnEvent(CEvent* event)
{
    //inherited::OnEvent(event);
    if (!event->_bProcessed) {
        //printf("on event: %d, %d\n",event->_type, event->_event);
    }
    switch (event->_type)
    {
        case eventType_internal:
            switch (event->_event)
            {
                default:
                break;
            }
            break;
        case eventType_key:
            {
                switch (event->_event)
                {
                    case key_up:

                    break;
                    case key_down:

                    break;
                }
            }
            break;
        default:
            break;
    }

    return true;
}


void ui::CameraAppl::StartUpdateTimer()
{
    CTimerThread::GetInstance()->ApplyTimer
        (10, CameraAppl::SecTimerBtnUpdate, this, true, uiUpdateTimerName);
}

void ui::CameraAppl::StopUpdateTimer()
{
    CTimerThread::GetInstance()->CancelTimer(uiUpdateTimerName);
}


