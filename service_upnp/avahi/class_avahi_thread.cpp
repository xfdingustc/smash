#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <getopt.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "agbox.h"
#include "aglog.h"
#include "class_avahi_thread.h"
#include "ccam_cmds.h"
#include "ccam_cmd_processor.h"



/***************************************************************
CAvahiServerThread::avahi_entry_group_callback()
*/
void CAvahiServerThread::avahi_entry_group_callback
	(AvahiServer *pserver
	,AvahiSEntryGroup *pgroup
	, AvahiEntryGroupState state
	, void* userdata)
{
	CAvahiServerThread *p_server_info;

	//AGBOX_UNUSED(pgroup);
	p_server_info = (CAvahiServerThread *)userdata;

	switch (state) {
	case AVAHI_ENTRY_GROUP_UNCOMMITED:
		printf("avahi AVAHI_ENTRY_GROUP_UNCOMMITED\n");
		//AGLOG_DEBUG("%s: AVAHI_ENTRY_GROUP_UNCOMMITED\n", __func__);
		break;
	case AVAHI_ENTRY_GROUP_REGISTERING:
		printf("avahi AVAHI_ENTRY_GROUP_REGISTERING\n");
		//AGLOG_DEBUG("%s: AVAHI_ENTRY_GROUP_REGISTERING\n", __func__);
		break;
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
		printf("avahi AVAHI_ENTRY_GROUP_ESTABLISHED\n");
		//AGLOG_DEBUG("%s: AVAHI_ENTRY_GROUP_ESTABLISHED\n", __func__);
		//AGLOG_DEBUG("%s: Service established under name <%s>\n",
		//	__func__, p_server_info->pserver_name);
		break;
	case AVAHI_ENTRY_GROUP_COLLISION:
		printf("avahi AVAHI_ENTRY_GROUP_COLLISION\n");
		//AGLOG_DEBUG("%s: AVAHI_ENTRY_GROUP_COLLISION\n", __func__);
	{
		char *n;

		n = avahi_alternative_service_name(p_server_info->pserver_name);
		avahi_free(p_server_info->pserver_name);
		p_server_info->pserver_name = n;

		//AGLOG_NOTICE("%s: Service name collision, renaming service"
		//	" to '%s'\n", __func__, p_server_info->pserver_name);
		avahi_create_services(pserver, p_server_info);
	}
		break;

	case AVAHI_ENTRY_GROUP_FAILURE :
		printf("avahi AVAHI_ENTRY_GROUP_FAILURE\n");
		//AGLOG_DEBUG("%s: AVAHI_ENTRY_GROUP_FAILURE\n", __func__);
		//AGLOG_ERR("%s: Entry group failure: %s\n", __func__,
		//	avahi_strerror(avahi_server_errno(pserver)));
		avahi_simple_poll_quit(p_server_info->simple_poll);
		break;
	default:
		//AGLOG_DEBUG("%s: entry group state[%i]\n", __func__, state);
		break;
	}
}

void CAvahiServerThread::avahi_create_services
	(AvahiServer *pserver,
	CAvahiServerThread *p_server_info)
{
	int ret_val;

	if (p_server_info->pserver_group == NULL) {
		p_server_info->pserver_group = avahi_s_entry_group_new(pserver,
			avahi_entry_group_callback, p_server_info);
		if (p_server_info->pserver_group == NULL) {
			goto agms_avahi_create_services_error;
		}
	}
	ret_val = avahi_server_add_service(pserver
		,p_server_info->pserver_group
		,AVAHI_IF_UNSPEC
		,AVAHI_PROTO_INET
		,(AvahiPublishFlags)0
		,p_server_info->pserver_name
		,"_ccam._tcp"
		,"local"
		, NULL
		, CCAME_TCP_PORT
		, "path=/index.html"
		, NULL);
	if (ret_val < 0) {
		//AGLOG_ERR("%s: avahi_server_add_service() = %s\n",
		//	__func__, avahi_strerror(ret_val));
		goto agms_avahi_create_services_error;
	}

	ret_val = avahi_s_entry_group_commit(p_server_info->pserver_group);
	if (ret_val < 0) {
		goto agms_avahi_create_services_error;
	}
	goto agms_avahi_create_services_exit;

agms_avahi_create_services_error:
	avahi_simple_poll_quit(p_server_info->simple_poll);
agms_avahi_create_services_exit:
	return;
}

void CAvahiServerThread::avahi_delete_services
	(AvahiServer *pserver
	,CAvahiServerThread *p_server_info)
{
	AGBOX_UNUSED(pserver);

	if (p_server_info->pserver_group) {
		avahi_s_entry_group_reset(p_server_info->pserver_group);
	}
}

void CAvahiServerThread::avahi_server_callback
	(AvahiServer *pserver
	,AvahiServerState state
	, void* userdata)
{
	int ret_val;
	CAvahiServerThread *p_server_info;

	p_server_info = (CAvahiServerThread *)userdata;

	switch (state) {
	case AVAHI_SERVER_RUNNING:
		printf("avahi AVAHI_SERVER_RUNNING\n");
		printf("avahi server_name = %s\n", p_server_info->pserver_name);
		avahi_create_services(pserver, p_server_info);
		break;
	case AVAHI_SERVER_COLLISION:
		//AGLOG_DEBUG("%s: AVAHI_SERVER_COLLISION\n", __func__);
		printf("avahi AVAHI_SERVER_COLLISION\n");
	{
		char *n;

		avahi_delete_services(pserver, p_server_info);
		n = avahi_alternative_host_name(
			avahi_server_get_host_name(pserver));
		//AGLOG_NOTICE("%s: Host name conflict, retrying with <%s>\n",
		//	__func__, n);
		ret_val = avahi_server_set_host_name(pserver, n);
		avahi_free(n);
		if (ret_val < 0) {
			//AGLOG_ERR("%s: Failed to set new host name: %s\n",
			//	__func__, avahi_strerror(ret_val));
			avahi_simple_poll_quit(p_server_info->simple_poll);
			return;
		}
	}
		break;
	case AVAHI_SERVER_REGISTERING:
		printf("avahi AVAHI_SERVER_REGISTERING\n");
		//AGLOG_DEBUG("%s: AVAHI_SERVER_REGISTERING\n", __func__);
		break;
	case AVAHI_SERVER_FAILURE:
		printf("avahi AVAHI_SERVER_FAILURE : %s\n", avahi_strerror(avahi_server_errno(pserver)));
		//AGLOG_DEBUG("%s: AVAHI_SERVER_FAILURE\n", __func__);
		//AGLOG_ERR("%s: Server failure: %s\n", __func__,
		//	avahi_strerror(avahi_server_errno(pserver)));
		avahi_simple_poll_quit(p_server_info->simple_poll);
		break;
	case AVAHI_SERVER_INVALID:
		printf("avahi AVAHI_SERVER_INVALID\n");
	default:
		printf("avahi default\n");
		break;
	}
}
void CAvahiServerThread::main(void * p)
{
	int ret_val;
	const AvahiPoll *poll_api;
	AvahiServerConfig config;
	//struct sigaction sig_action;
	//sigset_t sig_mask;
	AvahiServer *server = NULL;
	printf("try start avahi servie\n");

	//_ccamService->Go();
	/*ret_val = agms_avahi_init_param(argc, argv);
	if (ret_val) {
		agms_avahi_usage();
		goto agms_avahi_exit;
	}*/

	//ret_val = aglog_init(AGMS_AVAHI_NAME,
	//	agms_avahi_log_mode, agms_avahi_log_priority);
	//if (ret_val < 0) {
	//	goto agms_avahi_exit;
	//}
	//AGLOG_INFO("Start...\n");

	pserver_name = avahi_strdup("Vidit Camera");
	pserver_group = NULL;
	simple_poll = avahi_simple_poll_new();
	if (simple_poll == NULL) {
		ret_val = -1;
		//AGLOG_ERR("%s: avahi_simple_poll_new() = NULL\n", __func__);
		goto agms_avahi_free_server_name;
	}
	poll_api = avahi_simple_poll_get(simple_poll);
	if (poll_api == NULL) {
		ret_val = -1;
		//AGLOG_ERR("%s: avahi_simple_poll_get() = NULL\n", __func__);
		goto agms_avahi_simple_poll_free;
	}

	avahi_server_config_init(&config);
	config.use_ipv6 = 0;
	config.publish_hinfo = 0;
	config.publish_workstation = 0;
	server = avahi_server_new(poll_api, &config,
		avahi_server_callback, this, &ret_val);
	avahi_server_config_free(&config);

	/*sigemptyset(&sig_mask);
	sig_action.sa_mask = sig_mask;
	sig_action.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
	sig_action.sa_restorer = NULL;
	sig_action.sa_sigaction = agms_sa_sigaction;
	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGQUIT, &sig_action, NULL);
	sigaction(SIGTERM, &sig_action, NULL);*/

	ret_val = avahi_simple_poll_loop(simple_poll);
	if (pserver_group)
		avahi_s_entry_group_free(pserver_group);
	if (server)
		avahi_server_free(server);
agms_avahi_simple_poll_free:
	avahi_simple_poll_free(simple_poll);
agms_avahi_free_server_name:
	avahi_free(pserver_name);
	//_ccamService->Stop();
	_stopSignal->Give();
	printf("---avahi stop;\n");
//agms_avahi_aglog_deinit:
	//AGLOG_INFO("Exit...\n");
	//aglog_deinit();
//agms_avahi_exit:
	//exit(ret_val);
	return;
}

CAvahiServerThread::CAvahiServerThread
	():CThread("avahiThread", CThread::NORMAL, 0, NULL)
{
	_stopSignal = new CSemaphore(0, 1, "avahi stop sem");
	_pCamera = new CCAM_mirror(NULL, CCAME_TCP_PORT);
};


CAvahiServerThread::~CAvahiServerThread()
{
	delete _stopSignal;
	delete _pCamera;
}

void CAvahiServerThread::StopAvahi()
{
	avahi_simple_poll_quit(simple_poll);
	_stopSignal->Take(-1);
}

CAvahiServerThread* CAvahiServerThread::_pInstance = NULL;
void CAvahiServerThread::destroy()
{
	if(_pInstance != NULL)
	{
		_pInstance->StopAvahi();
		_pInstance->Stop();
		delete _pInstance;
	}
}
void CAvahiServerThread::create()
{
	if(_pInstance == NULL)
	{
		_pInstance = new CAvahiServerThread();
		_pInstance->Go();
	}
}

