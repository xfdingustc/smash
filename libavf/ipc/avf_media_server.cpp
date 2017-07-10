
#define LOG_TAG "media_server"

#include <fcntl.h>
#include <syslog.h>

#include <signal.h>
#include <stdarg.h>

#include "avf_common.h"
#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_util.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_base.h"
#include "avf_registry.h"

#include "libipc.h"

#include "avf_media.h"
#include "avf_plat_config.h"
#include "avf_camera_if.h"
#include "avf_playback_if.h"

//#define REDIRECT_STDOUT

IRegistry *g_registry;	// global registry
IMediaControl *g_media_control;
IVDBControl *g_vdb_control;
IClipData *g_clip_data;
ipc_cid_t g_service_cid;
int g_conf = 20;
int g_vmem = 0;

static void avf_media_on_avf_msg(void *context, avf_intptr_t tag, const avf_msg_t& msg)
{
	if (msg.sid == CMP_ID_VDB) {
		switch (msg.code) {
		case IMediaControl::APP_MSG_VDB_STATE_CHANGED:
			AVF_LOGD(C_CYAN "APP_MSG_VDB_STATE_CHANGED" C_NONE);
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventVdbStateChanged);
			break;

		case IMediaControl::APP_MSG_BUFFER_SPACE_FULL:
			AVF_LOGD(C_CYAN "APP_MSG_BUFFER_SPACE_FULL" C_NONE);
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventBufferSpaceFull);
			break;

		case IMediaControl::APP_MSG_PICTURE_LIST_CHANGED:
			AVF_LOGD(C_CYAN "APP_MSG_PICTURE_LIST_CHANGED" C_NONE);
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventPictureListChanged);
			break;

		default:
			AVF_LOGW("msg %d not processed", msg.code);
		}

		return;
	}

	switch (tag) {
	case AVF_MEDIA_NONE:
		AVF_LOGD("tag %d, msg %d", (int)tag, msg.code);
		break;

	case AVF_MEDIA_CAMERA:
		switch (msg.code) {
		case IMediaControl::APP_MSG_STATE_CHANGED:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventStatusChanged);
			break;

		case IMediaControl::APP_MSG_FILE_START:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventFileStart);
			break;

		case IMediaControl::APP_MSG_FILE_END:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventFileEnd);
			break;

		case IMediaControl::APP_MSG_DISK_ERROR:
			AVF_LOGD(C_RED "AVF_CAMERA_EventDiskError" C_NONE);
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventDiskError);
			break;

		case IMediaControl::APP_MSG_DISK_SLOW:
			AVF_LOGD(C_RED "AVF_CAMERA_EventDiskSlow" C_NONE);
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventDiskSlow);
			break;

		case IMediaControl::APP_MSG_DONE_SAVING:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventDoneSaving);
			break;

		case IMediaControl::APP_MSG_DONE_WRITE_NEXT_PICTURE:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventDoneSaveNextPicture);
			break;

		case IMediaControl::APP_MSG_BUFFER_SPACE_LOW:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventBufferSpaceLow);
			break;

		case IMediaControl::APP_MSG_STILL_CAPTURE_STATE_CHANGED:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventStillCaptureStateChanged);
			break;

		case IMediaControl::APP_MSG_PICTURE_TAKEN:
			if (g_vdb_control) {
				g_vdb_control->PictureTaken(msg.p1);
			}
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventPictureTaken);
			break;

		case IMediaControl::APP_MSG_UPLOAD_VIDEO:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventUploadVideo);
			break;

		case IMediaControl::APP_MSG_UPLOAD_PICTURE:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventUploadPicture);
			break;

		case IMediaControl::APP_MSG_UPLOAD_RAW:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventUploadRaw);
			break;

		case IMediaControl::APP_MSG_MMS_MODE1:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventMmsMode1);
			break;

		case IMediaControl::APP_MSG_MMS_MODE2:
			ipc_write_notification(g_service_cid, 1 << AVF_CAMERA_EventMmsMode2);
			break;

		default:
			AVF_LOGW("msg %d not processed", msg.code);
			break;
		}
		break;

	case AVF_MEDIA_PLAYBACK:
		switch (msg.code) {
		case IMediaControl::APP_MSG_STATE_CHANGED:
			ipc_write_notification(g_service_cid, 1 << AVF_PLAYBACK_EventStatusChanged);
			break;

		default:
			AVF_LOGW("msg %d not processed", msg.code);
			break;
		}
		break;

	default:
		AVF_LOGW("unknown msg tag %d", msg.code);
		break;
	}
}

static int load_config(const char *player, const char *filename, const char *config_file, avf_intptr_t tag, int conf)
{
	char *buffer;
	int rval;

	rval = avf_load_text_file(config_file, &buffer);
	if (rval < 0)
		return rval;

	rval = g_media_control->SetMediaSource(player, filename, buffer, tag, conf);
	avf_free_text_buffer(buffer);

	return rval;
}

#define check_read_result(_iwr) \
	do { \
		if (ipc_read_failed()) { \
			server_read_fail(_iwr); \
			return; \
		} \
	} while (0)

static void process_camera_cmd(ipc_write_read_t *iwr, char *write_buffer, int write_buffer_size)
{
	ipc_init_read_iwr(iwr);
	int rval;

	switch (iwr->read_val) {
	case AVF_CAMERA_CmdImageControl: {
			char *name;
			char *value;

			ipc_read_str(name);
			ipc_read_str(value);
			check_read_result(iwr);

			rval = Camera(g_media_control).SetImageControl(name, value);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdEnablePreview:
		rval = Camera(g_media_control).EnablePreview();
		server_cmd_done(iwr, rval);
		break;

	case AVF_CAMERA_CmdDisablePreview:
		rval = Camera(g_media_control).DisablePreview();
		server_cmd_done(iwr, rval);
		break;

	case AVF_CAMERA_CmdGetCurrentFile: {
			__init_write_buffer(write_buffer, write_buffer_size);

			char filename[REG_STR_MAX];
			g_media_control->ReadRegString(NAME_FILE_CURRENT, 0, filename, VALUE_FILE_CURRENT);

			ipc_write_str(filename);
			server_cmd_done_write(iwr, 0);
		}
		break;

	case AVF_CAMERA_CmdGetPreviousFile: {
			__init_write_buffer(write_buffer, write_buffer_size);

			char filename[REG_STR_MAX];
			g_media_control->ReadRegString(NAME_FILE_PREVIOUS, 0, filename, VALUE_FILE_PREVIOUS);

			ipc_write_str(filename);
			server_cmd_done_write(iwr, 0);
		}
		break;

	case AVF_CAMERA_CmdSaveCurrent: {
			int before;
			int after;
			char *filename;

			ipc_read_int(before);
			ipc_read_int(after);
			ipc_read_str(filename);
			check_read_result(iwr);

			rval = Camera(g_media_control).SaveCurrent(before, after, filename);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdSaveNextPicture: {
			char *filename;

			ipc_read_str(filename);
			check_read_result(iwr);

			rval = Camera(g_media_control).SaveNextPicture(filename);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdStillCapture: {
			int action;
			int flags;

			ipc_read_int32(action);
			ipc_read_int32(flags);
			check_read_result(iwr);

			rval = Camera(g_media_control).StillCapture(action, flags);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdGetStillCaptureInfo: {
			__init_write_buffer(write_buffer, write_buffer_size);
			IVideoControl::stillcap_info_s info = {0};
			rval = Camera(g_media_control).GetStillCaptureInfo(&info);
			ipc_write_int32(info.b_running);
			ipc_write_int32(info.stillcap_state);
			ipc_write_int32(info.b_raw);
			ipc_write_int32(info.num_pictures);
			ipc_write_int32(info.burst_ticks);
			ipc_write_int32(info.last_error);
			server_cmd_done_write(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdGetNumPictures: {
			__init_write_buffer(write_buffer, write_buffer_size);
			int num;

			if (g_vdb_control) {
				rval = g_vdb_control->GetNumPictures(&num);
			} else {
				rval = E_BADCALL;
			}

			ipc_write_int32(num);
			server_cmd_done_write(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdChangeVoutVideo: {
			char *config;
			ipc_read_str(config);
			rval = Camera(g_media_control).ChangeVoutVideo(config);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdSetRecordAttr: {
			char *attr_str;

			ipc_read_str(attr_str);
			check_read_result(iwr);

			if (g_vdb_control) {
				rval = g_vdb_control->SetRecordAttr(attr_str);
			} else {
				rval = E_BADCALL;
			}

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdMarkLiveClip: {
			int delay_ms;
			int before_live_ms;
			int after_live_ms;
			int b_continue;
			int live_mark_flag;

			ipc_read_int(delay_ms);
			ipc_read_int(before_live_ms);
			ipc_read_int(after_live_ms);
			ipc_read_int(b_continue);
			ipc_read_int(live_mark_flag);
			check_read_result(iwr);

			if (g_vdb_control) {
				rval = g_vdb_control->MarkLiveClip(delay_ms, before_live_ms, after_live_ms, b_continue, live_mark_flag, "");
			} else {
				rval = E_BADCALL;
			}

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdMarkLiveClipV2: {
			int delay_ms;
			int before_live_ms;
			int after_live_ms;
			int b_continue;
			char *attr_str;

			ipc_read_int(delay_ms);
			ipc_read_int(before_live_ms);
			ipc_read_int(after_live_ms);
			ipc_read_int(b_continue);
			ipc_read_str(attr_str);
			check_read_result(iwr);

			if (g_vdb_control) {
				rval = g_vdb_control->MarkLiveClip(delay_ms, before_live_ms, after_live_ms, b_continue, 1, attr_str);
			} else {
				rval = E_BADCALL;
			}

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_CAMERA_CmdCancelMarkLiveClip:
		if (g_vdb_control) {
			rval = g_vdb_control->StopMarkLiveClip();
		} else {
			rval = E_BADCALL;
		}
		server_cmd_done(iwr, rval);
		break;

	case AVF_CAMERA_CmdGetMarkLiveClipInfo:
		if (g_vdb_control) {
			__init_write_buffer(write_buffer, write_buffer_size);

			IVDBControl::mark_info_s info;
			rval = g_vdb_control->GetMarkLiveInfo(info);

			ipc_write_int(info.b_running);
			ipc_write_int(info.b_valid);
			ipc_write_int(info.delay_ms);
			ipc_write_int(info.before_live_ms);
			ipc_write_int(info.after_live_ms);
			ipc_write_int(info.clip_id);

			server_cmd_done_write(iwr, rval);
		} else {
			server_cmd_done(iwr, E_BADCALL);
		}
		break;

	case AVF_CAMERA_CmdSetClipSceneData:
		if (g_vdb_control) {
			int clip_type;
			uint32_t clip_id;
			uint32_t data_size;
			char *data;

			ipc_read_int(clip_type);
			ipc_read_int(clip_id);
			ipc_read_int(data_size);
			ipc_read_data(data, data_size);
			check_read_result(iwr);

			rval = g_vdb_control->SetClipSceneData(clip_type, clip_id, (const avf_u8_t*)data, data_size);
			server_cmd_done(iwr, rval);
		} else {
			server_cmd_done(iwr, E_BADCALL);
		}
		break;

	default:
		server_read_fail(iwr);
		break;
	}
}

static void process_playback_cmd(ipc_write_read_t *iwr, char *write_buffer, int write_buffer_size)
{
	ipc_init_read_iwr(iwr);
	int rval;

	switch (iwr->read_val) {
	case AVF_PLAYBACK_CmdGetMediaInfo: {
			__init_write_buffer(write_buffer, write_buffer_size);
			Playback::PlaybackMediaInfo_t pb_info = {0};

			rval = Playback(g_media_control).GetMediaInfo(&pb_info);
			if (rval == 0) {
				ipc_write_int32(pb_info.total_length_ms);
				ipc_write_int32(pb_info.audio_num);
				ipc_write_int32(pb_info.video_num);
				ipc_write_int32(pb_info.bPaused);
			}
			server_cmd_done_write(iwr, rval);
		}
		break;

	case AVF_PLAYBACK_CmdGetPlayPos: {
			__init_write_buffer(write_buffer, write_buffer_size);
			avf_u32_t pos = 0;
			rval = Playback(g_media_control).GetPlayPos(&pos);
			if (rval == 0) {
				ipc_write_int32(pos);
			}
			server_cmd_done_write(iwr, rval);
		}
		break;

	case AVF_PLAYBACK_CmdPauseResume:
		rval = Playback(g_media_control).PauseResume();
		server_cmd_done(iwr, rval);
		break;

	case AVF_PLAYBACK_CmdSeek: {
			avf_u32_t ms;

			ipc_read_int32(ms);
			check_read_result(iwr);

			rval = Playback(g_media_control).Seek(ms);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_PLAYBACK_CmdFastPlay: {
			Playback::FastPlayParam_t param;

			ipc_read_int32(param.mode);
			ipc_read_int32(param.speed);
			check_read_result(iwr);

			rval = Playback(g_media_control).FastPlay(&param);
			server_cmd_done(iwr, rval);
		}
		break;

	default:
		server_read_fail(iwr);
		break;
	}
}

static int read_clip_data(const char *video_filename, const char *video_filename1,
	const char *picture_filename, int raw_flags, const char *raw_filename)
{
	IClipData::desc_s desc;
	int rval;
	bool all_end = true;

	if (video_filename && *video_filename) {
		desc.data_type = VDB_DATA_VIDEO;
		desc.stream = 0;
		rval = g_clip_data->ReadClipDataToFile(&desc, video_filename);
		if (rval < 0)
			return rval;
		if (rval == E_OK)
			all_end = false;
	}

	if (video_filename1 && *video_filename1) {
		desc.data_type = VDB_DATA_VIDEO;
		desc.stream = 1;
		rval = g_clip_data->ReadClipDataToFile(&desc, video_filename1);
		if (rval < 0)
			return rval;
		if (rval == E_OK)
			all_end = false;
	}

	if (picture_filename && *picture_filename) {
		desc.data_type = VDB_DATA_JPEG;
		rval = g_clip_data->ReadClipDataToFile(&desc, picture_filename);
		if (rval < 0)
			return rval;
		if (rval == E_OK)
			all_end = false;
	}

	if (raw_flags && raw_filename && *raw_filename) {
		rval = g_clip_data->ReadClipRawDataToFile(raw_flags, raw_filename);
		if (rval < 0)
			return rval;
		if (rval == E_OK)
			all_end = false;
	}

	return all_end ? S_END : E_OK;
}

static void process_media_cmd(ipc_write_read_t *iwr, char *write_buffer, int write_buffer_size)
{
	ipc_init_read_iwr(iwr);
	int rval;

	switch (iwr->read_val) {
	case AVF_MEDIA_CmdOpen: {
			int wait;

			ipc_read_int(wait);
			check_read_result(iwr);

			rval = g_media_control->OpenMedia(wait);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_CmdClose: {
			int wait;

			ipc_read_int(wait);
			check_read_result(iwr);

			rval = g_media_control->CloseMedia(wait);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_CmdPrepare: {
			int wait;

			ipc_read_int(wait);
			check_read_result(iwr);

			rval = g_media_control->PrepareMedia(wait);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_CmdRun: {
			int wait;

			ipc_read_int(wait);
			check_read_result(iwr);

			//AVF_LOGI(C_YELLOW "===== call RunMedia ======" C_NONE);
			rval = g_media_control->RunMedia(wait);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_CmdStop: {
			int wait;

			ipc_read_int(wait);
			check_read_result(iwr);

			//AVF_LOGI(C_YELLOW "===== call StopMedia ======" C_NONE);
			rval = g_media_control->StopMedia(wait);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_CmdGetState: {
			IMediaControl::StateInfo stateInfo;
			g_media_control->GetStateInfo(stateInfo);
			int state;

			switch (stateInfo.state) {
			case IMediaControl::STATE_IDLE:
			case IMediaControl::STATE_OPENING:
				state = AVF_MEDIA_STATE_IDLE;
				break;

			case IMediaControl::STATE_OPEN:	// STATE_STOPPED
			case IMediaControl::STATE_PREPARING:
				state = AVF_MEDIA_STATE_STOPPED;
				break;

			case IMediaControl::STATE_PREPARED:
			case IMediaControl::STATE_STARTING:
				state = AVF_MEDIA_STATE_PREPARED;
				break;

			case IMediaControl::STATE_RUNNING:
			case IMediaControl::STATE_STOPPING:
				state = AVF_MEDIA_STATE_RUNNING;
				break;

			case IMediaControl::STATE_ERROR:
				state = AVF_MEDIA_STATE_ERROR;
				break;

			default:
				state = AVF_MEDIA_STATE_STOPPED;
				break;
			}
			server_cmd_done(iwr, state);
		}
		break;

	case AVF_MEDIA_CmdSetMediaSource: {
			char *player;
			char *filename;
			char *config;
			avf_intptr_t tag;
			int conf;

			ipc_read_str(player);
			ipc_read_str(filename);
			ipc_read_str(config);
			ipc_read_int32(tag);

			if (__ipc_rsize >= (int)sizeof(int32_t)) {
				ipc_read_int32(conf);
			} else {
				conf = g_conf;
			}

			check_read_result(iwr);

			rval = load_config(player, filename, config, tag, conf);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_SetConfigInt32: {
			char *name;
			int index;
			int32_t value;

			ipc_read_str(name);
			ipc_read_int(index);
			ipc_read_int32(value);
			check_read_result(iwr);

			rval = g_media_control->WriteRegInt32(name, index, value);

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_SetConfigInt64: {
			char *name;
			int index;
			int64_t value;

			ipc_read_str(name);
			ipc_read_int(index);
			ipc_read_int64(value);
			check_read_result(iwr);

			rval = g_media_control->WriteRegInt64(name, index, value);

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_SetConfigBool: {
			char *name;
			int index;
			int value;

			ipc_read_str(name);
			ipc_read_int(index);
			ipc_read_int(value);
			check_read_result(iwr);

			rval = g_media_control->WriteRegBool(name, index, value);

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_SetConfigString: {
			char *name;
			int index;
			char *value;

			ipc_read_str(name);
			ipc_read_int(index);
			ipc_read_str(value);
			check_read_result(iwr);

			rval = g_media_control->WriteRegString(name, index, value);

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_GetConfigInt32: {
			__init_write_buffer(write_buffer, write_buffer_size);

			char *name;
			int id;
			int32_t default_value;
			int32_t value;

			ipc_read_str(name);
			ipc_read_int(id);
			ipc_read_int32(default_value);
			check_read_result(iwr);

			value = g_media_control->ReadRegInt32(name, id, default_value);

			ipc_write_int32(value);

			server_cmd_done_write(iwr, 0);
		}
		break;

	case AVF_MEDIA_GetConfigInt64: {
			__init_write_buffer(write_buffer, write_buffer_size);

			char *name;
			int id;
			int64_t default_value;
			int64_t value;

			ipc_read_str(name);
			ipc_read_int(id);
			ipc_read_int64(default_value);
			check_read_result(iwr);

			value = g_media_control->ReadRegInt64(name, id, default_value);

			ipc_write_int64(value);

			server_cmd_done_write(iwr, 0);
		}
		break;

	case AVF_MEDIA_GetConfigBool: {
			__init_write_buffer(write_buffer, write_buffer_size);

			char *name;
			int id;
			int default_value;
			int value;

			ipc_read_str(name);
			ipc_read_int(id);
			ipc_read_int(default_value);
			check_read_result(iwr);

			value = g_media_control->ReadRegBool(name, id, default_value);

			ipc_write_int(value);

			server_cmd_done_write(iwr, 0);
		}
		break;

	case AVF_MEDIA_GetConfigString: {
			__init_write_buffer(write_buffer, write_buffer_size);

			char *name;
			int id;
			char *default_value;
			char value[REG_STR_MAX];

			ipc_read_str(name);
			ipc_read_int(id);
			ipc_read_str(default_value);
			check_read_result(iwr);

			g_media_control->ReadRegString(name, id, value, default_value);

			ipc_write_str(value);

			server_cmd_done_write(iwr, 0);
		}
		break;

	case AVF_MEDIA_GetNumClips: {
			IVdbInfo *pVdbInfo;
			int clip_type;
			avf_u32_t num_clips;
			avf_u32_t sid;

			__init_write_buffer(write_buffer, write_buffer_size);

			pVdbInfo = IVdbInfo::GetInterfaceFrom(g_media_control);
			if (pVdbInfo == NULL) {
				server_cmd_done(iwr, -1);
				return;
			}

			ipc_read_int32(clip_type);
			check_read_result(iwr);

			rval = pVdbInfo->GetNumClips(clip_type, &num_clips, &sid);
			if (rval == 0) {
				ipc_write_int32(num_clips);
				ipc_write_int32(sid);
			}
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_GetClipsInfo: {
			IVdbInfo *pVdbInfo;
			int clip_type;
			uint32_t start;
			uint32_t num;
			uint32_t sid;
			IVdbInfo::clip_set_info_s cs_info[__NUM_CLIP_INFO];

			__init_write_buffer(write_buffer, write_buffer_size);

			pVdbInfo = IVdbInfo::GetInterfaceFrom(g_media_control);
			if (pVdbInfo == NULL) {
				server_cmd_done(iwr, -1);
				return;
			}

			ipc_read_int32(clip_type);
			ipc_read_int32(start);
			ipc_read_int32(num);
			check_read_result(iwr);

			if (num > __NUM_CLIP_INFO) {
				server_cmd_done(iwr, -1);
				return;
			}

			rval = pVdbInfo->GetClipSetInfo(clip_type, cs_info, start, &num, &sid);
			if (rval == 0) {
				ipc_write_int32(num);
				ipc_write_int32(sid);
				for (avf_u32_t i = 0; i < num; i++) {
					avf_clip_info_t info;
					info.clip_id = cs_info[i].ref_clip_id;
					info.clip_date = cs_info[i].ref_clip_date;
					info.length_ms = cs_info[i].length_ms;
					info.clip_time_ms = cs_info[i].clip_time_ms;
					ipc_write_data(&info, sizeof(info));
				}
			}
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_DeleteClip: {
			IVdbInfo *pVdbInfo;
			int clip_type;
			uint32_t clip_id;

			pVdbInfo = IVdbInfo::GetInterfaceFrom(g_media_control);
			if (pVdbInfo == NULL) {
				server_cmd_done(iwr, -1);
				return;
			}

			ipc_read_int32(clip_type);
			ipc_read_int32(clip_id);
			check_read_result(iwr);

			rval = pVdbInfo->DeleteClip(clip_type, clip_id);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_CmdStorageReady: {
			int is_ready;

			ipc_read_int(is_ready);
			check_read_result(iwr);

			if (g_vdb_control) {
				g_vdb_control->StorageReady(is_ready);
				rval = 0;
			} else {
				rval = -1;
			}

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_CmdGetVdbInfo:
	case AVF_MEDIA_CmdFreeVdbSpace: {
			__init_write_buffer(write_buffer, write_buffer_size);

			IVDBControl::vdb_info_s info = {0};
			if (g_vdb_control) {
				int free_vdb_space = (iwr->read_val == AVF_MEDIA_CmdFreeVdbSpace);
				g_vdb_control->GetVdbInfo(&info, free_vdb_space);
			}

			ipc_write_int32(info.total_space_mb);
			ipc_write_int32(info.disk_space_mb);
			ipc_write_int32(info.file_space_mb);
			ipc_write_int(info.vdb_ready);
			ipc_write_int(info.enough_space);
			ipc_write_int(info.enough_space_autodel);

			server_cmd_done_write(iwr, 0);
		}
		break;

	case AVF_MEDIA_CmdInitClipReader: {
			IVdbInfo::range_s range = {0};
			ipc_read_int(range.clip_type);
			ipc_read_int32(range.ref_clip_id);
			ipc_read_int64(range.clip_time_ms);
			ipc_read_int32(range.length_ms);
			check_read_result(iwr);

			if (g_clip_data == NULL) {
				IVdbInfo *pVdbInfo = NULL;
				if (g_vdb_control) {
					pVdbInfo = IVdbInfo::GetInterfaceFrom(g_media_control);
				}
				if (pVdbInfo) {
					g_clip_data = pVdbInfo->CreateClipDataOfTime(&range);
				}
			}

			rval = g_clip_data ? E_OK : E_ERROR;
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_CmdReleaseClipReader:
		avf_safe_release(g_clip_data);
		server_cmd_done(iwr, 0);
		break;

	case AVF_MEDIA_CmdReadClipData: {
			char *video_filename;
			char *video_filename1;
			char *picture_filename;
			int raw_flags;
			char *raw_filename;

			if (g_clip_data == NULL) {
				server_cmd_done(iwr, E_ERROR);
				break;
			}

			ipc_read_str(video_filename);
			ipc_read_str(video_filename1);
			ipc_read_str(picture_filename);
			ipc_read_int(raw_flags);
			ipc_read_str(raw_filename);
			check_read_result(iwr);

			rval = read_clip_data(video_filename, video_filename1, 
				picture_filename, raw_flags, raw_filename);
			server_cmd_done(iwr, rval);

		}
		break;

	case AVF_MEDIA_CmdEnablePictureService: {
			char *picture_path;

			ipc_read_str(picture_path);
			check_read_result(iwr);

			if (g_vdb_control == NULL) {
				rval = -1;
			} else {
				rval = g_vdb_control->EnablePictureService(picture_path);
			}

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_MEDIA_CmdGetSpaceInfo: {
			__init_write_buffer(write_buffer, write_buffer_size);

			avf_u64_t total_space;
			avf_u64_t used_space;
			avf_u64_t marked_space;
			avf_u64_t clip_space;

			if (g_vdb_control) {
				rval = g_vdb_control->GetSpaceInfo(&total_space, &used_space, &marked_space, &clip_space);
			} else {
				rval = E_ERROR;
			}

			ipc_write_int64(total_space);
			ipc_write_int64(used_space);
			ipc_write_int64(marked_space);
			ipc_write_int64(clip_space);
		
			server_cmd_done_write(iwr, rval);
		}
		break;

	case AVF_MEDIA_ReadMem: {
			__init_write_buffer(write_buffer, write_buffer_size);

			char *name;
			int id;
			unsigned size;
			unsigned nsize;

			ipc_read_str(name);
			ipc_read_int(id);
			ipc_read_int(size);
			check_read_result(iwr);

			nsize = __ipc_wsize - sizeof(int);
			if (nsize > size)
				nsize = size;

			ipc_write_int32(nsize);

			g_media_control->ReadRegMem(name, id, (avf_u8_t*)__ipc_wptr, nsize);

			ipc_inc_pos(nsize);

			server_cmd_done_write(iwr, 0);
		}
		break;

	case AVF_MEDIA_WriteMem: {
			char *name;
			int id;
			unsigned size;

			ipc_read_str(name);
			ipc_read_int(id);
			ipc_read_int(size);
			check_read_result(iwr);

			if (size > (unsigned)__ipc_rsize) {
				rval = -EINVAL;
			} else {
				rval = g_media_control->WriteRegMem(name, id, (const avf_u8_t*)__ipc_rptr, size);
			}

			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_DISPLAY_SET_MODE: {
			int mode;

			ipc_read_int(mode);
			check_read_result(iwr);

			rval = avf_platform_display_set_mode(mode);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_DISPLAY_SET_VOUT: {
			char *data;
			vout_config_t *config;
			char *lcd_model = (char*)"t15p00";

			ipc_read_data(data, sizeof(vout_config_t));
			ipc_read_str(lcd_model);
			check_read_result(iwr);

			config = (vout_config_t*)data;
			rval = avf_platform_display_set_vout(config, lcd_model);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_DISPLAY_DISABLE_VOUT: {
			int vout_id;

			ipc_read_int32(vout_id);
			check_read_result(iwr);

			rval = avf_platform_display_disable_vout(vout_id);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_DISPLAY_SET_BACKLIGHT: {
			int vout_id;
			char *lcd_model;
			int on;

			ipc_read_int32(vout_id);
			ipc_read_str(lcd_model);
			ipc_read_int32(on);
			check_read_result(iwr);

			rval = avf_platform_display_set_backlight(vout_id, lcd_model, on);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_DISPLAY_SET_BRIGHTNESS: {
			vout_brightness_t param;

			ipc_read_int32(param.vout_id);
			ipc_read_int32(param.vout_type);
			ipc_read_int32(param.brightness);
			check_read_result(iwr);

			rval = avf_platform_display_set_brightness(&param);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_DISPLAY_GET_BRIGHTNESS: {
			__init_write_buffer(write_buffer, write_buffer_size);
			vout_brightness_t param;

			ipc_read_int32(param.vout_id);
			ipc_read_int32(param.vout_type);
			check_read_result(iwr);

			rval = avf_platform_display_get_brightness(&param);

			ipc_write_int32(param.brightness);

			server_cmd_done_write(iwr, rval);
		}
		break;

	case AVF_DISPLAY_GET_VOUT_INFO: {
			__init_write_buffer(write_buffer, write_buffer_size);
			int vout_id;
			int vout_type;
			vout_info_t vout_info;

			ipc_read_int32(vout_id);
			ipc_read_int32(vout_type);
			AVF_UNUSED(vout_type);
			check_read_result(iwr);

			rval = avf_platform_display_get_vout_info(vout_id, &vout_info);

			ipc_write_int32(vout_info.enabled);
			ipc_write_int32(vout_info.vout_width);
			ipc_write_int32(vout_info.vout_height);
			ipc_write_int32(vout_info.video_width);
			ipc_write_int32(vout_info.video_height);
			ipc_write_int32(vout_info.video_xoff);
			ipc_write_int32(vout_info.video_yoff);

			server_cmd_done_write(iwr, rval);
		}
		break;

	case AVF_DISPLAY_FLIP_VOUT_VIDEO: {
			int vout_id;
			int vout_type;
			int flip;

			ipc_read_int32(vout_id);
			ipc_read_int32(vout_type);
			ipc_read_int32(flip);
			check_read_result(iwr);

			rval = avf_platform_display_flip_vout_video(vout_id, vout_type, flip);
			server_cmd_done(iwr, rval);
		}
		break;

	case AVF_DISPLAY_FLIP_VOUT_OSD: {
			int vout_id;
			int vout_type;
			int flip;

			ipc_read_int32(vout_id);
			ipc_read_int32(vout_type);
			ipc_read_int32(flip);
			check_read_result(iwr);

			rval = avf_platform_display_flip_vout_osd(vout_id, vout_type, flip);
			server_cmd_done(iwr, rval);
		}
		break;

	default:
		if (iwr->read_val >= AVF_GET_VERSION) {
			__init_write_buffer(write_buffer, write_buffer_size);

			ipc_write_int32(AVF_VERSION);

			server_cmd_done_write(iwr, 0);

			break;
		}
		if (iwr->read_val >= AVF_MEDIA_CmdPlayback) {
			process_playback_cmd(iwr, write_buffer, write_buffer_size);
			break;
		}
		if (iwr->read_val >= AVF_MEDIA_CmdCamera) {
			process_camera_cmd(iwr, write_buffer, write_buffer_size);
			break;
		}
		server_read_fail(iwr);
		break;
	}
}

static int run_media_service(void)
{
	ipc_write_read_t iwr;
	char read_buffer[1024];
	char write_buffer[512];

#ifdef VALGRIND
	::memset(read_buffer, 0, sizeof(read_buffer));
	::memset(write_buffer, 0, sizeof(write_buffer));
#endif

	ipc_init_service_iwr(&iwr, g_service_cid, NULL, read_buffer, sizeof(read_buffer));

	while (1) {
		if (ipc_write_read(&iwr) < 0) {
			if (g_ipc_fd < 0) {
				return 0;
			} else {
				printf("ipc_write_read failed, read: %d, write:%d\n", iwr.read_val, iwr.write_val);
				ipc_init_service_iwr(&iwr, g_service_cid, NULL, read_buffer, sizeof(read_buffer));
				continue;
			}
		}
		process_media_cmd(&iwr, write_buffer, sizeof(write_buffer));
	}

	return 0;
}

static void signal_handler(int signo)
{
	// close the file so my main thread will fail and exit
	ipc_deinit();
}

#include <getopt.h>
#define NO_ARG	0
#define HAS_ARG	1

static int g_logo = 0;
static int g_syslog = 0;
static int g_stdlog = 0;
static int g_istty = 0;

static struct option long_options[] = {
	{"log", HAS_ARG, 0,'L'},
	{"set", HAS_ARG, 0,'S'},
	{"syslog", NO_ARG, 0, 'V'},
	{"stdlog", NO_ARG, 0, 'T'},
	{"logo", NO_ARG, 0, 'G'},	// enable builtin logo
	{"conf", HAS_ARG, 0, 'c'},	// set conf index
	{"vmem", NO_ARG, 0, 0x80},
	{NULL, NO_ARG, 0, 0}
};

static const char *short_options = "GL:S:VTc:";

int init_param(int argc, char **argv)
{
	int option_index = 0;
	int ch;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'G':
			g_logo = 1;
			break;

		case 'L':
			if (strcmp(optarg, "all") == 0)
				avf_set_logs(AVF_LOG_ALL);
			else
				avf_set_logs(optarg);
			break;

		case 'S': {
				char *p = strchr(optarg, '=');
				if (p) {
					char save = *p;
					*p = 0;
					g_registry->WriteRegString(optarg, 0, p + 1);
					*p = save;
				} else {
					AVF_LOGE("bad arg %s, '=' not found", optarg);
				}
			}
			break;

		case 'V':
			g_syslog = 1;
			break;

		case 'T':
			g_stdlog = 1;
			break;

		case 'c':
			g_conf = ::atoi(optarg);
			break;

		case 0x80:
			g_vmem = 1;
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

static int avf_syslog(const char *fmt, ...)
{
	va_list va;
	int rval = 0;

	va_start(va, fmt);

	if (g_syslog) {
		vsyslog(LOG_USER|LOG_INFO, fmt, va);
	}

	if (!g_syslog || g_stdlog) {
		rval = vfprintf(stdout, fmt, va);
		if (!g_istty) {
			fflush(stdout);
		}
	}

	va_end(va);

	return rval;
}

// avf_media_server 
//	--set dbg.overlay.fontsize=12
//	--set dbg.overlay.height=160
//	--set dbg.showosd=power+accel+obd
//	--log all|ewivdp
//	--syslog
//	--stdlog (when syslog is specified)
int main(int argc, char **argv)
{
#ifdef REDIRECT_STDOUT
	int fd, fd2;
	fd2 = dup(STDOUT_FILENO);
	fd = avf_open_file("/tmp/avf.txt", O_RDWR|O_CREAT|O_TRUNC, 0);
	dup2(fd, STDOUT_FILENO);
	avf_close_file(fd);
#endif

	g_registry = CRegistry2::Create(NULL);
	if (g_registry == NULL) {
		printf("cannot create registry\n");
		return -1;
	}

	g_registry->WriteRegString(NAME_AVF_VERSION, 0, VALUE_AVF_VERSION);

	init_param(argc, argv);

	if (g_logo) {
		g_registry->WriteRegInt32(NAME_INTERNAL_LOGO, 0, 1);		// main stream
		g_registry->WriteRegInt32(NAME_INTERNAL_LOGO, 1, 1);		// secondary stream
		g_registry->WriteRegInt32(NAME_INTERNAL_LOGO, 2, 1);		// preview stream
	}

	if (g_vmem) {
		g_registry->WriteRegInt32(NAME_DBG_VMEM, 0, 1);
	}

	if (ipc_init() < 0)
		return -1;

	if (ipc_register_service(AVF_MEDIA_SERVICE_NAME, &g_service_cid) < 0) {
		printf("register service \"%s\" failed!\n", AVF_MEDIA_SERVICE_NAME);
		return -1;
	}

	avf_module_init();
	avf_platform_init();

	avf_printf = avf_syslog;
	g_istty = isatty(fileno(stdout));

	g_media_control = avf_create_media_control(g_registry, avf_media_on_avf_msg, NULL, true);
	if (g_media_control == NULL) {
		printf("avf_create_media_control failed\n");
		return -1;
	}

	g_vdb_control = avf_create_vdb_control(g_registry, avf_media_on_avf_msg, NULL);
	if (g_vdb_control == NULL) {
		printf("avf_create_vdb_control failed\n");
		return -1;
	}

	g_media_control->RegisterComponent(g_vdb_control);

	// install signal handlers
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);

	run_media_service();

	g_media_control->Release();
	g_registry->Release();
	g_vdb_control->Release();

	// print memory for debug
	avf_mem_info(1);
	avf_file_info();
	avf_socket_info();

#ifdef REDIRECT_STDOUT
	avf_close_file(fd2);
#endif

	return 0;
}

