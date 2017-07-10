
#ifndef __AUDIO_RENDERER_H__
#define __AUDIO_RENDERER_H__

#include <alsa/asoundlib.h>

struct CConfigNode;
class CAudioRenderer;
class CAudioRendererInput;

//-----------------------------------------------------------------------
//
//  CAudioRenderer
//
//-----------------------------------------------------------------------
class CAudioRenderer: public CActiveRenderer
{
	typedef CActiveRenderer inherited;
	friend class CAudioRendererInput;

public:
	static avf_status_t RecognizeFormat(CMediaFormat *pFormat);
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CAudioRenderer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CAudioRenderer();

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
	virtual void OnStopRenderer();
	virtual bool OnPauseRenderer();

private:
	avf_status_t OpenDevice(unsigned *pBufferTime);
	avf_status_t PrepareBuffer(unsigned bufferTime);
	avf_status_t WaitEOS();
	bool WaitForResume(bool bForPause);
	void UpdatePlayPosition();
	avf_u32_t GetDecodePts();
	bool SyncWithVideo() {
		return Sync(NAME_PB_AUDIO_POS, 0, NAME_PB_VIDEO_POS, 0);
	}
	void SetAudioStoppedFlag(bool bStopped) {
		mpEngine->WriteRegInt32(NAME_PB_AUDIO_STOPPED, 0, (int)bStopped);
	}

private:
	CAudioRendererInput *mpInput;
	snd_pcm_t *mp_snd_pcm;
	avf_u32_t m_total_frames;

	bool bUseBuffer;
	avf_u8_t *mp_snd_buffer;
	avf_u8_t *mp_snd_wptr;	// write ptr
	avf_u8_t *mp_snd_rptr;	// current playback ptr
	avf_u32_t m_snd_size;	// buffer size
	avf_u32_t m_snd_tail;	// tail size
	avf_u32_t m_snd_avail;	// avail space to write
	avf_u32_t m_snd_filled;	// filled space
	avf_u32_t m_bytes_delay;

private:
	bool WriteSndData(avf_u8_t *pdata, snd_pcm_uframes_t frames, avf_u32_t bytes);
	void WritePCM(avf_u8_t *pdata, snd_pcm_uframes_t frames);
	avf_status_t Restart();
};

//-----------------------------------------------------------------------
//
//  CAudioRendererInput
//
//-----------------------------------------------------------------------
class CAudioRendererInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CAudioRenderer;

private:
	CAudioRendererInput(CAudioRenderer *pFilter);
	virtual ~CAudioRendererInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

protected:
	avf_size_t FrameSize() {
		CAudioFormat *pFormat = (CAudioFormat*)GetUsingMediaFormat();
		return pFormat->FrameSize();
	}
};

#endif

