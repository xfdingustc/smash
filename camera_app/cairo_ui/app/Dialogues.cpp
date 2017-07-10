
#include "linux.h"
#include "agbox.h"
#include "aglib.h"
#include "agobd.h"
#include "agsys_iio.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "GobalMacro.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "Widgets.h"
#include "class_camera_property.h"
#include "CameraAppl.h"
#include "AudioPlayer.h"

#include "ColorPalette.h"
#include "WindowRegistration.h"

#include "Dialogues.h"

extern bool bReleaseVersion;

extern void getStoragePropertyName(char *prop, int len);
extern bool is_no_write_record();

using namespace rapidjson;

namespace ui {

PowerOffWnd::PowerOffWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _poweroffCnt(0)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pTxt = new StaticText(_pPanel, Point(60, 80), Size(280, 40));
    _pTxt->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxt->SetText((UINT8*)"Power OFF?");

    _pYes = new ImageButton(
        _pPanel,
        Point(56, 200), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-yes_alt-up.bmp",
        "btn-dashboard-yes_alt-down.bmp");
    _pYes->setOnClickListener(this);

    _pNo = new ImageButton(
        _pPanel,
        Point(216, 200), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-no-up.bmp",
        "btn-dashboard-no-down.bmp");
    _pNo->setOnClickListener(this);

    _pAnimation = new BmpImage(
        _pPanel,
        Point(136, 160), Size(128, 128),
        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
        "power_OFF-spinner_00000.bmp");
    _pAnimation->SetHiden();
}

PowerOffWnd::~PowerOffWnd()
{
    delete _pAnimation;
    delete _pNo;
    delete _pYes;
    delete _pTxt;
    delete _pPanel;
}

void PowerOffWnd::onPause()
{
}

void PowerOffWnd::onResume()
{
}

void PowerOffWnd::onClick(Control *widget)
{
    if (_pYes == widget) {
        _pTxt->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, LEFT, MIDDLE);
        _pTxt->SetText((UINT8*)"Shutting down...");
        _pYes->SetHiden();
        _pNo->SetHiden();
        //_pAnimation->SetShow();
        _pPanel->Draw(true);

        _poweroffCnt = 10;
        CAMERALOG("Call \"agsh poweroff\" to power off now!");
        const char *tmp_args[4];
        tmp_args[0] = "agsh";
        tmp_args[1] = "poweroff";
        tmp_args[2] = NULL;
        agbox_sh(2, (const char *const *)tmp_args);
    } else if (_pNo == widget) {
        this->Close(Anim_Top2BottomExit);
    }
}

bool PowerOffWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_poweroffCnt > 0) {
                    if (_poweroffCnt % 20 == 0) {
                        _pTxt->SetText((UINT8*)"Shutting down");
                    } else if (_poweroffCnt % 20 == 5) {
                        _pTxt->SetText((UINT8*)"Shutting down.");
                    } else if (_poweroffCnt % 20 == 10) {
                        _pTxt->SetText((UINT8*)"Shutting down..");
                    } else if (_poweroffCnt % 20 == 15) {
                        _pTxt->SetText((UINT8*)"Shutting down...");
                    }

                    char source[128];
                    snprintf(source, 128, "power_OFF-spinner_%05d.bmp", _poweroffCnt % 10);
                    //CAMERALOG("source = %s", source);
                    _pAnimation->setSource(
                        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                        source);
                    _pPanel->Draw(true);

                    _poweroffCnt++;
                }
            }
            break;
        case eventType_key:
            switch(event->_event)
            {
                case key_ok_onKeyLongPressed: {
                    CAMERALOG("PowerOffWnd key_ok_onKeyLongPressed");
                    _pTxt->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, LEFT, MIDDLE);
                    _pTxt->SetText((UINT8*)"Shutting down...");
                    _pYes->SetHiden();
                    _pNo->SetHiden();
                    //_pAnimation->SetShow();
                    _pPanel->Draw(true);

                    _poweroffCnt = 10;
                    CAMERALOG("Call \"agsh poweroff\" to power off now!");
                    const char *tmp_args[4];
                    tmp_args[0] = "agsh";
                    tmp_args[1] = "poweroff";
                    tmp_args[2] = NULL;
                    agbox_sh(2, (const char *const *)tmp_args);
                    b = false;
                    break;
                }
            }
            break;
    }

    return b;
}


USBStorageWnd::USBStorageWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _bInUSBStorage(false)
  , _workStep(USB_NotIn)
  , _originWifiMode(wifi_mode_Off)
  , _bOriginBTOn(false)
  , _counter(0)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pBG = new BmpImage(
        _pPanel,
        Point(0, 0), Size(400, 400),
        "/usr/local/share/ui/BMP/misc/",
        "BG-blue.bmp");

    _pLaptop = new BmpImage(
        _pPanel,
        Point(100, 56), Size(200, 200),
        "/usr/local/share/ui/BMP/misc/",
        "icn-laptop.bmp");
    _pLaptop->SetHiden();

    _pTxt = new StaticText(_pPanel, Point(0, 104), Size(400, 40));
    _pTxt->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_White, CENTER, MIDDLE);
    _pTxt->SetLineHeight(40);
    _pTxt->SetText((UINT8*)"Switch to USB mode?");

    _pYes = new ImageButton(
        _pPanel,
        Point(56, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/misc/",
        "btn-usb-yes-up.bmp",
        "btn-usb-yes-down.bmp");
    _pYes->setOnClickListener(this);

    _pNo = new ImageButton(
        _pPanel,
        Point(216, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/misc/",
        "btn-usb-no-up.bmp",
        "btn-usb-no-down.bmp");
    _pNo->setOnClickListener(this);

    _pExit = new ImageButton(
        _pPanel,
        Point(156, 272), Size(88, 88),
        "/usr/local/share/ui/BMP/misc/",
        "btn-usb-cancel-up.bmp",
        "btn-usb-cancel-down.bmp");
    _pExit->setOnClickListener(this);
    _pExit->SetHiden();
}

USBStorageWnd::~USBStorageWnd()
{
    delete _pExit;
    delete _pNo;
    delete _pYes;
    delete _pTxt;
    delete _pLaptop;
    delete _pBG;
    delete _pPanel;
}

bool USBStorageWnd::_shouldExitUSBStorage()
{
    bool ret = false;
    char tmp[256];
    agcmd_property_get(USBSotragePropertyName, tmp, "");
    if (strcasecmp(tmp, "NA") == 0)
    {
        ret = true;
    }

    //CAMERALOG("It %s exit USB Storage.", ret ? "should" : "should not");
    return ret;
}

bool USBStorageWnd::_isAlreadyUSBStorage()
{
    bool rt = true;

    char tmp[256];
    agcmd_property_get(USBSotragePropertyName, tmp, "");
    //CAMERALOG("USBSotragePropertyName = %s, tmp = %s", USBSotragePropertyName, tmp);
    if (strcasecmp(tmp, "Mount") != 0) {
        rt = false;
    }

    return rt;
}

bool USBStorageWnd::_getOriginalWifiMode()
{
    int state = wifi_mode_Off;;
    char tmp[AGCMD_PROPERTY_SIZE];
    memset(tmp, 0x00, AGCMD_PROPERTY_SIZE);
    agcmd_property_get(WIFI_SWITCH_STATE, tmp, "NA");
    if(strcasecmp(tmp, "OFF") == 0) {
        state = wifi_mode_Off;
    } else {
        agcmd_property_get(WIFI_MODE_KEY, tmp, "NA");
        if (strcasecmp(tmp, WIFI_AP_KEY) == 0) {
            state = wifi_mode_AP;
        } else if (strcasecmp(tmp, WIFI_CLIENT_KEY) == 0) {
            state = wifi_mode_Client;
        } else if (strcasecmp(tmp, WIFI_MULTIROLE) == 0) {
            // under multirole mode, 4 cases:
            //    AP+WLAN
            //    AP
            //    WLAN
            //    0 (no AP or WLAN)
            agcmd_property_get(WIFI_MR_MODE, tmp, "AP");
            if (strcasecmp(tmp, WIFI_AP_KEY) == 0) {
                state = wifi_mode_AP;
            } else if (strcasecmp(tmp, WIFI_CLIENT_KEY) == 0) {
                state = wifi_mode_Client;
            } else if (strcasecmp(tmp, WIFI_MR_KEY) == 0) {
                state = wifi_mode_AP;
            } else {
                state = wifi_mode_Off;
            }
        } else {
            state = wifi_mode_Off;
        }
    }

    _originWifiMode = state;
    return true;
}

bool USBStorageWnd::_getOriginalBTMode()
{
    char tmp[256];
    agcmd_property_get(propBTStatus, tmp, "off");
    if (strcasecmp(tmp, "on") == 0) {
        _bOriginBTOn = true;
    } else {
        _bOriginBTOn = false;
    }

    return true;
}

void USBStorageWnd::onPause()
{
}

void USBStorageWnd::onResume()
{
    if (_isAlreadyUSBStorage()) {
        _bInUSBStorage = true;
        _pYes->SetHiden();
        _pNo->SetHiden();
        _pLaptop->SetShow();
        _pTxt->SetRelativePos(Point(0, 200));
        _pTxt->SetText((UINT8*)"USB Mode");
        _pExit->SetShow();
        _pPanel->Draw(true);
        _workStep = USB_Working;
    }
}

void USBStorageWnd::onClick(Control *widget)
{
    if (_pYes == widget) {
        if (USB_NotIn == _workStep) {
            _workStep = USB_Enabling;
            _pTxt->SetText((UINT8*)"Enabling USB Storage");
            _pYes->SetHiden();
            _pNo->SetHiden();
            _pPanel->Draw(true);

            _cp->EnableUSBStorage();
#ifdef DISABLE_CONNECTION
            // close wifi/bt when enter USB mode
            _getOriginalWifiMode();
            _getOriginalBTMode();
            _cp->SetWifiMode(wifi_mode_Off);
            _cp->EnableBluetooth(false);
#endif
        } else if (USB_Exit_NeedCorfirm == _workStep) {
            _workStep = USB_Exiting;
            CAMERALOG("Exit USB Storage");
            _cp->DisableUSBStorage(true);

#ifdef DISABLE_CONNECTION
            // recovery previous wifi/bt mode
            _cp->SetWifiMode((wifi_mode)_originWifiMode);
            _cp->EnableBluetooth(_bOriginBTOn);
#endif
            this->Close(Anim_Top2BottomExit);
        }
    } else if (_pNo == widget) {
        if (USB_NotIn == _workStep) {
            this->Close(Anim_Top2BottomExit);
        } else if (USB_Exit_NeedCorfirm == _workStep) {
            _pYes->SetHiden();
            _pNo->SetHiden();
            _pLaptop->SetShow();
            _pTxt->SetRelativePos(Point(0, 200));
            _pTxt->SetText((UINT8*)"USB Mode");
            _pTxt->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, CENTER, MIDDLE);
            _pExit->SetShow();
            _pPanel->Draw(true);
        }
    } else if (_pExit == widget) {
        _workStep = USB_Exit_NeedCorfirm;
        _pLaptop->SetHiden();
        _pExit->SetHiden();
        _pTxt->SetRelativePos(Point(0, 120));
        _pTxt->SetText((UINT8*)"Exit USB Mode?");
        _pTxt->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_White, CENTER, MIDDLE);
        _pYes->SetShow();
        _pNo->SetShow();
        _pPanel->Draw(true);
    }
}

bool USBStorageWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter % 10 == 0) {
                    if (!_bInUSBStorage && _isAlreadyUSBStorage()) {
                        _bInUSBStorage = true;
                        _pYes->SetHiden();
                        _pNo->SetHiden();
                        _pLaptop->SetShow();
                        _pTxt->SetRelativePos(Point(0, 200));
                        _pTxt->SetText((UINT8*)"USB Mode");
                        _pExit->SetShow();
                        _pPanel->Draw(true);
                        _workStep = USB_Working;
                    } else if (_bInUSBStorage && _shouldExitUSBStorage()) {
                        // recovery previous wifi/bt mode
                        CAMERALOG("Exit USB Storage");
#ifdef DISABLE_CONNECTION
                        _cp->SetWifiMode((wifi_mode)_originWifiMode);
                        _cp->EnableBluetooth(_bOriginBTOn);
#endif
                        _cp->DisableUSBStorage(false);
                        this->Close(Anim_Top2BottomExit);
                    }
                }
                b = false;
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_usb_storage) {
                    if (_bInUSBStorage) {
                        // recovery previous wifi/bt mode
                        CAMERALOG("Exit USB Storage");
#ifdef DISABLE_CONNECTION
                        _cp->SetWifiMode((wifi_mode)_originWifiMode);
                        _cp->EnableBluetooth(_bOriginBTOn);
#endif
                        _cp->DisableUSBStorage(false);
                        this->Close(Anim_Top2BottomExit);
                    } else if (USB_NotIn == _workStep) {
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                }
            } else if (event->_event == InternalEvent_app_state_update) {
                if (event->_paload == camera_State_enable_usbstorage_result) {
                    if (event->_paload1 == true) {
                        _bInUSBStorage = true;
                        _pLaptop->SetShow();
                        _pTxt->SetRelativePos(Point(0, 200));
                        _pTxt->SetText((UINT8*)"USB Mode");
                        _pExit->SetShow();
                        _pPanel->Draw(true);
                        _workStep = USB_Working;
                    } else {
                        _pTxt->SetText((UINT8*)"USB Mode Failed!");
                        _pPanel->Draw(true);
                        usleep(500 * 1000);
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                }
            }
            break;
        default:
            break;
    }

    return b;
}


CarMountUnplugWnd::CarMountUnplugWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _poweroffCnt(0)
  , _counter(30)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pRing = new Button(_pPanel, Point(20, 20), Size(360, 360), Color_Black, Color_Black);
    _pRing->SetStyle(180);
    _pRing->SetBorder(true, Color_White, 4);
    _pRing->setOnClickListener(this);

    snprintf(_txt, 64, "%s", "00");
    _pTime = new Button(_pPanel, Point(80, 168), Size(240, 100), Color_Black, Color_Black);
    _pTime->SetTextAlign(CENTER, MIDDLE);
    _pTime->SetText((UINT8*)_txt, 64, Color_White);
    _pTime->setOnClickListener(this);

    _pTitle = new StaticText(_pPanel, Point(60, 104), Size(280, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTitle->SetText((UINT8*)"Power OFF in");

    _pYes = new ImageButton(
        _pPanel,
        Point(56, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-yes_alt-up.bmp",
        "btn-dashboard-yes_alt-down.bmp");
    _pYes->setOnClickListener(this);
    _pYes->SetHiden();

    _pNo = new ImageButton(
        _pPanel,
        Point(216, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-no-up.bmp",
        "btn-dashboard-no-down.bmp");
    _pNo->setOnClickListener(this);
    _pNo->SetHiden();

    _pAnimation = new BmpImage(
        _pPanel,
        Point(136, 200), Size(128, 128),
        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
        "power_OFF-spinner_00000.bmp");
    _pAnimation->SetHiden();

    _getAutoOffTime();
}

CarMountUnplugWnd::~CarMountUnplugWnd()
{
    delete _pAnimation;
    delete _pRing;
    delete _pNo;
    delete _pYes;
    delete _pTime;
    delete _pTitle;
    delete _pPanel;
}

int CarMountUnplugWnd::_getAutoOffTime()
{
    char tmp[256];
    agcmd_property_get(PropName_Udc_Power_Delay, tmp, Udc_Power_Delay_Default);
    if (strcasecmp(tmp, "Never") == 0) {
        _counter = -1;
    } else if (strcasecmp(tmp, "10s") == 0) {
        _counter = 10;
    } else if (strcasecmp(tmp, "30s") == 0) {
        _counter = 30;
    } else if (strcasecmp(tmp, "60s") == 0) {
        _counter = 60;
    } else if (strcasecmp(tmp, "2min") == 0) {
        _counter = 120;
    } else if (strcasecmp(tmp, "5min") == 0) {
        _counter = 300;
    }

    _counter = _counter * 10;
    return _counter;
}

bool CarMountUnplugWnd::_shouldContinueCountdown()
{
    int ret_val = -1;
    char usbphy0_data[1024];
    char *sw_str = NULL;
    bool bContinue = true;

    ret_val = aglib_read_file_buffer("/proc/ambarella/usbphy0",
        usbphy0_data, (sizeof(usbphy0_data) - 1));
    if (ret_val < 0) {
        CAMERALOG("read /proc/ambarella/usbphy0 failed with error %d", ret_val);
        return bContinue;
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

    if (strcmp(status, "DEVICE") == 0) {
        if ((vbus == 1) && (gpio == 0)) {
            // Mount_DEVICE_Status_USB_Mount;
            bContinue = false;
        }
    } else if (strcmp(status, "HOST") == 0) {
        if ((vbus == 1) && (gpio == 1)) {
            // Mount_DEVICE_Status_Car_Mount_With_Power;
            bContinue = false;
        }
    }

    //CAMERALOG("It should %s continue count down to power off", bContinue ? "" : "not");
    return bContinue;
}

void CarMountUnplugWnd::onPause()
{
}

void CarMountUnplugWnd::onResume()
{
    if (_counter / 10 > 60) {
        snprintf(_txt, 64, "%02d:%02d", (_counter / 10) / 60, (_counter / 10) % 60);
    } else {
        snprintf(_txt, 64, "%02d", _counter / 10);
    }
    _pPanel->Draw(true);
}

void CarMountUnplugWnd::onClick(Control *widget)
{
    if (_pYes == widget) {
        _counter = 0;
        _pTitle->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, LEFT, MIDDLE);
        _pTitle->SetRelativePos(Point(60, 80));
        _pTitle->SetText((UINT8*)"Shutting down...");
        _pTime->SetHiden();
        _pYes->SetHiden();
        _pNo->SetHiden();
        //_pAnimation->SetShow();
        _pPanel->Draw(true);

        _poweroffCnt = 10;
        CAMERALOG("Call \"agsh poweroff\" to power off now!");
        const char *tmp_args[4];
        tmp_args[0] = "agsh";
        tmp_args[1] = "poweroff";
        tmp_args[2] = NULL;
        agbox_sh(2, (const char *const *)tmp_args);
    } else if (_pNo == widget) {
        this->Close(Anim_Top2BottomExit);
    } else if ((_pTime == widget) || (_pRing == widget)) {
        _pRing->SetHiden();
        _pYes->SetShow();
        _pNo->SetShow();
        _pTitle->SetRelativePos(Point(50, 80));
        _pTitle->SetText((UINT8*)"Power OFF?");
        _pTime->SetRelativePos(Point(80, 120));
        _pTime->SetText((UINT8*)_txt, 32, Color_White);

        _pPanel->Draw(true);
    }
}

bool CarMountUnplugWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_poweroffCnt > 0) {
                    if (_poweroffCnt % 20 == 0) {
                        _pTitle->SetText((UINT8*)"Shutting down");
                    } else if (_poweroffCnt % 20 == 5) {
                        _pTitle->SetText((UINT8*)"Shutting down.");
                    } else if (_poweroffCnt % 20 == 10) {
                        _pTitle->SetText((UINT8*)"Shutting down..");
                    } else if (_poweroffCnt % 20 == 15) {
                        _pTitle->SetText((UINT8*)"Shutting down...");
                    }

                    char source[128];
                    snprintf(source, 128, "power_OFF-spinner_%05d.bmp", _poweroffCnt % 10);
                    //CAMERALOG("source = %s", source);
                    _pAnimation->setSource(
                        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                        source);
                    _pPanel->Draw(true);

                    _poweroffCnt++;
                    break;
                }
                if (_counter > 0) {
                    _counter--;
                    if (_counter % 10 == 0) {
                        if (!_shouldContinueCountdown()) {
                            this->Close(Anim_Null);
                            b = false;
                        } else {
                            if (_counter / 10 > 60) {
                                snprintf(_txt, 64, "%02d:%02d", (_counter / 10) / 60, (_counter / 10) % 60);
                            } else {
                                snprintf(_txt, 64, "%02d", _counter / 10);
                            }
                            _pPanel->Draw(true);
                            if (_counter <= 0) {
                                _pTitle->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, LEFT, MIDDLE);
                                _pTitle->SetRelativePos(Point(60, 80));
                                _pTitle->SetText((UINT8*)"Shutting down...");
                                _pRing->SetHiden();
                                _pTime->SetHiden();
                                _pYes->SetHiden();
                                _pNo->SetHiden();
                                _pTime->SetHiden();
                                //_pAnimation->SetShow();
                                _pPanel->Draw(true);

                                _poweroffCnt = 10;
                                CAMERALOG("Call \"agsh poweroff\" to power off now!");
                                const char *tmp_args[4];
                                tmp_args[0] = "agsh";
                                tmp_args[1] = "poweroff";
                                tmp_args[2] = NULL;
                                agbox_sh(2, (const char *const *)tmp_args);
                            }
                        }
                    }
                } else {
                    this->Close(Anim_Null);
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
                this->Close(Anim_Null);
                b = false;
            } else if ((event->_event == MountEvent_Plug_To_USB)
                && (event->_paload == 1))
            {
                CAMERALOG("USB mount is plugged after unplug from car mount!");
                this->Close(Anim_Null);
                b = false;
            }
            break;
        default:
            break;
    }

    return b;
}


SpaceFullWnd::SpaceFullWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pOKFake = new Button(_pPanel, Point(50, 200), Size(300, 200), Color_Black, Color_Black);
    _pOKFake->setOnClickListener(this);

    _pRing = new FanRing(_pPanel, Point(200, 200),
        199, 168,
        Color_Yellow_2, Color_Black);
    _pRing->setAngles(0.0, 360.0);

    _pIcon = new BmpImage(
        _pPanel,
        Point(176, 56), Size(48, 48),
        "/usr/local/share/ui/BMP/misc/",
        "icn-TF-full.bmp");

    _pTxtError = new StaticText(_pPanel, Point(100, 120), Size(200, 40));
    _pTxtError->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Yellow_2, CENTER, MIDDLE);
    _pTxtError->SetText((UINT8*)"Card Full");

    _pTxtHint = new StaticText(_pPanel, Point(50, 174), Size(300, 100));
    _pTxtHint->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxtHint->SetLineHeight(40);
    _pTxtHint->SetText((UINT8*)"Recording would\nstop soon.");

    _pOK = new Button(_pPanel, Point(150, 260), Size(100, 100), Color_Black, Color_Grey_1);
    _pOK->SetText((UINT8 *)"OK", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pOK->setOnClickListener(this);
}

SpaceFullWnd::~SpaceFullWnd()
{
    delete _pOKFake;
    delete _pOK;
    delete _pTxtHint;
    delete _pTxtError;
    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void SpaceFullWnd::onPause()
{
}

void SpaceFullWnd::onResume()
{
}

void SpaceFullWnd::onClick(Control *widget)
{
    if ((_pOK == widget) || (_pOKFake == widget)) {
        this->Close(Anim_Top2BottomExit);
    }
}

bool SpaceFullWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    this->Close(Anim_Top2BottomExit);
                }
            }
            break;
        default:
            break;
    }

    return b;
}


CardMissingWnd::CardMissingWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pOKFake = new Button(_pPanel, Point(50, 200), Size(300, 200), Color_Black, Color_Black);
    _pOKFake->setOnClickListener(this);

    _pRing = new FanRing(_pPanel, Point(200, 200),
        199, 168,
        Color_Red_1, Color_Black);
    _pRing->setAngles(0.0, 360.0);

    _pIcon = new BmpImage(
        _pPanel,
        Point(176, 56), Size(48, 48),
        "/usr/local/share/ui/BMP/misc/",
        "icn-TF-missing.bmp");

    _pTxtError = new StaticText(_pPanel, Point(50, 120), Size(300, 40));
    _pTxtError->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Red_1, CENTER, MIDDLE);
    _pTxtError->SetText((UINT8*)"Card Missing");

    _pTxtHint = new StaticText(_pPanel, Point(50, 174), Size(300, 100));
    _pTxtHint->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxtHint->SetLineHeight(40);
    _pTxtHint->SetText((UINT8*)"Please insert a\nmicroSD card.");

    _pOK = new Button(_pPanel, Point(150, 260), Size(100, 100), Color_Black, Color_Grey_1);
    _pOK->SetText((UINT8 *)"OK", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pOK->setOnClickListener(this);
}

CardMissingWnd::~CardMissingWnd()
{
    delete _pOKFake;
    delete _pOK;
    delete _pTxtHint;
    delete _pTxtError;
    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void CardMissingWnd::onPause()
{
}

void CardMissingWnd::onResume()
{
    CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Alert);
}

void CardMissingWnd::onClick(Control *widget)
{
    if ((_pOK == widget) || (_pOKFake == widget)) {
        this->Close(Anim_Top2BottomExit);
    }
}

bool CardMissingWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    this->Close(Anim_Top2BottomExit);
                }
            }
            break;
        default:
            break;
    }

    return b;
}


WriteDiskErrorWnd::WriteDiskErrorWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pOKFake = new Button(_pPanel, Point(50, 200), Size(300, 200), Color_Black, Color_Black);
    _pOKFake->setOnClickListener(this);

    _pRing = new FanRing(_pPanel, Point(200, 200),
        199, 168,
        Color_Red_1, Color_Black);
    _pRing->setAngles(0.0, 360.0);

    _pIcon = new BmpImage(
        _pPanel,
        Point(176, 56), Size(48, 48),
        "/usr/local/share/ui/BMP/misc/",
        "icn-TF-error.bmp");

    _pTxtError = new StaticText(_pPanel, Point(100, 114), Size(200, 40));
    _pTxtError->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Red_1, CENTER, MIDDLE);
    _pTxtError->SetText((UINT8*)"Card Error");

    _pTxtHint = new StaticText(_pPanel, Point(50, 174), Size(300, 100));
    _pTxtHint->SetStyle(24, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxtHint->SetLineHeight(40);
    _pTxtHint->SetText((UINT8*)"Please change your\nmicroSD card.");

    _pOK = new Button(_pPanel, Point(150, 260), Size(100, 100), Color_Black, Color_Grey_1);
    _pOK->SetText((UINT8 *)"OK", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pOK->setOnClickListener(this);
}

WriteDiskErrorWnd::~WriteDiskErrorWnd()
{
    delete _pOKFake;
    delete _pOK;
    delete _pTxtHint;
    delete _pTxtError;
    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void WriteDiskErrorWnd::onPause()
{
}

void WriteDiskErrorWnd::onResume()
{
}

void WriteDiskErrorWnd::onClick(Control *widget)
{
    if ((_pOK == widget) || (_pOKFake == widget)) {
        this->Close(Anim_Top2BottomExit);
    }
}

bool WriteDiskErrorWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    this->Close(Anim_Top2BottomExit);
                }
            }
            break;
        default:
            break;
    }

    return b;
}


UnknowSDFormatWnd::UnknowSDFormatWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _formatStep(FormatStep_Idle)
    , _counter(0)
    , _delayClose(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pRing = new Circle(_pPanel, Point(200, 200),
        183, 32,
        Color_Yellow_2);

    _pIcon = new BmpImage(
        _pPanel,
        Point(176, 56), Size(48, 48),
        "/usr/local/share/ui/BMP/misc/",
        "icn-TF-incompatible.bmp");

    _pAnimation = new BmpImage(
        _pPanel,
        Point(60, 60), Size(280, 280),
        "/usr/local/share/ui/BMP/animation/format_TF_card-spinner/",
        "format_TF_card-spinner_00000.bmp");
    _pAnimation->SetHiden();

    snprintf(_txt, 64, "Card Incompatible");
    _pFormatInfo = new StaticText(_pPanel, Point(50, 130), Size(300, 120));
    _pFormatInfo->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Yellow_2, CENTER, TOP);
    _pFormatInfo->SetLineHeight(40);
    _pFormatInfo->SetText((UINT8 *)_txt);

    _pFormatHint = new StaticText(_pPanel, Point(50, 180), Size(300, 40));
    _pFormatHint->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, TOP);
    _pFormatHint->SetText((UINT8 *)"Eject or format.");

    _pFormat = new Button(_pPanel, Point(120, 260), Size(160, 48), Color_Grey_2, Color_Grey_1);
    _pFormat->SetText((UINT8 *)"Format", 32, Color_White);
    _pFormat->SetStyle(24);
    _pFormat->SetBorder(true, Color_White, 4);
    _pFormat->setOnClickListener(this);

    _pYes = new ImageButton(
        _pPanel,
        Point(56, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-yes_alt-up.bmp",
        "btn-dashboard-yes_alt-down.bmp");
    _pYes->setOnClickListener(this);
    _pYes->SetHiden();

    _pNo = new ImageButton(
        _pPanel,
        Point(216, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-no-up.bmp",
        "btn-dashboard-no-down.bmp");
    _pNo->setOnClickListener(this);
    _pNo->SetHiden();
}

UnknowSDFormatWnd::~UnknowSDFormatWnd()
{
    delete _pAnimation;
    delete _pNo;
    delete _pYes;
    delete _pFormat;
    delete _pFormatHint;
    delete _pFormatInfo;
    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void UnknowSDFormatWnd::updateStorageStatus()
{
    if (_formatStep != FormatStep_Formatting) {
        char storage_prop[256];
        getStoragePropertyName(storage_prop, sizeof(storage_prop));
        char tmp[256];
        memset(tmp, 0x00, sizeof(tmp));
        agcmd_property_get(storage_prop, tmp, "");
        if (strcasecmp(tmp, "NA") == 0) {
            this->Close(Anim_Top2BottomExit);
        }
    }
}

void UnknowSDFormatWnd::onResume()
{
}

void UnknowSDFormatWnd::onPause()
{
}

void UnknowSDFormatWnd::onClick(Control *widget)
{
    if (widget == _pFormat) {
        _pRing->SetHiden();
        _pIcon->SetHiden();
        _pFormat->SetHiden();
        _pFormatHint->SetHiden();
        snprintf(_txt, 64, "Sure to format\nthe microSD card?");
        _pFormatInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, TOP);
        _pFormatInfo->SetRelativePos(Point(50, 76));
        _pFormatInfo->SetShow();
        _pYes->SetShow();
        _pYes->SetRelativePos(Point(216, 216));
        _pNo->SetShow();
        _pNo->SetRelativePos(Point(56, 216));
        _pPanel->Draw(true);
    } else if (widget == _pYes) {
        if (_formatStep == FormatStep_Idle) {
            _pRing->SetHiden();
            _pIcon->SetHiden();
            _pFormat->SetHiden();
            snprintf(_txt, 64, "All data will be\nerased. Proceed?");
            _pYes->SetShow();
            _pYes->SetRelativePos(Point(56, 216));
            _pNo->SetShow();
            _pNo->SetRelativePos(Point(216, 216));
            _pPanel->Draw(true);
            _formatStep = FormatStep_NeedCorfirm;
        } else if (_formatStep == FormatStep_NeedCorfirm) {
            CAMERALOG("Format");
            _cp->FormatStorage();
            snprintf(_txt, 64, "Formatting");
            _pFormatInfo->SetRelativePos(Point(50, 180));
            _pAnimation->SetShow();
            _pRing->SetHiden();
            _pIcon->SetHiden();
            _pFormat->SetHiden();
            _pYes->SetHiden();
            _pNo->SetHiden();
            _pPanel->Draw(true);
            _formatStep = FormatStep_Formatting;
            _counter = 0;
        }
    } else if (widget == _pNo) {
        snprintf(_txt, 64, "Card Incompatible");
        _pFormatInfo->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Yellow_2, CENTER, TOP);
        _pFormatInfo->SetRelativePos(Point(50, 130));
        _pFormatHint->SetShow();
        _pRing->SetShow();
        _pIcon->SetShow();
        _pFormat->SetShow();
        _pYes->SetHiden();
        _pNo->SetHiden();
        _pPanel->Draw(true);
        _formatStep = FormatStep_Idle;
    }
}

bool UnknowSDFormatWnd::OnEvent(CEvent *event)
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
                } else if (_formatStep == FormatStep_Formatting) {
                    char source[128];
                    snprintf(source, 128, "format_TF_card-spinner_%05d.bmp", (_counter++) % 20);
                    //CAMERALOG("source = %s", source);
                    _pAnimation->setSource(
                        "/usr/local/share/ui/BMP/animation/format_TF_card-spinner/",
                        source);
                    _pPanel->Draw(true);
                }
                _counter++;
            } else if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    updateStorageStatus();
                    b = false;
                }
            } else if ((event->_event == InternalEvent_app_state_update)) {
                if (event->_paload == camera_State_FormatSD_result) {
                    if (event->_paload1 != false) {
                        snprintf(_txt, 64, "Format Success!");
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _delayClose = 5;
                    } else {
                        snprintf(_txt, 64, "Format Failed!\nPlease try other\ntools!");
                        _pFormatInfo->SetRelativePos(Point(50, 140));
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                    }
                    _pPanel->Draw(true);
                    _formatStep = FormatStep_Idle;
                    b = false;
                }
            }
            break;
    }

    return b;
}


BTNotifyWnd::BTNotifyWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop,
        Notification_Type type)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _pBtnOK(NULL)
    , _pInfo(NULL)
    , _pBtnYes(NULL)
    , _pBtnNo(NULL)
    , _counter(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pBtnOK = new Button(_pPanel, Point(100, 180), Size(200, 200), Color_Black, Color_Grey_1);
    _pBtnOK->SetText((UINT8 *)"OK", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pBtnOK->SetTextAlign(CENTER, BOTTOM);
    _pBtnOK->setOnClickListener(this);
    _pBtnOK->SetHiden();

    _pRing = new Circle(_pPanel, Point(200, 200),
        183, 32,
        Color_Green_1);

    _pIcon = new BmpImage(
        _pPanel,
        Point(152, 96), Size(96, 96),
        "/usr/local/share/ui/BMP/misc/",
        "icn-obd-notification-ON.bmp");

    _pTxtHint = new StaticText(_pPanel, Point(50, 216), Size(300, 100));
    _pTxtHint->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxtHint->SetLineHeight(40);
    _pTxtHint->SetText((UINT8 *)"OBD Transmitter\nConnected");

    if (type == Notification_Type_OBD_Connected) {
    } else if (type == Notification_Type_OBD_Disconnected) {
        _pIcon->setSource("/usr/local/share/ui/BMP/misc/",
            "icn-obd-notification-OFF.bmp");
        _pTxtHint->SetText((UINT8 *)"OBD Transmitter\nDisconnected");
        _pRing->setColor(Color_Yellow_2);
    } else if (type == Notification_Type_HID_Connected) {
        _pIcon->setSource("/usr/local/share/ui/BMP/misc/",
            "icn-RC-notification-ON.bmp");
        _pTxtHint->SetText((UINT8 *)"Remote Control\nConnected");
        _pRing->setColor(Color_Green_1);
    } else if (type == Notification_Type_HID_Disconnected) {
        _pIcon->setSource("/usr/local/share/ui/BMP/misc/",
            "icn-RC-notification-OFF.bmp");
        _pTxtHint->SetText((UINT8 *)"Remote Control\nDisconnected");
        _pRing->setColor(Color_Yellow_2);
    }

    _type = type;
    _counter = 20;
}

BTNotifyWnd::~BTNotifyWnd()
{
    delete _pBtnNo;
    delete _pBtnYes;
    delete _pInfo;
    delete _pBtnOK;

    delete _pTxtHint;
    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void BTNotifyWnd::updateStatus()
{
    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        if (_type == Notification_Type_OBD_Connected) {
            agcmd_property_get(propBTTempOBDStatus, tmp, "");
            if (strcasecmp(tmp, "on") != 0) {
                this->Close(Anim_Null);
            }
        } else if (_type == Notification_Type_HID_Connected) {
            agcmd_property_get(propBTTempHIDStatus, tmp, "");
            if (strcasecmp(tmp, "on") != 0) {
                this->Close(Anim_Null);
            }
        }
    } else {
        if (_type == Notification_Type_OBD_Disconnected) {
            agcmd_property_get(propBTTempOBDStatus, tmp, "");
            if (strcasecmp(tmp, "on") == 0) {
                this->Close(Anim_Null);
            }
        } else if (_type == Notification_Type_HID_Disconnected) {
            agcmd_property_get(propBTTempHIDStatus, tmp, "");
            if (strcasecmp(tmp, "on") == 0) {
                this->Close(Anim_Null);
            }
        }
    }
}

void BTNotifyWnd::onResume()
{
    updateStatus();
}

void BTNotifyWnd::onPause()
{
}

void BTNotifyWnd::onClick(Control *widget)
{
    if (_pBtnOK == widget) {
        _pTxtHint->SetHiden();
        _pIcon->SetHiden();
        _pRing->SetHiden();
        _pBtnOK->SetHiden();

        _pInfo = new StaticText(_pPanel, Point(0, 76), Size(400, 80));
        _pInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
        _pInfo->SetLineHeight(40);
        _pInfo->SetText((UINT8 *)"Remind me\nnext time?");

        _pBtnNo = new ImageButton(
            _pPanel,
            Point(216, 216), Size(128, 128),
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-no-up.bmp",
            "btn-dashboard-no-down.bmp");
        _pBtnNo->setOnClickListener(this);

        _pBtnYes = new ImageButton(
            _pPanel,
            Point(56, 216), Size(128, 128),
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-yes_alt-up.bmp",
            "btn-dashboard-yes_alt-down.bmp");
        _pBtnYes->setOnClickListener(this);

        _pPanel->Draw(true);
    } else if (_pBtnNo == widget) {
        agcmd_property_set(Prop_Disable_RC_Batt_Warning, "disabled");
        this->Close(Anim_Null);
    } else if (_pBtnYes == widget) {
        this->Close(Anim_Null);
    }
}

bool BTNotifyWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_counter > 0) {
                    if (_counter == 20) {
                        if ((_type == Notification_Type_OBD_Connected)
                            || (_type == Notification_Type_HID_Connected))
                        {
                            CAudioPlayer::getInstance()->playSoundEffect(
                                CAudioPlayer::SE_Connected);
                        } else if ((_type == Notification_Type_OBD_Disconnected)
                            || (_type == Notification_Type_HID_Disconnected))
                        {
                            CAudioPlayer::getInstance()->playSoundEffect(
                                CAudioPlayer::SE_Disconnected);
                        }
                    }
                    _counter--;
                    if (_counter <= 0) {
                        char tmp[256];
                        // delay some time to read battery level:
                        agcmd_property_get(Prop_RC_Battery_level, tmp, "0");
                        _batt_level = atoi(tmp);
                        //CAMERALOG("_batt_level = %d, tmp = %s", _batt_level, tmp);
                        if (_batt_level >= 40) {
                            agcmd_property_del(Prop_Disable_RC_Batt_Warning);
                        }
                        agcmd_property_get(Prop_Disable_RC_Batt_Warning, tmp, "enabled");
                        if ((_type == Notification_Type_HID_Connected)
                            && (_batt_level > 0) && (_batt_level < RC_Batt_Warning_Level)
                            && (strcasecmp(tmp, "enabled") == 0))
                        {
                            _pIcon->setSource("/usr/local/share/ui/BMP/misc/",
                                "icn-RC-notification-ON.bmp");
                            _pTxtHint->SetText((UINT8 *)"Remote Control\nLow Battery");
                            _pTxtHint->SetRelativePos(Point(50, 210));
                            _pRing->setColor(Color_Yellow_2);
                            _pBtnOK->SetShow();
                            _pPanel->Draw(true);

                            CAudioPlayer::getInstance()->playSoundEffect(
                                CAudioPlayer::SE_Alert);
                        } else {
                            this->Close(Anim_Null);
                        }
                    }
                }
            } else if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    updateStatus();
                    b = false;
                }
            }
            break;
    }

    return b;
}


OBDConnectedWnd::OBDConnectedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTNotifyWnd(cp, name, wndSize, leftTop, BTNotifyWnd::Notification_Type_OBD_Connected)
{
}

OBDConnectedWnd::~OBDConnectedWnd()
{
}


OBDDisconnectedWnd::OBDDisconnectedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTNotifyWnd(cp, name, wndSize, leftTop, BTNotifyWnd::Notification_Type_OBD_Disconnected)
{
}

OBDDisconnectedWnd::~OBDDisconnectedWnd()
{
}


HIDConnectedWnd::HIDConnectedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTNotifyWnd(cp, name, wndSize, leftTop, BTNotifyWnd::Notification_Type_HID_Connected)
{
}

HIDConnectedWnd::~HIDConnectedWnd()
{
}


HIDDisconnectedWnd::HIDDisconnectedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTNotifyWnd(cp, name, wndSize, leftTop, BTNotifyWnd::Notification_Type_HID_Disconnected)
{
}

HIDDisconnectedWnd::~HIDDisconnectedWnd()
{
}


BTDefectWnd::BTDefectWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _pTxtDefect(NULL)
    , _pBtnClear(NULL)
    , _pBtnIgnore(NULL)
    , _pList(NULL)
    , _pAdapter(NULL)
    , _num(0)
{
    for (int i = 0; i < MAX_CODES; i++) {
        _dtc_codes[i] = NULL;
    }

    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 10);
    this->SetMainObject(_pPanel);

    _pRing = new Circle(_pPanel, Point(200, 200),
        183, 32,
        Color_Red_1);

    _pTxtHint = new StaticText(_pPanel, Point(100, 60), Size(200, 40));
    _pTxtHint->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Red_1, CENTER, MIDDLE);
    _pTxtHint->SetText((UINT8*)"OBD DTC");

    if (bReleaseVersion) {
        _pBtnIgnore = new Button(_pPanel, Point(130, 280), Size(140, 80), Color_Black, Color_Grey_1);
        _pBtnIgnore->SetText((UINT8 *)"Ignore", 28, Color_White, FONT_ROBOTOMONO_Bold);
        _pBtnIgnore->SetTextAlign(CENTER, MIDDLE);
        _pBtnIgnore->SetBorder(true, Color_White, 4);
        _pBtnIgnore->SetStyle(0, 0.0);
        _pBtnIgnore->setOnClickListener(this);
    } else {
        _pBtnClear = new Button(_pPanel, Point(75, 280), Size(120, 80), Color_Black, Color_Grey_1);
        _pBtnClear->SetText((UINT8 *)"Clear", 28, Color_White, FONT_ROBOTOMONO_Bold);
        _pBtnClear->SetTextAlign(CENTER, MIDDLE);
        _pBtnClear->SetBorder(true, Color_White, 4);
        _pBtnClear->SetStyle(0, 0.0);
        _pBtnClear->setOnClickListener(this);

        _pBtnIgnore = new Button(_pPanel, Point(200, 280), Size(120, 80), Color_Black, Color_Grey_1);
        _pBtnIgnore->SetText((UINT8 *)"Ignore", 28, Color_White, FONT_ROBOTOMONO_Bold);
        _pBtnIgnore->SetTextAlign(CENTER, MIDDLE);
        _pBtnIgnore->SetBorder(true, Color_White, 4);
        _pBtnIgnore->SetStyle(0, 0.0);
        _pBtnIgnore->setOnClickListener(this);
    }
}

BTDefectWnd::~BTDefectWnd()
{
    delete _pList;
    for (int i = 0; i < _num; i++) {
        delete[] _dtc_codes[i];
    }
    delete _pAdapter;

    delete _pTxtDefect;

    delete _pBtnIgnore;
    delete _pBtnClear;

    delete _pTxtHint;

    delete _pRing;
    delete _pPanel;
}

void BTDefectWnd::updateStatus()
{
    if (_pList) {
        delete _pList;
        _pList = NULL;
        delete _pAdapter;
        _pAdapter = NULL;
        for (int i = 0; i < _num; i++) {
            delete[] _dtc_codes[i];
            _dtc_codes[i] = NULL;
        }
    }

    if (_pTxtDefect) {
        delete _pTxtDefect;
        _pTxtDefect = NULL;
    }

    char tmp[256];
    agcmd_property_get(Prop_OBD_Defect, tmp, "");
    if (strcasecmp(tmp, "") != 0) {
        _num = 0;
        char *str_1 = tmp;
        char *str_2 = NULL;
        str_2 = strstr(tmp, ";");
        while (str_2 && (_num <= MAX_CODES)) {
            _dtc_codes[_num] = new char [str_2 - str_1 + 4];
            if (!_dtc_codes[_num]) {
                CAMERALOG("Out of memory!");
                break;
            }
            snprintf(_dtc_codes[_num], str_2 - str_1 + 4, "DTC%s", str_1);
            _num++;

            str_1 = str_2 + 1;
            str_2 = strstr(str_1, ";");
        }

        if (_num <= 4) {
            memset(_txtDefect, 0x00, sizeof(_txtDefect));
            int len = 0;
            for (int i = 0; i < _num; i++) {
                len += snprintf(_txtDefect + len, sizeof(_txtDefect) - len,
                    "%s%s", (i > 0) ? "\n" : "", _dtc_codes[i]);
            }
            _pTxtDefect = new StaticText(_pPanel, Point(65, 100), Size(270, 180));
            _pTxtDefect->SetStyle(24, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
            _pTxtDefect->SetLineHeight(40);
            _pTxtDefect->SetText((UINT8*)_txtDefect);
        } else {
            _pList = new List(_pPanel, this, Point(65, 100), Size(270, 180), 20);

            _pAdapter = new TextListAdapter();
            _pAdapter->setTextList((UINT8 **)_dtc_codes, _num);
            _pAdapter->setItemStyle(Size(270, 40), Color_Black, Color_Grey_1,
                Color_Grey_4, 24, CENTER, MIDDLE);
            _pAdapter->setObserver(_pList);

            _pList->setItemPadding(0);
            _pList->setAdaptor(_pAdapter);
        }
    } else {
        snprintf(_txtDefect, sizeof(_txtDefect), "Read failed");
        _pTxtDefect = new StaticText(_pPanel, Point(40, 100), Size(320, 180));
        _pTxtDefect->SetStyle(24, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
        _pTxtDefect->SetLineHeight(40);
        _pTxtDefect->SetText((UINT8*)_txtDefect);
    }
}

void BTDefectWnd::onResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void BTDefectWnd::onPause()
{
}

void BTDefectWnd::onClick(Control *widget)
{
    if (_pBtnClear == widget) {
        struct agobd_client_info_s *obd_client = agobd_open_client();
        if (obd_client) {
            agobd_client_clr_obd(obd_client);
            agobd_close_client(obd_client);
        }
        this->Close(Anim_Null);
    } else if (_pBtnIgnore == widget) {
        this->Close(Anim_Null);
    }
}

bool BTDefectWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {

            } else if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    updateStatus();
                    b = false;
                }
            }
            break;
    }

    return b;
}


BTUpgradingWnd::BTUpgradingWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop,
        Upgrading_Type type)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _counter(0)
    , _type(type)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 10);
    this->SetMainObject(_pPanel);

    _pRing = new Circle(_pPanel, Point(200, 200),
        183, 32,
        Color_Blue_1);

    _pIcon = new BmpImage(
        _pPanel,
        Point(152, 96), Size(96, 96),
        "/usr/local/share/ui/BMP/misc/",
        "icn-RC-notification-OFF.bmp");

    _pTxtHint = new StaticText(_pPanel, Point(50, 220), Size(300, 40));
    _pTxtHint->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxtHint->SetLineHeight(40);
    _pTxtHint->SetText((UINT8 *)"Remote Control");

    snprintf(_txtUpgrading, sizeof(_txtUpgrading), "Upgrading");
    _pTxtUpgrading = new StaticText(_pPanel, Point(110, 260), Size(240, 40));
    _pTxtUpgrading->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, LEFT, MIDDLE);
    _pTxtUpgrading->SetLineHeight(40);
    _pTxtUpgrading->SetText((UINT8 *)_txtUpgrading);

    if (type == Upgrading_Type_HID) {
        // no need to change
    } else if (type == Upgrading_Type_OBD) {
        _pIcon->setSource("/usr/local/share/ui/BMP/misc/",
            "icn-obd-notification-OFF.bmp");
        _pTxtHint->SetText((UINT8 *)"OBD Transmitter");
    }
}

BTUpgradingWnd::~BTUpgradingWnd()
{
    delete _pTxtUpgrading;
    delete _pTxtHint;
    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void BTUpgradingWnd::updateStatus()
{
    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        if (_type == Upgrading_Type_OBD) {
            agcmd_property_get(propBTTempOBDStatus, tmp, "");
            if (strcasecmp(tmp, "upgrading") != 0) {
                this->Close(Anim_Null);
            }
        } else if (_type == Upgrading_Type_HID) {
            agcmd_property_get(propBTTempHIDStatus, tmp, "");
            if (strcasecmp(tmp, "upgrading") != 0) {
                this->Close(Anim_Null);
            }
        }
    } else {
        this->Close(Anim_Null);
    }
}

void BTUpgradingWnd::onResume()
{
    updateStatus();
}

void BTUpgradingWnd::onPause()
{
}

bool BTUpgradingWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                bool redraw = false;
                if (_counter % 20 == 0) {
                    snprintf(_txtUpgrading, sizeof(_txtUpgrading), "Upgrading");
                    redraw = true;
                } else if (_counter % 20 == 5) {
                    snprintf(_txtUpgrading, sizeof(_txtUpgrading), "Upgrading.");
                    redraw = true;
                } else if (_counter % 20 == 10) {
                    snprintf(_txtUpgrading, sizeof(_txtUpgrading), "Upgrading..");
                    redraw = true;
                } else if (_counter % 20 == 15) {
                    snprintf(_txtUpgrading, sizeof(_txtUpgrading), "Upgrading...");
                    redraw = true;
                }

                if (redraw) {
                    _pPanel->Draw(true);
                }
            } else if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    updateStatus();
                    b = false;
                }
            }
            break;
    }

    return b;
}


HIDUpgradingWnd::HIDUpgradingWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTUpgradingWnd(cp, name, wndSize, leftTop, BTUpgradingWnd::Upgrading_Type_HID)
{
}

HIDUpgradingWnd::~HIDUpgradingWnd()
{
}


OBDUpgradingWnd::OBDUpgradingWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTUpgradingWnd(cp, name, wndSize, leftTop, BTUpgradingWnd::Upgrading_Type_OBD)
{
}

OBDUpgradingWnd::~OBDUpgradingWnd()
{
}


HighlightWnd::HighlightWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _counter(0)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pIcon = new BmpImage(
        _pPanel,
        Point(0, 0), Size(400, 400),
        "/usr/local/share/ui/BMP/animation/highlight_successful/",
        "highlight_successful_00000.bmp");
}

HighlightWnd::~HighlightWnd()
{
    delete _pIcon;
    delete _pPanel;
}

void HighlightWnd::onPause()
{
}

void HighlightWnd::onResume()
{
    _counter = 0;
}

bool HighlightWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if ((event->_event == InternalEvent_app_timer_update)) {
#ifdef ENABLE_LOUDSPEAKER
                if (_counter == 0) {
                    CAudioPlayer::getInstance()->playSoundEffect(
                        CAudioPlayer::SE_SingleShot);
                }
#endif
                char name[128];
                snprintf(name, 128, "highlight_successful_%05d.bmp", _counter);
                _pIcon->setSource(
                    "/usr/local/share/ui/BMP/animation/highlight_successful/",
                    name);
                _pPanel->Draw(true);

                _counter++;
                if (_counter >= 10) {
                    _pIcon->setSource(
                        "/usr/local/share/ui/BMP/animation/highlight_successful/",
                        "highlight_successful_00000.bmp");
                    this->Close(Anim_Null);
                }
            }
            break;
    }

    return b;
}


HighlightFailedWnd::HighlightFailedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _counter(50)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pButtonFake = new Button(_pPanel, Point(50, 50), Size(300, 300), Color_Black, Color_Black);
    _pButtonFake->setOnClickListener(this);

    _pRing = new Circle(_pPanel, Point(200, 200),
        183, 32,
        Color_Red_1);

    _pIcon = new BmpImage(
        _pPanel,
        Point(176, 56), Size(48, 48),
        "/usr/local/share/ui/BMP/misc/",
        "icn-hightlight-error.bmp");

    _pTxtTitle = new StaticText(_pPanel, Point(50, 116), Size(300, 40));
    _pTxtTitle->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Red_1, CENTER, MIDDLE);
    _pTxtTitle->SetText((UINT8 *)"Highlight Error");

    _pTxtHint = new StaticText(_pPanel, Point(50, 172), Size(300, 80));
    _pTxtHint->SetStyle(24, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxtHint->SetLineHeight(40);
    _pTxtHint->SetText((UINT8 *)"microSD card doesn't\nhave enough space.");

    _pButton = new Button(_pPanel, Point(164, 272), Size(72, 72), Color_Black, Color_Black);
    _pButton->SetStyle(36);
    _pButton->SetBorder(true, Color_White, 4);
    snprintf(_txt, 16, "5");
    _pButton->SetText((UINT8 *)_txt, 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pButton->setOnClickListener(this);
}

HighlightFailedWnd::~HighlightFailedWnd()
{
    delete _pButtonFake;
    delete _pButton;
    delete _pTxtHint;
    delete _pTxtTitle;
    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void HighlightFailedWnd::onResume()
{
    CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Tag_Failed);
    _pPanel->Draw(true);
}

void HighlightFailedWnd::onPause()
{
}

void HighlightFailedWnd::onClick(Control *widget)
{
    if ((widget == _pButton) || (widget == _pButtonFake)) {
        _counter = 50;
        this->Close(Anim_Null);
    }
}

bool HighlightFailedWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if ((event->_event == InternalEvent_app_timer_update)) {
                if (_counter > 0) {
                    if (_counter % 10 == 0) {
                        snprintf(_txt, 16, "%d", _counter / 10);
                        _pPanel->Draw(true);
                    }
                    _counter--;
                    if (_counter <= 0) {
                        _counter = 50;
                        this->Close(Anim_Null);
                    }
                }
            }
            break;
    }

    return b;
}


OverheatWnd::OverheatWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _pTemp(NULL)
  , _origin_state(camera_State_record)
  , _counter(0)
  , _bInAWeek(false)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pBG = new BmpImage(
        _pPanel,
        Point(0, 0), Size(400, 400),
        "/usr/local/share/ui/BMP/misc/",
        "BG-red.bmp");

    _pIcon = new BmpImage(
        _pPanel,
        Point(100, 56), Size(200, 200),
        "/usr/local/share/ui/BMP/misc/",
        "icn-overheat.bmp");

    _pTxt = new StaticText(_pPanel, Point(100, 224), Size(200, 50));
    _pTxt->SetStyle(32, FONT_ROBOTOMONO_Bold, Color_White, CENTER, MIDDLE);
    _pTxt->SetText((UINT8*)"Overheat");

    snprintf(_txtTemp, sizeof(_txtTemp), " ");
    _pTemp = new StaticText(_pPanel, Point(120, 270), Size(200, 120));
    _pTemp->SetStyle(20, FONT_ROBOTOMONO_Regular, Color_White, LEFT, MIDDLE);
    _pTemp->SetText((UINT8*)_txtTemp);
}

OverheatWnd::~OverheatWnd()
{
    delete _pTemp;
    delete _pTxt;
    delete _pIcon;
    delete _pBG;
    delete _pPanel;
}

bool OverheatWnd::_isInAWeek()
{
    bool ret = false;
    const char *prop_calib_time = "calib.finalize.utc_time";
    char calib_time[256] = {0x00};
    agcmd_property_get(prop_calib_time, calib_time, "");
    if (strlen(calib_time) > 0) {
        tm tm_calib;
        time_t t_calib;
        char *pos = strptime(calib_time, "UTC %Y/%m/%d %H:%M:%S", &tm_calib);
        if (pos) {
            tm_calib.tm_isdst = -1;
            // change to seconds:
            t_calib  = mktime(&tm_calib);

            time_t t_now = time(NULL);
            int diff = t_now - t_calib;
            if (diff <= 3600 * 24 * 7) {
                // if time is bigger than a week until calib, it is definitely not in factory
                ret = true;
            }
        }
    }

    return ret;
}

void OverheatWnd::_updateStatus()
{
    int len = 0;
    char tmp[256] = {0x00};
    agcmd_property_get(PropName_Board_Temprature, tmp, "450");
    int board_temprature = atoi(tmp);
    len += snprintf(_txtTemp, sizeof(_txtTemp), "Board  %d.%d C",
        board_temprature / 10, board_temprature % 10);

    struct agsys_iio_temp_info_s *temp = agsys_iio_temp_open_by_name("bmp280");
    if (temp) {
        struct agsys_iio_temp_s temp_struct;
        memset(&temp_struct, 0x00, sizeof(temp_struct));
        int ret = agsys_iio_temp_read(temp, &temp_struct);
        if (!ret) {
            float sensor_temp = temp_struct.temperature * temp->temp_scale;
            len += snprintf(_txtTemp + len, sizeof(_txtTemp) - len,
                "\nSensor %.1f C", sensor_temp);
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
            len += snprintf(_txtTemp + len, sizeof(_txtTemp) - len,
                "\nWifi   %.1f C", wifi_temp);
        }
        agsys_iio_temp_close(temp);
    }
}

void OverheatWnd::onPause()
{
}

void OverheatWnd::onResume()
{
    _bInAWeek = _isInAWeek();
    if (_bInAWeek) {
        CAMERALOG("It is in a week from calib time.");
        _updateStatus();
        _pPanel->Draw(true);
    }

    camera_State state = _cp->getRecState();
    if (camera_State_record == state) {
        // if overheat, stop recording
        _origin_state = state;
        _cp->OnRecKeyProcess();
    }
}

bool OverheatWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;

                if (_bInAWeek && (_counter % 50 == 0)) {
                    // update temperature every 5 seconds
                    _updateStatus();
                    _pPanel->Draw(true);
                }

                // check temprature every 2 minutes
                if (_counter % (20 * 60) == 0) {
                    int temp_critical = 890, temp_high_off = 910;
                    char tmp[256];
                    agcmd_property_get(PropName_Board_Temp_critical, tmp, Critical_default);
                    temp_critical = atoi(tmp);

                    agcmd_property_get(PropName_Board_Temp_high_off, tmp, High_off_default);
                    temp_high_off = atoi(tmp);

                    agcmd_property_get(PropName_Board_Temprature, tmp, "450");
                    int board_temprature = atoi(tmp);

                    if (board_temprature > temp_high_off) {
                        CAMERALOG("board_temprature = %d, temp_high_off = %d, overheat! "
                            "Call \"agsh poweroff\" to power off now!",
                            board_temprature, temp_high_off);
                        const char *tmp_args[4];
                        tmp_args[0] = "agsh";
                        tmp_args[1] = "poweroff";
                        tmp_args[2] = NULL;
                        agbox_sh(2, (const char *const *)tmp_args);
                    } else if (board_temprature < temp_critical) {
                        // recover previouse status
                        camera_State state = _cp->getRecState();
                        if ((camera_State_record == _origin_state)
                            && (_origin_state != state))
                        {
                            _cp->OnRecKeyProcess();
                        }
                        // then exit
                        this->Close(Anim_Null);
                    }
                }
            }
            break;
        default:
            break;
    }

    return b;
}


BTNoBindWnd::BTNoBindWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop,
        EBTType type)
  : inherited(name, wndSize, leftTop)
  , _type(type)
  , _cp(cp)
  , _pBTList(NULL)
  , _numBTs(0)
  , _cntAnim(0)
  , _state(State_Wait_Confirm_To_Scan)
  , _cntDelayClose(0)
  , _counter(0)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);


    _pTxt = new StaticText(_pPanel, Point(0, 76), Size(400, 80));
    _pTxt->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxt->SetLineHeight(40);
    if (_type == BTType_OBD) {
        _pTxt->SetText((UINT8 *)"No OBD is paired.\nScan for new OBD?");
    } else if (_type == BTType_HID) {
        _pTxt->SetText((UINT8 *)"No RC is paired.\nScan for new RC?");
    }

    _pIconNo = new ImageButton(
        _pPanel,
        Point(56, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-no-up.bmp",
        "btn-dashboard-no-down.bmp");
    _pIconNo->setOnClickListener(this);

    _pIconYes = new ImageButton(
        _pPanel,
        Point(216, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-yes_alt-up.bmp",
        "btn-dashboard-yes_alt-down.bmp");
    _pIconYes->setOnClickListener(this);


    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100),
        Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);
    _pReturnBtnFake->SetHiden();

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);
    _pReturnBtn->SetHiden();

    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    if (_type == BTType_OBD) {
        _pTitle->SetText((UINT8 *)"New OBDs");
    } else if (_type == BTType_HID) {
        _pTitle->SetText((UINT8 *)"New RCs");
    }
    _pTitle->SetHiden();

    _pLine = new UILine(_pPanel, Point(100, 100), Size(200, 4));
    _pLine->setLine(Point(100, 100), Point(300, 100), 4, Color_Grey_2);
    _pLine->SetHiden();

    _pList = new List(_pPanel, this, Point(40, 110), Size(360, 170), 20);
    _pList->setListener(this);

    _pAdapter = new TextImageAdapter();
    _pAdapter->setTextList((UINT8 **)_pBTList, _numBTs);
    _pAdapter->setTextStyle(Size(360, 70), Color_Black, Color_Grey_2,
        Color_White, 24, FONT_ROBOTOMONO_Light,
        LEFT, MIDDLE);
    _pAdapter->setImageStyle(Size(32,32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu-add-up.bmp",
        "btn-menu-add-down.bmp",
        false);
    _pAdapter->setImagePadding(40);
    _pAdapter->setObserver(_pList);

    _pList->setAdaptor(_pAdapter);
    _pList->SetHiden();


    _pHelpInfo = new StaticText(_pPanel, Point(50, 80), Size(300, 180));
    _pHelpInfo->SetStyle(24, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pHelpInfo->SetLineHeight(40);
    if (_type == BTType_OBD) {
        _pHelpInfo->SetText((UINT8 *)"Please plug your OBD"
            "\n"
            "and power on your car"
            "\n"
            "before scanning.");
    } else if (_type == BTType_HID) {
        _pHelpInfo->SetText((UINT8 *)"Click the remote"
            "\n"
            "control to wake it"
            "\n"
            "up before scanning.");
    }
    _pHelpInfo->SetHiden();

    _pHelp = new Button(_pPanel, Point(90, 190), Size(220, 76), Color_Black, Color_Grey_1);
    _pHelp->SetText((UINT8 *)"Details", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pHelp->SetStyle(20);
    _pHelp->setOnClickListener(this);
    _pHelp->SetHiden();

    _pReScan = new ImageButton(
        _pPanel,
        Point(136, 268), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu-scan-up.bmp",
        "btn-menu-scan-down.bmp");
    _pReScan->setOnClickListener(this);
    _pReScan->SetHiden();


    _pInfo = new StaticText(_pPanel, Point(0, 110), Size(400, 80));
    _pInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pInfo->SetLineHeight(40);
    if (_type == BTType_OBD) {
        _pInfo->SetText((UINT8 *)"Scanning OBD ...");
    } else if (_type == BTType_HID) {
        _pInfo->SetText((UINT8 *)"Scanning RC ...");
    }
    _pInfo->SetHiden();

    _pAnimation = new BmpImage(
        _pPanel,
        Point(136, 200), Size(128, 128),
        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
        "power_OFF-spinner_00000.bmp");
    _pAnimation->SetHiden();

    _pBindResult = new BmpImage(
        _pPanel,
        Point(160, 80), Size(80, 80),
        "/usr/local/share/ui/BMP/dash_menu/",
        "feedback-good.bmp");
    _pBindResult->SetHiden();
}

BTNoBindWnd::~BTNoBindWnd()
{
    delete _pBindResult;
    delete _pAnimation;
    delete _pInfo;

    delete _pReScan;
    delete _pHelp;
    delete _pHelpInfo;

    delete _pList;
    delete _pAdapter;
    delete _pLine;
    delete _pTitle;

    delete _pReturnBtn;
    delete _pReturnBtnFake;

    delete _pIconYes;
    delete _pIconNo;
    delete _pTxt;

    delete _pPanel;

    for (int i = 0; i < _numBTs; i++) {
        delete[] _pBTList[i];
    }
    delete [] _pBTList;
}

bool BTNoBindWnd::isOBDBinded()
{
    char obd_mac[256];
    agcmd_property_get(propOBDMac, obd_mac, "NA");
    if (strcasecmp(obd_mac, "NA") == 0)
    {
        agcmd_property_get(Prop_OBD_MAC_Backup, obd_mac, "NA");
    }

    if (strcasecmp(obd_mac, "NA") == 0)
    {
        return false;
    }

    return true;
}

bool BTNoBindWnd::isHIDBinded()
{
    char hid_mac[256];
    agcmd_property_get(propHIDMac, hid_mac, "NA");
    if (strcasecmp(hid_mac, "NA") == 0)
    {
        agcmd_property_get(Prop_HID_MAC_Backup, hid_mac, "NA");
    }

    if (strcasecmp(hid_mac, "NA") == 0)
    {
        return false;
    }

    return true;
}

void BTNoBindWnd::parseBT(char *json, int type)
{
    int added = 0;
    int index = 0;
    char **list = NULL;
    const char *keywords = NULL;
    Document d;

    if (!json) {
        goto parse_error;
    }

    d.Parse(json);
    if(!d.IsObject()) {
        CAMERALOG("Didn't get a JSON object!");
        goto parse_error;
    }

    if (type == BTType_OBD) {
        keywords = "OBD-TW";
    } else if (type == BTType_HID) {
        keywords = "RC-TW";
    } else {
        return;
    }

    if (d.HasMember("Devices")) {
        const Value &a = d["Devices"];
        if (a.IsArray()) {
            for (SizeType i = 0; i < a.Size(); i++) {
                const Value &item = a[i];
                if (strstr(item["name"].GetString(), keywords)) {
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
                if (strstr(item["name"].GetString(), keywords)) {
                    const char *name = item["name"].GetString();
                    const char *mac = item["mac"].GetString();
                    list[index] = new char[strlen(name) + strlen(mac) + 5];
                    if (!list[index]) {
                        CAMERALOG("Out of memory!");
                        goto parse_error;
                    }
                    snprintf(list[index], strlen(name) + strlen(mac) + 5, "%s\n[%s]", name, mac);
                    index++;
                }
            }
        }
    }

parse_success:
    // Firstly, free the list which is out of date
    for (int i = 0; i < _numBTs; i++) {
        delete [] _pBTList[i];
    }
    delete [] _pBTList;
    _numBTs = 0;

    // Then, use the new list
    _numBTs = added;
    _pBTList = list;
    //for (int i = 0; i < _numBTs; i++) {
    //    CAMERALOG("Saved connections: %dth %s", i, _pBTList[i]);
    //}
    _pAdapter->setTextList((UINT8 **)_pBTList, _numBTs);
    _pAdapter->notifyDataSetChanged();

    return;

parse_error:
    for (int i = 0; i < _numBTs; i++) {
        delete [] _pBTList[i];
    }
    delete [] _pBTList;
    _numBTs = 0;
    _pBTList = NULL;

    for (int i = 0; i < index; i++) {
        delete [] list[i];
    }
    delete [] list;
}

void BTNoBindWnd::onClick(Control *widget)
{
    if (widget == _pIconNo) {
        if (State_Wait_Confirm_To_Scan == _state) {
            _state = State_Wait_Confirm_To_Exit;
            if (_type == BTType_OBD) {
                _pTxt->SetText((UINT8 *)"No OBD is paired.\nSure to exit?");
            } else if (_type == BTType_HID) {
                _pTxt->SetText((UINT8 *)"No RC is paired.\nSure to exit?");
            }
            _pIconYes->SetRelativePos(Point(56, 216));
            _pIconNo->SetRelativePos(Point(216, 216));
            _pPanel->Draw(true);
        } else if (State_Wait_Confirm_To_Exit == _state) {
            _state = State_Wait_Confirm_To_Scan;
            if (_type == BTType_OBD) {
                _pTxt->SetText((UINT8 *)"No OBD is paired.\nScan for new OBD?");
            } else if (_type == BTType_HID) {
                _pTxt->SetText((UINT8 *)"No RC is paired.\nScan for new RC?");
            }
            _pIconYes->SetRelativePos(Point(216, 216));
            _pIconNo->SetRelativePos(Point(56, 216));
            _pPanel->Draw(true);
        }
    } else if (widget == _pIconYes) {
        if (State_Wait_Confirm_To_Scan == _state) {
            _pInfo->SetShow();
            _pAnimation->SetShow();
            _cntAnim = 0;
            _pTxt->SetHiden();
            _pIconNo->SetHiden();
            _pIconYes->SetHiden();
            _pPanel->Draw(true);
            // Scan BT devices
            _counter = 0;
            _state = State_Scanning;
            _cp->ScanBTDevices();
        } else if (State_Wait_Confirm_To_Exit == _state) {
            this->Close(Anim_Null);
        }
    } else if ((widget == _pReturnBtn) || (widget == _pReturnBtnFake)) {
        this->Close(Anim_Null);
    } else if (widget == _pReScan) {
        if (_type == BTType_OBD) {
            _pInfo->SetText((UINT8 *)"Scanning OBD ...");
        } else if (_type == BTType_HID) {
            _pInfo->SetText((UINT8 *)"Scanning RC ...");
        }
        _pInfo->SetShow();
        _pAnimation->SetShow();
        _cntAnim = 0;
        _pTxt->SetHiden();
        _pIconNo->SetHiden();
        _pIconYes->SetHiden();
        _pReturnBtn->SetHiden();
        _pReturnBtnFake->SetHiden();
        _pTitle->SetHiden();
        _pLine->SetHiden();
        _pList->SetHiden();
        _pReScan->SetHiden();
        _pHelp->SetHiden();
        _pHelpInfo->SetHiden();
        _pPanel->Draw(true);
        // Scan BT devices
        _counter = 0;
        _state = State_Scanning;
        _cp->ScanBTDevices();
    } else if (widget == _pHelp) {
        _pTitle->SetHiden();
        _pLine->SetHiden();
        _pInfo->SetHiden();
        _pHelp->SetHiden();
        _pHelpInfo->SetShow();
        _pPanel->Draw(true);
    }
}

void BTNoBindWnd::onListClicked(Control *list, int index)
{
    if (_pList == list) {
        if ((index >= 0) && (index < _numBTs)) {
            char mac[64];
            memset(mac, 0x00, 64);
            char *tmp1 = strstr(_pBTList[index], "[");
            char *tmp2 = strstr(_pBTList[index], "]");
            if (tmp1 && tmp2 && (tmp2 - tmp1 > 1)) {
                if (_type == BTType_OBD) {
                    _pInfo->SetText((UINT8 *)"Pairing OBD ...");
                } else if (_type == BTType_HID) {
                    _pInfo->SetText((UINT8 *)"Pairing RC ...");
                }
                _pInfo->SetShow();
                _pAnimation->SetShow();
                _cntAnim = 0;
                _pReturnBtn->SetHiden();
                _pReturnBtnFake->SetHiden();
                _pTitle->SetHiden();
                _pLine->SetHiden();
                _pList->SetHiden();
                _pHelp->SetHiden();
                _pReScan->SetHiden();
                _pPanel->Draw(true);

                strncpy(mac, tmp1 + 1, tmp2 - tmp1 - 1);
                mac[tmp2 - tmp1 - 1] = 0x00;
                CAMERALOG("To bind device %s, strlen(mac) = %d", mac, strlen(mac));
                if (_type == BTType_OBD) {
                    _state = State_Wait_To_Bind_OBD;
                } else if (_type == BTType_HID) {
                    _state = State_Wait_To_Bind_HID;
                }

                char mac_insys[AGCMD_PROPERTY_SIZE];
                if (_type == BTType_OBD) {
                    agcmd_property_get(propOBDMac, mac_insys, "NA");
                    if (strcasecmp(mac_insys, "NA") == 0) {
                        agcmd_property_get(Prop_OBD_MAC_Backup, mac_insys, "NA");
                    }
                } else if (_type == BTType_HID) {
                    agcmd_property_get(propHIDMac, mac_insys, "NA");
                    if (strcasecmp(mac_insys, "NA") == 0) {
                        agcmd_property_get(Prop_HID_MAC_Backup, mac_insys, "NA");
                    }
                }
                if (strcasecmp(mac_insys, "NA") != 0) {
                    if (strcasecmp(mac_insys, mac) == 0) {
                        _pBindResult->setSource("/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-good.bmp");
                        _pInfo->SetText((UINT8 *)"Already paired");
                    } else {
                        _pBindResult->setSource("/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-bad.bmp");
                        _pInfo->SetText((UINT8 *)"Another device paired");
                    }
                    _pBindResult->SetShow();
                    _pInfo->SetRelativePos(Point(0, 180));
                    _pAnimation->SetHiden();
                    _pPanel->Draw(true);
                    _counter = 0;
                    _cntDelayClose = 20;
                } else {
                    if (_cp->BindBTDevice(mac, _type) != 0) {
                        _pBindResult->setSource("/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-bad.bmp");
                        _pBindResult->SetShow();
                        _pInfo->SetText((UINT8 *)"Pairing Failed");
                        _pInfo->SetRelativePos(Point(0, 180));
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _counter = 0;
                        _cntDelayClose = 20;
                    }
                }
            }
        }
    }
}

void BTNoBindWnd::onResume()
{
    bluetooth_state bt = _cp->getBluetoothState();
    if (bt == bt_state_off) {
        this->Close(Anim_Null);
    }
    if (((bt == bt_state_obd_paired) || (bt == bt_state_hid_obd_paired))
        && (_type == BTType_OBD))
    {
        this->Close(Anim_Null);
    }
    if (((bt == bt_state_hid_paired) || (bt == bt_state_hid_obd_paired))
        && (_type == BTType_HID))
    {
        this->Close(Anim_Null);
    }
}

void BTNoBindWnd::onPause()
{
}

bool BTNoBindWnd::OnEvent(CEvent *event)
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
                    this->Close(Anim_Null);
                    b = false;
                }
            }
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_cntDelayClose > 0) {
                    _cntDelayClose --;
                    if (_cntDelayClose <= 0) {
                        this->Close(Anim_Null);
                    }
                    break;
                }

                if (_state == State_Scanning) {
                    _counter ++;
                    if (_counter >= 300 /* 30s */) {
                        // Scan time out!
                        _counter = 0;
                        parseBT(NULL, _type);
                        _state = State_Got_List;
                        _pTitle->SetShow();
                        _pLine->SetShow();
                        _pList->SetShow();
                        if (_numBTs <= 0) {
                            _pInfo->SetText((UINT8 *)"Scan time out.");
                            _pInfo->SetRelativePos(Point(0, 120));
                            _pInfo->SetShow();
                            _pReScan->SetShow();
                            _pHelp->SetShow();
                        }
                        _pTxt->SetHiden();
                        _pIconNo->SetHiden();
                        _pIconYes->SetHiden();
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _state = State_Scan_Timeout;
                    } else {
                        char source[64];
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _cntAnim++ % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        _pPanel->Draw(true);
                    }
                } else if ((_state == State_Wait_To_Bind_HID)
                    || (_state == State_Wait_To_Bind_OBD))
                {
                    _counter ++;
                    if (_counter >= 200 /* 20s */) {
                        _pBindResult->setSource("/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-bad.bmp");
                        _pBindResult->SetShow();
                        _pInfo->SetText((UINT8 *)"Pairing Timeout!");
                        _pInfo->SetRelativePos(Point(0, 180));
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                        _counter = 0;
                        _cntDelayClose = 20;
                        _state = State_Bind_Timeout;
                    } else {
                        char source[64];
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _cntAnim++ % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        _pPanel->Draw(true);
                    }
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    bluetooth_state bt = _cp->getBluetoothState();
                    if (bt == bt_state_off) {
                        this->Close(Anim_Null);
                        b = false;
                    }
                    // if the dialogue is active, but APP bind a device,
                    // should exit the dialogue
                    if (((bt == bt_state_obd_paired) || (bt == bt_state_hid_obd_paired))
                        && (_type == BTType_OBD) && (_cntDelayClose <= 0))
                    {
                        this->Close(Anim_Null);
                        b = false;
                    }
                    if (((bt == bt_state_hid_paired) || (bt == bt_state_hid_obd_paired))
                        && (_type == BTType_HID) && (_cntDelayClose <= 0))
                    {
                        this->Close(Anim_Null);
                        b = false;
                    }
                }
            } else if (event->_event ==InternalEvent_app_state_update) {
                if (event->_paload == camera_State_scanBT_result) {
                    if ((_state == State_Scanning)|| (_state == State_Scan_Timeout))
                    {
                        char *str = (char *)event->_paload1;
                        parseBT(str, _type);
                        _state = State_Got_List;
                        _pReturnBtn->SetShow();
                        _pReturnBtnFake->SetShow();
                        _pTitle->SetShow();
                        _pLine->SetShow();
                        //CAMERALOG("str = %s, _numBTs = %d", str, _numBTs);
                        if (_numBTs <= 0) {
                            _pInfo->SetText((UINT8 *)"No new device\nwas found.");
                            _pInfo->SetRelativePos(Point(0, 120));
                            _pInfo->SetShow();
                            _pReScan->SetShow();
                            _pHelp->SetShow();
                        } else {
                            _pInfo->SetHiden();
                            _pList->SetShow();
                        }
                        _pReScan->SetShow();
                        _pTxt->SetHiden();
                        _pIconNo->SetHiden();
                        _pIconYes->SetHiden();
                        _pAnimation->SetHiden();
                        _pPanel->Draw(true);
                    }
                    b = false;
                } else if (event->_paload == camera_State_bindBT_result) {
                    int value = event->_paload1;
                    int type = (value >> 16) & 0xFFFF;
                    int result = value & 0xFFFF;
                    if ((type == _type) &&
                        ((_state == State_Wait_To_Bind_OBD) || (_state == State_Wait_To_Bind_HID)))
                    {
                        if (result == BTErr_OK) {
                            _pBindResult->setSource("/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-good.bmp");
                            _pBindResult->SetShow();
                            _pInfo->SetText((UINT8 *)"Pairing Successful");
                            _pInfo->SetRelativePos(Point(0, 180));
                            _pAnimation->SetHiden();
                            _pPanel->Draw(true);
                            _cntDelayClose = 20;
                        } else {
                            CAMERALOG("Bind failed");
                            _pBindResult->setSource("/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-bad.bmp");
                            _pBindResult->SetShow();
                            _pInfo->SetText((UINT8 *)"Pairing Failed");
                            _pInfo->SetRelativePos(Point(0, 180));
                            _pAnimation->SetHiden();
                            _pPanel->Draw(true);
                            _cntDelayClose = 20;
                        }
                    }
                    b = false;
                }
            }
            break;
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
    }

    return b;
}


OBDNotBindedWnd::OBDNotBindedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTNoBindWnd(cp, name, wndSize, leftTop, BTNoBindWnd::BTType_OBD)
{
}

OBDNotBindedWnd::~OBDNotBindedWnd()
{
}


HIDNotBindedWnd::HIDNotBindedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTNoBindWnd(cp, name, wndSize, leftTop, BTNoBindWnd::BTType_HID)
{
}

HIDNotBindedWnd::~HIDNotBindedWnd()
{
}


WrongWifiPwdWnd::WrongWifiPwdWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pOKFake = new Button(_pPanel, Point(50, 200), Size(300, 200), Color_Black, Color_Black);
    _pOKFake->SetStyle(0, 0.0);
    _pOKFake->setOnClickListener(this);

    _pRing = new FanRing(_pPanel, Point(200, 200),
        199, 168,
        Color_Yellow_2, Color_Black);
    _pRing->setAngles(0.0, 360.0);

    _pIcon = new BmpImage(
        _pPanel,
        Point(152, 64), Size(96, 96),
        "/usr/local/share/ui/BMP/misc/",
        "icn-wifi-notification-OFF.bmp");

    _pTxtHint = new StaticText(_pPanel, Point(45, 160), Size(310, 120));
    _pTxtHint->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxtHint->SetLineHeight(40);
    snprintf(_txtHint, sizeof(_txtHint), "Wrong password");
    _pTxtHint->SetText((UINT8*)_txtHint);

    _pOK = new Button(_pPanel, Point(150, 300), Size(100, 60), Color_Black, Color_Grey_1);
    _pOK->SetText((UINT8 *)"OK", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pOK->setOnClickListener(this);
}

WrongWifiPwdWnd::~WrongWifiPwdWnd()
{
    delete _pOKFake;
    delete _pOK;
    delete _pTxtHint;
    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void WrongWifiPwdWnd::onPause()
{
}

void WrongWifiPwdWnd::onResume()
{
    char ssid[32];
    _cp->GetWifiCltInfor(ssid, 32);
    if (strcmp(ssid, "Empty List") == 0) {
        snprintf(_txtHint, sizeof(_txtHint), "Looks like there's\nno saved Wi-Fi\navailable.");
    } else if (strcmp(ssid, "Searching") == 0) {
        snprintf(_txtHint, sizeof(_txtHint), "Wrong password or\nsignal weak!");
    } else {
        snprintf(_txtHint, sizeof(_txtHint), "Wrong password or\nsignal weak for\n%s", ssid);
    }
    _pPanel->Draw(true);

    CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Alert);
}

void WrongWifiPwdWnd::onClick(Control *widget)
{
    if ((_pOK == widget) || (_pOKFake == widget)) {
        this->Close(Anim_Top2BottomExit);
    }
}

bool WrongWifiPwdWnd::OnEvent(CEvent *event)
{
    bool b = true;
    return b;
}

WifiAddSuccessWnd::WifiAddSuccessWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _counter(20)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pRing = new FanRing(_pPanel, Point(200, 200),
        199, 168,
        Color_Green_1, Color_Black);
    _pRing->setAngles(0.0, 360.0);

    _pIcon = new BmpImage(
        _pPanel,
        Point(152, 64), Size(96, 96),
        "/usr/local/share/ui/BMP/misc/",
        "icn-wifi-notification-ON.bmp");

    _pTxtHint = new StaticText(_pPanel, Point(45, 160), Size(310, 120));
    _pTxtHint->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxtHint->SetLineHeight(40);
    snprintf(_txtHint, sizeof(_txtHint), "Connected");
    _pTxtHint->SetText((UINT8*)_txtHint);
}

WifiAddSuccessWnd::~WifiAddSuccessWnd()
{
    delete _pTxtHint;
    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void WifiAddSuccessWnd::onPause()
{
}

void WifiAddSuccessWnd::onResume()
{
    char ssid[32];
    bool ret = _cp->GetWifiCltInfor(ssid, 32);
    if (ret && (strcmp(ssid, "Empty List") != 0) &&
        (strcmp(ssid, "Searching") != 0) &&
        (strcmp(ssid, "Connecting") != 0))
    {
        snprintf(_txtHint, sizeof(_txtHint), "Connected to\n%s", ssid);
    }
    _pPanel->Draw(true);

    CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Connected);
}

bool WifiAddSuccessWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if ((event->_event == InternalEvent_app_timer_update)) {
                if (_counter > 0) {
                    _counter--;
                    if (_counter <= 0) {
                        _counter = 50;
                        this->Close(Anim_Null);
                    }
                }
            }
            break;
    }

    return b;
}


LoopSpaceWarningWnd::LoopSpaceWarningWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _pScroller(NULL)
  , _pReturnBtnFake(NULL)
  , _pReturnBtn(NULL)
  , _pTitle(NULL)
  , _pLine(NULL)
  , _pGuide(NULL)
  , _pClose(NULL)
  , _counter(0)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    _pRing = new FanRing(_pPanel, Point(200, 200),
        199, 168,
        Color_Yellow_2, Color_Black);
    _pRing->setAngles(0.0, 360.0);

    _pIcon = new BmpImage(
        _pPanel,
        Point(176, 56), Size(48, 48),
        "/usr/local/share/ui/BMP/misc/",
        "icn-TF-full.bmp");

    _pTxtHint = new StaticText(_pPanel, Point(50, 128), Size(300, 80));
    _pTxtHint->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxtHint->SetLineHeight(40);
    _pTxtHint->SetText((UINT8*)"Loop recording\nspace is small.");

    _pBtnMore = new Button(_pPanel, Point(50, 208), Size(300, 80), Color_Black, Color_Grey_1);
    _pBtnMore->SetStyle(0, 0.0);
    _pBtnMore->SetText((UINT8*)"More Info", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pBtnMore->SetTextAlign(CENTER, MIDDLE, 0);
    _pBtnMore->setOnClickListener(this);

    _pOK = new Button(_pPanel, Point(50, 288), Size(300, 70), Color_Black, Color_Grey_1);
    _pOK->SetText((UINT8 *)"OK", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pOK->SetTextAlign(CENTER, MIDDLE, 0);
    _pOK->SetStyle(0, 0.0);
    _pOK->setOnClickListener(this);
}

LoopSpaceWarningWnd::~LoopSpaceWarningWnd()
{
    delete _pClose;
    delete _pGuide;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pScroller;

    delete _pOK;
    delete _pBtnMore;
    delete _pTxtHint;

    delete _pIcon;
    delete _pRing;
    delete _pPanel;
}

void LoopSpaceWarningWnd::showDialogue()
{
    _pRing->SetHiden();
    _pIcon->SetHiden();
    _pTxtHint->SetHiden();
    _pBtnMore->SetHiden();
    _pOK->SetHiden();

    _visibleSizeScroller.width = 320;
    _visibleSizeScroller.height = 400;
    _pScroller = new Scroller(
        _pPanel,
        this,
        Point(40, 0),
        _visibleSizeScroller,
        Size(_visibleSizeScroller.width, 740),
        20);

    // All position are relative to scroller:
    _pReturnBtnFake = new Button(
        _pScroller->getViewRoot(),
        Point(60, 0), Size(200, 88), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pScroller->getViewRoot(),
        Point(136, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");

    _pTitle = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 60),
        Size(_visibleSizeScroller.width, 80));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Loop-Recording\nSpace");

    _pLine = new UILine(
        _pScroller->getViewRoot(),
        Point(90, 146),
        Size(140, 4));
    _pLine->setLine(Point(90, 150), Point(230, 150), 4, Color_Grey_2);

    snprintf(_txt, 256,
        "Remaining storage space for"
        "\n"
        "loop recording is small,"
        "\n"
        "which makes your MicroSD"
        "\n"
        "card more prone to failure."
        "\n"
        "\n"
        "Please back up highlights of"
        "\n"
        "your interests then free up"
        "\n"
        "space by removing them."
        "\n"
        "\n"
        "For more details, please visit"
        "\n"
        "www.waylens.com");
    _pGuide = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 180),
        Size(_visibleSizeScroller.width, 500));
    _pGuide->SetStyle(24, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, TOP);
    _pGuide->SetLineHeight(40);
    _pGuide->SetText((UINT8 *)_txt);

    _pClose = new Button(
        _pScroller->getViewRoot(),
        Point(0, 630),
        Size(_visibleSizeScroller.width, 80),
        Color_Black, Color_Grey_2);
    _pClose->SetText((UINT8 *)"OK, I know", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pClose->SetTextAlign(CENTER, MIDDLE);
    _pClose->setOnClickListener(this);
}

void LoopSpaceWarningWnd::checkSpaceWarning()
{
    // check whether there is enough space for loop recording:
    bool bSpaceWarning = false;
    storage_State st = _cp->getStorageState();
    if (storage_State_full == st) {
        bSpaceWarning = true;
    } else if (storage_State_ready == st) {
        int totle = 0, marked = 0, loop = 0;
        _cp->getSpaceInfoMB(totle, marked, loop);
        if ((totle > 10 * 1024) && (loop <= 8 * 1024)) {
            bSpaceWarning = true;
        } else if ((totle > 0) && (totle < 10 * 1024) && (loop <= 6 * 1024)) {
            bSpaceWarning = true;
        }
    }

    if (!bSpaceWarning) {
        this->Close(Anim_Top2BottomExit);
    }
}

void LoopSpaceWarningWnd::onPause()
{
}

void LoopSpaceWarningWnd::onResume()
{
    CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Alert);
}

void LoopSpaceWarningWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) || (widget == _pReturnBtnFake) ||
        (_pOK == widget) ||
        (_pClose == widget))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (_pBtnMore == widget) {
        showDialogue();
        _pPanel->Draw(true);
    }
}

bool LoopSpaceWarningWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if ((++_counter) % 100 == 0) {
                    checkSpaceWarning();
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    storage_State st = _cp->getStorageState();
                    if (storage_State_noMedia == st) {
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                }
            }
            break;
        default:
            break;
    }

    return b;
}


LoopSpaceWarningDetailWnd::LoopSpaceWarningDetailWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _pScroller(NULL)
  , _pReturnBtnFake(NULL)
  , _pReturnBtn(NULL)
  , _pTitle(NULL)
  , _pLine(NULL)
  , _pGuide(NULL)
  , _pClose(NULL)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 7);
    this->SetMainObject(_pPanel);

    showDialogue();
}

LoopSpaceWarningDetailWnd::~LoopSpaceWarningDetailWnd()
{
    delete _pClose;
    delete _pGuide;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pScroller;
    delete _pPanel;
}

void LoopSpaceWarningDetailWnd::showDialogue()
{
    _visibleSizeScroller.width = 320;
    _visibleSizeScroller.height = 400;
    _pScroller = new Scroller(
        _pPanel,
        this,
        Point(40, 0),
        _visibleSizeScroller,
        Size(_visibleSizeScroller.width, 740),
        20);

    // All position are relative to scroller:
    _pReturnBtnFake = new Button(
        _pScroller->getViewRoot(),
        Point(60, 0), Size(200, 88), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pScroller->getViewRoot(),
        Point(136, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");

    _pTitle = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 60),
        Size(_visibleSizeScroller.width, 80));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Loop-Recording\nSpace");

    _pLine = new UILine(
        _pScroller->getViewRoot(),
        Point(90, 146),
        Size(140, 4));
    _pLine->setLine(Point(90, 150), Point(230, 150), 4, Color_Grey_2);

    snprintf(_txt, 256,
        "Remaining storage space for"
        "\n"
        "loop recording is small,"
        "\n"
        "which makes your MicroSD"
        "\n"
        "card more prone to failure."
        "\n"
        "\n"
        "Please back up highlights of"
        "\n"
        "your interests then free up"
        "\n"
        "space by removing them."
        "\n"
        "\n"
        "For more details, please visit"
        "\n"
        "www.waylens.com");
    _pGuide = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 180),
        Size(_visibleSizeScroller.width, 500));
    _pGuide->SetStyle(24, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, TOP);
    _pGuide->SetLineHeight(40);
    _pGuide->SetText((UINT8 *)_txt);

    _pClose = new Button(
        _pScroller->getViewRoot(),
        Point(0, 630),
        Size(_visibleSizeScroller.width, 80),
        Color_Black, Color_Grey_2);
    _pClose->SetText((UINT8 *)"OK, I know", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pClose->SetTextAlign(CENTER, MIDDLE);
    _pClose->setOnClickListener(this);
}

void LoopSpaceWarningDetailWnd::onPause()
{
}

void LoopSpaceWarningDetailWnd::onResume()
{
}

void LoopSpaceWarningDetailWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) || (widget == _pReturnBtnFake) ||
        (_pClose == widget))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

bool LoopSpaceWarningDetailWnd::OnEvent(CEvent *event)
{
    bool b = true;

    return b;
}

};
