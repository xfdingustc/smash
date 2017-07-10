
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "libipc.h"
#include "ipc_vlc.h"

int ipc_libvlc_vlm_add_broadcast( ipc_libvlc_instance_t *p_instance,
				const char *psz_name,
				const char *psz_input,
				const char *psz_output, int i_options,
				const char * const *ppsz_options,
				int b_enabled, int b_loop )
{
	define_write_buffer(4096);

	ipc_write_str(psz_name);
	ipc_write_str(psz_input);
	ipc_write_str(psz_output);
	ipc_write_strlist(i_options, ppsz_options);
	ipc_write_int(b_enabled);
	ipc_write_int(b_loop);

	return ipc_call_remote_write(p_instance, VLC_CMD_VLM_ADD_BROADCAST);
}

int ipc_libvlc_vlm_set_enabled( ipc_libvlc_instance_t *p_instance,
				const char *psz_name, int b_enabled )
{
	define_write_buffer(512);

	ipc_write_str(psz_name);
	ipc_write_int(b_enabled);

	return ipc_call_remote_write(p_instance, VLC_CMD_VLM_SET_ENABLED);
}

int ipc_libvlc_vlm_play_media( ipc_libvlc_instance_t *p_instance,
				const char *psz_name )
{
	define_write_buffer(512);

	ipc_write_str(psz_name);

	return ipc_call_remote_write(p_instance, VLC_CMD_VLM_PLAY_MEDIA);
}

int ipc_libvlc_vlm_del_media( ipc_libvlc_instance_t *p_instance, 
				const char *psz_name )
{
	define_write_buffer(512);

	ipc_write_str(psz_name);

	return ipc_call_remote_write(p_instance, VLC_CMD_VLM_DEL_MEDIA);
}

int ipc_libvlc_vlm_stop_media( ipc_libvlc_instance_t *p_instance,
				const char *psz_name )
{
	define_write_buffer(512);

	ipc_write_str(psz_name);

	return ipc_call_remote_write(p_instance, VLC_CMD_VLM_STOP_MEDIA);
}

int ipc_libvlc_vlm_set_event_callback( ipc_libvlc_instance_t *p_instance,
			ipc_libvlc_event_callback_proc proc, void *pdata )
{
	pthread_mutex_lock(&p_instance->lock);
	p_instance->event_callback = proc;
	p_instance->event_callback_data = pdata;
	pthread_mutex_unlock(&p_instance->lock);
	return 0;
}

// notification callback
void ipc_vlc_notify_proc(ipc_client_t *self, int index)
{
	ipc_libvlc_instance_t *p_instance = (ipc_libvlc_instance_t*)self;

	pthread_mutex_lock(&p_instance->lock);

	if (p_instance->event_callback) {
		p_instance->event_callback(index, p_instance->event_callback_data);
	}

	pthread_mutex_unlock(&p_instance->lock);
}

void ipc_libvlc_vlm_release( ipc_libvlc_instance_t *p_instance )
{
	ipc_call_remote(p_instance, VLC_CMD_VLM_RELEASE);
}

static int ipc_vlc_ctor(ipc_client_t *self, void *args)
{
	ipc_libvlc_instance_t *p_instance = (ipc_libvlc_instance_t*)self;
	pthread_mutex_init(&p_instance->lock, NULL);
	p_instance->event_callback = NULL;
	p_instance->event_callback_data = NULL;
	return 0;
}

static void ipc_vlc_dtor(ipc_client_t *self)
{
	ipc_libvlc_instance_t *p_instance = (ipc_libvlc_instance_t*)self;
	pthread_mutex_destroy(&p_instance->lock);
}

ipc_libvlc_instance_t *ipc_libvlc_new( int argc, const char *const *argv )
{
	ipc_libvlc_instance_t *p_instance;

	define_write_buffer(4096);

	ipc_write_strlist(argc, argv);

	p_instance = ipc_create_client(ipc_libvlc_instance_t, VLC_SERVICE_NAME, VLC_CMD_NEW, 
		ipc_vlc_ctor, NULL, ipc_vlc_dtor, ipc_vlc_notify_proc);

	return p_instance;
}

void ipc_libvlc_release( ipc_libvlc_instance_t *p_instance )
{
	ipc_destroy_client(p_instance);
}

