

#include<stdio.h>
#include<stdlib.h>
#include <string.h>

#include "AudioPlayer.h"

struct WAV_HEADER
{
    char rld[4];    //riff 标志符号
    int  rLen;      //
    char wld[4];    //格式类型（wave）
    char fld[4];    //"fmt"

    int fLen;   //sizeof(wave format matex)

    short wFormatTag;   //编码格式
    short wChannels;    //声道数
    int   nSamplesPersec;  //采样频率
    int   nAvgBitsPerSample;//WAVE文件采样大小
    short wBlockAlign; //块对齐
    short wBitsPerSample;   //WAVE文件采样大小

    char dld[4];        //”data“
    int  wSampleLength; //音频数据的大小
};

CAudioPlayer* CAudioPlayer::pInstance = NULL;

void CAudioPlayer::Create() {
    if (pInstance == NULL) {
        pInstance = new CAudioPlayer();
        pInstance->Go();
    }
}

void CAudioPlayer::releaseInstance() {
    if (pInstance) {
        pInstance->Stop();
        delete pInstance;
    }
}

CAudioPlayer* CAudioPlayer::getInstance() {
    if (pInstance == NULL) {
        pInstance = new CAudioPlayer();
        pInstance->Go();
    }
    return pInstance;
}

CAudioPlayer::CAudioPlayer()
  : CThread("AudioPlayer", CThread::HIGH, 0, NULL)
  , _pFile(NULL)
  , _pHandle(NULL)
  , _bufferSize(0)
  , _curEffect(SE_Num)
  , _stopCurPlayback(false)
  , _loopPlayback(false)
{
    _pCmdSem = new CSemaphore(0, 1, "Audio Player");
    _pMutex = new CMutex("Audio Player");
    _audioName[SE_StartRecording] = "/usr/local/share/ui/sound/Start_Record.wav";
    _audioName[SE_StopRecording] = "/usr/local/share/ui/sound/Stop_Record.wav";
    _audioName[SE_SingleShot] = "/usr/local/share/ui/diablo/single-shot-48KHz-16bit-1ch.wav";
    _audioName[SE_ContinousShot] = "/usr/local/share/ui/diablo/continuous-shot-48KHz-16bit-1ch.wav";

    _audioName[SE_Alert] = "/usr/local/share/ui/sound/Alert.wav";
    _audioName[SE_Click] = "/usr/local/share/ui/sound/Tap_or_Choose.wav";
    _audioName[SE_Tag_Added] = "/usr/local/share/ui/sound/Tag_Successful.wav";
    _audioName[SE_Tag_Failed] = "/usr/local/share/ui/sound/Tag_Successful.wav";
    _audioName[SE_Connected] = "/usr/local/share/ui/sound/Connect.wav";
    _audioName[SE_Disconnected] = "/usr/local/share/ui/sound/Disconnect.wav";
    _audioName[SE_Volume_Changed] = "/usr/local/share/ui/sound/Volume.wav";

    _audioName[SE_060Test_Achieve_1] = "/usr/local/share/ui/sound/Achieve-1.wav";
    _audioName[SE_060Test_Achieve_2] = "/usr/local/share/ui/sound/Achieve-2.wav";
    _audioName[SE_060Test_Countdown] = "/usr/local/share/ui/sound/Countdown.wav";
    _audioName[SE_060Test_Countdown_0] = "/usr/local/share/ui/sound/Countdown-0.wav";
    _audioName[SE_060Test_Fail] = "/usr/local/share/ui/sound/Fail.wav";
    _audioName[SE_060Test_Slowdown] = "/usr/local/share/ui/sound/Slowdown.wav";

    memset(_fileToP, 0x00, 64);
}

CAudioPlayer::~CAudioPlayer()
{
    delete _pCmdSem;
    delete _pMutex;
    CAMERALOG("%s() destroyed", __FUNCTION__);
}

void CAudioPlayer::playSoundEffect(SoundEffect effect)
{
    _pMutex->Take();
    _curEffect = effect;
    strcpy(_fileToP, _audioName[_curEffect]);
    if (effect == SE_ContinousShot) {
        _loopPlayback = true;
    } else {
        _loopPlayback = false;
    }
    if (_pHandle != NULL) {
        _stopCurPlayback = true;
    } else {
        _stopCurPlayback = false;
    }
    _pCmdSem->Give();
    _pMutex->Give();
}

void CAudioPlayer::stopSoundEffect()
{
    _stopCurPlayback = true;
}

void CAudioPlayer::playSound(const char *file)
{
    _pMutex->Take();
    strcpy(_fileToP, file);
    _pCmdSem->Give();
    _pMutex->Give();
}

void CAudioPlayer::Stop()
{
    _stopCurPlayback = true;
}

void CAudioPlayer::main(void * p)
{
    char *buffer = NULL;
    int ret = 0;

    while (true) {
        _pCmdSem->Take(-1);
        _pMutex->Take();
        openAlsa();
        _pMutex->Give();

        buffer =(char*)malloc(_bufferSize);
        while (buffer && !_stopCurPlayback && (_pFile != NULL) && (_pHandle != NULL)) {
            memset(buffer, 0x00, _bufferSize);
            ret = fread(buffer, 1, _bufferSize, _pFile);
            //CAMERALOG("PCM read: ret = %d while _bufferSize = %d", ret, _bufferSize);
            if (ret == 0) {
                if (_loopPlayback) {
                    //CAMERALOG("%s() line %d loop playback", __FUNCTION__, __LINE__);
                    fseek(_pFile, 58, SEEK_SET);
                    continue;
                } else {
                    //CAMERALOG("%s() line %d playback finished", __FUNCTION__, __LINE__);
                    break;
                }
            } else if (ret != _bufferSize) {
            }

            ret = snd_pcm_writei(_pHandle, buffer, _frames);
            //CAMERALOG("PCM write: ret = %d", ret);
            if (ret < 0) {
                if (ret == -EPIPE) {
                    /* EPIPE means underrun */
                    CAMERALOG("underrun occurred");
                    //完成硬件参数设置，使设备准备好
                    snd_pcm_prepare(_pHandle);
                } else if (ret < 0) {
                    CAMERALOG("error from writei: %s\n", snd_strerror(ret));
                }
            } else if (ret != (int)_frames) {
                CAMERALOG("short write, write %d frames(%lu expected)", ret, _frames);
            }
        }
        if (buffer) {
            free(buffer);
            buffer = NULL;
        }

        _pMutex->Take();
        // close alsa
        if (_pHandle != NULL) {
            if (_stopCurPlayback) {
                //CAMERALOG("before snd_pcm_drop()");
                snd_pcm_drop(_pHandle);
                //CAMERALOG("after snd_pcm_drop()");
            } else {
                //CAMERALOG("before snd_pcm_drain()");
                snd_pcm_drain(_pHandle);
                //CAMERALOG("after snd_pcm_drain()");
            }
            snd_pcm_close(_pHandle);
            _pHandle = NULL;
        }
        if (_pFile != NULL) {
            fclose(_pFile);
            _pFile = NULL;
        }
        _stopCurPlayback = false;
        _loopPlayback = false;
        _pMutex->Give();
    }
}

bool CAudioPlayer::openAlsa() {
    //if ((_curEffect < SE_StartRecording) || (_curEffect >= SE_Num) ) {
    //    return false;
    //}

    if (_pFile != NULL) {
        fclose(_pFile);
        _pFile = NULL;
    }
    _pFile = fopen(_fileToP, "rb");
    if (_pFile == NULL) {
        perror("open file failed:\n");
        return false;
    }

    struct WAV_HEADER wav_header;
    int nread = 0;
    nread = fread(&wav_header, 1, sizeof(wav_header), _pFile);
    if (nread != sizeof(wav_header)) {
        CAMERALOG("%s() line %d, nread = %d", __FUNCTION__, __LINE__, nread);
    }

    int channels = wav_header.wChannels;
    int frequency = wav_header.nSamplesPersec;
    int bit = wav_header.wBitsPerSample;
    int datablock = wav_header.wBlockAlign;
    unsigned int val = 0;
    int dir = 0;
    int rc = 0;
    snd_pcm_hw_params_t *params;//硬件信息和PCM流配置
    snd_pcm_sw_params_t *sw_params;
    unsigned int buffer_time = 0;
    unsigned int period_time = 0;
    snd_pcm_uframes_t threshold = 0;
    snd_pcm_uframes_t frames = 0;

    rc = snd_pcm_open(&_pHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        perror("\nopen PCM device failed:");
        goto errorExit;
    }


    ////////////////////////////////////
    // set hw params
    ////////////////////////////////////
    rc = snd_pcm_hw_params_malloc(&params); //分配params结构体
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params_alloca:");
        goto errorExit;
    }
    rc = snd_pcm_hw_params_any(_pHandle, params);//初始化params
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params_any:");
        goto errorExit;
    }
    rc = snd_pcm_hw_params_set_access(_pHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);                                 //初始化访问权限
    if (rc < 0) {
        perror("\nsed_pcm_hw_set_access:");
        goto errorExit;
    }

    //采样位数
    switch (bit / 8)
    {
        case 1:
            rc = snd_pcm_hw_params_set_format(_pHandle, params, SND_PCM_FORMAT_U8);
            if (rc < 0) {
                perror("\nsnd_pcm_hw_params_set_format:");
                goto errorExit;
            }
            break;
        case 2:
            rc = snd_pcm_hw_params_set_format(_pHandle, params, SND_PCM_FORMAT_S16_LE);
            if (rc < 0) {
                perror("\nsnd_pcm_hw_params_set_format:");
                goto errorExit;
            }
            break;
        case 3:
            rc = snd_pcm_hw_params_set_format(_pHandle, params, SND_PCM_FORMAT_S24_LE);
            if (rc < 0) {
                perror("\nsnd_pcm_hw_params_set_format:");
                goto errorExit;
            }
            break;
    }

    rc = snd_pcm_hw_params_set_channels(_pHandle, params, channels);  //设置声道,1表示单声道，2表示立体声
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params_set_channels:");
        goto errorExit;
    }
    val = frequency;
    rc = snd_pcm_hw_params_set_rate_near(_pHandle, params, &val, &dir);  //设置频率
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params_set_rate_near:");
        goto errorExit;
    }

    rc = snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params_get_buffer_time_max:");
        goto errorExit;
    } else {
        //CAMERALOG("buffer_time = %u", buffer_time);
    }

    // Use proper wav length
    period_time = 10000; // 10ms
    if (period_time > (buffer_time / 4)) {
        period_time = (buffer_time / 4);
    }

    rc = snd_pcm_hw_params_set_period_time_near(_pHandle, params, &period_time, 0);
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params_set_period_time_near:");
        goto errorExit;
    } else {
        //CAMERALOG("period_time = %u", period_time);
    }
#if 0
    frames = 32;
    rc = snd_pcm_hw_params_set_period_size_near(_pHandle, params, &frames, 0);
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params_set_period_time_near:");
        goto errorExit;
    } else {
        CAMERALOG("frames = %lu", frames);
    }
#endif
    rc = snd_pcm_hw_params(_pHandle, params);
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params: ");
        goto errorExit;
    }

    rc = snd_pcm_hw_params_get_period_size(params, &_frames, &dir);  /*获取周期长度*/
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params_get_period_size:");
        goto errorExit;
    }

    snd_pcm_hw_params_free(params);


    ////////////////////////////////////
    // set sw params
    ////////////////////////////////////
    rc = snd_pcm_sw_params_malloc(&sw_params);
    if (rc < 0) {
        perror("\nsnd_pcm_sw_params_malloc:");
        goto errorExit;
    }

    rc = snd_pcm_sw_params_current(_pHandle, sw_params);
    if (rc < 0) {
        perror("\nsnd_pcm_sw_params_current:");
        goto errorExit;
    }

    rc = snd_pcm_sw_params_set_avail_min(_pHandle, sw_params, 4096);
    if (rc < 0) {
        perror("\nsnd_pcm_sw_params_set_avail_min:");
        goto errorExit;
    }

    threshold = 4096;
    rc = snd_pcm_sw_params_set_start_threshold(_pHandle, sw_params, threshold);
    if (rc < 0) {
        perror("\nsnd_pcm_sw_params_set_start_threshold:");
        goto errorExit;
    } else {
        //CAMERALOG("threshold = %lu", threshold);
    }

    rc = snd_pcm_sw_params(_pHandle, sw_params);
    if (rc < 0) {
        perror("\nsnd_pcm_sw_params: ");
        goto errorExit;
    }

    snd_pcm_sw_params_free(sw_params);


    ////////////////////////////////////
    // prepare
    ////////////////////////////////////
    rc = snd_pcm_prepare(_pHandle);
    if (rc < 0) {
        perror("\nsnd_pcm_prepare:");
        goto errorExit;
    }

    _bufferSize = _frames * datablock;   /*4 代表数据快长度*/
    //CAMERALOG("_frames = %ld, datablock = %d, _bufferSize = %d",
    //    _frames, datablock, _bufferSize);
    fseek(_pFile, 58, SEEK_SET);  //定位歌曲到数据区

    return true;

errorExit:
    if (_pHandle != NULL) {
        snd_pcm_close(_pHandle);
        _pHandle = NULL;
    }
    if (_pFile != NULL) {
        fclose(_pFile);
        _pFile = NULL;
    }
    return false;
}

