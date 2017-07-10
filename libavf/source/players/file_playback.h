
#ifndef __FILE_PLAYBACK_H__
#define __FILE_PLAYBACK_H__

#include "filter_if.h"
#include "avf_playback_if.h"

class CConfig;
class IMediaSource;

//-----------------------------------------------------------------------
//
//  CFilePlayback
//
//-----------------------------------------------------------------------
class CFilePlayback: public CBasePlayer
{
	typedef CBasePlayer inherited;

public:
	static CFilePlayback* Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);

protected:
	CFilePlayback(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);
	virtual ~CFilePlayback();

// IMediaPlayer
public:
	virtual avf_status_t OpenMedia(bool *pbDone);
	virtual avf_status_t PrepareMedia(bool *pbDone);
	virtual avf_status_t RunMedia(bool *pbDone);
	virtual void ProcessFilterMsg(const avf_msg_t& msg);
	virtual avf_status_t Command(avf_enum_t cmdId, avf_intptr_t arg);

protected:
	void ResumeAllMediaRenderers();
	avf_status_t OpenMediaSource(IFilter *pFilter, IMediaFileReader *reader, IAVIO *io);

protected:
	virtual avf_status_t GetMediaInfo(Playback::PlaybackMediaInfo_t *pMediaInfo);
	avf_status_t Seek(avf_u32_t ms);
	virtual avf_status_t PerformSeek(avf_u32_t ms);
	avf_u32_t GetPlayPosWithSpeed(int speed);
	INLINE avf_u32_t GetPlayPos() {
		return GetPlayPosWithSpeed(GetPlaySpeed());
	}
	avf_status_t PauseResume();
	avf_status_t FastPlay(Playback::FastPlayParam_t *param);
	INLINE int GetPlaySpeed() {
		return mpEngine->ReadRegInt32(NAME_PB_FASTPLAY, 0, VALUE_PB_FASTPLAY);
	}

protected:
	void StartSeeking() {
		mpEngine->WriteRegBool(NAME_PB_PAUSE, 0, 1);
	}
	void EndSeeking() {
		mpEngine->WriteRegBool(NAME_PB_PAUSE, 0, 0);
	}

protected:
	IMediaSource *mpMediaSource;
	avf_media_info_t mMediaInfo;
	bool mbPaused;
	bool mbStarting;
	avf_u32_t mStartTime;	// ms
};

#endif

