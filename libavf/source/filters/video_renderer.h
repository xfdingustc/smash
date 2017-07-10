
#ifndef __VIDEO_RENDERER_H__
#define __VIDEO_RENDERER_H__

struct CConfigNode;
class CVideoRenderer;
class CVideoRendererInput;

//-----------------------------------------------------------------------
//
//  CVideoRenderer
//
//-----------------------------------------------------------------------
class CVideoRenderer: public CActiveRenderer
{
	typedef CActiveRenderer inherited;
	friend class CVideoRendererInput;

	enum {
		INVALID_PTS = -1U,
		PTS_GAP = 5*_90kHZ,
	};

public:
	static avf_status_t RecognizeFormat(CMediaFormat *pFormat);
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CVideoRenderer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CVideoRenderer();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumInput() { return 1; }
	virtual IPin* GetInputPin(avf_uint_t index);

// IActiveObject
public:
	virtual void FilterLoop();

// CActiveRenderer
public:
	virtual void OnStopRenderer() {
		StopDecoder();
	}
	virtual bool OnPauseRenderer();

private:
	CVideoRendererInput *mpInput;

// amba
private:
	int m_iav_fd;
	bool mbFrameSeek;

	bool mb_decode_started;
	avf_u32_t m_last_pts;
	avf_u32_t m_start_pts;
	avf_u32_t m_first_pts;

	avf_u32_t m_written_bytes;
	avf_u32_t m_written_frames;

private:
	avf_status_t StartDecoder();
	avf_status_t StopDecoder();
	avf_status_t WaitDecoderBuffer(avf_u32_t size);
	void WriteBuffer(CBuffer *pBuffer);
	void FillGopHeader(avf_u32_t pts);
	void CopyToBSB(const avf_u8_t *ptr, avf_size_t size);
	avf_status_t DecodeBuffer(avf_u8_t *prev_ptr, int num_frames, avf_u64_t start_pts);
	bool WaitEOS(avf_u64_t eos_pts, bool *pbAudio);
	bool WaitForStart();
	void __SetPlaySpeed(avf_u32_t speed);
	void SetPlaySpeed(avf_u32_t speed) {
		if (mbFrameSeek)
			__SetPlaySpeed(speed);
	}
	bool WaitForResume();
	avf_u32_t UpdatePlayPosition();
	avf_u32_t GetDisplayPts();
	bool SyncWithAudio() {
		return Sync("pb.video-pos", 0, "pb.audio-pos", 0);
	}
	enum { GOP_HEADER_SIZE = 22};

private:
	avf_u8_t *m_bsb_ptr;
	avf_u8_t *m_bsb_end;
	avf_u32_t m_bsb_room;
	avf_u32_t m_bsb_size;
};

//-----------------------------------------------------------------------
//
//  CVideoRendererInput
//
//-----------------------------------------------------------------------
class CVideoRendererInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CVideoRenderer;

private:
	CVideoRendererInput(CVideoRenderer *pFilter);
	virtual ~CVideoRendererInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat, IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

private:
	CAmbaAVCFormat *mp_video_format;
};

#endif

