
#define LOG_TAG "tsmap"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "mpeg_utils.h"

#include "avio.h"
#include "buffer_io.h"

#include "avf_media_format.h"
#include "tsmap.h"

const char *g_tsmap_guid = "35354844-92FC-48D1-B004-0402C4A17D60";

//-----------------------------------------------------------------------
//
//  CTSMapInfo
//
//-----------------------------------------------------------------------
CTSMapInfo* CTSMapInfo::Create()
{
	CTSMapInfo *result = avf_new CTSMapInfo();
	CHECK_STATUS(result);
	return result;
}

CTSMapInfo::CTSMapInfo()
{
	m_groups._Init();
	m_v_desc = NULL;
	m_a_desc = NULL;
}

CTSMapInfo::~CTSMapInfo()
{
	m_groups._Release();
	avf_safe_release(m_v_desc);
	avf_safe_release(m_a_desc);
}

void *CTSMapInfo::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_ITSMapInfo)
		return static_cast<ITSMapInfo*>(this);
	return inherited::GetInterface(refiid);
}

void CTSMapInfo::SetVideoDesc(raw_data_t *v_desc)
{
	AUTO_LOCK(mMutex);
	avf_assign(m_v_desc, v_desc);
}

void CTSMapInfo::SetAudioDesc(raw_data_t *a_desc)
{
	AUTO_LOCK(mMutex);
	avf_assign(m_a_desc, a_desc);
}

void CTSMapInfo::ReadInfo(map_info_s *info)
{
	AUTO_LOCK(mMutex);

	avf_assign(info->v_desc, m_v_desc);
	avf_assign(info->a_desc, m_a_desc);
	info->group_count = m_groups.count;

	info->group_items = avf_malloc_array(ts_group_item_s, m_groups.count + 1);
	uint32_t size = sizeof(ts_group_item_s) * (m_groups.count + 1);
	::memcpy(info->group_items, m_groups.elems, size);

	ts_group_item_s *group = m_groups.Item(m_groups.count);
	info->ts_size = group->ts_addr;
}

avf_status_t CTSMapInfo::FinishGroup(uint32_t sample_array_addr,
	uint32_t num_samples, int group_index_m4,
	uint32_t group_ts_addr, uint32_t group_ts_size)
{
	AUTO_LOCK(mMutex);

	ts_group_item_s *group = m_groups._Append(NULL, 60, 1);
	if (group == NULL)
		return E_NOMEM;

	group->ts_addr = group_ts_addr;
	group->sample_array_addr = sample_array_addr;
	group->num_samples = num_samples;
	group->group_index_m4 = group_index_m4 & 0x0F;
	group->reserved = 0;

	// tail item
	group++;
	::memset(group, 0, sizeof(ts_group_item_s));
	group->ts_addr = group_ts_addr + group_ts_size;

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CTSMap
//
//-----------------------------------------------------------------------

CTSMap *CTSMap::Create(const tsmap_counter_s& counter)
{
	CTSMap *result = avf_new CTSMap(counter);
	CHECK_STATUS(result);
	return result;
}

CTSMap::CTSMap(const tsmap_counter_s& counter):
	mp_info(NULL),
	m_group_offset(0),
	m_group_size(0),
	m_group_index(0),
	m_start_pts(0),
	mb_has_idr(false),
	m_counter(counter),
	m_wbuf(NULL, 128)
{
	m_v_samples = 0;
	m_a_samples = 0;

	m_samples._Init();
	m_nals._Init();

	CREATE_OBJ(mp_info = CTSMapInfo::Create());
}

CTSMap::~CTSMap()
{
	m_nals._Release();
	m_samples._Release();
	avf_safe_release(mp_info);
}

avf_size_t CTSMap::GetSizeNeeded()
{
	int count = GetGroupCount();

	if (count <= 0)
		return 0;

	// 8: xRAW box
	// 36: guid
	// 12: reserved
	// 4: padding size
	// padding for alignment

	// 4: flags
	// 4 + sizeof(ts_info_s): sizeof(ts_info_s), ts_info_s

	// 4: desc_size
	// 4 + n: v_desc
	// 4 + n: a_desc
	// 4: end (type+length)
	// 4 + n: extra

	// 8: start_pts
	// 4: num_groups_items
	// group items
	// 256: tail 256 bytes

	avf_size_t size = 8 + 36 + 12 + 4 + (0);

	size += 12;	// flags, ts_size, duration_ms

	size += 4;	// desc_size
	size += GetRawDataSize(mp_info->m_v_desc);	// v_desc
	size += GetRawDataSize(mp_info->m_a_desc);	// a_desc
	size += 4;	// end

	size += 4;	// extra

	size += 8;	// start_pts
	size += 4 + count * sizeof(ts_group_item_s);
	size += 256;

	return size;
}

void CTSMap::WriteRawData(CWriteBuffer& wbuf, int type, raw_data_t *raw)
{
	if (raw) {
		avf_size_t size = raw->GetSize();
		wbuf.write_le_16(size);
		wbuf.write_le_16(type);
		wbuf.write_mem(raw->GetData(), size);
		avf_size_t tmp = size & 3;
		if (tmp) {
			wbuf.write_zero(4 - tmp);
		}
	}
}

avf_status_t CTSMap::FinishFile(CWriteBuffer& wbuf,
	uint32_t total_size, uint32_t duration_ms,
	tsmap_counter_s *pCounter)
{
	*pCounter = m_counter;

	int count = GetGroupCount();

	if (count == 0)
		return E_OK;

	uint32_t size = GetSizeNeeded();

	wbuf.write_be_32(total_size);
	wbuf.write_fcc("xRAW");
	wbuf.write_string(g_tsmap_guid, 36);
	wbuf.write_zero(12);	// reserved

	if (total_size > size) {
		avf_size_t padd = total_size - size;
		wbuf.write_le_32(padd);
		wbuf.write_zero(padd);
	} else {
		wbuf.write_le_32(0);
	}

	uint32_t group_data_offset = wbuf.GetWritePos();

	wbuf.write_le_32(0);	// flags
	wbuf.write_le_32(m_group_offset);	// ts_size
	wbuf.write_le_32(duration_ms);

	uint32_t desc_size = GetRawDataSize(mp_info->m_v_desc) +
		GetRawDataSize(mp_info->m_a_desc) + 4;
	wbuf.write_le_32(desc_size);
	WriteRawData(wbuf, ts::RAW_V_DESC, mp_info->m_v_desc);	// v_desc
	WriteRawData(wbuf, ts::RAW_A_DESC, mp_info->m_a_desc);	// a_desc
	wbuf.write_le_32(0);	// end

	wbuf.write_le_32(0);	// extra_size

	wbuf.write_le_64(m_start_pts);
	wbuf.write_le_32(count);
	wbuf.write_mem(mp_info->m_groups.elems, count * sizeof(ts_group_item_s));

	wbuf.write_fcc("xINF");
	wbuf.write_le_32(1);		// version
	wbuf.write_string(g_tsmap_guid, 36);
	wbuf.write_le_32(group_data_offset);	
	wbuf.write_zero(256 - (8 + 36 + 4));

	avf_safe_release(mp_info);
	mb_has_idr = false;

	return E_OK;
}

int CTSMap::DoEndGroup(IAVIO *io)
{
	uint32_t sample_array_addr = io->GetPos();
	avf_size_t size = m_samples.count * sizeof(ts_sample_item_s);

	int rval = io->Write(m_samples.elems, size);
	if (rval != (int)size) {
		return E_IO;
	}

	mp_info->FinishGroup(sample_array_addr, 
		m_v_samples + m_a_samples, m_counter.group,
		m_group_offset, m_group_size);

	m_samples._Clear();

	m_group_offset += m_group_size;
	m_group_size = 0;
	m_counter.group++;
	mb_has_idr = false;

	return size;
}

// group start: idr || m_ts_size == 0
avf_status_t CTSMap::StartGroup(IAVIO *io, uint64_t pts)
{
	if (EndGroup(io) < 0) {
		return E_IO;
	}

	m_group_size = 2 * PACKET_SIZE;	// PAT,PMT
	if (m_group_index == 0) {
		m_start_pts = pts;
	}
	m_group_index++;
	m_group_pts = pts - m_start_pts;

	return E_OK;
}

void CTSMap::AddNal(int nal_type, const uint8_t *ptr, const uint8_t *ptr_end)
{
	nal_desc_s *desc = m_nals._Append(NULL, 7);
	desc->p = ptr;
	desc->size = ptr_end - ptr;
	desc->nal_type = nal_type;

	if (nal_type == 7) {
		m_sps_index = m_nals.count - 1;
		m_m += 4 + desc->size;
		return;
	}

	if (nal_type == 8) {
		m_pps_index = m_nals.count - 1;
		m_m += 4 + desc->size;
		return;
	}

	if (m_m == 0) {
		m_n += 4 + desc->size;
	}
}

avf_status_t CTSMap::WriteSample_AVC(IAVIO *io, const uint8_t *mem, uint32_t size,
	uint64_t pts, int is_idr, sample_info_s *info)
{
	uint32_t start_pos = io->GetPos();

	if (m_group_size == 0) {
		StartGroup(io, pts);
	}

	if (is_idr) {
		if (mb_has_idr) {
			StartGroup(io, pts);
		}
		mb_has_idr = true;
	}

	info->sps = NULL;
	info->pps = NULL;

	const uint8_t *ptr = mem;
	const uint8_t *ptr_end = mem + size;

	const uint8_t *pNAL = avc_find_nal(ptr, ptr_end);
	const uint8_t *pNextNAL;

	if (pNAL >= ptr_end) {
		info->pos_inc = io->GetPos() - start_pos;
		return E_OK;
	}

	// find all NALs
	m_nals._Clear();
	m_sps_index = -1;
	m_pps_index = -1;
	m_m = 0;
	m_n = 0;

	int nal_type;
	while (true) {
		nal_type = *pNAL & 0x1F;
		if (nal_type == 1 || nal_type == 5)
			break;

		if ((pNextNAL = avc_find_nal(pNAL, ptr_end)) >= ptr_end)
			break;

		const uint8_t *p = pNextNAL - 3;
//		if (p[-1] == 0)
//			p--;

		AddNal(nal_type, pNAL, p);

		pNAL = pNextNAL;
	}

	AddNal(nal_type, pNAL, ptr_end);

	// save to IO
	uint32_t sample_addr = io->GetPos();
	m_wbuf.SetIO(io);

	m_wbuf.write_be_8(m_m);
	m_wbuf.write_be_8(m_m == 0 ? 0 : m_n);

	if (m_sps_index >= 0) {
		nal_desc_s *desc = m_nals.Item(m_sps_index);
		m_wbuf.write_be_32(desc->size);
		m_wbuf.write_mem(desc->p, desc->size);
	}
	if (m_pps_index >= 0) {
		nal_desc_s *desc = m_nals.Item(m_pps_index);
		m_wbuf.write_be_32(desc->size);
		m_wbuf.write_mem(desc->p, desc->size);
	}

	info->pos_inc = m_wbuf.GetWritePos() - start_pos;

	for (avf_size_t i = 0; i < m_nals.count; i++) {
		if (i != (avf_size_t)m_sps_index && i != (avf_size_t)m_pps_index) {
			nal_desc_s *desc = m_nals.Item(i);
			m_wbuf.write_be_32(desc->size);
			m_wbuf.write_mem(desc->p, desc->size);
		}
	}

	m_wbuf.Flush();

	info->sps = m_sps_index >= 0 ? m_nals.Item(m_sps_index) : NULL;
	info->pps = m_pps_index >= 0 ? m_nals.Item(m_pps_index) : NULL;

	// add sample
	m_v_samples++;
	ts_sample_item_s *sample = m_samples._Append(NULL, 120, 0);

	sample->mp4_addr = sample_addr;
	sample->sample_size = io->GetPos() - sample_addr - 2;
	sample->group_offset = m_group_size;
	sample->media_type = ts::M_AVC;
	sample->pc_flags = (m_counter.v & 0x0F) | (is_idr ? ts::F_IDR : 0);
	sample->reserved = 0;
	sample->pts_diff = pts - m_start_pts;

	uint32_t pes_size = 8 + 19 + sample->sample_size;	// 8: adaptation; 19: pes header
	uint32_t pes_packets = (pes_size + (PACKET_SIZE-5)) / (PACKET_SIZE-4);	// 4: PacketHeader

	m_counter.v += pes_packets;
	m_group_size += pes_packets * PACKET_SIZE;

	return E_OK;
}

avf_status_t CTSMap::WriteSample_AAC(IAVIO *io, const uint8_t *mem, uint32_t size, uint64_t pts, sample_info_s *info)
{
	uint32_t start_pos = io->GetPos();

	if (m_group_size == 0) {
		StartGroup(io, pts);
		info->pos_inc = io->GetPos() - start_pos;
	} else {
		info->pos_inc = 0;
	}

	aac_adts_header_t header;
	int res = aac_parse_header(mem, size, &header);
	if (res < 0) {
		AVF_LOGE("aac_parse_header: %d", res);
		return E_OK;
	}

	if (!header.crc_absent && header.num_aac_frames > 1) {
		AVF_LOGE("num_aac_frames: %d", header.num_aac_frames);
		return E_OK;
	}

	io->Write(mem, size);

	int header_size = 7 + 2 * !header.crc_absent;
	info->pos_inc += header_size;

	uint32_t pes_size = 14 + size;	// pes header
	uint32_t pes_packets = (pes_size + (PACKET_SIZE-5)) / (PACKET_SIZE-4); // 4: PacketHeader

	m_a_samples++;
	ts_sample_item_s *sample = m_samples._Append(NULL, 120, 0);

	sample->mp4_addr = start_pos;
	sample->sample_size = size;
	sample->group_offset = m_group_size;
	sample->media_type = ts::M_AAC;
	sample->pc_flags = m_counter.a & 0x0F;
	sample->reserved = 0;
	sample->pts_diff = pts - m_start_pts;

	m_counter.a += pes_packets;
	m_group_size += pes_packets * PACKET_SIZE;

	return E_OK;
}

//-----------------------------------------------------------------------
//
//	CTSMapUtil
//
//-----------------------------------------------------------------------
avf_status_t CTSMapUtil::GetGroupOffset(IAVIO *io, uint32_t *p_group_offset)
{
	uint8_t buf[8 + 36 + 4];
	const uint8_t *p;

	avf_status_t status = io->Seek(-256, IAVIO::kSeekEnd);
	if (status != E_OK) {
		return status;
	}

	int rval = io->Read(buf, sizeof(buf));
	if (rval != (int)sizeof(buf)) {
		return E_IO;
	}

	p = buf;

	if (load_be_32(p) != MKFCC('x','I','N','F')) {	// sig
		AVF_LOGE("no signature xINF");
		return E_UNKNOWN;
	}
	p += 4;

	if (load_le_32(p) != 1) {	// version
		AVF_LOGE("unknown version %d", load_le_32(p));
		return E_UNKNOWN;
	}
	p += 4;

	if (::memcmp(p, g_tsmap_guid, 36) != 0) {	// sig
		AVF_LOGE("no signature guid");
		return E_UNKNOWN;
	}
	p += 36;

	*p_group_offset = load_le_32(p);

	return E_OK;
}

avf_status_t CTSMapUtil::ReadGroup(CReadBuffer& rbuf, uint32_t ts_size,
	ts_group_item_s **p_group_items, uint32_t *p_group_count)
{
	uint32_t count = rbuf.read_le_32();

	if ((*p_group_items = avf_malloc_array(ts_group_item_s, count + 1)) == NULL) {
		return E_NOMEM;
	}

	uint32_t size = count * sizeof(ts_group_item_s);
	if (!rbuf.read_mem(*p_group_items, size)) {
		return E_IO;
	}

	*p_group_count = count;

	// tail item
	ts_group_item_s *group = *p_group_items + count;
	::memset(group, 0, sizeof(ts_group_item_s));
	group->ts_addr = ts_size;

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CTSMapIO
//
//-----------------------------------------------------------------------
CTSMapIO *CTSMapIO::Create()
{
	CTSMapIO *result = avf_new CTSMapIO();
	CHECK_STATUS(result);
	return result;
}

CTSMapIO::CTSMapIO():
	mp_io(NULL),

	m_pos(0),
	m_state(STATE_CLOSED),

	mp_sample_buf(NULL),
	m_rbuf(NULL, 256)
{
	m_info.v_desc = NULL;
	m_info.a_desc = NULL;
	m_info.group_count = 0;
	m_info.group_items = NULL;
	m_info.ts_size = 0;

	m_samples._Init();
}

CTSMapIO::~CTSMapIO()
{
	MyClose();
}

void CTSMapIO::MyClose()
{
	avf_safe_release(mp_io);

	m_pos = 0;
	m_state = STATE_CLOSED;

	avf_safe_release(m_info.v_desc);
	avf_safe_release(m_info.a_desc);
	m_info.group_count = 0;
	avf_safe_free(m_info.group_items);
	m_info.ts_size = 0;

	m_samples._Release();

	avf_safe_free(mp_sample_buf);
}

avf_status_t CTSMapIO::Close()
{
	if (m_state == STATE_CLOSED)
		return E_OK;

	MyClose();

	return E_OK;
}

avf_status_t CTSMapIO::OpenTSMap(IAVIO *io, ITSMapInfo *map_info)
{
	MyClose();

	mp_io = CShareReadIO::Create(io);
	if (mp_io == NULL) {
		return E_ERROR;
	}

	map_info->ReadInfo(&m_info);

	if (m_info.group_count) {
		m_state = STATE_SEEKING;
	} else {
		m_state = STATE_EOF;
	}

	m_io_pos = 0;
	m_pos = 0;

	m_group_index = -1;
	mp_group = NULL;

	mb_pat_created = 0;
	mb_pmt_created = 0;

	return E_OK;
}

avf_status_t CTSMapIO::OpenRead(const char *url)
{
	MyClose();

	AVF_LOGD("open " C_GREEN "%s" C_NONE, url);

	if ((mp_io = CSysIO::Create()) == NULL)
		return E_NOMEM;

	avf_status_t status = mp_io->OpenRead(url);
	if (status != E_OK) {
		avf_safe_release(mp_io);
		return status;
	}

	uint32_t info_addr;
	uint32_t flags;
	uint32_t extra_size;

	CReadBuffer rbuf(NULL, 4*KB);

	status = CTSMapUtil::GetGroupOffset(mp_io, &info_addr);
	if (status != E_OK)
		goto Error;

	// -------------------------------
	// flags, groups, extra
	// -------------------------------

	status = ForceSeek(info_addr);
	if (status != E_OK) {
		AVF_LOGE("bad info addr: %d", info_addr);
		goto Error;
	}

	rbuf.SetIO(mp_io);

	// flags
	flags = rbuf.read_le_32();
	if (flags != 0) {
		AVF_LOGE("unknown flags %d\n", flags);
		goto Error;
	}

	// duration_ms
	m_ts_size = rbuf.read_le_32();
	m_duration_ms = rbuf.read_le_32();

	if (!PacketAligned(m_ts_size)) {
		AVF_LOGE("bad ts_size: %d", m_ts_size);
		status = E_DATA;
		goto Error;
	}

	rbuf.read_le_32();	// desc_size
	for (;;) {
		int size = rbuf.read_le_16();
		int type = rbuf.read_le_16();
		if (size == 0)
			break;

		if (size > 0) {
			raw_data_t *raw = avf_alloc_raw_data(size, 0);
			if (raw == NULL) {
				status = E_NOMEM;
				goto Error;
			}

			if (!rbuf.read_mem(raw->GetData(), size)) {
				status = E_IO;
				goto Error;
			}

			avf_size_t t = size & 3;
			if (t) rbuf.Skip(4 - t);

			switch (type) {
			case ts::RAW_V_DESC: avf_assign(m_info.v_desc, raw); break;
			case ts::RAW_A_DESC: avf_assign(m_info.a_desc, raw); break;
			default:
				AVF_LOGD("unknown raw data type %d", type);
				break;
			}

			raw->Release();
		}
	}

	// extra
	extra_size = rbuf.read_le_32();
	rbuf.Skip(extra_size);

	// start_pts
	m_info.start_pts = rbuf.read_le_64();

	// count
	status = CTSMapUtil::ReadGroup(rbuf, m_ts_size, &m_info.group_items, &m_info.group_count);
	if (status != E_OK) {
		goto Error;
	}

	if (m_info.group_count) {
		m_info.ts_size = m_ts_size;
		m_state = STATE_SEEKING;
	} else {
		m_info.ts_size = 0;
		m_state = STATE_EOF;
	}

	m_io_pos = mp_io->GetPos();
	m_pos = 0;

	m_group_index = -1;
	mp_group = NULL;

	mb_pat_created = 0;
	mb_pmt_created = 0;

	return E_OK;

Error:
	MyClose();
	return status;
}

avf_status_t CTSMapIO::ReadRawData(CReadBuffer& rbuf, raw_data_t **raw)
{
	avf_safe_release(*raw);

	avf_size_t sz = rbuf.read_le_32();
	if (sz > 0) {
		raw_data_t *r = avf_alloc_raw_data(sz, 0);
		if (r == NULL)
			return E_NOMEM;

		if (!rbuf.read_mem(r->GetData(), sz))
			return E_IO;

		avf_size_t t = sz & 3;
		if (t) {
			rbuf.Skip(4 - t);
		}

		*raw = r;
	}

	return E_OK;
}

int CTSMapIO::Read(void *buffer, avf_size_t size)
{
	int nread = 0;

	if (size == 0 || !PacketAligned(size)) {
		AVF_LOGE("can only read full packets: %d", size);
		return E_DATA;
	}

Start:
	avf_status_t status;
	int rval = 0;

	switch (m_state) {
	case STATE_CLOSED:
		return E_STATE;

	case STATE_SEEKING:
		if ((status = DoSeek()) != E_OK) {
			m_state = STATE_ERROR;
			return status;
		}
		goto Start;

	case STATE_EOF:
		return nread;

	case STATE_ERROR:
		return E_ERROR;

	case STATE_PAT:
		rval = ReadPAT(buffer, size);
		break;

	case STATE_PMT:
		rval = ReadPMT(buffer, size);
		break;

	case STATE_INIT_SAMPLE:
		if ((status = InitSample()) != E_OK) {
			m_state = STATE_ERROR;
			return status;
		}
		goto Start;

	case STATE_AVC_HEAD:
		rval = Read_AVC_Head(buffer, size);
		break;

	case STATE_AVC_NAL:
		rval = Read_AVC_NAL(buffer, size);
		break;

	case STATE_AAC_HEAD:
		rval = Read_AAC_Head(buffer, size);
		break;

	case STATE_AAC_DATA:
		rval = Read_AAC_Data(buffer, size);
		break;

	default:
		return E_ERROR;
	}

	if (rval < 0) {
		m_state = STATE_ERROR;
		return rval;
	}

	nread += rval;

	if ((m_pos += rval) == m_info.ts_size) {
		AVF_LOGD("eof");
		m_state = STATE_EOF;
	}

	buffer = (uint8_t*)buffer + rval;

	if ((size -= rval) == 0)
		return nread;

	goto Start;
}

int CTSMapIO::ReadPAT(void *buffer, avf_size_t size)
{
	if (!mb_pat_created) {
		mb_pat_created = 1;
		CreatePAT();
	}

	uint8_t *p = m_pat + 3;
	*p = (*p & ~0x0F) | (mp_group->group_index_m4 & 0x0F);

	::memcpy(buffer, m_pat, PACKET_SIZE);

	m_state = STATE_PMT;

	return PACKET_SIZE;
}

int CTSMapIO::ReadPMT(void *buffer, avf_size_t size)
{
	if (!mb_pmt_created) {
		mb_pmt_created = 1;
		CreatePMT();
	}

	uint8_t *p = m_pmt + 3;
	*p = (*p & ~0x0F) | (mp_group->group_index_m4 & 0x0F);

	::memcpy(buffer, m_pmt, PACKET_SIZE);

	m_state = STATE_INIT_SAMPLE;
	m_si.group_start_pos = 2 * PACKET_SIZE;

	return PACKET_SIZE;
}

void CTSMapIO::CreatePAT()
{
	uint8_t *p = m_pat;
	uint32_t tmp;

	*p++ = ts::SYNC_BYTE;

	tmp = (1 << 14) | ts::PAT_PID;
	SAVE_BE_16(p, tmp);

	p[0] = (1<<4) | (0 & 0x0F);	// only payload; counter=0
	p[1] = 0;	// pointer_field
	p += 2;

	uint8_t *p_crc = p;
	*p++ = ts::PAT_TID;
	uint8_t *p_length = p;
	p += 2;		// section length; set later

	SAVE_BE_16(p, 1);	// transport_stream_id

	*p++ = (3<<6) | (0<<1) | 1;	// current_next=1
	SAVE_BE_16(p, 0);	// section number, last section number

	// item
	SAVE_BE_16(p, 1);
	tmp = (7<<13) | ts::PMT_PID;
	SAVE_BE_16(p, tmp);

	// fix section length
	tmp = (1 << 15) | (0 << 14) | (3 << 12) | (p - p_length + (4 - 2));
	SAVE_BE_16(p_length, tmp);

	tmp = avf_crc_ieee(-1, p_crc, p - p_crc);
	SAVE_LE_32(p, tmp);	// **

	::memset(p, 0xFF, PACKET_SIZE - (p - m_pat));
}

void CTSMapIO::CreatePMT()
{
	uint8_t *p = m_pmt;
	uint32_t tmp;

	*p++ = ts::SYNC_BYTE;

	tmp = (1 << 14) | ts::PMT_PID;
	SAVE_BE_16(p, tmp);

	p[0] = (1<<4) | (0 & 0x0F);	// only payload; counter=0
	p[1] = 0;	// pointer_field
	p += 2;

	uint8_t *p_crc = p;
	*p++ = ts::PMT_TID;
	uint8_t *p_length = p;
	p += 2;		// section length; set later

	SAVE_BE_16(p, 1);	// program number

	*p++ = (3<<6) | (0<<1) | 1;	// current_next=1
	SAVE_BE_16(p, 0);	// section number, last section number

	tmp = (7<<13) | ts::VIDEO_PID;	//
	SAVE_BE_16(p, tmp);

	uint8_t *p_info_length = p;
	p += 2;		// program_info_length, set later

	// registration descriptor
	p[0] = 5;	// descriptor tag
	p[1] = 4;	// descriptor length
	p[2] = 'H'; p[3] = 'D'; p[4] = 'M'; p[5] = 'V';
	p += 6;

	// fix program_info_length
	tmp = (15<<12) | (p - p_info_length - 2);
	SAVE_BE_16(p_info_length, tmp);

	if (m_info.v_desc) {
		p = WriteDescriptor(p, m_info.v_desc, ts::VIDEO_PID);
	}

	if (m_info.a_desc) {
		p = WriteDescriptor(p, m_info.a_desc, ts::AUDIO_PID);
	}

	// fix section length
	tmp = (1<<15) | (0<<14) | (3<<12) | (p - p_length + (4-2));
	SAVE_BE_16(p_length, tmp);

	tmp = avf_crc_ieee(-1, p_crc, p - p_crc);
	SAVE_LE_32(p, tmp);	// **

	::memset(p, 0xFF, PACKET_SIZE - (p - m_pmt));
}

// stream_type; length; tag; data[length-3]
uint8_t *CTSMapIO::WriteDescriptor(uint8_t *p, raw_data_t *desc, int pid)
{
	if (desc == NULL)
		return p;

	uint8_t *data = desc->GetData();
	uint32_t tmp;

	*p++ = *data++;	// stream_type

	uint32_t length = load_be_16(data);
	data += 2;

	tmp = (7<<13) | pid;
	SAVE_BE_16(p, tmp);

	tmp = (15<<12) | (length > 3 ? length - 1 : 0);
	SAVE_BE_16(p, tmp);

	if (length > 3) {
		uint32_t len = length - 3;
		*p++ = *data++;	// descriptor_tag
		*p++ = len;
		::memcpy(p, data, len);
		p += len;
	}

	return p;
}

avf_status_t CTSMapIO::DoSeek()
{
	avf_size_t index = FindGroupIndex(m_pos);
	if (index >= m_info.group_count) {
		AVF_LOGE("find group failed, pos %d", m_pos);
		return E_DATA;
	}

	if (m_group_index < 0 || (int)index != m_group_index) {
		// another group
		avf_status_t status = ReadSampleItems(index, m_samples);
		if (status != E_OK) {
			return status;
		}

		m_group_index = index;
		mp_group = GetGroup(index);

		if (!PacketAligned(mp_group->ts_addr)) {
			AVF_LOGE("bad group addr %d", mp_group->ts_addr);
			return E_DATA;
		}

		ts_group_item_s *next = mp_group + 1;
		if (next->ts_addr <= mp_group->ts_addr || !PacketAligned(next->ts_addr)) {
			AVF_LOGE("illegal next group %d", m_group_index);
			return E_DATA;
		}

		m_sample_index = -1;
		mp_sample = NULL;
	}

	uint32_t group_offset = m_pos - mp_group->ts_addr;

	if (group_offset < PACKET_SIZE) {
		m_state = STATE_PAT;
		return E_OK;
	}

	if (group_offset < 2 * PACKET_SIZE) {
		m_state = STATE_PMT;
		return E_OK;
	}

	m_state = STATE_INIT_SAMPLE;
	m_si.group_start_pos = group_offset;

	return E_OK;
}

avf_status_t CTSMapIO::InitSample()
{
	avf_size_t index;

	if ((index = FindSampleIndex(m_si.group_start_pos)) >= m_samples.count)
		return E_ERROR;

	m_sample_index = index;
	mp_sample = GetSample(index);

	if (!PacketAligned(mp_sample->group_offset)) {
		AVF_LOGE("bad sample group_offset: %d", mp_sample->group_offset);
		return E_DATA;
	}

	ts_sample_item_s *next = mp_sample + 1;
	if (next->group_offset <= mp_sample->group_offset || !PacketAligned(next->group_offset)) {
		AVF_LOGE("illegal sample group offset, group %d, sample %d", m_group_index, m_sample_index);
		return E_DATA;
	}

	switch (mp_sample->media_type) {
	case ts::M_AVC:
		return InitSample_AVC();

	case ts::M_AAC:
		return InitSample_AAC();

	default:
		return E_ERROR;
	}
}

avf_status_t CTSMapIO::InitSample_AAC()
{
	uint32_t sample_packet_offset = m_si.group_start_pos - mp_sample->group_offset;

	m_si.packet_index = sample_packet_offset / PACKET_SIZE;
	m_si.pid = ts::AUDIO_PID;
	m_si.stream_id = ts::SID_AUDIO;
	m_si.num_packets_minus1 = SampleSizeTS(mp_sample) / PACKET_SIZE - 1;
	m_si.pts = m_info.start_pts + mp_sample->pts_diff;

	m_si.adapt_size = 0;
	m_si.header_flag = 1 << 4;	// only payload

	// ----------------------------------------
	// pes
	// ----------------------------------------
	m_si.pes_header_size = 14;

	m_si.head_size = m_si.adapt_size + m_si.pes_header_size;
	if (4 + m_si.head_size >= PACKET_SIZE) {
		return E_ERROR;
	}

	uint32_t payload_size = PACKET_SIZE - (4 + m_si.head_size);

	if (m_si.num_packets_minus1 == 0) {
		uint32_t sample_size = mp_sample->sample_size;
		if (payload_size > sample_size) {
			uint32_t stuff = payload_size - sample_size;
			payload_size = sample_size;
			m_si.adapt_size = stuff;
			m_si.header_flag = 3 << 4;	// both
		}
	}

	if (m_si.packet_index == 0) {
		m_state = STATE_AAC_HEAD;
		return E_OK;
	}

	IOSeek(mp_sample->mp4_addr + payload_size);

	m_aac_remain = mp_sample->sample_size - payload_size;
	m_state = STATE_AAC_DATA;

	return E_OK;
}

// sample_packet_offset: [0..n*188)
avf_status_t CTSMapIO::InitSample_AVC()
{
	uint32_t sample_packet_offset = m_si.group_start_pos - mp_sample->group_offset;

	m_si.packet_index = sample_packet_offset / PACKET_SIZE;
	m_si.pid = ts::VIDEO_PID;
	m_si.stream_id = ts::SID_VIDEO;
	m_si.num_packets_minus1 = SampleSizeTS(mp_sample) / PACKET_SIZE - 1;
	m_si.pts = m_info.start_pts + mp_sample->pts_diff;

	// ----------------------------------------
	// adaptation & pes_header
	// ----------------------------------------
	m_si.header_flag = 3 << 4;	// both
	m_si.adapt_size = 2 + 6;	// field length, flags, PCR
	m_si.pes_header_size = 19;

	IOSeek(mp_sample->mp4_addr);
	m_rbuf.SetIO(mp_io);

	uint32_t m = m_rbuf.read_le_8();
	uint32_t n = m_rbuf.read_le_8();

	m_si.m = m;
	m_si.n = n;

	m_si.head_size = m_si.adapt_size + m_si.pes_header_size + m + n;
	if (4 + m_si.head_size >= PACKET_SIZE) {
		return E_ERROR;
	}

	if (mp_sample_buf == NULL && (mp_sample_buf = avf_malloc_bytes(SAMPLE_BUF_SZ)) == NULL) {
		return E_NOMEM;
	}

	m_nal.buf_remain = 0;
	m_nal.nal_remain = 0;
	m_nal.sample_remain = mp_sample->sample_size - (m + n);

	uint32_t payload_size = PACKET_SIZE - (4 + m_si.head_size);

	if (m_si.num_packets_minus1 == 0) {
		uint32_t sample_remain = m_nal.sample_remain;
		if (payload_size > sample_remain) {
			uint32_t stuff = payload_size - sample_remain;
			payload_size = sample_remain;
			m_si.adapt_size += stuff;
		}
	}

	if (m_si.packet_index == 0) {
		m_state = STATE_AVC_HEAD;
		return E_OK;
	}

	uint32_t skip_size = payload_size + (m_si.packet_index - 1) * (PACKET_SIZE - 4);

	m_rbuf.Skip(m + n);
	for (;;) {
		uint32_t nal_len = m_rbuf.read_be_32();
		if (nal_len == 0 || nal_len > m_nal.sample_remain) {
			AVF_LOGE("bad nal_len %d", nal_len);
			return E_DATA;
		}

		uint32_t total = 4 + nal_len;
		if (skip_size >= total) {
			skip_size -= total;
			m_nal.sample_remain -= total;
			m_rbuf.Skip(nal_len);
			continue;
		}

		// in this nal
		if (skip_size < 4) {
			uint8_t *p = mp_sample_buf;
			p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 1;
			p += 4;

			m_nal.buf_offset = skip_size;
			m_nal.buf_remain = 4 - skip_size;
			m_nal.nal_remain = nal_len;
			m_nal.sample_remain -= skip_size;
		} else {
			m_nal.nal_remain = total - skip_size;
			m_nal.sample_remain -= skip_size;
			m_rbuf.Skip(skip_size - 4);
		}

		m_state = STATE_AVC_NAL;
		return E_OK;
	}
}

avf_status_t CTSMapIO::ReadNalBuffer(void *buffer, avf_size_t size)
{
	if (m_nal.buf_remain > 0) {
Copy:
		uint32_t tocopy = MIN(m_nal.buf_remain, size);
		::memcpy(buffer, mp_sample_buf + m_nal.buf_offset, tocopy);

		m_nal.buf_offset += tocopy;
		m_nal.buf_remain -= tocopy;
		m_nal.sample_remain -= tocopy;

		if ((size -= tocopy) == 0)
			return E_OK;

		buffer = (uint8_t*)buffer + tocopy;
	}

	// buf_remain == 0
	if (m_nal.nal_remain > 0) {
		uint32_t toread = MIN(m_nal.nal_remain, SAMPLE_BUF_SZ);
		if (!m_rbuf.read_mem(mp_sample_buf, toread))
			return E_IO;

		m_nal.buf_offset = 0;
		m_nal.buf_remain = toread;
		m_nal.nal_remain -= toread;

		goto Copy;
	}

	if (m_nal.sample_remain == 0)
		return E_ERROR;

	// read nal
	uint32_t nal_len = m_rbuf.read_be_32();
	if (nal_len == 0 || 4 + nal_len > m_nal.sample_remain) {
		AVF_LOGE("bad nal_len %d", nal_len);
		return E_DATA;
	}

	uint8_t *p = mp_sample_buf;
	p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 1;
	p += 4;

	m_nal.buf_offset = 0;
	m_nal.buf_remain = 4;
	m_nal.nal_remain = nal_len;

	goto Copy;
}

int CTSMapIO::Read_AVC_Head(void *buffer, avf_size_t size)
{
	uint8_t *p = (uint8_t*)buffer;

	p[0] = ts::SYNC_BYTE;
	p[1] = (1 << 6) | (m_si.pid >> 8);	// PUSI, pid
	p[2] = m_si.pid;
	p[3] = m_si.header_flag | (mp_sample->pc_flags & 0x0F);
	p += 4;

	// adaptation
	int adapt_flags = 1 << 4;
	if (mp_sample->pc_flags & ts::F_IDR)
		adapt_flags |= 1 << 6;	// random access indicator

	p[0] = m_si.adapt_size - 1;
	p[1] = adapt_flags;
	p += 2;

	uint32_t adapt_remain = m_si.adapt_size - 2;

	uint32_t tmp = (uint32_t)(m_si.pts >> 1);
	SAVE_BE_32(p, tmp);
	tmp = (((uint32_t)m_si.pts & 1) << 15) | (0x3F<<9) | 0;
	SAVE_BE_16(p, tmp);
	adapt_remain -= 6;

	if (adapt_remain) {
		::memset(p, 0xFF, adapt_remain);
		p += adapt_remain;
	}

	// pes header
	p[0] = 0; p[1] = 0; p[2] = 1; p[3] = m_si.stream_id;
	p += 4;

	p[0] = 0; p[1] = 0;
	p += 2;

	p[0] = 0x84;	// '10' 00 0 1 0 0 : data_alignment_indicator
	p[1] = 0xC0;
	p[2] = 10;		// PES_header_data_length
	p += 3;

	// PTS[32..30], marker_bit
	tmp = (uint32_t)(m_si.pts >> 29) & (0x07<<1);
	*p++ = (3<<4) | tmp | 1;	// '0011'

	// PTS[29..15], marker_bit
	tmp = ((uint32_t)m_si.pts >> 14) & (0x7FFF<<1);
	tmp |= 1;
	SAVE_BE_16(p, tmp);

	// PTS[14..0], marker_bit
	tmp = ((uint32_t)m_si.pts << 1) & (0x7FFF<<1);
	tmp |= 1;
	SAVE_BE_16(p, tmp);

	// DTS[32..30], marker_bit
	tmp = (uint32_t)(m_si.pts >> 29) & (0x07<<1);
	*p++ = (1<<4) | tmp | 1;	// '0001'

	// DTS[29..15], marker_bit
	tmp = ((uint32_t)m_si.pts >> 14) & (0x7FFF<<1);
	tmp |= 1;
	SAVE_BE_16(p, tmp);

	// DTS[14..0], marker_bit
	tmp = ((uint32_t)m_si.pts << 1) & (0x7FFF<<1);
	tmp |= 1;
	SAVE_BE_16(p, tmp);

	if (m_si.m + m_si.n > 0) {
		const uint8_t *ptr = m_rbuf.Peek(m_si.m + m_si.n);
		if (ptr == NULL) {
			AVF_LOGE("m + n too large");
			return E_DATA;
		}

		p = CopyNals(p, ptr + m_si.m, m_si.n);
		p = CopyNals(p, ptr, m_si.m);

		m_rbuf.Skip(m_si.m + m_si.n);
	}

	// p < buffer + PACKET_SIZE

	// read nal buffer
	uint32_t payload_size = (uint8_t*)buffer + PACKET_SIZE - p;

	avf_status_t status = ReadNalBuffer(p, payload_size);
	if (status != E_OK)
		return status;

	m_si.packet_index++;

	m_state = STATE_AVC_NAL;
	return PACKET_SIZE;
}

// len + data -> 00 00 00 01 + data
uint8_t *CTSMapIO::CopyNals(uint8_t *p, const uint8_t *pf, avf_size_t size)
{
	while (size > 0) {
		uint32_t len = load_be_32(pf);
		if (len > size) {
			AVF_LOGE("bad nal at " LLD, m_rbuf.GetReadPos());
			return p + size;
		}

		pf += 4;
		size -= 4;

		p[0] = 0;
		p[1] = 0;
		p[2] = 0;
		p[3] = 1;
		p += 4;

		::memcpy(p, pf, len);
		p += len;

		pf += len;
		size -= len;
	}

	return p;
}

avf_status_t CTSMapIO::ReadSampleDone()
{
	// next sample
	if (m_sample_index + 1 < (int)m_samples.count) {
		m_state = STATE_INIT_SAMPLE;
		m_si.group_start_pos = (mp_sample+1)->group_offset;
		return E_OK;
	}

	// next group
	if (m_group_index + 1 < (int)m_info.group_count) {
		m_state = STATE_SEEKING;
		return E_OK;
	}

	// nothing left
	if (m_pos < m_info.ts_size)
		return E_ERROR;

	AVF_LOGD("EOF reached");

	m_state = STATE_EOF;
	return E_OK;
}

int CTSMapIO::Read_AVC_NAL(void *buffer, avf_size_t size)
{
	int nread = 0;

	while (true) {
		if (m_nal.sample_remain == 0) {
			avf_status_t status = ReadSampleDone();
			return status == E_OK ? nread : status;
		}

		uint8_t *p = (uint8_t*)buffer;

		p[0] = ts::SYNC_BYTE;
		p[1] = (0 << 6) | (m_si.pid >> 8);	// PUSI, pid
		p[2] = m_si.pid;
		p += 3;

		// last packet may have stuff
		if (m_si.packet_index == m_si.num_packets_minus1 && (PACKET_SIZE - 4) != m_nal.sample_remain) {

			if (PACKET_SIZE - 4 < m_nal.sample_remain) {
				AVF_LOGE("bad packet");
				return E_DATA;
			}

			uint32_t adapt_size = PACKET_SIZE - 4 - m_nal.sample_remain;

			p[0] = (3 << 4) | ((mp_sample->pc_flags + m_si.packet_index) & 0x0F);
			p[1] = adapt_size - 1;
			p += 2;

			if (--adapt_size) {
				*p++ = 0;	// flags
				if (--adapt_size) {
					::memset(p, 0xFF, adapt_size);
					p += adapt_size;
				}
			}
		} else {
			*p++ = (1 << 4) | ((mp_sample->pc_flags + m_si.packet_index) & 0x0F);
		}

		uint32_t payload_size = (uint8_t*)buffer + PACKET_SIZE - p;

		avf_status_t status = ReadNalBuffer(p, payload_size);
		if (status != E_OK)
			return status;

		nread += PACKET_SIZE;
		m_si.packet_index++;

		if ((size -= PACKET_SIZE) == 0)
			return nread;

		buffer = (uint8_t*)buffer + PACKET_SIZE;
	}
}

int CTSMapIO::Read_AAC_Head(void *buffer, avf_size_t size)
{
	uint8_t *p = (uint8_t*)buffer;

	p[0] = ts::SYNC_BYTE;
	p[1] = (1 << 6) | (m_si.pid >> 8);	// PUSI, pid
	p[2] = m_si.pid;
	p[3] = m_si.header_flag | (mp_sample->pc_flags & 0x0F);
	p += 4;

	// adaptation
	uint32_t adapt_size = m_si.adapt_size;
	if (adapt_size) {
		*p++ = --adapt_size;
		if (adapt_size) {
			*p++ = 0;	// adapt flags
			adapt_size--;
		}
		if (adapt_size) {
			::memset(p, 0xFF, adapt_size);
			p += adapt_size;
		}
	}

	uint32_t tmp;

	// pes header
	p[0] = 0; p[1] = 0; p[2] = 1; p[3] = m_si.stream_id;
	p += 4;

	tmp = m_si.pes_header_size + mp_sample->sample_size - 6;
	SAVE_BE_16(p, tmp);

	p[0] = 0x84;		// '10' 00 0 1 0 0 : data_alignment_indicator
	p[1] = 0x80;
	p[2] = 5;		// PES_header_data_length
	p += 3;

	// PTS[32..30], marker_bit
	tmp = (uint32_t)(m_si.pts >> 29) & (0x07<<1);
	*p++ = (2<<4) | tmp | 1;	// '0010'

	// PTS[29..15], marker_bit
	tmp = ((uint32_t)m_si.pts >> 14) & (0x7FFF<<1);
	tmp |= 1;
	SAVE_BE_16(p, tmp);

	// PTS[14..0], marker_bit
	tmp = ((uint32_t)m_si.pts << 1) & (0x7FFF<<1);
	tmp |= 1;
	SAVE_BE_16(p, tmp);

	// frame
	IOSeek(mp_sample->mp4_addr);
	uint32_t payload_size = (uint8_t*)buffer + PACKET_SIZE - p;

	int rval = mp_io->Read(p, payload_size);
	if (rval != (int)payload_size) {
		return E_IO;
	}

	m_aac_remain = mp_sample->sample_size - payload_size;

	m_si.packet_index++;

	m_state = STATE_AAC_DATA;

	return PACKET_SIZE;
}

int CTSMapIO::Read_AAC_Data(void *buffer, avf_size_t size)
{
	int nread = 0;

	while (true) {
		if (m_aac_remain == 0) {
			avf_status_t status = ReadSampleDone();
			return status == E_OK ? nread : status;
		}

		uint8_t *p = (uint8_t*)buffer;

		p[0] = ts::SYNC_BYTE;
		p[1] = (0 << 6) | (m_si.pid >> 8);	// PUSI, pid
		p[2] = m_si.pid;
		p += 3;

		// last packet may have stuff
		if (m_si.packet_index == m_si.num_packets_minus1 && (PACKET_SIZE - 4) != m_aac_remain) {

			if (PACKET_SIZE - 4 < m_aac_remain) {
				AVF_LOGE("bad aac packet");
				return E_DATA;
			}

			uint32_t adapt_size = PACKET_SIZE - 4 - m_aac_remain;

			p[0] = (3<<4) | ((mp_sample->pc_flags + m_si.packet_index) & 0x0F);
			p[1] = adapt_size - 1;
			p += 2;

			if (--adapt_size) {
				*p++ = 0;	// flags
				if (--adapt_size) {
					::memset(p, 0xFF, adapt_size);
					p += adapt_size;
				}
			}
		} else {
			*p++ = (1<<4) | ((mp_sample->pc_flags + m_si.packet_index) & 0x0F);
		}

		uint32_t payload_size = (uint8_t*)buffer + PACKET_SIZE - p;

		int rval = mp_io->Read(p, payload_size);
		if (rval != (int)payload_size)
			return rval;

		nread += PACKET_SIZE;
		m_si.packet_index++;

		m_aac_remain -= payload_size;

		if ((size -= PACKET_SIZE) == 0)
			return nread;

		buffer = (uint8_t*)buffer + PACKET_SIZE;
	}
}

// [0..count)
// return count: EOS
avf_size_t CTSMapIO::FindGroupIndex(uint32_t pos)
{
	if (pos == 0)
		return 0;

	if (pos >= m_info.ts_size)
		return m_info.group_count;

	ts_group_item_s *group;

	// check the next group
	if (m_group_index >= 0) {
		group = mp_group + 1;
		if (pos == group->ts_addr)
			return m_group_index + 1;
	}

	int a = 0;
	int b = m_info.group_count;

	while (true) {
		int m = (a + b) / 2;
		group = GetGroup(m);
		if (pos >= group->ts_addr) {
			if (a == m)
				return a;
			a = m;
		} else {
			b = m;
		}
	}
}

// [0..count)
// on error return count
avf_size_t CTSMapIO::FindSampleIndex(uint32_t offset)
{
	ts_sample_item_s *sample;

	// check the next sample
	if (m_sample_index >= 0) {
		sample = mp_sample + 1;
		if (offset == sample->group_offset)
			return m_sample_index + 1;
	}

	int a = 0;
	int b = m_samples.count;

	sample = GetSample(a);
	if (offset <= sample->group_offset) {
		if (offset < sample->group_offset) {
			AVF_LOGW("offset is small: %d, %d", offset, sample->group_offset);
			return b;	// error
		}
		return a;
	}

	sample = GetSample(b);
	if (offset >= sample->group_offset) {
		AVF_LOGW("offset is big: %d, %d", offset, sample->group_offset);
		return b;	// error
	}

	while (true) {
		int m = (a + b) / 2;
		sample = GetSample(m);
		if (offset >= sample->group_offset) {
			if (a == m)
				return a;
			a = m;
		} else {
			b = m;
		}
	}
}

avf_status_t CTSMapIO::ReadSampleItems(avf_size_t index, TDynamicArray<ts_sample_item_s>& samples)
{
	ts_group_item_s *group = GetGroup(index);
	uint32_t num_samples = group->num_samples;

	if (index > 0) {
		ts_group_item_s *prev = group - 1;
		num_samples -= prev->num_samples;
	}

	if (!samples._MakeMinSize(num_samples + 1))
		return E_NOMEM;

	avf_status_t status = ForceSeek(group->sample_array_addr);
	if (status != E_OK)
		return status;

	avf_size_t size = num_samples * sizeof(ts_sample_item_s);
	int rval = mp_io->Read(samples.elems, size);
	if (rval != (int)size) {
		return E_IO;
	}
	samples.count = num_samples;

	ts_sample_item_s *sample = samples.elems + samples.count;
	::memset(sample, 0, sizeof(ts_sample_item_s));
	sample->group_offset = GroupSize(group);

	return E_OK;
}

int CTSMapIO::Write(const void *buffer, avf_size_t size)
{
	return -1;
}

avf_status_t CTSMapIO::Seek(avf_s64_t pos, int whence)
{
	avf_u64_t new_pos;

	if (m_state == STATE_CLOSED)
		return E_STATE;

	switch (whence) {
	case kSeekSet:
		new_pos = pos;
		break;

	case kSeekCur:
		new_pos = m_pos + pos;
		break;

	case kSeekEnd:
		new_pos = m_info.ts_size + pos;
		break;

	default:
		AVF_LOGE("bad seek %d", whence);
		return E_INVAL;
	}

	if (new_pos >= m_info.ts_size) {
		if (new_pos > m_info.ts_size) {
			AVF_LOGE("seek beyond file range: " LLD " (" LLD ")", new_pos, pos);
			return E_INVAL;
		}

		m_pos = m_info.ts_size;
		m_state = STATE_EOF;
		return E_OK;
	}

	if ((new_pos % PACKET_SIZE) != 0) {
		AVF_LOGE("can only seek to packet boundary");
		return E_INVAL;
	}

	m_pos = new_pos;
	m_state = STATE_SEEKING;

	return E_OK;
}

