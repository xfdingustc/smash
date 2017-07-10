
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
#ifdef WIN32
#include <windows.h>
#include "debug.h"
#include "CWinThread.h"
#include "CEvent.h"
#include "CFile.h"
#include "CBmp.h"
#include "Canvas.h"
#include "Control.h"
#include "Wnd.h"
#include "App.h"
#include "Camera.h"
#else

#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
#include "CLanguage.h"
#include "Control.h"
#include "Wnd.h"
#include "App.h"
#include "Style.h"
#include "CameraAppl.h"
#include "class_camera_property.h"
#include <stdlib.h>
#include <unistd.h>

#include "agbox.h"
#include "agcmd.h"
#include <linux/input.h>
#endif

#define screen_saver_prop_name "system.ui.screensaver"
#define screen_saver_prop_default "15"
/*******************************************************************************
*   CameraAppl
*/

const static char* uiUpdateTimerName = "ui update Timer";

ui::CameraAppl* ui::CameraAppl::_pInstance = NULL;
void ui::CameraAppl::Create(CWndManager *pWndM)
{
    if (_pInstance == NULL) {
        _pInstance = new CameraAppl(pWndM);
        _pInstance->Go();
    }
}

void ui::CameraAppl::Destroy()
{
    CAMERALOG("destroy CameraAppl");
    if (_pInstance != NULL) {
        _pInstance->Stop();
        delete _pInstance;
        _pInstance = NULL;
    }
    CAMERALOG("CameraAppl destroyed");
}
ui::CameraAppl::CameraAppl(CWndManager *pWnd)
    : CAppl(pWnd)
    , _dvState(DvState_idel)
{

}

ui::CameraAppl::~CameraAppl()
{

}
bool ui::CameraAppl::OnEvent(CEvent* event)
{
    //inherited::OnEvent(event);
    if (!event->_bProcessed) {
        //CAMERALOG("on event: %d, %d",event->_type, event->_event);
//#ifdef  BitUI
//		CWnd* pWnd = GetWnd();
//		if(pWnd)
//			pWnd->OnEvent(event);
//#endif
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


