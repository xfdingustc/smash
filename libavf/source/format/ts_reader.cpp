
#define LOG_TAG "ts_reader"

#include <limits.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avio.h"

#include "avf_media_format.h"
#include "mpeg_utils.h"

#include "ts_reader.h"

//-----------------------------------------------------------------------
//
//  CPacketManager
//
//-----------------------------------------------------------------------
CPacketManager* CPacketManager::Create()
{
	CPacketManager *result = avf_new CPacketManager();
	CHECK_STATUS(result);
	return result;
}

CPacketManager::CPacketManager():
	m_free_list(NULL)
{
}

CPacketManager::~CPacketManager()
{
	ts_packet_t *packet = m_free_list;
	while (packet) {
		ts_packet_t *next = packet->next;
		avf_free(packet);
		packet = next;
	}
}

ts_packet_t *CPacketManager::AllocPacket()
{
	ts_packet_t *packet;

	if (m_free_list) {
		packet = m_free_list;
		m_free_list = m_free_list->next;
	} else {
		packet = (ts_packet_t*)avf_malloc(sizeof(ts_packet_t) + ts_packet_t::SIZE);
		if (packet == NULL)
			return NULL;
	}

	packet->mRefCount = 1;
	packet->mManager = this;

	return packet;
}

//-----------------------------------------------------------------------
//
//  packet_list_t
//
//-----------------------------------------------------------------------

void packet_list_t::PushPacket(ts_packet_t *packet)
{
	packet->next = NULL;

	if (m_head == NULL) {
		m_head = packet;
		m_last = packet;
	} else {
		m_last->next = packet;
		m_last = packet;
	}
}

void packet_list_t::InsertPacket(ts_packet_t *packet)
{
	if (m_head == NULL) {
		packet->next = NULL;
		m_head = packet;
		m_last = packet;
	} else {
		packet->next = m_head;
		m_head = packet;
	}
}

ts_packet_t *packet_list_t::PopPacket()
{
	ts_packet_t *packet = m_head;
	if (packet == m_last) {
		m_head = NULL;
		m_last = NULL;
	} else {
		m_head = packet->next;
	}
	return packet;
}

void packet_list_t::DoClear()
{
	ts_packet_t *packet = m_head;
	while (packet) {
		ts_packet_t *next = packet->next;
		packet->Release();
		packet = next;
	}
	Init();
}

avf_size_t packet_list_t::CopyToBuffer(avf_u8_t *buffer, avf_size_t size)
{
	ts_packet_t *packet = m_head;
	for (; packet; packet = packet->next) {
		avf_size_t tocopy = packet->GetPayloadSize();
		if (tocopy > size)
			tocopy = size;
		::memcpy(buffer, packet->payload, tocopy);
		buffer += tocopy;
		size -= tocopy;
		if (size == 0)
			return 0;
	}
	return size;
}

//-----------------------------------------------------------------------
//
//  CTSReader
//
//-----------------------------------------------------------------------

avf_int_t CTSReader::Recognize(IAVIO *io)
{
	// check 3 packets
	avf_size_t i;
	for (i = 0; i < 3; i++) {
		io->Seek(i * PACKET_SIZE);
		if (io->read_be_8() != 0x47)
			return 0;
	}
	return 100;
}

IMediaFileReader* CTSReader::Create(IEngine *pEngine, IAVIO *io)
{
	CTSReader *result = avf_new CTSReader(pEngine, io);
	CHECK_STATUS(result);
	return result;
}

CTSReader::CTSReader(IEngine *pEngine, IAVIO *io):
	inherited(pEngine),
	mbFastMode(false),
	mpPacketManager(NULL),
	mpIO(NULL),
	mpStreamPacketBuffer(NULL),
	mpStreamPacketPtr(NULL),
	mStreamPacketRemain(0),
	mStreamPacketCap(0),
	mbParseStream(false),
	mbHideAudio(false)
{
	m_pa_section.Init();
	m_pm_section.Init();

	CREATE_OBJ(mpPacketManager = CPacketManager::Create());

	mpIO = io->acquire();
	mpSeekToTime = ISeekToTime::GetInterfaceFrom(mpIO);
	m_io_size = io->GetSize();

	m_total_bytes = (m_io_size / PACKET_SIZE) * PACKET_SIZE;
	m_remain_bytes = m_total_bytes;
	ReportProgress();

	SET_ARRAY_NULL(mpTracks);
	mpVideoTrack = NULL;
	mpTextTrack = NULL;

	mNumTracks = 0;
	mNumVideoTracks = 0;
	mNumAudioTracks = 0;
	mNumTextTracks = 0;

	mbHideAudio = pEngine->ReadRegBool(NAME_HIDE_AUDIO, 0, VALUE_HIDE_AUDIO);
}

CTSReader::~CTSReader()
{
	m_pa_section.Clear();
	m_pm_section.Clear();
	SAFE_RELEASE_ARRAY(mpTracks);
	avf_safe_release(mpIO);
	avf_safe_release(mpPacketManager);
	avf_safe_free(mpStreamPacketBuffer);
}

void *CTSReader::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IReadProgress)
		return static_cast<IReadProgress*>(this);
	return inherited::GetInterface(refiid);
}

avf_u32_t CTSReader::GetCheckCount(avf_size_t bytes)
{
	avf_u32_t check_count = bytes / PACKET_SIZE;
	avf_u64_t total_packets = m_total_bytes / PACKET_SIZE;
	if (check_count > total_packets)
		check_count = (avf_u32_t)total_packets;
	return check_count;
}

avf_status_t CTSReader::DoOpenMediaSource(avf_u32_t check_count, bool bSeekable)
{
	avf_status_t status;
	avf_u32_t n;

	AVF_LOGD("parse %d packets", check_count);

	m_action = A_FIND_PAT | A_FIND_PMT | A_FIND_PCR;
	n = check_count;
	while (true) {
		if ((status = ReadOnePacket()) != E_OK) {
			return status;
		}

		if (m_action == 0)
			break;

		if (--n == 0) {
			if (m_action == A_FIND_AAC) {
				m_action = 0;
				AVF_LOGW("no AAC");
				if (mpAACTrack != NULL) {
					for (unsigned i = 0; i < mNumTracks; i++) {
						if (mpTracks[i] == mpAACTrack) {
							mpAACTrack->Release();
							mpAACTrack = NULL;
							for (unsigned j = i; j < mNumTracks - 1; j++) {
								mpTracks[j] = mpTracks[j + 1];
							}
							mpTracks[mNumTracks - 1] = NULL;
							mNumTracks--;
							break;
						}
					}
				}
				break;
			}
			AVF_LOGE("Parse TS failed, action: 0x%x", m_action);
			return E_ERROR;
		}
	}
	m_start_pcr = m_pcr_result;
	m_start_pcr_addr = m_pcr_addr;

	if (mpSeekToTime) {
		m_end_pcr = m_start_pcr + mpSeekToTime->GetLengthMs() * 90;
		m_end_pcr_addr = m_start_pcr_addr;	// todo
	} else if (!bSeekable) {
		// todo
		m_end_pcr = m_start_pcr;
		m_end_pcr_addr = m_start_pcr_addr;
	} else {
		// find the last PCR
		avf_u64_t addr = m_total_bytes - PACKET_SIZE;
		SeekStream(addr);

		m_action = A_FIND_PCR;
		n = check_count;
		while (true) {
			if ((status = ReadOnePacket()) != E_OK)
				return status;

			if (m_action == 0)
				break;

			if (--n == 0) {
				AVF_LOGE("last PCR not found");
				return E_ERROR;
			}

			addr -= PACKET_SIZE;
			SeekStream(addr);
		}
		m_end_pcr = m_pcr_result + m_pcr_frame_len;
		m_end_pcr_addr = m_pcr_addr;

		if (m_end_pcr < m_start_pcr) {
			// wrap around, adjust
			AVF_LOGI("pcr wrap around, start: " LLD ", end: " LLD, m_start_pcr, m_end_pcr);
			m_end_pcr += (1ULL << 33);
		}

		AVF_LOGD("start pcr: " LLD ", addr: " LLD, m_start_pcr, m_start_pcr_addr);
		AVF_LOGD("end pcr: " LLD ", addr: " LLD ", frame: %d", m_end_pcr, m_end_pcr_addr, m_pcr_frame_len);
	}

	return E_OK;
}

avf_status_t CTSReader::OpenMediaSource()
{
	avf_status_t status;

	mNumTracks = 0;
	mNumVideoTracks = 0;
	mNumAudioTracks = 0;
	mNumTextTracks = 0;
	mPercent = 0;

	m_pmt_pid = INVALID_PID;
	m_pcr_pid = INVALID_PID;
	m_aac_pid = INVALID_PID;
	m_pcr_result = -1ULL;
	mpAACTrack = NULL;

	m_pa_section.Clear();
	m_pm_section.Clear();

	avf_safe_free(mpStreamPacketBuffer);

	if (mpIO->CanSeek()) {
		bool bReport = mbReportProgress;
		mbReportProgress = false;

		avf_u32_t check_count = GetCheckCount(512*1024);
		SeekStream(0);
		status = DoOpenMediaSource(check_count, true);

		mbReportProgress = bReport;

	} else {

		avf_u32_t check_count = GetCheckCount(512*1024);
		avf_size_t size = check_count * PACKET_SIZE;
		mpStreamPacketBuffer = avf_malloc_bytes(size);
		if (mpStreamPacketBuffer == NULL) {
			return E_NOMEM;
		}
		if (!mpIO->ReadAndTest(mpStreamPacketBuffer, size)) {
			return E_IO;
		}
		mStreamPacketCap = check_count;
		mStreamPacketRemain = check_count;
		mpStreamPacketPtr = mpStreamPacketBuffer;
		mbParseStream = true;
		status = DoOpenMediaSource(check_count, false);
		mStreamPacketRemain = check_count;
		mpStreamPacketPtr = mpStreamPacketBuffer;
		mbParseStream = false;
	}

	return status;
}

bool CTSReader::CanFastPlay()
{
	return true;
}

avf_status_t CTSReader::SeekToTime(avf_u32_t ms, avf_u32_t *out_ms)
{
	*out_ms = ms;

	for (avf_size_t i = 0; i < mNumTracks; i++) {
		CTSTrack *track = mpTracks[i];
		track->ResetTrack();
	}

	return PerformSeek(ms, out_ms);
}

avf_status_t CTSReader::PerformSeek(avf_u32_t ms, avf_u32_t *out_ms)
{
	if (ms == 0) {
		SeekStream(0);
		return E_OK;
	}

	if (!mpIO->CanSeek()) {
		AVF_LOGE("Cannot seek");
		return E_PERM;
	}

	if (mpSeekToTime) {
		return mpSeekToTime->SeekToTime(ms);
	}

	//avf_u32_t start_ms = 0;
	avf_u64_t start_addr = m_start_pcr_addr;

	avf_u32_t end_ms = (avf_u32_t)((m_end_pcr - m_start_pcr) / 90);
	avf_u64_t end_addr = m_end_pcr_addr;

	if (ms >= end_ms) {
		AVF_LOGE("Seek out of range: %d %d", ms, end_ms);
		return E_INVAL;
	}

	avf_status_t status;
	while (true) {
		avf_u64_t curr_addr = (start_addr + end_addr) / 2;
		avf_u32_t curr_ms;

		curr_addr = curr_addr - (curr_addr % PACKET_SIZE);
		SeekStream(curr_addr);
		if ((status = ReadActionPackets(A_FIND_PCR_KEY)) != E_OK)
			return status;

		curr_ms = (avf_u32_t)((m_pcr_result - m_start_pcr) / 90);
		curr_addr = m_pcr_addr;

		if (curr_addr == end_addr) {
			while (true) {
				if (mb_pcr_is_keyframe && curr_ms <= ms) {
					//AVF_LOGD("Seek to packet " LLD, curr_addr);
					SeekStream(curr_addr);
					return E_OK;
				}
				status = ReadActionPacektsBackward(curr_addr, A_FIND_PCR_KEY);
				if (status != E_OK)
					return status;

				curr_ms = (avf_u32_t)((m_pcr_result - m_start_pcr) / 90);
				curr_addr = m_pcr_addr;
			}
		}

		if (curr_ms < ms) {
			//start_ms = curr_ms;
			start_addr = curr_addr;
		} else {
			end_ms = (avf_u32_t)((m_pcr_result + m_pcr_frame_len - m_start_pcr) / 90);
			end_addr = curr_addr;
		}
	}
}

avf_status_t CTSReader::ReadActionPackets(int action)
{
	m_action = action;
	while (true) {
		avf_status_t status = ReadOnePacket();
		if (status != E_OK)
			return status;
		if (m_action == 0)
			return E_OK;
	}
}

avf_status_t CTSReader::ReadActionPacektsBackward(avf_u64_t addr, int action)
{
	m_action = action;
	while (true) {
		if (addr == 0)
			return E_ERROR;
		addr -= PACKET_SIZE;
		SeekStream(addr);
		avf_status_t status = ReadOnePacket();
		if (status != E_OK)
			return status;
		if (m_action == 0)
			return E_OK;
	}
}

avf_status_t CTSReader::ReadOnePacket()
{
	ts_packet_t *packet;

	if ((packet = AllocPacket()) == NULL)
		return E_NOMEM;

	if (mStreamPacketCap > 0) {
		// get packet from stream buffer
		if (mStreamPacketRemain == 0) {
			avf_uint_t count = (32*1024)/PACKET_SIZE;
			if (count > mStreamPacketCap)
				count = mStreamPacketCap;
			avf_u64_t remain_packets = m_remain_bytes / PACKET_SIZE;
			if (count > remain_packets)
				count = remain_packets;
			if (count == 0) {
				AVF_LOGE("No packet\n");
				return E_INVAL;
			}
			avf_size_t size = count * PACKET_SIZE;
			if (!mpIO->ReadAndTest(mpStreamPacketBuffer, size)) {
				return E_IO;
			}
			mStreamPacketRemain = count;
			mpStreamPacketPtr = mpStreamPacketBuffer;
		}
		::memcpy(packet->GetData(), mpStreamPacketPtr, PACKET_SIZE);
		mpStreamPacketPtr += PACKET_SIZE;
		mStreamPacketRemain--;
	} else {
		if (!ReadPacket(packet)) {
			packet->Release();
			return E_IO;
		}
	}

	avf_status_t status = ProcessPacket(packet);
	m_remain_bytes -= PACKET_SIZE;

	packet->Release();
	return status;
}

void CTSReader::BadPacket()
{
	AVF_LOGE("bad packet, addr: " LLD, m_total_bytes - m_remain_bytes);
}

avf_status_t CTSReader::ProcessPacket(ts_packet_t *packet)
{
	avf_status_t status;

	avf_u8_t *p;
	avf_u32_t tmp;

	int pusi;
	avf_uint_t pid;
	int _playload_type;

	// check sync_byte
	p = packet->GetData();
	if (p[0] != SYNC_BYTE) {
		AVF_LOGD("no sync byte 0x%x", p[0]);
		return E_OK;
	}
	p++;

	// PUSI & pid
	tmp = load_be_16(p);
	pusi = (tmp >> 14) & 1;
	pid = tmp & 0x1FFF;
	p += 2;

	// payload type
	_playload_type = p[0] >> 4;
	p++;

	// skip adaptation field
	if (_playload_type & PT_ADAPT) {
		if (p[1] & 0x10) {
			// PCR
		}
		p += p[0] + 1;	// skip
		if (!packet->Contains(p)) {
			BadPacket();
			return E_ERROR;
		}
	}
	packet->payload = p;
	packet->pusi = pusi;

	if (_playload_type & PT_PAYLOAD) {
		if (HasAction(A_FIND_PES)) {
			for (avf_size_t i = 0; i < mNumTracks; i++) {
				CTSTrack *track = mpTracks[i];
				if (pid == track->m_pid && track->mbTrackEnabled) {
					if (!mbFastMode || track->mpMediaFormat->mMediaType == MEDIA_TYPE_VIDEO) {
						if ((status = track->FeedPacket(packet)) != E_OK)
							return status;
						if (track->PesFull()) {
							m_track_index = i;
							ActionDone(A_FIND_PES);
						}
						return E_OK;
					}
				}
			}

		} else {

			if (pid == PAT_PID) {
				if (HasAction(A_FIND_PAT)) {
					if ((status = ParseSection(packet, &m_pa_section)) != E_OK)
						return status;
				}

			} else if (pid == m_pmt_pid) {
				if (HasAction(A_FIND_PMT)) {
					if ((status = ParseSection(packet, &m_pm_section)) != E_OK)
						return status;
				}

			} else {

				if (pid == m_pcr_pid) {
					if ((_playload_type & PT_ADAPT) && HasAction(A_FIND_PCR_KEY)) {
						p = packet->GetData() + 5;	// adaptation flags
						if (p[0] & 0x10) {	// PCR flag
							avf_u32_t pcr_high = p[1];	// pcr[32..25], 8 bits
							avf_u32_t pcr_low = load_be_32(p + 2) >> 7;	// pcr[24..0], 25 bits
							m_pcr_result = ((avf_pts_t)pcr_high << 25) | pcr_low;
							m_pcr_addr = m_total_bytes - m_remain_bytes;
							m_pcr_frame_len = get_frame_duration(pid, packet);
							mb_pcr_is_keyframe = false;
							if (HasAction(_A_FIND_KEY)) {
								if (pusi && CheckKeyFrame(pid, packet)) {
									mb_pcr_is_keyframe = true;
								}
							}
							ActionDone(A_FIND_PCR_KEY);
						}
					}
				}

				if (pid == m_aac_pid) {
					if (HasAction(A_FIND_AAC)) {
						bool bDone = false;
						status = mpAACTrack->ParseAACHeader(packet, &bDone);
						if (status != E_OK)
							return status;
						if (bDone) {
							m_aac_pid = INVALID_PID;
							mpAACTrack = NULL;
							ActionDone(A_FIND_AAC);
						}
					}
				}
			}
		}
	}

	return E_OK;
}

avf_status_t CTSReader::ParseSection(ts_packet_t *packet, section_t *section)
{
	avf_u8_t *mem;
	avf_u8_t *ptr;
	avf_size_t size;
	avf_status_t status;

	if (packet->pusi) {
		section->Clear();

		ptr = packet->payload + (packet->payload[0] + 1);	// skip pointer field
		if (!packet->Contains(ptr + 3)) {
			BadPacket();
			return E_ERROR;
		}

		// section length from payload start
		section->m_size = (load_be_16(ptr + 1) & 0x3FF) + 3 + (ptr - packet->payload);
		section->m_remain = section->m_size;

	} else {

		if (section->IsEmpty()) {
			AVF_LOGD("no pusi, discard");
			return E_OK;
		}
	}

	packet->AddRef();
	section->m_packets.PushPacket(packet);

	size = packet->GetPayloadSize();
	if (section->m_remain > size) {
		section->m_remain -= size;
		return E_OK;
	}

	if ((mem = avf_malloc_bytes(section->m_size)) == NULL)
		return E_NOMEM;

	section->m_packets.CopyToBuffer(mem, section->m_size);

	if (section == &m_pa_section) {
		status = ParsePAT(mem, section->m_size);
	} else if (section == &m_pm_section) {
		status = ParsePMT(mem, section->m_size);
	} else {
		AVF_LOGE("Cannot be here");
		status = E_OK;
	}

	avf_free(mem);
	section->Clear();

	return status;
}

avf_status_t CTSReader::ParsePAT(avf_u8_t *mem, avf_size_t size)
{
	avf_u8_t *ptr = mem;

	ptr += ptr[0] + 1;	// skip pointer_field
	ptr += 8 + 2;		// to items, skip program_number

	m_pmt_pid = load_be_16(ptr) & 0x1FFF;

	ActionDone(A_FIND_PAT);
	// AVF_LOGD("Parse PA section done");

	return E_OK;
}

avf_status_t CTSReader::ParsePMT(avf_u8_t *mem, avf_size_t size)
{
	avf_u8_t *ptr = mem;
	avf_size_t section_remain;
	avf_size_t pg_info_len;
	avf_size_t descr_len;
	avf_size_t tmp;

	ptr += ptr[0] + 1;	// skip pointer_field
	section_remain = load_be_16(ptr + 1) & 0x3FF;
	m_pcr_pid = load_be_16(ptr + 8) & 0x1FFF;
	ptr += 10;	// to program_info_length

	if (section_remain < 13) {
		AVF_LOGE("bad PMT");
		return E_INVAL;
	}
	section_remain -= 13;	// 9 header + 4 CRC

	// skip descriptors
	pg_info_len = load_be_16(ptr) & 0x0FFF;
	ptr += 2;
	if (pg_info_len) {
		if (pg_info_len > section_remain) {
			AVF_LOGE("bad pg info len");
			return E_INVAL;
		}
		section_remain -= pg_info_len;
		while (true) {
			ptr = mpeg_get_descr_len(ptr + 1, &descr_len);
			ptr += descr_len;	// skip descr

			descr_len += 2;	// tag,len
			if (descr_len >= pg_info_len)
				break;
			pg_info_len -= descr_len;
		}
	}

	// streams
	if (section_remain > 0) {
		while (true) {
			avf_uint_t stream_type = ptr[0];
			avf_uint_t elem_pid = load_be_16(ptr + 1) & 0x1FFF;
			avf_uint_t es_info_len = load_be_16(ptr + 3) & 0x0FFF;
			ptr += 5;

			avf_status_t status = AddStream(stream_type, elem_pid, ptr, es_info_len);
			if (status != E_OK)
				return status;

			ptr += es_info_len;

			tmp = 5 + es_info_len;
			if (section_remain <= tmp)
				break;
			section_remain -= tmp;
		}
	}

	ActionDone(A_FIND_PMT);
	//AVF_LOGD("Parse PM section done");

	return E_OK;
}

bool CTSReader::CheckKeyFrame(avf_uint_t pid, ts_packet_t *packet)
{
	for (avf_size_t i = 0; i < mNumTracks; i++) {
		CTSTrack *track = mpTracks[i];
		if (pid == track->m_pid)
			return track->CheckKeyFrame(packet);
	}
	return false;
}

avf_status_t CTSReader::AddStream(avf_uint_t type, avf_uint_t pid, avf_u8_t *ptr, avf_size_t es_info_len)
{
	if (type != AVC_TYPE && type != AAC_TYPE)
		return E_OK;

	if (type == AAC_TYPE && mbHideAudio) {
		AVF_LOGD(C_WHITE "hide audio" C_NONE);
		return E_OK;
	}

	if (mNumTracks >= MAX_TRACKS) {
		AVF_LOGW("too many tracks");
		return E_OK;
	}

	CTSTrack *track = avf_new CTSTrack(this, mNumTracks, type, pid);
	CHECK_STATUS(track);
	if (track == NULL)
		return E_NOMEM;

	AVF_LOGD("add track %d, type=%d", mNumTracks, type);
	mpTracks[mNumTracks++] = track;

	switch (type) {
	case AVC_TYPE:
		mpVideoTrack = track;
		mNumVideoTracks++;
		if (ptr[0] == 0xFE && ptr[1] >= sizeof(amba_h264_t::info_t)) {	// todo
			CAmbaAVCFormat *pFormat = avf_alloc_media_format(CAmbaAVCFormat);
			//if (pFormat == NULL)
			//	return E_NOMEM;

			::memcpy(&pFormat->data.info, ptr + 2, sizeof(amba_h264_t::info_t));

			pFormat->mMediaType = MEDIA_TYPE_VIDEO;
			pFormat->mFormatType = MF_H264VideoFormat;
			pFormat->mFrameDuration = pFormat->data.info.rate;
			pFormat->mTimeScale = pFormat->data.info.scale;
			pFormat->mWidth = pFormat->data.info.idr_interval >> 16;
			pFormat->mHeight = pFormat->data.info.idr_size >> 16;
			pFormat->mDataFourcc = FCC_AMBA;
			::memset(pFormat->data.reserved, 0, sizeof(pFormat->data.reserved));

			pFormat->data.info.idr_interval &= 0xFFFF;
			pFormat->data.info.idr_size &= 0xFFFF;

			// check
			if (pFormat->mWidth == 0 || pFormat->mHeight == 0 || pFormat->mTimeScale == 0) {
				AVF_LOGE("bad avc info: width=%d, height=%d, timescale=%d",
					pFormat->mWidth, pFormat->mHeight, pFormat->mTimeScale);
				pFormat->Release();
				return E_INVAL;
			}

			track->mpMediaFormat = pFormat;

		} else {

			CH264VideoFormat *pFormat = avf_alloc_media_format(CH264VideoFormat);
			//if (pFormat == NULL)
			//	return E_NOMEM;

			pFormat->mMediaType = MEDIA_TYPE_VIDEO;
			pFormat->mFormatType = MF_H264VideoFormat;
			pFormat->mFrameDuration = 3003;	// TODO
			pFormat->mTimeScale = _90kHZ;	// TODO
			pFormat->mWidth = 1280;	// TODO
			pFormat->mHeight = 720;	// TODO
			pFormat->mDataFourcc = 0;

			track->mpMediaFormat = pFormat;

		}

		break;

	case AAC_TYPE: {
			mNumAudioTracks++;

			CAacFormat *pFormat = avf_alloc_media_format(CAacFormat);
			if (pFormat == NULL)
				return E_NOMEM;

			// CMediaFormat
			pFormat->mMediaType = MEDIA_TYPE_AUDIO;
			pFormat->mFormatType = MF_AACFormat;
			pFormat->mFrameDuration = 0;
			pFormat->mTimeScale = 0;

			// CAudioFormat
			pFormat->mNumChannels = 0;
			pFormat->mSampleRate = 0;
			pFormat->mBitsPerSample = 0;
			pFormat->mBitsFormat = 0;
			pFormat->mFramesPerChunk = 0;

			pFormat->mDecoderDataSize = 0;
			::memset(&pFormat->mDecoderData, 0, sizeof(pFormat->mDecoderData));

			track->mpMediaFormat = pFormat;

			m_aac_pid = pid;	// need be parsed
			mpAACTrack = track;
			AddAction(A_FIND_AAC);
		}
		break;

	default:
		mNumTracks--;
		track->Release();
		break;
	}

	return E_OK;
}

avf_uint_t CTSReader::GetNumTracks()
{
	return mNumTracks;
}

IMediaTrack* CTSReader::GetTrack(avf_uint_t index)
{
	IMediaTrack *track = index >= mNumTracks ? NULL : mpTracks[index];
	return track;
}

avf_status_t CTSReader::GetMediaInfo(avf_media_info_t *info)
{
	info->length_ms = (avf_uint_t)((m_end_pcr - m_start_pcr)/90);
	info->num_videos = mNumVideoTracks;
	info->num_audios = mNumAudioTracks;
	info->num_subtitles = 0;
	return E_OK;
}

avf_status_t CTSReader::GetVideoInfo(avf_uint_t index, avf_video_info_t *info)
{
	if (index == 0 && mpVideoTrack) {
		CVideoFormat *pFormat = (CVideoFormat*)mpVideoTrack->mpMediaFormat;
		if (pFormat) {
			info->width = pFormat->mWidth;
			info->height = pFormat->mHeight;
			info->framerate = pFormat->mFrameRate;
		}
	}
	return E_INVAL;
}

avf_status_t CTSReader::GetAudioInfo(avf_uint_t index, avf_audio_info_t *info)
{
	return E_UNIMP;	// todo
}

avf_status_t CTSReader::GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info)
{
	return E_UNIMP;	// todo
}

// either a sample or EOS
avf_status_t CTSReader::FetchSample(avf_size_t *pTrackIndex)
{
	avf_status_t status;

	// read until get one sample
	m_action = A_FIND_PES;
	while (m_remain_bytes) {
		if ((status = ReadOnePacket()) != E_OK) {
			return status;
		}

		if (m_action == 0) {
			*pTrackIndex = m_track_index;
			ReportProgress();
			return E_OK;
		}
	}

	// EOF
	for (avf_size_t i = 0; i < mNumTracks; i++) {
		CTSTrack *track = mpTracks[i];
		if (track->FeedEOF()) {
			*pTrackIndex = i;
			return E_OK;
		}
	}

	AVF_LOGD("reader stopped");
	return E_ERROR;
}

void CTSReader::ReportProgress()
{
	if (mbReportProgress) {
		DoReportProgress(m_total_bytes, m_remain_bytes);
	}
}

avf_status_t CTSReader::GetReadProgress(progress_t *progress)
{
	ReadProgress(&progress->total_bytes, &progress->remain_bytes);
	return E_OK;
}

avf_u32_t CTSReader::get_frame_duration(avf_uint_t pid, ts_packet_t *packet)
{
	for (avf_size_t i = 0; i < mNumTracks; i++) {
		CTSTrack *track = mpTracks[i];
		if (pid == track->m_pid) {
			switch (track->mType) {
			case AVC_TYPE: {
					// todo - always amba type
					CAmbaAVCFormat *pFormat = (CAmbaAVCFormat*)track->mpMediaFormat;
					return pFormat == NULL ? 0 : pFormat->data.info.rate;
				}
				break;

			case AAC_TYPE: {
					CAacFormat *pFormat = (CAacFormat*)track->mpMediaFormat;
					if (pFormat) {
						return (avf_u64_t)pFormat->mFramesPerChunk*_90kHZ / pFormat->mTimeScale;
					}
					return 0;
				}
				break;

			default:
				return 0;
			}
		}
	}
	return 0;
}

//-----------------------------------------------------------------------
//
//  CTSTrack
//
//-----------------------------------------------------------------------
CTSTrack::CTSTrack(CTSReader *pReader, avf_size_t index, int type, avf_uint_t pid):
	mpReader(pReader),
	mIndex(index),
	mType(type),
	m_pid(pid),
	mpMediaFormat(NULL)
{
	mInputPackets.Init();
	mPesPackets.Init();
	mTrackState = TRACK_STATE_RUNNING;
	mPesState = PES_STATE_EMPTY;
}

CTSTrack::~CTSTrack()
{
	mInputPackets.Clear();
	mPesPackets.Clear();
	avf_safe_release(mpMediaFormat);
}

CMediaFormat* CTSTrack::QueryMediaFormat()
{
	if (mpMediaFormat)
		mpMediaFormat->AddRef();
	return mpMediaFormat;
}

void CTSTrack::ResetTrack()
{
	mInputPackets.Clear();
	mPesPackets.Clear();
	mTrackState = TRACK_STATE_RUNNING;
	mPesState = PES_STATE_EMPTY;
}

// return true: pes ready or EOS
bool CTSTrack::FeedEOF()
{
	switch (mTrackState) {
	case TRACK_STATE_RUNNING:
		if (mPesState == PES_STATE_FILLING) {
			if (mPesRemain == 0) {
				// finish the last video pes
				mPesState = PES_STATE_FULL;
				return true;
			}
		}
		mTrackState = TRACK_STATE_STOPPING;
		return true;

	case TRACK_STATE_STOPPING:
		return true;

	case TRACK_STATE_STOPPED:
	default:
		return false;
	}
}

avf_status_t CTSTrack::FeedPacket(ts_packet_t *packet)
{
	if (mTrackState != TRACK_STATE_RUNNING)
		return E_OK;

	packet->AddRef();
	mInputPackets.PushPacket(packet);

	switch (mPesState) {
	case PES_STATE_EMPTY:
		if (!InitPes())
			break;
		mPesState = PES_STATE_FILLING;
		// fall

	case PES_STATE_FILLING:
		FillPes();
		break;

	case PES_STATE_FULL:
		return E_OK;

	default:
		AVF_ASSERT(0);
		break;
	}

	return E_OK;
}

bool CTSTrack::InitPes()
{
	ts_packet_t *packet;

	// find first packet with pusi
	while (true) {
		if ((packet = mInputPackets.PopPacket()) == NULL)
			return false;
		if (packet->pusi)
			break;
		// todo - check start code
		packet->Release();
	}

	avf_u8_t *p = packet->payload + 4;	// PES_packet_length
	avf_size_t pes_header_data_len;
	int pts_dts;

	mFrameType = 0;
	mPesSize = load_be_16(p);

	p += 3; // PTS_DTS_flags
	pts_dts = (*p >> 6) & 0x03;

	p++;	// PES_header_data_length
	pes_header_data_len = *p;

	p++;	// 0011
	if (pts_dts >= 2) {
		// 10,11
		m_pts = read_pts(p);
		m_dts = pts_dts == 3 ? read_pts(p + 5) : m_pts;
		if (mpMediaFormat->mTimeScale != _90kHZ) {
			m_pts = m_pts * mpMediaFormat->mTimeScale / _90kHZ;
			m_dts = m_dts * mpMediaFormat->mTimeScale / _90kHZ;
		}
	}

	p += pes_header_data_len;
	if (mPesSize) {
		mPesSize -= p - packet->payload - 6;
	}
	mPesRemain = mPesSize;

	// reinsert the packet
	packet->payload = p;
	packet->pusi = 0;
	mInputPackets.InsertPacket(packet);

	if (mpMediaFormat->mMediaType == MEDIA_TYPE_VIDEO) {
		mFrameType = GetAVCFlags(p, packet->GetPayloadSize());
	}

	return true;
}

// read until pes is full, or no input
void CTSTrack::FillPes()
{
	while (true) {
		ts_packet_t *packet;

		if ((packet = mInputPackets.PopPacket()) == NULL)
			return;

		if (packet->pusi) {
			if (mPesRemain != 0) {
				AVF_LOGD("pes remain %d", mPesRemain);
			}
			// should be a video frame
			mInputPackets.InsertPacket(packet);
			mPesState = PES_STATE_FULL;
			// fast-mode: discard non-IDR frames
			if (mpReader->mbFastMode && mFrameType == 0) {
				mPesPackets.Clear();
				mPesState = PES_STATE_EMPTY;
			}
			return;
		}

		mPesPackets.PushPacket(packet);
		avf_size_t packet_pes = packet->GetPayloadSize();

		if (packet_pes >= mPesRemain) {
			if (mPesRemain == 0) {
				// this is a video pes
				mPesSize += packet_pes;
			} else {
				if (packet_pes != mPesRemain) {
					AVF_LOGD("pes remain %d", packet_pes - mPesRemain);
				}
				mPesRemain = 0;
				mPesState = PES_STATE_FULL;
				return;
			}
		} else {
			mPesRemain -= packet_pes;
		}
	}
}

avf_pts_t CTSTrack::read_pts(const avf_u8_t *p)
{
	avf_u32_t t1 = (*p >> 1) & 0x07;	// 32..30
	p++;

	avf_u32_t t2 = load_be_16(p) >> 1;	// 29..15
	p += 2;

	avf_u32_t t3 = load_be_16(p) >> 1;	// 14..0
	p += 2;

	return ((avf_pts_t)t1 << 30) | (t2 << 15) | t3;
}

avf_status_t CTSTrack::ReadSample(CBuffer *pBuffer, bool& bEOS)
{
	switch (mTrackState) {
	case TRACK_STATE_RUNNING:
		if (mPesState != PES_STATE_FULL) {
			AVF_LOGE("Sample not ready");
			return E_ERROR;
		}
		break;

	case TRACK_STATE_STOPPING:
		mTrackState = TRACK_STATE_STOPPED;
		bEOS = true;
		return E_OK;

	case TRACK_STATE_STOPPED:
	default:
		AVF_LOGD("track stopped");
		return E_ERROR;
	}

	avf_status_t status = pBuffer->AllocMem(mPesSize);
	if (status != E_OK)
		return status;

	avf_size_t remain = mPesPackets.CopyToBuffer(pBuffer->mpData, mPesSize);
	if (remain) {
		AVF_LOGD("not enough data %d", remain);
	}
	mPesPackets.Clear();
	mPesState = PES_STATE_EMPTY;

	bEOS = false;
	//pBuffer->mpData = data;
	//pBuffer->mSize = mPesSize;
	pBuffer->mType = CBuffer::DATA;
	pBuffer->mFlags = 0;
	pBuffer->frame_type = 0;
	pBuffer->m_offset = 0;
	pBuffer->m_data_size = mPesSize - remain;
	pBuffer->m_dts = m_dts;
	pBuffer->m_pts = m_pts;
	pBuffer->m_duration = 0;

	switch (mType) {
	case CTSReader::AVC_TYPE:
		pBuffer->mFlags = GetAVCFlags(pBuffer->mpData, mPesSize);
		pBuffer->m_duration = mpMediaFormat->mFrameDuration;
		break;

	case CTSReader::AAC_TYPE:
		pBuffer->mFlags = CBuffer::F_KEYFRAME;
		pBuffer->m_duration = mpMediaFormat->mFrameDuration;
		break;
	}

	return E_OK;
}

avf_status_t CTSTrack::ParseAACHeader(ts_packet_t *packet, bool *pbDone)
{
	avf_u8_t *p = packet->payload + 8;	// skip pes header
	aac_adts_header_t header;

	p += 1 + p[0];	// skip PES_header_data
	if (aac_parse_header(p, packet->GetRemainSize(p), &header) < 0) {
		AVF_LOGE("aac_parse_header failed");
		return E_ERROR;
	}

	CAacFormat *pFormat = (CAacFormat*)mpMediaFormat;

	// CMediaFormat
	pFormat->mFrameDuration = header.samples;
	pFormat->mTimeScale = header.sample_rate;

	// CAudioFormat
	pFormat->mNumChannels = header.chan_config;
	pFormat->mSampleRate = header.sample_rate;
	pFormat->mBitsPerSample = 16;	// todo
	pFormat->mBitsFormat = 0;
	pFormat->mFramesPerChunk = header.samples;

	//
	pFormat->mFlags |= CMediaFormat::F_HAS_EXTRA_DATA;
	pFormat->mDecoderDataSize = 2;
	avf_u32_t data = (2 << 11) |	// AAC-LC, todo
		(header.sampling_index << 7) |
		(pFormat->mNumChannels << 3) | 
		(0 << 2) |	// frame length - 1024 samples
		(0 << 1) |	// does not depend on core coder
		(0);		// is not extension
	pFormat->mDecoderData[0] = data >> 8;
	pFormat->mDecoderData[1] = data;

	// indicate done
	*pbDone = true;
	return E_OK;
}

bool CTSTrack::CheckKeyFrame(ts_packet_t *packet)
{
	switch (mType) {
	case CTSReader::AVC_TYPE: {
			// todo - assume the frame type nal appears in the first packet of pes
			avf_u32_t flags = GetAVCFlags(packet->payload, packet->GetPayloadSize());
			if (flags & CBuffer::F_AVC_IDR)
				return true;
		}
		return false;

	case CTSReader::AAC_TYPE:
		return true;

	default:
		return false;
	}
}

avf_u32_t CTSTrack::GetAVCFlags(const avf_u8_t *p, avf_size_t size)
{
	const avf_u8_t *pend = p + size;

	p = avc_find_nal(p, pend);
	while (p < pend) {
		int nal_type = *p & 0x1F;
		switch (nal_type) {
		case 5:
			return CBuffer::F_KEYFRAME | CBuffer::F_AVC_IDR;
		case 1:
			return 0;
		default:
			break;
		}
		p = avc_find_nal(p, pend);
	}

	return 0;
}

