
#ifndef __MP4_WRITER_H__
#define __MP4_WRITER_H__

#include "mpeg_utils.h"
#include "tsmap.h"

class CMP4Writer;
class CMP4Stream;

class CBufferIO;

class IRecordObserver;
class CTSMap;

struct CConfigNode;

extern const avf_u32_t g_mp4_matrix[];

extern const char g_muxer_name[];
extern int g_muxer_name_len;

extern const char g_video_handler[];
extern int g_video_handler_len;

extern const char g_audio_handler[];
extern int g_audio_handler_len;

extern const char g_subtitle_handler[];
extern int g_subtitle_handler_len;

//-----------------------------------------------------------------------
//
//  CMP4Writer
//
//-----------------------------------------------------------------------
class CMP4Writer: public CMediaFileWriter
{
	typedef CMediaFileWriter inherited;
	friend class CMP4Stream;

	enum {
		MAX_STREAMS = 4,
	};

public:
	static IMediaFileWriter* Create(IEngine *pEngine, CConfigNode *node, int index, const char *avio);

protected:
	CMP4Writer(IEngine *pEngine, CConfigNode *node, int index, const char *avio);
	virtual ~CMP4Writer();

// IMediaFileWriter
public:
	virtual avf_status_t StartWriter(bool bEnableRecord);
	virtual avf_status_t EndWriter();

	virtual avf_status_t StartFile(string_t *filename, avf_time_t video_time,
		string_t *picture_name, avf_time_t picture_time, string_t *basename);
	virtual IAVIO *GetIO();
	virtual avf_status_t EndFile(avf_u32_t *fsize, bool bLast, avf_u64_t nextseg_ms);
	virtual const char *GetCurrentFile();

	virtual avf_status_t SetRawData(avf_size_t stream, raw_data_t *raw);

	virtual avf_status_t ClearMediaFormats() { return E_OK; }
	virtual avf_status_t CheckMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat);
	virtual avf_status_t SetMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat);
	virtual avf_status_t CheckStreams();
	virtual avf_status_t SetStreamAttr(avf_stream_attr_s& stream_attr) {
		return E_OK;
	}

	virtual avf_status_t WriteSample(int stream, CBuffer* pBuffer);
	virtual avf_status_t WritePoster(string_t *filename, avf_time_t gen_time, CBuffer* pBuffer) {
		return E_UNIMP;
	}

	virtual bool ProvideSamples();
	virtual ISampleProvider *CreateSampleProvider(avf_size_t before, avf_size_t after);

private:
	void InitFile();
	void CalcGlobalInfo(avf_u64_t nextseg_ms);
	void CompleteFile();
	void ResetStreams();
	void WriteFileTail();

private:
	IRecordObserver *mpObserver;

	avf_u64_t m_seg_start_ms;
	tsmap_counter_s mCounter;
	CTSMap *mpMap;
	IAVIO *mpIO;

	CMP4Stream *mpStreams[MAX_STREAMS];

private:
	int mIndex;
	const avf_u32_t mTimeScale;

	avf_sbool_t add_video_started;
	avf_time_t mGenTime;
	ap<string_t> mFileName;
	CWriteBuffer m_wbuf;

	avf_u64_t m_data_start;		// offset of media data
	avf_u64_t m_data_pos;		// current file offset
	avf_u32_t m_data_pos_inc;	// 

	// config
	avf_u32_t m_info_size;		// reserved size for moov. if 0, put moov at file tail
	bool mb_flat_chunks;		// true: one sample one chunk

private:
	typedef avf_u32_t (CMP4Writer::*WriteBoxProc)(avf_u32_t size);

#define WRITE_BOX(_proc) \
	(_proc(_proc(0)))

	INLINE bool IsBigFile() {
		// only affect SampleTable
		return (avf_u32_t)(m_data_pos >> 32) != 0;
	}

	INLINE void StartStreamer() {
		m_wbuf.SetIO(mpIO);
	}

	INLINE void FlushStreamer() {
		m_wbuf.Flush();
	}

	INLINE void write_be_8(avf_u8_t val) {
		m_wbuf.write_be_8(val);
	}

	INLINE void write_be_16(avf_u16_t val) {
		m_wbuf.write_be_16(val);
	}

	INLINE void write_be_24(avf_u32_t val) {
		m_wbuf.write_be_24(val);
	}

	INLINE void write_be_32(avf_u32_t val) {
		m_wbuf.write_be_32(val);
	}

	INLINE void write_be_64(avf_u64_t val) {
		m_wbuf.write_be_64(val);
	}

	INLINE void write_fcc(const char *fcc) {
		m_wbuf.write_fcc(fcc);
	}

	INLINE void write_mem(const avf_u8_t *p, avf_u32_t size) {
		m_wbuf.write_mem(p, size);
	}

	INLINE void write_zero(avf_uint_t count) {
		m_wbuf.write_zero(count);
	}

	void write_box(avf_u32_t size, const char *fcc);
	void write_large_box(avf_u64_t size, const char *fcc);
	void write_full_box(avf_u32_t size, const char *fcc, int version, avf_u32_t flags);
	void write_matrix();
	void write_time();
	void write_string(const char *str);
	avf_u32_t get_string_size(const char *str);

private:
	avf_u32_t WriteFileType(avf_u32_t size);
	avf_u32_t WriteMovie(avf_u32_t size);

	avf_u32_t WriteMovieHeader(avf_u32_t size);
	avf_u32_t WriteTracks(avf_u32_t size);

	avf_u32_t WriteUserData(avf_u32_t size);
	avf_u32_t WriteUserDataInf(avf_u32_t size);
	avf_u32_t WriteUserDataVideoBox(avf_u32_t size);

	avf_u32_t WriteTrack(avf_u32_t size);

	avf_u32_t WriteTrackHeader(avf_u32_t size);
	avf_u32_t WriteEditListContainer(avf_u32_t size);
	avf_u32_t WriteMedia(avf_u32_t size);

	avf_u32_t WriteEditList(avf_u32_t size);

	avf_u32_t WriteMediaHeader(avf_u32_t size);
	avf_u32_t WriteHandlerReference(avf_u32_t size);
	avf_u32_t WriteMediaInformation(avf_u32_t size);

	avf_u32_t WriteVideoMediaHeader(avf_u32_t size);
	avf_u32_t WriteAudioMediaHeader(avf_u32_t size);
	avf_u32_t WriteSubtitleMediaHeader(avf_u32_t size);
	avf_u32_t WriteDataInformation(avf_u32_t size);
	avf_u32_t WriteSampleTable(avf_u32_t size);

	avf_u32_t WriteNullMediaHeader(avf_u32_t size);
	avf_u32_t WriteBaseMediaInformationHeader(avf_u32_t size);

	avf_u32_t WriteDataReference(avf_u32_t size);

	avf_u32_t WriteSampleDescription(avf_u32_t size);
	avf_u32_t WriteDecodingTimeToSample(avf_u32_t size);
	avf_u32_t WriteCompositionTimeToSample(avf_u32_t size);
	avf_u32_t WriteSyncSampleTable(avf_u32_t size);
	avf_u32_t WriteSampleToChunk(avf_u32_t size);
	avf_u32_t WriteSampleSize(avf_u32_t size);
	avf_u32_t WriteChunkOffset(avf_u32_t size);

	avf_u32_t WriteVideoSampleEntry(avf_u32_t size);
	avf_u32_t WriteAudioSampleEntry(avf_u32_t size);
	avf_u32_t WriteTextSampleEntry(avf_u32_t size);

	avf_u32_t WriteTextSampleEntry_tx3g(avf_u32_t size);
	avf_u32_t WriteTextSampleEntry_text(avf_u32_t size);

	avf_u32_t WriteFontTable(avf_u32_t size);
	avf_u32_t WriteFontRecord(avf_u32_t size);

	avf_u32_t WriteAVCConfiguration(avf_size_t size);
	avf_u32_t WriteAVCConfigurationRecord(avf_size_t size);

	avf_u32_t WriteAudioESDesc(avf_size_t size);
	avf_u32_t WriteAudioESDescriptor(avf_size_t size);
	avf_u32_t WriteAudioDecConfig(avf_size_t size);
	avf_u32_t WriteAudioDecSpecificInfo(avf_size_t size);
	avf_u32_t WriteAudioSLConfig(avf_size_t size);

	avf_u32_t GetSizeSize(avf_size_t size);
	void WriteSize(avf_size_t size);

	avf_u32_t ProcessDecodingTimeToSample(avf_size_t size, bool bWrite);
	void WriteMediaDataBox();

// temp use
private:
	// global info
	avf_s64_t mCreationTimeStamp;
	avf_s64_t mModificationTimeStamp;
	avf_u64_t mDuration;
	avf_uint_t mTotalTracks;

	// current track
	avf_u32_t mTrackID;
	CMP4Stream *mpWriteStream;
};

//-----------------------------------------------------------------------
//
//  CMP4Stream
//
//-----------------------------------------------------------------------
class CMP4Stream: public CObject
{
	typedef CObject inherited;
	friend class CMP4Writer;

	enum {
		CHUNK_START = CBuffer::F_USER_START,
	};

	enum {
		FCC_SPS = MKFCC(':','S','P','S'),
		FCC_PPS = MKFCC(':','P','P','S'),
	};

	struct chunk_t {
		avf_u32_t	m_index;
		avf_u32_t	m_samples_in_chunk;
	};

	enum { SAMPLES_PER_BLOCK = 1024 };
	enum { CHUNKS_PER_BLOCK = 256 };

	struct block_t {
		block_t *m_next;
		avf_u32_t m_count;
	};

	// samples info
	// samples-block -> sample-block -> ...
	struct sample_block_t : public block_t {
		media_sample_t m_samples[SAMPLES_PER_BLOCK];
		sample_block_t *GetNext() {
			return (sample_block_t*)m_next;
		}
	};

	// SampleToChunk table
	struct chunk_block_t : public block_t {
		chunk_t m_chunks[CHUNKS_PER_BLOCK];
		chunk_block_t *GetNext() {
			return (chunk_block_t*)m_next;
		}
	};

private:
	CMP4Stream(CMP4Writer *pWriter, avf_size_t index);
	virtual ~CMP4Stream();

private:
	avf_status_t SetRawData(raw_data_t *raw);
	avf_status_t CheckMediaFormat(CMediaFormat *pMediaFormat);
	avf_status_t SetMediaFormat(CMediaFormat *pMediaFormat);
	avf_status_t StopStream();

private:
	INLINE bool IsValid() {
		return mMediaFormat.get() != NULL;
	}
	void Reset();
	avf_u64_t GetGlobalDuration() {
		return mTimeScale == 0 ? 0 : mDuration * mpWriter->mTimeScale / mTimeScale;
	}
	void GetSize(avf_u32_t *pWidth, avf_u32_t *pHeight);
	void WriteSampleData(CBuffer *pBuffer);
	void PreWriteSubtitleSample(CBuffer *pBuffer);
	avf_u32_t WriteAVCSample(CBuffer *pBuffer);
	avf_u32_t WriteAACSample(CBuffer *pBuffer);
	avf_u32_t WriteMp3Sample(CBuffer *pBuffer);
	avf_u32_t WriteSubtitleSample(CBuffer *pBuffer);
	avf_u32_t WriteNAL(const avf_u8_t *ptr, avf_size_t size);

private:
	ap<CMediaFormat> mMediaFormat;
	mp3_header_info_t mMp3Info;
	avf_size_t mHeaderLen;

private:
	CMP4Writer *mpWriter;
	avf_size_t mIndex;

	avf_u32_t mTimeScale;
	avf_u64_t mMaxPts;
	avf_u64_t mStartPts;
	avf_u64_t mDuration;
	avf_u64_t mTotalBytes;
	avf_u32_t mMaxBitrate;

	// all samples - save all sample info
	sample_block_t *m_first_sample_block;
	sample_block_t *m_last_sample_block;

	// chunks
	chunk_block_t *m_first_chunk_block;
	chunk_block_t *m_last_chunk_block;

	bool mb_write_ctts;
	avf_u32_t mSampleCount;
	avf_u32_t mSyncSampleCount;

	// for subtitle
	CBuffer *mpPrevBuffer;

	raw_data_t *m_sps;
	raw_data_t *m_pps;
	raw_data_t *m_aac;

private:
	bool NewBlock(avf_size_t size, block_t **pFirstBlock, block_t **pLastBlock);
	bool ResizeBlock(avf_size_t size);
	void FreeAllBlocks(block_t *block);

	INLINE bool NewSampleBlock() {
		return NewBlock(sizeof(sample_block_t), 
			(block_t**)&m_first_sample_block, 
			(block_t**)&m_last_sample_block);
	}

	INLINE bool NewChunkBlock() {
		return NewBlock(sizeof(chunk_block_t), 
			(block_t**)&m_first_chunk_block, 
			(block_t**)&m_last_chunk_block);
	}

private:
	bool IsVideo() {
		return mMediaFormat->IsVideo();
	}
	bool IsAudio() {
		return mMediaFormat->IsAudio();
	}
	bool IsMediaType(int media_type) {
		return mMediaFormat->IsMediaType(media_type);
	}
	bool IsSubtitle() {
		return (mMediaFormat->mMediaType == MEDIA_TYPE_SUBTITLE);
	}
	bool IsAVCFormat() {
		return (mMediaFormat->mFormatType == MF_H264VideoFormat);
	}
	bool IsMJPEGFormat() {
		return (mMediaFormat->mFormatType == MF_MJPEGVideoFormat);
	}
	bool IsAACFormat() {
		return (mMediaFormat->mFormatType == MF_AACFormat);
	}
	bool IsMp3Format() {
		return (mMediaFormat->mFormatType == MF_Mp3Format);
	}

private:
	void CalcChunkOffsetEntries();
	void AddSampleToChunkEntry(avf_u32_t chunk_index, avf_u32_t samples_in_chunk);
	void WriteChunkOffsetEntries();
	void WriteSampleToChunkEntries();

	avf_u32_t CalcBitrate(uint64_t size, uint64_t duration);
	avf_u32_t GetAverageBitrate() {
		return mDuration == 0 ? 0 : CalcBitrate(mTotalBytes, mDuration);
	}

// temp use
private:
	avf_u32_t m_trak_size;	// track size

	avf_u32_t m_tkhd_size;	// track header
	avf_u32_t m_edts_size;	// edit list container
	avf_u32_t m_mdia_size;	// media

	avf_u32_t m_mdhd_size;	// media header
	avf_u32_t m_hdlr_size;	// media handler reference
	avf_u32_t m_minf_size;	// media information

	avf_u32_t m_vshd_size;	// video/sound media header
	avf_u32_t m_dinf_size;	// data information
	avf_u32_t m_stbl_size;	// sample table

	avf_u32_t m_stsd_size;	// sample description
	avf_u32_t m_stts_size;	// decoding time to sample
	avf_u32_t m_ctts_size;	// composition time to sample
	avf_u32_t m_stss_size;	// sync sample table
	avf_u32_t m_stsc_size;	// sample to chunk
	avf_u32_t m_stsz_size;	// sample size
	avf_u32_t m_stco_size;	// chunk offset

	avf_u32_t m_chunk_offset_entries;
	avf_u32_t m_sample_to_chunk_entries;
	avf_u32_t m_sample_table_size;
};

#endif

