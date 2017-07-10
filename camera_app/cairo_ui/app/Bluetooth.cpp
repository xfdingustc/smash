
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

#include "Bluetooth.h"

extern bool is_no_write_record();

using namespace rapidjson;

namespace ui{

BluetoothMenuWnd::BluetoothMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _cntTimeout(0)
    , _targetMode(bt_state_on)
    , _counter(0)
    , _bConnectingOBD(false)
    , _bConnectingRC(false)
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


    _pStatusBtnFake = new Button(_pPanel, Point(30, 80), Size(340, 80), Color_Black, Color_Grey_1);
    _pStatusBtnFake->SetStyle(20);
    _pStatusBtnFake->setOnClickListener(this);

    _pStatus = new StaticText(_pPanel, Point(40, 100), Size(200, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"Bluetooth");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(304, 104), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");


    _pHIDBtnFake = new Button(_pPanel, Point(30, 160), Size(340, 80), Color_Black, Color_Grey_1);
    _pHIDBtnFake->SetStyle(20);
    _pHIDBtnFake->setOnClickListener(this);

    _pHIDTitle = new StaticText(_pPanel, Point(40, 180), Size(200, 40));
    _pHIDTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pHIDTitle->SetText((UINT8 *)"Remote Ctr.");

    _pHIDIcon = new BmpImage(
        _pPanel,
        Point(344, 192), Size(16, 16),
        "/usr/local/share/ui/BMP/dash_menu/",
        "status-disconnected.bmp");


    _pOBDBtnFake = new Button(_pPanel, Point(30, 240), Size(340, 80), Color_Black, Color_Grey_1);
    _pOBDBtnFake->SetStyle(20);
    _pOBDBtnFake->setOnClickListener(this);

    _pOBDTitle = new StaticText(_pPanel, Point(40, 260), Size(200, 40));
    _pOBDTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pOBDTitle->SetText((UINT8 *)"OBD Trsmt.");

    _pOBDIcon = new BmpImage(
        _pPanel,
        Point(344, 272), Size(16, 16),
        "/usr/local/share/ui/BMP/dash_menu/",
        "status-disconnected.bmp");

    _pAnimation = new BmpImage(
        _pPanel,
        Point(136, 160), Size(128, 128),
        "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
        "power_OFF-spinner_00000.bmp");
    _pAnimation->SetHiden();
}

BluetoothMenuWnd::~BluetoothMenuWnd()
{
    delete _pAnimation;

    delete _pOBDIcon;
    delete _pOBDTitle;
    delete _pOBDBtnFake;

    delete _pHIDIcon;
    delete _pHIDTitle;
    delete _pHIDBtnFake;

    delete _pSwitch;
    delete _pStatus;
    delete _pStatusBtnFake;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void BluetoothMenuWnd::updateBluetooth()
{
    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (((bt_state_on == _targetMode) && (strcasecmp(tmp, "on") == 0))
        || ((bt_state_off == _targetMode) && (strcasecmp(tmp, "off") == 0)))
    {
        _pStatusBtnFake->SetShow();
        _pStatus->SetShow();
        _pSwitch->SetShow();
        _pHIDBtnFake->SetShow();
        _pHIDTitle->SetShow();
        _pHIDIcon->SetShow();
        _pOBDBtnFake->SetShow();
        _pOBDTitle->SetShow();
        _pOBDIcon->SetShow();
        _pAnimation->SetHiden();
        _cntTimeout = 0;
    }

    if (strcasecmp(tmp, "on") == 0) {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");

        agcmd_property_get(propBTTempHIDStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            _pHIDIcon->setSource(
                "/usr/local/share/ui/BMP/dash_menu/",
                "status-connected.bmp");
        } else {
            _pHIDIcon->setSource(
                "/usr/local/share/ui/BMP/dash_menu/",
                "status-disconnected.bmp");
        }
        if (strcasecmp(tmp, "Connecting") == 0) {
            _bConnectingRC = true;
        } else {
            _bConnectingRC = false;
            if (_cntTimeout <= 0) {
                _pHIDIcon->SetShow();
            }
        }

        agcmd_property_get(propBTTempOBDStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            _pOBDIcon->setSource(
                "/usr/local/share/ui/BMP/dash_menu/",
                "status-connected.bmp");
        } else {
            _pOBDIcon->setSource(
                "/usr/local/share/ui/BMP/dash_menu/",
                "status-disconnected.bmp");
        }
        if (strcasecmp(tmp, "Connecting") == 0) {
            _bConnectingOBD = true;
        } else {
            _bConnectingOBD = false;
            if (_cntTimeout <= 0) {
                _pOBDIcon->SetShow();
            }
        }
    } else {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
        _pHIDIcon->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "status-disconnected.bmp");
        _pOBDIcon->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "status-disconnected.bmp");

        _bConnectingOBD = false;
        _bConnectingRC = false;
        if (_cntTimeout <= 0) {
            _pHIDIcon->SetShow();
        }
        if (_cntTimeout <= 0) {
            _pOBDIcon->SetShow();
        }
    }
}

void BluetoothMenuWnd::onMenuResume()
{
    _counter = 0;

    updateBluetooth();
    _pPanel->Draw(true);
}

void BluetoothMenuWnd::onMenuPause()
{
}

void BluetoothMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pStatusBtnFake) {
        char tmp[256];
        agcmd_property_get(propBTTempStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            _targetMode = bt_state_off;
        } else {
            _targetMode = bt_state_on;
        }

        _cntTimeout = 100;
        _pStatusBtnFake->SetHiden();
        _pStatus->SetHiden();
        _pSwitch->SetHiden();
        _pHIDBtnFake->SetHiden();
        _pHIDTitle->SetHiden();
        _pHIDIcon->SetHiden();
        _pOBDBtnFake->SetHiden();
        _pOBDTitle->SetHiden();
        _pOBDIcon->SetHiden();
        _pAnimation->SetShow();
        _pPanel->Draw(true);

        _cp->EnableBluetooth((bt_state_on == _targetMode) ? true : false);
    } else if (widget == _pHIDBtnFake) {
        this->StartWnd(WINDOW_hid_detail, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pOBDBtnFake) {
        this->StartWnd(WINDOW_obd_detail, Anim_Bottom2TopCoverEnter);
    }
}

bool BluetoothMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_cntTimeout > 0) {
                    char tmp[256];
                    agcmd_property_get(propBTTempStatus, tmp, "");
                    if (((bt_state_on == _targetMode) && (strcasecmp(tmp, "on") == 0))
                        || ((bt_state_off == _targetMode) && (strcasecmp(tmp, "off") == 0)))
                    {
                        updateBluetooth();
                        _pPanel->Draw(true);
                        _cntTimeout = 0;
                    } else {
                        char source[64];
                        snprintf(source, 64, "power_OFF-spinner_%05d.bmp", _cntTimeout % 10);
                        //CAMERALOG("source = %s", source);
                        _pAnimation->setSource(
                            "/usr/local/share/ui/BMP/animation/power_OFF-spinner/",
                            source);
                        _pPanel->Draw(true);
                        _cntTimeout--;
                        if (_cntTimeout <= 0) {
                            _pStatusBtnFake->SetShow();
                            _pStatus->SetShow();
                            _pSwitch->SetShow();
                            _pHIDBtnFake->SetShow();
                            _pHIDTitle->SetShow();
                            _pHIDIcon->SetShow();
                            _pOBDBtnFake->SetShow();
                            _pOBDTitle->SetShow();
                            _pOBDIcon->SetShow();
                            _pAnimation->SetHiden();
                            updateBluetooth();
                            _pPanel->Draw(true);
                            _cntTimeout = 0;
                        }
                    }
                    b = false;
                } else {
                    _counter++;
                    bool bReDraw = false;
                    if (_bConnectingOBD) {
                        // blink OBD icon when connecting
                        if (_counter % 10 == 1) {
                            _pOBDIcon->SetShow();
                            bReDraw = true;
                        } else if (_counter % 10 == 6) {
                            _pOBDIcon->SetHiden();
                            bReDraw = true;
                        }
                    } else {
                        _pOBDIcon->SetShow();
                    }

                    if (_bConnectingRC) {
                        // blink RC icon when connecting
                        if (_counter % 10 == 1) {
                            _pHIDIcon->SetShow();
                            bReDraw = true;
                        } else if (_counter % 10 == 6) {
                            _pHIDIcon->SetHiden();
                            bReDraw = true;
                        }
                    } else {
                        _pHIDIcon->SetShow();
                    }

                    if (bReDraw) {
                        _pPanel->Draw(true);
                    }
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    updateBluetooth();
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


BTDetailWnd::BTDetailWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop,
        EBTType type)
  : inherited(cp, name, wndSize, leftTop)
  , _type(type)
  , _pBTList(NULL)
  , _numBTs(0)
  , _cntAnim(0)
  , _state(State_Detail_Show_Info)
  , _cntDelayClose(0)
  , _counter(0)
  , _bConnecting(false)
{
    _pPanel = new Panel(NULL, this, Point(0,0), Size(400, 400), 20);
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

    if (_type == BTType_OBD) {
        _pIconBT = new BmpImage(
            _pPanel,
            Point(160, 80), Size(80, 80),
            "/usr/local/share/ui/BMP/dash_menu/",
            "icn-obd-menu.bmp");
    } else {
        _pIconBT = new BmpImage(
            _pPanel,
            Point(160, 80), Size(80, 80),
            "/usr/local/share/ui/BMP/dash_menu/",
            "icn-RC-menu.bmp");
    }

    memset(_btInfoDetail, 0x00, 64);
    _pBTInfoDetail = new StaticText(_pPanel, Point(0, 200), Size(400, 120));
    _pBTInfoDetail->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_2, CENTER, TOP);
    _pBTInfoDetail->SetLineHeight(50);
    _pBTInfoDetail->SetText((UINT8 *)_btInfoDetail);

    _pBtnRemove = new Button(_pPanel, Point(90, 300), Size(220, 76), Color_Black, Color_Grey_1);
    _pBtnRemove->SetText((UINT8 *)"Remove", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pBtnRemove->SetStyle(20);
    _pBtnRemove->setOnClickListener(this);


    _pTxt = new StaticText(_pPanel, Point(0, 76), Size(400, 80));
    _pTxt->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pTxt->SetText((UINT8 *)"Sure to remove\nthis device?");
    _pTxt->SetHiden();

    _pIconNo = new ImageButton(
        _pPanel,
        Point(56, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-no-up.bmp",
        "btn-dashboard-no-down.bmp");
    _pIconNo->setOnClickListener(this);
    _pIconNo->SetHiden();

    _pIconYes = new ImageButton(
        _pPanel,
        Point(216, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-yes_alt-up.bmp",
        "btn-dashboard-yes_alt-down.bmp");
    _pIconYes->setOnClickListener(this);
    _pIconYes->SetHiden();


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
        _pHelpInfo->SetText((UINT8 *)
            "Please plug in your"
            "\n"
            "OBD transmitter and"
            "\n"
            "power on your car"
            "\n"
            "before scanning.");
    } else if (_type == BTType_HID) {
        _pHelpInfo->SetText((UINT8 *)
            "Press your remote"
            "\n"
            "control once to"
            "\n"
            "wake it up before"
            "\n"
            "scanning.");
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

BTDetailWnd::~BTDetailWnd()
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

    delete _pIconYes;
    delete _pIconNo;
    delete _pTxt;

    delete _pBtnRemove;
    delete _pBTInfoDetail;
    delete _pIconBT;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;

    for (int i = 0; i < _numBTs; i++) {
        delete[] _pBTList[i];
    }
    delete [] _pBTList;
}

bool BTDetailWnd::isOBDBinded()
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

bool BTDetailWnd::isHIDBinded()
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

void BTDetailWnd::updateConnectingStatus()
{
    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        if (_type == BTType_HID) {
            agcmd_property_get(propBTTempHIDStatus, tmp, "");
            if (strcasecmp(tmp, "Connecting") == 0) {
                _bConnecting = true;
            } else {
                _bConnecting = false;
                if ((State_Detail_Show_Info == _state)
                    || (State_Detail_No_Device == _state))
                {
                    _pIconBT->SetShow();
                }
            }
        } else if (_type == BTType_OBD) {
            agcmd_property_get(propBTTempOBDStatus, tmp, "");
            if (strcasecmp(tmp, "Connecting") == 0) {
                _bConnecting = true;
            } else {
                _bConnecting = false;
                if ((State_Detail_Show_Info == _state)
                    || (State_Detail_No_Device == _state))
                {
                    _pIconBT->SetShow();
                }
            }
        }
    } else {
        _bConnecting = false;
        if ((State_Detail_Show_Info == _state)
            || (State_Detail_No_Device == _state))
        {
            _pIconBT->SetShow();
        }
    }
}

void BTDetailWnd::updateBTDetail()
{
    updateConnectingStatus();

    if ((State_Detail_Show_Info == _state)
        || (State_Detail_No_Device == _state))
    {
        char tmp1[AGCMD_PROPERTY_SIZE];
        char bdname_str[AGBT_MAX_NAME_SIZE];
        char bdpin_str[AGBT_MAX_PIN_SIZE];
        if (_type == BTType_OBD) {
            agcmd_property_get(propOBDMac, tmp1, "NA");
            if (strcasecmp(tmp1, "NA") == 0) {
                agcmd_property_get(Prop_OBD_MAC_Backup, tmp1, "NA");
            }
            if (strcasecmp(tmp1, "NA") == 0) {
                _pIconBT->SetShow();
                snprintf(_btInfoDetail, 64, "No OBD\nTransmitter Paired");
                _pBTInfoDetail->SetLineHeight(40);
                _pBTInfoDetail->SetRelativePos(Point(0, 180));
                _pBtnRemove->SetHiden();
                _pReScan->SetShow();
                _state = State_Detail_No_Device;
                return;
            }
        } else if (_type == BTType_HID) {
            agcmd_property_get(propHIDMac, tmp1, "NA");
            if (strcasecmp(tmp1, "NA") == 0) {
                agcmd_property_get(Prop_HID_MAC_Backup, tmp1, "NA");
            }
            if (strcasecmp(tmp1, "NA") == 0) {
                _pIconBT->SetShow();
                snprintf(_btInfoDetail, 64, "No Remote\nControl Paired");
                _pBTInfoDetail->SetLineHeight(40);
                _pBTInfoDetail->SetRelativePos(Point(0, 180));
                _pBtnRemove->SetHiden();
                _pReScan->SetShow();
                _state = State_Detail_No_Device;
                return;
            }
        }
        int len = 0;
        if (agbt_pin_get(tmp1, bdname_str, bdpin_str)) {
            len = snprintf(_btInfoDetail, 64, "unknown\n%s", tmp1);
        } else {
            len = snprintf(_btInfoDetail, 64, "%s\n%s", bdname_str, tmp1);
        }
        _pBTInfoDetail->SetLineHeight(50);
        _pBTInfoDetail->SetRelativePos(Point(0, 200));
        if (_type == BTType_HID) {
            agcmd_property_get(propBTTempHIDStatus, tmp1, "");
            if (strcasecmp(tmp1, "on") == 0) {
                agcmd_property_get(Prop_RC_Battery_level, tmp1, "0");
                if (len < 64) {
                    snprintf(_btInfoDetail + len, 64 - len, "\nbattery %s%%", tmp1);
                    _pBTInfoDetail->SetLineHeight(40);
                    _pBTInfoDetail->SetRelativePos(Point(0, 180));
                }
            }
        }
        _pBtnRemove->SetShow();
        _pReScan->SetHiden();
        _state = State_Detail_Show_Info;
        return;
    } else if (State_Wait_Confirm_To_Remove == _state) {
        if (_type == BTType_OBD) {
            char tmp1[AGCMD_PROPERTY_SIZE];
            agcmd_property_get(propOBDMac, tmp1, "NA");
            if (strcasecmp(tmp1, "NA") == 0) {
                agcmd_property_get(Prop_OBD_MAC_Backup, tmp1, "NA");
            }
            if (strcasecmp(tmp1, "NA") == 0) {
                _pIconBT->SetShow();
                snprintf(_btInfoDetail, 64, "No OBD\nTransmitter Paired");
                _pBTInfoDetail->SetLineHeight(40);
                _pBTInfoDetail->SetRelativePos(Point(0, 180));
                _pBTInfoDetail->SetShow();
                _pReScan->SetShow();
                _pTxt->SetHiden();
                _pIconYes->SetHiden();
                _pIconNo->SetHiden();
                _state = State_Detail_No_Device;
                return;
            }
        } else if (_type == BTType_HID) {
            char tmp1[AGCMD_PROPERTY_SIZE];
            agcmd_property_get(propHIDMac, tmp1, "NA");
            if (strcasecmp(tmp1, "NA") == 0) {
                agcmd_property_get(Prop_HID_MAC_Backup, tmp1, "NA");
            }
            if (strcasecmp(tmp1, "NA") == 0) {
                _pIconBT->SetShow();
                snprintf(_btInfoDetail, 64, "No Remote\nControl Paired");
                _pBTInfoDetail->SetLineHeight(40);
                _pBTInfoDetail->SetRelativePos(Point(0, 180));
                _pBTInfoDetail->SetShow();
                _pReScan->SetShow();
                _pTxt->SetHiden();
                _pIconYes->SetHiden();
                _pIconNo->SetHiden();
                _state = State_Detail_No_Device;
                return;
            }
        }
    } else {
        if (_cntDelayClose <= 0) {
            char tmp1[AGCMD_PROPERTY_SIZE];
            char bdname_str[AGBT_MAX_NAME_SIZE];
            char bdpin_str[AGBT_MAX_PIN_SIZE];
            if (_type == BTType_OBD) {
                agcmd_property_get(propOBDMac, tmp1, "NA");
                if (strcasecmp(tmp1, "NA") == 0) {
                    agcmd_property_get(Prop_OBD_MAC_Backup, tmp1, "NA");
                }
            } else if (_type == BTType_HID) {
                agcmd_property_get(propHIDMac, tmp1, "NA");
                if (strcasecmp(tmp1, "NA") == 0) {
                    agcmd_property_get(Prop_HID_MAC_Backup, tmp1, "NA");
                }
            }
            if (strcasecmp(tmp1, "NA") != 0) {
                if (agbt_pin_get(tmp1, bdname_str, bdpin_str)) {
                    snprintf(_btInfoDetail, 64, "unknown\n%s", tmp1);
                } else {
                    snprintf(_btInfoDetail, 64, "%s\n%s", bdname_str, tmp1);
                }

                _pIconBT->SetShow();
                _pBTInfoDetail->SetLineHeight(50);
                _pBTInfoDetail->SetRelativePos(Point(0, 200));
                _pBTInfoDetail->SetShow();
                _pBtnRemove->SetShow();

                _pTxt->SetHiden();
                _pIconYes->SetHiden();
                _pIconNo->SetHiden();
                _pTitle->SetHiden();
                _pLine->SetHiden();
                _pList->SetHiden();
                _pHelp->SetHiden();
                _pReScan->SetHiden();
                _pHelpInfo->SetHiden();
                _pInfo->SetHiden();
                _pAnimation->SetHiden();
                _pBindResult->SetHiden();

                _state = State_Detail_Show_Info;
                return;
            }
        }
    }
}

void BTDetailWnd::parseBT(char *json, int type)
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

void BTDetailWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pBtnRemove) {
        if (State_Detail_Show_Info == _state) {
            _pIconBT->SetHiden();
            _pBTInfoDetail->SetHiden();
            _pBtnRemove->SetHiden();
            _pTxt->SetShow();
            _pIconYes->SetShow();
            _pIconNo->SetShow();
            _pPanel->Draw(true);
            _state = State_Wait_Confirm_To_Remove;
        }
    } else if (widget == _pIconNo) {
        if (State_Wait_Confirm_To_Remove == _state) {
            this->Close(Anim_Top2BottomExit);
        }
    } else if (widget == _pIconYes) {
        if (State_Wait_Confirm_To_Remove == _state) {
            _pIconBT->SetShow();
            if (_type == BTType_OBD) {
                snprintf(_btInfoDetail, 64, "No OBD\nTransmitter Paired");
            } else if (_type == BTType_HID) {
                snprintf(_btInfoDetail, 64, "No Remote\nControl Paired");
            }
            _pBTInfoDetail->SetLineHeight(40);
            _pBTInfoDetail->SetRelativePos(Point(0, 180));
            _pBTInfoDetail->SetShow();
            _pReScan->SetShow();
            _pTxt->SetHiden();
            _pIconYes->SetHiden();
            _pIconNo->SetHiden();
            _pPanel->Draw(true);

            _cp->unBindBTDevice(_type);
            _state = State_Detail_No_Device;
        }
    } else if (widget == _pReScan) {
        char tmp[256];
        agcmd_property_get(propBTTempStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            if (_type == BTType_OBD) {
                _pInfo->SetText((UINT8 *)"Scanning OBD ...");
            } else if (_type == BTType_HID) {
                _pInfo->SetText((UINT8 *)"Scanning RC ...");
            }
            _pInfo->SetShow();
            _pAnimation->SetShow();
            _cntAnim = 0;
            _pIconBT->SetHiden();
            _pBTInfoDetail->SetHiden();
            _pBtnRemove->SetHiden();
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
        } else {
            _pInfo->SetText((UINT8 *)"Please enable BT!");
            _pInfo->SetStyle(32, FONT_ROBOTOMONO_Bold, Color_Red_1, CENTER, MIDDLE);
            _cntDelayClose = 10;
            _pInfo->SetShow();
            _pAnimation->SetHiden();
            _cntAnim = 0;
            _pIconBT->SetHiden();
            _pBTInfoDetail->SetHiden();
            _pBtnRemove->SetHiden();
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
        }
    } else if (widget == _pHelp) {
        _pTitle->SetHiden();
        _pLine->SetHiden();
        _pInfo->SetHiden();
        _pHelp->SetHiden();
        _pHelpInfo->SetShow();
        _pPanel->Draw(true);
    }
}

void BTDetailWnd::onListClicked(Control *list, int index)
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
                _counter = 0;
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

void BTDetailWnd::onMenuResume()
{
    updateBTDetail();
    _pPanel->Draw(true);
}

void BTDetailWnd::onMenuPause()
{
}

bool BTDetailWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                int x_pos = (position >> 16) & 0xFFFF;
                if ((x_pos < 70) || (x_pos > 330) || (y_pos < 60) || (y_pos > 350)) {
                    this->Close(Anim_Top2BottomExit);
                    b = false;
                }
            }
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_cntDelayClose > 0) {
                    _cntDelayClose --;
                    if (_cntDelayClose <= 0) {
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                    break;
                }

                if ((State_Detail_Show_Info == _state)
                    || (State_Detail_No_Device == _state))
                {
                    _counter++;
                    bool bReDraw = false;
                    if (_bConnecting) {
                        // blink OBD icon when connecting
                        if (_counter % 10 == 1) {
                            _pIconBT->SetShow();
                            bReDraw = true;
                        } else if (_counter % 10 == 6) {
                            _pIconBT->SetHiden();
                            bReDraw = true;
                        }
                    } else {
                        _pIconBT->SetShow();
                    }

                    if (bReDraw) {
                        _pPanel->Draw(true);
                    }
                } else if (_state == State_Scanning) {
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
                    b = false;
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
                    b = false;
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    bluetooth_state bt = _cp->getBluetoothState();
                    if (bt == bt_state_off) {
                        this->Close(Anim_Top2BottomExit);
                    } else {
                        updateBTDetail();
                        _pPanel->Draw(true);
                    }
                    b = false;
                }
            } else if (event->_event ==InternalEvent_app_state_update) {
                if (event->_paload == camera_State_scanBT_result) {
                    if ((_state == State_Scanning) || (_state == State_Scan_Timeout))
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

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}


OBDDetailWnd::OBDDetailWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTDetailWnd(cp, name, wndSize, leftTop, BTDetailWnd::BTType_OBD)
{
}

OBDDetailWnd::~OBDDetailWnd()
{
}


HIDDetailWnd::HIDDetailWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : BTDetailWnd(cp, name, wndSize, leftTop, BTDetailWnd::BTType_HID)
{
}

HIDDetailWnd::~HIDDetailWnd()
{
}

};
