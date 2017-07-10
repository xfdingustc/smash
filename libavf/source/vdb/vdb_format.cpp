
#define LOG_TAG "vdb_format"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "avio.h"
#include "sys_io.h"
#include "mem_stream.h"

#include "rbtree.h"
#include "vdb_util.h"
#include "vdb_cmd.h"
#include "vdb_clip.h"
#include "vdb_util.h"
#include "vdb_format.h"

#define AVF_UUID	"uuid="

#define FCC_CLIP	MKFCC_LE('C','L','I','P')
#define FCC_STRM	MKFCC_LE('S','T','R','M')
#define FCC_RCS0	MKFCC_LE('R','C','S','0')
#define FCC_RCLP	MKFCC_LE('R','C','L','P')
#define FCC_VDBI	MKFCC_LE('V','D','B','I')
#define FCC_PICT	MKFCC_LE('p','i','c','t')
#define FCC_VIDE	MKFCC_LE('v','i','d','e')
#define FCC_GPS0	MKFCC_LE('g','p','s','0')
#define FCC_ACC0	MKFCC_LE('a','c','c','0')
#define FCC_OBD0	MKFCC_LE('o','b','d','0')
#define FCC_DATA	MKFCC_LE('d','a','t','a')
#define FCC_VDBF	MKFCC_LE('V','D','B','F')
#define FCC_DIRS	MKFCC_LE('D','I','R','S')
#define FCC_LDIR	MKFCC_LE('L','D','I','R')
#define FCC_DESC	MKFCC_LE('D','E','S','C')
#define FCC_INF		MKFCC_LE('I','N','F',' ')
#define FCC_RAWI	MKFCC_LE('R','A','W','I')
#define FCC_VMEM	MKFCC_LE('V','M','E','M')
#define FCC_RFCC	MKFCC_LE('R','F','C','C')
#define FCC_RAWD	MKFCC_LE('R','A','W','D')

//-----------------------------------------------------------------------
//
//  CVdbFormat
//
//-----------------------------------------------------------------------

// load "YYYY.MM.DD-HH.MM.SS.clip"
// caller will close the io
avf_status_t CVdbFormat::LoadClipInfo(
	string_t *clipinfo_dir, vdb_clip_t *clip, IAVIO *io,
	avf_u64_t *ref_start_time_ms, avf_u64_t *ref_end_time_ms,
	int flag, bool b_remove_on_fail)
{
	local_time_name_t lname;
	local_time_to_name(clip->clip_id, CLIPINFO_FILE_EXT, lname);
	ap<string_t> clipinfo_name(clipinfo_dir, lname.buf);
	//AVF_LOGD(C_WHITE "load %s" C_NONE, clipinfo_name->string());

	avf_status_t status = io->OpenRead(clipinfo_name->string());
	if (status != E_OK) {
		AVF_LOGE("%s not found", clipinfo_name->string());
		return status;
	}

	CReadBuffer buffer(io);

	if (io->GetSize() == 0) {
		AVF_LOGW("should remove empty clipinfo: %s", clipinfo_name->string());
		io->Close();
		if (b_remove_on_fail) {
			AVF_LOGD(C_YELLOW "remove %s" C_NONE, clipinfo_name->string());
			avf_remove_file(clipinfo_name->string());
		}
		return E_ERROR;
	}

	buffer.aread_begin(4*4);
		avf_u32_t id = buffer.__aread_le_32();
		avf_u32_t size = buffer.__aread_le_32();
		avf_u32_t version = buffer.__aread_le_32();
		avf_u32_t extra_data_size = buffer.__aread_le_32();
	buffer.aread_end();

	if (id != FCC_CLIP) {
		AVF_LOGE("bad clipinfo file: %s", clipinfo_name->string());
		return E_ERROR;
	}

	if (version != 1) {
		AVF_LOGE("unknown clip version %d", version);
		return E_ERROR;
	}

	if (flag == LOAD_CLIP_ALL) {
		buffer.aread_begin(6*4);
			clip->clip_date = buffer.__aread_le_32();
			clip->gmtoff = buffer.__aread_le_32();
			clip->m_num_segs_0 = buffer.__aread_le_32();
			clip->m_num_segs_1 = buffer.__aread_le_32();
			clip->first_pic_name = buffer.__aread_le_32();
			clip->first_pic_size = buffer.__aread_le_32();
		buffer.aread_end();

		*ref_start_time_ms = buffer.aread_le_64();
		*ref_end_time_ms = buffer.aread_le_64();

		buffer.read_mem((avf_u8_t*)&clip->m_size_main, sizeof(vdb_size_t));
		buffer.read_mem((avf_u8_t*)&clip->m_size_sub, sizeof(vdb_size_t));

		buffer.read_mem((avf_u8_t*)&clip->main_stream_attr, sizeof(avf_stream_attr_s));
		buffer.read_mem((avf_u8_t*)&clip->sub_stream_attr, sizeof(avf_stream_attr_s));

		if (extra_data_size >= 4) {
			clip->m_video_type_0 = buffer.read_le_16();
			clip->m_video_type_1 = buffer.read_le_16();
			extra_data_size -= 4;
		}

		buffer.Skip(extra_data_size);
	}

	buffer.SeekTo(size + 8);

	while (true) {
		id = buffer.aread_le_32();
		size = buffer.aread_le_32();

		// reached trailing 0
		if (id == 0 || size == 0)
			break;

		avf_u32_t pos = (avf_u32_t)buffer.GetReadPos();

		switch (id) {
		case FCC_STRM:
			if (clip->m_cache) {
				if ((status = LoadStream(clip, buffer)) != E_OK)
					return status;
			}
			break;

		case FCC_RCS0:
			if (flag == LOAD_CLIP_ALL) {
				int num_buffered = 0;
				if ((status = LoadRefClips(clip, buffer, &num_buffered)) != E_OK)
					return status;
				if (num_buffered > 0 && ref_start_time_ms) {
					//AVF_LOGD(C_CYAN "there are %d buffered clips" C_NONE, num_buffered);
					*ref_start_time_ms = 0;
					*ref_end_time_ms = 0;
				}
			}
			break;

		case FCC_DESC:
			if (flag == LOAD_CLIP_ALL) {
				if ((status = ReadClipDesc(clip, buffer, size)) != E_OK)
					return status;
			}
			break;

		case FCC_RAWI:
			if (flag == LOAD_CLIP_ALL) {
				buffer.read_mem((avf_u8_t*)&clip->raw_info, sizeof(clip->raw_info));
				// extra_data_size = buffer.read_le_32();
				// buffer.Skip(extra_data_size);
			}
			break;

		case FCC_INF:
			if (flag == LOAD_CLIP_ALL) {
				if ((status = ReadClipInfo(clip, buffer, size)) != E_OK)
					return status;
			}
			break;

		default:
			break;
		}

		pos += size;
		if (pos + 8 >= buffer.GetFileSize())
			break;

		buffer.SeekTo(pos);
	}

	clip->m_clipinfo_filesize = io->GetSize();

	return E_OK;
}

avf_status_t CVdbFormat::ReadClipDesc(vdb_clip_t *clip, CReadBuffer& buffer, avf_u32_t size)
{
	char buf[256];

	if (size >= sizeof(buf)) {
		AVF_LOGE("clip desc too long: %d, skip", size);
		return E_OK;
	}

	if (!buffer.read_mem((avf_u8_t*)buf, size)) {
		AVF_LOGE("read clip desc failed, size=%d", size);
		return E_ERROR;
	}

	buf[size] = 0;

	if (::strncasecmp(buf, AVF_UUID, STR_SIZE(AVF_UUID)) != 0) {
		AVF_LOGW("bad clip desc %s", buf);
		return E_OK;	// continue
	}

	clip->SaveUUID((const avf_u8_t*)(buf + STR_SIZE(AVF_UUID)), size - STR_SIZE(AVF_UUID));

	return E_OK;
}

avf_status_t CVdbFormat::ReadClipInfo(vdb_clip_t *clip, CReadBuffer& buffer, avf_u32_t size)
{
	avf_size_t extra_size = buffer.read_le_32();
	buffer.Skip(extra_size);	// extra_data

	while (true) {
		int type = buffer.read_le_32();
		if (type == CLIP_DESC_NULL)
			break;

		int size = buffer.read_le_32();
		clip_desc_item_s *item = clip->SaveDescData(NULL, size, type);
		if (item) {
			buffer.read_mem(item->GetMem(), size);
		} else {
			buffer.Skip(size);
		}

		buffer.Skip(round_up(size) - size);
	}

	return E_OK;
}

avf_status_t CVdbFormat::LoadStream(vdb_clip_t *clip, CReadBuffer& buffer)
{
	avf_uint_t stream = buffer.aread_le_32();
	avf_uint_t num_segs = buffer.aread_le_32();
	buffer.Skip(4);	// reserved

	avf_uint_t extra_size = buffer.aread_le_32();
	buffer.Skip(extra_size);

	vdb_stream_t *vdb_stream = clip->__GetStream(stream);

	for (avf_uint_t i = 0; i < num_segs; i++) {
		buffer.aread_begin(6*4);
			avf_time_t subdir_name = buffer.__aread_le_32();
			avf_time_t index_name = buffer.__aread_le_32();
			avf_time_t video_name = buffer.__aread_le_32();
			avf_time_t picture_name = buffer.__aread_le_32();
			avf_u32_t first_pic_size = buffer.__aread_le_32();
			avf_u32_t extra_size = buffer.__aread_le_32();
		buffer.aread_end();

		// seg info
		vdb_seg_t::info_t info;
		if (!buffer.read_mem((avf_u8_t*)&info, sizeof(info))) {
			AVF_LOGE("cannot read seg info, clip %x", clip->clip_id);
			return E_ERROR;
		}

		// extra_size == 0
		// extra_size == 4
		// extra_size > 4

		// extra data size
		buffer.Skip(extra_size);

		if (info.duration_ms == 0) {
			AVF_LOGW("segment %d is empty, total %d", i, num_segs);
			// this segment will be removed when the clip is not used anymore
		}

		// for old clips
		if (info.stream_attr.stream_version == 0)
			info.stream_attr.stream_version = 1;

		// create segment
		vdb_seg_t *seg = vdb_stream->AddSegment(
			subdir_name, index_name, video_name, picture_name, &info);
		seg->m_first_pic_size = first_pic_size;
	}

	return E_OK;
}

avf_status_t CVdbFormat::LoadRefClipInfo(CReadBuffer& buffer, ref_info_t& info, avf_u32_t *clip_attr, raw_data_t **pSceneData)
{
	info.ver_code = 0;
	buffer.read_mem((avf_u8_t*)&info, sizeof(info));

	if (info.ver_code != FCC_RCLP) {
		AVF_LOGE("LoadRefClipInfo: bad ref clip");
		return E_ERROR;
	}

	unsigned extra_size = info.extra_size;
	if (extra_size >= 4) {
		*clip_attr = buffer.read_le_32();
		extra_size -= 4;
	}

	if (extra_size >= 4) {
		avf_size_t data_size = buffer.read_le_32();
		extra_size -= 4;
		if (data_size > extra_size) {
			AVF_LOGW("bad scene data size: %d,%d", data_size, extra_size);
		} else if (data_size > 0) {
			*pSceneData = avf_alloc_raw_data(data_size, MKFCC('S','C','E','N'));
			if (*pSceneData != NULL) {
				buffer.read_mem((*pSceneData)->GetData(), data_size);
				extra_size -= data_size;
			}
		}
	}

	buffer.Skip(extra_size);

	return E_OK;
}

// marked ref clips
avf_status_t CVdbFormat::LoadRefClips(vdb_clip_t *clip, CReadBuffer& buffer, int *p_num_buffered)
{
	avf_status_t status;

	avf_u32_t version = buffer.aread_le_32();
	if (version != 1) {
		AVF_LOGE("LoadRefClips: unknown version %d", version);
		return E_ERROR;
	}

	avf_uint_t num_ref_clips = buffer.aread_le_32();
	buffer.aread_le_32();	// reserved

	avf_uint_t extra_size = buffer.aread_le_32();
	buffer.Skip(extra_size);

	for (avf_uint_t i = 0; i < num_ref_clips; i++) {
		ref_info_t info;
		avf_u32_t clip_attr = 0;
		raw_data_t *scene_data = NULL;

		if ((status = LoadRefClipInfo(buffer, info, &clip_attr, &scene_data)) != E_OK) {
			AVF_LOGD("ref file: %x", clip->clip_id);
			return status;
		}

		if (info.clip_id != clip->clip_id) {
			AVF_LOGW("LoadRefClips: bad clip_id: %x", info.clip_id);
		}

		if (SAVE_IN_CLIPINFO(info.ref_clip_type)) {
			if (info.end_time_ms > info.start_time_ms) {
				ref_clip_t *ref_clip = clip->NewRefClip(info.ref_clip_type,
					info.ref_clip_id, info.start_time_ms, info.end_time_ms,
					clip_attr & ~CLIP_ATTR_LIVE);

				ref_clip->plist_index = info.plist_index;
				if (scene_data) {
					ref_clip->SetSceneData(scene_data);
				}

				if (info.ref_clip_type == CLIP_TYPE_BUFFER) {
					(*p_num_buffered)++;
					clip->buffered_clip_id = info.ref_clip_id;	// may be overriden
				}
			} else {
				AVF_LOGD("ref clip is empty: " LLD ", " LLD, info.start_time_ms, info.end_time_ms);
			}
		} else {
			AVF_LOGW("ref clip ignored: type %d id 0x%x", info.ref_clip_type, info.ref_clip_id);
		}

		avf_safe_release(scene_data);
	}

	return E_OK;
}

avf_status_t CVdbFormat::SaveClipInfo(string_t *clipinfo_dir, vdb_clip_t *clip, IAVIO *io)
{
	if (clip->m_cache == NULL) {
		AVF_LOGE("cache not loaded");
		return E_ERROR;
	}

	local_time_name_t lname;
	local_time_to_name(clip->clip_id, CLIPINFO_FILE_EXT, lname);
	ap<string_t> clipinfo_name(clipinfo_dir, lname.buf);
	AVF_LOGD(C_CYAN "save %s" C_NONE, clipinfo_name->string());

	if (io->CreateFile(clipinfo_name->string()) != E_OK) {
		return E_IO;
	}

	CWriteBuffer buffer(io);

	// header
	buffer.awrite_begin(4*10);

		buffer.__awrite_le_32(FCC_CLIP);
		buffer.__awrite_le_32(8*4 + 8*2 + sizeof(vdb_size_t)*2 +
			sizeof(avf_stream_attr_s)*2 + 4);

		buffer.__awrite_le_32(1);	// version
		buffer.__awrite_le_32(4);	// extra_size: m_video_type_0, m_video_type_1

		buffer.__awrite_le_32(clip->clip_date);
		buffer.__awrite_le_32(clip->gmtoff);

		buffer.__awrite_le_32(clip->m_num_segs_0);
		buffer.__awrite_le_32(clip->m_num_segs_1);

		buffer.__awrite_le_32(clip->first_pic_name);
		buffer.__awrite_le_32(clip->first_pic_size);

	buffer.awrite_end();

	// ref_start_time, ref_end_time
	ref_clip_t *ref_clip = NULL;
	list_head_t *node;
	list_for_each(node, clip->ref_clip_list) {
		ref_clip_t *tmp = ref_clip_t::from_clip_node(node);
		if (tmp->ref_clip_type == CLIP_TYPE_BUFFER) {
			ref_clip = tmp;
			break;
		}
	}

	// still save the buffered ref clip
	if (ref_clip == NULL) {
		buffer.write_zero(16);
	} else {
		buffer.awrite_le_64(ref_clip->start_time_ms);
		buffer.awrite_le_64(ref_clip->end_time_ms);
	}

	// size info
	buffer.write_mem((const avf_u8_t*)&clip->m_size_main, sizeof(vdb_size_t));
	buffer.write_mem((const avf_u8_t*)&clip->m_size_sub, sizeof(vdb_size_t));

	// stream attr
	buffer.write_mem((const avf_u8_t*)&clip->main_stream_attr, sizeof(avf_stream_attr_s));
	buffer.write_mem((const avf_u8_t*)&clip->sub_stream_attr, sizeof(avf_stream_attr_s));

	buffer.write_le_16(clip->m_video_type_0);
	buffer.write_le_16(clip->m_video_type_1);

	// save raw data FCCs
	avf_size_t nfcc = clip->m_extra_raw_fcc.count;
	if (nfcc > 0) {
		avf_u32_t size = 4 + 4 + 4 * nfcc;	// extra + nfcc + FCCs
		buffer.awrite_begin(4*4);
			buffer.__awrite_le_32(FCC_RFCC);
			buffer.__awrite_le_32(size);
			buffer.__awrite_le_32(0);	// extra size
			buffer.__awrite_le_32(nfcc);
		buffer.awrite_end();
		avf_u32_t *pfcc = clip->m_extra_raw_fcc.Item(0);
		for (avf_size_t i = 0; i < nfcc; i++, pfcc++) {
			buffer.write_be_32(*pfcc);
		}
	}

	// save clip desc
	WriteClipDesc(&clip->m_desc_list, buffer);

	// save raw info
	if (clip->raw_info.ShouldSave()) {
		WriteRawInfo(&clip->raw_info, buffer);
	}

	// save ref clip set
	SaveRefClips(clip, buffer);

	// save segments
	for (int i = 0; i < 2; i++) {
		vdb_stream_t *vdb_stream = clip->__GetStream(i);
		if (!vdb_stream->IsEmpty()) {
			SaveStream(clip, vdb_stream, buffer);
		}
	}

	buffer.Flush();
	io->Close();

	clip->m_clipinfo_filesize = avf_get_file_size(clipinfo_name->string());

	return E_OK;
}

// uuid[5+UUID_LEN+1]
avf_status_t CVdbFormat::WriteClipDesc(list_head_t *list, CWriteBuffer& buffer)
{
	list_head_t *node;

	avf_size_t total_size = 0;
	list_for_each_p(node, list) {
		clip_desc_item_s *item = clip_desc_item_s::from_node(node);
		if (item->type != CLIP_DESC_UUID) {
			total_size += 8;
			total_size += round_up(item->size);
		} else {
			int total = round_up(STR_SIZE(AVF_UUID) + item->size);

			buffer.write_le_32(FCC_DESC);
			buffer.write_le_32(total);

			buffer.write_string(AVF_UUID, STR_SIZE(AVF_UUID));
			buffer.write_mem(item->GetMem(), item->size);
			buffer.write_zero(total - STR_SIZE(AVF_UUID) - item->size);
		}
	}

	if (total_size == 0)
		return E_OK;

	buffer.write_le_32(FCC_INF);
	buffer.write_le_32(4 + total_size + 4);
	buffer.write_le_32(0);	// extra_size

	list_for_each_p(node, list) {
		clip_desc_item_s *item = clip_desc_item_s::from_node(node);
		if (item->type != CLIP_DESC_UUID) {
			buffer.write_le_32(item->type);
			buffer.write_le_32(item->size);
			buffer.write_mem(item->GetMem(), item->size);
			buffer.write_zero(round_up(item->size) - item->size);
		}
	}

	buffer.write_le_32(CLIP_DESC_NULL);

	return E_OK;
}

avf_status_t CVdbFormat::WriteRawInfo(const raw_info_t *raw_info, CWriteBuffer& buffer)
{
	buffer.write_le_32(FCC_RAWI);
	buffer.write_le_32(sizeof(raw_info_t) + 4);
	buffer.write_mem((const avf_u8_t*)raw_info, sizeof(raw_info_t));
	buffer.write_le_32(0);	// extra data szie
	return E_OK;
}

avf_status_t CVdbFormat::SaveStream(vdb_clip_t *clip, vdb_stream_t *vdb_stream, CWriteBuffer& buffer)
{
	avf_size_t extra_size = 0;
	avf_size_t seg_size;
	avf_size_t total_size;
	avf_size_t nfcc = clip->m_extra_raw_fcc.count;

	if (vdb_stream->m_index == 0) {
		extra_size = 4 + 4 * kNumExtraArrays;
		extra_size += 4 + 4 * nfcc;
	}

	seg_size = 24 + sizeof(vdb_seg_t::info_t) + extra_size;
	total_size = 16 + seg_size * vdb_stream->m_num_segs;

	buffer.awrite_begin(4*6);

		buffer.__awrite_le_32(FCC_STRM);
		buffer.__awrite_le_32(total_size);

		buffer.__awrite_le_32(vdb_stream->m_index);
		buffer.__awrite_le_32(vdb_stream->m_num_segs);

		buffer.__awrite_le_32(0);	// reserved
		buffer.__awrite_le_32(0);	// extra_size

	buffer.awrite_end();

	avf_uint_t count = 0;
	vdb_seg_t *seg = vdb_stream->GetFirstSegment();
	for (; seg; seg = seg->GetNextSegment(), count++) {

		buffer.awrite_begin(4*6);
			buffer.__awrite_le_32(seg->m_subdir_name);
			buffer.__awrite_le_32(seg->m_index_name);
			buffer.__awrite_le_32(seg->m_video_name);
			buffer.__awrite_le_32(seg->m_picture_name);
			buffer.__awrite_le_32(seg->m_first_pic_size);
			buffer.__awrite_le_32(extra_size);
		buffer.awrite_end();

		buffer.write_mem((const avf_u8_t*)&seg->m_info, sizeof(vdb_seg_t::info_t));

		if (vdb_stream->m_index == 0) {
			buffer.write_le_32(kNumExtraArrays);
			buffer.write_mem((const avf_u8_t*)&seg->extra_data_size, 4 * kNumExtraArrays);

			buffer.write_le_32(nfcc);
			avf_size_t tmp = seg->extra_raw_sizes.count;
			if (tmp > 0) {
				buffer.write_mem((const avf_u8_t*)seg->extra_raw_sizes.Item(0), tmp * 4);
			}
			if (tmp < nfcc) {
				buffer.write_zero((nfcc - tmp) * 4);
			}
		}
	}

	if (count != vdb_stream->m_num_segs) {
		AVF_LOGE("seg not match: %d, %d", count, vdb_stream->m_num_segs);
	}

	return E_OK;
}

avf_status_t CVdbFormat::SaveRCSHeader(CWriteBuffer& buffer, avf_size_t count, avf_size_t size)
{
	avf_uint_t total_size = 16 + size;
	buffer.awrite_begin(4*6);
		buffer.__awrite_le_32(FCC_RCS0);
		buffer.__awrite_le_32(total_size);

		buffer.__awrite_le_32(1);	// version
		buffer.__awrite_le_32(count);

		buffer.__awrite_le_32(0);	// reserved
		buffer.__awrite_le_32(0);	// extra_data_size
	buffer.awrite_end();
	return E_OK;
}

// marked ref clips
avf_status_t CVdbFormat::SaveRefClips(vdb_clip_t *clip, CWriteBuffer& buffer)
{
	// buffered/marked ref clips
	avf_size_t count = 0;
	avf_size_t size = 0;

	list_head_t *node;
	list_for_each(node, clip->ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_clip_node(node);
		if (SAVE_IN_CLIPINFO(ref_clip->ref_clip_type)) {
			count++;
			size += sizeof(ref_info_t) + 4 + 4;	// ref_info, clip_attr, scene_data_size
			if (ref_clip->m_scene_data) {
				size += ref_clip->m_scene_data->GetSize();
			}
		}
	}

	if (count == 0)
		return E_OK;

	SaveRCSHeader(buffer, count, size);

	list_for_each(node, clip->ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_clip_node(node);
		if (SAVE_IN_CLIPINFO(ref_clip->ref_clip_type)) {
			ref_info_t info;

			info.ver_code = FCC_RCLP;
			info.ref_clip_type = ref_clip->ref_clip_type;
			info.ref_clip_id = ref_clip->ref_clip_id;
			info.clip_id = ref_clip->clip->clip_id;
			info.plist_index = ref_clip->plist_index;
			info.extra_size = 4 + 4;	// clip_attr, scene_data_size
			if (ref_clip->m_scene_data) {
				info.extra_size += ref_clip->m_scene_data->GetSize();
			}
			info.start_time_ms = ref_clip->start_time_ms;
			info.end_time_ms = ref_clip->end_time_ms;

			buffer.write_mem((const avf_u8_t*)&info, sizeof(info));

			buffer.write_le_32(ref_clip->clip_attr & ~CLIP_ATTR_LIVE);
			if (ref_clip->m_scene_data) {
				buffer.write_le_32(ref_clip->m_scene_data->GetSize());
				buffer.write_mem(ref_clip->m_scene_data->GetData(), ref_clip->m_scene_data->GetSize());
			} else {
				buffer.write_le_32(0);	// scene_data_size
			}
		}
	}

	return E_OK;
}

avf_status_t CVdbFormat::SaveCopyClipInfo(string_t *clipinfo_dir, copy_clip_s *cclip,
	avf_time_t gen_time, copy_clip_info_s& info, IAVIO *io)
{
	local_time_name_t lname;
	local_time_to_name(gen_time, CLIPINFO_FILE_EXT, lname);
	ap<string_t> clipinfo_name(clipinfo_dir, lname.buf);
	AVF_LOGD(C_YELLOW "save %s" C_NONE, clipinfo_name->string());

	if (io->CreateFile(clipinfo_name->string()) != E_OK) {
		return E_IO;
	}

	CWriteBuffer buffer(io);

	// header
	buffer.awrite_begin(4*10);

		buffer.__awrite_le_32(FCC_CLIP);
		buffer.__awrite_le_32(8*4 + 8*2 + sizeof(vdb_size_t)*2 +
			sizeof(avf_stream_attr_s)*2);

		buffer.__awrite_le_32(1);	// version
		buffer.__awrite_le_32(0);	// extra_size

		buffer.__awrite_le_32(cclip->clip->clip_date);
		buffer.__awrite_le_32(cclip->clip->gmtoff);

		buffer.__awrite_le_32(info.stream_0.count);
		buffer.__awrite_le_32(info.stream_1.count);

		buffer.__awrite_le_32(AVF_INVALID_TIME);	// TODO: first_pic_name
		buffer.__awrite_le_32(0);	// TODO: first_pic_size

	buffer.awrite_end();

	// ref_start_time, ref_end_time
	buffer.write_zero(16);

	// size info
	buffer.write_mem((const avf_u8_t*)&info.stream_0.size, sizeof(vdb_size_t));
	buffer.write_mem((const avf_u8_t*)&info.stream_1.size, sizeof(vdb_size_t));

	// stream attr
	buffer.write_mem((const avf_u8_t*)&cclip->clip->main_stream_attr, sizeof(avf_stream_attr_s));
	buffer.write_mem((const avf_u8_t*)&cclip->clip->sub_stream_attr, sizeof(avf_stream_attr_s));

	// save clip desc
	WriteClipDesc(&cclip->clip->m_desc_list, buffer);

	// marked ref clips
	avf_size_t count = 0;
	avf_size_t size = 0;

	for (avf_uint_t i = 0; i < cclip->count; i++) {
		copy_ref_s *ref = cclip->elems + i;
		count++;
		size += sizeof(ref_info_t) + 4 + 4;	// ref_info, clip_attr, scene_data_size
		if (ref->scene_data) {
			size += ref->scene_data->GetSize();
		}
	}

	if (count > 0) {
		SaveRCSHeader(buffer, count, size);

		for (avf_uint_t i = 0; i < cclip->count; i++) {
			copy_ref_s *ref = cclip->elems + i;
			ref_info_t refinfo;
			refinfo.ver_code = FCC_RCLP;
			refinfo.ref_clip_type = ref->clip_type;
			refinfo.ref_clip_id = ref->clip_id;
			refinfo.clip_id = info.clipinfo_name;
			refinfo.plist_index = 0;
			refinfo.extra_size = 4 + 4;		// clip_attr, scene_data_size
			if (ref->scene_data) {
				refinfo.extra_size += ref->scene_data->GetSize();
			}
			refinfo.start_time_ms = ref->start_time_ms;
			refinfo.end_time_ms = ref->end_time_ms;

			buffer.write_mem((const avf_u8_t*)&refinfo, sizeof(refinfo));
			buffer.write_le_32(ref->clip_attr);

			if (ref->scene_data) {
				buffer.write_le_32(ref->scene_data->GetSize());
				buffer.write_mem(ref->scene_data->GetData(), ref->scene_data->GetSize());
			} else {
				buffer.write_le_32(0);	// scene_data_size;
			}
		}
	}

	// save segments
	if (info.stream_0.count > 0) {
		SaveCopyStreamInfo(buffer, cclip, info.stream_0, 0);
	}
	if (info.stream_1.count > 0) {
		SaveCopyStreamInfo(buffer, cclip, info.stream_1, 1);
	}

	buffer.Flush();
	io->Close();

	return E_OK;
}

avf_status_t CVdbFormat::SaveCopyStreamInfo(CWriteBuffer& buffer, 
	copy_clip_s *cclip, copy_stream_info_s& stream, int s)
{
	avf_size_t seg_size = 24 + sizeof(vdb_seg_t::info_t);
	avf_size_t total_size = 16 + seg_size * stream.count;

	buffer.awrite_begin(4*6);

		buffer.__awrite_le_32(FCC_STRM);
		buffer.__awrite_le_32(total_size);

		buffer.__awrite_le_32(s);
		buffer.__awrite_le_32(stream.count);

		buffer.__awrite_le_32(0);	// reserved
		buffer.__awrite_le_32(0);	// exstra_size

	buffer.awrite_end();

	for (avf_uint_t i = 0; i < stream.count; i++) {
		copy_seg_info_s::out_s *segout = stream.elems + i;
		buffer.awrite_begin(4*6);
			buffer.__awrite_le_32(AVF_INVALID_TIME);
			buffer.__awrite_le_32(segout->index_name);
			buffer.__awrite_le_32(segout->video_name);
			buffer.__awrite_le_32(segout->picture_name);
//			buffer.__awrite_le_32(seg->first_pic_size);
			buffer.__awrite_le_32(0);
			buffer.__awrite_le_32(0);	// extra data
		buffer.awrite_end();
		buffer.write_mem((const avf_u8_t*)&segout->seg_info, sizeof(vdb_seg_t::info_t));
	}

	return E_OK;
}

avf_status_t CVdbFormat::LoadPlaylist(string_t *playlist_dir, clip_set_t& cs)
{
	ap<string_t> playlist_name(playlist_dir, PLAYLIST_FILENAME);
	if (!avf_file_exists(playlist_name->string()))
		return E_ERROR;

	AVF_LOGD(C_WHITE "load %s" C_NONE, playlist_name->string());
	IAVIO *io = CSysIO::Create();

	avf_status_t status = io->OpenRead(playlist_name->string());
	if (status == E_OK) {
		CReadBuffer buffer(io);
		status = DoLoadPlaylist(cs, buffer);
	}

	io->Release();
	return status;
}

avf_status_t CVdbFormat::DoLoadPlaylist(clip_set_t& cs, CReadBuffer& buffer)
{
	if (buffer.aread_le_32() != FCC_RCS0) {
		AVF_LOGE("playlist unknown");
		return E_ERROR;
	}
	buffer.aread_le_32();	// size

	avf_u32_t version = buffer.aread_le_32();
	if (version != 1) {
		AVF_LOGE("playlist version unknown: %d", version);
		return E_ERROR;
	}

	avf_uint_t num_ref_clips = buffer.aread_le_32();
	buffer.aread_le_32();

	avf_uint_t extra_size = buffer.aread_le_32();
	buffer.Skip(extra_size);

	for (avf_uint_t i = 0; i < num_ref_clips; i++) {
		ref_info_t info;
		avf_u32_t clip_attr = 0;
		raw_data_t *scene_data = NULL;
		avf_status_t status;

		if ((status = LoadRefClipInfo(buffer, info, &clip_attr, &scene_data)) != E_OK) {
			AVF_LOGE("error loading playlists");
			return status;
		}

		vdb_clip_t *clip = cs.FindClip(info.clip_id);
		if (clip == NULL) {
			local_time_name_t lname;
			local_time_to_name(info.clip_id, NULL, lname);
			AVF_LOGW("LoadPlaylist: clip %s not found", lname.buf);
		} else {
			ref_clip_t *ref_clip = clip->NewRefClip(info.ref_clip_type,
				info.ref_clip_id, info.start_time_ms, info.end_time_ms,
				clip_attr & ~CLIP_ATTR_LIVE);

			ref_clip->plist_index = info.plist_index;
			if (scene_data) {
				ref_clip->SetSceneData(scene_data);
			}
		}

		avf_safe_release(scene_data);
	}

	return E_OK;
}

avf_status_t CVdbFormat::SavePlaylists(string_t *playlist_dir, IAVIO *io, playlist_set_t& plist_set)
{
	ap<string_t> playlist_name(playlist_dir, PLAYLIST_FILENAME);
	avf_size_t total = 0;
	avf_size_t size = 0;

	struct rb_node *node;

	for (node = rb_first(&plist_set.playlist_set); node; node = rb_next(node)) {
		ref_clip_set_t *rcs = ref_clip_set_t::from_rbnode(node);
		total += rcs->mTotalRefClips;
		size += sizeof(ref_info_t) + 4;
	}

	if (total == 0) {
		AVF_LOGD("no ref clips, remove %s", playlist_name->string());
		avf_remove_file(playlist_name->string(), false);
		return E_OK;
	}

	AVF_LOGD(C_WHITE "save %s" C_NONE, playlist_name->string());
	if (io->CreateFile(playlist_name->string()) != E_OK) {
		AVF_LOGE("cannot open %s", playlist_name->string());
		return E_ERROR;
	}

	CWriteBuffer buffer(io);

	SaveRCSHeader(buffer, total, size);
	for (node = rb_first(&plist_set.playlist_set); node; node = rb_next(node)) {
		ref_clip_set_t *rcs = ref_clip_set_t::from_rbnode(node);
		SaveOnePlaylist(rcs, buffer);
	}

	buffer.Flush();
	io->Close();

	return E_OK;
}

avf_status_t CVdbFormat::SaveOnePlaylist(ref_clip_set_t *rcs, CWriteBuffer& buffer)
{
	avf_uint_t count = 0;
	list_head_t *node;

	list_for_each(node, rcs->ref_clip_list) {
		ref_clip_t *ref_clip = ref_clip_t::from_list_node(node);
		ref_info_t info;

		info.ver_code = FCC_RCLP;
		info.ref_clip_type = rcs->ref_clip_type;
		info.ref_clip_id = ref_clip->ref_clip_id;
		info.clip_id = ref_clip->clip->clip_id;
		info.plist_index = ref_clip->plist_index = count++;
		info.extra_size = 4;
		info.start_time_ms = ref_clip->start_time_ms;
		info.end_time_ms = ref_clip->end_time_ms;

		buffer.write_mem((const avf_u8_t*)&info, sizeof(info));
		buffer.write_le_32(ref_clip->clip_attr);
	}

	return E_OK;
}

static const avf_u32_t K_FCC[kAllArrays] = {
	FCC_PICT,
	FCC_VIDE,
	FCC_GPS0,
	FCC_ACC0,
	FCC_OBD0,
	FCC_VMEM
};

avf_status_t CVdbFormat::SaveSegmentIndex(vdb_clip_t *clip, string_t *filename, IAVIO *io,
	int stream, seg_index_t *sindex, copy_seg_info_s *segi)
{
	avf_status_t status = io->CreateFile(filename->string());
	if (status != E_OK)
		return status;

	CWriteBuffer buffer(io);

	buffer.awrite_begin(4*4);
		buffer.__awrite_le_32(FCC_VDBI);
		buffer.__awrite_le_32(8);
		buffer.__awrite_le_32(2);		// version
		buffer.__awrite_le_32(0);		// extra_data_size
	buffer.awrite_end();

	CIndexArray **arrays = sindex->arrays;
	for (avf_uint_t i = 0; i < ARRAY_SIZE(sindex->arrays); i++) {
		CIndexArray *array = arrays[i];
		if (array && !array->IsEmpty()) {
			if (segi) {
				copy_seg_info_s::index_range_s *r = segi->range + i;
				SaveIndexArray(array, K_FCC[i], buffer, segi->seg_off_ms, r->from, r->to);
			} else {
				SaveIndexArray(array, K_FCC[i], buffer, 0, 0, array->GetCount());
			}
		}
	}

	// todo - for vdb copy
	avf_size_t n = sindex->ex_raw_arrays.count;
	if (n > 0) {
		avf_size_t nfcc = clip->m_extra_raw_fcc.count;
		CIndexArray **parray = sindex->ex_raw_arrays.Item(0);
		for (avf_size_t i = 0; i < n; i++, parray++) {
			CIndexArray *array = *parray;
			avf_size_t count = array->GetCount();	// save even it is empty
			avf_size_t total_size = 4 + 4 + 8 + 4;	// fcc, count, reserved, extra_size
			avf_size_t data_size = array->GetMemSize();

			total_size += (count + 1) * sizeof(index_item_t);	// index items
			total_size += data_size;

			buffer.awrite_begin(20);
				buffer.__awrite_le_32(FCC_RAWD);
				buffer.__awrite_le_32(total_size);
				buffer.__write_be_32(i < nfcc ? *clip->m_extra_raw_fcc.Item(i) : 0);
				buffer.__awrite_le_32(count);
				buffer.__awrite_le_32(0);	// reserved
				buffer.__awrite_le_32(0);	// reserved
				buffer.__awrite_le_32(0);	// extra_size
			buffer.awrite_end();

			buffer.write_mem((const avf_u8_t*)array->GetFirstItem(), (count + 1) * sizeof(index_item_t));
			if (data_size > 0) {
				buffer.write_mem((const avf_u8_t*)array->GetMem(), data_size);
			}
		}
	}

	buffer.Flush();
	io->Close();

	return E_OK;
}

avf_status_t CVdbFormat::SaveIndexItems(CWriteBuffer& buffer, CIndexArray *array,
	avf_u32_t seg_off_ms, avf_uint_t from, avf_uint_t size)
{
	if (size > 0) {
		if (from == 0) {
			buffer.write_mem((const avf_u8_t*)array->GetFirstItem(), size);
		} else {
			const index_item_t *first = array->GetItemAt(from);
			const index_item_t *item = first;
			for (avf_uint_t i = size / sizeof(index_item_t); i > 0; i--, item++) {
				buffer.awrite_begin(2*4);
					buffer.__awrite_le_32(item->time_ms - seg_off_ms);
					buffer.__awrite_le_32(item->fpos - first->fpos);
				buffer.awrite_end();
			}
		}
	}
	return E_OK;
}

avf_status_t CVdbFormat::SaveIndexArray(CIndexArray *array, avf_u32_t fcc, CWriteBuffer& buffer,
	avf_int_t seg_off_ms, avf_uint_t from, avf_uint_t to)
{
	avf_size_t count = to - from;
	avf_size_t size = count * sizeof(index_item_t);

	switch (array->GetType()) {
	case CIndexArray::TYPE_INDEX:
		buffer.awrite_begin(4*8);
			buffer.__awrite_le_32(fcc);
			buffer.__awrite_le_32(24 + size);
			buffer.__awrite_le_32(count);
			buffer.__awrite_le_32(array->GetType());
			buffer.__awrite_le_32(0);	// data_offset
			buffer.__awrite_le_32(0);
			buffer.__awrite_le_32(0);	// reserved
			buffer.__awrite_le_32(0);	// extra_size
		buffer.awrite_end();

		SaveIndexItems(buffer, array, seg_off_ms, from, size);

		break;

	case CIndexArray::TYPE_BLOCK_DATA: {
			buffer.awrite_begin(4*8);
				buffer.__awrite_le_32(fcc);
				buffer.__awrite_le_32(24 + size);
				buffer.__awrite_le_32(count);
				buffer.__awrite_le_32(array->GetType());
				avf_u32_t data_offset = buffer.GetWritePos() + 16 + size;
				buffer.__awrite_le_32(data_offset);
				buffer.__awrite_le_32(0);	// width, height
				buffer.__awrite_le_32(0);	// reserved
				buffer.__awrite_le_32(0);	// extra_size
			buffer.awrite_end();

			SaveIndexItems(buffer, array, seg_off_ms, from, size);

			avf_u32_t data_from = array->GetItemAt(from)->fpos;
			avf_u32_t data_size = array->GetItemAt(to)->fpos - data_from;

			buffer.awrite_begin(4*4);
				buffer.__awrite_le_32(FCC_DATA);
				buffer.__awrite_le_32(8 + data_size);
				buffer.__awrite_le_32(fcc);	// type
				buffer.__awrite_le_32(0);	// extra_size
			buffer.awrite_end();

			avf_uint_t block_offset;
			CMemBuffer *pmb = array->GetMemBuffer();
			CMemBuffer::block_t *block = pmb->GetBlockOf(data_from, &block_offset);

			if (block) {
				const avf_u8_t *ptr = block->GetMem() + block_offset;
				avf_uint_t block_remain = block->GetSize() - block_offset;

				while (true) {
					avf_uint_t tocopy = MIN(data_size, block_remain);
					buffer.write_mem(ptr, tocopy);

					data_size -= tocopy;
					if (data_size == 0)
						break;

					if ((block = block->GetNext()) == NULL) {
						AVF_LOGW("not enough data");
						break;
					}

					ptr = block->GetMem();
					block_remain = block->GetSize();
				}
			}
		}

		break;

	default:
		AVF_LOGE("unknown array type %x", array->GetType());
		break;
	}

	return E_OK;
}

avf_status_t CVdbFormat::LoadSegmentIndex(const char *filename, IAVIO *io, seg_index_t *sindex, vdb_seg_t *seg)
{
	CReadBuffer buffer(io);

	avf_u32_t id = buffer.aread_le_32();
	avf_u32_t size = buffer.aread_le_32();
	avf_u32_t version = buffer.aread_le_32();

	avf_u32_t extra_size = buffer.aread_le_32();
	buffer.Skip(extra_size);

	if (id != FCC_VDBI) {
		AVF_LOGE("bad index file: %s, id: 0x%x", filename, id);
		return E_ERROR;
	}

	if (version != 2) {
		AVF_LOGE("unknown index version %d", version);
		return E_ERROR;
	}

	// extra data skipped
	buffer.SeekTo(size + 8);

	while (true) {
		id = buffer.aread_le_32();
		size = buffer.aread_le_32();

		if (id == 0 || size == 0)
			break;

		avf_u32_t pos = buffer.GetReadPos();
		avf_status_t status;

		switch (id) {
		case FCC_PICT:
			if ((status = LoadArray(sindex->arrays, kPictureArray, buffer)) != E_OK)
				return status;
			break;

		case FCC_VIDE:
			if ((status = LoadArray(sindex->arrays, kVideoArray, buffer)) != E_OK)
				return status;
			break;

		case FCC_GPS0:
			if ((status = LoadArray(sindex->arrays, kGps0Array, buffer)) != E_OK)
				return status;
			break;

		case FCC_ACC0:
			if ((status = LoadArray(sindex->arrays, kAcc0Array, buffer)) != E_OK)
				return status;
			break;

		case FCC_OBD0:
			if ((status = LoadArray(sindex->arrays, kObd0Array, buffer)) != E_OK)
				return status;
			break;

		//case FCC_VMEM:

		default:
			//AVF_LOGD("Skip block %x", id);
			break;
		}

		pos += size;
		if (pos + 8 >= buffer.GetFileSize())
			break;

		buffer.SeekTo(pos);
	}

	CIndexArray *array;

	// update array's tail item
	for (int i = 0; i < kTotalArray; i++) {
		if ((array = sindex->arrays[i]) != NULL) {
			array->UpdateTailItem(seg->m_info.seg_data_size[i], seg->m_info.duration_ms);
		}
	}

	if ((array = sindex->arrays[kVmemArray]) != NULL) {
		array->UpdateTailItem(seg->extra_data_size[0], seg->m_info.duration_ms);
	}

	return E_OK;
}

avf_status_t CVdbFormat::LoadArray(CIndexArray **arrays, int index, CReadBuffer& buffer)
{
	if (arrays[index]) {
		AVF_LOGW("array[%d] already loaded", index);
		return E_OK;
	}

	arrays[index] = CIndexArray::Create(index);
	avf_status_t status = ReadArray(arrays[index], index, buffer);
	if (status != E_OK) {
		avf_safe_release(arrays[index]);
		return status;
	}

	return E_OK;
}

avf_status_t CVdbFormat::ReadArray(CIndexArray *array, int index, CReadBuffer& buffer)
{
	avf_u32_t count = buffer.aread_le_32();
	avf_u32_t type = buffer.aread_le_32();
	avf_u32_t data_offset = buffer.aread_le_32();
	buffer.aread_le_32();	// width, height
	buffer.aread_le_32();	// reserved;

	avf_u32_t extra_size = buffer.aread_le_32();
	buffer.Skip(extra_size);

	if (g_array_attr[index].type != (int)type) {
		AVF_LOGE("bad type %d", type);
		return E_ERROR;
	}

	switch (type) {
	case CIndexArray::TYPE_INDEX:
		break;

	case CIndexArray::TYPE_BLOCK_DATA:
		if (data_offset == 0) {
			AVF_LOGE("bad data_offset");
			return E_ERROR;
		}
		break;

	default:
		break;
	}

	AVF_ASSERT(array->mItems.elems == NULL);

	if (count > 0) {
		// always reserve the last entry
		if (!array->mItems._MakeMinSize(count + 1))
			return E_NOMEM;

		avf_size_t size = count * sizeof(index_item_t);
		if (!buffer.read_mem((avf_u8_t*)array->mItems.elems, size)) {
			array->mItems._Release();
			return E_NOMEM;
		}
		array->mItems.count = count;

		// old version: video is not started from 0, fix it here
		index_item_t *item = array->mItems.elems + 0;
		if (item->fpos != 0) {
			AVF_LOGD(C_LIGHT_PURPLE "item[0].fpos=%d, fix" C_NONE, item->fpos);
			item->fpos = 0;
		}
	}

	if (type == CIndexArray::TYPE_BLOCK_DATA) {
		avf_u32_t pos = (avf_u32_t)buffer.GetReadPos();

		buffer.SeekTo(data_offset);
		avf_status_t status = ReadBlockData(array, buffer);
		buffer.SeekTo(pos);

		if (status != E_OK) {
			AVF_LOGE("read data failed, pos: %d", data_offset);
			return status;
		}
	}

	return E_OK;
}

avf_status_t CVdbFormat::ReadBlockData(CIndexArray *array, CReadBuffer& buffer)
{
	avf_u32_t id = buffer.aread_le_32();
	avf_u32_t size = buffer.aread_le_32();
	buffer.aread_le_32();	// type

	avf_u32_t extra_size = buffer.aread_le_32();
	buffer.Skip(extra_size);

	if (id != FCC_DATA) {
		AVF_LOGE("bad data %x", id);
		return E_ERROR;
	}

	if (size <= 4) {
		AVF_LOGE("no data");
		return E_ERROR;
	}

	size -= 8;	// type, extra_data_size

	avf_u8_t *ptr = array->pmb->Alloc(size);
	if (ptr == NULL) {
		AVF_LOGE("no mem %d", size);
		return E_NOMEM;
	}

	if (!buffer.read_mem(ptr, size)) {
		AVF_LOGE("read data %d failed", size);
		return E_ERROR;
	}

	array->m_data_size = size;

	return E_OK;
}

bool CVdbFormat::VdbFileExists(string_t *path)
{
	ap<string_t> filename(path, VDB_FILENAME);
	return avf_file_exists(filename->string());
}

// TODO: - if the file is corrupted?
avf_status_t CVdbFormat::SaveVdb(string_t *path, IAVIO *io, clip_set_t& cs, clip_dir_set_t& cdir_set)
{
	if (!cs.mbDirty && !cdir_set.mbDirty) {
		AVF_LOGD(C_WHITE "SaveVdb: not dirty" C_NONE);
		return E_OK;
	}

	ap<string_t> filename(path, VDB_FILENAME);
	AVF_LOGD(C_WHITE "save %s" C_NONE, filename->string());

	avf_status_t status = io->CreateFile(filename->string());
	if (status != E_OK) {
		return status;
	}

	CWriteBuffer buffer(io);

	// header
	buffer.awrite_begin(4*6);
		buffer.__awrite_le_32(FCC_VDBF);
		buffer.__awrite_le_32(16 + sizeof(vdb_size_t));
		buffer.__awrite_le_32(1);	// version
		buffer.__awrite_le_32(0);	// reserved
		buffer.__awrite_le_32(cs.mTotalClips);	// TODO:
		buffer.__awrite_le_32(0);	// reserved
	buffer.awrite_end();

	// TODO:
	buffer.write_mem((const avf_u8_t*)&cs.m_size, sizeof(cs.m_size));

	// dir list
	buffer.awrite_begin(4*4);
		buffer.__awrite_le_32(FCC_DIRS);
		buffer.__awrite_le_32(8 + cdir_set.m_num_dirs * 16);
		buffer.__awrite_le_32(1);	// version
		buffer.__awrite_le_32(cdir_set.m_num_dirs);
	buffer.awrite_end();

	list_head_t *node;
	list_for_each(node, cdir_set.m_dir_list) {
		clip_dir_t *cdir = clip_dir_t::from_node(node);
		buffer.awrite_begin(4*4);
			buffer.__awrite_le_32(cdir->m_dirname);
			buffer.__awrite_le_32(cdir->m_num_clips);
			buffer.__awrite_le_32(cdir->m_num_segs_0);
			buffer.__awrite_le_32(cdir->m_num_segs_1);
		buffer.awrite_end();
	}

	// TODO: - last dir

	buffer.Flush();
	io->Close();

	cs.mbDirty = false;
	cdir_set.mbDirty = false;

	return E_OK;
}

avf_status_t CVdbFormat::LoadVdb(string_t *path, clip_set_t& cs, clip_dir_set_t& cdir_set)
{
	ap<string_t> filename(path, VDB_FILENAME);
	AVF_LOGI(C_WHITE "load %s" C_NONE, filename->string());

	IAVIO *io = CSysIO::Create();
	avf_status_t status = io->OpenRead(filename->string());
	if (status == E_OK) {
		AVF_LOGI("DoLoadVdb %s", filename->string());
		CReadBuffer buffer(io);
		status = DoLoadVdb(path, buffer, cs, cdir_set);
	}

	io->Release();
	return status;
}

avf_status_t CVdbFormat::DoLoadVdb(string_t *path, CReadBuffer& buffer, clip_set_t& cs, clip_dir_set_t& cdir_set)
{
	avf_u32_t id = buffer.aread_le_32();
	avf_u32_t size = buffer.aread_le_32();
	avf_u32_t version = buffer.aread_le_32();
	buffer.aread_le_32();	// reserved

	if (id != FCC_VDBF) {
		AVF_LOGE("bad vdb file");
		return E_ERROR;
	}

	if (version != 1) {
		AVF_LOGE("unkown vdb file version %d", version);
		return E_ERROR;
	}

	cs.mTotalClips = buffer.aread_le_32();
	buffer.aread_le_32();	// reserved

	buffer.read_mem((avf_u8_t*)&cs.m_size, sizeof(cs.m_size));

	buffer.SeekTo(size + 8);

	while (true) {
		id = buffer.aread_le_32();
		size = buffer.aread_le_32();

		if (id == 0 || size == 0)
			break;

		avf_u64_t pos = buffer.GetReadPos();
		avf_status_t status;

		switch (id) {
		case FCC_DIRS:
			if ((status = LoadDirs(path, buffer, cdir_set)) != E_OK)
				return status;
			break;

		case FCC_LDIR:
			if ((status = LoadLastDir(path, buffer, cdir_set)) != E_OK)
				return status;
			break;

		default:
			break;
		}

		pos += size;
		if (pos + 8 >= buffer.GetFileSize())
			break;

		buffer.SeekTo(pos);
	}

	return E_OK;
}

avf_status_t CVdbFormat::LoadDirs(string_t *path, CReadBuffer& buffer, clip_dir_set_t& cdir_set)
{
	avf_u32_t version = buffer.aread_le_32();
	avf_u32_t num_dirs = buffer.aread_le_32();

	if (version != 1) {
		AVF_LOGE("unknown DIRS version %d", version);
		return E_ERROR;
	}

	for (avf_size_t i = 0; i < num_dirs; i++) {
		buffer.aread_begin(4*4);
			avf_time_t dirname = buffer.__aread_le_32();
			avf_u32_t num_clips = buffer.__aread_le_32();
			avf_u32_t num_segs_0 = buffer.__aread_le_32();
			avf_u32_t num_segs_1 = buffer.__aread_le_32();
		buffer.aread_end();

		clip_dir_t *dir = cdir_set.AddClipDir(path, dirname, false, NULL);
		dir->m_num_clips = num_clips;
		dir->m_num_segs_0 = num_segs_0;
		dir->m_num_segs_1 = num_segs_1;
	}

	return E_OK;
}

avf_status_t CVdbFormat::LoadLastDir(string_t *path, CReadBuffer& buffer, clip_dir_set_t& cdir_set)
{
	// TODO:
	return E_OK;
}


