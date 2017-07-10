
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "libipc.h"
#include "avf_media.h"

//-----------------------------------------------------------------------
//
//	avf media control
//
//-----------------------------------------------------------------------

const char *avf_get_state_name(int state)
{
	switch (state) {
	case AVF_MEDIA_STATE_IDLE: return "Idle";
	case AVF_MEDIA_STATE_STOPPED: return "Stopped";
	case AVF_MEDIA_STATE_RUNNING: return "Running";
	case AVF_MEDIA_STATE_ERROR: return "Error";
	default: return "unknown";
	}
}

static void avf_media_notify_proc(ipc_client_t *client, int index)
{
	avf_media_t *self = (avf_media_t*)client;
	if (self->event_callback) {
		self->event_callback(index, self->pdata);
	}
}

static void avf_media_on_disconnected(ipc_client_t *client, int error)
{
	avf_media_t *self = (avf_media_t*)client;
	if (self->on_disconnected) {
		self->on_disconnected(error, self->pdata);
	}
}

static int avf_media_query_service(ipc_cid_t *cid)
{
	// try for 6 second until the media server is connected
	for (int i = 0; i < 300; i++) {
		*cid = 0;
		if (ipc_query_service(AVF_MEDIA_SERVICE_NAME, cid) == 0 && *cid != 0)
			return 0;
		usleep(20 * 1000);
	}
	return -1;
}

typedef struct avf_media_client_ctor_s {
	avf_media_event_callback_proc event_callback;
	avf_media_on_disconnected_proc on_disconnected;
	void *pdata;
} avf_media_client_ctor_t;

static int __avf_media_client_ctor(ipc_client_t *__self, void *args)
{
	avf_media_t *self = (avf_media_t*)__self;
	avf_media_client_ctor_t *ctor_args = (avf_media_client_ctor_t*)args;

	self->event_callback = ctor_args->event_callback;
	self->on_disconnected = ctor_args->on_disconnected;
	self->pdata = ctor_args->pdata;

	return 0;
}

avf_media_t *avf_media_create_ex(avf_media_event_callback_proc event_callback,
	avf_media_on_disconnected_proc on_disconnected, void *pdata)
{
	avf_media_t *self;
	ipc_cid_t cid;
	avf_media_client_ctor_t ctor_args;

	if (avf_media_query_service(&cid) < 0)
		return NULL;

	ctor_args.event_callback = event_callback;
	ctor_args.on_disconnected = on_disconnected;
	ctor_args.pdata = pdata;
	self = (avf_media_t*)__ipc_new_client_ex(cid, sizeof(avf_media_t),
		__avf_media_client_ctor, (void*)&ctor_args, NULL,
		avf_media_notify_proc, avf_media_on_disconnected);

	return self;
}

void avf_media_destroy(avf_media_t *self)
{
	ipc_destroy_client(self);
}

int avf_media_get_state(avf_media_t *self)
{
	int rval = ipc_call_remote(self, AVF_MEDIA_CmdGetState);
	return (rval < 0) ? AVF_MEDIA_STATE_ERROR : rval;
}

int avf_media_set_media_source(avf_media_t *self, 
	const char *player, const char *filename, const char *config, int tag)
{
	define_write_buffer(1024);
	ipc_write_str(player);
	ipc_write_str(filename ? filename : "");
	ipc_write_str(config ? config : "");
	ipc_write_int32(tag);
	return ipc_call_remote_write(self, AVF_MEDIA_CmdSetMediaSource);
}

int avf_media_open(avf_media_t *self, int wait)
{
	define_write_buffer(16);
	ipc_write_int(wait);
	return ipc_call_remote_write(self, AVF_MEDIA_CmdOpen);
}

int avf_media_close(avf_media_t *self, int wait)
{
	define_write_buffer(16);
	ipc_write_int(wait);
	return ipc_call_remote_write(self, AVF_MEDIA_CmdClose);
}

int avf_media_run(avf_media_t *self, int wait)
{
	define_write_buffer(16);
	ipc_write_int(wait);
	return ipc_call_remote_write(self, AVF_MEDIA_CmdRun);
}

int avf_media_prepare(avf_media_t *self, int wait)
{
	define_write_buffer(16);
	ipc_write_int(wait);
	return ipc_call_remote_write(self, AVF_MEDIA_CmdPrepare);
}

int avf_media_stop(avf_media_t *self, int wait)
{
	define_write_buffer(16);
	ipc_write_int(wait);
	return ipc_call_remote_write(self, AVF_MEDIA_CmdStop);
}

int avf_media_set_config_int32(avf_media_t *self, const char *key, int id, int32_t value)
{
	define_write_buffer(256);
	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_int32(value);
	return ipc_call_remote_write(self, AVF_MEDIA_SetConfigInt32);
}

int avf_media_set_config_int64(avf_media_t *self, const char *key, int id, int64_t value)
{
	define_write_buffer(256);
	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_int64(value);
	return ipc_call_remote_write(self, AVF_MEDIA_SetConfigInt64);
}

int avf_media_set_config_bool(avf_media_t *self, const char *key, int id, int value)
{
	define_write_buffer(256);
	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_int(value);
	return ipc_call_remote_write(self, AVF_MEDIA_SetConfigBool);
}

int avf_media_set_config_string(avf_media_t *self, const char *key, int id, const char *value)
{
	define_write_buffer(512);
	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_str(value);
	return ipc_call_remote_write(self, AVF_MEDIA_SetConfigString);
}

int avf_media_set_config_mem(avf_media_t *self, const char *key, int id, const uint8_t *mem, unsigned size)
{
	define_write_buffer(512);

	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_int(size);

	if (__ipc_wsize < size) {
		return -1;
	}

	ipc_write_data(mem, size);

	return ipc_call_remote_write(self, AVF_MEDIA_WriteMem);
}

int avf_media_get_int32(avf_media_t *self, const char *key, int id, int32_t *value, int32_t default_value)
{
	int rval;
	define_write_buffer(512);
	define_read_buffer(16);

	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_int32(default_value);

	rval = ipc_call_remote_write_read(self, AVF_MEDIA_GetConfigInt32);

	if (rval < 0)
		goto Default;

	ipc_read_int32(*value);
	if (ipc_read_failed())
		goto Default;

	return rval;

Default:
	*value = default_value;
	return rval;
}

int avf_media_get_int64(avf_media_t *self, const char *key, int id, int64_t *value, int64_t default_value)
{
	int rval;
	define_write_buffer(512);
	define_read_buffer(16);

	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_int64(default_value);

	rval = ipc_call_remote_write_read(self, AVF_MEDIA_GetConfigInt64);

	if (rval < 0)
		goto Default;

	ipc_read_int64(*value);
	if (ipc_read_failed())
		goto Default;

	return rval;

Default:
	*value = default_value;
	return rval;
}

int avf_media_get_bool(avf_media_t *self, const char *key, int id, int *value, int default_value)
{
	int rval;
	define_write_buffer(512);
	define_read_buffer(16);

	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_int(default_value);

	rval = ipc_call_remote_write_read(self, AVF_MEDIA_GetConfigBool);

	if (rval < 0)
		goto Default;

	ipc_read_int(*value);
	if (ipc_read_failed())
		goto Default;

	return rval;

Default:
	*value = default_value;
	return rval;
}

int avf_media_get_string(avf_media_t *self, const char *key, int id, 
	char *buffer, int buffer_size, const char *default_value)
{
	char *value;
	int rval;

	define_write_buffer(512);
	define_read_buffer(256);

	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_str(default_value);

	rval = ipc_call_remote_write_read(self, AVF_MEDIA_GetConfigString);

	if (rval < 0)
		goto Default;

	ipc_read_str(value);
	if (ipc_read_failed())
		goto Default;

	strncpy(buffer, value, buffer_size);

	return rval;

Default:
	strncpy(buffer, default_value, buffer_size);
	buffer[buffer_size-1] = 0;
	return rval;
}

int avf_media_get_mem(avf_media_t *self, const char *key, int id, uint8_t *mem, unsigned size)
{
	int rval;
	uint32_t data_size;
	char *pdata;
	uint32_t tocopy;

	define_write_buffer(512);
	define_read_buffer(256);

	if (size <= 0 || size > 256) {
		return -1;
	}

	ipc_write_str(key);
	ipc_write_int(id);
	ipc_write_int(size);

	rval = ipc_call_remote_write_read(self, AVF_MEDIA_ReadMem);

	if (rval < 0) {
		goto Fail;
	}

	ipc_read_int(data_size);
	ipc_read_data(pdata, data_size);
	if (ipc_read_failed()) {
		rval = -1;
		goto Fail;
	}

	tocopy = size;
	if (tocopy > data_size)
		tocopy = data_size;

	memcpy(mem, pdata, tocopy);
	if (size > tocopy) {
		memset(mem + tocopy, 0, size - tocopy);
	}

	return 0;

Fail:
	memset(mem, 0, size);
	return rval;
}

int avf_media_get_num_clips(avf_media_t *self, int clip_type, uint32_t *num_clips, uint32_t *sid)
{
	define_write_buffer(8);
	define_read_buffer(16);
	int rval;

	ipc_write_int32(clip_type);
	rval = ipc_call_remote_write_read(self, AVF_MEDIA_GetNumClips);
	if (rval < 0) {
		*num_clips = 0;
		return rval;
	}

	ipc_read_int32(*num_clips);
	ipc_read_int32(*sid);
	if (ipc_read_failed()) {
		*num_clips = 0;
		return -1;
	}

	return 0;
}

int __avf_media_get_clips_info(avf_media_t *self, int clip_type,
	avf_clip_info_t *info, uint32_t start, uint32_t *num, uint32_t *sid)
{
	define_write_buffer(16);
	define_read_buffer(__NUM_CLIP_INFO*sizeof(avf_clip_info_t));
	int rval;

	ipc_write_int32(clip_type);
	ipc_write_int32(start);
	ipc_write_int32(*num);

	rval = ipc_call_remote_write_read(self, AVF_MEDIA_GetClipsInfo);
	if (rval < 0)
		return rval;

	ipc_read_int32(*num);
	ipc_read_int32(*sid);
	ipc_copy_data(info, *num * sizeof(avf_clip_info_t));

	return 0;
}

// num: input & output
int avf_media_get_clips_info(avf_media_t *self, int clip_type,
	avf_clip_info_t *info, uint32_t start, uint32_t *num, uint32_t *sid)
{
	int total = 0;
	int rval;

	while (*num) {
		uint32_t toread = *num;
		if (toread > __NUM_CLIP_INFO)
			toread = __NUM_CLIP_INFO;

		rval = __avf_media_get_clips_info(self, clip_type, info, start, &toread, sid);
		if (rval < 0)
			return rval;

		if (toread == 0)
			break;

		total += toread;
		info += toread;
		start += toread;
		*num -= toread;
	}

	*num = total;
	return 0;
}

int avf_media_delete_clip(avf_media_t *self, int clip_type, uint32_t clip_id)
{
	define_write_buffer(16);
	ipc_write_int32(clip_type);
	ipc_write_int32(clip_id);
	return ipc_call_remote_write(self, AVF_MEDIA_DeleteClip);
}

int avf_media_storage_ready(avf_media_t *self, int is_ready)
{
	define_write_buffer(16);
	ipc_write_int(is_ready);
	return ipc_call_remote_write(self, AVF_MEDIA_CmdStorageReady);
}

int __avf_media_get_vdb_info(avf_media_t *self, avf_vdb_info_t *info, int cmd)
{
	define_read_buffer(128);
	memset(info, 0, sizeof(*info));
	int rval = ipc_call_remote_read(self, cmd);
	ipc_read_int32(info->total_space_mb);
	ipc_read_int32(info->disk_space_mb);
	ipc_read_int32(info->file_space_mb);
	ipc_read_int(info->vdb_ready);
	ipc_read_int(info->enough_space);
	ipc_read_int(info->enough_space_autodel);
	return rval;
}

int avf_media_get_vdb_info(avf_media_t *self, avf_vdb_info_t *info)
{
	return __avf_media_get_vdb_info(self, info, AVF_MEDIA_CmdGetVdbInfo);
}

int avf_media_free_vdb_space(avf_media_t *self, avf_vdb_info_t *info)
{
	return __avf_media_get_vdb_info(self, info, AVF_MEDIA_CmdFreeVdbSpace);
}

int avf_media_init_clip_reader(avf_media_t *self, int clip_type, uint32_t clip_id, 
	uint64_t start_time_ms, uint32_t length_ms)
{
	define_write_buffer(64);
	ipc_write_int32(clip_type);
	ipc_write_int32(clip_id);
	ipc_write_int64(start_time_ms);
	ipc_write_int32(length_ms);
	return ipc_call_remote_write(self, AVF_MEDIA_CmdInitClipReader);
}

int avf_media_release_clip_reader(avf_media_t *self)
{
	return ipc_call_remote(self, AVF_MEDIA_CmdReleaseClipReader);
}

int avf_media_read_clip_data(avf_media_t *self, 
	const char *video_filename,
	const char *video_filename1,
	const char *picture_filename,
	int raw_flags, const char *raw_filename)
{
	define_write_buffer(2*1024);
	ipc_write_str(video_filename ? video_filename : "");
	ipc_write_str(video_filename1 ? video_filename1 : "");
	ipc_write_str(picture_filename ? picture_filename : "");
	ipc_write_int(raw_flags);
	ipc_write_str(raw_filename ? raw_filename : "");
	return ipc_call_remote_write(self, AVF_MEDIA_CmdReadClipData);
}

int avf_media_enable_picture_service(avf_media_t *self, const char *picture_path)
{
	define_write_buffer(256);
	ipc_write_str(picture_path);
	return ipc_call_remote_write(self, AVF_MEDIA_CmdEnablePictureService);
}

int avf_media_get_space_info(avf_media_t *self, avf_space_info_t *space_info)
{
	define_read_buffer(64);
	int rval = ipc_call_remote_read(self, AVF_MEDIA_CmdGetSpaceInfo);
	ipc_read_int64(space_info->total_space);
	ipc_read_int64(space_info->used_space);
	ipc_read_int64(space_info->marked_space);
	ipc_read_int64(space_info->clip_space);
	return rval;
}

//-----------------------------------------------------------------------
//
//	avf camera
//
//-----------------------------------------------------------------------
int avf_camera_enable_preview(avf_media_t *self)
{
	return ipc_call_remote(self, AVF_CAMERA_CmdEnablePreview);
}

int avf_camera_disable_preview(avf_media_t *self)
{
	return ipc_call_remote(self, AVF_CAMERA_CmdDisablePreview);
}

int avf_camera_image_control(avf_media_t *self, const char *name, const char *value)
{
	define_write_buffer(512);
	ipc_write_str(name);
	ipc_write_str(value);
	return ipc_call_remote_write(self, AVF_CAMERA_CmdImageControl);
}

static int avf_camera_get_file(avf_media_t *self, char *buffer, int buffer_size, int cmd)
{
	define_read_buffer(512);
	int rval = ipc_call_remote_read(self, cmd);
	if (rval >= 0) {
		char *filename;
		ipc_read_str(filename);
		if (ipc_read_failed()) {
			rval = -1;
		} else {
			strncpy(buffer, filename, buffer_size);
			buffer[buffer_size-1] = 0;
		}
	}
	return 0;
}

int avf_camera_get_current_file(avf_media_t *self, char *buffer, int buffer_size)
{
	return avf_camera_get_file(self, buffer, buffer_size, AVF_CAMERA_CmdGetCurrentFile);
}

int avf_camera_get_previous_file(avf_media_t *self, char *buffer, int buffer_size)
{
	return avf_camera_get_file(self, buffer, buffer_size, AVF_CAMERA_CmdGetPreviousFile);
}

int avf_camera_save_current(avf_media_t *self, int before, int after, const char *filename)
{
	define_write_buffer(512);
	ipc_write_int(before);
	ipc_write_int(after);
	ipc_write_str(filename);
	return ipc_call_remote_write(self, AVF_CAMERA_CmdSaveCurrent);
}

int avf_camera_save_next_picture(avf_media_t *self, const char *filename)
{
	define_write_buffer(512);
	ipc_write_str(filename);
	return ipc_call_remote_write(self, AVF_CAMERA_CmdSaveNextPicture);
}

int avf_camera_mark_live_clip_ex(avf_media_t *self, int delay_ms, int before_live_ms, int after_live_ms, int b_continue)
{
	return avf_camera_mark_live_clip(self, delay_ms, before_live_ms, after_live_ms, b_continue, 0);
}

int avf_camera_mark_live_clip(avf_media_t *self, 
	int delay_ms, int before_live_ms, int after_live_ms, int b_continue, int live_mark_flag)
{
	define_write_buffer(64);
	ipc_write_int(delay_ms);
	ipc_write_int(before_live_ms);
	ipc_write_int(after_live_ms);
	ipc_write_int(b_continue);
	ipc_write_int(live_mark_flag);
	return ipc_call_remote_write(self, AVF_CAMERA_CmdMarkLiveClip);
}

int avf_camera_mark_live_clip_v2(avf_media_t *self,
	int delay_ms, int before_live_ms, int after_live_ms, int b_continue, const char *attr_str)
{
	define_write_buffer(128);
	ipc_write_int(delay_ms);
	ipc_write_int(before_live_ms);
	ipc_write_int(after_live_ms);
	ipc_write_int(b_continue);
	ipc_write_str(attr_str ? attr_str : "");
	return ipc_call_remote_write(self, AVF_CAMERA_CmdMarkLiveClipV2);
}

int avf_camera_cancel_mark_live_clip(avf_media_t *self)
{
	return ipc_call_remote(self, AVF_CAMERA_CmdCancelMarkLiveClip);
}

int avf_camera_get_mark_live_clip_info(avf_media_t *self, avf_mark_live_info_t *info)
{
	define_read_buffer(64);
	int rval = ipc_call_remote_read(self, AVF_CAMERA_CmdGetMarkLiveClipInfo);
	ipc_read_int(info->b_running);
	ipc_read_int(info->b_valid);
	ipc_read_int(info->delay_ms);
	ipc_read_int(info->before_live_ms);
	ipc_read_int(info->after_live_ms);
	ipc_read_int32(info->clip_id);
	return rval;
}

int avf_camera_set_clip_scene_data(avf_media_t *self, int clip_type, uint32_t clip_id,
	const uint8_t *data, uint32_t data_size)
{
	define_write_buffer(600);
	if (data_size == 0 || data_size > 512 || (data_size & 3) != 0) {
		return -1;
	}
	ipc_write_int(clip_type);
	ipc_write_int(clip_id);
	ipc_write_int(data_size);
	ipc_write_data(data, data_size);
	return ipc_call_remote_write(self, AVF_CAMERA_CmdSetClipSceneData);
}

int avf_camera_still_capture(avf_media_t *self, int action, int flags)
{
	define_write_buffer(64);
	ipc_write_int(action);
	ipc_write_int(flags);
	return ipc_call_remote_write(self, AVF_CAMERA_CmdStillCapture);
}

int avf_camera_get_still_capture_info(avf_media_t *self, still_capture_info_t *info)
{
	define_read_buffer(64);
	int rval = ipc_call_remote_read(self, AVF_CAMERA_CmdGetStillCaptureInfo);
	ipc_read_int32(info->b_running);
	ipc_read_int32(info->stillcap_state);
	ipc_read_int32(info->b_raw);
	ipc_read_int32(info->num_pictures);
	ipc_read_int32(info->burst_ticks);
	ipc_read_int32(info->last_error);
	return rval;
}

int avf_camera_get_num_pictures(avf_media_t *self)
{
	define_read_buffer(64);
	int rval = ipc_call_remote_read(self, AVF_CAMERA_CmdGetNumPictures);
	int num = 0;
	ipc_read_int32(num);
	return rval < 0 ? rval : num;
}

// lcd:x_off,y_off,width,height
int avf_camera_change_vout_video(avf_media_t *self, const char *config)
{
	define_write_buffer(256);
	ipc_write_str(config);
	return ipc_call_remote_write(self, AVF_CAMERA_CmdChangeVoutVideo);
}

int avf_camera_set_record_attr(avf_media_t *self, const char *attr_str)
{
	define_write_buffer(256);
	ipc_write_str(attr_str);
	return ipc_call_remote_write(self, AVF_CAMERA_CmdSetRecordAttr);
}

int avf_display_get_vout_info(avf_media_t *self, int vout_id, int vout_type, avf_vout_info_t *info)
{
	define_write_buffer(16);
	define_read_buffer(sizeof(*info) + 16);
	int rval;

	ipc_write_int32(vout_id);
	ipc_write_int32(vout_type);

	rval = ipc_call_remote_write_read(self, AVF_DISPLAY_GET_VOUT_INFO);
	if (rval < 0)
		return rval;

	ipc_read_int32(info->enabled);
	ipc_read_int32(info->vout_width);
	ipc_read_int32(info->vout_height);
	ipc_read_int32(info->video_width);
	ipc_read_int32(info->video_height);
	ipc_read_int32(info->video_xoff);
	ipc_read_int32(info->video_yoff);

	return 0;
}

//-----------------------------------------------------------------------
//
//	avf playback
//
//-----------------------------------------------------------------------
int avf_playback_get_media_info(avf_media_t *self, avf_playback_media_info_t *media_info)
{
	define_read_buffer(64);
	int rval = ipc_call_remote_read(self, AVF_PLAYBACK_CmdGetMediaInfo);
	ipc_read_int32(media_info->total_length_ms);
	ipc_read_int32(media_info->audio_num);
	ipc_read_int32(media_info->video_num);
	ipc_read_int32(media_info->paused);
	return rval;
}

int avf_playback_get_play_pos(avf_media_t *self, int *ms)
{
	define_read_buffer(64);
	int rval = ipc_call_remote_read(self, AVF_PLAYBACK_CmdGetPlayPos);
	ipc_read_int32(*ms);
	return rval;
}

int avf_playback_pause_resume(avf_media_t *self)
{
	return ipc_call_remote(self, AVF_PLAYBACK_CmdPauseResume);
}

int avf_playback_seek(avf_media_t *self, int ms)
{
	define_write_buffer(64);
	ipc_write_int32(ms);
	return ipc_call_remote_write(self, AVF_PLAYBACK_CmdSeek);
}

// mode:
//	not used now. set to 0.
// speed
//	0: normal; 1~n: forward x-speed; -1~-n: backward x-speed
int avf_playback_fastplay(avf_media_t *self, int mode, int speed)
{
	define_write_buffer(16);
	ipc_write_int32(mode);
	ipc_write_int32(speed);
	return ipc_call_remote_write(self, AVF_PLAYBACK_CmdFastPlay);
}

//-----------------------------------------------------------------------
//
//	avf config
//
//-----------------------------------------------------------------------

int avf_display_set_mode(avf_media_t *self, int mode)
{
	define_write_buffer(32);
	ipc_write_int32(mode);
	return ipc_call_remote_write(self, AVF_DISPLAY_SET_MODE);
}

int avf_display_set_vout(avf_media_t *self, vout_config_t *config, const char *lcd_model)
{
	define_write_buffer(256);
	ipc_write_data(config, sizeof(vout_config_t));
	ipc_write_str(lcd_model ? lcd_model : "");
	return ipc_call_remote_write(self, AVF_DISPLAY_SET_VOUT);
}

int avf_display_disable_vout(avf_media_t *self, int vout_id)
{
	define_write_buffer(16);
	ipc_write_int32(vout_id);
	return ipc_call_remote_write(self, AVF_DISPLAY_DISABLE_VOUT);
}

int avf_display_set_backlight(avf_media_t *self, int vout_id, const char *lcd_model, int on)
{
	define_write_buffer(64);
	ipc_write_int32(vout_id);
	ipc_write_str(lcd_model ? lcd_model : "");
	ipc_write_int32(on);
	return ipc_call_remote_write(self, AVF_DISPLAY_SET_BACKLIGHT);
}

int avf_display_set_brightness(avf_media_t *self, int vout_id, int vout_type, int brightness)
{
	define_write_buffer(64);
	ipc_write_int32(vout_id);
	ipc_write_int32(vout_type);
	ipc_write_int32(brightness);
	return ipc_call_remote_write(self, AVF_DISPLAY_SET_BRIGHTNESS);
}

int avf_display_get_brightness(avf_media_t *self, int vout_id, int vout_type, int *brightness)
{
	define_write_buffer(16);
	define_read_buffer(16);
	int rval;

	ipc_write_int32(vout_id);
	ipc_write_int32(vout_type);

	rval = ipc_call_remote_write_read(self, AVF_DISPLAY_GET_BRIGHTNESS);
	if (rval < 0)
		return rval;

	ipc_read_int32(*brightness);

	return 0;
}

int avf_display_flip_vout_video(avf_media_t *self, int vout_id, int vout_type, int flip)
{
	define_write_buffer(16);

	ipc_write_int32(vout_id);
	ipc_write_int32(vout_type);
	ipc_write_int32(flip);

	return ipc_call_remote_write(self, AVF_DISPLAY_FLIP_VOUT_VIDEO);
}

int avf_display_flip_vout_osd(avf_media_t *self, int vout_id, int vout_type, int flip)
{
	define_write_buffer(16);

	ipc_write_int32(vout_id);
	ipc_write_int32(vout_type);
	ipc_write_int32(flip);

	return ipc_call_remote_write(self, AVF_DISPLAY_FLIP_VOUT_OSD);
}

int avf_get_version(avf_media_t *self, uint32_t *version)
{
	define_read_buffer(64);
	int rval = ipc_call_remote_read(self, AVF_GET_VERSION);
	ipc_read_int32(*version);
	return rval;
}

