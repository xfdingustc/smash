#ifndef __Camera_LapTimer_h__
#define __Camera_LapTimer_h__

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

class LapScoreAdapter : public ListAdaptor
{
public:
    LapScoreAdapter();
    virtual ~LapScoreAdapter();
    virtual UINT16 getCount();
    virtual Control* getItem(UINT16 position, Container* pParent, const Size& size);
    virtual Control* getItem(UINT16 position, Container* pParent);
    virtual Size& getItemSize() { return _itemSize; }

    void setTextList(UINT8 **pText_1, UINT8 **pText_2, UINT16 num);
    void setItemStyle(const Size& size, UINT16 colorNormal, UINT16 colorPressed);
    void setText_1_Style(const Size& size,
        UINT16 txtColor, UINT16 txtSize, UINT16 font,
        UINT8 hAlign, UINT8 vAlign);
    void setText_2_Style(const Size& size,
        UINT16 txtColor, UINT16 txtSize, UINT16 font,
        UINT8 hAlign, UINT8 vAlign);
    void setImageStyle(const Size& size,
        const char *path,
        const char *name);

    void setBestIndex(int index) { _bestIndex = index; }

private:
    UINT8   **_pText_1;
    UINT8   **_pText_2;
    UINT16  _num;

    int     _bestIndex;

    Size    _itemSize;

    UINT16  _colorNormal;
    UINT16  _colorPressed;

    Size    _txt_1_Size;
    UINT16  _txt_1_Color;
    UINT16  _txt_1_fontSize;
    UINT16  _txt_1_fontType;
    UINT8   _txt_1_hAlign;
    UINT8   _txt_1_vAlign;

    Size    _txt_2_Size;
    UINT16  _txt_2_Color;
    UINT16  _txt_2_fontSize;
    UINT16  _txt_2_fontType;
    UINT8   _txt_2_hAlign;
    UINT8   _txt_2_vAlign;

    Size       _imageSize;
    const char *_imagePath;
    const char *_imageName;
};

class LapScoreWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    LapScoreWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~LapScoreWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

protected:
    void setRealtime(bool bRT) { _bRealtime = bRT; }

    bool _loadScores();

    CCameraProperties   *_cp;

    Panel               *_pPanel;

    Button              *_pReturnBtnFake;
    ImageButton         *_pReturnBtn;

    StaticText          *_pTitle;
    UILine              *_pLine;
    char                _txtTitle[64];

    List                *_pList;
    LapScoreAdapter     *_pAdapter;
    char                **_indexes;
    char                **_scores;
    int                 _scoreNum;
    int                 _bestIndex;

    StaticText          *_pInfo;

    bool                _bRealtime;
};

class LapRealtimeScoreWnd : public LapScoreWnd
{
public:
    LapRealtimeScoreWnd(
        CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~LapRealtimeScoreWnd();
};

class LaptimerWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    LaptimerWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~LaptimerWnd();

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
        Status_Waiting_start,
        Status_Waiting_accelerate,
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

    struct GPSPosition {
        double  latitude;
        double  longitude;
        float   speed;
        GPSTime time;
        float   heading; // the heading of Euler angles
    };

    GPSStatus _getGPSStatus();
    GPSStatus _getGPSPostion(GPSPosition &position);

    bool _updateRemainingTime();
    void _setStartPos();
    float _getHeadingDiff(float cur_heading);
    bool _isSameDirection();

    void _initDisplay();

    void _recordPosition(GPSPosition &pos);
    bool _isMoveBegin(GPSPosition &pos);

    void _waitingStart();
    void _waitingAccelerate();
    void _updateTesting();
    void _showTestSuccess();
    void _showFailedReason();
    void _updateStartRec();

    bool _storeScore(int time_ms);

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

    BmpImage        *_pRecStatus;
    StaticText      *_pRecTxt;
    char            _txt[64];

    Button          *_pStart;
    ImageButton     *_pStop;

    RoundCap        *_pCap;
    StaticText      *_pLapNum;
    char            _txtLapNum[16];

    StaticText      *_pTimer;
    char            _txtTimer[16];

    FanRing         *_pRing;
    BmpImage        *_pIcnFailed;
    Button          *_pOK;

#define HISTORY_RECORD_NUM      10
    GPSPosition     _historyPos[HISTORY_RECORD_NUM];

    GPSPosition     _startPos;
    bool            _bCheckingPos;
    float           _minDistance;
    GPSPosition     _closestPos;
    float           _maxSpeedInLap;
    int             _numLap;
    int             _bestTimeMs;

    TestStatus      _testStatus;
    FailedReason    _failedReason;

    float           _gpsReliableIndex;
};

};

#endif
