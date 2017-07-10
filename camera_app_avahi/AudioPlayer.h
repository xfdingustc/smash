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
    virtual void main(void * p);
    virtual void Stop();

private:
    CAudioPlayer();
    virtual ~CAudioPlayer();

    bool openAlsa();

    static CAudioPlayer *pInstance;

    CSemaphore  *_pCmdSem;
    CMutex      *_pMutex;

    FILE        *_pFile;
    snd_pcm_t   *_pHandle;
    snd_pcm_uframes_t _frames;
    int         _bufferSize;
    SoundEffect _curEffect;
    bool        _stopCurPlayback;
    bool        _loopPlayback;
    const char  *_audioName[SE_Num];

    char _fileToP[128];
};

#endif
