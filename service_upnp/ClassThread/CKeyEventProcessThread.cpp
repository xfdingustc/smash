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

#include "CKeyEventProcessThread.h"

bool KeyEventProcessor::event_process(struct input_event* ev)
{
	return false;
}

/***************************************************************
* EventProcessThread::getEPTObject
*/
static const char* event_name = "/dev/input/event0";
EventProcessThread* EventProcessThread::pInstance = NULL;
EventProcessThread* EventProcessThread::getEPTObject()
{
	if(pInstance == NULL)
	{
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
	if(pInstance != NULL)
	{
		//_bExecuting = false;
		pInstance->Stop();
		delete pInstance;
	}
}

/***************************************************************
* EventProcessThread::
*/
void EventProcessThread::main(void *)
{
	if(_event_fd < 0)
	{
		printf("event_fd < 0, cant start Event Process Thread normally!\n");	
		return;
	}
	
	_bExecuting = true;
	int rt;
	int i;
	while(_bExecuting)
	{
		//printf("--start event process\n");
		rt = read(_event_fd, _ev, sizeof(_ev));
		if (rt < (int)sizeof(struct input_event)) {
			printf("--evnet short read %d\n", rt);
			continue;
		}

		rt /= sizeof(struct input_event);
		for (i = 0; i < rt; i++) {
			event_process(&_ev[i]);
		}

	}
}

int EventProcessThread::AddProcessor(KeyEventProcessor* pr)
{
	if(_addedNum < KEY_EVENT_PROCESSOR_NUM)
	{
		_keyEventProcessor[_addedNum] = pr;
		_addedNum++;
	}
	else
		return -1;
}

int EventProcessThread::RemoveProcessor(KeyEventProcessor* pr)
{
	int i = 0;
	for(i = 0; i < _addedNum; i++)
	{
		if(_keyEventProcessor[_addedNum] == pr)
		{
			break;
		}
	}
	if(i < _addedNum)
	{
		for(int j = i; j < _addedNum-1; j++)
		{
			_keyEventProcessor[j] = _keyEventProcessor[j + 1];
		}
		_addedNum --;
	}
}

/***************************************************************
* EventProcessThread::
*/
EventProcessThread::EventProcessThread
	(): CThread("EventProcessThread", CThread::NORMAL, 0, NULL)
	,_addedNum(0)
{
	_event_fd = open(event_name, O_RDONLY);
	if(_event_fd < 0)
	{
		printf("event fd open error");
		return;
	}
	/*int ret_val = ioctl(_event_fd, EVIOCGVERSION,	_event_version);
	if (ret_val < 0) {
		printf("Can't get driver version for %s\n", _event_name);
		goto init_error;
	}

	ret_val = ioctl(_event_fd, EVIOCGID, _event_id);
	if (ret_val < 0) {
		printf("Can't get driver id for %s\n", _event_name);
		goto init_error;
	}

	ret_val = ioctl(_event_fd, EVIOCGNAME(sizeof(_event_name) - 1), _event_name);
	if (ret_val < 0) {
		printf("Can't get driver name for %s\n", _event_name);
		goto init_error;
	}

	ret_val = ioctl(_event_fd, EVIOCGPHYS(sizeof(_event_location) - 1), _event_location);
	if (ret_val < 0) {
		printf("Can't get driver location for %s\n", _event_name);
		goto init_error;
	}*/

	
	
	return;

//init_error: 
//	close(_event_fd);
//	_event_fd = -1;
	
}

/***************************************************************
* EventProcessThread::~EventProcessThread
*/
EventProcessThread::~EventProcessThread()
{
	close(_event_fd);
	_event_fd = -1;
}

/***************************************************************
* EventProcessThread::event_process
*/

void EventProcessThread::event_process(struct input_event* pev)
{
	bool rt;
	for(int i = 0; i < _addedNum; i++)
	{
		if(_keyEventProcessor[i] != NULL)
		{
			rt = _keyEventProcessor[i]->event_process(pev);
			if(!rt)
				break;
		}
	}
}
