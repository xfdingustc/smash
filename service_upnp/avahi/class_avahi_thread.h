/*****************************************************************************
 * class_avahi_thread.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2013, linnsong.
 *
 *
 *****************************************************************************/

#ifndef __H_class_avahi_thread__
#define __H_class_avahi_thread__

#include "GobalMacro.h"
#include "ClinuxThread.h"

#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/alternative.h>
#include <avahi-common/timeval.h>
#include <avahi-common/error.h>

#include <avahi-core/core.h>
#include <avahi-core/log.h>
#include <avahi-core/publish.h>
#include <avahi-core/lookup.h>
#include "sd_general_description.h"
#include "class_TCPService.h"
#include "ccam_mirror.h"

class MotoController;
class StandardTCPService;
class CloudCameraCMDProcessor;

class CAvahiServerThread : public CThread
{
public:
	static CAvahiServerThread* getInstance(){return _pInstance;};
	static void destroy();
	static void create();

	static void avahi_server_callback(AvahiServer *pserver, AvahiServerState state, void* userdata);
	static void avahi_entry_group_callback(AvahiServer *pserver, AvahiSEntryGroup *pgroup, AvahiEntryGroupState state, void* userdata);
	static void avahi_delete_services(AvahiServer *pserver, CAvahiServerThread *p_server_info);
	static void avahi_create_services(AvahiServer *pserver, CAvahiServerThread *p_server_info);
	void StopAvahi();

protected:	
	virtual void main(void * p);
		
private:
	static CAvahiServerThread* 	_pInstance;
	CAvahiServerThread();
	virtual ~CAvahiServerThread(); 
	
private:
	CSemaphore	*_stopSignal;
	char *pserver_name;
	AvahiSimplePoll *simple_poll;
	AvahiSEntryGroup *pserver_group;
	//StandardTCPService	*_ccamService;
	//TCPSessionService *_ccamService;
	//CloudCameraCMDProcessor *_processor;
	CCAM_mirror			*_pCamera;
};

/*
thread to liseon to a dedicated port for remote get / push.
1. get camera mode.
2. get camera stats.
3. get event port.
4. get stream address.
5. get playback 
*/
#define avahi_cmd_buffer_size 10240
#define avahi_cmd_session_size 10240
#define CCAME_TCP_PORT 10086
#define avahi_maxConnection 24

struct CCamClient
{
	int		_socket;
	int		_eventTimeOut;
	bool	_inControl;
};

/*
#define motoNode "/proc/agb_irm"
#define backlash 430
class MotoController
{
public:
	MotoController(char* node);
	~MotoController();

	void RotateAbsolutley(int angle);
	void RotateRelatively(int angle);
	void Step(int steps);
	
	
private:
	int _lastMovementAmount;
	int _fd;
};*/

#endif

