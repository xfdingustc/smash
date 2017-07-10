
#ifndef __VIDEO_SOURCE_H__
#define __VIDEO_SOURCE_H__

#include "amba_vsrc.h"
#include "video_overlay.h"

struct CConfigNode;
class CVideoSource;
class CVideoSourceOutput;
class CVideoSourceInput;
class CStillCapture;

//-----------------------------------------------------------------------
//
//  CVideoSource
//
//-----------------------------------------------------------------------
class CVideoSource: public CActiveFilter, public ISourceFilter, public IImageControl, public IVideoControl
{
	typedef CActiveFilter inherited;
	friend class CVideoSourceOutput;
	friend class CVideoSourceInput;
	DEFINE_INTERFACE;

	enum {
		CMD_START_SOURCE = inherited::CMD_LAST,
		CMD_STOP_SOURCE,
		CMD_SET_IMAGE_CONFIG,
		CMD_ENABLE_PREVIEW,
		CMD_DISABLE_PREVIEW,
		CMD_SAVE_NEXT_PICTURE,
		//CMD_STILL_CAPTURE,
		CMD_ChangeVoutVideo,	// TODO
		CMD_LAST
	};

	enum {
		MAX_FRAME_SIZE_H264 = 4*1024*1024,
		MAX_FRAME_SIZE_JPEG = 8*1024*1024,
	};

	struct vmem_usage_s {
		uint16_t max;
		uint16_t overrun;
		uint16_t overflow;
		uint16_t reserved;
		uint32_t curr_kb;
	};

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CVideoSource(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CVideoSource();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumInput() { return 1; }
	virtual avf_size_t GetNumOutput() { return mNumStream; }
	virtual IPin* GetInputPin(avf_uint_t index);
	virtual IPin* GetOutputPin(avf_uint_t index);

// CActiveFilter
public:
	virtual void FilterLoop();
	virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);

// ISourceFilter
public:
	virtual avf_status_t StartSourceFilter(bool *pbDone) {
		return mpWorkQ->SendCmd(CMD_START_SOURCE, pbDone);
	}
	virtual avf_status_t StopSourceFilter() {
		return mpWorkQ->SendCmd(CMD_STOP_SOURCE);
	}

// IImageControl
public:
	virtual avf_status_t SetImageConfig(name_value_t *param) {
		return mpWorkQ->SendCmd(CMD_SET_IMAGE_CONFIG, param);
	}
	virtual avf_status_t EnableVideoPreview() {
		return mpWorkQ->SendCmd(CMD_ENABLE_PREVIEW, NULL);
	}
	virtual avf_status_t DisableVideoPreview() {
		return mpWorkQ->SendCmd(CMD_DISABLE_PREVIEW, NULL);
	}

// IVideoControl
public:
	virtual avf_status_t SaveNextPicture(const char *filename) {
		return mpWorkQ->SendCmd(CMD_SAVE_NEXT_PICTURE, (void*)filename);
	}
	virtual avf_status_t StillCapture(int action, int flags);
	virtual avf_status_t GetStillCaptureInfo(stillcap_info_s *info);
	virtual avf_status_t ChangeVoutVideo(const char *config) {
		return mpWorkQ->SendCmd(CMD_ChangeVoutVideo, (void*)config);
	}

private:
	avf_status_t PrepareForEncoding();
	avf_status_t StartEncoding();
	avf_status_t StopEncoding();
	avf_status_t EnterPreview();
	avf_status_t EnterIdle(bool bStopDevice);
	bool ProcessSubtitle(bool& bStopped, bool bFirst);
	static void CopyMem(avf_u8_t *dst, const avf_u8_t *src, avf_size_t size);

private:
	vsrc_device_t mDevice;
	bool mbStreamConfigured;
	bool mbDeviceInitDone;
	bool mbPreview;
	bool mbSetClockMode;
	CVideoOverlay mOverlay;
	avf_uint_t mNumStream;
	enum { MAX_STREAM = 4 };
	CVideoSourceOutput *mpOutputs[MAX_STREAM];
	CVideoSourceInput *mpInput;
	bool mbStopped;
	CStillCapture *mpStillCap;
	IRecordObserver *mpObserver;

	tick_info_s m_tick_info;

private:
	bool mbSaveNextPicture;
	ap<string_t> mNextPictureFileName;

private:
	avf_size_t m_mem_max;
	avf_size_t m_mem_overrun;
	avf_size_t m_mem_overflow;
	bool mb_debug_vmem;

private:
	void GetMemLimit();
	avf_status_t ConfigVin();
	avf_status_t ConfigVideoStreams();
	avf_status_t ConfigOverlay();
	avf_status_t SetOverlayConfig(avf_uint_t stream, const char *pConfig);
	avf_status_t SetLogoConfig(avf_uint_t stream, const char *pConfig);
	bool SendEOS(CVideoSourceOutput *pPin);
	bool SendAllEOS();
	bool CalcPosters(vsrc_bits_buffer_t *bitsBuffer);
	void SendPoster(CBuffer *pBuffer);
};

//-----------------------------------------------------------------------
//
//  CVideoSourceOutput
//
//-----------------------------------------------------------------------
class CVideoSourceOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CVideoSource;

protected:
	CVideoSourceOutput(CVideoSource *pFilter, avf_uint_t index);
	virtual ~CVideoSourceOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat();
	virtual IBufferPool* QueryBufferPool();
	virtual bool NeedReconnect() {
		return true;
	}

	virtual void ResetState() {
		inherited::ResetState();
		m_last_pts_idr = 0;
		mDTS = 0;
	}

private:
	INLINE avf_u32_t GetAllocatedMem() {
		return mUsingBufferPool.get() ? mUsingBufferPool->GetAllocatedMem() : 0;
	}

private:
	vsrc_device_t *mpDevice;
	avf_uint_t mStreamIndex;
	IBufferPool *mpBufferPool;

	bool mbOverlayEnabled;
	bool mbLogoEnabled;

	avf_u32_t mSegLength;
	avf_u32_t mPosterLength;
	avf_u64_t m_next_seg_pts;
	avf_u64_t m_next_poster_pts;
	bool mbSendPoster;

	avf_u64_t m_last_pts_idr;
	avf_u64_t mDTS;
};

//-----------------------------------------------------------------------
//
//  CVideoSourceOutput
//
//-----------------------------------------------------------------------
class CVideoSourceInput: CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CVideoSource;

protected:
	CVideoSourceInput(CVideoSource *pFilter, avf_uint_t index);
	virtual ~CVideoSourceInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

private:
	CVideoSource *mpSource;
};


#endif

