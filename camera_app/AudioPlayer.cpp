

#include<stdio.h>
#include<stdlib.h>
#include <string.h>

#include "AudioPlayer.h"

extern void CameraDelayms(int time_ms);

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

short simpleAudioMix(short buffer1, short buffer2)
{
    int value = buffer1 + buffer2;
    return (short)(value / 2);
}

short simpleAudioMix(short buffer1, short buffer2, short buffer3)
{
    int value = buffer1 + buffer2 + buffer3;
    return (short)(value / 3);
}

short simpleAudioMix(short buffer1, short buffer2, short buffer3, short buffer4)
{
    int value = buffer1 + buffer2 + buffer3 + buffer4;
    return (short)(value / 4);
}

void NormalizationMix(char* sourseSound, int trackBytes, int number, char *objectSound)
{
    //归一化混音
    int const MAX = 32767;
    int const MIN = -32768;

    double f = 1.0;
    int output;
    int i = 0,j = 0;
    for (i = 0; i < trackBytes / 2; i++) {
        int temp=0;
        for (j = 0; j < number; j++) {
            temp += *(short*)(sourseSound + (j * trackBytes) + i * 2);
        }
        output = (int)(temp * f);
        if (output > MAX) {
            f = (double)MAX / (double)(output);
            output = MAX;
        }
        if (output < MIN) {
            f = (double)MIN / (double)(output);
            output = MIN;
        }
        if (f < 1) {
            f += ((double)1 - f) / (double)32;
        }
        *(short*)(objectSound + i * 2) = (short)output;
    }
}

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
  : CThread("AudioPlayer")
  , _numPlaying(0)
  , _pHandle(NULL)
  , _bufferSize(0)
  , _bStandby(true)
  , _stopCurPlayback(false)
  , _loopPlayback(false)
{
    _pCmdSem = new CSemaphore(0, 1, "Audio Player");
    _pMutex = new CMutex("Audio Player");

    for (int i = 0; i < MAX_PLAYING_NUM; i++) {
        _pFiles[i] = NULL;
    }

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
}

CAudioPlayer::~CAudioPlayer()
{
    delete _pCmdSem;
    delete _pMutex;
    CAMERALOG("%s() destroyed", __FUNCTION__);
}

void CAudioPlayer::playSoundEffect(SoundEffect effect)
{
    if ((effect < SE_StartRecording) || (effect >= SE_Num)) {
        return;
    }

    _pMutex->Take();

    if (_numPlaying >= MAX_PLAYING_NUM) {
        CAMERALOG("Too many audio effects!");
        _pMutex->Give();
        return;
    }

    //CAMERALOG("_numPlaying = %d", _numPlaying);

    for (int i = 0; i < MAX_PLAYING_NUM; i++) {
        if (_pFiles[i] == NULL) {
            _pFiles[i] = fopen(_audioName[effect], "rb");
            if (_pFiles[i] == NULL) {
                CAMERALOG("open file %s failed!", _audioName[effect]);
                _pMutex->Give();
                return;
            }
            fseek(_pFiles[i], 58, SEEK_SET);  //定位歌曲到数据区
            _numPlaying++;
            break;
        }
    }

    if (_bStandby) {
        _bStandby = false;
        _pCmdSem->Give();
    }

    _pMutex->Give();

#if 0
    // A simple audio mix code
    FILE *pFile_1 = fopen(_audioName[SE_StartRecording], "rb");
    if (pFile_1 == NULL) {
        CAMERALOG("open file %s failed!", _audioName[SE_StartRecording]);
        return;
    }

    FILE *pFile_2 = fopen(_audioName[SE_SingleShot], "rb");
    if (pFile_2 == NULL) {
        CAMERALOG("open file %s failed!", _audioName[SE_Volume_Changed]);
        return;
    }

    if (!_pHandle) {
        _pFile = pFile_1;
        openAlsa();
    }

    char *buffer_1 = (char*)malloc(_bufferSize);
    char *buffer_2 = (char*)malloc(_bufferSize);
    char *buffer_p = (char*)malloc(_bufferSize);
    int ret = 0;
    bool f1_end = false;
    bool f2_end = false;
    while (true) {
        memset(buffer_1, 0x00, _bufferSize);
        memset(buffer_2, 0x00, _bufferSize);
        memset(buffer_p, 0x00, _bufferSize);

        ret = fread(buffer_1, 1, _bufferSize, pFile_1);
        if (ret == 0) {
            f1_end = true;
        }

        ret = fread(buffer_2, 1, _bufferSize, pFile_2);
        if (ret == 0) {
            f2_end = true;
        }

        if (!f1_end && !f2_end) {
            for (int i = 0; i < _bufferSize / 2; i++) {
                short *a_p = (short *)(buffer_p + i * 2);
                short *a_1 = (short *)(buffer_1 + i * 2);
                short *a_2 = (short *)(buffer_2 + i * 2);

                *a_p = simpleAudioMix(*a_1, *a_2);
            }
        } else if (!f1_end) {
            memcpy(buffer_p, buffer_1, _bufferSize);
        } else if (!f2_end) {
            memcpy(buffer_p, buffer_2, _bufferSize);
        }

        ret = snd_pcm_writei(_pHandle, buffer_p, _frames);
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
        } else {
            // TODO: standby
        }

        if (f1_end && f2_end) {
            break;
        }
    }

    fclose(pFile_1);
    fclose(pFile_2);
    free(buffer_1);
    free(buffer_2);
    free(buffer_p);
#endif
}

void CAudioPlayer::stopSoundEffect()
{
    // TODO: stop loop playback
}

void CAudioPlayer::playSound(const char *file)
{
    // TODO: support loop playback
}

void CAudioPlayer::Stop()
{
    _stopCurPlayback = true;
}

bool CAudioPlayer::prepareTracks(char *buffer)
{
    if (_numPlaying <= 0) {
        return false;
    } else if (_numPlaying == 1) {
        for (int i = 0; i < MAX_PLAYING_NUM; i++) {
            if (_pFiles[i]) {
                int ret = fread(buffer, 1, _bufferSize, _pFiles[i]);
                if (ret == 0) {
                    fclose(_pFiles[i]);
                    _pFiles[i] = NULL;
                    _numPlaying--;
                    //CAMERALOG("File read finished! _numPlaying = %d", _numPlaying);
                }
                break;
            }
        }
    } else {
        char *bufferTrack = new char [_bufferSize * _numPlaying];
        memset(bufferTrack, 0x00, _bufferSize * _numPlaying);
        int index = 0;
        int ret = 0;
        int originNum = _numPlaying;
        for (int i = 0; i < MAX_PLAYING_NUM; i++) {
            if (_pFiles[i]) {
                ret = fread(bufferTrack + index * originNum,
                    1, _bufferSize, _pFiles[i]);
                if (ret == 0) {
                    fclose(_pFiles[i]);
                    _pFiles[i] = NULL;
                    _numPlaying--;
                    //CAMERALOG("File read finished! _numPlaying = %d", _numPlaying);
                }
                index++;
            }
        }
#if 0
        if (originNum == 2) {
            CAMERALOG("mix 2 audio tracks");
            for (int i = 0; i < _bufferSize / 2; i++) {
                short *a_1 = (short *)(bufferTrack + i * 2);
                short *a_2 = (short *)(bufferTrack + _bufferSize + i * 2);
                short *a_p = (short *)(buffer + i * 2);

                *a_p = simpleAudioMix(*a_1, *a_2);
            }
        } else if (originNum == 3) {
            CAMERALOG("mix 3 audio tracks");
            for (int i = 0; i < _bufferSize / 2; i++) {
                short *a_1 = (short *)(bufferTrack + i * 2);
                short *a_2 = (short *)(bufferTrack + _bufferSize + i * 2);
                short *a_3 = (short *)(bufferTrack + _bufferSize * 2 + i * 2);
                short *a_p = (short *)(buffer + i * 2);

                *a_p = simpleAudioMix(*a_1, *a_2, *a_3);
            }
        } else if (originNum == 4) {
            CAMERALOG("mix 4 audio tracks");
            for (int i = 0; i < _bufferSize / 2; i++) {
                short *a_1 = (short *)(bufferTrack + i * 2);
                short *a_2 = (short *)(bufferTrack + _bufferSize + i * 2);
                short *a_3 = (short *)(bufferTrack + _bufferSize * 2 + i * 2);
                short *a_4 = (short *)(bufferTrack + _bufferSize * 3 + i * 2);
                short *a_p = (short *)(buffer + i * 2);

                *a_p = simpleAudioMix(*a_1, *a_2, *a_3, *a_4);
            }
        }
#else
        NormalizationMix(bufferTrack, _bufferSize, originNum, buffer);
#endif
        delete [] bufferTrack;
    }

    return true;
}

void CAudioPlayer::main()
{
    char *buffer = NULL;
    int ret = 0;

    while (true) {
        _pCmdSem->Take(-1);
        openAlsa();

        int idleSentNum = 0;
        buffer = new char [_bufferSize];
        while (!_bStandby) {
            memset(buffer, 0x00, _bufferSize);

            bool ready = false;

            _pMutex->Take();

            if (_numPlaying > 0) {
                ready = prepareTracks(buffer);
            }
// keep playing 0x00 in order to avoid the huge latency of open audio pipeline
#if 0
            else {
                if (idleSentNum >= 100) {
                    _bStandby = true;
                    snd_pcm_drop(_pHandle);
                    snd_pcm_close(_pHandle);
                    _pHandle = NULL;
                    //CAMERALOG("Enter standby");
                    _pMutex->Give();
                    break;
                }
            }
#endif

            _pMutex->Give();

            if (ready && _pHandle) {
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
                idleSentNum = 0;
            } else {
                snd_pcm_sframes_t framesRdy = snd_pcm_avail(_pHandle);
                // If buffer is not big enough, underrun will happen, write some silence buffer,
                // otherwise sleep for an alsa period time.
                if (framesRdy >= _maxFramesWritable - _frames * 4) {
                    if (_pHandle) {
                        ret = snd_pcm_writei(_pHandle, buffer, _frames);
                        //CAMERALOG("PCM write: ret = %d", ret);
                        if (ret == -EPIPE) {
                            /* EPIPE means underrun */
                            CAMERALOG("underrun occurred");
                            //完成硬件参数设置，使设备准备好
                            snd_pcm_prepare(_pHandle);
                        }
                    }
                } else {
                    // sleep 10ms(an alsa period time), waiting data to be consumed
                    CameraDelayms(10);
                }
                idleSentNum++;
            }
        }

        delete [] buffer;
    }
}

//-----------------------------------------
// only support: 16bit, 1 channel, 48000Hz
//-----------------------------------------
bool CAudioPlayer::openAlsa() {
    int channels = 1;
    int frequency = 48000;
    unsigned int val = 0;
    int dir = 0;
    int rc = 0;
    snd_pcm_hw_params_t *params;//硬件信息和PCM流配置
    snd_pcm_sw_params_t *sw_params;
    unsigned int buffer_time = 0;
    unsigned int period_time = 0;
    snd_pcm_uframes_t threshold = 0;
    //snd_pcm_uframes_t frames = 0;

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
    rc = snd_pcm_hw_params_set_format(_pHandle, params, SND_PCM_FORMAT_S16_LE);
    if (rc < 0) {
        perror("\nsnd_pcm_hw_params_set_format:");
        goto errorExit;
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

    rc = snd_pcm_sw_params_get_avail_min(sw_params, &threshold);
    if (rc < 0) {
        perror("\nsnd_pcm_sw_params_get_avail_min:");
        goto errorExit;
    }
    //CAMERALOG("snd_pcm_sw_params_get_avail_min = %lu, _frames = %d", threshold, _frames);
    if (threshold < _frames * 2) {
        threshold = _frames * 2;
    }
    rc = snd_pcm_sw_params_set_avail_min(_pHandle, sw_params, threshold);
    if (rc < 0) {
        perror("\nsnd_pcm_sw_params_set_avail_min:");
        goto errorExit;
    }

    threshold = 2048;
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

    // each frame is 2 bytes
    _bufferSize = _frames * 2;
    //CAMERALOG("_frames = %ld, datablock = %d, _bufferSize = %d",
    //    _frames, datablock, _bufferSize);

    _maxFramesWritable = snd_pcm_avail(_pHandle);
    //CAMERALOG("max frames for written = %d, _frames = %d", _maxFramesWritable, _frames);

    return true;

errorExit:
    if (_pHandle != NULL) {
        snd_pcm_close(_pHandle);
        _pHandle = NULL;
    }
    return false;
}

