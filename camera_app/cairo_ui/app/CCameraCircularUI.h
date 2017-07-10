#ifndef __Camera_Circular_UI_h
#define __Camera_Circular_UI_h

#include <stdint.h>

#include "GobalMacro.h"
#include "Window.h"
#include "upload_service/uploadlister.h"
#include "aggps.h"

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

class LapTimerFragment : public Fragment, public OnClickListener
{
public:
    typedef Fragment inherited;
    LapTimerFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~LapTimerFragment();
    virtual void onResume();
    virtual void onPause();
    virtual void onClick(Control *widget);

private:
    CCameraProperties *_cp;

    StaticText        *_pTitle;
    UILine            *_pLine;

    ImageButton       *_pLapTimer;
    Button            *_pScores;
};

class FastFuriousFragment : public Fragment, public OnClickListener
{
public:
    typedef Fragment inherited;
    FastFuriousFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~FastFuriousFragment();
    virtual void onResume();
    virtual void onPause();
    virtual void onClick(Control *widget);

private:
    CCameraProperties *_cp;

    StaticText        *_pTitle;
    UILine            *_pLine;

    ImageButton       *_pCountdownMode;
    ImageButton       *_pAutoMode;
    Button            *_pScores;
};

class RecordFragment : public Fragment, public OnClickListener
{
public:
    typedef Fragment inherited;
    RecordFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~RecordFragment();
    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    void updateRecStatus();
    void updateRecButton();
    void checkSpaceWarning();

    CCameraProperties *_cp;

    BmpImage     *_pRecStatus;
    StaticText   *_pRecTxt;
    BmpImage     *_pSpaceWarning;
    Button       *_pBtnWarning;

    ImageButton  *_pRecButton;
    ImageButton  *_pTagButton;

    int   _counter;
    char  _txt[64];

    bool  _bSpaceWarning;

    int   _exitCnt;
};

class ShotcutsFragment : public Fragment, public OnClickListener
{
public:
    typedef Fragment inherited;
    ShotcutsFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~ShotcutsFragment();
    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    void updateMic();

    CCameraProperties *_cp;

    ImageButton  *_pBrightness;
    ImageButton  *_pSpeaker;
    ImageButton  *_pMicmute;
    ImageButton  *_pWifiInfo;
};


class ViewPagerShotcutsWnd : public Window,
                                public ViewPagerListener
{
public:
    typedef Window inherited;
    ViewPagerShotcutsWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~ViewPagerShotcutsWnd();

    virtual void onResume();
    virtual void onPause();

    virtual bool OnEvent(CEvent *event);

    virtual void onFocusChanged(int indexFocus);
    virtual void onViewPortChanged(const Point &leftTop);

private:
    int  _getScreenSaverTime();
    CCameraProperties *_cp;

    Panel           *_pPanel;

    ViewPager       *_pViewPager;
    Fragment        *_pPages[4];
    UINT8           _numPages;

    TabIndicator    *_pIndicator;
    int             _indexFocus;
    int             _indexShortcuts;

    int             _cntScreenSaver;
    int             _screenSaverTimeSetting;
};


class CoolantTempFragment: public Fragment
{
public:
    typedef Fragment inherited;
    CoolantTempFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~CoolantTempFragment();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    bool getTemperature();
    bool getTemperatureFake();
    bool readOBDData();
    void updateSymbols(bool bValid);
    bool checkOBDPaired();

    CCameraProperties *_cp;

    RoundCap   *_pRTempe;
    Round      *_pRound;

    BmpImage   *_pSign;
    BmpImage   *_pNum_h;
    BmpImage   *_pNum_d;
    BmpImage   *_pNum_c;
    BmpImage   *_pUnit;
    BmpImage   *_pLabel;
    BmpImage   *_pLabel_demo;

    int    _temperature;
    bool   _bChanged;

    bool   _bObdPaired;
    bool   _bDemoEnabled;

    bool   _bTita;
    int    _counter;

    bool   _bImperialUnits;
};

class BoostFragment: public Fragment
{
public:
    typedef Fragment inherited;
    BoostFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~BoostFragment();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    bool getBoostValue();
    bool getBoostValueFake();
    bool readOBDData();
    void updateSymbols(bool bValid);
    bool checkOBDPaired();

    CCameraProperties *_cp;

    Fan        *_pFanBoost;
    Round      *_pRound;

    BmpImage   *_pNum_d;
    BmpImage   *_pNum_c;
    BmpImage   *_pUnit;
    BmpImage   *_pLabel;
    BmpImage   *_pLabel_demo;

    float  _maxKpa;
    float  _minKpa;
    int    _boost;
    bool   _bChanged;

    bool   _bObdPaired;
    bool   _bDemoEnabled;
    bool   _bTita;
    int    _counter;
};

class MAFFragment: public Fragment
{
public:
    typedef Fragment inherited;
    MAFFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~MAFFragment();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    bool getMAF();
    bool getMAFFake();
    bool readOBDData();
    void updateSymbols(bool bValid);
    bool checkOBDPaired();

    CCameraProperties *_cp;

    Round      *_pRMAF;
    Round      *_pRound;

    BmpImage   *_pNum_h;
    BmpImage   *_pNum_d;
    BmpImage   *_pNum_c;
    BmpImage   *_pUnit;
    BmpImage   *_pLabel;
    BmpImage   *_pLabel_demo;

    int    _maf;
    bool   _bChanged;

    bool   _bObdPaired;
    bool   _bDemoEnabled;

    bool   _bTita;
    int    _counter;

    bool   _bImperialUnits;
};

class RPMFragment: public Fragment
{
public:
    typedef Fragment inherited;
    RPMFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~RPMFragment();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    bool getRPM();
    bool getRPMFake();
    bool readOBDData();
    void updateSymbols(bool bValid);
    bool checkOBDPaired();

    CCameraProperties *_cp;

    Round      *_pRRPM;
    Round      *_pRound;

    BmpImage   *_pNum_d;
    BmpImage   *_pPoint;
    BmpImage   *_pNum_c;
    BmpImage   *_pUnit;
    BmpImage   *_pLabel;
    BmpImage   *_pLabel_demo;

    int    _rpm;
    bool   _bChanged;

    bool   _bObdPaired;
    bool   _bDemoEnabled;

    bool   _bTita;
    int    _counter;
};

class SpeedFragment: public Fragment
{
public:
    typedef Fragment inherited;
    SpeedFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~SpeedFragment();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    bool getSpeedGPS(int &speedKmh);
    bool getSpeedOBD(int &speedKmh);
    bool getSpeedFake();
    bool readSpeedData();
    void updateSymbols(bool bValid);
    bool checkOBDPaired();

    struct aggps_client_info_s  *_paggps_client;

    CCameraProperties *_cp;

    Round      *_pRSpeed;
    Round      *_pRound;

    BmpImage   *_pNum_h;
    BmpImage   *_pNum_d;
    BmpImage   *_pNum_c;
    BmpImage   *_pLabel;
    BmpImage   *_pLabel_demo;

    int    _speed;
    bool   _bChanged;

    bool   _bImperialUnits;
    bool   _bObdPaired;
    bool   _bDemoEnabled;

    bool   _bTita;
    int    _counter;
};

class LiveViewFragment : public Fragment, public OnClickListener
{
public:
    typedef Fragment inherited;
    LiveViewFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~LiveViewFragment();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateRecTime();
    void updateBattery();
    void updateWifi();
    void updateBluetooth();
    void updateGPS();
    void updateMicSpk();
    void updateTempAlert();

    void checkSpaceWarning();
    void checkTempratureStatus();

    CCameraProperties *_cp;

    Round       *_pRPreview;
    Circle      *_pBorder;

    BmpImage    *_pRecStatus;
    StaticText  *_pTxt;
    BmpImage    *_pSpaceWarning;
    Button      *_pBtnWarning;

    BmpImage    *_pWifiStatus;
    BmpImage    *_pGPS;
    BmpImage    *_pOBDStatus;
    BmpImage    *_pRemoteStatus;
    BmpImage    *_pBatteryStatus;
    BmpImage    *_pMicMute;
    BmpImage    *_pSpkMute;

    BmpImage    *_pAlertTemp;

    int   _counter;
    char  _txt[64];

    bool  _bFullScreen;
    bool  _bConnectingOBD;
    bool  _bConnectingRC;
    bool  _bSpaceWarning;

    enum {
        TempratureStat_Normal = 0,
        TempratureStat_Overheat,
        TempratureStat_Overcold,
    };
    int   _TempratureStat;
};

class PitchRollFragment : public Fragment, public CThread
{
public:
    typedef Fragment inherited;
    PitchRollFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~PitchRollFragment();

    virtual void onResume();
    virtual void onPause();
    virtual void onTimerEvent();
    virtual bool OnEvent(CEvent *event);

    virtual void main();

private:
    bool getPitchRollAngle();
    void updateSymbols();
    bool _isRotate180();

    CCameraProperties *_cp;

    UILine     *_pHLine;
    RoundCap   *_pCap;
    Round      *_pRound;

    // pitch
    BmpImage   *_pSymPitch;
    BmpImage   *_pPitchPlus;
    BmpImage   *_pPitchMinus;
    BmpImage   *_pPitchNum_h;
    BmpImage   *_pPitchNum_d;
    BmpImage   *_pPitchNum_c;
    BmpImage   *_pPitchSymDeg;

    // roll
    BmpImage   *_pSymRoll;
    BmpImage   *_pRollPlus;
    BmpImage   *_pRollMinus;
    BmpImage   *_pRollNum_h;
    BmpImage   *_pRollNum_d;
    BmpImage   *_pRollNum_c;
    BmpImage   *_pRollSymDeg;

    BmpImage   *_pTitle;

    float  _angle_p;
    float  _angle_r;
    bool   _bChanged;

    bool   _bRotate180;

    CSemaphore *_pWaitEvent;
    CMutex     *_pLock;
    int        _numTimerEvent;
};

class PitchFragment : public Fragment, public CThread
{
public:
    typedef Fragment inherited;
    PitchFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~PitchFragment();

    virtual void onResume();
    virtual void onPause();
    virtual void onTimerEvent();
    virtual bool OnEvent(CEvent *event);

    virtual void main();

private:
    bool getPitchAngle();
    void updateSymbols();
    bool _isRotate180();

    CCameraProperties *_cp;

    UILine     *_pHLine;
    RoundCap   *_pCap;
    Round      *_pRound;
    BmpImage   *_pPlus;
    BmpImage   *_pMinus;
    BmpImage   *_pNum_h;
    BmpImage   *_pNum_d;
    BmpImage   *_pNum_c;
    BmpImage   *_pSymDeg;
    BmpImage   *_pTitle;

    float  _angle_p;
    bool   _bChanged;

    bool   _bRotate180;

    CSemaphore *_pWaitEvent;
    CMutex     *_pLock;
    int        _numTimerEvent;
};

class RollFragment : public Fragment, public CThread
{
public:
    typedef Fragment inherited;
    RollFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~RollFragment();

    virtual void onResume();
    virtual void onPause();
    virtual void onTimerEvent();
    virtual bool OnEvent(CEvent *event);

    virtual void main();

private:
    bool getRollAngle();
    void updateSymbols();
    bool _isRotate180();

    CCameraProperties *_cp;

    UILine     *_pHLine;
    RoundCap   *_pCap;
    Round      *_pRound;

    BmpImage   *_pPlus;
    BmpImage   *_pMinus;
    BmpImage   *_pNum_h;
    BmpImage   *_pNum_d;
    BmpImage   *_pNum_c;
    BmpImage   *_pSymDeg;

    BmpImage   *_pTitle;

    float  _angle_r;
    bool   _bChanged;

    bool   _bRotate180;

    CSemaphore *_pWaitEvent;
    CMutex     *_pLock;
    int        _numTimerEvent;
};

class GForceFragment: public Fragment
{
public:
    typedef Fragment inherited;
    GForceFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~GForceFragment();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    bool getAccelXYZ();
    void updateSymbols();
    bool _isRotate180();

    CCameraProperties *_cp;

    Round      *_pRAccel;
    Circle     *_pMaxHistory;
    Round      *_pRound;

    // X
    BmpImage   *_pSymX;
    BmpImage   *_pXPlus;
    BmpImage   *_pXMinus;
    BmpImage   *_pXNum_int;
    BmpImage   *_pXNum_dot;
    BmpImage   *_pXNum_f1;
    BmpImage   *_pXNum_f2;

    // Y
    BmpImage   *_pSymY;
    BmpImage   *_pYPlus;
    BmpImage   *_pYMinus;
    BmpImage   *_pYNum_int;
    BmpImage   *_pYNum_dot;
    BmpImage   *_pYNum_f1;
    BmpImage   *_pYNum_f2;

    BmpImage   *_pTitle;

    TabIndicator *_pIndicator;

    float  _accel_AB;
    float  _accel_LR;
    bool   _bChanged;

    bool   _bRotate180;

    UINT16  _rMaxDiff;
    UINT16  _rMaxHistory;
};

class GaugesWnd : public Window,
                    public ViewPagerListener
{
public:
    typedef Window inherited;
    GaugesWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~GaugesWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onFocusChanged(int indexFocus);
    virtual void onViewPortChanged(const Point &leftTop);

private:
    int  _getScreenSaverTime();
    UINT8 _getOBDGaugeFlags();
    void _loadOBDGauges();

    CCameraProperties *_cp;

    Panel           *_pPanel;

    ViewPager       *_pViewPager;
    Fragment        *_pPages[8];
    UINT8           _numPages;
    UINT8           _liveviewIndex;
    UINT8           _pitchrollIndex;

    TabIndicator    *_pIndicator;
    int             _indexFocus;

    Fragment        *_pPitchRolls[3];
    int             _indexPitchRoll;

    UINT8           _obdGaugeFlags;
    Fragment        *_pOBDGauges[5];
    int             _indexOBDStart;

    int             _cntScreenSaver;
    int             _screenSaverTimeSetting;
};

class Menu_P1_Fragment: public Fragment,
                        public OnClickListener
{
public:
    typedef Fragment inherited;
    Menu_P1_Fragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~Menu_P1_Fragment();

    virtual void onResume();
    virtual void onPause();

    virtual void onClick(Control *widget);

private:
    CCameraProperties *_cp;

    Button      *_pMenuVideo;
    Button      *_pMenuAudio;
    Button      *_pMenuDisplay;
    Button      *_pMenuWifi;
    Button      *_pMenuBluetooth;
};

class Menu_P2_Fragment: public Fragment,
                        public OnClickListener
{
public:
    typedef Fragment inherited;
    Menu_P2_Fragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp);
    virtual ~Menu_P2_Fragment();

    virtual void onResume();
    virtual void onPause();

    virtual void onClick(Control *widget);

private:
    CCameraProperties *_cp;

    Button      *_pMenuStorage;
    Button      *_pMenuGauges;
    Button      *_pMenuDateTime;
    Button      *_pMenuMisc;
};

class SettingsGaugesWnd : public Window,
                    public ViewPagerListener
{
public:
    typedef Window inherited;
    SettingsGaugesWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~SettingsGaugesWnd();

    virtual void onResume();
    virtual void onPause();

    virtual bool OnEvent(CEvent *event);

    virtual void onFocusChanged(int indexFocus);
    virtual void onViewPortChanged(const Point &leftTop);

private:
    int  _getScreenSaverTime();

    CCameraProperties *_cp;

    Panel           *_pPanel;

    ViewPager       *_pViewPager;
    Fragment        *_pPages[3];
    UINT8           _numPages;

    TabIndicator    *_pIndicator;
    int             _indexFocus;

    int             _cntScreenSaver;
    int             _screenSaverTimeSetting;
};

class MenuWindow : public Window
{
public:
    typedef Window inherited;
    MenuWindow(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~MenuWindow();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onMenuResume() = 0;
    virtual void onMenuPause() = 0;

protected:
    int  _getScreenSaverTime();

    CCameraProperties *_cp;

    int                _cntScreenSaver;
    int                _screenSaverTimeSetting;
};

class VideoMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    VideoMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~VideoMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void _updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    Button          *_pQualityBtnFake;
    StaticText      *_pQuality;
    StaticText      *_pQualityResult;
    char            _txtQuality[16];

    Button          *_pResolutionBtnFake;
    StaticText      *_pResolution;
    StaticText      *_pResolutionResult;
    char            _txtResolution[16];
};

class VideoQualityWnd : public MenuWindow,
                    public OnClickListener,
                    public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    VideoQualityWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~VideoQualityWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    int updateStatus();
    void showDialogue();
    void hideDialogue();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    char            *_pMenus[3];
    UINT8           _numMenus;

    Pickers         *_pPickers;
    Size            _pickerSize;

    StaticText      *_pTxt;
    ImageButton     *_pIconYes;
    ImageButton     *_pIconNo;
    int             _targetIndex;
};

class VideoResolutionWnd : public MenuWindow,
                    public OnClickListener,
                    public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    VideoResolutionWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~VideoResolutionWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    int updateStatus();
    void showDialogue();
    void hideDialogue();

    bool is2397Enabled();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    char            *_pResolutions[6];
    char            *_pMenus[6];
    UINT8           _numMenus;

    Pickers         *_pPickers;
    Size            _pickerSize;

    StaticText      *_pTxt;
    ImageButton     *_pIconYes;
    ImageButton     *_pIconNo;
    int             _targetIndex;
};

class AudioMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    AudioMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~AudioMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateVolumeDisplay();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    Button          *_pSwitchBtnFake;
    StaticText      *_pStatus;
    BmpImage        *_pSwitch;

    Button          *_pVolumeBtnFake;
    StaticText      *_pVolume;
    StaticText      *_pVolumeResult;
    char            _txtVolume[16];

    Button          *_pMicSwitchBtnFake;
    StaticText      *_pMicStatus;
    BmpImage        *_pMicSwitch;

    Button          *_pMicVolumeBtnFake;
    StaticText      *_pMicVolume;
    StaticText      *_pMicVolumeResult;
    char            _txtMicVolume[16];
};

class SpeakerVolumeMenuWnd : public MenuWindow,
                    public OnClickListener,
                    public OnSliderChangedListener
{
public:
    typedef MenuWindow inherited;
    SpeakerVolumeMenuWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~SpeakerVolumeMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onSliderChanged(Control *slider, int percentage);
    virtual void onSliderTriggered(Control *slider, bool triggered);

private:
    void updateSpkVolumeDisplay();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    char            _slVal[16];
    StaticText      *_pTitle;
    UILine          *_pLine;

    StaticText      *_pSlVal;
    Slider          *_pSlider;

    StaticText      *_pInfo;
};

class MicVolumeMenuWnd : public MenuWindow,
                    public OnClickListener,
                    public OnSliderChangedListener
{
public:
    typedef MenuWindow inherited;
    MicVolumeMenuWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~MicVolumeMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onSliderChanged(Control *slider, int percentage);
    virtual void onSliderTriggered(Control *slider, bool triggered);

private:
    void updateMicVolumeDisplay();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    char            _slVal[16];
    StaticText      *_pTitle;
    UILine          *_pLine;

    StaticText      *_pSlVal;
    Slider          *_pSlider;

    StaticText      *_pInfo;
};

class DisplayMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    DisplayMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~DisplayMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    Button          *_pBrghtBtnFake;
    StaticText      *_pBrght;
    StaticText      *_pBrghtValue;
    char            _txtBrght[16];

    Button          *_pModeBtnFake;
    StaticText      *_pMode;
    StaticText      *_pModeResult;
    char            _txtMode[16];

    Button          *_pRotateBtnFake;
    StaticText      *_pRotate;
    StaticText      *_pRotateResult;
    char            _txtRotate[16];
};

class RotationModeWnd : public MenuWindow,
                    public OnClickListener,
                    public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    RotationModeWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~RotationModeWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    enum Rotation_Mode {
        Rotation_Mode_None = 0,
        Rotation_Mode_Auto,
        Rotation_Mode_Normal,
        Rotation_Mode_180,
    };

    void updateStatus();
    void showDialogue(bool bRecording);
    void hideDialogue();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    Pickers         *_pPickers;
    Size            _pickerSize;
    char            *_pMenus[3];
    UINT8           _numMenus;

    StaticText      *_pTxt;
    ImageButton     *_pIconYes;
    ImageButton     *_pIconNo;

    Rotation_Mode   _currentMode;
    Rotation_Mode   _targetMode;
};

class BrightnessMenuWnd : public MenuWindow,
                    public OnClickListener,
                    public OnSliderChangedListener
{
public:
    typedef MenuWindow inherited;
    BrightnessMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~BrightnessMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onSliderChanged(Control *slider, int percentage);
    virtual void onSliderTriggered(Control *slider, bool triggered);

private:
    void updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    char            _slVal[16];
    StaticText      *_pTitle;
    UILine          *_pLine;

    StaticText      *_pSlVal;
    Slider          *_pSlider;

    RoundCap        *_pCap;
};

class ScreenSaverSettingsWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    ScreenSaverSettingsWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~ScreenSaverSettingsWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    Button          *_pOffBtnFake;
    StaticText      *_pOffTime;
    StaticText      *_pOffValue;
    char            _txtOff[16];

    Button          *_pStyleBtnFake;
    StaticText      *_pStyle;
    StaticText      *_pStyleResult;
    char            _txtStyle[16];
};

class DisplayOffMenuWnd : public MenuWindow,
                    public OnClickListener,
                    public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    DisplayOffMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~DisplayOffMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    int updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    char            *_pMenus[8];
    UINT8           _numMenus;

    Pickers         *_pPickers;
    Size            _pickerSize;
};

class ScreenSaverStyleWnd : public MenuWindow,
                    public OnClickListener,
                    public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    ScreenSaverStyleWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~ScreenSaverStyleWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    void updateMode();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    Pickers         *_pPickers;
    Size            _pickerSize;
    char            *_pMenus[2];
    UINT8           _numMenus;
};

class AutoOffMenuWnd : public MenuWindow,
                    public OnClickListener,
                    public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    AutoOffMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~AutoOffMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    int updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    char            *_pMenus[8];
    UINT8           _numMenus;

    Pickers         *_pPickers;
    Size            _pickerSize;
};

class AutoRecordWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    AutoRecordWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~AutoRecordWnd();

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

class StorageMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    StorageMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~StorageMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateStorageStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTotal;
    StaticText      *_pTotalResult;
    char            _txtTotle[16];

    StaticText      *_pHighlight;
    StaticText      *_pHighlightResult;
    char            _txtHighlight[16];

    StaticText      *_pLoop;
    StaticText      *_pLoopResult;
    char            _txtLoop[16];

    Button          *_pFormatBtnFake;
    StaticText      *_pFormat;

    int             _counter;
};

class FormatTFWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    FormatTFWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~FormatTFWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    enum {
        FormatStep_Idle = 0,
        FormatStep_NeedCorfirm_1,
        FormatStep_NeedCorfirm_2,
        FormatStep_Formatting,
        FormatStep_Success,
        FormatStep_Failed,
    };

    void updateStorageStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    BmpImage        *_pAnimation;

    UIRectangle     *_pLegend[2];
    StaticText      *_pLegendTxt[2];

    Button          *_pFormat;
    StaticText      *_pFormatInfo;

    Circle          *_pTotle;
    Arc             *_pLoop;
    Arc             *_pMark;

    ImageButton     *_pYes;
    ImageButton     *_pNo;

    int             _formatStep;

    char            _txt[64];
    int             _counter;
    int             _formatCnt;
    int             _delayToClose;
};

class AdvancedMenuWnd : public MenuWindow, public OnClickListener
{
public:
    typedef MenuWindow inherited;
    AdvancedMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~AdvancedMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    Scroller        *_pScroller;
    Size            _visibleSizeScroller;

    StaticText      *_pName;
    StaticText      *_pNameValue;
    char            _txtName[64];

    Button          *_pVerBtnFake;
    StaticText      *_pVersion;
    StaticText      *_pVersionValue;
    char            _txtVersionSys[16];

    Button          *_pUnitsFake;
    StaticText      *_pUnits;
    StaticText      *_pUnitsValue;
    char            _txtUnits[16];

    Button          *_pAutoOffFake;
    StaticText      *_pAutoOff;
    StaticText      *_pAutoOffValue;
    char            _txtAutoOff[16];

    Button          *_pAutoRecordFake;
    StaticText      *_pAutoRecord;
    StaticText      *_pAutoRecordValue;
    char            _txtAutoRecord[16];

    Button          *_pAutoMarkFake;
    StaticText      *_pAutoMark;
    StaticText      *_pAutoMarkValue;
    char            _txtAutoMark[16];

    Button          *_pResetBtnFake;
    Button          *_pFactoryReset;

    Button          *_pPowerOffBtnFake;
    Button          *_pPowerOff;
};

class VersionMenuWnd : public MenuWindow, public OnClickListener
{
public:
    typedef MenuWindow inherited;
    VersionMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~VersionMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    Scroller        *_pScroller;
    Size            _visibleSizeScroller;
    Size            _scrollableSize;

    StaticText      *_pBSP;
    StaticText      *_pBSPValue;
    char            _vBsp[32];

    StaticText      *_pSN;
    StaticText      *_pSNValue;
    char            _vSN[32];

    StaticText      *_pVAPI;
    StaticText      *_pVAPIValue;
    char            _vAPI[32];

    StaticText      *_pVSystem;
    StaticText      *_pVSystemValue;
    char            _vSys[32];
};

class FactoryResetWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    FactoryResetWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~FactoryResetWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    enum {
        ResetStep_Idle = 0,
        ResetStep_NeedCorfirm,
        ResetStep_Resetting,
    };

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pResetInfo;
    char            _txt[64];

    ImageButton     *_pYes;
    ImageButton     *_pNo;

    int             _resetStep;
    int             _poweroffCnt;
};

class TimeDateMenuWnd : public MenuWindow, public OnClickListener
{
public:
    typedef MenuWindow inherited;
    TimeDateMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~TimeDateMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateTime();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTime;
    StaticText      *_pTimeResult;
    Button          *_pBtnTimeFake;
    char            _txtTime[16];

    StaticText      *_pDate;
    StaticText      *_pDateResult;
    Button          *_pBtnDateFake;
    char            _txtDate[16];

    int             _counter;
};

class TimeFormatMenuWnd : public MenuWindow, public OnClickListener
{
public:
    typedef MenuWindow inherited;
    TimeFormatMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~TimeFormatMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateTime();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    StaticText      *_pTime;
    char            _txtTime[16];

    StaticText      *_pStatus;
    Button          *_pSwitchFake;
    BmpImage        *_pSwitch;

    int             _counter;
};

class DateFormatMenuWnd : public MenuWindow, public OnClickListener, public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    DateFormatMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~DateFormatMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    void updateTime();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    StaticText      *_pDate;
    char            _txtDate[16];

    StaticText      *_pStatus;
    StaticText      *_pFormat;
    Button          *_pSwitchFake;
    char            _dateFormat[16];

    Pickers         *_pPickers;
    Size            _pickerSize;
    char            *_pMenus[8];
    UINT8           _numMenus;

    int             _counter;
};

class OBDMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    OBDMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~OBDMenuWnd();

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

    Button          *_pStatusBtnFake;
    StaticText      *_pStatus;
    BmpImage        *_pSwitch;
};

class OBDGaugesWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    OBDGaugesWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~OBDGaugesWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void _updateStatus();
    void _initScroller();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    Scroller        *_pScroller;
    Size            _visibleSizeScroller;

    Button          *_pTemperatureFake;
    StaticText      *_pTemperatureStatus;
    BmpImage        *_pTemperatureSwitch;

    Button          *_pBoostFake;
    StaticText      *_pBoostStatus;
    BmpImage        *_pBoostSwitch;

    Button          *_pMAFFake;
    StaticText      *_pMAFStatus;
    BmpImage        *_pMAFSwitch;

    Button          *_pRPMFake;
    StaticText      *_pRPMStatus;
    BmpImage        *_pRPMSwitch;

    Button          *_pSpeedFake;
    StaticText      *_pSpeedStatus;
    BmpImage        *_pSpeedSwitch;
};

class UnitsWnd : public MenuWindow,
                    public OnClickListener,
                    public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    UnitsWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~UnitsWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    void updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    Pickers         *_pPickers;
    Size            _pickerSize;
    char            *_pMenus[2];
    UINT8           _numMenus;
};

class AutoMarkWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    AutoMarkWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~AutoMarkWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pStatus;
    Button          *_pSwitchFake;
    BmpImage        *_pSwitch;

    Button          *_pSensitivityFake;
    StaticText      *_pSensitivityStatus;
    StaticText      *_pSensitivityValue;
    char            _txtSensitivity[16];
};

class SensitivityWnd : public MenuWindow,
                    public OnClickListener,
                    public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    SensitivityWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~SensitivityWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    void updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    Pickers         *_pPickers;
    Size            _pickerSize;
    char            *_pMenus[3];
    UINT8           _numMenus;
};

class ScreenSaverWnd : public Window
{
public:
    typedef Window inherited;
    ScreenSaverWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~ScreenSaverWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    enum {
        Style_Display_Off = 0,
        Style_Bubble_Clock,
    };

    void _updateStatus();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    Round           *_pBubble;
    int             _counter;
    int             _style;
};

}
#endif
