
#ifndef __MP4_READER_H__
#define __MP4_READER_H__

class CMP4Reader;
class CMP4Track;

struct CAmbaAVCFormat;

//-----------------------------------------------------------------------
//
//  CMP4Reader
//
//-----------------------------------------------------------------------
class CMP4Reader: public CMediaFileReader
{
	typedef CMediaFileReader inherited;
	friend class CMP4Track;

	enum {
		MAX_TRACKS = 4,
	};

public:
	static avf_int_t Recognize(IAVIO *io);

public:
	static IMediaFileReader* Create(IEngine *pEngine, IAVIO *io);

protected:
	CMP4Reader(IEngine *pEngine, IAVIO *io);
	virtual ~CMP4Reader();

// IMediaFileReader
public:
	virtual avf_status_t OpenMediaSource();

	virtual avf_uint_t GetNumTracks();
	virtual IMediaTrack* GetTrack(avf_uint_t index);

	virtual avf_status_t GetMediaInfo(avf_media_info_t *info);
	virtual avf_status_t GetVideoInfo(avf_uint_t index, avf_video_info_t *info);
	virtual avf_status_t GetAudioInfo(avf_uint_t index, avf_audio_info_t *info);
	virtual avf_status_t GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info);

	virtual bool IsStreamFormat() {
		return false;
	}
	virtual bool CanFastPlay() {
		return false;
	}
	virtual avf_status_t SeekToTime(avf_u32_t ms, avf_u32_t *out_ms) {
		return E_UNIMP;
	}
	virtual avf_status_t FetchSample(avf_size_t *pTrackIndex) {
		return E_UNIMP;
	}

private:
	IAVIO *mpIO;

	CMP4Track *mpTracks[MAX_TRACKS];
	CMP4Track *mpVideoTrack;	// reference
	CMP4Track *mpAudioTrack;	// reference
	CMP4Track *mpTextTrack;		// reference

	CMP4Track *mpCurrentTrack;	// reference
	CAmbaAVCFormat *mpAmbaFormat;

	avf_size_t mNumTracks;	// total tracks in file
	avf_size_t mNumVideos;	// number of videos, 0 or 1 (if more, only 1 is used)
	avf_size_t mNumAudios;	// number of audios, 0 or 1 (if more, only 1 is used)
	avf_size_t mNumTexts;	// number of subtitles, 0 or 1 (if more, only 1 is used)

private:
	avf_size_t m_video_width;
	avf_size_t m_video_height;
	avf_u32_t m_tmp_width;
	avf_u32_t m_tmp_height;
	avf_u32_t m_duration;	// ms

private:
	avf_status_t ReadMovie(avf_size_t box_size);

	avf_status_t ReadMovieHeader(avf_size_t box_size);
	avf_status_t ReadTrack(avf_size_t box_size);
	avf_status_t ReadUserData(avf_size_t box_size);
	avf_status_t ReadAmbaFormat(avf_u32_t type);

	avf_status_t ReadTrackHeader(avf_size_t box_size);
	avf_status_t ReadMedia(avf_size_t box_size);

	avf_status_t ReadMediaHeader(avf_size_t box_size);
	avf_status_t ReadHandlerReference(avf_size_t box_size);
	avf_status_t ReadMediaInformation(avf_size_t box_size);

	avf_status_t ReadVideoMediaHeader(avf_size_t box_size);
	avf_status_t ReadAudioMediaHeader(avf_size_t box_size);
	avf_status_t ReadSampleTable(avf_size_t box_size);

private:
	avf_u32_t m_track_timescale;
	avf_u32_t m_track_duration;

private:
	CMP4Track *AllocTrack();
	INLINE bool IsInVideoTrack() {
		return mpCurrentTrack && mpCurrentTrack == mpVideoTrack;
	}
	INLINE bool IsInAudioTrack() {
		return mpCurrentTrack && mpCurrentTrack == mpAudioTrack;
	}
	INLINE bool IsInTextTrack() {
		return mpCurrentTrack && mpCurrentTrack == mpTextTrack;
	}
};

//-----------------------------------------------------------------------
//
//  CMP4Track
//
//-----------------------------------------------------------------------
class CMP4Track: public CMediaTrack
{
	typedef CMediaTrack inherited;
	friend class CMP4Reader;

private:
	CMP4Track(CMP4Reader *pReader, avf_uint_t index, IAVIO *io, bool bBufferAll);
	virtual ~CMP4Track();

// IMediaTrack
public:
	virtual CMediaFormat* QueryMediaFormat();
	virtual avf_status_t SeekToTime(avf_u32_t ms);
	virtual avf_status_t ReadSample(CBuffer *pBuffer, bool& bEOS);

private:
	avf_status_t ReadSampleDescription(avf_size_t box_size);
	avf_status_t ReadDecodingTimeToSample(avf_size_t box_size);
	avf_status_t ReadCompositionTimeToSample(avf_size_t box_size);
	avf_status_t ReadSyncSampleTable(avf_size_t box_size);
	avf_status_t ReadSampleToChunk(avf_size_t box_size);
	avf_status_t ReadSampleSize(avf_size_t box_size);
	avf_status_t ReadChunkOffset(avf_size_t box_size);
	avf_status_t ReadChunkOffset64(avf_size_t box_size);

private:
	avf_status_t ReadVideoSampleDescription(avf_size_t box_size);
	avf_status_t ReadAudioSampleDescription(avf_size_t box_size);
	avf_status_t ReadTextSampleDescription(avf_size_t box_size);

private:
	avf_size_t ReadVarSize(avf_size_t *nbytes);
	avf_status_t ReadAudioESDescTag();	// 3
	avf_status_t ReadAudioDecoderConfiguration(avf_size_t tag_remain_size);	// 4

private:
	avf_status_t GetSampleOfDts(avf_u64_t dts, avf_u32_t *sample_index, avf_u64_t *start_dts);

	avf_status_t ReadAACSample(CBuffer *pBuffer, avf_u64_t offset, avf_size_t size);
	avf_status_t ReadAVCSample(CBuffer *pBuffer, avf_u64_t offset, avf_size_t size);
	avf_status_t ReadTextSample(CBuffer *pBuffer, avf_u64_t offset, avf_size_t size);

private:
	typedef struct chunk_info_s{
		avf_u32_t	chunk_index;		// from 0
		avf_u32_t	sample_start_index;	// from 0
		avf_u32_t	sample_end_index;	// exclusive
		avf_u32_t	num_samples;		// in this chunk
	} chunk_info_t;

	typedef struct dts_info_s {
		avf_u32_t	sample_start_index;
		avf_u32_t	sample_end_index;	// exclusive
		avf_u32_t	sample_delta;
		avf_u64_t	start_dts;
		avf_u64_t DtsOfSample(avf_u32_t index) {
			return start_dts + (index - sample_start_index) * sample_delta;
		}
		avf_u64_t EndDts() {
			return DtsOfSample(sample_end_index);
		}
	} dts_info_t;

	typedef struct pts_info_s {
		avf_u32_t	sample_start_index;
		avf_u32_t	sample_end_index;
		avf_u32_t	sample_offset;
	} pts_info_t;

private:
	avf_u8_t *ReadBlock(avf_size_t size, const char *name);

	// samples and chunks
	avf_status_t GetChunkOfSample(avf_u32_t sample_index);
	avf_status_t GetFirstChunkInfo();
	avf_status_t GetNextChunkInfo();
	INLINE bool IsSampleInCurrChunk(avf_u32_t sample_index) {
		return sample_index >= m_curr_chunk.sample_start_index &&
			sample_index < m_curr_chunk.sample_end_index;
	}
	void FillNextChunk();
	avf_status_t StartChunk();

	// dts
	avf_status_t GetDtsOfSample(avf_u32_t sample_index);
	avf_status_t GetFirstDtsInfo();
	avf_status_t GetNextDtsInfo();
	INLINE bool IsSampleInCurrDts(avf_u32_t sample_index) {
		return sample_index >= m_curr_dts.sample_start_index &&
			sample_index < m_curr_dts.sample_end_index;
	}
	void StartDts(avf_u32_t sample_index);

	// pts
	avf_status_t GetPtsOfSample(avf_u32_t sample_index);
	avf_status_t GetFirstPtsInfo();
	avf_status_t GetNextPtsInfo();
	INLINE bool IsSampleInCurrPts(avf_u32_t sample_index) {
		return sample_index >= m_curr_pts.sample_start_index &&
			sample_index < m_curr_pts.sample_end_index;
	}
	void StartPts(avf_u32_t sample_index);

	// data read
	avf_status_t GetDecodingTimeToSample(avf_u32_t read_index,
		avf_u32_t *sample_count, avf_u32_t *sample_delta);
	avf_status_t GetCompositionTimeToSample(avf_u32_t read_index,
		avf_u32_t *sample_count, avf_u32_t *sample_offset);
	avf_status_t GetSyncSample(avf_u32_t sample_index, avf_u32_t *sync_sample_index);
	avf_status_t GetSampleToChunk(avf_u32_t read_index, 
		avf_u32_t curr_chunk_index, avf_u32_t *num_samples, avf_u32_t *next_chunk_index);
	avf_status_t GetChunkOffset(avf_u32_t chunk_index, avf_u64_t *offset);
	avf_status_t GetSampleSize(avf_u32_t sample_index, avf_u32_t *size);

private:
	CMP4Reader *mpReader;
	avf_uint_t mIndex;
	IAVIO *mpIO;
	bool mbBufferAll;

	CMediaFormat *mp_media_format;

	bool m_started;

	avf_u32_t m_timescale;
	avf_u32_t m_duration;
	avf_u32_t m_sample_count;	// number of total samples
	avf_u32_t m_sample_index;	// current sample index

private:
	// cache one chunk for sample size and offset
	avf_u64_t m_cache_sample_offset;
	avf_u32_t m_cache_sample_remain;

	// cache for decoding time
	avf_pts_t m_cache_sample_dts;
	avf_u32_t m_cache_dts_delta;
	avf_u32_t m_cache_dts_remain;	// number of samples

	avf_u32_t m_cache_pts_offset;
	avf_u32_t m_cache_pts_remain;

private:
	bool mb_chunk_valid;			// if m_curr_chunk is valid
	chunk_info_t m_curr_chunk;
	avf_u32_t m_chunk_read_index;	// next chunk entry to read
	avf_u32_t m_next_chunk_index;	// next chunk index to read

private:
	bool mb_dts_valid;
	dts_info_t m_curr_dts;
	avf_u32_t m_dts_read_index;

private:
	bool mb_pts_valid;
	pts_info_t m_curr_pts;
	avf_u32_t m_pts_read_index;

private:
	// decoding time to sample
	avf_u32_t m_stts_entry_count;
	avf_u64_t m_stts_offset;
	avf_u8_t *m_stts_orig;		// all of decoding time to sample entries

	// composition time to sample
	avf_u32_t m_ctts_entry_count;
	avf_u64_t m_ctts_offset;
	avf_u8_t *m_ctts_orig;		// all of composition time to sample entries

	// sync sample table
	avf_u32_t m_stss_entry_count;
	avf_u64_t m_stss_offset;
	avf_u8_t *m_stss_orig;		// all of sync sample entries

	// sample to chunk
	avf_u32_t m_stsc_entry_count;
	avf_u64_t m_stsc_offset;
	avf_u8_t *m_stsc_orig;		// all of sample to chunk entries

	// sample size
	avf_u32_t m_sample_size;
	avf_u32_t m_stsz_entry_count;
	avf_u64_t m_stsz_offset;
	avf_u8_t *m_stsz_orig;		// all of sample size entries

	// chunk offset
	avf_u32_t m_stco_entry_count;	// todo co64
	avf_u64_t m_stco_offset;
	avf_u8_t *m_stco_orig;			// all of chunk offset entries

private:
	avf_u8_t *m_sps;
	avf_size_t m_sps_size;
	avf_u8_t *m_pps;
	avf_size_t m_pps_size;

private:
	int m_aac_sample_rate_index;
	int m_aac_layer;
	int m_aac_profile;
};

#endif

