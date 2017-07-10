
#define LOG_TAG "vdb_manager"

#include <sys/stat.h>
#include <sys/statfs.h>
#include <dirent.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_config.h"
#include "avf_registry.h"
#include "avf_util.h"
#include "mem_stream.h"

#include "vdb_cmd.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_util.h"
#include "vdb_if.h"
#include "vdb.h"
#include "vdb_server.h"
#include "vdb_http_server.h"
#include "vdb_manager.h"
#include "picture_list.h"

#define APP_CHECK_STORAGE
#define RECORD_CH 0

extern "C" IVDBControl *avf_create_vdb_control(IRegistry *pRegistry,
	avf_app_msg_callback callback, void *context)
{
	return CVDBManager::Create(pRegistry, callback, context);
}

//-----------------------------------------------------------------------
//
//  CVDBManager
//
//-----------------------------------------------------------------------

const vdb_set_s CVDBManager::m_vdb_set = {
	CVDBManager::get_vdb,
	CVDBManager::release_vdb,
	NULL, 	// get_all_clips
	NULL,	// release_clips
	NULL,	// get_clip_poster
	NULL,	// release_clip_poster
	NULL,	// on_clip_created
	NULL,	// on_clip_deleted
};

CVDBManager* CVDBManager::Create(IRegistry *pRegistry, avf_app_msg_callback callback, void *context)
{
	CVDBManager *result = avf_new CVDBManager(pRegistry, callback, context);
	CHECK_STATUS(result);
	return result;
}

CVDBManager::CVDBManager(IRegistry *pRegistry, avf_app_msg_callback callback, void *context):
	mpRegistry(pRegistry),
	m_app_msg_callback(callback),
	m_app_context(context),
	mName("vdb_manager"),
	mpConfig(NULL),

	mpVDB(NULL),
	mpVdbServer(NULL),
	mpHttpServer(NULL),

	mpWorkQ(NULL),
	mpMsgQ(NULL)
{
	m_mount_point[0] = 0;
	mbStorageReady = false;
	SET_ARRAY_NULL(mpStorageListeners);

	CREATE_OBJ(mpConfig = CConfig::Create());

	CREATE_OBJ(mpVdbServer = CVDBServer::Create(&m_vdb_set, (void*)this));

	CREATE_OBJ(mpHttpServer = CVdbHttpServer::Create(&m_vdb_set, (void*)this));

	// false: none-server mode
	CREATE_OBJ(mpVDB = CVDB::Create(mpVdbServer, (void*)this, false));

	CREATE_OBJ(mpMsgQ = CMsgQueue::Create("VdbMsgQ", sizeof(vdb_man_cmd_t), 8));

	CREATE_OBJ(mpWorkQ = CWorkQueue::Create(static_cast<IActiveObject*>(this)));

	mpWorkQ->SetRTPriority(CThread::PRIO_WRITER);
	mpWorkQ->AttachMsgQueue(mpMsgQ, CMsgQueue::Q_INPUT);

#ifndef APP_CHECK_STORAGE
	// trigger - now app will trigger it
	AVF_LOGD(C_LIGHT_RED "trigger StorageReady" C_NONE);
	StorageReady(1);
#endif

	// start servers
	mpVdbServer->Start();
	mpHttpServer->Start();

	mpWorkQ->Run();
}

CVDBManager::~CVDBManager()
{
	if (mpWorkQ) {
		// must be after other cmds
		vdb_man_cmd_t cmd(CMD_Stop);
		SendCmd(cmd);
		if (mpMsgQ)
			mpWorkQ->DetachMsgQueue(mpMsgQ);
	}

	avf_safe_release(mpWorkQ);
	avf_safe_release(mpMsgQ);
	avf_safe_release(mpConfig);

	avf_safe_release(mpHttpServer);
	avf_safe_release(mpVdbServer);
	avf_safe_release(mpVDB);
}

void *CVDBManager::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IVDBControl)
		return static_cast<IVDBControl*>(this);
	if (refiid == IID_IActiveObject)
		return static_cast<IActiveObject*>(this);
	if (refiid == IID_IRecordObserver)
		return static_cast<IRecordObserver*>(this);
	if (refiid == IID_IVdbInfo)
		return mpVDB->GetInterface(refiid);
	return inherited::GetInterface(refiid);
}

CVDB *CVDBManager::get_vdb(void *context, const char *vdb_id)
{
	CVDBManager *self = (CVDBManager*)context;
	return self->mpVDB;
}

void CVDBManager::release_vdb(void *context, const char *vdb_id, CVDB *vdb)
{
}

void CVDBManager::AOLoop()
{
	mpWorkQ->ReplyCmd(E_OK);

	while (true) {
		CMsgHub::MsgResult result;
		mpWorkQ->WaitMsg(result, CMsgQueue::Q_CMD | CMsgQueue::Q_INPUT);

		if (result.pMsgQ) {
			if (mpWorkQ->IsCmdQ(result.pMsgQ)) {

				AO::CMD cmd;
				if (mpWorkQ->PeekCmd(cmd)) {
					if (!AOCmdProc(cmd, true)) {
						// returning false means stop AOLoop
						return;
					}
				}

			} else if (result.pMsgQ == mpMsgQ) {

				vdb_man_cmd_t cmd;
				if (mpMsgQ->PeekMsg(&cmd, sizeof(vdb_man_cmd_t))) {
					if (cmd.cmd_code == CMD_Stop) {
						AVF_LOGD(C_WHITE "vdb manager stopped" C_NONE);
						ReplyCmd(E_OK);
						return;
					}
					HandleCmd(cmd);
				}

			}
		}
	}
}

bool CVDBManager::AOCmdProc(const CMD& cmd, bool bInAOLoop)
{
	switch (cmd.code) {
	case AO::CMD_STOP:
		mpWorkQ->ReplyCmd(E_OK);
		return false;

	default:
		mpWorkQ->ReplyCmd(E_UNKNOWN);
		return true;
	}
}

bool CVDBManager::ShouldAutoDelete()
{
	return mpRegistry->ReadRegBool(NAME_VDB_AUTODEL, IRegistry::GLOBAL, VALUE_VDB_AUTODEL);
}

bool CVDBManager::IsStorageReady()
{
	DIR *dir = ::opendir(m_mount_point); 
	bool bExists = false;
	if (dir) {
		bExists = true;
		::closedir(dir);
	}
	return bExists;
}

void CVDBManager::AppCallback(avf_uint_t msg_code)
{
	avf_msg_t msg;
	msg.sid = CMP_ID_VDB;
	msg.code = msg_code; //IEngine::EV_APP_EVENT;
	msg.p0 = 0;//msg_code;
	msg.p1 = 0;
	m_app_msg_callback(m_app_context, 0, msg);
}

void CVDBManager::StorageReady(int is_ready)
{
	int no_delete = mpRegistry->ReadRegBool(NAME_VDB_NO_DELETE, IRegistry::GLOBAL, VALUE_VDB_NO_DELETE);
	AVF_LOGD(C_YELLOW "StorageReady %d no_delete %d" C_NONE, is_ready, no_delete);

	vdb_man_cmd_t cmd(CMD_MediaStateChanged);
	cmd.u.MediaStateChanged.is_ready = is_ready;
	cmd.u.MediaStateChanged.no_delete = no_delete;

	if (is_ready) {
		PostCmd(cmd);
	} else {
		SendCmd(cmd);
	}
}

avf_status_t CVDBManager::RegisterStorageListener(IStorageListener *listener, bool *pbReady)
{
	AUTO_LOCK(mMutex);
	for (unsigned i = 0; i < ARRAY_SIZE(mpStorageListeners); i++) {
		if (mpStorageListeners[i] == NULL) {
			mpStorageListeners[i] = listener;
			*pbReady = mbStorageReady;
			return E_OK;
		}
	}
	AVF_LOGE("RegisterStorageListener: no entry");
	return E_NOENT;
}

avf_status_t CVDBManager::UnregisterStorageListener(IStorageListener *listener)
{
	AUTO_LOCK(mMutex);
	for (unsigned i = 0; i < ARRAY_SIZE(mpStorageListeners); i++) {
		if (mpStorageListeners[i] == listener) {
			mpStorageListeners[i] = NULL;
			return E_OK;
		}
	}
	AVF_LOGE("UnregisterStorageListener: not found");
	return E_NOENT;
}

void CVDBManager::NotifyStorageState(int is_ready)
{
	AUTO_LOCK(mMutex);
	mbStorageReady = is_ready;
	for (unsigned i = 0; i < ARRAY_SIZE(mpStorageListeners); i++) {
		if (mpStorageListeners[i]) {
			mpStorageListeners[i]->NotifyStorageState(is_ready);
		}
	}
}

avf_u64_t CVDBManager::GetAutoDelFree()
{
	return mpRegistry->ReadRegInt64(NAME_VDB_AUTODEL_FREE, IRegistry::GLOBAL, 0);
}

avf_u64_t CVDBManager::GetAutoDelMin()
{
	return mpRegistry->ReadRegInt64(NAME_VDB_AUTODEL_MIN, IRegistry::GLOBAL, 0);
}

// free_vdb_space: not used. space will be freed in StartRecord()
void CVDBManager::GetVdbInfo(vdb_info_s *info, int free_vdb_space)
{
	mpVDB->GetVdbSpaceInfo(info, GetAutoDelFree(), GetAutoDelMin());
}

// return false: no tf or no space
bool CVDBManager::CheckStorageSpace()
{
	bool not_ready;
	bool no_space;
	mpVDB->CheckSpace(ShouldAutoDelete(), &not_ready, &no_space,
		GetAutoDelFree(), GetAutoDelMin());
	return !(not_ready || no_space);
}

int CVDBManager::ParseClipAttr(const char *attr_str)
{
	const char *p = attr_str;
	int c;
	int clip_attr = 0;

	while ((c = *p++) != 0) {
		switch (c) {
		case 'a': clip_attr |= CLIP_ATTR_AUTO; break;
		case 'm': clip_attr |= CLIP_ATTR_MANUALLY; break;
		case 'n': clip_attr |= CLIP_ATTR_NO_AUTO_DELETE; break;
		case 'u': clip_attr |= CLIP_ATTR_NEED_UPLOAD; break;
		default: break;
		}
	}

	return clip_attr;
}

avf_status_t CVDBManager::SetRecordAttr(const char *attr_str)
{
	AVF_LOGI("SetRecordAttr " C_YELLOW "%s" C_NONE, attr_str);
	int clip_attr = ParseClipAttr(attr_str);
	return mpVDB->SetRecordAttr(clip_attr);
}

avf_status_t CVDBManager::MarkLiveClip(int delay_ms, int before_live_ms, int after_live_ms, int b_continue, int live_mark_flag, const char *attr_str)
{
	int clip_attr = ParseClipAttr(attr_str);
	if (live_mark_flag) clip_attr |= CLIP_ATTR_LIVE_MARK;
	return mpVDB->MarkLiveClip(delay_ms, before_live_ms, after_live_ms, b_continue, clip_attr, RECORD_CH);
}

avf_status_t CVDBManager::StopMarkLiveClip()
{
	return mpVDB->StopMarkLiveClip(RECORD_CH);
}

avf_status_t CVDBManager::GetMarkLiveInfo(mark_info_s& info)
{
	return mpVDB->GetMarkLiveInfo(info, RECORD_CH);
}

avf_status_t CVDBManager::EnablePictureService(const char *picture_path)
{
	CPictureList *pPictureList = mpVdbServer->CreatePictureList(picture_path);
	if (pPictureList) {
		pPictureList->SetPictureListener(1, this);
		return E_OK;
	}
	return E_ERROR;
}

// not called
avf_status_t CVDBManager::RefreshPictureList()
{
	vdb_man_cmd_t cmd(CMD_RefreshPictureList);
	PostCmd(cmd);
	return E_OK;
}

avf_status_t CVDBManager::PictureTaken(int code)
{
	vdb_man_cmd_t cmd(CMD_PictureTaken);
	cmd.u.PictureTaken.code = code;
	PostCmd(cmd);
	return E_OK;
}

avf_status_t CVDBManager::GetNumPictures(int *num)
{
	CPictureList *pPictureList = mpVdbServer->GetPictureList();
	if (pPictureList == NULL) {
		*num = 0;
		return E_OK;
	}
	return pPictureList->GetNumPictures(num);
}

avf_status_t CVDBManager::GetSpaceInfo(avf_u64_t *total_space, avf_u64_t *used_space,
	avf_u64_t *marked_space, avf_u64_t *clip_space)
{
	return mpVDB->GetSpaceInfo(total_space, used_space, marked_space, clip_space);
}

avf_status_t CVDBManager::SetClipSceneData(int clip_type, avf_u32_t clip_id, const avf_u8_t *data, avf_size_t data_size)
{
	return mpVDB->SetClipSceneData(clip_type, clip_id, data, data_size);
}

string_t *CVDBManager::CreateVideoFileName(avf_time_t *gen_time, int stream, const char *ext)
{
	return mpVDB->CreateVideoFileName(gen_time, stream, false, RECORD_CH, ext);
}

string_t *CVDBManager::CreatePictureFileName(avf_time_t *gen_time, int stream)
{
	return mpVDB->CreatePictureFileName(gen_time, stream, false, RECORD_CH);
}

int CVDBManager::GetSegmentLength()
{
	return mpVDB->GetSegmentLength();
}

avf_status_t CVDBManager::StartRecord(bool bEnableRecord)
{
	vdb_man_cmd_t cmd(CMD_StartRecord);
	cmd.u.StartRecord.bEnableRecord = bEnableRecord;
	return SendCmd(cmd);
}

void CVDBManager::StopRecord()
{
	vdb_man_cmd_t cmd(CMD_StopRecord);
	SendCmd(cmd);
}

void CVDBManager::InitVideoStream(const avf_stream_attr_s *stream_attr, int video_type, int stream)
{
	vdb_man_cmd_t cmd(CMD_InitVideoStream);
	cmd.u.InitVideoStream.stream = stream;
	cmd.u.InitVideoStream.video_type = video_type;
	cmd.u.InitVideoStream.stream_attr = *stream_attr;
	PostCmd(cmd);
}

void CVDBManager::PostIO(IAVIO *io, ITSMapInfo *map_info, int stream)
{
	vdb_man_cmd_t cmd(CMD_PostIO);
	cmd.u.PostIO.io = io;
	if (io) {
		io->AddRef();
	}
	cmd.u.PostIO.map_info = map_info;
	if (map_info) {
		map_info->AddRef();
	}
	cmd.u.PostIO.stream = stream;
	PostCmd(cmd);
}

void CVDBManager::StartSegment(avf_time_t video_time, avf_time_t picture_time, int stream)
{
	vdb_man_cmd_t cmd(CMD_StartSegment);
	cmd.u.StartSegment.video_time = video_time;
	cmd.u.StartSegment.picture_time = picture_time;
	cmd.u.StartSegment.stream = stream;
	PostCmd(cmd);
}

void CVDBManager::EndSegment(avf_u64_t next_start_ms, avf_u32_t fsize, int stream, bool bLast)
{
	vdb_man_cmd_t cmd(CMD_EndSegment);
	cmd.u.EndSegment.next_start_ms = next_start_ms;
	cmd.u.EndSegment.fsize = fsize;
	cmd.u.EndSegment.stream = stream;
	cmd.u.EndSegment.bLast = bLast;
	PostCmd(cmd);
}

void CVDBManager::AddVideo(avf_u64_t pts, avf_u32_t timescale, avf_u32_t fpos, int stream)
{
	vdb_man_cmd_t cmd(CMD_AddVideo);
	cmd.u.AddVideo.pts = pts;
	cmd.u.AddVideo.timescale = timescale;
	cmd.u.AddVideo.fpos = fpos;
	cmd.u.AddVideo.stream = stream;
	PostCmd(cmd);
}

void CVDBManager::AddPicture(CBuffer *pBuffer, int stream)
{
	vdb_man_cmd_t cmd(CMD_AddPicture);
	cmd.u.AddPicture.pBuffer = pBuffer->acquire();
	cmd.u.AddPicture.stream = stream;
	PostCmd(cmd);
}

void CVDBManager::AddRawData(raw_data_t *raw, int stream)
{
	if (raw->ts_ms < 10) {
		// for the first sample, if time_ms is little like 1, then set it to 0
		//AVF_LOGD("raw pts: %d", (int)pts_ms);
		raw->ts_ms = 0;
	}

	// broadcast
	mpVdbServer->SendRawData(NULL, raw);

	vdb_man_cmd_t cmd(CMD_AddRawData);
	cmd.u.AddRawData.raw = raw->acquire();
	cmd.u.AddRawData.stream = stream;
	PostCmd(cmd);
}

void CVDBManager::SetConfigData(raw_data_t *raw)
{
	vdb_man_cmd_t cmd(CMD_SetConfigData);
	cmd.u.SetConfigData.raw = raw->acquire();
	PostCmd(cmd);
}

void CVDBManager::AddRawDataEx(raw_data_t *raw)
{
	vdb_man_cmd_t cmd(CMD_AddRawDataEx);
	cmd.u.AddRawDataEx.raw = raw->acquire();
	PostCmd(cmd);
}

void CVDBManager::PostRawDataFreq(int interval_gps, int interval_acc, int interval_obd)
{
	vdb_man_cmd_t cmd(CMD_PostRawDataFreq);
	cmd.u.PostRawDataFreq.interval_gps = interval_gps;
	cmd.u.PostRawDataFreq.interval_acc = interval_acc;
	cmd.u.PostRawDataFreq.interval_obd = interval_obd;
	PostCmd(cmd);
}

void CVDBManager::HandleCmd(vdb_man_cmd_t& cmd)
{
	switch (cmd.cmd_code) {
	case CMD_MediaStateChanged: {
			avf_status_t status;
			if (cmd.u.MediaStateChanged.is_ready) {

				mpVDB->SetNoDelete(cmd.u.MediaStateChanged.no_delete);

				mpRegistry->ReadRegString(NAME_VDB_FS, IRegistry::GLOBAL, m_mount_point, VALUE_VDB_FS);

				if (!IsStorageReady()) {
					AVF_LOGW("storage is not ready");
				} else {
					char camera_dir[REG_STR_MAX];
					avf_snprintf(with_size(camera_dir), "%s/100TRANS/", m_mount_point);

					status = mpVDB->Load(m_mount_point, camera_dir, true, NULL);
					if (status == E_OK) {
						CPictureList *pPictureList = mpVdbServer->GetPictureList();
						if (pPictureList) {
							pPictureList->LoadPictureList();
						}
					}
					// notify storage listeners that storage is ready
					NotifyStorageState(1);
				}

				AppCallback(IMediaControl::APP_MSG_VDB_STATE_CHANGED);

			} else {

				CPictureList *pPictureList = mpVdbServer->GetPictureList();
				if (pPictureList) {
					pPictureList->UnloadPictureList();
				}
				status = mpVDB->Unload();
				// notify storage listeners that storage is unloaded
				NotifyStorageState(0);
				ReplyCmd(status);
			}

		}
		break;

	case CMD_StartRecord: {
			m_check_space_tick = avf_get_curr_tick();
			if (!cmd.u.StartRecord.bEnableRecord) {
				ReplyCmd(E_OK);
				break;
			}

			bool bAutoDel = ShouldAutoDelete();

			avf_u64_t autodel_free = GetAutoDelFree();
			avf_u64_t autodel_min = GetAutoDelMin();

			AVF_LOGI("auto_del_free: %d MB, autodel_min: %d MB",
				(uint32_t)(autodel_free >> 20), (uint32_t)(autodel_min >> 20));

			bool not_ready;
			bool no_space;

			mpVDB->CheckSpace(bAutoDel, &not_ready, &no_space, autodel_free, autodel_min);

			if (not_ready || no_space) {
				AVF_LOGE("no space to record");
				ReplyCmd(E_PERM);
				break;
			}

			avf_status_t status = mpVDB->StartRecord(NULL, RECORD_CH);
			ReplyCmd(status);
		}
		break;

	case CMD_StopRecord:
		mpVDB->StopRecord(NULL, RECORD_CH);
		ReplyCmd(E_OK);
		break;

	case CMD_InitVideoStream:
		mpVDB->InitVideoStream(&cmd.u.InitVideoStream.stream_attr,
			cmd.u.InitVideoStream.video_type,
			cmd.u.InitVideoStream.stream, RECORD_CH);
		mpVDB->InitClipDate(0, 0, RECORD_CH);
		break;

	case CMD_PostIO: {
			IAVIO *io = cmd.u.PostIO.io;
			ITSMapInfo *map_info = cmd.u.PostIO.map_info;
			mpVDB->SetLiveIO(io, map_info, cmd.u.PostIO.stream, RECORD_CH);
			avf_safe_release(io);
			avf_safe_release(map_info);
		}
		break;

	case CMD_StartSegment:
		mpVDB->StartVideo(cmd.u.StartSegment.video_time,
			cmd.u.StartSegment.stream, RECORD_CH);
		mpVDB->StartPicture(cmd.u.StartSegment.picture_time,
			cmd.u.StartSegment.stream, RECORD_CH);
		break;

	case CMD_EndSegment:
		mpVDB->FinishSegment(cmd.u.EndSegment.next_start_ms,
			cmd.u.EndSegment.fsize,
			cmd.u.EndSegment.stream,
			cmd.u.EndSegment.bLast, RECORD_CH);
		break;

	case CMD_AddVideo:
		mpVDB->AddVideo(
			cmd.u.AddVideo.pts,
			cmd.u.AddVideo.timescale,
			cmd.u.AddVideo.fpos,
			cmd.u.AddVideo.stream, RECORD_CH);
		break;

	case CMD_AddPicture: {
			avf_u64_t tick = avf_get_curr_tick();
			if (tick >= m_check_space_tick + 20*1000) {
				m_check_space_tick = tick;
				if (!CheckStorageSpace()) {
					AVF_LOGW("storage space is full");
					AppCallback(IMediaControl::APP_MSG_BUFFER_SPACE_FULL);
				}
			}
			mpVDB->AddPicture(
				cmd.u.AddPicture.pBuffer,
				cmd.u.AddPicture.stream, RECORD_CH);
			cmd.u.AddPicture.pBuffer->Release();
		}
		break;

	case CMD_AddRawData:
		mpVDB->AddRawData(
			cmd.u.AddRawData.raw,
			cmd.u.AddRawData.stream, RECORD_CH);
		cmd.u.AddRawData.raw->Release();
		break;

	case CMD_SetConfigData:
		mpVDB->SetConfigData(cmd.u.SetConfigData.raw, RECORD_CH);
		cmd.u.SetConfigData.raw->Release();
		break;

	case CMD_AddRawDataEx:
		mpVDB->AddRawDataEx(
			cmd.u.AddRawDataEx.raw, RECORD_CH);
		cmd.u.AddRawDataEx.raw->Release();
		break;

	case CMD_PostRawDataFreq:
		mpVDB->SetRawDataFreq(
			cmd.u.PostRawDataFreq.interval_gps,
			cmd.u.PostRawDataFreq.interval_acc,
			cmd.u.PostRawDataFreq.interval_obd, RECORD_CH);
		break;

	case CMD_RefreshPictureList: {
			CPictureList *pPictureList = mpVdbServer->GetPictureList();
			if (pPictureList) {
				pPictureList->ClearPictureList();
			}
		}
		break;

	case CMD_PictureTaken: {
			CPictureList *pPictureList = mpVdbServer->GetPictureList();
			if (pPictureList) {
				SendPictureTaken(pPictureList, cmd.u.PictureTaken.code);
			}
		}
		break;

	default:
		AVF_LOGW("cmd %d not handled", cmd.cmd_code);
		break;
	}
}

void CVDBManager::SendPictureTaken(CPictureList *pPictureList, int code)
{
	int dcf_cnt = (code >> 22) & 0x3ff;
	int next_cnt = (code >> 8) & 0x3fff;
//	int with_raw = (code >> 6) & 0x3;
	char dir[16];
	char name[16];

	sprintf(dir, "%dVDT02", dcf_cnt);
	sprintf(name, "DSC_%04d.jpg", next_cnt);

	pPictureList->PictureTaken(dir, name);
}

void CVDBManager::OnListScaned(int total)
{
	AVF_LOGD("OnListScanned");
	AppCallback(IMediaControl::APP_MSG_PICTURE_LIST_CHANGED);
}

void CVDBManager::OnListCleared()
{
	AVF_LOGD("OnListCleared");
	AppCallback(IMediaControl::APP_MSG_PICTURE_LIST_CHANGED);
}

void CVDBManager::OnPictureTaken(string_t *picname, avf_u32_t picture_date)
{
	AVF_LOGD("OnPictureTaken %s", picname->string());
	AppCallback(IMediaControl::APP_MSG_PICTURE_LIST_CHANGED);
}

void CVDBManager::OnPictureRemoved(string_t *picname)
{
	AVF_LOGD("OnPictureRemoved %s", picname->string());
	AppCallback(IMediaControl::APP_MSG_PICTURE_LIST_CHANGED);
}

