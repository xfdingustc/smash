/*****************************************************************************
 * CkeyEventProcessThread.cpp:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "agsys_input.h"
#include "agbox.h"

#include "GobalMacro.h"
#include "CKeyEventProcessThread.h"

#define PropName_Input_Device       "temp.input.device"

EventProcessThread* EventProcessThread::pInstance = NULL;

/***************************************************************
* EventProcessThread::getEPTObject
*/
EventProcessThread* EventProcessThread::getEPTObject()
{
    if (pInstance == NULL) {
        pInstance = new EventProcessThread();
        pInstance->Go();
    }
    return pInstance;
}

/***************************************************************
* EventProcessThread::destroyEPTObject
*/
void EventProcessThread::destroyEPTObject()
{
    if (pInstance != NULL) {
        //_bExecuting = false;
        pInstance->Stop();
        delete pInstance;
    }
}

int EventProcessThread::InputDevicePropertyWaitCB
    (const char *key, const char *value)
{
    if (pInstance) {
        pInstance->inputDeviceChanged();
    }
    return 0;
}

/***************************************************************
* EventProcessThread::
*/
void EventProcessThread::main()
{
    if (_devs[0]._fd < 0) {
        CAMERALOG("_devs[0]._fd < 0, cant start Event Process Thread normally!");
        return;
    }

    struct agcmd_property_wait_info_s *pwait_info1 = agcmd_property_wait_start(
        PropName_Input_Device, InputDevicePropertyWaitCB);
    if (pwait_info1 == NULL) {
        CAMERALOG("pwait_info1 failed to start");
    }

    fd_set readfds;
    int Maxfd;
    struct timeval timeout;

    _bExecuting = true;
    int rt = 0;
    int i = 0, j = 0;
    while (_bExecuting) {
        //_mutex->Take();

        updateInputDevices();

        FD_ZERO(&readfds);
        Maxfd = 0;
        for (i = 0; i < _numDevs; i++) {
            if (_devs[i]._fd >= 0) {
                FD_SET(_devs[i]._fd, &readfds);
                Maxfd = (_devs[i]._fd > (Maxfd - 1))? (_devs[i]._fd + 1) : Maxfd;
            }
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000;
        switch (select(Maxfd, &readfds, NULL, NULL, &timeout))
        {
            case -1:
                CAMERALOG("select error in EventProcessThread");
                break;
            case 0:
                break;
            default:
                for (i = 0; i < _numDevs; i++) {
                    if (FD_ISSET(_devs[i]._fd, &readfds)) {
                        rt = read(_devs[i]._fd, _ev, sizeof(_ev));
                        if (rt < (int)sizeof(struct input_event)) {
                            //CAMERALOG("--evnet short read %d", rt);
                            continue;
                        }

                        rt /= sizeof(struct input_event);
                        for (j = 0; j < rt; j++) {
                            event_process(&_ev[j]);
                        }
                    }
                }
                break;
        }

        //_mutex->Give();
    }

    agcmd_property_wait_stop(pwait_info1);
}

int EventProcessThread::AddProcessor(KeyEventProcessor* pr)
{
    if (_addedNum < KEY_EVENT_PROCESSOR_NUM) {
        _keyEventProcessor[_addedNum] = pr;
        _addedNum++;
        return _addedNum;
    }

    return -1;
}

int EventProcessThread::RemoveProcessor(KeyEventProcessor* pr)
{
    int i = 0;
    for (i = 0; i < _addedNum; i++) {
        if (_keyEventProcessor[_addedNum] == pr) {
            break;
        }
    }
    if (i < _addedNum) {
        for (int j = i; j < _addedNum-1; j++) {
            _keyEventProcessor[j] = _keyEventProcessor[j + 1];
        }
        _addedNum --;
    }

    return _addedNum;
}

/***************************************************************
* EventProcessThread::
*/
EventProcessThread::EventProcessThread()
    : CThread("EventProcessThread")
    , _bDeviceChanged(false)
    , _numDevs(0)
    , _addedNum(0)
{
    _mutex = new CMutex("EventProcessThread mutex");

    int ret_val = -1;
    struct agsys_input_event_list_s event_list;
    ret_val = agsys_input_list_event(&event_list);
    if (ret_val) {
        CAMERALOG("get input list with error %d", ret_val);
        return;
    }

    for (int i = 0; i < event_list.valid_num; i++) {
        strcpy(_devs[i]._name, event_list.info[i].devname);
        _devs[i]._fd = open(_devs[i]._name, O_RDONLY);
        if (_devs[i]._fd < 0) {
            CAMERALOG("open %s error", _devs[i]._name);
            return;
        }
        _numDevs++;
    }
}

/***************************************************************
* EventProcessThread::~EventProcessThread
*/
EventProcessThread::~EventProcessThread()
{
    for (int i = 0; i < _numDevs; i++) {
        if (_devs[i]._fd >= 0) {
            close(_devs[i]._fd);
            _devs[i]._fd = -1;
        }
    }

    delete _mutex;
    CAMERALOG("%s() destroyed", __FUNCTION__);
}

/***************************************************************
* EventProcessThread::event_process
*/
void EventProcessThread::event_process(struct input_event* pev)
{
    bool rt;
    for (int i = 0; i < _addedNum; i++) {
        if (_keyEventProcessor[i] != NULL) {
            rt = _keyEventProcessor[i]->event_process(pev);
            if (!rt) {
                break;
            }
        }
    }
}

void EventProcessThread::inputDeviceChanged()
{
    CAMERALOG("input device changed Enter");

    //_mutex->Take();
    _bDeviceChanged = true;
    CAMERALOG("input device changed");
    //_mutex->Give();
}

void EventProcessThread::updateInputDevices()
{
    if (!_bDeviceChanged) {
        return;
    }

    char tmp[256] = {0x00};
    agcmd_property_get(PropName_Input_Device, tmp, "");
    int action = 0;
    char dev[16];
    if (strncmp(tmp, "add:", strlen("add:")) == 0) {
        action = 1;
        strcpy(dev, tmp + strlen("add:"));
    } else if (strncmp(tmp, "remove:", strlen("remove:")) == 0) {
        action = 2;
        strcpy(dev, tmp + strlen("remove:"));
    }
    CAMERALOG("tmp = %s, action = %d, dev = %s", tmp, action, dev);

    switch (action) {
        case 1:
        {
            bool found = false;
            for (int i = 0; i < _numDevs; i++) {
                if (strcmp(_devs[i]._name, dev) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                strcpy(_devs[_numDevs]._name, dev);
                _devs[_numDevs]._fd = open(_devs[_numDevs]._name, O_RDONLY);
                if (_devs[_numDevs]._fd < 0) {
                    CAMERALOG("open %s error", _devs[_numDevs]._name);
                    _bDeviceChanged = false;
                    return;
                }
                _numDevs++;
            }
        }
        break;
        case 2:
        {
            int index = _numDevs;
            for (int i = 0; i < _numDevs; i++) {
                if (strcmp(_devs[i]._name, dev) == 0) {
                    index = i;
                    break;
                }
            }
            if (index < _numDevs) {
                if (_devs[index]._fd >= 0) {
                    close(_devs[index]._fd);
                    _devs[index]._fd = -1;
                }
                memset(_devs[index]._name, 0x00, 32);
                for (int j = index; j < _numDevs - 1; j++) {
                    _devs[j]._fd = _devs[j + 1]._fd;
                    strcpy(_devs[j]._name, _devs[j + 1]._name);
                }
                _numDevs--;
            }
        }
        break;
    }

    _bDeviceChanged = false;
}

