#ifndef __Camera_DebugMenus_h__
#define __Camera_DebugMenus_h__

#include <stdint.h>

#include "GobalMacro.h"
#include "Window.h"
#include "aggps.h"
#include "CCameraCircularUI.h"

class CCameraProperties;

namespace ui{

class Container;
class Panel;
class Round;
class Fan;
class FanRing;
class StaticText;
class RoundCap;
class UILine;
class UIPoint;
class DrawingBoard;
class Button;
class PngImage;
class BmpImage;
class Circle;
class TabIndicator;
class ImageButton;
class UIRectangle;
class QrCodeControl;
class RadioGroup;
class Pickers;
class Slider;
class TextListAdapter;
class List;
class ViewPager;

class WavPlayWnd: public Window, public OnListClickListener
{
public:
    typedef Window inherited;
    WavPlayWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WavPlayWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onListClicked(Control *list, int index);

private:
    void getFileList();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    UIRectangle     *_pRect;
    List            *_pList;
    TextListAdapter *_pAdapter;
    UINT8           *_pMenus[32];
    UINT8           _numMenus;

    char        _slVal[32];
    StaticText  *_pSlVal;
};

class TouchTestWnd : public Window
{
public:
    typedef Window inherited;
    TouchTestWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~TouchTestWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    CCameraProperties *_cp;

    Panel       *_pPanel;
    UILine      *_pHLine;
    UILine      *_pVLine;

    DrawingBoard  *_pDrawingBoard;
};

class ColorPalleteWnd : public Window
{
public:
    typedef Window inherited;
    ColorPalleteWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~ColorPalleteWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    CCameraProperties *_cp;

    Panel       *_pPanel;

    Round       *_pRound01;
    Round       *_pRound02;
    Round       *_pRound03;
    Round       *_pRound04;
    Round       *_pRound05;
    Round       *_pRound06;
    Round       *_pRound07;
    Round       *_pRound08;
    Round       *_pRound09;
};

// make a complicated UI to test performance
class FpsTestWnd : public Window
{
public:
    typedef Window inherited;
    FpsTestWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~FpsTestWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    void updateSymbols();

    CCameraProperties *_cp;

    Panel      *_pPanel;

    Round      *_pRound_bg;
    Round      *_pRound_center;
    Round      *_pRound_indicator;
    Round      *_pRound_hint;
    UILine     *_pLine_indicator;

    UILine     *_pLine_12Clk;
    UILine     *_pLine_3Clk;
    UILine     *_pLine_6Clk;
    UILine     *_pLine_9Clk;

    StaticText *_pTxt_12Clk;
    StaticText *_pTxt_3Clk;
    StaticText *_pTxt_6Clk;
    StaticText *_pTxt_9Clk;

    StaticText *_pTxt_title;

    char       _text_12Clk[16];
    char       _text_3Clk[16];
    char       _text_6Clk[16];
    char       _text_9Clk[16];

    float      _h_gforce;
    float      _v_gforce;

    int        _counter;
    char       _fps[32];
};

class WifiModeTestWnd : public Window, public OnClickListener, public UploadListener
{
public:
    typedef Window inherited;
    WifiModeTestWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WifiModeTestWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

    virtual void onState(int state);
    virtual void onProgress(int percentage);

private:
    int getWLANIP(char *ip, int len);
    void updateWifi();

    CCameraProperties *_cp;

    Panel       *_pPanel;
    Button      *_pApMode;
    Button      *_pClientMode;
    Button      *_pMultiRole;
    Button      *_pOff;
    StaticText  *_pWifiInfo;

    Button      *_pUpload;
    StaticText  *_pStat;
    char        _stat[32];

    char        _txt[64];
    int         _counter;
};

class DebugMenuWnd : public MenuWindow,
                    public OnClickListener,
                    public OnListClickListener
{
public:
    typedef MenuWindow inherited;
    DebugMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~DebugMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onListClicked(Control *list, int index);

private:
    int copyLog();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    List            *_pList;
    TextListAdapter *_pAdapter;
#define NUM_MENUS   11
    UINT8           *_pMenus[NUM_MENUS];
    UINT8           _numMenus;

    StaticText      *_pLogResult;
};

class WifiSignalDebugWnd : public MenuWindow,
                    public OnClickListener,
                    public OnListClickListener
{
public:
    typedef MenuWindow inherited;
    WifiSignalDebugWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WifiSignalDebugWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onListClicked(Control *list, int index);

private:
    void _parseWifi(char *json);

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    List            *_pList;
    TextListAdapter *_pAdapter;
    char            **_wifiList;
    int             _wifiNum;

    int             _cnt;
};

class GPSDebugWnd : public MenuWindow
{
public:
    typedef MenuWindow inherited;
    GPSDebugWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~GPSDebugWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

private:
    void _printfGPSInfo();
    void _updateStatus();

    Panel           *_pPanel;

    StaticText      *_pTitle;
    char            _txtNum[48];

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

    struct aggps_client_info_s  *_paggps_client;

    int             _cnt;
};

class OBDVINWnd : public MenuWindow, public OnClickListener
{
public:
    typedef MenuWindow inherited;
    OBDVINWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~OBDVINWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void _updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    StaticText      *_pVINOrign;
    char            _txtVINOrign[32];

    StaticText      *_pMASK;
    char            _txtMASK[32];

    StaticText      *_pVINMASK;
    char            _txtVINMASK[32];
};

class Opt23976FpsMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    Opt23976FpsMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~Opt23976FpsMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pInfo;
    char            _txt[256];

    StaticText      *_pStatus;
    Button          *_pSwitchFake;
    BmpImage        *_pSwitch;
};

class NoWriteRecordMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    NoWriteRecordMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~NoWriteRecordMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pInfo;
    char            _txt[256];

    StaticText      *_pStatus;
    Button          *_pSwitchFake;
    BmpImage        *_pSwitch;
};

class TemperatureMenuWnd : public MenuWindow, public OnClickListener
{
public:
    typedef MenuWindow inherited;
    TemperatureMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~TemperatureMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void _updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    StaticText      *_pBoard;
    StaticText      *_pBoardTemp;
    char            _txtBoardTemp[32];

    StaticText      *_pSensor;
    StaticText      *_pSensorTemp;
    char            _txtSensorTemp[32];

    StaticText      *_pWifi;
    StaticText      *_pWifiTemp;
    char            _txtWifiTemp[32];

    int             _cnt;
};

};

#endif
