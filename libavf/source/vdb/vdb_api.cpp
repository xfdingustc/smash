
#define LOG_TAG "vdb_api"

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
#include "mem_stream.h"

#include "avio.h"
#include "file_io.h"
#include "buffer_io.h"

#include "vdb_cmd.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_util.h"
#include "vdb_if.h"
#include "vdb.h"
#include "vdb_server.h"
#include "vdb_cmd.h"
#include "vdb_api.h"

static CMutex g_vdb_lock;
static struct rb_root g_vdb_tree;

//#define VDB_API_V1
#define VDB_API_V2

#define GET_CAMID(_camid, _vdb) \
	::memcpy(_camid, _vdb->m_camera_id, CAMERA_ID_SZ)

#ifdef VDB_API_V1
#include "vdb_api_v1.cpp"
#endif

#ifdef VDB_API_V2
#include "vdb_api_v2.cpp"
#endif

// ch == 0
extern "C" int VDB_StartRecord(VDB *vdb, int has_video, int has_picture)
{
	return VDB_StartRecordEx(vdb, has_video, has_picture, 0);
}

// ch == 0
extern "C" int VDB_StopRecord(VDB *vdb)
{
	return VDB_StopRecordEx(vdb, 0);
}

// ch == 0
extern "C" int VDB_AddData(VDB *vdb, const uint8_t *data, int size)
{
	return VDB_AddDataEx(vdb, data, size, 0);
}

extern "C" int VDB_GetDataSize(VDB *vdb, uint64_t *video_size, uint64_t *picture_size, uint64_t *raw_data_size)
{
	vdb->pVDB->GetDataSize(video_size, picture_size, raw_data_size);
	return 0;
}

extern "C" int VDB_GetTotalClips(VDB *vdb)
{
	return vdb->pVDB->s_GetNumClips();
}

extern "C" clip_list_s *VDB_GetAllClipsInfo(VDB *vdb)
{
	return vdb->pVDB->s_GetAllClips();
}

int VDB_RemoveClip(VDB *vdb, vdb_clip_id_t clip_id)
{
	if (vdb->pVDB->s_RemoveClip(clip_id) != E_OK) {
		return -1;
	}

	if (vdb->m_cb) {
		VDB_ClipRemovedMsg_t msg;
		GET_CAMID(msg.camera_id, vdb);
		msg.clip_id = clip_id;
		vdb->m_cb(vdb->m_ctx, VDB_CB_ClipRemoved, &msg);
	}

	return 0;
}

int VDB_GetClipInfo(VDB *vdb, vdb_clip_id_t clip_id, vdbs_clip_info_t *info)
{
	return vdb->pVDB->s_GetClipInfo(clip_id, *info);
}

int VDB_GetIndexPicture(VDB *vdb, vdb_clip_id_t clip_id, void **pdata, int *psize)
{
	return vdb->pVDB->s_GetIndexPicture(clip_id, pdata, psize);
}

int VDB_GetIndexPictureAtPos(VDB *vdb, vdb_clip_id_t clip_id, uint64_t clip_time_ms,
	void **pdata, int *psize, uint64_t *clip_time_ms_out)
{
	return vdb->pVDB->s_GetIndexPictureAtPos(clip_id, clip_time_ms, pdata, psize, clip_time_ms_out);
}

void VDB_FreeIndexPicture(void *data)
{
	avf_safe_free(data);
}

int VDB_EnumRawData(VDB *vdb, vdb_enum_raw_data_proc proc, void *ctx,
	vdb_clip_id_t clip_id, int data_type, uint64_t clip_time_ms, uint32_t length_ms)
{
	return vdb->pVDB->s_EnumRawData(proc, ctx, clip_id, data_type, clip_time_ms, length_ms);
}

typedef struct raw_cb_pmb_s {
	CMemBuffer *pmb_index;
	CMemBuffer *pmb_data;
	int num_entries;
	avf_u64_t clip_start_time_ms;
} raw_cb_pmb_t;

// seg_stms: segment start_time_ms in clip
static int vdb_raw_data_cb(void *ctx, uint64_t seg_stms, int num_items,
	const raw_dentry_t *dentries, const void *data, uint32_t data_size)
{
	raw_cb_pmb_t *param = (raw_cb_pmb_t*)ctx;
/*
	if (param->num_entries == 0) {
		// for the first segment
		param->clip_start_time_ms = seg_stms;
		if (num_items > 0) {
			param->clip_start_time_ms += dentries->time_ms;
		}
	}
*/
	param->num_entries += num_items;

	uint32_t seg_start_offset = seg_stms - param->clip_start_time_ms;
	uint32_t data_off = param->pmb_data->GetTotalSize() - dentries->fpos;

	raw_dentry_t *dentries_w = (raw_dentry_t*)param->pmb_index->Alloc(num_items * sizeof(raw_dentry_t));
	void *data_w = (void*)param->pmb_data->Alloc(data_size - dentries->fpos);

	const raw_dentry_t *dentries_r = dentries;
	for (int i = num_items; i > 0; i--, dentries_w++, dentries_r++) {
		dentries_w->time_ms = dentries_r->time_ms + seg_start_offset;
		dentries_w->fpos = dentries_r->fpos + data_off;
	}

	::memcpy(data_w, (uint8_t*)data + dentries->fpos, data_size - dentries->fpos);

	return 1; // continue
}

// read raw data in [clip_time_ms, clip_time_ms+length_ms)
int VDB_ReadRawData(VDB *vdb, vdb_clip_id_t clip_id, int data_type, 
	uint64_t clip_time_ms, uint32_t length_ms, vdb_raw_data_t *result)
{
	raw_cb_pmb_t param = {NULL, NULL, 0, 0};
	avf_status_t status;

	::memset(result, 0, sizeof(vdb_raw_data_t));

	param.pmb_index = CMemBuffer::Create(1*KB);	// index block
	param.pmb_data = CMemBuffer::Create(4*KB);	// data block
	param.clip_start_time_ms = clip_time_ms;

	if (param.pmb_index == NULL || param.pmb_data == NULL) {
		status = E_NOMEM;
		goto Done;
	}

	status = vdb->pVDB->s_EnumRawData(vdb_raw_data_cb, (void*)&param,
		clip_id, data_type, clip_time_ms, length_ms);
	if (status != E_OK) {
		AVF_LOGW("s_EnumRawData failed %d", status);
	}

	if (param.num_entries > 0) {
		result->clip_start_time_ms = param.clip_start_time_ms;
		result->num_entries = param.num_entries;
		result->dentries = (raw_dentry_t*)param.pmb_index->CollectData();
		result->data = (void*)param.pmb_data->CollectData();
		result->data_size = param.pmb_data->GetTotalSize();
	}

	status = E_OK;

Done:
	avf_safe_release(param.pmb_index);
	avf_safe_release(param.pmb_data);
	return status;
}

void VDB_FreeRawData(vdb_raw_data_t *result)
{
	avf_safe_free(result->dentries);
	avf_safe_free(result->data);
}

int VDB_GetPlaybackUrl(VDB *vdb, vdb_clip_id_t clip_id, uint64_t clip_time_ms, char address[])
{
	vdb_format_url_clips(address, clip_id, 0, clip_time_ms, 0, 0, 0, 0);
	return 0;
}

int VDB_GetPlaybackUrlEx(VDB *vdb, vdb_clip_id_t clip_id, uint64_t clip_time_ms,
	int stream, int b_mute_audio, uint32_t param1, uint32_t param2,
	char address[200])
{
	vdb_format_url_clips(address, clip_id, stream, clip_time_ms, 0,
		b_mute_audio, param1, param2);
	return 0;
}

int VDB_ReadHLS(VDB *vdb, const char *address, void **pdata, int *psize)
{
	vdb_clip_id_t clip_id;
	int stream;
	int url_type;
	uint64_t clip_time_ms;
	uint32_t length_ms;
	int b_mute_audio;
	int b_full_path;
	int b_spec_time;
	uint32_t param1;
	uint32_t param2;

	if (vdb_parse_url_clips(address, &clip_id, &stream, &url_type,
			&clip_time_ms, &length_ms, &b_mute_audio, &b_full_path, &b_spec_time, &param1, &param2) == NULL) {
		return -1;
	}

	if (url_type != URL_TYPE_HLS) {
		return -1;
	}

	CMemBuffer *pmb = CMemBuffer::Create(4*KB);
	if (pmb == NULL)
		return -1;

	avf_status_t status = vdb->pVDB->s_ReadHLS(NULL, clip_id, clip_time_ms, pmb, stream, param1);
	if (status != E_OK)
		return status;

	*pdata = pmb->CollectData(1);
	*psize = pmb->GetTotalSize();

	// make 0 trailing
	char *ptr = (char*)(*pdata);
	ptr[*psize] = 0;

	pmb->Release();

	return 0;
}

void VDB_ReleaseHLSData(VDB *vdb, void *data)
{
	CMemBuffer::ReleaseData(data);
}

// address = ::sprintf(url, "" LLD "-%d-%d%s",
//	clip_time_ms, length_ms, ts_off_ms, VIDEO_FILE_TS_EXT);
// clips/[vdb_clip_id]/[stream]/
vdb_clip_reader_t VDB_CreateClipReader(VDB *vdb, const char *url, uint64_t start_offset)
{
	vdb_clip_id_t clip_id;
	int stream;
	int url_type;
	uint64_t clip_time_ms;
	uint32_t length_ms;
	int b_mute_audio;
	int b_full_path;
	int b_spec_time;
	uint32_t param1;
	uint32_t param2;

	if (vdb_parse_url_clips(url, &clip_id, &stream, &url_type,
			&clip_time_ms, &length_ms, &b_mute_audio, &b_full_path, &b_spec_time, &param1, &param2) == NULL) {
		return NULL;
	}

	if (url_type != URL_TYPE_TS) {
		return NULL;
	}

	IClipReader *reader = vdb->pVDB->s_CreateClipReader(
		clip_id, stream, clip_time_ms, length_ms, start_offset);

	return (vdb_clip_reader_t)reader;
}

void VDB_ReleaseClipReader(VDB *vdb, vdb_clip_reader_t _reader)
{
	IClipReader *reader = (IClipReader*)_reader;
	reader->Release();
}

int VDB_ClipReader_ReadClip(vdb_clip_reader_t _reader, uint8_t *buffer, int size)
{
	IClipReader *reader = (IClipReader*)_reader;
	return reader->ReadClip(buffer, size);
}

uint64_t VDB_ClipReader_GetSize(vdb_clip_reader_t _reader)
{
	IClipReader *reader = (IClipReader*)_reader;
	return reader->GetSize();
}

uint64_t VDB_ClipReader_GetPos(vdb_clip_reader_t _reader)
{
	IClipReader *reader = (IClipReader*)_reader;
	return reader->GetPos();
}

int VDB_ClipReader_Seek(vdb_clip_reader_t _reader, int64_t pos, int whence)
{
	IClipReader *reader = (IClipReader*)_reader;
	return reader->Seek(pos, whence);
}

int VDB_DumpData(VDB *vdb, const char *filename)
{
	CMemBuffer *pmb = CMemBuffer::Create(1024);
	vdb->pVDB->Serialize(pmb);

	IAVIO *io = CFileIO::Create();
	avf_status_t status = io->CreateFile(filename);
	if (status != E_OK) {
		perror("filename");
		io->Release();
		return status;
	}

	CMemBuffer::block_t *block = pmb->GetFirstBlock();
	for (; block; block = block->GetNext()) {
		io->Write(block->GetMem(), block->GetSize());
	}

	io->Release();
	return E_OK;
}

void VDB_EnableLogs(const char *logs)
{
	avf_set_logs(logs);
}

