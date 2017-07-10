/*******************************************************************************
**      Camera.cpp
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Description:
**
**      Revision History:
**      -----------------
**      01a 27- 8-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/

/*******************************************************************************
* <Includes>
*******************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#include "linux.h"

#include "GobalMacro.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "Widgets.h"
#include "class_camera_property.h"
#include "CameraAppl.h"
#include "agbox.h"
#include "aglib.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "CCameraCircularUI.h"
#include "ColorPalette.h"
#include "WindowRegistration.h"

#include "upload_service/vdbupload.h"

#include "AudioPlayer.h"

using namespace rapidjson;

namespace ui {

FastFuriousFragment::FastFuriousFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
{
    _pTitle = new StaticText(this, Point(0, 56), Size(400, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Performance Test");

    _pLine = new UILine(this, Point(50, 100), Size(300, 4));
    _pLine->setLine(Point(50, 100), Point(350, 100), 4, Color_Grey_2);

    _pCountdownMode = new ImageButton(
        this,
        Point(48, 136), Size(128, 128),
        "/usr/local/share/ui/BMP/performance_test/",
        "btn-shortcut-performance-countdown-up.bmp",
        "btn-shortcut-performance-countdown-up.bmp");
    _pCountdownMode->setOnClickListener(this);

    _pAutoMode = new ImageButton(
        this,
        Point(224, 136), Size(128, 128),
        "/usr/local/share/ui/BMP/performance_test/",
        "btn-shortcut-performance-auto-up.bmp",
        "btn-shortcut-performance-auto-up.bmp");
    _pAutoMode->setOnClickListener(this);

    _pScores = new Button(this, Point(100, 296), Size(200, 60), Color_Black, Color_Grey_1);
    _pScores->SetStyle(30);
    _pScores->SetBorder(true, Color_White, 4);
    _pScores->SetText((UINT8 *)"Scores", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pScores->SetTextAlign(CENTER, MIDDLE);
    _pScores->setOnClickListener(this);
}

FastFuriousFragment::~FastFuriousFragment()
{
    delete _pScores;
    delete _pAutoMode;
    delete _pCountdownMode;
    delete _pLine;
    delete _pTitle;
}

void FastFuriousFragment::onResume()
{
}

void FastFuriousFragment::onPause()
{
}

void FastFuriousFragment::onClick(Control *widget)
{
    if (widget == _pCountdownMode) {
        this->GetOwner()->StartWnd(WINDOW_60mph_countdown, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pAutoMode) {
        this->GetOwner()->StartWnd(WINDOW_60mph_auto, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pScores) {
        this->GetOwner()->StartWnd(WINDOW_test_score, Anim_Bottom2TopCoverEnter);
    }
}


RecordFragment::RecordFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
    , _counter(0)
    , _exitCnt(0)
{
    strcpy(_txt, "00:00:00");

    _pRecStatus = new BmpImage(
        this,
        Point(188, 14), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-tf_card.bmp");

    _pRecTxt = new StaticText(this, Point(100, 44), Size(200, 32));
    _pRecTxt->SetStyle(24, FONT_ROBOTOMONO_Medium, Color_White, CENTER, MIDDLE);
    _pRecTxt->SetText((UINT8 *)_txt);

    _pRecButton = new ImageButton(
        this,
        Point(136, 136), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-shortcut-rec-loop-up.bmp",
        "btn-shortcut-rec-loop-down.bmp");
    _pRecButton->setOnClickListener(this);

    _pTagButton = new ImageButton(
        this,
        Point(216, 136), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-highlight-up.bmp",
        "btn-dashboard-highlight-down.bmp");
    _pTagButton->setOnClickListener(this);
    _pTagButton->SetHiden();
}

RecordFragment::~RecordFragment()
{
    delete _pTagButton;
    delete _pRecButton;
    delete _pRecTxt;
    delete _pRecStatus;
}

void RecordFragment::updateRecStatus()
{
    camera_State state = _cp->getRecState();
    storage_State st = storage_State_ready;
    storage_rec_state tf_status = _cp->getStorageRecState();

    if (camera_State_stop == state) {
        // only read state from property when it is needed
        st = _cp->getStorageState();
    }

    // TODO: read record mode
    const char *rec_icon = "status-recording-loop.bmp";

    // Firstly, update record icon
    _pRecStatus->SetShow();
    if ((camera_State_record == state)
        || (camera_State_marking == state))
    {
        if (storage_rec_state_WriteSlow == tf_status) {
            const char *tf_icon = "status-tf_card-slow.bmp";
            if (_counter % 2) {
                _pRecStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    rec_icon);
            } else {
                _pRecStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    tf_icon);
            }
        } else if (storage_rec_state_DiskError == tf_status) {
            const char *tf_icon = "status-tf_card-error.bmp";
            if (_counter % 2) {
                _pRecStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    rec_icon);
            } else {
                _pRecStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    tf_icon);
            }
        } else {
            _pRecStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                rec_icon);
            if (_counter % 2) {
                _pRecStatus->SetHiden();
            } else {
                _pRecStatus->SetShow();
            }
        }
    }else if ((camera_State_starting == state)
        || (camera_State_stopping == state))
    {
        _pRecStatus->setSource(
            "/usr/local/share/ui/BMP/liveview/",
            rec_icon);
    } else {
        const char *tf_icon = "status-tf_card.bmp";
        if (storage_rec_state_DiskError == tf_status) {
            tf_icon = "status-tf_card-error.bmp";
        } else if (storage_rec_state_WriteSlow == tf_status) {
            tf_icon = "status-tf_card-slow.bmp";
        } else {
            if (storage_State_noMedia == st) {
                tf_icon = "status-tf_card-missing.bmp";
            }
        }
        _pRecStatus->setSource(
            "/usr/local/share/ui/BMP/liveview/",
            tf_icon);
    }

    // Secondly, update text
    if (camera_State_starting == state) {
        snprintf(_txt, 64, "%s...", "Starting");
    } else if (camera_State_stopping == state) {
        snprintf(_txt, 64, "%s...", "Stopping");
    } else if (camera_State_Error == state) {
        snprintf(_txt, 64, "%s", "Error");
    } else if (camera_State_record == state) {
        UINT64 tt = _cp->getRecordingTime();
        UINT64 hh =  tt /3600;
        UINT64 mm = (tt % 3600) /60;
        UINT64 ss = (tt % 3600) %60;
        int j = 0;
        j = snprintf(_txt, 64, "%02lld:", hh);
        j += snprintf(_txt + j, 64 - j, "%02lld:", mm);
        j += snprintf(_txt + j, 64 - j, "%02lld", ss);
    } else if (camera_State_marking == state) {
        UINT64 tt = _cp->getRecordingTime();
        UINT64 hh =  tt /3600;
        UINT64 mm = (tt % 3600) /60;
        UINT64 ss = (tt % 3600) %60;
        int j = 0;
        j = snprintf(_txt, 64, "Marking %02lld:", hh);
        j += snprintf(_txt + j, 64 - j, "%02lld:", mm);
        j += snprintf(_txt + j, 64 - j, "%02lld", ss);
    } else if (camera_State_stop == state){
        if (storage_rec_state_DiskError == tf_status) {
            snprintf(_txt, 64, "%s", "Card Error");
        } else {
                switch (st) {
                    case storage_State_ready:
                    {
                        int free = 0, total = 0;
                        int spaceMB = _cp->getFreeSpaceMB(free, total);
                        int bitrateKBS = _cp->getBitrate() / 8 / 1000;
                        if (bitrateKBS > 0) {
                            int time_sec = spaceMB * 1000 / bitrateKBS;
                            if (time_sec < 60) {
                                snprintf(_txt, 64, "%s", "<1min");
                            } else {
                                snprintf(_txt, 64, "%02dh%02dm",
                                    time_sec / 3600, (time_sec % 3600) / 60);
                            }
                        } else {
                            if (spaceMB > 1000) {
                                snprintf(_txt, 64, "%d.%dG",
                                    spaceMB / 1000, (spaceMB % 1000) / 100);
                            } else {
                                if (spaceMB >= 1) {
                                    snprintf(_txt, 64, "%dM", spaceMB);
                                } else {
                                    snprintf(_txt, 64, "%s", "<1MB");
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
        }
    }
}

void RecordFragment::updateRecButton()
{
    // TODO: different icon for different record mode
    const char *rec_icon_up = "btn-shortcut-rec-loop-up.bmp";
    const char *rec_icon_down = "btn-shortcut-rec-loop-down.bmp";

    camera_State state = _cp->getRecState();
    if ((camera_State_record == state)
        || (camera_State_marking == state)
        || (camera_State_starting == state))
    {
        _pRecButton->SetRelativePos(Point(56, 136));
        _pRecButton->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-stop-up.bmp",
            "btn-dashboard-stop-down.bmp");

        _pTagButton->SetShow();
    } else {
        _pRecButton->SetRelativePos(Point(136, 136));
        _pRecButton->setSource("/usr/local/share/ui/BMP/dash_menu/",
            rec_icon_up,
            rec_icon_down);

        _pTagButton->SetHiden();
    }
}

void RecordFragment::onResume()
{
    updateRecStatus();
    updateRecButton();
    this->Draw(true);
}

void RecordFragment::onPause()
{
}

void RecordFragment::onClick(Control *widget)
{
    if (_pRecButton == widget) {
        storage_State st = _cp->getStorageState();
        if ((storage_State_ready == st) ||
            ((storage_State_full == st) && _cp->isCircleMode()))
        {
            _cp->OnRecKeyProcess();
            _exitCnt = 6;
        } else if (storage_State_noMedia == st) {
            this->GetOwner()->StartWnd(WINDOW_cardmissing, Anim_Top2BottomEnter);
        } else {
            CAMERALOG("TF not ready");
            strcpy(_txt, "TF not ready");
            this->Draw(true);
        }
    } else if (_pTagButton == widget) {
        camera_State state = _cp->getRecState();
        if ((camera_State_record == state)
            || (camera_State_marking == state))
        {
            _cp->MarkVideo();
        }
    }
}

bool RecordFragment::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if ((event->_event == InternalEvent_app_timer_update)) {
#if 0
                if (_exitCnt > 0) {
                    _exitCnt--;
                    if (_exitCnt == 0) {
                        this->GetOwner()->StartWnd(WINDOW_gauges, Anim_Bottom2TopEnter);
                        b = false;
                        break;
                    }
                }
#endif
                camera_State state = _cp->getRecState();
                if ((camera_State_record == state)
                    || (camera_State_marking == state)
                    || (camera_State_starting == state)
                    || (camera_State_stopping == state)
                    || (camera_State_Error == state))
                {
                    if (_counter % 5 == 0) {
                        updateRecStatus();
                        this->Draw(true);
                    }
                } else {
                    if (_counter % 50 == 0) {
                        updateRecStatus();
                        this->Draw(true);
                    }
                }
                _counter++;
            } else if (event->_event == InternalEvent_app_state_update) {
                if ((event->_paload == camera_State_starting)
                    || (event->_paload == camera_State_stopping))
                {
                    updateRecStatus();
                    updateRecButton();
                    this->Draw(true);
                } else if ((event->_paload == camera_State_stop)
                    || (event->_paload == camera_State_record))
                {
                    if (_exitCnt > 0) {
                        _exitCnt = 0;
                        this->GetOwner()->StartWnd(WINDOW_gauges, Anim_Bottom2TopEnter);
                    } else {
                        updateRecStatus();
                        updateRecButton();
                        this->Draw(true);
                    }
                } else if (event->_paload == camera_State_Storage_changed) {
                    updateRecStatus();
                    this->Draw(true);
                } else if (event->_paload == camera_State_Storage_full) {
                    updateRecStatus();
                    this->Draw(true);
                }
                b = false;
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    updateRecStatus();
                    this->Draw(true);
                    b = false;
                }
            }
            break;
        default:
            break;
    }

    return b;
}


ShotcutsFragment::ShotcutsFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
{
    _pBrightness = new ImageButton(
        this,
        Point(56, 56), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-shortcut-power-up.bmp",
        "btn-shortcut-power-down.bmp");
    _pBrightness->setOnClickListener(this);

    _pSpeaker = new ImageButton(
        this,
        Point(216, 56), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-silent-up.bmp",
        "btn-dashboard-silent-up.bmp");
    _pSpeaker->setOnClickListener(this);

    _pMicmute = new ImageButton(
        this,
        Point(56, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-mute-up.bmp",
        "btn-dashboard-mute-up.bmp");
    _pMicmute->setOnClickListener(this);

    _pWifiInfo = new ImageButton(
        this,
        Point(216, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-wifi-up.bmp",
        "btn-dashboard-wifi-down.bmp");
    _pWifiInfo->setOnClickListener(this);
}

ShotcutsFragment::~ShotcutsFragment()
{
    delete _pWifiInfo;
    delete _pMicmute;
    delete _pSpeaker;
    delete _pBrightness;
}

void ShotcutsFragment::updateMic()
{
    char tmp[32];
    agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
    if (strcasecmp(tmp, "Normal") == 0) {
        _pMicmute->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-mute-up.bmp",
            "btn-dashboard-mute-up.bmp");
    } else if (strcasecmp(tmp, "Mute") == 0) {
        _pMicmute->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-mute-down.bmp",
            "btn-dashboard-mute-down.bmp");
    }

    agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
    if (strcasecmp(tmp, "Normal") == 0) {
        _pSpeaker->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-silent-up.bmp",
            "btn-dashboard-silent-up.bmp");
    } else if (strcasecmp(tmp, "Mute") == 0) {
        _pSpeaker->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-silent-down.bmp",
            "btn-dashboard-silent-down.bmp");
    }
}

void ShotcutsFragment::onResume()
{
    updateMic();
    this->Draw(true);
}

void ShotcutsFragment::onPause()
{
}

void ShotcutsFragment::onClick(Control *widget)
{
    if (_pBrightness == widget) {
        this->GetOwner()->StartWnd(WINDOW_poweroff, Anim_Bottom2TopCoverEnter);
    } else if (_pSpeaker == widget) {
        char tmp[32];
        agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
        if (strcasecmp(tmp, "Normal") == 0) {
            _cp->SetSpkMute(true);
        } else if (strcasecmp(tmp, "Mute") == 0) {
            _cp->SetSpkMute(false);
        }
    } else if (_pMicmute == widget) {
        char tmp[32];
        agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
        if (strcasecmp(tmp, "Normal") == 0) {
            _cp->SetMicMute(true);
            CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Alert);
        } else if (strcasecmp(tmp, "Mute") == 0) {
            _cp->SetMicMute(false);
        }
    } else if (_pWifiInfo == widget) {
        this->GetOwner()->StartWnd(WINDOW_wifimenu, Anim_Bottom2TopCoverEnter);
    }
}

bool ShotcutsFragment::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_audio) {
                    updateMic();
                    this->Draw(true);
                    b = false;
                }
            }
            break;
        default:
            break;
    }

    return b;
}

ViewPagerShotcutsWnd::ViewPagerShotcutsWnd(
    CCameraProperties *cp,
    const char *name,
    Size wndSize,
    Point leftTop)
  : inherited(name, wndSize, leftTop)
  , _cp(cp)
  , _numPages(0)
  , _indexFocus(0)
  , _cntScreenSaver(0)
  , _screenSaverTimeSetting(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 4);
    this->SetMainObject(_pPanel);

    _pViewPager = new ViewPager(_pPanel, this, Point(0, 0), Size(400, 400), 20);

    _numPages = 0;

    _pPages[_numPages++] = new FastFuriousFragment(
        "FastFuriousFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);

    _indexFocus = _numPages;
    _pPages[_numPages++] = new RecordFragment(
        "RecordFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);

    _pPages[_numPages++] = new ShotcutsFragment(
        "ShotcutsFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);

    _pViewPager->setPages(_pPages, _numPages);
    _pViewPager->setListener(this);

    _pIndicator = new TabIndicator(
        _pPanel, Point(140, 382), Size(120, 8),
        _numPages, _indexFocus,
        4, 0x9CF3, 0x3187);

    _pViewPager->setFocus(_indexFocus);
    _pIndicator->setHighlight(_indexFocus);
}

ViewPagerShotcutsWnd::~ViewPagerShotcutsWnd()
{
    delete _pIndicator;
    delete _pPages[2];
    delete _pPages[1];
    delete _pPages[0];
    delete _pViewPager;
    delete _pPanel;
}

int ViewPagerShotcutsWnd::_getScreenSaverTime()
{
    int timeOff = 60;
    char tmp[256];
    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);
    if (strcasecmp(tmp, "Never") == 0) {
        timeOff = -1;
    } else if (strcasecmp(tmp, "10s") == 0) {
        timeOff = 10;
    } else if (strcasecmp(tmp, "30s") == 0) {
        timeOff = 30;
    } else if (strcasecmp(tmp, "60s") == 0) {
        timeOff = 60;
    } else if (strcasecmp(tmp, "2min") == 0) {
        timeOff = 120;
    } else if (strcasecmp(tmp, "5min") == 0) {
        timeOff = 300;
    }

    _screenSaverTimeSetting = timeOff * 10;
    return _screenSaverTimeSetting;
}

void ViewPagerShotcutsWnd::onFocusChanged(int indexFocus)
{
    _indexFocus = indexFocus;
    _pIndicator->setHighlight(indexFocus);
    _pPanel->Draw(true);
}

void ViewPagerShotcutsWnd::onViewPortChanged(const Point &leftTop)
{
    _pIndicator->Draw(false);
}

void ViewPagerShotcutsWnd::onResume()
{
    _getScreenSaverTime();
    _cntScreenSaver = 0;
#if 1
    _pViewPager->getCurFragment()->onResume();
    _pIndicator->setHighlight(_indexFocus);
#else
    _pViewPager->setFocus(0);
    _pViewPager->getCurFragment()->onResume();
    _pIndicator->setHighlight(0);
#endif
}

void ViewPagerShotcutsWnd::onPause()
{
    _cntScreenSaver = 0;
#if 1
    _pViewPager->getCurFragment()->onPause();
#else
    _pViewPager->setFocus(0);
    _pViewPager->getCurFragment()->onPause();
    _pIndicator->setHighlight(0);
#endif
}

bool ViewPagerShotcutsWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _cntScreenSaver++;
                if ((_screenSaverTimeSetting > 0) && (_cntScreenSaver >= _screenSaverTimeSetting)) {
                    _cntScreenSaver = 0;
                    this->StartWnd(WINDOW_screen_saver, Anim_Null);
                }
            } else if (event->_paload == PropertyChangeType_settings) {
                _getScreenSaverTime();
            }
            break;
        case eventType_touch:
            _cntScreenSaver = 0;
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_UP)) {
                this->StartWnd(WINDOW_gauges, Anim_Bottom2TopEnter);
                b = false;
            }
            break;
        case eventType_key:
            _cntScreenSaver = 0;
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
            _cntScreenSaver = 0;
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

    snprintf(_txt, 64, "%s", "30");
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
                this->Close(Anim_Null);
                camera_State state = _cp->getRecState();
                if ((camera_State_record != state)
                    && (camera_State_marking != state)
                    && (camera_State_starting != state))
                {
                    _cp->startRecMode();
                }
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
        char tmp[256];
        memset(tmp, 0, sizeof(tmp));
        agcmd_property_get(SotrageStatusPropertyName, tmp, "");
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
  , _origin_state(camera_State_record)
  , _counter(0)
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
}

OverheatWnd::~OverheatWnd()
{
    delete _pTxt;
    delete _pIcon;
    delete _pBG;
    delete _pPanel;
}

void OverheatWnd::onPause()
{
}

void OverheatWnd::onResume()
{
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
                // check temprature every minute
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
                    if (_counter >= 200 /* 20s */) {
                        // Scan time out!
                        _counter = 0;
                        parseBT(NULL, _type);
                        _state = State_Got_List;
                        _pTitle->SetShow();
                        _pLine->SetShow();
                        _pList->SetShow();
                        if (_numBTs <= 0) {
                            _pReScan->SetShow();
                            _pHelp->SetShow();
                        }
                        _pTxt->SetHiden();
                        _pIconNo->SetHiden();
                        _pIconYes->SetHiden();
                        _pInfo->SetHiden();
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
                    if (_state == State_Scanning) {
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


CoolantTempFragment::CoolantTempFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
    , _temperature(50)
    , _bChanged(true)
    , _bObdPaired(false)
    , _bDemoEnabled(false)
    , _bTita(true)
    , _counter(0)
    , _bImperialUnits(true)
{
    _pRTempe = new RoundCap(this, Point(200, 200), 180, 0xFFE3);
    _pRTempe->setAngle(180.0);
    _pRTempe->setRatio(0.2);
    _pRTempe->SetHiden();

    _pRound = new Round(this, Point(200, 200), 130, 0x18A3);

    _pUnit = new BmpImage(
        this,
        Point(264, 180), Size(64, 40),
        "/usr/local/share/ui/BMP/gauge/",
        "label-unit_C.bmp");
    _pUnit->SetHiden();

    _pSign = new BmpImage(
        this,
        Point(152, 189), Size(22, 22),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-144-minus-active.bmp");
    _pSign->SetHiden();

    _pNum_h = new BmpImage(
        this,
        Point(122, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-0.bmp");
    _pNum_h->SetHiden();

    _pNum_d = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "N.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "A.bmp");

    _pLabel = new BmpImage(
        this,
        Point(133, 270), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-Engine_Temp.bmp");

    _pLabel_demo = new BmpImage(
        this,
        Point(133, 282), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-demo.bmp");
    _pLabel_demo->SetHiden();
}

CoolantTempFragment::~CoolantTempFragment()
{
    delete _pLabel_demo;
    delete _pLabel;
    delete _pNum_c;
    delete _pNum_d;
    delete _pNum_h;
    delete _pSign;
    delete _pUnit;
    delete _pRound;
    delete _pRTempe;
}

bool CoolantTempFragment::getTemperature()
{
    int tempe = 0;
    bool got = _cp->getEngineTemp(tempe);
    if (!got) {
        _bChanged = false;
        return false;
    }

    if (tempe < -40) {
        tempe = -40;
    } else if (tempe > 215) {
        tempe = 215;
    }

    if (_bImperialUnits) {
        tempe = tempe * 9 / 5 + 32;
    }

    if (tempe == _temperature) {
        _bChanged = false;
    } else {
        _temperature = tempe;
        _bChanged = true;
    }

    return true;
}

bool CoolantTempFragment::getTemperatureFake()
{
    if (_counter > 0) {
        _counter --;
        _bChanged = false;
        return false;
    }
    int Max = 215;
    int Min = -40;
    int step = 255 / 60;
    if (_bImperialUnits) {
        Max = Max * 1.8 + 32;
        step = step * 1.8;
    }
    if (_bTita) {
        _temperature += step;
        if (_temperature > Max) {
            _temperature = Max;
            _bTita = false;
            _counter = 10;
        }
    } else {
        _temperature -= step;
        if (_temperature < Min) {
            _temperature = Min;
            _bTita = true;
            _counter = 40;
        }
    }

    _bChanged = true;
    return true;
}

bool CoolantTempFragment::readOBDData()
{
    bool got = false;
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            got = getTemperatureFake();
        }
    } else {
        got = getTemperature();
        _bChanged = true;
    }

    return got;
}

void CoolantTempFragment::updateSymbols(bool bValid)
{
    if (!bValid) {
        _pNum_h->SetHiden();
        _pSign->SetHiden();

        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", "N.bmp");
        _pNum_d->SetRelativePos(Point(148, 130));

        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", "A.bmp");
        _pNum_c->SetRelativePos(Point(200, 130));

        _pUnit->SetHiden();
        _pRTempe->SetHiden();

        return;
    }

    _pUnit->SetShow();
    _pRTempe->SetShow();

    float Scope = 160.0;
    float Threshold = 100.0;
    float Offset = 40.0;
    if (_bImperialUnits) {
        Scope = 286.0;
        Threshold = 212.0;
    }

    int num = _temperature;

    float ratio = 0.2 + (num + Offset) / Scope;
    if (ratio < 0.0) {
        ratio = 0.0;
    } else if (ratio > 2.0) {
        ratio = 2.0;
    }
    if (num >= Threshold) {
        _pRTempe->setColor(0xE9C5);
    } else {
        _pRTempe->setColor(0xFFE3);
    }
    _pRTempe->setRatio(ratio);
    _pRTempe->setAngle(180.0);

    char name[64];
    if (num >= 100) {
        _pSign->SetHiden();

        snprintf(name, 64, "num-144-%d.bmp", num / 100);
        _pNum_h->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_h->SetShow();
        num = num % 100;

        snprintf(name, 64, "num-144-%d.bmp", num / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148 + 26, 130));

        snprintf(name, 64, "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200 + 26, 130));
    } else if (num < 0) {
        _pNum_h->SetHiden();
        _pSign->SetShow();

        num = -1 * num;
        if (num >= 100) {
            num = 99;
        }
        snprintf(name, 64, "num-144-%d.bmp", num / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148 + 26, 130));

        snprintf(name, 64, "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200 + 26, 130));
    } else {
        _pNum_h->SetHiden();
        _pSign->SetHiden();

        snprintf(name, 64, "num-144-%d.bmp", num / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148, 130));

        snprintf(name, 64, "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200, 130));
    }
}

bool CoolantTempFragment::checkOBDPaired()
{
    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        agcmd_property_get(propBTTempOBDStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            _bObdPaired = true;
        } else {
            _bObdPaired = false;
        }
    } else {
        _bObdPaired = false;
    }

    agcmd_property_get(PropName_Settings_OBD_Demo, tmp, "off");
    if (strcasecmp(tmp, "on") == 0) {
        _bDemoEnabled = true;
    } else {
        _bDemoEnabled = false;
    }

    return _bObdPaired;
}

void CoolantTempFragment::onResume()
{
    char tmp[256];
    agcmd_property_get(PropName_Units_System, tmp, Imperial_Units_System);
    if (strcasecmp(tmp, Imperial_Units_System) == 0) {
        _bImperialUnits = true;
        _pUnit->setSource("/usr/local/share/ui/BMP/gauge/", "label-unit_F.bmp");
    } else {
        _bImperialUnits = false;
        _pUnit->setSource("/usr/local/share/ui/BMP/gauge/", "label-unit_C.bmp");
    }

    checkOBDPaired();

    //CAMERALOG("%s: _bObdPaired = %d, _bDemoEnabled = %d",
    //    getFragName(), _bObdPaired, _bDemoEnabled);
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            _pLabel->SetRelativePos(Point(133, 256));
            _pLabel_demo->SetRelativePos(Point(133, 282));
            _pLabel_demo->SetShow();
            updateSymbols(true);
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _temperature = 0;
            updateSymbols(false);
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
        updateSymbols(true);
    }
    this->Draw(true);
}

void CoolantTempFragment::onPause()
{
}

bool CoolantTempFragment::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (readOBDData()) {
                    updateSymbols(true);
                } else {
                    updateSymbols(false);
                }

                if (_bChanged) {
                    this->Draw(true);
                    _bChanged = false;
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    checkOBDPaired();
                    if (!_bObdPaired) {
                        if (_bDemoEnabled) {
                            _pLabel->SetRelativePos(Point(133, 256));
                            _pLabel_demo->SetRelativePos(Point(133, 282));
                            _pLabel_demo->SetShow();
                            updateSymbols(true);
                        } else {
                            updateSymbols(false);
                        }
                    } else {
                        _pLabel_demo->SetHiden();
                        _pLabel->SetRelativePos(Point(133, 270));
                        updateSymbols(true);
                    }
                    this->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}


BoostFragment::BoostFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
    , _boost(0)
    , _bChanged(true)
    , _bObdPaired(false)
    , _bDemoEnabled(false)
    , _bTita(true)
    , _counter(0)
{
    _maxKpa = 35 * 6.895;
    _minKpa = -30 * 6.895;

    _pFanBoost = new Fan(this, Point(200, 200), 180, 0xFFE3);
    _pFanBoost->setAngles(225.01, 225);
    _pFanBoost->SetHiden();

    _pRound = new Round(this, Point(200, 200), 130, 0x18A3);

    _pNum_d = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "N.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "A.bmp");

    _pUnit = new BmpImage(
        this,
        Point(264, 180), Size(64, 40),
        "/usr/local/share/ui/BMP/gauge/",
        "label-unit_PSI.bmp");
    _pUnit->SetHiden();

    _pLabel = new BmpImage(
        this,
        Point(133, 270), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-boost.bmp");

    _pLabel_demo = new BmpImage(
        this,
        Point(133, 282), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-demo.bmp");
    _pLabel_demo->SetHiden();
}

BoostFragment::~BoostFragment()
{
    delete _pLabel_demo;
    delete _pLabel;
    delete _pUnit;
    delete _pNum_c;
    delete _pNum_d;
    delete _pRound;
    delete _pFanBoost;
}

bool BoostFragment::getBoostValue()
{
    int pressure = 0;
    bool got = _cp->getBoostKpa(pressure);
    if (!got) {
        _bChanged = false;
        return false;
    }

    if (pressure < _minKpa) {
        pressure = _minKpa;
    } else if (pressure > _maxKpa) {
        pressure = _maxKpa;
    }

    if (pressure == _boost) {
        _bChanged = false;
    } else {
        _bChanged = true;
        _boost = pressure;
    }
    return true;
}

bool BoostFragment::getBoostValueFake()
{
    if (_counter > 0) {
        _counter --;
        _bChanged = false;
        return false;
    }
    int stepP = _maxKpa / 20 + 1;
    int stepM = -1 * _minKpa / 20 - 1;
    if (_bTita) {
        if (_boost < 0) {
            _boost += stepM;
            if (_boost >= 0) {
                _boost = 0;
                _counter = 40;
            }
        } else {
            _boost += stepP;
            if (_boost > _maxKpa) {
                _boost = _maxKpa;
                _bTita = false;
                _counter = 10;
            }
        }
    } else {
        if (_boost > 0) {
            _boost -= stepP;
            if (_boost < 0) {
                _boost = 0;
            }
        } else {
            _boost -= stepM;
            if (_boost < _minKpa) {
                _boost = _minKpa;
                _bTita = true;
                _counter = 10;
            }
        }
    }
#if 0
    int minus = rand() % 2;
    float boost = (minus == 0 ? 1 : (-1)) * rand() % 3;
    if (abs(_boost) < 20) {
        _boost = _boost + 3 * boost;
    } else if (abs(_boost) < 60) {
        _boost = _boost + 2 * boost;
    } else if (_boost >= 100.0) {
        _boost = 99.0;
    } else if (_boost <= -100.0) {
        _boost = -99.0;
    } else {
        _boost = _boost + boost;
    }
#endif
    _bChanged = true;
    return true;
}

bool BoostFragment::readOBDData()
{
    bool got = false;
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            got = getBoostValueFake();
        }
    } else {
        got = getBoostValue();
        _bChanged = true;
    }

    return got;
}

void BoostFragment::updateSymbols(bool bValid)
{
    if (!bValid) {
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", "N.bmp");

        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", "A.bmp");

        _pUnit->SetHiden();
        _pFanBoost->SetHiden();

        return;
    }

    _pUnit->SetShow();
    _pFanBoost->SetShow();

    float val = 0.0;
    if (_boost >= 0) {
        // making start from 225.01 so that when boost is zero, there is no display
        _pFanBoost->setAngles(225.01 + _boost / _maxKpa * 135, 225);
        _pFanBoost->setColor(0xFFE3);
        _pUnit->setSource("/usr/local/share/ui/BMP/gauge/", "label-unit_PSI.bmp");
        val = _boost / 6.895;
    } else if (_boost < 0) {
        _pFanBoost->setAngles(224.99, 225 - _boost / _minKpa * 45);
        _pFanBoost->setColor(0xE9C5);
        _pUnit->setSource("/usr/local/share/ui/BMP/gauge/", "label-unit_InHg.bmp");
        val = -1 * _boost / 3.386;
    }

    int num = val;
    if (num >= 100) {
        num = num % 100;
    }
    char name[64];
    snprintf(name, 64, "num-144-%d.bmp", num / 10);
    _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);

    snprintf(name, 64, "num-144-%d.bmp", num % 10);
    _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
}

bool BoostFragment::checkOBDPaired()
{
    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        agcmd_property_get(propBTTempOBDStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            _bObdPaired = true;
        } else {
            _bObdPaired = false;
        }
    } else {
        _bObdPaired = false;
    }

    agcmd_property_get(PropName_Settings_OBD_Demo, tmp, "off");
    if (strcasecmp(tmp, "on") == 0) {
        _bDemoEnabled = true;
    } else {
        _bDemoEnabled = false;
    }

    return _bObdPaired;
}

void BoostFragment::onResume()
{
    checkOBDPaired();

    //CAMERALOG("%s: _bObdPaired = %d, _bDemoEnabled = %d",
    //    getFragName(), _bObdPaired, _bDemoEnabled);
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            _pLabel->SetRelativePos(Point(133, 260));
            _pLabel_demo->SetRelativePos(Point(133, 282));
            _pLabel_demo->SetShow();
            updateSymbols(true);
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _boost = 0;
            updateSymbols(false);
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
        updateSymbols(true);
    }
    this->Draw(true);
}

void BoostFragment::onPause()
{
}

bool BoostFragment::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (readOBDData()) {
                    updateSymbols(true);
                } else {
                    updateSymbols(false);
                }

                if (_bChanged) {
                    this->Draw(true);
                    _bChanged = false;
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    checkOBDPaired();
                    if (!_bObdPaired) {
                        if (_bDemoEnabled) {
                            _pLabel->SetRelativePos(Point(133, 260));
                            _pLabel_demo->SetRelativePos(Point(133, 282));
                            _pLabel_demo->SetShow();
                            updateSymbols(true);
                        } else {
                            updateSymbols(false);
                        }
                    } else {
                        _pLabel_demo->SetHiden();
                        _pLabel->SetRelativePos(Point(133, 270));
                        updateSymbols(true);
                    }
                    this->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}


MAFFragment::MAFFragment(
    const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
  : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
  , _cp(cp)
  , _maf(0)
  , _bChanged(true)
  , _bObdPaired(false)
  , _bDemoEnabled(false)
  , _bTita(true)
  , _counter(0)
  , _bImperialUnits(true)
{
    _pRMAF = new Round(this, Point(200, 200), 132, 0xFFE3);
    _pRMAF->SetHiden();

    _pRound = new Round(this, Point(200, 200), 130, 0x18A3);

    _pUnit = new BmpImage(
        this,
        Point(264, 180), Size(64, 40),
        "/usr/local/share/ui/BMP/gauge/",
        "label-unit_gs.bmp");
    _pUnit->SetHiden();

    _pNum_h = new BmpImage(
        this,
        Point(122, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-0.bmp");
    _pNum_h->SetHiden();

    _pNum_d = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "N.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "A.bmp");

    _pLabel = new BmpImage(
        this,
        Point(133, 270), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-MAF.bmp");

    _pLabel_demo = new BmpImage(
        this,
        Point(133, 282), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-demo.bmp");
    _pLabel_demo->SetHiden();
}

MAFFragment::~MAFFragment()
{
    delete _pLabel_demo;
    delete _pLabel;
    delete _pNum_c;
    delete _pNum_d;
    delete _pNum_h;
    delete _pUnit;
    delete _pRound;
    delete _pRMAF;
}

bool MAFFragment::getMAF()
{
    int maf = 0;
    bool got = _cp->getAirFlowRate(maf);
    if (!got) {
        _bChanged = false;
        return false;
    }

    if (maf < 0) {
        maf = 0;
    } else if (maf > 655) {
        maf = 655;
    }

    if (_bImperialUnits) {
        maf = maf * 60 / 454;
    }

    if (maf == _maf) {
        _bChanged = false;
    } else {
        _maf = maf;
        _bChanged = true;
    }

    return true;
}

bool MAFFragment::getMAFFake()
{
    if (_counter > 0) {
        _counter --;
        _bChanged = false;
        return false;
    }

    int Max = 655;
    if (_bImperialUnits) {
        Max = 87;
    }
    int step = Max / 40;
    if (_bTita) {
        _maf += step;
        if (_maf > Max) {
            _maf = Max;
            _bTita = false;
            _counter = 10;
        }
    } else {
        _maf -= step;
        if (_maf < 0) {
            _maf = 0;
            _bTita = true;
            _counter = 40;
        }
    }
    _bChanged = true;
    return true;
}

bool MAFFragment::readOBDData()
{
    bool got = false;
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            got = getMAFFake();
        }
    } else {
        got = getMAF();
        _bChanged = true;
    }

    return got;
}

void MAFFragment::updateSymbols(bool bValid)
{
    if (!bValid) {
        _pNum_h->SetHiden();

        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", "N.bmp");
        _pNum_d->SetRelativePos(Point(148, 130));

        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", "A.bmp");
        _pNum_c->SetRelativePos(Point(200, 130));

        _pUnit->SetHiden();
        _pRMAF->SetHiden();

        return;
    }

    _pUnit->SetShow();
    _pRMAF->SetShow();

    int Max = 200;
    int Threshold = 100;
    if (_bImperialUnits) {
        Max = Max * 60 / 454;
        Threshold = Threshold * 60 / 454;
    }
    int num = _maf;

    if (num >= Max) {
        _pRMAF->setParams(Point(200, 200), 199, 0xE9C5);
    } else if (num >= Threshold) {
        _pRMAF->setParams(Point(200, 200), 132 + 67.0 * num / Max, 0xE9C5);
    } else {
        _pRMAF->setParams(Point(200, 200), 132 + 67.0 * num / Max, 0xFFE3);
    }

    char name[64];
    if (num >= 100) {
        snprintf(name, 64, "num-144-%d.bmp", num / 100);
        _pNum_h->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_h->SetShow();
        num = num % 100;

        snprintf(name, 64, "num-144-%d.bmp", num / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148 + 26, 130));

        snprintf(name, 64, "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200 + 26, 130));
    } else {
        _pNum_h->SetHiden();

        snprintf(name, 64, "num-144-%d.bmp", num / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148, 130));

        snprintf(name, 64, "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200, 130));
    }
}

bool MAFFragment::checkOBDPaired()
{
    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        agcmd_property_get(propBTTempOBDStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            _bObdPaired = true;
        } else {
            _bObdPaired = false;
        }
    } else {
        _bObdPaired = false;
    }

    agcmd_property_get(PropName_Settings_OBD_Demo, tmp, "off");
    if (strcasecmp(tmp, "on") == 0) {
        _bDemoEnabled = true;
    } else {
        _bDemoEnabled = false;
    }

    return _bObdPaired;
}

void MAFFragment::onResume()
{
    char tmp[256];
    agcmd_property_get(PropName_Units_System, tmp, Imperial_Units_System);
    if (strcasecmp(tmp, Imperial_Units_System) == 0) {
        _bImperialUnits = true;
        _pUnit->setSource("/usr/local/share/ui/BMP/gauge/", "label-unit_lbmin.bmp");
    } else {
        _bImperialUnits = false;
        _pUnit->setSource("/usr/local/share/ui/BMP/gauge/", "label-unit_gs.bmp");
    }

    checkOBDPaired();

    //CAMERALOG("%s: _bObdPaired = %d, _bDemoEnabled = %d",
    //    getFragName(), _bObdPaired, _bDemoEnabled);
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            _pLabel->SetRelativePos(Point(133, 260));
            _pLabel_demo->SetRelativePos(Point(133, 282));
            _pLabel_demo->SetShow();
            updateSymbols(true);
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _maf = 0;
            updateSymbols(false);
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
        updateSymbols(true);
    }
    this->Draw(true);
}

void MAFFragment::onPause()
{
}

bool MAFFragment::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (readOBDData()) {
                    updateSymbols(true);
                } else {
                    updateSymbols(false);
                }

                if (_bChanged) {
                    this->Draw(true);
                    _bChanged = false;
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    checkOBDPaired();
                    if (!_bObdPaired) {
                        if (_bDemoEnabled) {
                            _pLabel->SetRelativePos(Point(133, 260));
                            _pLabel_demo->SetRelativePos(Point(133, 282));
                            _pLabel_demo->SetShow();
                            updateSymbols(true);
                        } else {
                            updateSymbols(false);
                        }
                    } else {
                        _pLabel_demo->SetHiden();
                        _pLabel->SetRelativePos(Point(133, 270));
                        updateSymbols(true);
                    }
                    this->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}


RPMFragment::RPMFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
    , _rpm(0)
    , _bChanged(true)
    , _bObdPaired(false)
    , _bDemoEnabled(false)
    , _bTita(true)
    , _counter(0)
{
    _pRRPM = new Round(this, Point(200, 200), 132, 0xFFE3);
    _pRRPM->SetHiden();

    _pRound = new Round(this, Point(200, 200), 130, 0x18A3);

    _pNum_h = new BmpImage(
        this,
        Point(122, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-0.bmp");
    _pNum_h->SetHiden();

    _pNum_d = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "N.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "A.bmp");

    _pUnit = new BmpImage(
        this,
        Point(264, 180), Size(64, 40),
        "/usr/local/share/ui/BMP/gauge/",
        "label-x100.bmp");
    _pUnit->SetHiden();

    _pLabel = new BmpImage(
        this,
        Point(133, 270), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-RPM.bmp");

    _pLabel_demo = new BmpImage(
        this,
        Point(133, 282), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-demo.bmp");
    _pLabel_demo->SetHiden();
}

RPMFragment::~RPMFragment()
{
    delete _pLabel_demo;
    delete _pLabel;
    delete _pUnit;
    delete _pNum_c;
    delete _pNum_d;
    delete _pNum_h;
    delete _pRound;
    delete _pRRPM;
}

bool RPMFragment::getRPM()
{
    int rpm = 0;
    bool got = _cp->getRPM(rpm);
    if (!got) {
        _bChanged = false;
        return false;
    }

    if (rpm < 0) {
        rpm = 0;
    } else if (rpm > 9900) {
        rpm = 9900;
    }

    if (rpm == _rpm) {
        _bChanged = false;
    } else {
        _rpm = rpm;
        _bChanged = true;
    }

    return true;
}

bool RPMFragment::getRPMFake()
{
    if (_counter > 0) {
        _counter --;
        _bChanged = false;
        return false;
    }
    int step = 10000 / 40;
    if (_bTita) {
        _rpm += step;
        if (_rpm > 9900) {
            _rpm = 9900;
            _bTita = false;
            _counter = 10;
        }
    } else {
        _rpm -= step;
        if (_rpm < 0) {
            _rpm = 0;
            _bTita = true;
            _counter = 40;
        }
    }
    _bChanged = true;
    return true;
}

bool RPMFragment::readOBDData()
{
    bool got = false;
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            got = getRPMFake();
        }
    } else {
        got = getRPM();
        _bChanged = true;
    }

    return got;
}

void RPMFragment::updateSymbols(bool bValid)
{
    if (!bValid) {
        _pNum_h->SetHiden();

        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", "N.bmp");
        _pNum_d->SetRelativePos(Point(148, 130));

        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", "A.bmp");
        _pNum_c->SetRelativePos(Point(200, 130));

        _pUnit->SetHiden();
        _pRRPM->SetHiden();

        return;
    }

    _pUnit->SetShow();
    _pRRPM->SetShow();

    int num = _rpm / 100;

    if (num >= 99) {
        _pRRPM->setParams(Point(200, 200), 199, 0xE9C5);
    } else if (num >= 40) {
        _pRRPM->setParams(Point(200, 200), 132 + 67 * num / 100.0, 0xE9C5);
    } else {
        _pRRPM->setParams(Point(200, 200), 132 + 67 * num / 100.0, 0xFFE3);
    }

    char name[64];
    if (num >= 100) {
        snprintf(name, 64, "num-144-%d.bmp", num / 100);
        _pNum_h->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_h->SetShow();
        num = num % 100;

        snprintf(name, 64, "num-144-%d.bmp", num / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148 + 26, 130));

        snprintf(name, 64, "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200 + 26, 130));
    } else {
        _pNum_h->SetHiden();

        snprintf(name, 64, "num-144-%d.bmp", num / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148, 130));

        snprintf(name, 64, "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200, 130));
    }
}

bool RPMFragment::checkOBDPaired()
{
    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        agcmd_property_get(propBTTempOBDStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            _bObdPaired = true;
        } else {
            _bObdPaired = false;
        }
    } else {
        _bObdPaired = false;
    }

    agcmd_property_get(PropName_Settings_OBD_Demo, tmp, "off");
    if (strcasecmp(tmp, "on") == 0) {
        _bDemoEnabled = true;
    } else {
        _bDemoEnabled = false;
    }

    return _bObdPaired;
}

void RPMFragment::onResume()
{
    checkOBDPaired();

    //CAMERALOG("%s: _bObdPaired = %d, _bDemoEnabled = %d",
    //    getFragName(), _bObdPaired, _bDemoEnabled);
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            _pLabel->SetRelativePos(Point(133, 260));
            _pLabel_demo->SetRelativePos(Point(133, 282));
            _pLabel_demo->SetShow();
            updateSymbols(true);
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _rpm = 0;
            updateSymbols(false);
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
        updateSymbols(true);
    }
    this->Draw(true);
}

void RPMFragment::onPause()
{
}

bool RPMFragment::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (readOBDData()) {
                    updateSymbols(true);
                } else {
                    updateSymbols(false);
                }

                if (_bChanged) {
                    this->Draw(true);
                    _bChanged = false;
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    checkOBDPaired();
                    if (!_bObdPaired) {
                        if (_bDemoEnabled) {
                            _pLabel->SetRelativePos(Point(133, 260));
                            _pLabel_demo->SetRelativePos(Point(133, 282));
                            _pLabel_demo->SetShow();
                            updateSymbols(true);
                        } else {
                            updateSymbols(false);
                        }
                    } else {
                        _pLabel_demo->SetHiden();
                        _pLabel->SetRelativePos(Point(133, 270));
                        updateSymbols(true);
                    }
                    this->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}


SpeedFragment::SpeedFragment(const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
    , _speed(0)
    , _bChanged(true)
    , _bImperialUnits(true)
    , _bObdPaired(false)
    , _bDemoEnabled(false)
    , _bTita(true)
    , _counter(0)
{
    _pRSpeed = new Round(this, Point(200, 200), 132, 0xFFE3);
    _pRSpeed->SetHiden();

    _pRound = new Round(this, Point(200, 200), 130, 0x18A3);

    _pNum_h = new BmpImage(
        this,
        Point(122, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-0.bmp");
    _pNum_h->SetHiden();

    _pNum_d = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "N.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "A.bmp");

    _pLabel = new BmpImage(
        this,
        Point(133, 270), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-MPH.bmp");

    _pLabel_demo = new BmpImage(
        this,
        Point(133, 282), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-demo.bmp");
    _pLabel_demo->SetHiden();
}

SpeedFragment::~SpeedFragment()
{
    delete _pLabel_demo;
    delete _pLabel;
    delete _pNum_c;
    delete _pNum_d;
    delete _pNum_h;
    delete _pRound;
    delete _pRSpeed;
}

bool SpeedFragment::getSpeed()
{
    int speedKmh = 0;
    bool got = _cp->getSpeedKmh(speedKmh);
    if (!got) {
        _bChanged = false;
        return false;
    }

    if (_bImperialUnits) {
        // convert to mph
        speedKmh = speedKmh / 1.609 + 0.5;
    }

    if (speedKmh == _speed) {
        _bChanged = false;
    } else {
        _speed = speedKmh;
        _bChanged = true;
    }

    return true;
}

bool SpeedFragment::getSpeedFake()
{
    if (_counter > 0) {
        _counter --;
        _bChanged = false;
        return false;
    }
    int step = 200 / 40;
    if (_bTita) {
        _speed += step;
        if (_speed > 200) {
            _speed = 200;
            _bTita = false;
            _counter = 10;
        }
    } else {
        _speed -= step;
        if (_speed < 0) {
            _speed = 0;
            _bTita = true;
            _counter = 40;
        }
    }
#if 0
    int minus = rand() % 2;
    float speed = (minus == 0 ? 1 : (-1)) * rand() % 3;
    if (_speed < 0.0) {
        _speed = 0.0;
    } else if (_speed <= 40) {
        _speed = _speed + 2 * abs(speed);
    } else if (_speed > 110) {
        _speed = _speed - abs(speed);
    } else {
        _speed = _speed + speed;
    }
#endif
    _bChanged = true;
    return true;
}

bool SpeedFragment::readOBDData()
{
    bool got = false;
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            got = getSpeedFake();
        }
    } else {
        got = getSpeed();
    }

    return got;
}

void SpeedFragment::updateSymbols(bool bValid)
{
    if (!bValid) {
        _pNum_h->SetHiden();

        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", "N.bmp");
        _pNum_d->SetRelativePos(Point(148, 130));

        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", "A.bmp");
        _pNum_c->SetRelativePos(Point(200, 130));

        _pRSpeed->SetHiden();

        return;
    }

    _pRSpeed->SetShow();
    if (_speed >= 200) {
        _pRSpeed->setParams(Point(200, 200), 199, 0xE9C5);
    } else if (_speed >= 100) {
        _pRSpeed->setParams(Point(200, 200), 132 + 67 * _speed / 198.0, 0xE9C5);
    } else {
        _pRSpeed->setParams(Point(200, 200), 132 + 67 * _speed / 198.0, 0xFFE3);
    }

    int num = _speed;
    char name[64];
    if (num >= 100) {
        snprintf(name, 64, "num-144-%d.bmp", num / 100);
        _pNum_h->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_h->SetShow();
        num = num % 100;

        snprintf(name, 64, "num-144-%d.bmp", num / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148 + 26, 130));

        snprintf(name, 64, "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200 + 26, 130));
    } else {
        _pNum_h->SetHiden();

        snprintf(name, 64, "num-144-%d.bmp", num / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148, 130));

        snprintf(name, 64, "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200, 130));
    }
}

bool SpeedFragment::checkOBDPaired()
{
    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        agcmd_property_get(propBTTempOBDStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            _bObdPaired = true;
        } else {
            _bObdPaired = false;
        }
    } else {
        _bObdPaired = false;
    }

    agcmd_property_get(PropName_Settings_OBD_Demo, tmp, "off");
    if (strcasecmp(tmp, "on") == 0) {
        _bDemoEnabled = true;
    } else {
        _bDemoEnabled = false;
    }

    return _bObdPaired;
}

void SpeedFragment::onResume()
{
    char tmp[256];
    agcmd_property_get(PropName_Units_System, tmp, Imperial_Units_System);
    if (strcasecmp(tmp, Imperial_Units_System) == 0) {
        _bImperialUnits = true;
        _pLabel->setSource("/usr/local/share/ui/BMP/gauge/", "label-MPH.bmp");
    } else {
        _bImperialUnits = false;
        _pLabel->setSource("/usr/local/share/ui/BMP/gauge/", "label-KPH.bmp");
    }

    checkOBDPaired();

    //CAMERALOG("%s: _bObdPaired = %d, _bDemoEnabled = %d",
    //    getFragName(), _bObdPaired, _bDemoEnabled);
    if (!_bObdPaired) {
        if (_bDemoEnabled) {
            _pLabel->SetRelativePos(Point(133, 260));
            _pLabel_demo->SetRelativePos(Point(133, 282));
            _pLabel_demo->SetShow();
            updateSymbols(true);
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _speed = 0;
            updateSymbols(false);
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
        updateSymbols(true);
    }
    this->Draw(true);
}

void SpeedFragment::onPause()
{
}

bool SpeedFragment::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (readOBDData()) {
                    updateSymbols(true);
                } else {
                    updateSymbols(false);
                }

                if (_bChanged) {
                    this->Draw(true);
                    _bChanged = false;
                }
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_bluetooth) {
                    checkOBDPaired();
                    if (!_bObdPaired) {
                        if (_bDemoEnabled) {
                            _pLabel->SetRelativePos(Point(133, 260));
                            _pLabel_demo->SetRelativePos(Point(133, 282));
                            _pLabel_demo->SetShow();
                            updateSymbols(true);
                        } else {
                            updateSymbols(false);
                        }
                    } else {
                        _pLabel_demo->SetHiden();
                        _pLabel->SetRelativePos(Point(133, 270));
                        updateSymbols(true);
                    }
                    this->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}


LiveViewFragment::LiveViewFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
    , _counter(0)
    , _bFullScreen(false)
    , _TempratureStat(TempratureStat_Normal)
{
    _pRPreview = new Round(this, Point(200, 200), 200, Color_Video_Transparency);
    _pBorder = new Circle(this, Point(200, 200), 199, 1, 0x00);

    _pRecStatus = new BmpImage(
        this,
        Point(188, 8), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-tf_card.bmp");

    memset(_txt, 0x00, 64);
    _pTxt = new StaticText(this, Point(100, 40), Size(200, 32));
    _pTxt->SetStyle(24, FONT_ROBOTOMONO_Medium, Color_White, CENTER, MIDDLE);
    _pTxt->SetText((UINT8 *)_txt);

    _pWifiStatus = new BmpImage(
        this,
        Point(132, 328), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-wifi-0.bmp");

    _pGPS = new BmpImage(
        this,
        Point(156, 328), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-GPS-off.bmp");

    _pOBDStatus = new BmpImage(
        this,
        Point(180, 328), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-obd-off.bmp");

    _pRemoteStatus = new BmpImage(
        this,
        Point(204, 328), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-remote-off.bmp");

    _pBatteryStatus = new BmpImage(
        this,
        Point(228, 328), Size(40, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-battery-40.bmp");

    _pSpkMute = new BmpImage(
        this,
        Point(204, 328), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-silent-on.bmp");
    _pSpkMute->SetHiden();

    _pMicMute = new BmpImage(
        this,
        Point(228, 328), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-mute-on.bmp");
    _pMicMute->SetHiden();

    _pAlertTemp = new BmpImage(
        this,
        Point(300, 328), Size(24, 24),
        "/usr/local/share/ui/BMP/liveview/",
        "status-thermal-cold.bmp");
    _pAlertTemp->SetHiden();
}

LiveViewFragment::~LiveViewFragment()
{
    delete _pAlertTemp;
    delete _pSpkMute;
    delete _pMicMute;
    delete _pBatteryStatus;
    delete _pOBDStatus;
    delete _pRemoteStatus;
    delete _pGPS;
    delete _pWifiStatus;
    delete _pTxt;
    delete _pRecStatus;
    delete _pBorder;
    delete _pRPreview;
}

void LiveViewFragment::updateRecTime()
{
    camera_State state = _cp->getRecState();
    storage_State st = storage_State_ready;
    storage_rec_state tf_status = _cp->getStorageRecState();

    if (camera_State_stop == state) {
        // only read state from property when it is needed
        st = _cp->getStorageState();
    }

    // TODO: read record mode
    const char *rec_icon = "status-recording-loop.bmp";

    // Firstly, update record icon
    if (_bFullScreen) {
        _pRecStatus->SetHiden();
        _pTxt->SetHiden();
    } else {
        _pRecStatus->SetShow();
        _pTxt->SetShow();
    }
    if ((camera_State_record == state)
        || (camera_State_marking == state))
    {
        if (storage_rec_state_WriteSlow == tf_status) {
            const char *tf_icon = "status-tf_card-slow.bmp";
            if (_counter % 2) {
                _pRecStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    rec_icon);
            } else {
                _pRecStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    tf_icon);
            }
        } else if (storage_rec_state_DiskError == tf_status) {
            const char *tf_icon = "status-tf_card-error.bmp";
            if (_counter % 2) {
                _pRecStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    rec_icon);
            } else {
                _pRecStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    tf_icon);
            }
        } else {
            if (_bFullScreen) {
                rec_icon = "status-recording-loop-alpha.bmp";
            }
            _pRecStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                rec_icon);
            if (_counter % 2) {
                _pRecStatus->SetHiden();
            } else {
                _pRecStatus->SetShow();
            }
        }
    }else if ((camera_State_starting == state)
        || (camera_State_stopping == state))
    {
        _pRecStatus->setSource(
            "/usr/local/share/ui/BMP/liveview/",
            rec_icon);
    } else {
        const char *tf_icon = "status-tf_card.bmp";
        if (storage_rec_state_DiskError == tf_status) {
            tf_icon = "status-tf_card-error.bmp";
        } else if (storage_rec_state_WriteSlow == tf_status) {
            tf_icon = "status-tf_card-slow.bmp";
        } else {
            if (storage_State_noMedia == st) {
                tf_icon = "status-tf_card-missing.bmp";
            }
        }
        _pRecStatus->setSource(
            "/usr/local/share/ui/BMP/liveview/",
            tf_icon);
    }

    // Secondly, update text
    if (camera_State_starting == state) {
        snprintf(_txt, 64, "%s...", "Starting");
    } else if (camera_State_stopping == state) {
        snprintf(_txt, 64, "%s...", "Stopping");
    } else if (camera_State_Error == state) {
        snprintf(_txt, 64, "%s", "Error");
    } else if (camera_State_record == state) {
        UINT64 tt = _cp->getRecordingTime();
        UINT64 hh =  tt /3600;
        UINT64 mm = (tt % 3600) /60;
        UINT64 ss = (tt % 3600) %60;
        int j = 0;
        j = snprintf(_txt, 64, "%02lld:", hh);
        j += snprintf(_txt + j, 64 - j, "%02lld:", mm);
        j += snprintf(_txt + j, 64 - j, "%02lld", ss);
    } else if (camera_State_marking == state) {
        UINT64 tt = _cp->getRecordingTime();
        UINT64 hh =  tt /3600;
        UINT64 mm = (tt % 3600) /60;
        UINT64 ss = (tt % 3600) %60;
        int j = 0;
        j = snprintf(_txt, 64, "Marking %02lld:", hh);
        j += snprintf(_txt + j, 64 - j, "%02lld:", mm);
        j += snprintf(_txt + j, 64 - j, "%02lld", ss);
    } else if (camera_State_stop == state){
        if (storage_rec_state_DiskError == tf_status) {
            snprintf(_txt, 64, "%s", "Card Error");
        } else {
                switch (st) {
                    case storage_State_ready:
                    {
                        int free = 0, total = 0;
                        int spaceMB = _cp->getFreeSpaceMB(free, total);
                        int bitrateKBS = _cp->getBitrate() / 8 / 1000;
                        if (bitrateKBS > 0) {
                            int time_sec = spaceMB * 1000 / bitrateKBS;
                            if (time_sec < 60) {
                                snprintf(_txt, 64, "%s", "<1min");
                            } else {
                                snprintf(_txt, 64, "%02dh%02dm",
                                    time_sec / 3600, (time_sec % 3600) / 60);
                            }
                        } else {
                            if (spaceMB > 1000) {
                                snprintf(_txt, 64, "%d.%dG",
                                    spaceMB / 1000, (spaceMB % 1000) / 100);
                            } else {
                                if (spaceMB >= 1) {
                                    snprintf(_txt, 64, "%dM", spaceMB);
                                } else {
                                    snprintf(_txt, 64, "%s", "<1MB");
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
        }
    }
}

void LiveViewFragment::updateBattery()
{
    if (_bFullScreen) {
        _pBatteryStatus->SetHiden();
        return;
    } else {
        _pBatteryStatus->SetShow();
    }

    const char *Prop_Has_Battery = "temp.power_supply.present";
    const char *Prop_External_Power = "temp.power_supply.online";
    const char *Prop_Charge_Type = "temp.power_supply.charge_type";
    const char *Prop_Power_Status = "temp.power_supply.status";
    const char *Prop_Current_Avg = "temp.power_supply.current_avg";
    const char *Prop_Capacity = "temp.power_supply.capacity";
    const char *Prop_Health = "temp.power_supply.health";

    char tmp[64];
    agcmd_property_get(Prop_Has_Battery, tmp, "1");
    if (strcasecmp(tmp, "1") == 0) {
        agcmd_property_get(Prop_External_Power, tmp, "1");
        if (strcasecmp(tmp, "1") == 0) {
            agcmd_property_get(Prop_Current_Avg, tmp, "0");
            int current_avg = atoi(tmp);

            char health[64];
            agcmd_property_get(Prop_Health, health, "Good");

            char charge_type[64];
            agcmd_property_get(Prop_Charge_Type, charge_type, "N/A");

            agcmd_property_get(Prop_Capacity, tmp, "0");
            int capacity = strtoul(tmp, NULL, 0);

            char status[64];
            agcmd_property_get(Prop_Power_Status, status, "Discharging");

            if (current_avg < 0) {
                if ((strcasecmp(status, "Full") == 0) || (capacity >= 100)) {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-draining-100.bmp");
                } else if(capacity >= 60) {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-draining-80.bmp");
                } else if(capacity >= 40) {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-draining-60.bmp");
                } else if(capacity >= 20) {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-draining-40.bmp");
                } else {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-draining-20.bmp");
                }
            } else if ((current_avg == 0) &&
                ((strcasecmp(health, "Overheat") == 0) || (strcasecmp(health, "Cold") == 0)))
            {
                if ((strcasecmp(status, "Full") == 0) || (capacity >= 100)) {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-thermal-100.bmp");
                } else if(capacity >= 60) {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-thermal-80.bmp");
                } else if(capacity >= 40) {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-thermal-60.bmp");
                } else if(capacity >= 20) {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-thermal-40.bmp");
                } else {
                    _pBatteryStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-battery-thermal-20.bmp");
                }
            } else {
                if (current_avg == 0) {
                    // Trickle charge
                    if ((strcasecmp(status, "Full") == 0) || (capacity >= 100)) {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-100.bmp");
                    } else if(capacity >= 60) {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-trickle-80.bmp");
                    } else if(capacity >= 40) {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-trickle-60.bmp");
                    } else if(capacity >= 20) {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-trickle-40.bmp");
                    } else {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-trickle-20.bmp");
                    }
                } else {
                    if ((strcasecmp(status, "Full") == 0) || (capacity >= 100)) {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-100.bmp");
                    } else if(capacity >= 60) {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-charging-80.bmp");
                    } else if(capacity >= 40) {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-charging-60.bmp");
                    } else if(capacity >= 20) {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-charging-40.bmp");
                    } else {
                        _pBatteryStatus->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-battery-charging-20.bmp");
                    }
                }
            }
        } else {
            // No external power supply
            agcmd_property_get(Prop_Capacity, tmp, "0");
            int rt = strtoul(tmp, NULL, 0);
            agcmd_property_get(Prop_Power_Status, tmp, "Discharging");
            if ((strcasecmp(tmp, "Full") == 0) || (rt >= 100)) {
                _pBatteryStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    "status-battery-100.bmp");
            } else if(rt >= 60) {
                _pBatteryStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    "status-battery-80.bmp");
            } else if(rt >= 40) {
                _pBatteryStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    "status-battery-60.bmp");
            } else if(rt >= 20) {
                _pBatteryStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    "status-battery-40.bmp");
            } else {
                _pBatteryStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    "status-battery-20.bmp");
            }
        }
    } else {
        // No battery
        _pBatteryStatus->setSource(
            "/usr/local/share/ui/BMP/liveview/",
            "status-battery-none.bmp");
    }
}

void LiveViewFragment::updateWifi()
{
    if (_bFullScreen) {
        _pWifiStatus->SetHiden();
        return;
    } else {
        _pWifiStatus->SetShow();
    }

    wifi_mode mode = _cp->getWifiMode();
    switch (mode)
    {
        case wifi_mode_Client:
            char ssid[32];
            _cp->GetWifiCltinfor(ssid, 32);
            if ((strcmp(ssid, "Empty List") == 0)
                || (strcmp(ssid, "Searching") == 0))
            {
                _pWifiStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    "status-wifi-0.bmp");
            } else {
                int signal = _cp->GetWifiCltSignal();
                if (signal < -70) {
                    _pWifiStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-wifi-50.bmp");
                } else if (signal < -55) {
                    _pWifiStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-wifi-75.bmp");
                } else {
                    _pWifiStatus->setSource(
                        "/usr/local/share/ui/BMP/liveview/",
                        "status-wifi-100.bmp");
                }
            }
            break;
        case wifi_mode_AP:
        case wifi_mode_Multirole:
            _pWifiStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-wifi-hotspot-1.bmp");
            break;
        case wifi_mode_Off:
            _pWifiStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-wifi-0.bmp");
            break;
        default:
            break;
    }
}

void LiveViewFragment::updateBluetooth()
{
    if (_bFullScreen) {
        _pRemoteStatus->SetHiden();
        _pOBDStatus->SetHiden();
        return;
    } else {
        _pRemoteStatus->SetShow();
        _pOBDStatus->SetShow();
    }

    char tmp[256];
    agcmd_property_get(propBTTempStatus, tmp, "");
    if (strcasecmp(tmp, "on") == 0) {
        agcmd_property_get(propBTTempOBDStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            _pOBDStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-obd-on.bmp");
        } else if (strcasecmp(tmp, "NS") == 0) {
            _pOBDStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-obd-unsupported.bmp");
        } else {
            _pOBDStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-obd-off.bmp");
        }

        agcmd_property_get(propBTTempHIDStatus, tmp, "");
        if (strcasecmp(tmp, "on") == 0) {
            agcmd_property_get(Prop_RC_Battery_level, tmp, "0");
            int batt_level = atoi(tmp);
            if (batt_level >= RC_Batt_Warning_Level) {
                _pRemoteStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    "status-remote-on.bmp");
            } else {
                _pRemoteStatus->setSource(
                    "/usr/local/share/ui/BMP/liveview/",
                    "status-remote-lowbattery.bmp");
            }
        } else {
            _pRemoteStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-remote-off.bmp");
        }
    } else {
        _pOBDStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-obd-off.bmp");
        _pRemoteStatus->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-remote-off.bmp");
    }
}

void LiveViewFragment::updateGPS()
{
    if (_bFullScreen) {
        _pGPS->SetHiden();
        return;
    } else {
        _pGPS->SetShow();
    }

    gps_state state = _cp->getGPSState();
    switch (state)
    {
        case gps_state_ready: {
            struct aggps_client_info_s  *paggps_client = aggps_open_client();
            int ret_val = aggps_read_client_tmo(paggps_client, 10);
            if (ret_val == 0) {
                if (paggps_client->pgps_info->devinfo.output_rate_div) {
                    float update_rate = 1;
                    update_rate = paggps_client->pgps_info->devinfo.output_rate_mul;
                    update_rate /= paggps_client->pgps_info->devinfo.output_rate_div;
                    if (update_rate >= 10.0) {
                        _pGPS->setSource(
                            "/usr/local/share/ui/BMP/liveview/",
                            "status-GPS-on.bmp");
                        if (paggps_client) {
                            aggps_close_client(paggps_client);
                        }
                        return;
                    }
                }
            }
            if (paggps_client) {
                aggps_close_client(paggps_client);
            }
            _pGPS->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-GPS-weak.bmp");
            break;
        }
        case gps_state_on:
        case gps_state_off:
            _pGPS->setSource(
                "/usr/local/share/ui/BMP/liveview/",
                "status-GPS-off.bmp");
            break;
        default:
            break;
    }
}

void LiveViewFragment::updateMicSpk()
{
    if (_bFullScreen) {
        return;
    }

    bool spkerMute = false, micMute = false;
    char tmp[256];
    agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
    if (strcasecmp(tmp, "Mute") == 0) {
        spkerMute = true;
    }

    agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
    if (strcasecmp(tmp, "Mute") == 0) {
        micMute = true;
    }

    if (spkerMute && micMute) {
        _pWifiStatus->SetRelativePos(Point(108, 328));
        _pGPS->SetRelativePos(Point(132, 328));
        _pOBDStatus->SetRelativePos(Point(156, 328));
        _pRemoteStatus->SetRelativePos(Point(180, 328));
        _pMicMute->SetShow();
        _pMicMute->SetRelativePos(Point(204, 328));
        _pSpkMute->SetShow();
        _pSpkMute->SetRelativePos(Point(228, 328));
        _pBatteryStatus->SetRelativePos(Point(252, 328));
    } else if (spkerMute) {
        _pWifiStatus->SetRelativePos(Point(120, 328));
        _pGPS->SetRelativePos(Point(144, 328));
        _pOBDStatus->SetRelativePos(Point(168, 328));
        _pRemoteStatus->SetRelativePos(Point(192, 328));
        _pMicMute->SetHiden();
        _pSpkMute->SetShow();
        _pSpkMute->SetRelativePos(Point(216, 328));
        _pBatteryStatus->SetRelativePos(Point(240, 328));
    } else if (micMute) {
        _pWifiStatus->SetRelativePos(Point(120, 328));
        _pGPS->SetRelativePos(Point(144, 328));
        _pOBDStatus->SetRelativePos(Point(168, 328));
        _pRemoteStatus->SetRelativePos(Point(192, 328));
        _pMicMute->SetShow();
        _pMicMute->SetRelativePos(Point(216, 328));
        _pSpkMute->SetHiden();
        _pBatteryStatus->SetRelativePos(Point(240, 328));
    } else {
        _pWifiStatus->SetRelativePos(Point(132, 328));
        _pGPS->SetRelativePos(Point(156, 328));
        _pOBDStatus->SetRelativePos(Point(180, 328));
        _pRemoteStatus->SetRelativePos(Point(204, 328));
        _pMicMute->SetHiden();
        _pSpkMute->SetHiden();
        _pBatteryStatus->SetRelativePos(Point(228, 328));
    }
}

void LiveViewFragment::updateTempAlert()
{
    if ((_counter / 10) % 2) {
        _pAlertTemp->SetShow();
    } else {
        _pAlertTemp->SetHiden();
    }
}

void LiveViewFragment::checkTempratureStatus()
{
    int temp_warning = 0;
    char tmp[256];
    agcmd_property_get(PropName_Board_Temp_warning, tmp, Warning_default);
    temp_warning = atoi(tmp);

    agcmd_property_get(PropName_Board_Temprature, tmp, "450");
    int board_temprature = atoi(tmp);

    if (board_temprature > temp_warning) {
        _TempratureStat = TempratureStat_Overheat;
    } else {
        _TempratureStat = TempratureStat_Normal;
    }
}

void LiveViewFragment::onResume()
{
    _bFullScreen = _cp->isVideoFullScreen();
    _counter = 0;
    checkTempratureStatus();

    updateRecTime();
    updateBattery();
    updateWifi();
    updateBluetooth();
    updateGPS();
    updateMicSpk();
    this->Draw(true);
}

void LiveViewFragment::onPause()
{
}

bool LiveViewFragment::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_counter % 50 == 0) {
                    updateGPS();
                }
                if (_counter % 100 == 0) {
                    updateWifi();
                }

                if (_TempratureStat == TempratureStat_Overheat) {
                    if (_counter % 10 == 0) {
                        updateTempAlert();
                        this->Draw(true);
                    }
                } else {
                    _pAlertTemp->SetHiden();
                }

                camera_State state = _cp->getRecState();
                if ((camera_State_record == state)
                    || (camera_State_marking == state)
                    || (camera_State_starting == state)
                    || (camera_State_stopping == state)
                    || (camera_State_Error == state))
                {
                    if (_counter % 5 == 0) {
                        updateRecTime();
                        this->Draw(true);
                    }
                } else {
                    if (_counter % 50 == 0) {
                        updateRecTime();
                        this->Draw(true);
                    }
                }

                // check temprature every minute
                if (_counter % (20 * 60) == 0) {
                    checkTempratureStatus();
                }

                _counter++;
            } else if (event->_event == InternalEvent_app_state_update) {
                if (event->_paload == camera_State_starting) {
                    updateRecTime();
                    this->Draw(true);
                } else if (event->_paload == camera_State_stopping) {
                    updateRecTime();
                    this->Draw(true);
                } else if (event->_paload == camera_State_stop) {
                    updateRecTime();
                    this->Draw(true);
                } else if (event->_paload == camera_State_Storage_changed) {
                    updateRecTime();
                    this->Draw(true);
                } else if (event->_paload == camera_State_Storage_full) {
                    updateRecTime();
                    this->Draw(true);
                }
                b = false;
            } else if (event->_event ==InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    updateRecTime();
                    this->Draw(true);
                    b = false;
                } else if (event->_paload == PropertyChangeType_bluetooth) {
                    updateBluetooth();
                    this->Draw(true);
                    b = false;
                } else if (event->_paload == PropertyChangeType_Wifi) {
                    updateWifi();
                    this->Draw(true);
                    b = false;
                } else if (event->_paload == PropertyChangeType_audio) {
                    updateMicSpk();
                    this->Draw(true);
                    b = false;
                } else if (event->_paload == PropertyChangeType_power) {
                    updateBattery();
                }
            }
            break;
        case eventType_touch:
            if (event->_event == TouchEvent_DoubleClick) {
                _bFullScreen = !_bFullScreen;
                if (_bFullScreen) {
                    _pRecStatus->SetHiden();
                    _pTxt->SetHiden();
                    _pWifiStatus->SetHiden();
                    _pGPS->SetHiden();
                    _pRemoteStatus->SetHiden();
                    _pOBDStatus->SetHiden();
                    _pBatteryStatus->SetHiden();
                    _pSpkMute->SetHiden();
                    _pMicMute->SetHiden();
                    this->Draw(true);
                }
                bool res = _cp->SetFullScreen(_bFullScreen);
                if (res) {
                    if (_bFullScreen) {
                        _pRecStatus->SetHiden();
                        _pTxt->SetHiden();
                        _pWifiStatus->SetHiden();
                        _pGPS->SetHiden();
                        _pRemoteStatus->SetHiden();
                        _pOBDStatus->SetHiden();
                        _pBatteryStatus->SetHiden();
                        _pSpkMute->SetHiden();
                        _pMicMute->SetHiden();
                    } else {
                        _pRecStatus->SetShow();
                        _pTxt->SetShow();
                        _pWifiStatus->SetShow();
                        _pGPS->SetShow();
                        _pRemoteStatus->SetShow();
                        _pOBDStatus->SetShow();
                        _pBatteryStatus->SetShow();
                        updateMicSpk();
                    }
                    this->Draw(true);
                } else {
                    _bFullScreen = !_bFullScreen;
                }
                b = false;
            }
            break;
    }

    return b;
}


PitchRollFragment::PitchRollFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , CThread("PitchRollFragment_thread", CThread::NORMAL , 0, NULL)
    , _cp(cp)
    , _angle_p(0.0)
    , _angle_r(0.0)
    , _bChanged(true)
    , _bRotate180(false)
    , _numTimerEvent(0)
{
    _bRotate180 = _isRotate180();

    _pHLine = new UILine(this, Point(0, 200), Size(2, 400));
    _pHLine->setLine(Point(0, 200), Point(400, 200), 4, 0x18A3);

    _pCap = new RoundCap(this, Point(200, 200), 199, 0xFFE3);
    _pCap->setAngle(0.0);
    _pCap->setRatio(1.0);

    _pRound = new Round(this, Point(200, 200), 130, 0x18A3);

    _pSymPitch = new BmpImage(
        this,
        Point(153, 169), Size(14, 27),
        "/usr/local/share/ui/BMP/gauge/",
        "label-p.bmp");

    _pPitchPlus = new BmpImage(
        this,
        Point(179, 174), Size(11, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-plus-inactive.bmp");

    _pPitchMinus = new BmpImage(
        this,
        Point(179, 185), Size(11, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-minus-inactive.bmp");

    _pPitchNum_h = new BmpImage(
        this,
        Point(200, 146), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-2.bmp");

    _pPitchNum_d = new BmpImage(
        this,
        Point(200, 146), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-2.bmp");

    _pPitchNum_c = new BmpImage(
        this,
        Point(222, 146), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-5.bmp");

    _pPitchSymDeg = new BmpImage(
        this,
        Point(244, 146), Size(10, 10),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-degree.bmp");

    _pSymRoll = new BmpImage(
        this,
        Point(153, 227), Size(14, 27),
        "/usr/local/share/ui/BMP/gauge/",
        "label-r.bmp");

    _pRollPlus = new BmpImage(
        this,
        Point(179, 232), Size(11, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-plus-inactive.bmp");

    _pRollMinus = new BmpImage(
        this,
        Point(179, 243), Size(11, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-minus-inactive.bmp");

    _pRollNum_h = new BmpImage(
        this,
        Point(200, 204), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-2.bmp");
    _pRollNum_h->SetHiden();

    _pRollNum_d = new BmpImage(
        this,
        Point(200, 204), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-2.bmp");

    _pRollNum_c = new BmpImage(
        this,
        Point(222, 204), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-5.bmp");

    _pRollSymDeg = new BmpImage(
        this,
        Point(244, 204), Size(10, 10),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-degree.bmp");

    _pTitle = new BmpImage(
        this,
        Point(133, 270), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-pitch_roll.bmp");

    _pWaitEvent = new CSemaphore(0, 1, "pitchroll sem");
    _pLock = new CMutex("pitchroll mutex");
    this->Go();
}

PitchRollFragment::~PitchRollFragment()
{
    delete _pLock;
    delete _pWaitEvent;
    delete _pTitle;
    delete _pRollSymDeg;
    delete _pRollNum_c;
    delete _pRollNum_d;
    delete _pRollNum_h;
    delete _pRollMinus;
    delete _pRollPlus;
    delete _pSymRoll;
    delete _pPitchSymDeg;
    delete _pPitchNum_c;
    delete _pPitchNum_d;
    delete _pPitchNum_h;
    delete _pPitchMinus;
    delete _pPitchPlus;
    delete _pSymPitch;
    delete _pRound;
    delete _pCap;
    delete _pHLine;
}

bool PitchRollFragment::_isRotate180()
{
    bool bRotate = false;
    char horizontal[256];
    char vertical[256];
    memset(horizontal, 0x00, 256);
    memset(vertical, 0x00, 256);
    agcmd_property_get(PropName_Rotate_Horizontal, horizontal, "0");
    agcmd_property_get(PropName_Rotate_Vertical, vertical, "0");
    if ((strcasecmp(horizontal, "0") == 0)
        && (strcasecmp(vertical, "0") == 0))
    {
        bRotate = false;
    } else if ((strcasecmp(horizontal, "1") == 0)
        && (strcasecmp(vertical, "1") == 0))
    {
        bRotate = true;
    }

    return bRotate;
}

bool PitchRollFragment::getPitchRollAngle()
{
    float roll, pitch, heading;

    bool got = _cp->getOrientation(heading, roll, pitch);
    if (!got) {
        return false;
    }

    bool pChanged = false;
    float diff_abs_p = (_angle_p > pitch) ? (_angle_p - pitch) : (pitch - _angle_p);
    if (diff_abs_p < 0.1) {
        pChanged = false;
    } else {
        _angle_p = pitch;
        pChanged = true;
    }

    // map scope [0, 360] to [-180, 180]
    if (_bRotate180) {
        roll = roll - 180.0;
    } else {
        if (roll > 180.0) {
            roll = roll - 360.0;
        }
    }

    float diff_abs = (_angle_r > roll) ? (_angle_r - roll) : (roll - _angle_r);
    bool rChanged = false;
    if (diff_abs < 0.1) {
        rChanged = false;
    } else {
        _angle_r = roll;
        rChanged = true;
    }

    _bChanged = pChanged || rChanged;
    return true;
}

void PitchRollFragment::updateSymbols()
{
    bool got = getPitchRollAngle();
    if (!got) {
        return;
    }

    if (!_bChanged) {
        return;
    }

    if (_angle_p > 180.0) {
        _angle_p = 180.0;
    } else if (_angle_p < -180.0) {
        _angle_p = -180.0;
    }

    float ratio = 1.0;
    if (_angle_p >= 90.0) {
        ratio = (270.0 - _angle_p) / 90.0;
    } else if (_angle_p >= -90.0) {
        ratio = 1.0 + _angle_p / 90.0;
    } else {
        ratio = -1.0 * _angle_p / 90.0 - 1;
    }
    _pCap->setRatio(ratio);
    _pCap->setAngle(_angle_r);


    int angle_pitch = _angle_p;
    if (_bRotate180) {
        if (_angle_p < 0.0) {
            angle_pitch = -180.0 - _angle_p;
        } else if (_angle_p > 0.0) {
            angle_pitch = 180.0 - _angle_p;
        }
    }
    if (angle_pitch == 0) {
        _pPitchPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-plus-inactive.bmp");
        _pPitchMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-minus-inactive.bmp");
    } else if (angle_pitch > 0) {
        _pPitchPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-plus-active.bmp");
        _pPitchMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-minus-inactive.bmp");
    } else {
        angle_pitch = -1 * angle_pitch;
        _pPitchPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-plus-inactive.bmp");
        _pPitchMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-minus-active.bmp");
    }

    char name[64];
    if (angle_pitch < 10) {
        _pPitchNum_h->SetHiden();

        _pPitchNum_d->SetRelativePos(Point(200, 146));
        _pPitchNum_d->setSource("/usr/local/share/ui/BMP/gauge/",
            "num-64-0_alt.bmp");

        snprintf(name, 64, "num-64-%d.bmp", angle_pitch % 10);
        _pPitchNum_c->SetRelativePos(Point(222, 146));
        _pPitchNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);

        _pPitchSymDeg->SetRelativePos(Point(244, 146));
    } else if (angle_pitch < 100) {
        _pPitchNum_h->SetHiden();

        snprintf(name, 64, "num-64-%d.bmp", angle_pitch / 10);
        _pPitchNum_d->SetRelativePos(Point(200, 146));
        _pPitchNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);

        snprintf(name, 64, "num-64-%d.bmp", angle_pitch % 10);
        _pPitchNum_c->SetRelativePos(Point(222, 146));
        _pPitchNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);

        _pPitchSymDeg->SetRelativePos(Point(244, 146));
    } else {
        snprintf(name, 64, "num-64-%d.bmp", angle_pitch / 100);
        _pPitchNum_h->SetShow();
        _pPitchNum_h->SetRelativePos(Point(200, 146));
        _pPitchNum_h->setSource("/usr/local/share/ui/BMP/gauge/", name);

        angle_pitch = angle_pitch % 100;
        snprintf(name, 64, "num-64-%d.bmp", angle_pitch / 10);
        _pPitchNum_d->SetRelativePos(Point(200 + 22, 146));
        _pPitchNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);

        snprintf(name, 64, "num-64-%d.bmp", angle_pitch % 10);
        _pPitchNum_c->SetRelativePos(Point(222 + 22, 146));
        _pPitchNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);

        _pPitchSymDeg->SetRelativePos(Point(244 + 22, 146));
    }


    int angle_roll = _angle_r;

    if (angle_roll == 0) {
        _pRollPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-plus-inactive.bmp");
        _pRollMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-minus-inactive.bmp");
    } else if (angle_roll > 0) {
        _pRollPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-plus-active.bmp");
        _pRollMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-minus-inactive.bmp");
    } else {
        angle_roll = -1 * angle_roll;
        _pRollPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-plus-inactive.bmp");
        _pRollMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-minus-active.bmp");
    }

    if (angle_roll < 10) {
        _pRollNum_h->SetHiden();

        _pRollNum_d->SetRelativePos(Point(200, 204));
        _pRollNum_d->setSource("/usr/local/share/ui/BMP/gauge/",
            "num-64-0_alt.bmp");

        snprintf(name, 64, "num-64-%d.bmp", angle_roll % 10);
        _pRollNum_c->SetRelativePos(Point(222, 204));
        _pRollNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);

        _pRollSymDeg->SetRelativePos(Point(244, 204));
    } else if (angle_roll < 100) {
        _pRollNum_h->SetHiden();

        snprintf(name, 64, "num-64-%d.bmp", angle_roll / 10);
        _pRollNum_d->SetRelativePos(Point(200, 204));
        _pRollNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);

        snprintf(name, 64, "num-64-%d.bmp", angle_roll % 10);
        _pRollNum_c->SetRelativePos(Point(222, 204));
        _pRollNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);

        _pRollSymDeg->SetRelativePos(Point(244, 204));
    } else {
        snprintf(name, 64, "num-64-%d.bmp", angle_roll / 100);
        _pRollNum_h->SetShow();
        _pRollNum_h->SetRelativePos(Point(200, 204));
        _pRollNum_h->setSource("/usr/local/share/ui/BMP/gauge/", name);

        angle_roll = angle_roll % 100;
        snprintf(name, 64, "num-64-%d.bmp", angle_roll / 10);
        _pRollNum_d->SetRelativePos(Point(200 + 22, 204));
        _pRollNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);

        snprintf(name, 64, "num-64-%d.bmp", angle_roll % 10);
        _pRollNum_c->SetRelativePos(Point(222 + 22, 204));
        _pRollNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);

        _pRollSymDeg->SetRelativePos(Point(244 + 22, 204));
    }
}

void PitchRollFragment::onResume()
{
    _bRotate180 = _isRotate180();
    updateSymbols();
    this->Draw(true);
    this->startTimer(1, true);
}

void PitchRollFragment::onPause()
{
    this->cancelTimer();
}

bool PitchRollFragment::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_rotation) {
                    _bRotate180 = _isRotate180();
                    updateSymbols();
                    this->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}

void PitchRollFragment::onTimerEvent()
{
    _pLock->Take();
    _numTimerEvent++;
    _pWaitEvent->Give();
    _pLock->Give();
}

void PitchRollFragment::main(void *)
{
    while (true) {
        _pWaitEvent->Take(-1);

        {
            bool bWaiting = false;
            _pLock->Take();
            bWaiting = _numTimerEvent > 0;
            _pLock->Give();
            if (bWaiting > 0) {
                _numTimerEvent--;

                updateSymbols();
                if (_bChanged) {
                    this->Draw(true);
                    _bChanged = false;
#if 0
                    static struct timeval last_tv;
                    static int counter = 0;
                    struct timeval cur_tv;
                    gettimeofday(&cur_tv, NULL);
                    float tm = cur_tv.tv_sec - last_tv.tv_sec
                        + (cur_tv.tv_usec - last_tv.tv_usec) / (float)1000000;
                    counter ++;
                    if (tm >= 1.0) {
                        CAMERALOG("%s: %d frames updated during %.2fs, fps = %.2f",
                            this->getWndName(), counter, tm, counter / tm);
                        counter = 0;
                        last_tv.tv_sec = cur_tv.tv_sec;
                        last_tv.tv_usec = cur_tv.tv_usec;
                    }
#endif
                }
            }
        }
    }
}


PitchFragment::PitchFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
  : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
  , CThread("PitchFragment_thread", CThread::NORMAL , 0, NULL)
  , _cp(cp)
  , _angle_p(0.0)
  , _bChanged(false)
  , _bRotate180(false)
  , _numTimerEvent(0)
{
    _bRotate180 = _isRotate180();

    _pHLine = new UILine(this, Point(0, 200), Size(2, 400));
    _pHLine->setLine(Point(0, 200), Point(400, 200), 4, 0x18A3);

    _pCap = new RoundCap(this, Point(200, 200), 199, 0xFFE3);
    _pCap->setAngle(0.0);
    _pCap->setRatio(1.0);

    _pRound = new Round(this, Point(200, 200), 130, 0x18A3);

    _pPlus = new BmpImage(
        this,
        Point(118, 178), Size(22, 22),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-144-plus-active.bmp");

    _pMinus = new BmpImage(
        this,
        Point(118, 200), Size(22, 22),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-144-minus-inactive.bmp");

    _pNum_h = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-0.bmp");

    _pNum_d = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-2.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-5.bmp");

    _pSymDeg = new BmpImage(
        this,
        Point(252, 144), Size(14, 14),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-144-degree.bmp");

    _pTitle = new BmpImage(
        this,
        Point(133, 270), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-pitch.bmp");

    _pWaitEvent = new CSemaphore(0, 1, "pitchroll sem");
    _pLock = new CMutex("pitchroll mutex");
    this->Go();
}

PitchFragment::~PitchFragment()
{
    this->cancelTimer();
    this->Stop();

    delete _pLock;
    delete _pWaitEvent;

    delete _pTitle;
    delete _pSymDeg;
    delete _pNum_c;
    delete _pNum_d;
    delete _pNum_h;
    delete _pMinus;
    delete _pPlus;
    delete _pRound;
    delete _pCap;
    delete _pHLine;
}

void PitchFragment::onPause()
{
    this->cancelTimer();
}

void PitchFragment::onResume()
{
    _bRotate180 = _isRotate180();
    updateSymbols();
    this->Draw(true);
    this->startTimer(2, true);
}

bool PitchFragment::_isRotate180()
{
    bool bRotate = false;
    char horizontal[256];
    char vertical[256];
    memset(horizontal, 0x00, 256);
    memset(vertical, 0x00, 256);
    agcmd_property_get(PropName_Rotate_Horizontal, horizontal, "0");
    agcmd_property_get(PropName_Rotate_Vertical, vertical, "0");
    if ((strcasecmp(horizontal, "0") == 0)
        && (strcasecmp(vertical, "0") == 0))
    {
        bRotate = false;
    } else if ((strcasecmp(horizontal, "1") == 0)
        && (strcasecmp(vertical, "1") == 0))
    {
        bRotate = true;
    }

    return bRotate;
}

bool PitchFragment::getPitchAngle()
{
    float roll, pitch, heading;
    bool got = _cp->getOrientation(heading, roll, pitch);
    if (!got) {
        return false;
    }

    float diff_abs_p = (_angle_p > pitch) ? (_angle_p - pitch) : (pitch - _angle_p);
    if (diff_abs_p < 0.1) {
        _bChanged = false;
    } else {
        _bChanged = true;
        _angle_p = pitch;
    }

    return true;
}

void PitchFragment::updateSymbols()
{
    if (!getPitchAngle()) {
        return;
    }

    if (!_bChanged) {
        return;
    }

    if (_angle_p > 180.0) {
        _angle_p = 180.0;
    } else if (_angle_p < -180.0) {
        _angle_p = -180.0;
    }

    float ratio = 1.0;
    if (_angle_p >= 90.0) {
        ratio = (270.0 - _angle_p) / 90.0;
        _pCap->setRatio(ratio);
        _pCap->setAngle(180.0 + (_bRotate180 ? 180 : 0));
    } else if (_angle_p >= -90.0) {
        ratio = 1.0 + _angle_p / 90.0;
        _pCap->setRatio(ratio);
        _pCap->setAngle(0.0 + (_bRotate180 ? 180 : 0));
    } else {
        ratio = -1.0 * _angle_p / 90.0 - 1;
        _pCap->setRatio(ratio);
        _pCap->setAngle(180.0 + (_bRotate180 ? 180 : 0));
    }

    int angle = _angle_p;
    if (_bRotate180) {
        if (_angle_p < 0.0) {
            angle = -180.0 - _angle_p;
        } else if (_angle_p > 0.0) {
            angle = 180.0 - _angle_p;
        }
    }
    if (angle == 0) {
        _pPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-plus-inactive.bmp");
        _pMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-minus-inactive.bmp");
    } else if (angle > 0) {
        _pPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-plus-active.bmp");
        _pMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-minus-inactive.bmp");
    } else {
        angle = -1 * angle;
        _pPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-plus-inactive.bmp");
        _pMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-minus-active.bmp");
    }

    char name[64];
    if (angle < 10) {
        _pPlus->SetRelativePos(Point(118, 178));
        _pMinus->SetRelativePos(Point(118, 200));
        _pNum_h->SetHiden();

        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/",
            "num-144-0_alt.bmp");
        _pNum_d->SetRelativePos(Point(148, 130));

        snprintf(name, 64, "num-144-%d.bmp", angle % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200, 130));

        _pSymDeg->SetRelativePos(Point(252, 144));
    } else if (angle < 100) {
        _pPlus->SetRelativePos(Point(118, 178));
        _pMinus->SetRelativePos(Point(118, 200));
        _pNum_h->SetHiden();

        snprintf(name, 64, "num-144-%d.bmp", angle / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148, 130));

        snprintf(name, 64, "num-144-%d.bmp", angle % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200, 130));

        _pSymDeg->SetRelativePos(Point(252, 144));
    } else {
        _pPlus->SetRelativePos(Point(118 - 26, 178));
        _pMinus->SetRelativePos(Point(118 - 26, 200));

        _pNum_h->SetShow();
        snprintf(name, 64, "num-144-%d.bmp", angle / 100);
        _pNum_h->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_h->SetRelativePos(Point(148 - 26, 130));

        angle = angle % 100;
        snprintf(name, 64, "num-144-%d.bmp", angle / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148 + 26, 130));

        snprintf(name, 64, "num-144-%d.bmp", angle % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200 + 26, 130));

        _pSymDeg->SetRelativePos(Point(252 + 26, 144));
    }
}

bool PitchFragment::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_rotation) {
                    _bRotate180 = _isRotate180();
                    updateSymbols();
                    this->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}

void PitchFragment::onTimerEvent()
{
    _pLock->Take();
    _numTimerEvent++;
    _pWaitEvent->Give();
    _pLock->Give();
}

void PitchFragment::main(void *)
{
    while (true) {
        _pWaitEvent->Take(-1);

        {
            bool bWaiting = false;
            _pLock->Take();
            bWaiting = _numTimerEvent > 0;
            _pLock->Give();
            if (bWaiting > 0) {
                _numTimerEvent--;

                updateSymbols();
                if (_bChanged) {
                    this->Draw(true);
                    _bChanged = false;
#if 0
                    static struct timeval last_tv;
                    static int counter = 0;
                    struct timeval cur_tv;
                    gettimeofday(&cur_tv, NULL);
                    float tm = cur_tv.tv_sec - last_tv.tv_sec
                        + (cur_tv.tv_usec - last_tv.tv_usec) / (float)1000000;
                    counter ++;
                    if (tm >= 1.0) {
                        CAMERALOG("%s: %d frames updated during %.2fs, fps = %.2f",
                            this->getWndName(), counter, tm, counter / tm);
                        counter = 0;
                        last_tv.tv_sec = cur_tv.tv_sec;
                        last_tv.tv_usec = cur_tv.tv_usec;
                    }
#endif
                }
            }
        }
    }
}


RollFragment::RollFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
  : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
  , CThread("RollFragment_thread", CThread::NORMAL , 0, NULL)
  , _cp(cp)
  , _angle_r(0.0)
  , _bChanged(true)
  , _bRotate180(false)
  , _numTimerEvent(0)
{
    _bRotate180 = _isRotate180();

    _pHLine = new UILine(this, Point(0, 200), Size(2, 400));
    _pHLine->setLine(Point(0, 200), Point(400, 200), 4, 0x18A3);

    _pCap = new RoundCap(this, Point(200, 200), 199, 0xFFE3);
    _pCap->setAngle(10.0);
    _pCap->setRatio(1.0);

    _pRound = new Round(this, Point(200, 200), 130, 0x18A3);

    _pPlus = new BmpImage(
        this,
        Point(118, 178), Size(22, 22),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-144-plus-active.bmp");

    _pMinus = new BmpImage(
        this,
        Point(118, 200), Size(22, 22),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-144-minus-inactive.bmp");

    _pNum_h = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-2.bmp");

    _pNum_d = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-2.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-5.bmp");

    _pSymDeg = new BmpImage(
        this,
        Point(252, 144), Size(14, 14),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-144-degree.bmp");

    _pTitle = new BmpImage(
        this,
        Point(133, 270), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-roll.bmp");

    _pWaitEvent = new CSemaphore(0, 1, "pitchroll sem");
    _pLock = new CMutex("pitchroll mutex");
    this->Go();
}

RollFragment::~RollFragment()
{
    this->cancelTimer();
    this->Stop();

    delete _pLock;
    delete _pWaitEvent;

    delete _pTitle;
    delete _pSymDeg;
    delete _pNum_c;
    delete _pNum_d;
    delete _pNum_h;
    delete _pMinus;
    delete _pPlus;
    delete _pRound;
    delete _pCap;
    delete _pHLine;
}

void RollFragment::onPause()
{
    this->cancelTimer();
}

void RollFragment::onResume()
{
    _bRotate180 = _isRotate180();
    updateSymbols();
    this->Draw(true);
    this->startTimer(1, true);
}

bool RollFragment::_isRotate180()
{
    bool bRotate = false;
    char horizontal[256];
    char vertical[256];
    memset(horizontal, 0x00, 256);
    memset(vertical, 0x00, 256);
    agcmd_property_get(PropName_Rotate_Horizontal, horizontal, "0");
    agcmd_property_get(PropName_Rotate_Vertical, vertical, "0");
    if ((strcasecmp(horizontal, "0") == 0)
        && (strcasecmp(vertical, "0") == 0))
    {
        bRotate = false;
    } else if ((strcasecmp(horizontal, "1") == 0)
        && (strcasecmp(vertical, "1") == 0))
    {
        bRotate = true;
    }

    return bRotate;
}

bool RollFragment::getRollAngle()
{
    float roll, pitch, heading;
    bool got = _cp->getOrientation(heading, roll, pitch);
    if (!got) {
        return false;
    }

    // map scope [0, 360] to [-180, 180]
    if (_bRotate180) {
        roll = roll - 180.0;
    } else {
        if (roll > 180.0) {
            roll = roll - 360.0;
        }
    }

    if (_angle_r == roll) {
        _bChanged = false;
    } else {
        _angle_r = roll;
        _bChanged = true;
    }

    return true;
}

void RollFragment::updateSymbols()
{
    bool got = getRollAngle();
    if (!got) {
        return;
    }

    if (!_bChanged) {
        return;
    }

    _pCap->setAngle(_angle_r);

    int angle = _angle_r;
    if (angle == 0) {
        _pPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-plus-inactive.bmp");
        _pMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-minus-inactive.bmp");
    } else if (angle > 0) {
        _pPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-plus-active.bmp");
        _pMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-minus-inactive.bmp");
    } else {
        angle = -1 * angle;
        _pPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-plus-inactive.bmp");
        _pMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-144-minus-active.bmp");
    }

    char name[64];
    if (angle < 10) {
        _pPlus->SetRelativePos(Point(118, 178));
        _pMinus->SetRelativePos(Point(118, 200));
        _pNum_h->SetHiden();

        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/",
            "num-144-0_alt.bmp");
        _pNum_d->SetRelativePos(Point(148, 130));

        snprintf(name, 64, "num-144-%d.bmp", angle % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200, 130));

        _pSymDeg->SetRelativePos(Point(252, 144));
    } else if (angle < 100) {
        _pPlus->SetRelativePos(Point(118, 178));
        _pMinus->SetRelativePos(Point(118, 200));
        _pNum_h->SetHiden();

        snprintf(name, 64, "num-144-%d.bmp", angle / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148, 130));

        snprintf(name, 64, "num-144-%d.bmp", angle % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200, 130));

        _pSymDeg->SetRelativePos(Point(252, 144));
    } else {
        _pPlus->SetRelativePos(Point(118 - 26, 178));
        _pMinus->SetRelativePos(Point(118 - 26, 200));

        _pNum_h->SetShow();
        snprintf(name, 64, "num-144-%d.bmp", angle / 100);
        _pNum_h->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_h->SetRelativePos(Point(148 - 26, 130));

        angle = angle % 100;
        snprintf(name, 64, "num-144-%d.bmp", angle / 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_d->SetRelativePos(Point(148 + 26, 130));

        snprintf(name, 64, "num-144-%d.bmp", angle % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
        _pNum_c->SetRelativePos(Point(200 + 26, 130));

        _pSymDeg->SetRelativePos(Point(252 + 26, 144));
    }
}

bool RollFragment::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_rotation) {
                    _bRotate180 = _isRotate180();
                    updateSymbols();
                    this->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}

void RollFragment::onTimerEvent()
{
    _pLock->Take();
    _numTimerEvent++;
    _pWaitEvent->Give();
    _pLock->Give();
}

void RollFragment::main(void *)
{
    while (true) {
        _pWaitEvent->Take(-1);

        {
            bool bWaiting = false;
            _pLock->Take();
            bWaiting = _numTimerEvent > 0;
            _pLock->Give();
            if (bWaiting > 0) {
                _numTimerEvent--;

                updateSymbols();
                if (_bChanged) {
                    this->Draw(true);
                    _bChanged = false;
#if 0
                    static struct timeval last_tv;
                    static int counter = 0;
                    struct timeval cur_tv;
                    gettimeofday(&cur_tv, NULL);
                    float tm = cur_tv.tv_sec - last_tv.tv_sec
                        + (cur_tv.tv_usec - last_tv.tv_usec) / (float)1000000;
                    counter ++;
                    if (tm >= 1.0) {
                        CAMERALOG("%s: %d frames updated during %.2fs, fps = %.2f",
                            this->getWndName(), counter, tm, counter / tm);
                        counter = 0;
                        last_tv.tv_sec = cur_tv.tv_sec;
                        last_tv.tv_usec = cur_tv.tv_usec;
                    }
#endif
                }
            }
        }
    }
}


GForceFragment::GForceFragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
    , _accel_X(2.0) // an unreasonable value, so that first time it got updated
    , _accel_Y(2.0)
    , _bChanged(true)
    , _bRotate180(false)
    , _rMaxDiff(35)
    , _rMaxHistory(160)
{
    _bRotate180 = _isRotate180();

    _pRAccel = new Round(this, Point(200, 200), 130 + 2, 0xFFE3);

    _pMaxHistory = new Circle(this, Point(200, 200), _rMaxHistory, 2, 0x18A3);

    _pRound = new Round(this, Point(200, 200), 130, 0x18A3);

    _pSymX = new BmpImage(
        this,
        Point(142, 169), Size(14, 27),
        "/usr/local/share/ui/BMP/gauge/",
        "label-x.bmp");

    _pXPlus = new BmpImage(
        this,
        Point(165, 174), Size(11, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-plus-inactive.bmp");

    _pXMinus = new BmpImage(
        this,
        Point(165, 185), Size(11, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-minus-inactive.bmp");

    _pXNum_int = new BmpImage(
        this,
        Point(185, 146), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");

    _pXNum_dot = new BmpImage(
        this,
        Point(207, 185), Size(7, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-point.bmp");

    _pXNum_f1= new BmpImage(
        this,
        Point(214, 146), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");

    _pXNum_f2 = new BmpImage(
        this,
        Point(235, 146), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");


    _pSymY = new BmpImage(
        this,
        Point(142, 227), Size(14, 27),
        "/usr/local/share/ui/BMP/gauge/",
        "label-y.bmp");

    _pYPlus = new BmpImage(
        this,
        Point(165, 232), Size(11, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-plus-inactive.bmp");

    _pYMinus = new BmpImage(
        this,
        Point(165, 243), Size(11, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-minus-inactive.bmp");

    _pYNum_int = new BmpImage(
        this,
        Point(185, 204), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");

    _pYNum_dot = new BmpImage(
        this,
        Point(207, 243), Size(7, 11),
        "/usr/local/share/ui/BMP/gauge/",
        "sign-64-point.bmp");

    _pYNum_f1= new BmpImage(
        this,
        Point(214, 204), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");

    _pYNum_f2 = new BmpImage(
        this,
        Point(235, 204), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");

    _pTitle = new BmpImage(
        this,
        Point(133, 270), Size(134, 30),
        "/usr/local/share/ui/BMP/gauge/",
        "label-gforce.bmp");
}

GForceFragment::~GForceFragment()
{
    delete _pTitle;
    delete _pYNum_f2;
    delete _pYNum_f1;
    delete _pYNum_dot;
    delete _pYNum_int;
    delete _pYMinus;
    delete _pYPlus;
    delete _pSymY;
    delete _pXNum_f2;
    delete _pXNum_f1;
    delete _pXNum_dot;
    delete _pXNum_int;
    delete _pXMinus;
    delete _pXPlus;
    delete _pSymX;
    delete _pRound;
    delete _pMaxHistory;
    delete _pRAccel;
}

bool GForceFragment::_isRotate180()
{
    bool bRotate = false;
    char horizontal[256];
    char vertical[256];
    memset(horizontal, 0x00, 256);
    memset(vertical, 0x00, 256);
    agcmd_property_get(PropName_Rotate_Horizontal, horizontal, "0");
    agcmd_property_get(PropName_Rotate_Vertical, vertical, "0");
    if ((strcasecmp(horizontal, "0") == 0)
        && (strcasecmp(vertical, "0") == 0))
    {
        bRotate = false;
    } else if ((strcasecmp(horizontal, "1") == 0)
        && (strcasecmp(vertical, "1") == 0))
    {
        bRotate = true;
    }

    return bRotate;
}

bool GForceFragment::getAccelXYZ()
{
    float x, y, z;
    bool got = _cp->getAccelXYZ(x, y, z);
    if (!got) {
        return false;
    }

    const float minimum = 0.02;
    if (x > 1.0) {
        x = 1.0;
    } else if (x < -1.0) {
        x = -1.0;
    } else if ((x > -1 * minimum) && (x < minimum)) {
        x = 0.0;
    }

    if (z > 1.0) {
        z = 1.0;
    } else if (z < -1.0) {
        z = -1.0;
    } else if ((z > -1 * minimum) && (z < minimum)) {
        z = 0.0;
    }

    bool xChanged = false;
    const float threshold = 0.01;
    if ((_accel_X - x <= threshold) && (_accel_X - x >= -1 * threshold)) {
        xChanged = false;
    } else {
        _accel_X = -x;
        xChanged = true;
    }

    bool yChanged = false;
    if ((_accel_Y - z <= threshold) && (_accel_Y - z >= -1 * threshold)) {
        yChanged = false;
    } else {
        _accel_Y = z;
        yChanged = true;
    }

    _bChanged = xChanged || yChanged;
    return true;
}

void GForceFragment::updateSymbols()
{
    bool got = getAccelXYZ();
    if (!got) {
        return;
    }

    if (!_bChanged) {
        return;
    }

    // update round for accel
    float g_force = sqrt(_accel_X * _accel_X + _accel_Y * _accel_Y);
    float MAX_g_force = 1.0;
    if (g_force == 0.0) {
        _pRAccel->setParams(Point(200, 200), 130 + 2, 0xFFE3);
        _pRAccel->SetRelativePos(Point(200, 200));
    } else {
        int r_diff = (g_force > MAX_g_force ? MAX_g_force : g_force) / MAX_g_force * _rMaxDiff;
        int x = r_diff * (_bRotate180 ? -1 : 1) * _accel_X / g_force;
        int y = r_diff * _accel_Y / g_force;;
        _pRAccel->setParams(Point(200 + x, 200 + y), 130 + 2 + r_diff, 0xFFE3);
        _pRAccel->SetRelativePos(Point(200 + x, 200 + y));
        if (_rMaxHistory < (130 + 2 + r_diff) * 2 - 130 - 2) {
            _rMaxHistory = (130 + 2 + r_diff) * 2 - 130 - 2;
            if (_rMaxHistory >= 199) {
               _rMaxHistory = 199;
            }
            _pMaxHistory->setParams(Point(200, 200), _rMaxHistory, 2, 0x18A3);
        }
    }

    // update numerical for x
    float accel = (_bRotate180 ? -1 : 1) * _accel_X;
    bool plus = accel > 0.0 ? true : false;
    bool overthreshold = (_accel_X >= 0.01) || (_accel_X <= -0.01);
    if (overthreshold) {
        if (plus) {
            _pXPlus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-plus-active.bmp");
            _pXMinus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-minus-inactive.bmp");
        } else {
            accel = -1 * accel;
            _pXPlus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-plus-inactive.bmp");
            _pXMinus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-minus-active.bmp");
        }
    } else {
        accel = 0.0;
        _pXPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-plus-inactive.bmp");
        _pXMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-minus-inactive.bmp");
    }

    char name[64];
    snprintf(name, 64, "num-64-%d.bmp", ((int)accel) % 10);
    _pXNum_int->setSource("/usr/local/share/ui/BMP/gauge/", name);

    accel = (accel - (int)accel) * 10;
    snprintf(name, 64, "num-64-%d.bmp", ((int)accel) % 10);
    _pXNum_f1->setSource("/usr/local/share/ui/BMP/gauge/", name);

    accel = (accel - (int)accel) * 10;
    snprintf(name, 64, "num-64-%d.bmp", ((int)accel) % 10);
    _pXNum_f2->setSource("/usr/local/share/ui/BMP/gauge/", name);

    // update numerical for y
    accel = _accel_Y;
    plus = _accel_Y > 0.0 ? true : false;
    overthreshold = (_accel_Y >= 0.01) || (_accel_Y <= -0.01);
    if (overthreshold) {
        if (plus) {
            accel = _accel_Y;
            _pYPlus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-plus-active.bmp");
            _pYMinus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-minus-inactive.bmp");
        } else {
            accel = -1 * _accel_Y;
            _pYPlus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-plus-inactive.bmp");
            _pYMinus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-minus-active.bmp");
        }
    } else {
        accel = 0.0;
        _pYPlus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-plus-inactive.bmp");
        _pYMinus->setSource("/usr/local/share/ui/BMP/gauge/",
            "sign-64-minus-inactive.bmp");
    }

    snprintf(name, 64, "num-64-%d.bmp", ((int)accel) % 10);
    _pYNum_int->setSource("/usr/local/share/ui/BMP/gauge/", name);

    accel = (accel - (int)accel) * 10;
    snprintf(name, 64, "num-64-%d.bmp", ((int)accel) % 10);
    _pYNum_f1->setSource("/usr/local/share/ui/BMP/gauge/", name);

    accel = (accel - (int)accel) * 10;
    snprintf(name, 64, "num-64-%d.bmp", ((int)accel) % 10);
    _pYNum_f2->setSource("/usr/local/share/ui/BMP/gauge/", name);
}

void GForceFragment::onResume()
{
    _bRotate180 = _isRotate180();
    updateSymbols();
    this->Draw(true);
}

void GForceFragment::onPause()
{
}

bool GForceFragment::OnEvent(CEvent *event)
{
    bool b = true;
    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                updateSymbols();
                if (_bChanged) {
                    this->Draw(true);
                    _bChanged = false;
                }
            } else if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_rotation) {
                    _bRotate180 = _isRotate180();
                    updateSymbols();
                    this->Draw(true);
                    b = false;
                }
            }
            break;
    }

    return b;
}


GaugesWnd::GaugesWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _numPages(0)
    , _liveviewIndex(0)
    , _pitchrollIndex(0)
    , _indexFocus(0)
    , _indexPitchRoll(0)
    , _cntScreenSaver(0)
    , _screenSaverTimeSetting(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 4);
    this->SetMainObject(_pPanel);

    _pViewPager = new ViewPager(_pPanel, this, Point(0, 0), Size(400, 400), 2);

    _pPitchRolls[0] = new PitchFragment(
        "PitchFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);
    _pPitchRolls[1] = new PitchRollFragment(
        "PitchRollFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);
    _pPitchRolls[2] = new RollFragment(
        "RollFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);
    _indexPitchRoll = 1;

    _numPages = 0;

    _liveviewIndex = _numPages;
    _pPages[_numPages++] = new LiveViewFragment(
        "LiveViewFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);

    _pitchrollIndex = _numPages;
    _pPages[_numPages++] = _pPitchRolls[_indexPitchRoll];

    _pPages[_numPages++] = new GForceFragment(
        "GForceFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);

#if 1
    _pPages[_numPages++] = new CoolantTempFragment(
        "CoolantTempFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);
    _pPages[_numPages++] = new BoostFragment(
        "BoostFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);
    _pPages[_numPages++] = new MAFFragment(
        "MAFFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);
    _pPages[_numPages++] = new RPMFragment(
        "RPMFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);
    _pPages[_numPages++] = new SpeedFragment(
        "SpeedFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);
#endif

    _pViewPager->setPages(_pPages, _numPages);
    _pViewPager->setListener(this);

    _pIndicator = new TabIndicator(
        _pPanel, Point(120, 382), Size(160, 8),
        _numPages, _liveviewIndex,
        4, 0x9CF3, 0x3187);

    _indexFocus = _liveviewIndex; // live view
    _pViewPager->setFocus(_indexFocus);
}

GaugesWnd::~GaugesWnd()
{
    delete _pIndicator;
    for (int i = 0; i < _numPages; i++) {
        delete _pPages[i];
    }
    delete _pViewPager;
    delete _pPanel;
}

int GaugesWnd::_getScreenSaverTime()
{
    int timeOff = 60;
    char tmp[256];
    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);
    if (strcasecmp(tmp, "Never") == 0) {
        timeOff = -1;
    } else if (strcasecmp(tmp, "10s") == 0) {
        timeOff = 10;
    } else if (strcasecmp(tmp, "30s") == 0) {
        timeOff = 30;
    } else if (strcasecmp(tmp, "60s") == 0) {
        timeOff = 60;
    } else if (strcasecmp(tmp, "2min") == 0) {
        timeOff = 120;
    } else if (strcasecmp(tmp, "5min") == 0) {
        timeOff = 300;
    }

    _screenSaverTimeSetting = timeOff * 10;
    return _screenSaverTimeSetting;
}

void GaugesWnd::onFocusChanged(int indexFocus)
{
    _cntScreenSaver = 0;
    _indexFocus = indexFocus;
    _pIndicator->setHighlight(_indexFocus);
    _pPanel->Draw(true);
}

void GaugesWnd::onViewPortChanged(const Point &leftTop)
{
    _cntScreenSaver = 0;
    _pIndicator->Draw(false);
}

void GaugesWnd::onResume()
{
    _getScreenSaverTime();
    _cntScreenSaver = 0;

#if 1
    _pViewPager->getCurFragment()->onResume();
    _pIndicator->setHighlight(_indexFocus);
#else
    _indexFocus = _liveviewIndex; // live view
    _pViewPager->setFocus(_liveviewIndex);
    _pViewPager->getCurFragment()->onResume();
    _pIndicator->setHighlight(_liveviewIndex);
#endif
}

void GaugesWnd::onPause()
{
    _cntScreenSaver = 0;
    _pViewPager->getCurFragment()->onPause();
#if 0
    _indexFocus = _liveviewIndex; // live view
    _pViewPager->setFocus(_liveviewIndex);
    _pIndicator->setHighlight(_liveviewIndex);
#endif
}

bool GaugesWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _cntScreenSaver++;
                if ((_indexFocus == _liveviewIndex)
                    && (_screenSaverTimeSetting > 0)
                    && (_cntScreenSaver >= _screenSaverTimeSetting)) {
                    _cntScreenSaver = 0;
                    this->StartWnd(WINDOW_screen_saver, Anim_Null);
                }
            } else if (event->_paload == PropertyChangeType_settings) {
                _getScreenSaverTime();
            }
            break;
        case eventType_touch:
            _cntScreenSaver = 0;
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                this->StartWnd(WINDOW_shotcuts, Anim_Top2BottomEnter);
                b = false;
            } else if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_UP)) {
                //this->StartWnd(WINDOW_settings, Anim_Bottom2TopEnter);
                this->StartWnd(WINDOW_settingsgauges, Anim_Bottom2TopEnter);
                b = false;
            } else if (event->_event == TouchEvent_DoubleClick) {
                if (_indexFocus == _pitchrollIndex) {
                    _pViewPager->getCurFragment()->onPause();
                    _indexPitchRoll++;
                    if (_indexPitchRoll >= 3) {
                        _indexPitchRoll = 0;
                    }
                    _pPages[_pitchrollIndex] = _pPitchRolls[_indexPitchRoll];
                    _pViewPager->setPages(_pPages, _numPages);
                    _pViewPager->getCurFragment()->onResume();
                    _pPanel->Draw(true);
                }
            }
            break;
        case eventType_key:
            _cntScreenSaver = 0;
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
            _cntScreenSaver = 0;
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


Menu_P1_Fragment::Menu_P1_Fragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
{
    _pMenuVideo = new Button(this, Point(80, 15), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuVideo->SetText((UINT8 *)"Video", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuVideo->SetStyle(35, 1.0);
    _pMenuVideo->setOnClickListener(this);

    _pMenuAudio = new Button(this, Point(80, 87), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuAudio->SetText((UINT8 *)"Audio", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuAudio->SetStyle(35, 1.0);
    _pMenuAudio->setOnClickListener(this);

    _pMenuDisplay = new Button(this, Point(80, 159), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuDisplay->SetText((UINT8 *)"Display", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuDisplay->SetStyle(35, 1.0);
    _pMenuDisplay->setOnClickListener(this);

    _pMenuWifi = new Button(this, Point(80, 231), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuWifi->SetText((UINT8 *)"Wifi", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuWifi->SetStyle(35, 1.0);
    _pMenuWifi->setOnClickListener(this);

    _pMenuBluetooth = new Button(this, Point(80, 303), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuBluetooth->SetText((UINT8 *)"Bluetooth", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuBluetooth->SetStyle(35, 1.0);
    _pMenuBluetooth->setOnClickListener(this);
}

Menu_P1_Fragment::~Menu_P1_Fragment()
{
    delete _pMenuBluetooth;
    delete _pMenuWifi;
    delete _pMenuDisplay;
    delete _pMenuAudio;
    delete _pMenuVideo;
}

void Menu_P1_Fragment::onClick(Control *widget)
{
    if (widget == _pMenuVideo) {
        this->GetOwner()->StartWnd(WINDOW_videoquality, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pMenuAudio) {
        this->GetOwner()->StartWnd(WINDOW_audiomenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pMenuDisplay) {
        this->GetOwner()->StartWnd(WINDOW_displaymenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pMenuWifi) {
        this->GetOwner()->StartWnd(WINDOW_wifimenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pMenuBluetooth) {
        this->GetOwner()->StartWnd(WINDOW_btmenu, Anim_Bottom2TopCoverEnter);
    }
}

void Menu_P1_Fragment::onResume()
{
}

void Menu_P1_Fragment::onPause()
{
}


Menu_P2_Fragment::Menu_P2_Fragment(
        const char *name,
        Container* pParent,
        Window* pOwner,
        Point leftTop,
        Size size,
        UINT16 maxChildren,
        CCameraProperties *cp)
    : inherited(name, pParent, pOwner, leftTop, size, maxChildren)
    , _cp(cp)
{
    _pMenuAutoOff = new Button(this, Point(80, 15), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuAutoOff->SetText((UINT8 *)"Auto Off", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuAutoOff->SetStyle(35, 1.0);
    _pMenuAutoOff->setOnClickListener(this);

    _pMenuStorage = new Button(this, Point(80, 87), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuStorage->SetText((UINT8 *)"Storage", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuStorage->SetStyle(35, 1.0);
    _pMenuStorage->setOnClickListener(this);

    _pMenuTimeDate = new Button(this, Point(80, 159), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuTimeDate->SetText((UINT8 *)"Date & Time", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuTimeDate->SetStyle(35, 1.0);
    _pMenuTimeDate->setOnClickListener(this);

    _pMenuMisc = new Button(this, Point(80, 231), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuMisc->SetText((UINT8 *)"Advanced", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuMisc->SetStyle(35, 1.0);
    _pMenuMisc->setOnClickListener(this);
}

Menu_P2_Fragment::~Menu_P2_Fragment()
{
    delete _pMenuMisc;
    delete _pMenuTimeDate;
    delete _pMenuStorage;
    delete _pMenuAutoOff;
}

void Menu_P2_Fragment::onClick(Control *widget)
{
    if (widget == _pMenuAutoOff) {
        this->GetOwner()->StartWnd(WINDOW_autooffmenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pMenuStorage) {
        this->GetOwner()->StartWnd(WINDOW_storagemenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pMenuTimeDate) {
        this->GetOwner()->StartWnd(WINDOW_timemenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pMenuMisc) {
        this->GetOwner()->StartWnd(WINDOW_advancedmenu, Anim_Bottom2TopCoverEnter);
    }
}

void Menu_P2_Fragment::onResume()
{
}

void Menu_P2_Fragment::onPause()
{
}


SettingsGaugesWnd::SettingsGaugesWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _numPages(2)
    , _indexFocus(0)
    , _cntScreenSaver(0)
    , _screenSaverTimeSetting(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 4);
    this->SetMainObject(_pPanel);

    _pViewPager = new ViewPager(_pPanel, this, Point(0, 0), Size(400, 400), 20);

    _pPages[0] = new Menu_P1_Fragment(
        "Menu_P1_Fragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);
    _pPages[1] = new Menu_P2_Fragment(
        "Menu_P2_Fragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);

    _numPages = 2;

    _pViewPager->setPages(_pPages, _numPages);
    _pViewPager->setListener(this);

    _pIndicator = new TabIndicator(
        _pPanel, Point(140, 382), Size(120, 8),
        2, 0,
        4, 0x9CF3, 0x3187);

    _indexFocus = 0;
    _pViewPager->setFocus(_indexFocus);
}

SettingsGaugesWnd::~SettingsGaugesWnd()
{
    delete _pIndicator;
    for (int i = 0; i < _numPages; i++) {
        delete _pPages[i];
    }
    delete _pViewPager;
    delete _pPanel;
}

int SettingsGaugesWnd::_getScreenSaverTime()
{
    int timeOff = 60;
    char tmp[256];
    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);
    if (strcasecmp(tmp, "Never") == 0) {
        timeOff = -1;
    } else if (strcasecmp(tmp, "10s") == 0) {
        timeOff = 10;
    } else if (strcasecmp(tmp, "30s") == 0) {
        timeOff = 30;
    } else if (strcasecmp(tmp, "60s") == 0) {
        timeOff = 60;
    } else if (strcasecmp(tmp, "2min") == 0) {
        timeOff = 120;
    } else if (strcasecmp(tmp, "5min") == 0) {
        timeOff = 300;
    }

    _screenSaverTimeSetting = timeOff * 10;
    return _screenSaverTimeSetting;
}

void SettingsGaugesWnd::onFocusChanged(int indexFocus)
{
    _indexFocus = indexFocus;
    _pIndicator->setHighlight(_indexFocus);
    _pPanel->Draw(true);
}

void SettingsGaugesWnd::onViewPortChanged(const Point &leftTop)
{
    _pIndicator->Draw(false);
}

void SettingsGaugesWnd::onResume()
{
    _getScreenSaverTime();
    _cntScreenSaver = 0;

    _pViewPager->getCurFragment()->onResume();
    _pIndicator->setHighlight(_indexFocus);
}

void SettingsGaugesWnd::onPause()
{
    _cntScreenSaver = 0;
    _pViewPager->getCurFragment()->onPause();
}

bool SettingsGaugesWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _cntScreenSaver++;
                if ((_screenSaverTimeSetting > 0) && (_cntScreenSaver >= _screenSaverTimeSetting)) {
                    _cntScreenSaver = 0;
                    this->StartWnd(WINDOW_screen_saver, Anim_Null);
                }
            } else if (event->_paload == PropertyChangeType_settings) {
                _getScreenSaverTime();
            }
            break;
        case eventType_touch:
            _cntScreenSaver = 0;
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                this->StartWnd(WINDOW_gauges, Anim_Top2BottomEnter);
                b = false;
            }
            break;
        case eventType_key:
            _cntScreenSaver = 0;
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
            _cntScreenSaver = 0;
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


MenuWindow::MenuWindow(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
{
}

MenuWindow::~MenuWindow()
{
}

int MenuWindow::_getScreenSaverTime()
{
    int timeOff = 60;
    char tmp[256];
    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);
    if (strcasecmp(tmp, "Never") == 0) {
        timeOff = -1;
    } else if (strcasecmp(tmp, "10s") == 0) {
        timeOff = 10;
    } else if (strcasecmp(tmp, "30s") == 0) {
        timeOff = 30;
    } else if (strcasecmp(tmp, "60s") == 0) {
        timeOff = 60;
    } else if (strcasecmp(tmp, "2min") == 0) {
        timeOff = 120;
    } else if (strcasecmp(tmp, "5min") == 0) {
        timeOff = 300;
    }

    _screenSaverTimeSetting = timeOff * 10;
    return _screenSaverTimeSetting;
}

void MenuWindow::onResume()
{
    _getScreenSaverTime();
    _cntScreenSaver = 0;

    onMenuResume();
}

void MenuWindow::onPause()
{
    _cntScreenSaver = 0;

    onMenuPause();
}

bool MenuWindow::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _cntScreenSaver++;
                if ((_screenSaverTimeSetting > 0) && (_cntScreenSaver >= _screenSaverTimeSetting)) {
                    _cntScreenSaver = 0;
                    this->StartWnd(WINDOW_screen_saver, Anim_Null);
                }
            } else if (event->_paload == PropertyChangeType_settings) {
                _getScreenSaverTime();
            }
            break;
        case eventType_touch:
            _cntScreenSaver = 0;
            break;
        case eventType_key:
            _cntScreenSaver = 0;
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
            _cntScreenSaver = 0;
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


VideoMenuWnd::VideoMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(0, 150), Size(100, 100), 0x0000, 0x8410);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(20, 176), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pMenuMode = new Button(_pPanel, Point(100, 125), Size(260, 75), 0x0000, 0x8410);
    _pMenuMode->SetText((UINT8 *)"Recording Mode", 28, Color_White);
    _pMenuMode->SetTextAlign(LEFT, MIDDLE);
    _pMenuMode->setOnClickListener(this);

    _pMenuQuality = new Button(_pPanel, Point(100, 200), Size(260, 75), 0x0000, 0x8410);
    _pMenuQuality->SetText((UINT8 *)"Quality", 28, Color_White);
    _pMenuQuality->SetTextAlign(LEFT, MIDDLE);
    _pMenuQuality->setOnClickListener(this);
}

VideoMenuWnd::~VideoMenuWnd()
{
    delete _pMenuQuality;
    delete _pMenuMode;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void VideoMenuWnd::onMenuResume()
{
}

void VideoMenuWnd::onMenuPause()
{
}

void VideoMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pMenuQuality) {
        this->StartWnd(WINDOW_videoquality, Anim_Right2LeftEnter);
    }
}

bool VideoMenuWnd::OnEvent(CEvent *event)
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


VideoQualityWnd::VideoQualityWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _pTxt(NULL)
    , _pIconYes(NULL)
    , _pIconNo(NULL)
    , _targetIndex(-1)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 10);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), 0x0000, 0x0000);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 42));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Quality");

    _pLine = new UILine(_pPanel, Point(105, 106), Size(190, 4));
    _pLine->setLine(Point(105, 100), Point(295, 100), 4, Color_Grey_2);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)"1080p60";
    _pMenus[_numMenus++] = (char *)"1080p30";
    _pMenus[_numMenus++] = (char *)"720p120";
    _pMenus[_numMenus++] = (char *)"720p60";
    _pMenus[_numMenus++] = (char *)"720p30";

    UINT16 itemheight = 60;
    UINT16 padding = 2;
    _pickerSize.width = 200;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding + itemheight / 3;
    _pPickers = new Pickers(_pPanel, Point(100, 120), Size(200, 260), _pickerSize);
    _pPickers->setItems(_pMenus, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);
}

VideoQualityWnd::~VideoQualityWnd()
{
    delete _pIconYes;
    delete _pIconNo;
    delete _pTxt;

    delete _pPickers;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

int VideoQualityWnd::updateStatus()
{
    char resolution[64];
    agcmd_property_get(PropName_rec_resoluton, resolution, "1080p30");
    int index = 0;
    for (index = 0; index < _numMenus; index++) {
        if (strcasecmp(resolution, _pMenus[index]) == 0) {
            break;
        }
    }
    _pPickers->setIndex(index);

    return index;
}

void VideoQualityWnd::showDialogue()
{
    _pTitle->SetHiden();
    _pLine->SetHiden();
    _pPickers->SetHiden();

    if (!_pTxt) {
        _pTxt = new StaticText(_pPanel, Point(0, 76), Size(400, 120));
        _pTxt->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    }
    _pTxt->SetText(
        (UINT8 *)"This action will\n"
        "terminate video\n"
        "recording. Continue?");
    _pTxt->SetShow();

    if (!_pIconNo) {
        _pIconNo = new ImageButton(
            _pPanel,
            Point(56, 216), Size(128, 128),
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-no-up.bmp",
            "btn-dashboard-no-down.bmp");
        _pIconNo->setOnClickListener(this);
    }
    _pIconNo->SetShow();

    if (!_pIconYes) {
        _pIconYes = new ImageButton(
            _pPanel,
            Point(216, 216), Size(128, 128),
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-yes_alt-up.bmp",
            "btn-dashboard-yes_alt-down.bmp");
        _pIconYes->setOnClickListener(this);
    }
    _pIconYes->SetShow();
}

void VideoQualityWnd::hideDialogue()
{
    _pTitle->SetShow();
    _pLine->SetShow();
    _pPickers->SetShow();

    if (_pIconYes) {
        _pIconYes->SetHiden();
    }
    if (_pIconNo) {
        _pIconNo->SetHiden();
    }
    if (_pTxt) {
        _pTxt->SetHiden();
    }
}

void VideoQualityWnd::onMenuResume()
{
    int index = updateStatus();
    if (index >= _numMenus / 2) {
        _pPickers->setViewPort(
            Point(0, _pickerSize.height - _pPickers->GetSize().height));
    } else {
        _pPickers->setViewPort(Point(0, 0));
    }
    _pPanel->Draw(true);
}

void VideoQualityWnd::onMenuPause()
{
}

void VideoQualityWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pIconYes) {
        agcmd_property_set(PropName_rec_resoluton, _pMenus[_targetIndex]);
        _pPickers->setIndex(_targetIndex);
        _targetIndex = -1;
        hideDialogue();
        _pPanel->Draw(true);
        usleep(200 * 1000);
        this->Close(Anim_Top2BottomExit);

        // If it is during recording, stop it.
        camera_State state = _cp->getRecState();
        if ((camera_State_record == state)
            || (camera_State_marking == state)
            || (camera_State_starting == state)
            || (camera_State_stopping == state))
        {
            _cp->OnRecKeyProcess();
        }
    } else if (widget == _pIconNo) {
        hideDialogue();
        _targetIndex = -1;
        _pPanel->Draw(true);
    }
}

void VideoQualityWnd::onPickersFocusChanged(Control *picker, int indexFocus)

{
    if (_pPickers == picker) {
        if ((indexFocus >= 0) && (indexFocus < _numMenus)) {
            bool bRecording = false;
            camera_State state = _cp->getRecState();
            if ((camera_State_record == state)
                || (camera_State_marking == state)
                || (camera_State_starting == state)
                || (camera_State_stopping == state))
            {
                bRecording = true;
            }
            if (bRecording) {
                _targetIndex = indexFocus;
                showDialogue();
                _pPanel->Draw(true);
            } else {
                agcmd_property_set(PropName_rec_resoluton, _pMenus[indexFocus]);
                _pPickers->setIndex(indexFocus);
                _pPanel->Draw(true);
                usleep(200 * 1000);
                this->Close(Anim_Top2BottomExit);
            }
        }
    }
}

bool VideoQualityWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_resolution) {
                    int index = updateStatus();
                    if (index >= _numMenus / 2) {
                        _pPickers->setViewPort(
                            Point(0, _pickerSize.height - _pPickers->GetSize().height));
                    } else {
                        _pPickers->setViewPort(Point(0, 0));
                    }
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
                if ((y_pos < 120)
                    || (x_pos < 100) || (x_pos > 300))
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


AudioMenuWnd::AudioMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);


    _pSwitchBtnFake = new Button(_pPanel, Point(30, 80), Size(340, 80), Color_Black, Color_Grey_1);
    _pSwitchBtnFake->SetStyle(20);
    _pSwitchBtnFake->setOnClickListener(this);

    _pStatus = new StaticText(_pPanel, Point(40, 100), Size(210, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"Speaker");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(304, 106), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");


    _pVolumeBtnFake = new Button(_pPanel, Point(30, 160), Size(340, 80), Color_Black, Color_Grey_1);
    _pVolumeBtnFake->SetStyle(20);
    _pVolumeBtnFake->setOnClickListener(this);

    _pVolume = new StaticText(_pPanel, Point(40, 180), Size(250, 40));
    _pVolume->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pVolume->SetText((UINT8 *)"Speaker Volume");

    snprintf(_txtVolume, 16, "50");
    _pVolumeResult = new StaticText(_pPanel, Point(290, 180), Size(70, 40));
    _pVolumeResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pVolumeResult->SetText((UINT8 *)_txtVolume);


    _pMicSwitchBtnFake = new Button(_pPanel, Point(30, 240), Size(340, 80), Color_Black, Color_Grey_1);
    _pMicSwitchBtnFake->SetStyle(20);
    _pMicSwitchBtnFake->setOnClickListener(this);

    _pMicStatus = new StaticText(_pPanel, Point(40, 260), Size(210, 40));
    _pMicStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pMicStatus->SetText((UINT8 *)"Microphone");

    _pMicSwitch = new BmpImage(
        _pPanel,
        Point(304, 266), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");
}

AudioMenuWnd::~AudioMenuWnd()
{
    delete _pMicSwitch;
    delete _pMicStatus;
    delete _pMicSwitchBtnFake;
    delete _pVolumeResult;
    delete _pVolume;
    delete _pVolumeBtnFake;
    delete _pSwitch;
    delete _pStatus;
    delete _pSwitchBtnFake;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void AudioMenuWnd::updateVolumeDisplay()
{
    char tmp[256];
    int volume = 0;
    agcmd_property_get(Prop_Spk_Volume, tmp, DEFAULT_SPK_VOLUME);
    volume = atoi(tmp);
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }
    snprintf(_txtVolume, 16, "%d", volume * 10);

    agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
    if (strcasecmp(tmp, "Normal") == 0) {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
        _pVolumeResult->SetColor(Color_White);
    } else if (strcasecmp(tmp, "Mute") == 0) {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
        _pVolumeResult->SetColor(0x8410);
    }

    agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
    if (strcasecmp(tmp, "Normal") == 0) {
        _pMicSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else if (strcasecmp(tmp, "Mute") == 0) {
        _pMicSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }

    _pPanel->Draw(true);
}

void AudioMenuWnd::onMenuResume()
{
    updateVolumeDisplay();
}

void AudioMenuWnd::onMenuPause()
{
}

void AudioMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pSwitchBtnFake) {
        char tmp[256];
        agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
        if (strcasecmp(tmp, "Normal") == 0) {
            _cp->SetSpkMute(true);
        } else if (strcasecmp(tmp, "Mute") == 0) {
            _cp->SetSpkMute(false);
        }
    } else if (widget == _pVolumeBtnFake) {
        char tmp[256];
        agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
        if (strcasecmp(tmp, "Normal") == 0) {
            this->StartWnd(WINDOW_spkvolume, Anim_Bottom2TopCoverEnter);
        }
    } else if (widget == _pMicSwitchBtnFake) {
        char tmp[256];
        agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
        if (strcasecmp(tmp, "Normal") == 0) {
            _cp->SetMicMute(true);
        } else if (strcasecmp(tmp, "Mute") == 0) {
            _cp->SetMicMute(false);
        }
    }
}

bool AudioMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_audio) {
                    updateVolumeDisplay();
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


SpeakerVolumeMenuWnd::SpeakerVolumeMenuWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
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
    _pTitle->SetText((UINT8 *)"Volume");

    _pLine = new UILine(_pPanel, Point(115, 106), Size(170, 4));
    _pLine->setLine(Point(115, 100), Point(285, 100), 4, Color_Grey_2);

    snprintf(_slVal, 16, "%d", 80);
    _pSlVal = new StaticText(_pPanel, Point(100, 140), Size(200, 88));
    _pSlVal->SetStyle(64, FONT_ROBOTOMONO_Light, Color_White, CENTER, MIDDLE);
    _pSlVal->SetText((UINT8 *)_slVal);

    _pSlider = new Slider(_pPanel, Point(40, 250), Size(320, 10));
    _pSlider->setStepper(10);
    _pSlider->setColor(Color_Grey_2, Color_White);
    _pSlider->setListener(this);
}

SpeakerVolumeMenuWnd::~SpeakerVolumeMenuWnd()
{
    delete _pSlider;
    delete _pSlVal;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void SpeakerVolumeMenuWnd::updateSpkVolumeDisplay()
{
    char tmp[256];
    int volume = 0;
    agcmd_property_get(Prop_Spk_Volume, tmp, DEFAULT_SPK_VOLUME);
    volume = atoi(tmp);
    snprintf(_slVal, 16, "%d", volume * 10);
    _pSlider->setValue(volume * 10);
}

void SpeakerVolumeMenuWnd::onMenuResume()
{
    updateSpkVolumeDisplay();
    _pPanel->Draw(true);
}

void SpeakerVolumeMenuWnd::onMenuPause()
{
}

void SpeakerVolumeMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void SpeakerVolumeMenuWnd::onSliderChanged(Control *slider, int percentage)
{
    if (_pSlider == slider) {
        char tmp[256];
        int volume = 0;
        agcmd_property_get(Prop_Spk_Volume, tmp, DEFAULT_SPK_VOLUME);
        volume = atoi(tmp);
        int newvolume = percentage / 10;
        if (volume != newvolume) {
            _cp->SetSpkVolume(newvolume);
            snprintf(_slVal, 16, "%d", newvolume * 10);
            _pPanel->Draw(true);
        }
    }
}

void SpeakerVolumeMenuWnd::onSliderTriggered(Control *slider, bool triggered)
{
    if (_pSlider == slider) {
        if (!triggered) {
            char tmp[256];
            agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
            if (strcasecmp(tmp, "Normal") == 0) {
                CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Volume_Changed);
            }
        }
    }
}

bool SpeakerVolumeMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_audio) {
                    updateSpkVolumeDisplay();
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


WifiMenuWnd::WifiMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _counter(0)
    , _cntTimeout(0)
    , _targetMode(wifi_mode_Off)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
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

    _visibleSizeScroller.width = 320;
    _visibleSizeScroller.height = 312;
    _pScroller = new Scroller(
        _pPanel,
        this,
        Point(40, 88),
        _visibleSizeScroller,
        Size(_visibleSizeScroller.width, 628),
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
    delete _pAnimation;

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
        Point(0, 10), Size(150, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"Wifi");

    _pSwitch = new BmpImage(
        _pScroller->getViewRoot(),
        Point(244, 14), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");

    _pModeFake= new Button(
        _pScroller->getViewRoot(),
        Point(0, 70), Size(_visibleSizeScroller.width, 80),
        Color_Black, Color_Black);
    _pModeFake->setOnClickListener(this);

    _pMode = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 90), Size(160, 40));
    _pMode->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pMode->SetText((UINT8 *)"Mode");

    strcpy(_txtMode, "Hotspot");
    _pModeResult = new StaticText(
        _pScroller->getViewRoot(),
        Point(160, 90), Size(160, 40));
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
        Point(0, 170),
        Size(104, 40));
    _pClient->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pClient->SetText((UINT8 *)"Status");
    _pClient->SetHiden();

    memset(_txtWifiInfo, 0x00, 256);
    _pWifiInfo = new StaticText(
        _pScroller->getViewRoot(),
        Point(100, 170),
        Size(220, 40));
    _pWifiInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pWifiInfo->SetText((UINT8 *)_txtWifiInfo);
    _pWifiInfo->SetHiden();

    // to show ap ssid / password / qrcode
    _pSSID = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 170),
        Size(160, 40));
    _pSSID->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pSSID->SetText((UINT8 *)"SSID");
    _pSSID->SetHiden();

    memset(_txtSSID, 0x00, 16);
    _pSSIDValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(160, 170),
        Size(160, 40));
    _pSSIDValue->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pSSIDValue->SetText((UINT8 *)_txtSSID);

    _pPWD = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 250),
        Size(160, 40));
    _pPWD->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pPWD->SetText((UINT8 *)"PWD");
    _pPWD->SetHiden();

    memset(_txtPWD, 0x00, 16);
    _pPWDValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(160, 250),
        Size(160, 40));
    _pPWDValue->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pPWDValue->SetText((UINT8 *)_txtPWD);

    _pQRBG = new UIRectangle(
        _pScroller->getViewRoot(),
        Point(50, 330),
        Size(220, 220),
        Color_White);
    _pQRBG->setCornerRadius(16);

    _pQrCode = new QrCodeControl(
        _pScroller->getViewRoot(),
        Point(60, 340),
        Size(200, 200));
}

void WifiMenuWnd::updateWifi()
{
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
            snprintf(_txtSSID, 16, "%s", ssid);
            snprintf(_txtPWD, 16, "%s", key);
            char tmp[64];
            memset(tmp, 0, sizeof(tmp));
            snprintf(tmp, 64, "<a>%s</a><p>%s</p>", ssid, key);
            //CAMERALOG("wifi info: %s", tmp);
            _pQrCode->setString(tmp);

            _pScroller->setScrollable(true);
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

            _cp->GetWifiCltinfor(ssid, 256);
            if (strcmp(ssid, "Empty List") == 0) {
                snprintf(_txtWifiInfo, 256, "%s", "N/A");
            } else {
                if (strlen(ssid) > 12) {
                    strncpy(_txtWifiInfo, ssid, 10);
                    _txtWifiInfo[10] = '.';
                    _txtWifiInfo[11] = '.';
                    _txtWifiInfo[12] = '\0';
                } else {
                    snprintf(_txtWifiInfo, 256, "%s", ssid);
                }
            }

            _pSSID->SetHiden();
            _pSSIDValue->SetHiden();

            _pPWD->SetHiden();
            _pPWDValue->SetHiden();

            _pQrCode->SetHiden();
            _pQRBG->SetHiden();

            _pScroller->setScrollable(false);
            _pScroller->setViewPort(Point(0, 0), _visibleSizeScroller);
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
            _cp->GetWifiCltinfor(ssid, 256);
            if (strcmp(ssid, "Empty List") == 0) {
                this->StartWnd(WINDOW_wifi_no_connection, Anim_Bottom2TopCoverEnter);
            } else {
                this->StartWnd(WINDOW_wifi_detail, Anim_Bottom2TopCoverEnter);
            }
        }
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

    _pAdvanced = new Button(_pPanel, Point(100, 320), Size(200, 60), Color_Black, Color_Grey_1);
    _pAdvanced->SetText((UINT8 *)"Advanced", 24, Color_Grey_3, FONT_ROBOTOMONO_Bold);
    _pAdvanced->SetTextAlign(CENTER, MIDDLE);
    _pAdvanced->setOnClickListener(this);

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
    _pTxtResult->SetText((UINT8 *)"Change wifi mode\nsuccessfully!");
    _pTxtResult->SetHiden();
}

WifiModeMenuWnd::~WifiModeMenuWnd()
{
    delete _pTxtResult;
    delete _pIconResult;
    delete _pAnimation;

    delete _pAdvanced;
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
    } else if (widget == _pAdvanced) {
        this->StartWnd(WINDOW_autohotspot_menu, Anim_Bottom2TopCoverEnter);
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
                    _pAdvanced->SetHiden();
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
                    _pAdvanced->SetHiden();
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
                        _pAdvanced->SetHiden();
                        _pAnimation->SetHiden();
                        _pIconResult->setSource(
                            "/usr/local/share/ui/BMP/dash_menu/",
                            "feedback-good.bmp");
                        _pIconResult->SetShow();
                        _pTxtResult->SetText((UINT8 *)"Change wifi mode\nsuccessfully!");
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
                            _pAdvanced->SetHiden();
                            _pAnimation->SetHiden();
                            _pIconResult->setSource(
                                "/usr/local/share/ui/BMP/dash_menu/",
                                "feedback-bad.bmp");
                            _pIconResult->SetShow();
                            _pTxtResult->SetText((UINT8 *)"Change wifi mode\ntime out!");
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

    _pInfo = new StaticText(_pPanel, Point(40, 90), Size(320, 160));
    _pInfo->SetStyle(28, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, TOP);
    _pInfo->SetLineHeight(40);
    snprintf(_txt, 256,
        "Enable it to automatically"
        "\n"
        "switch wifi to Hotspot"
        "\n"
        "mode if plugged to a car"
        "\n"
        "mount with power supply.");
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

    snprintf(_txt, 256,
        "Looks like you have't"
        "\n"
        "connected to any wifi"
        "\n"
        "network yet."
        "\n"
        "\n"
        "First you need to switch the"
        "\n"
        "camera to Hotspot mode."
        "\n"
        "Then connect your phone to"
        "\n"
        "camera's wifi, and configure it"
        "\n"
        "from the Waylens mobile app."
        "\n"
        "\n"
        "For more details, please visit"
        "\n"
        "www.waylens.com");
    _pGuide = new StaticText(
        _pScroller->getViewRoot(),
        Point(0, 70),
        Size(_visibleSizeScroller.width, 500));
    _pGuide->SetStyle(24, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, TOP);
    _pGuide->SetLineHeight(40);
    _pGuide->SetText((UINT8 *)_txt);

    _pHotspot = new Button(
        _pScroller->getViewRoot(),
        Point(0, 560),
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
                        _cp->GetWifiCltinfor(ssid, 256);
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

    memset(_txt, 256, 0x00);
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


    memset(_txtInfo, 0x00, 256);
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
        case wifi_mode_Client:
            _cp->GetWifiCltinfor(ssid, 256);
            if (strcmp(ssid, "Empty List") == 0) {
                snprintf(_txt, 256, "%s", "No network added!");
            } else {
                if (strlen(ssid) >= 256) {
                    strncpy(_txt, ssid, 252);
                    _txt[252] = '.';
                    _txt[253] = '.';
                    _txt[254] = '.';
                    _txt[255] = '\0';
                } else {
                    snprintf(_txt, 256, "%s", ssid);
                }
            }
            break;
        default:
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
                snprintf(_txtInfo, 256, "Connecting to\n%s", _ssid);
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
                        snprintf(_txtInfo, 256, "Scan wifi\ntime out!");
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

    return b;
}


BluetoothMenuWnd::BluetoothMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _cntTimeout(0)
    , _targetMode(bt_state_on)
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
        _pHIDTitle->SetColor(Color_Grey_4);
        _pOBDTitle->SetColor(Color_Grey_4);

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
    } else {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
        _pHIDTitle->SetColor(Color_Grey_2);
        _pHIDIcon->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "status-disconnected.bmp");
        _pOBDTitle->SetColor(Color_Grey_2);
        _pOBDIcon->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "status-disconnected.bmp");
    }
}

void BluetoothMenuWnd::onMenuResume()
{
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

void BTDetailWnd::updateBTDetail()
{
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

                if (_state == State_Scanning) {
                    _counter ++;
                    if (_counter >= 200 /* 20s */) {
                        // Scan time out!
                        _counter = 0;
                        parseBT(NULL, _type);
                        _state = State_Got_List;
                        _pTitle->SetShow();
                        _pLine->SetShow();
                        _pList->SetShow();
                        if (_numBTs <= 0) {
                            _pReScan->SetShow();
                            _pHelp->SetShow();
                        }
                        _pTxt->SetHiden();
                        _pIconNo->SetHiden();
                        _pIconYes->SetHiden();
                        _pInfo->SetHiden();
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
                    if (_state == State_Scanning) {
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


DisplayMenuWnd::DisplayMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel,
        Point(150, 0), Size(100, 100),
        Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);


    _pBrghtBtnFake = new Button(_pPanel,
        Point(30, 80), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pBrghtBtnFake->SetStyle(20);
    _pBrghtBtnFake->setOnClickListener(this);

    _pBrght = new StaticText(_pPanel, Point(40, 100), Size(210, 40));
    _pBrght->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pBrght->SetText((UINT8 *)"Brightness");

    strcpy(_txtBrght, "8");
    _pBrghtValue = new StaticText(_pPanel, Point(250, 100), Size(110, 40));
    _pBrghtValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pBrghtValue->SetText((UINT8 *)_txtBrght);


    _pModeBtnFake = new Button(_pPanel,
        Point(30, 160), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pModeBtnFake->SetStyle(20);
    _pModeBtnFake->setOnClickListener(this);

    _pMode = new StaticText(_pPanel, Point(40, 180), Size(220, 40));
    _pMode->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pMode->SetText((UINT8 *)"Screen saver");

    strcpy(_txtMode, "Never");
    _pModeResult = new StaticText(_pPanel, Point(260, 180), Size(100, 40));
    _pModeResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pModeResult->SetText((UINT8 *)_txtMode);


    _pRotateBtnFake = new Button(_pPanel,
        Point(30, 240), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pRotateBtnFake->SetStyle(20);
    _pRotateBtnFake->setOnClickListener(this);

    _pRotate = new StaticText(_pPanel, Point(40, 260), Size(180, 40));
    _pRotate->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pRotate->SetText((UINT8 *)"Rotation");

    strcpy(_txtRotate, "Auto");
    _pRotateResult = new StaticText(_pPanel, Point(220, 260), Size(140, 40));
    _pRotateResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pRotateResult->SetText((UINT8 *)_txtRotate);
}

DisplayMenuWnd::~DisplayMenuWnd()
{
    delete _pRotateResult;
    delete _pRotate;
    delete _pRotateBtnFake;

    delete _pModeResult;
    delete _pMode;
    delete _pModeBtnFake;

    delete _pBrghtValue;
    delete _pBrght;
    delete _pBrghtBtnFake;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void DisplayMenuWnd::updateStatus()
{
    int brightness = _cp->GetBrightness();
    brightness = ((brightness > Brightness_MIN) ? (brightness - Brightness_MIN) : 0)
        / Brightness_Step_Value;
    snprintf(_txtBrght, 16, "%d", brightness);

    char tmp[256];
    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);
    snprintf(_txtMode, 16, "%s", tmp);

    agcmd_property_get(PropName_Rotate_Mode, tmp, Rotate_Mode_Normal);
    if (strcasecmp(tmp, Rotate_Mode_Auto) == 0) {
        snprintf(_txtRotate, 16, "%s", "Auto");
    } else if (strcasecmp(tmp, Rotate_Mode_Normal) == 0) {
        snprintf(_txtRotate, 16, "%s", "Normal");
    } else if (strcasecmp(tmp, Rotate_Mode_180) == 0) {
        snprintf(_txtRotate, 16, "%s", "180");
    }
}

void DisplayMenuWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void DisplayMenuWnd::onMenuPause()
{
}

void DisplayMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pBrghtBtnFake) {
        this->StartWnd(WINDOW_brightnessmenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pModeBtnFake) {
        this->StartWnd(WINDOW_screensaver_settings, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pRotateBtnFake) {
        this->StartWnd(WINDOW_rotation_menu, Anim_Bottom2TopCoverEnter);
    }
}

bool DisplayMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_settings) {
                    updateStatus();
                    _pPanel->Draw(true);
                    b = false;
                } else if (event->_paload == PropertyChangeType_rotation_mode) {
                    updateStatus();
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


RotationModeWnd::RotationModeWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _pTxt(NULL)
    , _pIconYes(NULL)
    , _pIconNo(NULL)
    , _currentMode(Rotation_Mode_None)
    , _targetMode(Rotation_Mode_None)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel,
        Point(150, 0), Size(100, 100),
        Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Rotation");

    _pLine = new UILine(_pPanel, Point(100, 106), Size(200, 4));
    _pLine->setLine(Point(100, 106), Point(300, 106), 4, Color_Grey_2);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)"Auto";
    _pMenus[_numMenus++] = (char *)"Normal";
    _pMenus[_numMenus++] = (char *)"Rotate 180";

    UINT16 itemheight = 60;
    UINT16 padding = 20;
    _pickerSize.width = 240;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding;
    _pPickers = new Pickers(_pPanel,
        Point(80, 250 - _pickerSize.height / 2),
        _pickerSize,
        _pickerSize);
    _pPickers->setItems(_pMenus, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);
}

RotationModeWnd::~RotationModeWnd()
{
    delete _pIconYes;
    delete _pIconNo;
    delete _pTxt;

    delete _pPickers;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void RotationModeWnd::updateStatus()
{
    char rotate_mode[256];
    agcmd_property_get(PropName_Rotate_Mode, rotate_mode, Rotate_Mode_Normal);
    if (strcasecmp(rotate_mode, Rotate_Mode_Auto) == 0) {
        _currentMode = Rotation_Mode_Auto;
        _pPickers->setIndex(0);
    } else if (strcasecmp(rotate_mode, Rotate_Mode_Normal) == 0) {
        _currentMode = Rotation_Mode_Normal;
        _pPickers->setIndex(1);
    } else if (strcasecmp(rotate_mode, Rotate_Mode_180) == 0) {
        _currentMode = Rotation_Mode_180;
        _pPickers->setIndex(2);
    }
}

void RotationModeWnd::showDialogue(bool bRecording)
{
    _pTitle->SetHiden();
    _pLine->SetHiden();
    _pPickers->SetHiden();

    if (!_pTxt) {
        _pTxt = new StaticText(_pPanel, Point(0, 76), Size(400, 120));
        _pTxt->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    }
    if (Rotation_Mode_Auto == _targetMode) {
        _pTxt->SetText(
            (UINT8 *)"Locked if recording,\n"
            "automatically rotate\n"
            "otherwise. Continue?");
    } else {
        if (bRecording) {
            _pTxt->SetText(
                (UINT8 *)"Recording will be\n"
                "stopped after rotation\n"
                "Sure to continue?");
        } else {
            _pTxt->SetText(
                (UINT8 *)"Sure to do rotation?");
        }
    }
    _pTxt->SetShow();

    if (!_pIconNo) {
        _pIconNo = new ImageButton(
            _pPanel,
            Point(56, 216), Size(128, 128),
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-no-up.bmp",
            "btn-dashboard-no-down.bmp");
        _pIconNo->setOnClickListener(this);
    }
    _pIconNo->SetShow();

    if (!_pIconYes) {
        _pIconYes = new ImageButton(
            _pPanel,
            Point(216, 216), Size(128, 128),
            "/usr/local/share/ui/BMP/dash_menu/",
            "btn-dashboard-yes_alt-up.bmp",
            "btn-dashboard-yes_alt-down.bmp");
        _pIconYes->setOnClickListener(this);
    }
    _pIconYes->SetShow();
}

void RotationModeWnd::hideDialogue()
{
    _pTitle->SetShow();
    _pLine->SetShow();
    _pPickers->SetShow();

    if (_pIconYes) {
        _pIconYes->SetHiden();
    }
    if (_pIconNo) {
        _pIconNo->SetHiden();
    }
    if (_pTxt) {
        _pTxt->SetHiden();
    }
}

void RotationModeWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void RotationModeWnd::onMenuPause()
{
}

void RotationModeWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pIconYes) {
        if (Rotation_Mode_Auto == _targetMode) {
            agcmd_property_set(PropName_Rotate_Mode, Rotate_Mode_Auto);
        } else if (Rotation_Mode_Normal == _targetMode) {
            if (_cp->setOSDRotate(0)) {
                agcmd_property_set(PropName_Rotate_Mode, Rotate_Mode_Normal);
            }
        } else if (Rotation_Mode_180 == _targetMode) {
            if (_cp->setOSDRotate(1)) {
                agcmd_property_set(PropName_Rotate_Mode, Rotate_Mode_180);
            }
        }
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pIconNo) {
        hideDialogue();
        _pPanel->Draw(true);
    }
}

void RotationModeWnd::onPickersFocusChanged(Control *picker, int indexFocus)
{
    if (_pPickers == picker) {
        if ((indexFocus >= 0) && (indexFocus < _numMenus)) {
            bool bRecording = false;
            camera_State state = _cp->getRecState();
            if ((camera_State_record == state)
                || (camera_State_marking == state)
                || (camera_State_starting == state)
                || (camera_State_stopping == state))
            {
                bRecording = true;
            }
            if (0 == indexFocus) {
                if (Rotation_Mode_Auto == _currentMode) {
                    this->Close(Anim_Top2BottomExit);
                } else {
                    _targetMode = Rotation_Mode_Auto;
                    showDialogue(bRecording);
                }
            } else if (1 == indexFocus) {
                if (Rotation_Mode_Normal == _currentMode) {
                    this->Close(Anim_Top2BottomExit);
                } else {
                    _targetMode = Rotation_Mode_Normal;
                    showDialogue(bRecording);
                }
            } else if (2 == indexFocus) {
                if (Rotation_Mode_180 == _currentMode) {
                    this->Close(Anim_Top2BottomExit);
                } else {
                    _targetMode = Rotation_Mode_180;
                    showDialogue(bRecording);
                }
            }
            _pPanel->Draw(true);
        }
    }
}

bool RotationModeWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_rotation_mode) {
                    hideDialogue();
                    updateStatus();
                    _pPanel->Draw(true);
                    b = false;
                }
            }
            break;
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) &&
                (event->_paload == TouchFlick_Down))
            {
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


BrightnessMenuWnd::BrightnessMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
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
    _pTitle->SetText((UINT8 *)"Brightness");

    _pLine = new UILine(_pPanel, Point(80, 106), Size(240, 4));
    _pLine->setLine(Point(80, 100), Point(320, 100), 4, Color_Grey_2);

    snprintf(_slVal, 16, "%d", 8);
    _pSlVal = new StaticText(_pPanel, Point(150, 140), Size(100, 88));
    _pSlVal->SetStyle(64, FONT_ROBOTOMONO_Light, Color_White, CENTER, MIDDLE);
    _pSlVal->SetText((UINT8 *)_slVal);

    _pSlider = new Slider(_pPanel, Point(40, 250), Size(320, 10));
    _pSlider->setStepper(10);
    _pSlider->setColor(Color_Grey_2, Color_White);
    _pSlider->setListener(this);

    _pCap = new RoundCap(_pPanel, Point(200, 200), 199, Color_Video_Transparency);
    _pCap->setAngle(0.0);
    _pCap->setRatio(0.7);
    _pCap->SetHiden();
}

BrightnessMenuWnd::~BrightnessMenuWnd()
{
    delete _pCap;
    delete _pSlider;
    delete _pSlVal;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void BrightnessMenuWnd::updateStatus()
{
    int brightness = _cp->GetBrightness();
    brightness = ((brightness > Brightness_MIN) ? (brightness - Brightness_MIN) : 0)
        / Brightness_Step_Value;
    snprintf(_slVal, 16, "%d", brightness);
    _pSlider->setValue(brightness * 10);
}

void BrightnessMenuWnd::onMenuResume()
{
    _pCap->SetHiden();
    updateStatus();
    _pPanel->Draw(true);
}

void BrightnessMenuWnd::onMenuPause()
{
}

void BrightnessMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void BrightnessMenuWnd::onSliderChanged(Control *slider, int percentage)
{
    if (_pSlider == slider) {
        int brightness = _cp->GetBrightness();
        brightness = ((brightness > Brightness_MIN) ? (brightness - Brightness_MIN) : 0)
            / Brightness_Step_Value;
        int newbrightness = percentage / 10;
        if (brightness != newbrightness) {
            _cp->SetDisplayBrightness(newbrightness * Brightness_Step_Value + Brightness_MIN);
            snprintf(_slVal, 16, "%d", newbrightness);
            _pPanel->Draw(true);
        }
    }
}

void BrightnessMenuWnd::onSliderTriggered(Control *slider, bool triggered)
{
    if (_pSlider == slider) {
        if (triggered) {
            _cp->SetFullScreen(true);
            _pCap->SetShow();
            _pPanel->Draw(true);
        } else {
            _pCap->SetHiden();
            _pPanel->Draw(true);
            _cp->SetFullScreen(false);
        }
    }
}

bool BrightnessMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_settings) {
                    updateStatus();
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


ScreenSaverSettingsWnd::ScreenSaverSettingsWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
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

    _pTitle = new StaticText(_pPanel, Point(50, 60), Size(300, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Screen saver");

    _pLine = new UILine(_pPanel, Point(70, 106), Size(260, 4));
    _pLine->setLine(Point(70, 110), Point(330, 110), 4, Color_Grey_2);


    _pOffBtnFake = new Button(_pPanel, Point(30, 130), Size(340, 80), Color_Black, Color_Grey_1);
    _pOffBtnFake->SetStyle(20);
    _pOffBtnFake->setOnClickListener(this);

    _pOffTime = new StaticText(_pPanel, Point(40, 150), Size(210, 40));
    _pOffTime->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pOffTime->SetText((UINT8 *)"Start After");

    strcpy(_txtOff, "8");
    _pOffValue = new StaticText(_pPanel, Point(250, 150), Size(110, 40));
    _pOffValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pOffValue->SetText((UINT8 *)_txtOff);


    _pStyleBtnFake = new Button(_pPanel, Point(30, 210), Size(340, 80), Color_Black, Color_Grey_1);
    _pStyleBtnFake->SetStyle(20);
    _pStyleBtnFake->setOnClickListener(this);

    _pStyle = new StaticText(_pPanel, Point(40, 230), Size(100, 40));
    _pStyle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStyle->SetText((UINT8 *)"Style");

    strcpy(_txtStyle, "Dot");
    _pStyleResult = new StaticText(_pPanel, Point(140, 230), Size(220, 40));
    _pStyleResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pStyleResult->SetText((UINT8 *)_txtStyle);
}

ScreenSaverSettingsWnd::~ScreenSaverSettingsWnd()
{
    delete _pStyleResult;
    delete _pStyle;
    delete _pStyleBtnFake;

    delete _pOffValue;
    delete _pOffTime;
    delete _pOffBtnFake;

    delete _pLine;
    delete _pTitle;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void ScreenSaverSettingsWnd::updateStatus()
{
    char tmp[256];
    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);
    snprintf(_txtOff, 16, "%s", tmp);

    agcmd_property_get(PropName_Screen_saver_style, tmp, Default_Screen_saver_style);
    snprintf(_txtStyle, 16, "%s", tmp);
    if (strcasecmp(_txtOff, "Never") == 0) {
        _pStyleResult->SetColor(Color_Grey_2);
        _pStyleBtnFake->SetHiden();
    } else {
        _pStyleResult->SetColor(Color_White);
        _pStyleBtnFake->SetShow();
    }
}

void ScreenSaverSettingsWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void ScreenSaverSettingsWnd::onMenuPause()
{
}

void ScreenSaverSettingsWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pOffBtnFake) {
        this->StartWnd(WINDOW_displayoffmenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pStyleBtnFake) {
        this->StartWnd(WINDOW_screensaver_style, Anim_Bottom2TopCoverEnter);
    }
}

bool ScreenSaverSettingsWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_settings) {
                    updateStatus();
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


DisplayOffMenuWnd::DisplayOffMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
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
    _pTitle->SetText((UINT8 *)"Start After");

    _pLine = new UILine(_pPanel, Point(70, 106), Size(260, 4));
    _pLine->setLine(Point(70, 100), Point(330, 100), 4, Color_Grey_2);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)"Never";
    _pMenus[_numMenus++] = (char *)"10s";
    _pMenus[_numMenus++] = (char *)"30s";
    _pMenus[_numMenus++] = (char *)"60s";
    _pMenus[_numMenus++] = (char *)"2min";
    _pMenus[_numMenus++] = (char *)"5min";

    UINT16 itemheight = 60;
    UINT16 padding = 2;
    _pickerSize.width = 200;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding + itemheight / 3;
    _pPickers = new Pickers(_pPanel, Point(100, 120), Size(200, 260), _pickerSize);
    _pPickers->setItems(_pMenus, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);
}

DisplayOffMenuWnd::~DisplayOffMenuWnd()
{
    delete _pPickers;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

int DisplayOffMenuWnd::updateStatus()
{
    char tmp[256];
    agcmd_property_get(PropName_Display_Off_Time, tmp, Default_Display_Off_Time);
    int index = 0;
    for (index = 0; index < _numMenus; index++) {
        if (strcasecmp(tmp, _pMenus[index]) == 0) {
            break;
        }
    }
    _pPickers->setIndex(index);

    return index;
}

void DisplayOffMenuWnd::onMenuResume()
{
    int index = updateStatus();
    if (index >= _numMenus / 2) {
        _pPickers->setViewPort(
            Point(0, _pickerSize.height - _pPickers->GetSize().height));
    } else {
        _pPickers->setViewPort(Point(0, 0));
    }
    _pPanel->Draw(true);
}

void DisplayOffMenuWnd::onMenuPause()
{
}

void DisplayOffMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void DisplayOffMenuWnd::onPickersFocusChanged(Control *picker, int indexFocus)

{
    if (_pPickers == picker) {
        if ((indexFocus >= 0) && (indexFocus < _numMenus)) {
            agcmd_property_set(PropName_Display_Off_Time, _pMenus[indexFocus]);
            _pPanel->Draw(true);
            usleep(100 * 1000);
            this->Close(Anim_Top2BottomExit);
        }
    }
}

bool DisplayOffMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_settings) {
                    int index = updateStatus();
                    if (index >= _numMenus / 2) {
                        _pPickers->setViewPort(
                            Point(0, _pickerSize.height - _pPickers->GetSize().height));
                    } else {
                        _pPickers->setViewPort(Point(0, 0));
                    }
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
                if ((y_pos < 120)
                    || (x_pos < 100) || (x_pos > 300))
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


ScreenSaverStyleWnd::ScreenSaverStyleWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
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
    _pTitle->SetText((UINT8 *)"Style");

    _pLine = new UILine(_pPanel, Point(130, 106), Size(140, 4));
    _pLine->setLine(Point(130, 100), Point(270, 100), 4, Color_Grey_2);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)"All Black";
    _pMenus[_numMenus++] = (char *)"Dot";

    UINT16 itemheight = 60;
    UINT16 padding = 40;
    _pickerSize.width = 220;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding;
    _pPickers = new Pickers(_pPanel,
        Point(90, 240 - _pickerSize.height / 2),
        _pickerSize,
        _pickerSize);
    _pPickers->setItems(_pMenus, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);
}

ScreenSaverStyleWnd::~ScreenSaverStyleWnd()
{
    delete _pPickers;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void ScreenSaverStyleWnd::updateMode()
{
    char tmp[256];
    agcmd_property_get(PropName_Screen_saver_style, tmp, Default_Screen_saver_style);
    int index = 0;
    for (index = 0; index < _numMenus; index++) {
        if (strcasecmp(tmp, _pMenus[index]) == 0) {
            break;
        }
    }
    _pPickers->setIndex(index);

    return;
}

void ScreenSaverStyleWnd::onMenuResume()
{
    updateMode();
    _pPanel->Draw(true);
}

void ScreenSaverStyleWnd::onMenuPause()
{
}

void ScreenSaverStyleWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void ScreenSaverStyleWnd::onPickersFocusChanged(Control *picker, int indexFocus)
{
    if (_pPickers == picker) {
        if ((indexFocus >= 0) && (indexFocus < _numMenus)) {
            agcmd_property_set(PropName_Screen_saver_style, _pMenus[indexFocus]);
            _pPanel->Draw(true);
            usleep(100 * 1000);
            this->Close(Anim_Top2BottomExit);
        }
    }
}

bool ScreenSaverStyleWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_settings) {
                    updateMode();
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


AutoOffMenuWnd::AutoOffMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), 0x0000, 0x0000);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 42));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Auto Off");

    _pLine = new UILine(_pPanel, Point(95, 106), Size(210, 4));
    _pLine->setLine(Point(95, 100), Point(305, 100), 4, Color_Grey_2);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)"Never";
    _pMenus[_numMenus++] = (char *)"30s";
    _pMenus[_numMenus++] = (char *)"60s";
    _pMenus[_numMenus++] = (char *)"2min";
    _pMenus[_numMenus++] = (char *)"5min";

    UINT16 itemheight = 60;
    UINT16 padding = 2;
    _pickerSize.width = 200;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding + itemheight / 3;
    _pPickers = new Pickers(_pPanel, Point(100, 120), Size(200, 260), _pickerSize);
    _pPickers->setItems(_pMenus, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);
}

AutoOffMenuWnd::~AutoOffMenuWnd()
{
    delete _pPickers;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

int AutoOffMenuWnd::updateStatus()
{
    char tmp[256];
    agcmd_property_get(PropName_Udc_Power_Delay, tmp, Udc_Power_Delay_Default);
    int index = 0;
    for (index = 0; index < _numMenus; index++) {
        if (strcasecmp(tmp, _pMenus[index]) == 0) {
            break;
        }
    }
    _pPickers->setIndex(index);

    return index;
}

void AutoOffMenuWnd::onMenuResume()
{
    int index = updateStatus();
    if (index >= _numMenus / 2) {
        _pPickers->setViewPort(
            Point(0, _pickerSize.height - _pPickers->GetSize().height));
    } else {
        _pPickers->setViewPort(Point(0, 0));
    }
    _pPanel->Draw(true);
}

void AutoOffMenuWnd::onMenuPause()
{
}

void AutoOffMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void AutoOffMenuWnd::onPickersFocusChanged(Control *picker, int indexFocus)
{
    if (_pPickers == picker) {
        if ((indexFocus >= 0) && (indexFocus < _numMenus)) {
            agcmd_property_set(PropName_Udc_Power_Delay, _pMenus[indexFocus]);
            _pPanel->Draw(true);
            usleep(100 * 1000);
            this->Close(Anim_Top2BottomExit);
        }
    }
}

bool AutoOffMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_settings) {
                    int index = updateStatus();
                    if (index >= _numMenus / 2) {
                        _pPickers->setViewPort(
                            Point(0, _pickerSize.height - _pPickers->GetSize().height));
                    } else {
                        _pPickers->setViewPort(Point(0, 0));
                    }
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
                if ((y_pos < 120)
                    || (x_pos < 100) || (x_pos > 300))
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


StorageMenuWnd::StorageMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _counter(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pTotal = new StaticText(_pPanel, Point(40, 80), Size(160, 40));
    _pTotal->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pTotal->SetText((UINT8 *)"Capacity");

    strcpy(_txtTotle, " ");
    _pTotalResult = new StaticText(_pPanel, Point(200, 80), Size(160, 40));
    _pTotalResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pTotalResult->SetText((UINT8 *)_txtTotle);

    _pHighlight = new StaticText(_pPanel, Point(40, 160), Size(160, 40));
    _pHighlight->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pHighlight->SetText((UINT8 *)"Highlight");

    strcpy(_txtHighlight, " ");
    _pHighlightResult = new StaticText(_pPanel, Point(200, 160), Size(160, 40));
    _pHighlightResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pHighlightResult->SetText((UINT8 *)_txtHighlight);

    _pLoop = new StaticText(_pPanel, Point(40, 240), Size(180, 40));
    _pLoop->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pLoop->SetText((UINT8 *)"Loop Space");

    strcpy(_txtLoop, " ");
    _pLoopResult = new StaticText(_pPanel, Point(220, 240), Size(140, 40));
    _pLoopResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pLoopResult->SetText((UINT8 *)_txtLoop);

    _pFormatBtnFake = new Button(_pPanel,
        Point(30, 200), Size(340, 200),
        Color_Black, Color_Black);
    _pFormatBtnFake->SetStyle(0, 0.0);
    _pFormatBtnFake->setOnClickListener(this);

    _pFormat = new StaticText(_pPanel, Point(100, 320), Size(200, 50));
    _pFormat->SetStyle(32, FONT_ROBOTOMONO_Medium, Color_White, CENTER, MIDDLE);
    _pFormat->SetText((UINT8 *)"Format");
}

StorageMenuWnd::~StorageMenuWnd()
{
    delete _pFormat;
    delete _pFormatBtnFake;

    delete _pLoopResult;
    delete _pLoop;

    delete _pHighlightResult;
    delete _pHighlight;

    delete _pTotalResult;
    delete _pTotal;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void StorageMenuWnd::updateStorageStatus()
{
    storage_State st = _cp->getStorageState();
    if (st == storage_State_ready) {
        int totle = 0, marked = 0, loop = 0;
        _cp->getSpaceInfoMB(totle, marked, loop);
        if (totle < 0) {
            totle = 0;
        }
        if (marked < 0) {
            marked = 0;
        }
        if (loop < 0) {
            loop = 0;
        }

        if (totle > 1000) {
            snprintf(_txtTotle, 16, "%d.%dGB",
                totle / 1000, totle % 1000 / 100);
        } else {
            snprintf(_txtTotle, 16, "%dMB", totle);
        }

        if (marked > 1000) {
            snprintf(_txtHighlight, 16, "%d.%dGB",
                marked / 1000, marked % 1000 / 100);
        } else {
            snprintf(_txtHighlight, 16, "%dMB", marked);
        }

        if (loop > 1000) {
            snprintf(_txtLoop, 16, "%d.%dGB",
                loop / 1000, loop % 1000 / 100);
        } else {
            snprintf(_txtLoop, 16, "%dMB", loop);
        }
    } else {
        strcpy(_txtTotle, "0 GB");
        strcpy(_txtHighlight, "0 GB");
        strcpy(_txtLoop, "0 GB");
    }
    _pPanel->Draw(true);
}

void StorageMenuWnd::onMenuResume()
{
    _counter = 0;
}

void StorageMenuWnd::onMenuPause()
{
}

void StorageMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pFormatBtnFake) {
        this->StartWnd(WINDOW_formatTF, Anim_Bottom2TopCoverEnter);
    }
}

bool StorageMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_counter % 100 == 0) {
                    updateStorageStatus();
                }
                _counter++;
            } else if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    updateStorageStatus();
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


FormatTFWnd::FormatTFWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _formatStep(FormatStep_Idle)
    , _counter(0)
    , _formatCnt(0)
    , _delayToClose(0)
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

    _pTotle = new Circle(_pPanel, Point(200, 200), 132, 16, Color_Grey_2);

    _pLoop = new Arc(_pPanel, Point(200, 200), 132, 16, Color_Red_3);
    _pLoop->setAngles(270.0, 270.5);
    _pLoop->SetHiden();

    _pMark= new Arc(_pPanel, Point(200, 200), 132, 16, Color_Red_2);
    _pMark->setAngles(270.0, 270.5);
    _pMark->SetHiden();

    _pAnimation = new BmpImage(
        _pPanel,
        Point(60, 60), Size(280, 280),
        "/usr/local/share/ui/BMP/animation/format_TF_card-spinner/",
        "format_TF_card-spinner_00000.bmp");
    _pAnimation->SetHiden();

    memset(_txt, 0x00, 64);
    _pFormatInfo = new StaticText(_pPanel, Point(50, 90), Size(300, 80));
    _pFormatInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pFormatInfo->SetLineHeight(40);
    _pFormatInfo->SetText((UINT8 *)_txt);
    _pFormatInfo->SetHiden();

    _pLegend[0] = new UIRectangle(_pPanel, Point(110, 154), Size(16, 16), Color_Red_2);
    _pLegendTxt[0] = new StaticText(_pPanel, Point(140, 150), Size(280, 24));
    _pLegendTxt[0]->SetStyle(18, FONT_ROBOTOMONO_Regular, Color_Grey_4, LEFT, MIDDLE);
    _pLegendTxt[0]->SetText((UINT8 *)"Highlight");

    _pLegend[1] = new UIRectangle(_pPanel, Point(110, 194), Size(16, 16), Color_Red_3);
    _pLegendTxt[1] = new StaticText(_pPanel, Point(140, 190), Size(280, 24));
    _pLegendTxt[1]->SetStyle(18, FONT_ROBOTOMONO_Regular, Color_Grey_4, LEFT, MIDDLE);
    _pLegendTxt[1]->SetText((UINT8 *)"Loop Space");

    _pFormat = new Button(_pPanel, Point(120, 246), Size(160, 48), Color_Black, Color_Grey_1);
    _pFormat->SetText((UINT8 *)"Format", 32, Color_White, FONT_ROBOTOMONO_Bold);
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

FormatTFWnd::~FormatTFWnd()
{
    delete _pNo;
    delete _pYes;
    delete _pFormat;

    delete _pLegendTxt[1];
    delete _pLegend[1];
    delete _pLegendTxt[0];
    delete _pLegend[0];

    delete _pFormatInfo;
    delete _pAnimation;

    delete _pMark;
    delete _pLoop;
    delete _pTotle;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void FormatTFWnd::updateStorageStatus()
{
    if ((FormatStep_Idle == _formatStep) ||
        (FormatStep_NeedCorfirm_1 == _formatStep) ||
        (FormatStep_NeedCorfirm_2 == _formatStep))
    {
        _pFormatInfo->SetHiden();
        _pYes->SetHiden();
        _pNo->SetHiden();
        _pTotle->SetShow();

        storage_State st = _cp->getStorageState();
        if (st == storage_State_ready) {
            for (int i = 0; i < 2; i++) {
                _pLegend[i]->SetShow();
                _pLegendTxt[i]->SetShow();
            }
            _pFormat->SetShow();

            int totle = 0, marked = 0, loop = 0;
            _cp->getSpaceInfoMB(totle, marked, loop);
            if (totle < 0) {
                totle = 0;
            }
            if (marked < 0) {
                marked = 0;
            }
            if (loop < 0) {
                loop = 0;
            }

            float degree_mark = 0.0;
            float end_pos_mark = 270.0;
            if ((marked > 0) && (totle > 0)) {
                if (marked + loop > 0) {
                    degree_mark = 360.0 * marked / (marked + loop);
                }
                end_pos_mark = 270.0 + degree_mark;
                if (end_pos_mark > 360.0) {
                    end_pos_mark = end_pos_mark - 360.0;
                }
                _pMark->setAngles(270.0, end_pos_mark);
                _pMark->SetShow();
            } else {
                _pMark->SetHiden();
            }

            if ((loop > 0) && (totle > 0)) {
                _pLoop->setAngles(end_pos_mark, 270.0);
                _pLoop->SetShow();
            } else {
                _pLoop->SetHiden();
            }

            _pFormatInfo->SetHiden();
        } else if (st == storage_State_noMedia) {
            _pMark->SetHiden();
            _pLoop->SetHiden();
            for (int i = 0; i < 2; i++) {
                _pLegend[i]->SetHiden();
                _pLegendTxt[i]->SetHiden();
            }
            _pFormat->SetHiden();

            strcpy(_txt, "No Card");
            _pFormatInfo->SetColor(Color_Red_1);
            _pFormatInfo->SetRelativePos(Point(50, 160));
            _pFormatInfo->SetShow();
        } else if (st == storage_State_unknown) {
            _pMark->SetHiden();
            _pLoop->SetHiden();
            for (int i = 0; i < 2; i++) {
                _pLegend[i]->SetHiden();
                _pLegendTxt[i]->SetHiden();
            }
            _pFormat->SetHiden();

            strcpy(_txt, "Unknown");
            _pFormatInfo->SetColor(Color_Red_1);
            _pFormatInfo->SetRelativePos(Point(50, 160));
            _pFormatInfo->SetShow();
        } else if (st == storage_State_error) {
            _pMark->SetHiden();
            _pLoop->SetHiden();
            for (int i = 0; i < 2; i++) {
                _pLegend[i]->SetHiden();
                _pLegendTxt[i]->SetHiden();
            }
            _pFormat->SetHiden();

            strcpy(_txt, "Card Error");
            _pFormatInfo->SetColor(Color_Red_1);
            _pFormatInfo->SetRelativePos(Point(50, 160));
            _pFormatInfo->SetShow();
        }
    }
}

void FormatTFWnd::onMenuResume()
{
    _counter = 0;
}

void FormatTFWnd::onMenuPause()
{
}

void FormatTFWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pFormat) {
        camera_State state = _cp->getRecState();
        if ((camera_State_record == state)
            || (camera_State_marking == state)
            || (camera_State_starting == state)
            || (camera_State_stopping == state))
        {
            for (int i = 0; i < 2; i++) {
                _pLegend[i]->SetHiden();
                _pLegendTxt[i]->SetHiden();
            }
            _pFormat->SetHiden();
            strcpy(_txt, "In Recording!");
            _pFormatInfo->SetColor(Color_Red_1);
            _pFormatInfo->SetRelativePos(Point(50, 160));
            _pFormatInfo->SetShow();
            _pPanel->Draw(true);

            _delayToClose = 10;
            _counter = 0;
        } else {
            storage_State st = _cp->getStorageState();
            if (st != storage_State_noMedia) {
                _pTotle->SetHiden();
                _pMark->SetHiden();
                _pLoop->SetHiden();
                for (int i = 0; i < 2; i++) {
                    _pLegend[i]->SetHiden();
                    _pLegendTxt[i]->SetHiden();
                }
                _pFormat->SetHiden();

                snprintf(_txt, 64, "Sure to format\nthe microSD card?");
                _pFormatInfo->SetColor(Color_Grey_4);
                _pFormatInfo->SetRelativePos(Point(50, 90));
                _pFormatInfo->SetShow();
                _pYes->SetShow();
                _pYes->SetRelativePos(Point(216, 216));
                _pNo->SetShow();
                _pNo->SetRelativePos(Point(56, 216));
                _pPanel->Draw(true);
                _formatStep = FormatStep_NeedCorfirm_1;
            }
        }
    } else if (widget == _pYes) {
        if (_formatStep == FormatStep_NeedCorfirm_1) {
            _pTotle->SetHiden();
            _pMark->SetHiden();
            _pLoop->SetHiden();
            for (int i = 0; i < 2; i++) {
                _pLegend[i]->SetHiden();
                _pLegendTxt[i]->SetHiden();
            }
            _pFormat->SetHiden();

            snprintf(_txt, 64, "All data will be\nerased. Proceed?");
            _pFormatInfo->SetColor(Color_Grey_4);
            _pFormatInfo->SetRelativePos(Point(50, 90));
            _pFormatInfo->SetShow();
            _pYes->SetShow();
            _pYes->SetRelativePos(Point(56, 216));
            _pNo->SetShow();
            _pNo->SetRelativePos(Point(216, 216));
            _pPanel->Draw(true);
            _formatStep = FormatStep_NeedCorfirm_2;
        } else if (_formatStep == FormatStep_NeedCorfirm_2) {
            CAMERALOG("Format");
            _cp->FormatStorage();
            snprintf(_txt, 64, "Formatting");
            _pFormatInfo->SetColor(Color_Grey_4);
            _pFormatInfo->SetRelativePos(Point(50, 155));
            _pAnimation->SetShow();
            _pTotle->SetHiden();
            _pMark->SetHiden();
            _pLoop->SetHiden();
            for (int i = 0; i < 2; i++) {
                _pLegend[i]->SetHiden();
                _pLegendTxt[i]->SetHiden();
            }
            _pFormat->SetHiden();
            _pYes->SetHiden();
            _pNo->SetHiden();
            _pPanel->Draw(true);
            _formatStep = FormatStep_Formatting;
        }
    } else if (widget == _pNo) {
        _pTotle->SetShow();
        _pMark->SetShow();
        _pLoop->SetShow();
        for (int i = 0; i < 2; i++) {
            _pLegend[i]->SetShow();
            _pLegendTxt[i]->SetShow();
        }
        _pFormat->SetShow();
        _pFormatInfo->SetHiden();
        _pYes->SetHiden();
        _pNo->SetHiden();
        _pPanel->Draw(true);
        _formatStep = FormatStep_Idle;
    }
}

bool FormatTFWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_delayToClose > 0) {
                    _delayToClose--;
                    if (_delayToClose <= 0) {
                        this->Close(Anim_Top2BottomExit);
                    }
                    b = false;
                } else if ((_formatStep == FormatStep_Idle) && (_counter % 100 == 0)) {
                    updateStorageStatus();
                    _pPanel->Draw(true);
                } else if (_formatStep == FormatStep_Formatting) {
                    char source[64];
                    snprintf(source, 64, "format_TF_card-spinner_%05d.bmp", (_formatCnt++) % 20);
                    //CAMERALOG("source = %s", source);
                    _pAnimation->setSource(
                        "/usr/local/share/ui/BMP/animation/format_TF_card-spinner/",
                        source);
                    _pPanel->Draw(true);
                    b = false;
                }
                _counter++;
            } else if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_storage) {
                    if (_formatStep != FormatStep_Formatting) {
                        _pFormatInfo->SetHiden();
                    }
                    updateStorageStatus();
                    _pPanel->Draw(true);
                    b = false;
                }
            } else if ((event->_event == InternalEvent_app_state_update)) {
                if (event->_paload == camera_State_FormatSD_result) {
                    if (event->_paload1 != false) {
                        snprintf(_txt, 64, "Format Success!");
                        _formatStep = FormatStep_Success;
                        
                    } else {
                        snprintf(_txt, 64, "Format Failed!");
                        _pFormatInfo->SetStyle(28, FONT_ROBOTOMONO_Regular,
                            Color_Red_1, CENTER, MIDDLE);
                        _formatStep = FormatStep_Failed;
                    }
                    updateStorageStatus();
                    _delayToClose = 10;
                    _pAnimation->SetHiden();
                    _pPanel->Draw(true);
                    b = false;
                }
            }
            break;
        case eventType_touch:
            if ((event->_event == TouchEvent_flick)
                && (event->_paload == TouchFlick_Down)
                && (_formatStep != FormatStep_Formatting))
            {
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


AdvancedMenuWnd::AdvancedMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);


    _pVerBtnFake = new Button(_pPanel, Point(30, 90), Size(340, 80), Color_Black, Color_Grey_1);
    _pVerBtnFake->SetStyle(20);
    _pVerBtnFake->setOnClickListener(this);

    _pVersion = new StaticText(_pPanel, Point(50, 110), Size(150, 40));
    _pVersion->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pVersion->SetText((UINT8 *)"Version");

    snprintf(_txtVersionSys, 16, "%s", SERVICE_UPNP_VERSION);
    _pVersionValue = new StaticText(_pPanel, Point(200, 110), Size(150, 40));
    _pVersionValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pVersionValue->SetText((UINT8 *)_txtVersionSys);


    _pResetBtnFake = new Button(_pPanel, Point(30, 170), Size(340, 80), Color_Black, Color_Black);
    _pResetBtnFake->setOnClickListener(this);

    _pFactoryReset = new Button(_pPanel, Point(50, 180), Size(300, 60), Color_Black, Color_Grey_1);
    _pFactoryReset->SetText((UINT8 *)"Reset Camera", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pFactoryReset->SetTextAlign(CENTER, MIDDLE);
    _pFactoryReset->SetStyle(25);
    _pFactoryReset->setOnClickListener(this);


    _pPowerOffBtnFake = new Button(_pPanel, Point(30, 250), Size(340, 80), Color_Black, Color_Black);
    _pPowerOffBtnFake->setOnClickListener(this);

    _pPowerOff = new Button(_pPanel, Point(50, 260), Size(300, 60), Color_Black, Color_Grey_1);
    _pPowerOff->SetText((UINT8 *)"Power Off", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pPowerOff->SetTextAlign(CENTER, MIDDLE);
    _pPowerOff->SetStyle(25);
    _pPowerOff->setOnClickListener(this);
}

AdvancedMenuWnd::~AdvancedMenuWnd()
{
    delete _pPowerOff;
    delete _pPowerOffBtnFake;

    delete _pFactoryReset;
    delete _pResetBtnFake;

    delete _pVersionValue;
    delete _pVersion;
    delete _pVerBtnFake;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void AdvancedMenuWnd::onMenuResume()
{
}

void AdvancedMenuWnd::onMenuPause()
{
}

void AdvancedMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if ((widget == _pVersion)
        || (widget == _pVersionValue)
        || (widget == _pVerBtnFake))
    {
        this->StartWnd(WINDOW_versionmenu, Anim_Bottom2TopCoverEnter);
    } else if ((widget == _pFactoryReset)
        || (widget == _pResetBtnFake))
    {
        this->StartWnd(WINDOW_factory_reset_menu, Anim_Bottom2TopCoverEnter);
    } else if ((widget == _pPowerOff)
        || (widget == _pPowerOffBtnFake))
    {
        this->StartWnd(WINDOW_poweroff, Anim_Bottom2TopCoverEnter);
    }
}

bool AdvancedMenuWnd::OnEvent(CEvent *event)
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


VersionMenuWnd::VersionMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
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
    _pTitle->SetText((UINT8 *)"Version");

    _pLine = new UILine(_pPanel, Point(105, 106), Size(190, 4));
    _pLine->setLine(Point(105, 100), Point(295, 100), 4, Color_Grey_2);

    _pBSP = new StaticText(_pPanel, Point(40, 130), Size(60, 40));
    _pBSP->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pBSP->SetText((UINT8 *)"HW");

    memset(vBsp, 0x00, 32);
    agcmd_property_get(PropName_prebuild_bsp, vBsp + strlen(vBsp), "V0B");
    _pBSPValue = new StaticText(_pPanel, Point(100, 130), Size(260, 40));
    _pBSPValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pBSPValue->SetText((UINT8 *)vBsp);

    _pVAPI = new StaticText(_pPanel, Point(40, 200), Size(60, 40));
    _pVAPI->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pVAPI->SetText((UINT8 *)"API");

    memset(vAPI, 0x00, 32);
    snprintf(vAPI, 32, "%s", SERVICE_UPNP_VERSION);
    _pVAPIValue = new StaticText(_pPanel, Point(100, 200), Size(260, 40));
    _pVAPIValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pVAPIValue->SetText((UINT8 *)vAPI);

    _pVSystem = new StaticText(_pPanel, Point(40, 270), Size(60, 40));
    _pVSystem->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pVSystem->SetText((UINT8 *)"OS");

    memset(vSys, 0x00, 64);
    agcmd_property_get("prebuild.system.version", vSys + strlen(vSys), "Unknown");
    _pVSystemValue = new StaticText(_pPanel, Point(100, 270), Size(260, 40));
    _pVSystemValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pVSystemValue->SetText((UINT8 *)vSys);
}

VersionMenuWnd::~VersionMenuWnd()
{
    delete _pVSystemValue;
    delete _pVSystem;
    delete _pVAPIValue;
    delete _pVAPI;
    delete _pBSPValue;
    delete _pBSP;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void VersionMenuWnd::onMenuResume()
{
}

void VersionMenuWnd::onMenuPause()
{
}

void VersionMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

bool VersionMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                this->Close(Anim_Top2BottomExit);
                b = false;
            } else if (event->_event == TouchEvent_DoubleClick) {
                this->StartWnd(WINDOW_debug_menu, Anim_Bottom2TopCoverEnter);
            }
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}


FactoryResetWnd::FactoryResetWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _resetStep(ResetStep_Idle)
    , _poweroffCnt(0)
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

    snprintf(_txt, 64, "Reset camera to\nfactory settings?");
    _pResetInfo = new StaticText(_pPanel, Point(50, 90), Size(300, 80));
    _pResetInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pResetInfo->SetLineHeight(40);
    _pResetInfo->SetText((UINT8 *)_txt);

    _pYes = new ImageButton(
        _pPanel,
        Point(56, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-yes_alt-up.bmp",
        "btn-dashboard-yes_alt-down.bmp");
    _pYes->setOnClickListener(this);

    _pNo = new ImageButton(
        _pPanel,
        Point(216, 216), Size(128, 128),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-dashboard-no-up.bmp",
        "btn-dashboard-no-down.bmp");
    _pNo->setOnClickListener(this);
}

FactoryResetWnd::~FactoryResetWnd()
{
    delete _pNo;
    delete _pYes;
    delete _pResetInfo;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void FactoryResetWnd::onMenuResume()
{
}

void FactoryResetWnd::onMenuPause()
{
}

void FactoryResetWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pYes) {
        if (_resetStep == ResetStep_Idle) {
            snprintf(_txt, 64, "All data will be\nerased. Proceed?");
            _pYes->SetRelativePos(Point(216, 216));
            _pNo->SetRelativePos(Point(56, 216));
            _pPanel->Draw(true);
            _resetStep = ResetStep_NeedCorfirm;
        } else if (_resetStep == ResetStep_NeedCorfirm) {
            CAMERALOG("Reset");
            _cp->FactoryReset();

            snprintf(_txt, 64, "Resetting ...");
            _pResetInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, LEFT, MIDDLE);
            _pResetInfo->SetSize(Size(230, 80));
            _pResetInfo->SetRelativePos(Point(90, 90));
            _pReturnBtnFake->SetHiden();
            _pReturnBtn->SetHiden();
            _pYes->SetHiden();
            _pNo->SetHiden();
            _pPanel->Draw(true);
            _resetStep = ResetStep_Resetting;

            sleep(1);
            _poweroffCnt = 10;
            CAMERALOG("Call \"agsh reboot\" to finish factory resetting now!");
            const char *tmp_args[4];
            tmp_args[0] = "agsh";
            tmp_args[1] = "reboot";
            tmp_args[2] = NULL;
            agbox_sh(2, (const char *const *)tmp_args);
        }
    } else if (widget == _pNo) {
        if (_resetStep == ResetStep_Idle) {
            this->Close(Anim_Top2BottomExit);
        } else {
            snprintf(_txt, 64, "Reset camera to\nfactory settings?");
            _pYes->SetRelativePos(Point(56, 216));
            _pNo->SetRelativePos(Point(216, 216));
            _pPanel->Draw(true);
            _resetStep = ResetStep_Idle;
        }
    }
}

bool FactoryResetWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                if (_poweroffCnt > 0) {
                    if (_poweroffCnt % 20 == 0) {
                        strcpy(_txt, "Resetting");
                        _pPanel->Draw(true);
                    } else if (_poweroffCnt % 20 == 5) {
                        strcpy(_txt, "Resetting .");
                        _pPanel->Draw(true);
                    } else if (_poweroffCnt % 20 == 10) {
                        strcpy(_txt, "Resetting ..");
                        _pPanel->Draw(true);
                    } else if (_poweroffCnt % 20 == 15) {
                        strcpy(_txt, "Resetting ...");
                        _pPanel->Draw(true);
                    }
                    _poweroffCnt++;
                }
            }
            break;
        case eventType_touch:
            if ((event->_event == TouchEvent_flick)
                && (event->_paload == TouchFlick_Down)
                && (_resetStep != ResetStep_Resetting))
            {
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


TimeDateMenuWnd::TimeDateMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _counter(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);


    _pBtnTimeFake = new Button(_pPanel, Point(30, 90), Size(340, 80), Color_Black, Color_Grey_1);
    _pBtnTimeFake->SetStyle(20);
    _pBtnTimeFake->setOnClickListener(this);

    _pTime = new StaticText(_pPanel, Point(40, 110), Size(120, 40));
    _pTime->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pTime->SetText((UINT8 *)"Time");

    strcpy(_txtTime, "07:30");
    _pTimeResult = new StaticText(_pPanel, Point(160, 110), Size(200, 40));
    _pTimeResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pTimeResult->SetText((UINT8 *)_txtTime);


    _pBtnDateFake = new Button(_pPanel, Point(30, 170), Size(340, 80), Color_Black, Color_Grey_1);
    _pBtnDateFake->SetStyle(20);
    _pBtnDateFake->setOnClickListener(this);

    _pDate = new StaticText(_pPanel, Point(40, 190), Size(150, 40));
    _pDate->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pDate->SetText((UINT8 *)"Date");

    strcpy(_txtDate, "01/01/15");
    _pDateResult = new StaticText(_pPanel, Point(200, 190), Size(160, 40));
    _pDateResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pDateResult->SetText((UINT8 *)_txtDate);


    _pTimeZone = new StaticText(_pPanel, Point(40, 270), Size(150, 40));
    _pTimeZone->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pTimeZone->SetText((UINT8 *)"TimeZone");

    strcpy(_txtTZ, "UTC -08:00");
    _pTZResult = new StaticText(_pPanel, Point(200, 270), Size(160, 40));
    _pTZResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pTZResult->SetText((UINT8 *)_txtTZ);
}

TimeDateMenuWnd::~TimeDateMenuWnd()
{
    delete _pTZResult;
    delete _pTimeZone;
    delete _pDateResult;
    delete _pDate;
    delete _pBtnDateFake;
    delete _pTimeResult;
    delete _pTime;
    delete _pBtnTimeFake;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void TimeDateMenuWnd::updateTime()
{
    struct tm gtime;
    struct timeval now_val;

    gettimeofday(&now_val, NULL);
    gtime=(*localtime(&now_val.tv_sec));

    //CAMERALOG("%02d:%02d %s",
    //    gtime.tm_hour > 12 ? gtime.tm_hour - 12 : gtime.tm_hour,
    //    gtime.tm_min,
    //    gtime.tm_hour > 12 ? "PM" : "AM");
    char format[32];
    agcmd_property_get(PropName_Time_Format, format, "12h");
    if (strcasecmp(format, "12h") == 0) {
        snprintf(_txtTime, 16, "%02d:%02d %s",
            gtime.tm_hour > 12 ? gtime.tm_hour - 12 : gtime.tm_hour,
            gtime.tm_min,
            gtime.tm_hour > 12 ? "PM" : "AM");
    } else {
        snprintf(_txtTime, 16, "%02d:%02d",
            gtime.tm_hour,
            gtime.tm_min);
    }

    agcmd_property_get(PropName_Date_Format, format, "mm/dd/yy");
    if (strcasecmp(format, "mm/dd/yy") == 0) {
        snprintf(_txtDate, 16, "%02d/%02d/%02d",
            1 + gtime.tm_mon,
            gtime.tm_mday,
            (1900 + gtime.tm_year) % 100);
    } else if (strcasecmp(format, "dd/mm/yy") == 0) {
        snprintf(_txtDate, 16, "%02d/%02d/%02d",
            gtime.tm_mday,
            1 + gtime.tm_mon,
            (1900 + gtime.tm_year) % 100);
    } else if (strcasecmp(format, "yy/mm/dd") == 0) {
        snprintf(_txtDate, 16, "%02d/%02d/%02d",
            (1900 + gtime.tm_year) % 100,
            1 + gtime.tm_mon,
            gtime.tm_mday);
    }

    //CAMERALOG("timezone = %ld", timezone);
    int min = timezone / 60;
    snprintf(_txtTZ, 16, "UTC %+02d:%02d", -min / 60, abs(min) % 60);

    _pPanel->Draw(true);
}

void TimeDateMenuWnd::onMenuResume()
{
    updateTime();
    _counter = 0;
}

void TimeDateMenuWnd::onMenuPause()
{
}

void TimeDateMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pBtnTimeFake) {
        this->StartWnd(WINDOW_timeformat, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pBtnDateFake) {
        this->StartWnd(WINDOW_dateformat, Anim_Bottom2TopCoverEnter);
    }
}

bool TimeDateMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter % 100 == 0) {
                    updateTime();
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


TimeFormatMenuWnd::TimeFormatMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _counter(0)
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
    _pTitle->SetText((UINT8 *)"Time");

    _pLine = new UILine(_pPanel, Point(130, 106), Size(140, 4));
    _pLine->setLine(Point(130, 100), Point(270, 100), 4, Color_Grey_2);

    strcpy(_txtTime, "07:30");
    _pTime = new StaticText(_pPanel, Point(0, 140), Size(400, 100));
    _pTime->SetStyle(64, FONT_ROBOTOMONO_Light, Color_Grey_3, CENTER, MIDDLE);
    _pTime->SetText((UINT8 *)_txtTime);

    _pSwitchFake = new Button(
        _pPanel,
        Point(30, 240), Size(340, 100),
       Color_Black, Color_Grey_1);
    _pSwitchFake->SetStyle(20);
    _pSwitchFake->setOnClickListener(this);

    _pStatus = new StaticText(
        _pPanel,
        Point(50, 260), Size(300, 60));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"24-Hour");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(294, 274), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");
}

TimeFormatMenuWnd::~TimeFormatMenuWnd()
{
    delete _pSwitch;
    delete _pStatus;
    delete _pSwitchFake;
    delete _pTime;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void TimeFormatMenuWnd::updateTime()
{
    struct tm gtime;
    struct timeval now_val;

    gettimeofday(&now_val, NULL);
    gtime=(*localtime(&now_val.tv_sec));

    char format[32];
    agcmd_property_get(PropName_Time_Format, format, "12h");
    if (strcasecmp(format, "12h") == 0) {
        snprintf(_txtTime, 16, "%02d:%02d %s",
            gtime.tm_hour > 12 ? gtime.tm_hour - 12 : gtime.tm_hour,
            gtime.tm_min,
            gtime.tm_hour > 12 ? "PM" : "AM");
    } else {
        snprintf(_txtTime, 16, "%02d:%02d",
            gtime.tm_hour,
            gtime.tm_min);
    }

    _pPanel->Draw(true);
}

void TimeFormatMenuWnd::onMenuResume()
{
    char format[32];
    agcmd_property_get(PropName_Time_Format, format, "12h");
    if (strcasecmp(format, "24h") == 0) {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }
    updateTime();
    _counter = 0;
}

void TimeFormatMenuWnd::onMenuPause()
{
}

void TimeFormatMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pSwitchFake) {
        char format[32];
        agcmd_property_get(PropName_Time_Format, format, "12h");
        if (strcasecmp(format, "12h") == 0) {
            agcmd_property_set(PropName_Time_Format, "24h");
            _pSwitch->setSource(
                "/usr/local/share/ui/BMP/dash_menu/",
                "toggle_on.bmp");
        } else {
            agcmd_property_set(PropName_Time_Format, "12h");
            _pSwitch->setSource(
                "/usr/local/share/ui/BMP/dash_menu/",
                "toggle_off.bmp");
        }
        updateTime();
    }
}

bool TimeFormatMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter % 100 == 0) {
                    updateTime();
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


DateFormatMenuWnd::DateFormatMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _counter(0)
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
    _pTitle->SetText((UINT8 *)"Date");

    _pLine = new UILine(_pPanel, Point(130, 106), Size(140, 4));
    _pLine->setLine(Point(130, 100), Point(270, 100), 4, Color_Grey_2);

    strcpy(_txtDate, "01/01/15");
    _pDate = new StaticText(_pPanel, Point(0, 140), Size(400, 100));
    _pDate->SetStyle(64, FONT_ROBOTOMONO_Light, Color_Grey_3, CENTER, MIDDLE);
    _pDate->SetText((UINT8 *)_txtDate);

    _pSwitchFake = new Button(
        _pPanel,
        Point(30, 240), Size(340, 100),
       Color_Black, Color_Grey_1);
    _pSwitchFake->SetStyle(20);
    _pSwitchFake->setOnClickListener(this);

    _pStatus = new StaticText(_pPanel, Point(50, 260), Size(120, 60));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"Format");

    strcpy(_dateFormat, "mm/dd/yy");
    _pFormat = new StaticText(_pPanel, Point(170, 260), Size(200, 60));
    _pFormat->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pFormat->SetText((UINT8 *)_dateFormat);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)"mm/dd/yy";
    _pMenus[_numMenus++] = (char *)"dd/mm/yy";
    _pMenus[_numMenus++] = (char *)"yy/mm/dd";

    UINT16 itemheight = 60;
    UINT16 padding = 10;
    _pickerSize.width = 240;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding;
    _pPickers = new Pickers(_pPanel,
        Point(80, 200 - _pickerSize.height / 2),
        _pickerSize,
        _pickerSize);
    _pPickers->setItems(_pMenus, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);
    _pPickers->SetHiden();
}

DateFormatMenuWnd::~DateFormatMenuWnd()
{
    delete _pPickers;
    delete _pFormat;
    delete _pStatus;
    delete _pSwitchFake;
    delete _pDate;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void DateFormatMenuWnd::updateTime()
{
    struct tm gtime;
    struct timeval now_val;

    gettimeofday(&now_val, NULL);
    gtime=(*localtime(&now_val.tv_sec));

    char format[32];
    agcmd_property_get(PropName_Date_Format, format, "mm/dd/yy");
    strcpy(_dateFormat, format);

    if (strcasecmp(format, "mm/dd/yy") == 0) {
        snprintf(_txtDate, 16, "%02d/%02d/%02d",
            1 + gtime.tm_mon,
            gtime.tm_mday,
            (1900 + gtime.tm_year) % 100);
    } else if (strcasecmp(format, "dd/mm/yy") == 0) {
        snprintf(_txtDate, 16, "%02d/%02d/%02d",
            gtime.tm_mday,
            1 + gtime.tm_mon,
            (1900 + gtime.tm_year) % 100);
    } else if (strcasecmp(format, "yy/mm/dd") == 0) {
        snprintf(_txtDate, 16, "%02d/%02d/%02d",
            (1900 + gtime.tm_year) % 100,
            1 + gtime.tm_mon,
            gtime.tm_mday);
    }

    _pPanel->Draw(true);
}

void DateFormatMenuWnd::onMenuResume()
{
    updateTime();
    _counter = 0;
}

void DateFormatMenuWnd::onMenuPause()
{
}

void DateFormatMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pSwitchFake) {
        char tmp[256];
        agcmd_property_get(PropName_Date_Format, tmp, "mm/dd/yy");
        int index = 0;
        for (index = 0; index < _numMenus; index++) {
            if (strcasecmp(tmp, _pMenus[index]) == 0) {
                break;
            }
        }
        _pPickers->setIndex(index);

        _pPickers->SetShow();
        _pFormat->SetHiden();
        _pStatus->SetHiden();
        _pSwitchFake->SetHiden();
        _pDate->SetHiden();
        _pLine->SetHiden();
        _pTitle->SetHiden();
        _pReturnBtn->SetHiden();
        _pReturnBtnFake->SetHiden();
        _pPanel->Draw(true);
    }
}

void DateFormatMenuWnd::onPickersFocusChanged(Control *picker, int indexFocus)
{
    if (_pPickers == picker) {
        if ((indexFocus >= 0) && (indexFocus < _numMenus)) {
            agcmd_property_set(PropName_Date_Format, _pMenus[indexFocus]);
            _pPickers->SetHiden();
            _pFormat->SetShow();
            _pStatus->SetShow();
            _pSwitchFake->SetShow();
            _pDate->SetShow();
            _pLine->SetShow();
            _pTitle->SetShow();
            _pReturnBtn->SetShow();
            _pReturnBtnFake->SetShow();

            // TODO: delay more gracefully
            updateTime();
        }
    }
}

bool DateFormatMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counter++;
                if (_counter % 100 == 0) {
                    updateTime();
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


OBDMenuWnd::OBDMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
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

    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 80));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"OBD\nPreferences");

    _pLine = new UILine(_pPanel, Point(70, 136), Size(260, 4));
    _pLine->setLine(Point(70, 100), Point(330, 100), 4, Color_Grey_2);

    _pStatusBtnFake = new Button(_pPanel, Point(30, 200), Size(340, 80), Color_Black, Color_Grey_1);
    _pStatusBtnFake->SetStyle(20);
    _pStatusBtnFake->setOnClickListener(this);

    _pStatus = new StaticText(_pPanel, Point(40, 220), Size(200, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"OBD Demo");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(304, 224), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");
}

OBDMenuWnd::~OBDMenuWnd()
{
    delete _pSwitch;
    delete _pStatus;
    delete _pStatusBtnFake;

    delete _pLine;
    delete _pTitle;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void OBDMenuWnd::_updateStatus()
{
    char tmp[256];
    agcmd_property_get(PropName_Settings_OBD_Demo, tmp, "off");
    if (strcasecmp(tmp, "on") == 0) {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }
}

void OBDMenuWnd::onMenuResume()
{
    _updateStatus();
    _pPanel->Draw(true);
}

void OBDMenuWnd::onMenuPause()
{
}

void OBDMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pStatusBtnFake) {
        char tmp[256];
        agcmd_property_get(PropName_Settings_OBD_Demo, tmp, "off");
        if (strcasecmp(tmp, "on") == 0) {
            agcmd_property_set(PropName_Settings_OBD_Demo, "off");
        } else {
            agcmd_property_set(PropName_Settings_OBD_Demo, "on");
        }
        _updateStatus();
        _pPanel->Draw(true);
    }
}

bool OBDMenuWnd::OnEvent(CEvent *event)
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


ScreenSaverWnd::ScreenSaverWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(name, wndSize, leftTop)
    , _cp(cp)
    , _counter(0)
    , _style(Style_Bubble_Clock)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 20);
    this->SetMainObject(_pPanel);

    _pBubble = new Round(_pPanel, Point(200, 24), 8, Color_Grey_4);
    _pBubble->SetHiden();
}

ScreenSaverWnd::~ScreenSaverWnd()
{
    delete _pBubble;
    delete _pPanel;
}

void ScreenSaverWnd::_updateStatus()
{
    float angle = -6 * (_counter / 10) / 180.0 * M_PI;
    int x = 176 * sin(angle);
    int y = 176 * cos(angle);
    _pBubble->SetRelativePos(Point(200 - x, 200 - y));

    camera_State state = _cp->getRecState();
    if ((camera_State_record == state)
        || (camera_State_marking== state)
        || (camera_State_starting== state))
    {
        _pBubble->setColor(Color_Red_1);
    } else {
        _pBubble->setColor(Color_Grey_4);
    }
}

void ScreenSaverWnd::onResume()
{
    char tmp[256];
    agcmd_property_get(PropName_Screen_saver_style, tmp, Default_Screen_saver_style);
    if (strcasecmp(tmp, "All Black") == 0) {
        _style = Style_Display_Off;
        _pBubble->SetHiden();
        _cp->SetDisplayBrightness(0);
    } else {
        _style = Style_Bubble_Clock;
        _pBubble->SetShow();
        _updateStatus();
        _pPanel->Draw(true);
    }
}

void ScreenSaverWnd::onPause()
{
}

bool ScreenSaverWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if ((event->_event == InternalEvent_app_timer_update)) {
                if (Style_Bubble_Clock == _style) {
                    _counter++;
                    if (_counter % 10 == 0) {
                        _updateStatus();
                        _pPanel->Draw(true);

                        if (_counter % 600 == 0) {
                            _counter = 0;
                        }
                    }
                }
            }
            break;
        case eventType_touch:
            if (event->_event == TouchEvent_Move_Ended) {
                char tmp[256];
                agcmd_property_get(PropName_Brighness, tmp, Default_Brighness);
                int brightness = atoi(tmp);
                _cp->SetDisplayBrightness(brightness);
                this->Close(Anim_Null);
            }
            b = false;
            break;
        case eventType_key:
            switch(event->_event)
            {
                case key_ok_onKeyDown:
                    {
                        char tmp[256];
                        agcmd_property_get(PropName_Brighness, tmp, Default_Brighness);
                        int brightness = atoi(tmp);
                        _cp->SetDisplayBrightness(brightness);
                        this->Close(Anim_Null);
                    }
                    b = false;
                    break;
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
                char tmp[256];
                agcmd_property_get(PropName_Brighness, tmp, Default_Brighness);
                int brightness = atoi(tmp);
                _cp->SetDisplayBrightness(brightness);
                this->Close(Anim_Null);

                camera_State state = _cp->getRecState();
                if ((camera_State_record != state)
                    && (camera_State_marking != state)
                    && (camera_State_starting != state))
                {
                    _cp->startRecMode();
                }

                b = false;
            } else if (event->_event == MountEvent_UnPlug_From_Car_Mount) {
                CAMERALOG("unplugged from car mount!");
                this->Close(Anim_Null);
                b = false;
            } else if ((event->_event == MountEvent_Plug_To_USB)
                && (event->_paload == 1))
            {
                CAMERALOG("USB mount is plugged!");
                this->Close(Anim_Null);
                b = false;
            }
            break;
    }

    return b;
}

};

