#ifndef __Camera_FastFurious_h__
#define __Camera_FastFurious_h__

#include <stdint.h>

#include "GobalMacro.h"
#include "Window.h"
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


class CountdownTestWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    CountdownTestWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~CountdownTestWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    enum GPSStatus {
        GPS_Status_Not_Ready = 0,
        GPS_Status_Low_Accuracy,
        GPS_Status_Ready,
    };

    enum TestStatus {
        Status_None = 0,
        Status_Not_On_CarMount,
        Status_GPS_Not_Ready,
        Status_GPS_Low_Accuracy,
        Status_GPS_Ready,
        Status_Counting_Down,
        Status_Testing,
        Status_Test_Failed,
        Status_Test_Success,
        Status_Test_Show_Result,
    };

    enum FailedReason {
        Failed_Reason_None = 0,
        Failed_Reason_Not_On_CarMount,
        Failed_Reason_GPS_Not_Ready,
        Failed_Reason_GPS_Low_Accuracy,
        Failed_Reason_False_Start,
        Failed_Reason_Slowdown,
        Failed_Reason_Takes_Too_Long,
        Failed_Reason_Bad_Test,
    };

    struct GPSTime {
        time_t utc_time;
        int    utc_time_usec;
    };

    struct GPSSpeed {
        float speedkmh;
        float accel[3];
        GPSTime time;
    };

    GPSStatus _getGPSStatus();
    GPSStatus _getSpeedKMH(GPSSpeed &speed);

    bool _loadScores();
    void _saveTestScore();
    void _saveTestScore(bool bImperial, int score_1_us, int score_2_us);
    void _addClipSceneData();

    void _recordSpeed(GPSSpeed &speed);
    bool _isMoveBegin(GPSSpeed &speed);

    void _updateCountdown();
    void _updateTesting();
    void _showTestSuccess();
    void _showFailedReason();

    void _initDisplay();
    void _updateStartRec();

    CCameraProperties *_cp;

    struct aggps_client_info_s  *_paggps_client;

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    BmpImage        *_pAnimation;
    int             _cntAnim;

    BmpImage        *_pIcnFeedback;
    Button          *_pStartFake;
    Button          *_pStart;
    StaticText      *_pWarning;

    FanRing         *_pRing;
    Arc             *_p30MphProgress;
    Arc             *_p60MphProgress;

    StaticText      *_pTimer;
    char            _txtTimer[16];

    StaticText      *_p30MPHTime;
    char            _txt30MPHTime[16];

    StaticText      *_pScoreTitle30Mph;
    StaticText      *_pScore30Mph;
    char            _score30Mph[16];
    BmpImage        *_p30MphBest;

    StaticText      *_pScoreTitle60Mph;
    StaticText      *_pScore60Mph;
    char            _score60Mph[16];
    BmpImage        *_p60MphBest;

    StaticText      *_pHintInfo;
    char            _txtHint[32];
    StaticText      *_pOK;

    int             _counter;
    TestStatus      _testStatus;
    FailedReason    _failedReason;

#define CD_HISTORY_RECORD_NUM      10
    float           _bestSpeed;
    GPSSpeed        _historySpeed[CD_HISTORY_RECORD_NUM];

    GPSTime         _start_time;

    bool            _bImperialUnits;

    bool            _bMoveBegin;
    bool            _bExceed30Mph;
    bool            _bExceed50kmh;
    bool            _bExceed60Mph;
    bool            _bExceed100kmh;

    int             _usec_offset_begin_move;
    int             _usec_offset_30mph;
    int             _usec_offset_50kmh;
    int             _usec_offset_60mph;
    int             _usec_offset_100kmh;

    int             _best_score_1_ms;
    int             _best_score_2_ms;

    float           _gpsReliableIndex;
};

class AutoTestWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    AutoTestWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~AutoTestWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    enum GPSStatus {
        GPS_Status_Not_Ready = 0,
        GPS_Status_Low_Accuracy,
        GPS_Status_Ready,
    };

    enum TestStatus {
        Status_None = 0,
        Status_Not_On_CarMount,
        Status_GPS_Not_Ready,
        Status_GPS_Low_Accuracy,
        Status_Waiting,
        Status_Testing,
        Status_Test_Failed,
        Status_Test_Success,
        Status_Test_Show_Result,
    };

    enum FailedReason {
        Failed_Reason_None = 0,
        Failed_Reason_Not_On_CarMount,
        Failed_Reason_GPS_Not_Ready,
        Failed_Reason_GPS_Low_Accuracy,
        Failed_Reason_Slowdown,
        Failed_Reason_Takes_Too_Long,
        Failed_Reason_Bad_Test,
    };

    struct GPSTime {
        time_t utc_time;
        int    utc_time_usec;
    };

    struct GPSSpeed {
        float speedkmh;
        float accel[3];
        GPSTime time;
    };

    GPSStatus _getGPSStatus();
    GPSStatus _getSpeedKMH(GPSSpeed &speed);

    bool _loadScores();
    void _saveTestScore();
    void _saveTestScore(bool bImperial, int score_1_us, int score_2_us);
    void _addClipSceneData();

    void _recordSpeed(GPSSpeed &speed);
    bool _isMoveBegin(GPSSpeed &speed);

    void _updateWaiting();
    void _updateTesting();
    void _showTestSuccess();
    void _showFailedReason();

    void _initDisplay();

    CCameraProperties *_cp;

    struct aggps_client_info_s  *_paggps_client;

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    BmpImage        *_pAnimation;
    int             _cntAnim;

    StaticText      *_pHintInfo;
    char            _txtHint[32];
    StaticText      *_pWarning;

    FanRing         *_pRing;
    Arc             *_p30MphProgress;
    Arc             *_p60MphProgress;

    StaticText      *_pTimer;
    char            _txtTimer[16];

    StaticText      *_p30MPHTime;
    char            _txt30MPHTime[16];

    StaticText      *_pScoreTitle30Mph;
    StaticText      *_pScore30Mph;
    char            _score30Mph[16];
    BmpImage        *_p30MphBest;

    StaticText      *_pScoreTitle60Mph;
    StaticText      *_pScore60Mph;
    char            _score60Mph[16];
    BmpImage        *_p60MphBest;

    BmpImage        *_pIcnFailed;
    StaticText      *_pOK;

    int             _counter;
    TestStatus      _testStatus;
    FailedReason    _failedReason;

#define AUTO_HISTORY_RECORD_NUM      10
    float           _bestSpeed;
    GPSSpeed        _historySpeed[AUTO_HISTORY_RECORD_NUM];

    bool            _bImperialUnits;

    bool            _bMoveBegin;
    bool            _bExceed30Mph;
    bool            _bExceed50kmh;
    bool            _bExceed60Mph;
    bool            _bExceed100kmh;

    GPSTime         _begin_move_time;
    int             _usec_offset_30mph;
    int             _usec_offset_50kmh;
    int             _usec_offset_60mph;
    int             _usec_offset_100kmh;

    int             _best_score_1_ms;
    int             _best_score_2_ms;

    float           _gpsReliableIndex;
};

class ScoreFragment : public Fragment, public OnClickListener
{
public:
    typedef Fragment inherited;

    enum Mode {
        Mode_none = 0,
        Mode_CountDown,
        Mode_Auto,
    };

    ScoreFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren,
        int mode);
    virtual ~ScoreFragment();
    virtual void onResume();
    virtual void onPause();
    virtual void onClick(Control *widget);

private:
    void _storeFakeScores();
    bool _loadScores();
    void _showScoreScroller();
    void _destroyScoreScroller();

    int               _mode;

    Scroller          *_pScroller;
    Size              _visibleSizeScroller;

    StaticText        *_pTitle;
    UILine            *_pLine;

    StaticText        *_pScore_1;
    StaticText        *_pScore_2;

    StaticText        *_pBest_1[2];
    char              _txtBest_1[2][16];
    StaticText        *_pBest_2[2];
    char              _txtBest_2[2][16];
    int               _index_best_score_1;
    int               _index_best_score_2;

    BmpImage          *_p30MphBest;
    BmpImage          *_p60MphBest;

    StaticText        *_pScore_Latest;
    UILine            *_pLine_Latest;
    StaticText        *_pLatest_1[10];
    char              _txtLatest_1[10][16];
    StaticText        *_pLatest_2[10];
    char              _txtLatest_2[10][16];

    int               _num_latest;
    float             _scores[10][2];

    Button            *_pBtnClear;

    StaticText        *_pHintInfo;
    ImageButton       *_pIconYes;
    ImageButton       *_pIconNo;

    bool              _bImperialUnits;
};

class CountdownModeScore : public ScoreFragment
{
public:
    CountdownModeScore(const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren);
    virtual ~CountdownModeScore();

private:
};

class AutoModeScore : public ScoreFragment
{
public:
    AutoModeScore(const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren);
    virtual ~AutoModeScore();

private:
};

class TestScoreWnd : public Window, public OnClickListener, public ViewPagerListener
{
public:
    typedef Window inherited;
    TestScoreWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~TestScoreWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

    virtual void onFocusChanged(int indexFocus);
    virtual void onViewPortChanged(const Point &leftTop);

private:

    CCameraProperties *_cp;

    Panel             *_pPanel;

    Button            *_pReturnBtnFake;
    ImageButton       *_pReturnBtn;

    ViewPager         *_pViewPager;
    Fragment          *_pPages[2];
    UINT8             _numPages;

    TabIndicator      *_pIndicator;
    int               _indexFocus;
};

};

#endif
