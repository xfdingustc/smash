
#define LOG_TAG	"vdb_local"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_config.h"
#include "avf_registry.h"
#include "mem_stream.h"
#include "avf_util.h"

#include "avio.h"
#include "sys_io.h"

#include "vdb_cmd.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_util.h"
#include "vdb_if.h"
#include "vdb.h"
#include "clip_reader_io.h"
#include "vdb_server.h"
#include "vdb_http_server.h"

#include "avf_remuxer_api.h"
#include "vdb_local.h"

struct vdb_local_s
{
	CVDB *pVdb;
	CMutex *mMutex;

	copy_clip_set_s *cclip_set;
	vdb_transfer_info_t transfer_info;
	CThread *m_thread;
	vdb_local_s *pDestVdb;
	int b_dest;
	CCondition *mCond;

	CVDB::vdb_ref_clip_t *ref_clips;
	avf_uint_t ref_clips_cap;
	avf_uint_t ref_clips_count;

	avf_remuxer_t *remuxer;
	CMutex *mMutexRemuxer;	// protecte remuxer_info
	vdb_remux_info_t remuxer_info;
};

struct vdb_local_server_s
{
	CVDBServer *pserver;
	local_vdb_set_t vdb_set;
	void *vdb_context;
};

struct vdb_local_http_server_s
{
	CVdbHttpServer *pserver;
	local_vdb_set_t vdb_set;
	void *vdb_context;
};


int vdb_local_unload_l(vdb_local_t *vdb);
int vdb_transfer_clear_l(vdb_local_t *vdb);

INLINE void init_ack(vdb_local_ack_t *ack)
{
	ack->data = NULL;
	ack->size = 0;
}

INLINE void gen_ack(int rval, CMemBuffer *pmb, vdb_local_ack_t *ack)
{
	if (rval >= 0) {
		ack->data = (avf_u8_t*)pmb->CollectData();
		ack->size = pmb->GetTotalSize();
	}
	pmb->Release();
}

INLINE void destroy_remuxer(vdb_local_t *vdb)
{
	if (vdb->remuxer) {
		//avf_remuxer_unregister_vdb(vdb->remuxer, static_cast<IVdbInfo*>(vdb->pVdb));
		avf_remuxer_destroy(vdb->remuxer);
		vdb->remuxer = NULL;
	}
}

extern "C" vdb_local_t *vdb_local_create(vdb_server_t *vdb_server)
{
	vdb_local_t *vdb = avf_malloc_type(vdb_local_t);
	if (vdb == NULL)
		return NULL;

	::memset(vdb, 0, sizeof(*vdb));

	if ((vdb->mMutex = avf_new CMutex()) == NULL)
		goto Error;

	if ((vdb->mCond = avf_new CCondition()) == NULL)
		goto Error;

	if ((vdb->mMutexRemuxer = avf_new CMutex()) == NULL)
		goto Error;

	if ((vdb->pVdb = CVDB::Create(vdb_server ? vdb_server->pserver : NULL, (void*)vdb, false)) == NULL)
		goto Error;

	if ((vdb->cclip_set = vdb->pVdb->CreateCopyClipSet()) == NULL)
		goto Error;

	return vdb;

Error:
	vdb_local_destroy(vdb);
	return NULL;
}

extern "C" int vdb_local_destroy(vdb_local_t *vdb)
{
	if (vdb == NULL)
		return 0;

	destroy_remuxer(vdb);

	if (vdb->pVdb) {
		vdb_local_unload_l(vdb);
		if (vdb->cclip_set) {
			vdb->pVdb->DestroyCopyClipSet(vdb->cclip_set);
		}
		vdb->pVdb->Release();
	}

	avf_safe_free(vdb->ref_clips);

	avf_safe_delete(vdb->mMutex);

	avf_safe_delete(vdb->mCond);

	avf_safe_delete(vdb->mMutexRemuxer);

	avf_free(vdb);

	return 0;
}

extern "C" int vdb_local_load(vdb_local_t *vdb, const char *vdb_root, int b_create, const char *id)
{
	return vdb->pVdb->Load(NULL, vdb_root, b_create, id);
}

int vdb_local_unload_l(vdb_local_t *vdb)
{
	destroy_remuxer(vdb);
	int rval = vdb_transfer_clear_l(vdb);
	if (rval < 0)
		return rval;
	return vdb->pVdb->Unload();
}

extern "C" int vdb_local_unload(vdb_local_t *vdb)
{
	AUTO_LOCK(*vdb->mMutex);
	return vdb_local_unload_l(vdb);
}

int vdb_local_get_space_info(vdb_local_t *vdb, uint64_t *total_space,
	uint64_t *used_space, uint64_t *marked_space, uint64_t *clip_space)
{
	return vdb->pVdb->GetSpaceInfo(total_space, used_space, marked_space, clip_space);
}

extern "C" void vdb_local_free_ack(vdb_local_t *vdb, vdb_local_ack_t *ack)
{
	if (ack->data) {
		CMemBuffer::ReleaseData(ack->data);
		ack->data = NULL;
	}
}

extern "C" int vdb_local_get_clip_set_info(vdb_local_t *vdb, int clip_type, int flags, vdb_local_ack_t *ack)
{
	init_ack(ack);

	CMemBuffer *pmb = CMemBuffer::Create(4*KB);
	if (pmb == NULL) {
		return -1;
	}

	bool bNoDelete;
	int rval = vdb->pVdb->CmdGetClipSetInfo(clip_type, flags, true, pmb, &bNoDelete);
	gen_ack(rval, pmb, ack);
	return rval;
}

extern "C" int vdb_local_get_clip_info(vdb_local_t *vdb, int clip_type, uint32_t clip_id, int flags, vdb_local_ack_t *ack)
{
	init_ack(ack);

	CMemBuffer *pmb = CMemBuffer::Create(512);
	if (pmb == NULL) {
		return -1;
	}

	int rval = vdb->pVdb->CmdGetClipInfo(clip_type, clip_id, flags, true, pmb);
	gen_ack(rval, pmb, ack);
	return rval;
}

extern "C" int vdb_transfer_get_result(vdb_local_t *vdb, vdb_local_ack_t *ack)
{
	AUTO_LOCK(*vdb->mMutex);

	init_ack(ack);

	if (vdb->pDestVdb == NULL) {
		AVF_LOGE("vdb_transfer_get_result: no dest vdb");
		return -1;
	}

	CMemBuffer *pmb = CMemBuffer::Create(1*KB);
	if (pmb == NULL) {
		return -1;
	}

	int rval = vdb->pDestVdb->pVdb->GetRefClipSetInfo(vdb->ref_clips, vdb->ref_clips_count,
		GET_CLIP_EXTRA, pmb);
	gen_ack(rval, pmb, ack);

	//avf_safe_free(vdb->ref_clips);
	//vdb->ref_clips_cap = 0;
	vdb->ref_clips_count = 0;

	return rval;
}

extern "C" int vdb_local_get_index_picture(vdb_local_t *vdb, int clip_type, uint32_t clip_id, 
	uint64_t clip_time_ms, vdb_local_ack_t *ack)
{
	init_ack(ack);

	CMemBuffer *pmb = CMemBuffer::Create(4*KB);
	if (pmb == NULL) {
		return -1;
	}

	vdb_cmd_GetIndexPicture_t cmd;
	::memset(&cmd, 0, sizeof(cmd));
	cmd.clip_type = clip_type;
	cmd.clip_id = clip_id;
	cmd.clip_time_ms_lo = U64_LO(clip_time_ms);
	cmd.clip_time_ms_hi = U64_HI(clip_time_ms);

	int rval = vdb->pVdb->CmdGetIndexPicture(&cmd, pmb, false, false);
	gen_ack(rval, pmb, ack);
	return rval;
}

extern "C" int vdb_local_get_raw_data(vdb_local_t *vdb, int clip_type,
	uint32_t clip_id, uint64_t clip_time_ms, int data_types, vdb_local_ack_t *ack)
{
	init_ack(ack);

	CMemBuffer *pmb = CMemBuffer::Create(512);
	if (pmb == NULL) {
		return -1;
	}

	vdb_cmd_GetRawData_t cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.clip_type = clip_type;
	cmd.clip_id = clip_id;
	cmd.clip_time_ms_lo = U64_LO(clip_time_ms);
	cmd.clip_time_ms_hi = U64_HI(clip_time_ms);
	cmd.data_types = data_types;

	int rval = vdb->pVdb->CmdGetRawData(&cmd, pmb);
	gen_ack(rval, pmb, ack);
	return rval;
}

int vdb_local_get_raw_data_size(vdb_local_t *vdb, int clip_type, uint32_t clip_id, 
	uint64_t clip_time_ms, uint32_t length_ms, uint32_t data_type, uint32_t *size)
{
/*
	*size = 0;

	vdb_cmd_GetRawDataSize_t cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.clip_type = clip_type;
	cmd.clip_id = clip_id;
	cmd.data_type = data_type;
	cmd.clip_time_ms_lo = U64_LO(clip_time_ms);
	cmd.clip_time_ms_hi = U64_HI(clip_time_ms);
	cmd.length_ms = length_ms;

	return vdb->pVdb->CmdGetRawDataSize(&cmd, size);
*/
	return -1;
}

extern "C" int vdb_local_get_raw_data_block(vdb_local_t *vdb, int clip_type,
	uint32_t clip_id, uint64_t clip_time_ms, uint32_t length_ms, uint32_t data_type, vdb_local_ack_t *ack)
{
	init_ack(ack);

	CMemBuffer *pmb = CMemBuffer::Create(4*1024);
	if (pmb == NULL) {
		return -1;
	}

	vdb_cmd_GetRawDataBlock_t cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.clip_type = clip_type;
	cmd.clip_id = clip_id;
	cmd.clip_time_ms_lo = U64_LO(clip_time_ms);
	cmd.clip_time_ms_hi = U64_HI(clip_time_ms);
	cmd.length_ms = length_ms;
	cmd.data_type = data_type;

	CMemBuffer *pData = CMemBuffer::Create(8*KB);

	int rval = vdb->pVdb->CmdGetRawDataBlock(&cmd, pmb, pData, 0);
	if (rval == E_OK) {
		rval = pmb->Append(pData);	// TODO
	}

	avf_safe_release(pData);

	gen_ack(rval, pmb, ack);
	return rval;
}

int vdb_local_get_playback_info(vdb_local_t *vdb,
	int clip_type, uint32_t clip_id, int stream, int url_type,
	uint64_t clip_time_ms, uint32_t length_ms,
	uint64_t *real_time_ms, uint32_t *real_length_ms)
{
	IVdbInfo::range_s range;
	IVdbInfo::clip_info_s clip_info;

	range.clip_type = clip_type;
	range.ref_clip_id = clip_id;
	range._stream = stream;
	range.b_playlist = 0;
	range.clip_time_ms = clip_time_ms;
	range.length_ms = length_ms;

	avf_status_t status = vdb->pVdb->GetRangeInfo(&range, &clip_info, NULL, 0);
	if (status != E_OK)
		return status;

	*real_time_ms = clip_info.clip_time_ms;
	*real_length_ms = clip_info._length_ms;

	return 0;
}

#define LOCAL_IP	"127.0.0.1"

extern "C" int vdb_local_get_playback_url_ex(const char *vdb_id,
	int clip_type, uint32_t clip_id, int stream, int url_type, 
	uint64_t clip_time_ms, uint32_t length_ms, uint32_t param1, uint32_t param2,
	char url[256])
{
	vdb_format_url(LOCAL_IP, url, vdb_id, clip_type, clip_id, stream,
		0, 0,	// upload_opt, b_playlist
		url_type, clip_time_ms, length_ms, param1, param2);
	return 0;
}

extern "C" int vdb_local_get_playback_url(const char *vdb_id,
	int clip_type, uint32_t clip_id, int stream, int url_type, 
	uint64_t clip_time_ms, uint32_t length_ms,
	char url[256])
{
	vdb_format_url(LOCAL_IP, url, vdb_id, clip_type, clip_id, stream, 0, 0, url_type,
		clip_time_ms, length_ms, 0, 0);
	return 0;
}

extern "C" int vdb_local_delete_clip(vdb_local_t *vdb, int clip_type, uint32_t clip_id)
{
	vdb_cmd_DeleteClip_t cmd;
	::memset(&cmd, 0, sizeof(cmd));
	cmd.clip_type = clip_type;
	cmd.clip_id = clip_id;

	avf_u32_t error = 0;
	int rval = vdb->pVdb->CmdDeleteClip(&cmd, &error);
	if (rval < 0)
		return rval;

	return error;
}

extern "C" int vdb_local_mark_clip(vdb_local_t *vdb, int clip_type, uint32_t clip_id,
	uint64_t clip_time_ms, uint32_t length_ms, int *p_clip_type, uint32_t *p_clip_id)
{
	vdb_cmd_MarkClip_t cmd;
	::memset(&cmd, 0, sizeof(cmd));
	cmd.clip_type = clip_type;
	cmd.clip_id = clip_id;
	cmd.start_time_ms_lo = U64_LO(clip_time_ms);
	cmd.start_time_ms_hi = U64_HI(clip_time_ms);
	uint64_t end_time_ms = clip_time_ms + length_ms;
	cmd.end_time_ms_lo = U64_LO(end_time_ms);
	cmd.end_time_ms_hi = U64_HI(end_time_ms);

	avf_u32_t error = 0;
	int rval = vdb->pVdb->CmdMarkClip(&cmd, &error, p_clip_type, p_clip_id);
	if (rval < 0)
		return rval;

	return error;
}

extern "C" int vdb_local_set_clip_scene_data(vdb_local_t *vdb, int clip_type, uint32_t clip_id,
	const uint8_t *data, uint32_t data_size)
{
	return vdb->pVdb->SetClipSceneData(clip_type, clip_id, data, data_size);
}

// ---------------------------------------------------------------
//
//	clip transfer APIs
//
// ---------------------------------------------------------------

void vdb_local_set_dest(vdb_local_t *vdb, int b_dest)
{
	AUTO_LOCK(*vdb->mMutex);
	vdb->b_dest = b_dest;
}

int vdb_transfer_init(vdb_local_t *vdb, const vdb_local_item_t *items, int nitems)
{
	AUTO_LOCK(*vdb->mMutex);

	if (vdb->m_thread != NULL) {
		AVF_LOGE("vdb_transfer_init: not finished");
		return E_PERM;
	}

	vdb->cclip_set->Clear();

	int rval = vdb->pVdb->InitTransfer(vdb->cclip_set, items, nitems, &vdb->transfer_info.total);
	::memset(&vdb->transfer_info.transfered, 0, sizeof(vdb->transfer_info.transfered));
	vdb->transfer_info.b_finished = 0;

	return rval;
}

void vdb_local_finish_transfer(vdb_local_t *vdb)
{
	vdb_local_set_dest(vdb->pDestVdb, 0);

	AUTO_LOCK(*vdb->mMutex);
	vdb->transfer_info.b_finished = 1;

	vdb->m_thread->Release();
	vdb->m_thread = NULL;

	vdb->mCond->Wakeup();
}

void _vdb_transfer_entry(void *param)
{
	vdb_local_t *vdb = (vdb_local_t*)param;
	vdb->pVdb->TransferClipSet(vdb->cclip_set, vdb->pDestVdb->pVdb);
	vdb_local_finish_transfer(vdb);

	if (!vdb->cclip_set->b_cancel) {
		// TODO: callback
	}
}

int vdb_transfer_callback(void *context, int event, avf_intptr_t param)
{
	vdb_local_t *vdb = (vdb_local_t*)context;
	AUTO_LOCK(*vdb->mMutex);

	switch (event) {
	case COPY_EVENT_VIDEO_COPIED:
		vdb->transfer_info.transfered.video_size += (avf_uint_t)param;
		break;
	case COPY_EVENT_PICTURE_COPIED:
		vdb->transfer_info.transfered.picture_size += (avf_uint_t)param;
		break;
	case COPY_EVENT_INDEX_COPIED:
		vdb->transfer_info.transfered.index_size += (avf_uint_t)param;
		break;
	case COPY_EVENT_CLIP_TRANSFERED: {
			copy_clip_s *cclip = (copy_clip_s*)param;
			for (avf_uint_t i = 0; i < cclip->count; i++) {
				copy_ref_s *ref = cclip->elems + i;
				AVF_LOGD("ref clip transfered: %d of %d, clip_type: %d, clip_id: 0x%x",
					i, cclip->count, ref->clip_type, ref->clip_id);

				if (vdb->ref_clips_count == vdb->ref_clips_cap) {
					avf_uint_t new_cap = vdb->ref_clips_cap + 16;
					CVDB::vdb_ref_clip_t *new_ref_clips = avf_malloc_array(CVDB::vdb_ref_clip_t, new_cap);
					if (new_ref_clips == NULL) {
						// TODO:
					}
					if (vdb->ref_clips) {
						::memcpy(new_ref_clips, vdb->ref_clips, vdb->ref_clips_count * sizeof(CVDB::vdb_ref_clip_t));
						avf_free(vdb->ref_clips);
					}
					vdb->ref_clips = new_ref_clips;
					vdb->ref_clips_cap = new_cap;
				}

				CVDB::vdb_ref_clip_t *ref_clip = vdb->ref_clips + vdb->ref_clips_count;
				vdb->ref_clips_count++;

				ref_clip->clip_type = ref->clip_type;
				ref_clip->clip_id = ref->clip_id;
			}
			AVF_LOGD("COPY_EVENT_CLIP_TRANSFERED, total ref clips: %d", vdb->ref_clips_count);
		}
		break;
	}

	return 0;
}

int vdb_transfer_start(vdb_local_t *vdb, vdb_local_t *dest_vdb)
{
	AUTO_LOCK(*vdb->mMutex);

	if (vdb->m_thread != NULL) {
		AVF_LOGE("vdb_transfer_start: in progress");
		return E_PERM;
	}

	vdb->cclip_set->callback = vdb_transfer_callback;
	vdb->cclip_set->context = (void*)vdb;
	vdb->cclip_set->b_cancel = 0;

	vdb->transfer_info.b_finished = 0;

	vdb_local_set_dest(dest_vdb, 1);

	vdb->pDestVdb = dest_vdb;
	vdb->m_thread = CThread::Create("vdb_transfer", _vdb_transfer_entry, vdb, CThread::DETACHED);
	if (vdb->m_thread == NULL) {
		AVF_LOGE("vdb_transfer_start: cannot create thread");
		vdb_local_set_dest(dest_vdb, 0);
		return E_NOMEM;
	}

	return 0;
}

int vdb_transfer_clear_l(vdb_local_t *vdb)
{
	if (vdb->b_dest) {
		AVF_LOGE("cannot cancel dest vdb, should cancel source vdb");
		return E_ERROR;
	}

	while (vdb->m_thread) {
		vdb->cclip_set->b_cancel = 1;
		vdb->mCond->Wait(*vdb->mMutex);
	}

	vdb->cclip_set->Clear();

	return 0;
}

int vdb_transfer_cancel(vdb_local_t *vdb)
{
	AUTO_LOCK(*vdb->mMutex);
	return vdb_transfer_clear_l(vdb);
}

int vdb_transfer_get_info(vdb_local_t *vdb, vdb_transfer_info_t *info)
{
	AUTO_LOCK(*vdb->mMutex);
	*info = vdb->transfer_info;
	return 0;
}

// ---------------------------------------------------------------
//
//	remux APIs
//
// ---------------------------------------------------------------

static void remuxer_callback(void *context, int event, int arg1, int arg2)
{
	vdb_local_t *vdb = (vdb_local_t*)context;
	AUTO_LOCK(*vdb->mMutexRemuxer);

	switch (event) {
	case AVF_REMUXER_FINISHED:
		AVF_LOGD("AVF_REMUXER_FINISHED");
		vdb->remuxer_info.b_running = 0;
		vdb->remuxer_info.error = 0;
		break;

	case AVF_REMUXER_ERROR:
		AVF_LOGD("AVF_REMUXER_ERROR");
		vdb->remuxer_info.b_running = 0;
		vdb->remuxer_info.error = arg1;
		break;

	case AVF_REMUXER_PROGRESS:
		vdb->remuxer_info.percent = arg1;
		break;

	default:
		break;
	}
}

static void remuxer_set_running(vdb_local_t *vdb, int b_running)
{
	AUTO_LOCK(*vdb->mMutexRemuxer);
	vdb->remuxer_info.b_running = b_running;
	if (b_running) {
		vdb->remuxer_info.percent = 0;
		vdb->remuxer_info.error = 0;
	}
}

static int create_remuxer_l(vdb_local_t *vdb)
{
	destroy_remuxer(vdb);

	vdb->remuxer = avf_remuxer_create(remuxer_callback, (void*)vdb);
	if (vdb->remuxer == NULL) {
		AVF_LOGE("cannot create remuxer");
		return E_ERROR;
	}

	avf_remuxer_register_vdb(vdb->remuxer, static_cast<IVdbInfo*>(vdb->pVdb));

	return E_OK;
}

int vdb_remux_set_audio(vdb_local_t *vdb, int bDisableAudio,
	const char *pAudioFilename, const char *format)
{
	AUTO_LOCK(*vdb->mMutex);

	int rval = create_remuxer_l(vdb);
	if (rval < 0)
		return rval;

	avf_remuxer_set_audio(vdb->remuxer, bDisableAudio, pAudioFilename, format);
	return 0;
}

int vdb_remux_get_clip_size(vdb_local_t *vdb, const vdb_local_item_t *item,	
	int stream, uint64_t *size_bytes)
{
	IVdbInfo::range_s range;
	CVDB::clip_info_s info;
	avf_status_t status;

	range.clip_type = item->clip_type;
	range.ref_clip_id = item->clip_id;
	range._stream = stream;
	range.b_playlist = 0;
	range.clip_time_ms = item->start_time_ms;
	range.length_ms = (avf_u32_t)(item->end_time_ms - item->start_time_ms);

	status = vdb->pVdb->GetRangeInfo(&range, &info, NULL, 0);
	if (status != E_OK) {
		*size_bytes = 0;
		return status;
	}

	*size_bytes = info.v_size[0];
	return 0;
}

int vdb_remux_clip(vdb_local_t *vdb, const vdb_local_item_t *item,
	int stream, const char *output_filename, const char *output_format)
{
	AUTO_LOCK(*vdb->mMutex);

	int rval = create_remuxer_l(vdb);
	if (rval < 0)
		return rval;

	int duration = (avf_u32_t)(item->end_time_ms - item->start_time_ms);

	char url[200];
	CClipReaderIO::CreateUrl(with_size(url), item->clip_type, item->clip_id, stream,
		item->start_time_ms, duration);

	::strcat(url, ",0,-1;");

	remuxer_set_running(vdb, 1);
	rval = avf_remuxer_run(vdb->remuxer, url, "ts", output_filename, output_format,
		duration);
	if (rval < 0) {
		AVF_LOGE("avf_remuxer_run failed, rval = %d", rval);
		remuxer_set_running(vdb, 0);
		return rval;
	}

	return 0;
}

int vdb_remux_stop(vdb_local_t *vdb, int b_delete_output_file)
{
	AUTO_LOCK(*vdb->mMutex);
	char *filename = NULL;
	if (b_delete_output_file && vdb->remuxer) {
		const char *tmp_filename = avf_remuxer_get_output_filename(vdb->remuxer);
		if (tmp_filename != NULL) {
			filename = avf_strdup(tmp_filename);
		}
	}
	destroy_remuxer(vdb);
	if (filename != NULL) {
		AVF_LOGD("remove %s", filename);
		avf_remove_file(filename, true);
		avf_free(filename);
	}
	return 0;
}

// ignore errors
int vdb_remux_get_info(vdb_local_t *vdb, vdb_remux_info_t *info)
{
	{
		AUTO_LOCK(*vdb->mMutexRemuxer);
		*info = vdb->remuxer_info;
		info->total_bytes = 0;
		info->remain_bytes = 0;
	}

	{
		AUTO_LOCK(*vdb->mMutex);
		if (vdb->remuxer) {
			avf_remuxer_get_progress(vdb->remuxer, &info->total_bytes, &info->remain_bytes);
		}
	}

	return E_OK;
}

// ---------------------------------------------------------------
//
//	server APIs
//
// ---------------------------------------------------------------

static CVDB *vdb_server_get_vdb(void *context, const char *vdb_id)
{
	vdb_server_t *server = (vdb_server_t*)context;
	vdb_local_t *vdb = server->vdb_set.get_vdb(server->vdb_context, vdb_id);
	return vdb == NULL ? NULL : vdb->pVdb;
}

static void vdb_server_release_vdb(void *context, const char *vdb_id, CVDB *pvdb)
{
	vdb_server_t *server = (vdb_server_t*)context;
	vdb_local_t *vdb = (vdb_local_t*)pvdb->GetOwner();
	server->vdb_set.release_vdb(server->vdb_context, vdb_id, vdb);
}

static int vdb_server_get_all_clips(void *context, uint32_t *p_num_clips, vdb_clip_desc_t **p_clips)
{
	vdb_server_t *server = (vdb_server_t*)context;
	return server->vdb_set.get_all_clips == NULL ? -1 :
		server->vdb_set.get_all_clips(server->vdb_context, p_num_clips, p_clips);
}

static void vdb_server_release_clips(void *context, uint32_t num_clips, vdb_clip_desc_t *clips)
{
	vdb_server_t *server = (vdb_server_t*)context;
	if (server->vdb_set.release_clips) {
		server->vdb_set.release_clips(server->vdb_context, num_clips, clips);
	}
}

int vdb_server_get_clip_poster(void *context, const char *vdb_id,
	int clip_type, uint32_t clip_id, uint32_t *p_size, void **p_data)
{
	vdb_server_t *server = (vdb_server_t*)context;
	if (server->vdb_set.get_clip_poster) {
		if (server->vdb_set.get_clip_poster(server->vdb_context, vdb_id,
			clip_type, clip_id, p_size, p_data) == 0)
			return E_OK;
	}
	return E_ERROR;
}

void vdb_server_release_clip_poster(void *context, const char *vdb_id, uint32_t size, void *data)
{
	vdb_server_t *server = (vdb_server_t*)context;
	if (server->vdb_set.release_clip_poster) {
		server->vdb_set.release_clip_poster(server->vdb_context, vdb_id, size, data);
	}
}

void vdb_server_on_clip_created(void *context, const char *vdb_id, int clip_type, uint32_t clip_id)
{
	vdb_server_t *server = (vdb_server_t*)context;
	if (server->vdb_set.on_clip_created) {
		server->vdb_set.on_clip_created(server->vdb_context, vdb_id, clip_type, clip_id);
	}
}

void vdb_server_on_clip_deleted(void *context, const char *vdb_id, int clip_type, uint32_t clip_id)
{
	vdb_server_t *server = (vdb_server_t*)context;
	if (server->vdb_set.on_clip_deleted) {
		server->vdb_set.on_clip_deleted(server->vdb_context, vdb_id, clip_type, clip_id);
	}
}

static const vdb_set_s g_vdb_set = {
	// 1
	vdb_server_get_vdb,
	vdb_server_release_vdb,
	// 2
	vdb_server_get_all_clips,
	vdb_server_release_clips,
	// 3
	vdb_server_get_clip_poster,
	vdb_server_release_clip_poster,
	// 4
	vdb_server_on_clip_created,
	vdb_server_on_clip_deleted,
};

vdb_server_t *vdb_create_server(const local_vdb_set_t *vdb_set, void *vdb_context)
{
	vdb_server_t *result = avf_malloc_type(vdb_server_t);
	if (result != NULL) {
		result->vdb_set = *vdb_set;
		result->vdb_context = vdb_context;
		result->pserver = CVDBServer::Create(&g_vdb_set, (void*)result);
		if (result->pserver == NULL) {
			avf_free(result);
			return NULL;
		}
	}
	return result;
}

int vdb_destroy_server(vdb_server_t *server)
{
	if (server != NULL) {
		avf_safe_release(server->pserver);
		avf_free(server);
	}
	return 0;
}

int vdb_run_server(vdb_server_t *server)
{
	return server->pserver->Start();
}

static CVDB *vdb_http_server_get_vdb(void *context, const char *vdb_id)
{
	vdb_http_server_t *server = (vdb_http_server_t*)context;
	vdb_local_t *vdb = server->vdb_set.get_vdb(server->vdb_context, vdb_id);
	return vdb == NULL ? NULL : vdb->pVdb;
}

static void vdb_http_server_release_vdb(void *context, const char *vdb_id, CVDB *pvdb)
{
	vdb_http_server_t *server = (vdb_http_server_t*)context;
	vdb_local_t *vdb = (vdb_local_t*)pvdb->GetOwner();
	server->vdb_set.release_vdb(server->vdb_context, vdb_id, vdb);
}

static const vdb_set_s g_http_vdb_set = {
	vdb_http_server_get_vdb,
	vdb_http_server_release_vdb,
};

vdb_http_server_t *vdb_create_http_server(const local_vdb_set_t *vdb_set, void *vdb_context)
{
	vdb_http_server_t *result = avf_malloc_type(vdb_http_server_t);
	if (result != NULL) {
		result->vdb_set = *vdb_set;
		result->vdb_context = vdb_context;
		result->pserver = CVdbHttpServer::Create(&g_http_vdb_set, (void*)result);
		if (result->pserver == NULL) {
			avf_free(result);
			result = NULL;
		}
	}
	return result;
}

int vdb_destroy_http_server(vdb_http_server_t *server)
{
	if (server != NULL) {
		avf_safe_release(server->pserver);
		avf_free(server);
	}
	return 0;
}

int vdb_run_http_server(vdb_http_server_t *server)
{
	return server->pserver->Start();
}

// ---------------------------------------------------------------
//
//	read raw data APIs
//
// ---------------------------------------------------------------

vdb_local_clip_reader_t vdb_local_create_clip_reader(vdb_local_t *vdb,
	const vdb_local_item_t *item)
{
	IVdbInfo::range_s range = {0};

	range.clip_type = item->clip_type;
	range.ref_clip_id = item->clip_id;
	range._stream = 0;	// TODO:
	range.b_playlist = 0;
	range.clip_time_ms = item->start_time_ms;
	range.length_ms = (avf_u32_t)(item->end_time_ms - item->start_time_ms);

	IClipData *data = vdb->pVdb->CreateClipDataOfTime(&range);
	return (void*)data;
}

void vdb_local_destroy_clip_reader(vdb_local_t *vdb, vdb_local_clip_reader_t reader)
{
	IClipData *data = (IClipData*)reader;
	avf_safe_release(data);
}

int vdb_local_read_clip(vdb_local_clip_reader_t reader, int data_type, int stream, const char *filename)
{
	IClipData *data = (IClipData*)reader;

	IClipData::desc_s desc = {0};
	desc.data_type = data_type;
	desc.stream = stream;

	return data->ReadClipDataToFile(&desc, filename);
}

int vdb_local_read_clip_raw_data(vdb_local_clip_reader_t reader, int data_types, void **pdata, uint32_t *data_size)
{
	IClipData *data = (IClipData*)reader;
	return data->ReadClipRawData(data_types, pdata, data_size);
}

int vdb_local_free_clip_raw_data(vdb_local_clip_reader_t reader, void *_data)
{
	IClipData *data = (IClipData*)reader;
	data->ReleaseData(_data);
	return 0;
}

// ---------------------------------------------------------------
//
//	clip reader for upload
//
// ---------------------------------------------------------------
vdb_local_reader_t vdb_local_create_reader(vdb_local_t *vdb,
	const vdb_local_item_t *item, int b_mute_audio, int upload_opt)
{
	IVdbInfo::range_s range = {0};

	range.clip_type = item->clip_type;
	range.ref_clip_id = item->clip_id;
	range._stream = 0;
	range.b_playlist = 0;
	range.clip_time_ms = item->start_time_ms;
	range.length_ms = (avf_u32_t)(item->end_time_ms - item->start_time_ms);

	IClipReader *reader = vdb->pVdb->CreateClipReaderOfTime(&range, b_mute_audio, false, 0, upload_opt);
	return (void*)reader;
}

void vdb_local_destroy_reader(vdb_local_t *vdb, vdb_local_reader_t _reader)
{
	IClipReader *reader = (IClipReader*)_reader;
	avf_safe_release(reader);
}

int vdb_local_read(vdb_local_reader_t _reader, uint8_t *buffer, int len)
{
	IClipReader *reader = (IClipReader*)_reader;
	return reader->ReadClip(buffer, len);
}

int vdb_local_get_reader_info(vdb_local_reader_t _reader, vdb_local_reader_info_t *info)
{
	IClipReader *reader = (IClipReader*)_reader;
	info->clip_time_ms = reader->GetStartTimeMs();
	info->length_ms = reader->GetLengthMs();
	info->size = reader->GetSize();
	info->pos = reader->GetPos();
	return 0;
}

// ---------------------------------------------------------------
//
//	debug APIs
//
// ---------------------------------------------------------------

int vdb_local_clear_cache(vdb_local_t *vdb)
{
	return vdb->pVdb->ReleaseAllCaches();
}

extern "C" void vdb_local_enable_logs(const char *logs)
{
	avf_set_logs(logs);
}

void set_print_proc(void (*p)(const char*))
{
#if defined(WIN32_OS) || defined(MAC_OS)
	avf_print_proc = p;
#endif
}

