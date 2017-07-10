
#ifndef __TS_READER_H__
#define __TS_READER_H__

struct ts_packet_t;
struct packet_list_t;
class CPacketManager;

class CTSReader;
class CTSTrack;

//-----------------------------------------------------------------------
//
//  CPacketManager
//
//-----------------------------------------------------------------------
class CPacketManager: public CObject
{
	typedef CObject inherited;
	friend struct ts_packet_t;

public:
	static CPacketManager* Create();

private:
	CPacketManager();
	virtual ~CPacketManager();

public:
	ts_packet_t *AllocPacket();

private:
	ts_packet_t *m_free_list;
};

//-----------------------------------------------------------------------
//
//  ts_packet_t
//
//-----------------------------------------------------------------------
struct ts_packet_t
{
	avf_atomic_t mRefCount;
	CPacketManager *mManager;
	ts_packet_t *next;
	avf_u8_t *payload;	// payload start
	avf_u8_t pusi;		// pusi flag
	// follows 188 bytes data

	enum {
		SIZE = 188,
	};

	INLINE void AddRef() {
		avf_atomic_inc(&mRefCount);
	}

	INLINE void Release() {
		if (avf_atomic_dec(&mRefCount) == 1) {
			this->next = mManager->m_free_list;
			mManager->m_free_list = this;
		}
	}

	INLINE avf_u8_t *GetData() {
		return (avf_u8_t*)(this + 1);
	}

	INLINE avf_size_t GetRemainSize(avf_u8_t *ptr) {
		return SIZE - (ptr - GetData());
	}

	INLINE avf_size_t GetPayloadSize() {
		return SIZE - (payload - GetData());
	}

	INLINE bool Contains(const avf_u8_t *ptr) {
		return ptr <= GetData() + SIZE;
	}
};

//-----------------------------------------------------------------------
//
//  packet_list_t
//
//-----------------------------------------------------------------------
struct packet_list_t
{
	ts_packet_t *m_head;
	ts_packet_t *m_last;

	INLINE void Init() {
		m_head = NULL;
		m_last = NULL;
	}

	void InsertPacket(ts_packet_t *packet);
	void PushPacket(ts_packet_t *packet);
	ts_packet_t *PopPacket();

	void DoClear();

	INLINE void Clear() {
		if (m_head)
			DoClear();
	}

	INLINE bool IsEmpty() {
		return m_head == NULL;
	}

	avf_size_t CopyToBuffer(avf_u8_t *buffer, avf_size_t size);
};

//-----------------------------------------------------------------------
//
//  CTSReader
//
//-----------------------------------------------------------------------
class CTSReader: public CMediaFileReader, public IReadProgress
{
	typedef CMediaFileReader inherited;
	DEFINE_INTERFACE;

	friend class CTSTrack;

	enum {
		PACKET_SIZE = ts_packet_t::SIZE,
	};

public:
	enum {
		PAT_PID = 0,
		MAX_TRACKS = 4,
		SYNC_BYTE = 0x47,
		PA_TID = 0,
		PM_TID = 2,
		INVALID_PID = -1U,

		AVC_TYPE = 0x1B,
		AAC_TYPE = 0x0F,
	};

	enum {
		PT_PAYLOAD = 0x01,
		PT_ADAPT = 0x02,
	};

public:
	static avf_int_t Recognize(IAVIO *io);

public:
	static IMediaFileReader* Create(IEngine *pEngine, IAVIO *io);

protected:
	CTSReader(IEngine *pEngine, IAVIO *io);
	virtual ~CTSReader();

// IMediaFileReader
public:
	//virtual avf_status_t SetReportProgress(bool bReport);
	virtual avf_status_t OpenMediaSource();

	virtual avf_uint_t GetNumTracks();
	virtual IMediaTrack* GetTrack(avf_uint_t index);

	virtual avf_status_t GetMediaInfo(avf_media_info_t *info);
	virtual avf_status_t GetVideoInfo(avf_uint_t index, avf_video_info_t *info);
	virtual avf_status_t GetAudioInfo(avf_uint_t index, avf_audio_info_t *info);
	virtual avf_status_t GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info);

	virtual bool IsStreamFormat() { return true; }
	virtual bool CanFastPlay();

	virtual avf_status_t SeekToTime(avf_u32_t ms, avf_u32_t *out_ms);
	virtual avf_status_t FetchSample(avf_size_t *pTrackIndex);

// IReadProgress
public:
	virtual avf_status_t GetReadProgress(progress_t *progress);

// CMediaFileReader
private:
	virtual void ReportProgress();

// 
protected:
	virtual avf_status_t PerformSeek(avf_u32_t ms, avf_u32_t *out_ms);

private:
	avf_status_t DoOpenMediaSource(avf_u32_t count, bool bSeekable);
	INLINE void SeekStream(avf_u64_t offset) {
		mpIO->Seek(offset);
		m_remain_bytes = m_total_bytes - offset;
	}
	void BadPacket();

private:
	bool mbFastMode;
	CPacketManager *mpPacketManager;
	IAVIO *mpIO;
	ISeekToTime *mpSeekToTime;
	avf_u64_t m_io_size;
	avf_u64_t m_total_bytes;	// packet aligned
	avf_u64_t m_remain_bytes;	// packet aligned

	CTSTrack *mpTracks[MAX_TRACKS];
	CTSTrack *mpVideoTrack;
	CTSTrack *mpTextTrack;

	avf_size_t mNumTracks;
	avf_size_t mNumVideoTracks;
	avf_size_t mNumAudioTracks;
	avf_size_t mNumTextTracks;

private:
	struct section_t {
		packet_list_t m_packets;
		avf_size_t m_size;		// section size
		avf_size_t m_remain;	// bytes to be read

		INLINE void Init() {
			m_packets.Init();
		}

		INLINE void Clear() {
			m_packets.Clear();
			m_size = 0;
			m_remain = 0;
		}

		INLINE bool IsEmpty() {
			return m_packets.IsEmpty();
		}
	};

	section_t m_pa_section;
	section_t m_pm_section;

	avf_pts_t m_start_pcr;
	avf_pts_t m_end_pcr;	// exclusive

	avf_u64_t m_start_pcr_addr;	// packet aligned
	avf_u64_t m_end_pcr_addr;	// packet aligned

private:
	INLINE ts_packet_t *AllocPacket() {
		return mpPacketManager->AllocPacket();
	}

	INLINE bool ReadPacket(ts_packet_t *packet) {
		return mpIO->ReadAndTest(packet->GetData(), ts_packet_t::SIZE);
	}

private:
	enum {
		A_FIND_PAT = (1 << 0),
		A_FIND_PMT = (1 << 1),
		A_FIND_PCR = (1 << 2),
		_A_FIND_KEY = (1 << 3),
		A_FIND_AAC = (1 << 4),
		A_FIND_PES = (1 << 5),
		A_FIND_PCR_KEY = A_FIND_PCR | _A_FIND_KEY,
	};

private:
	avf_uint_t m_pmt_pid;
	avf_uint_t m_pcr_pid;
	avf_uint_t m_aac_pid;	// audio need be parsed when opening

	// A_FIND_PCR
	avf_pts_t m_pcr_result;
	avf_u64_t m_pcr_addr;	// packet index for PCR
	avf_u32_t m_pcr_frame_len;
	bool mb_pcr_is_keyframe;

	CTSTrack *mpAACTrack;	// temp use
	int m_action;		// action
	avf_size_t m_track_index;	// set when one track has filled a sample

private:
	INLINE bool HasAction(int action) {
		return (m_action & action) != 0;
	}
	INLINE void AddAction(int action) {
		m_action |= action;
	}
	INLINE void ActionDone(int action) {
		m_action &= ~action;
	}

private:
	avf_u32_t GetCheckCount(avf_size_t bytes);

	avf_status_t ReadOnePacket();
	avf_status_t ReadActionPackets(int action);
	avf_status_t ReadActionPacektsBackward(avf_u64_t addr, int action);
	avf_status_t ProcessPacket(ts_packet_t *packet);

	avf_status_t ParseSection(ts_packet_t *packet, section_t *section);

	avf_status_t ParsePAT(avf_u8_t *mem, avf_size_t size);
	avf_status_t ParsePMT(avf_u8_t *mem, avf_size_t size);

	bool CheckKeyFrame(avf_uint_t pid, ts_packet_t *packet);
	avf_status_t AddStream(avf_uint_t type, avf_uint_t pid, avf_u8_t *ptr, avf_size_t es_info_len);

private:
	avf_u32_t get_frame_duration(avf_uint_t pid, ts_packet_t *packet);

private:
	avf_u8_t *mpStreamPacketBuffer;
	avf_u8_t *mpStreamPacketPtr;
	avf_size_t mStreamPacketRemain;
	avf_size_t mStreamPacketCap;
	bool mbParseStream;
	bool mbHideAudio;
};

//-----------------------------------------------------------------------
//
//  CTSTrack
//
//-----------------------------------------------------------------------
class CTSTrack: public CMediaTrack
{
	typedef CMediaTrack inherited;
	friend class CTSReader;

private:
	CTSTrack(CTSReader *pReader, avf_size_t index, int type, avf_uint_t pid);
	virtual ~CTSTrack();

private:
	enum {
		PES_STATE_EMPTY,		// pes not started
		PES_STATE_FILLING,	// filling pes
		PES_STATE_FULL,		// pes ready
	};

	enum {
		TRACK_STATE_RUNNING,		// accept packets. StopTrack() -> STOPPING
		TRACK_STATE_STOPPING,		// ReadSample() will return EOS -> STOPPED
		TRACK_STATE_STOPPED,		// do nothing
	};

// IMediaTrack
public:
	virtual CMediaFormat* QueryMediaFormat();
	virtual avf_status_t SeekToTime(avf_u32_t ms) {
		return E_UNIMP;
	}
	virtual avf_status_t ReadSample(CBuffer *pBuffer, bool& bEOS);

private:
	void ResetTrack();
	bool FeedEOF();
	avf_status_t FeedPacket(ts_packet_t *packet);
	INLINE bool PesFull() {
		return mPesState == PES_STATE_FULL;
	}

private:
	avf_status_t ParseAACHeader(ts_packet_t *packet, bool *pbDone);
	bool CheckKeyFrame(ts_packet_t *packet);
	avf_u32_t GetAVCFlags(const avf_u8_t *p, avf_size_t size);

private:
	bool InitPes();
	void FillPes();
	avf_pts_t read_pts(const avf_u8_t *p);

private:
	CTSReader *mpReader;
	avf_size_t mIndex;
	int mType;
	avf_uint_t m_pid;
	CMediaFormat *mpMediaFormat;

private:
	int mTrackState;
	int mPesState;
	packet_list_t mInputPackets;	// to be processed
	packet_list_t mPesPackets;		// current PES

private:
	avf_u32_t mFrameType;				// for video
	avf_size_t mPesSize;		// increase from 0 to real size for video
	avf_size_t mPesRemain;		// 0 for video
	avf_pts_t m_pts;
	avf_pts_t m_dts;
};

#endif

