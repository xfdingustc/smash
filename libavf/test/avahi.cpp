
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/alternative.h>
#include <avahi-common/timeval.h>
#include <avahi-common/error.h>

#include <avahi-core/core.h>
#include <avahi-core/log.h>
#include <avahi-core/publish.h>
#include <avahi-core/lookup.h>

#if 1
#define AVAHI_LOGD(_expr...) \
	do { \
		if (0) { AVF_LOGD(_expr); } \
	} while (0)
#define AVAHI_LOGI(_expr...) \
	do { \
		if (0) { AVF_LOGI(_expr); } \
	} while (0)
#else
#define AVAHI_LOGD	AVF_LOGD
#define AVAHI_LOGI	AVF_LOGI
#endif

//-----------------------------------------------------------------------
//
//  CAvahi
//
//-----------------------------------------------------------------------

class CAvahi: public CObject
{
	typedef CObject inherited;

public:
	static CAvahi *Create(const char *name,
		const char *type, const char *domain,
		const char *host, int port);

private:
	CAvahi(const char *service_name,
		const char *type, const char *domain, const char *host, int port);
	virtual ~CAvahi();
	void RunThread();

private:
	static void ThreadEntry(void *p);
	void ThreadLoop();

private:
	static void avahi_server_callback(AvahiServer *server, 
		AvahiServerState state, void *userdata);
	static void avahi_entry_group_callback(AvahiServer *server,
		AvahiSEntryGroup *group, AvahiEntryGroupState state,
		void *userdata);

private:
	void CreateAvahiServices(AvahiServer *server);
	void DeleteAvahiServices(AvahiServer *server);

private:
	char *mp_server_name;
	char *mp_server_type;
	char *mp_domain;
	char *mp_host;
	int m_port;

	AvahiSimplePoll *mp_simple_poll;
	AvahiSEntryGroup *mp_server_group;
	CThread *mpThread;
};

//-----------------------------------------------------------------------
//
//  CAvahi
//
//-----------------------------------------------------------------------

CAvahi *CAvahi::Create(const char *name,
	const char *type, const char *domain, 
	const char *host, int port)
{
	CAvahi *result = avf_new CAvahi(name, type, domain, host, port);
	if (result) {
		result->RunThread();
	}
	CHECK_STATUS(result);
	return result;
}

CAvahi::CAvahi(const char *name,
	const char *type, const char *domain, 
	const char *host, int port):

	mp_server_name(NULL),
	mp_server_type(NULL),
	mp_domain(NULL),
	mp_host(NULL),
	m_port(port),

	mp_simple_poll(NULL),
	mp_server_group(NULL),
	mpThread(NULL)
{
	CREATE_OBJ(mp_server_name = avf_strdup(name));
	CREATE_OBJ(mp_server_type = avf_strdup(type));
	CREATE_OBJ(mp_domain = avf_strdup(domain));

	if (host) {
		CREATE_OBJ(mp_host = avf_strdup(host));
	}

	CREATE_OBJ(mp_simple_poll = avahi_simple_poll_new());
}

void CAvahi::RunThread()
{
	CREATE_OBJ(mpThread = CThread::Create("avahi", ThreadEntry, this));
}

CAvahi::~CAvahi()
{
	if (mp_simple_poll) {
		avahi_simple_poll_quit(mp_simple_poll);
	}

	avf_safe_release(mpThread);

	if (mp_simple_poll) {
		avahi_simple_poll_free(mp_simple_poll);
	}

	avf_safe_free(mp_server_name);
	avf_safe_free(mp_server_type);
	avf_safe_free(mp_domain);
	avf_safe_free(mp_host);
}

void CAvahi::ThreadEntry(void *p)
{
	CAvahi *self = (CAvahi*)p;
	self->ThreadLoop();
}

void CAvahi::avahi_server_callback(AvahiServer *server, 
	AvahiServerState state, void *userdata)
{
	CAvahi *self = (CAvahi*)userdata;

	switch (state) {
	case AVAHI_SERVER_RUNNING:
		AVAHI_LOGD(C_BROWN "AVAHI_SERVER_RUNNING" C_NONE);
		self->CreateAvahiServices(server);
		break;

	case AVAHI_SERVER_COLLISION: {
			AVAHI_LOGD(C_BROWN "AVAHI_SERVER_COLLISION" C_NONE);
		}
		break;

	case AVAHI_SERVER_REGISTERING:
		AVAHI_LOGD(C_BROWN "AVAHI_SERVER_REGISTERING" C_NONE);
		break;

	case AVAHI_SERVER_FAILURE:
		AVAHI_LOGD(C_BROWN "AVAHI_SERVER_FAILURE" C_NONE);
		avahi_simple_poll_quit(self->mp_simple_poll);
		break;

	case AVAHI_SERVER_INVALID:
		AVAHI_LOGD(C_BROWN "AVAHI_SERVER_INVALID" C_NONE);
		break;

	default:
		AVAHI_LOGD(C_BROWN "avahi server callback %d" C_NONE, state);
		break;
	}
}

void CAvahi::avahi_entry_group_callback(AvahiServer *server,
	AvahiSEntryGroup *group, AvahiEntryGroupState state,
	void *userdata)
{

	switch (state) {
	case AVAHI_ENTRY_GROUP_UNCOMMITED:
		AVAHI_LOGD(C_BROWN "AVAHI_ENTRY_GROUP_UNCOMMITED" C_NONE);
		break;

	case AVAHI_ENTRY_GROUP_REGISTERING:
		AVAHI_LOGD(C_BROWN "AVAHI_ENTRY_GROUP_REGISTERING" C_NONE);
		break;

	case AVAHI_ENTRY_GROUP_ESTABLISHED:
		AVAHI_LOGD(C_BROWN "AVAHI_ENTRY_GROUP_ESTABLISHED" C_NONE);
		break;

	case AVAHI_ENTRY_GROUP_COLLISION:
		AVAHI_LOGD(C_BROWN "AVAHI_ENTRY_GROUP_COLLISION" C_NONE);
		break;

	case AVAHI_ENTRY_GROUP_FAILURE:
		AVAHI_LOGD(C_BROWN "AVAHI_ENTRY_GROUP_FAILURE" C_NONE);
		break;

	default:
		AVAHI_LOGD(C_BROWN "group callback state %d" C_NONE, state);
		break;
	}
}

void CAvahi::CreateAvahiServices(AvahiServer *server)
{
	int rval;

	if (mp_server_group == NULL) {
		mp_server_group = avahi_s_entry_group_new(server,
			avahi_entry_group_callback, (void*)this);
		if (mp_server_group == NULL) {
			AVF_LOGE("avahi_s_entry_group_new failed");
			goto Error;
		}
	}

	rval = avahi_server_add_service(server,
		mp_server_group,
		AVAHI_IF_UNSPEC,
		AVAHI_PROTO_INET,
		(AvahiPublishFlags)0,
		mp_server_name,
		mp_server_type,
		mp_domain,
		mp_host,
		m_port,
		"path=/index.html",
		NULL);
	if (rval < 0) {
		AVF_LOGE("avahi_server_add_service failed");
		goto Error;
	}

	rval = avahi_s_entry_group_commit(mp_server_group);
	if (rval < 0) {
		AVF_LOGE("avahi_s_entry_group_commit failed");
		goto Error;
	}

	return;

Error:
	avahi_simple_poll_quit(mp_simple_poll);
}

void CAvahi::DeleteAvahiServices(AvahiServer *server)
{
	if (mp_server_group) {
		avahi_s_entry_group_reset(mp_server_group);
	}
}

void CAvahi::ThreadLoop()
{
	const AvahiPoll *poll_api;
	AvahiServerConfig config;
	AvahiServer *server;
	int rval;

	AVAHI_LOGD(C_BROWN"avahi thread running" C_NONE);

	poll_api = avahi_simple_poll_get(mp_simple_poll);
	if (poll_api == NULL) {
		AVF_LOGE("avahi_simple_poll_get failed");
		return;
	}

	avahi_server_config_init(&config);
	config.use_ipv6 = 0;
	config.publish_hinfo = 0;
	config.publish_workstation = 0;
	server = avahi_server_new(poll_api, &config,
		avahi_server_callback, (void*)this, &rval);
	avahi_server_config_free(&config);

	rval = avahi_simple_poll_loop(mp_simple_poll);

	if (mp_server_group) {
		avahi_s_entry_group_free(mp_server_group);
	}

	if (server) {
		avahi_server_free(server);
	}

	AVAHI_LOGD(C_BROWN "avahi thread stopped" C_NONE);
}

