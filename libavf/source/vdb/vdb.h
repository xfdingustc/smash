
#ifndef __VDB_H__
#define __VDB_H__

class IVDBBroadcast;
class CMP4Builder;

class CLiveInfo;
class CVDB;
class CVdbReader;
class CClipReader;
class CClipWriter;
class CVideoClipReader;
class CUploadClipReader;
class CISOReader;
class CClipData;
class clip_dir_set_t;

struct live_stream_s;
struct clip_dir_t;
struct path_filter_s;

struct clip_list_s;
struct clip_info_s;

#include "vdb_api.h"
#include "vdb_local.h"

//-----------------------------------------------------------------------
//
//  live_stream_s
//
//	sample
//		video
//		data
//			pic
//			raw: gps, acc, obd
//
//-----------------------------------------------------------------------
struct live_stream_s
{
	vdb_seg_t *ls_seg;	// current segment, may be NULL!
	avf_u32_t seg_id;	// segment id from 0

	avf_u64_t seg_start_time_ms;	// segment start time ms from 0
	avf_u64_t seg_v_start_addr;	// segment start address from 0
	avf_u64_t stream_end_time_ms;

	int video_type;		// VIDEO_TYPE_TS, VIDEO_TYPE_MP4

	avf_sbool_t has_video;	// stream has video
	avf_sbool_t has_data;	// stream has picture/raw

	struct live_si_s {
		avf_sbool_t picture_started;
		avf_sbool_t video_started;

		avf_u32_t picture_size;
		avf_u64_t picture_start_pts;
		avf_u64_t video_start_pts;

		avf_u32_t length_video;		// ms
		avf_u32_t length_data;		// ms

		IAVIO *picture_io;
		seg_index_t *sindex;	// point to seg->m_cache->sindex

		IAVIO *video_io;
		ITSMapInfo *tsmap_info;

		CRawFifo *fifos[kAllArrays];
		avf_uint_t sample_count[kAllArrays];
	};

	struct live_si_s si;

	live_stream_s();
	~live_stream_s();

	void InitSegment(vdb_stream_t *vdb_stream, vdb_seg_t::info_t& info);
	void FinishSegment();

	INLINE bool HasSample() {
		return has_video || has_data;
	}

	void Serialize(CMemBuffer *pmb);
};

//-----------------------------------------------------------------------
//
//  CLiveInfo
//
//-----------------------------------------------------------------------
class CLiveInfo: public CObject
{
	typedef CObject inherited;
	friend class CVDB;

private:
	static CLiveInfo* Create();
	CLiveInfo();
	virtual ~CLiveInfo();

private:
	void Serialize(CMemBuffer *pmb);

private:
	INLINE live_stream_s& GetLiveStream(int stream) {
		return stream == 0 ? m_stream_0 : m_stream_1;
	}

public:
	struct raw_stream_s {
		avf_u32_t fcc;
		raw_data_t *last_raw;
	};

private:
	avf_sbool_t mbClipDateSet;
	avf_sbool_t mbClipCreated;	// msg sent
	avf_size_t m_save_counter;	// 0: clean; > 0: need save clip
	vdb_clip_t *clip;			// live clip; not null
	ref_clip_t *ref_clip;		// live ref clip; not null
	live_stream_s m_stream_0;	// live stream 0
	live_stream_s m_stream_1;	// live stream 1

private:
	list_head_t input_rq;	// extra raw data queue
	TDynamicArray<raw_stream_s> raw_stream;

public:
	avf_u64_t AdjustClipLength();
};

//-----------------------------------------------------------------------
//
//  mark_info_s
//
//-----------------------------------------------------------------------
struct mark_info_s {
	enum {
		STATE_NONE,
		STATE_SET,
		STATE_RUNNING,
	};

	int state;
	int delay_ms;		// TODO
	int before_live_ms;	// -1: all before the live point
	int after_live_ms;	// -1: all after the live point
//	avf_u64_t start_time_ms;	// start time of the live ref clip
//	avf_u64_t end_time_ms;	// ref_clip->end_time_ms when start marking
	ref_clip_t *mp_live_ref_clip;
	clip_id_t ref_clip_id;	// stores current or previous live marked clip

	mark_info_s(): state(STATE_NONE), mp_live_ref_clip(NULL), ref_clip_id(INVALID_CLIP_ID) {}
};

//-----------------------------------------------------------------------
//
//  path_filter_s
//
//-----------------------------------------------------------------------
struct path_filter_s
{
	const char *dir_0; // dir name, not NULL
	const char *dir_1;

	const char *file_ext; // not NULL
	const char *file_ext2;	// not NULL

	string_t *path_0; // path name, may be NULL
	string_t *path_1;

	CVdbFileList *filelist_0; // may be NULL
	CVdbFileList *filelist_1;

	INLINE path_filter_s() {
		Clear();
	}

	INLINE void Clear() {
		dir_0 = NULL;
		dir_1 = NULL;
		path_0 = NULL;
		path_1 = NULL;
		file_ext = NULL;
		file_ext2 = NULL;
		filelist_0 = NULL;
		filelist_1 = NULL;
	}

	INLINE const char *GetFileExt(int type) {
		return type == 0 ? file_ext : file_ext2;
	}

	string_t *CreateFileName(clip_dir_t *cdir, int stream, avf_time_t *gen_time, bool bUseGenTime, const char *ext = NULL);
	string_t *GetFileName(clip_dir_t *cdir, int stream, avf_time_t local_time, int type);
	INLINE string_t *GetFileName(const vdb_seg_t *seg, avf_time_t local_time, int type) {
		return GetFileName(seg->m_clip->m_cdir, seg->m_stream, local_time, type);
	}
	void RemoveFile(clip_dir_t *cdir, int stream, avf_time_t local_time, int type);
	void RemoveFile(const vdb_seg_t *seg, avf_time_t local_time, int type);
	void Serialize(CMemBuffer *pmb);
};

//-----------------------------------------------------------------------
//
//  CVDB
//
//-----------------------------------------------------------------------
class CVDB:
	public CObject,
	public IVdbInfo,
	public IClipOwner
{
	typedef CObject inherited;
	friend class CVdbReader;
	friend class CClipReader;
	friend class CUploadClipReader;
	friend class CClipWriter;
	friend class CISOReader;
	friend class CClipData;
	DEFINE_INTERFACE;

public:
	enum {
		HLS_SEGMENT_LENGTH_MS = 2000,
		CLIP_SAVE_DELAY = 3,		// 
	};

	enum {
		MAX_CACHED_SEGMENT = 10,	// per clip set
		MAX_LOADED_CLIPS = 32,	// per clip set
		MAX_CACHED_CLIPS = 8,		// per clip set
	};

	enum {
		kEnoughSpace = 300*MB,	// can record, if lower, remove clip
		kMinSpace = 150*MB,		// start delete file
	};

	enum {
		kEnoughSpace_milli = 0,	// can record, if lower, remove clip
		kMinSpace_milli = 0,		// if lower, stop record
	};

	enum {
		MAX_SEGMENTS_PER_DIR = 1000,
	};

	enum {
		COPY_BUF_SZ = 16*KB,
	};

	typedef struct space_info_s {
		avf_u64_t disk_space_b;
		avf_u64_t free_disk_b;
		avf_u64_t free_file_b;
		avf_u64_t used_file_b;
	} space_info_t;

	typedef CIndexArray::item_t index_item_t;
	typedef CIndexArray::pos_t index_pos_t;

public:
	typedef struct vdb_record_info_s {
		bool valid;
		bool removed;
		avf_time_t dirname;
		clip_id_t ref_clip_id;
		avf_time_t ref_clip_date;
		avf_s32_t gmtoff;
		avf_u32_t duration_ms;
	} vdb_record_info_t;

	typedef struct vdb_ref_clip_s {
		int clip_type;
		clip_id_t clip_id;
	} vdb_ref_clip_t;

public:
	static CVDB *Create(IVDBBroadcast *pBroadcast, void *pOwner, bool b_server_mode);

public:
	CVDB(IVDBBroadcast *pBroadcast, void *pOwner, bool b_server_mode);
	virtual ~CVDB();
	INLINE void *GetOwner() { return mpOwner; }

// disk management API
public:
	avf_status_t Load(const char *mnt, const char *path, bool bCreate, const char *id);
	avf_status_t Unload(bool bInDestructor = false);
	void GetVdbSpaceInfo(IVDBControl::vdb_info_s *info, avf_u64_t autodel_free, avf_u64_t autodel_min);
	void CheckSpace(bool bAutoDelete, bool *not_ready, bool *no_space, avf_u64_t autodel_free, avf_u64_t autodel_min);
	INLINE void SetNoDelete(int no_delete) {
		m_no_delete = no_delete;
	}
	INLINE const char *GetVdbRoot() {
		return m_vdb_root->string();
	}

private:
	avf_u64_t GetReusableClipSpace();
	static avf_u64_t GetMinSpaceValue(const space_info_t& si);
	static avf_u64_t GetEnoughSpaceValue(const space_info_t& si);

private:
	avf_status_t ScanAllClipSetL();
	avf_status_t ScanClipSetL(clip_dir_t *cdir, string_t *clip_path);
	avf_status_t LoadClipL(clip_dir_t *cdir, avf_time_t local_time, string_t *clip_path, IAVIO *io,bool bUpdate, vdb_clip_t **pclip);
	avf_status_t LoadPlaylistL();
	avf_status_t SavePlaylistL();
	avf_status_t CollectAllRefClipsL();
	avf_status_t CollectRefClipsL(vdb_clip_t *clip);
	avf_status_t ScanFileListL(ap<string_t>& path, CVdbFileList& list);

private:
	void RegisterSegmentFilesL(int stream, vdb_seg_t *seg);
	avf_u64_t GetFreeFileSpaceL();
	avf_u64_t GetUsedFileSpaceL();
	avf_u64_t RemoveOldFilesL();
	avf_u64_t RemoveOneOldFileL(const ap<string_t>& path, CVdbFileList& list);
	bool GetSpaceInfoL(space_info_t& si);
	void PrintSpaceInfoL(const space_info_t& si);
	void ReportSpaceInfoL();

private:
	bool TryRemoveClipL(const space_info_t& si);
	ref_clip_t *GetRefClipToBeShrinkedL();

private:
	avf_time_t GenerateClipNameL(clip_dir_t *cdir, avf_time_t *gen_time = NULL);
	void RemoveClipNameL(clip_dir_t *cdir, avf_time_t local_time);
	string_t *CreateVideoFileNameL(CLiveInfo *li, avf_time_t *gen_time, int stream, bool bUseGenTime, const char *ext = NULL);
	string_t *CreatePictureFileNameL(CLiveInfo *li, avf_time_t *gen_time, int stream, bool bUseGenTime);
	string_t *CreateIndexFileNameL(CLiveInfo *li, avf_time_t *gen_time, int stream, bool bUseGenTime);

// record API
public:
	int GetSegmentLength();
	avf_status_t StartRecord(vdb_record_info_t *info, int ch);
	avf_status_t StopRecord(vdb_record_info_t *info, int ch);

	string_t *CreateVideoFileName(avf_time_t *gen_time, int stream, bool bUseGenTime, int ch, const char *ext = NULL);
	string_t *CreatePictureFileName(avf_time_t *gen_time, int stream, bool bUseGenTime, int ch);
	string_t *CreateIndexFileName(avf_time_t *gen_time, int stream, bool bUseGenTime, int ch);

	avf_status_t InitVideoStream(const avf_stream_attr_s *stream_attr, int video_type, int stream, int ch);
	avf_status_t SetLiveIO(IAVIO *io, ITSMapInfo *map_info, int stream, int ch);
	avf_status_t InitClipDate(avf_u32_t date, avf_s32_t gmtoff, int ch);

	avf_status_t StartVideo(avf_time_t video_time, int stream, int ch);
	avf_status_t StartPicture(avf_time_t picture_time, int stream, int ch);
	avf_status_t FinishSegment(avf_u64_t next_start_ms, avf_u32_t fsize, int stream, bool bLast, int ch);
	avf_status_t SegVideoLength(avf_u64_t end_time_ms, avf_u32_t v_fsize, int stream, int ch);

	avf_status_t AddVideo(avf_u64_t pts, avf_u32_t timescale, avf_u32_t fpos, int stream, int ch, avf_u32_t *length = NULL);
	avf_status_t AddPicture(CBuffer *pBuffer, int stream, int ch, avf_u32_t *length = NULL);
	avf_status_t AddRawData(raw_data_t *raw, int stream, int ch, avf_u32_t *length = NULL);
	avf_status_t SetConfigData(raw_data_t *raw, int ch);
	avf_status_t AddRawDataEx(raw_data_t *raw, int ch);
	avf_status_t SetRawDataFreq(int interval_gps, int interval_acc, int interval_obd, int ch);

	avf_status_t GetDataSize(avf_u64_t *video_size, avf_u64_t *picture_size, avf_u64_t *raw_data_size);
	avf_status_t SetClipSceneData(int clip_type, avf_u32_t clip_id, const avf_u8_t *data, avf_size_t data_size);

private:
	avf_status_t DoAddRawDataEx(CLiveInfo *li, raw_data_t *raw, bool bUpdateLength);

// mark live clip
public:
	avf_status_t SetRecordAttr(int clip_attr);
	avf_status_t MarkLiveClip(int delay_ms, int before_live_ms, int after_live_ms, int b_continue, int clip_attr, int ch);
	avf_status_t StopMarkLiveClip(int ch);
	avf_status_t GetMarkLiveInfo(IVDBControl::mark_info_s& info, int ch);

private:
	void StopMarkLiveClipL(bool bSaveClipInfo, int ch);
	void UpdateMarkLiveClipL(CLiveInfo *li);

private:
	enum {
		NUM_LIVE_CH = 2,
	};
	INLINE bool IsRecording() {
		return mplive != NULL || mplive1 != NULL;
	}
	INLINE CLiveInfo *GetLiveInfoL(int ch) {
		return ch == 0 ? mplive : mplive1;
	}
	INLINE void SetLiveInfoL(int ch, CLiveInfo *li) {
		if (ch == 0) mplive = li;
		else mplive1 = li;
	}

private:
	avf_status_t StartRecordL(vdb_record_info_t *info, int ch);
	avf_status_t StopRecordL(vdb_record_info_t *info, int ch);

	void CreateSegmentL(CLiveInfo *li, live_stream_s& ls, int stream);
	void EndSegmentL(CLiveInfo *li, int stream);

	void SaveLiveClipInfoL(CLiveInfo *li, bool bForce);
	IAVIO *GetTempIO();
	void SaveClipInfoL(vdb_clip_t *clip);

	void UpdateVideoSizeL(live_stream_s& ls, avf_u32_t fsize);
	void UpdatePictureSizeL(live_stream_s& ls, avf_u32_t bytes);
	void UpdateRawSizeL(live_stream_s& ls, int index, raw_data_t *raw);
	void UpdateRawSizeExL(live_stream_s& ls, int index, raw_data_t *raw);

	void UpdateStreamSize(CLiveInfo *li, int stream, avf_u64_t end_time_ms, avf_u32_t v_fsize);
	void UpdateSegmentLengthL(CLiveInfo *li, live_stream_s& ls, int stream, avf_u32_t *p_seg_length);
	void SaveRawData(CLiveInfo *li, live_stream_s& ls, bool bVideo);
	void SaveRawData(live_stream_s& ls, int index, CIndexArray *array, CRawFifo::item_s *item);
	void SetClipDate(CLiveInfo *li, avf_u32_t date = 0, avf_s32_t gmtoff = 0);

private:
	clip_dir_t *AddDirL(avf_time_t dirname, bool bCreate, bool *bExists = NULL);
	clip_dir_t *GetAvailDirL();

// IClipOwner
public:
	virtual seg_cache_list_t& GetSegCacheList() {
		return m_clip_set.GetSegCacheList();
	}
	virtual clip_cache_list_t& GetClipCacheList() {
		return m_clip_set.GetClipCacheList();
	}
	virtual clip_set_t& GetClipSet() {
		return m_clip_set;
	}
	virtual ref_clip_set_t *GetRefClipSet(int clip_type) {
		return GetRCS(clip_type);
	}

	virtual string_t *GetIndexFileName(const vdb_seg_t *seg);
	virtual string_t *GetPictureFileName(const vdb_seg_t *seg);
	virtual string_t *GetVideoFileName(const vdb_seg_t *seg);

	virtual void RemoveIndexFile(const vdb_seg_t *seg);
	virtual void RemoveVideoFile(const vdb_seg_t *seg);
	virtual void RemovePictureFile(const vdb_seg_t *seg);

	virtual void RemoveClipFiles(vdb_clip_t *clip);
	virtual void ClipRemoved(clip_dir_t *cdir);

	virtual bool LoadSegCache(vdb_seg_t *seg);
	virtual bool LoadClipCache(vdb_clip_t *clip);

	virtual avf_status_t OpenSegmentVideo(const vdb_seg_t *seg, IAVIO **pio);

private:
	void RemoveIndexFile(clip_dir_t *cdir, int stream, avf_time_t index_name);
	void RemoveVideoFile(clip_dir_t *cdir, int stream, avf_time_t video_name, int type);
	void RemovePictureFile(clip_dir_t *cdir, int stream, avf_time_t picture_name);

// IVdbInfo
public:
	virtual avf_status_t DeleteClip(int clip_type, avf_u32_t clip_id);

	virtual avf_status_t GetNumClips(int clip_type, avf_u32_t *num_clips, avf_u32_t *sid);
	virtual avf_status_t GetClipSetInfo(int clip_type, clip_set_info_s *info, avf_u32_t start, avf_u32_t *num, avf_u32_t *sid);

	virtual IClipReader *CreateClipReaderOfTime(const range_s *range,
		bool bMuteAudio, bool bFixPTS, avf_u32_t init_time_ms, avf_u32_t upload_opt);
	virtual IClipReader *CreateClipReaderOfAddress(const clip_info_s *clip_info,
		cliplist_info_s *cliplist_info, avf_u64_t offset,
		bool bMuteAudio, bool bFixPTS, avf_u32_t init_time_ms, avf_u32_t upload_opt);

	virtual IClipData *CreateClipDataOfTime(range_s *range);

	virtual IClipWriter *CreateClipWriter();

public:
	CISOReader *CreateISOReader(const clip_info_s *clip_info, cliplist_info_s *cliplist_info);

// server cmds
public:
	avf_status_t CmdGetClipSetInfo(int clip_type, int flags, bool bHasExtra, CMemBuffer *pmb, bool *pbNoDelete);
	avf_status_t CmdGetClipInfo(int clip_type, clip_id_t clip_id, int flags, bool bHasExtra, CMemBuffer *pmb);
	avf_status_t GetRefClipSetInfo(const vdb_ref_clip_t *ref_clips, avf_uint_t count, int flags, CMemBuffer *pmb);
	avf_status_t CmdGetIndexPicture(vdb_cmd_GetIndexPicture_t *cmd, CMemBuffer *pmb, bool bDataOnly, bool bPlaylist, bool bWriteSize = false);
	avf_status_t CmdGetIFrame(vdb_cmd_GetIFrame_t *cmd, CMemBuffer *pmb);
	avf_status_t GetClipRangeL(ref_clip_t *ref_clip, avf_u64_t start_time_ms, avf_u64_t end_time_ms, clip_info_s *clip_info);
	avf_status_t CmdMarkClip(vdb_cmd_MarkClip_t *cmd, avf_u32_t *error, int *p_clip_type, clip_id_t *p_clip_id);
	avf_status_t CmdDeleteClip(vdb_cmd_DeleteClip_t *cmd, avf_u32_t *error);
	avf_status_t CmdGetRawData(vdb_cmd_GetRawData_t *cmd, CMemBuffer *pmb);
	bool CmdGetRawDataL(vdb_seg_t *seg, avf_u64_t clip_time_ms, int index, int data_type, CMemBuffer *pmb);
	bool ReadItemDataL(vdb_seg_t *seg, CIndexArray *array, const index_item_t *item, avf_u32_t item_bytes, CMemBuffer *pmb);
	void RawDataNotAvail(CMemBuffer *pmb, int data_type, avf_u64_t clip_time_ms);
	avf_status_t CmdGetRawDataBlock(vdb_cmd_GetRawDataBlock_t *cmd, CMemBuffer *pmb, CMemBuffer *pdata, int interval_ms);
	avf_status_t GetRawDataBlockL(vdb_cmd_GetRawDataBlock_t *cmd, CMemBuffer *pmb, CMemBuffer *pData, int interval_ms);
	avf_status_t GetClipRawDataBlockL(vdb_cmd_GetRawDataBlock_t *cmd, CMemBuffer *pmb, CMemBuffer *pData, 
		bool bWriteSeparator, ref_clip_t *ref_clip, vdb_ack_GetRawDataBlock_t *ack, avf_u32_t offset_ms,
		int start_ms, int interval_ms);

	avf_status_t CmdGetAllPlaylists(vdb_cmd_GetAllPlaylists_t *cmd, CMemBuffer *pmb);
	avf_status_t CmdGetPlaylistIndexPicture(vdb_cmd_GetPlaylistIndexPicture_t *cmd, CMemBuffer *pmb);
	avf_status_t CmdClearPlaylist(vdb_cmd_ClearPlaylist_t *cmd, avf_u32_t *error);
	avf_status_t CmdInsertClip(vdb_cmd_InsertClip_t *cmd, bool bCheckStreamAttr, avf_u32_t *error);
	avf_status_t CmdMoveClip(vdb_cmd_MoveClip_t *cmd, avf_u32_t *error);
	avf_status_t CmdSetClipAttr(vdb_cmd_SetClipAttr_t *cmd, CMemBuffer *pmb);
	static avf_u32_t SetClearAttr(avf_u32_t clip_attr, avf_u32_t mask, avf_u32_t value);

	avf_status_t CmdCreatePlaylist(avf_u32_t *playlist_id);
	avf_status_t CmdDeletePlaylist(avf_u32_t playlist_id);
	bool PlaylistExists(avf_u32_t plist_id);

	avf_status_t CmdGetClipExtent(vdb_cmd_GetClipExtent_t *cmd, CMemBuffer *pmb);
	avf_status_t CmdSetClipExtent(vdb_cmd_SetClipExtent_t *cmd);
	avf_status_t GetSpaceInfo(avf_u64_t *total_space, avf_u64_t *used_space, avf_u64_t *marked_space, avf_u64_t *clip_space);

	avf_status_t Serialize(CMemBuffer *pmb);
	avf_status_t SerializeLocked(CMemBuffer *pmb);
	avf_status_t SerializeToFileLocked(const char *filename);

private:
	avf_status_t FetchIndexPictureL(vdb_seg_t *seg, const index_pos_t& pos, avf_u32_t bytes, 
		bool bWriteSize, CMemBuffer *pmb, avf_u8_t *buffer);
	static void FetchPlaylistInfoL(ref_clip_set_t *rcs, CMemBuffer *pmb);
	static void FetchRefClipInfo(ref_clip_t *ref_clip, int flags, bool bHasExtra, const char *vdb_id, CMemBuffer *pmb);

private:
	ref_clip_t *FindFirstRefClipInListL(ref_clip_set_t *rcs, avf_u32_t start_time_ms, avf_u32_t *clip_start_ms);

private:
	void LockClipL(const clip_info_s *clip_info);
	void UnlockClipL(const clip_info_s *clip_info);
	void UnlockClipL(vdb_clip_t *clip, avf_u64_t start_time_ms, avf_u64_t end_time_ms);

private:
	void LockClipListL(const cliplist_info_s *cliplist_info);
	void UnlockClipListL(const cliplist_info_s *cliplist_info);
	avf_status_t RemoveRefClipL(ref_clip_t *ref_clip, bool bSaveRefInfo);

private:
	struct clip_range_info_s {
		avf_u64_t v_addr;	// abs clip addr
		avf_u64_t start_time_ms;	// abs clip time
		avf_u64_t v_size;	// video size
		avf_u64_t ex_size;	// all upload headers + pic + raw
		avf_u32_t length_ms;
	};

private:
	avf_status_t GetPlaylistRangeInfoL(const IVdbInfo::range_s *range,
		clip_info_s *clip_info, cliplist_info_s *cliplist_info, avf_u32_t upload_opt);
	avf_status_t GetClipRangeInfoL(ref_clip_t *ref_clip, const IVdbInfo::range_s *range,
		clip_info_s *clip_info, avf_u32_t upload_opt);
	avf_status_t CalcClipRangeInfoL(ref_clip_t *ref_clip,
		avf_u64_t start_time_ms, avf_u64_t end_time_ms,
		int stream, avf_u32_t upload_opt, clip_range_info_s& info);

// called by vdb_http_server
public:
	avf_status_t GetRangeInfo(const IVdbInfo::range_s *range,
		clip_info_s *clip_info, IVdbInfo::cliplist_info_s *cliplist_info, avf_u32_t upload_opt);
	avf_status_t WritePlaylist(const char *server_ip, const IVdbInfo::range_s *range, CMemBuffer *pmb,
		bool bMuteAudio, bool bFullPath, bool bFixPTS, avf_u32_t hls_len, avf_u32_t init_time_ms);

private:
	avf_status_t WriteSegmentListL(const char *server_ip, ref_clip_t *ref_clip, const IVdbInfo::range_s *range, CMemBuffer *pmb, 
		bool bHead, bool bTail, bool bPlaylist, bool bMuteAudio, bool bFullPath, bool bFixPTS,
		avf_u32_t clip_start_ms, avf_u32_t hls_len, avf_u32_t init_time_ms);
	avf_status_t WriteClipListL(const char *server_ip, const IVdbInfo::range_s *range, CMemBuffer *pmb,
		bool bMuteAudio, bool bFullPath, bool bFixPTS, avf_u32_t hls_len, avf_u32_t init_time_ms);

private:
	static void InitClipListInfo(cliplist_info_s *cliplist_info);
	static void AddToClipListInfo(cliplist_info_s *cliplist_info, clip_info_s *clip_info);
	static void ReleaseClipListInfo(cliplist_info_s *cliplist_info);

// for vdb local
public:
	copy_clip_set_s *CreateCopyClipSet();
	void DestroyCopyClipSet(copy_clip_set_s *cclip_set);

	int InitTransfer(copy_clip_set_s *cclip_set, const vdb_local_item_t *items, int nitems, vdb_local_size_t *size);
	void GenerateRefClipIds(copy_clip_set_s *cclip_set);
	int TransferClipSet(copy_clip_set_s *cclip_set, CVDB *pDestVdb);
	int TransferOneClip(copy_clip_set_s *cclip_set, copy_clip_s *cclip, CVDB *pDestVdb);
	int TransferOneStream(copy_clip_set_s *cclip_set, copy_clip_s *cclip, int s,
		CVDB *pDestVdb, copy_clip_info_s& info);
	int OpenTransferSegment(copy_clip_s *cclip, int s, copy_stream_s *cstream, avf_uint_t seg_index,
		copy_state_s& state, CVDB *pDestVdb, copy_clip_info_s& info);
	int OpenTransferRange(copy_range_s *crange, const copy_state_s& state, copy_seg_info_s& segi, CVDB *pDestVdb);
	void CloseTransferRange(const copy_state_s& state, copy_seg_info_s& segi, bool bRemoveFile, CVDB *pDestVdb);
	int TransferRange(copy_clip_set_s *cclip_set, const copy_state_s& state, copy_seg_info_s& segi,
		copy_range_s *crange, CVDB *pDestVdb, copy_clip_info_s& info);
	void CloseTransferFilesL(copy_state_s& state);
	int CaclCopyRange(const copy_state_s& state, copy_seg_info_s& segi, copy_range_s *crange, int index);
	int CopyFile(copy_clip_set_s *cclip_set, const copy_state_s& state, copy_seg_info_s& segi,
		copy_range_s *crange, int index, int fd_from, int fd_to, int event);
	avf_status_t SaveCopyRangeIndex(const copy_state_s& state, copy_seg_info_s& segi, copy_range_s *crange);
	avf_status_t SaveCopyClipInfo(copy_clip_set_s *cclip_set, copy_clip_s *cclip, copy_clip_info_s& info);

	void RemoveTransferedClip(copy_clip_info_s& info);
	void RemoveTransferedStream(copy_stream_info_s& stream, int s);

	avf_status_t ReleaseAllCaches();

// CClipData
private:
	void CloseClipDataL(CClipData *data);
	void CloseClipData(CClipData *data);
	avf_status_t ReadClipData(CClipData *data, IClipData::desc_s *desc, void **pdata, avf_size_t *psize, IAVIO *avio, bool *proceed);
	avf_status_t GotoSegmentPosL(CClipData *data, vdb_seg_t **pseg, 
		avf_u64_t clip_time_ms, int stream, int index, index_pos_t *pos);
	static void fill_upload_header(upload_header_t *header,
		IClipData::desc_s *desc, avf_size_t extra_size,
		const index_item_t *item, const index_item_t *next_item);
	avf_status_t FetchClipDataL(vdb_seg_t *seg, CClipData *data, IClipData::desc_s *desc,
		IAVIO **pavio, string_t *filename, index_pos_t *pos,
		void **pdata, avf_size_t *psize, IAVIO *avio);

// server APIs
public:
	avf_u64_t s_GetLiveClipLength(bool& bHasVideo, bool& bHasPicture, int ch);
	clip_list_s *s_GetAllClips();
	void s_ScanClipDir(clip_list_s **pfirst, clip_list_s **plast, string_t *clip_path, avf_time_t dirname);
	avf_status_t s_GetClipInfo(vdb_clip_id_t vdb_clip_id, vdbs_clip_info_t& info);
	avf_status_t s_GetIndexPicture(vdb_clip_id_t vdb_clip_id, void **pdata, int *psize);
	avf_status_t s_GetIndexPictureAtPos(vdb_clip_id_t vdb_clip_id, uint64_t clip_time_ms, void **pdata, 
		int *psize, uint64_t *clip_time_ms_out);
	avf_status_t s_RemoveClip(vdb_clip_id_t vdb_clip_id);
	avf_status_t s_EnumRawData(vdb_enum_raw_data_proc proc, void *ctx,
		vdb_clip_id_t vdb_clip_id, int data_type,
		uint64_t clip_time_ms, uint32_t length_ms);
	avf_status_t s_GetClipRangeInfo(vdb_clip_id_t vdb_clip_id, const IVdbInfo::range_s *range, clip_info_s *clip_info);
	avf_status_t s_ReadHLS(const char *server_ip, vdb_clip_id_t vdb_clip_id, avf_u64_t clip_time_ms, CMemBuffer *pmb, int stream, avf_u32_t hls_len);
	IClipReader *s_CreateClipReader(vdb_clip_id_t vdb_clip_id, int stream,
		avf_u64_t clip_time_ms, avf_u32_t length_ms, avf_u64_t start_offset);
	avf_size_t s_GetNumClips(void);

private:
	ref_clip_t *s_GetRefClipL(vdb_clip_id_t vdb_clip_id);
	avf_status_t s_DynamicLoadClipL(avf_time_t dirname, clip_id_t clip_id);

private:
	INLINE string_t *GetClipDir() {
		return m_clipinfo_path.get();
	}
	INLINE string_t *GetIndexDir(int stream) {
		return stream == 0 ? m_index0_path.get() : m_index1_path.get();
	}
	INLINE string_t *GetPictureDir() {
		return m_indexpic_path.get();
	}
	INLINE string_t *GetVideoDir(int stream) {
		return stream == 0 ? m_video0_path.get() : m_video1_path.get();
	}
	INLINE CVdbFileList& GetIndexFileList(int stream) {
		return stream == 0 ? m_index0_filelist : m_index1_filelist;
	}
	INLINE CVdbFileList& GetIndexPicFileList() {
		return m_indexpic_filelist;
	}
	INLINE CVdbFileList& GetVideoFileList(int stream) {
		return stream == 0 ? m_video0_filelist : m_video1_filelist;
	}

private:
	INLINE ref_clip_set_t *GetPlaylist(int list_type) {
		return m_plist_set.GetPlaylist(list_type);
	}
	INLINE ref_clip_set_t *GetRCS(int clip_type) {
		switch (clip_type) {
		case CLIP_TYPE_BUFFER: return &m_buffered_rcs;
		case CLIP_TYPE_MARKED: return &m_marked_rcs;
		default: return GetPlaylist(clip_type);
		}
	}

	INLINE ref_clip_t *FindRefClipL(int clip_type, clip_id_t ref_clip_id) {
		ref_clip_set_t *rcs = GetRCS(clip_type);
		return rcs ? rcs->FindRefClip(ref_clip_id) : NULL;
	}
	INLINE vdb_seg_t *FindSegmentByTimeL(int clip_type, clip_id_t ref_clip_id, int stream, avf_u64_t clip_time_ms) {
		ref_clip_t *ref_clip = FindRefClipL(clip_type, ref_clip_id);
		return ref_clip ? ref_clip->FindSegmentByTime(stream, clip_time_ms) : NULL;
	}
	INLINE vdb_clip_t *FindClipL(clip_id_t clip_id) {
		return m_clip_set.FindClip(clip_id);
	}

	INLINE ref_clip_t *FindRefClipExL(int clip_type, avf_time_t dirname, clip_id_t ref_clip_id) {
		if (mb_server_mode) {
			ref_clip_set_t *rcs = GetRCS(clip_type);
			return rcs->FindRefClipEx(dirname, ref_clip_id);
		} else {
			return FindRefClipL(clip_type, ref_clip_id);
		}
	}

	INLINE vdb_clip_t *FindClipExL(avf_time_t dirname, clip_id_t clip_id) {
		if (mb_server_mode) {
			return m_clip_set.FindClipEx(dirname, clip_id);
		} else {
			return FindClipL(clip_id);
		}
	}

	INLINE void LockSegCache(vdb_seg_t *seg) {
		m_clip_set.LockSegCache(seg);
	}

	INLINE void UnlockSegCache(vdb_seg_t *seg) {
		m_clip_set.UnlockSegCache(seg);
	}

private:
	CMutex mMutex;
	void *mpOwner;
	IVDBBroadcast *mpBroadcast;
	string_t *mpVdbId;
	const bool mb_server_mode;

	bool mb_loaded;
	int m_no_delete;
	avf_u32_t m_sid;

	CVdbReader *mpFirstReader;
	CClipWriter *mpFirstClipWriter;
	CClipData *mpFirstClipData;

	path_filter_s m_pf_index;
	path_filter_s m_pf_picture;
	path_filter_s m_pf_video;

	ap<string_t> m_mnt;				// mount point
	ap<string_t> m_vdb_root;		// 100TRANS/

	ap<string_t> m_clipinfo_path;	// 100TRANS/*/clipinfo/
	ap<string_t> m_index0_path;		// 100TRANS/*/index-1/
	ap<string_t> m_index1_path;		// 100TRANS/*/index-2/
	ap<string_t> m_indexpic_path;	// 100TRANS/*/indexpic/
	ap<string_t> m_video0_path;		// 100TRANS/*/video-1/
	ap<string_t> m_video1_path;		// 100TRANS/*/video-2/

	CVdbFileList m_index0_filelist;
	CVdbFileList m_index1_filelist;
	CVdbFileList m_indexpic_filelist;
	CVdbFileList m_video0_filelist;
	CVdbFileList m_video1_filelist;

	ref_clip_set_t m_buffered_rcs;
	ref_clip_set_t m_marked_rcs;
	playlist_set_t m_plist_set;

	clip_set_t m_clip_set;
	clip_dir_set_t m_cdir_set;

	CLiveInfo *mplive;		// not NULL when RECORDING
	CLiveInfo *mplive1;		// not NULL when RECORDING

	int mRecordAttr;
	mark_info_s mMarkInfo;	// for marking live clip
	IAVIO *mpTempIO;		// to save clipinfo & index files
};

struct clip_list_s {
	clip_list_s *next;
	string_t *display_name;
	avf_u32_t clip_dir;
	avf_u32_t clip_id;
	void ReleaseAll();
};

struct clip_info_s {
	avf_u32_t clip_date;	// since 1970
	avf_s32_t gmtoff;
	avf_u32_t duration_ms;
};

//-----------------------------------------------------------------------
//
//  CVdbReader
//
//-----------------------------------------------------------------------
class CVdbReader: public CObject
{
	typedef CObject inherited;
	friend class CVDB;

protected:
	typedef IVdbInfo::clip_info_s clip_info_s;
	typedef IVdbInfo::cliplist_info_s cliplist_info_s;

protected:
	CVdbReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info);
	virtual ~CVdbReader();

// IInterface
public:
	//virtual void AddRef();
	virtual void Release();

protected:
	void CloseL();
	virtual void OnClose() = 0;
	INLINE int IsClipList() { return m_clip_info.b_cliplist; }

protected:
	CVDB *const mpVDB;
	bool mbClosed;
	DEFINE_HLIST_NODE(CVdbReader);
	clip_info_s m_clip_info;
	cliplist_info_s m_cliplist_info;	// valid if m_clip_info.b_cliplist
};

//-----------------------------------------------------------------------
//
//  CISOReader
//
//-----------------------------------------------------------------------
class CISOReader: public CVdbReader
{
	typedef CVdbReader inherited;
	friend class CVDB;

private:
	static CISOReader *Create(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info);

private:
	CISOReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info);
	virtual ~CISOReader();

protected:
	virtual void OnClose();

private:
	avf_status_t AddClipFiles(const clip_info_s *ci);

public:
	IAVIO *CreateIO();

private:
	CMP4Builder *mp_builder;
};

//-----------------------------------------------------------------------
//
//  CClipReader
//
//-----------------------------------------------------------------------
class CClipReader: public CVdbReader,
	public IClipReader,
	public ISeekToTime
{
	typedef CVdbReader inherited;
	friend class CVDB;
	DEFINE_INTERFACE;

protected:
	enum {
		PACKET_SIZE = 188,
	};

	typedef CIndexArray::item_t index_item_t;
	typedef CIndexArray::pos_t index_pos_t;

	struct clip_pos_s
	{
		vdb_seg_t *seg;
		avf_s32_t seg_start_ms;

		avf_u64_t clip_remain;
		avf_u32_t seg_offset;
		avf_u32_t seg_remain;

		avf_u64_t curr_pos; // virtual file pointer
	};

protected:
	CClipReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info, bool bMuteAudio, bool bFixPTS, avf_u32_t init_time_ms);
	virtual ~CClipReader();

// IClipReader
public:
	virtual int ReadClip(avf_u8_t *buffer, avf_size_t size);
	virtual avf_u64_t GetStartTimeMs() {
		return m_clip_info.clip_time_ms;
	}
	virtual avf_u32_t GetLengthMs() {
		return m_clip_info._length_ms;
	}
	// virtual avf_u64_t GetSize();
	// virtual avf_u64_t GetPos();
	virtual avf_status_t Seek(avf_s64_t offset, int whence);

// ISeekToTime
public:
	// virtual avf_u32_t GetLengthMs() = 0;
	virtual avf_status_t SeekToTime(avf_u32_t time_ms);

protected:
	virtual int ReadClipL(avf_u8_t *buffer, avf_size_t size) = 0;
	virtual avf_status_t SeekToAddrL(avf_s64_t offset, int whence) = 0;
	virtual avf_status_t SeekToTimeL(avf_u32_t time_ms) = 0;

protected:
	bool const mbMuteAudio;
	bool mbFixPTS;

protected:
	INLINE int IsClipList() { return m_clip_info.b_cliplist; }
	INLINE int ShouldFixPts() { return mbFixPTS; }

	static clip_info_s *FindClipInfoByAddrL(const cliplist_info_s *cliplist_info, avf_u64_t offset,
		avf_u64_t *cliplist_offset, avf_u32_t *start_time_ms);
	static clip_info_s *FindClipInfoByTimeL(const cliplist_info_s *cliplist_info, avf_u32_t time_ms,
		avf_u64_t *cliplist_offset, avf_u32_t *start_time_ms);

	avf_status_t SeekClipListToAddrL(const cliplist_info_s *cliplist_info, avf_u64_t offset, clip_pos_s& pos);
	avf_status_t SeekClipListToTimeL(const cliplist_info_s *cliplist_info, avf_u32_t time_ms, clip_pos_s& pos,
		int stream = -1, index_pos_t *ipos = NULL);

	avf_status_t SeekClipToAddrL(const clip_info_s *clip_info, avf_u64_t offset, clip_pos_s& pos);
	avf_status_t SeekClipToTimeL(const clip_info_s *clip_info, avf_u32_t time_ms, clip_pos_s& pos,
		int stream = -1, index_pos_t *ipos = NULL);

	static avf_status_t ReadIO(IAVIO *io, avf_u8_t *buffer, avf_size_t size);

protected:
	void FindAudio(IAVIO *io);
	void MuteAudio(avf_u8_t *buffer, avf_u32_t bytes, avf_uint_t pmt_pid);
	void FixPTS(avf_u8_t *buffer, avf_u32_t bytes);
	void FixPCR(avf_u8_t *p);
	void FixPES(avf_u8_t *p);
	avf_u64_t fix_pts(avf_u64_t pts);
	avf_u64_t read_pts(const avf_u8_t *p);
	void write_pts(avf_u8_t *p, avf_u64_t pts, int pts_dts);

protected:
	avf_uint_t m_pmt_pid;

private:
	// to adjust pts of playlist
	avf_u32_t m_start_ms;
	avf_u32_t m_clipinfo_start_ms;	// for cliplist: this clip_info's offset
	avf_u32_t m_init_time_ms;		// for adjust pts
	avf_u64_t m_ref_clip_start_ms;	// for cliplist: this clip_info's start pts
};

//-----------------------------------------------------------------------
//
//  CVideoClipReader - for read video stream
//
//-----------------------------------------------------------------------

class CVideoClipReader: public CClipReader
{
	typedef CClipReader inherited;
	friend class CVDB;

private:
	static CVideoClipReader *Create(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info, avf_u64_t offset, bool bMuteAudio,
		bool bFixPTS, avf_u32_t init_time_ms);

private:
	CVideoClipReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info, avf_u64_t offset, bool bMuteAudio,
		bool bFixPTS, avf_u32_t init_time_ms);
	//virtual ~CVideoClipReader();

// IClipReader:
public:
	virtual avf_u64_t GetSize() {
		return m_clip_info.v_size[0];
	}
	virtual avf_u64_t GetPos() {
		return mpos.curr_pos;
	}

protected:
	virtual void OnClose();
	virtual int ReadClipL(avf_u8_t *buffer, avf_size_t size);
	virtual avf_status_t SeekToAddrL(avf_s64_t offset, int whence);
	virtual avf_status_t SeekToTimeL(avf_u32_t time_ms);

private:
	avf_status_t OpenSegment();
	avf_status_t SetSegment(clip_pos_s& pos);

private:
	clip_pos_s mpos;
	IAVIO *mp_io;
};

//-----------------------------------------------------------------------
//
//  CUploadClipReader - for upload
//
//-----------------------------------------------------------------------

class CUploadClipReader: public CClipReader
{
	typedef CClipReader inherited;
	friend class CVDB;

private:
	static CUploadClipReader *Create(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info, bool bMuteAudio, avf_u32_t upload_opt);

private:
	CUploadClipReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info, bool bMuteAudio, avf_u32_t upload_opt);
	//virtual ~CUploadClipReader();

// IClipReader:
public:
	virtual avf_u64_t GetSize();
	virtual avf_u64_t GetPos() {
		return m_bytes_read;
	}

protected:
	virtual void OnClose();
	virtual int ReadClipL(avf_u8_t *buffer, avf_size_t size);
	virtual avf_status_t SeekToAddrL(avf_s64_t offset, int whence);
	virtual avf_status_t SeekToTimeL(avf_u32_t time_ms);

private:
	enum {
		STATE_V0,
		STATE_PIC,
		STATE_GPS,
		STATE_OBD,
		STATE_ACC,
		STATE_V1,
		STATE_NEXT,
		STATE_END,
		NSTATE
	};

	enum {
		DATA_PIC,
		DATA_GPS,
		DATA_OBD,
		DATA_ACC,
		NDATA
	};

	// video info of segment
	struct v_pos_s {
		vdb_seg_t *seg;
		IAVIO *io;
		avf_s32_t seg_start_ms;	// relative to clipinfo start

		int stream;
		int v_opt;
		bool b_end;

		CIndexArray *array;
		int item_index;
		const index_item_t *_start_item;	// for debug

		avf_u32_t item_size;
		avf_u32_t item_remain;
		avf_u32_t seg_offset;
		avf_u32_t seg_remain;

		avf_u64_t clip_remain;
		avf_u64_t next_clip_addr;

		INLINE void Init(int stream, int v_opt) {
			this->seg = NULL;
			this->io = NULL;
			this->stream = stream;
			this->v_opt = v_opt;
		}

		INLINE const index_item_t *GetItem() {
			return array->GetItemAt(item_index);
		}

		INLINE avf_u64_t GetItemStartMs() {
			const index_item_t *item = GetItem();
			return seg->GetStartTime_ms() + item->time_ms;
		}

		INLINE avf_u64_t GetItemEndMs() {
			const index_item_t *item = GetItem();
			return GetItemStartMs() + ((item + 1)->time_ms - item->time_ms);
		}
	};

	// pic & raw info of segment
	struct data_pos_s {
		short upload_type;
		short next_state;

		CIndexArray *array;
		int item_index;
		int data_offset;
		const index_item_t *_start_item;	// for debug
		const avf_u8_t *_start_mem;		// for debug

		avf_u32_t item_remain;
		int nitem;

		INLINE void Init(int upload_type, int next_state) {
			this->upload_type = upload_type;
			this->next_state = next_state;
		}

		INLINE const index_item_t *GetItem() {
			return array->GetItemAt(item_index);
		}

		INLINE const avf_u8_t *GetMemBase() {
			CMemBuffer *pmb = array->GetMemBuffer();
			return pmb ? pmb->GetFirstBlock()->GetMem() : NULL;
		}
	};

private:
	avf_status_t Read_Init();
	avf_status_t Read_InitData(v_pos_s& vpos);
	avf_status_t Read_InitVideoItem(v_pos_s& vpos);
	avf_status_t Read_NextVideoItem(v_pos_s& vpos);
	int Read_video(v_pos_s& vpos, avf_u8_t *buffer, avf_size_t size, int next_state);
	int Read_data(data_pos_s& data, avf_u8_t *buffer, avf_size_t size);

private:
	INLINE void SetState(int state) {
		m_item_bytes = 0;
		mState = state;
	}

private:
	int mState;
	avf_u32_t m_upload_opt;
	avf_u32_t m_uploading;
	avf_uint_t m_item_bytes;
	avf_u64_t m_bytes_read;
	IAVIO *mp_pic_io;
	v_pos_s v0;	// valid for UPLOAD_GET_STREAM_0
	v_pos_s v1;	// valid for UPLOAD_GET_STREAM_1
	data_pos_s data[NDATA];
};

//-----------------------------------------------------------------------
//
//  CClipWriter
//
//-----------------------------------------------------------------------
class CClipWriter:
	public CObject,
	public IClipWriter
{
	typedef CObject inherited;
	friend class CVDB;

public:
	static CClipWriter* Create(CVDB *pVDB);

private:
	CClipWriter(CVDB *pVDB);
	virtual ~CClipWriter();
	void CloseL();

public:
	virtual void *GetInterface(avf_refiid_t refiid);
	virtual void AddRef() {
		inherited::AddRef();
	}
	virtual void Release();

// IClipWriter
public:
	virtual void SetCallback(void (*cb_proc)(void *context,
		int cbid, void *param, int), void *context);
	virtual void SetSegmentLength(avf_uint_t v_length, avf_uint_t p_length, avf_uint_t r_length);
	virtual avf_status_t StartRecord(int ch);
	virtual avf_status_t StopRecord();
	virtual avf_status_t WriteData(const avf_u8_t *data, avf_size_t size);

private:
	int write_video(const avf_u8_t *data, avf_size_t size);
	int write_picture(const avf_u8_t *data, avf_size_t size);
	int write_raw(const avf_u8_t *data, avf_size_t size);

private:
	INLINE void NextItem() {
		m_read_bytes = 0;
		m_remain_bytes = sizeof(upload_header_v2_t);
		mState = STATE_WAIT_HEADER;
	}

	IAVIO *StartVideo();
	IAVIO *StartPicture();
	avf_status_t FinishSegment(int stream);

private:
	enum {
		STATE_IDLE = 0,
		STATE_WAIT_HEADER,
		STATE_WRITE_STREAM_ATTR,
		STATE_WRITE_VIDEO,
		STATE_WRITE_PICTURE,
		STATE_WRITE_RAW,
		STATE_WRITE_OTHER,
		STATE_END,
		STATE_ERROR,
	};

private:
	CVDB *const mpVDB;
	bool mbClosed;
	DEFINE_HLIST_NODE(CClipWriter);

private:
	CMutex mMutex;
	int mState;
	bool mbEnd;
	int m_ch;
	void (*m_cb_proc)(void *, int, void*, int);
	void *m_cb_context;
	avf_uint_t m_v_length;
	avf_uint_t m_p_length;
	avf_uint_t m_r_length;

private:
	avf_uint_t m_read_bytes;
	avf_uint_t m_remain_bytes;
	avf_uint_t m_num_items;
	upload_header_v2_t m_header;
	avf_stream_attr_t m_stream_attr;

private:
	IAVIO *mp_video_io[2];
	avf_u32_t m_video_fpos[2];
	avf_u32_t m_seg_length[2];
	avf_u32_t m_video_samples[2];	// video
	avf_u32_t m_data_samples;	// picture, raw
	IAVIO *mp_picture_io;

private:
	avf_u8_t *mp_raw_mem;
	avf_size_t m_raw_size;

private:
	CMJPEGVideoFormat *mpFormat;
};

//-----------------------------------------------------------------------
//
//  CClipData
//
//-----------------------------------------------------------------------
class CClipData:
	public CObject,
	public IClipData
{
	typedef CObject inherited;
	friend class CVDB;
	DEFINE_INTERFACE;

	typedef CIndexArray::item_t index_item_t;

public:
	static CClipData *Create(CVDB *pVDB, vdb_clip_t *clip);

private:
	CClipData(CVDB *pVDB, vdb_clip_t *clip);
	virtual ~CClipData();

// IClipData
public:
	virtual avf_status_t ReadClipData(desc_s *desc, void **pdata, avf_size_t *psize);
	virtual avf_status_t ReadClipDataToFile(desc_s *desc, const char *filename);
	virtual avf_status_t ReadClipRawData(int data_types, void **pdata, avf_size_t *psize);
	virtual avf_status_t ReadClipRawDataToFile(int data_types, const char *filename);
	virtual void ReleaseData(void *data);

private:
	avf_status_t ReadClipRawData(int data_types, void **pdata, avf_size_t *psize, IAVIO *io);

public:
	avf_u8_t *AllocData(desc_s *desc, const index_item_t *item, avf_size_t *psize, avf_size_t extra_size);

private:
	DEFINE_HLIST_NODE(CClipData);
	bool mbClosed;

private:
	struct item_s {
		avf_u64_t curr_time_ms;
		bool b_first;
		INLINE void Init(avf_u64_t _curr_time_ms) {
			curr_time_ms = _curr_time_ms;
			b_first = true;
		}
	};

private:
	CVDB *mpVDB;
	vdb_clip_t *m_clip;
	IAVIO *m_io_video;	// read .ts file; TODO - stream 1
	IAVIO *m_io_picture;	// read .jpg file

	vdb_seg_t *m_seg;	// video, picture & raw
	vdb_seg_t *m_seg_1;	// video stream 1

	avf_u64_t m_start_time_ms;
	avf_u64_t m_end_time_ms;

	item_s m_items[kAllArrays];
	item_s m_video_1;	// video stream 1
};

#endif

