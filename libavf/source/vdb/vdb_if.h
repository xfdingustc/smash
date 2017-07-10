
#ifndef __VDB_IF_H__
#define __VDB_IF_H__

extern const avf_iid_t IID_IVDBBroadcast;

class ref_clip_t;
class IVDBBroadcast;
struct mark_info_s;

//-----------------------------------------------------------------------
//
// IVDBBroadcast
//
//-----------------------------------------------------------------------
class IVDBBroadcast: public IInterface
{
public:
	DECLARE_IF(IVDBBroadcast, IID_IVDBBroadcast);

public:
	virtual avf_status_t VdbReady(string_t *vdb_id, int status) = 0;
	virtual avf_status_t VdbUnmounted(string_t *vdb_id) = 0;
	virtual avf_status_t ClipInfo(string_t *vdb_id, int action, ref_clip_t *ref_clip, mark_info_s *mark_info) = 0;
	virtual avf_status_t ReportSpaceInfo(string_t *vdb_id, avf_u64_t total_bytes, avf_u64_t used_bytes, avf_u64_t marked_bytes) = 0;
	virtual avf_status_t ClipRemoved(string_t *vdb_id, avf_u32_t clip_type, avf_u32_t clip_id) = 0;
	virtual avf_status_t BufferSpaceLow(string_t *vdb_id) = 0;
	virtual avf_status_t BufferFull(string_t *vdb_id) = 0;
	virtual avf_status_t SendRawData(string_t *vdb_id, raw_data_t *raw) = 0;
	virtual avf_status_t PlaylistCleared(string_t *vdb_id, avf_u32_t list_type) = 0;
	virtual avf_status_t PlaylistCreated(string_t *vdb_id, avf_u32_t list_type) = 0;
	virtual avf_status_t PlaylistDeleted(string_t *vdb_id, avf_u32_t list_type) = 0;
	virtual avf_status_t ClipAttrChanged(string_t *vdb_id, avf_u32_t clip_type, avf_u32_t clip_id, avf_u32_t clip_attr) = 0;
};

#endif

