
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

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "ColorPalette.h"
#include "WindowRegistration.h"

#include "FastFurious.h"

extern bool bReleaseVersion;

using namespace rapidjson;

namespace ui {

#define MAKE_FOURCC(a,b,c,d) \
    ( ((uint32_t)d) | ( ((uint32_t)c) << 8 ) | ( ((uint32_t)b) << 16 ) | ( ((uint32_t)a) << 24 ) )

#define Radius_Inner         (128)
#define Radius_Step          (7)
#define Ring_Steps           (10)

#define ONE_MPH_TO_KMH       (1.609344)
#define SPEED_TEST_30MPH     (30 * ONE_MPH_TO_KMH) // about 48.28kmh
#define SPEED_TEST_50KMH     (50)

#define SPEED_TEST_60MPH     (60 * ONE_MPH_TO_KMH) // about 96.56kmh
#define SPEED_TEST_100KMH    (100)

#define SEC_TO_USEC          (1000000)
#define TIME_TOO_LONG        (60)

#define SCORE_ACCURACY       (1000.0)

#define BEFORE_SECONDS       (6)
#define AFTER_SECONDS_LONG   (6)
#define AFTER_SECONDS_SHORT  (4)

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

CountdownTestWnd::CountdownTestWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _paggps_client(NULL)
    , _cntAnim(0)
    , _counter(0)
    , _testStatus(Status_Not_On_CarMount)
    , _bestSpeed(0.0)
    , _bImperialUnits(false)
    , _bMoveBegin(false)
    , _bExceed30Mph(false)
    , _bExceed50kmh(false)
    , _bExceed60Mph(false)
    , _bExceed100kmh(false)
    , _usec_offset_begin_move(0)
    , _usec_offset_30mph(0)
    , _usec_offset_50kmh(0)
    , _usec_offset_60mph(0)
    , _usec_offset_100kmh(0)
    , _best_score_1_ms(0)
    , _best_score_2_ms(0)
{
    memset(_historySpeed, 0x00, sizeof(_historySpeed));

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
    _pTitle->SetText((UINT8 *)"Countdown Mode");

    _pLine = new UILine(_pPanel, Point(50, 100), Size(300, 4));
    _pLine->setLine(Point(50, 100), Point(350, 100), 4, Color_Grey_2);

    _pAnimation = new BmpImage(
        _pPanel,
        Point(152, 152), Size(96, 96),
        "/usr/local/share/ui/BMP/performance_test/",
        "icn-GPS-searching-1.bmp");

    _pStartFake = new Button(_pPanel, Point(0, 150), Size(400, 100), Color_Black, Color_Black);
    _pStartFake->setOnClickListener(this);
    _pStartFake->SetHiden();

    _pIcnRC = new BmpImage(
        _pPanel,
        Point(40, 160), Size(80, 80),
        "/usr/local/share/ui/BMP/dash_menu/",
        "icn-RC-menu.bmp");
    _pIcnRC->SetHiden();

    _pDash = new StaticText(_pPanel, Point(120, 170), Size(80, 60));
    _pDash->SetStyle(40, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pDash->SetText((UINT8 *)"/");
    _pDash->SetHiden();

    _pStart = new Button(_pPanel, Point(200, 170), Size(180, 60), Color_Black, Color_Grey_1);
    _pStart->SetStyle(30);
    _pStart->SetBorder(true, Color_White, 4);
    _pStart->SetText((UINT8 *)"START", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pStart->SetTextAlign(CENTER, MIDDLE);
    _pStart->setOnClickListener(this);
    _pStart->SetHiden();

    _pWarning = new StaticText(_pPanel, Point(0, 312), Size(400, 40));
    _pWarning->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Red_1, CENTER, MIDDLE);
    _pWarning->SetText((UINT8 *)"Track Only");
    _pWarning->SetHiden();

    _pRing = new FanRing(_pPanel, Point(200, 200),
        Radius_Inner + Radius_Step * Ring_Steps, Radius_Inner,
        Color_Red_1, Color_Black);
    _pRing->setAngles(0.0, 360.0);
    _pRing->SetHiden();

    snprintf(_txtTimer, 16, " ");
    _pTimer = new StaticText(_pPanel, Point(50, 120), Size(300, 160));
    _pTimer->SetStyle(100, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _pTimer->SetText((UINT8 *)_txtTimer);
    _pTimer->SetHiden();

    snprintf(_txt30MPHTime, 16, " ");
    _p30MPHTime = new StaticText(_pPanel, Point(160, 110), Size(80, 60));
    _p30MPHTime->SetStyle(40, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _p30MPHTime->SetText((UINT8 *)_txt30MPHTime);
    _p30MPHTime->SetHiden();

    _p30MphProgress = new Arc(_pPanel, Point(200, 200), 140, 16, Color_White);
    _p30MphProgress->setAngles(269.5, 270.0);
    _p30MphProgress->SetHiden();

    _p60MphProgress = new Arc(_pPanel, Point(200, 200), 160, 16, Color_White);
    _p60MphProgress->setAngles(269.5, 270.0);
    _p60MphProgress->SetHiden();

    _pScoreTitle30Mph = new StaticText(_pPanel, Point(0, 100), Size(200, 40));
    _pScoreTitle30Mph->SetStyle(32, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pScoreTitle30Mph->SetText((UINT8 *)"30mph");
    _pScoreTitle30Mph->SetHiden();

    snprintf(_score30Mph, 16, " ");
    _pScore30Mph = new StaticText(_pPanel, Point(0, 140), Size(200, 120));
    _pScore30Mph->SetStyle(56, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _pScore30Mph->SetText((UINT8 *)_score30Mph);
    _pScore30Mph->SetHiden();

    _p30MphBest = new BmpImage(
        _pPanel,
        Point(40, 260), Size(120, 32),
        "/usr/local/share/ui/BMP/performance_test/",
        "icn-best2.bmp");
    _p30MphBest->SetHiden();

    _pScoreTitle60Mph = new StaticText(_pPanel, Point(200, 100), Size(200, 40));
    _pScoreTitle60Mph->SetStyle(32, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pScoreTitle60Mph->SetText((UINT8 *)"60mph");
    _pScoreTitle60Mph->SetHiden();

    snprintf(_score60Mph, 16, " ");
    _pScore60Mph = new StaticText(_pPanel, Point(200, 140), Size(200, 120));
    _pScore60Mph->SetStyle(56, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _pScore60Mph->SetText((UINT8 *)_score60Mph);
    _pScore60Mph->SetHiden();

    _p60MphBest = new BmpImage(
        _pPanel,
        Point(240, 260), Size(120, 32),
        "/usr/local/share/ui/BMP/performance_test/",
        "icn-best2.bmp");
    _p60MphBest->SetHiden();

    snprintf(_txtHint, 32, "Searching GPS...");
    _pHintInfo = new StaticText(_pPanel, Point(0, 272), Size(400, 40));
    _pHintInfo->SetStyle(28, FONT_ROBOTOMONO_Light, Color_Grey_4, CENTER, MIDDLE);
    _pHintInfo->SetText((UINT8 *)_txtHint);
    _pHintInfo->SetHiden();

    _pOK = new StaticText(_pPanel, Point(170, 288), Size(60, 60));
    _pOK->SetStyle(40, FONT_ROBOTOMONO_Bold, Color_White, CENTER, MIDDLE);
    _pOK->SetText((UINT8 *)"OK");
    _pOK->SetHiden();
}

CountdownTestWnd::~CountdownTestWnd()
{
    delete _pOK;
    delete _pHintInfo;
    delete _pAnimation;

    delete _p60MphBest;
    delete _pScore60Mph;
    delete _pScoreTitle60Mph;
    delete _p30MphBest;
    delete _pScore30Mph;
    delete _pScoreTitle30Mph;

    delete _p60MphProgress;
    delete _p30MphProgress;
    delete _p30MPHTime;
    delete _pTimer;
    delete _pRing;

    delete _pIcnRC;
    delete _pDash;
    delete _pStart;
    delete _pStartFake;
    delete _pWarning;

    delete _pReturnBtnFake;
    delete _pReturnBtn;
    delete _pTitle;
    delete _pLine;

    delete _pPanel;

    if (_paggps_client) {
        aggps_close_client(_paggps_client);
    }
}

CountdownTestWnd::GPSStatus CountdownTestWnd::_getGPSStatus()
{
    gps_state state = _cp->getGPSState();
    if (gps_state_ready != state) {
        return GPS_Status_Not_Ready;
    }

    int ret_val = aggps_read_client_tmo(_paggps_client, 10);
    if (ret_val) {
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

CountdownTestWnd::GPSStatus CountdownTestWnd::_getSpeedKMH(GPSSpeed &speed)
{
    memset(&speed, 0x00, sizeof(speed));

    if (_paggps_client == NULL) {
        _paggps_client = aggps_open_client();
        if (_paggps_client == NULL) {
            //snprintf(_txtSpeed, 32, "GPS Open Failed");
            return GPS_Status_Not_Ready;
        }
    }

    int ret_val = aggps_read_client_tmo(_paggps_client, 10);
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
    if ((plocation->set & AGGPS_SET_SPEED) &&
        (plocation->set & AGGPS_SET_TIME) &&
        (plocation->set & AGGPS_SET_DOP))
    {
        speed.speedkmh = plocation->speed;
        speed.time.utc_time = plocation->utc_time;
        speed.time.utc_time_usec = plocation->utc_time_usec;
        //CAMERALOG("#### speed = %.2fKm/h(%.2fMPH), time = %ld.%06ds, accuracy = %f",
        //    speed.speedkmh, speed.speedkmh * 0.6213712,
        //    speed.time.utc_time, speed.time.utc_time_usec, plocation->accuracy);

        return GPS_Status_Ready;
    }

    return GPS_Status_Not_Ready;
}

bool CountdownTestWnd::_loadScores()
{
    char scores_json[256];
    if (_bImperialUnits) {
        agcmd_property_get(PropName_60MPH_Test_Score, scores_json, "");
    } else {
        agcmd_property_get(PropName_100KMH_Test_Score, scores_json, "");
    }

    Document d;
    d.Parse(scores_json);
    if(!d.IsObject()) {
        //CAMERALOG("Didn't get a JSON object!");
        return false;
    }

    char *score_1 = (char *)"30mph";
    char *score_2 = (char *)"60mph";
    if (!_bImperialUnits) {
        score_1 = (char *)"50kmh";
        score_2 = (char *)"100kmh";
    }

    int num_score = 0;
    if (d.HasMember(score_1)) {
        const Value &a = d[score_1];
        if (a.IsArray()) {
            num_score = a.Size();
            if (num_score <= 0) {
                return false;
            }
            for (SizeType i = 0; i < a.Size(); i++) {
                if ((_best_score_1_ms <= 0) || (_best_score_1_ms > a[i].GetInt()))
                {
                    _best_score_1_ms = a[i].GetInt();
                }
            }
        }
    }

    if (d.HasMember(score_2)) {
        const Value &a = d[score_2];
        if (a.IsArray()) {
            int num = a.Size();
            if (num <= 0) {
                return false;
            }
            if (num_score != num) {
                CAMERALOG("incorrect scores: num_score = %d, num = %d", num_score, num);
                return false;
            }
            for (SizeType i = 0; i < a.Size(); i++) {
                if ((_best_score_2_ms <= 0) || (_best_score_2_ms > a[i].GetInt()))
                {
                    _best_score_2_ms = a[i].GetInt();
                }
            }
        }
    }

    return true;
}

void CountdownTestWnd::_saveTestScore()
{
    if (_bExceed60Mph) {
        _saveTestScore(true, _usec_offset_30mph, _usec_offset_60mph);
    } else if (_bExceed30Mph) {
        _saveTestScore(true, _usec_offset_30mph, 0);
    }

    if (_bExceed100kmh) {
        _saveTestScore(false, _usec_offset_50kmh, _usec_offset_100kmh);
    } else if (_bExceed50kmh) {
        _saveTestScore(false, _usec_offset_50kmh, 0);
    }
}

void CountdownTestWnd::_saveTestScore(bool bImperial, int score_1_us, int score_2_us)
{
    if ((score_1_us < 0) || (score_1_us > 60 * SEC_TO_USEC) ||
        (score_2_us < 0) || (score_2_us > 60 * SEC_TO_USEC))
    {
        return;
    }

    // Firstly load the scores:
    char scores_json[256];
    if (bImperial) {
        agcmd_property_get(PropName_60MPH_Test_Score, scores_json, "none");
    } else {
        agcmd_property_get(PropName_100KMH_Test_Score, scores_json, "none");
    }

    int scores[10][2];
    int num_score = 0;
    if (strcasecmp(scores_json, "none") != 0) {
        Document d;
        d.Parse(scores_json);
        if(d.IsObject()) {
            char *index_score_1 = (char *)"30mph";
            char *index_score_2 = (char *)"60mph";
            if (!bImperial) {
                index_score_1 = (char *)"50kmh";
                index_score_2 = (char *)"100kmh";
            }
            if (d.HasMember(index_score_1)) {
                const Value &a = d[index_score_1];
                if (a.IsArray()) {
                    num_score = a.Size();
                    if (num_score <= 0) {
                        return;
                    }
                    for (SizeType i = 0; i < a.Size(); i++) {
                        scores[i][0] = a[i].GetInt();
                    }
                }
            }
            if (d.HasMember(index_score_2)) {
                const Value &a = d[index_score_2];
                if (a.IsArray()) {
                    int num = a.Size();
                    if (num <= 0) {
                        return;
                    }
                    if (num_score != num) {
                        CAMERALOG("incorrect scores: num_score = %d, num = %d",
                            num_score, num);
                        return;
                    }
                    for (SizeType i = 0; i < a.Size(); i++) {
                        scores[i][1] = a[i].GetInt();
                    }
                }
            }
        }
    }

    // Secondly update the score:
    if (num_score < 10) {
        num_score++;
    }
    for (int i = num_score; i > 0; i--)
    {
        scores[i][0] = scores[i - 1][0];
        scores[i][1] = scores[i - 1][1];
    }
    scores[0][0] = score_1_us / 1000;
    scores[0][1] = score_2_us / 1000;

    // Thirdly save the score:
    Document d_new_score;
    Document::AllocatorType& allocator = d_new_score.GetAllocator();
    d_new_score.SetObject();

    Value array_30mph(kArrayType);
    for (int i = 0; i < num_score; i++) {
        Value object(kObjectType);

        int sec_30mph = scores[i][0];
        object.SetInt(sec_30mph);

        array_30mph.PushBack(object, allocator);
    }
    if (bImperial) {
        d_new_score.AddMember("30mph", array_30mph, allocator);
    } else {
        d_new_score.AddMember("50kmh", array_30mph, allocator);
    }

    Value array_60mph(kArrayType);
    for (int i = 0; i < num_score; i++) {
        Value object(kObjectType);

        int sec_60mph = scores[i][1];
        object.SetInt(sec_60mph);

        array_60mph.PushBack(object, allocator);
    }
    if (bImperial) {
        d_new_score.AddMember("60mph", array_60mph, allocator);
    } else {
        d_new_score.AddMember("100kmh", array_60mph, allocator);
    }

    StringBuffer buffer;
    Writer<StringBuffer> write(buffer);
    d_new_score.Accept(write);
    const char *str = buffer.GetString();
    CAMERALOG("strlen %d: %s", strlen(str), str);

    if (bImperial) {
        agcmd_property_set(PropName_60MPH_Test_Score, str);
    } else {
        agcmd_property_set(PropName_100KMH_Test_Score, str);
    }
}

void CountdownTestWnd::_addClipSceneData()
{
    if (!_bExceed50kmh) {
        return;
    }

    if (_bExceed50kmh && _bExceed100kmh) {
        // mark the test, add clip description for it:
        struct countdown_60mph_test_desc {
            uint32_t fourcc;

            // bytes including itself and following members
            uint32_t data_size;

            // start time, which is the time count down to 0
            uint32_t utc_sec_start;
            uint32_t utc_usec_start;

            // move begin time
            uint32_t usec_offset_move_begin;

            // 30mph time
            uint32_t usec_offset_30mph;
            // 50kmh time
            uint32_t usec_offset_50kmh;

            // 60mph time
            uint32_t usec_offset_60mph;
            // 100kmh time
            uint32_t usec_offset_100kmh;
        };
        struct countdown_60mph_test_desc desc;
        memset(&desc, 0x00, sizeof(desc));
        desc.fourcc = MAKE_FOURCC('C', 'D', '6', 'T');

        desc.data_size = sizeof(desc) - sizeof(desc.fourcc);

        desc.utc_sec_start = _start_time.utc_time;
        desc.utc_usec_start = _start_time.utc_time_usec;
        struct tm loctime;
        localtime_r((time_t*)(&(desc.utc_sec_start)), &loctime);
        CAMERALOG("#### desc.utc_sec_start = %d, _start_time.utc_time = %ld",
            desc.utc_sec_start, _start_time.utc_time);
        char date_buffer[512];
        strftime(date_buffer, sizeof(date_buffer),
            "%Z %04Y/%02m/%02d %02H:%02M:%02S", &loctime);
        CAMERALOG("#### start time: %s.%01d",
            date_buffer, (desc.utc_usec_start + 500) * 10 / SEC_TO_USEC);

        desc.usec_offset_move_begin = _usec_offset_begin_move;
        CAMERALOG("#### move begin time offset = %dus", desc.usec_offset_move_begin);

        desc.usec_offset_30mph = _usec_offset_30mph;
        CAMERALOG("#### 30mph time offset = %dus", desc.usec_offset_30mph);
        desc.usec_offset_50kmh = _usec_offset_50kmh;
        CAMERALOG("#### 50kmh time offset = %dus", desc.usec_offset_50kmh);

        desc.usec_offset_60mph = _usec_offset_60mph;
        CAMERALOG("#### 60mph time offset = %dus", desc.usec_offset_60mph);
        desc.usec_offset_100kmh = _usec_offset_100kmh;
        CAMERALOG("#### 100kmh time offset = %dus", desc.usec_offset_100kmh);

        CAMERALOG("#### countdown test finished with 60mph tested, mark the video [-%d, 5],"
            " sizeof(desc) = %d, desc.data_size = %d",
            desc.usec_offset_100kmh / SEC_TO_USEC + BEFORE_SECONDS,
            sizeof(desc), desc.data_size);
        _cp->MarkVideo(desc.usec_offset_100kmh / SEC_TO_USEC + BEFORE_SECONDS,
            AFTER_SECONDS_LONG,
            (const char*)&desc, sizeof(desc));
    }else if (_bExceed50kmh) {
        // mark the test, add clip description for it:
        struct countdown_30mph_test_desc {
            uint32_t fourcc;

            // bytes including itself and following members
            uint32_t data_size;

            // start time, which is the time count down to 0
            uint32_t utc_sec_start;
            uint32_t utc_usec_start;

            // move begin time
            uint32_t usec_offset_move_begin;

            // 30mph time
            uint32_t usec_offset_30mph;
            // 50kmh time
            uint32_t usec_offset_50kmh;
        };
        struct countdown_30mph_test_desc desc;
        memset(&desc, 0x00, sizeof(desc));
        desc.fourcc = MAKE_FOURCC('C', 'D', '3', 'T');

        desc.data_size = sizeof(desc) - sizeof(desc.fourcc);

        desc.utc_sec_start = _start_time.utc_time;
        desc.utc_usec_start = _start_time.utc_time_usec;
        struct tm loctime;
        localtime_r((time_t*)(&(desc.utc_sec_start)), &loctime);
        CAMERALOG("#### desc.utc_sec_start = %d, _start_time.utc_time = %ld",
            desc.utc_sec_start, _start_time.utc_time);
        char date_buffer[512];
        strftime(date_buffer, sizeof(date_buffer),
            "%Z %04Y/%02m/%02d %02H:%02M:%02S", &loctime);
        CAMERALOG("#### start time: %s.%d",
            date_buffer, (desc.utc_usec_start + 500) / SEC_TO_USEC);

        desc.usec_offset_move_begin = _usec_offset_begin_move;
        CAMERALOG("#### move begin time offset = %dus", desc.usec_offset_move_begin);

        desc.usec_offset_30mph = _usec_offset_30mph;
        CAMERALOG("#### 30mph time offset = %dus", desc.usec_offset_30mph);
        desc.usec_offset_50kmh = _usec_offset_50kmh;
        CAMERALOG("#### 50kmh time offset = %dus", desc.usec_offset_50kmh);

        int time_past_ms = (_historySpeed[0].time.utc_time - _start_time.utc_time) * 1000
            + (_historySpeed[0].time.utc_time_usec - _start_time.utc_time_usec) / 1000;
        CAMERALOG("#### countdown test finished with 30mph tested, mark the video [-%d, 5],"
            " sizeof(desc) = %d, desc.data_size = %d",
            time_past_ms / 1000 + BEFORE_SECONDS, sizeof(desc), desc.data_size);
        _cp->MarkVideo(time_past_ms / 1000 + BEFORE_SECONDS,
            AFTER_SECONDS_SHORT, (const char*)&desc, sizeof(desc));
    }
}

void CountdownTestWnd::_recordSpeed(GPSSpeed &speed)
{
    for (int i = CD_HISTORY_RECORD_NUM - 1; i > 0; i--) {
        _historySpeed[i].speedkmh = _historySpeed[i - 1].speedkmh;
        _historySpeed[i].accel[0] = _historySpeed[i - 1].accel[0];
        _historySpeed[i].accel[1] = _historySpeed[i - 1].accel[1];
        _historySpeed[i].accel[2] = _historySpeed[i - 1].accel[2];
        _historySpeed[i].time.utc_time = _historySpeed[i - 1].time.utc_time;
        _historySpeed[i].time.utc_time_usec = _historySpeed[i - 1].time.utc_time_usec;
    }
    _historySpeed[0].speedkmh = speed.speedkmh;
    _historySpeed[0].accel[0] = speed.accel[0];
    _historySpeed[0].accel[1] = speed.accel[1];
    _historySpeed[0].accel[2] = speed.accel[2];
    _historySpeed[0].time.utc_time = speed.time.utc_time;
    _historySpeed[0].time.utc_time_usec = speed.time.utc_time_usec;

    if (speed.speedkmh > _bestSpeed) {
        _bestSpeed = speed.speedkmh;
    }
}

bool CountdownTestWnd::_isMoveBegin(GPSSpeed &speed)
{
    const float Accel_Threshold = 0.1;

    if ((speed.speedkmh > 5.0) &&
        (_historySpeed[0].speedkmh > 0.0) &&
        (speed.speedkmh > _historySpeed[0].speedkmh))
    {
        if (bReleaseVersion) {
            for (int i = 0; i < CD_HISTORY_RECORD_NUM; i++) {
                CAMERALOG("#### _historySpeed[%d].speed = %f, "
                    "accel_x = %f, accel_y = %f, accel_z = %f, accel = %f",
                    i, _historySpeed[i].speedkmh,
                    _historySpeed[i].accel[0], _historySpeed[i].accel[1],
                    _historySpeed[i].accel[2],
                    sqrt(_historySpeed[i].accel[0] * _historySpeed[i].accel[0] +
                        _historySpeed[i].accel[1] * _historySpeed[i].accel[1] +
                        _historySpeed[i].accel[2] * _historySpeed[i].accel[2]));
            }
        }

        // check whether speed matched:
        int index_speed = 0;
        int i = 0;
        for (i = 0; i < CD_HISTORY_RECORD_NUM - 1; i++) {
            if ((_historySpeed[i].speedkmh <= 1.0) ||
                (_historySpeed[i + 1].speedkmh <= 1.0))
            {
                index_speed = i;
                break;
            } else if ((_historySpeed[i].speedkmh + 0.2 <= _historySpeed[i + 1].speedkmh)
                && (_historySpeed[i + 1].speedkmh < 5.0))
            {
                index_speed = i;
                break;
            }
        }
        if (_historySpeed[i].speedkmh <= 3.0) {
            index_speed = i;
        }

        // check whether acceleration matched:
        for (i = 0; i < CD_HISTORY_RECORD_NUM - 1; i++) {
            CAMERALOG("_historySpeed[%d].accel[2] = %f, Accel_Threshold = %f",
                i, _historySpeed[i].accel[2], Accel_Threshold);
            if (_historySpeed[i].accel[2] < Accel_Threshold) {
                CAMERALOG("index_accel = %d", i);
                break;
            }
        }
        int index_accel = i;

        // If speed and acceleration matched, find start position:
        if ((index_speed > 1) && (index_accel >= 3)) {
            CAMERALOG("#### speed = %f, index_speed = %d, index_accel = %d",
                speed.speedkmh, index_speed, index_accel);
            _bMoveBegin = true;
            _usec_offset_begin_move =
                (_historySpeed[index_speed].time.utc_time - _start_time.utc_time) * SEC_TO_USEC
                + (_historySpeed[index_speed].time.utc_time_usec - _start_time.utc_time_usec);
            CAMERALOG("################### speed = %f, index_speed = %d, "
                "index_accel = %d, move begin at %dms",
                speed.speedkmh, index_speed, index_accel, _usec_offset_begin_move / 1000);
            return true;
        } else {
            CAMERALOG("#### speed = %f, index_speed = %d, index_accel = %d, "
                "looks like it doesn't stop yet.",
                speed.speedkmh, index_speed, index_accel);
            return false;
        }
    } else {
        return false;
    }
}

void CountdownTestWnd::_updateCountdown()
{
    GPSSpeed speed;
    GPSStatus status = _getSpeedKMH(speed);
    if (GPS_Status_Ready != status) {
        _failedReason = (GPS_Status_Low_Accuracy == status)
            ? Failed_Reason_GPS_Low_Accuracy : Failed_Reason_GPS_Not_Ready;
        _testStatus = Status_Test_Failed;
        return;
    }

    if (!_cp->getAccelXYZ(speed.accel[0], speed.accel[1], speed.accel[2]))
    {
        CAMERALOG("#### getAccelXYZ failed!");
    }

    if (_counter > 5) {
        // If car begin to move during count down, the test fails.
        if (speed.speedkmh > 3) {
            _failedReason = Failed_Reason_False_Start;
            _testStatus = Status_Test_Failed;
            return;
        }

        _counter--;
        if ((_counter - 5) % 10 == 0) {
            snprintf(_txtTimer, 16, "%d",  (_counter - 5) / 10);
            if (((_counter - 5) > 0) && ((_counter - 5) <= 30)) {
                CAudioPlayer::getInstance()->playSoundEffect(
                    CAudioPlayer::SE_060Test_Countdown);
            }
        }
        _pRing->setRadius(
            Radius_Inner + ((_counter - 5) % Ring_Steps) * Radius_Step,
            Radius_Inner);
        if (_counter - 5 > 30) {
            _pRing->setColor(Color_Red_1, Color_Black);
        } else if (_counter - 5 >= 0) {
            _pRing->setColor(Color_Yellow_1, Color_Black);
        }
        if (_counter == 7) {
            CAudioPlayer::getInstance()->playSoundEffect(
                CAudioPlayer::SE_060Test_Countdown_0);
        } else if (_counter <= 5) {
            _pRing->setColor(Color_Green_1, Color_Black);
            _pRing->setRadius(Radius_Inner + Radius_Step * Ring_Steps, Radius_Inner);
            snprintf(_txtTimer, 16, "GO");

            // record the start time:
            _start_time.utc_time = speed.time.utc_time;
            _start_time.utc_time_usec = speed.time.utc_time_usec;
            struct tm loctime;
            localtime_r(&(_start_time.utc_time), &loctime);
            char date_buffer[512];
            strftime(date_buffer, sizeof(date_buffer),
                "%Z %04Y/%02m/%02d %02H:%02M:%02S", &loctime);
            CAMERALOG("#### start time: %s", date_buffer);
        }
    } else if (_counter > 0) {
        if (!_bMoveBegin) {
            _isMoveBegin(speed);
        }

        _counter--;
        if (_counter <= 0) {
            _counter = 0;
            _pRing->SetHiden();
            _p30MphProgress->SetShow();
            _p60MphProgress->SetShow();
            snprintf(_txtTimer, 16, "0.5");
            _testStatus = Status_Testing;
        }
    }

    _recordSpeed(speed);
}

void CountdownTestWnd::_updateTesting()
{
    GPSSpeed speed;
    GPSStatus status = _getSpeedKMH(speed);
    if (GPS_Status_Ready != status) {
        _failedReason = (GPS_Status_Low_Accuracy == status)
            ? Failed_Reason_GPS_Low_Accuracy : Failed_Reason_GPS_Not_Ready;
        _testStatus = Status_Test_Failed;
        return;
    }

    if (!_cp->getAccelXYZ(speed.accel[0], speed.accel[1], speed.accel[2]))
    {
        CAMERALOG("#### getAccelXYZ failed!");
    }

    int time_past_ms = (speed.time.utc_time - _start_time.utc_time) * 1000
        + (speed.time.utc_time_usec - _start_time.utc_time_usec + 500) / 1000;
    snprintf(_txtTimer, 16, "%d.%d", time_past_ms / 1000, (time_past_ms % 1000) / 100);
    if (!_bMoveBegin && (time_past_ms >= 5 * 1000)) {
        // If car doesn't start test after 5 seconds, the test fails.
        _failedReason = Failed_Reason_False_Start;
        _testStatus = Status_Test_Failed;
        return;
    }
    if (time_past_ms >= TIME_TOO_LONG * 1000) {
        // Takes too long time (more than 1min), fails.
        CAMERALOG("#### now = %ld.%ds, _start_time = %ld.%ds, time_past_ms = %d, "
            "test failed becaust it takes too long",
            speed.time.utc_time, speed.time.utc_time_usec,
            _start_time.utc_time, _start_time.utc_time_usec, time_past_ms);
        _failedReason = Failed_Reason_Takes_Too_Long;
        _testStatus = Status_Test_Failed;
        return;
    }

    // record the start time and start position:
    if (!_bMoveBegin) {
        _isMoveBegin(speed);
    }

    // check speed, if speed decrease, test finished
    if (((_bestSpeed > 0.0) && (speed.speedkmh + 5.0 < _bestSpeed))
        || (speed.speedkmh + 1.0 < _historySpeed[0].speedkmh))
    {
        if (_bExceed50kmh) {
            _testStatus = Status_Test_Success;
            _addClipSceneData();
            _counter = 10;
        } else {
            _failedReason = Failed_Reason_Slowdown;
            _testStatus = Status_Test_Failed;
        }
        return;
    } else {
        _recordSpeed(speed);
    }

    // update speed test display:
    if (_bMoveBegin) {
        if (!_bExceed50kmh) {
            if (!_bExceed30Mph && (speed.speedkmh >= SPEED_TEST_30MPH)) {
                _bExceed30Mph = true;
                _usec_offset_30mph =
                    (speed.time.utc_time - _start_time.utc_time) * SEC_TO_USEC
                    + (speed.time.utc_time_usec - _start_time.utc_time_usec);
                if ((speed.speedkmh - _historySpeed[1].speedkmh) != 0) {
                    int offset_us = 0;
                    offset_us = (speed.speedkmh - SPEED_TEST_30MPH) /
                        (speed.speedkmh - _historySpeed[1].speedkmh)
                        * 0.1 * SEC_TO_USEC;
                    CAMERALOG("#### speed.speedkmh = %f, _historySpeed[1].speedkmh = %f, "
                        "offset_us = %d",
                        speed.speedkmh, _historySpeed[1].speedkmh, offset_us);
                    _usec_offset_30mph = _usec_offset_30mph - offset_us;
                }
                CAMERALOG("############# 30mph tested, speed.speedkmh = %f, cost %dus",
                    speed.speedkmh, _usec_offset_30mph);
                if (_usec_offset_30mph <= 0) {
                    _failedReason = Failed_Reason_Bad_Test;
                    _testStatus = Status_Test_Failed;
                    return;
                }

                if (_bImperialUnits) {
                    int time_30mph_ms = _usec_offset_30mph / 1000;
                    _p30MphProgress->setAngles(270.0, 270.0);
                    _p30MphProgress->setColor(Color_Green_1);
                    snprintf(_txt30MPHTime, 16, "%d.%02d",
                        time_30mph_ms / 1000, (time_30mph_ms % 1000) / 10);
                    _p30MPHTime->SetShow();
                    CAudioPlayer::getInstance()->playSoundEffect(
                        CAudioPlayer::SE_060Test_Achieve_1);
                }
            }
            if (speed.speedkmh >= SPEED_TEST_50KMH) {
                _bExceed50kmh = true;
                // record scores:
                _usec_offset_50kmh =
                    (speed.time.utc_time - _start_time.utc_time) * SEC_TO_USEC
                    + (speed.time.utc_time_usec - _start_time.utc_time_usec);
                if ((speed.speedkmh - _historySpeed[1].speedkmh) != 0) {
                    int offset_us = 0;
                    offset_us = (speed.speedkmh - SPEED_TEST_50KMH) /
                        (speed.speedkmh - _historySpeed[1].speedkmh)
                        * 0.1 * SEC_TO_USEC;
                    CAMERALOG("#### speed.speedkmh = %f, _historySpeed[1].speedkmh = %f, "
                        "offset_us = %d",
                        speed.speedkmh, _historySpeed[1].speedkmh, offset_us);
                    _usec_offset_50kmh = _usec_offset_50kmh - offset_us;
                }
                CAMERALOG("############# 50kmh tested, speed.speedkmh = %f, , cost %dus",
                    speed.speedkmh, _usec_offset_50kmh);
                if (_usec_offset_50kmh <= 0) {
                    _failedReason = Failed_Reason_Bad_Test;
                    _testStatus = Status_Test_Failed;
                    return;
                }

                // update display:
                if (!_bImperialUnits) {
                    int time_50kmh_ms = _usec_offset_50kmh / 1000;
                    _p30MphProgress->setAngles(270.0, 270.0);
                    _p30MphProgress->setColor(Color_Green_1);
                    snprintf(_txt30MPHTime, 16, "%d.%02d",
                        time_50kmh_ms / 1000, (time_50kmh_ms % 1000) / 10);
                    _p30MPHTime->SetShow();
                    CAudioPlayer::getInstance()->playSoundEffect(
                        CAudioPlayer::SE_060Test_Achieve_1);
                }
            } else {
                _p30MphProgress->setColor(Color_White);
                float angle_speed = 360.0 * speed.speedkmh /
                    (_bImperialUnits ? SPEED_TEST_30MPH : SPEED_TEST_50KMH);
                angle_speed = angle_speed + 270;
                if (angle_speed > 360.0) {
                    angle_speed = angle_speed - 360.0;
                    _p30MphProgress->setColor(Color_Green_1);
                }
                _p30MphProgress->setAngles(270.0, angle_speed);
            }
        }

        if (!_bExceed100kmh) {
            if (!_bExceed60Mph && (speed.speedkmh >= SPEED_TEST_60MPH)) {
                _bExceed60Mph = true;
                _usec_offset_60mph =
                    (speed.time.utc_time - _start_time.utc_time) * SEC_TO_USEC
                    + (speed.time.utc_time_usec - _start_time.utc_time_usec);
                if ((speed.speedkmh - _historySpeed[1].speedkmh) != 0) {
                    int offset_us = 0;
                    offset_us = (speed.speedkmh - SPEED_TEST_60MPH) /
                        (speed.speedkmh - _historySpeed[1].speedkmh)
                        * 0.1 * SEC_TO_USEC;
                    _usec_offset_60mph = _usec_offset_60mph - offset_us;
                }
                CAMERALOG("############# 60mph tested, speed.speedkmh = %f, cost %dus",
                    speed.speedkmh, _usec_offset_60mph);
                if (_usec_offset_60mph <= 0) {
                    _failedReason = Failed_Reason_Bad_Test;
                    _testStatus = Status_Test_Failed;
                    return;
                }
            }
            if (speed.speedkmh >= SPEED_TEST_100KMH) {
                _bExceed100kmh = true;
                // record scores:
                _usec_offset_100kmh =
                    (speed.time.utc_time - _start_time.utc_time) * SEC_TO_USEC
                    + (speed.time.utc_time_usec - _start_time.utc_time_usec);
                if ((speed.speedkmh - _historySpeed[1].speedkmh) != 0) {
                    int offset_us = 0;
                    offset_us = (speed.speedkmh - SPEED_TEST_100KMH) /
                        (speed.speedkmh - _historySpeed[1].speedkmh)
                        * 0.1 * SEC_TO_USEC;
                    _usec_offset_100kmh = _usec_offset_100kmh - offset_us;
                }
                CAMERALOG("############# 100kmh tested, speed.speedkmh = %f, cost %dus",
                    speed.speedkmh, _usec_offset_100kmh);
                if (_usec_offset_100kmh <= 0) {
                    _failedReason = Failed_Reason_Bad_Test;
                    _testStatus = Status_Test_Failed;
                    return;
                }

                _p30MphProgress->setColor(Color_White);
                _p60MphProgress->setAngles(270.0, 270.0);
                _p60MphProgress->setColor(Color_White);
                _pRing->setRadius(Radius_Inner + Radius_Step * Ring_Steps, 0);
                _pRing->setColor(Color_Green_1, Color_Black);
                _pRing->SetShow();

                _testStatus = Status_Test_Success;
                CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_060Test_Achieve_2);
                _addClipSceneData();
                _counter = 10;
            } else {
                float angle_speed = 360.0 * speed.speedkmh /
                    (_bImperialUnits ? SPEED_TEST_60MPH : SPEED_TEST_100KMH);
                angle_speed = angle_speed + 270;
                if (angle_speed > 360.0) {
                    angle_speed = angle_speed - 360.0;
                }
                _p60MphProgress->setColor(Color_White);
                _p60MphProgress->setAngles(270.0, angle_speed);
            }
        }
    }
}

void CountdownTestWnd::_showTestSuccess()
{
    if (_counter > 0) {
        _counter--;
        if (_bExceed100kmh && (_counter == 6)) {
            _pRing->SetHiden();
            _p30MphProgress->setColor(Color_Green_1);
            _p60MphProgress->setColor(Color_Green_1);
        } else if (_counter <= 0) {
            _counter = 0;
            _testStatus = Status_Test_Show_Result;
            _p30MphProgress->SetHiden();
            _p60MphProgress->SetHiden();
            _p30MPHTime->SetHiden();
            _pTimer->SetHiden();

            _pScoreTitle30Mph->SetShow();
            int time_30mph_ms = 0;
            if (_bImperialUnits) {
                _pScoreTitle30Mph->SetText((UINT8 *)"30mph");
                time_30mph_ms = _usec_offset_30mph / 1000;
            } else {
                _pScoreTitle30Mph->SetText((UINT8 *)"50kmh");
                time_30mph_ms = _usec_offset_50kmh / 1000;
            }
            snprintf(_score30Mph, 16, "%d.%02ds",
                time_30mph_ms / 1000, (time_30mph_ms % 1000) / 10);
            _pScore30Mph->SetShow();
            if ((_best_score_1_ms <= 0) ||
                (time_30mph_ms <= _best_score_1_ms))
            {
                _best_score_1_ms = time_30mph_ms;
                _p30MphBest->SetShow();
            }

            _pScoreTitle60Mph->SetShow();
            int time_60mph_ms = 0;
            if (_bExceed100kmh) {
                if (_bImperialUnits) {
                    _pScoreTitle60Mph->SetText((UINT8 *)"60mph");
                    time_60mph_ms = _usec_offset_60mph / 1000;
                } else {
                    _pScoreTitle60Mph->SetText((UINT8 *)"100kmh");
                    time_60mph_ms = _usec_offset_100kmh / 1000;
                }
                snprintf(_score60Mph, 16, "%d.%02ds",
                    time_60mph_ms / 1000, (time_60mph_ms % 1000) / 10);
            } else {
                snprintf(_score60Mph, 16, "--");
            }
            _pScore60Mph->SetShow();
            if (_bExceed100kmh) {
                if ((_best_score_2_ms <= 0) ||
                    (time_60mph_ms <= _best_score_2_ms))
                {
                    _best_score_2_ms = time_60mph_ms;
                    _p60MphBest->SetShow();
                }
            }

            _saveTestScore();
        }
    }
}

void CountdownTestWnd::_showFailedReason()
{
    _pOK->SetHiden();
    _pHintInfo->SetHiden();
    _pAnimation->SetHiden();

    _p60MphBest->SetHiden();
    _pScore60Mph->SetHiden();
    _pScoreTitle60Mph->SetHiden();
    _p30MphBest->SetHiden();
    _pScore30Mph->SetHiden();
    _pScoreTitle30Mph->SetHiden();

    _p60MphProgress->SetHiden();
    _p30MphProgress->SetHiden();
    _p30MPHTime->SetHiden();
    _pTimer->SetHiden();
    _pRing->SetHiden();

    _pIcnRC->SetHiden();
    _pDash->SetHiden();
    _pStartFake->SetHiden();
    _pStart->SetHiden();
    _pWarning->SetHiden();

    _pReturnBtnFake->SetHiden();
    _pReturnBtn->SetHiden();
    _pTitle->SetHiden();
    _pLine->SetHiden();

    _pRing->setRadius(199, 167);
    _pRing->setColor(Color_Red_1, Color_Black);
    _pRing->SetShow();

    _pIcnRC->setSource(
        "/usr/local/share/ui/BMP/dash_menu/",
        "feedback-bad.bmp");
    _pIcnRC->SetRelativePos(Point(160, 96));
    _pIcnRC->SetShow();

    switch (_failedReason) {
        case Failed_Reason_Not_On_CarMount:
            snprintf(_txtHint, 32, "Check car mount!");
            break;
        case Failed_Reason_GPS_Not_Ready:
            snprintf(_txtHint, 32, "GPS NOT READY");
            break;
        case Failed_Reason_GPS_Low_Accuracy:
            snprintf(_txtHint, 32, "GPS LOW ACCURACY");
            break;
        case Failed_Reason_False_Start:
            snprintf(_txtHint, 32, "False Start");
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
    _pHintInfo->SetRelativePos(Point(0, 210));
    _pHintInfo->SetShow();

    _pOK->SetShow();
}

void CountdownTestWnd::_initDisplay()
{
    _pOK->SetHiden();
    _pHintInfo->SetHiden();
    _pAnimation->SetHiden();

    _p60MphBest->SetHiden();
    _pScore60Mph->SetHiden();
    _pScoreTitle60Mph->SetHiden();
    _p30MphBest->SetHiden();
    _pScore30Mph->SetHiden();
    _pScoreTitle30Mph->SetHiden();

    _p60MphProgress->SetHiden();
    _p30MphProgress->SetHiden();
    _p30MPHTime->SetHiden();
    _pTimer->SetHiden();
    _pRing->SetHiden();

    _pIcnRC->SetHiden();
    _pDash->SetHiden();
    _pStartFake->SetHiden();
    _pStart->SetHiden();
    _pWarning->SetHiden();

    _pReturnBtnFake->SetShow();
    _pReturnBtn->SetShow();
    _pTitle->SetShow();
    _pLine->SetShow();

    _p30MphProgress->setColor(Color_White);
    _p30MphProgress->setAngles(269.5, 270.0);
    _p60MphProgress->setColor(Color_White);
    _p60MphProgress->setAngles(269.5, 270.0);
    _pRing->setColor(Color_Red_1, Color_Black);
    _pRing->setRadius(Radius_Inner + Radius_Step * Ring_Steps, Radius_Inner);

    _pIcnRC->setSource(
        "/usr/local/share/ui/BMP/dash_menu/",
        "icn-RC-menu.bmp");
    _pIcnRC->SetRelativePos(Point(40, 160));

    if (!_isOnCarMount()) {
        _testStatus = Status_Not_On_CarMount;
        snprintf(_txtHint, 32, "Check car mount!");
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
        _testStatus = Status_GPS_Ready;
        _pIcnRC->SetShow();
        _pDash->SetShow();
        _pStartFake->SetShow();
        _pStart->SetShow();
        _pWarning->SetShow();
    }

    _cntAnim = 0;
}

void CountdownTestWnd::_updateStartRec()
{
    _failedReason = Failed_Reason_None;
    _bMoveBegin = false;
    _bExceed30Mph = false;
    _bExceed50kmh = false;
    _bExceed60Mph = false;
    _bExceed100kmh = false;
    _bestSpeed = 0.0;
    memset(_historySpeed, 0x00, sizeof(_historySpeed));

    _pIcnRC->SetHiden();
    _pDash->SetHiden();
    _pStartFake->SetHiden();
    _pStart->SetHiden();
    _pWarning->SetHiden();

    _pReturnBtnFake->SetHiden();
    _pReturnBtn->SetHiden();
    _pTitle->SetHiden();
    _pLine->SetHiden();

    _pRing->SetShow();
    _counter = 55;
    snprintf(_txtTimer, 16, "%d", (_counter - 5) / 10);
    _pTimer->SetShow();
}

void CountdownTestWnd::onResume()
{
    camera_State state = _cp->getRecState();
    storage_State sts = _cp->getStorageState();
    if ((storage_State_ready == sts)
        && (camera_State_record != state)
        && (camera_State_marking != state)
        && (camera_State_starting != state))
    {
        _cp->OnRecKeyProcess();
    }

    char tmp[256];
    agcmd_property_get(PropName_Units_System, tmp, Imperial_Units_System);
    if (strcasecmp(tmp, Imperial_Units_System) == 0) {
        _bImperialUnits = true;
    } else {
        _bImperialUnits = false;
    }
    if (_bImperialUnits) {
        _pScoreTitle30Mph->SetText((UINT8 *)"30mph");
        _pScoreTitle60Mph->SetText((UINT8 *)"60mph");
    } else {
        _pScoreTitle30Mph->SetText((UINT8 *)"50kmh");
        _pScoreTitle60Mph->SetText((UINT8 *)"100kmh");
    }

    _loadScores();

    if ((Status_Counting_Down != _testStatus)
        && (Status_Testing != _testStatus)
        && (Status_Test_Success != _testStatus)
        && (Status_Test_Show_Result != _testStatus))
    {
        _failedReason = Failed_Reason_None;
        _bMoveBegin = false;
        _bExceed30Mph = false;
        _bExceed50kmh = false;
        _bExceed60Mph = false;
        _bExceed100kmh = false;
        _bestSpeed = 0.0;
        memset(_historySpeed, 0x00, sizeof(_historySpeed));
        _initDisplay();
        _pPanel->Draw(true);
    }
}

void CountdownTestWnd::onPause()
{
}

void CountdownTestWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if ((widget == _pStart) ||
        (widget == _pStartFake))
    {
        if (Status_GPS_Ready == _testStatus) {
            _updateStartRec();
            _pPanel->Draw(true);
            _testStatus = Status_Counting_Down;
        }
    }
}

bool CountdownTestWnd::OnEvent(CEvent *event)
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
                        _pIcnRC->SetHiden();
                        _pDash->SetHiden();
                        _pStartFake->SetHiden();
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
                        _pIcnRC->SetHiden();
                        _pDash->SetHiden();
                        _pStartFake->SetHiden();
                        _pStart->SetHiden();
                        _pWarning->SetHiden();
                        _cntAnim++;
                    } else if (GPS_Status_Ready == status) {
                        _testStatus = Status_GPS_Ready;
                        _pHintInfo->SetHiden();
                        _pAnimation->SetHiden();
                        _pIcnRC->SetShow();
                        _pDash->SetShow();
                        _pStartFake->SetShow();
                        _pStart->SetShow();
                        _pWarning->SetShow();
                    }
                } else if (Status_Counting_Down == _testStatus) {
                    _updateCountdown();
                    if (Status_Test_Failed == _testStatus) {
                        CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_060Test_Fail);
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
                    this->Close(Anim_Top2BottomExit);
                    b = false;
                }
            } else if (event->_event == TouchEvent_SingleClick) {
                // click any area can restart a test
                if ((Status_Test_Success == _testStatus) ||
                    (Status_Test_Show_Result == _testStatus) ||
                    (Status_Test_Failed == _testStatus))
                {
                    _failedReason = Failed_Reason_None;
                    _bMoveBegin = false;
                    _bExceed30Mph = false;
                    _bExceed50kmh = false;
                    _bExceed60Mph = false;
                    _bExceed100kmh = false;
                    _bestSpeed = 0.0;
                    memset(_historySpeed, 0x00, sizeof(_historySpeed));
                    _initDisplay();
                    _pPanel->Draw(true);
                }
            }
        case eventType_key:
            switch(event->_event)
            {
                case key_right_onKeyUp: {
                    if (Status_GPS_Ready == _testStatus) {
                        _updateStartRec();
                        _pPanel->Draw(true);
                        _testStatus = Status_Counting_Down;
                        _pPanel->Draw(true);
                        _testStatus = Status_Counting_Down;
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


AutoTestWnd::AutoTestWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _paggps_client(NULL)
    , _cntAnim(0)
    , _counter(0)
    , _testStatus(Status_Not_On_CarMount)
    , _failedReason(Failed_Reason_None)
    , _bestSpeed(0.0)
    , _bImperialUnits(false)
    , _bMoveBegin(false)
    , _bExceed30Mph(false)
    , _bExceed50kmh(false)
    , _bExceed60Mph(false)
    , _bExceed100kmh(false)
    , _usec_offset_30mph(0)
    , _usec_offset_50kmh(0)
    , _usec_offset_60mph(0)
    , _usec_offset_100kmh(0)
    , _best_score_1_ms(0)
    , _best_score_2_ms(0)
{
    memset(_historySpeed, 0x00, sizeof(_historySpeed));

    _paggps_client = aggps_open_client();

    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
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
    _pTitle->SetText((UINT8 *)"Auto Mode");

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

    _pRing = new FanRing(_pPanel, Point(200, 200),
        Radius_Inner + Radius_Step * Ring_Steps, Radius_Inner,
        Color_Red_1, Color_Black);
    _pRing->setAngles(0.0, 360.0);
    _pRing->SetHiden();

    snprintf(_txtTimer, 16, " ");
    _pTimer = new StaticText(_pPanel, Point(50, 120), Size(300, 160));
    _pTimer->SetStyle(100, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _pTimer->SetText((UINT8 *)_txtTimer);
    _pTimer->SetHiden();

    snprintf(_txt30MPHTime, 16, " ");
    _p30MPHTime = new StaticText(_pPanel, Point(160, 110), Size(80, 60));
    _p30MPHTime->SetStyle(40, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _p30MPHTime->SetText((UINT8 *)_txt30MPHTime);
    _p30MPHTime->SetHiden();

    _p30MphProgress = new Arc(_pPanel, Point(200, 200), 140, 16, Color_White);
    _p30MphProgress->setAngles(269.5, 270.0);
    _p30MphProgress->SetHiden();

    _p60MphProgress = new Arc(_pPanel, Point(200, 200), 160, 16, Color_White);
    _p60MphProgress->setAngles(269.5, 270.0);
    _p60MphProgress->SetHiden();

    _pScoreTitle30Mph = new StaticText(_pPanel, Point(0, 100), Size(200, 40));
    _pScoreTitle30Mph->SetStyle(32, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pScoreTitle30Mph->SetText((UINT8 *)"30mph");
    _pScoreTitle30Mph->SetHiden();

    snprintf(_score30Mph, 16, " ");
    _pScore30Mph = new StaticText(_pPanel, Point(0, 140), Size(200, 120));
    _pScore30Mph->SetStyle(56, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _pScore30Mph->SetText((UINT8 *)_score30Mph);
    _pScore30Mph->SetHiden();

    _p30MphBest = new BmpImage(
        _pPanel,
        Point(40, 260), Size(120, 32),
        "/usr/local/share/ui/BMP/performance_test/",
        "icn-best2.bmp");
    _p30MphBest->SetHiden();

    _pScoreTitle60Mph = new StaticText(_pPanel, Point(200, 100), Size(200, 40));
    _pScoreTitle60Mph->SetStyle(32, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pScoreTitle60Mph->SetText((UINT8 *)"60mph");
    _pScoreTitle60Mph->SetHiden();

    snprintf(_score60Mph, 16, " ");
    _pScore60Mph = new StaticText(_pPanel, Point(200, 140), Size(200, 120));
    _pScore60Mph->SetStyle(56, FONT_ROBOTO_Regular, Color_White, CENTER, MIDDLE);
    _pScore60Mph->SetText((UINT8 *)_score60Mph);
    _pScore60Mph->SetHiden();

    _p60MphBest = new BmpImage(
        _pPanel,
        Point(240, 260), Size(120, 32),
        "/usr/local/share/ui/BMP/performance_test/",
        "icn-best2.bmp");
    _p60MphBest->SetHiden();

    _pIcnFailed = new BmpImage(
        _pPanel,
        Point(160, 96), Size(80, 80),
        "/usr/local/share/ui/BMP/dash_menu/",
        "feedback-bad.bmp");
    _pIcnFailed->SetHiden();

    _pOK = new StaticText(_pPanel, Point(170, 288), Size(60, 60));
    _pOK->SetStyle(40, FONT_ROBOTOMONO_Bold, Color_White, CENTER, MIDDLE);
    _pOK->SetText((UINT8 *)"OK");
    _pOK->SetHiden();
}

AutoTestWnd::~AutoTestWnd()
{
    delete _pOK;
    delete _pIcnFailed;

    delete _p60MphBest;
    delete _pScore60Mph;
    delete _pScoreTitle60Mph;

    delete _p30MphBest;
    delete _pScore30Mph;
    delete _pScoreTitle30Mph;

    delete _p60MphProgress;
    delete _p30MphProgress;
    delete _p30MPHTime;
    delete _pTimer;
    delete _pRing;

    delete _pWarning;
    delete _pHintInfo;
    delete _pAnimation;

    delete _pReturnBtnFake;
    delete _pReturnBtn;
    delete _pTitle;
    delete _pLine;

    delete _pPanel;

    if (_paggps_client) {
        aggps_close_client(_paggps_client);
    }
}

AutoTestWnd::GPSStatus AutoTestWnd::_getGPSStatus()
{
    gps_state state = _cp->getGPSState();
    if (gps_state_ready != state) {
        return GPS_Status_Not_Ready;
    }

    int ret_val = aggps_read_client_tmo(_paggps_client, 10);
    if (ret_val) {
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

AutoTestWnd::GPSStatus AutoTestWnd::_getSpeedKMH(GPSSpeed &speed)
{
    memset(&speed, 0x00, sizeof(speed));

    if (_paggps_client == NULL) {
        _paggps_client = aggps_open_client();
        if (_paggps_client == NULL) {
            //snprintf(_txtSpeed, 32, "GPS Open Failed");
            return GPS_Status_Not_Ready;
        }
    }

    int ret_val = aggps_read_client_tmo(_paggps_client, 10);
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
    if ((plocation->set & AGGPS_SET_SPEED) &&
        (plocation->set & AGGPS_SET_TIME) &&
        (plocation->set & AGGPS_SET_DOP))
    {
        speed.speedkmh = plocation->speed;
        speed.time.utc_time = plocation->utc_time;
        speed.time.utc_time_usec = plocation->utc_time_usec;
        //CAMERALOG("#### speed = %.2fKm/h(%.2fMPH), time = %ld.%06ds, accuracy = %f",
        //    speed.speedkmh, speed.speedkmh * 0.6213712,
        //    speed.time.utc_time, speed.time.utc_time_usec, plocation->accuracy);

        return GPS_Status_Ready;
    }

    return GPS_Status_Not_Ready;
}

bool AutoTestWnd::_loadScores()
{
    char scores_json[256];
    if (_bImperialUnits) {
        agcmd_property_get(PropName_60MPH_Test_Score_Auto, scores_json, "");
    } else {
        agcmd_property_get(PropName_100KMH_Test_Score_Auto, scores_json, "");
    }

    Document d;
    d.Parse(scores_json);
    if(!d.IsObject()) {
        //CAMERALOG("Didn't get a JSON object!");
        return false;
    }

    char *score_1 = (char *)"30mph";
    char *score_2 = (char *)"60mph";
    if (!_bImperialUnits) {
        score_1 = (char *)"50kmh";
        score_2 = (char *)"100kmh";
    }

    int num_score = 0;
    if (d.HasMember(score_1)) {
        const Value &a = d[score_1];
        if (a.IsArray()) {
            num_score = a.Size();
            if (num_score <= 0) {
                return false;
            }
            for (SizeType i = 0; i < a.Size(); i++) {
                if ((_best_score_1_ms <= 0) || (_best_score_1_ms > a[i].GetInt()))
                {
                    _best_score_1_ms = a[i].GetInt();
                }
            }
        }
    }

    if (d.HasMember(score_2)) {
        const Value &a = d[score_2];
        if (a.IsArray()) {
            int num = a.Size();
            if (num <= 0) {
                return false;
            }
            if (num_score != num) {
                CAMERALOG("incorrect scores: num_score = %d, num = %d", num_score, num);
                return false;
            }
            for (SizeType i = 0; i < a.Size(); i++) {
                if ((_best_score_2_ms <= 0) || (_best_score_2_ms > a[i].GetInt()))
                {
                    _best_score_2_ms = a[i].GetInt();
                }
            }
        }
    }

    return true;
}

void AutoTestWnd::_saveTestScore()
{
    if (_bExceed60Mph) {
        _saveTestScore(true, _usec_offset_30mph, _usec_offset_60mph);
    } else if (_bExceed30Mph) {
        _saveTestScore(true, _usec_offset_30mph, 0);
    }

    if (_bExceed100kmh) {
        _saveTestScore(false, _usec_offset_50kmh, _usec_offset_100kmh);
    } else if (_bExceed50kmh) {
        _saveTestScore(false, _usec_offset_50kmh, 0);
    }
}

void AutoTestWnd::_saveTestScore(bool bImperial, int score_1_us, int score_2_us)
{
    if ((score_1_us < 0) || (score_1_us > 60 * SEC_TO_USEC) ||
        (score_2_us < 0) || (score_2_us > 60 * SEC_TO_USEC))
    {
        return;
    }

    // Firstly load the scores:
    char scores_json[256];
    if (bImperial) {
        agcmd_property_get(PropName_60MPH_Test_Score_Auto, scores_json, "");
    } else {
        agcmd_property_get(PropName_100KMH_Test_Score_Auto, scores_json, "");
    }

    int scores[10][2];
    int num_score = 0;
    if (strcasecmp(scores_json, "none") != 0) {
        Document d;
        d.Parse(scores_json);
        if(d.IsObject()) {
            char *index_score_1 = (char *)"30mph";
            char *index_score_2 = (char *)"60mph";
            if (!bImperial) {
                index_score_1 = (char *)"50kmh";
                index_score_2 = (char *)"100kmh";
            }
            if (d.HasMember(index_score_1)) {
                const Value &a = d[index_score_1];
                if (a.IsArray()) {
                    num_score = a.Size();
                    if (num_score <= 0) {
                        return;
                    }
                    for (SizeType i = 0; i < a.Size(); i++) {
                        scores[i][0] = a[i].GetInt();
                    }
                }
            }
            if (d.HasMember(index_score_2)) {
                const Value &a = d[index_score_2];
                if (a.IsArray()) {
                    int num = a.Size();
                    if (num <= 0) {
                        return;
                    }
                    if (num_score != num) {
                        CAMERALOG("incorrect scores: num_score = %d, num = %d",
                            num_score, num);
                        return;
                    }
                    for (SizeType i = 0; i < a.Size(); i++) {
                        scores[i][1] = a[i].GetInt();
                    }
                }
            }
        }
    }

    // Secondly update the score:
    if (num_score < 10) {
        num_score++;
    }
    for (int i = num_score; i > 0; i--)
    {
        scores[i][0] = scores[i - 1][0];
        scores[i][1] = scores[i - 1][1];
    }
    scores[0][0] = score_1_us / 1000;
    scores[0][1] = score_2_us / 1000;

    // Thirdly save the score:
    Document d_new_score;
    Document::AllocatorType& allocator = d_new_score.GetAllocator();
    d_new_score.SetObject();

    Value array_30mph(kArrayType);
    for (int i = 0; i < num_score; i++) {
        Value object(kObjectType);

        int sec_30mph = scores[i][0];
        object.SetInt(sec_30mph);

        array_30mph.PushBack(object, allocator);
    }
    if (bImperial) {
        d_new_score.AddMember("30mph", array_30mph, allocator);
    } else {
        d_new_score.AddMember("50kmh", array_30mph, allocator);
    }

    Value array_60mph(kArrayType);
    for (int i = 0; i < num_score; i++) {
        Value object(kObjectType);

        int sec_60mph = scores[i][1];
        object.SetInt(sec_60mph);

        array_60mph.PushBack(object, allocator);
    }
    if (bImperial) {
        d_new_score.AddMember("60mph", array_60mph, allocator);
    } else {
        d_new_score.AddMember("100kmh", array_60mph, allocator);
    }

    StringBuffer buffer;
    Writer<StringBuffer> write(buffer);
    d_new_score.Accept(write);
    const char *str = buffer.GetString();
    CAMERALOG("strlen %d: %s", strlen(str), str);

    if (bImperial) {
        agcmd_property_set(PropName_60MPH_Test_Score_Auto, str);
    } else {
        agcmd_property_set(PropName_100KMH_Test_Score_Auto, str);
    }
}


void AutoTestWnd::_addClipSceneData()
{
    if (!_bExceed50kmh) {
        return;
    }

    if (_bExceed50kmh && _bExceed100kmh) {
        // mark the test, add clip description for it:
        struct auto_60mph_test_desc {
            uint32_t fourcc;

            // bytes including itself and following members
            uint32_t data_size;

            // move begin time
            uint32_t utc_sec_move_begin;
            uint32_t utc_usec_move_begin;

            // 30mph time
            uint32_t usec_offset_30mph;
            // 50kmh time
            uint32_t usec_offset_50kmh;

            // 60mph time
            uint32_t usec_offset_60mph;
            // 100kmh time
            uint32_t usec_offset_100kmh;
        };
        struct auto_60mph_test_desc desc;
        memset(&desc, 0x00, sizeof(desc));
        desc.fourcc = MAKE_FOURCC('A', 'U', '6', 'T');

        desc.data_size = sizeof(desc) - sizeof(desc.fourcc);

        desc.utc_sec_move_begin = _begin_move_time.utc_time;
        desc.utc_usec_move_begin = _begin_move_time.utc_time_usec;
        struct tm loctime;
        localtime_r((time_t*)(&(desc.utc_sec_move_begin)), &loctime);
        CAMERALOG("#### desc.utc_sec_move_begin = %d, _begin_move_time.utc_time = %ld",
            desc.utc_sec_move_begin, _begin_move_time.utc_time);
        char date_buffer[512];
        strftime(date_buffer, sizeof(date_buffer),
            "%Z %04Y/%02m/%02d %02H:%02M:%02S", &loctime);
        CAMERALOG("#### move begin time: %s.%01d",
            date_buffer, (desc.utc_usec_move_begin + 500) * 10 / 1000);

        desc.usec_offset_30mph = _usec_offset_30mph;
        CAMERALOG("#### 30mph time offset = %dus", desc.usec_offset_30mph);
        desc.usec_offset_50kmh = _usec_offset_50kmh;
        CAMERALOG("#### 50kmh time offset = %dus", desc.usec_offset_50kmh);

        desc.usec_offset_60mph = _usec_offset_60mph;
        CAMERALOG("#### 60mph time offset = %dus", desc.usec_offset_60mph);
        desc.usec_offset_100kmh = _usec_offset_100kmh;
        CAMERALOG("#### 100kmh time offset = %dus", desc.usec_offset_100kmh);

        CAMERALOG("#### auto test finished with 60mph tested, mark the video [-%d, 5],"
            " sizeof(desc) = %d, desc.data_size = %d",
            desc.usec_offset_100kmh / SEC_TO_USEC + BEFORE_SECONDS,
            sizeof(desc), desc.data_size);
        _cp->MarkVideo(desc.usec_offset_100kmh / SEC_TO_USEC + BEFORE_SECONDS,
            AFTER_SECONDS_LONG,
            (const char*)&desc, sizeof(desc));
    }else if (_bExceed50kmh) {
        // mark the test, add clip description for it:
        struct auto_30mph_test_desc {
            uint32_t fourcc;

            // bytes including itself and following members
            uint32_t data_size;

            // move begin time
            uint32_t utc_sec_move_begin;
            uint32_t utc_usec_move_begin;

            // 30mph time
            uint32_t usec_offset_30mph;
            // 50kmh time
            uint32_t usec_offset_50kmh;
        };
        struct auto_30mph_test_desc desc;
        memset(&desc, 0x00, sizeof(desc));
        desc.fourcc = MAKE_FOURCC('A', 'U', '3', 'T');

        desc.data_size = sizeof(desc) - sizeof(desc.fourcc);

        desc.utc_sec_move_begin = _begin_move_time.utc_time;
        desc.utc_usec_move_begin = _begin_move_time.utc_time_usec;
        struct tm loctime;
        localtime_r((time_t*)(&(desc.utc_sec_move_begin)), &loctime);
        CAMERALOG("#### desc.utc_sec_move_begin = %d, _begin_move_time.utc_time = %ld",
            desc.utc_sec_move_begin, _begin_move_time.utc_time);
        char date_buffer[512];
        strftime(date_buffer, sizeof(date_buffer),
            "%Z %04Y/%02m/%02d %02H:%02M:%02S", &loctime);
        CAMERALOG("#### move begin time: %s.%d",
            date_buffer, (desc.utc_usec_move_begin + 500) / 1000);

        desc.usec_offset_30mph = _usec_offset_30mph;
        CAMERALOG("#### 30mph time offset = %dus", desc.usec_offset_30mph);
        desc.usec_offset_50kmh = _usec_offset_50kmh;
        CAMERALOG("#### 50kmh time offset = %dus", desc.usec_offset_50kmh);

        int time_past_ms = (_historySpeed[0].time.utc_time - _begin_move_time.utc_time) * 1000
            + (_historySpeed[0].time.utc_time_usec - _begin_move_time.utc_time_usec) / 1000;
        CAMERALOG("#### auto test finished with 30mph tested, mark the video [-%d, 5],"
            " sizeof(desc) = %d, desc.data_size = %d",
            time_past_ms / 1000 + BEFORE_SECONDS, sizeof(desc), desc.data_size);
        _cp->MarkVideo(time_past_ms / 1000 + BEFORE_SECONDS,
            AFTER_SECONDS_SHORT, (const char*)&desc, sizeof(desc));
    }
}

void AutoTestWnd::_recordSpeed(GPSSpeed &speed)
{
    for (int i = AUTO_HISTORY_RECORD_NUM - 1; i > 0; i--) {
        _historySpeed[i].speedkmh = _historySpeed[i - 1].speedkmh;
        _historySpeed[i].accel[0] = _historySpeed[i - 1].accel[0];
        _historySpeed[i].accel[1] = _historySpeed[i - 1].accel[1];
        _historySpeed[i].accel[2] = _historySpeed[i - 1].accel[2];
        _historySpeed[i].time.utc_time = _historySpeed[i - 1].time.utc_time;
        _historySpeed[i].time.utc_time_usec = _historySpeed[i - 1].time.utc_time_usec;
    }
    _historySpeed[0].speedkmh = speed.speedkmh;
    _historySpeed[0].accel[0] = speed.accel[0];
    _historySpeed[0].accel[1] = speed.accel[1];
    _historySpeed[0].accel[2] = speed.accel[2];
    _historySpeed[0].time.utc_time = speed.time.utc_time;
    _historySpeed[0].time.utc_time_usec = speed.time.utc_time_usec;

    if (speed.speedkmh > _bestSpeed) {
        _bestSpeed = speed.speedkmh;
    }
}

bool AutoTestWnd::_isMoveBegin(GPSSpeed &speed)
{
    const float Accel_Threshold = 0.1;

    if ((speed.speedkmh > 5.0) &&
        (_historySpeed[0].speedkmh > 0.0) &&
        (speed.speedkmh > _historySpeed[0].speedkmh))
    {
        if (bReleaseVersion) {
            for (int i = 0; i < AUTO_HISTORY_RECORD_NUM; i++) {
                CAMERALOG("#### _historySpeed[%d].speed = %f, "
                    "accel_x = %f, accel_y = %f, accel_z = %f, accel = %f",
                    i, _historySpeed[i].speedkmh,
                    _historySpeed[i].accel[0], _historySpeed[i].accel[1],
                    _historySpeed[i].accel[2],
                    sqrt(_historySpeed[i].accel[0] * _historySpeed[i].accel[0] +
                        _historySpeed[i].accel[1] * _historySpeed[i].accel[1] +
                        _historySpeed[i].accel[2] * _historySpeed[i].accel[2]));
            }
        }

        // check whether speed matched:
        int index_speed = 0;
        int i = 0;
        for (i = 0; i < AUTO_HISTORY_RECORD_NUM - 1; i++) {
            if ((_historySpeed[i].speedkmh <= 1.0) ||
                (_historySpeed[i + 1].speedkmh <= 1.0))
            {
                index_speed = i;
                break;
            } else if ((_historySpeed[i].speedkmh + 0.2 <= _historySpeed[i + 1].speedkmh)
                && (_historySpeed[i + 1].speedkmh < 5.0))
            {
                index_speed = i;
                break;
            }
        }
        if (_historySpeed[i].speedkmh <= 3.0) {
            index_speed = i;
        }

        // check whether acceleration matched:
        for (i = 0; i < AUTO_HISTORY_RECORD_NUM - 1; i++) {
            CAMERALOG("_historySpeed[%d].accel[2] = %f, Accel_Threshold = %f",
                i, _historySpeed[i].accel[2], Accel_Threshold);
            if (_historySpeed[i].accel[2] < Accel_Threshold) {
                CAMERALOG("index_accel = %d", i);
                break;
            }
        }
        int index_accel = i;

        // If speed and acceleration matched, find start position:
        if ((index_speed > 1) && (index_accel >= 3)) {
            CAMERALOG("#### speed = %f, index_speed = %d, index_accel = %d",
                speed.speedkmh, index_speed, index_accel);
            _bMoveBegin = true;
            _begin_move_time.utc_time =
                _historySpeed[index_speed].time.utc_time;
            _begin_move_time.utc_time_usec =
                _historySpeed[index_speed].time.utc_time_usec;
            return true;
        } else {
            CAMERALOG("#### speed = %f, index_speed = %d, index_accel = %d, "
                "looks like it doesn't stop yet.",
                speed.speedkmh, index_speed, index_accel);
            return false;
        }
    } else {
        return false;
    }
}

void AutoTestWnd::_updateWaiting()
{
    GPSSpeed speed;
    GPSStatus status = _getSpeedKMH(speed);
    if (GPS_Status_Ready != status) {
        _failedReason = (GPS_Status_Low_Accuracy == status)
            ? Failed_Reason_GPS_Low_Accuracy : Failed_Reason_GPS_Not_Ready;
        _testStatus = Status_Test_Failed;
        return;
    }

    if (!_cp->getAccelXYZ(speed.accel[0], speed.accel[1], speed.accel[2]))
    {
        CAMERALOG("#### getAccelXYZ failed!");
    }
/*
    CAMERALOG("#### speed = %f, "
        "accel_x = %f, accel_y = %f, accel_z = %f, accel = %f",
        speed.speedkmh,
        speed.accel[0], speed.accel[1], speed.accel[2],
        sqrt(speed.accel[0] * speed.accel[0] +
            speed.accel[1] * speed.accel[1] +
            speed.accel[2] * speed.accel[2]));
*/

    if (!_bMoveBegin) {
        _isMoveBegin(speed);
    }

    if (_bMoveBegin) {
        _counter = 0;
        _pReturnBtnFake->SetHiden();
        _pReturnBtn->SetHiden();
        _pTitle->SetHiden();
        _pLine->SetHiden();
        _pHintInfo->SetHiden();
        _pWarning->SetHiden();
        _pRing->SetHiden();
        _pTimer->SetShow();
        _p30MphProgress->SetShow();
        _p60MphProgress->SetShow();
        int time_past_ms = (speed.time.utc_time - _begin_move_time.utc_time) * 1000
            + (speed.time.utc_time_usec - _begin_move_time.utc_time_usec + 500) / 1000;
        snprintf(_txtTimer, 16, "%d.%d", time_past_ms / 1000, (time_past_ms % 1000) / 100);
        _testStatus = Status_Testing;
    } else {
        if (speed.speedkmh >= 5.0) {
            snprintf(_txtHint, 32, "Stop your car\nbefore testing.");
            _pHintInfo->SetColor(Color_Grey_4);
            _pHintInfo->SetFont(FONT_ROBOTOMONO_Regular);
            _pHintInfo->SetRelativePos(Point(0, 180));
            _pHintInfo->SetShow();
            _pWarning->SetShow();
        } else {
            snprintf(_txtHint, 32, "Accelerate to Start");
            _pHintInfo->SetColor(Color_White);
            _pHintInfo->SetFont(FONT_ROBOTOMONO_Bold);
            _pHintInfo->SetRelativePos(Point(0, 200));
            _pHintInfo->SetShow();
            _pWarning->SetShow();
        }
    }

    _recordSpeed(speed);
}

void AutoTestWnd::_updateTesting()
{
    GPSSpeed speed;
    GPSStatus status = _getSpeedKMH(speed);
    if (GPS_Status_Ready != status) {
        _failedReason = (GPS_Status_Low_Accuracy == status)
            ? Failed_Reason_GPS_Low_Accuracy : Failed_Reason_GPS_Not_Ready;
        _testStatus = Status_Test_Failed;
        return;
    }

    if (!_cp->getAccelXYZ(speed.accel[0], speed.accel[1], speed.accel[2]))
    {
        CAMERALOG("#### getAccelXYZ failed!");
    }

    int time_past_ms = (speed.time.utc_time - _begin_move_time.utc_time) * 1000
        + (speed.time.utc_time_usec - _begin_move_time.utc_time_usec + 500) / 1000;
    snprintf(_txtTimer, 16, "%d.%d", time_past_ms / 1000, (time_past_ms % 1000) / 100);
    if (time_past_ms >= TIME_TOO_LONG * 1000) {
        // Takes too long time (more than 1min), fails
        CAMERALOG("#### now = %ld.%ds, _begin_move_time = %ld.%ds, time_past_ms = %d, "
            "test failed becaust it takes too long",
            speed.time.utc_time, speed.time.utc_time_usec,
            _begin_move_time.utc_time, _begin_move_time.utc_time_usec, time_past_ms);
        _failedReason = Failed_Reason_Takes_Too_Long;
        _testStatus = Status_Test_Failed;
        return;
    }

    // check speed, if speed decrease, test finished
    if (((_bestSpeed > 0.0) && (speed.speedkmh + 5.0 < _bestSpeed))
        || (speed.speedkmh + 1.0 < _historySpeed[0].speedkmh))
    {
        if (_bExceed50kmh) {
            _testStatus = Status_Test_Success;
            _addClipSceneData();
            _counter = 10;
        } else {
            _failedReason = Failed_Reason_Slowdown;
            _testStatus = Status_Test_Failed;
        }
        return;
    } else {
        _recordSpeed(speed);
    }

    // update speed test display:
    if (_bMoveBegin) {
        if (!_bExceed50kmh) {
            if (!_bExceed30Mph && (speed.speedkmh >= SPEED_TEST_30MPH)) {
                _bExceed30Mph = true;
                _usec_offset_30mph =
                    (speed.time.utc_time - _begin_move_time.utc_time) * SEC_TO_USEC
                    + (speed.time.utc_time_usec - _begin_move_time.utc_time_usec);
                if ((speed.speedkmh - _historySpeed[1].speedkmh) != 0) {
                    int offset_us = 0;
                    offset_us = (speed.speedkmh - SPEED_TEST_30MPH) /
                        (speed.speedkmh - _historySpeed[1].speedkmh)
                        * 0.1 * SEC_TO_USEC;
                    CAMERALOG("#### speed.speedkmh = %f, _historySpeed[1].speedkmh = %f, "
                        "offset_us = %d, _usec_offset_30mph = %d",
                        speed.speedkmh, _historySpeed[1].speedkmh, offset_us, _usec_offset_30mph);
                    _usec_offset_30mph = _usec_offset_30mph - offset_us;
                }
                CAMERALOG("#### 30mph tested, speed.speedkmh = %f, cost %dus",
                    speed.speedkmh, _usec_offset_30mph);
                if (_usec_offset_30mph <= 0) {
                    _failedReason = Failed_Reason_Bad_Test;
                    _testStatus = Status_Test_Failed;
                    return;
                }

                if (_bImperialUnits) {
                    int time_30mph_ms = _usec_offset_30mph / 1000;
                    _p30MphProgress->setAngles(270.0, 270.0);
                    _p30MphProgress->setColor(Color_Green_1);
                    snprintf(_txt30MPHTime, 16, "%d.%02d",
                        time_30mph_ms / 1000, (time_30mph_ms % 1000) / 10);
                    _p30MPHTime->SetShow();
                    CAudioPlayer::getInstance()->playSoundEffect(
                        CAudioPlayer::SE_060Test_Achieve_1);
                }
            }
            if (speed.speedkmh >= SPEED_TEST_50KMH) {
                _bExceed50kmh = true;
                // record scores:
                _usec_offset_50kmh =
                    (speed.time.utc_time - _begin_move_time.utc_time) * SEC_TO_USEC
                    + (speed.time.utc_time_usec - _begin_move_time.utc_time_usec);
                if ((speed.speedkmh - _historySpeed[1].speedkmh) != 0) {
                    int offset_us = 0;
                    offset_us = (speed.speedkmh - SPEED_TEST_50KMH) /
                        (speed.speedkmh - _historySpeed[1].speedkmh)
                        * 0.1 * SEC_TO_USEC;
                    CAMERALOG("#### speed.speedkmh = %f, _historySpeed[1].speedkmh = %f, "
                        "offset_us = %d, _usec_offset_50kmh = %d",
                        speed.speedkmh, _historySpeed[1].speedkmh, offset_us, _usec_offset_50kmh);
                    _usec_offset_50kmh = _usec_offset_50kmh - offset_us;
                }
                CAMERALOG("############# 50kmh tested, speed.speedkmh = %f, cost %dus",
                    speed.speedkmh, _usec_offset_50kmh);
                if (_usec_offset_50kmh <= 0) {
                    _failedReason = Failed_Reason_Bad_Test;
                    _testStatus = Status_Test_Failed;
                    return;
                }

                // update display:
                if (!_bImperialUnits) {
                    int time_50kmh_ms = _usec_offset_50kmh / 1000;
                    _p30MphProgress->setAngles(270.0, 270.0);
                    _p30MphProgress->setColor(Color_Green_1);
                    snprintf(_txt30MPHTime, 16, "%d.%02d",
                        time_50kmh_ms / 1000, (time_50kmh_ms % 1000) / 10);
                    _p30MPHTime->SetShow();
                    CAudioPlayer::getInstance()->playSoundEffect(
                        CAudioPlayer::SE_060Test_Achieve_1);
                }
            } else {
                _p30MphProgress->setColor(Color_White);
                float angle_speed = 360.0 * speed.speedkmh /
                    (_bImperialUnits ? SPEED_TEST_30MPH : SPEED_TEST_50KMH);
                if (angle_speed >= 360.0) {
                    angle_speed = 360.0;
                    _p30MphProgress->setColor(Color_Green_1);
                }
                angle_speed = angle_speed + 270;
                if (angle_speed > 360.0) {
                    angle_speed = angle_speed - 360.0;
                }
                _p30MphProgress->setAngles(270.0, angle_speed);
            }
        }

        if (!_bExceed100kmh) {
            if (!_bExceed60Mph && (speed.speedkmh >= SPEED_TEST_60MPH)) {
                _bExceed60Mph = true;
                _usec_offset_60mph =
                    (speed.time.utc_time - _begin_move_time.utc_time) * SEC_TO_USEC
                    + (speed.time.utc_time_usec - _begin_move_time.utc_time_usec);
                if ((speed.speedkmh - _historySpeed[1].speedkmh) != 0) {
                    int offset_us = 0;
                    offset_us = (speed.speedkmh - SPEED_TEST_60MPH) /
                        (speed.speedkmh - _historySpeed[1].speedkmh)
                        * 0.1 * SEC_TO_USEC;
                    CAMERALOG("#### speed.speedkmh = %f, _historySpeed[1].speedkmh = %f, "
                        "offset_us = %d, _usec_offset_60mph = %d",
                        speed.speedkmh, _historySpeed[1].speedkmh, offset_us, _usec_offset_60mph);
                    _usec_offset_60mph = _usec_offset_60mph - offset_us;
                }
                CAMERALOG("############# 60mph tested, speed.speedkmh = %f, cost %dus",
                    speed.speedkmh, _usec_offset_60mph);
                if (_usec_offset_60mph <= 0) {
                    _failedReason = Failed_Reason_Bad_Test;
                    _testStatus = Status_Test_Failed;
                    return;
                }
            }
            if (speed.speedkmh >= SPEED_TEST_100KMH) {
                _bExceed100kmh = true;
                // record scores:
                _usec_offset_100kmh =
                    (speed.time.utc_time - _begin_move_time.utc_time) * SEC_TO_USEC
                    + (speed.time.utc_time_usec - _begin_move_time.utc_time_usec);
                if ((speed.speedkmh - _historySpeed[1].speedkmh) != 0) {
                    int offset_us = 0;
                    offset_us = (speed.speedkmh - SPEED_TEST_100KMH) /
                        (speed.speedkmh - _historySpeed[1].speedkmh)
                        * 0.1 * SEC_TO_USEC;
                    CAMERALOG("#### speed.speedkmh = %f, _historySpeed[1].speedkmh = %f, "
                        "offset_us = %d, _usec_offset_100kmh = %d",
                        speed.speedkmh, _historySpeed[1].speedkmh, offset_us, _usec_offset_100kmh);
                    _usec_offset_100kmh = _usec_offset_100kmh - offset_us;
                }
                CAMERALOG("############# 100kmh tested, speed.speedkmh = %f, cost %dus",
                    speed.speedkmh, _usec_offset_100kmh);
                if (_usec_offset_100kmh <= 0) {
                    _failedReason = Failed_Reason_Bad_Test;
                    _testStatus = Status_Test_Failed;
                    return;
                }

                _p30MphProgress->setColor(Color_White);
                _p60MphProgress->setAngles(270.0, 270.0);
                _p60MphProgress->setColor(Color_White);
                _pRing->setRadius(Radius_Inner + Radius_Step * Ring_Steps, 0);
                _pRing->setColor(Color_Green_1, Color_Black);
                _pRing->SetShow();

                _testStatus = Status_Test_Success;
                CAudioPlayer::getInstance()->playSoundEffect(
                    CAudioPlayer::SE_060Test_Achieve_2);
                _addClipSceneData();
                _counter = 10;
            } else {
                float angle_speed = 360.0 * speed.speedkmh /
                    (_bImperialUnits ? SPEED_TEST_60MPH : SPEED_TEST_100KMH);
                if (angle_speed >= 360.0) {
                    angle_speed = 360.0;
                }
                angle_speed = angle_speed + 270;
                if (angle_speed > 360.0) {
                    angle_speed = angle_speed - 360.0;
                }
                _p60MphProgress->setColor(Color_White);
                _p60MphProgress->setAngles(270.0, angle_speed);
            }
        }
    }
}

void AutoTestWnd::_showTestSuccess()
{
    if (_counter > 0) {
        _counter--;
        if (_bExceed100kmh && (_counter == 6)) {
            _pRing->SetHiden();
            _p30MphProgress->setColor(Color_Green_1);
            _p60MphProgress->setColor(Color_Green_1);
        } else if (_counter <= 0) {
            _counter = 0;
            _testStatus = Status_Test_Show_Result;
            _p30MphProgress->SetHiden();
            _p60MphProgress->SetHiden();
            _p30MPHTime->SetHiden();
            _pTimer->SetHiden();

            _pScoreTitle30Mph->SetShow();
            int time_30mph_ms = 0;
            if (_bImperialUnits) {
                _pScoreTitle30Mph->SetText((UINT8 *)"30mph");
                time_30mph_ms = _usec_offset_30mph / 1000;
            } else {
                _pScoreTitle30Mph->SetText((UINT8 *)"50kmh");
                time_30mph_ms = _usec_offset_50kmh / 1000;
            }
            snprintf(_score30Mph, 16, "%d.%02ds",
                time_30mph_ms / 1000, (time_30mph_ms % 1000) / 10);
            _pScore30Mph->SetShow();
            CAMERALOG("##### _best_score_1_ms = %d, time_30mph_ms = %d",
                    _best_score_1_ms, time_30mph_ms);
            if ((_best_score_1_ms <= 0) ||
                (time_30mph_ms <= _best_score_1_ms))
            {
                _best_score_1_ms = time_30mph_ms;
                _p30MphBest->SetShow();
            }

            _pScoreTitle60Mph->SetShow();
            int time_60mph_ms = 0;
            if (_bExceed100kmh) {
                if (_bImperialUnits) {
                    _pScoreTitle60Mph->SetText((UINT8 *)"60mph");
                    time_60mph_ms = _usec_offset_60mph / 1000;
                } else {
                    _pScoreTitle60Mph->SetText((UINT8 *)"100kmh");
                    time_60mph_ms = _usec_offset_100kmh / 1000;
                }
                snprintf(_score60Mph, 16, "%d.%02ds",
                    time_60mph_ms / 1000, (time_60mph_ms % 1000) / 10);
            } else {
                snprintf(_score60Mph, 16, "--");
            }
            _pScore60Mph->SetShow();
            if (_bExceed100kmh) {
                CAMERALOG("##### _best_score_2_ms = %d, time_60mph_ms = %d",
                    _best_score_2_ms, time_60mph_ms);
                if ((_best_score_2_ms <= 0) ||
                    (time_60mph_ms <= _best_score_2_ms))
                {
                    _best_score_2_ms = time_60mph_ms;
                    _p60MphBest->SetShow();
                }
            }

            _saveTestScore();
        }
    }
}

void AutoTestWnd::_showFailedReason()
{
    _pOK->SetHiden();
    _pIcnFailed->SetHiden();

    _p60MphBest->SetHiden();
    _pScore60Mph->SetHiden();
    _pScoreTitle60Mph->SetHiden();
    _p30MphBest->SetHiden();
    _pScore30Mph->SetHiden();
    _pScoreTitle30Mph->SetHiden();

    _p60MphProgress->SetHiden();
    _p30MphProgress->SetHiden();
    _p30MPHTime->SetHiden();
    _pTimer->SetHiden();
    _pRing->SetHiden();

    _pWarning->SetHiden();
    _pHintInfo->SetHiden();
    _pAnimation->SetHiden();

    _pReturnBtnFake->SetHiden();
    _pReturnBtn->SetHiden();
    _pTitle->SetHiden();
    _pLine->SetHiden();

    _pRing->setRadius(199, 167);
    _pRing->setColor(Color_Red_1, Color_Black);
    _pRing->SetShow();

    _pIcnFailed->SetShow();

    switch (_failedReason) {
        case Failed_Reason_Not_On_CarMount:
            snprintf(_txtHint, 32, "Check car mount!");
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
}

void AutoTestWnd::_initDisplay()
{
    _pOK->SetHiden();
    _pIcnFailed->SetHiden();

    _p60MphBest->SetHiden();
    _pScore60Mph->SetHiden();
    _pScoreTitle60Mph->SetHiden();
    _p30MphBest->SetHiden();
    _pScore30Mph->SetHiden();
    _pScoreTitle30Mph->SetHiden();

    _p60MphProgress->SetHiden();
    _p30MphProgress->SetHiden();
    _p30MPHTime->SetHiden();
    _pTimer->SetHiden();
    _pRing->SetHiden();

    _pWarning->SetHiden();
    _pHintInfo->SetHiden();
    _pAnimation->SetHiden();

    _pReturnBtnFake->SetShow();
    _pReturnBtn->SetShow();
    _pTitle->SetShow();
    _pLine->SetShow();

    _p30MphProgress->setColor(Color_White);
    _p30MphProgress->setAngles(269.5, 270.0);
    _p60MphProgress->setColor(Color_White);
    _p60MphProgress->setAngles(269.5, 270.0);
    _pRing->setColor(Color_Red_1, Color_Black);
    _pRing->setRadius(Radius_Inner + Radius_Step * Ring_Steps, Radius_Inner);

    if (!_isOnCarMount()) {
        _testStatus = Status_Not_On_CarMount;
        snprintf(_txtHint, 32, "Check car mount!");
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
        _testStatus = Status_Waiting;
        snprintf(_txtHint, 32, "Accelerate to Start");
        _pHintInfo->SetShow();
        _pWarning->SetShow();
    }
}

void AutoTestWnd::onResume()
{
    camera_State state = _cp->getRecState();
    storage_State sts = _cp->getStorageState();
    if ((storage_State_ready == sts)
        && (camera_State_record != state)
        && (camera_State_marking != state)
        && (camera_State_starting != state))
    {
        _cp->OnRecKeyProcess();
    }

    char tmp[256];
    agcmd_property_get(PropName_Units_System, tmp, Imperial_Units_System);
    if (strcasecmp(tmp, Imperial_Units_System) == 0) {
        _bImperialUnits = true;
    } else {
        _bImperialUnits = false;
    }
    if (_bImperialUnits) {
        _pScoreTitle30Mph->SetText((UINT8 *)"30mph");
        _pScoreTitle60Mph->SetText((UINT8 *)"60mph");
    } else {
        _pScoreTitle30Mph->SetText((UINT8 *)"50kmh");
        _pScoreTitle60Mph->SetText((UINT8 *)"100kmh");
    }

    _loadScores();

    if ((Status_Testing != _testStatus)
        && (Status_Test_Success != _testStatus)
        && (Status_Test_Show_Result != _testStatus))
    {
        _failedReason = Failed_Reason_None;
        _bMoveBegin = false;
        _bExceed30Mph = false;
        _bExceed50kmh = false;
        _bExceed60Mph = false;
        _bExceed100kmh = false;
        _bestSpeed = 0.0;
        memset(_historySpeed, 0x00, sizeof(_historySpeed));
        _initDisplay();
        _pPanel->Draw(true);
    }
}

void AutoTestWnd::onPause()
{
}

void AutoTestWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

bool AutoTestWnd::OnEvent(CEvent *event)
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
                    (Status_GPS_Low_Accuracy == _testStatus))
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
                        _cntAnim++;
                    } else if (GPS_Status_Ready == status) {
                        _testStatus = Status_Waiting;
                        snprintf(_txtHint, 32, "Accelerate to Start");
                        _pHintInfo->SetColor(Color_White);
                        _pHintInfo->SetFont(FONT_ROBOTOMONO_Bold);
                        _pHintInfo->SetRelativePos(Point(0, 200));
                        _pHintInfo->SetShow();
                        _pWarning->SetShow();
                        _pAnimation->SetHiden();
                    }
                } else if (Status_Waiting == _testStatus) {
                    _updateWaiting();
                    if (Status_Test_Failed == _testStatus) {
                        CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Alert);
                        _showFailedReason();
                    }
                } else if (Status_Testing == _testStatus) {
                    _updateTesting();
                    if (Status_Test_Failed == _testStatus) {
                        CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Alert);
                        _showFailedReason();
                    }
                } else if (Status_Test_Success== _testStatus) {
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
                    this->Close(Anim_Top2BottomExit);
                    b = false;
                }
            } else if (event->_event == TouchEvent_SingleClick) {
                // click any area can restart a test
                if ((Status_Test_Success == _testStatus) ||
                    (Status_Test_Show_Result == _testStatus) ||
                    (Status_Test_Failed == _testStatus))
                {
                    _failedReason = Failed_Reason_None;
                    _bMoveBegin = false;
                    _bExceed30Mph = false;
                    _bExceed50kmh = false;
                    _bExceed60Mph = false;
                    _bExceed100kmh = false;
                    _bestSpeed = 0.0;
                    memset(_historySpeed, 0x00, sizeof(_historySpeed));
                    _initDisplay();
                    _pPanel->Draw(true);
                }
            }
        case eventType_key:
            switch(event->_event)
            {
                case key_right_onKeyUp: {
#if 0
                    if (_cp->isCarMode()) {
                        camera_State state = _cp->getRecState();
                        if ((camera_State_record == state)
                            || (camera_State_marking == state))
                        {
                            _cp->MarkVideo();
                        } else {
                            storage_State st = _cp->getStorageState();
                            if (storage_State_noMedia == st) {
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
                        } else if (storage_State_noMedia == st) {
                            this->StartWnd(WINDOW_cardmissing, Anim_Top2BottomEnter);
                        }
                    }
#endif
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


ScoreFragment::ScoreFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren,
        int mode)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _mode(mode)
    , _pScroller(NULL)
    , _pTitle(NULL)
    , _pLine(NULL)
    , _pScore_1(NULL)
    , _pScore_2(NULL)
    , _index_best_score_1(-1)
    , _index_best_score_2(-1)
    , _p30MphBest(NULL)
    , _p60MphBest(NULL)
    , _pScore_Latest(NULL)
    , _pLine_Latest(NULL)
    , _num_latest(-1)
    , _pBtnClear(NULL)
    , _pHintInfo(NULL)
    , _pIconYes(NULL)
    , _pIconNo(NULL)
    , _bImperialUnits(false)
{
    memset(_pBest_1, 0x00, sizeof(_pBest_1));
    memset(_pBest_2, 0x00, sizeof(_pBest_1));
    memset(_pLatest_1, 0x00, sizeof(_pLatest_1));
    memset(_pLatest_2, 0x00, sizeof(_pLatest_1));
}

ScoreFragment::~ScoreFragment()
{
    _destroyScoreScroller();
}

void ScoreFragment::_storeFakeScores()
{
    Document d1;
    Document::AllocatorType& allocator = d1.GetAllocator();
    d1.SetObject();

    int num = random() % 5 + 5;

    Value array_30mph(kArrayType);
    for (int i = 0; i < num; i++) {
        Value object(kObjectType);

        int sec_30mph = random() % 1000;
        object.SetInt(sec_30mph);

        array_30mph.PushBack(object, allocator);
    }
    d1.AddMember("30mph", array_30mph, allocator);

    Value array_60mph(kArrayType);
    for (int i = 0; i < num; i++) {
        Value object(kObjectType);

        int sec_60mph = random() % 10000;
        object.SetInt(sec_60mph);

        array_60mph.PushBack(object, allocator);
    }
    d1.AddMember("60mph", array_60mph, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> write(buffer);
    d1.Accept(write);
    const char *str = buffer.GetString();
    CAMERALOG("strlen %d: %s", strlen(str), str);

    if (_bImperialUnits) {
        if (Mode_CountDown == _mode) {
            agcmd_property_set(PropName_60MPH_Test_Score, str);
        } else if (Mode_Auto == _mode) {
            agcmd_property_set(PropName_60MPH_Test_Score_Auto, str);
        }
    } else {
        if (Mode_CountDown == _mode) {
            agcmd_property_set(PropName_100KMH_Test_Score, str);
        } else if (Mode_Auto == _mode) {
            agcmd_property_set(PropName_100KMH_Test_Score_Auto, str);
        }
    }
}

bool ScoreFragment::_loadScores()
{
    char scores_json[256] = {};
    if (_bImperialUnits) {
        if (Mode_CountDown == _mode) {
            agcmd_property_get(PropName_60MPH_Test_Score, scores_json, "");
        } else if (Mode_Auto == _mode) {
            agcmd_property_get(PropName_60MPH_Test_Score_Auto, scores_json, "");
        }
    } else {
        if (Mode_CountDown == _mode) {
            agcmd_property_get(PropName_100KMH_Test_Score, scores_json, "");
        } else if (Mode_Auto == _mode) {
            agcmd_property_get(PropName_100KMH_Test_Score_Auto, scores_json, "");
        }
    }
    //CAMERALOG("score = %s", scores_json);

    Document d;
    d.Parse(scores_json);
    if(!d.IsObject()) {
        //CAMERALOG("Didn't get a JSON object!");
        return false;
    }

    char *score_1 = (char *)"30mph";
    char *score_2 = (char *)"60mph";
    if (!_bImperialUnits) {
        score_1 = (char *)"50kmh";
        score_2 = (char *)"100kmh";
    }

    float best_score = 0.0;
    _num_latest = 0;
    if (d.HasMember(score_1)) {
        const Value &a = d[score_1];
        if (a.IsArray()) {
            _num_latest = a.Size();
            if (_num_latest <= 0) {
                return false;
            }
            for (SizeType i = 0; i < a.Size(); i++) {
                _scores[i][0] = (a[i].GetInt()) / SCORE_ACCURACY;
                if (best_score <= 0.0) {
                    best_score = _scores[i][0];
                    _index_best_score_1 = i;
                } else if ((_scores[i][0] > 0.0) && (_scores[i][0] < best_score)) {
                    best_score = _scores[i][0];
                    _index_best_score_1 = i;
                }
            }
        }
    }

    best_score = 0.0;
    if (d.HasMember(score_2)) {
        const Value &a = d[score_2];
        if (a.IsArray()) {
            int num = a.Size();
            if (num <= 0) {
                return false;
            }
            if (_num_latest != num) {
                CAMERALOG("incorrect scores: _num_latest = %d, num = %d", _num_latest, num);
                return false;
            }
            for (SizeType i = 0; i < a.Size(); i++) {
                _scores[i][1] = (a[i].GetInt()) / SCORE_ACCURACY;
                if (best_score <= 0.0) {
                    best_score = _scores[i][1];
                    _index_best_score_2 = i;
                } else if ((_scores[i][1] > 0.0) && (_scores[i][1] < best_score)) {
                    best_score = _scores[i][1];
                    _index_best_score_2 = i;
                }
            }
        }
    }

    return true;
}

void ScoreFragment::_showScoreScroller()
{
    const int score_height = 32;
    const int font_size = 26;

    int scroller_height = 330;
    if (_num_latest > 0) {
        scroller_height += (_num_latest + 1) * 60;
    }
    _visibleSizeScroller.width = 400;
    _visibleSizeScroller.height = 330;
    _pScroller = new Scroller(
        this,
        this->GetOwner(),
        Point(0, 70),
        _visibleSizeScroller,
        Size(_visibleSizeScroller.width, scroller_height),
        50);

    _pTitle = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 0), Size(400, 80));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    if (Mode_CountDown == _mode) {
        _pTitle->SetText((UINT8 *)"Countdown Mode\nScores");
    } else {
        _pTitle->SetText((UINT8 *)"Auto Mode\nScores");
    }

    _pLine = new UILine(
        _pScroller->getViewRoot(),
        Point(140, 80), Size(120, 4));
    _pLine->setLine(Point(140, 80), Point(260, 80), 4, Color_Grey_2);

    _pScore_1 = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 104), Size(200, score_height));
    _pScore_1->SetStyle(font_size, FONT_ROBOTO_Regular, Color_Grey_2, CENTER, MIDDLE);
    if (_bImperialUnits) {
        _pScore_1->SetText((UINT8 *)"30mph");
    } else {
        _pScore_1->SetText((UINT8 *)"50kmh");
    }

    _pScore_2 = new StaticText(
        _pScroller->getViewRoot(),
        Point(200, 104), Size(200, score_height));
    _pScore_2->SetStyle(font_size, FONT_ROBOTO_Regular, Color_Grey_2, CENTER, MIDDLE);
    if (_bImperialUnits) {
        _pScore_2->SetText((UINT8 *)"60mph");
    } else {
        _pScore_2->SetText((UINT8 *)"100kmh");
    }

    int y_pos = 156;
    if ((_num_latest > 0) &&
        ((_index_best_score_1 >= 0) || ((_index_best_score_2 >= 0))))
    {
        if (_index_best_score_1 == _index_best_score_2) {
            if (_scores[_index_best_score_1][0] <= 0.0) {
                snprintf(_txtBest_1[0], 16, "--");
            } else {
                snprintf(_txtBest_1[0], 16, "%.2fs", _scores[_index_best_score_1][0]);
                _p30MphBest = new BmpImage(
                    _pScroller->getViewRoot(),
                    Point(140, y_pos), Size(32, 32),
                    "/usr/local/share/ui/BMP/performance_test/",
                    "icn-best.bmp");
            }
            _pBest_1[0] = new StaticText(
                _pScroller->getViewRoot(),
                Point(40, y_pos), Size(100, score_height));
            _pBest_1[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pBest_1[0]->SetText((UINT8 *)_txtBest_1[0]);

            if (_scores[_index_best_score_2][1] <= 0.0) {
                snprintf(_txtBest_2[0], 16, "--");
            } else {
                snprintf(_txtBest_2[0], 16, "%.2fs", _scores[_index_best_score_2][1]);
                _p60MphBest = new BmpImage(
                    _pScroller->getViewRoot(),
                    Point(340, y_pos), Size(32, 32),
                    "/usr/local/share/ui/BMP/performance_test/",
                    "icn-best.bmp");
            }
            _pBest_2[0] = new StaticText(
                _pScroller->getViewRoot(),
                Point(240, y_pos), Size(100, score_height));
            _pBest_2[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pBest_2[0]->SetText((UINT8 *)_txtBest_2[0]);

            y_pos += 52;
        } else if ((_index_best_score_1 >= 0) && (_index_best_score_2 < 0)) {
            _p30MphBest = new BmpImage(
                _pScroller->getViewRoot(),
                Point(140, y_pos), Size(32, 32),
                "/usr/local/share/ui/BMP/performance_test/",
                "icn-best.bmp");
            snprintf(_txtBest_1[0], 16, "%.2fs", _scores[_index_best_score_1][0]);
            _pBest_1[0] = new StaticText(
                _pScroller->getViewRoot(),
                Point(40, y_pos), Size(100, score_height));
            _pBest_1[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pBest_1[0]->SetText((UINT8 *)_txtBest_1[0]);

            _pBest_2[0] = new StaticText(
                _pScroller->getViewRoot(),
                Point(240, y_pos), Size(100, score_height));
            _pBest_2[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_Grey_2, RIGHT, MIDDLE);
            _pBest_2[0]->SetText((UINT8 *)"--");

            y_pos += 52;
        } else if ((_index_best_score_1 < 0) && (_index_best_score_2 >= 0)) {
            _pBest_1[0] = new StaticText(
                _pScroller->getViewRoot(),
                Point(40, y_pos), Size(100, score_height));
            _pBest_1[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_Grey_2, RIGHT, MIDDLE);
            _pBest_1[0]->SetText((UINT8 *)"--");

            _p60MphBest = new BmpImage(
                _pScroller->getViewRoot(),
                Point(340, y_pos), Size(32, 32),
                "/usr/local/share/ui/BMP/performance_test/",
                "icn-best.bmp");
            snprintf(_txtBest_2[0], 16, "%.2fs", _scores[_index_best_score_2][1]);
            _pBest_2[0] = new StaticText(
                _pScroller->getViewRoot(),
                Point(240, y_pos), Size(100, score_height));
            _pBest_2[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pBest_2[0]->SetText((UINT8 *)_txtBest_2[0]);

            y_pos += 52;
        } else if ((_index_best_score_1 >= 0) && (_index_best_score_2 >= 0)) {
            // best score 1:
            if (_scores[_index_best_score_1][0] <= 0.0) {
                snprintf(_txtBest_1[0], 16, "--");
            } else {
                snprintf(_txtBest_1[0], 16, "%.2fs", _scores[_index_best_score_1][0]);
                _p30MphBest = new BmpImage(
                    _pScroller->getViewRoot(),
                    Point(140, y_pos), Size(32, 32),
                    "/usr/local/share/ui/BMP/performance_test/",
                    "icn-best.bmp");
            }
            _pBest_1[0] = new StaticText(
                _pScroller->getViewRoot(),
                Point(40, y_pos), Size(100, score_height));
            _pBest_1[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pBest_1[0]->SetText((UINT8 *)_txtBest_1[0]);

            if (_scores[_index_best_score_1][1] <= 0.0) {
                snprintf(_txtBest_2[0], 16, "--");
            } else {
                snprintf(_txtBest_2[0], 16, "%.2fs", _scores[_index_best_score_1][1]);
            }
            _pBest_2[0] = new StaticText(
                _pScroller->getViewRoot(),
                Point(240, y_pos), Size(100, score_height));
            _pBest_2[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pBest_2[0]->SetText((UINT8 *)_txtBest_2[0]);

            y_pos += 52;

            // best score 2:
            if (_scores[_index_best_score_2][0] <= 0.0) {
                snprintf(_txtBest_1[1], 16, "--");
            } else {
                snprintf(_txtBest_1[1], 16, "%.2fs", _scores[_index_best_score_2][0]);
            }
            _pBest_1[1] = new StaticText(
                _pScroller->getViewRoot(),
                Point(40, y_pos), Size(100, score_height));
            _pBest_1[1]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pBest_1[1]->SetText((UINT8 *)_txtBest_1[1]);

            if (_scores[_index_best_score_2][1] <= 0.0) {
                snprintf(_txtBest_2[1], 16, "--");
            } else {
                snprintf(_txtBest_2[1], 16, "%.2fs", _scores[_index_best_score_2][1]);
                _p60MphBest = new BmpImage(
                    _pScroller->getViewRoot(),
                    Point(340, y_pos), Size(32, 32),
                    "/usr/local/share/ui/BMP/performance_test/",
                    "icn-best.bmp");
            }
            _pBest_2[1] = new StaticText(
                _pScroller->getViewRoot(),
                Point(240, y_pos), Size(100, score_height));
            _pBest_2[1]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pBest_2[1]->SetText((UINT8 *)_txtBest_2[1]);

            y_pos += 52;
        }
    }

    if (_num_latest <= 0) {
        _pLatest_1[0] = new StaticText(
            _pScroller->getViewRoot(),
            Point(0, y_pos), Size(200, score_height));
        _pLatest_1[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_Grey_2, CENTER, MIDDLE);
        _pLatest_1[0]->SetText((UINT8 *)"--");

        _pLatest_2[0] = new StaticText(
            _pScroller->getViewRoot(),
            Point(200, y_pos), Size(200, score_height));
        _pLatest_2[0]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_Grey_2, CENTER, MIDDLE);
        _pLatest_2[0]->SetText((UINT8 *)"--");
    } else {
        _pScore_Latest = new StaticText(
            _pScroller->getViewRoot(),
            Point(100, y_pos), Size(200, 40));
        _pScore_Latest->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
        _pScore_Latest->SetText((UINT8 *)"Latest");

        _pLine_Latest = new UILine(
            _pScroller->getViewRoot(),
            Point(140, y_pos + 44), Size(120, 4));
        _pLine_Latest->setLine(Point(140, 104), Point(260, 104), 4, Color_Grey_2);

        y_pos += 68;

        for (int i = 0; i < _num_latest; i++) {
            if (_scores[i][0] <= 0.0) {
                snprintf(_txtLatest_1[i], 16, "--");
            } else {
                snprintf(_txtLatest_1[i], 16, "%.2fs", _scores[i][0]);
            }
            _pLatest_1[i] = new StaticText(
                _pScroller->getViewRoot(),
                Point(40, y_pos), Size(100, score_height));
            _pLatest_1[i]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pLatest_1[i]->SetText((UINT8 *)_txtLatest_1[i]);

            if (_scores[i][1] <= 0.0) {
                snprintf(_txtLatest_2[i], 16, "--");
            } else {
                snprintf(_txtLatest_2[i], 16, "%.2fs", _scores[i][1]);
            }
            _pLatest_2[i] = new StaticText(
                _pScroller->getViewRoot(),
                Point(240, y_pos), Size(100, score_height));
            _pLatest_2[i]->SetStyle(font_size, FONT_ROBOTO_Regular, Color_White, RIGHT, MIDDLE);
            _pLatest_2[i]->SetText((UINT8 *)_txtLatest_2[i]);

            y_pos += 52;
        }

        y_pos -= 10;
        _pBtnClear = new Button(
            _pScroller->getViewRoot(),
            Point(0, y_pos), Size(400, 100), Color_Black, Color_Grey_1);
        _pBtnClear->SetText((UINT8 *)"Clear", 32, Color_White, FONT_ROBOTOMONO_Medium);
        _pBtnClear->SetTextAlign(CENTER, MIDDLE);
        _pBtnClear->setOnClickListener(this);
    }
}

void ScoreFragment::_destroyScoreScroller()
{
    delete _pBtnClear;
    _pBtnClear = NULL;

    for (int i = 0; i < 10; i++) {
        delete _pLatest_2[i];
        _pLatest_2[i] = NULL;
        delete _pLatest_1[i];
        _pLatest_1[i] = NULL;
    }
    delete _pLine_Latest;
    _pLine_Latest = NULL;
    delete _pScore_Latest;
    _pScore_Latest = NULL;

    delete _p30MphBest;
    _p30MphBest = NULL;
    delete _p60MphBest;
    _p60MphBest = NULL;

    for (int i = 0; i < 2; i++) {
        delete _pBest_2[i];
        _pBest_2[i] = NULL;
        delete _pBest_1[i];
        _pBest_1[i] = NULL;
    }

    delete _pScore_2;
    _pScore_2 = NULL;
    delete _pScore_1;
    _pScore_1 = NULL;

    delete _pLine;
    _pLine = NULL;
    delete _pTitle;
    _pTitle = NULL;

    delete _pScroller;
    _pScroller = NULL;
}

void ScoreFragment::onResume()
{
    char tmp[256];
    agcmd_property_get(PropName_Units_System, tmp, Imperial_Units_System);
    if (strcasecmp(tmp, Imperial_Units_System) == 0) {
        _bImperialUnits = true;
    } else {
        _bImperialUnits = false;
    }

    if (_num_latest < 0) {
        if (_loadScores()) {
        } else {
            _num_latest = 0;
        }

        _showScoreScroller();
        this->Draw(true);
    } else {
        if (_pScroller) {
            _pScroller->setViewPort(Point(0, 0), _visibleSizeScroller);
            this->Draw(true);
        }
    }
}

void ScoreFragment::onPause()
{
    //_storeFakeScores();
}

void ScoreFragment::onClick(Control *widget)
{
    if (widget == _pBtnClear) {
        _pTitle->SetHiden();
        _pLine->SetHiden();
        _pScroller->SetHiden();
        if (!_pHintInfo) {
            _pHintInfo = new StaticText(this, Point(0, 76), Size(400, 80));
            _pHintInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
            _pHintInfo->SetLineHeight(40);
            _pHintInfo->SetText((UINT8 *)"Sure to clear\nthe test scores?");
        } else {
            _pHintInfo->SetShow();
        }
        if (!_pIconYes) {
            _pIconYes = new ImageButton(
                this,
                Point(56, 216), Size(128, 128),
                "/usr/local/share/ui/BMP/dash_menu/",
                "btn-dashboard-yes_alt-up.bmp",
                "btn-dashboard-yes_alt-down.bmp");
            _pIconYes->setOnClickListener(this);
        } else {
            _pIconYes->SetShow();
        }
        if (!_pIconNo) {
            _pIconNo = new ImageButton(
                this,
                Point(216, 216), Size(128, 128),
                "/usr/local/share/ui/BMP/dash_menu/",
                "btn-dashboard-no-up.bmp",
                "btn-dashboard-no-down.bmp");
            _pIconNo->setOnClickListener(this);
        } else {
            _pIconNo->SetShow();
        }
        this->Draw(true);
    } else if (widget == _pIconYes) {
        _pTitle->SetShow();
        _pLine->SetShow();
        _pHintInfo->SetHiden();
        _pIconYes->SetHiden();
        _pIconNo->SetHiden();
        _destroyScoreScroller();

        if (_bImperialUnits) {
            if (Mode_CountDown == _mode) {
                agcmd_property_del(PropName_60MPH_Test_Score);
                agcmd_property_del(PropName_100KMH_Test_Score);
            } else if (Mode_Auto == _mode) {
                agcmd_property_del(PropName_60MPH_Test_Score_Auto);
                agcmd_property_del(PropName_100KMH_Test_Score_Auto);
            }
        } else {
            if (Mode_CountDown == _mode) {
                agcmd_property_del(PropName_100KMH_Test_Score);
            } else if (Mode_Auto == _mode) {
                agcmd_property_del(PropName_100KMH_Test_Score_Auto);
            }
        }
        _num_latest = 0;
        _showScoreScroller();
        this->Draw(true);
    } else if (widget == _pIconNo) {
        _pTitle->SetShow();
        _pLine->SetShow();
        _pScroller->setViewPort(Point(0, 120), _visibleSizeScroller);
        _pScroller->SetShow();
        _pHintInfo->SetHiden();
        _pIconYes->SetHiden();
        _pIconNo->SetHiden();
        this->Draw(true);
    }
}


CountdownModeScore::CountdownModeScore(
        const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren)
    : ScoreFragment(name, pParent, pOwner, leftTop, size, maxChildren,
        ScoreFragment::Mode_CountDown)
{
}

CountdownModeScore::~CountdownModeScore()
{
}


AutoModeScore::AutoModeScore(
        const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren)
    : ScoreFragment(name, pParent, pOwner, leftTop, size, maxChildren,
        ScoreFragment::Mode_Auto)
{
}

AutoModeScore::~AutoModeScore()
{
}


TestScoreWnd::TestScoreWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 8);
    this->SetMainObject(_pPanel);

    _pViewPager = new ViewPager(_pPanel, this, Point(0, 0), Size(400, 400), 4);

    _numPages = 0;
    _indexFocus = _numPages;
    _pPages[_numPages++] = new CountdownModeScore(
        "CountdownModeScore",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 40);

    _pPages[_numPages++] = new AutoModeScore(
        "AutoModeScore",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 40);

    _pViewPager->setPages(_pPages, _numPages);
    _pViewPager->setListener(this);

    _pIndicator = new TabIndicator(
        _pPanel, Point(140, 382), Size(120, 8),
        _numPages, _indexFocus,
        4, 0x9CF3, 0x3187);

    _pViewPager->setFocus(_indexFocus);
    _pIndicator->setHighlight(_indexFocus);

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
}

TestScoreWnd::~TestScoreWnd()
{
    delete _pReturnBtn;
    delete _pReturnBtnFake;

    delete _pIndicator;
    delete _pPages[1];
    delete _pPages[0];
    delete _pViewPager;

    delete _pPanel;
}

void TestScoreWnd::onFocusChanged(int indexFocus)
{
    _indexFocus = indexFocus;
    _pIndicator->setHighlight(indexFocus);
    _pPanel->Draw(true);
}

void TestScoreWnd::onViewPortChanged(const Point &leftTop)
{
    _pIndicator->Draw(false);
    _pReturnBtn->Draw(false);
}

void TestScoreWnd::onResume()
{
    _pViewPager->getCurFragment()->onResume();
    _pIndicator->setHighlight(_indexFocus);
}

void TestScoreWnd::onPause()
{
    _pViewPager->getCurFragment()->onPause();
}

void TestScoreWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

bool TestScoreWnd::OnEvent(CEvent *event)
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
                            if (storage_State_noMedia == st) {
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
                        } else if (storage_State_noMedia == st) {
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

};

