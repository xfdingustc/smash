
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#include "linux.h"
#include "agbox.h"

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

#include "WiFi.h"

extern bool is_no_write_record();

using namespace rapidjson;

namespace ui{

WifiMenuWnd::WifiMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _counter(0)
    , _cntTimeout(0)
    , _targetMode(wifi_mode_Off)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 4);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(100, 0), Size(200, 88), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _visibleSizeScroller.width = 400;
    _visibleSizeScroller.height = 312;
    _scrollableSize.width = 400;
    _scrollableSize.height = 720;
    _pScroller = new Scroller(
        _pPanel,
        this,
        Point(0, 88),
        _visibleSizeScroller,
        _scrollableSize,
        20);
    initScroller();

    _pAnimation = new BmpImage(
        _pPanel,
        Point(136, 160), Size(128, 128),
        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
        "power_OFF-spinner_00000.bmp");
    _pAnimation->SetHiden();
}

WifiMenuWnd::~WifiMenuWnd()
{
    delete _pAutoAPInfo;
    delete _pAutoAP;
    delete _pAutoAPFake;

    delete _pQrCode;
    delete _pQRBG;
    delete _pPWDValue;
    delete _pPWD;
    delete _pSSIDValue;
    delete _pSSID;

    delete _pWifiInfo;
    delete _pClient;
    delete _pClientInfoFake;

    delete _pModeResult;
    delete _pMode;
    delete _pModeFake;
    delete _pSwitch;
    delete _pSwitchFake;
    delete _pStatus;

    delete _pAnimation;

    delete _pScroller;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void WifiMenuWnd::initScroller()
{
    // All position are relative to scroller:
    _pSwitchFake = new Button(
        _pScroller->getViewRoot(),
        Point(0, 0), Size(_visibleSizeScroller.width, 70),
        Color_Black, Color_Grey_1);
    _pSwitchFake->setOnClickListener(this);

    _pStatus = new StaticText(
        _pScroller->getViewRoot(),
        Point(30, 10), Size(150, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"Wi-Fi");

    _pSwitch = new BmpImage(
        _pScroller->getViewRoot(),
        Point(314, 14), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");

    _pModeFake= new Button(
        _pScroller->getViewRoot(),
        Point(0, 70), Size(_visibleSizeScroller.width, 80),
        Color_Black, Color_Black);
    _pModeFake->setOnClickListener(this);

    _pMode = new StaticText(
        _pScroller->getViewRoot(),
        Point(30, 90), Size(160, 40));
    _pMode->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pMode->SetText((UINT8 *)"Mode");

    strcpy(_txtMode, "Hotspot");
    _pModeResult = new StaticText(
        _pScroller->getViewRoot(),
        Point(200, 90), Size(170, 40));
    _pModeResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pModeResult->SetText((UINT8 *)_txtMode);

    // to show client ssid
    _pClientInfoFake= new Button(
        _pScroller->getViewRoot(),
        Point(0, 150), Size(_visibleSizeScroller.width, 80),
        Color_Black, Color_Grey_1);
    _pClientInfoFake->setOnClickListener(this);
    _pClientInfoFake->SetHiden();

    _pClient = new StaticText(
        _pScroller->getViewRoot(),
        Point(30, 170),
        Size(104, 40));
    _pClient->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pClient->SetText((UINT8 *)"Status");
    _pClient->SetHiden();

    memset(_txtWifiInfo, 0x00, sizeof(_txtWifiInfo));
    _pWifiInfo = new StaticText(
        _pScroller->getViewRoot(),
        Point(150, 170),
        Size(220, 40));
    _pWifiInfo->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pWifiInfo->SetText((UINT8 *)_txtWifiInfo);
    _pWifiInfo->SetHiden();

    // to show ap ssid / password / qrcode
    _pSSID = new StaticText(
        _pScroller->getViewRoot(),
        Point(30, 170),
        Size(160, 40));
    _pSSID->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pSSID->SetText((UINT8 *)"SSID");
    _pSSID->SetHiden();

    memset(_txtSSID, 0x00, sizeof(_txtSSID));
    _pSSIDValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(200, 170),
        Size(170, 40));
    _pSSIDValue->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pSSIDValue->SetText((UINT8 *)_txtSSID);

    _pPWD = new StaticText(
        _pScroller->getViewRoot(),
        Point(30, 250),
        Size(170, 40));
    _pPWD->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pPWD->SetText((UINT8 *)"PWD");
    _pPWD->SetHiden();

    memset(_txtPWD, 0x00, sizeof(_txtPWD));
    _pPWDValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(200, 250),
        Size(170, 40));
    _pPWDValue->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pPWDValue->SetText((UINT8 *)_txtPWD);

    _pQRBG = new UIRectangle(
        _pScroller->getViewRoot(),
        Point(90, 330),
        Size(220, 220),
        Color_White);
    _pQRBG->setCornerRadius(16);

    _pQrCode = new QrCodeControl(
        _pScroller->getViewRoot(),
        Point(100, 340),
        Size(200, 200));


    _pAutoAPFake = new Button(
        _pScroller->getViewRoot(),
        Point(0, 570), Size(_visibleSizeScroller.width, 80),
        Color_Black, Color_Grey_1);
    _pAutoAPFake->setOnClickListener(this);

    _pAutoAP = new StaticText(
        _pScroller->getViewRoot(),
        Point(30, 590),
        Size(200, 40));
    _pAutoAP->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pAutoAP->SetText((UINT8 *)"Auto Switch");

    memset(_txtAutoAP, 0x00, sizeof(_txtAutoAP));
    _pAutoAPInfo = new StaticText(
        _pScroller->getViewRoot(),
        Point(230, 590),
        Size(140, 40));
    _pAutoAPInfo->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pAutoAPInfo->SetText((UINT8 *)_txtAutoAP);
}

void WifiMenuWnd::updateWifi()
{
    agcmd_property_get(PropName_Auto_Hotspot, _txtAutoAP, Auto_Hotspot_Enabled);

    wifi_mode mode = _cp->getWifiMode();
    char ssid[256];
    memset(ssid, 0x00, 256);
    char key[128];
    memset(key, 0x00, 128);
    switch (mode) {
        case wifi_mode_Multirole:
            //CAMERALOG("########### wifi_mode_Multirole");
        case wifi_mode_AP:
            _pSwitch->setSource(
                "/usr/local/share/ui/BMP/dash_menu/",
                "toggle_on.bmp");
            strcpy(_txtMode, "Hotspot");
            _pModeResult->SetText((UINT8 *)_txtMode);
            _cp->GetWifiAPinfor(ssid, key);

            _pClient->SetHiden();
            _pWifiInfo->SetHiden();
            _pClientInfoFake->SetHiden();

            _pSSID->SetShow();
            _pSSIDValue->SetShow();

            _pPWD->SetShow();
            _pPWDValue->SetShow();

            _pQrCode->SetShow();
            _pQRBG->SetShow();
            snprintf(_txtSSID, sizeof(_txtSSID), "%s", ssid);
            snprintf(_txtPWD, sizeof(_txtPWD), "%s", key);
            char tmp[64];
            memset(tmp, 0x00, sizeof(tmp));
            snprintf(tmp, 64, "<a>%s</a><p>%s</p>", ssid, key);
            //CAMERALOG("wifi info: %s", tmp);
            _pQrCode->setString(tmp);

            _pAutoAPFake->SetRelativePos(Point(0, 570));
            _pAutoAP->SetRelativePos(Point(30, 590));
            _pAutoAPInfo->SetRelativePos(Point(230, 590));

            _pScroller->setScrollable(true);
            _pScroller->setScrollableSize(_scrollableSize);
            break;
        case wifi_mode_Client:
            _pSwitch->setSource(
                "/usr/local/share/ui/BMP/dash_menu/",
                "toggle_on.bmp");
            strcpy(_txtMode, "Client");
            _pModeResult->SetText((UINT8 *)_txtMode);

            _pClient->SetShow();
            _pWifiInfo->SetShow();
            _pClientInfoFake->SetShow();

            _cp->GetWifiCltInfor(ssid, sizeof(ssid));
            if (strcmp(ssid, "Empty List") == 0) {
                snprintf(_txtWifiInfo, sizeof(_txtWifiInfo), "%s", "N/A");
            } else {
                if (strlen(ssid) > 12) {
                    strncpy(_txtWifiInfo, ssid, 10);
                    _txtWifiInfo[10] = '.';
                    _txtWifiInfo[11] = '.';
                    _txtWifiInfo[12] = '\0';
                } else {
                    snprintf(_txtWifiInfo, sizeof(_txtWifiInfo), "%s", ssid);
                }
            }

            _pSSID->SetHiden();
            _pSSIDValue->SetHiden();

            _pPWD->SetHiden();
            _pPWDValue->SetHiden();

            _pQrCode->SetHiden();
            _pQRBG->SetHiden();

            _pAutoAPFake->SetRelativePos(Point(0, 230));
            _pAutoAP->SetRelativePos(Point(30, 250));
            _pAutoAPInfo->SetRelativePos(Point(230, 250));

            _pScroller->setScrollable(true);
            _pScroller->setScrollableSize(Size(_scrollableSize.width, 380));
            break;
        case wifi_mode_Off:
            _pSwitch->setSource(
                "/usr/local/share/ui/BMP/dash_menu/",
                "toggle_off.bmp");
            char mr_mode[256];
            agcmd_property_get(WIFI_MR_MODE, mr_mode, "AP");
            if (strcasecmp(mr_mode, "WLAN") == 0) {
                strcpy(_txtMode, "Client");
            } else {
                strcpy(_txtMode, "Hotspot");
            }
            _pModeResult->SetText((UINT8 *)_txtMode);

            _pClient->SetHiden();
            _pWifiInfo->SetHiden();
            _pClientInfoFake->SetHiden();

            _pSSID->SetHiden();
            _pSSIDValue->SetHiden();

            _pPWD->SetHiden();
            _pPWDValue->SetHiden();

            _pQrCode->SetHiden();
            _pQRBG->SetHiden();

            _pAutoAPFake->SetRelativePos(Point(0, 150));
            _pAutoAP->SetRelativePos(Point(30, 170));
            _pAutoAPInfo->SetRelativePos(Point(230, 170));

            _pScroller->setScrollable(false);
            _pScroller->setViewPort(Point(0, 0), _visibleSizeScroller);
            break;
        default:
            break;
    }
}

void WifiMenuWnd::onMenuResume()
{
    _counter = 0;
    updateWifi();
    _pPanel->Draw(true);
}

void WifiMenuWnd::onMenuPause()
{
}

void WifiMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pSwitchFake) {
        wifi_mode mode = _cp->getWifiMode();
        if (mode != wifi_mode_Off) {
            _cntTimeout = 300;
            _pScroller->SetHiden();
            _pAnimation->SetShow();
            _pPanel->Draw(true);

            _cp->SetWifiMode(wifi_mode_Off);
            _targetMode = wifi_mode_Off;
        } else {
            _cntTimeout = 300;
            _pScroller->SetHiden();
            _pAnimation->SetShow();
            _pPanel->Draw(true);

            char tmp[AGCMD_PROPERTY_SIZE];
            memset(tmp, 0x00, AGCMD_PROPERTY_SIZE);
            agcmd_property_get(WIFI_MR_MODE, tmp, "0");
            if ((strcasecmp(tmp, WIFI_AP_KEY) == 0)
                || (strcasecmp(tmp, WIFI_MR_KEY) == 0))
            {
                _cp->SetWifiMode(wifi_mode_AP);
                _targetMode = wifi_mode_AP;
            } else if (strcasecmp(tmp, WIFI_CLIENT_KEY) == 0) {
                _cp->SetWifiMode(wifi_mode_Client);
                _targetMode = wifi_mode_Client;
            } else {
                // default is AP mode
                _cp->SetWifiMode(wifi_mode_AP);
                _targetMode = wifi_mode_AP;
            }
        }
    } else if (widget == _pModeFake) {
        this->StartWnd(WINDOW_wifimodemenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pClientInfoFake) {
        wifi_mode mode = _cp->getWifiMode();
        if (wifi_mode_Client == mode) {
            char ssid[256];
            _cp->GetWifiCltInfor(ssid, 256);
            /*if (strcmp(ssid, "Empty List") == 0) {
                this->StartWnd(WINDOW_wifi_no_connection, Anim_Bottom2TopCoverEnter);
            } else */{
                this->StartWnd(WINDOW_wifi_detail, Anim_Bottom2TopCoverEnter);
            }
        }
    } else if (widget == _pAutoAPFake) {
        this->StartWnd(WINDOW_autohotspot_menu, Anim_Bottom2TopCoverEnter);
    }
}

bool WifiMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter % 100 == 0) {
                    updateWifi();
                    _pPanel->Draw(true);
                }

                if (_cntTimeout > 0) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (mode == _targetMode) {
                        _pScroller->SetShow();
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _cntTimeout = 0;
                    } else {
                        char source[64];
                        // TODO:
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _cntTimeout % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        updateWifi();
                        _pPanel->Draw(true);
                        _cntTimeout--;
                        if (_cntTimeout <= 0) {
                            _pScroller->SetShow();
                            _pAnimation->SetHiden();
                            _pPanel->Draw(true);
                        }
                    }
                    b = false;
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_Wifi) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (mode == _targetMode) {
                        _pScroller->SetShow();
                        _pAnimation->SetHiden();
                        _cntTimeout = 0;
                    }
                    updateWifi();
                    _pPanel->Draw(true);
                    b = false;
                }
            }
            break;
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                //int x_pos = (position >> 16) & 0xFFFF;
                if (y_pos < 108) {
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


WifiModeMenuWnd::WifiModeMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _counter(0)
    , _delayClose(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
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
    _pTitle->SetText((UINT8 *)"Mode");

    _pLine = new UILine(_pPanel, Point(130, 106), Size(140, 4));
    _pLine->setLine(Point(130, 100), Point(270, 100), 4, Color_Grey_2);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)"Hotspot";
    _pMenus[_numMenus++] = (char *)"Client";

    UINT16 itemheight = 60;
    UINT16 padding = 20;
    _pickerSize.width = 200;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding;
    _pPickers = new Pickers(_pPanel,
        Point(100, 220 - _pickerSize.height / 2),
        _pickerSize,
        _pickerSize);
    _pPickers->setItems(_pMenus, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);

    _pAnimation = new BmpImage(
        _pPanel,
        Point(136, 160), Size(128, 128),
        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
        "power_OFF-spinner_00000.bmp");
    _pAnimation->SetHiden();

    _pIconResult = new BmpImage(
        _pPanel,
        Point(160, 150), Size(80, 80),
        "/usr/local/share/ui/BMP/dash_menu/",
        "feedback-good.bmp");
    _pIconResult->SetHiden();

    _pTxtResult = new StaticText(_pPanel, Point(0, 260), Size(400, 80));
    _pTxtResult->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, TOP);
    _pTxtResult->SetText((UINT8 *)"Change Wi-Fi mode\nsuccessfully!");
    _pTxtResult->SetHiden();
}

WifiModeMenuWnd::~WifiModeMenuWnd()
{
    delete _pTxtResult;
    delete _pIconResult;
    delete _pAnimation;

    delete _pPickers;

    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void WifiModeMenuWnd::updateWifi()
{
    wifi_mode mode = _cp->getWifiMode();
    switch (mode) {
        case wifi_mode_AP:
        case wifi_mode_Multirole:
            _pPickers->setIndex(0);
            break;
        case wifi_mode_Client:
            _pPickers->setIndex(1);
            break;
        case wifi_mode_Off:
            _pPickers->setIndex(2);
            break;
        default:
            break;
    }
}

void WifiModeMenuWnd::onMenuResume()
{
    updateWifi();
    _pPanel->Draw(true);
    _counter = 0;
}

void WifiModeMenuWnd::onMenuPause()
{
}

void WifiModeMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void WifiModeMenuWnd::onPickersFocusChanged(Control *picker, int indexFocus)
{
    if (_pPickers == picker) {
        if ((indexFocus >= 0) && (indexFocus < _numMenus)) {
            wifi_mode mode = _cp->getWifiMode();
            if (indexFocus == 0) {
               if (mode != wifi_mode_AP) {
                    _counter = 300;
                    _pPickers->SetHiden();
                    _pAnimation->SetShow();
                    _pPanel->Draw(true);

                    _cp->SetWifiMode(wifi_mode_AP);
                    _targetMode = wifi_mode_AP;
                } else {
                    this->Close(Anim_Top2BottomExit);
                }
            } else if (indexFocus == 1) {
                if (mode != wifi_mode_Client) {
                    _counter = 300;
                    _pPickers->SetHiden();
                    _pAnimation->SetShow();
                    _pPanel->Draw(true);

                    _cp->SetWifiMode(wifi_mode_Client);
                    _targetMode = wifi_mode_Client;
                } else {
                    this->Close(Anim_Top2BottomExit);
                }
            }
        }
    }
}

bool WifiModeMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_delayClose > 0) {
                    _delayClose--;
                    if (_delayClose <= 0) {
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                } else if (_counter > 0) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (mode == _targetMode) {
                        _pPickers->SetHiden();
                        _pAnimation->SetHiden();
                        _pIconResult->setSource(
                            "/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-good.bmp");
                        _pIconResult->SetShow();
                        _pTxtResult->SetText((UINT8 *)"Change Wi-Fi mode\nsuccessfully!");
                        _pTxtResult->SetShow();
                        _pPanel->Draw(true);
                        _counter = -1;
                        _delayClose = 5;
                    } else {
                        char source[64];
                        // TODO:
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _counter % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        updateWifi();
                        _pPanel->Draw(true);
                        _counter--;
                        if (_counter <= 0) {
                            _pPickers->SetHiden();
                            _pAnimation->SetHiden();
                            _pIconResult->setSource(
                                "/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-bad.bmp");
                            _pIconResult->SetShow();
                            _pTxtResult->SetText((UINT8 *)"Change Wi-Fi mode\ntime out!");
                            _pTxtResult->SetShow();
                            _pPanel->Draw(true);
                            _delayClose = 10;
                        }
                    }
                    b = false;
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


AutoAPMenuWnd::AutoAPMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
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
        "Set Wi-Fi mode to"
        "\n"
        "Hotspot mode when"
        "\n"
        "Waylens camera powered"
        "\n"
        "on through car mount.");
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
    _pStatus->SetText((UINT8 *)"Auto Switch");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(304, 274), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");
}

AutoAPMenuWnd::~AutoAPMenuWnd()
{
    delete _pSwitch;
    delete _pStatus;
    delete _pSwitchFake;
    delete _pInfo;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void AutoAPMenuWnd::updateStatus()
{
    char auto_hotspot[256];
    agcmd_property_get(PropName_Auto_Hotspot, auto_hotspot, Auto_Hotspot_Enabled);
    if (strcasecmp(auto_hotspot, Auto_Hotspot_Enabled) == 0) {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }
}

void AutoAPMenuWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void AutoAPMenuWnd::onMenuPause()
{
}

void AutoAPMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pSwitchFake) {
        char auto_hotspot[256];
        agcmd_property_get(PropName_Auto_Hotspot, auto_hotspot, Auto_Hotspot_Enabled);
        if (strcasecmp(auto_hotspot, Auto_Hotspot_Enabled) == 0) {
            agcmd_property_set(PropName_Auto_Hotspot, Auto_Hotspot_Disabled);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_off.bmp");
        } else {
            agcmd_property_set(PropName_Auto_Hotspot, Auto_Hotspot_Enabled);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_on.bmp");
        }
        _pPanel->Draw(true);
    }
}

bool AutoAPMenuWnd::OnEvent(CEvent *event)
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

WifiNoConnectWnd::WifiNoConnectWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _counter(0)
    , _cntTimeout(0)
    , _targetMode(wifi_mode_Off)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(100, 0), Size(200, 90), 0x0000, 0x0000);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _visibleSizeScroller.width = 320;
    _visibleSizeScroller.height = 250;
    _pScroller = new Scroller(
        _pPanel,
        this,
        Point(40, 80),
        _visibleSizeScroller,
        Size(_visibleSizeScroller.width, 660),
        20);
    initScroller();

    _pAnimation = new BmpImage(
        _pPanel,
        Point(136, 160), Size(128, 128),
        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
        "power_OFF-spinner_00000.bmp");
    _pAnimation->SetHiden();
}

WifiNoConnectWnd::~WifiNoConnectWnd()
{
    delete _pHotspot;
    delete _pGuide;
    delete _pLine;
    delete _pTitle;
    delete _pAnimation;
    delete _pScroller;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void WifiNoConnectWnd::initScroller()
{
    // All position are relative to scroller:
    _pTitle = new StaticText(
        _pScroller->getViewRoot(),
        Point(60, 0),
        Size(200, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Oops");

    _pLine = new UILine(
        _pScroller->getViewRoot(),
        Point(90, 46),
        Size(140, 4));
    _pLine->setLine(Point(90, 100), Point(230, 100), 4, Color_Grey_2);

    snprintf(_txt, sizeof(_txt),
        "Looks like you have't"
        "\n"
        "connected to any Wi-Fi"
        "\n"
        "network yet."
        "\n"
        "\n"
        "First you need to switch the"
        "\n"
        "camera to Hotspot mode."
        "\n"
        "Then connect your phone"
        "\n"
        "to camera's Wi-Fi, and"
        "\n"
        "configure it from the Waylens"
        "\n"
        "mobile app."
        "\n"
        "\n"
        "For more details, please visit"
        "\n"
        "www.waylens.com");
    _pGuide = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 70),
        Size(_visibleSizeScroller.width, 540));
    _pGuide->SetStyle(24, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, TOP);
    _pGuide->SetLineHeight(40);
    _pGuide->SetText((UINT8 *)_txt);

    _pHotspot = new Button(
        _pScroller->getViewRoot(),
        Point(0, 600),
        Size(_visibleSizeScroller.width, 60),
        Color_Black, Color_Grey_2);
    _pHotspot->SetText((UINT8 *)"Hotspot Mode", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pHotspot->SetTextAlign(CENTER, MIDDLE);
    _pHotspot->setOnClickListener(this);
}

void WifiNoConnectWnd::onMenuResume()
{
    _counter = 0;
}

void WifiNoConnectWnd::onMenuPause()
{
}

void WifiNoConnectWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pHotspot) {
        _cntTimeout = 300;
        _pScroller->SetHiden();
        _pAnimation->SetShow();
        _pPanel->Draw(true);

        _cp->SetWifiMode(wifi_mode_AP);
        _targetMode = wifi_mode_AP;
    }
}

bool WifiNoConnectWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter % 100 == 0) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (mode != wifi_mode_Client) {
                        this->Close(Anim_Top2BottomExit);
                    } else {
                        char ssid[256];
                        _cp->GetWifiCltInfor(ssid, 256);
                        if (strcmp(ssid, "Empty List") != 0) {
                            this->Close(Anim_Top2BottomExit);
                        }
                    }
                }

                if (_cntTimeout > 0) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (mode == _targetMode) {
                        this->Close(Anim_Top2BottomExit);
                        _cntTimeout = 0;
                    } else {
                        char source[64];
                        // TODO:
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _cntTimeout % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        _pPanel->Draw(true);
                        _cntTimeout--;
                        if (_cntTimeout <= 0) {
                            _pScroller->SetShow();
                            _pAnimation->SetHiden();
                            _pPanel->Draw(true);
                        }
                    }
                    b = false;
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_Wifi) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (mode != wifi_mode_Client) {
                        this->Close(Anim_Top2BottomExit);
                    } else if (mode == _targetMode) {
                        _pScroller->SetShow();
                        _pAnimation->SetHiden();
                        _cntTimeout = 0;
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                }
            }
            break;
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                int x_pos = (position >> 16) & 0xFFFF;
                if ((y_pos < 108)
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


WifiDetailWnd::WifiDetailWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _wifiList(NULL)
    , _wifiNum(0)
    , _cntAnim(0)
    , _state(State_Show_Detail)
    , _counter(0)
    , _cntDelayClose(0)
    , _cntTimeout(0)
{
    memset(_ssid, 0x00, 256);

    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(100, 0), Size(200, 90), 0x0000, 0x0000);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    memset(_txt, 0x00, sizeof(_txt));
    _pSSID = new StaticText(
        _pPanel,
        Point(50, 80),
        Size(300, 160));
    _pSSID->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pSSID->SetLineHeight(40);
    _pSSID->SetText((UINT8 *)_txt);

    _pDisconnect = new Button(
        _pPanel,
        Point(50, 260),
        Size(300, 60),
        Color_Black, Color_Grey_2);
    _pDisconnect->SetText((UINT8 *)"Disconnect", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pDisconnect->SetTextAlign(CENTER, MIDDLE);
    _pDisconnect->setOnClickListener(this);


    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 80));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Saved\nConnections");
    _pTitle->SetHiden();

    _pLine = new UILine(_pPanel, Point(70, 136), Size(260, 4));
    _pLine->setLine(Point(70, 100), Point(330, 100), 4, Color_Grey_2);
    _pLine->SetHiden();

    _pList = new List(_pPanel, this, Point(70, 148), Size(260, 190), 20);
    _pList->setListener(this);

    _pAdapter = new TextListAdapter();
    _pAdapter->setTextList((UINT8 **)_wifiList, _wifiNum);
    _pAdapter->setItemStyle(Size(260, 50), Color_Black, Color_Grey_2,
        Color_White, 24, FONT_ROBOTOMONO_Light,
        LEFT, MIDDLE);
    _pAdapter->setObserver(_pList);

    _pList->setAdaptor(_pAdapter);
    _pList->SetHiden();


    memset(_txtInfo, 0x00, sizeof(_txtInfo));
    _pInfo = new StaticText(
        _pPanel,
        Point(40, 100),
        Size(320, 160));
    _pInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pInfo->SetLineHeight(40);
    _pInfo->SetText((UINT8 *)_txtInfo);
    _pInfo->SetHiden();

    _pAnimation = new BmpImage(
        _pPanel,
        Point(136, 240), Size(128, 128),
        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
        "power_OFF-spinner_00000.bmp");
    _pAnimation->SetHiden();

    _pConnectResult = new BmpImage(
        _pPanel,
        Point(160, 80), Size(80, 80),
        "/usr/local/share/ui/BMP/dash_menu/",
        "feedback-good.bmp");
    _pConnectResult->SetHiden();
}

WifiDetailWnd::~WifiDetailWnd()
{
    delete _pConnectResult;
    delete _pAnimation;
    delete _pInfo;

    delete _pList;
    delete _pAdapter;
    delete _pLine;
    delete _pTitle;

    delete _pDisconnect;
    delete _pSSID;

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

void WifiDetailWnd::updateWifi()
{
    wifi_mode mode = _cp->getWifiMode();
    char ssid[256];
    memset(ssid, 0x00, 256);
    switch (mode) {
        case wifi_mode_Client: {
            bool ret = _cp->GetWifiCltInfor(ssid, 256);
            if (strcmp(ssid, "Empty List") == 0) {
                snprintf(_txt, sizeof(_txt), "%s", "No network added!");
            } else {
                int len = 0;
                if (!ret) {
                    len = snprintf(_txt, sizeof(_txt), "%s", "(Wrong password)\n");
                }
                if ((strlen(ssid) + len) >= 256) {
                    snprintf(_txt + len, sizeof(_txt) - len - 4, "%s", ssid);
                    _txt[252] = '.';
                    _txt[253] = '.';
                    _txt[254] = '.';
                    _txt[255] = '\0';
                } else {
                    snprintf(_txt + len, sizeof(_txt) - len, "%s", ssid);
                }
            }
            break;
        }
        default:
            snprintf(_txt, sizeof(_txt), "%s", "No network\nconnected!");
            break;
    }
}

void WifiDetailWnd::parseWifi(char *json)
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
            for (SizeType i = 0; i < a.Size(); i++) {
                const Value &item = a[i];
                if (item["added"].GetBool()) {
                    added++;
                }
            }
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
                /*CAMERALOG("%dth: %s %s %s",
                    i,
                    item["ssid"].GetString(),
                    item["added"].GetBool() ? "added" : "",
                    item["current"].GetBool() ? "current" : "");*/
                if (item["added"].GetBool()) {
                    const char *name = item["ssid"].GetString();
                    list[index] = new char[strlen(name) + 3];
                    if (!list[index]) {
                        CAMERALOG("Out of memory!");
                        goto parse_error;
                    }
                    snprintf(list[index], strlen(name) + 3, "%s %s",
                        item["current"].GetBool() ? "*" : " ",
                        item["ssid"].GetString());
                    index++;
                }
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

void WifiDetailWnd::onMenuResume()
{
    _counter = 0;
    updateWifi();
    _pPanel->Draw(true);
}

void WifiDetailWnd::onMenuPause()
{
}

void WifiDetailWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pDisconnect) {
        _pSSID->SetHiden();
        _pDisconnect->SetHiden();
        _pTitle->SetShow();
        _pLine->SetShow();
        _pList->SetHiden();
        strcpy(_txtInfo, "Scanning...");
        _pInfo->SetShow();
        _pAnimation->SetShow();
        _cntAnim = 0;
        _pPanel->Draw(true);
        _cp->ScanNetworks();
        _state = State_Scan_Wifi;
        _cntTimeout = 0;
    }
}

void WifiDetailWnd::onListClicked(Control *list, int index)
{
    if (list == _pList) {
        if (index < _wifiNum) {
            if (_wifiList[index][0] != '*') {
                snprintf(_ssid, 256, _wifiList[index] + 2);
                CAMERALOG("Connecting %s", _ssid);
                _pTitle->SetHiden();
                _pLine->SetHiden();
                _pList->SetHiden();
                snprintf(_txtInfo, sizeof(_txtInfo), "Connecting to\n%s", _ssid);
                _pInfo->SetRelativePos(Point(40, 70));
                _pInfo->SetShow();
                _pAnimation->SetShow();
                _pPanel->Draw(true);
                _cp->ConnectHost(_ssid);
                _state = State_Connecting;
                _cntTimeout = 0;
                _cntAnim = 0;
            } else {
                this->Close(Anim_Top2BottomExit);
            }
        }
    }
}

bool WifiDetailWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if ((event->_event == InternalEvent_app_timer_update)) {
                if (_cntDelayClose > 0) {
                    _cntDelayClose--;
                    if (_cntDelayClose <= 0) {
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                } else {
                    _counter++;
                    if ((_counter % 100 == 0) && (_state == State_Show_Detail)) {
                        updateWifi();
                        _pPanel->Draw(true);
                    }
                }

                if (_state == State_Scan_Wifi) {
                    _cntTimeout++;
                    if (_cntTimeout >= 300) {
                        _pTitle->SetHiden();
                        _pLine->SetHiden();
                        _pList->SetHiden();
                        _pConnectResult->setSource(
                            "/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-bad.bmp");
                        snprintf(_txtInfo, sizeof(_txtInfo), "Scan Wi-Fi\ntime out!");
                        _pConnectResult->SetShow();
                        _pInfo->SetRelativePos(Point(40, 160));
                        _pInfo->SetShow();
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _cntTimeout = 0;
                        _cntDelayClose = 20;
                        _state = State_Scan_Wifi_Timeout;
                    } else {
                        char source[64];
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _cntAnim++ % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        _pPanel->Draw(true);
                    }
                    b = false;
                } else if (_state == State_Connecting) {
                    _cntTimeout++;
                    if (_cntTimeout >= 400) {
                        _pConnectResult->setSource(
                            "/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-bad.bmp");
                        snprintf(_txtInfo, 256, "Time out connect:\n%s", _ssid);
                        _pConnectResult->SetShow();
                        _pInfo->SetRelativePos(Point(40, 160));
                        _pInfo->SetShow();
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _cntTimeout = 0;
                        _cntDelayClose = 20;
                        _state = State_Connect_Timeout;
                    } else {
                        char source[64];
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _cntAnim++ % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        _pPanel->Draw(true);
                    }
                    b = false;
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_Wifi) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (mode == wifi_mode_Client) {
                        updateWifi();
                        _pPanel->Draw(true);
                    } else {
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                }
            } else if (event->_event ==InternalEvent_app_state_update) {
                if (event->_paload == camera_State_scanwifi_result) {
                    if (_state == State_Scan_Wifi) {
                        char *str = (char *)event->_paload1;
                        //CAMERALOG("str = %s", str);
                        parseWifi(str);
                        if (_wifiNum == 0) {
                            snprintf(_txtInfo, 256, "No saved\nconnections found!");
                            _pInfo->SetRelativePos(Point(40, 180));
                            _pInfo->SetShow();
                        } else {
                            _pInfo->SetHiden();
                        }
                        _pList->SetShow();
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _state = State_Show_Wifi_List;
                    }
                    b = false;
                } else if (event->_paload == camera_State_connectwifi_result) {
                    if (_state == State_Connecting) {
                        int result = event->_paload1;
                        if (result == 0) {
                            _pConnectResult->setSource(
                                "/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-bad.bmp");
                            snprintf(_txtInfo, 256, "Can't Connect to\n%s", _ssid);
                            _state = State_Connect_Failed;
                        } else {
                            _pConnectResult->setSource(
                                "/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-good.bmp");
                            snprintf(_txtInfo, 256, "Connected to\n%s", _ssid);
                            _state = State_Connect_Success;
                        }
                        _pConnectResult->SetShow();
                        _pInfo->SetRelativePos(Point(40, 160));
                        _pInfo->SetShow();
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _cntDelayClose = 10;
                    }
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


WifiListAdapter::WifiListAdapter()
    : _pList(NULL)
    , _num(0)
    , _itemSize(320, 50)
{
}

WifiListAdapter::~WifiListAdapter()
{
}

UINT16 WifiListAdapter::getCount()
{
    return _num;
}

Control* WifiListAdapter::getItem(UINT16 position, Container* pParent, const Size& size)
{
    if (position >= _num) {
        return NULL;
    }

    Container *item = new Container(pParent, pParent->GetOwner(), Point(0, 0), _itemSize, 2);
    if (!item) {
        CAMERALOG("Out of Memory!");
        return NULL;
    }

    if (!_pList[position]->ssid || (strlen(_pList[position]->ssid) == 0)) {
        // empty item
        return item;
    }

    StaticText *txt_1 = new StaticText(item, Point(0, 0), _itemSize);
    if (!txt_1) {
        CAMERALOG("Out of Memory!");
        delete item;
        return NULL;
    }
    txt_1->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, LEFT, MIDDLE);
    txt_1->SetText((UINT8*)_pList[position]->ssid);

    char name[64] = {0x00};
    if (_pList[position]->current) {
        if (_pList[position]->signal_level < -70) {
            snprintf(name, sizeof(name), "%s", "status-wif-list-50-connected.bmp");
        } else if (_pList[position]->signal_level < -55) {
            snprintf(name, sizeof(name), "%s", "status-wif-list-75-connected.bmp");
        } else {
            snprintf(name, sizeof(name), "%s", "status-wif-list-100-connected.bmp");
        }
    } else {
        if (_pList[position]->signal_level < -70) {
            snprintf(name, sizeof(name), "%s", "status-wif-list-50.bmp");
        } else if (_pList[position]->signal_level < -55) {
            snprintf(name, sizeof(name), "%s", "status-wif-list-75.bmp");
        } else {
            snprintf(name, sizeof(name), "%s", "status-wif-list-100.bmp");
        }
    }

    BmpImage *icon = new BmpImage(
        item,
        Point(_itemSize.width - 32, (_itemSize.height - 32) / 2),
        Size(32, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        name);
    if (!icon) {
        CAMERALOG("Out of Memory!");
        delete txt_1;
        delete item;
        return NULL;
    }

    return item;
}

Control* WifiListAdapter::getItem(UINT16 position, Container* pParent)
{
    if (position >= _num) {
        return NULL;
    }

    Container *item = new Container(pParent, pParent->GetOwner(), Point(0, 0), _itemSize, 2);
    if (!item) {
        CAMERALOG("Out of Memory!");
        return NULL;
    }

    if (!_pList[position]->ssid || (strlen(_pList[position]->ssid) == 0)) {
        // empty item
        return item;
    }

    StaticText *txt_1 = new StaticText(item, Point(0, 0), _itemSize);
    if (!txt_1) {
        CAMERALOG("Out of Memory!");
        delete item;
        return NULL;
    }
    txt_1->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, LEFT, MIDDLE);
    txt_1->SetText((UINT8*)_pList[position]->ssid);

    char name[64] = {0x00};
    if (_pList[position]->current) {
        if (_pList[position]->signal_level < -70) {
            snprintf(name, sizeof(name), "%s", "status-wif-list-50-connected.bmp");
        } else if (_pList[position]->signal_level < -55) {
            snprintf(name, sizeof(name), "%s", "status-wif-list-75-connected.bmp");
        } else {
            snprintf(name, sizeof(name), "%s", "status-wif-list-100-connected.bmp");
        }
    } else {
        if (_pList[position]->signal_level < -70) {
            snprintf(name, sizeof(name), "%s", "status-wif-list-50.bmp");
        } else if (_pList[position]->signal_level < -55) {
            snprintf(name, sizeof(name), "%s", "status-wif-list-75.bmp");
        } else {
            snprintf(name, sizeof(name), "%s", "status-wif-list-100.bmp");
        }
    }

    BmpImage *icon = new BmpImage(
        item,
        Point(_itemSize.width - 32, (_itemSize.height - 32) / 2),
        Size(32, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        name);
    if (!icon) {
        CAMERALOG("Out of Memory!");
        delete txt_1;
        delete item;
        return NULL;
    }

    return item;
}

void WifiListAdapter::setWifiList(WifiItem **pList, UINT16 num)
{
    _pList = pList;
    _num = num;
}


WifiClientWnd::WifiClientWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _wifisFound(NULL)
    , _wifiNum(0)
    , _cntAnim(0)
    , _state(State_Show_Wifi_List)
    , _counter(0)
    , _cntDelayClose(0)
    , _cntTimeout(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(100, 0), Size(200, 90), 0x0000, 0x0000);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

// wifi list:
    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Networks");

    _pLine = new UILine(_pPanel, Point(100, 106), Size(200, 4));
    _pLine->setLine(Point(100, 106), Point(300, 106), 4, Color_Grey_2);

    _pList = new List(_pPanel, this, Point(40, 110), Size(320, 290), 20);
    _pList->setListener(this);

    _pAdapter = new WifiListAdapter();
    _pAdapter->setWifiList(_wifisFound, _wifiNum);
    _pAdapter->setItemSize(Size(320, 72));
    _pAdapter->setObserver(_pList);

    _pList->setAdaptor(_pAdapter);

// dialogue to connect or forget:
    memset(_txtSSID, 0x00, sizeof(_txtSSID));
    _pSSID = new StaticText(
        _pPanel,
        Point(50, 60),
        Size(300, 160));
    _pSSID->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pSSID->SetLineHeight(40);
    _pSSID->SetText((UINT8 *)_txtSSID);
    _pSSID->SetHiden();

    _pConnect = new Button(
        _pPanel,
        Point(50, 220),
        Size(300, 80),
        Color_Black, Color_Grey_1);
    _pConnect->SetText((UINT8 *)"Connect", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pConnect->SetTextAlign(CENTER, MIDDLE);
    _pConnect->setOnClickListener(this);
    _pConnect->SetHiden();

    _pForget = new Button(
        _pPanel,
        Point(50, 300),
        Size(300, 80),
        Color_Black, Color_Grey_1);
    _pForget->SetText((UINT8 *)"Forget", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pForget->SetTextAlign(CENTER, MIDDLE);
    _pForget->setOnClickListener(this);
    _pForget->SetHiden();

// animiation:
    _pAnimation = new BmpImage(
        _pPanel,
        Point(136, 190), Size(128, 128),
        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
        "power_OFF-spinner_00000.bmp");
    _pAnimation->SetHiden();

// help info:
    memset(_txtInfo, 0x00, sizeof(_txtInfo));
    _pInfo = new StaticText(
        _pPanel,
        Point(40, 100),
        Size(320, 160));
    _pInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, TOP);
    _pInfo->SetLineHeight(40);
    _pInfo->SetText((UINT8 *)_txtInfo);
    _pInfo->SetHiden();

    _pConnectResult = new BmpImage(
        _pPanel,
        Point(160, 80), Size(80, 80),
        "/usr/local/share/ui/BMP/dash_menu/",
        "feedback-good.bmp");
    _pConnectResult->SetHiden();

    _pReset = new Button(
        _pPanel,
        Point(50, 220),
        Size(300, 80),
        Color_Black, Color_Grey_1);
    _pReset->SetText((UINT8 *)"Reset", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pReset->SetTextAlign(CENTER, MIDDLE);
    _pReset->setOnClickListener(this);
    _pReset->SetHiden();

    _pCancel = new Button(
        _pPanel,
        Point(50, 300),
        Size(300, 80),
        Color_Black, Color_Grey_1);
    _pCancel->SetText((UINT8 *)"Cancel", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pCancel->SetTextAlign(CENTER, MIDDLE);
    _pCancel->setOnClickListener(this);
    _pCancel->SetHiden();
}

WifiClientWnd::~WifiClientWnd()
{
    delete _pCancel;
    delete _pReset;
    delete _pConnectResult;
    delete _pInfo;

    delete _pAnimation;

    delete _pForget;
    delete _pConnect;
    delete _pSSID;

    delete _pList;
    delete _pAdapter;
    delete _pLine;
    delete _pTitle;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;

    for (int i = 0; i < _wifiNum; i++) {
        delete _wifisFound[i];
    }
    delete [] _wifisFound;
    _wifisFound = NULL;
    _wifiNum = 0;
}

void WifiClientWnd::_getInitWifi()
{
    if (_wifiNum > 0) {
        return;
    }

    wifi_mode mode = _cp->getWifiMode();
    if ((mode != wifi_mode_Client) && (mode != wifi_mode_Multirole)) {
        this->Close(Anim_Top2BottomExit);
    }

    char ssid[256];
    memset(ssid, 0x00, 256);
    _cp->GetWifiCltInfor(ssid, 256);
    if ((strlen(ssid) == 0)
        || (strcmp(ssid, "Empty List") == 0)
        || (strcmp(ssid, "Searching") == 0)
        || (strcmp(ssid, "Connecting") == 0))
    {
        return;
    }

    WifiItem *cur_wifi = new WifiItem;
    if (!cur_wifi) {
        CAMERALOG("Out of memory!");
        return;
    }
    cur_wifi->ssid = new char [strlen(ssid) + 1];
    if (!cur_wifi->ssid) {
        CAMERALOG("Out of memory!");
        delete cur_wifi;
        return;
    }
    snprintf(cur_wifi->ssid, strlen(ssid) + 1, "%s", ssid);
    cur_wifi->current = true;
    cur_wifi->added = true;
    cur_wifi->signal_level = -80;
    cur_wifi->secured = true;

    _wifisFound = new WifiItem* [1];
    if (!_wifisFound) {
        CAMERALOG("Out of memory!");
        delete cur_wifi;
        return;
    }
    _wifisFound[0] = cur_wifi;
    _wifiNum = 1;

    _pAdapter->setWifiList(_wifisFound, _wifiNum);
    _pAdapter->notifyDataSetChanged();
}

void WifiClientWnd::_updateWifi()
{
    wifi_mode mode = _cp->getWifiMode();
    if ((mode != wifi_mode_Client) && (mode != wifi_mode_Multirole)) {
        this->Close(Anim_Top2BottomExit);
    }
}

void WifiClientWnd::_parseWifi(char *json)
{
    Document d;
    d.Parse(json);
    if(!d.IsObject()) {
        CAMERALOG("Didn't get a JSON object!");
        return;
    }

    int added = 0;
    int index = 0;
    WifiItem **wifisFound = NULL;
    if (d.HasMember("networks")) {
        const Value &a = d["networks"];
        if (a.IsArray() && (a.Size() > 0)) {
            // firstly find the current network:
            int index_current = -1;
            for (SizeType i = 0; i < a.Size(); i++) {
                const Value &item = a[i];
                if (item["current"].GetBool()) {
                    index_current = i;
                    break;
                }
            }

            // then parse all the networks found, and put the current at the first position:
            added = a.Size() + 1;
            wifisFound = new WifiItem* [added];
            if (!wifisFound) {
                CAMERALOG("Out of memory!");
                goto parse_error;
            }
            memset(wifisFound, 0x00, sizeof(WifiItem*) * added);

            // put the current network at the first position:
            if (index_current >= 0) {
                const Value &item = a[index_current];
                wifisFound[0] = new WifiItem;
                if (!wifisFound[0]) {
                    CAMERALOG("Out of memory!");
                    goto parse_error;
                }
                const char *name = item["ssid"].GetString();
                wifisFound[0]->ssid = new char[strlen(name) + 1];
                if (!wifisFound[0]->ssid) {
                    delete wifisFound[0];
                    wifisFound[0] = NULL;
                    CAMERALOG("Out of memory!");
                    goto parse_error;
                }
                snprintf(wifisFound[0]->ssid, strlen(name) + 1,
                    "%s", item["ssid"].GetString());
                wifisFound[0]->signal_level = item["signal_level"].GetInt();
                wifisFound[0]->added = item["added"].GetBool();
                wifisFound[0]->current = item["current"].GetBool();
                const char *flags = item["flags"].GetString();
                if (strstr(flags, "WPA") || strstr(flags, "WEP")) {
                    wifisFound[0]->secured = true;
                } else {
                    wifisFound[0]->secured = false;
                }

                index++;
            }

            for (SizeType i = 0; i < a.Size(); i++) {
                if ((index_current >= 0) && (index_current == i)) {
                    continue;
                }

                const Value &item = a[i];
                //CAMERALOG("%dth: %s %s %s %d",
                //    i,
                //    item["ssid"].GetString(),
                //    item["added"].GetBool() ? "added" : "",
                //    item["current"].GetBool() ? "current" : "",
                //    item["signal_level"].GetInt());

                wifisFound[index] = new WifiItem;
                if (!wifisFound[index]) {
                    CAMERALOG("Out of memory!");
                    goto parse_error;
                }
                const char *name = item["ssid"].GetString();
                wifisFound[index]->ssid = new char[strlen(name) + 1];
                if (!wifisFound[index]->ssid) {
                    delete wifisFound[index];
                    wifisFound[index] = NULL;
                    CAMERALOG("Out of memory!");
                    goto parse_error;
                }
                snprintf(wifisFound[index]->ssid, strlen(name) + 1,
                    "%s", item["ssid"].GetString());
                wifisFound[index]->signal_level = item["signal_level"].GetInt();
                wifisFound[index]->added = item["added"].GetBool();
                wifisFound[index]->current = item["current"].GetBool();
                const char *flags = item["flags"].GetString();
                if (strstr(flags, "WPA") || strstr(flags, "WEP")) {
                    wifisFound[index]->secured = true;
                } else {
                    wifisFound[index]->secured = false;
                }

                index++;
            }

            // the empty item
            wifisFound[index++] = new WifiItem;
        }
    }

//parse_success:
    // Firstly, free the list which is out of date
    for (int i = 0; i < _wifiNum; i++) {
        delete _wifisFound[i];
    }
    _wifiNum = 0;
    delete [] _wifisFound;
    _wifisFound = NULL;

    // Then, use the new list
    _wifiNum = added;
    _wifisFound = wifisFound;
    //for (int i = 0; i < _wifiNum; i++) {
    //    CAMERALOG("Saved connections: %dth %s", i, _wifisFound[i]->ssid);
    //}
    _pAdapter->setWifiList(_wifisFound, _wifiNum);
    _pAdapter->notifyDataSetChanged();

    return;


parse_error:
    for (int i = 0; i < index; i++) {
        delete wifisFound[i];
    }
    delete [] wifisFound;

    return;
}

void WifiClientWnd::_connectWifi(char *ssid, char *pwd)
{
    if (!ssid) {
        CAMERALOG("not a valid ssid");
        return;
    }

    if (!pwd) {
        CAMERALOG("not a valid pwd");
        return;
    }

    if (pwd && ((strlen(pwd) > 0) && (strlen(pwd) < 8))) {
        CAMERALOG("not a valid pwd");
        return;
    }

    Document d1;
    Document::AllocatorType& allocator = d1.GetAllocator();
    d1.SetObject();

    Value jSsid(kStringType);
    jSsid.SetString(ssid, allocator);
    d1.AddMember("ssid", jSsid, allocator);

    Value jPWD(kStringType);
    jPWD.SetString(pwd, allocator);
    d1.AddMember("password", jPWD, allocator);

    d1.AddMember("is_hide", 0, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> write(buffer);
    d1.Accept(write);
    const char *json = buffer.GetString();
    CAMERALOG("strlen %d: %s", strlen(json), json);

    _cntTimeout = 0;
    _cntAnim = 0;
    _state = State_Connecting;
    _cp->AddWifiNetwork(json);
}

void WifiClientWnd::onMenuResume()
{
    if (State_Waiting_Input == _state) {
        char *pwd = CameraAppl::getInstance()->GetInputedText();
        if (pwd && (strlen(pwd) >= 8)) {
            _state = State_Waiting_To_Connect;
        }
    } else {
        if (_wifiNum == 0) {
            _getInitWifi();
        }
        if (_wifiNum == 0) {
            _pList->SetHiden();
            _pAnimation->SetRelativePos(Point(136, 190));
            _pAnimation->SetShow();

            _cntTimeout = 0;
            _cntAnim = 0;
            _cp->ScanNetworks();
            _state = State_Scan_Wifi;
        } else {
            // scan networks in background
            _cp->ScanNetworks();
        }
        _pPanel->Draw(true);
    }
}

void WifiClientWnd::onMenuPause()
{
}

void WifiClientWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        if (State_Show_Forget == _state) {
            _pSSID->SetHiden();
            _pConnect->SetHiden();
            _pForget->SetHiden();

            _pTitle->SetShow();
            _pLine->SetShow();
            _pList->SetShow();

            _pPanel->Draw(true);

            _state = State_Show_Wifi_List;
        } else {
            this->Close(Anim_Top2BottomExit);
        }
    } else if (widget == _pConnect) {
        _pSSID->SetHiden();
        _pConnect->SetHiden();
        _pForget->SetHiden();

        _pAnimation->SetRelativePos(Point(136, 96));
        _pAnimation->SetShow();
        snprintf(_txtInfo, sizeof(_txtInfo), "Connecting to\n%s", _txtSSID);
        _pInfo->SetRelativePos(Point(40, 240));
        _pInfo->SetShow();

        _pPanel->Draw(true);

        _cntTimeout = 0;
        _cntAnim = 0;
        _state = State_Connecting;
        _cp->ConnectHost(_txtSSID);
    } else if (widget == _pForget) {
        _cp->ForgetHost(_txtSSID);

        _pSSID->SetHiden();
        _pConnect->SetHiden();
        _pForget->SetHiden();

        _pTitle->SetShow();
        _pLine->SetShow();
        _pList->SetShow();

        // update the list
        for (int i = 0; i < _wifiNum; i++) {
            if (strcmp(_txtSSID, _wifisFound[i]->ssid) == 0) {
                _wifisFound[i]->added = false;
                _wifisFound[i]->current = false;
                break;
            }
        }
        _pAdapter->setWifiList(_wifisFound, _wifiNum);
        _pAdapter->notifyDataSetChanged();

        _pPanel->Draw(true);

        _state = State_Show_Wifi_List;
    } else if (widget == _pReset) {
        if (State_Connect_Timeout == _state) {
            _pCancel->SetHiden();
            _pReset->SetHiden();
            _pConnectResult->SetHiden();
            _pInfo->SetHiden();

            _pAnimation->SetRelativePos(Point(136, 96));
            _pAnimation->SetShow();
            snprintf(_txtInfo, sizeof(_txtInfo), "Connecting to\n%s", _txtSSID);
            _pInfo->SetRelativePos(Point(40, 240));
            _pInfo->SetShow();

            _pPanel->Draw(true);

            _cntTimeout = 0;
            _cntAnim = 0;
            _state = State_Connecting;
            _cp->ConnectHost(_txtSSID);
        } else {
            _pCancel->SetHiden();
            _pReset->SetHiden();
            _pConnectResult->SetHiden();
            _pInfo->SetHiden();

            _pTitle->SetShow();
            _pLine->SetShow();
            _pList->SetShow();

            _pPanel->Draw(true);

            _cntTimeout = 0;
            _cntAnim = 0;
            _state = State_Show_Wifi_List;
        }
    } else if (widget == _pCancel) {
        _pCancel->SetHiden();
        _pReset->SetHiden();
        _pConnectResult->SetHiden();
        _pInfo->SetHiden();

        _pTitle->SetShow();
        _pLine->SetShow();
        _pList->SetShow();

        _pPanel->Draw(true);

        _cntTimeout = 0;
        _cntAnim = 0;
        _state = State_Show_Wifi_List;
    }
}

void WifiClientWnd::onListClicked(Control *list, int index)
{
    if (list == _pList) {
        if (index < _wifiNum) {
            if (_wifisFound[index]->current) {
                this->Close(Anim_Top2BottomExit);
            } else {
                snprintf(_txtSSID, sizeof(_txtSSID), "%s", _wifisFound[index]->ssid);
                if (!_wifisFound[index]->secured) {
                    // connect an open wifi directly
                    _pTitle->SetHiden();
                    _pLine->SetHiden();
                    _pList->SetHiden();

                    _pAnimation->SetRelativePos(Point(136, 96));
                    _pAnimation->SetShow();
                    snprintf(_txtInfo, sizeof(_txtInfo), "Connecting to\n%s", _txtSSID);
                    _pInfo->SetRelativePos(Point(40, 240));
                    _pInfo->SetShow();

                    _pPanel->Draw(true);

                    char pwd[4] = {0x00};
                    _connectWifi(_txtSSID, pwd);
                } else if (_wifisFound[index]->added) {
                    // pop up dialogue to select connect or forget
                    _pTitle->SetHiden();
                    _pLine->SetHiden();
                    _pList->SetHiden();

                    _pSSID->SetShow();
                    _pConnect->SetShow();
                    _pForget->SetShow();

                    _pPanel->Draw(true);

                    _state = State_Show_Forget;
                } else {
                    // pop up IME to input password
                    _state = State_Waiting_Input;
                    CameraAppl::getInstance()->StartIME(_txtSSID, true);
                }
            }
        } else {
            this->Close(Anim_Top2BottomExit);
        }
    }
}

bool WifiClientWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if ((event->_event == InternalEvent_app_timer_update)) {
                if (_cntDelayClose > 0) {
                    _cntDelayClose--;
                    if (_cntDelayClose <= 0) {
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                } else {
                    _counter++;
                    if ((_counter % 200 == 0) && (_state == State_Show_Wifi_List)) {
                        // every 20 seconds, scan wifi:
                        _cp->ScanNetworks();
                    }
                }

                if (State_Waiting_To_Connect == _state) {
                    _pTitle->SetHiden();
                    _pLine->SetHiden();
                    _pList->SetHiden();

                    _pAnimation->SetRelativePos(Point(136, 96));
                    _pAnimation->SetShow();
                    snprintf(_txtInfo, sizeof(_txtInfo), "Connecting to\n%s", _txtSSID);
                    _pInfo->SetRelativePos(Point(40, 240));
                    _pInfo->SetShow();

                    _pPanel->Draw(true);

                    _connectWifi(_txtSSID, CameraAppl::getInstance()->GetInputedText());
                } else if (_state == State_Scan_Wifi) {
                    _cntTimeout++;
                    if (_cntTimeout >= 300) {
                        _pTitle->SetHiden();
                        _pLine->SetHiden();
                        _pList->SetHiden();
                        _pAnimation->SetHiden();

                        _pConnectResult->setSource(
                            "/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-bad.bmp");
                        _pConnectResult->SetRelativePos(Point(160, 136));
                        _pConnectResult->SetShow();
                        snprintf(_txtInfo, sizeof(_txtInfo), "Scan Wi-Fi\ntime out!");
                        _pInfo->SetRelativePos(Point(40, 220));
                        _pInfo->SetShow();
                        
                        _pPanel->Draw(true);

                        _cntTimeout = 0;
                        _cntDelayClose = 20;
                        _state = State_Scan_Wifi_Timeout;
                    } else {
                        char source[64];
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _cntAnim++ % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        _pPanel->Draw(true);
                    }
                    b = false;
                } else if (_state == State_Connecting) {
                    _cntTimeout++;
                    if (_cntTimeout >= 400) {
                        _pAnimation->SetHiden();
                        _pConnectResult->setSource(
                            "/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-bad.bmp");
                        _pConnectResult->SetShow();
                        _pConnectResult->SetRelativePos(Point(160, 64));
                        snprintf(_txtInfo, 256, "Connect time out");
                        _pInfo->SetRelativePos(Point(40, 168));
                        _pInfo->SetShow();
                        _pReset->SetText((UINT8 *)"Retry");
                        _pReset->SetShow();
                        _pCancel->SetShow();

                        _pPanel->Draw(true);

                        _cntTimeout = 0;
                        _state = State_Connect_Timeout;
                    } else {
                        char source[64];
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _cntAnim++ % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        _pPanel->Draw(true);
                    }
                    b = false;
                }
            } else if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_Wifi) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (mode == wifi_mode_Client) {
                        _updateWifi();
                        _pPanel->Draw(true);
                    } else {
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                }
            } else if (event->_event == InternalEvent_app_state_update) {
                if (event->_paload == camera_State_scanwifi_result) {
                    if (_state == State_Scan_Wifi) {
                        char *str = (char *)event->_paload1;
                        //CAMERALOG("str = %s", str);
                        _parseWifi(str);
                        _pList->SetShow();
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _state = State_Show_Wifi_List;
                    } else if (_state == State_Show_Wifi_List) {
                        char *str = (char *)event->_paload1;
                        //CAMERALOG("str = %s", str);
                        _parseWifi(str);
                        _pPanel->Draw(true);
                    }
                    b = false;
                } else if (event->_paload == camera_State_addwifi_result) {
                    if (_state == State_Connecting) {
                        int result = event->_paload1;
                        if (result) {
                            _cp->ConnectHost(_txtSSID);
                        } else {
                            CAMERALOG("Failed to add a wifi network!");
                            _pAnimation->SetHiden();
                            _pConnectResult->setSource(
                                "/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-bad.bmp");
                            _pConnectResult->SetShow();
                            _pConnectResult->SetRelativePos(Point(160, 64));
                            snprintf(_txtInfo, 256, "Password Incorrect");
                            _pInfo->SetRelativePos(Point(40, 168));
                            _pInfo->SetShow();
                            _pReset->SetText((UINT8 *)"Reset");
                            _pReset->SetShow();
                            _pCancel->SetShow();

                            _pPanel->Draw(true);
                            _cntTimeout = 0;
                            _state = State_Connect_Failed;
                        }
                    }
                    b = false;
                } else if (event->_paload == camera_State_connectwifi_result) {
                    if (_state == State_Connecting) {
                        int result = event->_paload1;
                        if (result == 0) {
                            // don't close on failure
                            _pConnectResult->setSource(
                                "/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-bad.bmp");
                            _pConnectResult->SetShow();
                            _pConnectResult->SetRelativePos(Point(160, 64));
                            snprintf(_txtInfo, 256, "Password Incorrect");
                            _pInfo->SetRelativePos(Point(40, 168));
                            _pInfo->SetShow();
                            _pReset->SetText((UINT8 *)"Reset");
                            _pReset->SetShow();
                            _pCancel->SetShow();
                            _state = State_Connect_Failed;
                        } else {
                            // close on successfully connected
                            _pConnectResult->setSource(
                                "/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-good.bmp");
                            _pConnectResult->SetShow();
                            _pConnectResult->SetRelativePos(Point(160, 136));
                            snprintf(_txtInfo, 256, "Connected to\n%s", _txtSSID);
                            _pInfo->SetRelativePos(Point(40, 240));
                            _pInfo->SetShow();
                            _state = State_Connect_Success;
                            _cntDelayClose = 10;
                        }
                        _pAnimation->SetHiden();

                        _pPanel->Draw(true);
                    } else if (_state == State_Show_Wifi_List) {
                        _cp->ScanNetworks();
                    }
                    b = false;
                } else if (event->_paload == camera_State_Wifi_pwd_error) {
                    if (_state == State_Connecting) {
                        int result = event->_paload1;
                        if (result == 0) {
                            // don't close on failure
                            _pConnectResult->setSource(
                                "/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-bad.bmp");
                            _pConnectResult->SetShow();
                            _pConnectResult->SetRelativePos(Point(160, 64));
                            snprintf(_txtInfo, 256, "Password Incorrect");
                            _pInfo->SetRelativePos(Point(40, 168));
                            _pInfo->SetShow();
                            _pReset->SetText((UINT8 *)"Reset");
                            _pReset->SetShow();
                            _pCancel->SetShow();
                            _state = State_Connect_Failed;
                        } else {
                            // close on successfully connected
                            _pConnectResult->setSource(
                                "/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-good.bmp");
                            _pConnectResult->SetShow();
                            _pConnectResult->SetRelativePos(Point(160, 136));
                            snprintf(_txtInfo, 256, "Connected to\n%s", _txtSSID);
                            _pInfo->SetRelativePos(Point(40, 240));
                            _pInfo->SetShow();
                            _state = State_Connect_Success;
                            _cntDelayClose = 10;
                        }
                        _pAnimation->SetHiden();

                        _pPanel->Draw(true);
                    } else {
                        if (event->_paload1) {
                            CameraAppl::getInstance()->PopupDialogue(
                                DialogueType_Wifi_add_success);
                        } else {
                            CameraAppl::getInstance()->PopupDialogue(
                                DialogueType_Wifi_pwd_wrong);
                        }
                    }
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

};
