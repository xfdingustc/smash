#ifndef __CLASS_AUDIO_PLAYER_H__
#define __CLASS_AUDIO_PLAYER_H__

#include <alsa/asoundlib.h>

#include "ClinuxThread.h"
#include "GobalMacro.h"

class CAudioPlayer : public CThread
{
public:
    enum SoundEffect {
        SE_StartRecording = 0,
        SE_StopRecording,
        SE_SingleShot,
        SE_ContinousShot,

        SE_Alert,
        SE_Click,
        SE_Tag_Added,
        SE_Tag_Failed,
        SE_Connected,
        SE_Disconnected,
        SE_Volume_Changed,

        SE_060Test_Achieve_1,
        SE_060Test_Achieve_2,
        SE_060Test_Countdown,
        SE_060Test_Countdown_0,
        SE_060Test_Fail,
        SE_060Test_Slowdown,

        SE_Num,
    };

    static void Create();
    static CAudioPlayer* getInstance();
    static void releaseInstance();

    void playSoundEffect(SoundEffect effect);
    void stopSoundEffect();

    void playSound(const char *file);

protected:
    virtual void main();
    virtual void Stop();

private:
    CAudioPlayer();
    virtual ~CAudioPlayer();

    bool openAlsa();

    bool prepareTracks(char *buffer);

    static CAudioPlayer *pInstance;

    CSemaphore  *_pCmdSem;
    CMutex      *_pMutex;

    const char  *_audioName[SE_Num];

#define MAX_PLAYING_NUM     4
    FILE        *_pFiles[MAX_PLAYING_NUM];
    int         _numPlaying;
    time_t      _lastPlayTime;

    snd_pcm_t           *_pHandle;
    snd_pcm_uframes_t   _frames;
    int                 _bufferSize;
    snd_pcm_sframes_t   _maxFramesWritable;

    bool        _bStandby;
    bool        _stopCurPlayback;
    bool        _loopPlayback;
};

#endif
