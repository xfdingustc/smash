
/*****************************************************************************
 * CKeyEventProcessThread.h
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
#ifndef __H_class_event_proc_thread__
#define __H_class_event_proc_thread__

#include "ClinuxThread.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include <unistd.h>

class KeyEventProcessor
{
public:
	KeyEventProcessor(void* obj){_obj = obj;};
	~KeyEventProcessor(){};
	virtual bool event_process(struct input_event* ev);
private:
	void* _obj;
};

#define KEY_EVENT_PROCESSOR_NUM 5
class EventProcessThread : public CThread
{
public:
	static EventProcessThread* getEPTObject();
	static void destroyEPTObject();

	int AddProcessor(KeyEventProcessor* pr);
	int RemoveProcessor(KeyEventProcessor* pr);
protected:
	void main(void *);
private:

	EventProcessThread();
	~EventProcessThread();
	static EventProcessThread* pInstance;

	bool				_bExecuting;

	void event_process(struct input_event* ev);
	
	struct input_event	_ev[64];
	int					_event_fd;
	int					_event_version;
	int					_event_id;
	char				_event_name[16];
	char				_event_location[32];
	KeyEventProcessor	*_keyEventProcessor[KEY_EVENT_PROCESSOR_NUM];
	int					_addedNum;
	
};

#endif

