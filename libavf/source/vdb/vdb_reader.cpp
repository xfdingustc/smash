
#define LOG_TAG "vdb_reader"

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
#include "mpeg_utils.h"

#include "avf_util.h"
#include "mem_stream.h"
#include "mp4_builder.h"

#include "vdb_cmd.h"
#include "vdb_util.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_format.h"
#include "vdb_if.h"
#include "vdb.h"
#include "vdb_server.h"

//-----------------------------------------------------------------------
//
//  CVDB
//
//-----------------------------------------------------------------------

void CVDB::LockClipL(const clip_info_s *clip_info)
{
	vdb_clip_t *clip = FindClipL(clip_info->clip_id);
	if (clip) {
		clip->LockClip(clip_info->clip_time_ms,
			clip_info->clip_time_ms + clip_info->_length_ms);
	}
}

void CVDB::UnlockClipL(const clip_info_s *clip_info)
{
	vdb_clip_t *clip = FindClipL(clip_info->clip_id);
	if (clip) {
		UnlockClipL(clip, clip_info->clip_time_ms,
			clip_info->clip_time_ms + clip_info->_length_ms);
	}
}

void CVDB::UnlockClipL(vdb_clip_t *clip, avf_u64_t start_time_ms, avf_u64_t end_time_ms)
{
	clip->_UnlockClip(start_time_ms, end_time_ms);
	if (clip->IsNotUsed()) {
		m_clip_set.RemoveClip(clip, this);
	}
}

void CVDB::LockClipListL(const cliplist_info_s *cliplist_info)
{
	clip_info_s *clip_info = cliplist_info->elems;
	for (avf_size_t i = cliplist_info->count; i; i--, clip_info++) {
		LockClipL(clip_info);
	}
}

void CVDB::UnlockClipListL(const cliplist_info_s *cliplist_info)
{
	clip_info_s *clip_info = cliplist_info->elems;
	for (avf_size_t i = cliplist_info->count; i; i--, clip_info++) {
		UnlockClipL(clip_info);
	}
}

IClipReader *CVDB::s_CreateClipReader(vdb_clip_id_t vdb_clip_id, int stream,
	avf_u64_t clip_time_ms, avf_u32_t length_ms, avf_u64_t start_offset)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = s_GetRefClipL(vdb_clip_id);
	if (ref_clip == NULL) {
		AVF_LOGE("s_GetRefClipL failed");
		return NULL;
	}

	if (clip_time_ms < ref_clip->start_time_ms || clip_time_ms >= ref_clip->end_time_ms) {
		AVF_LOGE("s_CreateClipReader out of range: " LLD ", " LLD ", " LLD,
			clip_time_ms, ref_clip->start_time_ms, ref_clip->end_time_ms);
		return NULL;
	}

	range_s range;
	clip_info_s clip_info;

	range.clip_type = CLIP_TYPE_BUFFER;
	range.ref_clip_id = ref_clip->ref_clip_id;
	range._stream = stream;
	range.b_playlist = 0;
	range.length_ms = length_ms;
	range.clip_time_ms = clip_time_ms;

	avf_status_t status = GetClipRangeInfoL(ref_clip, &range, &clip_info, 0);
	if (status != E_OK) {
		AVF_LOGE("GetClipRangeInfoFailed");
		return NULL;
	}

	IClipReader *reader = CVideoClipReader::Create(this, &clip_info, NULL, start_offset, false, false, 0);
	if (reader == NULL) {
		AVF_LOGE("CreateClipReader Failed");
	}

	return reader;
}

IClipReader *CVDB::CreateClipReaderOfTime(const range_s *range,
	bool bMuteAudio, bool bFixPTS, avf_u32_t init_time_ms, avf_u32_t upload_opt)
{
	clip_info_s clip_info;
	cliplist_info_s cliplist_info;

	if (GetRangeInfo(range, &clip_info, &cliplist_info, upload_opt) != E_OK)
		return NULL;

	return CreateClipReaderOfAddress(&clip_info, &cliplist_info, 0, bMuteAudio, bFixPTS, init_time_ms, upload_opt);
}

// cliplist_info: no need to free
IClipReader *CVDB::CreateClipReaderOfAddress(const clip_info_s *clip_info,
	cliplist_info_s *cliplist_info, avf_u64_t offset, bool bMuteAudio,
	bool bFixPTS, avf_u32_t init_time_ms, avf_u32_t upload_opt)
{
	AUTO_LOCK(mMutex);

	IClipReader *reader;
	if (upload_opt) {
		reader = CUploadClipReader::Create(this, clip_info, cliplist_info, bMuteAudio, upload_opt);
	} else {
		reader = CVideoClipReader::Create(this, clip_info, cliplist_info, offset, bMuteAudio, bFixPTS, init_time_ms);
	}

	if (reader == NULL && cliplist_info) {
		ReleaseClipListInfo(cliplist_info);
	}

	return reader;
}

CISOReader *CVDB::CreateISOReader(const clip_info_s *clip_info, cliplist_info_s *cliplist_info)
{
	AUTO_LOCK(mMutex);

	CISOReader *reader = CISOReader::Create(this, clip_info, cliplist_info);

	if (reader == NULL && cliplist_info) {
		ReleaseClipListInfo(cliplist_info);
	}

	return reader;
}

IClipData *CVDB::CreateClipDataOfTime(range_s *range)
{
	AUTO_LOCK(mMutex);

	ref_clip_t *ref_clip = FindRefClipL(range->clip_type, range->ref_clip_id);
	if (ref_clip == NULL) {
		AVF_LOGE("CreateClipDataOfTime: clip not found");
		return NULL;
	}

	if (range->clip_time_ms < ref_clip->start_time_ms) {
		AVF_LOGE("invalid clip_time_ms: " LLD, range->clip_time_ms);
		return NULL;
	}

	avf_u64_t end_time_ms = range->clip_time_ms + range->length_ms;
	if (end_time_ms > ref_clip->end_time_ms) {
		AVF_LOGE("invalid length_ms: %d", range->length_ms);
		return NULL;
	}

	// lock
	ref_clip->clip->LockClip(range->clip_time_ms, end_time_ms);

	CClipData *data = CClipData::Create(this, ref_clip->clip);
	data->m_start_time_ms = range->clip_time_ms;
	data->m_end_time_ms = end_time_ms;

	for (unsigned i = 0; i < ARRAY_SIZE(data->m_items); i++) {
		data->m_items[i].Init(data->m_start_time_ms);
	}
	data->m_video_1.Init(data->m_start_time_ms);

	// add to list
	HLIST_INSERT(mpFirstClipData, data);
	data->mbClosed = false;

	return data;
}

void CVDB::CloseClipDataL(CClipData *data)
{
	if (data->mbClosed)
		return;

	avf_safe_release(data->m_io_video);
	avf_safe_release(data->m_io_picture);

	// unlock
	UnlockClipL(data->m_clip, data->m_start_time_ms, data->m_end_time_ms);

	HLIST_REMOVE(data);
	data->mbClosed = true;
}

void CVDB::CloseClipData(CClipData *data)
{
	AUTO_LOCK(mMutex);
	CloseClipDataL(data);
}

avf_status_t CVDB::GotoSegmentPosL(CClipData *data, vdb_seg_t **pseg, 
		avf_u64_t clip_time_ms, int stream, int index, index_pos_t *pos)
{
	if (clip_time_ms >= data->m_end_time_ms)
		return S_END;

	if ((*pseg) == NULL || clip_time_ms >= (*pseg)->GetEndTime_ms()) {
		if ((*pseg = data->m_clip->FindSegmentByTime(stream, clip_time_ms)) == NULL) {
			AVF_LOGE("ReadClipData: no next segment");
			return E_ERROR;
		}
	}

	avf_status_t status;
	if ((status = (*pseg)->FindPos(index, clip_time_ms, pos)) != E_OK) {
		AVF_LOGE("FindPos failed");
		return status;
	}

	return E_OK;
}

// return S_END: end of stream
// data_block is NULL, or avio is NULL
// on success, *pdata should be released by ReleaseData()
// proceed: for pic/raw data. when returns an error, proceed=true indicats that there may be more data next
avf_status_t CVDB::ReadClipData(CClipData *data, IClipData::desc_s *desc, 
	void **pdata, avf_size_t *psize, IAVIO *avio, bool *proceed)
{
	if (pdata) {
		*pdata = NULL;
		*psize = 0;
	}
	*proceed = false;

	int index = get_vdb_array_index(desc->data_type);
	if (index < 0) {
		AVF_LOGE("ReadClipData: unknown data type %d", desc->data_type);
		return E_INVAL;
	}
	if (index != kVideoArray) {
		// picture & raw data are in video stream 0
		desc->stream = 0;
	}

	AUTO_LOCK(mMutex);

	if (data->mbClosed) {
		AVF_LOGD("ReadClipData: closed");
		return E_CLOSE;
	}

	avf_status_t status;
	avf_u64_t curr_time_ms;
	index_pos_t pos;

	if (desc->stream == 0) {
		// video stream 0, and picture/raw
		curr_time_ms = data->m_items[index].curr_time_ms;
	} else {
		// video stream 1
		curr_time_ms = data->m_video_1.curr_time_ms;
	}

	if (index == kVideoArray) {

		vdb_seg_t **pseg = desc->stream == 0 ? &data->m_seg : &data->m_seg_1;
		status = GotoSegmentPosL(data, pseg, curr_time_ms, desc->stream, kVideoArray, &pos);
		if (status != E_OK)
			return status;

		vdb_seg_t *seg = *pseg;

		desc->seg_time_ms = seg->GetStartTime_ms();
		desc->clip_time_ms = seg->GetStartTime_ms() + pos.item->time_ms;
		desc->length_ms = CIndexArray::GetItemDuration(pos.item);
		desc->num_entries = 1;
		desc->total_size = CIndexArray::GetItemSize(pos.item);

		// update
		if (desc->stream == 0) {
			data->m_items[index].curr_time_ms = desc->clip_time_ms + desc->length_ms;
		} else {
			data->m_video_1.curr_time_ms = desc->clip_time_ms + desc->length_ms;
		}

		// fetch data
		status = FetchClipDataL(seg, data, desc, &data->m_io_video, 
			GetVideoFileName(seg), &pos, pdata, psize, avio);

	} else {

		status = GotoSegmentPosL(data, &data->m_seg, curr_time_ms, 0, kVideoArray, &pos);
		if (status != E_OK)
			return status;

		vdb_seg_t *seg = data->m_seg;
		avf_s32_t seg_time_ms = pos.item->time_ms;
		avf_s32_t seg_end_time_ms = (pos.item + 1)->time_ms;

		// for picture & raw data, increase curr_time_ms even an error occurs
		data->m_items[index].curr_time_ms = seg->GetStartTime_ms() + seg_end_time_ms;

		if ((status = seg->FindPosRelative(index, seg_time_ms, &pos)) != E_OK) {
			//AVF_LOGE("FindPosRelative failed");
			*proceed = true;
			return status;
		}

		index_pos_t pos_end;
		if ((status = seg->FindPosRelative(index, seg_end_time_ms - 1, &pos_end)) != E_OK) {
			//AVF_LOGE("FindPosRelative failed");
			*proceed = true;
			return status;
		}

		// adjust start item, it may be already read by previous turn
		if (!data->m_items[index].b_first) {
			while (seg->GetStartTime_ms() + pos.item->time_ms < curr_time_ms) {
				pos.item++;
				if (pos.item > pos_end.item) {
					// nothing to read this turn
					*proceed = true;
					return E_ERROR;
				}
			}
		}

		desc->seg_time_ms = seg->GetStartTime_ms();
		desc->clip_time_ms = seg->GetStartTime_ms() + pos.item->time_ms;
		desc->length_ms = CIndexArray::GetDurationInclusive(pos.item, pos_end.item);
		desc->num_entries = CIndexArray::GetCountInclusive(pos.item, pos_end.item);
		desc->total_size = CIndexArray::GetSizeInclusive(pos.item, pos_end.item);

		// update
		data->m_items[index].curr_time_ms = desc->clip_time_ms + desc->length_ms;
		data->m_items[index].b_first = false;

		// fetch data
		if (index == kPictureArray) {
			status = FetchClipDataL(seg, data, desc, &data->m_io_picture, 
				GetPictureFileName(seg), &pos, pdata, psize, avio);
		} else {
			status = FetchClipDataL(seg, data, desc, NULL, NULL, &pos, pdata, psize, avio);
		}

	}

	return status;
}

void CVDB::fill_upload_header(upload_header_t *header,
	IClipData::desc_s *desc, avf_size_t extra_size,
	const index_item_t *item, const index_item_t *next_item)
{
	header->data_type = desc->data_type;
	header->data_size = extra_size + next_item->fpos - item->fpos;
	header->timestamp = desc->seg_time_ms + item->time_ms;
	header->stream = desc->stream;
	header->duration = next_item->time_ms - item->time_ms;
	header->extra.version = UPLOAD_VERSION_v2;
	header->extra.reserved1 = 0;
	header->extra.reserved2 = 0;
	header->extra.reserved3 = 0;
}

// pavio != NULL: video or picture
avf_status_t CVDB::FetchClipDataL(vdb_seg_t *seg, 
	CClipData *data, IClipData::desc_s *desc,
	IAVIO **pavio, string_t *filename, index_pos_t *pos,
	void **pdata, avf_size_t *psize, IAVIO *avio)
{
	avf_status_t status;

	if (pavio != NULL) {
		// create input avio
		if (*pavio == NULL) {
			if ((*pavio = CSysIO::Create()) == NULL) {
				filename->Release();
				return E_NOMEM;
			}
		}

		// open .ts or .jpg file
		status = (*pavio)->OpenRead(filename->string());
		if (status != E_OK) {
			AVF_LOGE("cannot open %s", filename->string());
		}
		filename->Release();
		if (status != E_OK)
			return status;

		if ((status = (*pavio)->Seek(pos->item->fpos)) != E_OK)
			return status;
	}

	avf_u8_t *mem_base = pos->array->GetType() == CIndexArray::TYPE_INDEX ? 
		NULL : (avf_u8_t*)pos->array->GetMem();
	const index_item_t *item = pos->item;
	const index_item_t *next_item = item + 1;
	upload_header_t *header;

	bool is_video = desc->data_type == VDB_DATA_VIDEO;
	avf_size_t extra_size = is_video ? sizeof(avf_stream_attr_t) : 0;

	if (pdata != NULL) {	// read to memory

		if ((*pdata = data->AllocData(desc, item, psize, extra_size)) == NULL)
			return E_NOMEM;

		header = (upload_header_t*)(*pdata);

		avf_size_t total_size = 0;
		for (unsigned i = 0; i < desc->num_entries; i++) {
			fill_upload_header(header, desc, extra_size, item, next_item);

			avf_u8_t *ptr = (avf_u8_t*)(header + 1);
			avf_size_t remain = header->data_size;
			if (is_video) {
				::memcpy(ptr, &seg->m_info.stream_attr, sizeof(avf_stream_attr_t));
				ptr += sizeof(avf_stream_attr_t);
				remain -= sizeof(avf_stream_attr_t);
			}

			if (pavio != NULL) { // from file to mem
				if ((*pavio)->Read(ptr, remain) != (int)remain) {
					data->ReleaseData(*pdata);
					*pdata = NULL;
					return E_IO;
				}
			} else { // from mem block to mem
				::memcpy(ptr, mem_base + item->fpos, remain);
			}

			total_size += sizeof(upload_header_t) + header->data_size;
			header = go_next_header(header);
			item++;
			next_item++;
		}

		set_upload_end_header(header, total_size);

	} else {	 // read to file

		int rval;
		upload_header_t _header;
		header = &_header;

		avf_size_t total_size = 0;
		for (unsigned i = 0; i < desc->num_entries; i++) {
			fill_upload_header(header, desc, extra_size, item, next_item);
			if (avio->Write(header, sizeof(upload_header_t)) != (int)sizeof(upload_header_t)) {
				return E_IO;
			}

			avf_size_t remain = header->data_size;
			if (is_video) {
				rval = avio->Write(&seg->m_info.stream_attr, sizeof(avf_stream_attr_t));
				if (rval != (int)sizeof(avf_stream_attr_t)) {
					return E_IO;
				}
				remain -= sizeof(avf_stream_attr_t);
			}

			if (pavio != NULL) {	// from file to file
				status = CAVIO::CopyFile(*pavio, remain, avio);
				if (status != E_OK) {
					return status;
				}
			} else {	// from mem block to file
				rval = avio->Write(mem_base + item->fpos, remain);
				if (rval != (int)remain) {
					return E_IO;
				}
			}

			total_size += sizeof(upload_header_t) + header->data_size;
			item++;
			next_item++;
		}

		set_upload_end_header(header, total_size);
		rval = avio->Write(header, sizeof(upload_header_t));
		if (rval != (int)sizeof(upload_header_t)) {
			return E_IO;
		}
	}

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  vdb transfer
//
//-----------------------------------------------------------------------

copy_clip_set_s *CVDB::CreateCopyClipSet()
{
	return avf_new copy_clip_set_s(&m_clip_set, static_cast<IClipOwner*>(this));
}

void CVDB::DestroyCopyClipSet(copy_clip_set_s *cclip_set)
{
	avf_delete cclip_set;
}

int CVDB::InitTransfer(copy_clip_set_s *cclip_set, const vdb_local_item_t *items, int nitems, vdb_local_size_t *size)
{
	AVF_LOGD("InitTransfer: %d items", nitems);

	AUTO_LOCK(mMutex);

	int count = 0;
	for (int i = 0; i < nitems; i++) {
		const vdb_local_item_t *item = items + i;

		ref_clip_t *ref_clip = FindRefClipL(item->clip_type, item->clip_id);
		if (ref_clip == NULL) {
			AVF_LOGE("cannot find ref clip %x of type %d", item->clip_id, item->clip_type);
			return E_INVAL;
		}

		copy_ref_s ref;
		ref.clip_type = item->clip_type;
		ref.clip_date = ref_clip->clip->clip_date;
		ref.clip_id = 0; // fixed in GenerateRefClipIds()
		ref.clip_attr = ref_clip->clip_attr;
		ref.start_time_ms = ref_clip->start_time_ms;
		ref.end_time_ms = ref_clip->end_time_ms;

		ref.scene_data = NULL;
		if ((item->flags & VDB_LOCAL_TRANSFER_SCENE_DATA) && ref_clip->m_scene_data) {
			ref_clip->m_scene_data->AddRef();
			ref.scene_data = ref_clip->m_scene_data;
		}

		int result = cclip_set->AddCopyRef(ref_clip->clip, &ref);
		if (result == 0) {
			count++;
		}
	}

	// calculate total size to copy: picture + video + index
	cclip_set->GetSize(GetClipDir(), &size->picture_size, &size->video_size, &size->index_size);

#ifdef AVF_DEBUG
	copy_clip_s *cclip = cclip_set->elems;
	for (unsigned i = 0; i < cclip_set->count; i++, cclip++) {
		AVF_LOGD("clip %x", cclip->clip->clip_id);
		for (unsigned j = 0; j < 2; j++) {
			AVF_LOGD("  stream %d", j);
			vdb_stream_t *stream = cclip->clip->GetStream(j);
			copy_stream_s *cstream = j == 0 ? &cclip->stream_0 : &cclip->stream_1;
			copy_seg_s *cseg = cstream->elems;
			for (unsigned k = 0; k < cstream->count; k++, cseg++) {
				vdb_seg_t *seg = stream->FindSegmentById(cseg->seg_id);
				AVF_LOGD("    seg %d: (" LLD " - " LLD ")",
					cseg->seg_id, seg->GetStartTime_ms(), seg->GetEndTime_ms());
				copy_range_s *crange = cseg->elems;
				for (unsigned m = 0; m < cseg->count; m++, crange++) {
					AVF_LOGD("      " LLD " - " LLD, crange->start_time_ms, crange->end_time_ms);
				}
			}
		}
	}
#endif

	return count;
}

// TODO: if vdb will be edited, IDs will not be unique
void CVDB::GenerateRefClipIds(copy_clip_set_s *cclip_set)
{
	AUTO_LOCK(mMutex);
	copy_clip_s *cclip = cclip_set->elems;
	for (avf_uint_t i = 0; i < cclip_set->count; i++, cclip++) {
		copy_ref_s *ref = cclip->elems;
		for (avf_uint_t j = 0; j < cclip->count; j++, ref++) {
			ref_clip_set_t *rcs = GetRCS(ref->clip_type);
			cclip_set->GenerateRefClipId(rcs, ref);
		}
	}
}

int CVDB::TransferClipSet(copy_clip_set_s *cclip_set, CVDB *pDestVdb)
{
	pDestVdb->GenerateRefClipIds(cclip_set);

	copy_clip_s *cclip = cclip_set->elems;
	for (avf_uint_t i = 0; i < cclip_set->count; i++, cclip++) {
		int rval = TransferOneClip(cclip_set, cclip, pDestVdb);
		if (rval < 0) {
			return rval;
		}
	}

	return 0;
}

int CVDB::TransferOneClip(copy_clip_set_s *cclip_set, copy_clip_s *cclip, CVDB *pDestVdb)
{
	AVF_LOGD(C_LIGHT_CYAN "transfer clip 0x%x" C_NONE, cclip->clip->clip_id);

	copy_clip_info_s info;
	info.Init();

	int rval;

	// transfer stream 0
	if ((rval = TransferOneStream(cclip_set, cclip, 0, pDestVdb, info)) < 0)
		goto Done;

	// transfer stream 1
	if ((rval = TransferOneStream(cclip_set, cclip, 1, pDestVdb, info)) < 0)
		goto Done;

	// create clipinfo file
	if ((rval = pDestVdb->SaveCopyClipInfo(cclip_set, cclip, info)) != E_OK)
		goto Done;

	// callback
	cclip_set->callback(cclip_set->context, COPY_EVENT_INDEX_COPIED, cclip->clipinfo_filesize);

Done:
	if (rval < 0) {
		AVF_LOGD(C_LIGHT_RED "remove transfered clip files" C_NONE);
		pDestVdb->RemoveTransferedClip(info);
	}
	info.Release();
	return rval;
}

void CVDB::RemoveTransferedClip(copy_clip_info_s& info)
{
	RemoveTransferedStream(info.stream_0, 0);
	RemoveTransferedStream(info.stream_1, 1);
	if (info.clipinfo_name != AVF_INVALID_TIME) {
		local_time_name_t lname;
		local_time_to_name(info.clipinfo_name, CLIPINFO_FILE_EXT, lname);
		ap<string_t> clipinfo_name(GetClipDir(), lname.buf);
		AVF_LOGD("remove %s", clipinfo_name->string());
		avf_remove_file(clipinfo_name->string(), true);
	}
}

void CVDB::RemoveTransferedStream(copy_stream_info_s& stream, int s)
{
	for (avf_uint_t i = 0; i < stream.count; i++) {
		copy_seg_info_s::out_s *segout = stream.elems + i;
		if (segout->index_name != AVF_INVALID_TIME) {
			RemoveIndexFile(NULL, s, segout->index_name);
			segout->index_name = AVF_INVALID_TIME;
		}
		if (segout->video_name != AVF_INVALID_TIME) {
			RemoveVideoFile(NULL, s, segout->video_name, 0);
			segout->video_name = AVF_INVALID_TIME;
		}
		if (segout->picture_name != AVF_INVALID_TIME) {
			RemovePictureFile(NULL, s, segout->picture_name);
			segout->picture_name = AVF_INVALID_TIME;
		}
	}
}

int CVDB::TransferOneStream(copy_clip_set_s *cclip_set, copy_clip_s *cclip, int s,
	CVDB *pDestVdb, copy_clip_info_s& info)
{
	copy_state_s state;
	state.Init(s);

	copy_seg_info_s segi;
	segi.Init();

	copy_stream_s *cstream = cclip->GetStream(s);
	int rval = 0;

	avf_uint_t num_segs = 0;
	for (avf_uint_t i = 0; i < cstream->count; i++) {

		if ((rval = OpenTransferSegment(cclip, s, cstream, i, state, pDestVdb, info)) < 0) {
			return rval;
		}

		if (state.src_seg == NULL) {
			// empty segment
			continue;
		}

		copy_range_s *crange = state.cseg->elems;
		for (avf_uint_t j = 0; j < state.cseg->count; j++, crange++) {

			if ((rval = OpenTransferRange(crange, state, segi, pDestVdb)) < 0)
				goto Done;

			rval = TransferRange(cclip_set, state, segi, crange, pDestVdb, info);
			if (rval >= 0) {
				if (info.AddSegInfo(&segi, s) == NULL) {
					rval = E_ERROR;
				}
			}

			CloseTransferRange(state, segi, rval < 0, pDestVdb);

			if (rval < 0)
				goto Done;

			segi.seg_id++;
		}

		num_segs++;
	}

	AVF_LOGI("transfered %d segments for stream %d", num_segs, s);

Done:
	AUTO_LOCK(mMutex);
	CloseTransferFilesL(state);
	return rval;
}

// ts, jpg, index
int CVDB::TransferRange(copy_clip_set_s *cclip_set, const copy_state_s& state, copy_seg_info_s& segi,
	copy_range_s *crange, CVDB *pDestVdb, copy_clip_info_s& info)
{
	segi.raw_data_size = 0;

	for (int i = 0; i < kAllArrays; i++) {
		if (CaclCopyRange(state, segi, crange, i) < 0)
			return E_ERROR;
	}

	if (CopyFile(cclip_set, state, segi, crange, kVideoArray,
			state.fd_video, segi.fd_video, COPY_EVENT_VIDEO_COPIED) < 0)
		return E_ERROR;

	if (CopyFile(cclip_set, state, segi, crange, kPictureArray,
			state.fd_picture, segi.fd_picture, COPY_EVENT_PICTURE_COPIED) < 0)
		return E_ERROR;

	if (pDestVdb->SaveCopyRangeIndex(state, segi, crange) != E_OK)
		return E_ERROR;

	// callback
	cclip_set->callback(cclip_set->context, COPY_EVENT_INDEX_COPIED, state.cseg->index_size);

	return 0;
}

int CVDB::OpenTransferRange(copy_range_s *crange, const copy_state_s& state, copy_seg_info_s& segi, CVDB *pDestVdb)
{
	avf_time_t gen_time;
	string_t *filename;

	vdb_seg_t *seg = state.src_seg;
	int seg_off = (crange->start_time_ms - seg->GetStartTime_ms()) / 1000;

	// index
	gen_time = avf_time_add(seg->m_index_name, seg_off);
	filename = pDestVdb->CreateIndexFileName(&gen_time, state.stream, true, 0);
	if (filename == NULL)
		goto Fail;
	segi.out.index_name = gen_time;
	filename->Release();

	// video
	gen_time = avf_time_add(seg->m_video_name, seg_off);
	filename = pDestVdb->CreateVideoFileName(&gen_time, state.stream, true, 0);
	if (filename == NULL)
		goto Fail;
	segi.out.video_name = gen_time;

	AVF_LOGD("create %s", filename->string());
	segi.fd_video = avf_open_file(filename->string(), O_RDWR|O_CREAT|O_TRUNC, 0);
	filename->Release();
	if (segi.fd_video < 0)
		goto Fail;

	// picture
	if (seg->m_picture_name != AVF_INVALID_TIME) {
		gen_time = avf_time_add(seg->m_picture_name, seg_off);
		filename = pDestVdb->CreatePictureFileName(&gen_time, state.stream, true, 0);
		if (filename == NULL)
			goto Fail;
		segi.out.picture_name = gen_time;

		AVF_LOGD("create %s", filename->string());
		segi.fd_picture = avf_open_file(filename->string(), O_RDWR|O_CREAT|O_TRUNC, 0);
		filename->Release();
		if (segi.fd_picture < 0)
			goto Fail;
	}

	return 0;

Fail:
	CloseTransferRange(state, segi, true, pDestVdb);
	return E_ERROR;
}

void CVDB::CloseTransferRange(const copy_state_s& state, copy_seg_info_s& segi, bool bRemoveFile, CVDB *pDestVdb)
{
	avf_safe_close(segi.fd_video);
	avf_safe_close(segi.fd_picture);

	if (bRemoveFile) {
		if (segi.out.index_name != AVF_INVALID_TIME) {
			pDestVdb->RemoveIndexFile(NULL, state.stream, segi.out.index_name);
		}
		if (segi.out.video_name != AVF_INVALID_TIME) {
			pDestVdb->RemoveVideoFile(NULL, state.stream, segi.out.video_name, 0);
		}
		if (segi.out.picture_name != AVF_INVALID_TIME) {
			pDestVdb->RemovePictureFile(NULL, state.stream, segi.out.picture_name);
		}
	}

	segi.out.index_name = AVF_INVALID_TIME;
	segi.out.video_name = AVF_INVALID_TIME;
	segi.out.picture_name = AVF_INVALID_TIME;
}

int CVDB::OpenTransferSegment(copy_clip_s *cclip, int s, copy_stream_s *cstream, avf_uint_t seg_index,
	copy_state_s& state, CVDB *pDestVdb, copy_clip_info_s& info)
{
	AUTO_LOCK(mMutex);

	CloseTransferFilesL(state);

	// get the seg
	vdb_stream_t *stream = cclip->clip->GetStream(s);
	if (stream == NULL)
		return E_ERROR;

	copy_seg_s *cseg = cstream->elems + seg_index;
	vdb_seg_t *seg = stream->FindSegmentById(cseg->seg_id);
	if (seg == NULL)
		return E_ERROR;

	if (seg->GetDuration_ms() == 0) {
		// this tail segment is empty, ignore it
		return E_OK;
	}

	if (seg->LoadSegCache() == NULL)
		return E_ERROR;

	LockSegCache(seg);
	state.src_seg = seg;
	state.cseg = cseg;

	string_t *filename;

	// ----------------------------------
	// open files
	// ----------------------------------

	filename = GetVideoFileName(seg);

	AVF_LOGD("open %s", filename->string());
	state.fd_video = avf_open_file(filename->string(), O_RDONLY, 0);
	filename->Release();
	if (state.fd_video < 0) {
		goto Fail;
	}

	if (seg->m_picture_name != AVF_INVALID_TIME) {
		filename = GetPictureFileName(seg);
		AVF_LOGD("open %s", filename->string());
		state.fd_picture = avf_open_file(filename->string(), O_RDONLY, 0);
		filename->Release();
		if (state.fd_picture < 0) {
			goto Fail;
		}
	}

	return 0;

Fail:
	CloseTransferFilesL(state);
	return E_ERROR;
}

void CVDB::CloseTransferFilesL(copy_state_s& state)
{
	if (state.src_seg) {
		UnlockSegCache(state.src_seg);
	}

	state.src_seg = NULL;
	state.cseg = NULL;

	avf_safe_close(state.fd_video);
	avf_safe_close(state.fd_picture);
}

avf_status_t CVDB::SaveCopyRangeIndex(const copy_state_s& state, copy_seg_info_s& segi, copy_range_s *crange)
{
	AUTO_LOCK(mMutex);

	segi.out.seg_info = state.src_seg->m_info;

	segi.out.seg_info.id = segi.seg_id;
	segi.out.seg_info.duration_ms = segi.seg_length_ms;
	segi.out.seg_info.start_time_ms = state.src_seg->GetStartTime_ms() + segi.seg_off_ms;
	segi.out.seg_info.v_start_addr = state.src_seg->GetStartAddr() + segi.seg_v_addr_off;
	segi.out.seg_info.raw_data_size = segi.raw_data_size;
	for (int i = 0; i < kTotalArray; i++) {
		segi.out.seg_info.seg_data_size[i] = segi.range[i].size;
	}

	string_t *filename = m_pf_index.GetFileName(NULL, state.stream, segi.out.index_name, 0);
	avf_status_t status = CVdbFormat::SaveSegmentIndex(state.src_seg->m_clip, filename, GetTempIO(),
		state.stream, &state.src_seg->m_cache->sindex, &segi);
	filename->Release();

	return status;
}

int CVDB::CaclCopyRange(const copy_state_s& state, copy_seg_info_s& segi, copy_range_s *crange, int index)
{
	copy_seg_info_s::index_range_s *r = segi.range + index;

	CIndexArray **arrays = state.src_seg->m_cache->sindex.arrays;
	if (arrays[index] == NULL) {
		r->size = 0;
		r->from = 0;
		r->to = 0;
		return E_OK;
	}

	index_pos_t spos;
	if (state.src_seg->FindPos(index, crange->start_time_ms, &spos) != E_OK)
		return E_ERROR;

	index_pos_t epos;
	if (state.src_seg->FindPos(index, crange->end_time_ms - 1, &epos) != E_OK)
		return E_ERROR;

	if (index == kVideoArray) {
		segi.seg_off_ms = spos.item->time_ms;
		segi.seg_length_ms = (epos.item + 1)->time_ms - spos.item->time_ms;
		segi.seg_v_addr_off = spos.item->fpos;
	}

	r->size = (epos.item + 1)->fpos - spos.item->fpos;
	r->from = spos.array->GetItemIndex(spos.item);
	r->to = epos.array->GetItemIndex(epos.item) + 1;

	if (index != kVideoArray && index != kPictureArray) {
		segi.raw_data_size += r->size + (epos.item - spos.item + 1) * sizeof(index_item_t);
	}

	return E_OK;
}

int CVDB::CopyFile(copy_clip_set_s *cclip_set, const copy_state_s& state, copy_seg_info_s& segi,
	copy_range_s *crange, int index, int fd_from, int fd_to, int event)
{
	if (fd_from < 0 || fd_to < 0) {
		return E_OK;
	}

	CIndexArray **arrays = state.src_seg->m_cache->sindex.arrays;
	avf_u32_t pos = arrays[index]->GetItemAt(segi.range[index].from)->fpos;
	avf_u32_t size = arrays[index]->GetItemAt(segi.range[index].to)->fpos - pos;

	if (::lseek(fd_from, pos, SEEK_SET) < 0) {
		AVF_LOGE("lseek failed");
		return E_ERROR;
	}

	avf_u8_t *buffer = avf_malloc_bytes(COPY_BUF_SZ);
	if (buffer == NULL)
		return E_NOMEM;

	while (size > 0) {
		if (cclip_set->b_cancel) {
			AVF_LOGW("copy file cancelled");
			goto Error;
		}

		avf_uint_t tocopy = MIN(size, COPY_BUF_SZ);

		int nread = ::read(fd_from, buffer, tocopy);
		if (nread != (int)tocopy) {
			AVF_LOGE("read file failed, tocopy: %d, nread: %d", tocopy, nread);
			goto Error;
		}

		int nwrite = ::write(fd_to, buffer, tocopy);
		if (nwrite != (int)tocopy) {
			AVF_LOGE("write file failed, tocopy: %d, nwrite: %d", tocopy, nwrite);
			goto Error;
		}

		// callback
		cclip_set->callback(cclip_set->context, event, (avf_intptr_t)tocopy);

		size -= tocopy;
	}

	avf_free(buffer);
	return 0;

Error:
	avf_free(buffer);
	return E_ERROR;
}

avf_status_t CVDB::SaveCopyClipInfo(copy_clip_set_s *cclip_set, copy_clip_s *cclip, copy_clip_info_s& info)
{
	AUTO_LOCK(mMutex);

	avf_time_t gen_time = cclip->clip->clip_id;
	info.clipinfo_name = GenerateClipNameL(NULL, &gen_time);

	avf_status_t status = CVdbFormat::SaveCopyClipInfo(GetClipDir(), cclip, info.clipinfo_name, info, GetTempIO());
	if (status != E_OK) {
		RemoveClipNameL(NULL, info.clipinfo_name);
		return status;
	}

	// load the clip into vdb

	vdb_clip_t *clip;

	IAVIO *io = CSysIO::Create();
	status = LoadClipL(NULL, info.clipinfo_name, m_clipinfo_path.get(), io, true, &clip);
	io->Release();

	if (status != E_OK) {
		return status;
	}

	CollectRefClipsL(clip);

	cclip_set->callback(cclip_set->context, COPY_EVENT_CLIP_TRANSFERED, (avf_intptr_t)cclip);

	if (mpBroadcast) {
		list_head_t *node;
		list_head_t *next;
		list_for_each_del(node, next, clip->ref_clip_list) {
			ref_clip_t *ref_clip = ref_clip_t::from_clip_node(node);
			mpBroadcast->ClipInfo(mpVdbId, CLIP_ACTION_CREATED, ref_clip, NULL);
		}
	}

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CVdbReader
//
//-----------------------------------------------------------------------

// called by vdb when mMutex is locked
CVdbReader::CVdbReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info):
	mpVDB(pVDB),
	mbClosed(true)
{
	// lock
	if (clip_info->b_cliplist) {
		mpVDB->LockClipListL(cliplist_info);
	} else {
		mpVDB->LockClipL(clip_info);
	}

	m_clip_info = *clip_info;
	if (clip_info->b_cliplist) {
		m_cliplist_info = *cliplist_info;
	} else {
		mpVDB->InitClipListInfo(&m_cliplist_info);
	}

	HLIST_INSERT(mpVDB->mpFirstReader, this);
	mbClosed = false;
}

CVdbReader::~CVdbReader()
{
}

void CVdbReader::Release()
{
	if (avf_atomic_dec(&m_ref_count) == 1) {
		AUTO_LOCK(mpVDB->mMutex);
		CloseL();
		avf_delete this;
	}
}

// called by vdb/user
void CVdbReader::CloseL()
{
	if (mbClosed)
		return;

	OnClose();

	// unlock
	if (IsClipList()) {
		mpVDB->UnlockClipListL(&m_cliplist_info);
		mpVDB->ReleaseClipListInfo(&m_cliplist_info);
	} else {
		mpVDB->UnlockClipL(&m_clip_info);
	}

	HLIST_REMOVE(this);
	mbClosed = true;
}

//-----------------------------------------------------------------------
//
//  CClipReader
//
//-----------------------------------------------------------------------

CClipReader::CClipReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info, bool bMuteAudio,
		bool bFixPTS, avf_u32_t init_time_ms):
	inherited(pVDB, clip_info, cliplist_info),
	mbMuteAudio(bMuteAudio),
	mbFixPTS(bFixPTS),
	m_init_time_ms(init_time_ms)
{
	if (clip_info->b_cliplist) {
		m_start_ms = clip_info->clip_time_ms + 3 * 1000;
		mbFixPTS = true;	// always fix pts for cliplist
	} else {
		m_start_ms = 3 * 1000;
	}
}

CClipReader::~CClipReader()
{
}

void *CClipReader::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IClipReader)
		return static_cast<IClipReader*>(this);
	if (refiid == IID_ISeekToTime)
		return static_cast<ISeekToTime*>(this);
	return inherited::GetInterface(refiid);
}

int CClipReader::ReadClip(avf_u8_t *buffer, avf_size_t size)
{
	AUTO_LOCK(mpVDB->mMutex);
	if (mbClosed) {
		AVF_LOGW("ReadClip: was closed");
		return E_CLOSE;
	}
	return ReadClipL(buffer, size);
}

avf_status_t CClipReader::Seek(avf_s64_t offset, int whence)
{
	AUTO_LOCK(mpVDB->mMutex);
	if (mbClosed) {
		AVF_LOGW("Seek: was closed");
		return E_CLOSE;
	}
	return SeekToAddrL(offset, whence);
}

avf_status_t CClipReader::SeekToTime(avf_u32_t time_ms)
{
	AUTO_LOCK(mpVDB->mMutex);
	if (mbClosed) {
		AVF_LOGW("SeekToTime: was closed");
		return E_CLOSE;
	}
	return SeekToTimeL(time_ms);
}

CClipReader::clip_info_s *CClipReader::FindClipInfoByAddrL(const cliplist_info_s *cliplist_info,
	avf_u64_t offset, avf_u64_t *cliplist_offset, avf_u32_t *start_time_ms)
{
	avf_u64_t size = 0;
	avf_u32_t length = 0;
	clip_info_s *clip_info = cliplist_info->elems;
	for (avf_size_t i = cliplist_info->count; i; i--, clip_info++) {
		avf_u64_t next_size = size + clip_info->v_size[0];
		if (offset < next_size) {
			*cliplist_offset = size;
			*start_time_ms = length;
			return clip_info;
		}
		length += clip_info->_length_ms;
		size = next_size;
	}
	return NULL;
}

CClipReader::clip_info_s *CClipReader::FindClipInfoByTimeL(const cliplist_info_s *cliplist_info,
	avf_u32_t time_ms, avf_u64_t *cliplist_offset, avf_u32_t *start_time_ms)
{
	avf_u64_t size = 0;
	avf_u32_t length = 0;
	clip_info_s *clip_info = cliplist_info->elems;
	for (avf_size_t i = cliplist_info->count; i; i--, clip_info++) {
		avf_u32_t next_length = length + clip_info->_length_ms;
		if (time_ms < next_length) {
			*cliplist_offset = size;
			*start_time_ms = length;
			return clip_info;
		}
		length = next_length;
		size += clip_info->v_size[0];
	}
	return NULL;
}

avf_status_t CClipReader::SeekClipListToAddrL(const cliplist_info_s *cliplist_info,
	avf_u64_t offset, clip_pos_s& cpos)
{
	avf_u64_t cliplist_offset;
	avf_u32_t cliplist_time_ms;
	clip_info_s *clip_info;

	clip_info = FindClipInfoByAddrL(cliplist_info, offset, &cliplist_offset, &cliplist_time_ms);
	if (clip_info == NULL) {
		AVF_LOGW("SeekClipListToAddrL failed, offset: " LLD, offset);
		return E_ERROR;
	}

	clip_pos_s tmp;
	avf_u64_t clip_offset = offset - cliplist_offset;
	avf_status_t status = SeekClipToAddrL(clip_info, clip_offset, tmp);
	if (status != E_OK)
		return status;

	m_clipinfo_start_ms = cliplist_time_ms + m_init_time_ms;
	m_ref_clip_start_ms = clip_info->clip_time_ms;

	cpos.seg = tmp.seg;
	cpos.seg_start_ms = tmp.seg->GetStartTime_ms() - clip_info->clip_time_ms;

	cpos.clip_remain = tmp.clip_remain;
	cpos.seg_offset = tmp.seg_offset;
	cpos.seg_remain = tmp.seg_remain;

	cpos.curr_pos = offset;

	return E_OK;
}

avf_status_t CClipReader::SeekClipListToTimeL(const cliplist_info_s *cliplist_info,
	avf_u32_t time_ms, clip_pos_s& cpos, int stream, index_pos_t *ipos)
{
	avf_u64_t cliplist_offset;
	avf_u32_t cliplist_time_ms;
	clip_info_s *clip_info;

	clip_info = FindClipInfoByTimeL(cliplist_info, time_ms, &cliplist_offset, &cliplist_time_ms);
	if (clip_info == NULL) {
		AVF_LOGD("SeekClipListToTimeL %d out of range", time_ms);
		return E_ERROR;
	}

	clip_pos_s tmp;
	avf_u32_t clip_time_ms = time_ms - cliplist_time_ms;
	avf_status_t status = SeekClipToTimeL(clip_info, clip_time_ms, tmp, stream, ipos);
	if (status != E_OK)
		return status;

	m_clipinfo_start_ms = cliplist_time_ms + m_init_time_ms;
//	m_ref_clip_start_ms = tmp.ref_clip_start_ms;

	cpos.seg = tmp.seg;
	cpos.seg_start_ms = tmp.seg->GetStartTime_ms() - clip_info->clip_time_ms;

	cpos.clip_remain = tmp.clip_remain;
	cpos.seg_offset = tmp.seg_offset;
	cpos.seg_remain = tmp.seg_remain;

	cpos.curr_pos = cliplist_offset + tmp.curr_pos;

	return E_OK;
}

avf_status_t CClipReader::SeekClipToAddrL(const clip_info_s *clip_info,
	avf_u64_t offset, clip_pos_s& cpos)
{
	vdb_clip_t *clip;

	if ((clip = mpVDB->FindClipL(clip_info->clip_id)) == NULL) {
		AVF_LOGD("SeekClipToAddrL: ref clip not found");
		return E_ERROR;
	}

	avf_u64_t clip_start_addr = clip_info->clip_addr[0] + offset;
	if ((cpos.seg = clip->FindSegmentByAddr(clip_info->stream, clip_start_addr)) == NULL) {
		AVF_LOGD("SeekClipToAddrL: seg not found");
		return E_ERROR;
	}

	m_clipinfo_start_ms = m_init_time_ms;
	m_ref_clip_start_ms = clip_info->clip_time_ms;

	cpos.curr_pos = offset;

	cpos.clip_remain = clip_info->v_size[0] - offset;
	cpos.seg_offset = clip_start_addr - cpos.seg->GetStartAddr();
	cpos.seg_remain = cpos.seg->GetVideoDataSize() - cpos.seg_offset;
	if (cpos.seg_remain > cpos.clip_remain)
		cpos.seg_remain = cpos.clip_remain;

	return E_OK;
}

avf_status_t CClipReader::SeekClipToTimeL(const clip_info_s *clip_info,
	avf_u32_t time_ms, clip_pos_s& cpos, int stream, index_pos_t *ipos)
{
	vdb_clip_t *clip;

	if ((clip = mpVDB->FindClipL(clip_info->clip_id)) == NULL) {
		AVF_LOGD("SeekClipToTimeL: clip not found");
		return E_ERROR;
	}

	avf_u64_t clip_time_ms = clip_info->clip_time_ms;
	avf_u64_t abs_time_ms = clip_time_ms + time_ms;

	int istream = 0;
	if (stream < 0) {
		stream = clip_info->stream;
	} else {
		istream = stream ? 1 : 0;
	}

	if ((cpos.seg = clip->FindSegmentByTime(stream, abs_time_ms)) == NULL) {
		AVF_LOGD("SeekClipToTimeL: seg not found");
		return E_ERROR;
	}

	cpos.seg_start_ms = cpos.seg->GetStartTime_ms() - clip_info->clip_time_ms;

	index_pos_t tmp;
	avf_status_t status = cpos.seg->FindPos(kVideoArray, abs_time_ms, &tmp);
	if (status != E_OK) {
		AVF_LOGD("SeekClipToTimeL failed");
		return status;
	}

	if (ipos) {
		*ipos = tmp;
	}

	m_clipinfo_start_ms = m_init_time_ms;
	m_ref_clip_start_ms = clip_info->clip_time_ms;

	cpos.curr_pos = cpos.seg->GetStartAddr() + tmp.item->fpos - clip_info->clip_addr[istream];

	cpos.clip_remain = clip_info->v_size[istream] - cpos.curr_pos;
	cpos.seg_offset = tmp.item->fpos;
	cpos.seg_remain = cpos.seg->GetVideoDataSize() - cpos.seg_offset;
	if (cpos.seg_remain > cpos.clip_remain)
		cpos.seg_remain = cpos.clip_remain;

	return E_OK;
}

avf_status_t CClipReader::ReadIO(IAVIO *io, avf_u8_t *buffer, avf_size_t size)
{
	int rval = io->Read(buffer, size);
	if (rval != (int)size) {
		AVF_LOGE("ReadIO() failed, size: %d, return: %d", rval, size);
		AVF_LOGE("file size: " LLD ", file offset: " LLD, io->GetSize(), io->GetPos());
		return E_IO;
	}
	return E_OK;
}

void CClipReader::FindAudio(IAVIO *io)
{
	m_pmt_pid = -1U;
	for (avf_int_t i = 0; i < 32; i++) {
		avf_u8_t buffer[PACKET_SIZE];
		avf_u8_t *ptr = buffer;
		if (io->Read(buffer, sizeof(buffer)) != (int)sizeof(buffer)) {
			AVF_LOGW("read failed");
			return;
		}
		avf_uint_t pid = ((ptr[1] << 8) | ptr[2]) & 0x1FFF;
		if (pid == 0) {
			ptr += 4;
			ptr += ptr[0] + 1;	// skip pointer_field
			ptr += 8 + 2;		// to items, skip program_number
			m_pmt_pid = load_be_16(ptr) & 0x1FFF;
			break;
		}
	}
	if (m_pmt_pid == -1U) {
		AVF_LOGW("no pat found");
		//return E_ERROR;
	}
}

void CClipReader::MuteAudio(avf_u8_t *buffer, avf_u32_t bytes, avf_uint_t pmt_pid)
{
	avf_size_t num = bytes / PACKET_SIZE;
	for (; num; num--, buffer += PACKET_SIZE) {
		avf_u8_t *p = buffer;
		if (p[0] != 0x47) {
			AVF_LOGD("no sync byte 0x%x", p[0]);
			continue;
		}

		avf_uint_t pid = ((p[1] << 8) | p[2]) & 0x1FFF;
		if (pid == pmt_pid) {
			avf_size_t section_remain;
			avf_size_t pg_info_len;

			p += 4; 	// skip packet header
			p += p[0] + 1;	// skip pointer_field

			if (p[0] != 2)
				continue;

			avf_u8_t *ptr_start = p;

			section_remain = load_be_16(p + 1) & 0x3FF;
			p += 10;	// to program_info_length
			if ((section_remain >= 13) && PACKET_SIZE - (p - buffer) >= (int)section_remain) {

				section_remain -= 13;

				// skip descriptors
				pg_info_len = load_be_16(p) & 0x0FFF;
				p += 2 + pg_info_len;
				section_remain -= pg_info_len;

				if (section_remain > 0) {
					while (true) {
						avf_uint_t stream_type = p[0];
						if (stream_type == 0x0F) {
							//AVF_LOGD("change stream type at offset %d", p - buffer);
							p[0] = 0;
						}

						avf_uint_t es_info_len = load_be_16(p + 3) & 0x0FFF;
						p += 5;
						p += es_info_len;
						avf_uint_t tmp = 5 + es_info_len;
						if (section_remain <= tmp)
							break;
						section_remain -= tmp;
					}

					avf_u32_t crc = avf_crc_ieee(-1, ptr_start, p - ptr_start);
					//AVF_LOGD("change crc at %d", p - buffer);
					SAVE_LE_32(p, crc);
				}
			}
		}
	}
}

void CClipReader::FixPTS(avf_u8_t *buffer, avf_u32_t bytes)
{
	avf_size_t num = bytes / PACKET_SIZE;
	for (; num; num--, buffer += PACKET_SIZE) {
		avf_u8_t *p = buffer;
		if (p[0] != 0x47) {
			AVF_LOGD("no sync byte 0x%x", p[0]);
			continue;
		}

		// PUSI & pid
		if (p[1] & 0x40) {	// PUSI
			int _payload_type = p[3];
			p += 4;

			if (_payload_type & 0x20) { // adaptation
				//int adap_len = p[0];
				int flags = p[1];
				if (flags & 0x10) { // PCR_flag
					FixPCR(p + 2);
				}
				p += p[0] + 1;
			}

			if (_payload_type & 0x10) { // payload
				FixPES(p);
			}
		}
	}
}

void CClipReader::FixPCR(avf_u8_t *p)
{
	avf_u32_t pcr_high = p[0];	// pcr[32..25], 8 bits
	avf_u32_t pcr_low = load_be_32(p + 1) >> 7; // pcr[24..0], 25 bits
	avf_u64_t pcr = (((avf_pts_t)pcr_high << 25) | pcr_low);

	pcr = fix_pts(pcr);

	avf_u32_t tmp32 = (avf_u32_t)(pcr >> 1);
	SAVE_BE_32(p, tmp32);
	tmp32 = (((avf_uint_t)pcr & 1) << 15) | (0x3F<<9) | 0;
	SAVE_BE_16(p, tmp32);
}

void CClipReader::FixPES(avf_u8_t *p)
{
	avf_u32_t tmp32;

	tmp32 = (p[0] << 16) | (p[1] << 8) | (p[2]);
	if (tmp32 != 0x000001) {
		// should be PAT/PMT
		return;
	}

	avf_u32_t pts_dts = p[7] >> 6;
	if (pts_dts >= 2) {
		p += 9;

		avf_u64_t pts = read_pts(p);
		pts = fix_pts(pts);
		write_pts(p, pts, pts_dts);
		p += 5;

		if (pts_dts == 3) {
			avf_u64_t dts = read_pts(p);
			dts = fix_pts(dts);
			write_pts(p, dts, 1);
		}
	}
}

avf_u64_t CClipReader::fix_pts(avf_u64_t pts)
{
	// relative to start of clipinfo
	pts -= m_ref_clip_start_ms * 90;
	// relative to start of playlist
	pts += (m_clipinfo_start_ms + m_start_ms) * 90;	// **
	return pts;
}

avf_u64_t CClipReader::read_pts(const avf_u8_t *p)
{
	avf_u32_t t1 = (*p >> 1) & 0x07;	// 32..30
	p++;

	avf_u32_t t2 = load_be_16(p) >> 1;	// 29..15
	p += 2;

	avf_u32_t t3 = load_be_16(p) >> 1;	// 14..0
	p += 2;

	return ((avf_pts_t)t1 << 30) | (t2 << 15) | t3;
}

void CClipReader::write_pts(avf_u8_t *p, avf_u64_t pts, int pts_dts)
{
	avf_u32_t pts_hi = (avf_u32_t)(pts >> 32) & 1;
	avf_u32_t pts_lo = (avf_u32_t)pts;
	avf_u32_t tmp32;

	*p++ = (pts_dts << 4) | (pts_hi << 3) | ((pts_lo >> 30) << 1) | 1;

	tmp32 = (((pts_lo >> 15) & 0x7FFF) << 1) | 1;
	SAVE_BE_16(p, tmp32);

	tmp32 = ((pts_lo & 0x7FFF) << 1) | 1;
	SAVE_BE_16(p, tmp32);
}

//-----------------------------------------------------------------------
//
//  CVideoClipReader - for read video stream
//
//-----------------------------------------------------------------------

// called by vdb when mMutex is locked
CVideoClipReader *CVideoClipReader::Create(CVDB *pVDB, const clip_info_s *clip_info,
	const cliplist_info_s *cliplist_info, avf_u64_t offset, bool bMuteAudio,
	bool bFixPTS, avf_u32_t init_time_ms)
{
	CVideoClipReader *result = avf_new CVideoClipReader(pVDB,
		clip_info, cliplist_info, offset, bMuteAudio, bFixPTS, init_time_ms);
	if (result && result->Status() != E_OK) {
		result->CloseL();
		avf_safe_delete(result);
	}
	return result;
}

CVideoClipReader::CVideoClipReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info, avf_u64_t offset, bool bMuteAudio,
		bool bFixPTS, avf_u32_t init_time_ms):
	inherited(pVDB, clip_info, cliplist_info, bMuteAudio, bFixPTS, init_time_ms),
	mp_io(NULL)
{
	avf_status_t status;

	if (IsClipList()) {
		AVF_LOGD("create CVideoClipReader for cliplist, offset: " LLD, offset);
		status = SeekClipListToAddrL(cliplist_info, offset, mpos);
	} else {
		AVF_LOGD("create CVideoClipReader for clip, offset: " LLD, offset);
		status = SeekClipToAddrL(clip_info, offset, mpos);
	}

	if (status != E_OK) {
		mStatus = status;
		return;
	}
}

void CVideoClipReader::OnClose()
{
	avf_safe_release(mp_io);
}

int CVideoClipReader::ReadClipL(avf_u8_t *buffer, avf_size_t size)
{
	avf_u32_t bytes_read = 0;
	avf_status_t status;

	size -= size % PACKET_SIZE;

	while (size > 0) {

		// open segment
		if (mp_io == NULL || !mp_io->IsOpen()) {
			if (mpos.seg == NULL) {
				AVF_LOGW("ReadClip: EOF reached");
				break;
			}
			if ((status = OpenSegment()) != E_OK)
				return status;
		}

		// read
		avf_size_t toread = MIN(size, mpos.seg_remain);
		if (toread > 0) {
			if ((status = ReadIO(mp_io, buffer, toread)) != E_OK) {
				AVF_LOGE("read video failed");
				return status;
			}

			if (mbMuteAudio) {
				MuteAudio(buffer, toread, m_pmt_pid);
			}

			if (ShouldFixPts()) {
				FixPTS(buffer, toread);
			}

			buffer += toread;
			size -= toread;
			bytes_read += toread;

			mpos.seg_offset += toread;
			mpos.seg_remain -= toread;
			mpos.clip_remain -= toread;
			mpos.curr_pos += toread;
		}

		// refill
		if (mpos.seg_remain == 0) {

			AVF_LOGD("close segment");
			mp_io->Close();

			if (mpos.clip_remain > 0) {

				mpos.seg = mpos.seg->GetNextSegInRefClip();
				if (mpos.seg == NULL) {
					AVF_LOGE("ReadClip: no more segment, bytes left " LLD, mpos.clip_remain);
					return E_NOENT;
				}

				mpos.seg_offset = 0;
				mpos.seg_remain = mpos.seg->GetVideoDataSize();
				if (mpos.seg_remain > mpos.clip_remain)
					mpos.seg_remain = mpos.clip_remain;

			} else {

				mpos.seg = NULL;

				// make pts contiguous (buffer should be packet-aligned)
				if (IsClipList()) {
					if (mpos.curr_pos < m_clip_info.v_size[0]) {
						clip_pos_s next_pos;
						if (SeekClipListToAddrL(&m_cliplist_info, mpos.curr_pos, next_pos) == E_OK) {
							mpos = next_pos;
						}
					}
				}

			}
		}
	}

	return (int)bytes_read;
}

avf_status_t CVideoClipReader::SeekToAddrL(avf_s64_t offset, int whence)
{
	avf_u64_t new_offset;

	switch (whence) {
	case IAVIO::kSeekSet:
		new_offset = offset;
		break;

	case IAVIO::kSeekCur:
		new_offset = mpos.curr_pos + offset;
		break;

	case IAVIO::kSeekEnd:
		new_offset = m_clip_info.v_size[0] + offset;
		break;

	default:
		AVF_LOGD("SeekToAddrL: unknown position");
		return E_ERROR;
	}

	if (new_offset > m_clip_info.v_size[0]) {
		AVF_LOGD("SeekToAddrL: out of range");
		return E_ERROR;
	}

	clip_pos_s pos;
	avf_status_t status;

	if (IsClipList()) {
		status = SeekClipListToAddrL(&m_cliplist_info, new_offset, pos);
	} else {
		status = SeekClipToAddrL(&m_clip_info, new_offset, pos);
	}

	if (status != E_OK)
		return status;

	return SetSegment(pos);
}

avf_status_t CVideoClipReader::SeekToTimeL(avf_u32_t time_ms)
{
	if (time_ms > m_clip_info._length_ms) {
		AVF_LOGD("SeekToTime: bad time");
		return E_ERROR;
	}

	clip_pos_s pos;
	avf_status_t status;

	if (IsClipList()) {
		status = SeekClipListToTimeL(&m_cliplist_info, time_ms, pos);
	} else {
		status = SeekClipToTimeL(&m_clip_info, time_ms, pos);
	}

	if (status != E_OK)
		return status;

	return SetSegment(pos);
}

avf_status_t CVideoClipReader::OpenSegment()
{
	avf_status_t status = mpVDB->OpenSegmentVideo(mpos.seg, &mp_io);
	if (status != E_OK)
		return status;

	if (mbMuteAudio) {
		FindAudio(mp_io);
	}

	if ((status = mp_io->Seek(mpos.seg_offset)) != E_OK)
		return status;

	return E_OK;
}

avf_status_t CVideoClipReader::SetSegment(clip_pos_s& pos)
{
	if (pos.seg == mpos.seg) {
		// same segment
		if (mp_io->IsOpen()) {
			avf_status_t status = mp_io->Seek(pos.seg_offset);
			if (status != E_OK)
				return status;
		}
	} else {
		// another segment, will be opened later
		mp_io->Close();
	}

	mpos = pos;
	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CUploadClipReader - for upload
//
//-----------------------------------------------------------------------

// called by vdb when mMutex is locked
CUploadClipReader *CUploadClipReader::Create(CVDB *pVDB, const clip_info_s *clip_info,
	const cliplist_info_s *cliplist_info, bool bMuteAudio, avf_u32_t upload_opt)
{
	CUploadClipReader *result = avf_new CUploadClipReader(pVDB,
		clip_info, cliplist_info, bMuteAudio, upload_opt);
	if (result && result->Status() != E_OK) {
		result->CloseL();
		avf_safe_delete(result);
	}
	return result;
}

CUploadClipReader::CUploadClipReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info, bool bMuteAudio, avf_u32_t upload_opt):
	inherited(pVDB, clip_info, cliplist_info, bMuteAudio, false, 0),
	mState(STATE_V0),
	m_upload_opt(upload_opt),
	m_uploading(upload_opt),
	m_item_bytes(0),
	m_bytes_read(0),
	mp_pic_io(NULL)
{
	v0.Init(STREAM_MAIN, UPLOAD_GET_V0);
	v1.Init(STREAM_SUB_1, UPLOAD_GET_V1);

	data[DATA_PIC].Init(VDB_DATA_JPEG, STATE_GPS);
	data[DATA_GPS].Init(RAW_DATA_GPS, STATE_OBD);
	data[DATA_OBD].Init(RAW_DATA_OBD, STATE_ACC);
	data[DATA_ACC].Init(RAW_DATA_ACC, STATE_V1);

	if ((mStatus = Read_Init()) != E_OK)
		return;

	v0.b_end = (m_uploading & UPLOAD_GET_STREAM_0) == 0;
	v1.b_end = (m_uploading & UPLOAD_GET_STREAM_1) == 0;
}

void CUploadClipReader::OnClose()
{
	avf_safe_release(v0.io);
	avf_safe_release(v1.io);
	avf_safe_release(mp_pic_io);

	mpVDB->UnlockSegCache(v0.seg);
	mpVDB->UnlockSegCache(v1.seg);
}

avf_u64_t CUploadClipReader::GetSize()
{
	avf_u64_t result = m_clip_info.ex_size;
	if (m_upload_opt & UPLOAD_GET_V0)
		result += m_clip_info.v_size[0];
	if (m_upload_opt & UPLOAD_GET_V1)
		result += m_clip_info.v_size[1];
	return result + sizeof(upload_header_v2_t);	// __UPLOAD_END
}

int CUploadClipReader::ReadClipL(avf_u8_t *buffer, avf_size_t size)
{
	avf_uint_t header_size = sizeof(upload_header_v2_t) + sizeof(avf_stream_attr_t);
	avf_uint_t packet_size = PACKET_SIZE;
	avf_uint_t min_size = MAX(header_size, packet_size);

	int total_read = 0;
	int rval;

#ifdef AVF_DEBUG

	if (m_upload_opt & UPLOAD_GET_STREAM_0) {
		if (v0._start_item != v0.array->GetFirstItem()) {
			AVF_LOGD(" --- v0 items changed! --- ");
			v0._start_item = v0.array->GetFirstItem();
		}
	}

	if (m_upload_opt & UPLOAD_GET_STREAM_1) {
		if (v1._start_item != v1.array->GetFirstItem()) {
			AVF_LOGD(" --- v1 items changed! --- ");
			v1._start_item = v1.array->GetFirstItem();
		}
	}

	for (avf_size_t i = 0; i < NDATA; i++) {
		if (data[i].nitem > 0) {
			if (data[i]._start_item != data[i].array->GetFirstItem()) {
				AVF_LOGD(" --- raw %d items changed! --- ", i);
				data[i]._start_item = data[i].array->GetFirstItem();
			}
			if (data[i]._start_mem != data[i].GetMemBase()) {
				AVF_LOGD(" --- raw %d mem changed! --- ", i);
				data[i]._start_mem = data[i].GetMemBase();
			}
		}
	}

#endif

	while (size >= min_size) {

		switch (mState) {
		case STATE_V0:
			if (m_uploading & UPLOAD_GET_V0) {
				rval = Read_video(v0, buffer, size, STATE_PIC);
				break;
			}
			SetState(STATE_PIC);
			// fall

		case STATE_PIC:
			if (m_uploading & UPLOAD_GET_PIC) {
				rval = Read_data(data[DATA_PIC], buffer, size);
				break;
			}
			SetState(STATE_GPS);
			// fall

		case STATE_GPS:
			if (m_uploading & UPLOAD_GET_GPS) {
				rval = Read_data(data[DATA_GPS], buffer, size);
				break;
			}
			SetState(STATE_OBD);
			// fall

		case STATE_OBD:
			if (m_uploading & UPLOAD_GET_OBD) {
				rval = Read_data(data[DATA_OBD], buffer, size);
				break;
			}
			SetState(STATE_ACC);
			// fall

		case STATE_ACC:
			if (m_uploading & UPLOAD_GET_ACC) {
				rval = Read_data(data[DATA_ACC], buffer, size);
				break;
			}
			SetState(STATE_V1);
			// fall

		case STATE_V1:
			if (m_uploading & UPLOAD_GET_V1) {
				rval = Read_video(v1, buffer, size, STATE_NEXT);
				break;
			}
			SetState(STATE_NEXT);
			// fall

		case STATE_NEXT:
			if (!v0.b_end) {
				avf_status_t status = Read_NextVideoItem(v0);
				if (status != E_OK)
					return status;
			}

			// v1 has already got next item or end
			if (!v0.b_end) {
				SetState(STATE_V0);
			} else if (!v1.b_end) {
				SetState(STATE_V1);
			} else {
				// all end
				upload_header_v2_t tmp;
				upload_header_v2_t *header = &tmp;
				header->u_data_type = __UPLOAD_END;
				header->u_data_size = 0;
				header->u_timestamp = m_clip_info._length_ms;
				header->u_stream = 0;
				header->u_duration = 0;
				header->u_version = UPLOAD_VERSION_v2;
				header->u_flags = 0;
				header->u_param1 = 0;
				header->u_param2 = 0;

				avf_uint_t tocopy = sizeof(upload_header_v2_t);
				::memcpy(buffer, header, tocopy);

				total_read += tocopy;

				SetState(STATE_END);
				m_bytes_read += total_read;
				return total_read;
			}
			rval = 0;
			break;

		case STATE_END:
			m_bytes_read += total_read;
			return total_read;

		default:
			AVF_LOGE("unexpected state %d", mState);
			return (int)E_ERROR;
		}

		if (rval < 0) {
			return rval;
		}

		total_read += rval;
		buffer += rval;
		size -= rval;
	}

	m_bytes_read += total_read;
	return total_read;
}

avf_status_t CUploadClipReader::SeekToAddrL(avf_s64_t offset, int whence)
{
	AVF_LOGW("SeekToAddrL: not implemented for upload");
	return E_UNIMP;
}

avf_status_t CUploadClipReader::SeekToTimeL(avf_u32_t time_ms)
{
	AVF_LOGW("SeekToTimeL: not implemented for upload");
	return E_UNIMP;
}

avf_status_t CUploadClipReader::Read_Init()
{
	avf_status_t status;

	if (m_upload_opt & UPLOAD_GET_STREAM_0) {
		if ((status = Read_InitVideoItem(v0)) != E_OK)
			return status;
	}

	if (m_upload_opt & UPLOAD_GET_STREAM_1) {
		if ((status = Read_InitVideoItem(v1)) != E_OK)
			return status;
	}

	return E_OK;
}

avf_status_t CUploadClipReader::Read_InitData(v_pos_s& vpos)
{
	avf_u32_t seg_end_offset = vpos.seg_offset + vpos.seg_remain;
	const index_item_t *end_v_item = vpos.array->FindIndexItemByAddr_Next(seg_end_offset);

	if (end_v_item == NULL) {
		AVF_LOGE("error");
		return E_ERROR;
	}

	int indices[NDATA];
	indices[0] = (m_upload_opt & UPLOAD_GET_PIC) ? kPictureArray : -1;
	indices[1] = (m_upload_opt & UPLOAD_GET_GPS) ? kGps0Array : -1;
	indices[2] = (m_upload_opt & UPLOAD_GET_OBD) ? kObd0Array : -1;
	indices[3] = (m_upload_opt & UPLOAD_GET_ACC) ? kAcc0Array : -1;

	for (int i = 0; i < NDATA; i++) {
		int index = indices[i];
		data[i].nitem = 0;

		CIndexArray *array = index >= 0 ? vpos.seg->m_cache->GetArray(index) : NULL;
		if (array) {
			const index_item_t *item_start = array->FindIndexItem(vpos.GetItem()->time_ms);
			if (item_start == NULL)
				continue;

			const index_item_t *item_end = array->FindIndexItem_Next(end_v_item->time_ms);
			if (item_end == NULL)
				continue;

			if (item_end > item_start) {
				data[i].array = array;
				data[i].item_index = array->GetItemIndex(item_start);
				data[i].data_offset = item_start->fpos;
				data[i]._start_item = array->GetFirstItem();
				data[i]._start_mem = data[i].GetMemBase();
				data[i].item_remain = CIndexArray::GetItemSize(item_start);
				data[i].nitem = item_end - item_start;
			}
		}
	}

	return E_OK;
}

avf_status_t CUploadClipReader::Read_InitVideoItem(v_pos_s& vpos)
{
	avf_status_t status;
	clip_pos_s cpos;
	index_pos_t ipos;

	if (IsClipList()) {
		status = SeekClipListToTimeL(&m_cliplist_info, 0, cpos, vpos.stream, &ipos);
	} else {
		status = SeekClipToTimeL(&m_clip_info, 0, cpos, vpos.stream, &ipos);
	}

	if (status != E_OK)
		return status;

	vpos.seg = cpos.seg;
	vpos.seg_start_ms = cpos.seg_start_ms;

	vpos.array = ipos.array;
	vpos.item_index = ipos.array->GetItemIndex(ipos.item);
	vpos._start_item = ipos.array->GetFirstItem();
	vpos.item_size = ipos.GetItemBytes();
	vpos.item_remain = vpos.item_size;
	vpos.seg_offset = cpos.seg_offset;
	vpos.seg_remain = cpos.seg_remain;
	vpos.clip_remain = cpos.clip_remain;
	vpos.next_clip_addr = cpos.clip_remain;

	AVF_LOGD("Read_InitVideoItem, stream %d, seg %d", vpos.seg->m_stream, vpos.seg->m_info.id);
	mpVDB->LockSegCache(vpos.seg);

	if (vpos.stream == STREAM_MAIN) {
		if (m_upload_opt & UPLOAD_GET_PIC_RAW) {
			Read_InitData(vpos);
		}
	}

	return E_OK;
}

avf_status_t CUploadClipReader::Read_NextVideoItem(v_pos_s& vpos)
{
	vpos.seg_remain -= vpos.item_size;
	vpos.clip_remain -= vpos.item_size;

	// next item
	if (vpos.seg_remain > 0) {
		vpos.item_index++;
		vpos.item_size = CIndexArray::GetItemSize(vpos.array->GetItemAt(vpos.item_index));
		vpos.item_remain = vpos.item_size;
		return E_OK;
	}

	// close this segment
	vdb_seg_t *seg = vpos.seg;
	avf_s32_t seg_start_ms;
	vpos.seg = NULL;

	if (seg) {
		mpVDB->UnlockSegCache(seg);
	}
	if (vpos.io) {
		vpos.io->Close();
	}

	// next segment
	if (vpos.clip_remain > 0) {

		if ((seg = seg->GetNextSegInRefClip()) == NULL) {
			AVF_LOGE("no next segment");
			return E_ERROR;
		}

		seg_start_ms = seg->GetStartTime_ms() - m_clip_info.clip_time_ms;

	} else {

		// clip/cliplist done
		if (!IsClipList() || vpos.next_clip_addr >= m_clip_info.v_size[vpos.stream]) {
			// stream end
			m_uploading &= ~vpos.v_opt;
			vpos.b_end = true;
			return E_OK;
		}

		// next clip
		clip_pos_s next_pos;
		avf_status_t status = SeekClipListToAddrL(&m_cliplist_info, vpos.next_clip_addr, next_pos);
		if (status != E_OK)
			return status;

		vpos.clip_remain = next_pos.clip_remain;
		vpos.next_clip_addr += vpos.clip_remain;

		seg = next_pos.seg;
		seg_start_ms = next_pos.seg_start_ms;
	}

	// start with seg
	index_pos_t ipos;
	avf_status_t status = seg->GetFirstPos(kVideoArray, &ipos);
	if (status != E_OK)
		return status;

	vpos.seg = seg;
	vpos.seg_start_ms = seg_start_ms;

	vpos.array = ipos.array;
	vpos.item_index = ipos.array->GetItemIndex(ipos.item);
	vpos.item_size = ipos.GetItemBytes();
	vpos.item_remain = vpos.item_size;
	vpos.seg_offset = 0;
	vpos.seg_remain = seg->GetVideoDataSize();
	if (vpos.seg_remain > vpos.clip_remain)
		vpos.seg_remain = vpos.clip_remain;

	AVF_LOGD("Read_NextVideoItem, stream %d, seg %d", vpos.seg->m_stream, vpos.seg->m_info.id);
	mpVDB->LockSegCache(vpos.seg);

	if (vpos.stream == STREAM_MAIN) {
		if (m_upload_opt & UPLOAD_GET_PIC_RAW) {
			Read_InitData(vpos);
		}
	}

	return E_OK;
}

int CUploadClipReader::Read_video(v_pos_s& vpos, avf_u8_t *buffer, avf_size_t size, int next_state)
{
	if (m_item_bytes == 0) {

		// start to read video item
		if (vpos.stream != STREAM_MAIN && !v0.b_end) {
			// read v1 with stream 0
			if (vpos.GetItemStartMs() >= v0.GetItemEndMs()) {
				// belongs to next v0 item
				SetState(next_state);
				return 0;
			}
		}

		// open segment - todo: read live
		if (vpos.io == NULL || !vpos.io->IsOpen()) {
//			if (vpos.io == NULL && (vpos.io = CSysIO::Create()) == NULL)
//				return E_NOMEM;

			avf_status_t status = mpVDB->OpenSegmentVideo(vpos.seg, &vpos.io);
			//vpos.seg->OpenVideoRead(&vpos.io);
			if (status != E_OK)
				return status;

//			if (mbMuteAudio) {
//				FindAudio(vpos.io);
//			}

			if ((status = vpos.io->Seek(vpos.seg_offset)) != E_OK)
				return status;
		}

		// write item header
		// timestamp is relative to clipinfo start
		upload_header_v2_t tmp;
		upload_header_v2_t *header = &tmp;
		header->u_data_type = VDB_DATA_VIDEO;
		header->u_data_size = vpos.item_size + sizeof(avf_stream_attr_t);
		header->u_timestamp = vpos.GetItem()->time_ms + vpos.seg_start_ms;
		header->u_stream = vpos.stream;
		header->u_duration = CIndexArray::GetItemDuration(vpos.GetItem());
		header->u_version = UPLOAD_VERSION_v2;
		header->u_flags = 0;
		header->u_param1 = 0;
		header->u_param2 = 0;

		avf_uint_t tocopy1 = sizeof(upload_header_v2_t);
		::memcpy(buffer, header, tocopy1);
		buffer += tocopy1;
		m_item_bytes += tocopy1;

		// write stream attr
		avf_stream_attr_t *attr = &vpos.seg->m_info.stream_attr;
		avf_uint_t tocopy2 = sizeof(avf_stream_attr_t);
		::memcpy(buffer, attr, tocopy2);
		m_item_bytes += tocopy2;

		return tocopy1 + tocopy2;
	}

	// read packets
	size -= size % PACKET_SIZE;
	avf_uint_t toread = MIN(size, vpos.item_remain);

	avf_status_t status = ReadIO(vpos.io, buffer, toread);
	if (status != E_OK) {
		AVF_LOGE("read video(%d) failed", vpos.stream);
		return status;
	}

	m_item_bytes += toread;
	vpos.item_remain -= toread;

	if (vpos.item_remain == 0) {
		// item done
		m_item_bytes = 0;
		if (vpos.stream == STREAM_MAIN) {
			// stream 0: goto next item later
			SetState(next_state);
		} else {
			// stream 1: check next item
			avf_status_t status = Read_NextVideoItem(vpos);
			if (status != E_OK)
				return status;

			if (vpos.b_end) {
				// stream 1 done
				SetState(next_state);
			} else {
				// v1 has next item
				if (!v0.b_end && vpos.GetItemStartMs() >= v0.GetItemEndMs()) {
					// belongs to next v0 item
					SetState(next_state);
				}
			}
		}
	}

	return toread;
}

// pic/raw data are aligned with v0
int CUploadClipReader::Read_data(data_pos_s& data, avf_u8_t *buffer, avf_size_t size)
{
	const avf_u8_t *mem;
	int total_read = 0;
	int toread;

	if (m_item_bytes == 0) {

		if (data.nitem == 0) {
			SetState(data.next_state);
			goto Done;
		}

Item_Start:
		if (data.GetItem()->time_ms >= (v0.GetItem() + 1)->time_ms) {
			// belongs to next v0 item
			SetState(data.next_state);
			goto Done;
		}

		// write item header
		// timestamp is relative to clip start
		upload_header_v2_t tmp;
		upload_header_v2_t *header = &tmp;
		header->u_data_type = data.upload_type;
		header->u_data_size = data.item_remain;
		header->u_timestamp = data.GetItem()->time_ms + v0.seg_start_ms;
		header->u_stream = 0;
		header->u_duration = 0;
		header->u_version = UPLOAD_VERSION_v2;
		header->u_flags = 0;
		header->u_param1 = 0;
		header->u_param2 = 0;

		int tocopy = sizeof(upload_header_v2_t);
		::memcpy(buffer, header, tocopy);

		buffer += tocopy;
		size -= tocopy;

		m_item_bytes += tocopy;
		total_read += tocopy;

		if (size == 0) {
			goto Done;
		}
	}

	toread = MIN(size, data.item_remain);
	if ((mem = data.GetMemBase()) != NULL) {
		::memcpy(buffer, mem + data.data_offset, toread);
	} else {
		if (data.upload_type != VDB_DATA_JPEG) {
			AVF_LOGE("must be jpeg");
			return E_ERROR;
		}
		// pic - todo: read live
		if (mp_pic_io == NULL || !mp_pic_io->IsOpen()) {
			if (mp_pic_io == NULL && (mp_pic_io = CSysIO::Create()) == NULL)
				return E_NOMEM;
			avf_status_t status = v0.seg->OpenPictureRead(mp_pic_io);
			if (status != E_OK)
				return status;
			if ((status = mp_pic_io->Seek(data.data_offset)) != E_OK)
				return status;
		}
		// read
		avf_status_t status = ReadIO(mp_pic_io, buffer, toread);
		if (status != E_OK) {
			AVF_LOGE("read pic failed");
			return status;
		}
	}
	data.data_offset += toread;
	data.item_remain -= toread;

	buffer += toread;
	size -= toread;

	m_item_bytes += toread;
	total_read += toread;

	if (data.item_remain == 0) {
		m_item_bytes = 0;

		if (--data.nitem == 0) {
			// no more items in this segment
			if (data.upload_type == VDB_DATA_JPEG) {
				if (mp_pic_io) {
					mp_pic_io->Close();
				}
			}
			SetState(data.next_state);
			goto Done;
		}

		// next item
		data.item_index++;
		data.item_remain = CIndexArray::GetItemSize(data.GetItem());

		if (size >= sizeof(upload_header_v2_t)) {
			goto Item_Start;
		}
	}

Done:
	return total_read;
}

//-----------------------------------------------------------------------
//
//  CISOReader
//
//-----------------------------------------------------------------------
CISOReader *CISOReader::Create(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info)
{
	CISOReader *result = avf_new CISOReader(pVDB, clip_info, cliplist_info);
	if (result && result->Status() != E_OK) {
		result->CloseL();
		avf_safe_delete(result);
	}
	return result;
}

CISOReader::CISOReader(CVDB *pVDB, const clip_info_s *clip_info,
		const cliplist_info_s *cliplist_info):
	inherited(pVDB, clip_info, cliplist_info),
	mp_builder(NULL)
{
	CREATE_OBJ(mp_builder = CMP4Builder::Create());

	avf_status_t status;
	if (IsClipList()) {
		clip_info_s *ci = cliplist_info->elems;
		for (avf_size_t i = cliplist_info->count; i; i--, ci++) {
			if ((status = AddClipFiles(ci)) != E_OK) {
				mStatus = status;
				return;
			}
		}
	} else {
		if ((status = AddClipFiles(clip_info)) != E_OK) {
			mStatus = status;
			return;
		}
	}
}

CISOReader::~CISOReader()
{
	avf_safe_release(mp_builder);
}

void CISOReader::OnClose()
{
	if (mp_builder) {
		mp_builder->Close();
	}
}

IAVIO *CISOReader::CreateIO()
{
	if (mp_builder->FinishAddFile() != E_OK)
		return NULL;

	mp_builder->SetDate(m_clip_info.clip_date,
		m_clip_info.clip_date + (m_clip_info._length_ms/1000));

	return mp_builder;
}

avf_status_t CISOReader::AddClipFiles(const clip_info_s *ci)
{
	if (ci->_length_ms == 0) {
		return E_OK;
	}

	vdb_clip_t *clip = mpVDB->FindClipL(ci->clip_id);
	if (clip == NULL) {
		AVF_LOGE("clip not found");
		return E_ERROR;
	}

	vdb_seg_t *seg = clip->FindSegmentByTime(ci->stream, ci->clip_time_ms);
	if (seg == NULL) {
		AVF_LOGE("cannot find segment");
		return E_ERROR;
	}

	avf_u32_t start_time_ms = ci->clip_time_ms - seg->GetStartTime_ms();
	avf_u32_t duration_ms = ci->_length_ms;

	for (;;) {
		string_t *filename = seg->GetVideoFileName();
		if (filename == NULL) {
			AVF_LOGE("GetVideoFileName() failed");
			return E_ERROR;
		}

		avf_u32_t seg_duration = seg->GetDuration_ms();
		avf_u32_t length_ms = duration_ms;
		if (start_time_ms + length_ms > seg_duration) {
			length_ms = seg_duration - start_time_ms;
		}

		CMP4Builder::range_s range;
		range.start_time_ms = start_time_ms;
		range.end_time_ms = start_time_ms + length_ms;
		range.b_set_duration = 1;

		avf_status_t status = mp_builder->AddFile(filename->string(), &range);
		filename->Release();

		if (status != E_OK)
			return status;

		start_time_ms = 0;
		duration_ms -= length_ms;
		if (duration_ms == 0)
			break;

		if ((seg = seg->GetNextSegment()) == NULL) {
			AVF_LOGE("no next segment");
			return E_ERROR;
		}
	}

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CClipData
//
//-----------------------------------------------------------------------
CClipData *CClipData::Create(CVDB *pVDB, vdb_clip_t *clip)
{
	CClipData *data = avf_new CClipData(pVDB, clip);
	CHECK_STATUS(data);
	return data;
}

CClipData::CClipData(CVDB *pVDB, vdb_clip_t *clip):
	mbClosed(true),
	mpVDB(pVDB),
	m_clip(clip),
	m_io_video(NULL),
	m_io_picture(NULL),
	m_seg(NULL),
	m_seg_1(NULL)
{
}

CClipData::~CClipData()
{
	mpVDB->CloseClipData(this);
}

void *CClipData::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IClipData)
		return static_cast<IClipData*>(this);
	return inherited::GetInterface(refiid);
}

INLINE avf_status_t gen_data_result(avf_status_t status, bool proceed)
{
	if (status == S_END)
		return S_END;
	if (proceed)
		return E_OK;
	return status;
}

avf_status_t CClipData::ReadClipData(desc_s *desc, void **pdata, avf_size_t *psize)
{
	bool proceed;
	avf_status_t status;
	status = mpVDB->ReadClipData(this, desc, pdata, psize, NULL, &proceed);
	return gen_data_result(status, proceed);
}

avf_status_t CClipData::ReadClipDataToFile(desc_s *desc, const char *filename)
{
	IAVIO *avio;

	if (desc->data_type == VDB_DATA_VIDEO || desc->data_type == VDB_DATA_JPEG) {
		avio = CSysIO::Create();
	} else {
		avio = CFileIO::Create();
	}
	if (avio == NULL) {
		return E_NOMEM;
	}

	avf_status_t status = avio->CreateFile(filename);
	if (status != E_OK) {
		avio->Release();
		return status;
	}

	bool proceed;
	status = mpVDB->ReadClipData(this, desc, NULL, NULL, avio, &proceed);
	avio->Release();

	if (status != E_OK) {
		avf_remove_file(filename, false);
	}

	return gen_data_result(status, proceed);
}

#define NUM_RAW	3
avf_status_t CClipData::ReadClipRawData(int data_types, void **pdata, avf_size_t *psize, IAVIO *io)
{
	int type_array[NUM_RAW] = {RAW_DATA_GPS, RAW_DATA_OBD, RAW_DATA_ACC};
	void *data_array[NUM_RAW] = {NULL};
	avf_size_t size_array[NUM_RAW] = {0};

	bool all_end = true;
	bool got_data = false;

	avf_status_t status;
	avf_size_t total_size = 0;

	for (unsigned i = 0; i < NUM_RAW; i++) {
		if (data_types & (1 << type_array[i])) {
			bool proceed;
			desc_s desc;

			desc.data_type = type_array[i];
			status = mpVDB->ReadClipData(this, &desc, data_array + i, size_array + i, NULL, &proceed);

			if (status == E_OK) {
				got_data = true;
				all_end = false;
				continue;
			}

			if (status == S_END)
				continue;

			all_end = false;

			if (!proceed) {
				// there's an error
				goto Done;
			}
		}
	}

	if (all_end) {
		// all data return S_END
		return S_END;
	}

	if (!got_data) {
		// read nothing but no error
		*pdata = NULL;
		*psize = 0;
		return E_OK;
	}

	if (io != NULL) {

		int rval;
		for (unsigned i = 0; i < ARRAY_SIZE(type_array); i++) {
			if (data_array[i] != NULL) {
				avf_size_t size = size_array[i] - sizeof(upload_header_t);
				if ((rval = io->Write(data_array[i], size)) != (int)size) {
					status = E_IO;
					goto Done;
				}
				total_size += size;
			}
		}

		if (total_size > 0) {
			upload_header_t header;
			set_upload_end_header(&header, total_size);
			if ((rval = io->Write(&header, sizeof(header))) != (int)sizeof(header)) {
				status = E_IO;
				goto Done;
			}
		}

	} else {

		for (unsigned i = 0; i < ARRAY_SIZE(type_array); i++) {
			if (data_array[i] != NULL) {
				avf_size_t size = size_array[i] - sizeof(upload_header_t);
				total_size += size;
			}
		}

		if (total_size > 0) {
			*pdata = avf_malloc(total_size + sizeof(upload_header_t));
			if (*pdata == NULL) {
				status = E_NOMEM;
				goto Done;
			}
			*psize = total_size + sizeof(upload_header_t);

			avf_u8_t *ptr = (avf_u8_t*)(*pdata);
			for (unsigned i = 0; i < ARRAY_SIZE(type_array); i++) {
				if (data_array[i] != NULL) {
					avf_size_t size = size_array[i] - sizeof(upload_header_t);
					::memcpy(ptr, data_array[i], size);
					ptr += size;
				}
			}

			upload_header_t header;
			set_upload_end_header(&header, total_size);
			::memcpy(ptr, &header, sizeof(header));
		}
	}

	status = E_OK;

Done:
	for (unsigned i = 0; i < ARRAY_SIZE(type_array); i++) {
		avf_safe_free(data_array[i]);
	}
	return status;
}

avf_status_t CClipData::ReadClipRawData(int data_types, void **pdata, avf_size_t *psize)
{
	if ((data_types & (data_types - 1)) == 0) {
		desc_s desc;
		if (data_types == (1 << RAW_DATA_OBD)) {
			desc.data_type = RAW_DATA_OBD;
		} else if (data_types == (1 << RAW_DATA_GPS)) {
			desc.data_type = RAW_DATA_GPS;
		} else if (data_types == (1 << RAW_DATA_ACC)) {
			desc.data_type = RAW_DATA_ACC;
		} else {
			return E_ERROR;
		}
		return ReadClipData(&desc, pdata, psize);
	}
	return ReadClipRawData(data_types, pdata, psize, NULL);
}

avf_status_t CClipData::ReadClipRawDataToFile(int data_types, const char *filename)
{
	IAVIO *io = CFileIO::Create();
	if (io == NULL)
		return E_NOMEM;

	avf_status_t status = io->CreateFile(filename);
	if (status != E_OK) {
		avf_safe_release(io);
		return status;
	}

	status = ReadClipRawData(data_types, NULL, NULL, io);
	io->Release();

	if (status != E_OK) {
		avf_remove_file(filename, false);
	}

	return status;
}

avf_u8_t *CClipData::AllocData(desc_s *desc, const index_item_t *item, avf_size_t *psize, avf_size_t extra_size)
{
	avf_size_t size = 0;
	const index_item_t *next_item = item + 1;
	for (unsigned i = 0; i < desc->num_entries; i++) {
		size += sizeof(upload_header_t) + extra_size + (next_item->fpos - item->fpos);
		item++;
		next_item++;
	}
	size += sizeof(upload_header_t);
	*psize = size;
	return avf_malloc_bytes(size);
}

void CClipData::ReleaseData(void *data)
{
	avf_safe_free(data);
}

