
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#include "linux.h"
#include "agbox.h"
#include "aglib.h"

#include "GobalMacro.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "Widgets.h"
#include "class_camera_property.h"
#include "CameraAppl.h"
#include "AudioPlayer.h"

#include "class_gsensor_thread.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "ColorPalette.h"
#include "WindowRegistration.h"

#include "LapTimer.h"

extern bool bReleaseVersion;

extern bool is_no_write_record();

using namespace rapidjson;

namespace ui {

#define MAKE_FOURCC(a,b,c,d) \
    ( ((uint32_t)d) | ( ((uint32_t)c) << 8 ) | ( ((uint32_t)b) << 16 ) | ( ((uint32_t)a) << 24 ) )

// about 5meters:
#define ALTITUDE_ACCURACY               (0.00004)
#define LONGITUDE_ACCURACY              (0.00004)
// about 14meters:
#define ALTITUDE_ACCURACY_HIGH_SPEED    (0.0001)
#define LONGITUDE_ACCURACY_HIGH_SPEED   (0.0001)

#define AMPLIFY_TIMES                   (10000)

#define Score_File_Name                 "/castle/laptimer.score"

#define GPS_Reliable_Index_Loose        (2.5)
#define GPS_Reliable_Index_Strict       (2.0)

#define Sensor_WORK_MODE_NORMAL         (0x000C)
#define Sensor_WORK_MODE_PERFORMANCE    (0x0107)

extern int changeSensorMode(int opr_mode);

static float GetDistance(float fLati1, float fLong1, float fLati2, float fLong2)
{
    const float EARTH_RADIUS = 6378.137;
    double radLat1 = fLati1 * M_PI / 180.0;
    double radLat2 = fLati2 * M_PI / 180.0;
    double a = radLat1 - radLat2;
    double b = (fLong1 * M_PI / 180.0) - (fLong2 * M_PI / 180.0);
    float s = 2 * asin(sqrt(pow(sin(a/2), 2) + cos(radLat1) * cos(radLat2) * pow(sin(b/2), 2)));
    s = s * EARTH_RADIUS;
    s = (s * 10000000) / 10000;
    return s;
}

static bool _isOnCarMount()
{
    int ret_val = -1;
    char usbphy0_data[1024];
    char *sw_str;

    ret_val = aglib_read_file_buffer("/proc/ambarella/usbphy0",
        usbphy0_data, (sizeof(usbphy0_data) - 1));
    if (ret_val < 0) {
        CAMERALOG("read /proc/ambarella/usbphy0 failed with error %d", ret_val);
        return false;
    }
    usbphy0_data[ret_val] = 0;
    //CAMERALOG("%s", usbphy0_data);

    // MODE & STATUS
    char status[16];
    memset(status, 0x00, 16);
    char *begin = NULL;
    char *end = NULL;
    begin = strstr(usbphy0_data, "STATUS[");
    if (begin) {
        begin += strlen("STATUS[");
        end = strstr(begin, "]");
        memcpy(status, begin, (end - begin));
    }

    // VBUS
    int vbus = -1;
    sw_str = strstr(usbphy0_data, "VBUS[");
    if (sw_str) {
        vbus = strtoul((sw_str + 5), NULL, 0);
    }

    // GPIO
    int gpio = -1;
    sw_str = strstr(usbphy0_data, "GPIO[");
    if (sw_str) {
        gpio = strtoul((sw_str + 5), NULL, 0);
    }

    if (strcmp(status, "HOST") == 0) {
        if ((vbus == 1) && (gpio == 1)) {
            return true;
        }
    }

    return false;
}


LapScoreAdapter::LapScoreAdapter()
    : _pText_1(NULL)
    , _pText_2(NULL)
    , _num(0)
    , _bestIndex(-1)
    , _itemSize(240, 50)
    , _colorNormal(0x0000)
    , _colorPressed(0x8410)
    , _txt_1_Color(0xFFFF)
    , _txt_1_fontSize(24)
    , _txt_1_fontType(FONT_ROBOTOMONO_Medium)
    , _txt_1_hAlign(LEFT)
    , _txt_1_vAlign(MIDDLE)
    , _txt_2_Color(0xFFFF)
    , _txt_2_fontSize(24)
    , _txt_2_fontType(FONT_ROBOTOMONO_Medium)
    , _txt_2_hAlign(LEFT)
    , _txt_2_vAlign(MIDDLE)
    , _imageSize(0, 0)
    , _imagePath(NULL)
    , _imageName(NULL)
{
}

LapScoreAdapter::~LapScoreAdapter()
{
}

UINT16 LapScoreAdapter::getCount()
{
    return _num;
}

Control* LapScoreAdapter::getItem(UINT16 position, Container* pParent, const Size& size)
{
    if (position >= _num) {
        return NULL;
    }

    Container *item = new Container(pParent, pParent->GetOwner(), Point(0, 0), _itemSize, 5);
    if (!item) {
        CAMERALOG("Out of Memory!");
        return NULL;
    }

    StaticText *txt_1 = new StaticText(item, Point(0, 0), _txt_1_Size);
    if (!txt_1) {
        CAMERALOG("Out of Memory!");
    }
    txt_1->SetStyle(_txt_1_fontSize, _txt_1_fontType, _txt_1_Color, _txt_1_hAlign, _txt_1_vAlign);
    txt_1->SetText(_pText_1[position]);

    StaticText *txt_2 = new StaticText(item, Point(_txt_1_Size.width, 0), _txt_2_Size);
    if (!txt_2) {
        delete txt_1;
        CAMERALOG("Out of Memory!");
    }
    txt_2->SetStyle(_txt_2_fontSize, _txt_2_fontType, _txt_2_Color, _txt_2_hAlign, _txt_2_vAlign);
    txt_2->SetText(_pText_2[position]);

    if ((position == _bestIndex) && (_imagePath != NULL) && (_imageName != NULL)) {
        Point pos = txt_2->GetTxtEndPos();
        BmpImage *icon = new BmpImage(
            item,
            Point(pos.x + 4,
                (_itemSize.height - _imageSize.height) / 2),
            _imageSize,
            _imagePath,
            _imageName);
        if (!icon) {
            delete txt_2;
            delete txt_1;
            CAMERALOG("Out of Memory!");
        }
    }

    return item;
}

Control* LapScoreAdapter::getItem(UINT16 position, Container* pParent)
{
    if (position >= _num) {
        return NULL;
    }

    Container *item = new Container(pParent, pParent->GetOwner(), Point(0, 0), _itemSize, 5);
    if (!item) {
        CAMERALOG("Out of Memory!");
        return NULL;
    }

    StaticText *txt_1 = new StaticText(item, Point(0, 0), _txt_1_Size);
    if (!txt_1) {
        CAMERALOG("Out of Memory!");
    }
    txt_1->SetStyle(_txt_1_fontSize, _txt_1_fontType, _txt_1_Color, _txt_1_hAlign, _txt_1_vAlign);
    txt_1->SetText(_pText_1[position]);

    StaticText *txt_2 = new StaticText(item, Point(_txt_1_Size.width, 0), _txt_2_Size);
    if (!txt_2) {
        delete txt_1;
        CAMERALOG("Out of Memory!");
    }
    txt_2->SetStyle(_txt_2_fontSize, _txt_2_fontType, _txt_2_Color, _txt_2_hAlign, _txt_2_vAlign);
    txt_2->SetText(_pText_2[position]);

    if ((position == _bestIndex) && (_imagePath != NULL) && (_imageName != NULL)) {
        Point pos = txt_2->GetTxtEndPos();
        BmpImage *icon = new BmpImage(
            item,
            Point(pos.x + 4,
                (_itemSize.height - _imageSize.height) / 2),
            _imageSize,
            _imagePath,
            _imageName);
        if (!icon) {
            delete txt_2;
            delete txt_1;
            CAMERALOG("Out of Memory!");
        }
    }

    return item;
}

void LapScoreAdapter::setTextList(UINT8 **pText_1, UINT8 **pText_2, UINT16 num)
{
    _pText_1 = pText_1;
    _pText_2 = pText_2;
    _num = num;
}

void LapScoreAdapter::setItemStyle(const Size& size, UINT16 colorNormal, UINT16 colorPressed)
{
    _itemSize = size;
    _colorNormal = colorNormal;
    _colorPressed = colorPressed;
}

void LapScoreAdapter::setText_1_Style(const Size& size,
    UINT16 txtColor, UINT16 txtSize, UINT16 font,
    UINT8 hAlign, UINT8 vAlign)
{
    _txt_1_Size = size;
    _txt_1_Color = txtColor;
    _txt_1_fontSize = txtSize;
    _txt_1_fontType = font;
    _txt_1_hAlign = hAlign;
    _txt_1_vAlign = vAlign;
}

void LapScoreAdapter::setText_2_Style(const Size& size,
    UINT16 txtColor, UINT16 txtSize, UINT16 font,
    UINT8 hAlign, UINT8 vAlign)
{
    _txt_2_Size = size;
    _txt_2_Color = txtColor;
    _txt_2_fontSize = txtSize;
    _txt_2_fontType = font;
    _txt_2_hAlign = hAlign;
    _txt_2_vAlign = vAlign;
}

void LapScoreAdapter::setImageStyle(
    const Size& size,
    const char *path,
    const char *name)
{
    _imageSize = size;
    _imagePath = path;
    _imageName = name;
}


LapScoreWnd::LapScoreWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _indexes(NULL)
    , _scores(NULL)
    , _scoreNum(0)
    , _bestIndex(-1)
    , _pInfo(NULL)
    , _bRealtime(false)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 8);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 80),
        Color_Black, Color_Black);
    _pReturnBtnFake->SetStyle(0, 0.0);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    snprintf(_txtTitle, sizeof(_txtTitle), "Last Record");
    _pTitle = new StaticText(_pPanel, Point(0, 56), Size(400, 80));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)_txtTitle);

    _pLine = new UILine(_pPanel, Point(50, 136), Size(300, 4));
    _pLine->setLine(Point(50, 100), Point(350, 100), 4, Color_Grey_2);

    _pList = new List(_pPanel, this, Point(96, 148), Size(304, 252), 20);

    _pAdapter = new LapScoreAdapter();
    _pAdapter->setTextList((UINT8 **)_indexes, (UINT8 **)_scores, _scoreNum);
    _pAdapter->setItemStyle(Size(304, 50), Color_Black, Color_Grey_2);
    _pAdapter->setText_1_Style(Size(88, 50),
        Color_Grey_2, 26, FONT_ROBOTO_Regular, LEFT, MIDDLE);
    _pAdapter->setText_2_Style(Size(216, 50),
        Color_White, 26, FONT_ROBOTO_Regular, LEFT, MIDDLE);
    _pAdapter->setImageStyle(Size(32, 32),
        "/usr/local/share/ui/BMP/performance_test/",
        "icn-best.bmp");
    _pAdapter->setObserver(_pList);

    _pList->setAdaptor(_pAdapter);
}

LapScoreWnd::~LapScoreWnd()
{
    delete _pInfo;

    delete _pAdapter;
    delete _pList;

    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;

    delete _pPanel;

    for (int i = 0; i < _scoreNum; i++) {
        delete [] _indexes[i];
        delete [] _scores[i];
    }
    delete [] _indexes;
    delete [] _scores;
}

bool LapScoreWnd::_loadScores()
{
    bool ret = false;
    FILE *fp = fopen(Score_File_Name, "rt+");
    if (fp) {
        char score[256] = {0x00};

        // firstly read capture time
        if (fgets(score, 256, fp)) {
            if (!_bRealtime) {
                time_t tt = atol(score);
                struct tm gtime;
                gtime = (*localtime(&tt));
                snprintf(_txtTitle, sizeof(_txtTitle),
                    "Last Record\n%02d/%02d/%d %d:%02d %s",
                    1 + gtime.tm_mon, gtime.tm_mday, (1900 + gtime.tm_year) % 100,
                    gtime.tm_hour > 12 ? gtime.tm_hour - 12 : gtime.tm_hour,
                    gtime.tm_min,
                    gtime.tm_hour > 12 ? "PM" : "AM");
                CAMERALOG("_txtTitle = %s", _txtTitle);
            }
        }

        // secondly get num of scores
        _scoreNum = 0;
        while (fgets(score, 256, fp)) {
            _scoreNum++;
        }

        // lastly read the scores
        _indexes = new char* [_scoreNum];
        if (!_indexes) {
            CAMERALOG("Out of Memory!");
            _scoreNum = 0;
            return ret;
        }
        _scores = new char* [_scoreNum];
        if (!_scores) {
            CAMERALOG("Out of Memory!");
            delete _indexes;
            _scoreNum = 0;
            return ret;
        }

        fseek(fp, 0, SEEK_SET);
        fgets(score, 256, fp);
        int best = 0, indexBest = -1, index = 0;
        while (fgets(score, 256, fp) && (index <= _scoreNum)) {
            int time_ms  = atoi(score);
            if ((best == 0) || (time_ms < best)) {
                best = time_ms;
                indexBest = index;
            }

            _indexes[index] = new char[8];
            if (!_indexes[index]) {
                CAMERALOG("Out of Memory!");
                break;
            }
            _scores[index] = new char[16];
            if (!_scores[index]) {
                CAMERALOG("Out of Memory!");
                break;
            }

            snprintf(_indexes[index], 8, "%d", index + 1);
            if (time_ms / 1000 >= 3600) {
                snprintf(_scores[index], 16,
                    "%02d.%02d.%02d:%02d",
                    (time_ms / 1000) / 3600,
                    (time_ms / 1000) / 60,
                    time_ms / 1000,
                    (time_ms % 1000) / 10);
            } else if (time_ms / 1000 >= 60) {
                snprintf(_scores[index], 16,
                    "%02d.%02d:%02d",
                    (time_ms / 1000) / 60,
                    time_ms / 1000,
                    (time_ms % 1000) / 10);
            } else {
                snprintf(_scores[index], 16,
                    "0.%d:%02d", time_ms / 1000, (time_ms % 1000) / 10);
            }

            index++;
        }
        fclose(fp);
        _bestIndex = indexBest;
    }
/*
    for (int i = 0; i < _scoreNum; i++) {
        CAMERALOG("%s %s", _indexes[i], _scores[i]);
    }
*/
    return ret;
}

void LapScoreWnd::onResume()
{
    _loadScores();

    if (_scoreNum > 0) {
        _pAdapter->setTextList((UINT8 **)_indexes, (UINT8 **)_scores, _scoreNum);
        if (_scoreNum > 1) {
            CAMERALOG("_bestIndex = %d", _bestIndex);
            _pAdapter->setBestIndex(_bestIndex);
        } else {
            _pAdapter->setBestIndex(-1);
        }
        _pAdapter->notifyDataSetChanged();
    } else {
        _pInfo = new StaticText(_pPanel, Point(50, 200), Size(300, 50));
        _pInfo->SetStyle(32, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
        _pInfo->SetText((UINT8 *)"No record yet.");
    }
    _pPanel->Draw(true);
}

void LapScoreWnd::onPause()
{
}

void LapScoreWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

bool LapScoreWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                int x_pos = (position >> 16) & 0xFFFF;
                if ((y_pos < 132)
                    || (x_pos < 50) || (x_pos > 350))
                {
                    this->Close(Anim_Top2BottomExit);
                    b = false;
                }
            }
        case eventType_key:
            switch(event->_event)
            {
                case key_right_onKeyUp: {
                    if (_cp->isCarMode()) {
                        camera_State state = _cp->getRecState();
                        if ((camera_State_record == state)
                            || (camera_State_marking == state))
                        {
                            _cp->MarkVideo();
                        } else {
                            storage_State st = _cp->getStorageState();
                            if ((storage_State_noMedia == st) && !is_no_write_record()) {
                                this->StartWnd(WINDOW_cardmissing, Anim_Top2BottomEnter);
                            } else  {
                                _cp->OnRecKeyProcess();
                            }
                        }
                    } else {
                        storage_State st = _cp->getStorageState();
                        if ((storage_State_ready == st) ||
                            ((storage_State_full == st) && _cp->isCircleMode()))
                        {
                            _cp->OnRecKeyProcess();
                        } else if ((storage_State_noMedia == st) && !is_no_write_record()) {
                            this->StartWnd(WINDOW_cardmissing, Anim_Top2BottomEnter);
                        }
                    }
                    b = false;
                    break;
                }
            }
            break;
        case eventType_external:
            if ((event->_event == MountEvent_Plug_To_Car_Mount)
                && (event->_paload == 1))
            {
                camera_State state = _cp->getRecState();
                if ((camera_State_record != state)
                    && (camera_State_marking != state)
                    && (camera_State_starting != state))
                {
                    _cp->startRecMode();
                }

                b = false;
            }
            break;
    }

    return b;
}


LapRealtimeScoreWnd::LapRealtimeScoreWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
    : LapScoreWnd(cp, name, wndSize, leftTop)
{
    setRealtime(true);
    _pReturnBtn->setSource(
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
}

LapRealtimeScoreWnd::~LapRealtimeScoreWnd()
{
}

LaptimerWnd::LaptimerWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _paggps_client(NULL)
    , _bCheckingPos(false)
    , _minDistance(0.0)
    , _maxSpeedInLap(0.0)
    , _numLap(0)
    , _bestTimeMs(0)
    , _testStatus(Status_Not_On_CarMount)
    , _failedReason(Failed_Reason_None)
    , _gpsReliableIndex(GPS_Reliable_Index_Loose)
{
    memset(_historyPos, 0x00, sizeof(_historyPos));

    _paggps_client = aggps_open_client();

    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 30);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 80), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pTitle = new StaticText(_pPanel, Point(0, 56), Size(400, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"LapTimer");

    _pLine = new UILine(_pPanel, Point(100, 100), Size(200, 4));
    _pLine->setLine(Point(100, 100), Point(300, 100), 4, Color_Grey_2);

    _pAnimation = new BmpImage(
        _pPanel,
        Point(152, 152), Size(96, 96),
        "/usr/local/share/ui/BMP/performance_test/",
        "icn-GPS-searching-1.bmp");

    snprintf(_txtHint, 32, "Searching GPS...");
    _pHintInfo = new StaticText(_pPanel, Point(0, 272), Size(400, 80));
    _pHintInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, TOP);
    _pHintInfo->SetLineHeight(40);
    _pHintInfo->SetText((UINT8 *)_txtHint);
    _pHintInfo->SetHiden();

    _pWarning = new StaticText(_pPanel, Point(0, 312), Size(400, 40));
    _pWarning->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Red_1, CENTER, MIDDLE);
    _pWarning->SetText((UINT8 *)"Track Only");
    _pWarning->SetHiden();

    _pRecStatus = new BmpImage(
        _pPanel,
        Point(110, 126), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-tf_card.bmp");
    _pRecStatus->SetHiden();

    strcpy(_txt, "00:00:00");
    _pRecTxt = new StaticText(_pPanel, Point(136, 122), Size(200, 32));
    _pRecTxt->SetStyle(24, FONT_ROBOTOMONO_Medium, Color_White, LEFT, MIDDLE);
    _pRecTxt->SetText((UINT8 *)_txt);
    _pRecTxt->SetHiden();

    _pStart = new Button(_pPanel, Point(40, 180), Size(320, 80), Color_Black, Color_Grey_1);
    _pStart->SetStyle(40);
    _pStart->SetBorder(true, Color_White, 4);
    _pStart->SetText((UINT8 *)"Set Start Point", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pStart->SetTextAlign(CENTER, MIDDLE);
    _pStart->setOnClickListener(this);
    _pStart->SetHiden();

    _pStop = new ImageButton(_pPanel,
        Point(136, 248), Size(128, 128),
        "/usr/local/share/ui/BMP/performance_test/",
        "btn-lap_timer-stop-up.bmp",
        "btn-lap_timer-stop-down.bmp");
    _pStop->setOnClickListener(this);
    _pStop->SetHiden();

    _pCap = new RoundCap(_pPanel, Point(200, 200), 199, Color_Grey_2);
    _pCap->setAngle(0.0);
    _pCap->setRatio(0.5);
    _pCap->SetHiden();

    snprintf(_txtLapNum, sizeof(_txtLapNum), "Lap 0");
    _pLapNum = new StaticText(_pPanel, Point(50, 20), Size(300, 60));
    _pLapNum->SetStyle(48, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _pLapNum->SetText((UINT8 *)_txtLapNum);
    _pLapNum->SetHiden();

    snprintf(_txtTimer, sizeof(_txtTimer), "00:00.0");
    _pTimer = new StaticText(_pPanel, Point(0, 100), Size(400, 140));
    _pTimer->SetStyle(80, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _pTimer->SetText((UINT8 *)_txtTimer);
    _pTimer->SetHiden();

    //------------------------------------
    // flowing to show failed reason
    //------------------------------------
    _pRing = new FanRing(_pPanel, Point(200, 200),
        199, 167,
        Color_Red_1, Color_Black);
    _pRing->setAngles(0.0, 360.0);
    _pRing->SetHiden();

    _pIcnFailed = new BmpImage(
        _pPanel,
        Point(160, 96), Size(80, 80),
        "/usr/local/share/ui/BMP/dash_menu/",
        "feedback-bad.bmp");
    _pIcnFailed->SetHiden();

    _pOK = new Button(_pPanel, Point(140, 288), Size(120, 80), Color_Black, Color_Grey_1);
    _pOK->SetText((UINT8 *)"OK", 40, Color_White, FONT_ROBOTOMONO_Bold);
    _pOK->SetStyle(0, 0.0);
    _pOK->setOnClickListener(this);
    _pOK->SetHiden();
}

LaptimerWnd::~LaptimerWnd()
{
    delete _pOK;
    delete _pIcnFailed;
    delete _pRing;

    delete _pTimer;
    delete _pLapNum;
    delete _pCap;

    delete _pStop;
    delete _pStart;
    delete _pRecTxt;
    delete _pRecStatus;

    delete _pWarning;
    delete _pHintInfo;
    delete _pAnimation;

    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;

    if (_paggps_client) {
        aggps_close_client(_paggps_client);
    }
}

LaptimerWnd::GPSStatus LaptimerWnd::_getGPSStatus()
{
    gps_state state = _cp->getGPSState();
    if (gps_state_ready != state) {
        return GPS_Status_Not_Ready;
    }

    int ret_val = aggps_read_client_tmo(_paggps_client, 0);
    if (ret_val) {
        return GPS_Status_Not_Ready;
    }

    struct aggps_location_info_s *plocation = &_paggps_client->pgps_info->location;
    if (plocation->set & AGGPS_SET_DOP) {
        if (plocation->hdop >= _gpsReliableIndex) {
            return GPS_Status_Low_Accuracy;
        }
    } else {
        return GPS_Status_Not_Ready;
    }

    if (_paggps_client->pgps_info->devinfo.output_rate_div) {
        float update_rate = 1;
        update_rate = _paggps_client->pgps_info->devinfo.output_rate_mul;
        update_rate /= _paggps_client->pgps_info->devinfo.output_rate_div;
        if (update_rate < 10.0) {
            return GPS_Status_Low_Accuracy;
        }
    } else {
        return GPS_Status_Not_Ready;
    }

    return GPS_Status_Ready;
}

LaptimerWnd::GPSStatus LaptimerWnd::_getGPSPostion(GPSPosition &position)
{
    memset(&position, 0x00, sizeof(position));

    if (_paggps_client == NULL) {
        _paggps_client = aggps_open_client();
        if (_paggps_client == NULL) {
            //snprintf(_txtSpeed, 32, "GPS Open Failed");
            return GPS_Status_Not_Ready;
        }
    }

    int ret_val = aggps_read_client_tmo(_paggps_client, 0);
    if (ret_val) {
        //snprintf(_txtSpeed, 32, "GPS Read Failed");
        return GPS_Status_Not_Ready;
    }

    if (_paggps_client->pgps_info->devinfo.output_rate_div) {
        float update_rate = 1;
        update_rate = _paggps_client->pgps_info->devinfo.output_rate_mul;
        update_rate /= _paggps_client->pgps_info->devinfo.output_rate_div;
        if (update_rate < 10.0) {
            return GPS_Status_Low_Accuracy;
        }
    } else {
        return GPS_Status_Not_Ready;
    }

    struct aggps_location_info_s *plocation = &_paggps_client->pgps_info->location;
    if (plocation->set & AGGPS_SET_DOP) {
        if (plocation->hdop >= _gpsReliableIndex) {
            return GPS_Status_Low_Accuracy;
        }
    } else {
        return GPS_Status_Not_Ready;
    }

    if ((plocation->set & AGGPS_SET_LATLON) &&
        (plocation->set & AGGPS_SET_TIME) &&
        (plocation->set & AGGPS_SET_SPEED))
    {
        position.latitude = plocation->latitude;
        position.longitude = plocation->longitude;
        position.speed= plocation->speed;
        position.time.utc_time = plocation->utc_time;
        position.time.utc_time_usec = plocation->utc_time_usec;

        float euler_heading = 0.0, euler_roll = 0.0, euler_pitch = 0.0;
        if (_cp->getOrientation(euler_heading, euler_roll, euler_pitch)) {
            position.heading = euler_heading;
        }

        //CAMERALOG("#### latitude = %f, longitude = %f, speed = %fkm/h, euler_heading = %f",
        //    position.latitude, position.longitude, position.speed, position.heading);
        return GPS_Status_Ready;
    }

    return GPS_Status_Not_Ready;
}

bool LaptimerWnd::_updateRemainingTime()
{
    storage_State st = _cp->getStorageState();
    switch (st) {
        case storage_State_ready:
            {
                int totle = 0, marked = 0, spaceMB = 0;
                _cp->getSpaceInfoMB(totle, marked, spaceMB);
                int bitrateKBS = _cp->getBitrate() / 8 / 1000;
                if (bitrateKBS > 0) {
                    int time_sec = spaceMB * 1000 / bitrateKBS;
                    if (time_sec < 60) {
                        snprintf(_txt, 64, "%s", "<1min left");
                    } else {
                        snprintf(_txt, 64, "%02dh%02dm left",
                            time_sec / 3600, (time_sec % 3600) / 60);
                    }
                } else {
                    if (spaceMB > 1000) {
                        snprintf(_txt, 64, "%d.%dG left",
                            spaceMB / 1000, (spaceMB % 1000) / 100);
                    } else {
                        if (spaceMB >= 1) {
                            snprintf(_txt, 64, "%dM left", spaceMB);
                        } else {
                            snprintf(_txt, 64, "%s left", "<1MB");
                        }
                    }
                }
            }
            break;
        case storage_State_error:
            strcpy(_txt, "SD Error");
            break;
        case storage_State_prepare:
            strcpy(_txt, "SD Checking");
            break;
        case storage_State_noMedia:
            strcpy(_txt, "No card");
            _pRecStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-tf_card-missing.bmp");
            break;
        case storage_State_full:
            strcpy(_txt, "SD Full");
            break;
        case storage_State_unknown:
            strcpy(_txt, "Unknown SD");
            break;
        case storage_State_usb:
            strcpy(_txt, "USB Storage");
            break;
    }

    return true;
}

void LaptimerWnd::_setStartPos()
{
    // set start position to the manual recording live clip
    struct LapTimerSceneData {
        uint32_t fourcc;

        // bytes including itself and following members
        uint32_t data_size;

        // GPS position and start time
        double    latitude; // sizeof(double) = 8
        double    longitude;
        uint32_t  utc_time;
        uint32_t  utc_time_usec;

        // the heading of Euler angles
        float     euler_heading;
    };
    struct LapTimerSceneData scene;
    memset(&scene, 0x00, sizeof(scene));
    scene.fourcc = MAKE_FOURCC('L', 'A', 'P', 'T');
    scene.data_size = sizeof(scene) - sizeof(scene.fourcc);
    scene.latitude = _startPos.latitude;
    scene.longitude = _startPos.longitude;
    scene.utc_time = _startPos.time.utc_time;
    scene.utc_time_usec = _startPos.time.utc_time_usec;
    scene.euler_heading = _startPos.heading;
    CAMERALOG("########## sizeof(LapTimerSceneData) = %d, sizeof(double) = %d",
        sizeof(scene), sizeof(double));
    _cp->MarkVideo(10, -1, (const char*)&scene, sizeof(scene));
}

float LaptimerWnd::_getHeadingDiff(float cur_heading)
{
    // Euler heading scope is [0.0, 360.0]
    float diff = cur_heading - _startPos.heading;
    if (diff > 180.0) {
        diff = diff - 180.0;
    } else if (diff < -180.0) {
        diff= diff + 180.0;
    }

    diff = abs(diff);
    //CAMERALOG("Euler heading angle diff is %f (cur_heading = %f, _startPos.heading = %f)",
    //    diff, cur_heading, _startPos.heading);
    return diff;
}

bool LaptimerWnd::_isSameDirection()
{
    int num = 0;
    for (int i = 0; i < HISTORY_RECORD_NUM / 2; i++) {
        if (_getHeadingDiff(_historyPos[i].heading) < 45.0) {
            num++;
        }
    }

    bool bSame = false;
    if (num >= 3) {
        bSame =  true;
    }

    CAMERALOG("it is %s the same direction (cur_heading = %f, _startPos.heading = %f)",
        bSame ? "" : "not", _historyPos[0].heading, _startPos.heading);

    return bSame;
}

void LaptimerWnd::_initDisplay()
{
    _pOK->SetHiden();
    _pIcnFailed->SetHiden();
    _pRing->SetHiden();

    _pAnimation->SetHiden();
    _pHintInfo->SetHiden();

    _pWarning->SetHiden();
    _pRecStatus->SetHiden();
    _pRecTxt->SetHiden();
    _pStart->SetHiden();
    _pStop->SetHiden();

    _pCap->SetHiden();
    _pLapNum->SetHiden();
    _pTimer->SetHiden();

    _pReturnBtnFake->SetShow();
    _pReturnBtn->SetShow();
    _pTitle->SetShow();
    _pLine->SetShow();

    if (!_isOnCarMount()) {
        _testStatus = Status_Not_On_CarMount;
        snprintf(_txtHint, 32, "Check car mount\nconnection!");
        _pHintInfo->SetColor(Color_Grey_4);
        _pHintInfo->SetFont(FONT_ROBOTOMONO_Regular);
        _pHintInfo->SetRelativePos(Point(0, 210));
        _pHintInfo->SetShow();
        return;
    }

    _pHintInfo->SetColor(Color_Grey_4);
    _pHintInfo->SetFont(FONT_ROBOTOMONO_Regular);
    _pHintInfo->SetRelativePos(Point(0, 272));

    GPSStatus status = _getGPSStatus();
    if (GPS_Status_Not_Ready == status) {
        _testStatus = Status_GPS_Not_Ready;
        snprintf(_txtHint, 32, "Searching GPS...");
        _pHintInfo->SetShow();
        _pAnimation->setSource(
            "/usr/local/share/ui/BMP/performance_test/",
            "icn-GPS-searching-1.bmp");
        _pAnimation->SetShow();
    } else if (GPS_Status_Low_Accuracy == status) {
        _testStatus = Status_GPS_Low_Accuracy;
        snprintf(_txtHint, 32, "Improving GPS...");
        _pHintInfo->SetShow();
        _pAnimation->setSource(
            "/usr/local/share/ui/BMP/performance_test/",
            "icn-GPS-improving-1.bmp");
        _pAnimation->SetShow();
    } else {
        _testStatus = Status_Waiting_start;
        _pRecStatus->SetShow();
        _updateRemainingTime();
        _pRecTxt->SetShow();
        _pStart->SetShow();
        _pWarning->SetShow();
    }

    _cntAnim = 0;
}

void LaptimerWnd::_recordPosition(GPSPosition &pos)
{
    for (int i = HISTORY_RECORD_NUM - 1; i > 0; i--) {
        _historyPos[i].latitude = _historyPos[i - 1].latitude;
        _historyPos[i].longitude = _historyPos[i - 1].longitude;
        _historyPos[i].speed = _historyPos[i - 1].speed;
        _historyPos[i].time.utc_time = _historyPos[i - 1].time.utc_time;
        _historyPos[i].time.utc_time_usec = _historyPos[i - 1].time.utc_time_usec;
        _historyPos[i].heading = _historyPos[i - 1].heading;
    }
    _historyPos[0].latitude = pos.latitude;
    _historyPos[0].longitude = pos.longitude;
    _historyPos[0].speed = pos.speed;
    _historyPos[0].time.utc_time = pos.time.utc_time;
    _historyPos[0].time.utc_time_usec = pos.time.utc_time_usec;
    _historyPos[0].heading = pos.heading;
}

bool LaptimerWnd::_isMoveBegin(GPSPosition &pos)
{
    const float Threshold = 5.0;
    bool moved = false;
    if (pos.speed >= Threshold) {
        int num = 0;
        for (int i = 0; i < HISTORY_RECORD_NUM; i++) {
            if (_historyPos[i].speed >= Threshold) {
                num++;
            } else {
                break;
            }
        }
        // In case car stopped, but GPS still gives out some big random speed.
        if (num >= 2) {
            moved = true;
        } else {
            moved = false;
        }
    } else {
        moved = false;
    }

    return moved;
}

void LaptimerWnd::_waitingStart()
{
    GPSPosition cur_pos;
    GPSStatus status = _getGPSPostion(cur_pos);
    if (GPS_Status_Ready == status) {
        _recordPosition(cur_pos);
    } else {
        _testStatus = Status_GPS_Not_Ready;
    }
}

void LaptimerWnd::_waitingAccelerate()
{
    GPSPosition cur_pos;
    GPSStatus status = _getGPSPostion(cur_pos);
    if (GPS_Status_Ready == status) {
        if (_isMoveBegin(cur_pos)) {
            _startPos.latitude = cur_pos.latitude;
            _startPos.longitude = cur_pos.longitude;
            _startPos.speed = cur_pos.speed;
            _startPos.time.utc_time = cur_pos.time.utc_time;
            _startPos.time.utc_time_usec = cur_pos.time.utc_time_usec;
            _setStartPos();

            _pHintInfo->SetHiden();
            _pTimer->SetShow();
            _testStatus = Status_Testing;
            CAMERALOG("Start lap testing!");
            CAudioPlayer::getInstance()->playSoundEffect(
                CAudioPlayer::SE_060Test_Countdown_0);
        }
        _recordPosition(cur_pos);
    } else {
        _failedReason = (GPS_Status_Low_Accuracy == status)
            ? Failed_Reason_GPS_Low_Accuracy : Failed_Reason_GPS_Not_Ready;
        _testStatus = Status_Test_Failed;
    }
}

bool LaptimerWnd::_storeScore(int time_ms)
{
    bool ret = false;

    if (_numLap == 1) {
        int rt = remove(Score_File_Name);
        CAMERALOG("rt = remove = %d", rt);
        FILE *fp = fopen(Score_File_Name, "w+x");
        if (fp) {
            char score[128] = {0x00};

            // store the test date
            snprintf(score, sizeof(score), "%ld\n", _startPos.time.utc_time);
            fputs(score, fp);

            // store lap time
            snprintf(score, sizeof(score), "%d\n", time_ms);
            fputs(score, fp);

            fclose(fp);
            ret = true;
        }
    } else {
        FILE *fp = fopen(Score_File_Name, "rt+");
        if (fp) {
            // append lap time
            fseek(fp, 0, SEEK_END);
            char score[128] = {0x00};
            snprintf(score, sizeof(score), "%d\n", time_ms);
            fputs(score, fp);
            fclose(fp);
            ret = true;
        }
    }

    return ret;
}

void LaptimerWnd::_updateTesting()
{
    _cntAnim++;
    if ((_numLap >= 1) && (_cntAnim == 10)) {
        _pCap->setColor(Color_Grey_2);
        _pLapNum->SetColor(Color_White);
    }

    GPSPosition cur_pos;
    GPSStatus status = _getGPSPostion(cur_pos);
    if ((GPS_Status_Ready == status) ||
        (GPS_Status_Low_Accuracy == status))
    {
        _recordPosition(cur_pos);

        if (_maxSpeedInLap < cur_pos.speed) {
            _maxSpeedInLap = cur_pos.speed;
        }
        int time_past_ms = (cur_pos.time.utc_time - _startPos.time.utc_time) * 1000
            + (cur_pos.time.utc_time_usec - _startPos.time.utc_time_usec + 500) / 1000;
        snprintf(_txtTimer, sizeof(_txtTimer), "%02d:%02d.%02d",
            (time_past_ms / 1000) / 60, (time_past_ms / 1000) % 60,
            (time_past_ms % 1000) / 10);
        if ((time_past_ms > 10 * 1000) && (_maxSpeedInLap >= 5.0)/* && _isSameDirection()*/)
        {
            if ((fabs(cur_pos.latitude - _startPos.latitude) <= ALTITUDE_ACCURACY_HIGH_SPEED) &&
                (fabs(cur_pos.longitude - _startPos.longitude) <= LONGITUDE_ACCURACY_HIGH_SPEED))
            {
                float diff_lat = (cur_pos.latitude - _startPos.latitude) * AMPLIFY_TIMES;
                float diff_long = (cur_pos.longitude - _startPos.longitude) * AMPLIFY_TIMES;
                float distance = diff_lat * diff_lat + diff_long * diff_long;
                //CAMERALOG("#### _minDistance = %f, distance = %f", _minDistance, distance);
                if (!_bCheckingPos) {
                    _bCheckingPos = true;
                    _minDistance = distance;
                    _closestPos.latitude = cur_pos.latitude;
                    _closestPos.longitude = cur_pos.longitude;
                    _closestPos.time = cur_pos.time;
                    CAMERALOG("#### begin check closest position");
                } else {
                    if (_minDistance > distance) {
                        _minDistance = distance;
                        _closestPos.latitude = cur_pos.latitude;
                        _closestPos.longitude = cur_pos.longitude;
                        _closestPos.time = cur_pos.time;
                    }
                }
            } else {
                if (_bCheckingPos) {
                    _bCheckingPos = false;
                    CAudioPlayer::getInstance()->playSoundEffect(
                        CAudioPlayer::SE_060Test_Achieve_1);
                    time_past_ms = (_closestPos.time.utc_time - _startPos.time.utc_time) * 1000
                        + (_closestPos.time.utc_time_usec - _startPos.time.utc_time_usec + 500) / 1000;
                    CAMERALOG("finish one lap, _closestPos.latitude = %f(diff = %f), "
                        "_closestPos.longitude = %f(diff = %f), cost %dms, "
                        "the distance to start position is %fm",
                        _closestPos.latitude, _closestPos.latitude - _startPos.latitude,
                        _closestPos.longitude, _closestPos.longitude - _startPos.longitude,
                        time_past_ms,
                        GetDistance(_closestPos.latitude, _closestPos.longitude,
                            _startPos.latitude, _startPos.longitude));
                    if ((_bestTimeMs == 0) || (_bestTimeMs > time_past_ms)) {
                        _bestTimeMs = time_past_ms;
                    }
                    _numLap++;
                    if (_numLap >= 1) {
                        _pCap->setColor(Color_Yellow_1);
                        _pLapNum->SetColor(Color_Black);
                    }
                    _maxSpeedInLap = _closestPos.speed;
                    _minDistance = 0.0;
                    _startPos.time = _closestPos.time;
                    snprintf(_txtLapNum, sizeof(_txtLapNum), "Lap %d", _numLap);
                    CAMERALOG("start a new lap, _startPos.latitude = %f, _startPos.longitude = %f",
                        _startPos.latitude, _startPos.longitude);

                    _cntAnim = 0;
                    _storeScore(time_past_ms);
                }
            }
        } else {
            _bCheckingPos = false;
        }
    } else {
        // read gps data failed
        _failedReason = (GPS_Status_Low_Accuracy == status)
            ? Failed_Reason_GPS_Low_Accuracy : Failed_Reason_GPS_Not_Ready;
        _testStatus = Status_Test_Failed;
    }
}

void LaptimerWnd::_showTestSuccess()
{
}

void LaptimerWnd::_showFailedReason()
{
    _cp->stopMark();

    _pTimer->SetHiden();
    _pLapNum->SetHiden();
    _pCap->SetHiden();

    _pStop->SetHiden();
    _pStart->SetHiden();
    _pRecTxt->SetHiden();
    _pRecStatus->SetHiden();

    _pWarning->SetHiden();
    _pHintInfo->SetHiden();
    _pAnimation->SetHiden();

    _pLine->SetHiden();
    _pTitle->SetHiden();
    _pReturnBtn->SetHiden();
    _pReturnBtnFake->SetHiden();

    switch (_failedReason) {
        case Failed_Reason_Not_On_CarMount:
            snprintf(_txtHint, 32, "Check car mount\nconnection!");
            break;
        case Failed_Reason_GPS_Not_Ready:
            snprintf(_txtHint, 32, "GPS NOT READY");
            break;
        case Failed_Reason_GPS_Low_Accuracy:
            snprintf(_txtHint, 32, "GPS LOW ACCURACY");
            break;
        case Failed_Reason_Slowdown:
            snprintf(_txtHint, 32, "Slowdown");
            break;
        case Failed_Reason_Takes_Too_Long:
            snprintf(_txtHint, 32, "Timeout");
            break;
        case Failed_Reason_Bad_Test:
            snprintf(_txtHint, 32, "Bad Test");
            break;
        default:
            snprintf(_txtHint, 32, "unknown error");
            break;
    }
    _pHintInfo->SetColor(Color_Red_1);
    _pHintInfo->SetFont(FONT_ROBOTOMONO_Regular);
    _pHintInfo->SetRelativePos(Point(0, 210));
    _pHintInfo->SetShow();

    _pOK->SetShow();
    _pIcnFailed->SetShow();
    _pRing->SetShow();
}

void LaptimerWnd::_updateStartRec()
{
    GPSStatus status = _getGPSPostion(_startPos);
    if (GPS_Status_Ready == status)
    {
        CAMERALOG("start lap test, _startPos.latitude = %f, _startPos.longitude = %f",
            _startPos.latitude, _startPos.longitude);
        _pReturnBtnFake->SetHiden();
        _pReturnBtn->SetHiden();
        _pTitle->SetHiden();
        _pLine->SetHiden();
        _pRecStatus->SetHiden();
        _pRecTxt->SetHiden();
        _pStart->SetHiden();
        _pWarning->SetHiden();

        _pCap->setColor(Color_Grey_2);
        _pCap->SetShow();
        _pLapNum->SetColor(Color_White);
        _pLapNum ->SetShow();
        _pStop->SetShow();

        _numLap = 0;
        _bestTimeMs = 0;
        _maxSpeedInLap = 0.0;
        _bCheckingPos = false;
        _minDistance = 0.0;

        snprintf(_txtLapNum, sizeof(_txtLapNum), "Lap %d", _numLap);
        snprintf(_txtTimer, sizeof(_txtTimer), "00:00.0");

        if (_isMoveBegin(_startPos)) {
            _setStartPos();
            _pTimer->SetShow();
            _pHintInfo->SetHiden();
            _testStatus = Status_Testing;
            CAMERALOG("Start lap testing!");
            CAudioPlayer::getInstance()->playSoundEffect(
                CAudioPlayer::SE_060Test_Countdown_0);
        } else {
            _pHintInfo->SetRelativePos(Point(0, 160));
            _pHintInfo->SetColor(Color_White);
            _pHintInfo->SetShow();
            _pTimer->SetHiden();
            snprintf(_txtHint, 32, "Accelerate to Start");
            _pHintInfo->SetFont(FONT_ROBOTOMONO_Bold);
            _testStatus = Status_Waiting_accelerate;
        }
    } else {
        // GPS is not ready
        _failedReason = (GPS_Status_Low_Accuracy == status)
            ? Failed_Reason_GPS_Low_Accuracy : Failed_Reason_GPS_Not_Ready;
        _testStatus = Status_Test_Failed;
    }
}

void LaptimerWnd::onResume()
{
    char vBsp[32];
    memset(vBsp, 0x00, 32);
    agcmd_property_get(PropName_prebuild_bsp, vBsp, "V0B");
    if (strcasecmp(vBsp, "HACHI_V0C") == 0) {
        _gpsReliableIndex = GPS_Reliable_Index_Loose;
    } else {
        _gpsReliableIndex = GPS_Reliable_Index_Strict;
    }
    CAMERALOG("_gpsReliableIndex = %f", _gpsReliableIndex);

    camera_State state = _cp->getRecState();
    storage_State sts = _cp->getStorageState();
    if ((storage_State_ready == sts)
        && (camera_State_record != state)
        && (camera_State_marking != state)
        && (camera_State_starting != state))
    {
        _cp->OnRecKeyProcess();
    }

    if ((Status_Testing != _testStatus)
        && (Status_Test_Success != _testStatus)
        && (Status_Test_Show_Result != _testStatus)
        && (Status_Test_Failed != _testStatus))
    {
        _failedReason = Failed_Reason_None;
        _initDisplay();
        _pPanel->Draw(true);
    }

    changeSensorMode(Sensor_WORK_MODE_PERFORMANCE);
    _cp->enableAutoMark(false);

#if 0
    CAMERALOG("accuracy test:");
    CAMERALOG("    (%f, %f) -> (%f, %f) is %fm",
        31.191090, 121.601369, 31.191090 + ALTITUDE_ACCURACY, 121.601369 + LONGITUDE_ACCURACY,
        GetDistance(31.191090, 121.601369, 31.191090 + ALTITUDE_ACCURACY, 121.601369 + LONGITUDE_ACCURACY));
    CAMERALOG("    (%f, %f) -> (%f, %f) is %fm",
        31.191090, 121.601369, 31.191090 + ALTITUDE_ACCURACY_HIGH_SPEED, 121.601369 + LONGITUDE_ACCURACY_HIGH_SPEED,
        GetDistance(31.191090, 121.601369, 31.191090 + ALTITUDE_ACCURACY_HIGH_SPEED, 121.601369 + LONGITUDE_ACCURACY_HIGH_SPEED));
#endif
}

void LaptimerWnd::onPause()
{
    changeSensorMode(Sensor_WORK_MODE_NORMAL);
    _cp->enableAutoMark(true);
}

void LaptimerWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake) ||
        (widget == _pOK))
    {
        _cp->stopMark();
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pStart) {
        _updateStartRec();
        _pPanel->Draw(true);
    } else if (widget == _pStop) {
        _testStatus = Status_Test_Success;
        _cp->stopMark();
        _initDisplay();

        if (_numLap <= 0) {
            _pPanel->Draw(true);
        } else {
            this->StartWnd(WINDOW_lap_realtime_score, Anim_Bottom2TopCoverEnter);
        }
    }
}

bool LaptimerWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (Status_Not_On_CarMount == _testStatus) {
                    if (_isOnCarMount()) {
                        _testStatus = Status_GPS_Not_Ready;
                    }
                } else if ((Status_GPS_Not_Ready == _testStatus) ||
                    (Status_GPS_Low_Accuracy == _testStatus) ||
                    (Status_GPS_Ready == _testStatus))
                {
                    _pHintInfo->SetColor(Color_Grey_4);
                    _pHintInfo->SetFont(FONT_ROBOTOMONO_Regular);
                    _pHintInfo->SetRelativePos(Point(0, 272));
                    GPSStatus status = _getGPSStatus();
                    if (GPS_Status_Not_Ready == status) {
                        _testStatus = Status_GPS_Not_Ready;
                        snprintf(_txtHint, 32, "Searching GPS...");
                        _pHintInfo->SetShow();
                        if (_cntAnim % 3 == 0) {
                            char source[64];
                            snprintf(source, 64, "icn-GPS-searching-%d.bmp",
                                (_cntAnim / 3) % 3 + 1);
                            _pAnimation->setSource(
                                "/usr/local/share/ui/BMP/performance_test/",
                                source);
                        }
                        _pAnimation->SetShow();
                        _pRecStatus->SetHiden();
                        _pRecTxt->SetHiden();
                        _pStart->SetHiden();
                        _pWarning->SetHiden();
                        _cntAnim++;
                    } else if (GPS_Status_Low_Accuracy == status) {
                        _testStatus = Status_GPS_Not_Ready;
                        snprintf(_txtHint, 32, "Improving GPS...");
                        _pHintInfo->SetShow();
                        if (_cntAnim % 5 == 0) {
                            char source[64];
                            snprintf(source, 64, "icn-GPS-improving-%d.bmp",
                                (_cntAnim / 5) % 2 + 1);
                            _pAnimation->setSource(
                                "/usr/local/share/ui/BMP/performance_test/",
                                source);
                        }
                        _pAnimation->SetShow();
                        _pRecStatus->SetHiden();
                        _pRecTxt->SetHiden();
                        _pStart->SetHiden();
                        _pWarning->SetHiden();
                        _cntAnim++;
                    } else if (GPS_Status_Ready == status) {
                        _testStatus = Status_Waiting_start;
                        _pHintInfo->SetHiden();
                        _pAnimation->SetHiden();
                        _pRecStatus->SetShow();
                        _updateRemainingTime();
                        _pRecTxt->SetShow();
                        _pStart->SetShow();
                        _pWarning->SetShow();
                        _cntAnim = 0;
                    }
                } else if (Status_Waiting_start == _testStatus) {
                    if (_cntAnim++ % 10 == 0) {
                        _updateRemainingTime();
                    }
                    _waitingStart();
                } else if (Status_Waiting_accelerate == _testStatus) {
                    if (_cntAnim++ % 10 == 0) {
                        _updateRemainingTime();
                    }
                    _waitingAccelerate();
                    if (Status_Test_Failed == _testStatus) {
                        CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Alert);
                        _showFailedReason();
                    }
                } else if (Status_Testing == _testStatus) {
                    _updateTesting();
                    if (Status_Test_Failed == _testStatus) {
                        CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_060Test_Fail);
                        _showFailedReason();
                    }
                } else if (Status_Test_Success == _testStatus) {
                    _showTestSuccess();
                }
                _pPanel->Draw(true);
            }
            break;
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                int x_pos = (position >> 16) & 0xFFFF;
                if ((y_pos < 132)
                    || (x_pos < 50) || (x_pos > 350))
                {
                    _cp->stopMark();
                    this->Close(Anim_Top2BottomExit);
                    b = false;
                }
            }
            break;
        case eventType_key:
            switch(event->_event)
            {
                case key_right_onKeyUp: {
                    if ((Status_GPS_Ready == _testStatus) ||
                        (Status_Waiting_start == _testStatus)) {
                        _updateStartRec();
                        _pPanel->Draw(true);
                    }
                    b = false;
                    break;
                }
            }
            break;
    }

    return b;
}

};

