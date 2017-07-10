
#ifndef __TS_WRITER_H__
#define __TS_WRITER_H__

class CBufferIO;
class CTSWriter;
struct CConfigNode;

class CTSBuffer
{
	enum {
		PACKET_SIZE = 188,
	};

public:
	CTSBuffer(IAVIO *io, IBlockIO *bio):
		mbUsePacket(bio == NULL),
		mp_io(io),
		mp_bio(bio)
	{
		if (!mbUsePacket) {
			if (bio->_GetBlockDesc(m_block_desc) != E_OK) {
				mbUsePacket = true;
			}
		}
	}

public:
	INLINE avf_u8_t *GetPacket() {
		if (mbUsePacket || m_block_desc.remain < PACKET_SIZE) {
			return m_packet;
		}
		return m_block_desc.ptr;
	}

	INLINE void WritePacket() {
		if (mbUsePacket) {
			mp_io->Write(m_packet, PACKET_SIZE);
		} else {
			if (m_block_desc.remain < PACKET_SIZE) {
				mp_bio->_UpdateWrite(m_block_desc, m_packet, PACKET_SIZE);
			} else {
				m_block_desc.ptr += PACKET_SIZE;
				m_block_desc.remain -= PACKET_SIZE;
			}
		}
	}

	INLINE void Update() {
		if (!mbUsePacket) {
			mp_bio->_UpdateWrite(m_block_desc, NULL, 0);
		}
	}

private:
	bool mbUsePacket;
	IAVIO *mp_io;
	IBlockIO *mp_bio;
	IBlockIO::block_desc_s m_block_desc;
	avf_u8_t m_packet[PACKET_SIZE];
};

//-----------------------------------------------------------------------
//
//  CTSWriter
//
//-----------------------------------------------------------------------
class CTSWriter: public CMediaFileWriter
{
	typedef CMediaFileWriter inherited;

	enum {
		MAX_STREAMS = 4,
	};

	enum {
		TS_ID = 1,
		PG_NUMBER = 1,
		PAT_PID = 0,
		PMT_PID = 256,
		START_PID = 512,
		PACKET_SIZE = 188,
		SYNC_BYTE = 0x47,
		PA_TID = 0,
		PM_TID = 2,
		ONLY_PAYLOAD = 0x01,
		ONLY_ADAPTATION = 0x02,
		ADAPTATION_PAYLOAD = 0x03,
	};

public:
	static IMediaFileWriter* Create(IEngine *pEngine, CConfigNode *node, int index, const char *avio);

private:
	CTSWriter(IEngine *pEngine, CConfigNode *node, int index, const char *avio);
	virtual ~CTSWriter();

// IMediaFileWriter
public:
	//virtual avf_status_t StartWriter(bool bEnableRecord);
	virtual avf_status_t EndWriter();

	virtual avf_status_t StartFile(string_t *path, avf_time_t video_time,
		string_t *picture_name, avf_time_t picture_time, string_t *basename);
	virtual IAVIO *GetIO();
	virtual avf_status_t EndFile(avf_u32_t *fsize, bool bLast, avf_u64_t nextseg_ms);
	virtual const char *GetCurrentFile();

	virtual avf_status_t SetRawData(avf_size_t stream, raw_data_t *raw);

	virtual avf_status_t ClearMediaFormats();
	virtual avf_status_t CheckMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat);
	virtual avf_status_t SetMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat);
	virtual avf_status_t CheckStreams();
	virtual avf_status_t SetStreamAttr(avf_stream_attr_s& stream_attr);

	virtual avf_status_t WriteSample(int stream, CBuffer* pBuffer);
	virtual avf_status_t WritePoster(string_t *filename, avf_time_t gen_time, CBuffer* pBuffer) {
		return E_UNIMP;
	}

	virtual bool ProvideSamples() { return false; }
	virtual ISampleProvider *CreateSampleProvider(avf_size_t before, avf_size_t after) { return NULL; }

private:
	int mIndex;
	avf_time_t mGenTime;
	ap<string_t> mFileName;
	bool mbDirect;
	IAVIO *mpIO;
	IBlockIO *mpBlockIO;
	avf_size_t m_video_upload_num;

	avf_u64_t m_total_length;
	avf_u32_t m_time_scale;

	avf_stream_attr_s m_stream_attr;
	IRecordObserver *mpObserver;

	CMediaFormat *mpFormat[MAX_STREAMS];
	avf_uint_t mPIDs[MAX_STREAMS];
	avf_u8_t mCounter[MAX_STREAMS];
	avf_u8_t mStreamId[MAX_STREAMS];
	avf_u64_t mStartPTS[MAX_STREAMS];
	avf_u64_t mStartDTS[MAX_STREAMS];
	bool mbStarted[MAX_STREAMS];

	avf_int_t m_video_index;
	avf_int_t m_num_video;

	avf_int_t m_audio_index;
	avf_int_t m_num_audio;

	avf_int_t m_subt_index;
	avf_int_t m_num_subt;

	avf_uint_t m_pcr_pid;
	bool mb_stream_started;

	avf_u8_t pat_counter;
	avf_u8_t pmt_counter;
	avf_sbool_t pmt_written;
	avf_sbool_t add_video_started;

private:
	bool mb_uploading;
	IAVIO *mp_upload_io;
	avf_u32_t m_upload_counter;
	avf_u64_t m_upload_start_pts;
	upload_header_v2_t m_upload_header;
	ap<string_t> m_upload_filename;

private:
	void GetUploadingFilename(char *buffer, int len, avf_uint_t index);
	void StartUploading(CBuffer *pBuffer);
	void EndUploading(bool bLast);

private:
	int GetStream(CMediaFormat *format);

private:
	void WritePAT_PMT(int b_io, int b_uploading);
	avf_u64_t WritePES(CMediaFormat *pFormat, avf_uint_t stream, CBuffer* pBuffer, avf_size_t subt, int pcr);

	INLINE void WriteIO(const void *buffer, avf_size_t size) {
		if (mbEnableRecord) {
			mpIO->Write(buffer, size);
		}
	}

	INLINE void WriteUpload(const void *buffer, avf_size_t size) {
		if (mp_upload_io) {
			mp_upload_io->Write(buffer, size);
			m_upload_header.u_data_size += size;
		}
	}

private:
	void CreatePAT();
	void CreatePMT();
	void WriteSection(int pid, int table_id, avf_u8_t *counter, 
		avf_u8_t *data, avf_size_t size, int b_io, int b_uploading);

private:
	avf_size_t CreateIOD(avf_u8_t *pstart);
	avf_size_t CreateInitialObjectDescriptor(avf_u8_t *pstart);
	avf_size_t CreateESDescriptor(avf_u8_t *pstart);
	avf_size_t CreateDecoderConfiguration(avf_u8_t *pstart);
	avf_size_t CreateDecSpecificInfo(avf_u8_t *pstart);
	avf_size_t CreateSLConfig(avf_u8_t *pstart);
	void SetSize3(avf_u8_t *p, avf_size_t size);

private:
	bool mb_pat_valid;
	avf_size_t m_pat_size;
	avf_u8_t m_pat_data[32];

	bool mb_pmt_valid;
	avf_size_t m_pmt_size;
	avf_u8_t m_pmt_data[256];
};

#endif

