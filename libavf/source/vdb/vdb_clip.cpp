
#define LOG_TAG "vdb_clip"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "avio.h"
#include "file_io.h"
#include "sys_io.h"
#include "tsmap.h"
#include "mem_stream.h"

#include "rbtree.h"
#include "vdb_cmd.h"
#include "vdb_clip.h"
#include "vdb_util.h"
#include "vdb_format.h"
#include "vdb_util.h"

//-----------------------------------------------------------------------
//
//  CRawFifo
//
//-----------------------------------------------------------------------
CRawFifo *CRawFifo::Create()
{
	CRawFifo *result = avf_new CRawFifo();
	CHECK_STATUS(result);
	return result;
}

CRawFifo::CRawFifo():
	mWritePos(0),
	mReadPos(0),
	mCount(0)
{
}

CRawFifo::~CRawFifo()
{
	Clear();
}

CRawFifo::item_s *CRawFifo::Pop()
{
	item_s *result;

	if (mCount == 0) {
		result = NULL;
	} else {
		result = mItems + mReadPos;
		mReadPos = (mReadPos + 1) % MAX_ITEMS;
		mCount--;
	}

	return result;
}

CRawFifo::item_s *CRawFifo::Push(raw_data_t *rdata)
{
	item_s *item = mItems + mWritePos;
	mWritePos = (mWritePos + 1) % MAX_ITEMS;

	rdata->AddRef();

	if (mCount < MAX_ITEMS) {
		mCount++;
	} else {
		item->Release();
		mReadPos = (mReadPos + 1) % MAX_ITEMS;
	}

	item->rdata = rdata;

	return item;
}

void CRawFifo::Clear()
{
	CRawFifo::item_s *item;
	while ((item = Pop()) != NULL) {
		item->Release();
	}
}

void CRawFifo::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_32(mWritePos);
	pmb->write_le_32(mReadPos);
	pmb->write_le_32(mCount);
	int pos = mReadPos;
	for (int i = 0; i < mCount; i++) {
		const item_s *item = mItems + pos;
		pos = (pos + 1) % MAX_ITEMS;
		pmb->write_le_64(item->rdata->ts_ms);
	}
}

//-----------------------------------------------------------------------
//
//  CIndexArray
//
//-----------------------------------------------------------------------

const array_attr_t g_array_attr[kAllArrays] = {
	{CIndexArray::TYPE_INDEX, 0},	// kPictureArray
	{CIndexArray::TYPE_INDEX, 0},	// kVideoArray
	{CIndexArray::TYPE_BLOCK_DATA, 16 * KB},	// kGps0Array
	{CIndexArray::TYPE_BLOCK_DATA, 16 * KB},	// kAcc0Array
	{CIndexArray::TYPE_BLOCK_DATA, 16 * KB},	// kObd0Array
	{CIndexArray::TYPE_BLOCK_DATA, 16 * KB},	// kVmemArray
};

CIndexArray *CIndexArray::Create(int type, avf_size_t block_size)
{
	CIndexArray *result = avf_new CIndexArray(type, block_size);
	CHECK_STATUS(result);
	return result;
}

CIndexArray::CIndexArray(int type, int block_size):
	mType(type),
	pmb(NULL),
	m_data_size(0),
	m_length_ms(0)
{
	mItems._Init();
	if (mType == TYPE_BLOCK_DATA) {
		// pmb: one block of memory
		CREATE_OBJ(pmb = CMemBuffer::Create(block_size, true));
	}
}

CIndexArray::~CIndexArray()
{
	mItems._Release();
	avf_safe_release(pmb);
}

avf_status_t CIndexArray::DoAddEntry(avf_s32_t time_ms, avf_u32_t fpos, avf_s32_t end_time_ms, avf_u32_t end_fpos)
{
	// always reserve the last entry
	item_t *item = mItems._Append(NULL, 0, 1);
	if (item == NULL)
		return E_NOMEM;

	item->time_ms = time_ms;
	item->fpos = fpos;

	// the tail item
	item++;
	item->time_ms = end_time_ms;
	item->fpos = end_fpos;

	m_data_size = end_fpos;
	m_length_ms = end_time_ms;

	return E_OK;
}

// update the last reserved item
void CIndexArray::UpdateTailItem(avf_u32_t data_size, avf_u32_t length_ms)
{
	if (mItems._Avail() == 0) {
		AVF_LOGW("no elems");
		return;
	}

	m_data_size = data_size;
	m_length_ms = length_ms;

	item_t *item = mItems.elems + mItems.count;
	item->fpos = m_data_size;

	if (mItems.count > 0) {
		item_t *last_item = item - 1;
		if ((int)length_ms < last_item->time_ms)
			length_ms = last_item->time_ms;
	}

	item->time_ms = length_ms;
}

// add entry and data
avf_status_t CIndexArray::AddRawDataEntry(avf_s32_t time_ms, raw_data_t *raw)
{
	avf_size_t size = raw->GetSize();

	if (AddEntry(time_ms, pmb->GetTotalSize()) == E_OK) {
		avf_u8_t *ptr = pmb->Alloc(size);
		::memcpy(ptr, raw->GetData(), size);
	}

	return E_OK;
}

avf_status_t CIndexArray::AddRawDataEntryEx(avf_s32_t time_ms, avf_s32_t end_time_ms, raw_data_t *raw)
{
	avf_size_t size = raw->GetSize();
	avf_u32_t fpos = pmb->GetTotalSize();

	if (DoAddEntry(time_ms, fpos, end_time_ms, fpos + size) == E_OK) {
		avf_u8_t *ptr = pmb->Alloc(size);
		::memcpy(ptr, raw->GetData(), size);
	}

	return E_OK;
}

const CIndexArray::item_t *CIndexArray::FindIndexItem(avf_s32_t time_ms) const
{
	int a = 0, b = mItems.count - 1;
	const CIndexArray::item_t *item;

	if (b < 0)
		return NULL;

	item = GetItemAt(a);
	if (time_ms <= item->time_ms)
		return item;

	item = GetItemAt(b);
	if (time_ms >= item->time_ms)
		return item;

	while (true) {
		int m = (a + b) / 2;
		item = GetItemAt(m);
		if (time_ms >= item->time_ms) {
			if (a == m)
				return item;
			a = m;
		} else {
			b = m;
		}
	}
}

const CIndexArray::item_t *CIndexArray::FindIndexItemByAddr(avf_u32_t fpos) const
{
	int a = 0, b = mItems.count - 1;
	const CIndexArray::item_t *item;

	if (b < 0)
		return NULL;

	item = GetItemAt(a);
	if (fpos <= item->fpos)
		return item;

	item = GetItemAt(b);
	if (fpos >= item->fpos)
		return item;

	while (true) {
		int m = (a + b) / 2;
		item = GetItemAt(m);
		if (fpos >= item->fpos) {
			if (a == m)
				return item;
			a = m;
		} else {
			b = m;
		}
	}
}

void CIndexArray::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_32(mType);
	pmb->write_le_32(0);
	pmb->write_le_32(0);
	pmb->write_le_32(mItems._cap);
	pmb->write_le_32(mItems.count);
	pmb->write_le_32(m_data_size);
	pmb->write_le_32(m_length_ms);
	if (mItems.count) {
		pmb->write_mem((const avf_u8_t*)mItems.elems, (mItems.count + 1) * sizeof(item_t));
	}
}

//-----------------------------------------------------------------------
//
//  vdb_stream_t
//
//-----------------------------------------------------------------------
vdb_stream_t::vdb_stream_t(seg_cache_list_t& seg_cache_list, vdb_clip_t *clip, int index):
	m_clip(clip),
	m_index(index),
	mp_seg_cache_list(&seg_cache_list)
{
	seg_tree.rb_node = NULL;
	m_num_segs = 0;
}

vdb_stream_t::~vdb_stream_t()
{
	if (seg_tree.rb_node) {
		ReleaseSegments(seg_tree.rb_node);
	}
}

// seg cache list should be released before this
void vdb_stream_t::ReleaseSegments(struct rb_node *node)
{
	if (node->rb_left) ReleaseSegments(node->rb_left);
	if (node->rb_right) ReleaseSegments(node->rb_right);
	vdb_seg_t *seg = vdb_seg_t::from_node(node);
	if (seg->m_cache) {
		mp_seg_cache_list->ReleaseCache(seg->m_cache);
		seg->m_cache = NULL;
	}
	avf_delete seg;
}

vdb_seg_t *vdb_stream_t::AddSegment(
	avf_time_t subdir_name, avf_time_t index_name, 
	avf_time_t video_name, avf_time_t picture_name,
	const vdb_seg_t::info_t *info)
{
	struct rb_node **p = &seg_tree.rb_node;
	struct rb_node *parent = NULL;
	avf_u32_t id = info->id;

	// if the seg (id) already exists
	while (*p) {
		parent = *p;
		vdb_seg_t *tmp = vdb_seg_t::from_node(parent);

		if (id < tmp->m_info.id) {
			p = &parent->rb_left;
		} else if (id > tmp->m_info.id) {
			p = &parent->rb_right;
		} else {
			AVF_LOGE("seg already exists %d", id);
			return tmp;
		}
	}

	vdb_seg_t *seg = avf_new vdb_seg_t(m_clip, m_index, 
		subdir_name, index_name, video_name, picture_name, info);

	// add to the tree
	rb_link_node(&seg->rbnode, parent, p);
	rb_insert_color(&seg->rbnode, &seg_tree);

	// update
	m_num_segs++;
	m_clip->SetStreamAttr(m_index, &info->stream_attr);

	return seg;
}

void vdb_stream_t::RemoveAllFiles()
{
	vdb_seg_t *seg = GetFirstSegment();
	while (seg) {
		RemoveSegmentFiles(seg);
		seg = seg->GetNextSegment();
	}
}

void vdb_stream_t::RemoveSegmentFiles(vdb_seg_t *seg)
{
	IClipOwner *owner = m_clip->m_owner;

	__LOCK_IO__(LOCK_FOR_REMOVE_FILE, "remove segment");

	// remove index file
	if (seg->m_index_name != AVF_INVALID_TIME) {
		owner->RemoveIndexFile(seg);
	}

	// remove picture file: only for main stream
	if (m_index == 0 && seg->m_picture_name != AVF_INVALID_TIME) {
		owner->RemovePictureFile(seg);
	}

	// remove video file
	if (seg->m_video_name != AVF_INVALID_TIME) {
		owner->RemoveVideoFile(seg);
	}
}

void vdb_stream_t::RemoveSegment(vdb_seg_t *seg)
{
	IClipOwner *owner = m_clip->m_owner;

	// release cache first
	if (seg->m_cache) {
		owner->GetSegCacheList().ReleaseCache(seg->m_cache);
		seg->m_cache = NULL;
	}

	RemoveSegmentFiles(seg);

	// update stream
	m_num_segs--;

	// update cdir
	clip_dir_t *cdir = seg->m_clip->m_cdir;
	if (cdir) {
		cdir->SegmentRemoved(m_index);
	}

	// update clip
	m_clip->BeforeRemoveSegment(seg);

	// remove the seg
	rb_erase(&seg->rbnode, &seg_tree);
	avf_delete seg;
}

vdb_seg_t *vdb_stream_t::FindSegmentById(avf_uint_t id)
{
	struct rb_node *n = seg_tree.rb_node;
	vdb_seg_t *max_le = NULL;

	while (n) {
		vdb_seg_t *seg = vdb_seg_t::from_node(n);
		if (id < seg->m_info.id) {
			n = n->rb_left;
		} else {
			max_le = seg;
			n = n->rb_right;
		}
	}

	if (max_le && id == max_le->m_info.id)
		return max_le;

	AVF_LOGE("FindSegmentById failed, id=%d, stream=%d", id, m_index);
	return NULL;
}

vdb_seg_t *vdb_stream_t::FindSegmentByTime(avf_u64_t clip_time_ms)
{
	struct rb_node *n = seg_tree.rb_node;
	vdb_seg_t *max_le = NULL;

	while (n) {
		vdb_seg_t *seg = vdb_seg_t::from_node(n);
		if (clip_time_ms < seg->GetStartTime_ms()) {
			n = n->rb_left;
		} else {
			max_le = seg;
			n = n->rb_right;
		}
	}

	// clip_time_ms >= max_le->GetStartTime_ms()
	if (max_le && clip_time_ms < max_le->GetEndTime_ms())
		return max_le;

	AVF_LOGE("FindSegmentByTime failed, clip_time_ms=" LLD ", stream=%d",
		clip_time_ms, m_index);

#ifdef AVF_DEBUG
	vdb_seg_t *first_seg = GetFirstSegment();
	vdb_seg_t *last_seg = GetLastSegment();
	if (first_seg && last_seg) {
		AVF_LOGD("first: " LLD ", last: " LLD, first_seg->GetStartTime_ms(),
			last_seg->GetEndTime_ms());
	}
#endif

	return NULL;
}

// returns the first segment: seg->GetEndTime_ms() > start_time_ms
// returns NULL if not exist
vdb_seg_t *vdb_stream_t::FindSegmentSinceTime(avf_u64_t start_time_ms)
{
	struct rb_node *n = seg_tree.rb_node;
	vdb_seg_t *max_le = NULL;

	while (n) {
		vdb_seg_t *seg = vdb_seg_t::from_node(n);
		if (start_time_ms < seg->GetStartTime_ms()) {
			n = n->rb_left;
		} else {
			max_le = seg;
			n = n->rb_right;
		}
	}

	if (max_le == NULL) {
		// start_time_ms < all
		return GetFirstSegment();
	}

	// start_time_ms >= max_le->GetStartTime_ms()

	if (start_time_ms < max_le->GetEndTime_ms())
		return max_le;

	return max_le->GetNextSegment();
}

// return the segment that contains the address
vdb_seg_t *vdb_stream_t::FindSegmentByAddr(avf_u64_t offset)
{
	struct rb_node *n = seg_tree.rb_node;
	vdb_seg_t *max_le = NULL;

	while (n) {
		vdb_seg_t *seg = vdb_seg_t::from_node(n);
		if (offset < seg->GetStartAddr()) {
			n = n->rb_left;
		} else {
			max_le = seg;
			n = n->rb_right;
		}
	}

	if (max_le && offset < max_le->GetEndAddr())
		return max_le;

	return NULL;
}

// returns true: number of segments in [start_time_ms, end_time_ms) >= num
bool vdb_stream_t::CalcNumSegments(avf_size_t num, avf_u64_t start_time_ms, avf_u64_t end_time_ms)
{
	if (start_time_ms >= end_time_ms)
		return false;

	vdb_seg_t *seg = FindSegmentSinceTime(start_time_ms);
	avf_size_t num_segs = 0;
	while (seg && end_time_ms > seg->GetStartTime_ms()) {
		if (++num_segs >= num)
			return true;
		seg = seg->GetNextSegment();
	}

	return false;
}

// lock range: [start_time_ms, end_time_ms)
avf_size_t vdb_stream_t::AddReference(avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_int_t mark_count)
{
	if (start_time_ms >= end_time_ms) {
		AVF_LOGW("AddReference: range is empty, " LLD ">=" LLD, start_time_ms, end_time_ms);
		return 0;
	}

	vdb_seg_t *seg = FindSegmentSinceTime(start_time_ms);
	avf_size_t num_segs = 0;
	while (seg && end_time_ms > seg->GetStartTime_ms()) {
		seg->AddRef();
		if (seg->m_mark_count == 0 && mark_count) {
			m_clip->IncMarkedSize_clip(seg->m_filesize);
		}
		seg->m_mark_count += mark_count;
		num_segs++;
		seg = seg->GetNextSegment();
	}

	return num_segs;
}

// [,end_time_ms) => [,end_time_ms + inc_ms)
avf_size_t vdb_stream_t::IncReference(avf_u64_t end_time_ms, avf_u32_t inc_ms, avf_int_t mark_count)
{
	if (inc_ms == 0) {
		AVF_LOGW("IncReference: inc_ms is 0");
		return 0;
	}

	vdb_seg_t *seg = FindSegmentSinceTime(end_time_ms);
	if (seg == NULL) {
		AVF_LOGW("no segment since time " LLD, end_time_ms);
		return 0;
	}

	if (end_time_ms > seg->GetStartTime_ms()) {
		// this segment is already referenced, go next
		seg = seg->GetNextSegment();
	}

	avf_u64_t new_end_time_ms = end_time_ms + inc_ms;
	avf_size_t num_segs = 0;
	while (seg && new_end_time_ms > seg->GetStartTime_ms()) {
		seg->AddRef();
		if (seg->m_mark_count == 0 && mark_count) {
			m_clip->IncMarkedSize_clip(seg->m_filesize);
		}
		seg->m_mark_count += mark_count;
		num_segs++;
		seg = seg->GetNextSegment();
	}

	return num_segs;
}

// unlock range: [start_time_ms, end_time_ms)
avf_size_t vdb_stream_t::RemoveReference(avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_int_t mark_count)
{
	if (start_time_ms >= end_time_ms) {
		AVF_LOGW("RemoveReference: range is empty, " LLD ">=" LLD, start_time_ms, end_time_ms);
		return 0;
	}

	vdb_seg_t *seg = FindSegmentSinceTime(start_time_ms);
	avf_size_t num_segs = 0;
	while (seg && end_time_ms > seg->GetStartTime_ms()) {
		vdb_seg_t *next = seg->GetNextSegment();

		if (seg->m_mark_count == 1 && mark_count) {
			m_clip->DecMarkedSize_clip(seg->m_filesize);
		}

		if (seg->ReleaseRef()) {
			RemoveSegment(seg);
			num_segs++;
		} else {
			seg->m_mark_count -= mark_count;
		}

		seg = next;
	}

	return num_segs;
}

avf_u64_t vdb_stream_t::GetFileSize(avf_u64_t start_time_ms, avf_u64_t end_time_ms)
{
	if (start_time_ms >= end_time_ms) {
		return 0;
	}

	avf_u64_t size = 0;

	vdb_seg_t *seg = FindSegmentSinceTime(start_time_ms);
	while (seg && end_time_ms > seg->GetStartTime_ms()) {
		vdb_seg_t *next = seg->GetNextSegment();

		size += seg->m_filesize;

		seg = next;
	}

	return size;
}

void vdb_stream_t::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_32(m_index);
	pmb->write_le_32(m_num_segs);
	struct rb_node *node = rb_first(&seg_tree);
	for (; node; node = rb_next(node)) {
		vdb_seg_t *seg = vdb_seg_t::from_node(node);
		pmb->write_le_8(1);
		seg->Serialize(pmb);
	}
	pmb->write_le_8(0);
}

//-----------------------------------------------------------------------
//
//  vdb_clip_t
//
//-----------------------------------------------------------------------

vdb_clip_t::vdb_clip_t(IClipOwner *owner, clip_set_t *clip_set, clip_dir_t *cdir, clip_id_t clip_id)
{
	// this->rbnode
	// this->list_clip

	this->m_clip_set = clip_set;
	list_init(this->ref_clip_list);
	this->num_ref_clips = 0;

	this->m_cdir = cdir;
	this->clip_id = clip_id;
	this->buffered_clip_id = INVALID_CLIP_ID;

	int curr_gmtoff;
	this->clip_date = avf_get_sys_time(&curr_gmtoff);
	this->gmtoff = curr_gmtoff;

	this->lock_count = 0;
	this->m_num_segs_0 = 0;
	this->m_num_segs_1 = 0;

	this->first_pic_name = AVF_INVALID_TIME;
	this->first_pic_size = 0;

	m_size_main.Init();
	m_size_sub.Init();

	::memset(&main_stream_attr, 0, sizeof(main_stream_attr));
	::memset(&sub_stream_attr, 0, sizeof(sub_stream_attr));

	m_video_type_0 = VIDEO_TYPE_TS;
	m_video_type_1 = VIDEO_TYPE_TS;

	::memset(&raw_info, 0, sizeof(raw_info));
	m_extra_raw_fcc._Init();

	this->m_owner = owner;
	this->m_cache = NULL;

	this->m_clipinfo_filesize = 0;
	this->m_clip_size = 0;
	this->m_marked_size = 0;

	this->m_clip_attr = 0;
	list_init(this->m_desc_list);
}

vdb_clip_t::~vdb_clip_t()
{
#ifdef AVF_DEBUG
	AVF_ASSERT(list_is_empty(ref_clip_list));
	AVF_ASSERT(num_ref_clips == 0);
	AVF_ASSERT(m_cache == NULL);
#endif

	list_head_t *node;
	list_head_t *next;
	list_for_each_del(node, next, m_desc_list) {
		clip_desc_item_s *item = clip_desc_item_s::from_node(node);
		avf_free(item);
	}

	m_extra_raw_fcc._Release();
}

void vdb_clip_t::GenerateUUID()
{
	avf_u8_t uuid[UUID_LEN+1];
	avf_gen_uuid(uuid);
	SaveUUID(uuid, sizeof(uuid));
}

clip_desc_item_s *vdb_clip_t::FindClipDesc(avf_u32_t type)
{
	list_head_t *node;
	list_for_each(node, m_desc_list) {
		clip_desc_item_s *item = clip_desc_item_s::from_node(node);
		if (item->type == type)
			return item;
	}
	return NULL;
}

clip_desc_item_s *vdb_clip_t::SaveDescData(const avf_u8_t *data, avf_size_t size, avf_u32_t type)
{
	clip_desc_item_s *item = avf_malloc_type_extra(clip_desc_item_s, size);
	if (item == NULL)
		return NULL;

	item->type = type;
	item->size = size;
	if (data) {
		::memcpy(item->GetMem(), data, size);
	}
	list_add(&item->list_desc, m_desc_list);

	return item;
}

void vdb_clip_t::SaveUUID(const avf_u8_t *data, avf_size_t size)
{
	if (size != UUID_LEN) {
		if (size < UUID_LEN) {
			AVF_LOGE("bad UUID_LEN: %d", size);
			return;
		}
		size = UUID_LEN;
	}
	SaveDescData(data, size, CLIP_DESC_UUID);
}

void vdb_clip_t::GetUUID(avf_u8_t uuid[UUID_LEN])
{
	clip_desc_item_s *item = FindUUID();
	if (item == NULL) {
		::memset(uuid, 0, UUID_LEN);
		return;
	}

	avf_size_t size = item->size;
	if (size >= UUID_LEN) {
		size = UUID_LEN;
	}

	::memcpy(uuid, item->GetMem(), size);
	if (size < UUID_LEN) {
		::memset(uuid + size, 0, UUID_LEN - size);
	}
}

avf_size_t vdb_clip_t::GetClipDesc(CMemBuffer *pmb)
{
	avf_size_t total = 0;
	list_head_t *node;
	list_for_each(node, m_desc_list) {
		clip_desc_item_s *item = clip_desc_item_s::from_node(node);
		pmb->write_le_32(item->type);
		pmb->write_le_32(item->size);
		pmb->write_mem(item->GetMem(), item->size);
		int n = ROUND_UP(item->size, 4) - item->size;
		if (n > 0) {
			pmb->write_zero(n);
		}
		total += 8 + item->size + n;
	}
	pmb->write_le_32(0);
	return total + 4;
}

// note: segments not locked yet
ref_clip_t *vdb_clip_t::NewRefClip(avf_u32_t ref_clip_type, clip_id_t ref_clip_id,
	avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_u32_t clip_attr)
{
	ref_clip_t *ref_clip = avf_new ref_clip_t(this, ref_clip_type, ref_clip_id, clip_attr);

	if (ref_clip) {
		ref_clip->ref_clip_date = clip_date;// + (avf_time_t)(start_time_ms/1000);
		ref_clip->plist_index = 0;

		ref_clip->start_time_ms = start_time_ms;
		ref_clip->end_time_ms = end_time_ms;
	}

	return ref_clip;
}

// lock segments in range and the clip
void vdb_clip_t::LockClip(avf_u64_t start_time_ms, avf_u64_t end_time_ms)
{
	AVF_LOGD(C_YELLOW "LockClip(" LLD "," LLD ")" C_NONE, start_time_ms, end_time_ms);
	AddReference(start_time_ms, end_time_ms, 0);
	this->lock_count++;
}

// unlock segments in range and the clip
void vdb_clip_t::_UnlockClip(avf_u64_t start_time_ms, avf_u64_t end_time_ms)
{
	AVF_LOGD(C_YELLOW "UnlockClip(" LLD "," LLD ")" C_NONE, start_time_ms, end_time_ms);
	RemoveReference(start_time_ms, end_time_ms, 0);
	if (this->lock_count == 0) {
		AVF_LOGW("UnlockClip: clip lock_count is 0");
	} else {
		this->lock_count--;
	}
}

void vdb_clip_t::AddReference(avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_int_t mark_count)
{
	if (m_cache == NULL && !LoadClipCache()) {
		AVF_LOGW("AddReference: cache not loaded");
		return;
	}
	m_cache->main_stream.AddReference(start_time_ms, end_time_ms, mark_count);
	if (!m_cache->sub_stream.IsEmpty()) {
		m_cache->sub_stream.AddReference(start_time_ms, end_time_ms, mark_count);
	}
}

void vdb_clip_t::IncReference(avf_u64_t end_time_ms, avf_u32_t inc_ms, avf_int_t mark_count)
{
	if (m_cache == NULL && !LoadClipCache()) {
		AVF_LOGW("IncReference: cache not loaded");
		return;
	}
	m_cache->main_stream.IncReference(end_time_ms, inc_ms, mark_count);
	if (!m_cache->sub_stream.IsEmpty()) {
		m_cache->sub_stream.IncReference(end_time_ms, inc_ms, mark_count);
	}
}

avf_u64_t vdb_clip_t::GetFileSize(avf_u64_t start_time_ms, avf_u64_t end_time_ms)
{
	if (m_cache == NULL && !LoadClipCache()) {
		AVF_LOGW("GetFileSize: cache not loaded");
		return 0;
	}
	avf_u64_t size = m_cache->main_stream.GetFileSize(start_time_ms, end_time_ms);
	if (!m_cache->sub_stream.IsEmpty()) {
		size += m_cache->sub_stream.GetFileSize(start_time_ms, end_time_ms);
	}
	return size;
}

void vdb_clip_t::RemoveReference(avf_u64_t start_time_ms, avf_u64_t end_time_ms, avf_int_t mark_count)
{
	if (m_cache == NULL && !LoadClipCache()) {
		AVF_LOGW("RemoveReference: cache not loaded");
		return;
	}
	m_cache->main_stream.RemoveReference(start_time_ms, end_time_ms, mark_count);
	if (!m_cache->sub_stream.IsEmpty()) {
		m_cache->sub_stream.RemoveReference(start_time_ms, end_time_ms, mark_count);
	}
}

// lock all segments that are used by ref clips
void vdb_clip_t::LockAllSegments()
{
	if (m_cache == NULL && !LoadClipCache()) {
		AVF_LOGW("LockAllSegments: cache not loaded");
		return;
	}

	list_head_t *node;
	list_for_each(node, ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_clip_node(node);
		AddReference(ref_clip);
	}
}

clip_cache_t* vdb_clip_t::LoadClipCache()
{
	if (m_cache == NULL) {
		m_owner->LoadClipCache(this);
	} else {
		m_owner->GetClipCacheList().BringToFront(m_cache);
	}
	return m_cache;
}

void vdb_clip_t::ReleaseAllRefClips()
{
	list_head_t *node;
	list_head_t *next;
	list_for_each_del(node, next, ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_clip_node(node);
		ref_clip_set_t *rcs = m_owner->GetRefClipSet(ref_clip->ref_clip_type);
		if (rcs == NULL) {
			AVF_ASSERT(0);
		} else {
			rcs->ReleaseRefClip(ref_clip);
		}
	}
}

// find the min time for the ref clip
avf_status_t vdb_clip_t::GetMinStartTime(int stream, avf_u64_t time_ms, avf_u64_t& min_time_ms)
{
	vdb_seg_t *seg = FindSegmentByTime(stream, time_ms);
	if (seg == NULL) {
		AVF_LOGE("GetMinStartTime: segment not found, time_ms=" LLD, time_ms);
		return E_ERROR;
	}

	while (true) {
		min_time_ms = seg->GetStartTime_ms();
		if ((seg = seg->GetPrevSegInRefClip()) == NULL)
			break;
	}

	return E_OK;
}

// find the max time for the ref clip
avf_status_t vdb_clip_t::GetMaxEndTime(int stream, avf_u64_t time_ms, avf_u64_t& max_time_ms)
{
	vdb_seg_t *seg = FindSegmentByTime(stream, time_ms);
	if (seg == NULL) {
		AVF_LOGE("GetMaxEndTime: segment not found, time_ms=" LLD, time_ms);
		return E_ERROR;
	}

	while (true) {
		max_time_ms = seg->GetEndTime_ms();
		if ((seg = seg->GetNextSegInRefClip()) == NULL)
			break;
	}

	return E_OK;
}

avf_status_t vdb_clip_t::GetMinStartTime(avf_u64_t time_ms, avf_u64_t& min_time_ms)
{
	avf_u64_t min_time_ms_0;
	avf_u64_t min_time_ms_1;
	avf_status_t status;

	if ((status = GetMinStartTime(0, time_ms, min_time_ms_0)) != E_OK)
		return status;

	if (GetNumSegments(1) == 0) {
		// stream 1 is empty
		min_time_ms = min_time_ms_0;
		return E_OK;
	}

	if ((status = GetMinStartTime(1, time_ms, min_time_ms_1)) != E_OK)
		return status;

	min_time_ms = MAX(min_time_ms_0, min_time_ms_1);
	return E_OK;
}

avf_status_t vdb_clip_t::GetMaxEndTime(avf_u64_t time_ms, avf_u64_t& max_time_ms)
{
	avf_u64_t max_time_ms_0;
	avf_u64_t max_time_ms_1;
	avf_status_t status;

	if ((status = GetMaxEndTime(0, time_ms, max_time_ms_0)) != E_OK)
		return status;

	if (GetNumSegments(1) == 0) {
		// stream 1 is empty
		max_time_ms = max_time_ms_0;
		return E_OK;
	}

	if ((status = GetMaxEndTime(1, time_ms, max_time_ms_1)) != E_OK)
		return status;

	max_time_ms = MIN(max_time_ms_0, max_time_ms_1);
	return E_OK;
}

void vdb_clip_t::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_32(num_ref_clips);
	pmb->write_le_32(clip_id);
	pmb->write_le_32(buffered_clip_id);
	pmb->write_le_32(clip_date);
	pmb->write_le_32(gmtoff);

	pmb->write_le_32(lock_count);
	pmb->write_le_32(m_num_segs_0);
	pmb->write_le_32(m_num_segs_1);

	pmb->write_le_32(first_pic_name);
	pmb->write_le_32(first_pic_size);

	pmb->write_mem((const avf_u8_t*)&m_size_main, sizeof(vdb_size_t));
	pmb->write_mem((const avf_u8_t*)&m_size_sub, sizeof(vdb_size_t));

	pmb->write_mem((const avf_u8_t*)&main_stream_attr, sizeof(main_stream_attr));
	pmb->write_mem((const avf_u8_t*)&sub_stream_attr, sizeof(sub_stream_attr));
	pmb->write_mem((const avf_u8_t*)&raw_info, sizeof(raw_info));

	if (m_cache) {
		pmb->write_le_8(1);
		m_cache->main_stream.Serialize(pmb);
		m_cache->sub_stream.Serialize(pmb);
	} else {
		pmb->write_le_8(0);
	}

	pmb->write_le_32(m_clipinfo_filesize);
	pmb->write_le_64(m_clip_size);
	pmb->write_le_64(m_marked_size);
	pmb->write_le_32(m_clip_attr);

	list_head_t *node;
	list_for_each(node, m_desc_list) {
		clip_desc_item_s *item = clip_desc_item_s::from_node(node);
		pmb->write_le_32(item->type);
		pmb->write_le_32(item->size);
		pmb->write_mem(item->GetMem(), item->size);
	}
	pmb->write_le_32(0);
}

//-----------------------------------------------------------------------
//
//  ref_clip_t
//
//-----------------------------------------------------------------------

ref_clip_t::ref_clip_t(vdb_clip_t *clip, avf_u32_t ref_clip_type, clip_id_t ref_clip_id, avf_u32_t clip_attr)
{
	if (ref_clip_type != CLIP_TYPE_BUFFER) {	
		// can only auto delete buffered clips
		clip_attr |= CLIP_ATTR_NO_AUTO_DELETE;
	}
	this->clip_attr = clip_attr;
	this->m_scene_data = NULL;
	this->clip = clip;
	if (clip->m_cdir) {
		this->m_dirname = clip->m_cdir->m_dirname;
	} else {
		this->m_dirname = AVF_INVALID_TIME;
	}
	this->ref_clip_type = ref_clip_type;
	this->ref_clip_id = ref_clip_id;
	list_add_tail(&this->list_clip, clip->ref_clip_list);
	clip->num_ref_clips++;
}

ref_clip_t::~ref_clip_t()
{
	__list_del(&this->list_clip);
	avf_safe_release(this->m_scene_data);
	vdb_clip_t *clip = this->clip;
	clip->num_ref_clips--;
}

void ref_clip_t::SetSceneData(raw_data_t *scene_data)
{
	avf_assign(m_scene_data, scene_data);
}

// shrink the ref clip by the first seg (main) length
// caller should check empty clip
void ref_clip_t::ShrinkOneSegment()
{
	if (clip->m_cache) {
		vdb_seg_t *seg = clip->m_cache->main_stream.FindSegmentSinceTime(start_time_ms);
		if (seg == NULL) {
			// no segments in the ref_clip, force to delete
			if (end_time_ms != start_time_ms) {
				AVF_LOGW("ref_clip is empty: " LLD ", " LLD, start_time_ms, end_time_ms);
				end_time_ms = start_time_ms;
			}
		} else {
			avf_u64_t new_start_time_ms = seg->GetEndTime_ms();
			clip->RemoveReference(start_time_ms, new_start_time_ms, CalcMarkedSpace());
			start_time_ms = new_start_time_ms;
		}
	}
}

void ref_clip_t::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_32(clip_attr);
	pmb->write_le_32(clip->clip_id);
	pmb->write_le_32(m_dirname);
	pmb->write_le_32(ref_clip_type);
	pmb->write_le_32(ref_clip_id);
	pmb->write_le_32(ref_clip_date);
	pmb->write_le_32(plist_index);
	pmb->write_le_64(start_time_ms);
	pmb->write_le_64(end_time_ms);
}

//-----------------------------------------------------------------------
//
//  clip_set_t
//
//-----------------------------------------------------------------------

clip_set_t::~clip_set_t()
{
	ReleaseAllClips(clip_set.rb_node);
}

void clip_set_t::Init()
{
	clip_set.rb_node = NULL;
	list_init(clip_list);
	m_last_clip = NULL;

	mbDirty = false;
	mbSpaceChanged = false;

	mTotalClips = 0;
	mLoadedClips = 0;
	m_size.Init();

	m_clip_space = 0;
	m_marked_space = 0;
	m_clipinfo_space = 0;
}

void clip_set_t::Clear()
{
	m_seg_cache_list.Clear();
	m_clip_cache_list.Clear();
	ReleaseAllClips(clip_set.rb_node);
	Init();
}

void clip_set_t::AddClip(vdb_clip_t *clip, bool bUpdate)
{
	struct rb_node **p = &this->clip_set.rb_node;
	struct rb_node *parent = NULL;

	if (clip->m_cdir == NULL) {

		clip_id_t clip_id = clip->clip_id;
		while (*p) {
			parent = *p;
			vdb_clip_t *tmp = vdb_clip_t::from_node(parent);

			if (clip_id < tmp->clip_id) {
				p = &parent->rb_left;
			} else if (clip_id > tmp->clip_id) {
				p = &parent->rb_right;
			} else {
				AVF_LOGE("AddClip: internal error, clip 0x%x already exists", clip_id);
				return;
			}
		}

	} else {

		avf_u64_t id = MK_U64(clip->m_cdir->m_dirname, clip->clip_id);
		while (*p) {
			parent = *p;
			vdb_clip_t *tmp = vdb_clip_t::from_node(parent);
			avf_u64_t tmpid = MK_U64(tmp->m_cdir->m_dirname, tmp->clip_id);

			if (id < tmpid) {
				p = &parent->rb_left;
			} else if (id > tmpid) {
				p = &parent->rb_right;
			} else {
				AVF_LOGE("AddClip: internal error, clip " LLD " already exists", id);
				return;
			}
		}
	}

	rb_link_node(&clip->rbnode, parent, p);
	rb_insert_color(&clip->rbnode, &this->clip_set);
	list_add(&clip->list_clip, clip_list);

	mLoadedClips++;
	if (bUpdate) {
		mTotalClips++;
		mbDirty = true;
		m_size.v_size += clip->GetVideoSize();
		m_size.p_size += clip->GetPictureSize();
		m_size.r_size += clip->GetRawDataSize();
	}
}

// request to delete an empty clip
void clip_set_t::RemoveClip(vdb_clip_t *clip, IClipOwner *owner)
{
	// remove all files used by this clip
	// there is a case that the clip is empty, but stream 1 has files
	for (int i = 0; i < 2; i++) {
		vdb_stream_t *stream = clip->GetStream(i);
		if (stream) {
			stream->RemoveAllFiles();
		}
	}

	// release cache first
	if (clip->m_cache) {
		m_clip_cache_list.ReleaseCache(clip->m_cache);
		clip->m_cache = NULL;
	}

	owner->RemoveClipFiles(clip);

	clip_dir_t *cdir = clip->m_cdir;

	mTotalClips--;
	mbDirty = true;

	EraseClip(clip);

	owner->ClipRemoved(cdir);
}

void clip_set_t::ReleaseClip(vdb_clip_t *clip)
{
	// release ref clips
	clip->ReleaseAllRefClips();

	// release cache
	if (clip->m_cache) {
		m_clip_cache_list.ReleaseCache(clip->m_cache);
		clip->m_cache = NULL;
	}

	EraseClip(clip);
}

void clip_set_t::EraseClip(vdb_clip_t *clip)
{
	if (clip == m_last_clip) {
		m_last_clip = NULL;
	}
	mLoadedClips--;
	rb_erase(&clip->rbnode, &clip_set);
	__list_del(&clip->list_clip);
	avf_delete clip;
}

vdb_clip_t *clip_set_t::FindClip(clip_id_t clip_id)
{
	if (m_last_clip && clip_id == m_last_clip->clip_id) {
		return m_last_clip;
	}

	struct rb_node *n = clip_set.rb_node;
	vdb_clip_t *min_ge = NULL;

	while (n) {
		vdb_clip_t *clip = vdb_clip_t::from_node(n);
		if (clip_id <= clip->clip_id) {
			min_ge = clip;
			n = n->rb_left;
		} else {
			n = n->rb_right;
		}
	}

	if (min_ge && clip_id == min_ge->clip_id) {
		m_last_clip = min_ge;
		return min_ge;
	}

	AVF_LOGE("clip 0x%x not found", clip_id);
	return NULL;
}

vdb_clip_t *clip_set_t::FindClipEx(avf_time_t dirname, clip_id_t clip_id)
{
	if (mb_server_mode) {
		// compare with cache
		if (m_last_clip && m_last_clip->clip_id == clip_id && m_last_clip->m_cdir->m_dirname == dirname) {
			return m_last_clip;
		}

		avf_u64_t id = MK_U64(dirname, clip_id);
		struct rb_node *n = clip_set.rb_node;
		vdb_clip_t *min_ge = NULL;

		while (n) {
			vdb_clip_t *clip = vdb_clip_t::from_node(n);
			avf_u64_t tmpid = MK_U64(clip->m_cdir->m_dirname, clip->clip_id);
			if (id <= tmpid) {
				min_ge = clip;
				n = n->rb_left;
			} else {
				n = n->rb_right;
			}
		}

		if (min_ge && dirname == min_ge->m_cdir->m_dirname && clip_id == min_ge->clip_id) {
			m_last_clip = min_ge;
			return min_ge;
		}
	}

	return NULL;
}

// clip cache list should be released before this
void clip_set_t::ReleaseClipRecursive(struct rb_node *rbnode)
{
	ReleaseAllClips(rbnode->rb_left);
	ReleaseAllClips(rbnode->rb_right);
	vdb_clip_t *clip = vdb_clip_t::from_node(rbnode);
	avf_delete clip;
}

void clip_set_t::DoTrimLoadedClips(avf_size_t max)
{
	list_head_t *node;
	list_head_t *prev;
	list_for_each_del_backward(node, prev, clip_list) {
		vdb_clip_t *clip = vdb_clip_t::from_cs_node(node);
		if (clip->lock_count == 0) {
			ReleaseClip(clip);
			if (mLoadedClips <= max)
				return;
		}
	}
}

void clip_set_t::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_8(mbDirty);
	pmb->write_le_8(mbSpaceChanged);

	pmb->write_le_32(mTotalClips);
	pmb->write_le_32(mLoadedClips);
	pmb->write_mem((const avf_u8_t*)&m_size, sizeof(vdb_size_t));

	pmb->write_le_64(m_clip_space);
	pmb->write_le_64(m_marked_space);
	pmb->write_le_64(m_clipinfo_space);

	struct rb_node *node = rb_first(&clip_set);
	for (; node; node = rb_next(node)) {
		vdb_clip_t *clip = vdb_clip_t::from_node(node);
		pmb->write_le_8(1);
		clip->Serialize(pmb);
	}
	pmb->write_le_8(0);

	m_seg_cache_list.Serialize(pmb);
	m_clip_cache_list.Serialize(pmb);
}

//-----------------------------------------------------------------------
//
//  ref_clip_set_t
//
//-----------------------------------------------------------------------

ref_clip_set_t::ref_clip_set_t(bool b_server_mode, avf_u32_t ref_clip_type):
	mb_server_mode(b_server_mode)
{
	this->ref_clip_type = ref_clip_type;
	Init();
}

ref_clip_set_t::~ref_clip_set_t()
{
	ReleaseAll();
}

void ref_clip_set_t::Init()
{
	ref_clip_set.rb_node = NULL;
	list_init(ref_clip_list);
	m_last_ref_clip = NULL;

	mTotalRefClips = 0;
	m_total_length_ms = 0;
	ref_clip_type = ref_clip_type;
	m_max_clip_id = 0;
}

// should be called before all the clips are released
void ref_clip_set_t::ReleaseAll()
{
	list_head_t *node;
	list_head_t *next;
	list_for_each_del(node, next, ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_list_node(node);
		avf_delete ref_clip;
	}
}

// start_time_ms: input/output
// TODO: should use clip_id
clip_id_t ref_clip_set_t::GetRefClipId(sys_time_t clip_date, avf_u64_t *start_time_ms)
{
	clip_id_t ref_clip_id = clip_date + (avf_time_t)(*start_time_ms/1000);
	if (ref_clip_id <= m_max_clip_id) {
		ref_clip_id = m_max_clip_id + 1;
	}
	// adjust clip_id to be unique
	while (FindRefClip(ref_clip_id)) {
		AVF_LOGD(C_YELLOW "ref_clip_id 0x%x already exists, retry" C_NONE, ref_clip_id);
		(*start_time_ms) += 1000;
		ref_clip_id++;
	}
	return ref_clip_id;
}

// note: should make sure ref_clip_id is unique in the ref clip set
bool ref_clip_set_t::AddUniqueRefClip(ref_clip_t *ref_clip, avf_uint_t list_pos)
{
	struct rb_node **p = &ref_clip_set.rb_node;
	struct rb_node *parent = NULL;

	if (ref_clip->clip->m_cdir == NULL) {
		clip_id_t clip_id = ref_clip->ref_clip_id;
		while (*p) {
			parent = *p;
			ref_clip_t *tmp = ref_clip_t::from_rcs_node(parent);

			if (clip_id < tmp->ref_clip_id) {
				p = &parent->rb_left;
			} else if (clip_id > tmp->ref_clip_id) {
				p = &parent->rb_right;
			} else {
				AVF_LOGE("AddUniqueRefClip: internal error, clip 0x%x already exists", clip_id);
				return false;
			}
		}

	} else {

		avf_u64_t id = MK_U64(ref_clip->m_dirname, ref_clip->ref_clip_id);
		while (*p) {
			parent = *p;
			ref_clip_t *tmp = ref_clip_t::from_rcs_node(parent);
			avf_u64_t tmpid = MK_U64(tmp->m_dirname, tmp->ref_clip_id);

			if (id < tmpid) {
				p = &parent->rb_left;
			} else if (id > tmpid) {
				p = &parent->rb_right;
			} else {
				AVF_LOGE("AddUniqueRefClip: internal error, clip " LLD " already exists", id);
				return false;
			}
		}
	}

	// add to tree
	rb_link_node(&ref_clip->rbnode, parent, p);
	rb_insert_color(&ref_clip->rbnode, &ref_clip_set);

	if (list_pos == DEFAULT_LIST_POS) {
		// append to list
		list_add_tail(&ref_clip->list_node, ref_clip_list);
	} else {
		// insert into list
		list_head_t *node;
		list_for_each_backward(node, ref_clip_list) {
			ref_clip_t *ref_clip = ref_clip_t::from_list_node(node);
			if (list_pos > ref_clip->plist_index)
				break;
		}
		list_append(ref_clip->list_node, *node);
	}

	mTotalRefClips++;
	m_total_length_ms += ref_clip->GetDurationLong();

	if (m_max_clip_id < ref_clip->ref_clip_id) {
		m_max_clip_id = ref_clip->ref_clip_id;
	}

	return true;
}

bool ref_clip_set_t::CheckStreamSttr(vdb_stream_t& stream)
{
	if (!stream.IsEmpty()) {
		const avf_stream_attr_s& stream_attr = stream.GetStreamAttr();
		if (stream_attr.stream_version != CURRENT_STREAM_VERSION) {
			AVF_LOGE("This version of stream: %d cannot be added to playlist", stream_attr.stream_version);
			return false;
		}
	}
	return true;
}

bool ref_clip_set_t::CompareStreamAttr(vdb_stream_t& stream1, vdb_stream_t& stream2)
{
	if (stream1.IsEmpty()) {
		if (!stream2.IsEmpty()) {
			return false;
		}
		return true;
	}

	if (stream2.IsEmpty()) {
		return false;
	}

	const avf_stream_attr_s& attr1 = stream1.GetStreamAttr();
	const avf_stream_attr_s& attr2 = stream2.GetStreamAttr();

	if (attr1.stream_version != CURRENT_STREAM_VERSION || attr2.stream_version != CURRENT_STREAM_VERSION) {
		return false;
	}

	if (attr1.video_version != attr2.video_version)
		return false;
	if (attr1.video_coding != attr2.video_coding)
		return false;
	if (attr1.video_framerate != attr2.video_framerate)
		return false;
	if (attr1.video_width != attr2.video_width)
		return false;
	if (attr1.video_height != attr2.video_height)
		return false;

	if (attr1.audio_version != attr2.audio_version)
		return false;
	if (attr1.audio_coding != attr2.audio_coding)
		return false;
	if (attr1.audio_num_channels != attr2.audio_num_channels)
		return false;
	if (attr1.audio_sampling_freq != attr2.audio_sampling_freq)
		return false;

	return true;
}

// called by CmdMarkClip(), CmdInsertClip()
// clip[start_time_ms,end_time_ms) => ref_clip
// error==1 : stream not match
// error==2 : unknown stream
ref_clip_t *ref_clip_set_t::CreateRefClip(vdb_clip_t *clip,
	avf_u64_t start_time_ms, avf_u64_t end_time_ms, int *error,
	bool bCheckAttr, avf_uint_t list_pos, avf_u32_t clip_attr)
{
	*error = 0;

	// check stream attr
	if (bCheckAttr) {
		// todo - LoadClipCache
		ref_clip_t *first_ref_clip = GetFirstRefClipOfList();
		if (first_ref_clip) {
			if (!CompareStreamAttr(first_ref_clip->clip->m_cache->main_stream, 
					clip->m_cache->main_stream)) {
				AVF_LOGE("stream not match");
				*error = 1;
				return NULL;
			}
			if (!CompareStreamAttr(first_ref_clip->clip->m_cache->sub_stream,
					clip->m_cache->sub_stream)) {
				AVF_LOGE("sub stream not match");
				*error = 1;
				return NULL;
			}
		} else {
			if (!CheckStreamSttr(clip->m_cache->main_stream)) {
				*error = 2;
				return NULL;
			}
			if (!CheckStreamSttr(clip->m_cache->sub_stream)) {
				*error = 2;
				return NULL;
			}
		}
	}

	avf_u64_t tmp = start_time_ms;
	clip_id_t ref_clip_id = GetRefClipId(clip->clip_date, &tmp);

	ref_clip_t *ref_clip = clip->NewRefClip(ref_clip_type, ref_clip_id,
		start_time_ms, end_time_ms, clip_attr);
	if (!ref_clip)
		return NULL;

	if (AddUniqueRefClip(ref_clip, list_pos)) {
		clip->AddReference(ref_clip);
	} else {
		avf_delete ref_clip;
		ref_clip = NULL;
	}

	return ref_clip;
}

avf_status_t ref_clip_set_t::MoveRefClip(ref_clip_t *ref_clip, avf_u32_t new_list_pos)
{
	AVF_LOGD("MoveRefClip: %d -> %d", ref_clip->plist_index, new_list_pos);

	if (new_list_pos == ref_clip->plist_index)
		return E_ERROR;

	list_head_t *node;
	if (new_list_pos > ref_clip->plist_index) {
		node = &ref_clip->list_node;
		for (node = node->next; node != &ref_clip_list; node = node->next) {
			ref_clip_t *tmp = ref_clip_t::from_list_node(node);
			if (tmp->plist_index == new_list_pos) {
				list_del(&ref_clip->list_node);
				list_append(ref_clip->list_node, *node);
				return E_OK;
			}
		}
	} else {
		node = &ref_clip->list_node;
		for (node = node->prev; node != &ref_clip_list; node = node->prev) {
			ref_clip_t *tmp = ref_clip_t::from_list_node(node);
			if (tmp->plist_index == new_list_pos) {
				list_del(&ref_clip->list_node);
				list_insert(ref_clip->list_node, *node);
				return E_OK;
			}
		}
	}

	return E_ERROR;
}

// for live clip - clip is not referenced since its duration is 0
ref_clip_t *ref_clip_set_t::NewLiveRefClip(vdb_clip_t *clip, bool bServerMode, int clip_attr)
{
	avf_u64_t tmp = 0;
	clip_id_t ref_clip_id = bServerMode ? clip->clip_id : GetRefClipId(clip->clip_date, &tmp);
	ref_clip_t *ref_clip = clip->NewRefClip(ref_clip_type, ref_clip_id, 0, 0, clip_attr);

	if (!AddUniqueRefClip(ref_clip, DEFAULT_LIST_POS)) {
		avf_delete ref_clip;
		ref_clip = NULL;
	}

	clip->buffered_clip_id = ref_clip->ref_clip_id;

	return ref_clip;
}

// caller should check empty clip
void ref_clip_set_t::RemoveRefClip(ref_clip_t *ref_clip)
{
	// remove all references
	ref_clip->clip->RemoveReference(ref_clip);
	// release ref clip
	ReleaseRefClip(ref_clip);
}

void ref_clip_set_t::RemoveEmptyRefClip(ref_clip_t *ref_clip)
{
	AVF_ASSERT(ref_clip->IsEmpty());
	ReleaseRefClip(ref_clip);
}

void ref_clip_set_t::ReleaseRefClip(ref_clip_t *ref_clip)
{
	mTotalRefClips--;
	m_total_length_ms -= ref_clip->GetDurationLong();

	rb_erase(&ref_clip->rbnode, &ref_clip_set);
	__list_del(&ref_clip->list_node);

	if (ref_clip == m_last_ref_clip) {
		m_last_ref_clip = NULL;
	}

	if (ref_clip->ref_clip_type == CLIP_TYPE_BUFFER) {
		// 
		ref_clip->clip->buffered_clip_id = INVALID_CLIP_ID;
	}

	avf_delete ref_clip;
}

ref_clip_t *ref_clip_set_t::FindRefClip(clip_id_t ref_clip_id)
{
	// compare with cache
	if (m_last_ref_clip && ref_clip_id == m_last_ref_clip->ref_clip_id) {
		return m_last_ref_clip;
	}

	struct rb_node *n = ref_clip_set.rb_node;
	ref_clip_t *min_ge = NULL;

	while (n) {
		ref_clip_t *ref_clip = ref_clip_t::from_rcs_node(n);
		if (ref_clip_id <= ref_clip->ref_clip_id) {
			min_ge = ref_clip;
			n = n->rb_left;
		} else {
			n = n->rb_right;
		}
	}

	// clip_id <= min_ge->ref_clip_id
	if (min_ge && ref_clip_id == min_ge->ref_clip_id) {
		m_last_ref_clip = min_ge;
		return min_ge;
	}

	return NULL;
}

ref_clip_t *ref_clip_set_t::FindRefClipEx(avf_time_t dirname, clip_id_t ref_clip_id)
{
	if (mb_server_mode) {
		// compare with cache
		if (m_last_ref_clip && m_last_ref_clip->ref_clip_id == ref_clip_id &&
				m_last_ref_clip->m_dirname == dirname) {
			return m_last_ref_clip;
		}

		avf_u64_t id = MK_U64(dirname, ref_clip_id);
		struct rb_node *n = ref_clip_set.rb_node;
		ref_clip_t *min_ge = NULL;

		while (n) {
			ref_clip_t *ref_clip = ref_clip_t::from_rcs_node(n);
			avf_u64_t tmpid = MK_U64(ref_clip->m_dirname, ref_clip->ref_clip_id);
			if (id <= tmpid) {
				min_ge = ref_clip;
				n = n->rb_left;
			} else {
				n = n->rb_right;
			}
		}

		if (min_ge && dirname == min_ge->m_dirname && ref_clip_id == min_ge->ref_clip_id) {
			m_last_ref_clip = min_ge;
			return min_ge;
		}
	}

	return NULL;
}

void ref_clip_set_t::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_32(mTotalRefClips);
	pmb->write_le_32(m_total_length_ms);
	pmb->write_le_32(ref_clip_type);
	pmb->write_le_32(0);

	list_head_t *node;
	list_for_each(node, ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_list_node(node);
		pmb->write_le_8(1);
		ref_clip->Serialize(pmb);
	}

	pmb->write_le_8(0);
}

//-----------------------------------------------------------------------
//
//  playlist_set_t
//
//-----------------------------------------------------------------------

void playlist_set_t::ReleasePlaylist(struct rb_node *node)
{
	if (node->rb_left) ReleasePlaylist(node->rb_left);
	if (node->rb_right) ReleasePlaylist(node->rb_right);
	ref_clip_set_t *rcs = ref_clip_set_t::from_rbnode(node);
	avf_delete rcs;
}

avf_status_t playlist_set_t::AddPlaylist(ref_clip_set_t *rcs)
{
	struct rb_node **p = &playlist_set.rb_node;
	struct rb_node *parent = NULL;
	avf_u32_t playlist_id = rcs->ref_clip_type;

	// if the playlist already exists
	while (*p) {
		parent = *p;
		ref_clip_set_t *tmp = ref_clip_set_t::from_rbnode(parent);

		if (playlist_id < tmp->ref_clip_type) {
			p = &parent->rb_left;
		} else if (playlist_id > tmp->ref_clip_type) {
			p = &parent->rb_right;
		} else {
			AVF_LOGE("playlist %d already exists", playlist_id);
			return E_ERROR;
		}
	}

	rb_link_node(&rcs->rbnode, parent, p);
	rb_insert_color(&rcs->rbnode, &playlist_set);

	return E_OK;
}

avf_status_t playlist_set_t::RemovePlaylistFiles(const char *vdb_root, ref_clip_set_t *rcs)
{
	char buffer[32];
	::sprintf(buffer, "%d/", rcs->ref_clip_type);

	ap<string_t> path(vdb_root, PLAYLIST_PATH, buffer);
	return avf_remove_dir_recursive(path->string(), true) ? E_OK : E_ERROR;
}

avf_status_t playlist_set_t::RemovePlaylist(ref_clip_set_t *rcs)
{
	rb_erase(&rcs->rbnode, &playlist_set);
	avf_delete rcs;
	return E_OK;
}

avf_u32_t playlist_set_t::GeneratePlaylistId()
{
	avf_u32_t playlist_id = playlist_id_start;
	for (;;) {
		if (GetPlaylist(playlist_id) == NULL) {
			return playlist_id;
		}
		playlist_id++;
	}
}

ref_clip_set_t *playlist_set_t::GetPlaylist(avf_u32_t playlist_id)
{
	struct rb_node *n = playlist_set.rb_node;
	ref_clip_set_t *min_ge = NULL;

	while (n) {
		ref_clip_set_t *rcs = ref_clip_set_t::from_rbnode(n);
		if (playlist_id <= rcs->ref_clip_type) {
			min_ge = rcs;
			n = n->rb_left;
		} else {
			n = n->rb_right;
		}
	}

	if (min_ge && playlist_id == min_ge->ref_clip_type)
		return min_ge;

	return NULL;
}

ref_clip_set_t *playlist_set_t::CreatePlaylist(bool b_server_mode, avf_u32_t playlist_id)
{
	ref_clip_set_t *rcs = avf_new ref_clip_set_t(b_server_mode, playlist_id);
	if (rcs == NULL) {
		return NULL;
	}

	avf_status_t status = AddPlaylist(rcs);
	if (status != E_OK) {
		avf_delete rcs;
		return NULL;
	}

	avf_u32_t new_id = playlist_id + 1;
	if (playlist_id_start < new_id)
		playlist_id_start = new_id;

	return rcs;
}

//-----------------------------------------------------------------------
//
//  vdb_seg_t
//
//-----------------------------------------------------------------------

vdb_seg_t::vdb_seg_t(vdb_clip_t *clip, int stream,
	avf_time_t subdir_name, avf_time_t index_name, 
	avf_time_t video_name, avf_time_t picture_name,
	const info_t *info)
{
	// rbnode
	m_clip = clip;
	m_stream = stream;

	m_subdir_name = subdir_name;
	m_index_name = index_name;
	m_video_name = video_name;
	m_picture_name = picture_name;
	m_first_pic_size = 0;

	m_ref_count = 0;
	m_info = *info;

	for (avf_size_t i = 0; i < kNumExtraArrays; i++) {
		extra_data_size[i] = 0;
	}

	extra_raw_sizes._Init();

	m_mark_count = 0;
	m_filesize = 0;

	m_cache = NULL;
}

avf_status_t vdb_seg_t::OpenVideoRead(IAVIO **pio) const
{
	string_t *video_filename = GetVideoFileName();
	if (video_filename == NULL) {
		AVF_LOGE("seg has no video");
		return E_ERROR;
	}

	avf_safe_release(*pio);
	if (GetVideoType() == VIDEO_TYPE_MP4)
		*pio = CTSMapIO::Create();
	else
		*pio = CSysIO::Create();
	if (*pio == NULL)
		return E_NOMEM;

	avf_status_t status = (*pio)->OpenRead(video_filename->string());
	video_filename->Release();

	return status;
}

avf_status_t vdb_seg_t::OpenPictureRead(IAVIO *io) const
{
	string_t *picture_filename = GetPictureFileName();
	if (picture_filename == NULL) {
		AVF_LOGE("seg has no picture");
		return E_ERROR;
	}

	avf_status_t status = io->OpenRead(picture_filename->string());

	if (status != E_OK) {
		AVF_LOGE("cannot open %s", picture_filename->string());
	}

	picture_filename->Release();

	return status;
}

avf_status_t vdb_seg_t::ReadIndexPicture(avf_u32_t offset, avf_u8_t *buffer, avf_u32_t size) const
{
	seg_cache_list_t& cache_list = m_clip->m_owner->GetSegCacheList();
	IAVIO *io;

	if ((io = cache_list.GetPictureIO(this)) == NULL)
		return E_ERROR;

	if (io->ReadAt(buffer, size, offset) != (int)size) {
		cache_list.ReleasePictureIO();
		return E_IO;
	}

	cache_list.ReleasePictureIO();
	return E_OK;
}

avf_status_t vdb_seg_t::ReadIFrame(avf_u32_t offset, avf_u32_t bytes, CMemBuffer *pmb, avf_u8_t *pFrameSize) const
{
	IAVIO *io;

	// todo: use cache
	avf_status_t status = m_clip->m_owner->OpenSegmentVideo(this, &io);
	if (status != E_OK)
		return status;

	avf_u32_t buf_size = 88*188;
	avf_u8_t *buf = avf_malloc_bytes(buf_size);
	if (buf == NULL) {
		io->Release();
		return E_NOMEM;
	}

	if ((status = io->Seek(offset)) == E_OK) {
		status = DoReadIFrame(io, bytes, buf, buf_size, pmb, pFrameSize);
	}

	avf_free(buf);
	io->Release();

	return status;
}

avf_status_t vdb_seg_t::DoReadTSB(tsb_s *tsb)
{
	if (tsb->bytes == 0)
		return E_ERROR;

	avf_u32_t toread = MIN(tsb->bytes, tsb->buf_size);
	if (tsb->io->Read(tsb->buf, toread) != (int)toread) {
		AVF_LOGE("DoReadTSB failed");
		return E_IO;
	}

	tsb->bytes -= toread;
	tsb->packet = tsb->buf;
	tsb->npackets = toread / 188;

	return E_OK;
}

avf_status_t vdb_seg_t::DoReadIFrame(IAVIO *io, avf_u32_t bytes, 
	avf_u8_t *buf, avf_u32_t buf_size, CMemBuffer *pmb, avf_u8_t *pFrameSize)
{
	tsb_s tsb;
	avf_status_t status;

	tsb.io = io;
	tsb.bytes = bytes;
	tsb.buf = buf;
	tsb.buf_size = buf_size;
	tsb.packet = NULL;
	tsb.npackets = 0;

	int pmt_pid = 0;
	for (;;) {
		if (tsb.npackets == 0 && (status = DoReadTSB(&tsb)) != E_OK)
			return status;

		avf_u8_t *p = tsb.packet;
		if (*p != 0x47) {
			AVF_LOGE("bad packet");
			return E_ERROR;
		}

		tsb.packet += 188;
		tsb.npackets--;

		int pid = load_be_16(p + 1) & 0x1FFF;
		if (pid == 0) {
			p += 4;
			p += p[0] + 1;	// skip pointer_field
			p += 8 + 2;		// to items, skip program_number
			pmt_pid = load_be_16(p) & 0x1FFF;
			break;
		}
	}

	int v_pid = 0;
	for (;;) {
		if (tsb.npackets == 0 && (status = DoReadTSB(&tsb)) != E_OK)
			return status;

		avf_u8_t *p = tsb.packet;
		if (*p != 0x47) {
			AVF_LOGE("bad packet");
			return E_ERROR;
		}

		tsb.packet += 188;
		tsb.npackets--;

		int pid = load_be_16(p + 1) & 0x1FFF;
		if (pid == pmt_pid) {
			avf_size_t section_remain;
			avf_size_t pg_info_len;

			p += 4; 	// skip packet header
			p += p[0] + 1;	// skip pointer_field

			if (p[0] != 2)
				continue;

			section_remain = load_be_16(p + 1) & 0x3FF;
			p += 10;	// to program_info_length

			if ((section_remain >= 13) && tsb.packet - p >= (int)section_remain) {

				section_remain -= 13;

				// skip descriptors
				pg_info_len = load_be_16(p) & 0x0FFF;
				p += 2 + pg_info_len;
				section_remain -= pg_info_len;

				if (section_remain > 0) {
					while (true) {
						if (*p == 0x1B) {
							v_pid = load_be_16(p + 1) & 0x1FFF;
							break;
						}

						avf_uint_t es_info_len = load_be_16(p + 3) & 0x0FFF;
						p += 5;
						p += es_info_len;
						avf_uint_t tmp = 5 + es_info_len;
						if (section_remain <= tmp)
							break;
						section_remain -= tmp;
					}
				}
			}

			if (v_pid != 0)
				break;
		}
	}

	int v_start = 1;
	avf_u32_t nread = 0;

	for (;;) {
		if (tsb.npackets == 0) {
			if (tsb.bytes == 0)
				break;
			if ((status = DoReadTSB(&tsb)) != E_OK)
				return status;
		}

		avf_u8_t *p = tsb.packet;
		if (*p != 0x47) {
			AVF_LOGE("bad packet");
			return E_ERROR;
		}

		tsb.packet += 188;
		tsb.npackets--;

		int pid = load_be_16(p + 1) & 0x1FFF;
		if (pid == v_pid) {
			int pusi = (p[1] >> 6) & 1;
			if (v_start != pusi) {
				if (nread > 0)
					break;
				AVF_LOGE("v_start: %d, pusi: %d", v_start, pusi);
				return E_ERROR;
			}

			int adapt = (p[3] >> 4) & 3;
			p += 4;

			if (adapt & 2) {	// 2,3
				p += p[0] + 1;	// adaptation_field
			}

			if (v_start) {
				v_start = 0;
				p += p[8] + 9;	// pes header
			}

			avf_u32_t len = tsb.packet - p;
			avf_u8_t *ptr = pmb->Alloc(len);
			if (ptr == NULL)
				return E_NOMEM;

			::memcpy(ptr, p, len);
			nread += len;
		} else {
			if (nread > 0)
				break;
		}
	}

	*(avf_u32_t*)pFrameSize = nread;
	AVF_LOGD("IFrame %d bytes", nread);

	return E_OK;
}

avf_status_t vdb_seg_t::GetFirstPos(int index, index_pos_t *pos)
{
	if ((pos->array = GetIndexArray(index)) == NULL)
		return E_ERROR;

	if ((pos->item = pos->array->GetFirstItem()) == NULL)
		return E_ERROR;

	return E_OK;
}

avf_status_t vdb_seg_t::FindPosRelative(int index, avf_s32_t seg_time_ms, index_pos_t *pos)
{
	if ((pos->array = GetIndexArray(index)) == NULL)
		return E_ERROR;

	if ((pos->item = pos->array->FindIndexItem(seg_time_ms)) == NULL)
		return E_ERROR;

	return E_OK;
}

vdb_seg_t *vdb_seg_t::FindSegWithRawData(int index, avf_u64_t end_time_ms, index_pos_t *pos)
{
	vdb_seg_t *seg = this;

	while (end_time_ms > seg->GetStartTime_ms()) {

		if (seg->GetRawDataSize(index) > 0) {
			pos->array = seg->GetIndexArray(index);
			if (pos->array && pos->array->GetMemBuffer())
				return seg;
		}

		if ((seg = seg->GetNextSegment()) == NULL)
			break;
	}

	return NULL;
}

seg_cache_t *vdb_seg_t::LoadSegCache()
{
	if (m_cache == NULL) {
		m_clip->m_owner->LoadSegCache(this);
	} else {
		m_clip->m_owner->GetSegCacheList().BringToFront(m_cache);
	}
	return m_cache;
}

void vdb_seg_t::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_32(m_stream);
	pmb->write_le_32(m_subdir_name);
	pmb->write_le_32(m_index_name);
	pmb->write_le_32(m_video_name);
	pmb->write_le_32(m_picture_name);
	pmb->write_le_32(m_ref_count);
	pmb->write_le_32(m_mark_count);
	pmb->write_le_64(m_filesize);
	pmb->write_mem((const avf_u8_t*)&m_info, sizeof(m_info));
	if (m_cache == NULL) {
		pmb->write_le_8(0);
	} else {
		pmb->write_le_8(1);
		m_cache->Serialize(pmb);
	}
}

//-----------------------------------------------------------------------
//
//	seg_cache_t
//
//-----------------------------------------------------------------------
void seg_cache_t::Serialize(CMemBuffer *pmb) const
{
	for (int i = 0; i < kAllArrays; i++) {
		if (sindex.arrays[i] == NULL) {
			pmb->write_le_8(0);
		} else {
			pmb->write_le_8(1);
			sindex.arrays[i]->Serialize(pmb);
		}
	}
}

//-----------------------------------------------------------------------
//
//  seg_cache_list_t 
//		- must be used by VdbContent to be protected together with segment_t
//
//-----------------------------------------------------------------------
seg_cache_list_t::seg_cache_list_t(bool b_server_mode, avf_size_t max_cache):
	mb_server_mode(b_server_mode),
	m_max_cache(max_cache)
{
	list_init(m_cache_list);

	m_num_cache = 0;
	m_num_locked = 0;

	m_io_seg = NULL;
	m_io = NULL;

	m_io_hit = 0;
	m_io_miss = 0;
}

seg_cache_list_t::~seg_cache_list_t()
{
	// AVF_LOGD("cache io miss: %d, hit: %d", m_io_miss, m_io_hit);
	TrimCache(0);
	AVF_ASSERT(m_num_cache == 0);
}

IAVIO *seg_cache_list_t::DoGetPictureIO(const vdb_seg_t *seg)
{
	m_io_miss++;

	if (m_io == NULL) {
		if ((m_io = CFileIO::Create()) == NULL)
			return NULL;
	}

	avf_status_t status = seg->OpenPictureRead(m_io);
	if (status != E_OK) {
		avf_safe_release(m_io);
		m_io_seg = NULL;
		return NULL;
	}

	m_io_seg = seg;
	return m_io;
}

void seg_cache_list_t::AllocCache(vdb_seg_t *seg)
{
	if (seg->m_cache) {
		AVF_LOGW("seg already has cache");
		return;
	}

	seg_cache_t *cache = avf_new seg_cache_t(seg);
	seg->m_cache = cache;

	list_add(&cache->list_cache, m_cache_list);
	m_num_cache++;
}

void seg_cache_list_t::ReleaseCache(seg_cache_t *cache)
{
	AVF_ASSERT(cache->lock_count == 0);

	// close the io
	if (cache->seg == m_io_seg) {
		m_io_seg = NULL;
		avf_safe_release(m_io);
	}

	// remove cache from list
	list_del(&cache->list_cache);
	avf_delete cache;
	m_num_cache--;
}

void seg_cache_list_t::LockSegCache(vdb_seg_t *seg)
{
	seg_cache_t *cache = seg->m_cache;

	if (cache == NULL) {
		AVF_LOGW("seg cache not loaded");
		return;
	}

	if (cache->lock_count == 0) {
		m_num_locked++;
		AVF_LOGD(C_YELLOW "seg(%d) cache locked %d" C_NONE, seg->m_info.id, m_num_locked);
	}

	cache->lock_count++;
}

void seg_cache_list_t::UnlockSegCache(vdb_seg_t *seg)
{
	seg_cache_t *cache = seg->m_cache;

	if (cache == NULL) {
		AVF_LOGW("seg cache not loaded");
		return;
	}

	if (cache->lock_count <= 0) {
		AVF_LOGW("lock_count is 0");
		return;
	}

	if (--cache->lock_count == 0) {
		m_num_locked--;
		AVF_LOGD(C_YELLOW "seg[%d] cache unlocked, total %d" C_NONE, seg->m_info.id, m_num_locked);
		Trim();
	}
}

// remove caches until the count is <= count
void seg_cache_list_t::TrimCache(avf_uint_t count)
{
	if (m_num_cache <= m_num_locked + count)
		return;

	list_head_t *node;
	list_head_t *prev;
	list_for_each_del_backward(node, prev, m_cache_list) {
		seg_cache_t *cache = seg_cache_t::from_node(node);
		vdb_seg_t *seg = cache->seg;

		if (cache->lock_count == 0) {
			AVF_ASSERT(seg->m_cache == cache);
			seg->m_cache = NULL;
			ReleaseCache(cache);

			if (m_num_cache <= m_num_locked + count)
				return;
		}
	}
}

void seg_cache_list_t::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_32(m_max_cache);
	pmb->write_le_32(m_num_cache);
	pmb->write_le_32(m_num_locked);
	pmb->write_le_32(m_io_hit);
	pmb->write_le_32(m_io_miss);
}

//-----------------------------------------------------------------------
//
//  clip_cache_t
//
//-----------------------------------------------------------------------
clip_cache_t::clip_cache_t(seg_cache_list_t& seg_cache_list, vdb_clip_t *aclip):
	clip(aclip),
	main_stream(seg_cache_list, aclip, 0),
	sub_stream(seg_cache_list, aclip, 1)
{
}

//-----------------------------------------------------------------------
//
//  clip_cache_list_t
//
//-----------------------------------------------------------------------
clip_cache_list_t::clip_cache_list_t(seg_cache_list_t& seg_cache_list, bool bKeepAll, avf_size_t max_cache):
	m_max_cache(max_cache),
	mb_keep_all(bKeepAll),
	m_seg_cache_list(seg_cache_list)
{
	list_init(m_cache_list);
	m_num_cache = 0;
}

clip_cache_list_t::~clip_cache_list_t()
{
	TrimCache(0);
	AVF_ASSERT(m_num_cache == 0);
}

void clip_cache_list_t::AllocCache(vdb_clip_t *clip)
{
	if (clip->m_cache) {
		AVF_LOGW("clip already has cache");
		return;
	}

	clip_cache_t *cache = avf_new clip_cache_t(m_seg_cache_list, clip);
	clip->m_cache = cache;

	list_add(&cache->list_cache, m_cache_list);
	m_num_cache++;
}

void clip_cache_list_t::ReleaseCache(clip_cache_t *cache)
{
	// remove cache from list
	list_del(&cache->list_cache);
	avf_delete cache;
	m_num_cache--;
}

// do not release the locked clips
void clip_cache_list_t::TrimCache(avf_uint_t count)
{
	if (m_num_cache <= count)
		return;

	list_head_t *node;
	list_head_t *prev;
	list_for_each_del_backward(node, prev, m_cache_list) {
		clip_cache_t *cache = clip_cache_t::from_node(node);
		vdb_clip_t *clip = cache->clip;

		AVF_ASSERT(clip->m_cache == cache);

		if (clip->lock_count == 0) {
			clip->m_cache = NULL;
			ReleaseCache(cache);
			if (m_num_cache <= count)
				return;
		}
	}
}

void clip_cache_list_t::Serialize(CMemBuffer *pmb) const
{
	pmb->write_le_32(m_max_cache);
	pmb->write_le_32(m_num_cache);
	pmb->write_le_32(mb_keep_all);
}

//-----------------------------------------------------------------------
//
//  copy_clip_set_s
//
//-----------------------------------------------------------------------

copy_clip_set_s::copy_clip_set_s(clip_set_t *clip_set, IClipOwner *clip_owner)
{
	_Init();

	this->clip_set = clip_set;
	this->clip_owner = clip_owner;

	callback = NULL;
	context = NULL;
	b_cancel = 0;
}

copy_clip_set_s::~copy_clip_set_s()
{
	Clear();
}

void copy_clip_set_s::Clear()
{
	copy_clip_s *cclip = elems;
	for (avf_uint_t i = 0; i < count; i++, cclip++) {
		vdb_clip_t *clip = cclip->clip;
		cclip->Clear();
		if (clip->IsNotUsed()) {
			AVF_LOGD("clear copy_clip_set_s remove clip");
			clip_set->RemoveClip(clip, clip_owner);
		}
	}
	_Release();
}

int copy_clip_set_s::AddCopyRef(vdb_clip_t *clip, const copy_ref_s *ref)
{
	copy_clip_s *cclip = FindCopyClip(clip);
	if (cclip == NULL) {
		cclip = NewCopyClip(clip);
		if (cclip == NULL) {
			return E_ERROR;
		}
	}

	int result = cclip->AddCopyRef(ref);
	if (result < 0) {
		// TODO: if it failed
	}

	return result;
}

void copy_clip_set_s::GetSize(string_t *clipinfo_dir, avf_u64_t *picture_size, avf_u64_t *video_size, avf_u64_t *index_size)
{
	*picture_size = 0;
	*video_size = 0;
	*index_size = 0;

	copy_clip_s *cclip = elems;
	for (avf_uint_t i = 0; i < count; i++, cclip++) {

		avf_u64_t p_size = 0;
		avf_u64_t v_size = 0;
		avf_u64_t i_size = 0;
		avf_u64_t c_size = 0;

		cclip->GetSize(clipinfo_dir, &p_size, &v_size, &i_size, &c_size);

		*picture_size += p_size;
		*video_size += v_size;
		*index_size += i_size;

		cclip->clipinfo_filesize = c_size;
		*index_size += c_size;
	}
}

bool copy_clip_set_s::ClipIdExists(int clip_type, clip_id_t clip_id)
{
	for (avf_uint_t i = 0; i < count; i++) {
		copy_clip_s *cclip = elems + i;
		if (cclip->ClipIdExists(clip_type, clip_id))
			return true;
	}

	return false;
}

void copy_clip_set_s::GenerateRefClipId(ref_clip_set_t *rcs, copy_ref_s *ref)
{
	avf_u64_t start_time_ms = ref->start_time_ms;
	clip_id_t clip_id;

	while (true) {
		// find a unique ref clip id in ref clip set
		clip_id = rcs->GetRefClipId(ref->clip_date, &start_time_ms);

		// if it does not exist
		if (!ClipIdExists(ref->clip_type, clip_id))
			break;

		start_time_ms += 1000;
	}

	ref->clip_id = clip_id;
}

copy_clip_s *copy_clip_set_s::FindCopyClip(vdb_clip_t *clip)
{
	copy_clip_s *cclip = elems;
	for (avf_uint_t i = 0; i < count; i++, cclip++) {
		if (cclip->clip == clip)
			return cclip;
	}
	return NULL;
}

copy_clip_s *copy_clip_set_s::NewCopyClip(vdb_clip_t *clip)
{
	copy_clip_s *cclip = _Append(NULL);
	if (cclip) {
		cclip->Init(clip);
	}
	return cclip;
}

void copy_range_s::GetSize(vdb_seg_t *seg, int index, avf_u32_t *size)
{
	CIndexArray::pos_t spos;
	if (seg->FindPos(index, start_time_ms, &spos) != E_OK) {
		return;
	}

	CIndexArray::pos_t epos;
	if (seg->FindPos(index, end_time_ms - 1, &epos) != E_OK) {
		return;
	}

	*size = (epos.item + 1)->fpos - spos.item->fpos;
}

void copy_seg_s::GetSize(vdb_stream_t *stream, avf_u32_t *p_size, avf_u32_t *v_size, avf_u32_t *i_size)
{
	vdb_seg_t *seg = stream->FindSegmentById(seg_id);
	if (seg == NULL)
		return;

	copy_range_s *crange = elems;
	for (avf_uint_t i = 0; i < count; i++, crange++) {
		avf_u32_t c_p_size = 0;
		avf_u32_t c_v_size = 0;
		crange->GetSize(seg, kPictureArray, &c_p_size);
		crange->GetSize(seg, kVideoArray, &c_v_size);
		*p_size += c_p_size;
		*v_size += c_v_size;
	}

	string_t *filename = seg->GetIndexFileName();
	if (filename) {
		// estimate index_size
		index_size = avf_get_file_size(filename->string());
		*i_size += index_size * count;
		filename->Release();
	}

	AVF_LOGD("seg %d size: %d, %d, %d", seg_id, *p_size, *v_size, *i_size);
}

void copy_stream_s::Release()
{
	copy_seg_s *cseg = elems;
	for (avf_uint_t i = 0; i < count; i++, cseg++) {
		cseg->Release();
	}
	_Release();
}

void copy_stream_s::AddCopyRef(vdb_stream_t& stream, const copy_ref_s *ref)
{
	if (ref->start_time_ms < ref->end_time_ms) {
		vdb_seg_t *seg = stream.FindSegmentSinceTime(ref->start_time_ms);
		while (seg && ref->end_time_ms > seg->GetStartTime_ms()) {
			AddSegment(ref, seg);
			seg = seg->GetNextSegment();
		}
	}
}

void copy_stream_s::AddSegment(const copy_ref_s *ref, vdb_seg_t *seg)
{
	avf_u64_t seg_start_time_ms = seg->GetStartTime_ms();
	avf_u64_t seg_end_time_ms = seg->GetEndTime_ms();

	avf_u64_t ref_start_time_ms;
	avf_u64_t ref_end_time_ms;

	// align ref->start_time_ms to video boundary
	if (ref->start_time_ms <= seg_start_time_ms) {
		ref_start_time_ms = seg_start_time_ms;
	} else {
		CIndexArray::pos_t pos;
		if (seg->FindPos(kVideoArray, ref->start_time_ms, &pos) != E_OK) {
			AVF_LOGE("AddSegment failed, ref->start_time_ms: " LLD, ref->start_time_ms);
			return;
		}
		ref_start_time_ms = seg_start_time_ms + pos.item->time_ms;
	}

	// align ref->end_time_ms to video boundary
	if (ref->end_time_ms >= seg_end_time_ms) {
		ref_end_time_ms = seg_end_time_ms;
	} else {
		CIndexArray::pos_t pos;
		if (seg->FindPos(kVideoArray, ref->end_time_ms - 1, &pos) != E_OK) {
			AVF_LOGE("AddSegment failed, ref->end_time_ms: " LLD, ref->end_time_ms);
			return;
		}
		ref_end_time_ms = seg_start_time_ms + (pos.item + 1)->time_ms;
	}

	int index = FindSegmentGE(seg->m_info.id);
	copy_seg_s *cseg;

	if (index >= 0 && elems[index].seg_id == seg->m_info.id) {
		// already exists
		cseg = elems + index;
	} else {
		if (index < 0) {
			cseg = _Append(NULL);
		} else if (elems[index].seg_id != seg->m_info.id) {
			cseg = _Insert(NULL, index);
		}
		if (cseg == NULL)
			return;
		cseg->Init();
		cseg->seg_id = seg->m_info.id;
	}

	cseg->AddRange(ref_start_time_ms, ref_end_time_ms);
}

void copy_seg_s::AddRange(avf_u64_t ref_start_time_ms, avf_u64_t ref_end_time_ms)
{
	copy_range_s *crange = elems;
	for (avf_uint_t i = 0; i < count; i++, crange++) {
		if (ref_end_time_ms < crange->start_time_ms) {
			crange = _Insert(NULL, i);
			if (crange) {
				crange->start_time_ms = ref_start_time_ms;
				crange->end_time_ms = ref_end_time_ms;
			}
			return;
		}

		// ref_end_time_ms >= crange->start_time_ms
		if (ref_start_time_ms <= crange->end_time_ms) {
			// combine
			crange->start_time_ms = MIN(ref_start_time_ms, crange->start_time_ms);
			crange->end_time_ms = MAX(ref_end_time_ms, crange->end_time_ms);
			// combine the following
			copy_range_s *cnext = crange + 1;
			i++;
			while (i < count) {
				if (crange->end_time_ms < cnext->start_time_ms)
					break;
				crange->end_time_ms = MAX(crange->end_time_ms, cnext->start_time_ms);
				_Remove(i);
			}
			return;
		}
	}

	// not found
	crange = _Append(NULL);
	if (crange) {
		crange->start_time_ms = ref_start_time_ms;
		crange->end_time_ms = ref_end_time_ms;
	}
}

int copy_stream_s::FindSegmentGE(avf_uint_t seg_id)
{
	copy_seg_s *cseg = elems;
	for (avf_uint_t i = 0; i < count; i++, cseg++) {
		if (cseg->seg_id >= seg_id)
			return i;
	}
	return -1;
}

void copy_stream_s::GetSize(vdb_stream_t *stream, avf_u64_t *p_size, avf_u64_t *v_size, avf_u64_t *i_size)
{
	copy_seg_s *cseg = elems;
	for (avf_uint_t i = 0; i < count; i++, cseg++) {
		avf_u32_t seg_p_size = 0;
		avf_u32_t seg_v_size = 0;
		avf_u32_t seg_i_size = 0;
		cseg->GetSize(stream, &seg_p_size, &seg_v_size, &seg_i_size);
		*p_size += seg_p_size;
		*v_size += seg_v_size;
		*i_size += seg_i_size;
	}
}

void copy_clip_s::Init(vdb_clip_t *clip)
{
	_Init();

	this->clip = clip;

	stream_0.Init();
	stream_1.Init();
}

void copy_clip_s::Clear()
{
	stream_0.Release();
	stream_1.Release();

	copy_ref_s *ref = elems;
	for (avf_uint_t i = 0; i < count; i++, ref++) {
		clip->_UnlockClip(ref->start_time_ms, ref->end_time_ms);
		avf_safe_release(ref->scene_data);
	}

	clip = NULL;

	_Release();
}

int copy_clip_s::AddCopyRef(const copy_ref_s *ref)
{
	copy_ref_s *this_ref = _Append(ref);

	stream_0.AddCopyRef(clip->m_cache->main_stream, ref);
	if (!clip->m_cache->sub_stream.IsEmpty()) {
		stream_1.AddCopyRef(clip->m_cache->sub_stream, ref);
	}

	// lock clip
	clip->LockClip(this_ref->start_time_ms, this_ref->end_time_ms);

	return 0;
}

// stream 0, 1:
//	seg + seg + ... + seg + clipinfo
//	seg:
//		ts + jpg + idx
//	clipinfo:
//		clip
void copy_clip_s::GetSize(string_t *clipinfo_dir, avf_u64_t *p_size, avf_u64_t *v_size, avf_u64_t *i_size, avf_u64_t *c_size)
{
	AVF_ASSERT(clip->m_cache != NULL);

	avf_u64_t p_size_main = 0;
	avf_u64_t v_size_main = 0;
	avf_u64_t i_size_main = 0;
	stream_0.GetSize(clip->GetStream(0), &p_size_main, &v_size_main, &i_size_main);

	avf_u64_t p_size_sub = 0;
	avf_u64_t v_size_sub = 0;
	avf_u64_t i_size_sub = 0;
	stream_1.GetSize(clip->GetStream(1), &p_size_sub, &v_size_sub, &i_size_sub);

	*p_size = p_size_main + p_size_sub;
	*v_size = v_size_main + v_size_sub;
	*i_size = i_size_main + i_size_sub;

	local_time_name_t lname;
	local_time_to_name(clip->clip_id, CLIPINFO_FILE_EXT, lname);
	ap<string_t> clipinfo_name(clipinfo_dir, lname.buf);

	*c_size = avf_get_file_size(clipinfo_name->string());
}

bool copy_clip_s::ClipIdExists(int clip_type, clip_id_t clip_id)
{
	copy_ref_s *ref = elems;
	for (avf_uint_t i = 0; i < count; i++, ref++) {
		if (clip_type == ref->clip_type && clip_id == ref->clip_id)
			return true;
	}

	return false;
}

copy_seg_info_s::out_s* copy_stream_info_s::AddSegInfo(copy_seg_info_s *segi)
{
	copy_seg_info_s::out_s *out = _Append(&segi->out);

	if (out) {
		size.v_size += segi->out.seg_info.seg_data_size[kVideoArray];
		size.p_size += segi->out.seg_info.seg_data_size[kPictureArray];
		size.r_size += segi->out.seg_info.raw_data_size;
	}

	return out;
}

