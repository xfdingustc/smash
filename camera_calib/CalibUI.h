
#ifndef __CalibUI_h__
#define __CalibUI_h__

#include <alsa/asoundlib.h>

#include <aglib.h>
#include <agbox.h>
#include <agbt.h>

#include "Window.h"
#include "Widgets.h"

namespace ui {

#define  WND_BEGIN       "CalibBeginWnd"
#define  WND_TOUCH       "TouchTestWnd"
#define  WND_BATTERY     "BatteryTestWnd"
#define  WND_SDCARD      "SDCardTestWnd"
#define  WND_Audio       "AudioTestWnd"
#define  WND_Wifi        "WifiTestWnd"
#define  WND_Bluetooth   "BluetoothTestWnd"
#define  WND_IIO         "IIOTestWnd"
#define  WND_GPS         "GPSTestWnd"


class SimpleTestWnd : public Window, public OnClickListener
{
public:
    SimpleTestWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~SimpleTestWnd();
    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

protected:
    Panel       *_pPanel;
    StaticText  *_pTitle;
    StaticText  *_pTxt;
    StaticText  *_pVersion;
    StaticText  *_pBoard;

    Button      *_pPowerOff;
    Button      *_pNext;

    char  _title[64];
    char  _content[128];
    char  _version[64];
    char  _board[64];

    enum {
        STATE_IDLE = 0,
        STATE_TESTING,
        STATE_SUCCESS,
        STATE_FAILED,
    };
    int   _state;
};


class CalibBeginWnd : public SimpleTestWnd
{
public:
    CalibBeginWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~CalibBeginWnd();

    virtual void onClick(Control *widget);

private:
};

class TouchTestWnd : public SimpleTestWnd
{
public:
    TouchTestWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~TouchTestWnd();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void prepareSingleClickTest();
    void prepareDoubleClickTest();
    void prepareHorizontalFlick();
    void prepareVerticalFlick();

    enum {
        CALIB_STEP_INVALID = 0,
        CALIB_STEP_Line_Test,
        CALIB_STEP_SingleClick_Test,
        CALIB_STEP_DoubleClick_Test,
        CALIB_STEP_FLICK_LEFT,
        CALIB_STEP_FLICK_RIGHT,
        CALIB_STEP_FLICK_UP,
        CALIB_STEP_FLICK_DOWN,
    };
    int   _step;

    DrawingBoard  *_pDrawingBoard;

    Button        *_pButtons[16];
    UINT16        _btnFlags;

    StaticText    *_pBtnDblClick[4];
    UINT8         _doubleFlags;

    StaticText    *_pHFlick[4];
    UINT8         _hFlickFlags;

    int   _counter;
};

class BatteryTestWnd : public SimpleTestWnd
{
public:
    BatteryTestWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~BatteryTestWnd();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    bool   doTest();
    int   _counter;
};

class IIOTestWnd : public SimpleTestWnd
{
public:
    IIOTestWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~IIOTestWnd();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    bool doTest();
    bool iioAccelTest();
    bool iioOrientationTest();
    bool iioGyroTest();
    bool iioMangTest();
    bool iioPressureTest();
    bool iioTemperatureTest();

    int   _counter;
};

class SDCardTestWnd : public SimpleTestWnd
{
public:
    SDCardTestWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~SDCardTestWnd();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    bool   doTest();

    int   _counter;
};

class AudioTestWnd : public SimpleTestWnd
{
public:
    AudioTestWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~AudioTestWnd();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    bool   doTest();
    int    calib_diag_audio_config(snd_pcm_t *p_snd_pcm);
    int    calib_diag_audio_read();
    bool   calib_diag_audio_play();
    bool   audio_playback_test();
    bool   audio_capture_test();

    int   _counter;
    int   _match_count;
    UINT8 _audioBuffer[512 * 1024];
};

class WifiTestWnd : public SimpleTestWnd
{
public:
    WifiTestWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~WifiTestWnd();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    bool   doTest();
    int    get_mac_address(char* name, unsigned char *pmac);

    int    _count;
};

class BluetoothTestWnd : public SimpleTestWnd, public OnListClickListener
{
public:
    BluetoothTestWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~BluetoothTestWnd();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

    virtual void onListClicked(Control *list, int index);

private:
    bool   doTest();
    bool   checkDeviceOn();
    bool   scanDevice();
    int    _count;

    struct agbt_scan_list_s _bt_scan_list;

    List            *_pList;
    TextListAdapter *_pAdapter;
    char            *_pMenus[8];
    UINT8           _numMenus;

    Button          *_pReScan;
};

class GPSTestWnd : public Window, public OnClickListener
{
public:
    GPSTestWnd(const char *name, Size wndSize, Point leftTop);
    virtual ~GPSTestWnd();
    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void _printfGPSInfo();
    void _updateStatus();

    Panel           *_pPanel;

    StaticText      *_pTitle;
    char            _txtNum[32];

    UILine          *_pLineLevel_0;
    UILine          *_pLineLevel_20;
    UILine          *_pLineLevel_40;

    int             _numSat;
    StaticText      **_pSatSNR;
    char            _txtSNR[13][8];
    UIRectangle     **_pSNR;
    StaticText      **_pSatPRN;
    char            _txtPRN[13][8];

    StaticText      *_pSatInfo;
    char            _txtInfo[512];

    Button          *_pFinish;
    StaticText      *_pTestResult;

    struct aggps_client_info_s  *_paggps_client;

    int             _cnt;
};


};

#endif
