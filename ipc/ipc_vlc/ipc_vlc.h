
#ifndef __IPC_VLC_H__
#define __IPC_VLC_H__

// service name
#define VLC_SERVICE_NAME	"vlc service"

// vlc service cmds
enum {
	VLC_CMD_NEW = 0,
	VLC_CMD_NUM
};

// vlc instance cmds
enum {
	VLC_CMD_VLM_DEL_MEDIA,
	VLC_CMD_VLM_STOP_MEDIA,
	VLC_CMD_VLM_PLAY_MEDIA,
	VLC_CMD_VLM_RELEASE,
	VLC_CMD_VLM_SET_ENABLED,
	VLC_CMD_VLM_ADD_BROADCAST,
	VLC_CMD_VLM_NUM
};

// vlc instance events
enum {
	VLC_VLM_EventMediaAdded = 0,
	VLC_VLM_EventMediaRemoved,
	VLC_VLM_EventMediaChanged,
	VLC_VLM_EventMediaInstanceStarted,

	VLC_VLM_EventMediaInstanceStopped,
	VLC_VLM_EventMediaInstanceStatusInit,
	VLC_VLM_EventMediaInstanceStatusOpening,
	VLC_VLM_EventMediaInstanceStatusPlaying,

	VLC_VLM_EventMediaInstanceStatusPause,
	VLC_VLM_EventMediaInstanceStatusEnd,
	VLC_VLM_EventMediaInstanceStatusError,
};

typedef void (*ipc_libvlc_event_callback_proc)( int type, void *pdata );

typedef struct ipc_libvlc_instance_s {
	ipc_client_t super;
	pthread_mutex_t lock;
	ipc_libvlc_event_callback_proc event_callback;
	void *event_callback_data;
} ipc_libvlc_instance_t;

int ipc_libvlc_vlm_add_broadcast( ipc_libvlc_instance_t *p_instance,
                              const char *psz_name,
                              const char *psz_input,
                              const char *psz_output, int i_options,
                              const char * const *ppsz_options,
                              int b_enabled, int b_loop );

int ipc_libvlc_vlm_set_enabled( ipc_libvlc_instance_t *p_instance,
                            const char *psz_name, int b_enabled );

int ipc_libvlc_vlm_play_media( ipc_libvlc_instance_t *p_instance,
                           const char *psz_name );

int ipc_libvlc_vlm_del_media( ipc_libvlc_instance_t *p_instance, const char *psz_name );

int ipc_libvlc_vlm_stop_media( ipc_libvlc_instance_t *p_instance,
                           const char *psz_name );

int ipc_libvlc_vlm_set_event_callback( ipc_libvlc_instance_t *p_instance,
			ipc_libvlc_event_callback_proc proc, void *pdata );

void ipc_libvlc_vlm_release( ipc_libvlc_instance_t *p_instance );

ipc_libvlc_instance_t *ipc_libvlc_new( int argc, const char *const *argv );

void ipc_libvlc_release( ipc_libvlc_instance_t *p_instance );

#endif

