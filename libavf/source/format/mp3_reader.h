
#ifndef __MP3_READER_H__
#define __MP3_READER_H__

#include "mpeg_utils.h"

class IAVIO;
class CMp3Reader;
class CMp3Track;

//-----------------------------------------------------------------------
//
//  CMp3Reader
//
//-----------------------------------------------------------------------
class CMp3Reader: public CMediaFileReader
{
	typedef CMediaFileReader inherited;
	friend class CMp3Track;

public:
	static avf_int_t Recognize(IAVIO *io);

public:
	static IMediaFileReader* Create(IEngine *pEngine, IAVIO *io);

private:
	CMp3Reader(IEngine *pEngine, IAVIO *io);
	virtual ~CMp3Reader();

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

	virtual bool IsStreamFormat() {
		return true;
	}
	virtual bool CanFastPlay() {
		return false;
	}
	virtual avf_status_t SeekToTime(avf_u32_t ms, avf_u32_t *out_ms);
	virtual avf_status_t FetchSample(avf_size_t *pTrackIndex);

// CMediaFileReader
protected:
	virtual void ReportProgress();

private:
	static avf_u32_t sync_size(avf_u32_t size);
	static avf_status_t get_header_info(CReadBuffer& buf, mp3_header_info_t& info);

private:
	IAVIO *mpIO;
	CReadBuffer mBuffer;
	mp3_header_info_t mInfo;
	CMp3Track *mpTrack;
	avf_size_t mPercent;
};

//-----------------------------------------------------------------------
//
//  CMp3Track
//
//-----------------------------------------------------------------------
class CMp3Track: public CMediaTrack
{
	typedef CMediaTrack inherited;
	friend class CMp3Reader;

private:
	static CMp3Track *Create(CMp3Reader *pReader);
	CMp3Track(CMp3Reader *pReader);
	virtual ~CMp3Track();

// IMediaTrack
public:
	virtual CMediaFormat* QueryMediaFormat();
	virtual avf_status_t SeekToTime(avf_u32_t ms);
	virtual avf_status_t ReadSample(CBuffer *pBuffer, bool& bEOS);

private:
	avf_u32_t FindSync(CReadBuffer& buf);

private:
	CMp3Reader *mpReader;
	CMp3Format *mpFormat;
	avf_u32_t mReadBytes;
	avf_u32_t mTotalBytes;
	avf_u32_t mRemainBytes;
	avf_u64_t m_pts;
};

#endif

