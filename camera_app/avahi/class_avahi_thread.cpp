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


CAvahiServerThread* CAvahiServerThread::_pInstance = NULL;

void CAvahiServerThread::destroy()
{
    if (_pInstance != NULL)
    {
        _pInstance->StopAvahi();
        _pInstance->Stop();
        delete _pInstance;
    }
}
void CAvahiServerThread::create()
{
    if (_pInstance == NULL)
    {
        _pInstance = new CAvahiServerThread();
        _pInstance->Go();
    }
}

void CAvahiServerThread::avahi_entry_group_callback
    (AvahiServer *pserver
     , AvahiSEntryGroup *pgroup
     , AvahiEntryGroupState state
     , void* userdata)
{
    CAvahiServerThread *p_server_info = NULL;
    p_server_info = (CAvahiServerThread *)userdata;
    if (!p_server_info) {
        return;
    }

    switch (state) {
        case AVAHI_ENTRY_GROUP_UNCOMMITED:
            CAMERALOG("avahi AVAHI_ENTRY_GROUP_UNCOMMITED");
            break;
        case AVAHI_ENTRY_GROUP_REGISTERING:
            CAMERALOG("avahi AVAHI_ENTRY_GROUP_REGISTERING");
            break;
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            CAMERALOG("avahi AVAHI_ENTRY_GROUP_ESTABLISHED");
            CAMERALOG("Service established under name <%s>",
                p_server_info->_pserver_name);
            break;
        case AVAHI_ENTRY_GROUP_COLLISION:
            CAMERALOG("avahi AVAHI_ENTRY_GROUP_COLLISION");
            {
                char *n;

                n = avahi_alternative_service_name(p_server_info->_pserver_name);
                avahi_free(p_server_info->_pserver_name);
                p_server_info->_pserver_name = n;

                CAMERALOG("Service name collision, renaming service to '%s'",
                    p_server_info->_pserver_name);
                avahi_create_services(pserver, p_server_info);
            }
            break;
        case AVAHI_ENTRY_GROUP_FAILURE :
            CAMERALOG("avahi AVAHI_ENTRY_GROUP_FAILURE");
            CAMERALOG("Entry group failure: %s\n", 
                avahi_strerror(avahi_server_errno(pserver)));
            avahi_simple_poll_quit(p_server_info->_simple_poll);
            break;
        default:
            //AGLOG_DEBUG("%s: entry group state[%i]\n", __func__, state);
            break;
    }
}

void CAvahiServerThread::avahi_create_services
    (AvahiServer *pserver, CAvahiServerThread *p_server_info)
{
    if (!p_server_info) {
        return;
    }

    int ret_val;

    if (p_server_info->_pserver_group == NULL) {
        p_server_info->_pserver_group = avahi_s_entry_group_new(pserver,
            avahi_entry_group_callback, p_server_info);
        if (p_server_info->_pserver_group == NULL) {
            goto agms_avahi_create_services_error;
        }
    }

    ret_val = avahi_server_add_service(pserver
        , p_server_info->_pserver_group
        , AVAHI_IF_UNSPEC
        , AVAHI_PROTO_INET
        , (AvahiPublishFlags)0
        , p_server_info->_pserver_name
        , "_ccam._tcp"
        , "local"
        , NULL
        , CCAME_TCP_PORT
        , "path=/index.html"
        , NULL);
    if (ret_val < 0) {
        CAMERALOG("avahi_server_add_service() = %d: %s", ret_val, avahi_strerror(ret_val));
        goto agms_avahi_create_services_error;
    }

    ret_val = avahi_s_entry_group_commit(p_server_info->_pserver_group);
    if (ret_val < 0) {
        CAMERALOG("avahi_s_entry_group_commit() = %d: %s", ret_val, avahi_strerror(ret_val));
        goto agms_avahi_create_services_error;
    }

    goto agms_avahi_create_services_exit;

agms_avahi_create_services_error:
    avahi_simple_poll_quit(p_server_info->_simple_poll);

agms_avahi_create_services_exit:
    return;
}

void CAvahiServerThread::avahi_delete_services
    (AvahiServer *pserver, CAvahiServerThread *p_server_info)
{
    AGBOX_UNUSED(pserver);
    if (!p_server_info) {
        return;
    }

    if (p_server_info->_pserver_group) {
        avahi_s_entry_group_reset(p_server_info->_pserver_group);
    }
}

void CAvahiServerThread::avahi_server_callback
    (AvahiServer *pserver
     , AvahiServerState state
     , void* userdata)
{
    int ret_val;
    CAvahiServerThread *p_server_info;
    p_server_info = (CAvahiServerThread *)userdata;
    if (!p_server_info) {
        return;
    }

    p_server_info->_server_state = state;
    switch (state) {
        case AVAHI_SERVER_RUNNING:
            CAMERALOG("avahi AVAHI_SERVER_RUNNING");
            CAMERALOG("avahi server_name = %s", p_server_info->_pserver_name);
            avahi_create_services(pserver, p_server_info);
            break;
        case AVAHI_SERVER_COLLISION:
            CAMERALOG("avahi AVAHI_SERVER_COLLISION");
            {
                char *n;

                avahi_delete_services(pserver, p_server_info);
                n = avahi_alternative_host_name(
                    avahi_server_get_host_name(pserver));
                CAMERALOG("Host name conflict, retrying with <%s>", n);
                ret_val = avahi_server_set_host_name(pserver, n);
                avahi_free(n);
                if (ret_val < 0) {
                    CAMERALOG("Failed to set new host name: %s", avahi_strerror(ret_val));
                    avahi_simple_poll_quit(p_server_info->_simple_poll);
                    return;
                }
            }
            break;
        case AVAHI_SERVER_REGISTERING:
            CAMERALOG("avahi AVAHI_SERVER_REGISTERING");
            break;
        case AVAHI_SERVER_FAILURE:
            CAMERALOG("avahi AVAHI_SERVER_FAILURE: %s",
                avahi_strerror(avahi_server_errno(pserver)));
            avahi_simple_poll_quit(p_server_info->_simple_poll);
            break;
        case AVAHI_SERVER_INVALID:
            CAMERALOG("avahi AVAHI_SERVER_INVALID");
            break;
        default:
            CAMERALOG("avahi default");
            break;
    }
}

void CAvahiServerThread::main()
{
    int ret_val;
    const AvahiPoll *poll_api;
    AvahiServerConfig config;

restart_service:
    CAMERALOG("try start avahi servie");

#if defined(PLATFORMHELLFIRE) || defined(PLATFORMDIABLO)
    _pserver_name = avahi_strdup("Vidit Camera");
#elif defined(PLATFORMHACHI)
    _pserver_name = avahi_strdup("Waylens Camera");
#elif defined(PLATFORM22A)
    _pserver_name = avahi_strdup("HD22A Camera");
#endif
    _pserver_group = NULL;

    _simple_poll = avahi_simple_poll_new();
    if (_simple_poll == NULL) {
        ret_val = -1;
        CAMERALOG("avahi_simple_poll_new() = NULL");
        goto agms_avahi_free_server_name;
    }

    poll_api = avahi_simple_poll_get(_simple_poll);
    if (poll_api == NULL) {
        ret_val = -1;
        CAMERALOG("avahi_simple_poll_get() = NULL");
        goto agms_avahi_simple_poll_free;
    }

    avahi_server_config_init(&config);
    config.use_ipv6 = 0;
    config.publish_hinfo = 0;
    config.publish_workstation = 0;
    _pserver = avahi_server_new(poll_api, &config,
        avahi_server_callback, this, &ret_val);
    avahi_server_config_free(&config);

    ret_val = avahi_simple_poll_loop(_simple_poll);
    if (_pserver_group) {
        avahi_s_entry_group_free(_pserver_group);
        _pserver_group = NULL;
    }

    if (_pserver) {
        avahi_server_free(_pserver);
        _pserver = NULL;
    }

agms_avahi_simple_poll_free:
    avahi_simple_poll_free(_simple_poll);
    _simple_poll = NULL;

agms_avahi_free_server_name:
    avahi_free(_pserver_name);
    _pserver_name = NULL;

    if (_restart) {
        _restart = false;
        goto restart_service;
    }

    _stopSignal->Give();
    CAMERALOG("---avahi stop");
    return;
}

CAvahiServerThread::CAvahiServerThread()
    : CThread("avahiThread")
    , _pserver_name(NULL)
    , _simple_poll(NULL)
    , _pserver_group(NULL)
    , _pserver(NULL)
    , _server_state(AVAHI_SERVER_INVALID)
    , _restart(false)
{
    _stopSignal = new CSemaphore(0, 1, "avahi stop sem");
    _pCamera = new CCAM_mirror(NULL, CCAME_TCP_PORT);
}

CAvahiServerThread::~CAvahiServerThread()
{
    delete _stopSignal;
    delete _pCamera;
    CAMERALOG("destroyed");
}

void CAvahiServerThread::StopAvahi()
{
    avahi_simple_poll_quit(_simple_poll);
    _stopSignal->Take(-1);
}

void CAvahiServerThread::RestartAvahi()
{
    CAMERALOG("restart avahi");
    if (_simple_poll != NULL) {
        _restart = true;
        avahi_simple_poll_quit(_simple_poll);
    }
}

