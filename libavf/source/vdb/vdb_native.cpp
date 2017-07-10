
#define LOG_TAG	"vdb_native"

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

#include "vdb_cmd.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_util.h"
#include "vdb.h"

#include "vdb_native.h"

static CMutex g_vdb_native_lock;
static CVDB *g_vdb;

CVDB* get_vdb(void)
{
	AUTO_LOCK(g_vdb_native_lock);
	if (g_vdb != NULL) {
		g_vdb->AddRef();
	} else {
		g_vdb = CVDB::Create(NULL, NULL, false);
	}
	return g_vdb;
}

void free_vdb(CVDB *pVdb)
{
	AUTO_LOCK(g_vdb_native_lock);
	if (pVdb == g_vdb) {
		if (pVdb->_GetRefCount() == 1) {
			g_vdb = NULL;
		}
		pVdb->Release();
	}
}

struct vdb_native_s
{
	CVDB *pVdb;
};

#define NATIVE_CH	0

extern "C" vdb_native_t *vdb_native_create(void)
{
	vdb_native_t *vdb = avf_malloc_type(vdb_native_t);
	if (vdb) {
		vdb->pVdb = get_vdb();
		if (vdb->pVdb == NULL) {
			avf_free(vdb);
			vdb = NULL;
		}
	}
	return vdb;
}

extern "C" int vdb_native_destroy(vdb_native_t *vdb)
{
	free_vdb(vdb->pVdb);
	avf_free(vdb);
	return 0;
}

extern "C" int vdb_native_load(vdb_native_t *vdb, const char *vdb_root)
{
	return vdb->pVdb->Load(vdb_root, true, NULL);
}

extern "C" int vdb_native_unload(vdb_native_t *vdb)
{
	return vdb->pVdb->Unload();
}

extern "C" int vdb_native_start_record(vdb_native_t *vdb, int has_video, int has_picture)
{
	return vdb->pVdb->StartRecord(has_video, has_picture, NULL, NATIVE_CH);
}

extern "C" int vdb_native_stop_record(vdb_native_t *vdb)
{
	return vdb->pVdb->StopRecord(NULL, NATIVE_CH);
}

extern "C" int vdb_native_get_state(vdb_native_t *vdb)
{
/*
	int state = vdb->pVdb->GetVdbState();
	int result = 0;
	if ((state & CVDB::STATE_LOADED) != 0)
		result |= VDB_NATIVE_LOADED;
	if ((state & CVDB::STATE_RECORDING) != 0)
		result |= VDB_NATIVE_RECORDING;
	return result;
*/
	return 0;
}

extern "C" int vdb_native_init_clip(vdb_native_t *vdb, int stream)
{
	return 0;
}

extern "C" char *vdb_native_create_video_filename(vdb_native_t *vdb, vdb_native_time_t *time, int stream)
{
	string_t *name = vdb->pVdb->CreateVideoFileName(time, stream, false, NATIVE_CH);
	char *result = avf_strdup(name->string());
	name->Release();
	return result;
}

extern "C" char *vdb_native_create_picture_filename(vdb_native_t *vdb, vdb_native_time_t *time, int stream)
{
	string_t *name = vdb->pVdb->CreatePictureFileName(time, stream, false, NATIVE_CH);
	char *result = avf_strdup(name->string());
	name->Release();
	return result;
}

extern "C" void vdb_native_free_filename(vdb_native_t *vdb, char *filename)
{
	avf_safe_free(filename);
}

extern "C" int vdb_native_start_clip(vdb_native_t *vdb, const avf_stream_attr_t *stream_attr, int stream)
{
	return vdb->pVdb->InitStream(stream_attr, stream, NATIVE_CH);
}

extern "C" int vdb_native_end_clip(vdb_native_t *vdb, int stream)
{
	return 0;
}

extern "C" int vdb_native_start_segment(vdb_native_t *vdb, vdb_native_time_t video_time, vdb_native_time_t picture_time, int stream)
{
	return vdb->pVdb->StartSegment(video_time, picture_time, stream, NATIVE_CH);
}

extern "C" int vdb_native_end_segment(vdb_native_t *vdb, uint32_t duration_ms, uint32_t video_fsize, int stream, int is_last_segment)
{
	return vdb->pVdb->EndSegment(duration_ms, video_fsize, stream, is_last_segment, is_last_segment, NATIVE_CH);
}

extern "C" int vdb_native_add_video(vdb_native_t *vdb, uint32_t time_ms, uint32_t file_pos, int stream)
{
	return vdb->pVdb->AddVideo(time_ms, file_pos, stream, NATIVE_CH);
}

extern "C" int vdb_native_video_update(vdb_native_t *vdb, uint32_t duration_ms, int stream)
{
	return 0;
}

extern "C" int vdb_native_add_picture(vdb_native_t *vdb, const uint8_t *p, int size, uint64_t pts, uint32_t timescale, int stream)
{
	CBuffer buffer;
	buffer.mpNext = NULL;
	buffer.mpData = (avf_u8_t*)p;
	buffer.mSize = size;
	buffer.m_offset = 0;
	buffer.m_data_size = buffer.mSize;
	buffer.m_dts = buffer.m_pts = pts;
	buffer.m_duration = 0;	// todo
	buffer.m_timescale = timescale;
	return vdb->pVdb->AddPicture(&buffer, 0, NATIVE_CH);
}

extern "C" int vdb_native_add_raw_data(vdb_native_t *vdb, int type, const uint8_t *buffer, int size, uint64_t timestamp_ms, int stream)
{
	raw_data_t *raw = NULL;
	avf_u32_t fcc = 0;

	switch (type) {
	case RAW_DATA_GPS: fcc = IRecordObserver::GPS_DATA; break;
	case RAW_DATA_OBD: fcc = IRecordObserver::OBD_DATA; break;
	case RAW_DATA_ACC: fcc = IRecordObserver::ACC_DATA; break;
	default: break;
	}

	if (fcc == 0) {
		return -1;
	}

	raw = avf_alloc_raw_data(size, fcc);
	::memcpy(raw->GetData(), buffer, size);
	int result = vdb->pVdb->AddRawData(timestamp_ms, raw, 0, NATIVE_CH);
	raw->Release();

	return result;
}

extern "C" uint64_t vdb_native_get_timestamp(void)
{
	return avf_get_curr_tick();
}

int get_log_flags(const char *logs)
{
	int result = 0;
	while (true) {
		int c = *logs++;
		if (c == 0)
			break;
		switch (c) {
		case 'e': result |= AVF_LOG_E; break;
		case 'w': result |= AVF_LOG_W; break;
		case 'i': result |= AVF_LOG_I; break;
		case 'v': result |= AVF_LOG_V; break;
		case 'd': result |= AVF_LOG_D; break;
		case 'p': result |= AVF_LOG_P; break;
		}
	}
	return result;
}

extern "C" void vdb_native_enable_logs(const char *logs)
{
	avf_set_logs(logs);
}

