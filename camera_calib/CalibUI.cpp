
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <math.h>

#include <agbox.h>
#include <agcmd.h>
#include <aglog.h>
#include <aglib.h>
#include <agbt.h>
#include <agsys_iio.h>
#include <aggps.h>

extern "C" {
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <agbt_hci.h>
#include <agbt_util.h>
}

#include "CalibUI.h"

int calib_enter_preview(int fd_iav);

namespace ui {

#define  BG_COLOR       0x1863
#define  INFO_COLOR     0xFFFF
#define  CONTENT_COLOR  0x07FF
#define  ERROR_COLOR    0xF000

#define Color_Video_Transparency   0x1863

#define Color_Black    0x0000   //  <- 0x000000

#define Color_Grey_1   0x18A3   //  <- 0x151419

#define Color_Grey_2   0x3187   //  <- 0x313038

#define Color_Grey_3   0x6B6F   //  <- 0x686c7c

#define Color_Grey_4   0xA558   //  <- 0xa5abc3

#define Color_White    0xFFFF   //  <- 0xffffff

#define Color_Yellow   0xffE3   //  <- 0xffff15

#define Color_Red_1    0xE1C5   //  <- 0xe83828

#define Color_Green_1  0x4E4C   //  <- 0x4aca62

#define Color_Blue_1   0x361D   //  <- 0x2fc3ec


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


SimpleTestWnd::SimpleTestWnd(const char *name, Size wndSize, Point leftTop)
    : Window(name, wndSize, leftTop)
    , _state(STATE_IDLE)
{
#if 0
    struct agsys_platform_info_s platform_info;
    int rval = agsys_platform_get_info(&platform_info);
    CAMERALOG("Board: Hardware[%s], Revision[0x%08X], SN[%s]",
        platform_info.hardware, platform_info.revision,
        platform_info.sn);
    CAMERALOG("Ambarella: Chip[%d], Type[%d], Rev[%d], POC[%08X]",
        platform_info.ambarella_info.board_chip,
        platform_info.ambarella_info.board_type,
        platform_info.ambarella_info.board_rev,
        platform_info.ambarella_info.board_poc);
#endif
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 30);
    _pPanel->setBgColor(BG_COLOR);
    this->SetMainObject(_pPanel);

    strcpy(_title, "Calib");
    _pTitle = new StaticText(_pPanel, Point(0, 20), Size(400, 80));
    _pTitle->SetStyle(28, INFO_COLOR, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)_title);

    strcpy(_content, "Calib Test");
    _pTxt = new StaticText(_pPanel, Point(20, 110), Size(360, 200));
    _pTxt->SetStyle(28, CONTENT_COLOR, CENTER, TOP);
    _pTxt->SetText((UINT8 *)_content);

    _pPowerOff = new Button(_pPanel, Point(26, 230), Size(164, 80), 0x20C4, 0x8410);
    _pPowerOff->SetText((UINT8 *)"Exit", 32, 0xFFFF);
    _pPowerOff->setOnClickListener(this);

    _pNext = new Button(_pPanel, Point(210, 230), Size(164, 80), 0x20C4, 0x8410);
    _pNext->SetText((UINT8 *)"Start", 32, 0xFFFF);
    _pNext->setOnClickListener(this);

    char ver[64];
    agcmd_property_get("prebuild.system.version", ver, "Unknown");
    memset(_version, 0x00, 64);
    snprintf(_version, 64, "ver: %s", ver);
    _pVersion = new StaticText(_pPanel, Point(0, 310), Size(400, 28));
    _pVersion->SetStyle(20, INFO_COLOR, CENTER, MIDDLE);
    _pVersion->SetText((UINT8 *)_version);

    agcmd_property_get("prebuild.system.bsp", ver, "NA");
    memset(_board, 0x00, 64);
    snprintf(_board, 64, "BSP: %s", ver);
    _pBoard = new StaticText(_pPanel, Point(0, 338), Size(400, 28));
    _pBoard->SetStyle(20, INFO_COLOR, CENTER, MIDDLE);
    _pBoard->SetText((UINT8 *)_board);
}

SimpleTestWnd::~SimpleTestWnd()
{
}

void SimpleTestWnd::onClick(Control *widget)
{
    if (widget == _pPowerOff) {
        int ret = 0;
        if (STATE_FAILED == _state) {
            ret = 2;
        }
        _pPowerOff->SetHiden();
        _pNext->SetHiden();
        snprintf(_content, 128, "exit(%d)", ret);
        _pPanel->Draw(true);

        exit(ret);
    }
}

void SimpleTestWnd::willHide()
{
}

void SimpleTestWnd::willShow()
{
}

bool SimpleTestWnd::OnEvent(CEvent *event)
{
    return true;
}


CalibBeginWnd::CalibBeginWnd(const char *name, Size wndSize, Point leftTop)
    : SimpleTestWnd(name, wndSize, leftTop)
{
}

CalibBeginWnd::~CalibBeginWnd()
{
}

void CalibBeginWnd::onClick(Control *widget)
{
    if (widget == _pNext) {
        this->StartWnd(WND_BATTERY, Anim_Right2LeftEnter);
    } else {
        SimpleTestWnd::onClick(widget);
    }
}


BatteryTestWnd::BatteryTestWnd(const char *name, Size wndSize, Point leftTop)
    : SimpleTestWnd(name, wndSize, leftTop)
    , _counter(0)
{
    strcpy(_title, "Battery");
    snprintf(_content, sizeof(_content), "Battery Test");
    _pPowerOff->SetHiden();
    _pNext->SetHiden();
}

BatteryTestWnd::~BatteryTestWnd()
{
}

bool BatteryTestWnd::doTest()
{
    // firstly check GPIO
    char usbphy0_data[1024];
    char *sw_str;
    int ret_val = aglib_read_file_buffer("/proc/ambarella/usbphy0",
        usbphy0_data, (sizeof(usbphy0_data) - 1));
    if (ret_val < 0) {
        snprintf(_content, sizeof(_content),
            "read /proc/ambarella/usbphy0 failed with error %d", ret_val);
        _pPanel->Draw(true);
        return false;
    }
    usbphy0_data[ret_val] = 0;
    CAMERALOG("%s", usbphy0_data);
    // GPIO
    int gpio = -1;
    sw_str = strstr(usbphy0_data, "GPIO[");
    if (sw_str) {
        gpio = strtoul((sw_str + 5), NULL, 0);
    }
    if (gpio != 0) {
        snprintf(_content, sizeof(_content),
            "%s", usbphy0_data);
        _pPanel->Draw(true);
        return false;
    }

    // Check battery:
    ret_val = -1;
    char power_supply_prop[AGCMD_PROPERTY_SIZE];
    const char delimiters[] = " ";
    char *token;
    char *prop_key_buf = NULL;
    char *prop_key = NULL;
    char power_supply_event_name[AGCMD_PROPERTY_SIZE];
    int event_fd;

    agcmd_property_get("prebuild.board.power_supply", power_supply_prop, "");
    ret_val = strlen(power_supply_prop);
    if (ret_val == 0) {
        CAMERALOG("Power supply skip ...");
        snprintf(_content, sizeof(_content), "Power supply skip ...");
        _pPanel->Draw(true);
        return true;
    }

    CAMERALOG("Power supply: %s", power_supply_prop);
    snprintf(_content, sizeof(_content), "Power supply: %s", power_supply_prop);
    _pPanel->Draw(true);
    usleep(500 * 1000);

    prop_key_buf = prop_key = strdup(power_supply_prop);
    do {
        token = strsep(&prop_key, delimiters);
        if (token) {
            snprintf(power_supply_event_name,
                sizeof(power_supply_event_name),
                "/sys/class/power_supply/%s/uevent",
                token);
            event_fd = open(power_supply_event_name, O_RDONLY);
            if (event_fd < 0) {
                CAMERALOG("Power supply: %s fail", token);
                snprintf(_content, sizeof(_content), "Power supply:\n%s fail", token);
                _pPanel->Draw(true);
                return false;
            } else {
                CAMERALOG("Power supply: %s pass", token);
                snprintf(_content, sizeof(_content), "Power supply:\n%s pass", token);
                _pPanel->Draw(true);
                sleep(1);
                close(event_fd);
            }
        }
    } while(token);

    if (prop_key_buf) {
        free(prop_key_buf);
    }
    return true;
}

void BatteryTestWnd::onClick(Control *widget)
{
    if (widget == _pNext) {
        if (STATE_SUCCESS == _state) {
            this->StartWnd(WND_Audio, Anim_Right2LeftEnter);
        }
    } else {
        SimpleTestWnd::onClick(widget);
    }
}

bool BatteryTestWnd::OnEvent(CEvent *event)
{
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter == 2) {
                    _state = STATE_TESTING;
                    _pPowerOff->SetHiden();
                    _pNext->SetHiden();
                    memset(_content, 0x00, sizeof(_content));
                    snprintf(_content, sizeof(_content), "Battery Testing...");
                    _pPanel->Draw(true);

                    if (doTest()) {
                        _state = STATE_SUCCESS;
                        _pPowerOff->SetShow();
                        _pNext->SetShow();
                        _pNext->SetText((UINT8 *)"Next", 32, 0xFFFF);
                        memset(_content, 0x00, sizeof(_content));
                        snprintf(_content, sizeof(_content), "Battery Success!");
                        _pPanel->Draw(true);
                    } else {
                        _state = STATE_FAILED;
                        _pTxt->SetColor(ERROR_COLOR);
                        _pPowerOff->SetShow();
                        _pPanel->Draw(true);
                    }
                }
            }
            break;
        default:
            break;
    }
    return true;
}


AudioTestWnd::AudioTestWnd(const char *name, Size wndSize, Point leftTop)
    : SimpleTestWnd(name, wndSize, leftTop)
    , _counter(0)
{
    strcpy(_title, "Audio");
    snprintf(_content, 128, "Audio Test");
    _pPowerOff->SetHiden();
    _pNext->SetHiden();
}

AudioTestWnd::~AudioTestWnd()
{
}

bool AudioTestWnd::audio_playback_test()
{
    snprintf(_content, 128, "Playback test");
    _pPanel->Draw(true);

    // Set speaker volume to max
    int ret_val = -1;
    const char *volume_args[8];
    char volume_str[AGCMD_PROPERTY_SIZE];
    snprintf(volume_str, sizeof(volume_str), "%d", 10);
    volume_args[0] = "agboard_helper";
    volume_args[1] = "--spk_volume";
    volume_args[2] = volume_str;
    volume_args[3] = NULL;
    ret_val = agcmd_agexecvp(volume_args[0], (char * const *)volume_args);
    if (ret_val) {
        CAMERALOG("#### change speaker volume failed");
    }

    FILE *pFile = fopen("/usr/local/share/ui/diablo/continuous-shot-48KHz-16bit-1ch.wav", "rb");
    if (pFile == NULL) {
        snprintf(_content, 128, "open wav file failed");
        _pPanel->Draw(true);
        return false;
    }

    struct WAV_HEADER wav_header;
    int nread = 0;
    nread = fread(&wav_header, 1, sizeof(wav_header), pFile);
    if (nread != sizeof(wav_header)) {
        printf("%s() line %d, nread = %d", __FUNCTION__, __LINE__, nread);
    }

    int channels = wav_header.wChannels;
    int frequency = wav_header.nSamplesPersec;
    int bit = wav_header.wBitsPerSample;
    int datablock = wav_header.wBlockAlign;
    unsigned int val;
    int dir = 0;
    int rc = 0;
    snd_pcm_hw_params_t *params;//硬件信息和PCM流配置

    snd_pcm_t *pHandle = NULL;
    snd_pcm_uframes_t frames;
    int bufferSize;
    int ret = 0;
    char *buffer = NULL;

    rc = snd_pcm_open(&pHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        snprintf(_content, 128, "alsa open PCM device failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    snd_pcm_hw_params_alloca(&params); //分配params结构体
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_alloca() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }
    rc = snd_pcm_hw_params_any(pHandle, params);//初始化params
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_any() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }
    rc = snd_pcm_hw_params_set_access(pHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);                                 //初始化访问权限
    if (rc < 0) {
        snprintf(_content, 128, "sed_pcm_hw_set_access() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    //采样位数
    switch (bit / 8)
    {
        case 1:
            snd_pcm_hw_params_set_format(pHandle, params, SND_PCM_FORMAT_U8);
            break;
        case 2:
            snd_pcm_hw_params_set_format(pHandle, params, SND_PCM_FORMAT_S16_LE);
            break;
        case 3:
            snd_pcm_hw_params_set_format(pHandle, params, SND_PCM_FORMAT_S24_LE);
            break;
    }
    rc = snd_pcm_hw_params_set_channels(pHandle, params, channels);  //设置声道,1表示单声道，2表示立体声
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_channels() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }
    val = frequency;
    rc = snd_pcm_hw_params_set_rate_near(pHandle, params, &val, &dir);  //设置频率
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_rate_near() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    rc = snd_pcm_hw_params(pHandle, params);
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    rc = snd_pcm_hw_params_get_period_size(params, &frames, &dir);  /*获取周期长度*/
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_get_period_size() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    bufferSize = frames * datablock;   /*4 代表数据快长度*/
    fseek(pFile, 58, SEEK_SET);  //定位歌曲到数据区

    buffer =(char*)malloc(bufferSize);
    if (!buffer) {
        snprintf(_content, 128, "malloc error, out of memory");
        _pPanel->Draw(true);
        goto errorExit;
    }
    do {
        memset(buffer, 0, bufferSize);
        ret = fread(buffer, 1, bufferSize, pFile);
        if (ret == 0) {
            //printf("%s() line %d playback finished\n", __FUNCTION__, __LINE__);
            break;
        } else if (ret != bufferSize) {
        }

        ret = snd_pcm_writei(pHandle, buffer, frames);
        if (ret < 0) {
            if (ret == -EPIPE) {
                /* EPIPE means underrun */
                fprintf(stderr, "underrun occurred\n");
                //完成硬件参数设置，使设备准备好
                snd_pcm_prepare(pHandle);
            } else if (ret < 0) {
                fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
            }
        }
    }while (true);

    if (buffer) {
        free(buffer);
    }
    snd_pcm_drain(pHandle);
    snd_pcm_close(pHandle);
    fclose(pFile);
    return true;

errorExit:
    if (buffer) {
        free(buffer);
    }
    if (pHandle != NULL) {
        snd_pcm_close(pHandle);
        pHandle = NULL;
    }
    if (pFile != NULL) {
        fclose(pFile);
        pFile = NULL;
    }
    return false;
}

bool AudioTestWnd::audio_capture_test()
{
    int ret_val = calib_diag_audio_read();
    if (ret_val < 0) {
        return false;
    }

    if (!calib_diag_audio_play()) {
        return false;
    }

    return true;
}

int AudioTestWnd::calib_diag_audio_config(snd_pcm_t *p_snd_pcm)
{
    int ret_val = 0;
    snd_pcm_hw_params_t *p_hwparams = NULL;
    uint32_t period_time;
    uint32_t buffer_time;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_sw_params_t *p_swparams = NULL;
    snd_pcm_uframes_t start_threshold;
    snd_pcm_format_t snd_pcm_format;
    uint32_t snd_pcm_channels;
    uint32_t snd_pcm_rate;

    snd_pcm_format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_rate = 48000;
    snd_pcm_channels = 2;

    ret_val = snd_pcm_hw_params_malloc(&p_hwparams);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_malloc() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_any(p_snd_pcm, p_hwparams);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_any() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_set_access(p_snd_pcm, p_hwparams,
        SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_access() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_set_format(p_snd_pcm, p_hwparams,
        snd_pcm_format);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_format() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_set_channels(p_snd_pcm, p_hwparams,
        snd_pcm_channels);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_channels() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_set_rate(p_snd_pcm, p_hwparams,
        snd_pcm_rate, 0);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_rate() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_get_buffer_time_max(p_hwparams,
        &buffer_time, 0);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_get_buffer_time_max() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_set_buffer_time_near(p_snd_pcm,
        p_hwparams, &buffer_time, 0);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_buffer_time_near() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_get_buffer_size_max(
        p_hwparams, &buffer_size);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_get_buffer_size_max() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    period_time = buffer_time / 4;
    ret_val = snd_pcm_hw_params_set_period_time_near(
        p_snd_pcm, p_hwparams, &period_time, 0);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_period_time_near() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params(p_snd_pcm, p_hwparams);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_get_period_size(p_hwparams,
        &period_size, 0);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_get_period_size() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_hw_params_get_period_time(p_hwparams,
        &period_time, 0);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_get_period_time() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    if (period_size >= buffer_size) {
        snprintf(_content, 128, "alsa period_size config failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    if (period_time >= buffer_time) {
        snprintf(_content, 128, "alsa period_time config failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }

    ret_val = snd_pcm_sw_params_malloc(&p_swparams);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_sw_params_malloc() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_sw_params_current(p_snd_pcm, p_swparams);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_sw_params_current() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_sw_params_set_avail_min(p_snd_pcm,
        p_swparams, period_size);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_sw_params_set_avail_min() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    start_threshold = 1;
    ret_val = snd_pcm_sw_params_set_start_threshold(p_snd_pcm,
        p_swparams, start_threshold);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_sw_params_set_start_threshold() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_sw_params_set_stop_threshold(p_snd_pcm,
        p_swparams, buffer_size);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_sw_params_set_stop_threshold() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }
    ret_val = snd_pcm_sw_params(p_snd_pcm, p_swparams);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_sw_params() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }

    ret_val = snd_pcm_prepare(p_snd_pcm);
    if (ret_val < 0) {
        snprintf(_content, 128, "snd_pcm_prepare() failed");
        _pPanel->Draw(true);
        goto calib_diag_audio_config_exit;
    }

calib_diag_audio_config_exit:
    if (p_hwparams)
        snd_pcm_hw_params_free(p_hwparams);
    if (p_swparams)
        snd_pcm_sw_params_free(p_swparams);

    return ret_val;
}

int AudioTestWnd::calib_diag_audio_read()
{
    snprintf(_content, 128, "CAPTURE test\n\nRecording...");
    _pPanel->Draw(true);

    int ret_val = -1;
    snd_pcm_t *p_snd_pcm = NULL;
    ret_val = snd_pcm_open(&p_snd_pcm, "default",
        SND_PCM_STREAM_CAPTURE, 0);
    if (ret_val < 0) {
        snprintf(_content, 128, "Fail: Audio CAPTURE open");
        _pPanel->Draw(true);
        return ret_val;
    }

    ret_val = calib_diag_audio_config(p_snd_pcm);
    if (ret_val < 0) {
        snd_pcm_close(p_snd_pcm);
        return ret_val;
    }

    _match_count = 0;
    memset(_audioBuffer, 0x00, 512 * 1024);

    int read_bytes = 0;
    size_t read_count = 1024;
    while (read_bytes < 508 * 1024) {
        ret_val = snd_pcm_readi(p_snd_pcm, _audioBuffer + read_bytes, read_count);
        if (ret_val == -EAGAIN) {
            snprintf(_content, 128, "Audio wait 0 ...");
            _pPanel->Draw(true);
            snd_pcm_wait(p_snd_pcm, 1000);
        } else if (ret_val < 0) {
            CAMERALOG("snd_pcm_readi() = %d", ret_val);
            snprintf(_content, 128, "snd_pcm_readi() failed");
            _pPanel->Draw(true);
            break;
        } else {
            read_bytes += ret_val * 4;
            _match_count++;
        }
    }

    if (ret_val < 0) {
        snprintf(_content, 128, "Read Fail:\nAudio read(%d)", _match_count);
        _pPanel->Draw(true);
    } else {
        snprintf(_content, 128, "Read OK:\nAudio read(%d)", _match_count);
        _pPanel->Draw(true);
        usleep(1000 * 1000);
    }

    return ret_val;
}

bool AudioTestWnd::calib_diag_audio_play()
{
    snprintf(_content, 128, "CAPTURE test\n\nPlayback...");
    _pPanel->Draw(true);

    // Set speaker volume to max
    int ret_val = -1;
    const char *volume_args[8];
    char volume_str[AGCMD_PROPERTY_SIZE];
    snprintf(volume_str, sizeof(volume_str), "%d", 10);
    volume_args[0] = "agboard_helper";
    volume_args[1] = "--spk_volume";
    volume_args[2] = volume_str;
    volume_args[3] = NULL;
    ret_val = agcmd_agexecvp(volume_args[0], (char * const *)volume_args);
    if (ret_val) {
        CAMERALOG("#### change speaker volume failed");
    }

    int channels = 2;
    int frequency = 48000;
    unsigned int val;
    int dir = 0;
    int rc = 0;
    int bytes_sent = 0;
    snd_pcm_hw_params_t *params;//硬件信息和PCM流配置

    snd_pcm_t *pHandle = NULL;
    snd_pcm_uframes_t frames;
    int bufferSize;
    int ret = 0;
    char *buffer = NULL;

    rc = snd_pcm_open(&pHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        snprintf(_content, 128, "alsa open PCM device failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    snd_pcm_hw_params_alloca(&params); //分配params结构体
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_alloca() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }
    rc = snd_pcm_hw_params_any(pHandle, params);//初始化params
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_any() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }
    rc = snd_pcm_hw_params_set_access(pHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);                                 //初始化访问权限
    if (rc < 0) {
        snprintf(_content, 128, "sed_pcm_hw_set_access() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    //采样位数
    snd_pcm_hw_params_set_format(pHandle, params, SND_PCM_FORMAT_S16_LE);

    rc = snd_pcm_hw_params_set_channels(pHandle, params, channels);  //设置声道,1表示单声道，2表示立体声
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_channels() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }
    val = frequency;
    rc = snd_pcm_hw_params_set_rate_near(pHandle, params, &val, &dir);  //设置频率
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_set_rate_near() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    rc = snd_pcm_hw_params(pHandle, params);
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    rc = snd_pcm_hw_params_get_period_size(params, &frames, &dir);  /*获取周期长度*/
    if (rc < 0) {
        snprintf(_content, 128, "snd_pcm_hw_params_get_period_size() failed");
        _pPanel->Draw(true);
        goto errorExit;
    }

    bufferSize = frames * 4;
    buffer =(char*)malloc(bufferSize);
    if (!buffer) {
        snprintf(_content, 128, "malloc error, out of memory");
        _pPanel->Draw(true);
        goto errorExit;
    }
    bytes_sent = 0;
    while (bytes_sent < 512 * 1024 - bufferSize)
    {
        memcpy(buffer, _audioBuffer + bytes_sent, bufferSize);
        ret = snd_pcm_writei(pHandle, buffer, frames);
        if (ret < 0) {
            if (ret == -EPIPE) {
                /* EPIPE means underrun */
                fprintf(stderr, "underrun occurred\n");
                //完成硬件参数设置，使设备准备好
                snd_pcm_prepare(pHandle);
            } else if (ret < 0) {
                fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
            }
        }
        bytes_sent += ret * 4;
    }

    if (buffer) {
        free(buffer);
    }
    snd_pcm_drain(pHandle);
    snd_pcm_close(pHandle);
    return true;

errorExit:
    if (buffer) {
        free(buffer);
    }
    if (pHandle != NULL) {
        snd_pcm_close(pHandle);
        pHandle = NULL;
    }
    return false;
}

bool AudioTestWnd::doTest()
{
    int ret_val = -1;
    char support_audio_prop[AGCMD_PROPERTY_SIZE];

    agcmd_property_get("prebuild.board.support_audio",
        support_audio_prop, "1");
    ret_val = strtoul(support_audio_prop, NULL, 0);
    if (ret_val == 0) {
        snprintf(_content, 128, "Audio skip ...");
        _pPanel->Draw(true);
        return true;
    }

    if (!audio_playback_test()) {
        return false;
    }

    if (!audio_capture_test()) {
        return false;
    }

    return true;
}

void AudioTestWnd::onClick(Control *widget)
{
    if (widget == _pNext) {
        if (STATE_SUCCESS == _state) {
            this->StartWnd(WND_IIO, Anim_Right2LeftEnter);
        }
    } else {
        SimpleTestWnd::onClick(widget);
    }
}

bool AudioTestWnd::OnEvent(CEvent *event)
{
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter == 2) {
                    _state = STATE_TESTING;
                    _pPowerOff->SetHiden();
                    _pNext->SetHiden();

                    if (doTest()) {
                        _state = STATE_SUCCESS;
                        _pPowerOff->SetShow();
                        _pNext->SetShow();
                        _pNext->SetText((UINT8 *)"Next", 32, 0xFFFF);
                        memset(_content, 0x00, 128);
                        snprintf(_content, 128, "Audio Success!");
                        _pPanel->Draw(true);
                    } else {
                        _state = STATE_FAILED;
                        _pTxt->SetColor(ERROR_COLOR);
                        _pPowerOff->SetShow();
                        _pPanel->Draw(true);
                    }
                }
            }
            break;
        default:
            break;
    }
    return true;
}


IIOTestWnd::IIOTestWnd(const char *name, Size wndSize, Point leftTop)
    : SimpleTestWnd(name, wndSize, leftTop)
    , _counter(0)
{
    strcpy(_title, "IIO");
    strcpy(_content, "IIO Test");
    _pPowerOff->SetHiden();
    _pNext->SetHiden();
}

IIOTestWnd::~IIOTestWnd()
{
}

bool IIOTestWnd::doTest()
{
    int ret_val = -1;
    char support_iio_prop[AGCMD_PROPERTY_SIZE];

    agcmd_property_get("prebuild.board.support_iio",
        support_iio_prop, "1");
    ret_val = strtoul(support_iio_prop, NULL, 0);
    if (ret_val == 0) {
        snprintf(_content, 128, "IIO skip ...");
        _pPanel->Draw(true);
        return true;
    }

    snprintf(_content, 128, "accel testing...");
    _pPanel->Draw(true);
    if (!iioAccelTest()) {
        return false;
    }
    sleep(1);

    snprintf(_content, 128, "orientation testing...");
    _pPanel->Draw(true);
    if (!iioOrientationTest()) {
        return false;
    }
    sleep(1);

    snprintf(_content, 128, "gyro testing...");
    _pPanel->Draw(true);
    if (!iioGyroTest()) {
        return false;
    }
    sleep(1);

    snprintf(_content, 128, "magn testing...");
    _pPanel->Draw(true);
    if (!iioMangTest()) {
        return false;
    }
    sleep(1);

    snprintf(_content, 128, "pressure testing...");
    _pPanel->Draw(true);
    if (!iioPressureTest()) {
        return false;
    }
    sleep(1);

    snprintf(_content, 128, "temp testing...");
    _pPanel->Draw(true);
    if (!iioTemperatureTest()) {
        return false;
    }
    sleep(1);

    return true;
}

bool IIOTestWnd::iioAccelTest()
{
    struct agsys_iio_accel_event_setting_s calib_diag_iio_accel_event = {
        .sampling_frequency = 100,
        .accel_scale = 0.001,
        .thresh_value = 0.200,
        .thresh_duration = 0.01,
        .accel_x_thresh_falling_en = 0,
        .accel_x_thresh_rising_en = 1,
        .accel_y_thresh_falling_en = 0,
        .accel_y_thresh_rising_en = 1,
        .accel_z_thresh_falling_en = 0,
        .accel_z_thresh_rising_en = 1,
    };

    int ret_val = -1;
    struct agsys_iio_accel_info_s *piio_info;
    struct agsys_iio_accel_xyz_s iio_accel_xyz;
    float accel_x_f;
    float accel_y_f;
    float accel_z_f;
    struct iio_event_data iio_ed;

    char name[256] = "";
    agcmd_property_get("prebuild.board.iio_accel", name, "unknown");

    piio_info = agsys_iio_accel_open_by_name(name);
    if (piio_info == NULL) {
        snprintf(_content, 128, "%s accel Fail:\nagsys_iio_accel_open_by_name()", name);
        _pPanel->Draw(true);
        return false;
    }
    usleep(500 * 1000);
    CAMERALOG("%s sampling_frequency = %d Hz",
        piio_info->name, piio_info->sampling_frequency);
    CAMERALOG("%s accel_scale = %f mg",
        piio_info->name, piio_info->accel_scale);

    ret_val = agsys_iio_accel_read_xyz(piio_info, &iio_accel_xyz);
    if (ret_val < 0) {
        CAMERALOG("%s Fail: accel read %d", piio_info->name, ret_val);
        snprintf(_content, 128, "%s accel Fail:\naccel read, ret = %d",
            piio_info->name, ret_val);
        _pPanel->Draw(true);
        agsys_iio_accel_close(piio_info);
        return false;
    }
    accel_x_f = (iio_accel_xyz.accel_x * piio_info->accel_scale);
    accel_y_f = (iio_accel_xyz.accel_y * piio_info->accel_scale);
    accel_z_f = (iio_accel_xyz.accel_z * piio_info->accel_scale);
    CAMERALOG("%s: X[%8.6f G], Y[%8.6f G], Z[%8.6f G]\n",
        piio_info->name, accel_x_f, accel_y_f, accel_z_f);
    snprintf(_content, 128, "%s accel\nX[%8.6f G], Y[%8.6f G], Z[%8.6f G]",
        piio_info->name, accel_x_f, accel_y_f, accel_z_f);
    _pPanel->Draw(true);
    sleep(1);

    ret_val = agsys_iio_accel_set_event(piio_info,
        &calib_diag_iio_accel_event);
    if (ret_val) {
        CAMERALOG("%s Fail: accel event_set(1) = %d", piio_info->name, ret_val);
        snprintf(_content, 128, "%s accel Fail:\nevent_set(1) = %d",
            piio_info->name, ret_val);
        _pPanel->Draw(true);
        agsys_iio_accel_close(piio_info);
        return false;
    }

    snprintf(_content, 128, "%s accel\nwait event...\n\nShaking to coninue",
        piio_info->name);
    _pPanel->Draw(true);
    ret_val = agsys_iio_accel_wait_event(piio_info, &iio_ed);
    if (ret_val < 0) {
        CAMERALOG("%s Fail: iio_accel event_wait = %d", piio_info->name, ret_val);
        sprintf(_content, "%s accel Fail:\nevent_wait = %d",
            piio_info->name, ret_val);
        _pPanel->Draw(true);
        agsys_iio_accel_close(piio_info);
        return false;
    }

    calib_diag_iio_accel_event.accel_x_thresh_rising_en = 0;
    calib_diag_iio_accel_event.accel_y_thresh_rising_en = 0;
    calib_diag_iio_accel_event.accel_z_thresh_rising_en = 0;
    ret_val = agsys_iio_accel_set_event(piio_info,
        &calib_diag_iio_accel_event);
    if (ret_val) {
        CAMERALOG("%s Fail: accel event_set(0) = %d", piio_info->name, ret_val);
        snprintf(_content, 128, "%s accel Fail:\n event_set(0) = %d",
            piio_info->name, ret_val);
        _pPanel->Draw(true);
        agsys_iio_accel_close(piio_info);
        return false;
    }

    snprintf(_content, 128, "%s accel\nSuccess", piio_info->name);
    _pPanel->Draw(true);

    agsys_iio_accel_close(piio_info);
    return true;
}

bool IIOTestWnd::iioOrientationTest()
{
    int ret_val = 0;
    struct agsys_iio_orientation_info_s *piio_info = NULL;
    struct agsys_iio_orientation_s iio_orientation;
    float euler_heading_f;
    float euler_roll_f;
    float euler_pitch_f;

    char name[256] = "";
    agcmd_property_get("prebuild.board.iio_orientation", name, "unknown");

    piio_info = agsys_iio_orientation_open_by_name(name);
    if (piio_info == NULL) {
        snprintf(_content, 128, "%s orientation Fail:\n"
            "agsys_iio_orientation_open_by_name()",
            name);
        _pPanel->Draw(true);
        return false;
    }
    usleep(500 * 1000);

    ret_val = agsys_iio_orientation_read(piio_info, &iio_orientation);
    if (ret_val < 0) {
        CAMERALOG("%s Fail: IIO orientation read %d", piio_info->name, ret_val);
        snprintf(_content, 128, "%s orientation Fail:\norientation read %d",
            piio_info->name, ret_val);
        _pPanel->Draw(true);
        agsys_iio_orientation_close(piio_info);
        return false;
    }
    euler_heading_f = (iio_orientation.euler_heading *
        piio_info->orientation_scale);
    euler_roll_f = (iio_orientation.euler_roll *
        piio_info->orientation_scale);
    euler_pitch_f = (iio_orientation.euler_pitch *
        piio_info->orientation_scale);
    snprintf(_content, 128, "%s orientation:\nEuler[%8.6f %8.6f %8.6f Degrees], "
        "Quaternion[%d %d %d %d]",
        piio_info->name,
        euler_heading_f, euler_roll_f, euler_pitch_f,
        iio_orientation.quaternion_w,
        iio_orientation.quaternion_x,
        iio_orientation.quaternion_y,
        iio_orientation.quaternion_z);
    _pPanel->Draw(true);

    agsys_iio_orientation_close(piio_info);
    return true;
}

bool IIOTestWnd::iioGyroTest()
{
    int ret_val = 0;
    struct agsys_iio_gyro_info_s *piio_info = NULL;
    struct agsys_iio_gyro_xyz_s iio_gyro_xyz;
    float gyro_x_f;
    float gyro_y_f;
    float gyro_z_f;

    char name[256] = "";
    agcmd_property_get("prebuild.board.iio_gyro", name, "unknown");

    piio_info = agsys_iio_gyro_open_by_name(name);
    if (piio_info == NULL) {
        snprintf(_content, 128, "%s gyro Fail:\nagsys_iio_gyro_open_by_name()", name);
        _pPanel->Draw(true);
        return false;
    }
    usleep(500 * 1000);

    ret_val = agsys_iio_gyro_read_xyz(piio_info, &iio_gyro_xyz);
    if (ret_val < 0) {
        CAMERALOG("Fail: IIO gyro read %d", ret_val);
        snprintf(_content, 128, "%s gyro Fail:\ngyro read %d",
            piio_info->name, ret_val);
        _pPanel->Draw(true);
        agsys_iio_gyro_close(piio_info);
        return false;
    }
    gyro_x_f = (iio_gyro_xyz.gyro_x * piio_info->gyro_scale);
    gyro_y_f = (iio_gyro_xyz.gyro_y * piio_info->gyro_scale);
    gyro_z_f = (iio_gyro_xyz.gyro_z * piio_info->gyro_scale);
    snprintf(_content, 128, "%s gyro:\nX[%8.6f Dps], Y[%8.6f Dps], Z[%8.6f Dps]",
        piio_info->name, gyro_x_f, gyro_y_f, gyro_z_f);
    _pPanel->Draw(true);

    agsys_iio_gyro_close(piio_info);
    return true;
}

bool IIOTestWnd::iioMangTest()
{
    int ret_val = 0;
    struct agsys_iio_magn_info_s *piio_info = NULL;
    struct agsys_iio_magn_xyz_s iio_magn_xyz;
    float magn_x_f;
    float magn_y_f;
    float magn_z_f;

    char name[256] = "";
    agcmd_property_get("prebuild.board.iio_magn", name, "unknown");

    piio_info = agsys_iio_magn_open_by_name(name);
    if (piio_info == NULL) {
        snprintf(_content, 128, "%s magn Fail:\nagsys_iio_magn_open_by_name()", name);
        _pPanel->Draw(true);
        return false;
    }
    usleep(500 * 1000);

    ret_val = agsys_iio_magn_read_xyz(piio_info, &iio_magn_xyz);
    if (ret_val < 0) {
        CAMERALOG("Fail: IIO magn read %d", ret_val);
        snprintf(_content, 128, "%s magn Fail:\nIIO magn read %d",
            piio_info->name, ret_val);
        _pPanel->Draw(true);
        agsys_iio_magn_close(piio_info);
        return false;
    }
    magn_x_f = (iio_magn_xyz.magn_x * piio_info->magn_scale);
    magn_y_f = (iio_magn_xyz.magn_y * piio_info->magn_scale);
    magn_z_f = (iio_magn_xyz.magn_z * piio_info->magn_scale);
    snprintf(_content, 128, "%s magn:\nX[%8.6f uT], Y[%8.6f uT], Z[%8.6f uT]",
        piio_info->name, magn_x_f, magn_y_f, magn_z_f);
    _pPanel->Draw(true);

    agsys_iio_magn_close(piio_info);
    return true;
}

bool IIOTestWnd::iioPressureTest()
{
    int ret_val = 0;
    struct agsys_iio_pressure_info_s *piio_info = NULL;
    struct agsys_iio_pressure_s iio_pressure;
    float pressure_f;

    char name[256] = "";
    agcmd_property_get("prebuild.board.iio_pressure", name, "unknown");

    piio_info = agsys_iio_pressure_open_by_name(name);
    if (piio_info == NULL) {
        snprintf(_content, 128, "%s pressure Fail:\n"
            "agsys_iio_pressure_open_by_name()",
            name);
        _pPanel->Draw(true);
        return false;
    }
    usleep(500 * 1000);

    ret_val = agsys_iio_pressure_read(piio_info, &iio_pressure);
    if (ret_val < 0) {
        CAMERALOG("Fail: IIO pressure read %d", ret_val);
        snprintf(_content, 128, "%s pressure Fail:\nIIO pressure read %d",
            piio_info->name, ret_val);
        _pPanel->Draw(true);
        agsys_iio_pressure_close(piio_info);
        return false;
    }
    pressure_f = (iio_pressure.pressure * piio_info->pressure_scale);
    snprintf(_content, 128, "%s pressure:\nPressure[%8.6f KPa]",
        piio_info->name, pressure_f / 1000);
    _pPanel->Draw(true);

    agsys_iio_pressure_close(piio_info);
    return true;
}

bool IIOTestWnd::iioTemperatureTest()
{
    int ret_val = 0;
    struct agsys_iio_temp_info_s *piio_info = NULL;
    struct agsys_iio_temp_s iio_temp;

    char name[256] = "";
    agcmd_property_get("prebuild.board.iio_temp", name, "unknown");

    piio_info = agsys_iio_temp_open_by_name(name);
    if (piio_info == NULL) {
        snprintf(_content, 128, "%s temperature Fail:\n"
            "agsys_iio_temp_open_by_name()",
            name);
        _pPanel->Draw(true);
        return false;
    }
    usleep(500 * 1000);

    ret_val = agsys_iio_temp_read(piio_info, &iio_temp);
    if (ret_val < 0) {
        CAMERALOG("Fail: temp read %d", ret_val);
        snprintf(_content, 128, "%s temp Fail:\nIIO temperature read %d",
            piio_info->name, ret_val);
        _pPanel->Draw(true);
        agsys_iio_temp_close(piio_info);
        return false;
    }
    snprintf(_content, 128, "%s temp:\ntemperature[%d]",
        piio_info->name, iio_temp.temperature);
    _pPanel->Draw(true);

    agsys_iio_temp_close(piio_info);
    return true;
}


void IIOTestWnd::onClick(Control *widget)
{
    if (widget == _pNext) {
        if (STATE_SUCCESS == _state) {
            this->StartWnd(WND_SDCARD, Anim_Right2LeftEnter);
        }
    } else {
        SimpleTestWnd::onClick(widget);
    }
}

bool IIOTestWnd::OnEvent(CEvent *event)
{
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter == 2) {
                    _state = STATE_TESTING;
                    _pPowerOff->SetHiden();
                    _pNext->SetHiden();
                    memset(_content, 0x00, 128);
                    snprintf(_content, 128, "IIO Testing...");
                    _pPanel->Draw(true);

                    if (doTest()) {
                        _state = STATE_SUCCESS;
                        _pPowerOff->SetShow();
                        _pNext->SetShow();
                        _pNext->SetText((UINT8 *)"Next", 32, 0xFFFF);
                        memset(_content, 0x00, 128);
                        snprintf(_content, 128, "IIO Success!");
                        _pPanel->Draw(true);
                    } else {
                        _state = STATE_FAILED;
                        _pTxt->SetColor(ERROR_COLOR);
                        _pPowerOff->SetShow();
                        _pPanel->Draw(true);
                    }
                }
            }
            break;
        default:
            break;
    }
    return true;
}


SDCardTestWnd::SDCardTestWnd(const char *name, Size wndSize, Point leftTop)
    : SimpleTestWnd(name, wndSize, leftTop)
    , _counter(0)
{
    strcpy(_title, "SDCard");
    strcpy(_content, "SDCard Test");
    _pPowerOff->SetHiden();
    _pNext->SetHiden();
}

SDCardTestWnd::~SDCardTestWnd()
{
}

bool SDCardTestWnd::doTest()
{
    const char system_config_disk[] = "system.config.disk";
    int ret_val = -1;
    char support_sd_prop[AGCMD_PROPERTY_SIZE];
    char disk_name[AGCMD_PROPERTY_SIZE];
    char partition_name[AGCMD_PROPERTY_SIZE];
    char dev_name[AGCMD_PROPERTY_SIZE];
    char *fsck_vfat_args[8];

    agcmd_property_get("prebuild.board.support_sd", support_sd_prop, "1");
    ret_val = strtoul(support_sd_prop, NULL, 0);
    if (ret_val == 0) {
        strcpy(_content, "SD skip ...");
        _pPanel->Draw(true);
        return true;
    }

    agcmd_property_get(system_config_disk, disk_name, "mmcblk0");
    snprintf(partition_name, sizeof(partition_name), "%sp1", disk_name);
    snprintf(dev_name, sizeof(dev_name), "/dev/%s", partition_name);
    fsck_vfat_args[0] = (char *)"fsck.fat";
    fsck_vfat_args[1] = (char *)"-av";
    fsck_vfat_args[2] = dev_name;
    fsck_vfat_args[3] = NULL;
    do {
        sleep(1);
        ret_val = agcmd_agexecvp(fsck_vfat_args[0], fsck_vfat_args);
    } while (ret_val);

    return true;
}

void SDCardTestWnd::onClick(Control *widget)
{
    if (widget == _pNext) {
        if (STATE_SUCCESS == _state) {
            this->StartWnd(WND_Wifi, Anim_Right2LeftEnter);
        }
    } else {
        SimpleTestWnd::onClick(widget);
    }
}

bool SDCardTestWnd::OnEvent(CEvent *event)
{
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter == 2) {
                    _state = STATE_TESTING;
                    _pPowerOff->SetHiden();
                    _pNext->SetHiden();
                    memset(_content, 0x00, 128);
                    strcpy(_content, "SDCard Checking...");
                    _pPanel->Draw(true);

                    if (doTest()) {
                        _state = STATE_SUCCESS;
                        _pPowerOff->SetShow();
                        _pNext->SetShow();
                        _pNext->SetText((UINT8 *)"Next", 32, 0xFFFF);
                        memset(_content, 0x00, 128);
                        strcpy(_content, "SDCard Success!");
                        _pPanel->Draw(true);
                    } else {
                        _state = STATE_FAILED;
                        _pTxt->SetColor(ERROR_COLOR);
                        _pPowerOff->SetShow();
                        _pPanel->Draw(true);
                    }
                }
            }
            break;
        default:
            break;
    }
    return true;
}


WifiTestWnd::WifiTestWnd(const char *name, Size wndSize, Point leftTop)
    : SimpleTestWnd(name, wndSize, leftTop)
    , _count(0)
{
    strcpy(_title, "Wifi");
    strcpy(_content, "Wifi Test");
    _pPowerOff->SetHiden();
    _pNext->SetHiden();
}

WifiTestWnd::~WifiTestWnd()
{
}

int WifiTestWnd::get_mac_address(char* name, unsigned char *pmac)
{
    int ret_val = -1;
    int ifconfig_socket;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(struct ifreq));
    strlcpy(ifr.ifr_name, name, IFNAMSIZ);

    ifconfig_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (ifconfig_socket < 0) {
        perror("ifconfig_socket");
        return ifconfig_socket;
    }
    ret_val = ioctl(ifconfig_socket, SIOCGIFHWADDR, &ifr);
    if (ret_val < 0) {
        close(ifconfig_socket);
        return ret_val;
    }
    memcpy(pmac, ifr.ifr_hwaddr.sa_data, 8);

    return 0;
}

bool WifiTestWnd::doTest()
{
    const char temp_earlyboot_if_mac[] = "temp.earlyboot.if_mac";
    const char temp_earlyboot_ssid_name[] = "temp.earlyboot.ssid_name";
    const char temp_earlyboot_ssid_key[] = "temp.earlyboot.ssid_key";

    unsigned char pssid_mac[8];
    char pssid_name[AGCMD_PROPERTY_SIZE];
    char pssid_key[AGCMD_PROPERTY_SIZE];
    char mac_interface[AGCMD_PROPERTY_SIZE];

    int ret_val = -1;
    ret_val = agcmd_property_get(temp_earlyboot_if_mac,
        mac_interface, "NA");
    if (ret_val) {
        CAMERALOG("MAC_Name[%s]", mac_interface);
        snprintf(_content, 128, "MAC_Name[%s]", mac_interface);
        _pPanel->Draw(true);
        return false;
    }

    ret_val = agcmd_property_get(temp_earlyboot_ssid_name, pssid_name, "NA");
    if (ret_val) {
        CAMERALOG("SSID_Name[%s]", pssid_name);
        snprintf(_content, 128, "MAC_Name[%s]", mac_interface);
        _pPanel->Draw(true);
        return false;
    }

    ret_val = agcmd_property_get(temp_earlyboot_ssid_key, pssid_key, "NA");
    if (ret_val) {
        CAMERALOG("SSID_Key[%s]", pssid_key);
        snprintf(_content, 128, "SSID_Key[%s]", pssid_key);
        _pPanel->Draw(true);
        return false;
    }

    CAMERALOG("MAC_Name[%s]", mac_interface);
    CAMERALOG("SSID_Name[%s]", pssid_name);
    CAMERALOG("SSID_Key[%s]", pssid_key);

    ret_val = get_mac_address(mac_interface, pssid_mac);
    if (ret_val) {
        CAMERALOG("%s: MAC[FAIL...]", mac_interface);
        snprintf(_content, 128, "%s: MAC[FAIL...]", mac_interface);
        _pPanel->Draw(true);
        return false;
    }

    CAMERALOG("%s: MAC[%02X:%02X:%02X:%02X:%02X:%02X]",
        mac_interface,
        pssid_mac[0], pssid_mac[1], pssid_mac[2],
        pssid_mac[3], pssid_mac[4], pssid_mac[5]);
    snprintf(_content, 128, "%s:\nMAC[%02X:%02X:%02X:%02X:%02X:%02X]",
        mac_interface,
        pssid_mac[0], pssid_mac[1], pssid_mac[2],
        pssid_mac[3], pssid_mac[4], pssid_mac[5]);
    _pPanel->Draw(true);
    sleep(1);

#if 1

    snprintf(_content, 128, "Searching hot spots...");
    _pPanel->Draw(true);

    // scan hot spots
    char wlan_prop[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];
    agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
    agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
    struct agcmd_wpa_network_scan_s networkList;
    do {
        memset(&networkList, 0x00, sizeof(networkList));
        int ret = agcmd_wpa_scan_networks(wlan_prop, &networkList);
        if (ret != 0) {
            CAMERALOG("wifi scan failed with error %d", ret);
            snprintf(_content, 128, "Searching hot spots failed");
            _pPanel->Draw(true);
            return false;
        } else if (networkList.valid_num > 32) {
            CAMERALOG("wifi scan incorrect, valid_num = %d", networkList.valid_num);
            snprintf(_content, 128, "Searching hot spots incorrect");
            _pPanel->Draw(true);
            return false;
        }
        CAMERALOG("networkList.valid_num = %d", networkList.valid_num);
    } while (networkList.valid_num <= 0);

    int num = 0;
    for (int i = 0; (i < networkList.valid_num) && (i < 4); i++) {
        num += snprintf(_content + num, 128 - num, "%s:%d\n",
            networkList.info[i].ssid, networkList.info[i].signal_level);
    }
    _pPanel->Draw(true);

#endif

    return true;
}


void WifiTestWnd::onClick(Control *widget)
{
    if (widget == _pNext) {
        if (STATE_SUCCESS == _state) {
            this->StartWnd(WND_Bluetooth, Anim_Right2LeftEnter);
        }
    } else {
        SimpleTestWnd::onClick(widget);
    }
}

bool WifiTestWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _count++;
                if (_count == 2) {
                    _state = STATE_TESTING;
                    _pPowerOff->SetHiden();
                    _pNext->SetHiden();
                    memset(_content, 0x00, 128);
                    snprintf(_content, 128, "Wifi Testing...");
                    _pPanel->Draw(true);

                    if (doTest()) {
                        _state = STATE_SUCCESS;
                        _pPowerOff->SetShow();
                        _pNext->SetShow();
                        _pNext->SetText((UINT8 *)"Next", 32, 0xFFFF);
                        _pPanel->Draw(true);
                    } else {
                        _state = STATE_FAILED;
                        _pTxt->SetColor(ERROR_COLOR);
                        _pPowerOff->SetShow();
                        _pPanel->Draw(true);
                    }
                }
                if (STATE_SUCCESS == _state) {
                    if (_count % 10 == 0) {
                        // scan hot spots
                        char wlan_prop[AGCMD_PROPERTY_SIZE];
                        char prebuild_prop[AGCMD_PROPERTY_SIZE];
                        agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");
                        agcmd_property_get("temp.earlyboot.if_wlan", wlan_prop, prebuild_prop);
                        struct agcmd_wpa_network_scan_s networkList;
                        memset(&networkList, 0x00, sizeof(networkList));
                        do {
                            memset(&networkList, 0x00, sizeof(networkList));
                            int ret = agcmd_wpa_scan_networks(wlan_prop, &networkList);
                            if (ret != 0) {
                                CAMERALOG("wifi scan failed with error %d", ret);
                                break;
                            } else if (networkList.valid_num > 32) {
                                CAMERALOG("wifi scan incorrect, valid_num = %d", networkList.valid_num);
                                break;
                            }
                            CAMERALOG("networkList.valid_num = %d", networkList.valid_num);
                        } while (networkList.valid_num <= 0);

                        int num = 0;
                        for (int i = 0; (i < networkList.valid_num) && (i < 4); i++) {
                            num += snprintf(_content + num, 128 - num, "%s:%d\n",
                                networkList.info[i].ssid, networkList.info[i].signal_level);
                        }
                        _pPanel->Draw(true);
                    }
                }
            }
        }
    return b;
}


BluetoothTestWnd::BluetoothTestWnd(const char *name, Size wndSize, Point leftTop)
    : SimpleTestWnd(name, wndSize, leftTop)
    , _count(0)
    , _numMenus(0)
{
    strcpy(_title, "Bluetooth");
    strcpy(_content, "Bluetooth Test");
    _pPowerOff->SetHiden();
    _pNext->SetHiden();

    _pList = new List(_pPanel, this, Point(20, 80), Size(360, 150), 20);
    _pList->setListener(this);

    _pAdapter = new TextListAdapter();
    _pAdapter->setTextList((UINT8 **)_pMenus, _numMenus);
    _pAdapter->setItemStyle(Size(360, 70), 0x0142, 0x8410,
        0xFFFF, 24, LEFT, MIDDLE);
    _pAdapter->setObserver(_pList);

    _pList->setAdaptor(_pAdapter);
    _pList->SetHiden();

    _pReScan = new Button(_pPanel, Point(118, 145), Size(164, 80), 0x50C4, 0x8410);
    _pReScan->SetText((UINT8 *)"ReScan", 32, 0xFFFF);
    _pReScan->setOnClickListener(this);
    _pReScan->SetHiden();
}

BluetoothTestWnd::~BluetoothTestWnd()
{
}

bool BluetoothTestWnd::doTest()
{
    snprintf(_content, 128, "BT module\nchecking ...");
    _pPanel->Draw(true);
    sleep(1);

    if (!checkDeviceOn()) {
        return false;
    }

    sleep(1);

    snprintf(_content, 128, "Scanning device\nnearby ...");
    _pPanel->Draw(true);

    return scanDevice();
}

bool BluetoothTestWnd::checkDeviceOn()
{
    int ret_val = 0;
    char hci_name[AGCMD_PROPERTY_SIZE];
    int hci_id;
    int hci_socket;
    struct hci_dev_info hci_detail;

    agcmd_property_get("prebuild.board.bt_hci", hci_name, "hci0");

    hci_id = strtoul(&hci_name[3], NULL, 10);
    hci_socket = agbt_hci_dev_open(hci_id);
    if (hci_socket < 0) {
        CAMERALOG("hci_open_dev(%d) = %d\n", hci_id, hci_socket);
        snprintf(_content, 128, "open dev\nfailed");
        ret_val = -1;
        goto agbt_test_hci_exit;
    }

    // check whether device is up
    ret_val = agbt_hci_get_devinfo(hci_id, &hci_detail);
    if (ret_val < 0) {
        CAMERALOG("hci_get_devinfo(%d) = %d\n", hci_id, ret_val);
        snprintf(_content, 128, "get devinfo\nfailed");
        goto agbt_test_hci_dev_close;
    }
    if ((hci_detail.flags & (1 << HCI_UP)) != (1 << HCI_UP)) {
        CAMERALOG("hci not up\n");
        snprintf(_content, 128, "dev is not up!");
        ret_val = -1;
        goto agbt_test_hci_dev_close;
    }

    //update display
    {
        char addr[18];
        agbt_ba2str(&hci_detail.bdaddr, addr);
        int len = snprintf(_content, 128, "%s: type 0x%02X",
            hci_detail.name, hci_detail.type);
        len += snprintf(_content + len, 128 - len, "\nBD Address:\n%s", addr);
    }

agbt_test_hci_dev_close:
    agbt_hci_dev_close(hci_socket);

agbt_test_hci_exit:
    if (ret_val < 0) {
        _pTxt->SetColor(ERROR_COLOR);
    }
    _pPanel->Draw(true);

    return ret_val >= 0;
}

bool BluetoothTestWnd::scanDevice()
{
    do {
        int ret = agbt_scan_devices(&_bt_scan_list, 10);
        if (ret != 0) {
            snprintf(_content, 128, "agbt_scan_devices failed: %d", ret);
            _pPanel->Draw(true);
            return false;
        }
        CAMERALOG("BlueTooth Scan, Find: %d devices\n", _bt_scan_list.total_num);
    } while (_bt_scan_list.total_num <= 0);

    _numMenus = 0;
    for (int i = 0; (i < _bt_scan_list.total_num) && (_numMenus < 4); i++) {
        if (strstr(_bt_scan_list.result[i].name, "RC-TW")) {
            _pMenus[_numMenus] = new char[64];
            snprintf(_pMenus[_numMenus], 64, "%s:[%02X-%02X-%02X-%02X-%02X-%02X]",
                _bt_scan_list.result[i].name,
                _bt_scan_list.result[i].mac[0],
                _bt_scan_list.result[i].mac[1],
                _bt_scan_list.result[i].mac[2],
                _bt_scan_list.result[i].mac[3],
                _bt_scan_list.result[i].mac[4],
                _bt_scan_list.result[i].mac[5]);
            _numMenus++;
        }
    }

    if (_numMenus > 0) {
        _pList->SetShow();
        _pAdapter->setTextList((UINT8 **)_pMenus, _numMenus);
        _pAdapter->notifyDataSetChanged();
    } else {
        _pReScan->SetShow();
        snprintf(_content, 128, "Success, 0 RC-TW/%d", _bt_scan_list.total_num);
    }
    _pPanel->Draw(true);

    return true;
}

void BluetoothTestWnd::onClick(Control *widget)
{
    if (widget == _pReScan) {
        _pReScan->SetHiden();
        strcpy(_content, "Scanning...");
        _pPanel->Draw(true);

        if (!scanDevice()) {
            _pReScan->SetShow();
            strcpy(_content, "Success, 0 RC-TW");
            _pPanel->Draw(true);
        }
    } else if (widget == _pNext) {
        if (STATE_SUCCESS == _state) {
            this->StartWnd(WND_TOUCH, Anim_Right2LeftEnter);
        }
    } else if (widget == _pPowerOff) {
        int ret = 0;
        if (STATE_FAILED == _state) {
            ret = 2;
        }
        _pPowerOff->SetHiden();
        _pNext->SetHiden();
        _pList->SetHiden();
        snprintf(_content, 128, "exit(%d)", ret);
        _pPanel->Draw(true);

        exit(ret);
    }
}

void BluetoothTestWnd::onListClicked(Control *list, int index)
{
    if (index < 0) {
        index = 0;
    } else if (index >= _numMenus) {
        index = _numMenus - 1;
    }

    for (int i = 0; i < _bt_scan_list.total_num; i++) {
        if ((strlen(_bt_scan_list.result[i].name) > 0) &&
            strstr(_pMenus[index], _bt_scan_list.result[i].name))
        {
            _pList->SetHiden();
            agcmd_property_set("system.bt.hid_mac", _bt_scan_list.result[i].mac);
            snprintf(_content, 128, "bind device\n%s", _bt_scan_list.result[i].name);
            _pPanel->Draw(true);
            break;
        }
    }
}

bool BluetoothTestWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _count++;
                if (_count == 2) {
                    _state = STATE_TESTING;
                    _pPowerOff->SetHiden();
                    _pNext->SetHiden();
                    memset(_content, 0x00, 128);
                    strcpy(_content, "Bluetooth Checking...");
                    _pPanel->Draw(true);

                    if (doTest()) {
                        _state = STATE_SUCCESS;
                        _pPowerOff->SetShow();
                        _pNext->SetShow();
                        _pNext->SetText((UINT8 *)"Next", 32, 0xFFFF);
                        _pPanel->Draw(true);
                    } else {
                        _state = STATE_FAILED;
                        _pTxt->SetColor(ERROR_COLOR);
                        _pPowerOff->SetShow();
                        _pPanel->Draw(true);
                    }
                }
            }
        }
    return b;
}


TouchTestWnd::TouchTestWnd(const char *name, Size wndSize, Point leftTop)
    : SimpleTestWnd(name, wndSize, leftTop)
    , _step(CALIB_STEP_INVALID)
    , _btnFlags(0x00)
    , _doubleFlags(0x00)
    , _hFlickFlags(0x00)
    , _counter(0)
{
    memset(_pButtons, 0x00, sizeof(_pButtons));
    memset(_pBtnDblClick, 0x00, sizeof(_pBtnDblClick));
    memset(_pHFlick, 0x00, sizeof(_pHFlick));

    _pPanel->setBgColor(Color_Black);
    strcpy(_title, "Touch");
    strcpy(_content, "Touch Test");
    _pPowerOff->SetHiden();
    _pNext->SetHiden();

    _pDrawingBoard = new DrawingBoard(_pPanel, Point(0, 0), Size(400, 400), Color_Green_1);
}

TouchTestWnd::~TouchTestWnd()
{
}

void TouchTestWnd::prepareSingleClickTest()
{
    _step = CALIB_STEP_SingleClick_Test;
    _btnFlags = 0x00;
    for (int i = 0; i < 16; i++) {
        _pButtons[i] = new Button(_pPanel,
            Point((i % 4) * 100 + 20, (i / 4) * 100 + 20), Size(60, 60),
            Color_Black, Color_Grey_1);
        _pButtons[i]->SetText((UINT8 *)"X", 40, Color_Red_1);
        _pButtons[i]->SetTextAlign(CENTER, MIDDLE);
        _pButtons[i]->setOnClickListener(this);
    }
    _pButtons[0]->SetHiden();
    _btnFlags |= 0x1;
    _pButtons[3]->SetHiden();
    _btnFlags |= 0x1 << 3;
    _pButtons[12]->SetHiden();
    _btnFlags |= 0x1 << 12;
    _pButtons[15]->SetHiden();
    _btnFlags |= 0x1 << 15;
}

void TouchTestWnd::prepareDoubleClickTest()
{
    _step = CALIB_STEP_DoubleClick_Test;
    _doubleFlags = 0x00;
    for (int i = 0; i < 4; i++) {
        _pBtnDblClick[i] = new StaticText(_pPanel,
            Point((i % 2) * 200 + 50, (i / 2) * 200 + 50), Size(100, 100));
        _pBtnDblClick[i]->SetStyle(40, Color_Red_1, CENTER, MIDDLE);
        _pBtnDblClick[i]->SetText((UINT8 *)"XX");
    }
}

void TouchTestWnd::prepareHorizontalFlick()
{
    _step = CALIB_STEP_FLICK_LEFT;
    _hFlickFlags = 0x00;
    for (int i = 0; i < 4; i++) {
        _pHFlick[i] = new StaticText(_pPanel,
            Point(0, i * 100), Size(400, 100));
        _pHFlick[i]->SetStyle(40, Color_Green_1, CENTER, MIDDLE);
        _pHFlick[i]->SetText((UINT8 *)"<- Left");
        _pHFlick[i]->SetColor(Color_Green_1, Color_Grey_1);
        _pHFlick[i]->SetHiden();
    }
    _pHFlick[0]->SetShow();
    _hFlickFlags = 0x00;
}

void TouchTestWnd::prepareVerticalFlick()
{
    _step = CALIB_STEP_FLICK_UP;
    _hFlickFlags = 0x00;
    for (int i = 0; i < 4; i++) {
        _pHFlick[i] = new StaticText(_pPanel,
            Point(i * 100, 0), Size(100, 400));
        _pHFlick[i]->SetStyle(40, Color_Green_1, CENTER, MIDDLE);
        _pHFlick[i]->SetText((UINT8 *)"^\n |\nUP");
        _pHFlick[i]->SetColor(Color_Green_1, Color_Grey_1);
        _pHFlick[i]->SetHiden();
    }
    _pHFlick[0]->SetShow();
    _hFlickFlags = 0x00;
}

void TouchTestWnd::onClick(Control *widget)
{
    if (CALIB_STEP_SingleClick_Test == _step) {
        for (int i = 0; i < 16; i++) {
            if (widget == _pButtons[i]) {
                _pButtons[i]->SetHiden();
                _btnFlags |= 0x1 << i;
                _pPanel->Draw(true);
                break;
            }
        }

        CAMERALOG("####### _btnFlags = 0x%x", _btnFlags);
        if (0xFFFF == _btnFlags) {
            CAMERALOG("####### All buttons clicked");
            _step = CALIB_STEP_DoubleClick_Test;
            for (int i = 0; i < 16; i++) {
                delete _pButtons[i];
                _pButtons[i] = NULL;
            }
            prepareDoubleClickTest();
            _pPanel->Draw(true);
        }
    }

    if (widget == _pNext) {
        if (STATE_SUCCESS == _state) {
            this->StartWnd(WND_GPS, Anim_Right2LeftEnter);
        }
    } else {
        SimpleTestWnd::onClick(widget);
    }
}

bool TouchTestWnd::OnEvent(CEvent *event)
{
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter == 2) {
                    _pTitle->SetHiden();
                    _pTxt->SetHiden();
                    _pVersion->SetHiden();
                    _pBoard->SetHiden();

                    _state = STATE_TESTING;
                    _step = CALIB_STEP_INVALID;
                    _pPowerOff->SetHiden();
                    _pNext->SetHiden();
                    memset(_content, 0x00, 128);

                    prepareSingleClickTest();
                    _pPanel->Draw(true);
                }
            }
            break;
        case eventType_touch:
            if ((event->_event == TouchEvent_SingleClick) &&
                (_step == CALIB_STEP_SingleClick_Test))
            {
                int x = event->_paload;
                int y = event->_paload1;
                CAMERALOG("####### single click(%d, %d)", x, y);
                for (int i = 0; i < 5; i++) {
                    for (int j = 0; j < 5; j++) {
                        if ((x + i - 2 >= 0) && (x + i - 2 < 400) &&
                            (y + j - 2 >= 0) && (y + j - 2 < 400))
                        {
                            _pDrawingBoard->drawPoint(Point(x + i - 2, y + j - 2));
                        }
                    }
                }
            } else if ((event->_event == TouchEvent_DoubleClick) &&
                (_step == CALIB_STEP_DoubleClick_Test))
            {
                int x = event->_paload;
                int y = event->_paload1;
                CAMERALOG("####### double click(%d, %d)", x, y);
                if (y < 200) {
                    if (x < 200) {
                        _pBtnDblClick[0]->SetHiden();
                        _doubleFlags |= 0x01;
                    } else if (x < 400) {
                        _pBtnDblClick[1]->SetHiden();
                        _doubleFlags |= 0x01 << 1;
                    }
                } else if (y < 400) {
                    if (x < 200) {
                        _pBtnDblClick[2]->SetHiden();
                        _doubleFlags |= 0x01 << 2;
                    } else if (x < 400) {
                        _pBtnDblClick[3]->SetHiden();
                        _doubleFlags |= 0x01 << 3;
                    }
                }
                if (_doubleFlags == 0x0F) {
                    CAMERALOG("####### All buttons double clicked");
                    for (int i = 0; i < 4; i++) {
                        delete _pBtnDblClick[i];
                        _pBtnDblClick[i] = NULL;
                    }
                    prepareHorizontalFlick();
                }
                _pPanel->Draw(true);
            } else if (event->_event == TouchEvent_flick) {
                if ((_step == CALIB_STEP_FLICK_LEFT) &&
                    (event->_paload == TouchFlick_Left))
                {
                    UINT32 position = event->_paload1;
                    int y = position & 0xFFFF;
                    int x = (position >> 16) & 0xFFFF;
                    CAMERALOG("####### flick left(%d, %d), _hFlickFlags = 0x%x", x, y, _hFlickFlags);
                    if (0x00 == _hFlickFlags) {
                        if (y < 100) {
                            _hFlickFlags |= 0x1;
                            _pHFlick[0]->SetHiden();
                            _pHFlick[1]->SetShow();
                        }
                    } else if (0x01 == _hFlickFlags) {
                        if ((y > 100) && (y < 200)) {
                            _hFlickFlags |= 0x1 << 1;
                            _pHFlick[1]->SetHiden();
                            _pHFlick[2]->SetShow();
                        }
                    } else if (0x03 == _hFlickFlags) {
                        if ((y > 200) && (y < 300)) {
                            _hFlickFlags |= 0x1 << 2;
                            _pHFlick[2]->SetHiden();
                            _pHFlick[3]->SetShow();
                        }
                    } else if (0x07 == _hFlickFlags) {
                        if (y > 300) {
                            _hFlickFlags |= 0x1 << 3;
                        }
                    }
                    if (_hFlickFlags == 0x0F) {
                        CAMERALOG("####### All left flick tested");
                        for (int i = 0; i < 4; i++) {
                            _pHFlick[i]->SetText((UINT8 *)"Right ->");
                            _pHFlick[i]->SetHiden();
                        }
                        _pHFlick[0]->SetShow();
                        _hFlickFlags = 0x00;
                        _step = CALIB_STEP_FLICK_RIGHT;
                    }
                    _pPanel->Draw(true);
                } else if ((_step == CALIB_STEP_FLICK_RIGHT) &&
                    (event->_paload == TouchFlick_Right))
                {
                    UINT32 position = event->_paload1;
                    int y = position & 0xFFFF;
                    int x = (position >> 16) & 0xFFFF;
                    CAMERALOG("####### flick right(%d, %d), _hFlickFlags = 0x%x", x, y, _hFlickFlags);
                    if (0x00 == _hFlickFlags) {
                        if (y < 100) {
                            _hFlickFlags |= 0x1;
                            _pHFlick[0]->SetHiden();
                            _pHFlick[1]->SetShow();
                        }
                    } else if (0x01 == _hFlickFlags) {
                        if ((y > 100) && (y < 200)) {
                            _hFlickFlags |= 0x1 << 1;
                            _pHFlick[1]->SetHiden();
                            _pHFlick[2]->SetShow();
                        }
                    } else if (0x03 == _hFlickFlags) {
                        if ((y > 200) && (y < 300)) {
                            _hFlickFlags |= 0x1 << 2;
                            _pHFlick[2]->SetHiden();
                            _pHFlick[3]->SetShow();
                        }
                    } else if (0x07 == _hFlickFlags) {
                        if (y > 300) {
                            _hFlickFlags |= 0x1 << 3;
                        }
                    }
                    if (_hFlickFlags == 0x0F) {
                        CAMERALOG("####### All right flick tested");
                        for (int i = 0; i < 4; i++) {
                            delete _pHFlick[i];
                            _pHFlick[i] = NULL;
                        }
                        _hFlickFlags = 0x00;
                        _step = CALIB_STEP_FLICK_UP;
                        prepareVerticalFlick();
                    }
                    _pPanel->Draw(true);
                } else if ((_step == CALIB_STEP_FLICK_UP) &&
                    (event->_paload == TouchFlick_UP))
                {
                    UINT32 position = event->_paload1;
                    int y = position & 0xFFFF;
                    int x = (position >> 16) & 0xFFFF;
                    CAMERALOG("####### flick up(%d, %d), _hFlickFlags = 0x%x", x, y, _hFlickFlags);
                    if (0x00 == _hFlickFlags) {
                        if (x < 100) {
                            _hFlickFlags |= 0x1;
                            _pHFlick[0]->SetHiden();
                            _pHFlick[1]->SetShow();
                        }
                    } else if (0x01 == _hFlickFlags) {
                        if ((x > 100) && (x < 200)) {
                            _hFlickFlags |= 0x1 << 1;
                            _pHFlick[1]->SetHiden();
                            _pHFlick[2]->SetShow();
                        }
                    } else if (0x03 == _hFlickFlags) {
                        if ((x > 200) && (x < 300)) {
                            _hFlickFlags |= 0x1 << 2;
                            _pHFlick[2]->SetHiden();
                            _pHFlick[3]->SetShow();
                        }
                    } else if (0x07 == _hFlickFlags) {
                        if ((x > 300) && (x < 400)) {
                            _hFlickFlags |= 0x1 << 3;
                        }
                    }
                    if (_hFlickFlags == 0x0F) {
                        CAMERALOG("####### All up flick tested");
                        for (int i = 0; i < 4; i++) {
                            _pHFlick[i]->SetText((UINT8 *)"Down\n |\nv");
                            _pHFlick[i]->SetHiden();
                        }
                        _hFlickFlags = 0x00;
                        _pHFlick[0]->SetShow();
                        _step = CALIB_STEP_FLICK_DOWN;
                    }
                    _pPanel->Draw(true);
                } else if ((_step == CALIB_STEP_FLICK_DOWN) &&
                    (event->_paload == TouchFlick_Down))
                {
                    UINT32 position = event->_paload1;
                    int y = position & 0xFFFF;
                    int x = (position >> 16) & 0xFFFF;
                    CAMERALOG("####### flick down(%d, %d), _hFlickFlags = 0x%x", x, y, _hFlickFlags);
                    if (0x00 == _hFlickFlags) {
                        if (x < 100) {
                            _hFlickFlags |= 0x1;
                            _pHFlick[0]->SetHiden();
                            _pHFlick[1]->SetShow();
                        }
                    } else if (0x01 == _hFlickFlags) {
                        if ((x > 100) && (x < 200)) {
                            _hFlickFlags |= 0x1 << 1;
                            _pHFlick[1]->SetHiden();
                            _pHFlick[2]->SetShow();
                        }
                    } else if (0x03 == _hFlickFlags) {
                        if ((x > 200) && (x < 300)) {
                            _hFlickFlags |= 0x1 << 2;
                            _pHFlick[2]->SetHiden();
                            _pHFlick[3]->SetShow();
                        }
                    } else if (0x07 == _hFlickFlags) {
                        if ((x > 300) && (x < 400)) {
                            _hFlickFlags |= 0x1 << 3;
                        }
                    }
                    if (_hFlickFlags == 0x0F) {
                        CAMERALOG("####### All down flick tested");
                        for (int i = 0; i < 4; i++) {
                            _pHFlick[i]->SetHiden();
                        }

                        _state = STATE_SUCCESS;
                        _pTitle->SetShow();
                        _pTxt->SetShow();
                        _pPowerOff->SetShow();
                        _pNext->SetShow();
                        _pNext->SetText((UINT8 *)"Next", 32, 0xFFFF);
                        memset(_content, 0x00, 128);
                        strcpy(_content, "Touch Success!");
                    }
                    _pPanel->Draw(true);
                }
            }
            break;
        default:
            break;
    }
    return true;
}


GPSTestWnd::GPSTestWnd(const char *name, Size wndSize, Point leftTop)
    : Window(name, wndSize, leftTop)
    , _numSat(0)
    , _pSatSNR(NULL)
    , _pSNR(NULL)
    , _pSatPRN(NULL)
    , _pTestResult(NULL)
    , _paggps_client(NULL)
    , _cnt(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 50);
    this->SetMainObject(_pPanel);

    memset(_txtNum, 0x00, 32);
    snprintf(_txtNum, 32, "GPS TEST");
    _pTitle = new StaticText(_pPanel, Point(0, 20), Size(400, 80));
    _pTitle->SetStyle(24, Color_Grey_4, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)_txtNum);

    _pLineLevel_40 = new UILine(_pPanel, Point(0, 115), Size(400, 1));
    _pLineLevel_40->setLine(Point(0, 190), Point(400, 190), 1, Color_Green_1);
    _pLineLevel_40->setDash(true);

    _pLineLevel_20 = new UILine(_pPanel, Point(0, 175), Size(400, 1));
    _pLineLevel_20->setLine(Point(0, 190), Point(400, 190), 1, Color_Green_1);
    _pLineLevel_20->setDash(true);

    _pLineLevel_0 = new UILine(_pPanel, Point(0, 235), Size(400, 1));
    _pLineLevel_0->setLine(Point(00, 235), Point(400, 235), 1, Color_Red_1);
    _pLineLevel_0->setDash(true);

    memset(_txtInfo, 0x00, 512);
    _pSatInfo = new StaticText(_pPanel, Point(50, 270), Size(300, 106));
    _pSatInfo->SetStyle(20, Color_White, CENTER, TOP);
    _pSatInfo->SetText((UINT8 *)_txtInfo);

    _pFinish = new Button(_pPanel, Point(120, 300), Size(160, 80), 0x20C4, 0x8410);
    _pFinish->SetText((UINT8 *)"Exit", 32, 0xFFFF);
    _pFinish->SetStyle(10, 0.6);
    _pFinish->setOnClickListener(this);
    _pFinish->SetHiden();

    _paggps_client = aggps_open_client();
}

GPSTestWnd::~GPSTestWnd()
{
    delete _pFinish;
    delete _pTestResult;

    if (_pSatSNR) {
        for (int i = 0; i < _numSat; i++) {
            delete _pSatSNR[i];
            _pSatSNR[i] = NULL;
        }
        delete [] _pSatSNR;
        _pSatSNR = NULL;
    }
    if (_pSNR) {
        for (int i = 0; i < _numSat; i++) {
            delete _pSNR[i];
            _pSNR[i] = NULL;
        }
        delete [] _pSNR;
        _pSNR = NULL;
    }
    if (_pSatPRN) {
        for (int i = 0; i < _numSat; i++) {
            delete _pSatPRN[i];
            _pSatPRN[i] = NULL;
        }
        delete [] _pSatPRN;
        _pSatPRN = NULL;
    }

    delete _pSatInfo;

    delete _pLineLevel_0;
    delete _pLineLevel_20;
    delete _pLineLevel_40;

    delete _pTitle;

    delete _pPanel;

    if (_paggps_client) {
        aggps_close_client(_paggps_client);
    }
}

void GPSTestWnd::_printfGPSInfo()
{
    char gps_buffer[512];
    size_t gb_offset = 0;
    char date_buffer[512];
    size_t db_offset = 0;
    struct tm loctime;

    struct aggps_location_info_s *plocation = &_paggps_client->pgps_info->location;
    if (plocation->set & AGGPS_SET_TIME) {
        localtime_r(&plocation->utc_time, &loctime);
        db_offset = strftime(date_buffer, sizeof(date_buffer),
            "%Z %04Y/%02m/%02d %02H:%02M:%02S", &loctime);
    }
    if (plocation->set & AGGPS_SET_LATLON) {
        gb_offset = snprintf(gps_buffer, sizeof(gps_buffer),
            "%10.6f%c %9.6f%c\n",
            fabs(plocation->longitude),
            (plocation->longitude < 0) ? 'W' : 'E',
            fabs(plocation->latitude),
            (plocation->latitude < 0) ? 'S' : 'N');
    }
    if (plocation->set & AGGPS_SET_SPEED) {
        if (sizeof(gps_buffer) > gb_offset) {
            gb_offset += snprintf((gps_buffer + gb_offset),
                (sizeof(gps_buffer) - gb_offset),
                " %4.2fkm/h\n", plocation->speed);
        }
    }
    if (plocation->set & AGGPS_SET_TRACK) {
        if (sizeof(gps_buffer) > gb_offset) {
            gb_offset += snprintf((gps_buffer + gb_offset),
                (sizeof(gps_buffer) - gb_offset),
                " %4.2fTrac", plocation->track);
        }
    }
    if (plocation->set & AGGPS_SET_DOP) {
        if (sizeof(gps_buffer) > gb_offset) {
            gb_offset += snprintf((gps_buffer + gb_offset),
                (sizeof(gps_buffer) - gb_offset),
                " %4.2fAccu", plocation->accuracy);
        }
    }

    //CAMERALOG("%.*s %.*s", (int)db_offset, date_buffer,
    //    (int)gb_offset, gps_buffer);
    snprintf(_txtInfo, 512, "%.*s\n%.*s", (int)db_offset, date_buffer,
        (int)gb_offset, gps_buffer);
}

void GPSTestWnd::_updateStatus()
{
    if (_paggps_client == NULL) {
        _paggps_client = aggps_open_client();
        if (_paggps_client == NULL) {
            snprintf(_txtNum, 32, "GPS Open Failed");
            return;
        }
    }

    if (_pSatSNR) {
        for (int i = 0; i < _numSat; i++) {
            delete _pSatSNR[i];
            _pSatSNR[i] = NULL;
        }
        delete [] _pSatSNR;
        _pSatSNR = NULL;
    }
    if (_pSNR) {
        for (int i = 0; i < _numSat; i++) {
            delete _pSNR[i];
            _pSNR[i] = NULL;
        }
        delete [] _pSNR;
        _pSNR = NULL;
    }
    if (_pSatPRN) {
        for (int i = 0; i < _numSat; i++) {
            delete _pSatPRN[i];
            _pSatPRN[i] = NULL;
        }
        delete [] _pSatPRN;
        _pSatPRN = NULL;
    }
    _numSat = 0;
    memset(_txtInfo, 0x00, 512);

    int ret_val = aggps_read_client(_paggps_client);
    if (ret_val) {
        snprintf(_txtNum, 32, "GPS Read Failed");
        return;
    }

    struct aggps_satellite_info_s *psatellite = &_paggps_client->pgps_info->satellite;
    _numSat = psatellite->num_svs;
    snprintf(_txtNum, 32, "%s\nSIV: %d",
        _paggps_client->pgps_info->devinfo.dev_name, _numSat);
    //CAMERALOG("%d satellites found.", _numSat);

    if (_numSat > 13) {
        _numSat = 13;
    } else if (_numSat <= 0) {
        return;
    }

    _pSatSNR = new StaticText*[_numSat];
    if (_pSatSNR) {
        for (int i = 0; i < _numSat; i++) {
            _pSatSNR[i] = NULL;
        }
    }

    _pSNR = new UIRectangle*[_numSat];
    if (_pSNR) {
        for (int i = 0; i < _numSat; i++) {
            _pSNR[i] = NULL;
        }
    }

    _pSatPRN = new StaticText*[_numSat];
    if (_pSatPRN) {
        for (int i = 0; i < _numSat; i++) {
            _pSatPRN[i] = NULL;
        }
    }

    if (_pSatSNR && _pSNR && _pSatPRN) {
        _printfGPSInfo();

        int font_size = 20;
        int width = 40;
        int padding = 2;
        if (_numSat > 8) {
            width = 28;
            padding = 1;
        }
        int start_pos = (400 - _numSat * (width + padding) + padding) / 2;
        for (int i = 0; i < _numSat; i++) {
            int snr = psatellite->sv_list[i].snr;
            snprintf(_txtSNR[i], 8, "%d", snr);

            int prn = psatellite->sv_list[i].prn;
            snprintf(_txtPRN[i], 8, "%d", prn);

            _pSatPRN[i] = new StaticText(
                _pPanel,
                Point(start_pos + (width + padding) * i, 235),
                Size(width, 30));
            _pSatPRN[i]->SetStyle(font_size, Color_Blue_1, CENTER, MIDDLE);
            _pSatPRN[i]->SetText((UINT8 *)_txtPRN[i]);

            int height = snr * 180 / 60.0;
            if (height > 180) {
                height = 180;
            }
            UINT16 rect_color = Color_Green_1;
            if (snr >= 20) {
                rect_color = Color_Green_1;
            } else {
                rect_color = Color_Grey_3;
            }
            if (height > 0) {
                _pSNR[i] = new UIRectangle(
                    _pPanel,
                    Point(start_pos + (width + padding) * i, 235 - height),
                    Size(width, height),
                    rect_color);
            } else {
                _pSNR[i] = NULL;
            }

            _pSatSNR[i] = new StaticText(
                _pPanel,
                Point(start_pos + (width + padding) * i, 235 - height - 30),
                Size(width, 30));
            _pSatSNR[i]->SetStyle(font_size, Color_White, CENTER, MIDDLE);
            _pSatSNR[i]->SetText((UINT8 *)_txtSNR[i]);
        }
    } else {
        delete [] _pSatSNR;
        _pSatSNR = NULL;
        delete [] _pSNR;
        _pSNR = NULL;
        delete [] _pSatPRN;
        _pSatPRN = NULL;
    }
}

void GPSTestWnd::willHide()
{
}

void GPSTestWnd::willShow()
{
}


void GPSTestWnd::onClick(Control *widget)
{
    if (widget == _pFinish) {
        _pTestResult = new StaticText(_pPanel, Point(0, 150), Size(400, 100));
        _pTestResult->SetStyle(40, CONTENT_COLOR, CENTER, TOP);
        _pTestResult->SetText((UINT8 *)"Test Finished");
        _pFinish->SetHiden();
        _pPanel->Draw(true);
        exit(0);
    }
}

bool GPSTestWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _cnt++;
                if (_cnt >= 10) {
                    _pFinish->SetShow();
                }
                if (_cnt % 2 == 0) {
                    _updateStatus();
                    _pPanel->Draw(true);
                }
            }
        default:
            break;
    }
    return b;
}

};
