/*******************************************************************************
**       fast_Camera.cpp
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a  7-14-2011 linnsong CREATE AND MODIFY
**
*******************************************************************************/

#include <netinet/in.h>
#include <math.h>

#include "fast_Camera.h"
#include "agbox.h"
#include "CameraWndFactory.h"

namespace ui{

#define TOUCH_R   276
#define SCREEN_R  200
#define T_S_DIFF  (TOUCH_R - SCREEN_R)

ui::CFastCamera::CFastCamera()
    :KeyEventProcessor(NULL)
    ,_dataReceived(0x00)
    ,_flickValue(0)
    ,_hasFlickEvent(false)
    ,_bMoveTriggered(false)
{
    CTimerThread::Create();
    WindowFactory *factory = new CameraWndFactory();
    WindowManager::Create(factory);
    CameraAppl::Create();
    {
        if (WindowManager::getInstance()) {
            int width = 0, height = 0, bitdepth = 0;
            WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
            _pCamWnd = new CalibBeginWnd(WND_BEGIN, Size(width, height), Point(0, 0));
        }
        // Init key filters:
        memset(&last_touch_end_time, 0x00, sizeof(last_touch_end_time));

        CameraAppl::getInstance()->StartUpdateTimer();
        CameraAppl::getInstance()->PushEvent(eventType_internal,
            InternalEvent_app_state_update, 0, 0);

        EventProcessThread::getEPTObject()->AddProcessor(this);

        agcmd_property_set("system.input.key_mute_diff", "10000");
    }
}


ui::CFastCamera::~CFastCamera()
{
    EventProcessThread::getEPTObject()->RemoveProcessor(this);
    CameraAppl::getInstance()->StopUpdateTimer();
    CameraAppl::Destroy();
    WindowManager::Destroy();
}

void ui::CFastCamera::Start()
{
    _pCamWnd->StartWnd(WND_BEGIN);
}

void ui::CFastCamera::Stop()
{
}

bool ui::CFastCamera::event_process(struct input_event* ev)
{
    //CAMERALOG("key event: type = %d code = %d value = %d",
    //    ev->type, ev->code, ev->value);
    if ((ev->type == EV_SYN) && (ev->code == 0) && (ev->value == 0)) {
        // get coordinate
        if ((_dataReceived & DATA_TYPE_x) || (_dataReceived & DATA_TYPE_y)) {
            int distance = (_x - TOUCH_R) * (_x - TOUCH_R) + (_y - TOUCH_R) * (_y - TOUCH_R);
            if (distance <= SCREEN_R * SCREEN_R) {
                CameraAppl::getInstance()->PushEvent(
                        eventType_touch,
                        TouchEvent_coordinate,
                        _x - T_S_DIFF,
                        _y - T_S_DIFF);
            } else {
                //CAMERALOG("point(%d, %d) is not on screen", _x, _y);
            }
            if (!_bMoveTriggered && (((_x - _xOnPressed) * (_x - _xOnPressed)
                + (_y - _yOnPressed) * (_y - _yOnPressed)) > 8 * 8))
            {
                _bMoveTriggered = true;
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_Move_Began,
                    _xOnPressed - T_S_DIFF,
                    _yOnPressed - T_S_DIFF);
            }
            if (_bMoveTriggered && ((_x != _xOnPressed) || (_y != _yOnPressed))) {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_Move_Changed,
                    _x - _xOnPressed,
                    _y - _yOnPressed);
            }
            _dataReceived &= ~(DATA_TYPE_x | DATA_TYPE_y);
        }

        if (_dataReceived & DATA_TYPE_touch_up) {
            int radius = (_x - TOUCH_R) * (_x - TOUCH_R)
                + (_y - TOUCH_R) * (_y - TOUCH_R);
            if (radius <= SCREEN_R * SCREEN_R)
            {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_OnReleased,
                    _x - T_S_DIFF,
                    _y - T_S_DIFF);
            } else {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_OnReleased,
                    400,
                    400);
                //CAMERALOG("point(%d, %d) is not on screen", _x, _y);
            }
            // send move ended, if there is flick event, then it will end with flick
            CameraAppl::getInstance()->PushEvent(
                eventType_touch,
                TouchEvent_Move_Ended,
                _flickValue,
                0/* TODO: speed instead */);

            if (_dataReceived & DATA_TYPE_flick) {
                // send flick event if any
                int x_pos = _x - T_S_DIFF;
                if (x_pos < 0) {
                    x_pos = 0;
                } else if (x_pos > 400) {
                    x_pos = 400;
                }
                int y_pos = _y - T_S_DIFF;
                if (y_pos < 0) {
                    y_pos = 0;
                } else if (y_pos > 400) {
                    y_pos = 400;
                }
                UINT32 position = 0x00;
                position = (x_pos << 16) | y_pos;
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch, TouchEvent_flick, _flickValue, position);
            }
            if ((_dataReceived & DATA_TYPE_tap)
                && (radius <= SCREEN_R * SCREEN_R))
            {
                // send touch click event if any
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_SingleClick,
                    _x - T_S_DIFF,
                    _y - T_S_DIFF);
            }
            if ((_dataReceived & DATA_TYPE_press)
                && (radius <= SCREEN_R * SCREEN_R))
            {
                if (((_dataReceived & DATA_TYPE_tap) == 0x00)
                    && ((_dataReceived & DATA_TYPE_flick) == 0x00)
                    && ((_dataReceived & DATA_TYPE_double_click) == 0x00))
                {
                    // treat press as a kind of single click
                    CAMERALOG("#### Only press event happens, treat it as a single click.");
                    CameraAppl::getInstance()->PushEvent(
                        eventType_touch,
                        TouchEvent_SingleClick,
                        _x_pressed - T_S_DIFF,
                        _y_pressed - T_S_DIFF);
                }
            }
            // reset values
            _flickValue = 0;
            _dataReceived = 0x00;
        }

        if (_dataReceived & DATA_TYPE_double_click) {
            int radius = (_x - TOUCH_R) * (_x - TOUCH_R)
                + (_y - TOUCH_R) * (_y - TOUCH_R);
            if (radius <= SCREEN_R * SCREEN_R)
            {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_DoubleClick,
                    _x - T_S_DIFF,
                    _y - T_S_DIFF);
            }
        }

        if (_dataReceived & DATA_TYPE_touch_down) {
            int radius = (_x - TOUCH_R) * (_x - TOUCH_R)
                + (_y - TOUCH_R) * (_y - TOUCH_R);
            if (radius <= SCREEN_R * SCREEN_R)
            {
                CameraAppl::getInstance()->PushEvent(
                    eventType_touch,
                    TouchEvent_OnPressed,
                    _x - T_S_DIFF,
                    _y - T_S_DIFF);
            }
            // reset record at the beginning of each operation
            _hasFlickEvent = false;
            _dataReceived &= ~DATA_TYPE_touch_down;
        }
    } else if ((ev->type == EV_KEY) && (ev->code == 330)) {
        // Touch pressed/released event
        if (ev->value == 1)
        {
            _xOnPressed = _x;
            _yOnPressed = _y;
            //CAMERALOG("_xOnPressed = %d, _yOnPressed = %d", _xOnPressed, _yOnPressed);
            _dataReceived |= DATA_TYPE_touch_down;
        } else if (ev->value == 0) {
            _dataReceived |= DATA_TYPE_touch_up;
            _bMoveTriggered = false;
            last_touch_end_time.tv_sec = ev->time.tv_sec;
            last_touch_end_time.tv_usec = ev->time.tv_usec;
        }
    } else if ((ev->type == EV_KEY) && (ev->value == 0) && (ev->code == 212)) {
        // key event from remote controller
        // TODO: identify long press
        CameraAppl::getInstance()->PushEvent(
            eventType_key,
            key_right_onKeyUp,
            0, 0);
    } else if ((ev->type == EV_REL) && (ev->code == REL_X)) {
        _x_rel = ev->value;
    } else if ((ev->type == EV_REL) && (ev->code == REL_Y)) {
        _y_rel = ev->value;
    } else if ((ev->type == EV_ABS) && (ev->code == ABS_X)) {
        _x = ev->value;
        _dataReceived |= DATA_TYPE_x;
    } else if ((ev->type == EV_ABS) && (ev->code == ABS_Y)) {
        _y = ev->value;
        _dataReceived |= DATA_TYPE_y;
    } else if (ev->type == EV_MSC) {
        //CAMERALOG("#### ev->value = 0x%x", ev->value);
        int eventType = (ev->value >> 24) & 0xFF;
        if (eventType == 0x20) {
            //CAMERALOG("#### tap on (%d, %d)", _x, _y);
            _dataReceived |= DATA_TYPE_tap;
        } else if (eventType == 0x21) {
            //CAMERALOG("#### press on (%d, %d)", _x, _y);
            _x_pressed = _x;
            _y_pressed = _y;
            _dataReceived |= DATA_TYPE_press;
        } else if (eventType == 0x22) {
            _flickValue = ev->value & 0xFF;
            if (_flickValue != TouchFlick_disabled) {
                int distance = _x_rel * _x_rel + _y_rel * _y_rel;
                const int threshold = 50;
                if (distance >= threshold * threshold) {
                    //CAMERALOG("#### flick = 0x%x, from (%d, %d) with movement (%d, %d)",
                    //    _flickValue, _x, _y, _x_rel, _y_rel);
                    _dataReceived |= DATA_TYPE_flick;
                    _hasFlickEvent = true;
                } else {
                    //CAMERALOG("#### flick = 0x%x, from (%d, %d) with movement "
                    //    "(%d, %d) which is smaller than %d pixels",
                    //    _flickValue, _x, _y, _x_rel, _y_rel, threshold);
                }
            }
        } else if (eventType == 0x23) {
            //CAMERALOG("#### double click on (%d, %d)", _x, _y);
            _dataReceived |= DATA_TYPE_double_click;
            _hasFlickEvent = true;
        }
    }
    return false;
}

};
