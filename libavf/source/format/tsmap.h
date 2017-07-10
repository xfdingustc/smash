
#ifndef __TSMAP_H__
#define __TSMAP_H__

#include "sys_io.h"

extern const char *g_tsmap_guid;

class CTSMapInfo;
class CTSMap;
class CTSMapUtil;
class CTSMapIO;

// 20 bytes
struct ts_sample_item_s
{
	uint32_t mp4_addr;
	uint32_t sample_size;
	uint32_t group_offset;	// relative to group start in ts
	uint8_t media_type;
	uint8_t pc_flags;	// 7654: flags; 3210: packet counter
	int16_t reserved;
	int32_t pts_diff;
};

// 12 bytes
struct ts_group_item_s
{
	uint32_t ts_addr;
	uint32_t sample_array_addr;
	uint16_t num_samples;
	uint8_t group_index_m4;
	uint8_t reserved;
};

class ts
{
public:
	enum {
		PACKET_SIZE = 188,
		SYNC_BYTE = 0x47,
	};

	enum {
		PAT_PID = 0,
		PMT_PID = 256,
		VIDEO_PID = 512,
		AUDIO_PID = 513,
	};

	enum {
		PAT_TID = 0,
		PMT_TID = 2,
	};

	enum {
		SID_VIDEO = 0xE0,
		SID_AUDIO = 0xC0,
	};

	enum {
		M_AVC = 1,
		M_AAC = 2,
	};

	enum {
		F_IDR = 1 << 4,
	};

	enum {
		RAW_END = 0,
		RAW_V_DESC = 1,
		RAW_A_DESC = 2,
	};
};

struct tsmap_counter_s
{
	uint32_t v;
	uint32_t a;
	uint32_t group;

	INLINE void Init() {
		v = 0;
		a = 0;
		group = 0;
	}
};

//-----------------------------------------------------------------------
//
//  CTSMapInfo
//
//-----------------------------------------------------------------------
class CTSMapInfo: public CObject, public ITSMapInfo
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

	friend class CTSMap;

public:
	static CTSMapInfo* Create();

private:
	CTSMapInfo();
	virtual ~CTSMapInfo();

// ITSMapInfo
public:
	virtual void ReadInfo(map_info_s *info);

private:
	avf_status_t FinishGroup(uint32_t sample_array_addr, 
		uint32_t num_samples, int group_index_m4, 
		uint32_t group_ts_addr, uint32_t group_ts_size);

	void SetVideoDesc(raw_data_t *v_desc);
	void SetAudioDesc(raw_data_t *a_desc);

private:
	CMutex mMutex;
	TDynamicArray<ts_group_item_s> m_groups;
	raw_data_t *m_v_desc;
	raw_data_t *m_a_desc;
};

//-----------------------------------------------------------------------
//
//  CTSMap
//
//-----------------------------------------------------------------------

class CTSMap: public CObject
{
	typedef CObject inherited;

public:
	static CTSMap *Create(const tsmap_counter_s& counter);

private:
	CTSMap(const tsmap_counter_s& counter);
	virtual ~CTSMap();

public:
	struct nal_desc_s {
		const uint8_t *p;
		uint32_t size;
		int nal_type;
	};

	struct sample_info_s {
		uint32_t pos_inc;
		nal_desc_s *sps;
		nal_desc_s *pps;
	};

public:
	avf_size_t GetSizeNeeded();

	INLINE uint32_t GetFileSize() {
		return m_group_offset + m_group_size;
	}

	INLINE int PreFinishFile(IAVIO *io) {
		return EndGroup(io);
	}

	avf_status_t FinishFile(CWriteBuffer& wbuf,
		uint32_t total_size, uint32_t duration_ms,
		tsmap_counter_s *pCounter);

	INLINE void SetVideoDesc(raw_data_t *v_desc) {
		mp_info->SetVideoDesc(v_desc);
	}

	INLINE void SetAudioDesc(raw_data_t *a_desc) {
		mp_info->SetAudioDesc(a_desc);
	}

	avf_status_t WriteSample_AVC(IAVIO *io, const uint8_t *mem, uint32_t size, uint64_t pts, int is_idr, sample_info_s *info);
	avf_status_t WriteSample_AAC(IAVIO *io, const uint8_t *mem, uint32_t size, uint64_t pts, sample_info_s *info);

	ITSMapInfo *GetMapInfo() {
		return mp_info;
	}

private:
	void AddNal(int nal_type, const uint8_t *ptr, const uint8_t *ptr_end);

private:
	INLINE int GetGroupCount() {
		return mp_info ? mp_info->m_groups.count : 0;
	}

	avf_status_t StartGroup(IAVIO *io, uint64_t pts);

	INLINE int EndGroup(IAVIO *io) {
		return mb_has_idr ? DoEndGroup(io) : 0;
	}

	int DoEndGroup(IAVIO *io);

	INLINE avf_size_t GetRawDataSize(raw_data_t *raw) {
		return raw ? (4 + ((raw->GetSize() + 3) & ~3)) : 0;
	}

	void WriteRawData(CWriteBuffer& wbuf, int type, raw_data_t *raw);

private:
	enum {
		PACKET_SIZE = ts::PACKET_SIZE,
	};

private:
	CTSMapInfo *mp_info;
	uint32_t m_group_offset;
	uint32_t m_group_size;
	uint32_t m_group_index;
	uint32_t m_group_pts;
	uint64_t m_start_pts;
	bool mb_has_idr;

	tsmap_counter_s m_counter;

	uint32_t m_v_samples;
	uint32_t m_a_samples;
	TDynamicArray<ts_sample_item_s> m_samples;

	TDynamicArray<nal_desc_s> m_nals;
	int m_sps_index;		// tmp
	int m_pps_index;		// tmp
	uint32_t m_m;	// tmp
	uint32_t m_n;	// tmp
	CWriteBuffer m_wbuf;
};

//-----------------------------------------------------------------------
//
//	CTSMapUtil
//
//-----------------------------------------------------------------------
class CTSMapUtil
{
public:
	static avf_status_t GetGroupOffset(IAVIO *io, uint32_t *p_group_offset);
	static avf_status_t ReadGroup(CReadBuffer& rbuf, uint32_t ts_size,
		ts_group_item_s **p_group_items, uint32_t *p_group_count);
};

//-----------------------------------------------------------------------
//
//  CTSMapIO - can only read from packet boundary and full packets
//
//-----------------------------------------------------------------------

class CTSMapIO: public CAVIO
{
	typedef CAVIO inherited;

public:
	static CTSMapIO *Create();

private:
	CTSMapIO();
	virtual ~CTSMapIO();

public:
	avf_status_t OpenTSMap(IAVIO *io, ITSMapInfo *map_info);

// IAVIO
public:
	virtual avf_status_t CreateFile(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenRead(const char *url);
	virtual avf_status_t OpenWrite(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenExisting(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t Close();
	virtual bool IsOpen() { return m_state != STATE_CLOSED; }

	virtual const char *GetIOType() { return "tsmap"; }

	virtual int Read(void *buffer, avf_size_t size);
	virtual int Write(const void *buffer, avf_size_t size);

	virtual avf_status_t Flush() {
		return E_ERROR;
	}

	virtual bool CanSeek() {
		return true;
	}
	virtual avf_status_t Seek(avf_s64_t pos, int whence);

	virtual avf_u64_t GetSize() {
		return m_info.ts_size;
	}
	virtual avf_u64_t GetPos() {
		return m_pos;
	}

private:
	void MyClose();

	INLINE avf_status_t ForceSeek(uint32_t pos) {
//		m_io_pos = pos;
		return mp_io->Seek(pos);
	}

	INLINE avf_status_t IOSeek(uint32_t pos) {
		return mp_io->Seek(pos);
//		return pos == m_io_pos ? E_OK : ForceSeek(pos);
	}

	INLINE int PacketAligned(uint32_t addr) {
		return ((addr % PACKET_SIZE) == 0);
	}

	avf_status_t ReadRawData(CReadBuffer& rbuf, raw_data_t **raw);

	avf_status_t DoSeek();

	INLINE ts_group_item_s *GetGroup(int index) {
		return m_info.group_items + index;
	}

	INLINE ts_sample_item_s *GetSample(int index) {
		return m_samples.elems + index;
	}

	INLINE avf_size_t GroupSize(ts_group_item_s *group) {
		return (group + 1)->ts_addr - group->ts_addr;
	}

	INLINE avf_size_t SampleSizeTS(ts_sample_item_s *sample) {
		return (sample + 1)->group_offset - sample->group_offset;
	}

	avf_size_t FindGroupIndex(uint32_t pos);
	avf_size_t FindSampleIndex(uint32_t offset);

	avf_status_t ReadSampleItems(avf_size_t index, TDynamicArray<ts_sample_item_s>& samples);

	int ReadPAT(void *buffer, avf_size_t size);
	int ReadPMT(void *buffer, avf_size_t size);
	uint8_t *WriteDescriptor(uint8_t *p, raw_data_t *desc, int pid);

	avf_status_t InitSample();
	avf_status_t InitSample_AVC();
	avf_status_t InitSample_AAC();

	uint8_t *CopyNals(uint8_t *p, const uint8_t *pf, avf_size_t size);
	int Read_AVC_Head(void *buffer, avf_size_t size);
	int Read_AVC_NAL(void *buffer, avf_size_t size);

	int Read_AAC_Head(void *buffer, avf_size_t size);
	int Read_AAC_Data(void *buffer, avf_size_t size);

	avf_status_t ReadSampleDone();

	avf_status_t ReadNalBuffer(void *buffer, avf_size_t size);

	void CreatePAT();
	void CreatePMT();

private:
	enum {
		PACKET_SIZE = ts::PACKET_SIZE,
	};

	enum {
		STATE_CLOSED = 0,
		STATE_SEEKING,
		STATE_EOF,
		STATE_ERROR,

		STATE_PAT,
		STATE_PMT,

		STATE_INIT_SAMPLE,

		STATE_AVC_HEAD,
		STATE_AVC_NAL,

		STATE_AAC_HEAD,
		STATE_AAC_DATA,
	};

	enum {
		SAMPLE_BUF_SZ = 64*1024,
	};

	struct sample_info_s {
		uint32_t group_start_pos;	// STATE_INIT_SAMPLE
		///////////////////////////
		uint32_t packet_index;
		///////////////////////////
		uint16_t pid;
		uint16_t stream_id;
		uint32_t num_packets_minus1;
		uint64_t pts;
		///////////////////////////
		uint8_t header_flag;
		uint16_t adapt_size;	// first packet not 0
		uint16_t pes_header_size;
		uint16_t m;
		uint16_t n;
		uint32_t head_size;	// adapt + pes_header + m + n
	};

	struct nal_buf_info_s {
		uint32_t buf_offset;
		uint32_t buf_remain;
		uint32_t nal_remain;
		uint32_t sample_remain;
	};

private:
	IAVIO *mp_io;

	uint32_t m_io_pos;
	uint32_t m_pos;

	int m_state;

	sample_info_s m_si;
	uint8_t *mp_sample_buf;
	CReadBuffer m_rbuf;
	nal_buf_info_s m_nal;
	uint32_t m_aac_remain;

	int m_group_index;	// -1; [0..count)
	ts_group_item_s *mp_group;

	int m_sample_index;	// -1; [0..count)
	ts_sample_item_s *mp_sample;

	uint32_t m_ts_size;
	uint32_t m_duration_ms;

	ITSMapInfo::map_info_s m_info;
	TDynamicArray<ts_sample_item_s> m_samples;

	uint8_t mb_pat_created;
	uint8_t mb_pmt_created;

	uint8_t m_pat[PACKET_SIZE];
	uint8_t m_pmt[PACKET_SIZE];
};

#endif

