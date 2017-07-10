
#ifndef __VDB_FORMAT_H__
#define __VDB_FORMAT_H__

class vdb_clip_t;
class CVdbFormat;


#define PLAYLIST_FILENAME		"playlist"
#define PLAYLIST_PATH			"data/playlist/"
#define VDB_FILENAME			"vdb.info"


//-----------------------------------------------------------------------
//
//  CVdbFormat
//
//-----------------------------------------------------------------------
class CVdbFormat
{
	typedef struct ref_info_s {
		avf_u32_t ver_code;		// 'RCLP'
		avf_u32_t ref_clip_type;
		clip_id_t ref_clip_id;
		clip_id_t clip_id;		// the clip's id - same with the clip
		avf_u32_t plist_index;	// index in ref_clip_list
		avf_u32_t extra_size;		// bytes behind this struct
		avf_u64_t start_time_ms;
		avf_u64_t end_time_ms;
	} ref_info_t;

	typedef CIndexArray::item_t index_item_t;

public:
	enum {
		LOAD_CLIP_ALL = 0,
		LOAD_CLIP_CACHE = 1,
	};

	INLINE static avf_size_t round_up(avf_size_t size) {
		return ROUND_UP(size, 4);
	}

public:
	static avf_status_t LoadClipInfo(
		string_t *clipinfo_dir, vdb_clip_t *clip, IAVIO *io,
		avf_u64_t *ref_start_time_ms, avf_u64_t *ref_end_time_ms,
		int flag, bool b_remove_on_fail);
	static avf_status_t SaveClipInfo(string_t *clipinfo_dir, vdb_clip_t *clip, IAVIO *io);
	static avf_status_t SaveCopyClipInfo(string_t *clipinfo_dir, copy_clip_s *cclip, 
		avf_time_t gen_time, copy_clip_info_s& info, IAVIO *io);
	static avf_status_t SaveCopyStreamInfo(CWriteBuffer& buffer, 
		copy_clip_s *cclip, copy_stream_info_s& stream, int s);

	static avf_status_t LoadPlaylist(string_t *playlist_dir, clip_set_t& cs);
	static avf_status_t SavePlaylists(string_t *playlist_dir, IAVIO *io, playlist_set_t& plist_set);

	static avf_status_t SaveSegmentIndex(vdb_clip_t *clip, string_t *filename, IAVIO *io, int stream, seg_index_t *sindex, copy_seg_info_s *segi = NULL);
	static avf_status_t LoadSegmentIndex(const char *filename, IAVIO *io, seg_index_t *sindex, vdb_seg_t *seg);
	INLINE static avf_status_t LoadSegmentIndex(const char *filename, IAVIO *io, vdb_seg_t *seg) {
		return LoadSegmentIndex(filename, io, &seg->m_cache->sindex, seg);
	}

	static bool VdbFileExists(string_t *path);
	static avf_status_t SaveVdb(string_t *path, IAVIO *io, clip_set_t& cs, clip_dir_set_t& cdir_set);
	static avf_status_t LoadVdb(string_t *path, clip_set_t& cs, clip_dir_set_t& cdir_set);

private:
	static avf_status_t LoadStream(vdb_clip_t *clip, CReadBuffer& buffer);
	static avf_status_t LoadRefClips(vdb_clip_t *clip, CReadBuffer& buffer, int *p_num_buffered);
	static avf_status_t LoadRefClipInfo(CReadBuffer& buffer, ref_info_t& info, avf_u32_t *clip_attr, raw_data_t **pSceneData);

	static avf_status_t ReadClipDesc(vdb_clip_t *clip, CReadBuffer& buffer, avf_u32_t size);
	static avf_status_t ReadClipInfo(vdb_clip_t *clip, CReadBuffer& buffer, avf_u32_t size);
	static avf_status_t WriteClipDesc(list_head_t *list, CWriteBuffer& buffer);
	static avf_status_t WriteRawInfo(const raw_info_t *raw_info, CWriteBuffer& buffer);

	static avf_status_t SaveStream(vdb_clip_t *clip, vdb_stream_t *vdb_stream, CWriteBuffer& buffer);
	static avf_status_t SaveRefClips(vdb_clip_t *clip, CWriteBuffer& buffer);
	static avf_status_t SaveRCSHeader(CWriteBuffer& buffer, avf_size_t count, avf_size_t extra_size);

	static avf_status_t DoLoadPlaylist(clip_set_t& cs, CReadBuffer& buffer);
	static avf_status_t SaveOnePlaylist(ref_clip_set_t *rcs, CWriteBuffer& buffer);

	static avf_status_t SaveIndexItems(CWriteBuffer& buffer, CIndexArray *array,
		avf_u32_t seg_off_ms, avf_uint_t from, avf_uint_t size);
	static avf_status_t SaveIndexArray(CIndexArray *array, avf_u32_t fcc, CWriteBuffer& buffer,
		avf_int_t seg_off_ms, avf_uint_t from, avf_uint_t to);

	static avf_status_t LoadArray(CIndexArray **arrays, int index, CReadBuffer& buffer);
	static avf_status_t ReadArray(CIndexArray *array, int index, CReadBuffer& buffer);
	static avf_status_t ReadBlockData(CIndexArray *array, CReadBuffer& buffer);

	static avf_status_t DoLoadVdb(string_t *path, CReadBuffer& buffer, clip_set_t& cs, clip_dir_set_t& cdir_set);
	static avf_status_t LoadDirs(string_t *path, CReadBuffer& buffer, clip_dir_set_t& cdir_set);
	static avf_status_t LoadLastDir(string_t *path, CReadBuffer& buffer, clip_dir_set_t& cdir_set);
};

#endif

