
#ifndef __VDB_CLIP_H__
#define __VDB_CLIP_H__

#include "avio.h"

class CIndexArray;
class CRawFifo;

class vdb_seg_t;
class vdb_stream_t;

class seg_cache_t;
class seg_cache_list_t;

class clip_cache_t;
class clip_cache_list_t;

class vdb_clip_t;
class ref_clip_t;

class clip_set_t;
class ref_clip_set_t;
class playlist_set_t;

class CWriteBuffer;
class CReadBuffer;

struct clip_dir_t;

typedef struct vdb_size_s {
	avf_u64_t v_size;		// video
	avf_u64_t p_size;		// picture
	avf_u64_t r_size;		// rawdata
	INLINE void Init() {
		v_size = 0;
		p_size = 0;
		r_size = 0;
	}
	INLINE avf_u64_t GetTotal() const {
		return v_size + p_size + r_size;
	}
} vdb_size_t;

//-----------------------------------------------------------------------
//
//  IClipOwner
//
//-----------------------------------------------------------------------
class IClipOwner
{
public:
	virtual seg_cache_list_t& GetSegCacheList() = 0;
	virtual clip_cache_list_t& GetClipCacheList() = 0;
	virtual clip_set_t& GetClipSet() = 0;
	virtual ref_clip_set_t *GetRefClipSet(int clip_type) = 0;

	virtual string_t *GetIndexFileName(const vdb_seg_t *seg) = 0;
	virtual string_t *GetPictureFileName(const vdb_seg_t *seg) = 0;
	virtual string_t *GetVideoFileName(const vdb_seg_t *seg) = 0;

	virtual void RemoveIndexFile(const vdb_seg_t *seg) = 0;
	virtual void RemoveVideoFile(const vdb_seg_t *seg) = 0;
	virtual void RemovePictureFile(const vdb_seg_t *seg) = 0;

	virtual void RemoveClipFiles(vdb_clip_t *clip) = 0;
	virtual void ClipRemoved(clip_dir_t *cdir) = 0;

	virtual bool LoadSegCache(vdb_seg_t *seg) = 0;
	virtual bool LoadClipCache(vdb_clip_t *clip) = 0;

	virtual avf_status_t OpenSegmentVideo(const vdb_seg_t *seg, IAVIO **pio) = 0;
};

//-----------------------------------------------------------------------
//
//  data types
//
//-----------------------------------------------------------------------

enum {
	kPictureArray = 0,
	kVideoArray = 1,
	kGps0Array = 2,
	kAcc0Array = 3,
	kObd0Array = 4,
	kTotalArray,	// do not change
	//
	kVmemArray = kTotalArray,
	// other arrays
	kAllArrays,
	kNumExtraArrays = kAllArrays - kTotalArray,
};

typedef struct array_attr_s {
	int type;
	int block_size;
} array_attr_t;

extern const array_attr_t g_array_attr[];

//-----------------------------------------------------------------------
//
//  CRawFifo
//
//-----------------------------------------------------------------------
class CRawFifo: public CObject
{
public:
	enum {
		MAX_ITEMS = 20,
	};

public:
	struct item_s {
		raw_data_t *rdata;
		INLINE void Release() {
			rdata->Release();
		}
	};

private:
	item_s mItems[MAX_ITEMS];
	int mWritePos;
	int mReadPos;
	int mCount;

public:
	static CRawFifo *Create();
	void Serialize(CMemBuffer *pmb) const;

private:
	CRawFifo();
	virtual ~CRawFifo();

public:
	INLINE item_s *Peek() {
		return mCount == 0 ? NULL : mItems + mReadPos;
	}
	item_s *Pop();
	item_s *Push(raw_data_t *rdata);
	void Clear();
};

//-----------------------------------------------------------------------
//
//  CIndexArray
//
//-----------------------------------------------------------------------
class CIndexArray: public CObject
{
	friend class CVdbFormat;

public:
	typedef raw_dentry_t item_t;

	typedef struct pos_s
	{
		CIndexArray *array;
		const item_t *item;

		INLINE avf_size_t GetRemain() const {
			return array->GetRemain(item);
		}

		INLINE avf_u32_t GetItemBytes() const {
			return CIndexArray::GetItemSize(item);
		}

		INLINE avf_u32_t GetItemDuration() const {
			return CIndexArray::GetItemDuration(item);
		}
	} pos_t;

	enum {
		TYPE_INDEX = 0,		// index only
		TYPE_BLOCK_DATA = 1,	// has extra data block
	};

public:
	static CIndexArray *Create(int type, avf_size_t block_size);

	INLINE static CIndexArray *Create(int index) {
		return Create(g_array_attr[index].type, g_array_attr[index].block_size);
	}

	static CIndexArray *CreateBlockData() {
		return Create(TYPE_BLOCK_DATA, 16*KB);
	}

	void Serialize(CMemBuffer *pmb) const;

private:
	CIndexArray(int type, int block_size);
	virtual ~CIndexArray();

public:
	INLINE void Clear() {
		mItems._Clear();
		m_data_size = 0;
		m_length_ms = 0;
		if (pmb) {
			pmb->Clear();
		}
	}

	avf_status_t DoAddEntry(avf_s32_t time_ms, avf_u32_t fpos, avf_s32_t end_time_ms, avf_u32_t end_fpos);
	INLINE avf_status_t AddEntry(avf_s32_t time_ms, avf_u32_t fpos) {
		return DoAddEntry(time_ms, fpos, time_ms, fpos);
	}

	avf_status_t AddRawDataEntry(avf_s32_t time_ms, raw_data_t *raw);
	avf_status_t AddRawDataEntryEx(avf_s32_t time_ms, avf_s32_t end_time_ms, raw_data_t *raw);

	void UpdateTailItem(avf_u32_t data_size, avf_u32_t length_ms);

	const item_t *FindIndexItem(avf_s32_t time_ms) const;
	const item_t *FindIndexItemByAddr(avf_u32_t fpos) const;

	// find item: item->time_ms >= time_ms
	INLINE const item_t *FindIndexItem_Next(avf_s32_t time_ms) const {
		const item_t *item = FindIndexItem(time_ms - 1);
		if (item == NULL) return NULL;
		return item->time_ms >= time_ms ? item : item + 1;
	}

	// find item: item->fpos >= fpos
	INLINE const item_t *FindIndexItemByAddr_Next(avf_u32_t fpos) const {
		const item_t *item = FindIndexItemByAddr(fpos - 1);
		if (item == NULL) return NULL;
		return item->fpos >= fpos ? item : item + 1;
	}

	INLINE avf_size_t GetCount() const {
		return mItems.count;
	}

	INLINE bool IsEmpty() const {
		return mItems.count == 0;
	}

	INLINE const item_t *GetFirstItem() const {
		return mItems.elems;
	}

	INLINE const item_t *GetItemAt(int index) const {
		return mItems.elems + index;
	}

	INLINE avf_size_t GetItemIndex(const item_t *item) const {
		return item - mItems.elems;
	}

	INLINE avf_size_t GetRemain(const item_t *item) const {
		return mItems.count - (item - mItems.elems);
	}

	static INLINE avf_u32_t GetItemSize(const item_t *item) {
		return (item + 1)->fpos - item->fpos;
	}

	static INLINE avf_u32_t GetItemDuration(const item_t *item) {
		return (item + 1)->time_ms - item->time_ms;
	}

	static INLINE avf_u32_t GetSizeInclusive(const item_t *item_from, const item_t *item_to) {
		return (item_to + 1)->fpos - item_from->fpos;
	}

	static INLINE avf_u32_t GetDurationInclusive(const item_t *item_from, const item_t *item_to) {
		return (item_to + 1)->time_ms - item_from->time_ms;
	}

	static INLINE avf_u32_t GetCountInclusive(const item_t *item_from, const item_t *item_to) {
		return (item_to - item_from) + 1;
	}

	INLINE int GetType() const {
		return mType;
	}

	INLINE CMemBuffer *GetMemBuffer() const {
		return pmb;
	}

	INLINE void *GetMem() const {
		return pmb->GetFirstBlock()->GetMem();
	}

	INLINE avf_size_t GetMemSize() const {
		CMemBuffer::block_t *block = pmb->GetFirstBlock();
		return block ? block->GetSize() : 0;
	}

	static INLINE avf_u32_t RawDataSize(raw_data_t *raw) {
		return sizeof(item_t) + raw->GetSize();
	}

private:
	const int mType;		// TYPE_INDEX, TYPE_BLOCK_DATA
	TDynamicArray<item_t> mItems;
	CMemBuffer *pmb;	// valid for TYPE_BLOCK_DATA
	avf_u32_t m_data_size;
	avf_u32_t m_length_ms;
};

//-----------------------------------------------------------------------
//
//  seg_index_t
//
//-----------------------------------------------------------------------
class seg_index_t
{
public:
	CIndexArray *arrays[kAllArrays];
	TDynamicArray<CIndexArray*> ex_raw_arrays;

public:
	INLINE seg_index_t() {
		SET_ARRAY_NULL(arrays);
		ex_raw_arrays._Init();
	}

	INLINE ~seg_index_t() {
		SAFE_RELEASE_ARRAY(arrays);
		CIndexArray **parray = ex_raw_arrays.Item(0);
		for (avf_size_t i = ex_raw_arrays.count; i; i--, parray++) {
			CIndexArray *array = *parray;
			if (array) {
				array->Release();
			}
		}
		ex_raw_arrays._Release();
	}
};

//-----------------------------------------------------------------------
//
//  seg_cache_t
//
//-----------------------------------------------------------------------
class seg_cache_t
{
public:
	list_head_t list_cache;
	vdb_seg_t *seg;
	int lock_count;
	seg_index_t sindex;

public:
	INLINE seg_cache_t(vdb_seg_t *seg) {
		this->seg = seg;
		this->lock_count = 0;
	}

	INLINE ~seg_cache_t() {
	}

	INLINE CIndexArray *GetArray(int index) {
		return sindex.arrays[index];
	}

	static INLINE seg_cache_t *from_node(list_head_t *node) {
		return list_entry(node, seg_cache_t, list_cache);
	}

	void Serialize(CMemBuffer *pmb) const;
};

//-----------------------------------------------------------------------
//
//  seg_cache_list_t
//
//-----------------------------------------------------------------------
class seg_cache_list_t
{
public:
	seg_cache_list_t(bool b_server_mode, avf_size_t max_cache);
	~seg_cache_list_t();

public:
	void AllocCache(vdb_seg_t *seg);
	void ReleaseCache(seg_cache_t *cache);

	void LockSegCache(vdb_seg_t *seg);
	void UnlockSegCache(vdb_seg_t *seg);

	INLINE IAVIO *GetPictureIO(const vdb_seg_t *seg) {
		if (seg == m_io_seg) {
			m_io_hit++;
			return m_io;
		}
		return DoGetPictureIO(seg);
	}

	INLINE void ReleasePictureIO() {
		if (mb_server_mode) {
			avf_safe_release(m_io);
			m_io_seg = NULL;
		}
	}

	INLINE void Trim() {
		if (m_num_cache > m_max_cache + m_num_locked) {
			TrimCache(m_max_cache);
		}
	}

	INLINE void Clear() {
		TrimCache(0);
	}

	// avoid be trimed
	INLINE void BringToFront(seg_cache_t *cache) {
		list_head_t *node = &cache->list_cache;
		__list_del(node);
		list_add(node, m_cache_list);
	}

	void Serialize(CMemBuffer *pmb) const;

private:
	IAVIO *DoGetPictureIO(const vdb_seg_t *seg);
	void TrimCache(avf_uint_t count);

private:
	const bool mb_server_mode;
	list_head_t m_cache_list;

	const avf_size_t m_max_cache;
	avf_size_t m_num_cache;
	avf_size_t m_num_locked;

	const vdb_seg_t *m_io_seg;	// for index picture
	IAVIO *m_io;

	avf_size_t m_io_hit;
	avf_size_t m_io_miss;
};

//-----------------------------------------------------------------------
//
//  vdb_seg_t
//
//-----------------------------------------------------------------------
class vdb_seg_t
{
public:
	typedef CIndexArray::pos_t index_pos_t;
	typedef CIndexArray::item_t index_item_t;

	struct info_t
	{
		avf_u32_t id;				// id in clip (start from 0, increase mono)
		avf_u32_t duration_ms;		// length in ms

		avf_u64_t start_time_ms;		// abs start time in virtual clip
		avf_u64_t v_start_addr;		// video start offset in virtual clip

		avf_u32_t raw_data_size;		// index items + raw data (not including header)
		avf_u32_t seg_data_size[kTotalArray];	// video/picture/raw data size

		avf_stream_attr_s stream_attr;
	};

public:
	struct rb_node rbnode;	// link to vdb_stream_t::seg_tree
	vdb_clip_t *m_clip;		// owner clip
	int m_stream;			// stream number

	avf_time_t m_subdir_name;
	avf_time_t m_index_name;
	avf_time_t m_video_name;
	avf_time_t m_picture_name;
	avf_u32_t m_first_pic_size;	// for indexpic

	avf_u32_t m_ref_count;		// ref counter
	info_t m_info;				// header info

	avf_u32_t extra_data_size[kNumExtraArrays];
	TDynamicArray<avf_u32_t> extra_raw_sizes;

	avf_u32_t m_mark_count;	// marked(n, from marked clips) + protected (from buffered clips)
	avf_u64_t m_filesize;	// index + video + picture

	seg_cache_t *m_cache;

public:
	vdb_seg_t(vdb_clip_t *clip, int stream,
		avf_time_t subdir_name, avf_time_t index_name, 
		avf_time_t video_name, avf_time_t picture_name,
		const info_t *info);

	INLINE ~vdb_seg_t() {
		AVF_ASSERT(m_cache == NULL);
		extra_raw_sizes._Release();
	}

	static INLINE vdb_seg_t *from_node(struct rb_node *node) {
		return container_of(node, vdb_seg_t, rbnode);
	}

	INLINE vdb_seg_t *GetPrevSegment() const {
		struct rb_node *prev = rb_prev(&rbnode);
		return prev ? from_node(prev) : NULL;
	}

	INLINE vdb_seg_t *GetNextSegment() const {
		struct rb_node *next = rb_next(&rbnode);
		return next ? from_node(next) : NULL;
	}

	INLINE vdb_seg_t *GetPrevSegInRefClip() {
		vdb_seg_t *prev = GetPrevSegment();
		return prev && (prev->m_info.id + 1) == m_info.id ? prev : NULL;
	}

	INLINE vdb_seg_t *GetNextSegInRefClip() {
		vdb_seg_t *next = GetNextSegment();
		return next && (m_info.id + 1) == next->m_info.id ? next : NULL;
	}

	INLINE void AddRef() {
		m_ref_count++;
	}

	INLINE bool ReleaseRef() {
		if (m_ref_count == 0) {
			AVF_LOGE("ref_count is 0");
			return true;
		}
		return --m_ref_count == 0;
	}

	INLINE void AddNewExtraRaw() {
		avf_u32_t *size = extra_raw_sizes._Append(NULL);
		*size = 0;
	}

	INLINE void IncRawSize(int index, avf_u32_t inc_size) {
		avf_u32_t *size = extra_raw_sizes.Item(index);
		*size += inc_size;
	}

	INLINE bool IsStartSeg() {
		return m_info.id == 0;
	}

	INLINE CIndexArray *GetIndexArray(int index) {
		seg_cache_t *cache = LoadSegCache();
		return cache ? cache->GetArray(index) : NULL;
	}

	// time range
	INLINE avf_u64_t GetStartTime_ms() const {
		return m_info.start_time_ms;
	}
	INLINE avf_u32_t GetDuration_ms() const {
		return m_info.duration_ms;
	}
	INLINE avf_u64_t GetEndTime_ms() const {
		return GetStartTime_ms() + GetDuration_ms();
	}

	INLINE void SetDuration_ms(avf_u32_t duration_ms) {
		m_info.duration_ms = duration_ms;
	}

	// file range
	INLINE avf_u64_t GetStartAddr() const {
		return m_info.v_start_addr;
	}
	INLINE avf_u64_t GetEndAddr() const {
		return GetStartAddr() + GetVideoDataSize();
	}

	// data size
	INLINE avf_u32_t GetVideoDataSize() const {
		return m_info.seg_data_size[kVideoArray];
	}
	INLINE avf_u32_t GetPictureDataSize() const {
		return m_info.seg_data_size[kPictureArray];
	}
	INLINE avf_u32_t GetRawDataSize() const {
		return m_info.raw_data_size;
	}
	INLINE avf_u32_t GetRawDataSize(avf_uint_t index) const {
		return index >= kTotalArray ? 0 : m_info.seg_data_size[index];
	}

	INLINE vdb_stream_t* GetStream() const;
	INLINE int GetVideoType() const;
	INLINE void UpdateStreamAttr();

	INLINE void IncFileSize(avf_u32_t inc);
	INLINE void AdjustFileSize(avf_u64_t old_filesize);

	seg_cache_t *LoadSegCache();

	INLINE string_t *GetIndexFileName() const;
	INLINE string_t *GetPictureFileName() const;
	INLINE string_t *GetVideoFileName() const;

	avf_status_t OpenVideoRead(IAVIO **pio) const;
	avf_status_t OpenPictureRead(IAVIO *io) const;

	avf_status_t ReadIndexPicture(avf_u32_t offset, avf_u8_t *buffer, avf_u32_t size) const;
	avf_status_t ReadIFrame(avf_u32_t offset, avf_u32_t bytes, CMemBuffer *pmb, avf_u8_t *pFrameSize) const;

	struct tsb_s {
		IAVIO *io;
		avf_u32_t bytes;
		avf_u8_t *buf;
		avf_u32_t buf_size;
		avf_u8_t *packet;
		avf_u32_t npackets;
	};

	static avf_status_t DoReadIFrame(IAVIO *io, avf_u32_t bytes, avf_u8_t *buf, avf_u32_t buf_size, CMemBuffer *pmb, avf_u8_t *pFrameSize);
	static avf_status_t DoReadTSB(tsb_s *tsb);

	avf_status_t GetFirstPos(int index, index_pos_t *pos);

	vdb_seg_t *FindSegWithRawData(int index, avf_u64_t end_time_ms, index_pos_t *pos);

	avf_status_t FindPosRelative(int index, avf_s32_t seg_time_ms, index_pos_t *pos);

	INLINE avf_status_t FindPos(int index, avf_u64_t clip_time_ms, index_pos_t *pos) {
		return FindPosRelative(index, (avf_s32_t)(clip_time_ms - GetStartTime_ms()), pos);
	}

	INLINE avf_status_t FindPosEx(int index, avf_u64_t clip_time_ms,
			index_pos_t *pos, avf_u32_t *duration, avf_u32_t *bytes) {
		avf_status_t status = FindPos(index, clip_time_ms, pos);
		if (status == E_OK) {
			*duration = pos->GetItemDuration();
			*bytes = pos->GetItemBytes();
		}
		return status;
	}

	void Serialize(CMemBuffer *pmb) const;
};

//-----------------------------------------------------------------------
//
//  vdb_stream_t: a list of segments
//	the segments may be not contiguous, but should be increasing
//
//-----------------------------------------------------------------------
class vdb_stream_t
{
public:
	struct size_info_s {
		avf_u64_t duration_ms;
		avf_u64_t video_size;
		avf_u64_t picture_size;
	};

public:
	vdb_clip_t *m_clip;		// owner clip
	int m_index;			// stream index, 0 or 1
	struct rb_root seg_tree;	// all segments
	avf_size_t m_num_segs;	// total segments
	seg_cache_list_t* mp_seg_cache_list;

public:
	vdb_stream_t(seg_cache_list_t& seg_cache_list, vdb_clip_t *clip, int index);
	~vdb_stream_t();

private:
	void ReleaseSegments(struct rb_node *node);

public:
	vdb_seg_t *AddSegment(avf_time_t subdir_name, avf_time_t index_name, 
		avf_time_t video_name, avf_time_t picture_name,
		const vdb_seg_t::info_t *info);
	void RemoveSegment(vdb_seg_t *seg);
	void RemoveSegmentFiles(vdb_seg_t *seg);
	void RemoveAllFiles();

	vdb_seg_t *FindSegmentById(avf_uint_t id);
	vdb_seg_t *FindSegmentByTime(avf_u64_t clip_time_ms);
	vdb_seg_t *FindSegmentSinceTime(avf_u64_t start_time_ms);
	vdb_seg_t *FindSegmentByAddr(avf_u64_t offset);

	avf_size_t AddReference(avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_int_t mark_count);
	avf_size_t RemoveReference(avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_int_t mark_count);
	avf_size_t IncReference(avf_u64_t end_time_ms, avf_u32_t inc_ms, avf_int_t mark_count);
	avf_u64_t GetFileSize(avf_u64_t start_time_ms, avf_u64_t end_time_ms);

	bool CalcNumSegments(avf_size_t num, avf_u64_t start_time_ms, avf_u64_t end_time_ms);
	void Serialize(CMemBuffer *pmb) const;

	INLINE const avf_stream_attr_s& GetStreamAttr() const;

	INLINE vdb_seg_t *GetFirstSegment() const {
		struct rb_node *first = rb_first(&seg_tree);
		return first ? vdb_seg_t::from_node(first) : NULL;
	}

	INLINE vdb_seg_t *GetLastSegment() const {
		struct rb_node *last = rb_last(&seg_tree);
		return last ? vdb_seg_t::from_node(last) : NULL;
	}

	INLINE bool IsEmpty() const {
		return m_num_segs == 0;
	}

	INLINE avf_size_t GetNumSegments() const {
		return m_num_segs;
	}
};

//-----------------------------------------------------------------------
//
//  clip_cache_t
//
//-----------------------------------------------------------------------
class clip_cache_t
{
public:
	list_head_t list_cache;	// link to clip_cache_t::m_cache_list
	vdb_clip_t *clip;

	vdb_stream_t main_stream;
	vdb_stream_t sub_stream;

public:
	clip_cache_t(seg_cache_list_t& seg_cache_list, vdb_clip_t *clip);
	INLINE ~clip_cache_t() {}

	static INLINE clip_cache_t *from_node(list_head_t *node) {
		return list_entry(node, clip_cache_t, list_cache);
	}
};

//-----------------------------------------------------------------------
//
//  clip_cache_list_t
//
//-----------------------------------------------------------------------
class clip_cache_list_t
{
public:
	clip_cache_list_t(seg_cache_list_t& seg_cache_list, bool bKeepAll, avf_size_t max_cache);
	~clip_cache_list_t();

public:
	void AllocCache(vdb_clip_t *clip);
	void ReleaseCache(clip_cache_t *cache);

	INLINE void Trim() {
		if (!mb_keep_all && m_num_cache >= m_max_cache) {
			TrimCache(m_max_cache);
		}
	}

	INLINE void Clear() {
		TrimCache(0);
	}

	INLINE void BringToFront(clip_cache_t *cache) {
		list_head_t *node = &cache->list_cache;
		__list_del(node);
		list_add(node, m_cache_list);
	}

	void Serialize(CMemBuffer *pmb) const;

private:
	void TrimCache(avf_uint_t count);

private:
	list_head_t m_cache_list;
	const avf_size_t m_max_cache;
	avf_size_t m_num_cache;
	const bool mb_keep_all;	// do not release any cache
	seg_cache_list_t& m_seg_cache_list;
};

//-----------------------------------------------------------------------
//
//  CClipDesc
//
//-----------------------------------------------------------------------

#define CLIP_DESC_NULL		0
#define CLIP_DESC_UUID		MKFCC_LE('U','U','I','D')
#define CLIP_DESC_OBDVIN	MKFCC_LE('V','I','N','0')
#define CLIP_DESC_VINCONFIG	MKFCC_LE('V','I','N','C')

struct clip_desc_item_s
{
	list_head_t list_desc;
	avf_u32_t type;
	avf_size_t size;
	// follows byte array

	INLINE avf_u8_t *GetMem() {
		return (avf_u8_t*)(this + 1);
	}

	static clip_desc_item_s *from_node(list_head_t *node) {
		return container_of(node, clip_desc_item_s, list_desc);
	}
};

//-----------------------------------------------------------------------
//
//  vdb_clip_t
//
//-----------------------------------------------------------------------

typedef struct raw_info_s {
	uint32_t	interval_gps;
	uint32_t	interval_acc;
	uint32_t	interval_obd;
	uint32_t	reserved;
	INLINE bool ShouldSave() {
		return interval_gps + interval_acc + interval_obd > 0;
	}
} raw_info_t;

class vdb_clip_t
{
public:
	struct rb_node rbnode;			// link to clip_set_t
	list_head_t list_clip;			// link to clip_list

	clip_set_t *m_clip_set;
	list_head_t ref_clip_list;		// ref clip list (buffered, marked & playlist)
	avf_size_t num_ref_clips;		// number of ref clips (including buffered)

	clip_dir_t *m_cdir;				// not NULL in server mode
	clip_id_t clip_id;				// unique in clip set
	clip_id_t buffered_clip_id;		//
	sys_time_t clip_date;			// clip date (local time)
	avf_s32_t gmtoff;				// time zone

	avf_u32_t lock_count;			// if not 0, then clip cache cannot be released
	avf_u32_t m_num_segs_0;			// for main stream
	avf_u32_t m_num_segs_1;			// for sub stream

	avf_time_t first_pic_name;		// used only for server; AVF_INVALID_TIME
	avf_size_t first_pic_size;		// used only for server; 0

	vdb_size_t m_size_main;
	vdb_size_t m_size_sub;

	avf_stream_attr_s main_stream_attr;
	avf_stream_attr_s sub_stream_attr;
	avf_u8_t m_video_type_0;
	avf_u8_t m_video_type_1;

	raw_info_t raw_info;
	TDynamicArray<avf_u32_t> m_extra_raw_fcc;

	IClipOwner *m_owner;
	clip_cache_t *m_cache;

	avf_u32_t m_clipinfo_filesize;	// clipinfo file size
	avf_u64_t m_clip_size;		// all segments files size
	avf_u64_t m_marked_size;		// marked segments files size

	avf_u32_t m_clip_attr;
	list_head_t m_desc_list;

public:
	INLINE bool IsLive() {
		return (m_clip_attr & CLIP_ATTR_LIVE) != 0;
	}
	INLINE void SetLive() {
		m_clip_attr |= CLIP_ATTR_LIVE;
	}
	INLINE void ClearLive() {
		m_clip_attr &= ~CLIP_ATTR_LIVE;
	}

public:
	vdb_clip_t(IClipOwner *owner, clip_set_t *clip_set, clip_dir_t *cdir, clip_id_t clip_id);
	~vdb_clip_t();

	static INLINE vdb_clip_t *from_node(struct rb_node *node) {
		return container_of(node, vdb_clip_t, rbnode);
	}
	static INLINE vdb_clip_t *from_cs_node(list_head_t *node) {
		return container_of(node, vdb_clip_t, list_clip);
	}

	void GenerateUUID();

	clip_desc_item_s *SaveDescData(const avf_u8_t *data, avf_size_t size, avf_u32_t type);

	void SaveUUID(const avf_u8_t *data, avf_size_t size);

	INLINE void SaveObdVin(const avf_u8_t *data, avf_size_t size) {
		SaveDescData(data, size, CLIP_DESC_OBDVIN);
	}

	INLINE void SaveVinConfig(const avf_u8_t *data, avf_size_t size) {
		SaveDescData(data, size, CLIP_DESC_VINCONFIG);
	}

	clip_desc_item_s *FindClipDesc(avf_u32_t type);

	INLINE clip_desc_item_s *FindUUID() {
		return FindClipDesc(CLIP_DESC_UUID);
	}

	INLINE clip_desc_item_s *FindObdVin() {
		return FindClipDesc(CLIP_DESC_OBDVIN);
	}

	void GetUUID(avf_u8_t uuid[UUID_LEN]);

	avf_size_t GetClipDesc(CMemBuffer *pmb);

	INLINE void AddRawFCC(avf_u32_t fcc) {
		avf_u32_t *pfcc = m_extra_raw_fcc._Append(NULL);
		*pfcc = fcc;
		m_clip_attr |= CLIP_ATTR_HAS_EX_RAW_DATA;
	}

	ref_clip_t *NewRefClip(avf_u32_t ref_clip_type, clip_id_t ref_clip_id,
		avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_u32_t clip_attr);

	void LockClip(avf_u64_t start_time_ms, avf_u64_t end_time_ms);
	void _UnlockClip(avf_u64_t start_time_ms, avf_u64_t end_time_ms);

	void AddReference(avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_int_t mark_count);
	void RemoveReference(avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_int_t mark_count);
	void IncReference(avf_u64_t end_time_ms, avf_u32_t inc_ms, avf_int_t mark_count);
	avf_u64_t GetFileSize(avf_u64_t start_time_ms, avf_u64_t end_time_ms);

	INLINE void AddReference(ref_clip_t *ref_clip);
	INLINE void RemoveReference(ref_clip_t *ref_clip);

	void LockAllSegments();

	clip_cache_t *LoadClipCache();

	void ReleaseAllRefClips();

	avf_status_t GetMinStartTime(int stream, avf_u64_t time_ms, avf_u64_t& min_time_ms);
	avf_status_t GetMaxEndTime(int stream, avf_u64_t time_ms, avf_u64_t& max_time_ms);

	avf_status_t GetMinStartTime(avf_u64_t time_ms, avf_u64_t& min_time_ms);
	avf_status_t GetMaxEndTime(avf_u64_t time_ms, avf_u64_t& max_time_ms);

	INLINE void Lock() {
		lock_count++;
	}

	INLINE void Unlock() {
		if (lock_count == 0) {
			AVF_LOGE("lock count is 0");
		} else {
			lock_count--;
		}
	}

	INLINE void BeforeRemoveSegment(vdb_seg_t *seg);

	INLINE const avf_stream_attr_s& GetStreamAttr(int stream) const {
		return stream == 0 ? main_stream_attr : sub_stream_attr;
	}

	INLINE void SetStreamAttr(int stream, const avf_stream_attr_s *stream_attr) {
		if (stream == 0) main_stream_attr = *stream_attr;
		else sub_stream_attr = *stream_attr;
	}

	INLINE void SetVideoType(int stream, int video_type) {
		if (stream == 0) m_video_type_0 = video_type;
		else m_video_type_1 = video_type;
	}

	INLINE bool CalcNumSegments(avf_size_t num, avf_u64_t start_time_ms, avf_u64_t end_time_ms) {
		if (!LoadClipCache()) return false;
		return m_cache->main_stream.CalcNumSegments(num, start_time_ms, end_time_ms);
	}

	INLINE vdb_stream_t* __GetStream(int stream) const {
		return (stream == 0) ? &m_cache->main_stream : &m_cache->sub_stream;
	}

	INLINE vdb_stream_t* GetStream(int stream) {
		if (!LoadClipCache()) return NULL;
		return __GetStream(stream);
	}

	INLINE vdb_seg_t *GetFirstSegment(int stream) {
		if (!LoadClipCache()) return NULL;
		return __GetStream(stream)->GetFirstSegment();
	}

	INLINE vdb_seg_t *GetLastSegment(int stream) {
		if (!LoadClipCache()) return NULL;
		return __GetStream(stream)->GetLastSegment();
	}

	INLINE avf_size_t GetNumSegments(int stream) const {
		return stream == 0 ? m_num_segs_0 : m_num_segs_1;
	}

	INLINE bool IsNotUsed() const {
		// the clip is useless when there're no ref clips nor locked
		return num_ref_clips == 0 && lock_count == 0;
	}

	INLINE int GetVideoType(int stream) {
		return stream == 0 ? m_video_type_0 : m_video_type_1;
	}

	INLINE avf_u64_t GetVideoSize() const {
		return m_size_main.v_size + m_size_sub.v_size;
	}
	INLINE avf_u64_t GetPictureSize() const {
		return m_size_main.p_size + m_size_sub.p_size;
	}
	INLINE avf_u64_t GetRawDataSize() const {
		return m_size_main.r_size + m_size_sub.r_size;
	}

	INLINE void IncVideoDataSize_clip(int stream, avf_u32_t inc);
	INLINE void IncPictureDataSize_clip(int stream, avf_u32_t inc);
	INLINE void IncRawDataSize_clip(int stream, avf_u32_t inc);

	INLINE vdb_seg_t *FindSegmentByTime(int stream, avf_u64_t clip_time_ms) {
		if (!LoadClipCache()) return NULL;
		return __GetStream(stream)->FindSegmentByTime(clip_time_ms);
	}

	INLINE vdb_seg_t *FindSegmentByAddr(int stream, avf_u64_t clip_addr) {
		if (!LoadClipCache()) return NULL;
		return __GetStream(stream)->FindSegmentByAddr(clip_addr);
	}

	INLINE void IncClipSize_clip(avf_u64_t inc);
	INLINE void DecClipSize_clip(avf_u64_t dec);

	INLINE void IncMarkedSize_clip(avf_u64_t inc);
	INLINE void DecMarkedSize_clip(avf_u64_t dec);

	void Serialize(CMemBuffer *pmb) const;
};

INLINE vdb_stream_t* vdb_seg_t::GetStream() const
{
	return m_clip->GetStream(m_stream);
}

INLINE int vdb_seg_t::GetVideoType() const
{
	return m_clip->GetVideoType(m_stream);
}

INLINE void vdb_seg_t::UpdateStreamAttr()
{
	m_info.stream_attr = m_clip->GetStreamAttr(m_stream);
}

INLINE string_t *vdb_seg_t::GetIndexFileName() const {
	if (m_index_name == AVF_INVALID_TIME)
		return NULL;
	return m_clip->m_owner->GetIndexFileName(this);
}

INLINE string_t *vdb_seg_t::GetPictureFileName() const {
	if (m_picture_name == AVF_INVALID_TIME)
		return NULL;
	return m_clip->m_owner->GetPictureFileName(this);
}

INLINE string_t *vdb_seg_t::GetVideoFileName() const {
	if (m_video_name == AVF_INVALID_TIME)
		return NULL;
	return m_clip->m_owner->GetVideoFileName(this);
}

INLINE const avf_stream_attr_s& vdb_stream_t::GetStreamAttr() const
{
	return m_clip->GetStreamAttr(m_index);
}

//-----------------------------------------------------------------------
//
//  ref_clip_t
//
//-----------------------------------------------------------------------
class ref_clip_t
{
public:
	struct rb_node rbnode;	// link to ref_clip_set_t::ref_clip_set
	list_head_t list_node;	// link to ref_clip_set_t::ref_clip_list
	list_head_t list_clip;	// link to vdb_clip_t::ref_clip_list

	avf_u32_t clip_attr;
	raw_data_t *m_scene_data;

	vdb_clip_t *clip;		// target_clip_id
	avf_time_t m_dirname;

	avf_u32_t ref_clip_type;	// 0: buffer; 1: marked; others: playlist; same with the rcs
	clip_id_t ref_clip_id;	// unique clip_id

	avf_time_t ref_clip_date;	// real time, not unique (clip->clip_date + start_time_ms)
	avf_u32_t plist_index;		// index in ref_clip_list, for playlist

	avf_u64_t start_time_ms;	// abs in vdb_clip_t
	avf_u64_t end_time_ms;	// exclusive

public:
	ref_clip_t(vdb_clip_t *clip, avf_u32_t ref_clip_type, clip_id_t ref_clip_id, avf_u32_t clip_attr);
	~ref_clip_t();

public:
	INLINE bool IsLive() {
		return (clip_attr & CLIP_ATTR_LIVE) != 0;
	}
	INLINE void SetLive() {
		clip_attr |= CLIP_ATTR_LIVE;
	}
	INLINE void ClearLive() {
		clip_attr &= ~CLIP_ATTR_LIVE;
	}

	INLINE void SetLiveMark() {
		clip_attr |= CLIP_ATTR_LIVE_MARK;
	}

	INLINE bool CanAutoDelete() {
		return (clip_attr & (CLIP_ATTR_LIVE|CLIP_ATTR_NO_AUTO_DELETE)) == 0;
	}
	INLINE bool AutoDelete() {
		return (clip_attr & CLIP_ATTR_NO_AUTO_DELETE) == 0;
	}

public:
	static INLINE ref_clip_t *from_rcs_node(struct rb_node *node) {
		return container_of(node, ref_clip_t, rbnode);
	}
	static INLINE ref_clip_t *from_list_node(list_head_t *node) {
		return container_of(node, ref_clip_t, list_node);
	}
	static INLINE ref_clip_t *from_clip_node(list_head_t *node) {
		return container_of(node, ref_clip_t, list_clip);
	}

	INLINE ref_clip_t *GetNextRefClip() const {
		struct rb_node *next = rb_next(&rbnode);
		return next ? from_rcs_node(next) : NULL;
	}

	INLINE avf_u64_t GetDurationLong() const {
		return end_time_ms - start_time_ms;
	}

	INLINE bool CalcNumSegments(avf_size_t num) {
		return clip->CalcNumSegments(num, start_time_ms, end_time_ms);
	}

	INLINE bool IsEmpty() const {
		return start_time_ms >= end_time_ms;
	}

	INLINE bool CalcMarkedSpace() {
		return ref_clip_type != CLIP_TYPE_BUFFER || !AutoDelete();
	}

	void ShrinkOneSegment();

	INLINE vdb_seg_t *FindSegmentByTime(int stream, avf_u64_t clip_time_ms) const {
		return clip->FindSegmentByTime(stream, clip_time_ms);
	}

	INLINE vdb_seg_t *GetFirstSegment(int stream) const {
		return clip->GetFirstSegment(stream);
	}

	INLINE vdb_seg_t *GetLastSegment(int stream) const {
		return clip->GetLastSegment(stream);
	}

	INLINE avf_size_t GetNumSegments(int stream) const {
		return clip->GetNumSegments(stream);
	}

	INLINE void IncReference(avf_u32_t inc_ms) {
		clip->IncReference(end_time_ms, inc_ms, CalcMarkedSpace());
		end_time_ms += inc_ms;
	}

	INLINE avf_u64_t GetFileSize() {
		return clip->GetFileSize(start_time_ms, end_time_ms);
	}

	void SetSceneData(raw_data_t *scene_data);

	void Serialize(CMemBuffer *pmb) const;
};

INLINE void vdb_clip_t::AddReference(ref_clip_t *ref_clip)
{
	AddReference(ref_clip->start_time_ms, ref_clip->end_time_ms, ref_clip->CalcMarkedSpace());
}

INLINE void vdb_clip_t::RemoveReference(ref_clip_t *ref_clip)
{
	RemoveReference(ref_clip->start_time_ms, ref_clip->end_time_ms, ref_clip->CalcMarkedSpace());
}

//-----------------------------------------------------------------------
//
//  ref_clip_set_t (playlist)
//
//-----------------------------------------------------------------------
class ref_clip_set_t
{
	friend class vdb_clip_t;

public:
	struct rb_root ref_clip_set;		// all ref clips (tree)
	list_head_t ref_clip_list;		// all ref clips (list)
	ref_clip_t *m_last_ref_clip;	// cache for lookup

	struct rb_node rbnode;			// link to playlist_set_t::playlist_set

	avf_size_t mTotalRefClips;		// number of clips
	avf_u64_t m_total_length_ms;
	avf_u32_t ref_clip_type;			// 0: buffer; 1: marked
	clip_id_t m_max_clip_id;

	const bool mb_server_mode;

public:
	enum { DEFAULT_LIST_POS = -1U };

public:
	ref_clip_set_t(bool b_server_mode, avf_u32_t ref_clip_type);
	~ref_clip_set_t();

	INLINE void Clear() {
		ReleaseAll();
		Init();
	}

	static INLINE ref_clip_set_t *from_rbnode(struct rb_node *node) {
		return container_of(node, ref_clip_set_t, rbnode);
	}

	INLINE bool IsEmpty() const {
		return ref_clip_set.rb_node == NULL;
	}

	INLINE ref_clip_t *GetFirstRefClip() const {
		struct rb_node *node = rb_first(&ref_clip_set);
		return node ? ref_clip_t::from_rcs_node(node) : NULL;
	}

	INLINE ref_clip_t *GetFirstRefClipOfList() const {
		list_head_t *node = ref_clip_list.next;
		return node == &ref_clip_list ? NULL : ref_clip_t::from_list_node(node);
	}

public:
	ref_clip_t *FindRefClip(clip_id_t ref_clip_id);
	ref_clip_t *FindRefClipEx(avf_time_t dirname, clip_id_t ref_clip_id);

	clip_id_t GetRefClipId(sys_time_t clip_date, avf_u64_t *start_time_ms);
	bool AddUniqueRefClip(ref_clip_t *ref_clip, avf_uint_t list_pos);

	ref_clip_t *CreateRefClip(vdb_clip_t *clip,
		avf_u64_t start_time_ms, avf_u64_t end_time_ms, int *error,
		bool bCheckAttr, avf_uint_t list_pos, avf_u32_t clip_attr);

	avf_status_t MoveRefClip(ref_clip_t *ref_clip, avf_u32_t new_list_pos);

	ref_clip_t *NewLiveRefClip(vdb_clip_t *clip, bool bServerMode, int clip_attr);

	void RemoveRefClip(ref_clip_t *ref_clip);
	void RemoveEmptyRefClip(ref_clip_t *ref_clip);

	void Serialize(CMemBuffer *pmb) const;

private:
	void Init();
	void ReleaseAll();
	void ReleaseRefClip(ref_clip_t *ref_clip);
	static bool CheckStreamSttr(vdb_stream_t& stream);
	static bool CompareStreamAttr(vdb_stream_t& stream1, vdb_stream_t& stream2);
};

//-----------------------------------------------------------------------
//
//  playlist_set_t
//
//-----------------------------------------------------------------------
class playlist_set_t
{
public:
	struct rb_root playlist_set;		// all playlists (tree)
	avf_u32_t playlist_id_start;		// start from PLAYLIST_ID_START

public:
	INLINE playlist_set_t() {
		Init();
	}

	INLINE ~playlist_set_t() {
		Clear();
	}

public:
	INLINE void Init() {
		playlist_set.rb_node = NULL;
		playlist_id_start = PLAYLIST_ID_START;
	}

	INLINE void Clear() {
		if (playlist_set.rb_node) {
			ReleasePlaylist(playlist_set.rb_node);
		}
		Init();
	}

	ref_clip_set_t *GetPlaylist(avf_u32_t playlist_id);
	ref_clip_set_t *CreatePlaylist(bool b_server_mode, avf_u32_t playlist_id);
	avf_u32_t GeneratePlaylistId();
	avf_status_t AddPlaylist(ref_clip_set_t *rcs);
	avf_status_t RemovePlaylistFiles(const char *vdb_root, ref_clip_set_t *rcs);
	avf_status_t RemovePlaylist(ref_clip_set_t *rcs);

private:
	void ReleasePlaylist(struct rb_node *node);
};

//-----------------------------------------------------------------------
//
//  clip_set_t
//
//-----------------------------------------------------------------------
class clip_set_t
{
public:
	struct rb_root clip_set;		// all loaded clips (tree)
	list_head_t clip_list;		// all loaded clips (list)
	vdb_clip_t *m_last_clip;	// cache for lookup

	bool mbDirty;
	bool mbSpaceChanged;

	avf_size_t mTotalClips;		// total in VDB
	avf_size_t mLoadedClips;		// total loaded
	vdb_size_t m_size;			// all data size

	avf_u64_t m_clip_space;		// all clips' files size
	avf_u64_t m_marked_space;	// protected clips' size
	avf_u64_t m_clipinfo_space;	// used by .clip files

	const bool mb_server_mode;
	seg_cache_list_t m_seg_cache_list;
	clip_cache_list_t m_clip_cache_list;

public:
	clip_set_t(bool b_server_mode, avf_size_t max_seg_cache, bool bKeepClipCache, avf_size_t max_clip_cache):
		mb_server_mode(b_server_mode),
		m_seg_cache_list(b_server_mode, max_seg_cache),
		m_clip_cache_list(m_seg_cache_list, bKeepClipCache, max_clip_cache)
	{
		Init();
	}
	~clip_set_t();

	void Init();
	void Clear();

	INLINE avf_u64_t GetSpace() const {
		return m_size.GetTotal();
	}

	INLINE seg_cache_list_t& GetSegCacheList() {
		return m_seg_cache_list;
	}
	INLINE clip_cache_list_t& GetClipCacheList() {
		return m_clip_cache_list;
	}

	INLINE avf_u64_t GetVideoDataSize() const {
		return m_size.v_size;
	}
	INLINE avf_u64_t GetPictureDataSize() const {
		return m_size.p_size;
	}
	INLINE avf_u64_t GetRawDataSize() const {
		return m_size.r_size;
	}

	INLINE void IncVideoDataSize_cs(avf_u32_t inc) {
		m_size.v_size += inc;
		mbDirty = true;
	}

	INLINE void IncPictureDataSize_cs(avf_u32_t inc) {
		m_size.p_size += inc;
		mbDirty = true;
	}

	INLINE void IncRawDataSize_cs(avf_u32_t inc) {
		m_size.r_size += inc;
		mbDirty = true;
	}

	INLINE void IncMarkedSpace_cs(avf_u64_t inc) {
		m_marked_space += inc;
		mbSpaceChanged = true;
	}

	INLINE void DecMarkedSpace_cs(avf_u64_t dec) {
		m_marked_space -= dec;
		mbSpaceChanged = true;
	}

	INLINE void IncClipSize_cs(avf_u64_t inc) {
		m_clip_space += inc;
		mbSpaceChanged = true;
	}

	INLINE void DecClipSize_cs(avf_u64_t dec) {
		m_clip_space -= dec;
		mbSpaceChanged = true;
	}

	INLINE void BeforeRemoveSegment(vdb_seg_t *seg) {
		mbDirty = true;
		m_size.v_size -= seg->GetVideoDataSize();
		m_size.p_size -= seg->GetPictureDataSize();
		m_size.r_size -= seg->GetRawDataSize();
		m_clip_space -= seg->m_filesize;
	}

	INLINE void TrimSegCache() {
		m_seg_cache_list.Trim();
	}

	INLINE void TrimClipCache() {
		m_clip_cache_list.Trim();
	}

	INLINE void TrimLoadedClip(avf_size_t max) {
		if (mLoadedClips > max) {
			DoTrimLoadedClips(max);
		}
	}

	void DoTrimLoadedClips(avf_size_t max);

	INLINE void AllocSegCache(vdb_seg_t *seg) {
		TrimSegCache();
		m_seg_cache_list.AllocCache(seg);
	}

	INLINE void LockSegCache(vdb_seg_t *seg) {
		m_seg_cache_list.LockSegCache(seg);
	}

	INLINE void UnlockSegCache(vdb_seg_t *seg) {
		if (seg) {
			m_seg_cache_list.UnlockSegCache(seg);
		}
	}

	INLINE void AllocClipCache(vdb_clip_t *clip) {
		TrimClipCache();
		m_clip_cache_list.AllocCache(clip);
	}

	INLINE void ReleaseClipCache(vdb_clip_t *clip) {
		if (clip->m_cache) {
			m_clip_cache_list.ReleaseCache(clip->m_cache);
			clip->m_cache = NULL;
		}
	}

	INLINE void BringToFront(vdb_clip_t *clip) {
		list_head_t *node = &clip->list_clip;
		__list_del(node);
		list_add(node, clip_list);
	}

	void AddClip(vdb_clip_t *clip, bool bUpdate);
	void RemoveClip(vdb_clip_t *clip, IClipOwner *owner);
	void ReleaseClip(vdb_clip_t *clip);

	vdb_clip_t *FindClip(clip_id_t clip_id);
	vdb_clip_t *FindClipEx(avf_time_t dirname, clip_id_t clip_id);

	void Serialize(CMemBuffer *pmb) const;

private:
	static void ReleaseClipRecursive(struct rb_node *rbnode);
	static INLINE void ReleaseAllClips(struct rb_node *rbnode) {
		if (rbnode) {
			ReleaseClipRecursive(rbnode);
		}
	}

	void EraseClip(vdb_clip_t *clip);
};

//-----------------------------------------------------------------------

INLINE void vdb_clip_t::IncVideoDataSize_clip(int stream, avf_u32_t inc) {
	if (stream == 0) m_size_main.v_size += inc;
	else m_size_sub.v_size += inc;
	m_clip_set->IncVideoDataSize_cs(inc);
}

INLINE void vdb_clip_t::IncPictureDataSize_clip(int stream, avf_u32_t inc) {
	if (stream == 0) m_size_main.p_size += inc;
	else m_size_sub.p_size += inc;
	m_clip_set->IncPictureDataSize_cs(inc);
}

INLINE void vdb_clip_t::IncRawDataSize_clip(int stream, avf_u32_t inc) {
	if (stream == 0) m_size_main.r_size += inc;
	else m_size_sub.r_size += inc;
	m_clip_set->IncRawDataSize_cs(inc);
}

INLINE void vdb_clip_t::IncClipSize_clip(avf_u64_t inc)
{
	m_clip_size += inc;
	m_clip_set->IncClipSize_cs(inc);
/*
	avf_u64_t seg_size = 0;

	vdb_seg_t *seg = m_cache->main_stream.GetFirstSegment();
	for (; seg; seg = seg->GetNextSegment())
		seg_size += seg->m_filesize;

	seg = m_cache->sub_stream.GetFirstSegment();
	for (; seg; seg = seg->GetNextSegment())
		seg_size += seg->m_filesize;

	AVF_LOGD("seg filesize: " LLD ", clip size: " LLD, seg_size, m_clip_size);
*/
}

INLINE void vdb_clip_t::DecClipSize_clip(avf_u64_t dec)
{
	m_clip_size -= dec;
	m_clip_set->DecClipSize_cs(dec);
}

INLINE void vdb_clip_t::IncMarkedSize_clip(avf_u64_t inc)
{
	m_marked_size += inc;
	m_clip_set->IncMarkedSpace_cs(inc);
}

INLINE void vdb_clip_t::DecMarkedSize_clip(avf_u64_t dec)
{
	m_marked_size -= dec;
	m_clip_set->DecMarkedSpace_cs(dec);
}

INLINE void vdb_clip_t::BeforeRemoveSegment(vdb_seg_t *seg) {
	if (seg->m_stream == 0) {
		m_num_segs_0--;
		m_size_main.v_size -= seg->GetVideoDataSize();
		m_size_main.p_size -= seg->GetPictureDataSize();
		m_size_main.r_size -= seg->GetRawDataSize();
	} else {
		m_num_segs_1--;
		m_size_sub.v_size -= seg->GetVideoDataSize();
		m_size_sub.p_size -= seg->GetPictureDataSize();
		m_size_sub.r_size -= seg->GetRawDataSize();
	}
	m_clip_set->BeforeRemoveSegment(seg);
}

//-----------------------------------------------------------------------

INLINE void vdb_seg_t::IncFileSize(avf_u32_t inc)
{
	m_filesize += inc;
	m_clip->IncClipSize_clip(inc);
	if (m_mark_count) {
		m_clip->IncMarkedSize_clip(inc);
	}
}

INLINE void vdb_seg_t::AdjustFileSize(avf_u64_t old_filesize)
{
//	AVF_LOGI("AdjustFileSize: " LLD " -> " LLD, old_filesize, m_filesize);
	if (old_filesize < m_filesize) {
		avf_u64_t inc = m_filesize - old_filesize;
		m_clip->IncClipSize_clip(inc);
		if (m_mark_count) {
			m_clip->IncMarkedSize_clip(inc);
		}
	} else {
		avf_u64_t dec = old_filesize - m_filesize;
		m_clip->DecClipSize_clip(dec);
		if (m_mark_count) {
			m_clip->DecMarkedSize_clip(dec);
		}
	}
}

//-----------------------------------------------------------------------
//
//  copy_clip_set_s
//
//-----------------------------------------------------------------------

class copy_seg_s;

class copy_state_s
{
public:
	int stream;
	vdb_seg_t *src_seg;	// LockSegCache
	copy_seg_s *cseg;

	int fd_video;
	int fd_picture;

	INLINE void Init(int s) {
		stream = s;
		src_seg = NULL;
		cseg = NULL;

		fd_video = -1;
		fd_picture = -1;
	}
};

class copy_ref_s
{
public:
	int clip_type;			// ref clip type
	sys_time_t clip_date;	// the buffered clip date when recording it
	clip_id_t clip_id;		// ref clip id
	avf_u32_t clip_attr;
	avf_u64_t start_time_ms;
	avf_u64_t end_time_ms;
	raw_data_t *scene_data;
};

// a section of a seg
class copy_range_s
{
public:
	avf_u64_t start_time_ms;
	avf_u64_t end_time_ms;

public:
	void GetSize(vdb_seg_t *seg, int index, avf_u32_t *size);
};

class copy_seg_s: public TDynamicArray<copy_range_s>
{
public:
	avf_uint_t seg_id;
	avf_u32_t index_size;

public:
	INLINE void Init() {
		_Init();
	}
	INLINE void Release() {
		_Release();
	}

	void AddRange(avf_u64_t ref_start_time_ms, avf_u64_t ref_end_time_ms);
	void GetSize(vdb_stream_t *stream, avf_u32_t *p_size, avf_u32_t *v_size, avf_u32_t *i_size);
};

class copy_stream_s: public TDynamicArray<copy_seg_s>
{
public:
	INLINE void Init() {
		_Init();
	}

	void Release();
	void AddCopyRef(vdb_stream_t& stream, const copy_ref_s *ref);
	void AddSegment(const copy_ref_s *ref, vdb_seg_t *seg);
	int FindSegmentGE(avf_uint_t seg_id);

	void GetSize(vdb_stream_t *stream, avf_u64_t *p_size, avf_u64_t *v_size, avf_u64_t *i_size);
};

// all refs to the vdb_clip_t
class copy_clip_s: public TDynamicArray<copy_ref_s>
{
public:
	vdb_clip_t *clip;

	copy_stream_s stream_0;
	copy_stream_s stream_1;

	avf_u32_t clipinfo_filesize;

	void Init(vdb_clip_t *clip);
	void Clear();

	int AddCopyRef(const copy_ref_s *ref);

	void GetSize(string_t *clipinfo_dir, avf_u64_t *p_size, avf_u64_t *v_size, avf_u64_t *i_size, avf_u64_t *c_size);

	bool ClipIdExists(int clip_type, clip_id_t clip_id);

	INLINE copy_stream_s *GetStream(int s) {
		return s == 0 ? &stream_0 : &stream_1;
	}
};

typedef int (*copy_callback_t)(void *context, int event, avf_intptr_t param);

enum {
	COPY_EVENT_NONE,
	COPY_EVENT_VIDEO_COPIED,		// avf_uint_t size
	COPY_EVENT_PICTURE_COPIED,	// avf_uint_t size
	COPY_EVENT_INDEX_COPIED,		// avf_uint_t size
	COPY_EVENT_CLIP_TRANSFERED,	// copy_clip_s
};

// all copy_ref_s
class copy_clip_set_s: public TDynamicArray<copy_clip_s>
{
public:
	/////
	clip_set_t *clip_set;
	IClipOwner *clip_owner;
	/////

	copy_callback_t callback;
	void *context;
	volatile int b_cancel;

	copy_clip_set_s(clip_set_t *clip_set, IClipOwner *clip_owner);
	~copy_clip_set_s();

	void Clear();

	int AddCopyRef(vdb_clip_t *clip, const copy_ref_s *ref);
	void GetSize(string_t *clipinfo_dir, avf_u64_t *picture_size, avf_u64_t *video_size, avf_u64_t *index_size);

	bool ClipIdExists(int clip_type, clip_id_t clip_id);
	void GenerateRefClipId(ref_clip_set_t *rcs, copy_ref_s *ref);

private:
	copy_clip_s *FindCopyClip(vdb_clip_t *clip);
	copy_clip_s *NewCopyClip(vdb_clip_t *clip);
};

class copy_seg_info_s
{
public:
	struct index_range_s {
		avf_u32_t size;	// ts/jpg/gps/acc/obd
		int from;
		int to;	// exclusive
	};

	struct out_s {
		avf_time_t index_name;
		avf_time_t video_name;
		avf_time_t picture_name;
		vdb_seg_t::info_t seg_info;
	};

public:
	out_s out;
	index_range_s range[kAllArrays];

	avf_u32_t seg_id;

	avf_int_t seg_off_ms;
	avf_int_t seg_length_ms;
	avf_u32_t seg_v_addr_off;
	avf_u32_t raw_data_size;

	int fd_video;
	int fd_picture;

public:
	INLINE void Init() {
		out.index_name = AVF_INVALID_TIME;
		out.video_name = AVF_INVALID_TIME;
		out.picture_name = AVF_INVALID_TIME;
		seg_id = 0;
		fd_video = -1;
		fd_picture = -1;
	}
};

class copy_stream_info_s: public TDynamicArray<copy_seg_info_s::out_s>
{
public:
	vdb_size_t size;

	INLINE void Init() {
		_Init();
		size.Init();
	}

	INLINE void Release() {
		_Release();
	}

	copy_seg_info_s::out_s *AddSegInfo(copy_seg_info_s *segi);
};

class copy_clip_info_s
{
public:
	copy_stream_info_s stream_0;
	copy_stream_info_s stream_1;
	avf_time_t clipinfo_name;

	INLINE void Init() {
		stream_0.Init();
		stream_1.Init();
		clipinfo_name = AVF_INVALID_TIME;
	}

	INLINE copy_seg_info_s::out_s *AddSegInfo(copy_seg_info_s *segi, int stream) {
		return stream == 0 ? stream_0.AddSegInfo(segi) : stream_1.AddSegInfo(segi);
	}

	INLINE void Release() {
		stream_0.Release();
		stream_1.Release();
	}
};

#endif

