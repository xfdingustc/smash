
#ifndef __FILE_DEMUXER_H__
#define __FILE_DEMUXER_H__

struct CConfigNode;
class CFileDemuxer;
class CFileDemuxerOutput;

//-----------------------------------------------------------------------
//
//  CFileDemuxer
//
//-----------------------------------------------------------------------
class CFileDemuxer: public CActiveFilter, public IMediaSource
{
	typedef CActiveFilter inherited;
	friend class CFileDemuxerOutput;
	DEFINE_INTERFACE;

	enum {
		MAX_OUTPUT = 3,
	};

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CFileDemuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CFileDemuxer();

// IFilter
public:
	//virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumOutput() { return mNumOutput; }
	virtual IPin* GetOutputPin(avf_uint_t index);

// IActiveObject
public:
	virtual void FilterLoop();
	//virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);
	virtual IPin *GetOutputMaster();

// IMediaSource
public:
	virtual avf_status_t OpenMediaSource(IAVIO *io) { return E_UNIMP; }
	virtual avf_status_t OpenMediaSource(IMediaFileReader *pReader);
	virtual avf_status_t GetMediaInfo(avf_media_info_t *info);
	virtual avf_status_t GetVideoInfo(avf_uint_t index, avf_video_info_t *info);
	virtual avf_status_t GetAudioInfo(avf_uint_t index, avf_audio_info_t *info);
	virtual avf_status_t GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info);

	virtual avf_status_t SetRange(avf_u32_t start_time_ms, avf_s32_t length_ms, bool bLastFile);
	virtual avf_status_t SeekToTime(avf_u32_t ms);

	virtual bool CanFastPlay();
	virtual avf_status_t SetFastPlay(int mode, int speed);

private:
	bool ReadInputSample(bool bStream, COutputPin *&pPin, CBuffer *&pBuffer, bool &bEOS);
	bool FastModeSendEOS();
	void NormalPlayLoop();
	void FastPlayLoop(int speed);
	avf_status_t EnableAllTracks();

private:
	avf_status_t SeekToCurrent();
	avf_status_t StartAllPins();
	avf_status_t StartPin(CFileDemuxerOutput *pPin);
	void SendAllEOS();

private:
	ap<IMediaFileReader> mReader;
	CFileDemuxerOutput *mpOutput[MAX_OUTPUT];
	avf_size_t mNumOutput;
	CFileDemuxerOutput *mpVideoOutput;
	CFileDemuxerOutput *mpAudioOutput;
	avf_u32_t mStartTimeMs;
	avf_s32_t mLengthMs;
	bool mbLastFile;
	int mFastPlayMode;
	int mFastPlaySpeed;
	bool mbIframeOnly;
};

//-----------------------------------------------------------------------
//
//  CFileDemuxerOutput
//
//-----------------------------------------------------------------------
class CFileDemuxerOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CFileDemuxer;

private:
	static CFileDemuxerOutput* Create(CFileDemuxer *pFilter, int index, IMediaTrack *track);
	CFileDemuxerOutput(CFileDemuxer *pFilter, int index, IMediaTrack *track);
	virtual ~CFileDemuxerOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat() {
		return mpTrack->QueryMediaFormat();
	}
	virtual IBufferPool *QueryBufferPool();

private:
	INLINE IMediaTrack* GetTrack() {
		return mpTrack;
	}
	INLINE void SetTrack(IMediaTrack *pTrack) {
		mpTrack = pTrack;
	}
	INLINE avf_status_t EnableTrack(bool bEnable) {
		return mpTrack->EnableTrack(bEnable);
	}
	INLINE int GetMediaType() {
		return mUsingMediaFormat->mMediaType;
	}
	INLINE avf_status_t SeekToTime(avf_u32_t ms) {
		return mpTrack->SeekToTime(ms);
	}
	INLINE avf_status_t ReadSample(CBuffer *pBuffer, bool& bEOS) {
		return mpTrack->ReadSample(pBuffer, bEOS);
	}

private:
	IMediaTrack *mpTrack;
};

#endif

