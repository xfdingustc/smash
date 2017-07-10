
#ifndef __AVF_MEDIA_H__
#define __AVF_MEDIA_H__

//-----------------------------------------------------------------------
//
//	avf media control
//
//-----------------------------------------------------------------------

// service name
#define AVF_MEDIA_SERVICE_NAME	"avf media service"

// avf cmds
enum {
	AVF_MEDIA_CmdOpen = 0,
	AVF_MEDIA_CmdClose,
	AVF_MEDIA_CmdPrepare,
	AVF_MEDIA_CmdRun,
	AVF_MEDIA_CmdStop,

	AVF_MEDIA_CmdGetState,
	AVF_MEDIA_CmdSetMediaSource,

	AVF_MEDIA_SetConfigInt32,
	AVF_MEDIA_SetConfigInt64,
	AVF_MEDIA_SetConfigBool,
	AVF_MEDIA_SetConfigString,

	AVF_MEDIA_GetConfigInt32,
	AVF_MEDIA_GetConfigInt64,
	AVF_MEDIA_GetConfigBool,
	AVF_MEDIA_GetConfigString,

	AVF_MEDIA_GetNumClips,
	AVF_MEDIA_GetClipsInfo,
	AVF_MEDIA_DeleteClip,

	AVF_MEDIA_CmdStorageReady,	// sd card is plugged/unplugged
	AVF_MEDIA_CmdGetVdbInfo,		// 
	AVF_MEDIA_CmdFreeVdbSpace,	// only for auto delete

	AVF_MEDIA_CmdInitClipReader,
	AVF_MEDIA_CmdReleaseClipReader,
	AVF_MEDIA_CmdReadClipData,

	AVF_MEDIA_CmdEnablePictureService,
	AVF_MEDIA_CmdGetSpaceInfo,

	AVF_MEDIA_ReadMem,
	AVF_MEDIA_WriteMem,

	AVF_DISPLAY_SET_MODE = 100,
	AVF_DISPLAY_SET_VOUT,
	AVF_DISPLAY_DISABLE_VOUT,
	AVF_DISPLAY_SET_BACKLIGHT,
	AVF_DISPLAY_SET_BRIGHTNESS,
	AVF_DISPLAY_GET_BRIGHTNESS,
	AVF_DISPLAY_GET_VOUT_INFO,
	AVF_DISPLAY_FLIP_VOUT_VIDEO,
	AVF_DISPLAY_FLIP_VOUT_OSD,

	AVF_MEDIA_CmdCamera = 200,
	// camera commands

	AVF_MEDIA_CmdPlayback = 300,
	// playback commands

	AVF_GET_VERSION = 400,
};

// avf states
enum {
	AVF_MEDIA_STATE_IDLE,
	AVF_MEDIA_STATE_STOPPED,
	AVF_MEDIA_STATE_RUNNING,
	AVF_MEDIA_STATE_ERROR,
	AVF_MEDIA_STATE_PREPARED,
};

const char *avf_get_state_name(int state);

typedef void (*avf_media_event_callback_proc)(int type, void *pdata);
typedef void (*avf_media_on_disconnected_proc)(int error, void *pdata);

enum {
	AVF_MEDIA_NONE = 0,
	AVF_MEDIA_CAMERA,
	AVF_MEDIA_PLAYBACK,
};

typedef struct avf_media_s {
	ipc_client_t super;
	pthread_mutex_t lock;
	avf_media_event_callback_proc event_callback;
	avf_media_on_disconnected_proc on_disconnected;
	void *pdata;
	int media_type;	// CAMERA, PLAYBACK
} avf_media_t;

typedef struct avf_clip_info_s {
	uint32_t clip_id;
	uint32_t clip_date;
	uint32_t length_ms;
	uint64_t clip_time_ms;
} avf_clip_info_t;

#define __NUM_CLIP_INFO	16

// TODO: should be same with vdb_cmd.h
enum {
	CLIP_TYPE_UNSPECIFIED = -1,
	CLIP_TYPE_BUFFER = 0,
	CLIP_TYPE_MARKED = 1,
};

typedef struct avf_vdb_info_s {
	// disk_space_bytes + file_space_bytes = available
	uint32_t total_space_mb;
	uint32_t disk_space_mb;
	uint32_t file_space_mb;
	uint8_t vdb_ready;		// if vdb initialization is done
	uint8_t enough_space;	// if there's enough space for start recording
	uint8_t enough_space_autodel;
} avf_vdb_info_t;

typedef struct avf_space_info_s {
	uint64_t total_space;	// total space in bytes
	uint64_t used_space;		// used space in bytes (all clips and other files)
	uint64_t marked_space;	// marked clips in bytes
	uint64_t clip_space;		// 0 if not set
} avf_space_info_t;

avf_media_t *avf_media_create_ex(avf_media_event_callback_proc event_callback,
	avf_media_on_disconnected_proc on_disconnected, void *pdata);
static inline avf_media_t *avf_media_create(avf_media_event_callback_proc event_callback, void *pdata)
{
	return avf_media_create_ex(event_callback, NULL, pdata);
}
void avf_media_destroy(avf_media_t *self);

int avf_media_get_state(avf_media_t *self);
int avf_media_set_media_source(avf_media_t *self, 
	const char *player, const char *filename, const char *config, int tag);

int avf_media_open(avf_media_t *self, int wait);
int avf_media_close(avf_media_t *self, int wait);
int avf_media_run(avf_media_t *self, int wait);
int avf_media_prepare(avf_media_t *self, int wait);
int avf_media_stop(avf_media_t *self, int wait);

//--------------------------------------------------------------------------------------------

// if id is set with AVF_GLOBAL_CONFIG, the config value is write to (read from) the global registry
#define AVF_GLOBAL_CONFIG	0x80000000

int avf_media_set_config_int32(avf_media_t *self, const char *key, int id, int32_t value);
int avf_media_set_config_int64(avf_media_t *self, const char *key, int id, int64_t value);
int avf_media_set_config_bool(avf_media_t *self, const char *key, int id, int value);
int avf_media_set_config_string(avf_media_t *self, const char *key, int id, const char *value);
int avf_media_set_config_mem(avf_media_t *self, const char *key, int id, const uint8_t *mem, unsigned size);

int avf_media_get_int32(avf_media_t *self, const char *key, int id, int32_t *value, int32_t default_value);
int avf_media_get_int64(avf_media_t *self, const char *key, int id, int64_t *value, int64_t default_value);
int avf_media_get_bool(avf_media_t *self, const char *key, int id, int *value, int default_value);
int avf_media_get_string(avf_media_t *self, const char *key, int id, char *buffer, int buffer_size, const char *default_value);
int avf_media_get_mem(avf_media_t *self, const char *key, int id, uint8_t *mem, unsigned size);

//--------------------------------------------------------------------------------------------

int avf_media_state_changed(avf_media_t *self);
int avf_media_get_num_clips(avf_media_t *self, int clip_type, uint32_t *num_clips, uint32_t *sid);
int avf_media_get_clips_info(avf_media_t *self, int clip_type, avf_clip_info_t *info, 
	uint32_t start, uint32_t *num, uint32_t *sid);	// num: input & output
int avf_media_delete_clip(avf_media_t *self, int clip_type, uint32_t clip_id);

int avf_media_storage_ready(avf_media_t *self, int is_ready);
int __avf_media_get_vdb_info(avf_media_t *self, avf_vdb_info_t *info, int cmd);

int avf_media_get_vdb_info(avf_media_t *self, avf_vdb_info_t *info);
int avf_media_free_vdb_space(avf_media_t *self, avf_vdb_info_t *info);

int avf_media_init_clip_reader(avf_media_t *self, int clip_type, uint32_t clip_id, 
	uint64_t start_time_ms, uint32_t length_ms);

int avf_media_release_clip_reader(avf_media_t *self);

// filenames: set to NULL if don't want to read
// raw_flags: RAW_DATA_GPS_FLAG | RAW_DATA_OBD_FLAG | RAW_DATA_ACC_FLAG
// return 1 when end
// return < 0: error
// return 0: ok
int avf_media_read_clip_data(avf_media_t *self, 
	const char *video_filename,
	const char *video_filename1,
	const char *picture_filename,
	int raw_flags, const char *raw_filename);

int avf_media_enable_picture_service(avf_media_t *self, const char *picture_path);

int avf_media_get_space_info(avf_media_t *self, avf_space_info_t *space_info);

int avf_get_version(avf_media_t *self, uint32_t *version);

//-----------------------------------------------------------------------
//
//	avf camera
//
//-----------------------------------------------------------------------

// camera cmds
enum {
	AVF_CAMERA_CmdImageControl = AVF_MEDIA_CmdCamera,
	AVF_CAMERA_CmdEnablePreview,
	AVF_CAMERA_CmdDisablePreview,
	AVF_CAMERA_CmdGetCurrentFile,
	AVF_CAMERA_CmdGetPreviousFile,
	AVF_CAMERA_CmdSaveCurrent,
	AVF_CAMERA_CmdSaveNextPicture,
	///
	AVF_CAMERA_CmdMarkLiveClip,
	AVF_CAMERA_CmdCancelMarkLiveClip,
	AVF_CAMERA_CmdGetMarkLiveClipInfo,
	///
	AVF_CAMERA_CmdStillCapture,
	AVF_CAMERA_CmdGetStillCaptureInfo,
	AVF_CAMERA_CmdGetNumPictures,
	///
	AVF_CAMERA_CmdChangeVoutVideo,
	AVF_CAMERA_CmdSetRecordAttr,
	AVF_CAMERA_CmdMarkLiveClipV2,
	///
	AVF_CAMERA_CmdSetClipSceneData,
};

// camera events
enum {
	AVF_CAMERA_EventStatusChanged = 0,
	AVF_CAMERA_EventFileStart = 1,
	AVF_CAMERA_EventFileEnd = 2,
	AVF_CAMERA_EventDoneSaving = 5,
	AVF_CAMERA_EventDoneSaveNextPicture = 6,
	AVF_CAMERA_EventBufferSpaceLow = 7,
	AVF_CAMERA_EventBufferSpaceFull = 8,		// app should call StopRecord
	AVF_CAMERA_EventVdbStateChanged = 9,		// 

	AVF_CAMERA_EventUploadVideo = 10,
	AVF_CAMERA_EventUploadPicture = 11,
	AVF_CAMERA_EventUploadRaw = 12,
	AVF_CAMERA_EventDiskError = 13, 			// disk error

	// still capture: state changed
	// should call avf_camera_get_still_capture_info() to get current state and error code
	AVF_CAMERA_EventStillCaptureStateChanged = 14,

	// a picture is taken, for single and burst
	// should call avf_camera_get_still_capture_info() to get total pictures taken
	AVF_CAMERA_EventPictureTaken = 15,			// still capture: picture taken

	// picture list is changed, due to capturing or deleting
	// should call avf_camera_get_num_pictures() to get total number of pictures
	AVF_CAMERA_EventPictureListChanged = 16,	// picture list changed

	AVF_CAMERA_EventDiskSlow = 17,

	AVF_CAMERA_EventMmsMode1 = 18,
	AVF_CAMERA_EventMmsMode2 = 19,
};

static inline int avf_camera_set_media_source(avf_media_t *self, const char *filename, const char *config) {
	return avf_media_set_media_source(self, "camera", filename, config, AVF_MEDIA_CAMERA);
}

static inline int avf_camera_open(avf_media_t *self, int wait) {
	return avf_media_open(self, wait);
}

static inline int avf_camera_close(avf_media_t *self, int wait) {
	return avf_media_close(self, wait);
}

static inline int avf_camera_prepare(avf_media_t *self, int wait) {
	return avf_media_prepare(self, wait);
}

static inline int avf_camera_run(avf_media_t *self, int wait) {
	return avf_media_run(self, wait);
}

static inline int avf_camera_stop(avf_media_t *self, int wait) {
	return avf_media_stop(self, wait);
}

int avf_camera_enable_preview(avf_media_t *self);
int avf_camera_disable_preview(avf_media_t *self);

int avf_camera_image_control(avf_media_t *self, const char *name, const char *value);

int avf_camera_get_current_file(avf_media_t *self, char *buffer, int buffer_size);
int avf_camera_get_previous_file(avf_media_t *self, char *buffer, int buffer_size);

int avf_camera_save_current(avf_media_t *self, int before, int after, const char *filename);
int avf_camera_save_next_picture(avf_media_t *self, const char *filename);

// mark the live clip
// delay_ms: not used. set to 0
// before_live_ms: range before the live point. use -1 for all before the live point
// after_live_ms: range after the live point. use -1 for all after the live point
// b_continue: if it is marking now, this request will be 'appended' to the current one;
//	in this case, 'before_live_ms' is ignored

// obsolete, use avf_camera_mark_live_clip_v2()
int avf_camera_mark_live_clip_ex(avf_media_t *self, 
	int delay_ms, int before_live_ms, int after_live_ms, int b_continue);

// obsolete
int avf_camera_mark_live_clip(avf_media_t *self, 
	int delay_ms, int before_live_ms, int after_live_ms, int b_continue, int live_mark_flag);

int avf_camera_mark_live_clip_v2(avf_media_t *self,
	int delay_ms, int before_live_ms, int after_live_ms, int b_continue, const char *attr_str);

// stop mark
int avf_camera_cancel_mark_live_clip(avf_media_t *self);

typedef struct avf_mark_live_info_s {
	int b_running;
	int b_valid;
	int delay_ms;		// not used, set to 0
	int before_live_ms;
	int after_live_ms;
	uint32_t clip_id;	// CLIP_TYPE_MARKED
} avf_mark_live_info_t;

int avf_camera_get_mark_live_clip_info(avf_media_t *self, avf_mark_live_info_t *info);

// 
int avf_camera_set_clip_scene_data(avf_media_t *self, int clip_type, uint32_t clip_id,
		const uint8_t *data, uint32_t data_size);

enum {
	STILLCAP_ACTION_NONE = 0,
	STILLCAP_ACTION_SINGLE = 1,	// capture single
	STILLCAP_ACTION_BURST = 2,	// start burst capture
	STILLCAP_ACTION_STOP = 3,		// stop burst capture
	STILLCAP_ACTION_SET_RAW = 4,	// set capturing raw
};

int avf_camera_still_capture(avf_media_t *self, int action, int flags);

enum {
	STILLCAP_ERR_OK = 0,
	STILLCAP_ERR_NO_SD,		// no sd card
	STILLCAP_ERR_NO_SPACE,	// no space on sd card
	STILLCAP_ERR_CAPTURE,		// capture error
	STILLCAP_ERR_WRITE,		// write error
	STILLCAP_ERR_SOFT_JPG,	// soft jpeg error
	STILLCAP_ERR_CREATE_FILE,	// cannot create filename
};

enum {
	STILLCAP_STATE_IDLE = 0,	// idle
	STILLCAP_STATE_SINGLE,	// single capturing
	STILLCAP_STATE_BURST,		// burst capturing
	STILLCAP_STATE_STOPPING,	// stopping burst capturing
};

typedef struct still_capture_info_s {
	int b_running;		// if it is in still capture mode
	int stillcap_state;	// 
	int b_raw;			// if using raw
	int num_pictures;	// number of pictures taken
	int burst_ticks;
	int last_error;		// 
} still_capture_info_t;

int avf_camera_get_still_capture_info(avf_media_t *self, still_capture_info_t *info);

// get total number of pictures on tf card
// return -1 on error
int avf_camera_get_num_pictures(avf_media_t *self);

int avf_camera_change_vout_video(avf_media_t *self, const char *config);

int avf_camera_set_record_attr(avf_media_t *self, const char *attr_str);

//-----------------------------------------------------------------------
//
//	avf playback
//
//-----------------------------------------------------------------------

// playback cmds
enum {
	AVF_PLAYBACK_CmdGetMediaInfo = AVF_MEDIA_CmdPlayback,
	AVF_PLAYBACK_CmdGetPlayPos,
	AVF_PLAYBACK_CmdPauseResume,
	AVF_PLAYBACK_CmdSeek,
	AVF_PLAYBACK_CmdFastPlay,
};

// playback events
enum {
	AVF_PLAYBACK_EventStatusChanged = 0,
};

static inline int avf_playback_set_media_source(avf_media_t *self, const char *filename, const char *config) {
	return avf_media_set_media_source(self, "playback", filename, config, AVF_MEDIA_PLAYBACK);
}

static inline int avf_playback_open(avf_media_t *self, int wait) {
	return avf_media_open(self, wait);
}

static inline int avf_playback_close(avf_media_t *self, int wait) {
	return avf_media_close(self, wait);
}

static inline int avf_playback_run(avf_media_t *self, int wait) {
	return avf_media_run(self, wait);
}

static inline int avf_playback_stop(avf_media_t *self, int wait) {
	return avf_media_stop(self, wait);
}

typedef struct avf_playback_media_info_s {
	int total_length_ms;
	int audio_num;
	int video_num;
	int paused;
} avf_playback_media_info_t;

int avf_playback_get_media_info(avf_media_t *self, avf_playback_media_info_t *media_info);
int avf_playback_get_play_pos(avf_media_t *self, int *ms);
int avf_playback_pause_resume(avf_media_t *self);
int avf_playback_seek(avf_media_t *self, int ms);
int avf_playback_fastplay(avf_media_t *self, int mode, int speed);

//-----------------------------------------------------------------------
//
//	avf config
//
//-----------------------------------------------------------------------

#include "avf_plat_config.h"

int avf_display_set_mode(avf_media_t *self, int mode);
int avf_display_set_vout(avf_media_t *self, vout_config_t *config, const char *lcd_model);
int avf_display_disable_vout(avf_media_t *self, int vout_id);
int avf_display_set_backlight(avf_media_t *self, int vout_id, const char *lcd_model, int on);
int avf_display_set_brightness(avf_media_t *self, int vout_id, int vout_type, int brightness);
int avf_display_get_brightness(avf_media_t *self, int vout_id, int vout_type, int *brightness);
int avf_display_flip_vout_video(avf_media_t *self, int vout_id, int vout_type, int flip);
int avf_display_flip_vout_osd(avf_media_t *self, int vout_id, int vout_type, int flip);

typedef struct avf_vout_info_s {
	int	enabled;
	int vout_width;
	int vout_height;
	int video_width;
	int video_height;
	int video_xoff;
	int video_yoff;
} avf_vout_info_t;

int avf_display_get_vout_info(avf_media_t *self, int vout_id, int vout_type, avf_vout_info_t *info);

#endif

