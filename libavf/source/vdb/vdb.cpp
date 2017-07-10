
#define LOG_TAG "vdb"

#include <fcntl.h>
#include <sys/stat.h>

#ifndef MINGW
#ifdef MAC_OS
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#endif
#endif

#include <dirent.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "rbtree.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avio.h"
#include "buffer_io.h"
#include "sys_io.h"
#include "file_io.h"
#include "tsmap.h"
#include "mpeg_utils.h"

#include "avf_util.h"
#include "mem_stream.h"

#include "vdb_cmd.h"
#include "vdb_util.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_format.h"
#include "vdb_if.h"
#include "vdb.h"
#include "vdb_server.h"

#define CLIPINFO_DIR	"clipinfo" PATH_SEP
#define INDEX0_DIR		"index-1" PATH_SEP
#define INDEX1_DIR		"index-2" PATH_SEP
#define INDEXPIC_DIR	"indexpic" PATH_SEP
#define VIDEO0_DIR		"video-1" PATH_SEP
#define VIDEO1_DIR		"video-2" PATH_SEP


//-----------------------------------------------------------------------
//
//  live_stream_s
//
//-----------------------------------------------------------------------
live_stream_s::live_stream_s()
{
	ls_seg = NULL;
	seg_id = 0;

	seg_start_time_ms = 0;
	seg_v_start_addr = 0;
	stream_end_time_ms = 0;

	video_type = VIDEO_TYPE_TS;	// default

	has_video = false;
	has_data = false;

	si.picture_io = NULL;
	si.sindex = NULL;

	si.video_io = NULL;
	si.tsmap_info = NULL;

	SET_ARRAY_NULL(si.fifos);
}

live_stream_s::~live_stream_s()
{
	avf_safe_release(si.picture_io);
	avf_safe_release(si.video_io);
	avf_safe_release(si.tsmap_info);
	SAFE_RELEASE_ARRAY(si.fifos);
}

void live_stream_s::InitSegment(vdb_stream_t *vdb_stream, vdb_seg_t::info_t& info)
{
	info.id = seg_id++;
	info.duration_ms = 0;
	info.start_time_ms = seg_start_time_ms;
	info.v_start_addr = seg_v_start_addr;
	info.raw_data_size = 0;

	for (unsigned i = 0; i < kTotalArray; i++)
		info.seg_data_size[i] = 0;

	info.stream_attr = vdb_stream->GetStreamAttr();

	si.length_video = 0;
	si.length_data = 0;

	for (unsigned i = 0; i < ARRAY_SIZE(si.sample_count); i++)
		si.sample_count[i] = 0;

	si.video_started = false;
	si.picture_started = false;

	si.video_start_pts = 0;
	si.picture_start_pts = 0;
	si.picture_size = 0;
}

void live_stream_s::FinishSegment()
{
	seg_start_time_ms = ls_seg->GetEndTime_ms();
	seg_v_start_addr = ls_seg->GetEndAddr();

	// close picture IO
	if (si.picture_io) {
		si.picture_io->Close();
	}

	// release the segment
	ls_seg->ReleaseRef();
	ls_seg = NULL;
}

void live_stream_s::Serialize(CMemBuffer *pmb)
{
	if (ls_seg) {
		pmb->write_le_8(1);
		ls_seg->Serialize(pmb);
	} else {
		pmb->write_le_8(0);
	}

	pmb->write_le_32(seg_id);
	pmb->write_le_64(seg_start_time_ms);
	pmb->write_le_64(seg_v_start_addr);
	pmb->write_le_64(stream_end_time_ms);

	pmb->write_le_8(has_video);
	pmb->write_le_8(has_data);

	pmb->write_le_8(si.picture_started);
	pmb->write_le_8(si.video_started);

	pmb->write_le_32(si.picture_size);
	pmb->write_le_64(si.picture_start_pts);
	pmb->write_le_64(si.video_start_pts);

	pmb->write_le_32(si.length_video);
	pmb->write_le_32(si.length_data);

	for (int i = 0; i < kAllArrays; i++) {
		if (si.sindex == NULL || si.sindex->arrays[i] == NULL) {
			pmb->write_le_8(0);
		} else {
			pmb->write_le_8(1);
			si.sindex->arrays[i]->Serialize(pmb);
		}
	}

	for (int i = 0; i < kAllArrays; i++) {
		if (si.fifos[i] == NULL) {
			pmb->write_le_8(0);
		} else {
			pmb->write_le_8(1);
			si.fifos[i]->Serialize(pmb);
		}
	}

	for (int i = 0; i < kAllArrays; i++) {
		pmb->write_le_32(si.sample_count[i]);
	}
}

//-----------------------------------------------------------------------
//
//  CLiveInfo
//
//-----------------------------------------------------------------------
CLiveInfo* CLiveInfo::Create()
{
	CLiveInfo *result = avf_new CLiveInfo();
	CHECK_STATUS(result);
	return result;
}

CLiveInfo::CLiveInfo():
	mbClipDateSet(false),
	mbClipCreated(false),
	m_save_counter(0),
	clip(NULL),
	ref_clip(NULL)
{
	list_init(input_rq);
	raw_stream._Init();
}

CLiveInfo::~CLiveInfo()
{
	list_head_t *node, *next;
	list_for_each_del(node, next, input_rq) {
		raw_data_t *raw = raw_data_t::from_node(node);
		raw->Release();
	}

	raw_stream_s *rs = raw_stream.Item(0);
	for (avf_size_t i = 0; i < raw_stream.count; i++, rs++) {
		avf_safe_release(rs->last_raw);
	}
	raw_stream._Release();
}

avf_u64_t CLiveInfo::AdjustClipLength()
{
	avf_u64_t clip_end_time_ms;

	if (m_stream_0.has_video) {

		if (m_stream_1.has_video) {
			// 1: both have video
			clip_end_time_ms = MIN(m_stream_0.stream_end_time_ms, m_stream_1.stream_end_time_ms);
		} else {
			// 2, 3: stream 0 has video
			clip_end_time_ms = m_stream_0.stream_end_time_ms;
			if (m_stream_1.has_data) {
				// 2: stream 1 has data
				m_stream_1.stream_end_time_ms = clip_end_time_ms;
			}
		}

	} else if (m_stream_0.has_data) {

		if (m_stream_1.has_video) {
			// 4: stream 0 has data, stream 1 has video
			clip_end_time_ms = m_stream_1.stream_end_time_ms;
			m_stream_0.stream_end_time_ms = clip_end_time_ms;
		} else if (m_stream_1.has_data) {
			// 5: both have data, use max
			if (m_stream_0.stream_end_time_ms > m_stream_1.stream_end_time_ms) {
				clip_end_time_ms = m_stream_0.stream_end_time_ms;
				m_stream_1.stream_end_time_ms = clip_end_time_ms;
			} else {
				clip_end_time_ms = m_stream_1.stream_end_time_ms;
				m_stream_0.stream_end_time_ms = clip_end_time_ms;
			}
		} else {
			// 6: stream 0 has data, stream 1 has nothing
			clip_end_time_ms = m_stream_0.stream_end_time_ms;
		}

	} else {
		// 7,8,9: stream 0 has nothing
		clip_end_time_ms = m_stream_1.stream_end_time_ms;
	}

	return clip_end_time_ms;
}

void CLiveInfo::Serialize(CMemBuffer *pmb)
{
	pmb->write_le_8(mbClipDateSet);
	pmb->write_le_8(mbClipCreated);
	pmb->write_le_16(m_save_counter);
	clip->Serialize(pmb);
	ref_clip->Serialize(pmb);
	m_stream_0.Serialize(pmb);
	m_stream_1.Serialize(pmb);
}

//-----------------------------------------------------------------------
//
//  path_filter_s
//
//-----------------------------------------------------------------------
// create a file name
//	if cdir == NULL, returns [vdb]/[stream_dir]/yyyymmdd-hhmmss.[file_ext]
//	else, returns [vdb]/[cdir]/[stream_dir]/yyyymmdd-hhmmss.[file_ext]
string_t *path_filter_s::CreateFileName(clip_dir_t *cdir, int stream, avf_time_t *gen_time, bool bUseGenTime, const char *ext)
{
	string_t *vpath = NULL;
	if (cdir) {
		vpath = cdir->CalcPathName(stream == 0 ? dir_0 : dir_1);
	}

	avf_time_t curr_time;

	// try until a filename is not used
	for (avf_uint_t incre = 0; ; incre++) {
		local_time_name_t lname;

		if (bUseGenTime) {
			curr_time = avf_time_add(*gen_time, incre);
		} else {
			curr_time = avf_get_curr_time(incre);
		}

		local_time_to_name(curr_time, ext ? ext : file_ext, lname);

		if (vpath == NULL) {
			ap<string_t> path(stream == 0 ? path_0 : path_1, lname.buf);
			if (!avf_file_exists(path->string())) {
				avf_touch_file(path->string());
				*gen_time = curr_time;
				return path->acquire();
			}
		} else {
			ap<string_t> path(vpath, lname.buf);
			if (!avf_file_exists(path->string())) {
				avf_touch_file(path->string());
				vpath->Release();
				*gen_time = curr_time;
				return path->acquire();
			}
		}
	}
}

// get the filename
//	if cdir == NULL, returns [vdb]/[stream_dir]/yyyymmdd-hhmmss.[file_ext]
//	else, returns [vdb]/[cdir]/[stream_dir]/yyyymmdd-hhmmss.[file_ext]
string_t *path_filter_s::GetFileName(clip_dir_t *cdir, int stream, avf_time_t local_time, int type)
{
	local_time_name_t lname;
	local_time_to_name(local_time, GetFileExt(type), lname);

	if (cdir == NULL) {
		ap<string_t> fullname(stream == 0 ? path_0 : path_1, lname.buf);
		return fullname->acquire();
	} else {
		string_t *fullname = cdir->CalcFileName(stream == 0 ? dir_0 : dir_1, lname);
		return fullname;
	}
}

void path_filter_s::RemoveFile(clip_dir_t *cdir, int stream, avf_time_t local_time, int type)
{
	if (cdir == NULL) {
		CVdbFileList *list = stream == 0 ? filelist_0 : filelist_1;
		string_t *path = stream == 0 ? path_0 : path_1;
		list->UnuseFile(path, local_time, type, true); // tell filelist to remove it
	} else {
		local_time_name_t lname;
		local_time_to_name(local_time, GetFileExt(type), lname);
		string_t *fullname = cdir->CalcFileName(stream == 0 ? dir_0 : dir_1, lname);
		AVF_LOGD(C_LIGHT_PURPLE "remove file %s" C_NONE, fullname->string());
		avf_remove_file(fullname->string()); // remove directly
		fullname->Release();
	}
}

void path_filter_s::RemoveFile(const vdb_seg_t *seg, avf_time_t local_time, int type)
{
	RemoveFile(seg->m_clip->m_cdir, seg->m_stream, local_time, type);
}

void path_filter_s::Serialize(CMemBuffer *pmb)
{
	pmb->write_length_string(dir_0, 0);
	pmb->write_length_string(dir_1, 0);
	if (path_0 == NULL) {
		pmb->write_length_string("", 0);
	} else {
		pmb->write_length_string(path_0->string(), path_0->size());
	}
	if (path_1 == NULL) {
		pmb->write_length_string("", 0);
	} else {
		pmb->write_length_string(path_1->string(), path_1->size());
	}
	pmb->write_length_string(file_ext, 0);
}

//-----------------------------------------------------------------------
//
//  CVDB
//
//-----------------------------------------------------------------------
CVDB *CVDB::Create(IVDBBroadcast *pBroadcast, void *pOwner, bool b_server_mode)
{
	CVDB *result = avf_new CVDB(pBroadcast, pOwner, b_server_mode);
	CHECK_STATUS(result);
	return result;
}

CVDB::CVDB(IVDBBroadcast *pBroadcast, void *pOwner, bool b_server_mode):
	mpOwner(pOwner),
	mpBroadcast(pBroadcast),
	mpVdbId(NULL),
	mb_server_mode(b_server_mode),

	mb_loaded(false),
	m_no_delete(0),
	m_sid(0),

	mpFirstReader(NULL),
	mpFirstClipWriter(NULL),
	mpFirstClipData(NULL),

	m_index0_filelist(INDEX_FILE_EXT, ""),
	m_index1_filelist(INDEX_FILE_EXT, ""),
	m_indexpic_filelist(PICTURE_FILE_EXT, ""),
	m_video0_filelist(VIDEO_FILE_TS_EXT, VIDEO_FILE_MP4_EXT),
	m_video1_filelist(VIDEO_FILE_TS_EXT, VIDEO_FILE_MP4_EXT),

	m_buffered_rcs(b_server_mode, CLIP_TYPE_BUFFER),
	m_marked_rcs(b_server_mode, CLIP_TYPE_MARKED),

	m_clip_set(b_server_mode, MAX_CACHED_SEGMENT, !mb_server_mode, MAX_CACHED_CLIPS),

	mplive(NULL),
	mplive1(NULL),

	mRecordAttr(0),
	mpTempIO(NULL)
{
}

CVDB::~CVDB()
{
	Unload(true);
	avf_safe_release(mpTempIO);
	avf_safe_release(mpVdbId);
}

void *CVDB::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IVdbInfo)
		return static_cast<IVdbInfo*>(this);
	return inherited::GetInterface(refiid);
}

// for write
// vdb, playlist, segment index, clip info
IAVIO *CVDB::GetTempIO()
{
	if (mpTempIO == NULL) {
		if (mb_server_mode) {
			mpTempIO = CSysIO::Create();
		} else {
			mpTempIO = CBufferIO::Create();
		}
	} else {
		if (mpTempIO->IsOpen()) {
			AVF_LOGE("io is open!");
		}
	}
	return mpTempIO;
}

// path: /tmp/mmcblk0p1/100TRANS/
avf_status_t CVDB::Load(const char *mnt, const char *path, bool bCreate, const char *id)
{
	AVF_LOGD(C_GREEN "load vdb: %s" C_NONE, path);
	AUTO_LOCK(mMutex);

	if (mb_loaded) {
		AVF_LOGD("vdb already loaded %s", path);
		return E_OK;
	}

	avf_u64_t tick = avf_get_curr_tick();

	m_mnt = mnt ? mnt : "";
	m_vdb_root = path;

	// vdb root dir
	if (bCreate) {
		avf_safe_mkdir(m_vdb_root->string());
	}

	if (!mb_server_mode) {
		m_clipinfo_path = ap<string_t>(m_vdb_root.get(), CLIPINFO_DIR);
		if (bCreate) {
			avf_safe_mkdir(m_clipinfo_path->string());
		}

		m_index0_path = ap<string_t>(m_vdb_root.get(), INDEX0_DIR);
		if (bCreate) {
			avf_safe_mkdir(m_index0_path->string());
		}

		m_index1_path = ap<string_t>(m_vdb_root.get(), INDEX1_DIR);
		if (bCreate) {
			avf_safe_mkdir(m_index1_path->string());
		}

		m_indexpic_path = ap<string_t>(m_vdb_root.get(), INDEXPIC_DIR);
		if (bCreate) {
			avf_safe_mkdir(m_indexpic_path->string());
		}

		m_video0_path = ap<string_t>(m_vdb_root.get(), VIDEO0_DIR);
		if (bCreate) {
			avf_safe_mkdir(m_video0_path->string());
		}

		m_video1_path = ap<string_t>(m_vdb_root.get(), VIDEO1_DIR);
		if (bCreate) {
			avf_safe_mkdir(m_video1_path->string());
		}
	}

	//-------------------------------------
	// index
	//-------------------------------------
	m_pf_index.dir_0 = INDEX0_DIR;
	m_pf_index.dir_1 = INDEX1_DIR;
	m_pf_index.file_ext = INDEX_FILE_EXT;
	m_pf_index.file_ext2 = "";
	m_pf_index.path_0 = m_index0_path.get();
	m_pf_index.path_1 = m_index1_path.get();
	m_pf_index.filelist_0 = &m_index0_filelist;
	m_pf_index.filelist_1 = &m_index1_filelist;

	//-------------------------------------
	// picture
	//-------------------------------------
	m_pf_picture.dir_0 = INDEXPIC_DIR;
	m_pf_picture.dir_1 = INDEXPIC_DIR;
	m_pf_picture.file_ext = PICTURE_FILE_EXT;
	m_pf_picture.file_ext2 = "";
	m_pf_picture.path_0 = m_indexpic_path.get();
	m_pf_picture.path_1 = m_indexpic_path.get(); // same
	m_pf_picture.filelist_0 = &m_indexpic_filelist;
	m_pf_picture.filelist_1 = &m_indexpic_filelist; // same

	//-------------------------------------
	// video
	//-------------------------------------
	m_pf_video.dir_0 = VIDEO0_DIR;
	m_pf_video.dir_1 = VIDEO1_DIR;
	m_pf_video.file_ext = VIDEO_FILE_TS_EXT;
	m_pf_video.file_ext2 = VIDEO_FILE_MP4_EXT;
	m_pf_video.path_0 = m_video0_path.get();
	m_pf_video.path_1 = m_video1_path.get();
	m_pf_video.filelist_0 = &m_video0_filelist;
	m_pf_video.filelist_1 = &m_video1_filelist;

	// load clips & files
	if (mb_server_mode) {
		if (CVdbFormat::VdbFileExists(m_vdb_root.get())) {
			// only load clip dirs info
			AVF_LOGI("load %s", m_vdb_root->string());
			CVdbFormat::LoadVdb(m_vdb_root.get(), m_clip_set, m_cdir_set);
		} else {
			AVF_LOGI(C_YELLOW "try to re-create %s" C_NONE, m_vdb_root->string());
			ScanAllClipSetL();
			// LoadPlaylistL();
			CollectAllRefClipsL();
			CVdbFormat::SaveVdb(m_vdb_root.get(), GetTempIO(), m_clip_set, m_cdir_set);
		}
	} else {
		ScanClipSetL(NULL, m_clipinfo_path.get());
		LoadPlaylistL();
		CollectAllRefClipsL();

		ScanFileListL(m_index0_path, m_index0_filelist);
		ScanFileListL(m_index1_path, m_index1_filelist);
		ScanFileListL(m_indexpic_path, m_indexpic_filelist);
		ScanFileListL(m_video0_path, m_video0_filelist);
		ScanFileListL(m_video1_path, m_video1_filelist);
	}

	avf_safe_release(mpVdbId);
	if (id != NULL) {
		mpVdbId = string_t::CreateFrom(id, ::strlen(id));
	}

	AVF_LOGI("there are %d buffered clips and %d marked clips", m_buffered_rcs.mTotalRefClips, m_marked_rcs.mTotalRefClips);
	AVF_LOGI("clip space: " LLD ", marked space: " LLD ", info space: " LLD,
		m_clip_set.m_clip_space, m_clip_set.m_marked_space, m_clip_set.m_clipinfo_space);
	AVF_LOGI(C_GREEN "load vdb used %d ms" C_NONE, (int)(avf_get_curr_tick() - tick));

	mb_loaded = true;

	if (mpBroadcast) {
		mpBroadcast->VdbReady(mpVdbId, 0);
		ReportSpaceInfoL();
	}

	return E_OK;
}

avf_status_t CVDB::Unload(bool bInDestructor)
{
	AUTO_LOCK(mMutex);

	if (!mb_loaded) {
		//AVF_LOGD("CVDB::Unload(): vdb not loaded");
		return E_OK;
	}

	AVF_LOGD(C_GREEN "unload vdb" C_NONE);

	if (IsRecording()) {
		AVF_LOGE("Unload() - still recording, should call StopRecord() first");
		return E_PERM;
	}

	// close all readers
	CVdbReader *reader = mpFirstReader;
	while (reader) {
		CVdbReader *next = reader->mpNext;
		reader->CloseL();
		reader = next;
	}

	// close all clip writers
	CClipWriter *writer = mpFirstClipWriter;
	while (writer) {
		CClipWriter *next = writer->mpNext;
		writer->Release();
		writer = next;
	}

	// close all clip data
	CClipData *pData = mpFirstClipData;
	while (pData) {
		CClipData *next = pData->mpNext;
		CloseClipDataL(pData);
		pData = next;
	}

	StopRecordL(NULL, 0);
	StopRecordL(NULL, 1);

	AVF_LOGD("clip space: " LLD ", marked space: " LLD ", info space: " LLD,
		m_clip_set.m_clip_space, m_clip_set.m_marked_space, m_clip_set.m_clipinfo_space);

	if (mb_server_mode) {
		CVdbFormat::SaveVdb(m_vdb_root.get(), GetTempIO(), m_clip_set, m_cdir_set);
	} else {
		m_index0_filelist.Clear();
		m_index1_filelist.Clear();
		m_indexpic_filelist.Clear();
		m_video0_filelist.Clear();
		m_video1_filelist.Clear();
	}

	m_buffered_rcs.Clear();
	m_marked_rcs.Clear();
	m_plist_set.Clear();

	m_clip_set.Clear();	// should be cleared after ref clips
	m_cdir_set.Clear();

	m_pf_index.Clear();
	m_pf_picture.Clear();
	m_pf_video.Clear();

	mb_loaded = false;

	if (!bInDestructor && mpBroadcast) {
		mpBroadcast->VdbUnmounted(mpVdbId);
	}

	return E_OK;
}

void CVDB::PrintSpaceInfoL(const space_info_t& si)
{
	AVF_LOGD(C_BROWN "total space %d MB, free: %d MB disk, %d MB file" C_NONE,
		(avf_u32_t)(si.disk_space_b >> 20),
		(avf_u32_t)(si.free_disk_b >> 20),
		(avf_u32_t)(si.free_file_b >> 20));
}

avf_u64_t CVDB::GetMinSpaceValue(const space_info_t& si)
{
	avf_u64_t min_space_value = (si.disk_space_b * kMinSpace_milli) / 1000;
	if (min_space_value < kMinSpace)
		min_space_value = kMinSpace;
	return min_space_value;
}

avf_u64_t CVDB::GetEnoughSpaceValue(const space_info_t& si)
{
	avf_u64_t enough_space_value = (si.disk_space_b * kEnoughSpace_milli) / 1000;
	if (enough_space_value < kEnoughSpace)
		enough_space_value = kEnoughSpace;
	return enough_space_value;
}

avf_u64_t CVDB::GetReusableClipSpace()
{
	avf_u64_t clip_space = m_clip_set.m_clip_space;
	avf_u64_t marked_space = m_clip_set.m_marked_space;
	return clip_space - marked_space;
}

void CVDB::GetVdbSpaceInfo(IVDBControl::vdb_info_s *info, avf_u64_t autodel_free, avf_u64_t autodel_min)
{
	AUTO_LOCK(mMutex);

	// set to all 0
	::memset(info, 0, sizeof(*info));

	if (!mb_loaded)
		return;

	space_info_t si;
	if (!GetSpaceInfoL(si))
		return;

	avf_u64_t total_avail_space = si.free_disk_b + si.free_file_b;

	info->total_space_mb = si.disk_space_b >> 20;
	info->disk_space_mb = total_avail_space >> 20;
	info->file_space_mb = 0;
	info->vdb_ready = 1;

	avf_u64_t enough_space_value = autodel_free ? autodel_free : GetEnoughSpaceValue(si);
	info->enough_space = total_avail_space >= enough_space_value;	// total avail >= 300M

	if (!m_no_delete) {
		total_avail_space += GetReusableClipSpace();
	}
	info->enough_space_autodel = total_avail_space >= enough_space_value;
}

void CVDB::CheckSpace(bool bAutoDelete, bool *not_ready, bool *no_space,
	avf_u64_t autodel_free, avf_u64_t autodel_min)
{
	AUTO_LOCK(mMutex);

	if (!mb_loaded) {
		*not_ready = true;
		*no_space = true;
		return;
	}

	if (m_no_delete) {
		// removing clips is prohibited
		bAutoDelete = false;
	}

	while (true) {
		space_info_t si;

		if (!GetSpaceInfoL(si)) {
			*not_ready = true;
			*no_space = true;
			return;
		}

		PrintSpaceInfoL(si);

		avf_u64_t min_space_value = autodel_min ? autodel_min : GetMinSpaceValue(si);
		avf_u64_t enough_space_value = autodel_free ? autodel_free : GetEnoughSpaceValue(si);

		if (si.free_disk_b >= min_space_value) {
			// there's min disk space for recording
			if (si.free_disk_b + si.free_file_b < enough_space_value) {
				// free space is low
				if (RemoveOldFilesL() != 0) {
					// delete some files and try again
					continue;
				}
				// no files to delete, delete some clip
				if (bAutoDelete && TryRemoveClipL(si)) {
					continue;
				}
				// space is low, but no file/clip can be removed
				AVF_LOGW("buffer space low");
			}
			*not_ready = false;
			*no_space = false;
			return;
		}

		// no disk space - delete some file and check again
		if (RemoveOldFilesL() != 0) {
			// try again
			continue;
		}

		// no space and no files to delete, delete some clip
		if (bAutoDelete && TryRemoveClipL(si)) {
			// try again
			continue;
		}

		// no space, no files/clips can be removed
		AVF_LOGD(C_LIGHT_RED "buffer full" C_NONE);

		if (mpBroadcast) {
			mpBroadcast->BufferFull(mpVdbId);
		}

		*not_ready = false;
		*no_space = true;
		return;
	}
}

bool CVDB::TryRemoveClipL(const space_info_t& si)
{
	AVF_LOGD(C_YELLOW "try remove clip" C_NONE);

	if (m_no_delete) {
		//AVF_LOGW("no delete clip");
		return false;
	}

	while (true) {

		ref_clip_t *ref_clip = GetRefClipToBeShrinkedL();
		if (ref_clip == NULL) {
			AVF_LOGW("try remove clip: no ref clip");
			return false;
		}

		avf_u64_t old_size = m_clip_set.GetSpace();
		vdb_clip_t *clip = ref_clip->clip;
		ref_clip->ShrinkOneSegment();

		if (ref_clip->IsEmpty()) {
			avf_u32_t ref_clip_type = ref_clip->ref_clip_type;
			clip_id_t ref_clip_id = ref_clip->ref_clip_id;

			// remove the ref clip
			m_buffered_rcs.RemoveEmptyRefClip(ref_clip);

			// remove empty clip
			if (clip->IsNotUsed()) {
				AVF_LOGD("remove clip %x", clip->clip_id);
				m_clip_set.RemoveClip(clip, this);
			} else {
				SaveClipInfoL(clip);
			}

			if (mpBroadcast) {
				mpBroadcast->ClipRemoved(mpVdbId, ref_clip_type, ref_clip_id);
			}

		} else {

			SaveClipInfoL(clip);

			if (mpBroadcast) {
				mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_CHANGED, ref_clip, NULL);
			}
		}

		avf_u64_t new_size = m_clip_set.GetSpace();
		if (new_size < old_size) {
			AVF_LOGD(C_YELLOW "clip removed, space +%d MB" C_NONE,
				(avf_u32_t)((old_size - new_size) >> 20));
			return true;
		}
	}
}

ref_clip_t *CVDB::GetRefClipToBeShrinkedL()
{
	ref_clip_t *ref_clip = m_buffered_rcs.GetFirstRefClip();

	for (; ref_clip; ref_clip = ref_clip->GetNextRefClip()) {
		if (ref_clip->CanAutoDelete())
			return ref_clip;
	}

	// if the live clip is long enough
	for (unsigned i = 0; i < NUM_LIVE_CH; i++) {
		CLiveInfo *li = GetLiveInfoL(i);
		if (li && li->ref_clip) {
			if (li->ref_clip->AutoDelete() && li->ref_clip->CalcNumSegments(2))
				return li->ref_clip;
		}
	}

	return NULL;
}

clip_list_s *CVDB::s_GetAllClips()
{
	clip_list_s *first = NULL;
	clip_list_s *last = NULL;

	if (mb_server_mode) {
		avf_dir_t adir;
		struct dirent *ent;

		if (!avf_open_dir(&adir, m_vdb_root->string())) {
			AVF_LOGW("cannot open %s", m_vdb_root->string());
			return NULL;
		}

		while ((ent = avf_read_dir(&adir)) != NULL) {
			if (avf_is_parent_dir(ent->d_name))
				continue;

			if (avf_path_is_dir(&adir, ent)) {
				avf_time_t local_time;
				if (name_to_local_time(ent->d_name, &local_time, NULL, NULL, NULL)) {
					ap<string_t> path(m_vdb_root.get(), ent->d_name, PATH_SEP);
					ap<string_t> clip_path(path.get(), CLIPINFO_DIR);
					s_ScanClipDir(&first, &last, clip_path.get(), local_time);
				}
			}
		}

		avf_close_dir(&adir);
		return first;
	}

	AUTO_LOCK(mMutex);

	list_head_t *node;
	list_for_each(node, m_buffered_rcs.ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_list_node(node);

		local_time_name_t lname;
		local_time_to_name(ref_clip->ref_clip_id, "", lname);

		clip_list_s *item = avf_new clip_list_s();
		item->next = NULL;
		item->display_name = string_t::Add(lname.buf, lname.len, "", 0, NULL, 0, NULL, 0);
		item->clip_dir = AVF_INVALID_TIME;
		item->clip_id = ref_clip->ref_clip_id;

		if (last) {
			last->next = item;
			last = item;
		} else {
			first = item;
			last = item;
		}
	}

	return first;
}

void CVDB::s_ScanClipDir(clip_list_s **pfirst, clip_list_s **plast, string_t *clip_path, avf_time_t dirname)
{
	avf_dir_t adir;
	struct dirent *clip_ent;

	if (avf_open_dir(&adir, clip_path->string())) {
		while ((clip_ent = avf_read_dir(&adir)) != NULL) {
			if (avf_is_parent_dir(clip_ent->d_name))
				continue;

			avf_time_t clip_local_time;
			if (name_to_local_time(clip_ent->d_name, &clip_local_time, CLIPINFO_FILE_EXT, NULL, NULL)) {
				local_time_name_t lname;
				local_time_to_name(clip_local_time, "", lname);

				clip_list_s *item = avf_new clip_list_s();
				item->next = NULL;
				item->display_name = string_t::Add(lname.buf, lname.len, "", 0, NULL, 0, NULL, 0);
				item->clip_dir = dirname;
				item->clip_id = clip_local_time;

				if ((*plast)) {
					(*plast)->next = item;
					*plast = item;
				} else {
					*pfirst = item;
					*plast = item;
				}
			}
		}
		avf_close_dir(&adir);
	}
}

avf_status_t CVDB::s_GetClipInfo(vdb_clip_id_t vdb_clip_id, vdbs_clip_info_t& info)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = s_GetRefClipL(vdb_clip_id);
	if (ref_clip == NULL)
		return E_ERROR;

	vdb_clip_t *clip = ref_clip->clip;
	m_clip_set.BringToFront(clip);

	::memset(&info, 0, sizeof(vdbs_clip_info_t));

	info.clip_date = clip->clip_date;
	info.gmtoff = clip->gmtoff;
	info.clip_duration_ms = ref_clip->GetDurationLong();
	info.clip_start_time_ms = ref_clip->start_time_ms;

	if (clip->m_num_segs_0 > 0)
		info.num_streams++;
	if (clip->m_num_segs_1 > 0)
		info.num_streams++;

	info.stream_info[0] = clip->GetStreamAttr(0);
	info.stream_info[1] = clip->GetStreamAttr(1);

	return E_OK;
}

avf_status_t CVDB::s_GetIndexPicture(vdb_clip_id_t vdb_clip_id, void **pdata, int *psize)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = s_GetRefClipL(vdb_clip_id);
	if (ref_clip == NULL)
		return E_INVAL;

	vdb_clip_t *clip = ref_clip->clip;
	int result;

	void *data = avf_malloc_bytes(clip->first_pic_size);
	if (data == NULL)
		return E_NOMEM;

	avf_time_t first_pic_name = ref_clip->clip->first_pic_name;
	avf_size_t first_pic_size = ref_clip->clip->first_pic_size;
	*psize = first_pic_size;

	if (ref_clip->IsLive()) {
		for (unsigned i = 0; i < NUM_LIVE_CH; i++) {
			CLiveInfo *li = GetLiveInfoL(i);
			if (li) {
				live_stream_s& ls = li->GetLiveStream(0);
				if (ls.ls_seg && ls.ls_seg->m_picture_name == first_pic_name) {
					if (ls.si.picture_io == NULL)
						return E_ERROR;
					result = ls.si.picture_io->ReadAt(data, first_pic_size, 0);
					if (result != (int)first_pic_size) {
						avf_free(data);
						return E_IO;
					}

					*pdata = data;
					return E_OK;
				}
			}
		}
	}

	IAVIO *io = CFileIO::Create();

	string_t *jpeg_filename = m_pf_picture.GetFileName(clip->m_cdir, 0, clip->first_pic_name, 0);
	avf_status_t status = io->OpenRead(jpeg_filename->string());
	jpeg_filename->Release();

	if (status != E_OK) {
		avf_safe_free(data);
		goto Done;
	}

	result = io->ReadAt(data, first_pic_size, 0);
	if (result != (int)first_pic_size) {
		avf_safe_free(data);
		status = E_IO;
		goto Done;
	}

Done:
	io->Release();

	*pdata = data;
	return status;
}

avf_status_t CVDB::s_GetIndexPictureAtPos(vdb_clip_id_t vdb_clip_id, uint64_t clip_time_ms, 
	void **pdata, int *psize, uint64_t *clip_time_ms_out)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = s_GetRefClipL(vdb_clip_id);
	if (ref_clip == NULL)
		return E_INVAL;

	if (clip_time_ms < ref_clip->start_time_ms)
		clip_time_ms = ref_clip->start_time_ms;

	vdb_seg_t *seg = ref_clip->FindSegmentByTime(0, clip_time_ms);
	if (seg == NULL) {
		AVF_LOGD("s_GetIndexPictureAtPos: no such segment");
		return E_INVAL;
	}

	index_pos_t pos;
	avf_u32_t duration;
	avf_u32_t bytes;

	avf_status_t status = seg->FindPosEx(kPictureArray, clip_time_ms, &pos, &duration, &bytes);
	if (status != E_OK) {
		AVF_LOGD("s_GetIndexPictureAtPos failed");
		return E_ERROR;
	}

	avf_u8_t *buffer = avf_malloc_bytes(bytes);
	if (buffer == NULL)
		return E_NOMEM;

	status = FetchIndexPictureL(seg, pos, bytes, true, NULL, buffer);
	if (status != E_OK) {
		avf_safe_free(buffer);
		return E_ERROR;
	}

	*pdata = buffer;
	*psize = bytes;
	*clip_time_ms_out = seg->GetStartTime_ms() + pos.item->time_ms;

	return E_OK;
}

avf_status_t CVDB::s_RemoveClip(vdb_clip_id_t vdb_clip_id)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = s_GetRefClipL(vdb_clip_id);
	if (ref_clip == NULL)
		return E_ERROR;

	// make sure it is not the live clip
	if (ref_clip->IsLive()) {
		AVF_LOGD("cannot delete the live clip");
		return E_ERROR;
	}

	RemoveRefClipL(ref_clip, true);

	return E_OK;
}

// for server use
ref_clip_t *CVDB::s_GetRefClipL(vdb_clip_id_t vdb_clip_id)
{
	avf_time_t dirname;
	clip_id_t clip_id;
	GET_VDB_CLIP_ID(vdb_clip_id, dirname, clip_id);

	// if the ref clip already exists
	ref_clip_t *ref_clip = FindRefClipExL(CLIP_TYPE_BUFFER, dirname, clip_id);
	if (ref_clip) {
		return ref_clip;
	}

	if (mb_server_mode) {

		// make sure the clip is not loaded, and load it dynamically
		if (FindClipExL(dirname, clip_id) == NULL) {
			m_clip_set.TrimLoadedClip(MAX_LOADED_CLIPS);

			if (s_DynamicLoadClipL(dirname, clip_id) != E_OK) {
				return NULL;
			}

			ref_clip = FindRefClipL(CLIP_TYPE_BUFFER, clip_id);
			if (ref_clip) {
				return ref_clip;
			}
		}

	}

	AVF_LOGE("s_GetRefClipL: no such clip 0x%x", clip_id);
	return NULL;
}

avf_status_t CVDB::s_DynamicLoadClipL(avf_time_t dirname, clip_id_t clip_id)
{
	clip_dir_t *cdir = m_cdir_set.FindDir(dirname);
	bool bExists = true;

	if (cdir == NULL) {
		AVF_LOGW("cdir %x not found, add it", dirname);
		cdir = AddDirL(dirname, false, &bExists);
		if (cdir == NULL) {
			return E_ERROR;
		}
	}

	string_t *clip_path = cdir->CalcPathName(CLIPINFO_DIR);
	IAVIO *io = CSysIO::Create();

	avf_status_t status = LoadClipL(cdir, clip_id, clip_path, io, false, NULL);

	io->Release();
	clip_path->Release();

	// if load clip failed, and the dir is created, then remove it
	if (status != E_OK && !bExists) {
		AVF_LOGW("remove cdir %x", dirname);
		m_cdir_set.RemoveClipDir(cdir, false);
	}

	return status;
}

avf_size_t CVDB::s_GetNumClips(void)
{
	AUTO_LOCK(mMutex);
	return m_clip_set.mTotalClips;
}

avf_u64_t CVDB::s_GetLiveClipLength(bool& bHasVideo, bool& bHasPicture, int ch)
{
	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL)
		return 0;

	bHasVideo = false; //li->mbHasVideo;
	bHasPicture = false; //li->mbHasPicture;

	return li->ref_clip->GetDurationLong();
}

avf_status_t CVDB::s_EnumRawData(vdb_enum_raw_data_proc proc, void *ctx,
	vdb_clip_id_t vdb_clip_id, int data_type,
	uint64_t clip_time_ms, uint32_t length_ms)
{
	int index = get_vdb_array_index(data_type);
	if (index < 0) {
		AVF_LOGD("s_EnumRawData: unknown data_type %d", data_type);
		return E_INVAL;
	}

	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = s_GetRefClipL(vdb_clip_id);
	if (ref_clip == NULL)
		return E_INVAL;

	vdb_seg_t *curr_seg = ref_clip->FindSegmentByTime(0, clip_time_ms);
	if (curr_seg == NULL) {
		AVF_LOGE("segment not found");
		return E_INVAL;
	}

	index_pos_t start_pos;
	avf_status_t status;

	status = curr_seg->FindPos(index, clip_time_ms, &start_pos);
	if (status != E_OK) {
		AVF_LOGE("FindPos failed");
		return status;
	}

	avf_u64_t end_time_ms = clip_time_ms + length_ms;
	while (true) {
		avf_u64_t seg_end_time_ms = curr_seg->GetEndTime_ms();

		if (end_time_ms >= seg_end_time_ms) {
			int result = proc(ctx, curr_seg->GetStartTime_ms(),
				start_pos.GetRemain(), start_pos.item, 
				start_pos.array->GetMem(),
				start_pos.array->GetMemSize());
			if (result == 0)
				break;

		} else {

			index_pos_t end_pos;
			status = curr_seg->FindPos(index, end_time_ms - 1, &end_pos);
			if (status != E_OK) {
				AVF_LOGE("FindPos failed");
				return status;
			}

			proc(ctx, curr_seg->GetStartTime_ms(),
				end_pos.item - start_pos.item + 1, start_pos.item,
				start_pos.array->GetMem(),
				end_pos.item->fpos + end_pos.GetItemBytes());

			break;
		}

		if (end_time_ms == seg_end_time_ms)
			break;

		if ((curr_seg = curr_seg->GetNextSegInRefClip()) == NULL) {
			AVF_LOGW("no next segment");
			break;
		}

		status = curr_seg->GetFirstPos(index, &start_pos);
		if (status != E_OK) {
			AVF_LOGE("no position in segment");
			break;
		}
	}

	return E_OK;
}

avf_status_t CVDB::s_GetClipRangeInfo(vdb_clip_id_t vdb_clip_id, 
	const range_s *range, clip_info_s *clip_info)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = s_GetRefClipL(vdb_clip_id);
	if (ref_clip == NULL) {
		return E_ERROR;
	}

	return GetClipRangeInfoL(ref_clip, range, clip_info, 0);
}

avf_status_t CVDB::s_ReadHLS(const char *server_ip,
	vdb_clip_id_t vdb_clip_id, avf_u64_t clip_time_ms,
	CMemBuffer *pmb, int stream, avf_u32_t hls_len)
{
	AUTO_LOCK(mMutex);

	if (hls_len == 0) {
		hls_len = HLS_SEGMENT_LENGTH_MS;
	}

	ref_clip_t *ref_clip = s_GetRefClipL(vdb_clip_id);
	if (ref_clip == NULL) {
		return E_ERROR;
	}

	range_s range;
	range.clip_type = ref_clip->ref_clip_type;
	range.ref_clip_id = ref_clip->ref_clip_id;
	range._stream = stream;
	range.b_playlist = 0;
	range.length_ms = 0;
	range.clip_time_ms = clip_time_ms;

	return WriteSegmentListL(server_ip, ref_clip, &range, pmb,
		true, true, false, false, false, false,
		0, hls_len, 0);
}

// scan camera/*/*
// server mode
avf_status_t CVDB::ScanAllClipSetL()
{
	avf_dir_t adir;
	struct dirent *ent;

	if (!avf_open_dir(&adir, m_vdb_root->string())) {
		AVF_LOGW("cannot open %s", m_vdb_root->string());
		return E_ERROR;
	}

	while ((ent = avf_read_dir(&adir)) != NULL) {
		if (avf_is_parent_dir(ent->d_name))
			continue;

		if (avf_path_is_dir(&adir, ent)) {
			avf_time_t local_time;
			if (name_to_local_time(ent->d_name, &local_time, NULL, NULL, NULL)) {
				ap<string_t> path(m_vdb_root.get(), ent->d_name, PATH_SEP);
				clip_dir_t *cdir = AddDirL(local_time, false);

				ap<string_t> clip_path(path.get(), CLIPINFO_DIR);
				ScanClipSetL(cdir, clip_path.get());
			}
		}
	}

	avf_close_dir(&adir);
	return E_OK;
}

// scan camera/clipinfo/*
avf_status_t CVDB::ScanClipSetL(clip_dir_t *cdir, string_t *clip_path)
{
	avf_dir_t adir;
	struct dirent *ent;

	if (!avf_open_dir(&adir, clip_path->string())) {
		AVF_LOGD("cannot open clip dir %s", clip_path->string());
		return E_ERROR;
	}

#ifdef WIN32_OS
	IAVIO *io = CFileIO::Create();
#else
	IAVIO *io = CSysIO::Create();
#endif

	while ((ent = avf_read_dir(&adir)) != NULL) {
		if (avf_is_parent_dir(ent->d_name))
			continue;

		// remove dirs
		if (avf_path_is_dir(&adir, ent)) {
			AVF_LOGW("'%s' is a folder, remove", ent->d_name);
			ap<string_t> path(clip_path, ent->d_name);
			avf_remove_dir_recursive(path->string());
			continue;
		}

		// remove unknown files
		avf_time_t local_time;
		if (!name_to_local_time(ent->d_name, &local_time, CLIPINFO_FILE_EXT, NULL, NULL)) {
			AVF_LOGW("'%s' is not a valid clipinfo file, remove", ent->d_name);
			ap<string_t> path(clip_path, ent->d_name);
			avf_remove_file(path->string());
			continue;
		}

		LoadClipL(cdir, local_time, clip_path, io, true, NULL);
	}

	io->Release();
	avf_close_dir(&adir);

	return E_OK;
}

// bUpdate:
//	true - by scan
//	false - dynamic
avf_status_t CVDB::LoadClipL(clip_dir_t *cdir, 
	avf_time_t local_time, string_t *clip_path, IAVIO *io, bool bUpdate, vdb_clip_t **pclip)
{
	// load the clipinfo file
	vdb_clip_t *clip = avf_new vdb_clip_t(this, &m_clip_set, cdir, local_time);
	if (!mb_server_mode) {
		// load streams if not server mode
		m_clip_set.AllocClipCache(clip);
	}

	avf_u64_t ref_start_time_ms = 0;
	avf_u64_t ref_end_time_ms = 0;

	// load .clip
	avf_status_t status = CVdbFormat::LoadClipInfo(clip_path, clip, io,
		&ref_start_time_ms, &ref_end_time_ms, CVdbFormat::LOAD_CLIP_ALL, bUpdate);
	io->Close();

	if (status != E_OK) {
		m_clip_set.ReleaseClipCache(clip);
		avf_delete clip;
		return status;
	}

	m_clip_set.m_clipinfo_space += clip->m_clipinfo_filesize;

	if (bUpdate && cdir) {
		cdir->ClipAdded();
	}

	// create buffered ref clip
	if (ref_start_time_ms < ref_end_time_ms) {
		// note: the ref clip id equals clip id
		// segments will be locked later
		clip->NewRefClip(CLIP_TYPE_BUFFER, clip->clip_id, ref_start_time_ms, ref_end_time_ms, 0);
		AVF_LOGD(C_CYAN "buffered clip: " LLD ", " LLD C_NONE, ref_start_time_ms, ref_end_time_ms);
	}

	// register clip files
	if (cdir) {
		if (bUpdate) {
			cdir->SegmentAdded(clip->m_num_segs_0, clip->m_num_segs_1);
		}
	} else {
		if (clip->m_cache == NULL) {
			AVF_LOGW("clip cache not loaded");
		} else {
			// register all segments files of the clip
			for (int i = 0; i < 2; i++) {
				vdb_seg_t *seg = clip->__GetStream(i)->GetFirstSegment();
				for (; seg; seg = seg->GetNextSegment()) {
					RegisterSegmentFilesL(i, seg);
				}
			}
		}
	}

	// add the clip
	m_clip_set.AddClip(clip, bUpdate);

	// marked ref clips; not for server mode
	if (!bUpdate) {
		CollectRefClipsL(clip);
	}

	// upgrade
	if (clip->FindUUID() == NULL) {
		AVF_LOGD(C_WHITE "generate uuid for old clips" C_NONE);
		clip->GenerateUUID();
		SaveClipInfoL(clip);
	}

	if (pclip) {
		*pclip = clip;
	}

	return E_OK;
}

// requested by segment
bool CVDB::LoadSegCache(vdb_seg_t *seg)
{
	string_t *filename = seg->GetIndexFileName();
	if (filename == NULL)
		return false;

	AVF_LOGD(" --- LoadSegCache(%d) ---", seg->m_info.id);

	m_clip_set.AllocSegCache(seg);

	IAVIO *io = CSysIO::Create();
	avf_status_t status = io->OpenRead(filename->string());
	if (status == E_OK) {
		status = CVdbFormat::LoadSegmentIndex(filename->string(), io, seg);
		if (status != E_OK) {
			AVF_LOGE("failed to load %s", filename->string());
		}
	}

	io->Release();
	filename->Release();

	return status == E_OK;
}

// requested by vdb_clip_t
bool CVDB::LoadClipCache(vdb_clip_t *clip)
{
	if (clip->m_cdir == NULL) {
		AVF_LOGE("cannot load clip cache");
		return false;
	}

	m_clip_set.AllocClipCache(clip);

	string_t *clip_path = clip->m_cdir->CalcPathName(CLIPINFO_DIR);
	IAVIO *io = CSysIO::Create();

	avf_status_t status = CVdbFormat::LoadClipInfo(clip_path, clip, io,
		NULL, NULL, CVdbFormat::LOAD_CLIP_CACHE, false);

	io->Release();
	clip_path->Release();

	if (status == E_OK) {
		// add ref for ref clips
		clip->LockAllSegments();
		return true;
	}

	return false;
}

avf_status_t CVDB::LoadPlaylistL()
{
	if (mb_server_mode) {
		AVF_LOGW("playlist is not supported under server mode");
		return E_ERROR;
	}

#ifdef LINUX_OS
	for (int i = 0; i < NUM_PLISTS; i++) {
		ref_clip_set_t *rcs = avf_new ref_clip_set_t(mb_server_mode, CLIP_TYPE_FIRST_PLIST + i);
		if (rcs) {
			m_plist_set.AddPlaylist(rcs);
		}
	}
#endif

	CVdbFormat::LoadPlaylist(m_vdb_root.get(), m_clip_set);

	// 
	ap<string_t> playlist_path(GetVdbRoot(), PLAYLIST_PATH);

	avf_dir_t adir;
	struct dirent *ent;

	if (!avf_open_dir(&adir, playlist_path->string())) {
		return E_OK;
	}

	while ((ent = avf_read_dir(&adir)) != NULL) {
		if (avf_is_parent_dir(ent->d_name))
			continue;

		if (avf_path_is_dir(&adir, ent)) {
			char *pend;
			avf_uint_t id = ::strtoul(ent->d_name, &pend, 10);
			if (pend == ent->d_name || GetPlaylist(id) == NULL) {
				if (pend == ent->d_name) {
					AVF_LOGW("unknown dir %s, remove", ent->d_name);
				} else {
					AVF_LOGW("playlist %d does not exist, remove data files", id);
				}
				ap<string_t> path(playlist_path->string(), ent->d_name);
				avf_remove_dir_recursive(path->string(), false);
				continue;
			}
		}
	}

	avf_close_dir(&adir);
	return E_OK;
}

avf_status_t CVDB::SavePlaylistL()
{
	if (mb_server_mode) {
		AVF_LOGW("playlist is not supported under server mode");
		return E_ERROR;
	}

	avf_status_t status = CVdbFormat::SavePlaylists(m_vdb_root.get(),
		GetTempIO(), m_plist_set);

	if (!IsRecording()) {
		avf_sync();
	}

	return status;
}

avf_status_t CVDB::CollectAllRefClipsL()
{
	struct rb_node *node = rb_first(&m_clip_set.clip_set);
	while (node) {
		struct rb_node *next = rb_next(node);

		vdb_clip_t *clip = vdb_clip_t::from_node(node);
		CollectRefClipsL(clip);

		if (clip->IsNotUsed()) {
			// the clip is empty or bad, remove it
			AVF_LOGI(C_RED "remove clip %x" C_NONE, clip->clip_id);
			m_clip_set.RemoveClip(clip, this);
		}

		node = next;
	}
	return E_OK;
}

avf_status_t CVDB::CollectRefClipsL(vdb_clip_t *clip)
{
	list_head_t *node;
	list_head_t *next;

	list_for_each_del(node, next, clip->ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_clip_node(node);
		bool result;

		switch (ref_clip->ref_clip_type) {
		case CLIP_TYPE_BUFFER:
			result = m_buffered_rcs.AddUniqueRefClip(ref_clip, ref_clip_set_t::DEFAULT_LIST_POS);
			break;
		case CLIP_TYPE_MARKED:
			result = m_marked_rcs.AddUniqueRefClip(ref_clip, ref_clip_set_t::DEFAULT_LIST_POS);
			break;
		default: {
				ref_clip_set_t *rcs = GetPlaylist(ref_clip->ref_clip_type);
				if (rcs == NULL) {
					rcs = m_plist_set.CreatePlaylist(mb_server_mode, ref_clip->ref_clip_type);
					if (rcs == NULL) {
						result = false;
					} else {
						AVF_LOGD("create playlist %d", rcs->ref_clip_type);
						result = rcs->AddUniqueRefClip(ref_clip, ref_clip->plist_index);
					}
				} else {
					result = rcs->AddUniqueRefClip(ref_clip, ref_clip->plist_index);
				}
			}
			break;
		}

		if (!result) {
			avf_delete ref_clip;
			continue;
		}

		if (clip->m_cache) {
			clip->AddReference(ref_clip);
		}
	}

	return E_OK;
}

avf_status_t CVDB::ScanFileListL(ap<string_t>& path, CVdbFileList& list)
{
	avf_dir_t adir;
	struct dirent *ent;

	if (!avf_open_dir(&adir, path->string())) {
		AVF_LOGD("cannot open clip dir %s", path->string());
		return E_ERROR;
	}

	const char *file_ext = list.GetFileExt(0);
	const char *file_ext2 = list.GetFileExt(1);

	while ((ent = avf_read_dir(&adir)) != NULL) {
		if (avf_is_parent_dir(ent->d_name))
			continue;

		// remove dirs
		if (avf_path_is_dir(&adir, ent)) {
			AVF_LOGW("'%s' is a folder, remove", ent->d_name);
			ap<string_t> tmp(path.get(), ent->d_name);
			avf_remove_dir_recursive(tmp->string());
			continue;
		}

		avf_time_t local_time;
		int type = 0;
		if (!name_to_local_time(ent->d_name, &local_time, file_ext, file_ext2, &type)) {
			// ignore unknown files
			continue;
		}

		list.AddFile(path.get(), ent->d_name, local_time, type, false);
	}

	avf_close_dir(&adir);

#ifdef AVF_DEBUG
	list.Print(path->string(), false);
#endif

	return E_OK;
}

// after scan clip
// after a live segment is finished
void CVDB::RegisterSegmentFilesL(int stream, vdb_seg_t *seg)
{
	avf_u64_t old_filesize = seg->m_filesize;
	seg->m_filesize = 0;

	if (!mb_server_mode) {
		if (seg->m_video_name != AVF_INVALID_TIME) {
			seg->m_filesize += 
				GetVideoFileList(stream).AddFile(GetVideoDir(stream), NULL, seg->m_video_name, seg->GetVideoType(), true);
		}
		if (seg->m_index_name != AVF_INVALID_TIME) {
			seg->m_filesize += 
				GetIndexFileList(stream).AddFile(GetIndexDir(stream), NULL, seg->m_index_name, 0, true);
		}
		if (seg->m_picture_name != AVF_INVALID_TIME) {
			seg->m_filesize += 
				GetIndexPicFileList().AddFile(GetPictureDir(), NULL, seg->m_picture_name, 0, true);
		}
	}

	seg->AdjustFileSize(old_filesize);
}

avf_u64_t CVDB::GetFreeFileSpaceL()
{
	if (mb_server_mode)
		return 0;

	return m_index0_filelist.GetUnusedSize() +
		m_index1_filelist.GetUnusedSize() +
		m_indexpic_filelist.GetUnusedSize() +
		m_video0_filelist.GetUnusedSize() +
		m_video1_filelist.GetUnusedSize();
}

avf_u64_t CVDB::GetUsedFileSpaceL()
{
	if (mb_server_mode)
		return 0;

	return m_index0_filelist.GetUsedSize() +
		m_index1_filelist.GetUsedSize() +
		m_indexpic_filelist.GetUsedSize() +
		m_video0_filelist.GetUsedSize() +
		m_video1_filelist.GetUsedSize();
}

avf_u64_t CVDB::RemoveOldFilesL()
{
	if (mb_server_mode)
		return 0;

	__LOCK_IO__(LOCK_FOR_REMOVE_FILE, "remove old files");
	avf_u64_t total_size = 0;
	total_size += RemoveOneOldFileL(m_index0_path, m_index0_filelist);
	total_size += RemoveOneOldFileL(m_index1_path, m_index1_filelist);
	total_size += RemoveOneOldFileL(m_indexpic_path, m_indexpic_filelist);
	total_size += RemoveOneOldFileL(m_video0_path, m_video0_filelist);
	total_size += RemoveOneOldFileL(m_video1_path, m_video1_filelist);
	AVF_LOGD("RemoveOldFilesL: %d MB", (avf_u32_t)(total_size >> 20));
	return total_size;
}

avf_u64_t CVDB::RemoveOneOldFileL(const ap<string_t>& path, CVdbFileList& list)
{
	local_time_name_t lname;
	avf_u32_t size;

	if (list.RemoveUnusedFile(lname, &size)) {
		ap<string_t> fullname(path.get(), lname.buf);
		AVF_LOGD(C_LIGHT_PURPLE "remove old file %s" C_NONE, fullname->string());
		avf_remove_file(fullname->string());
		return size;
	}

	return 0;
}

bool CVDB::GetSpaceInfoL(space_info_t& si)
{
#ifdef MINGW
	return false;
#else
	struct statfs stfs;

	if (::statfs(m_mnt->string(), &stfs) < 0) {
		AVF_LOGE("statfs(%s) failed", m_mnt->string());
		return false;
	}

#ifdef WIN32_OS
	si.disk_space_b = (avf_u64_t)stfs.f_blocks * stfs.f_bsize;
	si.free_disk_b = (avf_u64_t)stfs.f_bfree * stfs.f_bsize;
#elif defined(MAC_OS)
	si.disk_space_b = (avf_u64_t)stfs.f_blocks * stfs.f_bsize;
	si.free_disk_b = (avf_u64_t)stfs.f_bfree * stfs.f_bsize;
#else
	si.disk_space_b = (avf_u64_t)stfs.f_blocks * stfs.f_frsize;
	si.free_disk_b = (avf_u64_t)stfs.f_bfree * stfs.f_frsize;
#endif

//	if (si.free_disk_b >= 7*1000*1000*1000UL) {
//		si.free_disk_b -= 7*1000*1000*1000UL;
//	}

	si.free_file_b = GetFreeFileSpaceL();
	si.used_file_b = GetUsedFileSpaceL();

	return true;
#endif
}

void CVDB::ReportSpaceInfoL()
{
	if (!m_clip_set.mbSpaceChanged)
		return;

	m_clip_set.mbSpaceChanged = false;

	if (mpBroadcast == NULL)
		return;

	space_info_t si;
	if (!GetSpaceInfoL(si))
		return;

//	AVF_LOGD("clip space: " LLD ", marked space: " LLD, m_clip_set.m_clip_space, m_clip_set.m_marked_space);

	mpBroadcast->ReportSpaceInfo(mpVdbId, si.disk_space_b, si.disk_space_b - si.free_disk_b, m_clip_set.m_marked_space);
}

avf_status_t CVDB::GetSpaceInfo(avf_u64_t *total_space, avf_u64_t *used_space, avf_u64_t *marked_space, avf_u64_t *clip_space)
{
	AUTO_LOCK(mMutex);

#if !defined(WIN32_OS) && !defined(MAC_OS)
	space_info_t si;
	if (!GetSpaceInfoL(si))
		return E_ERROR;

	*total_space = si.disk_space_b;
	*used_space = si.disk_space_b - si.free_disk_b;
#endif

	*marked_space = m_clip_set.m_marked_space;
	*clip_space = m_clip_set.m_clip_space;

	return E_OK;
}

avf_status_t CVDB::SetRecordAttr(int clip_attr)
{
	AUTO_LOCK(mMutex);
	mRecordAttr = clip_attr;	
	return E_OK;
}

avf_status_t CVDB::MarkLiveClip(int delay_ms, int before_live_ms, int after_live_ms, int b_continue, int clip_attr, int ch)
{
	AUTO_LOCK(mMutex);

	if (b_continue && mMarkInfo.state == mark_info_s::STATE_RUNNING) {

		CLiveInfo *li = GetLiveInfoL(ch);
		if (li == NULL) {
			AVF_LOGE("unexpected");
			return E_ERROR;
		}

		AVF_LOGD(C_YELLOW "update mark info: %d to %d" C_NONE,
			mMarkInfo.after_live_ms, after_live_ms);

		mMarkInfo.after_live_ms = after_live_ms;

		UpdateMarkLiveClipL(li);

	} else {

		// not continue, or not running
		StopMarkLiveClipL(true, ch);

		AVF_LOGD("MarkLiveClip, delay_ms=%d, before_live_ms=%d, after_live_ms=%d",
			delay_ms, before_live_ms, after_live_ms);

		mMarkInfo.state = mark_info_s::STATE_SET;
		mMarkInfo.delay_ms = delay_ms;
		mMarkInfo.before_live_ms = before_live_ms;
		mMarkInfo.after_live_ms = after_live_ms;
		mMarkInfo.mp_live_ref_clip = NULL;

		CLiveInfo *li = GetLiveInfoL(ch);
		if (li == NULL) {
			// recording not started
			return E_OK;
		}

		ref_clip_t *live_ref_clip = li->ref_clip;
		avf_u64_t ref_clip_end_time_ms = live_ref_clip->end_time_ms;

		avf_u64_t start_time_ms;
		avf_u64_t end_time_ms;

		if (before_live_ms < 0 || live_ref_clip->start_time_ms + before_live_ms >= ref_clip_end_time_ms) {
			start_time_ms = live_ref_clip->start_time_ms;
			mMarkInfo.before_live_ms = (int)(live_ref_clip->GetDurationLong());
		} else {
			start_time_ms = ref_clip_end_time_ms - mMarkInfo.before_live_ms;
		}
		end_time_ms = ref_clip_end_time_ms;

		int err;
		mMarkInfo.mp_live_ref_clip = m_marked_rcs.CreateRefClip(li->clip, start_time_ms, end_time_ms,
			&err, false, ref_clip_set_t::DEFAULT_LIST_POS, clip_attr);
		if (mMarkInfo.mp_live_ref_clip == NULL) {
			mMarkInfo.state = mark_info_s::STATE_NONE;
			return E_ERROR;
		}

		AVF_LOGD(C_YELLOW "MarkLiveClip, clipid: %x, start_time: " LLD ", duration: " LLD C_NONE,
			mMarkInfo.mp_live_ref_clip->ref_clip_id,
			mMarkInfo.mp_live_ref_clip->start_time_ms,
			mMarkInfo.mp_live_ref_clip->GetDurationLong());

		mMarkInfo.mp_live_ref_clip->SetLive();

		mMarkInfo.ref_clip_id = mMarkInfo.mp_live_ref_clip->ref_clip_id;
		mMarkInfo.state = mark_info_s::STATE_RUNNING;

		if (mpBroadcast) {
			mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_CREATED, mMarkInfo.mp_live_ref_clip, &mMarkInfo);
		}

		UpdateMarkLiveClipL(li);
	}

	ReportSpaceInfoL();

	return E_OK;
}

avf_status_t CVDB::StopMarkLiveClip(int ch)
{
	AUTO_LOCK(mMutex);
	StopMarkLiveClipL(true, ch);
	return E_OK;
}

avf_status_t CVDB::GetMarkLiveInfo(IVDBControl::mark_info_s& info, int ch)
{
	AUTO_LOCK(mMutex);
	info.b_running = mMarkInfo.state == mark_info_s::STATE_RUNNING;
	info.b_valid = info.b_running || mMarkInfo.state == mark_info_s::STATE_SET;
	info.delay_ms = mMarkInfo.delay_ms;
	info.before_live_ms = mMarkInfo.before_live_ms;
	info.after_live_ms = mMarkInfo.after_live_ms;
	info.clip_id = mMarkInfo.ref_clip_id;
	return E_OK;
}

void CVDB::StopMarkLiveClipL(bool bSaveClipInfo, int ch)
{
	if (mMarkInfo.state != mark_info_s::STATE_RUNNING)
		return;

	mMarkInfo.mp_live_ref_clip->end_time_ms = mMarkInfo.mp_live_ref_clip->end_time_ms;

	AVF_LOGD(C_YELLOW "StopMarkLiveClipL, clipid: %x, start_time: " LLD ", duration: " LLD C_NONE,
		mMarkInfo.mp_live_ref_clip->ref_clip_id,
		mMarkInfo.mp_live_ref_clip->start_time_ms,
		mMarkInfo.mp_live_ref_clip->GetDurationLong());

	if (bSaveClipInfo) {
		SaveClipInfoL(mMarkInfo.mp_live_ref_clip->clip);
	}

	if (mpBroadcast) {
		mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_FINISHED, mMarkInfo.mp_live_ref_clip, &mMarkInfo);
	}

	mMarkInfo.mp_live_ref_clip->ClearLive();
	mMarkInfo.state = mark_info_s::STATE_NONE;
}

void CVDB::UpdateMarkLiveClipL(CLiveInfo *li)
{
	if (mMarkInfo.state != mark_info_s::STATE_RUNNING)
		return;

	int inc_ms = (int)(li->ref_clip->end_time_ms - mMarkInfo.mp_live_ref_clip->end_time_ms);
	if (mMarkInfo.after_live_ms >= 0 && inc_ms > mMarkInfo.after_live_ms) {
		inc_ms = mMarkInfo.after_live_ms;
	}

	ref_clip_t *ref_clip = mMarkInfo.mp_live_ref_clip;

	if (inc_ms > 0) {
		mMarkInfo.before_live_ms += inc_ms;
		if (mMarkInfo.after_live_ms > 0) {
			mMarkInfo.after_live_ms -= inc_ms;
		}

		ref_clip->IncReference(inc_ms);

		if (mpBroadcast) {
			mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_CHANGED, ref_clip, &mMarkInfo);
		}
	}

	AVF_LOGD(C_CYAN "UpdateMarkLiveClipL, before=%d, after=%d, duration=" LLD C_NONE,
		mMarkInfo.before_live_ms, mMarkInfo.after_live_ms, ref_clip->GetDurationLong());

	if (mMarkInfo.after_live_ms == 0) {
		StopMarkLiveClipL(li, true);
	}
}

avf_status_t CVDB::StartRecordL(vdb_record_info_t *info, int ch)
{
	if (info) {
		info->valid = false;
	}

	// loaded?
	if (!mb_loaded) {
		AVF_LOGE("StartRecord: vdb not loaded");
		return E_ERROR;
	}

	// already recording?
	CLiveInfo *li = GetLiveInfoL(ch);
	if (li != NULL) {
		AVF_LOGE("StartRecord: already recording");
		return E_ERROR;
	}

	// create live info
	li = CLiveInfo::Create();
	SetLiveInfoL(ch, li);

	clip_dir_t *cdir = GetAvailDirL();
	avf_time_t start_record_time = GenerateClipNameL(cdir);

	// live clip
	li->clip = avf_new vdb_clip_t(this, &m_clip_set, cdir, start_record_time);
	li->clip->GenerateUUID();
	m_clip_set.AllocClipCache(li->clip);
	if (cdir) {
		cdir->ClipAdded();
	}

	li->clip->SetLive();
	li->clip->Lock();
	m_clip_set.AddClip(li->clip, true);

	// ref live clip
	li->ref_clip = m_buffered_rcs.NewLiveRefClip(li->clip, mb_server_mode, mRecordAttr);
	li->ref_clip->SetLive();

	AVF_LOGD(C_YELLOW "create live clip %x" C_NONE, li->ref_clip->ref_clip_id);

	mRecordAttr = 0;	// clear

	if (info) {
		ref_clip_t *ref_clip = li->ref_clip;
		info->valid = true;
		info->removed = false;
		info->dirname = ref_clip->m_dirname;
		info->ref_clip_id = ref_clip->ref_clip_id;
		info->ref_clip_date = ref_clip->ref_clip_date;
		info->gmtoff = ref_clip->clip->gmtoff;
		info->duration_ms = 0;
	}

	return E_OK;
}

// calc segment length according to tf card size
// to avoid too many files on large card
// 8 GB ~ 1 min
// 16 GB ~ 2 min
// 32 GB ~ 4 min
// 64 GB ~ 8 min
// 128 GB ~ 16 min
int CVDB::GetSegmentLength()
{
	AUTO_LOCK(mMutex);

	if (!mb_loaded) {
		AVF_LOGE("GetSegmentLength: vdb not loaded");
		return E_ERROR;
	}

	space_info_t si;
	if (!GetSpaceInfoL(si)) {
		return E_ERROR;
	}

	avf_u32_t size_gb = (avf_u32_t)(si.disk_space_b >> 30);

	if (size_gb <= 8)
		return 1*60;

	if (size_gb <= 16)
		return 2*60;

	if (size_gb <= 32)
		return 4*60;

	if (size_gb <= 64)
		return 8*60;

	//
	return 16*60;
}

avf_status_t CVDB::StartRecord(vdb_record_info_t *info, int ch)
{
	AVF_LOGD(C_GREEN "StartRecord" C_NONE);
	AUTO_LOCK(mMutex);
	return StartRecordL(info, ch);
}

avf_status_t CVDB::StopRecord(vdb_record_info_t *info, int ch)
{
	AVF_LOGD(C_GREEN "StopRecord" C_NONE);
	AUTO_LOCK(mMutex);
	return StopRecordL(info, ch);
}

avf_status_t CVDB::StopRecordL(vdb_record_info_t *info, int ch)
{
	if (info) {
		info->valid = false;
	}

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		return E_INVAL;
	}

	bool is_empty = li->clip->IsNotUsed();

	EndSegmentL(li, 0);
	EndSegmentL(li, 1);

	if (li->m_stream_0.HasSample() && li->m_stream_1.HasSample()) {
		AVF_LOGD(C_CYAN "stream length: " LLD ", " LLD C_NONE,
			li->m_stream_0.stream_end_time_ms, li->m_stream_1.stream_end_time_ms);
	}

	li->ref_clip->end_time_ms = li->AdjustClipLength();

	StopMarkLiveClipL(false, ch);

	SaveLiveClipInfoL(li, true);

	li->clip->Unlock();

	if (info) {
		ref_clip_t *ref_clip = li->ref_clip;
		info->valid = true;
		info->removed = false;
		info->dirname = ref_clip->m_dirname;
		info->ref_clip_id = ref_clip->ref_clip_id;
		info->ref_clip_date = ref_clip->ref_clip_date;
		info->gmtoff = ref_clip->clip->gmtoff;
		info->duration_ms = ref_clip->GetDurationLong();
	}

	if (mpBroadcast) {
		mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_FINISHED, li->ref_clip, NULL);
	}

	li->clip->ClearLive();
	li->ref_clip->ClearLive();

	if (is_empty) {
		AVF_LOGD(C_YELLOW "live clip is empty, remove" C_NONE);
		RemoveRefClipL(li->ref_clip, false);
		if (info) {
			info->removed = true;
		}
	}

	avf_sync();

	avf_safe_release(li);
	SetLiveInfoL(ch, NULL);

	ReportSpaceInfoL();

	return E_OK;
}

void CVDB::SaveLiveClipInfoL(CLiveInfo *li, bool bForce)
{
	if (li->m_save_counter > 0) {
		if (bForce) {
			li->m_save_counter = 0;
		} else {
			if (--li->m_save_counter > 0)
				return;
		}
		SaveClipInfoL(li->clip);
	}
}

string_t *CVDB::CreateVideoFileName(avf_time_t *gen_time, int stream, bool bUseGenTime, int ch, const char *ext)
{
	AUTO_LOCK(mMutex);
	return CreateVideoFileNameL(GetLiveInfoL(ch), gen_time, stream, bUseGenTime, ext);
}

string_t *CVDB::CreatePictureFileName(avf_time_t *gen_time, int stream, bool bUseGenTime, int ch)
{
	AUTO_LOCK(mMutex);
	return CreatePictureFileNameL(GetLiveInfoL(ch), gen_time, stream, bUseGenTime);
}

string_t *CVDB::CreateIndexFileName(avf_time_t *gen_time, int stream, bool bUseGenTime, int ch)
{
	AUTO_LOCK(mMutex);
	return CreateIndexFileNameL(GetLiveInfoL(ch), gen_time, stream, bUseGenTime);
}

avf_status_t CVDB::InitVideoStream(const avf_stream_attr_s *stream_attr, int video_type, int stream, int ch)
{
	if ((unsigned)stream >= 2)
		return E_ERROR;

	AVF_LOGD(C_GREEN "InitVideoStream(%d: %dx%d %s fps, %d hz %d channels)" C_NONE,
		stream, stream_attr->video_width, stream_attr->video_height,
		avf_get_framerate(stream_attr->video_framerate),
		stream_attr->audio_sampling_freq, stream_attr->audio_num_channels);

	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		AVF_LOGE("InitVideoStream: no live info");
		return E_ERROR;
	}

	// update stream attr
	li->clip->SetStreamAttr(stream, stream_attr);
	li->clip->SetVideoType(stream, video_type);

	// fix seg stream_attr
	live_stream_s& ls = li->GetLiveStream(stream);
	ls.has_video = true;
	ls.video_type = video_type;

	if (ls.ls_seg) {
		ls.ls_seg->UpdateStreamAttr();
	}

	return E_OK;
}

avf_status_t CVDB::SetLiveIO(IAVIO *io, ITSMapInfo *map_info, int stream, int ch)
{
	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		AVF_LOGE("SetLiveIO: no live info");
		return E_ERROR;
	}

	//AVF_LOGD(C_LIGHT_BLUE "SetLiveIO : %p %p stream %d" C_NONE, io, map_info, stream);

	live_stream_s& ls = li->GetLiveStream(stream);
	avf_assign(ls.si.video_io, io);
	avf_assign(ls.si.tsmap_info, map_info);

	return E_OK;
}

avf_status_t CVDB::InitClipDate(avf_u32_t date, avf_s32_t gmtoff, int ch)
{
	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		AVF_LOGE("InitVideoStream: no live info");
		return E_ERROR;
	}

	SetClipDate(li, date, gmtoff);

	return E_OK;
}

void CVDB::SetClipDate(CLiveInfo *li, avf_u32_t date, avf_s32_t gmtoff)
{
	// update clip date for once
	if (!li->mbClipDateSet) {
		li->mbClipDateSet = true;
		if (date == 0 && gmtoff == 0) {
			date = avf_get_sys_time(&gmtoff);
		}
		li->clip->clip_date = date;
		li->clip->gmtoff = gmtoff;
		li->ref_clip->ref_clip_date = date;
	}
}

avf_status_t CVDB::StartVideo(avf_time_t video_time, int stream, int ch)
{
	if ((unsigned)stream >= 2)
		return E_ERROR;

	if (video_time == AVF_INVALID_TIME) {
		AVF_LOGE("bad video_time %x", video_time);
		return E_ERROR;
	}

	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		AVF_LOGE("StartVideo: no live info");
		return E_ERROR;
	}

	// create segment if neccessary
	live_stream_s& ls = li->GetLiveStream(stream);
	if (ls.ls_seg == NULL) {
		CreateSegmentL(li, ls, stream);
	}

	AVF_LOGD(C_CYAN "StartVideo(%d), seg_start_ms=" LLD C_NONE, stream, ls.seg_start_time_ms);

	// video filename
	ls.ls_seg->m_video_name = video_time;

	// video array
	if (ls.si.sindex->arrays[kVideoArray] == NULL) {
		ls.si.sindex->arrays[kVideoArray] = CIndexArray::Create(kVideoArray);
	}

	// read raw data sent prev
	if (stream == 0) {
		SaveRawData(li, ls, true);
	}

	return E_OK;
}

avf_status_t CVDB::StartPicture(avf_time_t picture_time, int stream, int ch)
{
	if (stream != 0 || picture_time == AVF_INVALID_TIME) {
		//AVF_LOGE("bad stream or picture_time: %d, %x", stream, picture_time);
		return E_ERROR;
	}

	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		AVF_LOGE("StartPicture: no live info");
		return E_ERROR;
	}

	// create segment if neccessary
	live_stream_s& ls = li->GetLiveStream(stream);
	if (ls.ls_seg == NULL) {
		CreateSegmentL(li, ls, stream);
	}

	// picture filename
	ls.ls_seg->m_picture_name = picture_time;

	// picture array
	if (ls.si.sindex->arrays[kPictureArray] == NULL) {
		ls.si.sindex->arrays[kPictureArray] = CIndexArray::Create(kPictureArray);
	}

	// read raw data sent prev
	if (stream == 0) {
		SaveRawData(li, ls, false);
	}

	return E_OK;
}

void CVDB::SaveRawData(CLiveInfo *li, live_stream_s& ls, bool bVideo)
{
	CIndexArray **arrays = ls.si.sindex->arrays;
	for (avf_uint_t index = 0; index < ARRAY_SIZE(ls.si.sindex->arrays); index++) {
		CRawFifo *fifo = ls.si.fifos[index];
		if (fifo && ls.si.sample_count[index] == 0) {
			if (arrays[index] == NULL) {
				arrays[index] = CIndexArray::Create(index);
			}

			CIndexArray *array = arrays[index];
			CRawFifo::item_s *item = fifo->Pop();
			if (item != NULL) {
				while (true) {
					CRawFifo::item_s *next = fifo->Peek();
					if (next == NULL)
						break;
					if (next->rdata->ts_ms >= ls.seg_start_time_ms)
						break;
					item->Release();
					item = fifo->Pop();
				}

				SaveRawData(ls, index, array, item);
				int count = 1;

				while (true) {
					item = fifo->Pop();
					if (item == NULL)
						break;
					SaveRawData(ls, index, array, item);
					count++;
				}
			}
		}
	}

	if (bVideo) {
		avf_size_t i, n = li->raw_stream.count;
		CLiveInfo::raw_stream_s *rs = li->raw_stream.Item(0);
		for (i = 0; i < n; i++, rs++) {
			if (rs->last_raw) {
				DoAddRawDataEx(li, rs->last_raw, false);
			}
		}
	}
}

void CVDB::SaveRawData(live_stream_s& ls, int index, CIndexArray *array, CRawFifo::item_s *item)
{
	ls.has_data = true;
	ls.si.sample_count[index]++;

	avf_s32_t time_ms = (avf_s32_t)(item->rdata->ts_ms - ls.seg_start_time_ms);
	array->AddRawDataEntry(time_ms, item->rdata);
	UpdateRawSizeL(ls, index, item->rdata);

	item->Release();
}

// bLast==true: next_start_ms and fsize not used
// bSetLength: set next_start_ms and fsize
avf_status_t CVDB::FinishSegment(avf_u64_t next_start_ms, avf_u32_t fsize, int stream, bool bLast, int ch)
{
	if ((unsigned)stream >= 2)
		return E_OK;

	AVF_LOGD(C_GREEN "FinishSegment(%d), next_start_ms=" LLD C_NONE, stream, next_start_ms);
	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		AVF_LOGE("EndSegment: no live info");
		return E_ERROR;
	}

	live_stream_s& ls = li->GetLiveStream(stream);
	if (ls.ls_seg == NULL) {
		//AVF_LOGW("EndSegment: no segment");
		return E_ERROR;
	}

	if (!bLast) {

		UpdateVideoSizeL(ls, fsize);

		ls.si.length_video = next_start_ms - ls.seg_start_time_ms;
		UpdateSegmentLengthL(li, ls, stream, NULL);
	}

	// todo
	if (stream == 0) {
		list_head_t *node, *next;
		list_for_each_del(node, next, li->input_rq) {
			raw_data_t *raw = raw_data_t::from_node(node);
			if (raw->ts_ms < ls.stream_end_time_ms) {
				DoAddRawDataEx(li, raw, false);
				__list_del(node);
				raw->Release();
			}
		}
	}

	EndSegmentL(li, stream);

	return E_OK;
}

void CVDB::UpdateStreamSize(CLiveInfo *li, int stream, avf_u64_t end_time_ms, avf_u32_t v_fsize)
{
	if ((unsigned)stream >= 2)
		return;

	live_stream_s& ls = li->GetLiveStream(stream);
	if (ls.HasSample()) {
		if (ls.has_video && v_fsize > 0) {
			UpdateVideoSizeL(ls, v_fsize);
		}
		avf_u32_t length_ms = end_time_ms - ls.seg_start_time_ms;
		if (ls.has_video) {
			ls.si.length_video = length_ms;
		}
		if (ls.has_data) {
			ls.si.length_data = length_ms;
		}
		UpdateSegmentLengthL(li, ls, stream, NULL);
	}
}

avf_status_t CVDB::SegVideoLength(avf_u64_t end_time_ms, avf_u32_t v_fsize, int stream, int ch)
{
	if ((unsigned)stream >= 2)
		return E_ERROR;

	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		AVF_LOGE("SegVideoLength: no live info");
		return E_ERROR;
	}

	UpdateStreamSize(li, stream, end_time_ms, v_fsize);

	return E_OK;
}

avf_status_t CVDB::AddVideo(avf_u64_t pts, avf_u32_t timescale, avf_u32_t fpos,
	int stream, int ch, avf_u32_t *length)
{
	if ((unsigned)stream >= 2)
		return E_ERROR;

	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		AVF_LOGE("AddVideo: no live info");
		return E_ERROR;
	}

	// video array should be created by StartVideo()
	live_stream_s& ls = li->GetLiveStream(stream);
	if (ls.si.sindex == NULL || ls.si.sindex->arrays[kVideoArray] == NULL) {
		AVF_LOGE("video not started");
		return E_ERROR;
	}

	// calc length
	avf_u32_t length_ms;
	if (!ls.si.video_started) {
		ls.si.video_started = true;
		ls.si.video_start_pts = pts;
		length_ms = 0;
	} else {
		length_ms = (pts - ls.si.video_start_pts) * 1000 / timescale;
	}

	// add entry
	ls.has_video = true;
	ls.si.sample_count[kVideoArray]++;
	ls.si.sindex->arrays[kVideoArray]->AddEntry(length_ms, fpos);
	UpdateVideoSizeL(ls, fpos);

	// update length
	if (ls.si.length_video < length_ms) {
		ls.si.length_video = length_ms;
		UpdateSegmentLengthL(li, ls, stream, length);
	}

	// todo
	if (stream == 0) {
		list_head_t *node, *next;
		list_for_each_del(node, next, li->input_rq) {
			raw_data_t *raw = raw_data_t::from_node(node);
			if (raw->ts_ms < ls.stream_end_time_ms) {
				DoAddRawDataEx(li, raw, false);
				__list_del(node);
				raw->Release();
			}
		}
	}

	// check delayed save
	SaveLiveClipInfoL(li, false);

	// report space info every 1 second
	ReportSpaceInfoL();

	return E_OK;
}

avf_status_t CVDB::AddPicture(CBuffer *pBuffer, int stream, int ch, avf_u32_t *length)
{
	if (stream != 0) {
		AVF_LOGE("only stream 0 has picture");
		return E_ERROR;
	}

	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		AVF_LOGE("AddPicture: no live info");
		return E_ERROR;
	}

	// picture array should be created by StartPicture()
	live_stream_s& ls = li->GetLiveStream(stream);
	if (ls.si.sindex == NULL || ls.si.sindex->arrays[kPictureArray] == NULL) {
		AVF_LOGE("picture not started");
		return E_ERROR;
	}

	// write to file
	avf_u32_t bytes = pBuffer->GetDataSize();

	if (pBuffer->GetMem()) {
		// create picture file
		if (ls.si.picture_io == NULL) {
			ls.si.picture_io = CBufferIO::Create();
		}

		if (!ls.si.picture_io->IsOpen()) {
			string_t *filename = ls.ls_seg->GetPictureFileName();
			ls.si.picture_io->CreateFile(filename->string());
			filename->Release();
		}

		ls.si.picture_io->Write(pBuffer->GetData(), bytes);
		CBuffer *pNext = pBuffer->mpNext;
		if (pNext) {
			avf_u32_t bytes_next = pNext->GetDataSize();
			ls.si.picture_io->Write(pNext->GetData(), bytes_next);
			bytes += bytes_next;
		}
	}

	// calc length
	avf_u32_t length_ms;
	if (!ls.si.picture_started) {
		ls.si.picture_started = true;
		ls.si.picture_start_pts = pBuffer->m_pts;
		length_ms = 0;
		// set first picture size for server use
		ls.ls_seg->m_first_pic_size = bytes;
		if (ls.ls_seg->IsStartSeg()) {
			li->clip->first_pic_name = ls.ls_seg->m_picture_name;
			li->clip->first_pic_size = bytes;
		}
	} else {
		length_ms = (pBuffer->m_pts - ls.si.picture_start_pts) * 1000 / pBuffer->GetTimeScale();
	}

	// add entry
	ls.has_data = true;
	ls.si.sample_count[kPictureArray]++;
	ls.si.sindex->arrays[kPictureArray]->AddEntry(length_ms, ls.si.picture_size);
	ls.si.picture_size += bytes;
	UpdatePictureSizeL(ls, bytes);

	// update length
	if (ls.si.length_data < length_ms) {
		ls.si.length_data = length_ms;
		UpdateSegmentLengthL(li, ls, stream, length);
	}

	return E_OK;
}

avf_status_t CVDB::AddRawData(raw_data_t *raw, int stream, int ch, avf_u32_t *length)
{
	if (stream != 0) {
		AVF_LOGE("only stream 0 has raw data");
		return E_ERROR;
	}

	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		// AVF_LOGE("AddRawData: no live info");
		return E_ERROR;
	}

	// this is the stream 0
	live_stream_s& ls = li->GetLiveStream(stream);

	// create segment if neccessary
	if (ls.ls_seg == NULL) {
		CreateSegmentL(li, ls, stream);
	}

	int index;
	switch (raw->GetFCC()) {
	case IRecordObserver::GPS_DATA:
		index = kGps0Array;
		break;

	case IRecordObserver::ACC_DATA:
		index = kAcc0Array;
		break;

	case IRecordObserver::OBD_DATA:
		index = kObd0Array;
		break;

	case IRecordObserver::VMEM_DATA:
		index = kVmemArray;
		break;

	default:
		AVF_LOGE("unknown raw data: 0x%x", raw->GetFCC());
		return E_UNKNOWN;
	}

	// save to fifo
	if (ls.si.fifos[index] == NULL) {
		ls.si.fifos[index] = CRawFifo::Create();
	}
	ls.si.fifos[index]->Push(raw);

	if (ls.si.sindex->arrays[index] == NULL) {
		ls.si.sindex->arrays[index] = CIndexArray::Create(index);
	}

	// add raw data
	avf_s32_t time_ms = raw->ts_ms - ls.seg_start_time_ms;

	ls.has_data = true;
	ls.si.sample_count[index]++;
	ls.si.sindex->arrays[index]->AddRawDataEntry(time_ms, raw);
	UpdateRawSizeL(ls, index, raw);

	// update length
	if ((avf_s32_t)ls.si.length_data < ++time_ms) {
		ls.si.length_data = time_ms;
		UpdateSegmentLengthL(li, ls, stream, length);
	}

	return E_OK;
}

avf_status_t CVDB::SetConfigData(raw_data_t *raw, int ch)
{
	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		return E_ERROR;
	}

	switch (raw->GetFCC()) {
	case IRecordObserver::VIN_DATA:
		li->clip->SaveObdVin(raw->GetData(), raw->GetSize());
		return E_OK;

	case IRecordObserver::VIN_CONFIG:
		li->clip->SaveVinConfig(raw->GetData(), raw->GetSize());
		return E_OK;

	default:
		return E_ERROR;
	}
}

avf_status_t CVDB::AddRawDataEx(raw_data_t *raw, int ch)
{
	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li == NULL) {
		// AVF_LOGE("AddRawData: no live info");
		return E_ERROR;
	}

	// this is the stream 0
	live_stream_s& ls = li->GetLiveStream(0);

	// create segment if neccessary
	if (ls.ls_seg == NULL) {
		CreateSegmentL(li, ls, 0);
	}

	if (!ls.has_video || raw->ts_ms < ls.stream_end_time_ms) {
		return DoAddRawDataEx(li, raw, true);
	}

	raw->AddRef();
	list_add_tail(&raw->node, li->input_rq);

	return E_OK;
}

avf_status_t CVDB::DoAddRawDataEx(CLiveInfo *li, raw_data_t *raw, bool bUpdateLength)
{
	CLiveInfo::raw_stream_s *rs = li->raw_stream.Item(0);
	avf_size_t count = li->raw_stream.count;
	avf_size_t i;

	for (i = 0; i < count; i++, rs++) {
		if (raw->fcc == rs->fcc)
			break;
	}

	live_stream_s& ls = li->GetLiveStream(0);
	CIndexArray *array;

	if (i == count) {
		// clip entry
		rs = li->raw_stream._Append(NULL);
		rs->fcc = raw->fcc;
		rs->last_raw = NULL;

		// seg index
		array = CIndexArray::CreateBlockData();
		ls.si.sindex->ex_raw_arrays._Append(&array);

		//
		li->clip->AddRawFCC(raw->fcc);
		ls.ls_seg->AddNewExtraRaw();
	} else {
		array = *ls.si.sindex->ex_raw_arrays.Item(i);
	}

	// add raw data
	avf_s32_t time_ms = raw->ts_ms - ls.seg_start_time_ms;

	// update length
	if ((avf_s32_t)ls.si.length_data < time_ms) {
		ls.si.length_data = time_ms;
		if (bUpdateLength) {
			UpdateSegmentLengthL(li, ls, 0, NULL);
		}
	}

	ls.has_data = true;

	avf_assign(rs->last_raw, raw);
	array->AddRawDataEntryEx(time_ms, ls.ls_seg->GetDuration_ms(), raw);

	UpdateRawSizeExL(ls, i, raw);

	return E_OK;
}

avf_status_t CVDB::SetRawDataFreq(int interval_gps, int interval_acc, int interval_obd, int ch)
{
	AUTO_LOCK(mMutex);

	CLiveInfo *li = GetLiveInfoL(ch);
	if (li) {
		vdb_clip_t *clip = li->clip;
		if (clip) {
			AVF_LOGD("gps: %d, acc: %d, obd: %d", interval_gps, interval_acc, interval_obd);
			clip->raw_info.interval_gps = interval_gps;
			clip->raw_info.interval_acc = interval_acc;
			clip->raw_info.interval_obd = interval_obd;
		}
	}

	return E_OK;
}

avf_status_t CVDB::GetDataSize(avf_u64_t *video_size, avf_u64_t *picture_size, avf_u64_t *raw_data_size)
{
	AUTO_LOCK(mMutex);
	*video_size = m_clip_set.GetVideoDataSize();
	*picture_size = m_clip_set.GetPictureDataSize();
	*raw_data_size = m_clip_set.GetRawDataSize();
	return E_OK;
}

avf_status_t CVDB::SetClipSceneData(int clip_type, avf_u32_t clip_id, const avf_u8_t *data, avf_size_t data_size)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = FindRefClipL(clip_type, clip_id);
	if (ref_clip == NULL) {
		AVF_LOGE("SetClipSceneData: clip not found");
		return E_INVAL;
	}

	raw_data_t *scene_data = avf_alloc_raw_data(data_size, MKFCC('S','C','E','N'));
	if (scene_data == NULL) {
		return E_NOMEM;
	}

	::memcpy(scene_data->GetData(), data, data_size);
	ref_clip->SetSceneData(scene_data);

	scene_data->Release();

	if (ref_clip->IsLive()) {
		AVF_LOGD("SetClipSceneData, live clip");
	} else {
		SaveClipInfoL(ref_clip->clip);
		AVF_LOGD("SetClipSceneData done");
	}

	return E_OK;
}

void CVDB::CreateSegmentL(CLiveInfo *li, live_stream_s& ls, int stream)
{
	vdb_stream_t *vdb_stream = li->clip->GetStream(stream);

	// init seg info
	vdb_seg_t::info_t info;
	ls.InitSegment(vdb_stream, info);

	// create segment
	avf_time_t local_time;
	string_t *index_filename = CreateIndexFileNameL(li, &local_time, stream, false);
	index_filename->Release();

	// video_name & picture_name are set later
	ls.ls_seg = vdb_stream->AddSegment(AVF_INVALID_TIME,
		local_time, AVF_INVALID_TIME, AVF_INVALID_TIME,
		&info);

	if (!li->ref_clip->AutoDelete()) {
		ls.ls_seg->m_mark_count++;
	}

	m_clip_set.AllocSegCache(ls.ls_seg);
	m_clip_set.LockSegCache(ls.ls_seg);
	ls.si.sindex = &ls.ls_seg->m_cache->sindex;

	// addref: for buffered ref clip
	ls.ls_seg->AddRef();

	// avoid being removed by checking space, the temp reference will be removed in FinishSegment()
	ls.ls_seg->AddRef();

	avf_size_t i, n = li->clip->m_extra_raw_fcc.count;
	for (i = 0; i < n; i++) {
		CIndexArray *array = CIndexArray::CreateBlockData();
		ls.si.sindex->ex_raw_arrays._Append(&array);
		ls.ls_seg->AddNewExtraRaw();
	}

	// update segments count
	vdb_clip_t *clip = ls.ls_seg->m_clip;
	clip_dir_t *cdir = clip->m_cdir;

	if (stream == 0) {
		clip->m_num_segs_0++;
		if (cdir) {
			cdir->SegmentAdded(1, 0);
		}
	} else {
		clip->m_num_segs_1++;
		if (cdir) {
			cdir->SegmentAdded(0, 1);
		}
	}
}

void CVDB::EndSegmentL(CLiveInfo *li, int stream)
{
	live_stream_s& ls = li->GetLiveStream(stream);

	avf_safe_release(ls.si.video_io);
	avf_safe_release(ls.si.tsmap_info);

	if (ls.ls_seg == NULL)
		return;

	avf_u32_t duration = ls.ls_seg->GetDuration_ms();
	if (duration == 0) {
		AVF_LOGI("stream %d segment length is 0", stream);
	} else {
		for (avf_size_t i = 0; i < kTotalArray; i++) {
			avf_u32_t data_size = ls.ls_seg->m_info.seg_data_size[i];
			if (data_size > 0) {
				if (ls.si.sindex) {
					CIndexArray *array = ls.si.sindex->arrays[i];
					if (array) {
						array->UpdateTailItem(data_size, duration);
					} else {
						AVF_LOGW("array[%d] is null", i);
					}
				} else {
					AVF_LOGW("no sindex %d", i);
				}
			}
		}

		// save seg-index
		string_t *filename = ls.ls_seg->GetIndexFileName();
		CVdbFormat::SaveSegmentIndex(li->clip, filename, GetTempIO(), stream, ls.si.sindex);
		filename->Release();
	}

	li->m_save_counter = CLIP_SAVE_DELAY;

	vdb_seg_t *seg = ls.ls_seg;

	m_clip_set.UnlockSegCache(ls.ls_seg);
	ls.si.sindex = NULL;

	ls.FinishSegment();

	RegisterSegmentFilesL(stream, seg);
}

void CVDB::UpdateVideoSizeL(live_stream_s& ls, avf_u32_t fsize)
{
	avf_u32_t inc_size = fsize - ls.ls_seg->m_info.seg_data_size[kVideoArray];
	ls.ls_seg->IncFileSize(inc_size);
	ls.ls_seg->m_info.seg_data_size[kVideoArray] = fsize;
	ls.ls_seg->m_clip->IncVideoDataSize_clip(ls.ls_seg->m_stream, inc_size);
}

void CVDB::UpdatePictureSizeL(live_stream_s& ls, avf_u32_t bytes)
{
	ls.ls_seg->IncFileSize(bytes);
	ls.ls_seg->m_info.seg_data_size[kPictureArray] += bytes;
	ls.ls_seg->m_clip->IncPictureDataSize_clip(ls.ls_seg->m_stream, bytes);
}

void CVDB::UpdateRawSizeL(live_stream_s& ls, int index, raw_data_t *raw)
{
	avf_u32_t inc_size = sizeof(raw_dentry_t) + raw->GetSize();
	ls.ls_seg->IncFileSize(inc_size);
	if (index < kTotalArray) {
		ls.ls_seg->m_info.seg_data_size[index] += raw->GetSize();
	} else {
		ls.ls_seg->extra_data_size[index - kTotalArray] += raw->GetSize();
	}
	avf_u32_t raw_size = CIndexArray::RawDataSize(raw);
	ls.ls_seg->m_info.raw_data_size += raw_size;
	ls.ls_seg->m_clip->IncRawDataSize_clip(ls.ls_seg->m_stream, raw_size);
}

void CVDB::UpdateRawSizeExL(live_stream_s& ls, int index, raw_data_t *raw)
{
	avf_u32_t inc_size = sizeof(raw_dentry_t) + raw->GetSize();
	ls.ls_seg->IncFileSize(inc_size);
	ls.ls_seg->IncRawSize(index, inc_size);
	avf_u32_t raw_size = CIndexArray::RawDataSize(raw);
	ls.ls_seg->m_info.raw_data_size += raw_size;
	ls.ls_seg->m_clip->IncRawDataSize_clip(ls.ls_seg->m_stream, raw_size);
}

void CVDB::UpdateSegmentLengthL(CLiveInfo *li, live_stream_s& ls, int stream, avf_u32_t *p_seg_length)
{
	avf_u32_t seg_length;

	seg_length = ls.has_video ? ls.si.length_video : ls.si.length_data;

	// should increase mono
	if (seg_length <= ls.ls_seg->GetDuration_ms())
		return;

	if (p_seg_length) {
		*p_seg_length = seg_length;
	}

	// update segment length
	ls.ls_seg->SetDuration_ms(seg_length);
	ls.stream_end_time_ms = ls.ls_seg->GetEndTime_ms();

	// update clip length
	// get min end time of the 2 streams, use it as clip end time
	avf_u64_t clip_end_time_ms = li->AdjustClipLength();
	ref_clip_t *live_ref_clip = li->ref_clip;

	if (live_ref_clip->end_time_ms < clip_end_time_ms) {
		live_ref_clip->end_time_ms = clip_end_time_ms;

		if (mpBroadcast) {
			if (!li->mbClipCreated) {
				mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_CREATED, live_ref_clip, NULL);
			} else {
				mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_CHANGED, live_ref_clip, NULL);
			}
		}
		li->mbClipCreated = true;

		if (mMarkInfo.state != mark_info_s::STATE_NONE) {
			mMarkInfo.state = mark_info_s::STATE_RUNNING;
			UpdateMarkLiveClipL(li);
		}
	}
}

clip_dir_t *CVDB::AddDirL(avf_time_t dirname, bool bCreate, bool *bExists)
{
	if (!mb_server_mode) {
		return NULL;
	}
	return m_cdir_set.AddClipDir(m_vdb_root.get(), dirname, bCreate, bExists);
}

clip_dir_t *CVDB::GetAvailDirL()
{
	if (!mb_server_mode) {
		return NULL;
	}

	clip_dir_t *cdir = m_cdir_set.GetAvail(MAX_SEGMENTS_PER_DIR);
	if (cdir) {
		return cdir;
	}

	// create a new cdir
	ap<string_t> path;
	avf_time_t local_time;

	// find a dir name which does not exist
	for (avf_uint_t incre = 0; ; incre++) {
		local_time_name_t lname;

		local_time = avf_get_curr_time(incre);
		local_time_to_name(local_time, PATH_SEP, lname);
		path = ap<string_t>(m_vdb_root.get(), lname.buf);

		if (!avf_file_exists(path->string())) {
			break;
		}
	}

	__LOCK_IO__(LOCK_FOR_CREATE_DIR, "create cdir");

	AVF_LOGD(C_YELLOW "create cdir %s" C_NONE, path->string());
	avf_mkdir(path->string());
	ap<string_t> tmp;

	tmp = ap<string_t>(path.get(), CLIPINFO_DIR);
	avf_mkdir(tmp->string());

	tmp = ap<string_t>(path.get(), INDEX0_DIR);
	avf_mkdir(tmp->string());

	tmp = ap<string_t>(path.get(), INDEX1_DIR);
	avf_mkdir(tmp->string());

	tmp = ap<string_t>(path.get(), INDEXPIC_DIR);
	avf_mkdir(tmp->string());

	tmp = ap<string_t>(path.get(), VIDEO0_DIR);
	avf_mkdir(tmp->string());

	tmp = ap<string_t>(path.get(), VIDEO1_DIR);
	avf_mkdir(tmp->string());

	return AddDirL(local_time, true);
}

// TODO - use queue for delayed save
void CVDB::SaveClipInfoL(vdb_clip_t *clip)
{
	clip_dir_t *cdir = clip->m_cdir;
	if (cdir) {
		string_t *path = cdir->CalcPathName(CLIPINFO_DIR);
		CVdbFormat::SaveClipInfo(path, clip, GetTempIO());
		path->Release();
	} else {
		m_clip_set.m_clipinfo_space -= clip->m_clipinfo_filesize;
		CVdbFormat::SaveClipInfo(GetClipDir(), clip, GetTempIO());
		m_clip_set.m_clipinfo_space += clip->m_clipinfo_filesize;
	}

	if (!IsRecording()) {
		avf_sync();
	}
}

// generate clipinfo filename: 2004.01.10-14.09.20
avf_time_t CVDB::GenerateClipNameL(clip_dir_t *cdir, avf_time_t *gen_time)
{
	for (avf_uint_t incre = 0; ; incre++) {
		local_time_name_t lname;
		avf_time_t local_time;

		if (gen_time) {
			local_time = avf_time_add(*gen_time, incre);
		} else {
			local_time = avf_get_curr_time(incre);
		}

		local_time_to_name(local_time, CLIPINFO_FILE_EXT, lname);

		if (cdir) {
			string_t *fullname = cdir->CalcFileName(CLIPINFO_DIR, lname);
			bool bExists = avf_file_exists(fullname->string());

			if (!bExists) {
				avf_touch_file(fullname->string());
				fullname->Release();
				return local_time;
			}

			fullname->Release();

		} else {

			ap<string_t> path(GetClipDir(), lname.buf);
			if (!avf_file_exists(path->string())) {
				avf_touch_file(path->string());
				return local_time;
			}
		}
	}
}

void CVDB::RemoveClipNameL(clip_dir_t *cdir, avf_time_t local_time)
{
	local_time_name_t lname;
	local_time_to_name(local_time, CLIPINFO_FILE_EXT, lname);
	if (cdir) {
		string_t *fullname = cdir->CalcFileName(CLIPINFO_DIR, lname);
		avf_remove_file(fullname->string(), false);
		fullname->Release();
	} else {
		ap<string_t> path(GetClipDir(), lname.buf);
		avf_remove_file(path->string());
	}
}

string_t *CVDB::CreateVideoFileNameL(CLiveInfo *li, avf_time_t *gen_time, int stream, bool bUseGenTime, const char *ext)
{
	clip_dir_t *cdir = li ? li->clip->m_cdir : NULL;
	return m_pf_video.CreateFileName(cdir, stream, gen_time, bUseGenTime, ext);
}

string_t *CVDB::CreatePictureFileNameL(CLiveInfo *li, avf_time_t *gen_time, int stream, bool bUseGenTime)
{
	clip_dir_t *cdir = li ? li->clip->m_cdir : NULL;
	return m_pf_picture.CreateFileName(cdir, stream, gen_time, bUseGenTime);
}

string_t *CVDB::CreateIndexFileNameL(CLiveInfo *li, avf_time_t *gen_time, int stream, bool bUseGenTime)
{
	clip_dir_t *cdir = li ? li->clip->m_cdir : NULL;
	return m_pf_index.CreateFileName(cdir, stream, gen_time, bUseGenTime);
}

// called by vdb_seg_t::GetIndexFileName()
string_t *CVDB::GetIndexFileName(const vdb_seg_t *seg)
{
	return m_pf_index.GetFileName(seg, seg->m_index_name, 0);
}

// called by vdb_seg_t::GetPictureFileName()
string_t *CVDB::GetPictureFileName(const vdb_seg_t *seg)
{
	return m_pf_picture.GetFileName(seg, seg->m_picture_name, 0);
}

// called by vdb_seg_t::GetVideoFileName()
string_t *CVDB::GetVideoFileName(const vdb_seg_t *seg)
{
	return m_pf_video.GetFileName(seg, seg->m_video_name, seg->GetVideoType());
}

void CVDB::RemoveIndexFile(clip_dir_t *cdir, int stream, avf_time_t index_name)
{
	m_pf_index.RemoveFile(cdir, stream, index_name, 0);
}

void CVDB::RemoveVideoFile(clip_dir_t *cdir, int stream, avf_time_t video_name, int type)
{
	m_pf_video.RemoveFile(cdir, stream, video_name, type);
}

void CVDB::RemovePictureFile(clip_dir_t *cdir, int stream, avf_time_t picture_name)
{
	m_pf_picture.RemoveFile(cdir, stream, picture_name, 0);
}

void CVDB::RemoveIndexFile(const vdb_seg_t *seg)
{
	m_pf_index.RemoveFile(seg, seg->m_index_name, 0);
}

void CVDB::RemoveVideoFile(const vdb_seg_t *seg)
{
	m_pf_video.RemoveFile(seg, seg->m_video_name, seg->GetVideoType());
}

void CVDB::RemovePictureFile(const vdb_seg_t *seg)
{
	m_pf_picture.RemoveFile(seg, seg->m_picture_name, 0);
}

avf_status_t CVDB::OpenSegmentVideo(const vdb_seg_t *seg, IAVIO **pio)
{
//	AUTO_LOCK(mMutex);

	//AVF_LOGD(C_LIGHT_BLUE "OpenSegmentVideo, seg: %p, seg_id: %d" C_NONE, seg, seg->m_info.id);

	for (unsigned i = 0; i < NUM_LIVE_CH; i++) {
		CLiveInfo *li = GetLiveInfoL(i);
		if (li != NULL) {
			live_stream_s& ls = li->GetLiveStream(seg->m_stream);
			//AVF_LOGD(C_LIGHT_BLUE "ls_seg: %p, seg_id: %d" C_NONE, ls.ls_seg, ls.ls_seg->m_info.id);
			if (seg == ls.ls_seg) {
				if (ls.si.tsmap_info) {
					AVF_LOGD(C_CYAN "use live io & tsmap" C_NONE);

					CTSMapIO *tsmap_io = CTSMapIO::Create();
					if (tsmap_io == NULL)
						return E_NOMEM;

					avf_status_t status = tsmap_io->OpenTSMap(ls.si.video_io, ls.si.tsmap_info);
					if (status != E_OK) {
						avf_safe_release(tsmap_io);
						return status;
					}

					*pio = tsmap_io;
					return E_OK;

				} else if (ls.si.video_io) {

					AVF_LOGD(C_CYAN "use live io" C_NONE);
					*pio = ls.si.video_io;
					ls.si.video_io->AddRef();
					return E_OK;

				}

				goto Default;
			}
		}
	}

Default:
	AVF_LOGD(C_GREEN "open existing file" C_GREEN);
	return seg->OpenVideoRead(pio);
}

// called by clip_set_t::RemoveClip()
void CVDB::RemoveClipFiles(vdb_clip_t *clip)
{
#ifdef AVF_DEBUG
	if (!clip->IsNotUsed()) {
		AVF_LOGW("remove clip files: num_segs=%d,%d", clip->m_num_segs_0, clip->m_num_segs_1);
	}
#endif

	local_time_name_t lname;
	local_time_to_name(clip->clip_id, CLIPINFO_FILE_EXT, lname);

	clip_dir_t *cdir = clip->m_cdir;
	if (cdir == NULL) {
		ap<string_t> fullname(GetClipDir(), lname.buf);
		__LOCK_IO__(LOCK_FOR_REMOVE_FILE, "remove clipinfo");
		avf_remove_file(fullname->string(), false);
	} else {
		string_t *fullname = cdir->CalcFileName(CLIPINFO_DIR, lname);
		__LOCK_IO__(LOCK_FOR_REMOVE_FILE, "remove clipinfo");
		m_clip_set.m_clipinfo_space -= avf_get_file_size(fullname->string(), false);
		avf_remove_file(fullname->string(), false);
		fullname->Release();
	}
}

void CVDB::ClipRemoved(clip_dir_t *cdir)
{
	if (cdir) {
		cdir->ClipRemoved();
		if (cdir->IsEmpty()) {
			// rm dir
			string_t *path = cdir->CalcPathName("");
			AVF_LOGD(C_YELLOW "no clips in %s" C_NONE, path->string());
			avf_remove_dir_recursive(path->string());
			path->Release();

			// free cdir
			m_cdir_set.RemoveClipDir(cdir);
		}
	}
}

avf_status_t CVDB::DeleteClip(int clip_type, avf_u32_t clip_id)
{
	vdb_cmd_DeleteClip_t cmd;
	cmd.clip_type = clip_type;
	cmd.clip_id = clip_id;

	avf_u32_t error = 0;
	avf_status_t status = CmdDeleteClip(&cmd, &error);

	if (status != E_OK)
		return status;
	if (error != e_DeleteClip_OK)
		return E_ERROR;

	return E_OK;
}

avf_status_t CVDB::GetNumClips(int clip_type, avf_u32_t *num_clips, avf_u32_t *sid)
{
	AUTO_LOCK(mMutex);

	ref_clip_set_t *rcs = GetRCS(clip_type);
	if (rcs == NULL) {
		*num_clips = 0;
	} else {
		*num_clips = rcs->mTotalRefClips;
		for (unsigned i = 0; i < NUM_LIVE_CH; i++) {
			CLiveInfo *li = GetLiveInfoL(i);
			if (li && li->ref_clip && li->ref_clip->GetDurationLong() == 0)
				(*num_clips)--;

		}
	}

	*sid = m_sid;
	return E_OK;
}

avf_status_t CVDB::GetClipSetInfo(int clip_type, clip_set_info_s *info, avf_u32_t start, avf_u32_t *num, avf_u32_t *sid)
{
	AUTO_LOCK(mMutex);
	*sid = m_sid;

	if (*num) {
		ref_clip_set_t *rcs = GetRCS(clip_type);
		ref_clip_t *ref_clip = rcs ? rcs->GetFirstRefClip() : NULL;
		avf_u32_t total = 0;

		for (; ref_clip; ref_clip = ref_clip->GetNextRefClip()) {
			if (ref_clip->IsLive() && ref_clip->GetDurationLong() == 0) {
				continue;
			}
			if (start) {
				// skip the first 'start' clips
				start--;
			} else {
				info->ref_clip_id = ref_clip->ref_clip_id;
				info->ref_clip_date = ref_clip->ref_clip_date;
				info->clip_time_ms = ref_clip->start_time_ms;
				info->length_ms = ref_clip->GetDurationLong();

				info++;
				total++;

				if (--*num == 0)
					break;
			}
		}

		*num = total;
	}

	return E_OK;
}

void CVDB::FetchRefClipInfo(ref_clip_t *ref_clip, int flags, bool bHasExtra, const char *vdb_id, CMemBuffer *pmb)
{
	// vdb_clip_info_t

	pmb->write_le_32(ref_clip->ref_clip_id);	// clip_id
	pmb->write_le_32(ref_clip->ref_clip_date);	// clip_date
	pmb->write_le_32(ref_clip->GetDurationLong());	// clip_length_ms
	pmb->write_le_64(ref_clip->start_time_ms);

	vdb_clip_t *clip = ref_clip->clip;

	avf_uint_t num_streams = clip->m_num_segs_1 > 0 ? 2 : 1;
	pmb->write_le_16(num_streams);

	avf_u8_t *out_flags_ptr = NULL;

	if (bHasExtra) {
		out_flags_ptr = pmb->Alloc(2);
	} else {
		pmb->write_le_16(0);
	}

	const avf_stream_attr_s& stream_attr = clip->GetStreamAttr(0);
	pmb->write_mem((const avf_u8_t*)&stream_attr, sizeof(avf_stream_attr_s));

	if (num_streams > 1) {
		const avf_stream_attr_s& stream_attr = clip->GetStreamAttr(1);
		pmb->write_mem((const avf_u8_t*)&stream_attr, sizeof(avf_stream_attr_s));
	}

	// vdb_clip_info_ex_t

	if (bHasExtra) {
		pmb->write_le_32(ref_clip->ref_clip_type);

		int out_flags = 0;
		avf_u8_t *extra_size_ptr = pmb->Alloc(4);
		avf_size_t extra_size = 0;

		if (flags & GET_CLIP_EXTRA) {
			avf_u8_t *uuid = pmb->Alloc(UUID_LEN);
			clip->GetUUID(uuid);

			pmb->write_le_32(ref_clip->ref_clip_date);
			pmb->write_le_32(clip->gmtoff);
			pmb->write_le_32(clip->clip_id);

			out_flags |= GET_CLIP_EXTRA;
			extra_size += UUID_LEN + (3 * 4);
		}

		if (flags & GET_CLIP_DESC) {
			extra_size += clip->GetClipDesc(pmb);
			out_flags |= GET_CLIP_DESC;
		}

		if (flags & GET_CLIP_ATTR) {
			extra_size += 4;
			pmb->write_le_32(ref_clip->clip_attr);
			out_flags |= GET_CLIP_ATTR;
		}

		if (flags & GET_CLIP_SIZE) {
			extra_size += 8;
			avf_u64_t filesize = ref_clip->GetFileSize();
			pmb->write_le_32(U64_LO(filesize));
			pmb->write_le_32(U64_HI(filesize));
			out_flags |= GET_CLIP_SIZE;
		}

		if ((flags & GET_CLIP_SCENE_DATA) && ref_clip->m_scene_data) {
			raw_data_t *raw = ref_clip->m_scene_data;
			extra_size += 4 + raw->GetSize();
			pmb->write_le_32(raw->GetSize());
			pmb->write_mem(raw->GetData(), raw->GetSize());
			out_flags |= GET_CLIP_SCENE_DATA;
		}

		if (flags & GET_CLIP_RAW_FREQ) {
			raw_info_t *raw_info = &clip->raw_info;
			extra_size += 16;
			pmb->write_le_32(raw_info->interval_gps);
			pmb->write_le_32(raw_info->interval_acc);
			pmb->write_le_32(raw_info->interval_obd);
			pmb->write_le_32(0);
			out_flags |= GET_CLIP_RAW_FREQ;
		}

		if (vdb_id) {
			extra_size += pmb->write_string_aligned(vdb_id);
			out_flags |= GET_CLIP_VDB_ID;
		}

		SAVE_LE_16(out_flags_ptr, out_flags);
		SAVE_LE_32(extra_size_ptr, extra_size);
	}
}

avf_status_t CVDB::FetchIndexPictureL(vdb_seg_t *seg, 
	const index_pos_t& pos, avf_u32_t bytes,
	bool bWriteSize, CMemBuffer *pmb, avf_u8_t *buffer)
{
	if (pmb) {
		if (bWriteSize) {
			pmb->write_le_32(bytes);
		}
		if ((buffer = pmb->Alloc(bytes)) == NULL) {
			return E_ERROR;
		}
	}

	for (unsigned i = 0; i < NUM_LIVE_CH; i++) {
		CLiveInfo *li = GetLiveInfoL(i);
		if (li != NULL) {
			live_stream_s& ls = li->GetLiveStream(0);
			if (seg == ls.ls_seg) {
				if (ls.si.picture_io == NULL)
					return E_ERROR;
				AVF_LOGD(C_YELLOW "read picture io" C_NONE);
				if (ls.si.picture_io->ReadAt(buffer, bytes, pos.item->fpos) < 0)
					return E_IO;
				return E_OK;
			}
		}
	}

	if (seg->ReadIndexPicture(pos.item->fpos, buffer, bytes) < 0)
		return E_IO;

	return E_OK;
}

void CVDB::FetchPlaylistInfoL(ref_clip_set_t *rcs, CMemBuffer *pmb)
{
	// vdb_playlist_info_t
	pmb->write_le_32(rcs->ref_clip_type);
	pmb->write_le_32(0);
	pmb->write_le_32(rcs->mTotalRefClips);
	pmb->write_le_32(rcs->m_total_length_ms);
}

avf_status_t CVDB::CmdGetClipSetInfo(int clip_type, int flags, bool bHasExtra, CMemBuffer *pmb, bool *pbNoDelete)
{
	AUTO_LOCK(mMutex);

	ref_clip_set_t *rcs = GetRCS(clip_type);
	if (rcs == NULL) {
		AVF_LOGD("CmdGetClipSetInfo: %d not found", clip_type);
		return E_INVAL;
	}

	vdb_ack_GetClipSetInfo_t *ack = (vdb_ack_GetClipSetInfo_t*)pmb->Alloc(sizeof(*ack));
	ack->clip_type = clip_type;
	ack->total_clips = rcs->mTotalRefClips;
	ack->total_length_ms = rcs->m_total_length_ms;
	ack->live_clip_id = INVALID_CLIP_ID;

	if (IS_PLAYLIST(clip_type)) {
		list_head_t *node;
		list_for_each(node, rcs->ref_clip_list) {
			ref_clip_t *ref_clip = ref_clip_t::from_list_node(node);
			FetchRefClipInfo(ref_clip, flags, bHasExtra, NULL, pmb);
		}
		*pbNoDelete = false;
	} else {
		ref_clip_t *ref_clip = rcs ? rcs->GetFirstRefClip() : NULL;
		for (; ref_clip; ref_clip = ref_clip->GetNextRefClip()) {
			if (ref_clip->IsLive() && ref_clip->GetDurationLong() == 0) {
				ack->total_clips--;
				continue;
			}
			FetchRefClipInfo(ref_clip, flags, bHasExtra, NULL, pmb);
		}
		*pbNoDelete = m_no_delete;
	}

	return E_OK;
}

avf_status_t CVDB::CmdGetClipInfo(int clip_type, clip_id_t clip_id, int flags, bool bHasExtra, CMemBuffer *pmb)
{
	AUTO_LOCK(mMutex);

	ref_clip_set_t *rcs = GetRCS(clip_type);
	if (rcs == NULL) {
		AVF_LOGD("CmdGetClipInfo: no such clip set %d", clip_type);
		return E_INVAL;
	}

	ref_clip_t *ref_clip = rcs->FindRefClip(clip_id);
	if (ref_clip == NULL) {
		AVF_LOGD("CmdGetClipInfo: no such ref clip %x", clip_id);
		return E_INVAL;
	}

	FetchRefClipInfo(ref_clip, flags, bHasExtra, NULL, pmb);
	return E_OK;
}

avf_status_t CVDB::GetRefClipSetInfo(const vdb_ref_clip_t *ref_clips, avf_uint_t count, int flags, CMemBuffer *pmb)
{
	AUTO_LOCK(mMutex);

	vdb_ack_GetClipSetInfo_t *ack = (vdb_ack_GetClipSetInfo_t*)pmb->Alloc(sizeof(*ack));
	ack->clip_type = (uint32_t)CLIP_TYPE_UNSPECIFIED;
	ack->total_clips = count;
	ack->total_length_ms = 0;
	ack->live_clip_id = INVALID_CLIP_ID;

	if (ref_clips != NULL) {
		for (avf_uint_t i = 0; i < count; i++) {
			const vdb_ref_clip_t *rclip = ref_clips + i;

			ref_clip_set_t *rcs = GetRCS(rclip->clip_type);
			if (rcs == NULL) {
				AVF_LOGE("GetRefClipSetInfo: unknown clip type %d", rclip->clip_type);
				return E_INVAL;
			}

			ref_clip_t *ref_clip = rcs->FindRefClip(rclip->clip_id);
			if (ref_clip == NULL) {
				AVF_LOGE("GetRefClipSetInfo[%d]: cannot find ref clip, type %d, id 0x%x",
					i, rclip->clip_type, rclip->clip_id);
				return E_INVAL;
			}

			FetchRefClipInfo(ref_clip, flags, true, NULL, pmb);
		}
	}

	return E_OK;
}

avf_status_t CVDB::CmdGetIndexPicture(vdb_cmd_GetIndexPicture_t *cmd, CMemBuffer *pmb, 
	bool bDataOnly, bool bPlaylist, bool bWriteSize)
{
	AUTO_LOCK(mMutex);

	avf_u64_t clip_time_ms;
	vdb_seg_t *seg;

	if (bPlaylist) {
		ref_clip_set_t *rcs = GetRCS(cmd->clip_type);
		if (rcs == NULL) {
			AVF_LOGE("CmdGetIndexPicture: bad clip_type: %d", cmd->clip_type);
			return E_NOENT;
		}

		avf_u32_t start_time_ms = cmd->clip_time_ms_lo;
		avf_u32_t length_ms = 0;
		list_head_t *node;
		list_for_each(node, rcs->ref_clip_list) {
			ref_clip_t *ref_clip = ref_clip_t::from_list_node(node);
			avf_u32_t next_length_ms = length_ms + ref_clip->GetDurationLong();
			if (start_time_ms < next_length_ms) {
				clip_time_ms = ref_clip->start_time_ms + (start_time_ms - length_ms);
				seg = ref_clip->FindSegmentByTime(0, clip_time_ms);
				goto Start;
			}
			length_ms = next_length_ms;
		}

		AVF_LOGE("segment not found");
		return E_INVAL;

	} else {

		ref_clip_t *ref_clip = FindRefClipL(cmd->clip_type, cmd->clip_id);
		if (ref_clip == NULL) {
			AVF_LOGD("CmdGetIndexPicture: clip not found");
			return E_INVAL;
		}
		clip_time_ms = MK_U64(cmd->clip_time_ms_lo, cmd->clip_time_ms_hi);
		if (clip_time_ms < ref_clip->start_time_ms) {
			clip_time_ms = ref_clip->start_time_ms;
		}
		seg = ref_clip->FindSegmentByTime(0, clip_time_ms);
		if (seg == NULL) {
			AVF_LOGD("no such segment, clip_time_ms: %d, num_segs: %d",
				(int)clip_time_ms, ref_clip->GetNumSegments(0));
		}

	}

Start:
	if (seg == NULL) {
		AVF_LOGD("CmdGetIndexPicture: segment not found");
		return E_INVAL;
	}

	index_pos_t pos;
	avf_u32_t duration;
	avf_u32_t bytes;

	avf_status_t status = seg->FindPosEx(kPictureArray, clip_time_ms, &pos, &duration, &bytes);
	if (status != E_OK) {
		AVF_LOGD("CmdGetIndexPicture failed");
		return status;
	}

	// vdb_ack_GetIndexPicture_t
	if (!bDataOnly) {
		pmb->write_le_32(cmd->clip_type);
		pmb->write_le_32(cmd->clip_id);
		pmb->write_le_32(seg->m_clip->clip_date);
		pmb->write_le_32(cmd->user_data);
		pmb->write_le_64(clip_time_ms);
		pmb->write_le_64(seg->GetStartTime_ms() + pos.item->time_ms);
		pmb->write_le_32(duration);
		bWriteSize = true;
	}

	return FetchIndexPictureL(seg, pos, bytes, bWriteSize, pmb, NULL);
}

avf_status_t CVDB::CmdGetIFrame(vdb_cmd_GetIFrame_t *cmd, CMemBuffer *pmb)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = FindRefClipL(cmd->clip_type, cmd->clip_id);
	if (ref_clip == NULL) {
		AVF_LOGD("CmdGetIFrame: clip not found");
		return E_INVAL;
	}
	avf_u64_t clip_time_ms = MK_U64(cmd->clip_time_ms_lo, cmd->clip_time_ms_hi);
	if (clip_time_ms < ref_clip->start_time_ms) {
		clip_time_ms = ref_clip->start_time_ms;
	}
	vdb_seg_t *seg = ref_clip->FindSegmentByTime(cmd->stream, clip_time_ms);
	if (seg == NULL) {
		AVF_LOGD("CmdGetIFrame: no such segment, clip_time_ms: %d, num_segs: %d",
			(int)clip_time_ms, ref_clip->GetNumSegments(0));
		return E_INVAL;
	}

	index_pos_t pos;
	avf_u32_t duration;
	avf_u32_t bytes;

	avf_status_t status = seg->FindPosEx(kVideoArray, clip_time_ms, &pos, &duration, &bytes);
	if (status != E_OK) {
		AVF_LOGD("CmdIFrame failed");
		return status;
	}

	// vdb_ack_GetIFrame_t
	pmb->write_le_32(cmd->clip_type);
	pmb->write_le_32(cmd->clip_id);
	pmb->write_le_32(seg->m_clip->clip_date);
	pmb->write_le_32(cmd->user_data);
	pmb->write_le_64(clip_time_ms);
	pmb->write_le_64(seg->GetStartTime_ms() + pos.item->time_ms);
	pmb->write_le_32(duration);

	pmb->write_le_16(cmd->stream);
	pmb->write_le_16(cmd->flags);
	pmb->write_le_16(0);	// pic_type
	pmb->write_le_16(0);	// flags

	avf_u8_t *pFrameSize = pmb->Alloc(4);	// iframe_size
	pmb->write_le_32(0);	// data_offset

	return seg->ReadIFrame(pos.item->fpos, bytes, pmb, pFrameSize);
}

avf_status_t CVDB::GetClipRangeL(ref_clip_t *ref_clip,
	avf_u64_t start_time_ms, avf_u64_t end_time_ms,
	clip_info_s *clip_info)
{
	if (start_time_ms >= end_time_ms) {
		if (start_time_ms == end_time_ms) {
			AVF_LOGD("GetClipRangeL: 0 duration");
			return E_ERROR;
		}
		avf_u64_t tmp = start_time_ms;
		start_time_ms = end_time_ms;
		end_time_ms = tmp;
	}

	range_s range;
	range.clip_type = ref_clip->ref_clip_type;
	range.ref_clip_id = ref_clip->ref_clip_id;
	range._stream = STREAM_MAIN;
	range.b_playlist = 0;
	range.length_ms = end_time_ms - start_time_ms;
	range.clip_time_ms = start_time_ms;

	return GetClipRangeInfoL(ref_clip, &range, clip_info, 0);
}

avf_status_t CVDB::CmdMarkClip(vdb_cmd_MarkClip_t *cmd, avf_u32_t *error, int *p_clip_type, clip_id_t *p_clip_id)
{
	AUTO_LOCK(mMutex);
	*error = 0;

	avf_u64_t start_time_ms = MK_U64(cmd->start_time_ms_lo, cmd->start_time_ms_hi);
	avf_u64_t end_time_ms = MK_U64(cmd->end_time_ms_lo, cmd->end_time_ms_hi);
	clip_info_s clip_info;

	ref_clip_t *src = FindRefClipL(cmd->clip_type, cmd->clip_id);
	if (src == NULL) {
		AVF_LOGD("CmdMarkClip: clip not found");
		*error = e_MarkClip_BadParam;
		return E_OK;
	}

	if (GetClipRangeL(src, start_time_ms, end_time_ms, &clip_info) != E_OK) {
		*error = e_MarkClip_BadParam;
		return E_OK;
	}

	int err;
	ref_clip_t *ref_clip = m_marked_rcs.CreateRefClip(src->clip,
		clip_info.clip_time_ms, clip_info.clip_time_ms + clip_info._length_ms, &err, false,
		ref_clip_set_t::DEFAULT_LIST_POS, 0);

	if (ref_clip == NULL) {
		*error = e_MarkClip_BadParam;
		return E_OK;
	}

	if (p_clip_type) {
		*p_clip_type = ref_clip->ref_clip_type;
	}
	if (p_clip_id) {
		*p_clip_id = ref_clip->ref_clip_id;
	}

	// save refino
	SaveClipInfoL(ref_clip->clip);

	if (mpBroadcast) {
		mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_CREATED, ref_clip, NULL);
		ReportSpaceInfoL();
	}

	return E_OK;
}

avf_status_t CVDB::CmdDeleteClip(vdb_cmd_DeleteClip_t *cmd, avf_u32_t *error)
{
	AUTO_LOCK(mMutex);
	*error = 0;

	if (m_no_delete) {
		AVF_LOGE("cannot delete any clip");
		*error = e_DeleteClip_NotPermitted;
		return E_OK;
	}

	ref_clip_t *ref_clip = FindRefClipL(cmd->clip_type, cmd->clip_id);
	if (ref_clip == NULL) {
		AVF_LOGD("CmdDeleteClip: clip %d not found", cmd->clip_id);
		*error = e_DeleteClip_NotFound;
		return E_OK;
	}

	if (ref_clip->IsLive()) {
		AVF_LOGD("cannot delete the live clip");
		*error = e_DeleteClip_NotPermitted;
		return E_OK;
	}

	RemoveRefClipL(ref_clip, true);

	ref_clip_set_t *rcs = GetPlaylist(cmd->clip_type);
	if (rcs != NULL) {
		SavePlaylistL();
	}

	if (mpBroadcast) {
		mpBroadcast->ClipRemoved(mpVdbId, cmd->clip_type, cmd->clip_id);
		ReportSpaceInfoL();
	}

	return E_OK;
}

avf_status_t CVDB::CmdGetRawData(vdb_cmd_GetRawData_t *cmd, CMemBuffer *pmb)
{
	AUTO_LOCK(mMutex);

	avf_u64_t clip_time_ms = MK_U64(cmd->clip_time_ms_lo, cmd->clip_time_ms_hi);
	vdb_seg_t *seg = FindSegmentByTimeL(cmd->clip_type, cmd->clip_id, 0, clip_time_ms);
	if (seg == NULL) {
		AVF_LOGD("CmdGetRawData: segment not found");
		return E_INVAL;
	}

	// vdb_ack_GetRawData_t
	pmb->write_le_32(cmd->clip_type);
	pmb->write_le_32(cmd->clip_id);
	pmb->write_le_32(seg->m_clip->clip_date);

	if (cmd->data_types & (1<<kRawData_GPS)) {
		if (!CmdGetRawDataL(seg, clip_time_ms, kGps0Array, kRawData_GPS, pmb))
			RawDataNotAvail(pmb, kRawData_GPS, clip_time_ms);
	}
	if (cmd->data_types & (1<<kRawData_ACC)) {
		if (!CmdGetRawDataL(seg, clip_time_ms, kAcc0Array, kRawData_ACC, pmb))
			RawDataNotAvail(pmb, kRawData_ACC, clip_time_ms);
	}
	if (cmd->data_types & (1<<kRawData_OBD)) {
		if (!CmdGetRawDataL(seg, clip_time_ms, kObd0Array, kRawData_OBD, pmb))
			RawDataNotAvail(pmb, kRawData_OBD, clip_time_ms);
	}

	pmb->write_le_32(0);

	return E_OK;
}

bool CVDB::CmdGetRawDataL(vdb_seg_t *seg, 
	avf_u64_t clip_time_ms, int index, int data_type, CMemBuffer *pmb)
{
	index_pos_t pos;
	avf_status_t status = seg->FindPos(index, clip_time_ms, &pos);
	if (status != E_OK) {
		//AVF_LOGD("CmdGetRawDataLocked(%d) failed", data_type);
		return false;
	}

	if (pos.array->GetMemBuffer() == NULL)
		return false;

	//AVF_LOGD("GetRawData: %x, " LLD ", %d", data_type, 
	//	seg->GetStartTime_ms() + item->time_ms, item_bytes);

	pmb->write_le_32(data_type);
	pmb->write_le_64(seg->GetStartTime_ms() + pos.item->time_ms);

	avf_u32_t item_bytes = pos.GetItemBytes();
	pmb->write_le_32(item_bytes);

	if (!ReadItemDataL(seg, pos.array, pos.item, item_bytes, pmb))
		return false;

	return true;
}

bool CVDB::ReadItemDataL(vdb_seg_t *seg, CIndexArray *array,
	const index_item_t *item, avf_u32_t item_bytes, CMemBuffer *pmb)
{
	avf_u8_t *ptr = pmb->Alloc(item_bytes);
	if (ptr == NULL)
		return false;
	// pmb should be simple
	CMemBuffer::block_t *block = array->GetMemBuffer()->GetFirstBlock();
	::memcpy(ptr, block->GetMem() + item->fpos, item_bytes);
	return true;
}

void CVDB::RawDataNotAvail(CMemBuffer *pmb, int data_type, avf_u64_t clip_time_ms)
{
	pmb->write_le_32(data_type);
	pmb->write_le_64(clip_time_ms);
	pmb->write_le_32(0);
}

avf_status_t CVDB::CmdGetRawDataBlock(vdb_cmd_GetRawDataBlock_t *cmd, CMemBuffer *pmb, CMemBuffer *pdata, int interval_ms)
{
	AUTO_LOCK(mMutex);
	return GetRawDataBlockL(cmd, pmb, pdata, interval_ms);
}

// pmb != NULL && pData != NULL : fetch raw data
// playlist: clip_type >= CLIP_TYPE_PLIST0 && clip_id == 0
// playlist clip: clip_type >= CLIP_TYPE_PLIST0 && clip_id != 0
avf_status_t CVDB::GetRawDataBlockL(vdb_cmd_GetRawDataBlock_t *cmd,
	CMemBuffer *pmb, CMemBuffer *pData, int interval_ms)
{
	avf_status_t status;

	AVF_LOGD(C_GREEN "GetRawDataBlock: data_type=%d, clip_type=%d, clip_id=0x%x" C_NONE,
		cmd->data_type, cmd->clip_type, cmd->clip_id);

	int start_ms = -10000;

	if (cmd->clip_id != INVALID_CLIP_ID) {

		// get clip raw data block
		status = GetClipRawDataBlockL(cmd, pmb, pData, false, NULL, NULL, 0, start_ms, interval_ms);
		if (status != E_OK)
			return status;

		return E_OK;
	}

	// find the playlist
	ref_clip_set_t *rcs = GetPlaylist(cmd->clip_type);
	if (rcs == NULL) {
		AVF_LOGE("no such playlist %d", cmd->clip_type);
		return E_NOENT;
	}

	vdb_ack_GetRawDataBlock_t *ack = NULL;

	if (pmb) {
		if ((ack = (vdb_ack_GetRawDataBlock_t*)pmb->Alloc(sizeof(*ack))) == NULL) {
			return E_NOMEM;
		}
	}

	if (ack) {
		ack->clip_date = 0;	// fixed later
		ack->num_items = 0;
		ack->data_size = 0;
		ack->clip_type = cmd->clip_type;
		ack->clip_id = cmd->clip_id;
		ack->data_type = cmd->data_type;
		ack->data_options = cmd->data_options;
		ack->requested_time_ms_lo = cmd->clip_time_ms_lo;
		ack->requested_time_ms_hi = cmd->clip_time_ms_hi;
	}

	avf_u32_t pl_start_time = cmd->clip_time_ms_lo;
	avf_u32_t pl_remain_ms = cmd->length_ms;

	avf_u32_t offset_ms = 0;
	ref_clip_t *ref_clip = rcs->GetFirstRefClip();

	for (; ref_clip && pl_remain_ms > 0; ref_clip = ref_clip->GetNextRefClip()) {
		vdb_ack_GetRawDataBlock_t clip_ack;
		vdb_cmd_GetRawDataBlock_t clip_cmd;

		avf_u32_t clip_length_ms = ref_clip->GetDurationLong();
		avf_u32_t next_offset_ms = offset_ms + clip_length_ms;

		// find the first clip, which contains pl_start_time
		if (next_offset_ms <= pl_start_time) {
			offset_ms = next_offset_ms;
			continue;
		}

		// clip offset to read
		avf_u32_t clip_offset_ms = pl_start_time > offset_ms ? pl_start_time - offset_ms : 0;
		avf_u32_t length_ms;

		// clip length to read
		if (clip_offset_ms + pl_remain_ms > clip_length_ms) {
			length_ms = clip_length_ms - clip_offset_ms;
		} else {
			length_ms = pl_remain_ms;
		}
		pl_remain_ms -= length_ms;

		clip_cmd.clip_type = ref_clip->ref_clip_type;
		clip_cmd.clip_id = ref_clip->ref_clip_id;

		avf_u64_t clip_start_time_ms = ref_clip->start_time_ms + clip_offset_ms;
		clip_cmd.clip_time_ms_lo = U64_LO(clip_start_time_ms);
		clip_cmd.clip_time_ms_hi = U64_HI(clip_start_time_ms);
		clip_cmd.length_ms = length_ms;
		clip_cmd.data_type = cmd->data_type;
		clip_cmd.data_options = cmd->data_options;

		status = GetClipRawDataBlockL(&clip_cmd, pmb, pData,
			true, ref_clip, &clip_ack, offset_ms, start_ms, interval_ms);
		if (status != E_OK) {
			return status;
		}

		if (ack) {
			ack->num_items += clip_ack.num_items;
			ack->data_size += clip_ack.data_size;
			if (ack->clip_date == 0) {
				ack->clip_date = clip_ack.clip_date;
			}
		}

		offset_ms = next_offset_ms;

		if (cmd->data_options & DATA_OPTION_DOWNSAMPLING) {
			start_ms = offset_ms;
		}
	}

	return E_OK;
}

// pmb != NULL && pData != NULL : fetch raw data
avf_status_t CVDB::GetClipRawDataBlockL(vdb_cmd_GetRawDataBlock_t *cmd, CMemBuffer *pmb,
	CMemBuffer *pData, bool bWriteSeparator,
	ref_clip_t *ref_clip, vdb_ack_GetRawDataBlock_t *ack,
	uint32_t offset_ms, int start_ms, int interval_ms)
{
	avf_u64_t clip_time_ms = MK_U64(cmd->clip_time_ms_lo, cmd->clip_time_ms_hi);
	vdb_seg_t *seg;

	if (ref_clip) {
		seg = ref_clip->clip->FindSegmentByTime(0, clip_time_ms);
	} else {
		seg = FindSegmentByTimeL(cmd->clip_type, cmd->clip_id, 0, clip_time_ms);
	}

	if (seg == NULL) {
		AVF_LOGD("GetClipRawDataBlockL: segment not found");
		return E_INVAL;
	}

	// type
	int index = get_raw_array_index(cmd->data_type);
	if (index < 0) {
		AVF_LOGE("GetClipRawDataBlockL: unknown data_type %d", cmd->data_type);
		return E_UNKNOWN;
	}

	const avf_u64_t clip_end_time = clip_time_ms + cmd->length_ms;
	index_pos_t pos;

	seg = seg->FindSegWithRawData(index, clip_end_time, &pos);
	if (seg == NULL)
		return E_NOENT;

	if (clip_time_ms <= seg->GetStartTime_ms()) {
		pos.item = pos.array->FindIndexItem(0);
	} else {
		pos.item = pos.array->FindIndexItem(clip_time_ms - seg->GetStartTime_ms());
	}

	if (ack == NULL) {
		if ((ack = (vdb_ack_GetRawDataBlock_t*)pmb->Alloc(sizeof(*ack))) == NULL)
			return E_NOMEM;
	}

	ack->clip_type = cmd->clip_type;
	ack->clip_id = cmd->clip_id;
	ack->clip_date = seg->m_clip->clip_date;
	ack->data_type = cmd->data_type;
	ack->requested_time_ms_lo = cmd->clip_time_ms_lo;
	ack->requested_time_ms_hi = cmd->clip_time_ms_hi;
	ack->num_items = 0; // fixed later
	ack->data_size = 0; // fixed later

	const avf_int_t end_ms = cmd->length_ms;
	avf_int_t item_time_ms = 0;
	int next_time_ms = start_ms;
	bool bWriteIndex = (cmd->data_options & DATA_OPTION_NO_INDEX) == 0;

	while (pos.item) {
		avf_size_t remain = pos.GetRemain();
		if (remain > 0) {

			avf_int_t seg_offset_ms = (avf_int_t)(seg->GetStartTime_ms() - clip_time_ms);

			while (true) {
				avf_int_t time_ms = pos.item->time_ms + seg_offset_ms;
				avf_u32_t item_bytes = pos.GetItemBytes();

				item_time_ms = time_ms + offset_ms;
				if (item_time_ms >= next_time_ms) {
					if (bWriteIndex) {
						pmb->write_le_32(item_time_ms);
						pmb->write_le_32(item_bytes);
					}

					if (pData) {
						if (!ReadItemDataL(seg, pos.array, pos.item, item_bytes, pData))
							return E_ERROR;
					}

					ack->num_items++;
					ack->data_size += item_bytes;
				}

				if (time_ms >= end_ms || --remain == 0)
					break;

				pos.item++;

				if (next_time_ms < 0) {
					next_time_ms = interval_ms;
				} else {
					next_time_ms += interval_ms;
				}
			}
		}

		if ((seg = seg->GetNextSegInRefClip()) == NULL)
			break;

		seg = seg->FindSegWithRawData(index, clip_end_time, &pos);
		if (seg == NULL)
			break;

		pos.item = pos.array->GetFirstItem();
	}

	// repeat the last item with size 0 as separator
	if (bWriteIndex && bWriteSeparator && ack->num_items > 0) {
		pmb->write_le_32(item_time_ms);
		pmb->write_le_32(0);
		ack->num_items++;
	}

	return E_OK;
}

avf_status_t CVDB::CmdGetAllPlaylists(vdb_cmd_GetAllPlaylists_t *cmd, CMemBuffer *pmb)
{
	AUTO_LOCK(mMutex);

	// vdb_ack_GetAllPlaylist_t
	pmb->write_le_32(cmd->flags);
	pmb->write_le_32(NUM_PLISTS);

	struct rb_node *node;
	for (node = rb_first(&m_plist_set.playlist_set); node; node = rb_next(node)) {
		ref_clip_set_t *rcs = ref_clip_set_t::from_rbnode(node);
		FetchPlaylistInfoL(rcs, pmb);
	}

	return E_OK;
}

avf_status_t CVDB::CmdGetPlaylistIndexPicture(vdb_cmd_GetPlaylistIndexPicture_t *cmd, CMemBuffer *pmb)
{
	AUTO_LOCK(mMutex);

	ref_clip_set_t *rcs = GetPlaylist(cmd->list_type);
	if (rcs == NULL) {
		AVF_LOGD("CmdGetPlaylistIndexPicture %d: not found", cmd->list_type);
		return E_ERROR;
	}

	ref_clip_t *ref_clip = rcs->GetFirstRefClipOfList();
	if (ref_clip == NULL) {
		AVF_LOGD("CmdGetPlaylistIndexPicture %d: empty", cmd->list_type);
		return E_ERROR;
	}

	vdb_seg_t *seg = ref_clip->FindSegmentByTime(0, ref_clip->start_time_ms);
	if (seg == NULL) {
		AVF_LOGD("CmdGetPlaylistIndexPicture %d: no segment", cmd->list_type);
		return E_ERROR;
	}

	index_pos_t pos;
	avf_u32_t duration;
	avf_u32_t bytes;

	avf_status_t status = seg->FindPosEx(kPictureArray, ref_clip->start_time_ms, &pos, &duration, &bytes);
	if (status != E_OK) {
		AVF_LOGD("CmdGetPlaylistIndexPicture failed");
		return status;
	}

	// vdb_ack_GetPlaylistIndexPicture_t
	pmb->write_le_32(cmd->list_type);
	pmb->write_le_32(cmd->flags);

	pmb->write_le_32(ref_clip->ref_clip_date);
	pmb->write_le_64(ref_clip->start_time_ms);

	return FetchIndexPictureL(seg, pos, bytes, true, pmb, NULL);
}

avf_status_t CVDB::CmdClearPlaylist(vdb_cmd_ClearPlaylist_t *cmd, avf_u32_t *error)
{
	AUTO_LOCK(mMutex);
	*error = 0;

	ref_clip_set_t *rcs = GetPlaylist(cmd->list_type);
	if (rcs == NULL) {
		AVF_LOGD("CmdClearPlaylist %d: not found", cmd->list_type);
		*error = e_ClearPlaylist_NotPermitted;
		return E_OK;
	}

	if (rcs->mTotalRefClips > 0) {
		while (true) {
			ref_clip_t *ref_clip = rcs->GetFirstRefClipOfList();
			if (ref_clip == NULL)
				break;
			RemoveRefClipL(ref_clip, false);
		}

		if (rcs->mTotalRefClips == 0) {
			if (mpBroadcast) {
				mpBroadcast->PlaylistCleared(mpVdbId, cmd->list_type);
			}
		}

		SavePlaylistL();
	}

	return E_OK;
}

avf_status_t CVDB::CmdInsertClip(vdb_cmd_InsertClip_t *cmd, bool bCheckStreamAttr, avf_u32_t *error)
{
	AUTO_LOCK(mMutex);
	*error = 0;

	avf_u64_t start_time_ms = MK_U64(cmd->start_time_ms_lo, cmd->start_time_ms_hi);
	avf_u64_t end_time_ms = MK_U64(cmd->end_time_ms_lo, cmd->end_time_ms_hi);
	clip_info_s clip_info;

	// source clip
	ref_clip_t *src = FindRefClipL(cmd->clip_type, cmd->clip_id);
	if (src == NULL) {
		AVF_LOGD("CmdInsertClip: clip not found");
		*error = e_InsertClip_BadParam;
		return E_OK;
	}

	// source info
	if (GetClipRangeL(src, start_time_ms, end_time_ms, &clip_info) != E_OK) {
		*error = e_MarkClip_BadParam;
		return E_OK;
	}

	// destination
	ref_clip_set_t *rcs = GetPlaylist(cmd->list_type);
	if (rcs == NULL) {
		AVF_LOGD("CmdInsertClip: must be playlist");
		*error = e_InsertClip_BadParam;
		return E_OK;
	}

	int err;
	ref_clip_t *ref_clip = rcs->CreateRefClip(src->clip,
		clip_info.clip_time_ms, clip_info.clip_time_ms + clip_info._length_ms,
		&err, bCheckStreamAttr, cmd->list_pos, 0);

	if (ref_clip == NULL) {
		switch (err) {
		case 1:
			*error = e_InsertClip_StreamNotMatch;
			break;
		case 2:
			*error = e_InsertClip_UnknownStream;
			break;
		default:
			*error = e_InsertClip_NotFound;
			break;
		}

		return E_OK;
	}

	SavePlaylistL();

	if (mpBroadcast) {
		mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_INSERTED, ref_clip, NULL);
		ReportSpaceInfoL();
	}

	AVF_LOGD("total %d clips in playlist %d", rcs->mTotalRefClips, rcs->ref_clip_type);

	return E_OK;
}

avf_status_t CVDB::CmdMoveClip(vdb_cmd_MoveClip_t *cmd, avf_u32_t *error)
{
	AUTO_LOCK(mMutex);
	*error = 0;

	AVF_LOGD("CmdMoveClip: clip_type=%x, clip_id: %x, new_pos: %d",
		cmd->clip_type, cmd->clip_id, cmd->new_clip_pos);

	ref_clip_set_t *rcs = GetPlaylist(cmd->clip_type);
	if (rcs == NULL) {
		AVF_LOGD("CmdMoveClip: must be playlist");
		*error = e_MoveClip_BadParam;
		return E_OK;
	}

	ref_clip_t *ref_clip = rcs->FindRefClip(cmd->clip_id);
	if (ref_clip == NULL) {
		AVF_LOGD("CmdMoveClip: clip not found");
		*error = e_MoveClip_BadParam;
		return E_OK;
	}

	avf_status_t status = rcs->MoveRefClip(ref_clip, cmd->new_clip_pos);
	if (status != E_OK) {
		*error = e_MoveClip_BadParam;
		return E_OK;
	}

	SavePlaylistL();

	if (mpBroadcast) {
		mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_MOVED, ref_clip, NULL);
		ReportSpaceInfoL();
	}

	return E_OK;
}

avf_u32_t CVDB::SetClearAttr(avf_u32_t clip_attr, avf_u32_t mask, avf_u32_t value)
{
	if (mask) {
		if (mask & value) {
			clip_attr |= mask;
		} else {
			clip_attr &= ~mask;
		}
	}
	return clip_attr;
}

avf_status_t CVDB::CmdSetClipAttr(vdb_cmd_SetClipAttr_t *cmd, CMemBuffer *pmb)
{
	AUTO_LOCK(mMutex);

	vdb_ack_SetClipAttr_t result;
	result.clip_type = cmd->clip_type;
	result.clip_id = cmd->clip_id;
	result.attr_mask = cmd->attr_mask;
	result.attr_value = cmd->attr_value;

	result.result = E_INVAL;
	result.new_attr = 0;

	ref_clip_t *ref_clip = FindRefClipL(cmd->clip_type, cmd->clip_id);
	if (ref_clip == NULL) {
		AVF_LOGW("CmdSetClipAttr: no such ref clip");
		goto Done;
	}

	if (ref_clip->IsLive()) {
		AVF_LOGW("CmdSetClipAttr: cannot change clip attr of the live clip");
		goto Done;
	}

	result.new_attr = ref_clip->clip_attr;
	result.new_attr = SetClearAttr(result.new_attr, CLIP_ATTR_AUTO, cmd->attr_value);
	result.new_attr = SetClearAttr(result.new_attr, CLIP_ATTR_MANUALLY, cmd->attr_value);
	result.new_attr = SetClearAttr(result.new_attr, CLIP_ATTR_UPLOADED, cmd->attr_value);
	result.new_attr = SetClearAttr(result.new_attr, CLIP_ATTR_NEED_UPLOAD, cmd->attr_value);

#if 0
	result.new_attr = SetClearAttr(result.new_attr, CLIP_ATTR_NO_AUTO_DELETE, cmd->attr_value);
#endif

	if (result.new_attr != ref_clip->clip_attr) {
		ref_clip->clip_attr = result.new_attr;
		if (mpBroadcast) {
			mpBroadcast->ClipAttrChanged(mpVdbId, cmd->clip_type, cmd->clip_id, ref_clip->clip_attr);
		}
		SaveClipInfoL(ref_clip->clip);
	}

	result.result = E_OK;

Done:
	pmb->write_mem((const avf_u8_t*)&result, sizeof(result));
	return E_OK;
}

avf_status_t CVDB::CmdCreatePlaylist(avf_u32_t *playlist_id)
{
	AUTO_LOCK(mMutex);

	if (!mb_loaded) {
		AVF_LOGE("CmdCreatePlaylist: not loaded");
		return E_ERROR;
	}

	avf_u32_t id = m_plist_set.GeneratePlaylistId();
	ref_clip_set_t *rcs = m_plist_set.CreatePlaylist(mb_server_mode, id);
	if (rcs == NULL) {
		return E_ERROR;
	}

	SavePlaylistL();

	if (mpBroadcast) {
		mpBroadcast->PlaylistCreated(mpVdbId, id);
	}

	*playlist_id = id;
	return E_OK;
}

avf_status_t CVDB::CmdDeletePlaylist(avf_u32_t playlist_id)
{
	AUTO_LOCK(mMutex);

	if (!mb_loaded) {
		AVF_LOGE("CmdDeletePlaylist: not loaded");
		return E_ERROR;
	}

	ref_clip_set_t *rcs = GetPlaylist(playlist_id);
	if (rcs == NULL) {
		AVF_LOGE("no such playlist: %d", playlist_id);
		return E_ERROR;
	}

	if (rcs->mTotalRefClips > 0) {
		while (true) {
			ref_clip_t *ref_clip = rcs->GetFirstRefClipOfList();
			if (ref_clip == NULL)
				break;
			RemoveRefClipL(ref_clip, false);
		}
	}

	m_plist_set.RemovePlaylistFiles(GetVdbRoot(), rcs);
	m_plist_set.RemovePlaylist(rcs);	

	SavePlaylistL();

	if (mpBroadcast) {
		mpBroadcast->PlaylistDeleted(mpVdbId, playlist_id);
	}

	return E_OK;
}

bool CVDB::PlaylistExists(avf_u32_t plist_id)
{
	AUTO_LOCK(mMutex);
	return GetPlaylist(plist_id) != NULL;
}

avf_status_t CVDB::CmdGetClipExtent(vdb_cmd_GetClipExtent_t *cmd, CMemBuffer *pmb)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = FindRefClipL(cmd->clip_type, cmd->clip_id);
	if (ref_clip == NULL) {
		AVF_LOGD("CmdGetClipExtent: clip not found");
		return E_INVAL;
	}

	avf_u64_t min_start_time_ms;
	avf_u64_t max_end_time_ms;
	avf_status_t status;

	status = ref_clip->clip->GetMinStartTime(ref_clip->start_time_ms, min_start_time_ms);
	if (status != E_OK)
		return status;

	status = ref_clip->clip->GetMaxEndTime(ref_clip->end_time_ms - 1, max_end_time_ms);
	if (status != E_OK)
		return status;

	// vdb_ack_GetClipExtent_t
	pmb->write_le_32(cmd->clip_type);
	pmb->write_le_32(cmd->clip_id);
	pmb->write_le_32(ref_clip->clip->clip_id);
	pmb->write_le_32(ref_clip->clip->buffered_clip_id);
	pmb->write_le_64(min_start_time_ms);
	pmb->write_le_64(max_end_time_ms);
	pmb->write_le_64(ref_clip->start_time_ms);
	pmb->write_le_64(ref_clip->end_time_ms);

	return E_OK;
}

avf_status_t CVDB::CmdSetClipExtent(vdb_cmd_SetClipExtent_t *cmd)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = FindRefClipL(cmd->clip_type, cmd->clip_id);
	if (ref_clip == NULL) {
		AVF_LOGD("CmdGetClipExtent: clip not found");
		return E_INVAL;
	}

	avf_u64_t min_start_time_ms;
	avf_u64_t max_end_time_ms;
	avf_status_t status;

	status = ref_clip->clip->GetMinStartTime(ref_clip->start_time_ms, min_start_time_ms);
	if (status != E_OK)
		return status;

	status = ref_clip->clip->GetMaxEndTime(ref_clip->end_time_ms - 1, max_end_time_ms);
	if (status != E_OK)
		return status;

	avf_u64_t start_time_ms = MK_U64(cmd->new_clip_start_time_ms_lo, cmd->new_clip_start_time_ms_hi);
	avf_u64_t end_time_ms = MK_U64(cmd->new_clip_end_time_ms_lo, cmd->new_clip_end_time_ms_hi);

	if (start_time_ms < min_start_time_ms || end_time_ms > max_end_time_ms) {
		AVF_LOGE("CmdSetClipExtent: out of range");
		return E_PERM;
	}

	avf_u64_t old_length_ms = ref_clip->GetDurationLong();

	ref_clip->start_time_ms = start_time_ms;
	ref_clip->end_time_ms = end_time_ms;

	if (IS_PLAYLIST(ref_clip->ref_clip_type)) {
		ref_clip_set_t *rcs = GetPlaylist(ref_clip->ref_clip_type);
		if (rcs) {
			rcs->m_total_length_ms += ref_clip->GetDurationLong();
			rcs->m_total_length_ms -= old_length_ms;
		}
	}

	SaveClipInfoL(ref_clip->clip);

	if (mpBroadcast) {
		mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_CHANGED, ref_clip, NULL);
		ReportSpaceInfoL();
	}

	return E_OK;
}

ref_clip_t *CVDB::FindFirstRefClipInListL(ref_clip_set_t *rcs, avf_u32_t start_time_ms, avf_u32_t *clip_start_ms)
{
	avf_u32_t length_ms = 0;
	list_head_t *node;
	list_for_each(node, rcs->ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_list_node(node);
		avf_u32_t next_length_ms = length_ms + ref_clip->GetDurationLong();
		if (start_time_ms < next_length_ms) {
			*clip_start_ms = length_ms;
			return ref_clip;
		}
		length_ms = next_length_ms;
	}
	return NULL;
}

// cliplist_info may be NULL: summary info is returned by clip_info
avf_status_t CVDB::GetPlaylistRangeInfoL(const range_s *range,
	clip_info_s *clip_info, cliplist_info_s *cliplist_info, avf_u32_t upload_opt)
{
	ref_clip_set_t *rcs = GetRCS(range->clip_type);
	if (rcs == NULL) {
		return E_NOENT;
	}

	ref_clip_t *ref_clip;
	avf_u32_t clip_start_ms;

	// ---------------------------------------
	// the first ref clip
	// ---------------------------------------
	ref_clip = FindFirstRefClipInListL(rcs, range->clip_time_ms, &clip_start_ms);
	if (ref_clip == NULL) {
		AVF_LOGD("GetPlaylistRangeInfoL: bad clip_time_ms: " LLD, range->clip_time_ms);
		return E_ERROR;
	}

	range_s tmp_range = *range;
	clip_info_s tmp_info;
	avf_status_t status;

	avf_u32_t remain_ms = range->length_ms;
	bool bGetAll = remain_ms == 0;

	tmp_range.ref_clip_id = ref_clip->ref_clip_id;
	tmp_range.clip_time_ms = ref_clip->start_time_ms + (range->clip_time_ms - clip_start_ms);
	tmp_range.length_ms = ref_clip->end_time_ms - tmp_range.clip_time_ms;

	if (!bGetAll && tmp_range.length_ms > remain_ms) {
		tmp_range.length_ms = remain_ms;
	}

	if ((status = GetClipRangeInfoL(ref_clip, &tmp_range, &tmp_info, upload_opt)) != E_OK)
		return status;

	clip_info->clip_id = INVALID_CLIP_ID;
	clip_info->clip_date = tmp_info.clip_date;

	clip_info->clip_addr[0] = 0;
	clip_info->clip_addr[1] = 0;

	if (tmp_info.clip_time_ms <= ref_clip->start_time_ms)
		clip_info->clip_time_ms = clip_start_ms;
	else
		clip_info->clip_time_ms = clip_start_ms + (tmp_info.clip_time_ms - ref_clip->start_time_ms);

	clip_info->v_size[0] = tmp_info.v_size[0];
	clip_info->v_size[1] = tmp_info.v_size[1];
	clip_info->ex_size = tmp_info.ex_size;
	clip_info->_length_ms = tmp_info._length_ms;

	clip_info->b_cliplist = 1;
	clip_info->stream = range->_stream;

	int video_type_0;
	int video_type_1;

	clip_info->video_type[0] = video_type_0 = tmp_info.video_type[0];
	clip_info->video_type[1] = video_type_1 = tmp_info.video_type[1];

	if (cliplist_info) {
		AddToClipListInfo(cliplist_info, &tmp_info);
	}

	// ---------------------------------------
	// the other clips
	// ---------------------------------------
	if (!bGetAll) {
		if (tmp_info._length_ms >= remain_ms)
			return E_OK;
		remain_ms -= tmp_info._length_ms;
	}

	list_head_t *node = &ref_clip->list_node;
	list_for_each_next(node, rcs->ref_clip_list) {
		ref_clip = ref_clip_t::from_list_node(node);

		tmp_range.ref_clip_id = ref_clip->ref_clip_id;
		tmp_range.clip_time_ms = ref_clip->start_time_ms;
		tmp_range.length_ms = ref_clip->GetDurationLong();

		if (!bGetAll && tmp_range.length_ms > remain_ms) {
			tmp_range.length_ms = remain_ms;
		}

		if ((status = GetClipRangeInfoL(ref_clip, &tmp_range, &tmp_info, upload_opt)) != E_OK)
			return status;

		clip_info->v_size[0] += tmp_info.v_size[0];
		clip_info->v_size[1] += tmp_info.v_size[1];
		clip_info->ex_size += tmp_info.ex_size;
		clip_info->_length_ms += tmp_info._length_ms;

		if (video_type_0 != tmp_info.video_type[0]) {
			clip_info->video_type[0] = VIDEO_TYPE_TS;
		}
		if (video_type_1 != tmp_info.video_type[1]) {
			clip_info->video_type[1] = VIDEO_TYPE_TS;
		}

		if (cliplist_info) {
			AddToClipListInfo(cliplist_info, &tmp_info);
		}

		if (!bGetAll) {
			if (tmp_info._length_ms >= remain_ms)
				return E_OK;
			remain_ms -= tmp_info._length_ms;
		}
	}

	return E_OK;
}

avf_status_t CVDB::GetRangeInfo(const range_s *range,
	clip_info_s *clip_info, cliplist_info_s *cliplist_info,
	avf_u32_t upload_opt)
{
	AUTO_LOCK(mMutex);

	if (cliplist_info) {
		InitClipListInfo(cliplist_info);
	}

	avf_status_t status;
	if (range->b_playlist) {
		status = GetPlaylistRangeInfoL(range, clip_info, cliplist_info, upload_opt);
	} else {
		status = GetClipRangeInfoL(NULL, range, clip_info, upload_opt);
	}

	if (status != E_OK) {
		if (cliplist_info) {
			ReleaseClipListInfo(cliplist_info);
		}
	}

	return status;
}

avf_status_t CVDB::WritePlaylist(const char *server_ip,
	const range_s *range, CMemBuffer *pmb,
	bool bMuteAudio, bool bFullPath, bool bFixPTS,
	avf_u32_t hls_len, avf_u32_t init_time_ms)
{
	AUTO_LOCK(mMutex);

	if (hls_len == 0) {
		hls_len = HLS_SEGMENT_LENGTH_MS;
	}

	if (range->b_playlist) {
		return WriteClipListL(server_ip, range, pmb,
			bMuteAudio, bFullPath, bFixPTS,
			hls_len, init_time_ms);
	} else {
		return WriteSegmentListL(server_ip, NULL, range, pmb,
			true, true, false, bMuteAudio, bFullPath, bFixPTS,
			0, hls_len, init_time_ms);
	}
}

avf_status_t CVDB::WriteSegmentListL(const char *server_ip,
	ref_clip_t *ref_clip, const range_s *range, CMemBuffer *pmb, 
	bool bHead, bool bTail, bool bPlaylist, bool bMuteAudio, bool bFullPath, bool bFixPTS, 
	avf_u32_t clip_start_ms, avf_u32_t hls_len, avf_u32_t init_time_ms)
{
	char buffer[32];

	if (ref_clip == NULL) {
		ref_clip = FindRefClipL(range->clip_type, range->ref_clip_id);
		if (ref_clip == NULL) {
			AVF_LOGD("WriteSegmentListL: clip not found");
			return E_INVAL;
		}
	}

	vdb_seg_t *seg = ref_clip->FindSegmentByTime(range->_stream, range->clip_time_ms);
	if (seg == NULL) {
		AVF_LOGD("WriteSegmentListL: segment not found, clip_time_ms=" LLD, range->clip_time_ms);
		return E_INVAL;
	}

	avf_u32_t time_off = (avf_u32_t)(range->clip_time_ms - seg->GetStartTime_ms());
	avf_u32_t length_remain = range->length_ms;
	int get_all = length_remain == 0;

	int url_type = URL_TYPE_TS;
	if (bMuteAudio) url_type |= URL_MUTE_AUDIO;
	if (bFixPTS) url_type |= URL_SPEC_TIME;

	bool bIncInitTime = false;
	if (bFixPTS && !bPlaylist) {
		bIncInitTime = true;
	}

	// for each segment
	while (true) {
		avf_u32_t seg_start_ms = clip_start_ms + (avf_u32_t)(seg->GetStartTime_ms() - ref_clip->start_time_ms);

		index_pos_t pos;
		avf_status_t status = seg->FindPosRelative(kVideoArray, time_off, &pos);
		if (status != E_OK) {
			AVF_LOGE("FindPosRelative failed");
			return E_OK;
			//return status;
		}

		avf_u32_t seg_time_off = pos.item->time_ms;
		while (true) {
			avf_u32_t duration = hls_len; //HLS_SEGMENT_LENGTH_MS;
			const index_item_t *item_next = NULL;
			int len;

			if (!get_all) {
				if (duration > length_remain)
					duration = length_remain;
			}

			avf_u32_t tmp = seg_time_off + duration;
			if (tmp >= seg->GetDuration_ms()) {
				duration = seg->GetDuration_ms() - seg_time_off;
				if (duration == 0) {
					break;
				}
			} else {
				if ((item_next = pos.array->FindIndexItem(tmp)) == NULL) {
					AVF_LOGE("WriteSegmentList: FindIndexItem failed");
					return E_ERROR;
				}
				if (item_next == pos.item) {
					item_next = pos.item + 1;
				}
				duration = item_next->time_ms - pos.item->time_ms;
			}

			if (bHead) {
				bHead = false;
				pmb->write_string(STR_WITH_SIZE("#EXTM3U\n"));
				pmb->write_string(STR_WITH_SIZE("#EXT-X-TARGETDURATION:"));
				//::sprintf(buffer, "%d.%03d\n", duration/1000, duration%1000);
				//len = ::sprintf(buffer, "%d\n", HLS_SEGMENT_LENGTH_MS/1000);
				len = ::sprintf(buffer, "%d\n", hls_len/1000);
				pmb->write_string(buffer, len);
				pmb->write_string(STR_WITH_SIZE("#EXT-X-VERSION:3\n"));
				pmb->write_string(STR_WITH_SIZE("#EXT-X-MEDIA-SEQUENCE:0\n"));
			}

			pmb->write_string(STR_WITH_SIZE("#EXTINF:"));
			len = ::sprintf(buffer, "%d.%03d,\n", duration/1000, duration%1000);
			pmb->write_string(buffer, len);

			avf_u64_t clip_time_ms;
			char url[200];

			if (bPlaylist) {
				// relative to playlist start
				clip_time_ms = seg_start_ms + seg_time_off;
			} else {
				clip_time_ms = seg->GetStartTime_ms() + seg_time_off;
			}

			if (bFullPath) {
				len = vdb_format_url(server_ip, url, mpVdbId ? mpVdbId->string() : NULL,
					ref_clip->ref_clip_type, ref_clip->ref_clip_id,
					range->_stream, 0, bPlaylist, url_type, clip_time_ms, duration, init_time_ms, 0);
			} else {
				len = vdb_format_hls_item(url, bMuteAudio, bFixPTS, clip_time_ms, duration, init_time_ms, 0);
			}

			pmb->write_string(url, len);
			pmb->write_char('\n');

			if (bIncInitTime) {
				init_time_ms += duration;
			}

			if (!get_all) {
				if (length_remain > duration) {
					length_remain -= duration;
				} else {
					length_remain = 0;
					break;
				}
			}

			if (item_next == NULL)
				break;

			seg_time_off = item_next->time_ms;
			pos.item = item_next;
		}

		if (!get_all && length_remain == 0)
			break;

		time_off = 0;
		if ((seg = seg->GetNextSegInRefClip()) == NULL)
			break;
	}

	if (bTail) {
		pmb->write_string(STR_WITH_SIZE("\n#EXT-X-ENDLIST"));
	}

	return E_OK;
}

avf_status_t CVDB::WriteClipListL(const char *server_ip, const range_s *range, CMemBuffer *pmb, 
	bool bMuteAudio, bool bFullPath, bool bFixPTS, avf_u32_t hls_len, avf_u32_t init_time_ms)
{
	// the rcs
	ref_clip_set_t *rcs = GetRCS(range->clip_type);
	if (rcs == NULL) {
		AVF_LOGD("WriteClipListL: clip set %d not found", range->clip_type);
		return E_NOENT;
	}

	// the first ref clip
	avf_u32_t clip_start_ms;
	ref_clip_t *ref_clip = FindFirstRefClipInListL(rcs, range->clip_time_ms, &clip_start_ms);
	if (ref_clip == NULL) {
		AVF_LOGD("WriteClipListL: FindFirstRefClipInList failed, " LLD, range->clip_time_ms);
		return E_NOENT;
	}

	// relative to clip start
	avf_u32_t clip_off_ms = range->clip_time_ms - clip_start_ms;

	// all clips
	bool bFirst = true;
	range_s tmp_range = *range;
	avf_u32_t remain_ms = range->length_ms;

	list_head_t *node = &ref_clip->list_node;
	list_for_each_from(node, rcs->ref_clip_list) {
		ref_clip = ref_clip_t::from_list_node(node);

		tmp_range.ref_clip_id = ref_clip->ref_clip_id;
		tmp_range.clip_time_ms = ref_clip->start_time_ms + clip_off_ms;

		if (remain_ms) {
			tmp_range.length_ms = ref_clip->GetDurationLong() - clip_off_ms;
			if (tmp_range.length_ms > remain_ms) {
				tmp_range.length_ms = remain_ms;
			}
		}

		avf_status_t status = WriteSegmentListL(server_ip, ref_clip, &tmp_range, pmb, bFirst, 
			list_is_last(node, rcs->ref_clip_list), true, bMuteAudio, bFullPath, bFixPTS,
			clip_start_ms, hls_len, init_time_ms);
		if (status != E_OK)
			return status;

		if (remain_ms) {
			if ((remain_ms -= tmp_range.length_ms) == 0)
				break;
		}

		bFirst = false;
		clip_start_ms += ref_clip->GetDurationLong();
		clip_off_ms = 0;
	}

	return E_OK;
}

avf_status_t CVDB::RemoveRefClipL(ref_clip_t *ref_clip, bool bSaveRefInfo)
{
	ref_clip_set_t *rcs = GetRCS(ref_clip->ref_clip_type);
	if (rcs == NULL) {
		AVF_LOGE("RemoveRefClipL, rcs(%d,0x%x) not found", 
			ref_clip->ref_clip_type, ref_clip->ref_clip_id);
		return E_INVAL;
	}

	vdb_clip_t *clip = ref_clip->clip;

	rcs->RemoveRefClip(ref_clip);

	if (clip->IsNotUsed()) {
		m_clip_set.RemoveClip(clip, this);
	} else {
		if (bSaveRefInfo) {
			SaveClipInfoL(clip);
		}
	}

	return E_OK;
}

// v0, v1: (header + data)
// pic, gps, obd, acc: (header + data) + (header + data) + ... + (header + data)
// stream 0: UPLOAD_GET_V0 | UPLOAD_GET_PIC | UPLOAD_GET_GPS | UPLOAD_GET_OBD | UPLOAD_GET_ACC
// stream 1: UPLOAD_GET_V1

avf_status_t CVDB::CalcClipRangeInfoL(ref_clip_t *ref_clip,
	avf_u64_t start_time_ms, avf_u64_t end_time_ms,
	int stream, avf_u32_t upload_opt, clip_range_info_s& info)
{
	avf_status_t status;
	vdb_seg_t *seg;

	// the start seg
	if ((seg = ref_clip->FindSegmentByTime(stream, start_time_ms)) == NULL)
		return E_ERROR;

	// start pos
	index_pos_t vpos;
	if ((status = seg->FindPos(kVideoArray, start_time_ms, &vpos)) != E_OK)
		return status;

	info.v_addr = seg->GetStartAddr() + vpos.item->fpos;
	info.start_time_ms = seg->GetStartTime_ms() + vpos.item->time_ms;
	info.length_ms = 0;
	info.v_size = 0;
	info.ex_size = 0;

	// for each segment
	avf_u32_t remain_ms = (avf_u32_t)(end_time_ms - info.start_time_ms);
	while (remain_ms > 0) {

		avf_u32_t max_seg_end_ms = vpos.item->time_ms + remain_ms;
		const index_item_t *end_v_item = vpos.array->FindIndexItem_Next(max_seg_end_ms);
		if (end_v_item == NULL) {
			AVF_LOGE("error");
			return E_ERROR;
		}

		// video size is always calculated
		info.v_size += end_v_item->fpos - vpos.item->fpos;

		// count video upload headers
		if (upload_opt & UPLOAD_GET_VIDEO) {
			// upload_header_v2_t + avf_stream_attr_t + data
			info.ex_size += (end_v_item - vpos.item) * (sizeof(upload_header_v2_t) + sizeof(avf_stream_attr_t));
		}

		// pic & raw data
		if (upload_opt & UPLOAD_GET_PIC_RAW) {
			CIndexArray *all_arrays[4];
			CIndexArray **arrays = seg->m_cache->sindex.arrays;

			all_arrays[0] = (upload_opt & UPLOAD_GET_PIC) ? arrays[kPictureArray] : NULL;
			all_arrays[1] = (upload_opt & UPLOAD_GET_GPS) ? arrays[kGps0Array] : NULL;
			all_arrays[2] = (upload_opt & UPLOAD_GET_OBD) ? arrays[kObd0Array] : NULL;
			all_arrays[3] = (upload_opt & UPLOAD_GET_ACC) ? arrays[kAcc0Array] : NULL;

			for (avf_uint_t i = 0; i < ARRAY_SIZE(all_arrays); i++) {
				CIndexArray *array = all_arrays[i];
				if (array) {
					const index_item_t *item_start = array->FindIndexItem(vpos.item->time_ms);
					if (item_start == NULL)
						continue;

					const index_item_t *item_end = array->FindIndexItem_Next(end_v_item->time_ms);
					if (item_end == NULL)
						continue;

					// extra size: header + data
					if (item_end > item_start) {
						info.ex_size += (item_end - item_start) * sizeof(upload_header_v2_t) +
							(item_end->fpos - item_start->fpos);
					}
				}
			}
		}

		avf_u32_t inc_ms = end_v_item->time_ms - vpos.item->time_ms;
		info.length_ms += inc_ms;

		if (remain_ms <= inc_ms)
			break;

		remain_ms -= inc_ms;

		if ((seg = seg->GetNextSegInRefClip()) == NULL)
			break;

		if ((status = seg->GetFirstPos(kVideoArray, &vpos)) != E_OK)
			return status;
	}

	return E_OK;
}

// length_at_most: valid if != 0 and range->length_ms == 0
avf_status_t CVDB::GetClipRangeInfoL(ref_clip_t *ref_clip, const range_s *range,
	clip_info_s *clip_info, avf_u32_t upload_opt)
{
	avf_u64_t end_time_ms;
	avf_status_t status;

	// find ref clip
	if (ref_clip == NULL) {
		if ((ref_clip = FindRefClipL(range->clip_type, range->ref_clip_id)) == NULL) {
			AVF_LOGE("GetClipRangeInfoL: clip not found");
			return E_INVAL;
		}
	}

	clip_info->video_type[0] = ref_clip->clip->m_video_type_0;
	clip_info->video_type[1] = ref_clip->clip->m_video_type_1;

	// calc end_time_ms
	if (range->length_ms == 0) {
		// get all remain; not greater than length_at_most
		end_time_ms = ref_clip->end_time_ms;
	} else {
		end_time_ms = range->clip_time_ms + range->length_ms;
		if (end_time_ms > ref_clip->end_time_ms)
			end_time_ms = ref_clip->end_time_ms;
	}

	clip_info->clip_id = ref_clip->clip->clip_id;
	clip_info->clip_date = ref_clip->clip->clip_date;

	clip_info->clip_addr[0] = 0;
	clip_info->clip_addr[1] = 0;
	clip_info->clip_time_ms = 0;

	clip_info->v_size[0] = 0;
	clip_info->v_size[1] = 0;
	clip_info->ex_size = 0;
	clip_info->_length_ms = 0;

	clip_info->b_cliplist = 0;
	clip_info->stream = range->_stream;

	if (upload_opt) {

		if (upload_opt & UPLOAD_GET_STREAM_0) {
			clip_range_info_s info;

			status = CalcClipRangeInfoL(ref_clip, range->clip_time_ms, end_time_ms,
					STREAM_MAIN, upload_opt & UPLOAD_GET_STREAM_0, info);
			if (status != E_OK)
				return status;

			clip_info->clip_addr[0] = info.v_addr;
			clip_info->clip_time_ms = info.start_time_ms;
			clip_info->v_size[0] = info.v_size;
			clip_info->ex_size += info.ex_size;
			clip_info->_length_ms = info.length_ms;
		}

		if (upload_opt & UPLOAD_GET_STREAM_1) {
			clip_range_info_s info;

			status = CalcClipRangeInfoL(ref_clip, range->clip_time_ms, end_time_ms,
					STREAM_SUB_1, upload_opt & UPLOAD_GET_STREAM_1, info);
			if (status != E_OK)
				return status;

			clip_info->clip_addr[1] = info.v_addr;
			clip_info->v_size[1] = info.v_size;
			clip_info->ex_size += info.ex_size;

			if ((upload_opt & UPLOAD_GET_STREAM_0) == 0) {
				// only v1, use its time range
				clip_info->clip_time_ms = info.start_time_ms;
				clip_info->_length_ms = info.length_ms;
			} else {
				// has both v0 & v1
				avf_u64_t end_time_0 = clip_info->clip_time_ms + clip_info->_length_ms;
				avf_u64_t end_time_1 = info.start_time_ms + info.length_ms;
				avf_u64_t end_time = MIN(end_time_0, end_time_1);

				// max start_time_ms
				if (clip_info->clip_time_ms < info.start_time_ms)
					clip_info->clip_time_ms = info.start_time_ms;

				// min end_time_ms
				clip_info->_length_ms = end_time - clip_info->clip_time_ms;
			}
		}

	} else {

		// --------------------------------------------
		// start seg & pos
		// --------------------------------------------
		vdb_seg_t *seg = ref_clip->FindSegmentByTime(range->_stream, range->clip_time_ms);
		if (seg == NULL) {
			return E_INVAL;
		}

		if (range->clip_time_ms == seg->GetStartTime_ms()) {	// avoid load cache
			clip_info->clip_addr[0] = seg->GetStartAddr();
			clip_info->clip_time_ms = range->clip_time_ms;
		} else {
			index_pos_t pos;
			avf_status_t status = seg->FindPos(kVideoArray, range->clip_time_ms, &pos);
			if (status != E_OK) {
				AVF_LOGE("GetClipRangeInfoL failed");
				return status;
			}
			clip_info->clip_addr[0] = seg->GetStartAddr() + pos.item->fpos;
			clip_info->clip_time_ms = seg->GetStartTime_ms() + pos.item->time_ms;
		}

		// --------------------------------------------
		// end seg & pos
		// --------------------------------------------
		if ((seg = ref_clip->FindSegmentByTime(range->_stream, end_time_ms - 1)) == NULL) {
			AVF_LOGW("GetClipRangeInfoL: FindSegmentByTime failed, stream=%d, end_time_ms=" LLD,
				range->_stream, end_time_ms);
			// workaround: stream 1 might be shorter than stream 0
			if ((seg = ref_clip->GetLastSegment(range->_stream)) == NULL) {
				return E_INVAL;
			}
			avf_u64_t seg_end_time = seg->GetEndTime_ms();
			if (end_time_ms < seg_end_time) {
				// this cannot happen
				return E_INVAL;
			}
			end_time_ms = seg_end_time;
		}

		// --------------------------------------------
		// size, length_ms
		// --------------------------------------------
		if (end_time_ms == seg->GetEndTime_ms()) {	// avoid load cache
			clip_info->v_size[0] = seg->GetEndAddr() - clip_info->clip_addr[0];
			clip_info->_length_ms = end_time_ms - clip_info->clip_time_ms;
		} else {
			index_pos_t pos;
			status = seg->FindPos(kVideoArray, end_time_ms - 1, &pos);
			if (status != E_OK) {
				AVF_LOGD("GetClipRangeInfoL failed");
				return status;
			}

			const index_item_t *next_item = pos.item + 1;
			clip_info->v_size[0] = seg->GetStartAddr() + next_item->fpos - clip_info->clip_addr[0];
			clip_info->_length_ms = seg->GetStartTime_ms() + next_item->time_ms - clip_info->clip_time_ms;
		}
	}

	return E_OK;
}

void CVDB::InitClipListInfo(cliplist_info_s *cliplist_info)
{
	cliplist_info->_Init();
}

void CVDB::AddToClipListInfo(cliplist_info_s *cliplist_info, clip_info_s *clip_info)
{
	cliplist_info->_Append(clip_info);
}

void CVDB::ReleaseClipListInfo(cliplist_info_s *cliplist_info)
{
	cliplist_info->_Release();
}

avf_status_t CVDB::ReleaseAllCaches()
{
	AUTO_LOCK(mMutex);
	AVF_LOGD("ReleaseAllCaches");
	m_clip_set.m_seg_cache_list.Clear();
	m_clip_set.m_clip_cache_list.Clear();
	return E_OK;
}

avf_status_t CVDB::Serialize(CMemBuffer *pmb)
{
	AUTO_LOCK(mMutex);
	return SerializeLocked(pmb);
}

avf_status_t CVDB::SerializeToFileLocked(const char *filename)
{
	CMemBuffer *pmb = NULL;
	IAVIO *io = NULL;
	avf_status_t rval;
	CMemBuffer::block_t *block;

	pmb = CMemBuffer::Create(4*1024);
	if (pmb == NULL) {
		rval = E_NOMEM;
		goto Done;
	}

	io = CSysIO::Create();
	if (io == NULL) {
		rval = E_NOMEM;
		goto Done;
	}

	rval = io->CreateFile(filename);
	if (rval != E_OK)
		goto Done;

	SerializeLocked(pmb);

	block = pmb->GetFirstBlock();
	for (; block; block = block->GetNext()) {
		int n = io->Write(block->GetMem(), block->GetSize());
		if (n != (int)block->GetSize()) {
			rval = (avf_status_t)n;
			goto Done;
		}
	}

Done:
	avf_safe_release(io);
	avf_safe_release(pmb);
	return rval;
}

avf_status_t CVDB::SerializeLocked(CMemBuffer *pmb)
{
	pmb->write_string("vdbdump0");

	pmb->write_le_32(mb_server_mode);
	pmb->write_le_32(mb_loaded);
	pmb->write_le_32(m_sid);

	m_pf_index.Serialize(pmb);
	m_pf_picture.Serialize(pmb);
	m_pf_video.Serialize(pmb);

	pmb->write_length_string(m_vdb_root->string(), m_vdb_root->size());

	pmb->write_le_32(1);
	pmb->write_length_string(m_clipinfo_path->string(), m_clipinfo_path->size());
	pmb->write_length_string(m_index0_path->string(), m_index0_path->size());
	pmb->write_length_string(m_index1_path->string(), m_index1_path->size());
	pmb->write_length_string(m_indexpic_path->string(), m_indexpic_path->size());
	pmb->write_length_string(m_video0_path->string(), m_video0_path->size());
	pmb->write_length_string(m_video1_path->string(), m_video1_path->size());

	pmb->write_le_32(1);
	m_index0_filelist.Serialize(pmb);
	m_index1_filelist.Serialize(pmb);
	m_indexpic_filelist.Serialize(pmb);
	m_video0_filelist.Serialize(pmb);
	m_video1_filelist.Serialize(pmb);

	m_buffered_rcs.Serialize(pmb);
	m_marked_rcs.Serialize(pmb);

	struct rb_node *node;
	for (node = rb_first(&m_plist_set.playlist_set); node; node = rb_next(node)) {
		ref_clip_set_t *rcs = ref_clip_set_t::from_rbnode(node);
		pmb->write_le_8(1);
		rcs->Serialize(pmb);
	}
	pmb->write_le_8(0);

	m_clip_set.Serialize(pmb);
	m_cdir_set.Serialize(pmb);

	for (unsigned i = 0; i < NUM_LIVE_CH; i++) {
		CLiveInfo *li = GetLiveInfoL(i);
		if (li) {
			pmb->write_le_8(1);
			li->Serialize(pmb);
		} else {
			pmb->write_le_8(0);
		}
	}

	return E_OK;
}

void clip_list_s::ReleaseAll()
{
	clip_list_s *item = this;
	while (item) {
		clip_list_s *next = item->next;
		avf_safe_release(item->display_name);
		avf_delete item;
		item = next;
	}
}

