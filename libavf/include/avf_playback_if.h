
#ifndef __AVF_PLAYBACK_IF_H__
#define __AVF_PLAYBACK_IF_H__

class IMediaControl;

//-----------------------------------------------------------------------
//
//  Playback - please do not subclass it
//
//-----------------------------------------------------------------------
class Playback
{
public:
	Playback(IMediaControl *pMediaControl) {
		mpMediaControl = pMediaControl;
	}
	~Playback() {}

private:
	IMediaControl *mpMediaControl;

public:
	enum {
		CMD_GetMediaInfo,
		CMD_GetPlayPos,
		CMD_PauseResume,
		CMD_Seek,
		CMD_FastPlay,
	};

	typedef struct PlaybackMediaInfo_s {
		avf_u32_t total_length_ms;
		avf_u32_t audio_num;
		avf_u32_t video_num;
		bool bPaused;
	} PlaybackMediaInfo_t;

	typedef struct FastPlayParam_s {
		int mode;		// not used now
		int speed;		// 0: normal; 1~n: forward x-speed; -1~-n: backward x-speed
	} FastPlayParam_t;

public:
	avf_status_t GetMediaInfo(PlaybackMediaInfo_t *pMediaInfo) {
		return mpMediaControl->Command(CMP_ID_PLAYBACK, CMD_GetMediaInfo, (avf_intptr_t)pMediaInfo);
	}
	avf_status_t GetPlayPos(avf_u32_t *pPos) {
		return mpMediaControl->Command(CMP_ID_PLAYBACK, CMD_GetPlayPos, (avf_intptr_t)pPos);
	}
	avf_status_t PauseResume() {
		return mpMediaControl->Command(CMP_ID_PLAYBACK, CMD_PauseResume, 0);
	}
	avf_status_t Seek(avf_u32_t ms) {
		return mpMediaControl->Command(CMP_ID_PLAYBACK, CMD_Seek, (avf_intptr_t)ms);
	}
	avf_status_t FastPlay(FastPlayParam_t *param) {
		return mpMediaControl->Command(CMP_ID_PLAYBACK, CMD_FastPlay, (avf_intptr_t)param);
	}
};

#endif

