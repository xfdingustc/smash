
#include <stdlib.h>
#include <stdio.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_vlm.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_events.h>

#include "libipc.h"
#include "ipc_vlc.h"

// vlc server object
typedef struct vlc_server_s {
	ipc_server_t super;
	libvlc_instance_t *p_vlc_instance;
} vlc_server_t;

typedef struct vlc_ctor_arg_s {
	int argc;
	char **argv;
} vlc_ctor_arg_t;

// note - should keep same with VLC_VLM_EventMediaAdded, etc
static const libvlc_event_type_t g_vlc_events[] = 
{
	libvlc_VlmMediaAdded,
	libvlc_VlmMediaRemoved,
	libvlc_VlmMediaChanged,
	libvlc_VlmMediaInstanceStarted,

	libvlc_VlmMediaInstanceStopped,
	libvlc_VlmMediaInstanceStatusInit,
	libvlc_VlmMediaInstanceStatusOpening,
	libvlc_VlmMediaInstanceStatusPlaying,

	libvlc_VlmMediaInstanceStatusPause,
	libvlc_VlmMediaInstanceStatusEnd,
	libvlc_VlmMediaInstanceStatusError,
};

#define NUM_VLC_EVENTS \
	(sizeof(g_vlc_events)/sizeof(g_vlc_events[0]))

static void vlc_event_callback(const struct libvlc_event_t *p_event, void *pdata)
{
	vlc_server_t *vlc_server = (vlc_server_t *)pdata;
	int index;

	for (index = 0; index < NUM_VLC_EVENTS; index++) {
		if (p_event->type == g_vlc_events[index])
			break;
	}

	if (index < NUM_VLC_EVENTS) {
		// send the notification to client
		ipc_write_notification(vlc_server->super.cid, 1 << index);
	}
}

// constructor
static int vlc_server_ctor(ipc_server_t *_server, void *_args)
{
	vlc_server_t *vlc_server = (vlc_server_t *)_server;
	vlc_ctor_arg_t *args = (vlc_ctor_arg_t *)_args;
	libvlc_event_manager_t *event_manager;
	int i;

	if ((vlc_server->p_vlc_instance = libvlc_new(args->argc, args->argv)) == NULL) {
		printf("libvlc_new failed\n");
		return -1;
	}

	event_manager = libvlc_vlm_get_event_manager(vlc_server->p_vlc_instance);
	if (event_manager != NULL) {
		for (i = 0; i < NUM_VLC_EVENTS; i++)
			libvlc_event_attach(event_manager, g_vlc_events[i], vlc_event_callback, (void*)vlc_server);
	}

	return 0;
}

// destructor
static void vlc_server_dtor(ipc_server_t *_server)
{
	vlc_server_t *vlc_server = (vlc_server_t *)_server;
	libvlc_event_manager_t *event_manager;
	int i;

	if (vlc_server->p_vlc_instance == NULL)
		return;

	event_manager = libvlc_vlm_get_event_manager(vlc_server->p_vlc_instance);
	if (event_manager != NULL) {
		for (i = 0; i < NUM_VLC_EVENTS; i++)
			libvlc_event_detach(event_manager, g_vlc_events[i], vlc_event_callback, (void*)vlc_server);
	}

	libvlc_release(vlc_server->p_vlc_instance);
}

// execute remote call
static int vlc_server_proc(ipc_server_t *_server, ipc_write_read_t *iwr)
{
	vlc_server_t *server = (vlc_server_t*)_server;
	ipc_init_read_iwr(iwr);

	switch (iwr->read_val) {
	case VLC_CMD_VLM_DEL_MEDIA: {
			char *psz_name;

			ipc_read_str(psz_name);
			if (ipc_read_failed())
				return -1;

			iwr->write_val = libvlc_vlm_del_media(server->p_vlc_instance,
				psz_name);
		}
		break;

	case VLC_CMD_VLM_STOP_MEDIA: {
			char *psz_name;

			ipc_read_str(psz_name);
			if (ipc_read_failed())
				return -1;

			iwr->write_val = libvlc_vlm_stop_media(server->p_vlc_instance,
				psz_name);
		}
		break;

	case VLC_CMD_VLM_PLAY_MEDIA: {
			char *psz_name;

			ipc_read_str(psz_name);
			if (ipc_read_failed())
				return -1;

			iwr->write_val = libvlc_vlm_play_media(server->p_vlc_instance,
				psz_name);
		}
		break;

	case VLC_CMD_VLM_RELEASE:
		// terminate the connection and destroy the server object
		return -2;

	case VLC_CMD_VLM_SET_ENABLED: {
			char *psz_name;
			int enabled;

			ipc_read_str(psz_name);
			ipc_read_int(enabled);
			if (ipc_read_failed())
				return -1;

			iwr->write_val = libvlc_vlm_set_enabled(server->p_vlc_instance,
				psz_name, enabled);
		}
		break;

	case VLC_CMD_VLM_ADD_BROADCAST: {
			char *psz_name;
			char *psz_input;
			char *psz_output;
			int i_options;
			char **ppsz_options;
			int b_enabled;
			int b_loop;

			ipc_read_str(psz_name);
			ipc_read_str(psz_input);
			ipc_read_str(psz_output);
			ipc_read_strlist(i_options, ppsz_options);
			ipc_read_int(b_enabled);
			ipc_read_int(b_loop);
			if (ipc_read_failed())
				return -1;

#if 0
			printf("call libvlc_vlm_add_broadcast\n"
				"psz_name=%s, psz_input=%s, psz_output=%s"
				"i_options=%d, ppsz_options=%p, b_enabled=%d, b_loop=%d\n",
				psz_name, psz_input, psz_output, 
				i_options, ppsz_options, b_enabled, b_loop);
#endif

			iwr->write_val = libvlc_vlm_add_broadcast(server->p_vlc_instance,
				psz_name, psz_input, psz_output, 
				i_options, ppsz_options, b_enabled, b_loop);
		}
		break;

	default:
		return -1;
	}

	return 0;
}

static int create_vlc_server(ipc_write_read_t *iwr)
{
	ipc_init_read_iwr(iwr);
	vlc_ctor_arg_t args = {0, NULL};
	vlc_server_t *vlc_server;

	// read parameters - (argc, argv)
	ipc_read_strlist(args.argc, args.argv);
	if (ipc_read_failed()) {
		printf("bad parameter\n");
		return -1;
	}

	// create the server object
	vlc_server = ipc_create_server(vlc_server_t, 4096, 1024,
		vlc_server_ctor, &args, vlc_server_dtor, vlc_server_proc);
	if (vlc_server == NULL)
		return -1;

	if (ipc_run_server(&vlc_server->super, iwr) < 0)
		return -1;

	return 0;
}

static int run_vlc_service(void)
{
	ipc_cid_t service_cid;
	ipc_write_read_t iwr;
	char read_buffer[2048];

	if (ipc_register_service(VLC_SERVICE_NAME, &service_cid) < 0)
		return -1;

	ipc_init_service_iwr(&iwr, service_cid, NULL, read_buffer, sizeof(read_buffer));

	while (1) {
		if (ipc_write_read(&iwr) < 0)
			return -1;

		switch (iwr.read_val) {
		case VLC_CMD_NEW:
			if (create_vlc_server(&iwr) < 0) {
				iwr.flags = IPC_FLAG_DENY_CMD | IPC_FLAG_READ;
			} else {
				iwr.flags = IPC_FLAG_READ;
			}
			break;

		default:
			iwr.flags = IPC_FLAG_DENY_CMD | IPC_FLAG_READ;
			break;
		}
	}

	return 0;
}

int main(int arc, char **argv)
{
	if (ipc_init() < 0)
		return -1;

	// if there're more than one services, should spawn threads to run them
	if (run_vlc_service() < 0)
		return -1;

	return 0;
}

