
#ifndef __WAVE_DEMUXER_H__
#define __WAVE_DEMUXER_H__

#include "avio.h"

struct CConfigNode;
class CWaveDemuxer;
class CWaveDemuxerOutput;
struct CAudioFormat;

//-----------------------------------------------------------------------
//
//  CWaveDemuxer
//
//-----------------------------------------------------------------------
class CWaveDemuxer: public CActiveFilter, public IMediaSource
{
	typedef CActiveFilter inherited;
	friend class CWaveDemuxerOutput;
	DEFINE_INTERFACE;

public:
	static avf_int_t Recognize(IAVIO *io);
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CWaveDemuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CWaveDemuxer();

// IFilter
protected:
	virtual avf_status_t InitFilter(avf_int_t index);
	virtual avf_size_t GetNumOutput() { return 1; }
	virtual IPin* GetOutputPin(avf_uint_t index);

// IActiveObject
protected:
	virtual void FilterLoop();
	virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);

// IMediaSource
public:
	virtual avf_status_t OpenMediaSource(IAVIO *io);
	virtual avf_status_t GetMediaInfo(avf_media_info_t *info);
	virtual avf_status_t OpenMediaSource(IMediaFileReader *pReader) { return E_UNIMP; }
	virtual avf_status_t GetVideoInfo(avf_uint_t index, avf_video_info_t *info) { return E_NOENT; }
	virtual avf_status_t GetAudioInfo(avf_uint_t index, avf_audio_info_t *info);
	virtual avf_status_t GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info) { return E_NOENT; }
	virtual avf_status_t SetRange(avf_u32_t start_time_ms, avf_s32_t length_ms, bool bLastFile);
	virtual avf_status_t SeekToTime(avf_u32_t ms);
	virtual bool CanFastPlay();
	virtual avf_status_t SetFastPlay(int mode, int speed);

private:
	CWaveDemuxerOutput *mpOutput;
	CAudioFormat *mpAudioFormat;
	bool mbStopped;
	IAVIO *mpIO;
};

//-----------------------------------------------------------------------
//
//  CWaveDemuxerOutput
//
//-----------------------------------------------------------------------
class CWaveDemuxerOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CWaveDemuxer;

private:
	CWaveDemuxerOutput(CWaveDemuxer *pFilter);
	virtual ~CWaveDemuxerOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat();
	virtual IBufferPool *QueryBufferPool();
};

#endif

