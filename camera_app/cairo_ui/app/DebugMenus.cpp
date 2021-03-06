
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#include "linux.h"
#include "agbox.h"
#include "aglib.h"
#include "agobd.h"
#include "agsys_iio.h"

#include "GobalMacro.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "Widgets.h"
#include "class_camera_property.h"
#include "CameraAppl.h"
#include "AudioPlayer.h"

#include "ColorPalette.h"
#include "WindowRegistration.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "upload_service/vdbupload.h"

#include "DebugMenus.h"

using namespace rapidjson;

extern bool bReleaseVersion;

namespace ui {

WavPlayWnd::WavPlayWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
  : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _numMenus(0)
{
    getFileList();

    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    strcpy(_slVal, "Wav List");
    _pSlVal = new StaticText(_pPanel, Point(0, 10), Size(400, 50));
    _pSlVal->SetStyle(24, 0xFFFF, CENTER, MIDDLE);
    _pSlVal->SetText((UINT8 *)_slVal);

    _pList = new List(_pPanel, this, Point(70, 60), Size(260, 280), 20);
    _pList->setListener(this);

    _pAdapter = new TextListAdapter();
    _pAdapter->setTextList((UINT8 **)_pMenus, _numMenus);
    _pAdapter->setItemStyle(Size(260, 70), 0x1111, 0x8410,
        0xFFFF, 24, LEFT, MIDDLE);
    _pAdapter->setObserver(_pList);

    _pList->setAdaptor(_pAdapter);
}

WavPlayWnd::~WavPlayWnd()
{
    delete _pSlVal;
    delete _pList;
    for (int i = 0; i < _numMenus; i++) {
        delete[] _pMenus[i];
    }
    delete _pAdapter;
    delete _pPanel;
}

void WavPlayWnd::getFileList()
{
    for (int i = 0; i < _numMenus; i++) {
        delete[] _pMenus[i];
    }
    _numMenus = 0;

    DIR *dir;
    struct dirent *ptr;
    char tmp[256];
    agcmd_property_get(PropName_Media_dir, tmp, "");
    dir = opendir(tmp);

    int i = 0;
    if (dir != NULL) {
        while ((ptr = readdir(dir)) != NULL) {
            if (strstr(ptr->d_name, ".wav") && (i < 31) && (ptr->d_type == DT_REG)) {
                //CAMERALOG("%dth: %s, type %d", i, ptr->d_name, ptr->d_type);
                _pMenus[i] = new UINT8[strlen(ptr->d_name) + 1];
                strcpy((char *)_pMenus[i], ptr->d_name);
                i++;
                if (i >= 32) {
                    break;
                }
            }
        }
        closedir(dir);
    }
    _numMenus = i;
    CAMERALOG("found %d files under %s", _numMenus, tmp);
}

void WavPlayWnd::onListClicked(Control *list, int index)
{
    if (_pList == list) {
        _pSlVal->SetShow();
        _pPanel->Draw(true);

        if (index < 0) {
            index = 0;
        } else if (index > _numMenus) {
            index = _numMenus;
        }
        char tmp[256];
        agcmd_property_get(PropName_Media_dir, tmp, "");
        char file[128];
        snprintf(file, sizeof(file), "%s/%s", tmp, _pMenus[index]);
        CAMERALOG("To play file %s", file);
        CAudioPlayer::getInstance()->playSound((const char*)file);
    }
}

void WavPlayWnd::onResume()
{
    //getFileList();
    _pAdapter->setTextList((UINT8 **)_pMenus, _numMenus);
    _pAdapter->notifyDataSetChanged();
}

void WavPlayWnd::onPause()
{
}

bool WavPlayWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                int x_pos = (position >> 16) & 0xFFFF;
                if ((x_pos < 70) || (x_pos > 330) || (y_pos < 60) || (y_pos >350)) {
                    this->Close(Anim_Top2BottomExit);
                    b = false;
                }
            }
        case eventType_internal:
            if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    storage_State st = _cp->getStorageState();
                    if ((st == storage_State_ready) ||
                        (st == storage_State_full) ||
                        (st == storage_State_noMedia))
                    {
                        getFileList();
                        _pAdapter->setTextList((UINT8 **)_pMenus, _numMenus);
                        _pAdapter->notifyDataSetChanged();
                    }
                    _pPanel->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}


TouchTestWnd::TouchTestWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pHLine = new UILine(_pPanel, Point(0, 200), Size(1, 400));
    _pHLine->setLine(Point(0, 200), Point(400, 200), 1, 0xF800);

    _pVLine = new UILine(_pPanel, Point(200, 0), Size(400, 1));
    _pVLine->setLine(Point(200, 0), Point(200, 400), 1, 0xF800);

    _pDrawingBoard = new DrawingBoard(_pPanel, Point(0, 0), Size(400, 400), Color_White);
}

TouchTestWnd::~TouchTestWnd()
{
    delete _pDrawingBoard;
    delete _pVLine;
    delete _pHLine;
    delete _pPanel;
}

void TouchTestWnd::onPause()
{
    CAMERALOG("#########################");
}

void TouchTestWnd::onResume()
{
    CAMERALOG("#########################");
}

bool TouchTestWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if (event->_event == TouchEvent_OnPressed) {
                _pPanel->Draw(true);
                b = false;
            } else if (event->_event == TouchEvent_coordinate) {
                //CAMERALOG("############### drawPoint (%d, %d)",
                //    event->_paload, event->_paload1);
                _pDrawingBoard->drawPoint(Point(event->_paload, event->_paload1));
                b = false;
            } else if ((event->_event == TouchEvent_flick) &&
                (event->_paload == TouchFlick_Down))
            {
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
            break;
    }

    return b;
}


ColorPalleteWnd::ColorPalleteWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pRound01 = new Round(_pPanel, Point(100, 100), 30, Color_Grey_1);

    _pRound02 = new Round(_pPanel, Point(200, 100), 30, Color_Grey_2);

    _pRound03 = new Round(_pPanel, Point(300, 100), 30, Color_Grey_3);

    _pRound04 = new Round(_pPanel, Point(100, 200), 30, Color_Grey_4);

    _pRound05 = new Round(_pPanel, Point(200, 200), 30, Color_White);

    _pRound06 = new Round(_pPanel, Point(300, 200), 30, Color_Yellow_2);

    _pRound07 = new Round(_pPanel, Point(100, 300), 30, Color_Red_1);

    _pRound08 = new Round(_pPanel, Point(200, 300), 30, Color_Green_1);

    _pRound09 = new Round(_pPanel, Point(300, 300), 30, Color_Blue_1);
}

ColorPalleteWnd::~ColorPalleteWnd()
{
    delete _pRound09;
    delete _pRound08;
    delete _pRound07;
    delete _pRound06;
    delete _pRound05;
    delete _pRound04;
    delete _pRound03;
    delete _pRound02;
    delete _pRound01;
    delete _pPanel;
}

void ColorPalleteWnd::onPause()
{
    CAMERALOG("#########################");
}

void ColorPalleteWnd::onResume()
{
    CAMERALOG("#########################");
}

bool ColorPalleteWnd::OnEvent(CEvent *event)
{
    bool b = true;

    return b;
}


FpsTestWnd::FpsTestWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _h_gforce(0.1)
  , _v_gforce(0.1)
  , _counter(0)
{
    snprintf(_text_12Clk, 16, "%s", "");
    snprintf(_text_3Clk, 16, "%s", "");
    snprintf(_text_6Clk, 16, "%s", "");
    snprintf(_text_9Clk, 16, "%s", "");
    snprintf(_fps, 32, "%s", "fps:");

    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pRound_bg = new Round(_pPanel, Point(200, 200), 100, 0x1515);

    _pLine_12Clk = new UILine(_pPanel, Point(200, 100), Size(2, 10));
    _pLine_12Clk->setLine(Point(200, 100), Point(200, 110), 2, 0xF890);

    _pLine_3Clk = new UILine(_pPanel, Point(300, 200), Size(10, 2));
    _pLine_3Clk->setLine(Point(290, 200), Point(300, 200), 2, 0xF890);

    _pLine_6Clk = new UILine(_pPanel, Point(200, 300), Size(2, 10));
    _pLine_6Clk->setLine(Point(200, 290), Point(200, 300), 2, 0xF890);

    _pLine_9Clk = new UILine(_pPanel, Point(100, 200), Size(10, 2));
    _pLine_9Clk->setLine(Point(100, 200), Point(110, 200), 2, 0xF890);

    _pTxt_title = new StaticText(_pPanel, Point(100, 300), Size(200, 100));
    _pTxt_title->SetStyle(50, 0x5555, CENTER, MIDDLE);
    _pTxt_title->SetText((UINT8 *)_fps);

    _pTxt_12Clk = new StaticText(_pPanel, Point(150, 48), Size(100, 50));
    _pTxt_12Clk->SetStyle(30, 0x9999, CENTER, BOTTOM);
    _pTxt_12Clk->SetText((UINT8 *)_text_12Clk);

    _pTxt_3Clk = new StaticText(_pPanel, Point(302, 175), Size(100, 50));
    _pTxt_3Clk->SetStyle(30, 0x9999, LEFT, MIDDLE);
    _pTxt_3Clk->SetText((UINT8 *)_text_3Clk);

    _pTxt_6Clk = new StaticText(_pPanel, Point(150, 302), Size(100, 50));
    _pTxt_6Clk->SetStyle(30, 0x9999, CENTER, TOP);
    _pTxt_6Clk->SetText((UINT8 *)_text_6Clk);

    _pTxt_9Clk = new StaticText(_pPanel, Point(0, 175), Size(98, 50));
    _pTxt_9Clk->SetStyle(30, 0x9999, RIGHT, MIDDLE);
    _pTxt_9Clk->SetText((UINT8 *)_text_9Clk);

    _pRound_center = new Round(_pPanel, Point(200, 200), 8, 0xAAAA);
    _pRound_indicator = new Round(_pPanel, Point(240, 200), 10, 0xAAFF);
    _pLine_indicator = new UILine(_pPanel, Point(200, 200), Size(5, 1));
    _pLine_indicator->setLine(Point(208, 200), Point(230, 200), 4, 0xAAAA);
    _pRound_hint = new Round(_pPanel, Point(292, 200), 6, 0xFFFF);
}

FpsTestWnd::~FpsTestWnd()
{
    delete _pRound_hint;
    delete _pLine_indicator;
    delete _pRound_indicator;
    delete _pRound_center;

    delete _pTxt_9Clk;
    delete _pTxt_6Clk;
    delete _pTxt_3Clk;
    delete _pTxt_12Clk;
    delete _pTxt_title;

    delete _pLine_9Clk;
    delete _pLine_6Clk;
    delete _pLine_3Clk;
    delete _pLine_12Clk;

    delete _pRound_bg;
    delete _pPanel;
}

void FpsTestWnd::updateSymbols() {
    float g_force = sqrt(_h_gforce * _h_gforce + _v_gforce * _v_gforce);
    if (g_force < 0.2) {
        //_pRound_center->SetHiden();
        _pRound_indicator->SetHiden();
        _pLine_indicator->SetHiden();
        _pRound_hint->SetHiden();
    } else {
        //_pRound_center->SetShow();
        _pRound_indicator->SetShow();
        _pLine_indicator->SetShow();
        _pRound_hint->SetShow();

        float theta = asin(_v_gforce / g_force);
        _pRound_indicator->setParams(
            Point(200 + g_force * 100 * cos(theta), 200 - g_force * 100 * sin(theta)),
            10,
            0xAAAF);
        _pRound_hint->setParams(
            Point(200 + (100 - 8) * cos(theta), 200 - (100 - 8) * sin(theta)),
            6,
            0xFFFF);
        _pLine_indicator->setLine(
            Point(200, 200),
            Point(200 + (g_force * 100 - 10) * cos(theta), 200 - (g_force * 100 - 10) * sin(theta)),
            4,
            0xAAAA);
    }

    if (_h_gforce == 0.0) {
        snprintf(_text_3Clk, 16, "%s", "");
        snprintf(_text_9Clk, 16, "%s", "");
    } else if (_h_gforce > 0.0) {
        snprintf(_text_3Clk, 16, "%.02f", _h_gforce);
        snprintf(_text_9Clk, 16, "%s", "");
    } else if (_h_gforce < 0.0) {
        snprintf(_text_3Clk, 16, "%s", "");
        snprintf(_text_9Clk, 16, "%.02f", _h_gforce);
    }

    if (_v_gforce == 0.0) {
        snprintf(_text_12Clk, 16, "%s", "");
        snprintf(_text_6Clk, 16, "%s", "");
    } else if (_v_gforce > 0.0) {
        snprintf(_text_12Clk, 16, "%.02f", _v_gforce);
        snprintf(_text_6Clk, 16, "%s", "");
    } else if (_v_gforce < 0.0) {
        snprintf(_text_12Clk, 16, "%s", "");
        snprintf(_text_6Clk, 16, "%.02f", _v_gforce);
    }
}

void FpsTestWnd::onPause()
{
}

void FpsTestWnd::onResume()
{
    _counter = 0;
    updateSymbols();
}

bool FpsTestWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if ((event->_event == InternalEvent_app_timer_update)) {
                struct timeval t_begin, t_end;
                gettimeofday(&t_begin, NULL);
                UINT16 color = 0x00;
                while (1) {
                    _counter++;
                    if (_counter % 50) {
                        gettimeofday(&t_end, NULL);
                        float sec = t_end.tv_sec - t_begin.tv_sec
                            + (t_end.tv_usec - t_begin.tv_usec) / 1000000.0;
                        float fps = _counter / sec;
                        snprintf(_fps, 32, "fps: %.2f", fps);
                    }

                    color++;
                    _pPanel->setBgColor(color);

                    _h_gforce = rand() % 101 / (double)(161);
                    if (_h_gforce > 0.6) {
                        _h_gforce = 0.6;
                    }
                    _v_gforce = rand() % 73 /(double)(151);
                    if (_v_gforce > 0.6) {
                        _v_gforce = 0.6;
                    }
                    updateSymbols();
                    _pPanel->Draw(true);
                }
            }
            break;
    }

    return b;
}


WifiModeTestWnd::WifiModeTestWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    snprintf(_txt, 64, "Wi-Fi");
    _pWifiInfo = new StaticText(_pPanel, Point(0, 0), Size(400, 160));
    _pWifiInfo->SetStyle(24, Color_White, CENTER, MIDDLE);
    _pWifiInfo->SetText((UINT8 *)_txt);

    _pApMode = new Button(_pPanel, Point(30, 160), Size(160, 70), 0x20C4, 0x8410);
    _pApMode->SetText((UINT8 *)"Hotspot", 32, Color_White);
    _pApMode->SetStyle(10);
    _pApMode->setOnClickListener(this);

    _pClientMode = new Button(_pPanel, Point(210, 160), Size(160, 70), 0x20C4, 0x8410);
    _pClientMode->SetText((UINT8 *)"Client", 32, Color_White);
    _pClientMode->SetStyle(10);
    _pClientMode->setOnClickListener(this);

    _pMultiRole = new Button(_pPanel, Point(30, 240), Size(160, 70), 0x20C4, 0x8410);
    _pMultiRole->SetText((UINT8 *)"MR", 32, Color_White);
    _pMultiRole->SetStyle(10);
    _pMultiRole->setOnClickListener(this);

    _pOff = new Button(_pPanel, Point(210, 240), Size(160, 70), 0x20C4, 0x8410);
    _pOff->SetText((UINT8 *)"OFF", 32, Color_White);
    _pOff->SetStyle(10);
    _pOff->setOnClickListener(this);

#if 0
    _pUpload = new Button(_pPanel, Point(80, 300), Size(160, 60), 0x20CF, 0x8410);
    _pUpload->SetText((UINT8 *)"UPLOAD", 36, Color_White);
    _pUpload->setOnClickListener(this);

    strcpy(_stat, "IDLE");
    _pStat = new StaticText(_pPanel, Point(240, 320), Size(100, 30));
    _pStat->SetStyle(24, 0xFFF2, CENTER, MIDDLE);
    _pStat->SetText((UINT8 *)_stat);
#endif
}

WifiModeTestWnd::~WifiModeTestWnd()
{
#if 0
    delete _pStat;
    delete _pUpload;
#endif
    delete _pOff;
    delete _pMultiRole;
    delete _pClientMode;
    delete _pApMode;
    delete _pWifiInfo;
    delete _pPanel;
}

int WifiModeTestWnd::getWLANIP(char *ip, int len)
{
    if (!ip || (len <= 0)) {
        return 0;
    }

    char wifi_prop[AGCMD_PROPERTY_SIZE];
    char ip_prop_name[AGCMD_PROPERTY_SIZE];
    char prebuild_prop[AGCMD_PROPERTY_SIZE];

    memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(wifiStatusPropertyName, wifi_prop, "NA");
    if (strcasecmp(wifi_prop, "on") == 0) {
        // wifi is on, continue
    } else {
        return 0;
    }

    memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(wifiModePropertyName, wifi_prop, "NA");
    if ((strcasecmp(wifi_prop, "MULTIROLE") == 0) ||
        (strcasecmp(wifi_prop, "WLAN") == 0))
    {
        memset(prebuild_prop, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");

        memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get("temp.earlyboot.if_wlan", wifi_prop, prebuild_prop);
    } else {
        return 0;
    }

    snprintf(ip_prop_name, AGCMD_PROPERTY_SIZE, "temp.%s.ip", wifi_prop);
    memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(ip_prop_name, wifi_prop, "NA");

    UINT32 rt = inet_addr(wifi_prop);
    //CAMERALOG("%s = %s, ip = %d", ip_prop_name, wifi_prop, rt);
    if ((rt != (UINT32)-1) && (rt != 0)) {
        if (ip) {
            snprintf(ip, len, "%s", wifi_prop);
        }
        return 1;
    }

    // IP is not ready.
    return 0;
}

void WifiModeTestWnd::updateWifi()
{
    wifi_mode mode = _cp->getWifiMode();
    char ssid[32];
    char key[32];
    switch (mode) {
        case wifi_mode_AP:
            _cp->GetWifiAPinfor(ssid, key);
            snprintf(_txt, 64, "AP: %s|%s", ssid, key);
            break;
        case wifi_mode_Client: {
            _cp->GetWifiCltInfor(ssid, 32);
            char ip[32] = {"null"};
            getWLANIP(ip, 32);
            snprintf(_txt, 64, "Client: %s\n(%s)", ssid, ip);
            break;
        }
        case wifi_mode_Multirole: {
            _cp->GetWifiAPinfor(ssid, key);
            snprintf(_txt, 64, "MR\nAP: %s|%s", ssid, key);
            _cp->GetWifiCltInfor(ssid, 32);
            char ip[32] = {"null"};
            getWLANIP(ip, 32);
            snprintf(_txt + strlen(_txt), 64 - strlen(_txt), "\nClient: %s\n(%s)", ssid, ip);
            break;
        }
        case wifi_mode_Off:
            snprintf(_txt, 64, "%s", "Wi-Fi off");
            break;
        default:
            break;
    }
}

void WifiModeTestWnd::onPause()
{
}

void WifiModeTestWnd::onResume()
{
    _counter = 0;
    updateWifi();
    _pPanel->Draw(true);
}

void WifiModeTestWnd::onClick(Control *widget)
{
    wifi_mode mode = _cp->getWifiMode();
    if ((widget == _pClientMode) && (mode != wifi_mode_Client)) {
        CAMERALOG("Set Client mode");
        snprintf(_txt, 64, "%s", "Changing mode ...");
        _pPanel->Draw(true);
        _cp->SetWifiMode(wifi_mode_Client);
    } else if ((widget == _pApMode) && (mode != wifi_mode_AP)) {
        CAMERALOG("Set AP mode");
        snprintf(_txt, 64, "%s", "Changing mode ...");
        _pPanel->Draw(true);
        _cp->SetWifiMode(wifi_mode_AP);
    } else if ((widget == _pMultiRole) && (mode != wifi_mode_Multirole)) {
        CAMERALOG("Set MultiRole mode");
        snprintf(_txt, 64, "%s", "Changing mode ...");
        _pPanel->Draw(true);
        _cp->SetWifiMode(wifi_mode_Multirole);
    } else if (widget == _pOff) {
        CAMERALOG("Set Wi-Fi off");
        snprintf(_txt, 64, "%s", "Changing mode ...");
        _pPanel->Draw(true);
        _cp->SetWifiMode(wifi_mode_Off);
    } else if (widget == _pUpload) {
        VdbUploader *uploader = new VdbUploader();
        uploader->setListenser(this);
        uploader->uploadMarkedClip();
    }
}

void WifiModeTestWnd::onState(int state)
{
    memset(_stat, 0x00, 16);
    switch (state)
    {
        case UploadListener::STATE_IDLE:
            snprintf(_stat, 32, "%s", "IDLE");
            break;
        case UploadListener::STATE_LOGIN:
            snprintf(_stat, 32, "%s", "LOGIN..");
            break;
        case UploadListener::STATE_SUCCESS:
            snprintf(_stat, 32, "%s", "SUCCESS");
            break;
        case UploadListener::STATE_FAILED:
            snprintf(_stat, 32, "%s", "FAILED");
            break;
        default:
            break;
    }

    _pPanel->Draw(true);
}

void WifiModeTestWnd::onProgress(int percentage)
{
    memset(_stat, 0x00, 16);
    snprintf(_stat, 32, "%d%%", percentage);
    _pPanel->Draw(true);
}

bool WifiModeTestWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if ((event->_event == InternalEvent_app_timer_update)) {
                _counter++;
                if (_counter % 50 == 0) {
                    updateWifi();
                    _pPanel->Draw(true);
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_Wifi) {
                    updateWifi();
                    _pPanel->Draw(true);
                    b = false;
                }
            }
            break;
        case eventType_touch:
            if ((event->_paload == TouchFlick_Down)) {
                this->Close(Anim_Top2BottomExit);
                b = false;
            }
            break;
    }

    return b;
}


DebugMenuWnd::DebugMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _pLogResult(NULL)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 4);
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

    _pList = new List(_pPanel, this, Point(70, 80), Size(260, 300), 20);
    _pList->setListener(this);

    _numMenus = 0;
    _pMenus[_numMenus++] = (UINT8*)"GPS signal";
    if (!bReleaseVersion) {
        _pMenus[_numMenus++] = (UINT8*)"copy log";
        _pMenus[_numMenus++] = (UINT8*)"touch test";
        _pMenus[_numMenus++] = (UINT8*)"OBD VIN";
        _pMenus[_numMenus++] = (UINT8*)"wav playback";
    }
    _pMenus[_numMenus++] = (UINT8*)"1080p23.976";
    _pMenus[_numMenus++] = (UINT8*)"OBD demo";
    _pMenus[_numMenus++] = (UINT8*)"Wi-Fi mode";
    _pMenus[_numMenus++] = (UINT8*)"Wi-Fi signal";
    _pMenus[_numMenus++] = (UINT8*)"NoWrite record";
    _pMenus[_numMenus++] = (UINT8*)"Temperature";

    _pAdapter = new TextListAdapter();
    _pAdapter->setTextList((UINT8 **)_pMenus, _numMenus);
    _pAdapter->setItemStyle(Size(260, 70), Color_Grey_1, Color_Grey_2,
        Color_White, 24, FONT_ROBOTOMONO_Light, LEFT, MIDDLE);
    _pAdapter->setObserver(_pList);

    _pList->setAdaptor(_pAdapter);
}

DebugMenuWnd::~DebugMenuWnd()
{
    delete _pLogResult;
    delete _pList;
    delete _pAdapter;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

int DebugMenuWnd::copyLog()
{
    time_t now;
    struct timeval now_val;
    time(&now);
    struct tm gtime;
    gtime=(*localtime(&now));
    gettimeofday(&now_val,NULL);

    char tmp[256];
    agcmd_property_get(PropName_Media_dir, tmp, "");
    char dirname[256];
    snprintf(dirname, sizeof(dirname), "%s/log/%02d%02d%02d_%02d_%02d_%02d",
        tmp, 1900 + gtime.tm_year, 1 + gtime.tm_mon, gtime.tm_mday,
        gtime.tm_hour, gtime.tm_min, gtime.tm_sec);
    int ret_val = aglib_make_dir(dirname, 0755, NULL);
    if (ret_val < 0) {
        CAMERALOG("aglib_make_dir failed with ret %d", ret_val);
        return ret_val;
    }

    char filename[256];
    snprintf(filename, 256, "%s/avf_media_server", dirname);
    ret_val = aglib_copy_file("/dev/avf_media_server", filename);
    if (ret_val < 0) {
        CAMERALOG("aglib_copy_file(%s) failed with ret %d", filename, ret_val);
        return ret_val;
    }

    snprintf(filename, 256, "%s/service_upnp", dirname);
    ret_val = aglib_copy_file("/dev/service_upnp", filename);
    if (ret_val < 0) {
        CAMERALOG("aglib_copy_file(%s) failed with ret %d", filename, ret_val);
        return ret_val;
    }

    snprintf(filename, 256, "%s/messages", dirname);
    ret_val = aglib_copy_file("/var/log/messages", filename);
    if (ret_val < 0) {
        CAMERALOG("aglib_copy_file(%s) failed with ret %d", filename, ret_val);
        return ret_val;
    }

    return ret_val;
}

void DebugMenuWnd::onMenuResume()
{
}

void DebugMenuWnd::onMenuPause()
{
}

void DebugMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void DebugMenuWnd::onListClicked(Control *list, int index)
{
    if ((_pList == list) && (index >= 0) && (index < _numMenus)) {
        if (strcasecmp((const char*)_pMenus[index], "copy log") == 0) {
            _pLogResult = new StaticText(_pPanel, Point(0, 150), Size(400, 100));
            _pLogResult->SetStyle(64, 0xF000, CENTER, MIDDLE);
            _pLogResult->SetAlpha(0.8);
            if (copyLog() < 0) {
                _pLogResult->SetText((UINT8 *)"Copy Failed");
            } else {
                _pLogResult->SetText((UINT8 *)"Copy Success");
            }
            _pPanel->Draw(true);
            usleep(500 * 1000);
            this->Close(Anim_Top2BottomExit);
        } else if (strcasecmp((const char*)_pMenus[index], "touch test") == 0) {
            this->StartWnd(WINDOW_touch, Anim_Bottom2TopCoverEnter);
        } else if (strcasecmp((const char*)_pMenus[index], "OBD VIN") == 0) {
            this->StartWnd(WINDOW_obd_vin, Anim_Bottom2TopCoverEnter);
        } else if (strcasecmp((const char*)_pMenus[index], "OBD demo") == 0) {
            this->StartWnd(WINDOW_obd_preferences, Anim_Bottom2TopCoverEnter);
        } else if (strcasecmp((const char*)_pMenus[index], "Wi-Fi mode") == 0) {
            this->StartWnd(WINDOW_wifi_mode_test, Anim_Bottom2TopCoverEnter);
        } else if (strcasecmp((const char*)_pMenus[index], "Wi-Fi signal") == 0) {
            this->StartWnd(WINDOW_wifi_signal_test, Anim_Bottom2TopCoverEnter);
        } else if (strcasecmp((const char*)_pMenus[index], "wav playback") == 0) {
            this->StartWnd(WINDOW_wav_play, Anim_Bottom2TopCoverEnter);
        } else if (strcasecmp((const char*)_pMenus[index], "GPS signal") == 0) {
            this->StartWnd(WINDOW_gps_test, Anim_Bottom2TopCoverEnter);
        } else if (strcasecmp((const char*)_pMenus[index], "1080p23.976") == 0) {
            this->StartWnd(WINDOW_option_23976_fps, Anim_Bottom2TopCoverEnter);
        } else if (strcasecmp((const char*)_pMenus[index], "NoWrite record") == 0) {
            this->StartWnd(WINDOW_no_write_record, Anim_Bottom2TopCoverEnter);
        } else if (strcasecmp((const char*)_pMenus[index], "Temperature") == 0) {
            this->StartWnd(WINDOW_temperature, Anim_Bottom2TopCoverEnter);
        }
    }
}

bool DebugMenuWnd::OnEvent(CEvent *event)
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
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}


WifiSignalDebugWnd::WifiSignalDebugWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _wifiList(NULL)
    , _wifiNum(0)
    , _cnt(0)
{
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

    _pTitle = new StaticText(_pPanel, Point(90, 60), Size(220, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Wi-Fi Signal");

    _pLine = new UILine(_pPanel, Point(70, 104), Size(260, 4));
    _pLine->setLine(Point(70, 104), Point(330, 104), 4, Color_Grey_2);

    _pList = new List(_pPanel, this, Point(40, 110), Size(320, 280), 20);
    _pList->setListener(this);

    _wifiNum = 0;
    _pAdapter = new TextListAdapter();
    _pAdapter->setTextList((UINT8 **)_wifiList, _wifiNum);
    _pAdapter->setItemStyle(Size(320, 70), Color_Grey_1, Color_Grey_2,
        Color_White, 24, FONT_ROBOTOMONO_Light, CENTER, MIDDLE);
    _pAdapter->setObserver(_pList);

    _pList->setAdaptor(_pAdapter);
}

WifiSignalDebugWnd::~WifiSignalDebugWnd()
{
    delete _pList;
    delete _pAdapter;

    delete _pLine;
    delete _pTitle;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;

    for (int i = 0; i < _wifiNum; i++) {
        delete [] _wifiList[i];
    }
    delete [] _wifiList;
    _wifiNum = 0;
    _wifiList = NULL;
}

void WifiSignalDebugWnd::_parseWifi(char *json)
{
    Document d;
    d.Parse(json);
    if(!d.IsObject()) {
        CAMERALOG("Didn't get a JSON object!");
        return;
    }

    int added = 0;
    int index = 0;
    char **list = NULL;
    if (d.HasMember("networks")) {
        const Value &a = d["networks"];
        if (a.IsArray()) {
            added = a.Size();
            if (added <= 0) {
                goto parse_success;
            }

            list = new char* [added];
            if (!list) {
                CAMERALOG("Out of memory!");
                goto parse_error;
            }
            memset(list, 0x00, sizeof(char*) * added);
            index = 0;
            for (SizeType i = 0; (i < a.Size()) && (index < added); i++) {
                const Value &item = a[i];
                const char *name = item["ssid"].GetString();
                list[index] = new char[strlen(name) + 16];
                if (!list[index]) {
                    CAMERALOG("Out of memory!");
                    goto parse_error;
                }
                snprintf(list[index], strlen(name) + 16, "%s %s  :  %d",
                    item["current"].GetBool() ? "*" : " ",
                    item["ssid"].GetString(),
                    item["signal_level"].GetInt());
                index++;
            }
        }
    }

parse_success:
    // Firstly, free the list which is out of date
    for (int i = 0; i < _wifiNum; i++) {
        delete [] _wifiList[i];
    }
    _wifiNum = 0;
    delete [] _wifiList;

    // Then, use the new list
    _wifiNum = added;
    _wifiList = list;
    //for (int i = 0; i < _wifiNum; i++) {
    //    CAMERALOG("Saved connections: %dth %s", i, _wifiList[i]);
    //}
    _pAdapter->setTextList((UINT8 **)_wifiList, _wifiNum);
    _pAdapter->notifyDataSetChanged();
    _pPanel->Draw(true);

    return;

parse_error:
    for (int i = 0; i < _wifiNum; i++) {
        delete [] _wifiList[i];
    }
    delete [] _wifiList;
    _wifiNum = 0;
    _wifiList = NULL;

    for (int i = 0; i < index; i++) {
        delete [] list[i];
    }
    delete [] list;
}

void WifiSignalDebugWnd::onMenuResume()
{
    _cp->ScanNetworks();
}

void WifiSignalDebugWnd::onMenuPause()
{
}

void WifiSignalDebugWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void WifiSignalDebugWnd::onListClicked(Control *list, int index)
{
    if ((_pList == list) && (index >= 0) && (index < _wifiNum)) {
    }
}

bool WifiSignalDebugWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _cnt++;
                if (_cnt % 50 == 0) {
                    _cp->ScanNetworks();
                }
            } else if (event->_event ==InternalEvent_app_state_update) {
                if (event->_paload == camera_State_scanwifi_result) {
                    char *str = (char *)event->_paload1;
                    //CAMERALOG("str = %s", str);
                    _parseWifi(str);
                    _pPanel->Draw(true);
                    b = false;
                }
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
            }
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}


GPSDebugWnd::GPSDebugWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _numSat(0)
    , _pSatSNR(NULL)
    , _pSNR(NULL)
    , _pSatPRN(NULL)
    , _paggps_client(NULL)
    , _cnt(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 50);
    this->SetMainObject(_pPanel);

    memset(_txtNum, 0x00, sizeof(_txtNum));
    snprintf(_txtNum, sizeof(_txtNum), "GPS TEST");
    _pTitle = new StaticText(_pPanel, Point(0, 20), Size(400, 80));
    _pTitle->SetStyle(20, FONT_ROBOTOMONO_Medium, Color_Grey_4, CENTER, MIDDLE);
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

    memset(_txtInfo, 0x00, sizeof(_txtInfo));
    _pSatInfo = new StaticText(_pPanel, Point(20, 270), Size(360, 130));
    _pSatInfo->SetStyle(20, FONT_ROBOTOMONO_Medium, Color_White, CENTER, TOP);
    _pSatInfo->SetText((UINT8 *)_txtInfo);

    _paggps_client = aggps_open_client();
}

GPSDebugWnd::~GPSDebugWnd()
{
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

void GPSDebugWnd::_printfGPSInfo()
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
    if (plocation->set & AGGPS_SET_DOP) {
        if (sizeof(gps_buffer) > gb_offset) {
            gb_offset += snprintf((gps_buffer + gb_offset),
                (sizeof(gps_buffer) - gb_offset),
                "%4.2fH/%4.2fV/%4.2fPDOP\n",
                plocation->hdop, plocation->vdop, plocation->pdop);
        }
    }
    if (plocation->set & AGGPS_SET_SPEED) {
        if (sizeof(gps_buffer) > gb_offset) {
            gb_offset += snprintf((gps_buffer + gb_offset),
                (sizeof(gps_buffer) - gb_offset),
                " %4.2fkm/h %4.1fM\n",
                plocation->speed, plocation->altitude);
        }
    }
    if (plocation->set & AGGPS_SET_TRACK) {
        if (sizeof(gps_buffer) > gb_offset) {
            gb_offset += snprintf((gps_buffer + gb_offset),
                (sizeof(gps_buffer) - gb_offset),
                " %4.2fTrac", plocation->track);
        }
    }

    //CAMERALOG("%.*s %.*s", (int)db_offset, date_buffer,
    //    (int)gb_offset, gps_buffer);
    snprintf(_txtInfo, sizeof(_txtInfo), "%.*s\n%.*s", (int)db_offset, date_buffer,
        (int)gb_offset, gps_buffer);
}

void GPSDebugWnd::_updateStatus()
{
    if (_paggps_client == NULL) {
        _paggps_client = aggps_open_client();
        if (_paggps_client == NULL) {
            snprintf(_txtNum, sizeof(_txtNum), "GPS Open Failed");
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
    memset(_txtInfo, 0x00, sizeof(_txtInfo));

    int ret_val = aggps_read_client(_paggps_client);
    if (ret_val) {
        snprintf(_txtNum, sizeof(_txtNum), "GPS Read Failed");
        return;
    }

    struct aggps_satellite_info_s *psatellite = &_paggps_client->pgps_info->satellite;
    _numSat = psatellite->num_svs;

    float update_rate = 1;
    if (_paggps_client->pgps_info->devinfo.output_rate_div) {
        update_rate = _paggps_client->pgps_info->devinfo.output_rate_mul;
        update_rate /= _paggps_client->pgps_info->devinfo.output_rate_div;
    }
    if (bReleaseVersion) {
        snprintf(_txtNum, sizeof(_txtNum),
            "SIV: %d %4.2fHZ", _numSat, update_rate);
    } else {
        snprintf(_txtNum, sizeof(_txtNum),
            "SIV: %d %4.2fHZ\n%s/%s", _numSat, update_rate,
            _paggps_client->pgps_info->devinfo.dev_name,
            &_paggps_client->pgps_info->devinfo.dev_sn[5]);
    }
    //CAMERALOG("%d satellites found.", num);

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

        UINT16 rect_color = Color_Grey_3;

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
            _pSatPRN[i]->SetStyle(font_size, FONT_ROBOTOMONO_Light, Color_Blue_1, CENTER, MIDDLE);
            _pSatPRN[i]->SetText((UINT8 *)_txtPRN[i]);

            int height = snr * 180 / 60.0;
            if (height > 180) {
                height = 180;
            }

            int sv_active = 0;
            for (int j = 0; j < AGGPS_MAX_SVS; j++) {
                if (prn == psatellite->sv_active[j]) {
                    sv_active = 1;
                    break;
                }
            }
            if (sv_active) {
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
            _pSatSNR[i]->SetStyle(font_size, FONT_ROBOTOMONO_Medium, Color_White, CENTER, MIDDLE);
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

void GPSDebugWnd::onMenuResume()
{
}

void GPSDebugWnd::onMenuPause()
{
}

bool GPSDebugWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _cnt++;
                if (_cnt % 10 == 0) {
                    _updateStatus();
                    _pPanel->Draw(true);
                }
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
            }
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}


OBDVINWnd::OBDVINWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 8);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), 0x0000, 0x0000);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 42));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"OBD VIN");

    _pLine = new UILine(_pPanel, Point(120, 106), Size(160, 4));
    _pLine->setLine(Point(120, 100), Point(280, 100), 4, Color_Grey_2);

    snprintf(_txtVINOrign, sizeof(_txtVINOrign), " ");
    _pVINOrign = new StaticText(_pPanel, Point(50, 110), Size(300, 60));
    _pVINOrign->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_White, CENTER, MIDDLE);
    _pVINOrign->SetText((UINT8 *)_txtVINOrign);

    snprintf(_txtMASK, sizeof(_txtMASK), " ");
    _pMASK = new StaticText(_pPanel, Point(50, 170), Size(300, 60));
    _pMASK->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_White, CENTER, MIDDLE);
    _pMASK->SetText((UINT8 *)_txtMASK);

    snprintf(_txtVINMASK, sizeof(_txtVINMASK), " ");
    _pVINMASK = new StaticText(_pPanel, Point(50, 230), Size(300, 60));
    _pVINMASK->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_White, CENTER, MIDDLE);
    _pVINMASK->SetText((UINT8 *)_txtVINMASK);
}

OBDVINWnd::~OBDVINWnd()
{
    delete _pVINMASK;
    delete _pMASK;
    delete _pVINOrign;

    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void OBDVINWnd::_updateStatus()
{
    memset(_txtMASK, 0x00, sizeof(_txtMASK));
    agcmd_property_get(PropName_VIN_MASK, _txtMASK, Default_VIN_MASK);

    struct agobd_client_info_s *obd_client = agobd_open_client();
    if (obd_client) {
        int ret_val = agobd_client_get_info(obd_client);
        if (ret_val == 0) {
            struct agobd_info_s *info = obd_client->pobd_info;
            const char *pvin = (const char*)info + info->vin_offset;
            if (*pvin != 0x00) {
                memset(_txtVINOrign, 0x00, sizeof(_txtVINOrign));
                snprintf(_txtVINOrign, sizeof(_txtVINOrign), "%s", pvin);

                memset(_txtVINMASK, 0x00, sizeof(_txtVINMASK));
                for (unsigned int i = 0;
                    (i < strlen(_txtMASK)) && (i < strlen(_txtVINOrign));
                    i++)
                {
                    if (_txtMASK[i] == '-') {
                        _txtVINMASK[i] = '-';
                    } else {
                        _txtVINMASK[i] = _txtVINOrign[i];
                    }
                }
            }
        }
    }
}

void OBDVINWnd::onMenuResume()
{
    _updateStatus();
    _pPanel->Draw(true);
}

void OBDVINWnd::onMenuPause()
{
}

void OBDVINWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}


bool OBDVINWnd::OnEvent(CEvent *event)
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
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}


Opt23976FpsMenuWnd::Opt23976FpsMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 8);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), 0x0000, 0x0000);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pInfo = new StaticText(_pPanel, Point(40, 90), Size(320, 160));
    _pInfo->SetStyle(28, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, TOP);
    _pInfo->SetLineHeight(40);
    snprintf(_txt, sizeof(_txt),
        "Enable it to record"
        "\n"
        "video with resolution"
        "\n"
        "1920x1080 fps 23.976"
        "\n"
        "(1080p24 for short).");
    _pInfo->SetText((UINT8 *)_txt);

    _pSwitchFake = new Button(
        _pPanel,
        Point(0, 250), Size(400, 80),
        Color_Black, Color_Grey_1);
    _pSwitchFake->setOnClickListener(this);

    _pStatus = new StaticText(
        _pPanel,
        Point(40, 270), Size(200, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"1080p24");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(304, 274), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");
}

Opt23976FpsMenuWnd::~Opt23976FpsMenuWnd()
{
    delete _pSwitch;
    delete _pStatus;
    delete _pSwitchFake;
    delete _pInfo;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void Opt23976FpsMenuWnd::updateStatus()
{
    char tmp[256];
    agcmd_property_get(PropName_support_2397_fps, tmp, Disabled_2397_fps);
    if (strcasecmp(tmp, Enabled_2397_fps) == 0) {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }
}

void Opt23976FpsMenuWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void Opt23976FpsMenuWnd::onMenuPause()
{
}

void Opt23976FpsMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pSwitchFake) {
        char tmp[256];
        agcmd_property_get(PropName_support_2397_fps, tmp, Disabled_2397_fps);
        if (strcasecmp(tmp, Enabled_2397_fps) == 0) {
            agcmd_property_set(PropName_support_2397_fps, Disabled_2397_fps);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_off.bmp");

            memset(tmp, 0x00, sizeof(tmp));
            agcmd_property_get(PropName_rec_resoluton, tmp, "1080p30");
            if (strcasecmp(tmp, "1080p23976") == 0) {
                // change to default 1080p30
                agcmd_property_set(PropName_rec_resoluton, "1080p30");
            }
        } else {
            agcmd_property_set(PropName_support_2397_fps, Enabled_2397_fps);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_on.bmp");
            // change to 23.976fps
            agcmd_property_set(PropName_rec_resoluton, "1080p23976");
        }
        _pPanel->Draw(true);
    }
}

bool Opt23976FpsMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                this->Close(Anim_Top2BottomExit);
                b = false;
            }
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}


NoWriteRecordMenuWnd::NoWriteRecordMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 8);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), 0x0000, 0x0000);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pInfo = new StaticText(_pPanel, Point(40, 90), Size(320, 160));
    _pInfo->SetStyle(28, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, TOP);
    _pInfo->SetLineHeight(40);
    snprintf(_txt, sizeof(_txt),
        "Enable it to record"
        "\n"
        "video but don't write to"
        "\n"
        "SD card. It takes effect"
        "\n"
        "until shut down.");
    _pInfo->SetText((UINT8 *)_txt);

    _pSwitchFake = new Button(
        _pPanel,
        Point(0, 250), Size(400, 80),
        Color_Black, Color_Grey_1);
    _pSwitchFake->setOnClickListener(this);

    _pStatus = new StaticText(
        _pPanel,
        Point(40, 270), Size(260, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"NoWrite record");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(304, 274), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");
}

NoWriteRecordMenuWnd::~NoWriteRecordMenuWnd()
{
    delete _pSwitch;
    delete _pStatus;
    delete _pSwitchFake;
    delete _pInfo;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void NoWriteRecordMenuWnd::updateStatus()
{
    char tmp[256];
    agcmd_property_get(PropName_No_Write_Record, tmp, No_Write_Record_Disabled);
    if (strcasecmp(tmp, No_Write_Record_Enalbed) == 0) {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }
}

void NoWriteRecordMenuWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void NoWriteRecordMenuWnd::onMenuPause()
{
}

void NoWriteRecordMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pSwitchFake) {
        char tmp[256];
        agcmd_property_get(PropName_No_Write_Record, tmp, No_Write_Record_Disabled);
        if (strcasecmp(tmp, No_Write_Record_Enalbed) == 0) {
            agcmd_property_set(PropName_No_Write_Record, No_Write_Record_Disabled);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_off.bmp");
        } else {
            agcmd_property_set(PropName_No_Write_Record, No_Write_Record_Enalbed);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_on.bmp");
        }
        _pPanel->Draw(true);

        // stop record
    }
}

bool NoWriteRecordMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                this->Close(Anim_Top2BottomExit);
                b = false;
            }
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}


TemperatureMenuWnd::TemperatureMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _cnt(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 10);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), 0x0000, 0x0000);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 42));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Temperature");

    _pLine = new UILine(_pPanel, Point(70, 106), Size(260, 4));
    _pLine->setLine(Point(70, 100), Point(330, 100), 4, Color_Grey_2);


    _pBoard = new StaticText(_pPanel, Point(40, 110), Size(160, 80));
    _pBoard->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pBoard->SetText((UINT8 *)"Board");

    snprintf(_txtBoardTemp, sizeof(_txtBoardTemp), "Unknown");
    _pBoardTemp = new StaticText(_pPanel, Point(200, 110), Size(160, 80));
    _pBoardTemp->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, RIGHT, MIDDLE);
    _pBoardTemp->SetText((UINT8 *)_txtBoardTemp);


    _pSensor = new StaticText(_pPanel, Point(40, 190), Size(160, 80));
    _pSensor->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pSensor->SetText((UINT8 *)"Sensor");

    snprintf(_txtSensorTemp, sizeof(_txtSensorTemp), "Unknown");
    _pSensorTemp = new StaticText(_pPanel, Point(200, 190), Size(160, 80));
    _pSensorTemp->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, RIGHT, MIDDLE);
    _pSensorTemp->SetText((UINT8 *)_txtSensorTemp);


    _pWifi = new StaticText(_pPanel, Point(40, 270), Size(160, 80));
    _pWifi->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pWifi->SetText((UINT8 *)"Wifi");

    snprintf(_txtWifiTemp, sizeof(_txtWifiTemp), "Unknown");
    _pWifiTemp = new StaticText(_pPanel, Point(200, 270), Size(160, 80));
    _pWifiTemp->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, RIGHT, MIDDLE);
    _pWifiTemp->SetText((UINT8 *)_txtWifiTemp);
}

TemperatureMenuWnd::~TemperatureMenuWnd()
{
    delete _pWifiTemp;
    delete _pWifi;

    delete _pSensorTemp;
    delete _pSensor;

    delete _pBoardTemp;
    delete _pBoard;

    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void TemperatureMenuWnd::_updateStatus()
{
    char tmp[256] = {0x00};
    agcmd_property_get(PropName_Board_Temprature, tmp, "450");
    int board_temprature = atoi(tmp);
    snprintf(_txtBoardTemp, sizeof(_txtBoardTemp), "%d.%d C",
        board_temprature / 10, board_temprature % 10);

    struct agsys_iio_temp_info_s *temp = agsys_iio_temp_open_by_name("bmp280");
    if (temp) {
        struct agsys_iio_temp_s temp_struct;
        memset(&temp_struct, 0x00, sizeof(temp_struct));
        int ret = agsys_iio_temp_read(temp, &temp_struct);
        if (!ret) {
            float sensor_temp = temp_struct.temperature * temp->temp_scale;
            snprintf(_txtSensorTemp, sizeof(_txtSensorTemp), "%.1f C", sensor_temp);
        }
        agsys_iio_temp_close(temp);
    }

    temp = agsys_iio_temp_open_by_name("bno055");
    if (temp) {
        struct agsys_iio_temp_s temp_struct;
        memset(&temp_struct, 0x00, sizeof(temp_struct));
        int ret = agsys_iio_temp_read(temp, &temp_struct);
        if (!ret) {
            float wifi_temp = temp_struct.temperature * temp->temp_scale;
            snprintf(_txtWifiTemp, sizeof(_txtWifiTemp), "%.1f C", wifi_temp);
        }
        agsys_iio_temp_close(temp);
    }
}

void TemperatureMenuWnd::onMenuResume()
{
    _updateStatus();
    _pPanel->Draw(true);
}

void TemperatureMenuWnd::onMenuPause()
{
}

void TemperatureMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}


bool TemperatureMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _cnt++;
                if (_cnt % 50 == 0) {
                    // update every 5 seconds
                    _updateStatus();
                    _pPanel->Draw(true);
                }
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
            }
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}

};

