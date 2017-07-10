
#ifndef __MP4_BUILDER_H__
#define __MP4_BUILDER_H__

struct raw_data_t;
class CMP4Builder;

//-----------------------------------------------------------------------
//
//  CMP4Builder
//
//-----------------------------------------------------------------------
class CMP4Builder: public CAVIO
{
	typedef CAVIO inherited;

public:
	static CMP4Builder* Create(bool bForceBigFile = false);

private:
	CMP4Builder(bool bForceBigFile);
	virtual ~CMP4Builder();

public:
	enum {
		STATE_IDLE = 0,
		STATE_PARSING = 1,
		STATE_READY = 2,
		STATE_ERROR = 3,
	};

	enum {
		S_ftyp,
		S_moov,
		S_mvhd,

		S_trak,
		S_tkhd,
		S_edts,
		S_mdia,
		S_mdhd,
		S_hdlr,
		S_minf,
		S_xmhd,
		S_dinf,
		S_stbl,
		S_stsd,
		S_stts,
		S_stts_data,
		S_stss,
		S_stss_data,
		S_stsc,
		S_stsz,
		S_stsz_data,
		S_stco,
		S_stco_data,

		S_udta,
		S_mdat,
		S_mdat_data,
		S_eof,
	};

	enum {
		VIDEO,
		AUDIO,
	};

	struct mp4_track_info_s
	{
		uint16_t	width;
		uint16_t	height;
		uint32_t	timescale;

		uint32_t	sample_start;

		uint32_t	sample_start_index;
		uint32_t	sample_end_index;
		uint32_t	sample_count;
		uint32_t	duration_pts;

		uint32_t	stss_fpos;
		uint32_t	stss_count;

		uint32_t	stsz_fpos;
		uint32_t	stco_fpos;

		uint32_t	first_sample_fpos;
		uint32_t	last_sample_fpos;
		uint32_t	last_sample_size;
	};

	struct out_track_info_s
	{
		int			media_type;

		uint32_t	trak_size;
		uint32_t	trak_total_size;

		uint32_t	tkhd_size;
		uint32_t	edts_size;

		uint32_t	mdia_size;
		uint32_t	mdia_total_size;

		uint32_t	mdhd_size;
		uint32_t	hdlr_size;

		uint32_t	minf_size;
		uint32_t	minf_total_size;

		uint32_t	xmhd_size;
		uint32_t	dinf_size;

		uint32_t	stbl_size;
		uint32_t	stbl_total_size;

		uint32_t	stsd_size;

		uint32_t	stts_size;
		uint32_t	stts_total_size;

		uint32_t	stss_size;
		uint32_t	stss_total_size;
		uint32_t	stss_count;

		uint32_t	stsc_size;

		uint32_t	stsz_size;
		uint32_t	stsz_total_size;

		uint32_t	stco_size;
		uint32_t	stco_total_size;
	};

	struct mp4_info_s
	{
		uint32_t	creation_time;
		uint32_t	modification_time;

		uint32_t	duration_ms;	// ***
		uint32_t	timescale;

		uint32_t	v_timescale;
		uint32_t	a_timescale;

		uint64_t	v_duration_pts;
		uint64_t	a_duration_pts;

		uint16_t	video_width;
		uint16_t	video_height;

		uint32_t	total_v_samples;
		uint32_t	total_a_samples;
		uint64_t	total_mdata_size;

		uint32_t	ftyp_size;

		uint32_t	moov_size;
		uint32_t	moov_total_size;

		uint32_t	mvhd_size;
		uint32_t	udta_size;

		uint32_t	mdat_fpos;
		uint32_t	mdat_size;
		uint64_t	mdat_total_size;

		out_track_info_s	v;
		out_track_info_s	a;
	};

	struct range_s {
		int32_t	start_time_ms;
		int32_t	end_time_ms;
		bool b_set_duration;
	};

public:
	int GetState() { return mState; }
	avf_status_t AddFile(const char *pFileName, const range_s *range);
	void SetDate(uint32_t creation_time, uint32_t modification_time);
	avf_status_t FinishAddFile();

// IAVIO
public:
	virtual avf_status_t CreateFile(const char *url) { return E_ERROR; }
	virtual avf_status_t OpenRead(const char *url) { return E_ERROR; }
	virtual avf_status_t OpenWrite(const char *url) { return E_ERROR; }
	virtual avf_status_t OpenExisting(const char *url) { return E_ERROR; }
	virtual avf_status_t Close();
	virtual bool IsOpen() { return mState != STATE_IDLE; }

	virtual const char *GetIOType() { return "mp4builder"; }

	virtual int Read(void *buffer, avf_size_t size);
	virtual int Write(const void *buffer, avf_size_t size) { return E_ERROR; }
	virtual avf_status_t Flush() { return E_ERROR; }

	virtual int ReadAt(void *buffer, avf_size_t size, avf_u64_t pos);
	virtual int WriteAt(const void *buffer, avf_size_t size, avf_u64_t pos) { return E_ERROR; }

	virtual bool CanSeek() { return true; }
	virtual avf_status_t Seek(avf_s64_t pos, int whence = kSeekSet);

	virtual avf_u64_t GetSize();
	virtual avf_u64_t GetPos();

private:
	avf_status_t ParseFile(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseMovie(CReadBuffer& rbuf, avf_size_t box_remain);

	avf_status_t ParseMovieHeader(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseTrack(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseUserData(CReadBuffer& rbuf, avf_size_t box_remain);

	avf_status_t ParseTrackHeader(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseMedia(CReadBuffer& rbuf, avf_size_t box_remain);

	avf_status_t ParseMediaHeader(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseMediaInformation(CReadBuffer& rbuf, avf_size_t box_remain);

	avf_status_t ParseVideoMediaHeader(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseSoundMediaHeader(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseSampleTable(CReadBuffer& rbuf, avf_size_t box_remain);

	avf_status_t ParseSampleDescription(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseTimeToSample(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseSyncSample(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseSampleToChunk(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseSampleSize(CReadBuffer& rbuf, avf_size_t box_remain);
	avf_status_t ParseChunkOffset(CReadBuffer& rbuf, avf_size_t box_remain);

private:
	typedef void (CMP4Builder::*WriteBoxData_proc)(uint8_t *p);
	typedef void (CMP4Builder::*WriteArray_proc)(uint8_t *p, avf_size_t nitems);

private:
	void WriteFileType(uint8_t *p);
	void WriteMovie(uint8_t *p);
	void WriteMediaData(uint8_t *p);

	void WriteMovieHeader(uint8_t *p);
	void WriteTrack(uint8_t *p);
	void WriteUserData(uint8_t *p);

	void WriteTrackHeader(uint8_t *p);
	void WriteEditList(uint8_t *p);
	void WriteMedia(uint8_t *p);

	void WriteMediaHeader(uint8_t *p);
	void WriteHandlerReference(uint8_t *p);
	void WriteMediaInformation(uint8_t *p);

	void WriteAVMediaHeader(uint8_t *p);
	void WriteDataInformation(uint8_t *p);
	void WriteSampleTable(uint8_t *p);

	void WriteSampleDescription(uint8_t *p);

	void WriteTimeToSample(uint8_t *p);
	void write_stts(uint8_t *p, avf_size_t nitems);

	void WriteSyncSample(uint8_t *p);
	void write_stss(uint8_t *p, avf_size_t nitems);

	void WriteSampleToChunk(uint8_t *p);

	void WriteSampleSize(uint8_t *p);
	void write_stsz(uint8_t *p, avf_size_t nitems);

	void WriteChunkOffset(uint8_t *p);
	void write_stco(uint8_t *p, avf_size_t nitems);

private:
	struct stts_entry_s {
		uint32_t	sample_count;
		uint32_t	sample_delta;
	};

	struct file_info_s {
		char *filename;
		range_s range;
		uint32_t mdata_size;
		uint32_t mdata_orig_start;
		uint64_t mdata_new_start;
	};

private:
	void InitParser();

	uint8_t *DoGetBuffer(avf_size_t size);
	INLINE uint8_t *GetBuffer(avf_size_t size) {
		return size <= m_buf_size ? m_buf : DoGetBuffer(size);
	}

	uint8_t *DoGetAuxBuffer(avf_size_t size);
	INLINE uint8_t *GetAuxBuffer(avf_size_t size) {
		return size <= m_aux_buf_size ? m_aux_buf : DoGetAuxBuffer(size);
	}

	bool ReadData(void *buffer, avf_size_t buffer_size,
		WriteBoxData_proc write_proc, avf_size_t data_size, avf_size_t *nread);

	bool ReadArray(void *buffer, avf_size_t buffer_size,
		WriteArray_proc write_proc, avf_size_t *nread);

	void ReadMediaData(void *buffer, avf_size_t buffer_size, avf_size_t *nread);

	void add_stts(uint32_t n, uint32_t delta);

	INLINE raw_data_t **get_media_desc(int media_type) {
		return media_type == VIDEO ? &mp_v_desc : &mp_a_desc;
	}

	INLINE TDynamicArray<stts_entry_s>& get_stts(int media_type) {
		return media_type == VIDEO ? m_v_stts : m_a_stts;
	}

	avf_status_t SeekInTrack(out_track_info_s *track, uint32_t offset);

	INLINE avf_status_t SetFileState(int state, uint32_t offset) {
		m_file_state = state;
		m_state_pos = offset;
		m_buf_remain = 0;
		return E_OK;
	}

	INLINE TDynamicArray<mp4_track_info_s>& get_tracks(int media_type) {
		return media_type == VIDEO ? m_v_tracks : m_a_tracks;
	}

	bool DoOpenSegFile();

	INLINE bool OpenSegFile() {
		return (m_file_index != m_io_index) ? DoOpenSegFile() : true;
	}

private:
	bool mbForceBigFile;
	int mState;

	IAVIO *mpIO;
	avf_size_t m_io_index;

	int m_file_state;
	avf_size_t m_state_pos;
	avf_size_t m_buf_remain;

	avf_size_t m_file_index;
	avf_size_t m_array_index;
	avf_size_t m_array_remain;
	avf_size_t m_array_item_size;

	uint32_t m_mdat_offset;
	uint32_t m_mdat_remain;

	uint32_t m_big_file;
	avf_u64_t m_filesize;
	avf_u64_t m_filepos;

private:
	range_s m_range;

	mp4_info_s m_info;

	TDynamicArray<mp4_track_info_s> m_v_tracks;
	TDynamicArray<mp4_track_info_s> m_a_tracks;

	int m_media_type;

	mp4_track_info_s *mp_track;
	mp4_track_info_s *mp_v_track;
	mp4_track_info_s *mp_a_track;

	out_track_info_s *mp_write_track;

private:
	uint16_t m_width;
	uint16_t m_height;
	uint32_t m_timescale;

private:
	uint8_t *m_buf;
	uint8_t *m_buf_ptr;
	uint32_t m_buf_size;

	uint8_t *m_aux_buf;
	uint32_t m_aux_buf_size;

private:
	raw_data_t *mp_v_desc;
	raw_data_t *mp_a_desc;
	raw_data_t *mp_udata;
	TDynamicArray<stts_entry_s> m_v_stts;
	TDynamicArray<stts_entry_s> m_a_stts;
	TDynamicArray<file_info_s> m_filelist;
};

#endif

