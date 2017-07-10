
#ifndef __AUDIO_SOURCE_H__
#define __AUDIO_SOURCE_H__

struct CConfigNode;
class CAudioSource;
class CAudioSourceOutput;

#include <alsa/asoundlib.h>

//-----------------------------------------------------------------------
//
//  CAudioSource
//
//-----------------------------------------------------------------------
class CAudioSource: public CActiveFilter, public ISourceFilter
{
	typedef CActiveFilter inherited;
	friend class CAudioSourceOutput;
	DEFINE_INTERFACE;

	enum {
		CMD_START_SOURCE = inherited::CMD_LAST,
		CMD_STOP_SOURCE,
		CMD_LAST
	};

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CAudioSource(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CAudioSource();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumOutput() { return 1; }
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

private:
	CAudioSourceOutput *mpOutput;
	bool mbStopped;
	bool mbEnabled;

private:
	snd_pcm_t *mp_snd_pcm;
	snd_pcm_format_t m_snd_pcm_format;		// SND_PCM_FORMAT_S16_LE
	uint32_t m_snd_pcm_channels;			// 2
	uint32_t m_snd_pcm_rate;				// 48000
	avf_u8_t *mp_buffer;
	tick_info_s m_tick_info;

private:
	avf_u32_t m_bytes_per_sample;
	avf_u32_t m_frames_per_chunk;
	avf_pts_t m_curr_pts;

private:
	avf_int_t m_lag;

private:
	INLINE avf_u32_t GetBytesPerChunk() {
		return m_snd_pcm_channels * m_bytes_per_sample * m_frames_per_chunk;
	}

private:
	avf_status_t OpenDevice();
	void CloseDevice();
	avf_status_t StartDevice();
	void StopDevice();
	avf_status_t ReadDevice(avf_u8_t *buffer);
	void SetBuffer(CBuffer *pBuffer);
};

//-----------------------------------------------------------------------
//
//  CAudioSourceOutput
//
//-----------------------------------------------------------------------
class CAudioSourceOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CAudioSource;

protected:
	CAudioSourceOutput(CAudioSource *pFilter);
	virtual ~CAudioSourceOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat();
	virtual IBufferPool *QueryBufferPool();

private:
	CAudioSource *mpSource;
};

#endif

