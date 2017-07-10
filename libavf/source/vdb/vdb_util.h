
#ifndef __VDB_UTIL_H__
#define __VDB_UTIL_H__

class CVDB;
class CSocket;

typedef struct local_time_name_s {
	char buf[64];
	avf_size_t len;
} local_time_name_t;

bool name_to_local_time(const char *name, avf_time_t *local_time, const char *ext, const char *ext2, int *ptype);
void local_time_to_name(avf_time_t local_time, const char *ext, local_time_name_t& lname);

const char *get_raw_data_name(int raw_type);

int get_raw_array_index(int raw_type);
int get_vdb_array_index(int data_type);

class clip_dir_t;
class clip_dir_set_t;

//-----------------------------------------------------------------------
//
//  clip_dir_t
//
//-----------------------------------------------------------------------
class clip_dir_t
{
public:
	struct rb_node rbnode;
	list_head_t list_dir;			// link to all dir list
	clip_dir_set_t *m_set;

	avf_u32_t m_num_clips;		// number of clips
	avf_u32_t m_num_segs_0;		// number of segments
	avf_u32_t m_num_segs_1;		// number of segments

	avf_time_t m_dirname;
	string_t *vdb_path;

	string_t *CalcPathName(const char *subdir);
	string_t *CalcFileName(const char *subdir, local_time_name_t& lname);

	clip_dir_t(clip_dir_set_t *set, avf_time_t dirname, string_t *vdb_path);
	~clip_dir_t();

	static INLINE clip_dir_t *from_rbnode(struct rb_node *node) {
		return container_of(node, clip_dir_t, rbnode);
	}

	static INLINE clip_dir_t *from_node(list_head_t *node) {
		return list_entry(node, clip_dir_t, list_dir);
	}

	INLINE bool SegmentsLessThan(avf_size_t num_segments) {
		return m_num_segs_0 < num_segments && m_num_segs_1 < num_segments;
	}

	INLINE void ClipAdded();
	INLINE void ClipRemoved();

	INLINE void SegmentAdded(avf_size_t segs_0, avf_size_t segs_1);
	INLINE void SegmentRemoved(int stream);

	INLINE bool IsEmpty() {
		return m_num_clips == 0;
	}
};

//-----------------------------------------------------------------------
//
//  clip_dir_set_t
//
//-----------------------------------------------------------------------
class clip_dir_set_t
{
public:
	struct rb_root m_cdir_set;	// cdir tree
	list_head_t m_dir_list;		// cdir list
	avf_uint_t m_num_dirs;
	bool mbDirty;
	clip_dir_t *m_last_dir;

public:
	INLINE clip_dir_set_t() {
		Init();
	}
	~clip_dir_set_t();

	void Clear();

	clip_dir_t *FindDir(avf_time_t dirname);
	clip_dir_t *GetAvail(avf_size_t max_segments);
	clip_dir_t *AddClipDir(string_t *vdb_path, avf_time_t dirname, bool bCreate, bool *bExists);
	void RemoveClipDir(clip_dir_t *cdir, bool bDirty = true);

	clip_dir_t *GetLastDir();

	void Serialize(CMemBuffer *pmb);

private:
	INLINE void Init() {
		m_cdir_set.rb_node = NULL;
		list_init(m_dir_list);
		m_num_dirs = 0;
		mbDirty = false;
		m_last_dir = NULL;
	}
};

//-----------------------------------------------------------------------
//
// clip_dir_t
//
//-----------------------------------------------------------------------
INLINE void clip_dir_t::ClipAdded()
{
	m_num_clips++;
	m_set->mbDirty = true;
}

INLINE void clip_dir_t::ClipRemoved() {
	m_num_clips--;
	m_set->mbDirty = true;
}

INLINE void clip_dir_t::SegmentAdded(avf_size_t segs_0, avf_size_t segs_1)
{
	m_num_segs_0 += segs_0;
	m_num_segs_1 += segs_1;
	m_set->mbDirty = true;
}

INLINE void clip_dir_t::SegmentRemoved(int stream)
{
	if (stream == 0) m_num_segs_0--;
	else m_num_segs_1--;
	m_set->mbDirty = true;
}

//-----------------------------------------------------------------------
//
// vdb_set_t
//
//-----------------------------------------------------------------------

struct vdb_set_s {
	// 1
	CVDB *(*get_vdb)(void *context, const char *vdb_id);
	void (*release_vdb)(void *context, const char *vdb_id, CVDB *vdb);
	// 2
	int (*get_all_clips)(void *context, uint32_t *p_num_clips, vdb_clip_desc_t **p_clips);
	void (*release_clips)(void *context, uint32_t num_clips, vdb_clip_desc_t *clips);
	// 3
	int (*get_clip_poster)(void *context, const char *vdb_id,
		int clip_type, uint32_t clip_id, uint32_t *p_size, void **p_data);
	void (*release_clip_poster)(void *context, const char *vdb_id, uint32_t size, void *data);
	// 4
	void (*on_clip_created)(void *context, const char *vdb_id, int clip_type, uint32_t clip_id);
	void (*on_clip_deleted)(void *context, const char *vdb_id, int clip_type, uint32_t clip_id);
};

//-----------------------------------------------------------------------
//
// misc
//
//-----------------------------------------------------------------------

void vdb_get_server_ip(CSocket *socket, char *server_ip);

int vdb_format_url(const char *server_ip, char *url, const char *vdb_id, int clip_type, clip_id_t clip_id,
	int stream, int upload_opt, int b_playlist, int url_type, avf_u64_t clip_time_ms, avf_u32_t length_ms,
	avf_u32_t param1, avf_u32_t param2);

int vdb_format_hls_item(char *url, bool bMuteAudio, bool bFixPTS,
	avf_u64_t clip_time_ms, avf_u32_t length_ms, avf_u32_t param1, avf_u32_t param2);

const char *vdb_parse_url(const char *url, char **pvdb_id, int *clip_type, clip_id_t *clip_id,
	int *stream, int *upload_opt, int *b_playlist, int *url_type,
	avf_u64_t *clip_time_ms, avf_u32_t *length_ms, int *b_mute_audio, int *b_full_path, int *b_spec_time,
	avf_u32_t *param1, avf_u32_t *param2);

void vdb_format_url_clips(char *url, avf_u64_t cdir_clip_id, 
	int stream, avf_u64_t clip_time_ms, avf_u32_t length_ms,
	int b_mute_audio, avf_u32_t param1, avf_u32_t param2);

const char *vdb_parse_url_clips(const char *url, avf_u64_t *cdir_clip_id, int *stream, int *url_type,
	avf_u64_t *clip_time_ms, avf_u32_t *length_ms, int *b_mute_audio, int *b_full_path, int *b_spec_time,
	avf_u32_t *param1, avf_u32_t *param2);

void vdb_format_playlist_path(CSocket *socket, char *path, clip_id_t plist_id);

#endif

