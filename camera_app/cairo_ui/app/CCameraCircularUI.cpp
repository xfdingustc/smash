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

extern bool bReleaseVersion;

extern bool is_no_write_record();

using namespace rapidjson;

namespace ui {

LapTimerFragment::LapTimerFragment(
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
    _pTitle->SetText((UINT8 *)"Lap Timer");

    _pLine = new UILine(this, Point(110, 100), Size(180, 4));
    _pLine->setLine(Point(110, 100), Point(290, 100), 4, Color_Grey_2);

    _pLapTimer = new ImageButton(
        this,
        Point(136, 136), Size(128, 128),
        "/usr/local/share/ui/BMP/performance_test/",
        "btn-shortcut-lap_timer-up.bmp",
        "btn-shortcut-lap_timer-down.bmp");
    _pLapTimer->setOnClickListener(this);

    _pScores = new Button(this, Point(50, 280), Size(300, 80), Color_Black, Color_Grey_1);
    _pScores->SetStyle(40);
    _pScores->SetText((UINT8 *)"Last Record", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pScores->SetTextAlign(CENTER, MIDDLE);
    _pScores->setOnClickListener(this);
}

LapTimerFragment::~LapTimerFragment()
{
    delete _pScores;
    delete _pLapTimer;
    delete _pLine;
    delete _pTitle;
}

void LapTimerFragment::onResume()
{
}

void LapTimerFragment::onPause()
{
}

void LapTimerFragment::onClick(Control *widget)
{
    if (widget == _pLapTimer) {
        this->GetOwner()->StartWnd(WINDOW_laptimer, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pScores) {
        this->GetOwner()->StartWnd(WINDOW_lap_score, Anim_Bottom2TopCoverEnter);
    }
}


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
        "btn-shortcut-performance-countdown-down.bmp");
    _pCountdownMode->setOnClickListener(this);

    _pAutoMode = new ImageButton(
        this,
        Point(224, 136), Size(128, 128),
        "/usr/local/share/ui/BMP/performance_test/",
        "btn-shortcut-performance-auto-up.bmp",
        "btn-shortcut-performance-auto-down.bmp");
    _pAutoMode->setOnClickListener(this);

    _pScores = new Button(this, Point(50, 280), Size(300, 80), Color_Black, Color_Grey_1);
    _pScores->SetStyle(40);
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
    , _pSpaceWarning(NULL)
    , _pBtnWarning(NULL)
    , _counter(0)
    , _bSpaceWarning(false)
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
    delete _pBtnWarning;
    delete _pSpaceWarning;
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
                        int totle = 0, marked = 0, spaceMB = 0;
                        _cp->getSpaceInfoMB(totle, marked, spaceMB);
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

    if (_bSpaceWarning) {
        Point pos = _pRecTxt->GetTxtEndPos();
        if (!_pSpaceWarning) {
            _pSpaceWarning = new BmpImage(
                this,
                pos, Size(24, 24),
                "/usr/local/share/ui/BMP/liveview/",
                "status_notification.bmp");
        }
        if (_pSpaceWarning) {
            Point posW = pos;
            posW.x = pos.x + 2;
            posW.y = pos.y + 4;
            _pSpaceWarning->SetRelativePos(posW);
            _pSpaceWarning->SetShow();
        }
    } else {
        if (_pSpaceWarning) {
            _pSpaceWarning->SetHiden();
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

void RecordFragment::checkSpaceWarning()
{
    // check whether there is enough space for loop recording:
    _bSpaceWarning = false;
    storage_State st = _cp->getStorageState();
    if (storage_State_full == st) {
        _bSpaceWarning = true;
    } else if (storage_State_ready == st) {
        int totle = 0, marked = 0, loop = 0;
        _cp->getSpaceInfoMB(totle, marked, loop);
        if ((totle > 10 * 1024) && (loop <= 8 * 1024)) {
            _bSpaceWarning = true;
        } else if ((totle > 0) && (totle < 10 * 1024) && (loop <= 6 * 1024)) {
            _bSpaceWarning = true;
        }
    }

    if (_bSpaceWarning) {
        if (!_pBtnWarning) {
            _pBtnWarning = new Button(this,
                Point(50, 0), Size(300, 120),
                Color_Black, Color_Black);
            _pBtnWarning->SetStyle(0, 0.0);
            _pBtnWarning->setOnClickListener(this);
        }
        if (_pBtnWarning) {
            _pBtnWarning->SetShow();
        }
    } else {
        if (_pBtnWarning) {
            _pBtnWarning->SetHiden();
        }
    }
}

void RecordFragment::onResume()
{
    updateRecStatus();
    updateRecButton();
    checkSpaceWarning();
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
            ((storage_State_full == st) && _cp->isCircleMode()) ||
            is_no_write_record())
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
    } else if (_pBtnWarning == widget) {
        if (_bSpaceWarning) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_Space_Warning_guide);
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
                if (_counter % 100 == 0) {
                    checkSpaceWarning();
                }

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
                    checkSpaceWarning();
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
    if ((strcasecmp(tmp, "Normal") == 0) ||
        (strcasecmp(tmp, "Manual") == 0))
    {
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
    if ((strcasecmp(tmp, "Normal") == 0) ||
        (strcasecmp(tmp, "Manual") == 0))
    {
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
        if ((strcasecmp(tmp, "Normal") == 0) ||
            (strcasecmp(tmp, "Manual") == 0))
        {
            agcmd_property_get(Prop_Spk_Volume, tmp, DEFAULT_SPK_VOLUME);
            if (strcasecmp(tmp, "0") == 0) {
                snprintf(tmp, sizeof(tmp), "%s", DEFAULT_SPK_VOLUME);
            }
            agcmd_property_set(Settings_Spk_Volume, tmp);
            _cp->SetSpkVolume(0);
        } else if (strcasecmp(tmp, "Mute") == 0) {
            agcmd_property_get(Settings_Spk_Volume, tmp, DEFAULT_SPK_VOLUME);
            _cp->SetSpkVolume(atoi(tmp));
        }
    } else if (_pMicmute == widget) {
        char tmp[32];
        agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
        if ((strcasecmp(tmp, "Normal") == 0) ||
            (strcasecmp(tmp, "Manual") == 0))
        {
            agcmd_property_get(Prop_Mic_Volume, tmp, DEFAULT_MIC_VOLUME);
            if (strcasecmp(tmp, "0") == 0) {
                snprintf(tmp, sizeof(tmp), "%s", DEFAULT_MIC_VOLUME);
            }
            agcmd_property_set(Settings_Mic_Volume, tmp);
            _cp->SetMicVolume(0);
            CAudioPlayer::getInstance()->playSoundEffect(CAudioPlayer::SE_Alert);
        } else if (strcasecmp(tmp, "Mute") == 0) {
            agcmd_property_get(Settings_Mic_Volume, tmp, DEFAULT_MIC_VOLUME);
            _cp->SetMicVolume(atoi(tmp));
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
  , _indexShortcuts(0)
  , _cntScreenSaver(0)
  , _screenSaverTimeSetting(0)
{
    memset(_pPages, 0x00, sizeof(_pPages));

    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 4);
    this->SetMainObject(_pPanel);

    _pViewPager = new ViewPager("ShotcutsViewPager",
        _pPanel, this, Point(0, 0), Size(400, 400), 20);

    _numPages = 0;

    if (!bReleaseVersion) {
        _pPages[_numPages++] = new LapTimerFragment(
            "LapTimerFragment",
            _pViewPager, this,
            Point(0, 0), Size(400, 400), 20, cp);
    }

    _pPages[_numPages++] = new FastFuriousFragment(
        "FastFuriousFragment",
        _pViewPager, this,
        Point(0, 0), Size(400, 400), 20, cp);

    _indexFocus = _numPages;
    _indexShortcuts = _numPages;
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
    delete _pPages[3];
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
    _indexFocus = _indexShortcuts; // ShotcutsFragment
    _pViewPager->setFocus(_indexFocus);
    _pViewPager->getCurFragment()->onResume();
    _pIndicator->setHighlight(_indexFocus);
#endif
}

void ViewPagerShotcutsWnd::onPause()
{
    _cntScreenSaver = 0;

#if 1
    _pViewPager->getCurFragment()->onPause();
#else
    _pViewPager->setFocus(_indexShortcuts);
    _pViewPager->getCurFragment()->onPause();
    _pIndicator->setHighlight(_indexShortcuts);
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
                this->Close(Anim_Bottom2TopExit, false);
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
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _temperature = 0;
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
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
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _boost = 0;
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
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
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _maf = 0;
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
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

    _pUnit = new BmpImage(
        this,
        Point(264, 180), Size(64, 40),
        "/usr/local/share/ui/BMP/gauge/",
        "label-x1000.bmp");
    _pUnit->SetHiden();

    _pNum_d = new BmpImage(
        this,
        Point(148, 140), Size(52, 120),
        "/usr/local/share/ui/BMP/gauge/",
        "N.bmp");

    _pPoint = new BmpImage(
        this,
        Point(190, 230), Size(19, 30),
        "/usr/local/share/ui/BMP/performance_test/",
        "sign-144-point.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 140), Size(52, 120),
        "/usr/local/share/ui/BMP/gauge/",
        "A.bmp");
}

RPMFragment::~RPMFragment()
{
    delete _pNum_c;
    delete _pPoint;
    delete _pNum_d;

    delete _pUnit;
    delete _pLabel_demo;
    delete _pLabel;

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
    int step = 12000 / 40;
    if (_bTita) {
        _rpm += step;
        if (_rpm > 12000) {
            _rpm = 12000;
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
        _pPoint->SetHiden();

        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", "N.bmp");
        _pNum_d->SetRelativePos(Point(148, 140));

        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", "A.bmp");
        _pNum_c->SetRelativePos(Point(200, 140));

        _pUnit->SetHiden();
        _pRRPM->SetHiden();

        return;
    }

    _pUnit->SetShow();
    _pRRPM->SetShow();

    if (_rpm >= 10000) {
        _pRRPM->setParams(Point(200, 200), 199, 0xE9C5);
    } else if (_rpm >= 6000) {
        _pRRPM->setParams(Point(200, 200), 132 + 67 * _rpm / 10000.0, 0xE9C5);
    } else {
        _pRRPM->setParams(Point(200, 200), 132 + 67 * _rpm / 10000.0, 0xFFE3);
    }

    char name[64];
    if (_rpm >= 10000) {
        _pPoint->SetHiden();
        _pNum_d->SetRelativePos(Point(148, 140));
        _pNum_c->SetRelativePos(Point(200, 140));

        int num = _rpm / 10000;
        snprintf(name, sizeof(name), "num-144-%d.bmp", num % 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);

        num = (_rpm % 10000) / 1000;
        snprintf(name, sizeof(name), "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
    } else {
        _pPoint->SetShow();
        _pNum_d->SetRelativePos(Point(138, 140));
        _pNum_c->SetRelativePos(Point(210, 140));

        int num = _rpm / 1000;
        snprintf(name, sizeof(name), "num-144-%d.bmp", num % 10);
        _pNum_d->setSource("/usr/local/share/ui/BMP/gauge/", name);

        num = (_rpm % 1000) / 100;
        snprintf(name, sizeof(name), "num-144-%d.bmp", num % 10);
        _pNum_c->setSource("/usr/local/share/ui/BMP/gauge/", name);
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
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _rpm = 0;
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
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
    , _paggps_client(NULL)
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

    if (_paggps_client) {
        aggps_close_client(_paggps_client);
        _paggps_client = NULL;
    }
}

bool SpeedFragment::getSpeedGPS(int &speedKmh)
{
    if (_cp->getGPSState() != gps_state_ready) {
        return false;
    }

    if (_paggps_client == NULL) {
        _paggps_client = aggps_open_client();
        if (_paggps_client == NULL) {
            return false;
        }
    }

    int ret_val = aggps_read_client_tmo(_paggps_client, 0);
    if (ret_val) {
        return false;
    }

    struct aggps_location_info_s *plocation = &_paggps_client->pgps_info->location;
    if (plocation->set & AGGPS_SET_SPEED)
    {
        speedKmh = plocation->speed;
        //CAMERALOG("#### speed = %dKm/h", speedKmh);
        return true;
    }

    return false;
}

bool SpeedFragment::getSpeedOBD(int &speedKmh)
{
    bool got = _cp->getSpeedKmh(speedKmh);
    if (!got) {
        return false;
    }
    //CAMERALOG("#### got = %d, speed = %dKm/h", got, speedKmh);

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

bool SpeedFragment::readSpeedData()
{
    bool got = false;
    int speedKmh = 0;
#if 0
    if (getSpeedGPS(speedKmh)) {
        got = true;
        if (speedKmh < 5) {
            int speedKmhOBD = 0;
            if (getSpeedOBD(speedKmhOBD)) {
                if (speedKmhOBD != 0) {
                    speedKmh = speedKmhOBD;
                }
            }
        }
    } else {
        if (_bObdPaired) {
            got = getSpeedOBD(speedKmh);
        }
    }
#else
    if (_bObdPaired) {
        got = getSpeedOBD(speedKmh);
    }
#endif
    if (got) {
        if (_bImperialUnits) {
            // convert to mph
            speedKmh = speedKmh / 1.609 + 0.5;
        }
        if (speedKmh == _speed) {
            if (_speed == 0) {
                _bChanged = true;
            } else {
                _bChanged = false;
            }
        } else {
            _speed = speedKmh;
            _bChanged = true;
        }
    } else {
        if (_bDemoEnabled) {
            got = getSpeedFake();
        }
    }

    //CAMERALOG("#### got = %d, _speed = %d", got, _speed);
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
    int threshold = 160;
    if (_bImperialUnits) {
        threshold = 100;
    }
    if (_speed >= 200) {
        _pRSpeed->setParams(Point(200, 200), 199, 0xE9C5);
    } else if (_speed >= threshold) {
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
        _pLabel->setSource("/usr/local/share/ui/BMP/gauge/", "label-kmh.bmp");
    }

    checkOBDPaired();

    //CAMERALOG("%s: _bObdPaired = %d, _bDemoEnabled = %d",
    //    getFragName(), _bObdPaired, _bDemoEnabled);
    if (!_bObdPaired) {
        if (_bDemoEnabled && (_cp->getGPSState() != gps_state_ready)) {
            _pLabel->SetRelativePos(Point(133, 260));
            _pLabel_demo->SetRelativePos(Point(133, 282));
            _pLabel_demo->SetShow();
        } else {
            _pLabel_demo->SetHiden();
            _pLabel->SetRelativePos(Point(133, 270));
            _speed = 0;
        }
    } else {
        _pLabel_demo->SetHiden();
        _pLabel->SetRelativePos(Point(133, 270));
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
                if (readSpeedData()) {
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
                        if (_bDemoEnabled && (_cp->getGPSState() != gps_state_ready)) {
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
    , _pSpaceWarning(NULL)
    , _pBtnWarning(NULL)
    , _counter(0)
    , _bFullScreen(false)
    , _bConnectingOBD(false)
    , _bConnectingRC(false)
    , _bSpaceWarning(false)
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
    delete _pBtnWarning;
    delete _pSpaceWarning;
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
        UINT64 hh =  tt / 3600;
        UINT64 mm = (tt % 3600) / 60;
        UINT64 ss = (tt % 3600) % 60;
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
                        int totle = 0, marked = 0, spaceMB = 0;
                        _cp->getSpaceInfoMB(totle, marked, spaceMB);
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

    if (_bFullScreen) {
        if (_pSpaceWarning) {
            _pSpaceWarning->SetHiden();
        }
        if (_pBtnWarning) {
            _pBtnWarning->SetHiden();
        }
    } else {
        if (_bSpaceWarning) {
            Point pos = _pTxt->GetTxtEndPos();
            if (!_pSpaceWarning) {
                _pSpaceWarning = new BmpImage(
                    this,
                    pos, Size(24, 24),
                    "/usr/local/share/ui/BMP/liveview/",
                    "status_notification.bmp");
            }
            if (_pSpaceWarning) {
                Point posW = pos;
                posW.x = pos.x + 2;
                posW.y = pos.y + 4;
                _pSpaceWarning->SetRelativePos(posW);
                _pSpaceWarning->SetShow();
            }
        } else {
            if (_pSpaceWarning) {
                _pSpaceWarning->SetHiden();
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
            _cp->GetWifiCltInfor(ssid, 32);
            if ((strcmp(ssid, "Empty List") == 0)
                || (strcmp(ssid, "Searching") == 0)
                || (strcmp(ssid, "Connecting") == 0))
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
        _bConnectingOBD = false;
        _bConnectingRC = false;
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
        //CAMERALOG("%s = %s", propBTTempOBDStatus, tmp);
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

        if (strcasecmp(tmp, "Connecting") == 0) {
            _bConnectingOBD = true;
        } else {
            _bConnectingOBD = false;
        }

        agcmd_property_get(propBTTempHIDStatus, tmp, "");
        if (strcasecmp(tmp, "Connecting") == 0) {
            _bConnectingRC = true;
        } else {
            _bConnectingRC = false;
        }
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

        _bConnectingOBD = false;
        _bConnectingRC = false;
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
            int ret_val = aggps_read_client_tmo(paggps_client, 0);
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

void LiveViewFragment::checkSpaceWarning()
{
    // check whether there is enough space for loop recording:
    _bSpaceWarning = false;
    storage_State st = _cp->getStorageState();
    if (storage_State_full == st) {
        _bSpaceWarning = true;
    } else if (storage_State_ready == st) {
        int totle = 0, marked = 0, loop = 0;
        _cp->getSpaceInfoMB(totle, marked, loop);
        if ((totle > 10 * 1024) && (loop <= 8 * 1024)) {
            _bSpaceWarning = true;
        } else if ((totle > 0) && (totle < 10 * 1024) && (loop <= 6 * 1024)) {
            _bSpaceWarning = true;
        }
    }

    if (_bSpaceWarning) {
        if (!_pBtnWarning) {
            _pBtnWarning = new Button(this,
                Point(50, 0), Size(300, 120),
                Color_Black, Color_Black);
            _pBtnWarning->SetStyle(0, 0.0);
            _pBtnWarning->setOnClickListener(this);
        }
        if (_pBtnWarning) {
            _pBtnWarning->SetShow();
        }
    } else {
        if (_pBtnWarning) {
            _pBtnWarning->SetHiden();
        }
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
    _counter = 1;
    checkTempratureStatus();

    updateRecTime();
    updateBattery();
    updateWifi();
    updateBluetooth();
    updateGPS();
    updateMicSpk();
    checkSpaceWarning();
    this->Draw(true);
}

void LiveViewFragment::onPause()
{
}

void LiveViewFragment::onClick(Control *widget)
{
    if (widget == _pBtnWarning) {
        if (_bSpaceWarning) {
            CameraAppl::getInstance()->PopupDialogue(DialogueType_Space_Warning_guide);
        }
    }
}

bool LiveViewFragment::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                bool bReDraw = false;

                if (_bConnectingOBD && !_bFullScreen) {
                    // blink OBD icon when connecting
                    if (_counter / 10 % 2 == 0) {
                        if ((_counter % 10 == 0) ||
                            (_counter % 10 == 4) ||
                            (_counter % 10 == 8))
                        {
                            _pOBDStatus->setSource(
                                "/usr/local/share/ui/BMP/liveview/",
                                "status-obd-on.bmp");
                            bReDraw = true;
                        } else if ((_counter % 10 == 2) ||
                            (_counter % 10 == 6) ||
                            (_counter % 10 == 9))
                        {
                            _pOBDStatus->setSource(
                                "/usr/local/share/ui/BMP/liveview/",
                                "status-obd-off.bmp");
                            bReDraw = true;
                        }
                    }
                }

                if (_bConnectingRC && !_bFullScreen) {
                    // blink RC icon when connecting
                    if (_counter / 10 % 2 == 0) {
                        if ((_counter % 10 == 0) ||
                            (_counter % 10 == 4) ||
                            (_counter % 10 == 8))
                        {
                            _pRemoteStatus->setSource(
                                "/usr/local/share/ui/BMP/liveview/",
                                "status-remote-on.bmp");
                            bReDraw = true;
                        } else if ((_counter % 10 == 2) ||
                            (_counter % 10 == 6) ||
                            (_counter % 10 == 9))
                        {
                            _pRemoteStatus->setSource(
                                "/usr/local/share/ui/BMP/liveview/",
                                "status-remote-off.bmp");
                            bReDraw = true;
                        }
                    }
                }

                if (_counter % 50 == 0) {
                    updateGPS();
                    bReDraw = true;
                }

                if (_counter % 100 == 0) {
                    wifi_mode mode = _cp->getWifiMode();
                    if (wifi_mode_Client == mode) {
                        char ssid[32];
                        bool ret = _cp->GetWifiCltInfor(ssid, 32);
                        if (ret) {
                            updateWifi();
                            bReDraw = true;
                        } else {
                            CameraAppl::getInstance()->PopupDialogue(
                                DialogueType_Wifi_pwd_wrong);
                        }
                    }

                    checkSpaceWarning();
                }

                if (_TempratureStat == TempratureStat_Overheat) {
                    if (_counter % 10 == 0) {
                        updateTempAlert();
                        bReDraw = true;
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
                        bReDraw = true;
                    }
                } else {
                    if (_counter % 50 == 0) {
                        updateRecTime();
                        bReDraw = true;
                    }
                }

                // check temperature every minute
                if (_counter % (10 * 60) == 0) {
                    checkTempratureStatus();
                }

                if (bReDraw) {
                    this->Draw(true);
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
                    checkSpaceWarning();
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
            if (false/*event->_event == TouchEvent_DoubleClick*/) {
                _bFullScreen = !_bFullScreen;
                if (_bFullScreen) {
                    _pRecStatus->SetHiden();
                    _pTxt->SetHiden();
                    if (_pSpaceWarning) {
                        _pSpaceWarning->SetHiden();
                    }
                    if (_pBtnWarning) {
                        _pBtnWarning->SetHiden();
                    }
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
                        if (_pSpaceWarning) {
                            _pSpaceWarning->SetHiden();
                        }
                        if (_pBtnWarning) {
                            _pBtnWarning->SetHiden();
                        }
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
    , CThread("PitchRollFragment_thread")
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
        "num-64-0.bmp");

    _pPitchNum_d = new BmpImage(
        this,
        Point(200, 146), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");

    _pPitchNum_c = new BmpImage(
        this,
        Point(222, 146), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");

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
        "num-64-0.bmp");
    _pRollNum_h->SetHiden();

    _pRollNum_d = new BmpImage(
        this,
        Point(200, 204), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");

    _pRollNum_c = new BmpImage(
        this,
        Point(222, 204), Size(22, 50),
        "/usr/local/share/ui/BMP/gauge/",
        "num-64-0.bmp");

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

void PitchRollFragment::main()
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
  , CThread("PitchFragment_thread")
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
        "num-144-0.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-0.bmp");

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

void PitchFragment::main()
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
  , CThread("RollFragment_thread")
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
        "num-144-0.bmp");

    _pNum_d = new BmpImage(
        this,
        Point(148, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-0.bmp");

    _pNum_c = new BmpImage(
        this,
        Point(200, 130), Size(52, 140),
        "/usr/local/share/ui/BMP/gauge/",
        "num-144-0.bmp");

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

void RollFragment::main()
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
    , _accel_AB(2.0) // an unreasonable value, so that first time it got updated
    , _accel_LR(2.0)
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
        Point(136, 169), Size(24, 27),
        "/usr/local/share/ui/BMP/gauge/",
        "label-ab.bmp");

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
        Point(136, 227), Size(24, 27),
        "/usr/local/share/ui/BMP/gauge/",
        "label-lr.bmp");

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
    if ((_accel_AB - z <= threshold) && (_accel_AB - z >= -1 * threshold)) {
        xChanged = false;
    } else {
        _accel_AB = z;
        xChanged = true;
    }

    bool yChanged = false;
    if ((_accel_LR - x <= threshold) && (_accel_LR - x >= -1 * threshold)) {
        yChanged = false;
    } else {
        _accel_LR = -x;
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
    float g_force = sqrt(_accel_AB * _accel_AB + _accel_LR * _accel_LR);
    float MAX_g_force = 1.0;
    if (g_force == 0.0) {
        _pRAccel->setParams(Point(200, 200), 130 + 2, 0xFFE3);
        _pRAccel->SetRelativePos(Point(200, 200));
    } else {
        int r_diff = (g_force > MAX_g_force ? MAX_g_force : g_force) / MAX_g_force * _rMaxDiff;
        int x = r_diff * (_bRotate180 ? -1 : 1) * _accel_LR / g_force;
        int y = r_diff * _accel_AB / g_force;;
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
    float accel = (_bRotate180 ? -1 : 1) * _accel_AB;
    bool plus = accel > 0.0 ? true : false;
    bool overthreshold = (_accel_AB >= 0.01) || (_accel_AB <= -0.01);
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
    accel = _accel_LR;
    plus = _accel_LR > 0.0 ? true : false;
    overthreshold = (_accel_LR >= 0.01) || (_accel_LR <= -0.01);
    if (overthreshold) {
        if (plus) {
            accel = _accel_LR;
            _pYPlus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-plus-active.bmp");
            _pYMinus->setSource("/usr/local/share/ui/BMP/gauge/",
                "sign-64-minus-inactive.bmp");
        } else {
            accel = -1 * _accel_LR;
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
    , _obdGaugeFlags(0x00)
    , _cntScreenSaver(0)
    , _screenSaverTimeSetting(0)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 4);
    this->SetMainObject(_pPanel);

    _pViewPager = new ViewPager("GaugesViewPager",
        _pPanel, this, Point(0, 0), Size(400, 400), 2);

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
    _obdGaugeFlags = _getOBDGaugeFlags();
    memset(_pOBDGauges, 0x00, sizeof(_pOBDGauges));
    _loadOBDGauges();

    _indexOBDStart = _numPages;
    for (int i = 0; i < 5; i++) {
        if (_pOBDGauges[i]) {
            _pPages[_numPages++] = _pOBDGauges[i];
        }
    }
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

UINT8 GaugesWnd::_getOBDGaugeFlags()
{
    UINT8 flags = 0x00;

    char tmp[256];
    agcmd_property_get(PropName_Settings_Temp_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        flags |= 0x01;
    }

    agcmd_property_get(PropName_Settings_Boost_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        flags |= 0x01 << 1;
    }

    agcmd_property_get(PropName_Settings_MAF_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        flags |= 0x01 << 2;
    }

    agcmd_property_get(PropName_Settings_RPM_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        flags |= 0x01 << 3;
    }

    agcmd_property_get(PropName_Settings_Speed_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        flags |= 0x01 << 4;
    }

    //CAMERALOG("obd gauge flags = 0x%x", flags);
    return flags;
}

void GaugesWnd::_loadOBDGauges()
{
    char tmp[256];
    agcmd_property_get(PropName_Settings_Temp_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        if (!_pOBDGauges[0]) {
            _pOBDGauges[0] = new CoolantTempFragment(
                "CoolantTempFragment",
                _pViewPager, this,
                Point(0, 0), Size(400, 400), 20, _cp);
        }
    } else {
        if (_pOBDGauges[0]) {
            delete _pOBDGauges[0];
            _pOBDGauges[0] = NULL;
        }
    }

    agcmd_property_get(PropName_Settings_Boost_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        if (!_pOBDGauges[1]) {
            _pOBDGauges[1] = new BoostFragment(
                "BoostFragment",
                _pViewPager, this,
                Point(0, 0), Size(400, 400), 20, _cp);
        }
    } else {
        if (_pOBDGauges[1]) {
            delete _pOBDGauges[1];
            _pOBDGauges[1] = NULL;
        }
    }

    agcmd_property_get(PropName_Settings_MAF_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        if (!_pOBDGauges[2]) {
            _pOBDGauges[2] = new MAFFragment(
                "MAFFragment",
                _pViewPager, this,
                Point(0, 0), Size(400, 400), 20, _cp);
        }
    } else {
        if (_pOBDGauges[2]) {
            delete _pOBDGauges[2];
            _pOBDGauges[2] = NULL;
        }
    }

    agcmd_property_get(PropName_Settings_RPM_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        if (!_pOBDGauges[3]) {
            _pOBDGauges[3] = new RPMFragment(
                "RPMFragment",
                _pViewPager, this,
                Point(0, 0), Size(400, 400), 20, _cp);
        }
    } else {
        if (_pOBDGauges[3]) {
            delete _pOBDGauges[3];
            _pOBDGauges[3] = NULL;
        }
    }

    agcmd_property_get(PropName_Settings_Speed_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        if (!_pOBDGauges[4]) {
            _pOBDGauges[4] = new SpeedFragment(
                "SpeedFragment",
                _pViewPager, this,
                Point(0, 0), Size(400, 400), 20, _cp);
        }
    } else {
        if (_pOBDGauges[4]) {
            delete _pOBDGauges[4];
            _pOBDGauges[4] = NULL;
        }
    }
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

    UINT8 flags = _getOBDGaugeFlags();
    if (_obdGaugeFlags != flags) {
        //CAMERALOG("obd gauge flags changed!");
        _obdGaugeFlags = flags;
        _loadOBDGauges();
        _numPages = _indexOBDStart;
        for (int i = 0; i < 5; i++) {
            if (_pOBDGauges[i]) {
                _pPages[_numPages++] = _pOBDGauges[i];
            }
        }
        _pViewPager->setPages(_pPages, _numPages);
        _pViewPager->setListener(this);
        _indexFocus = _liveviewIndex; // live view
        _pViewPager->setFocus(_indexFocus);

        delete _pIndicator;
        _pIndicator = new TabIndicator(
            _pPanel, Point(120, 382), Size(160, 8),
            _numPages, _liveviewIndex,
            4, 0x9CF3, 0x3187);
        _pIndicator->setHighlight(_liveviewIndex);
    } else {
        _pViewPager->getCurFragment()->onResume();
        _pIndicator->setHighlight(_indexFocus);
    }
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
                this->StartWnd(WINDOW_shotcuts, Anim_Top2BottomCoverEnter);
                b = false;
            } else if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_UP)) {
                //this->StartWnd(WINDOW_settings, Anim_Bottom2TopEnter);
                this->StartWnd(WINDOW_settingsgauges, Anim_Bottom2TopCoverEnter);
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
    _pMenuWifi->SetText((UINT8 *)"Wi-Fi", 32, Color_White, FONT_ROBOTOMONO_Bold);
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
        this->GetOwner()->StartWnd(WINDOW_videomenu, Anim_Bottom2TopCoverEnter);
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
    _pMenuStorage = new Button(this, Point(80, 15), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuStorage->SetText((UINT8 *)"Storage", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuStorage->SetStyle(35, 1.0);
    _pMenuStorage->setOnClickListener(this);

    _pMenuGauges = new Button(this, Point(80, 87), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuGauges->SetText((UINT8 *)"OBD Gauges", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuGauges->SetStyle(35, 1.0);
    _pMenuGauges->setOnClickListener(this);

    _pMenuDateTime = new Button(this, Point(80, 159), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuDateTime->SetText((UINT8 *)"Date & Time", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuDateTime->SetStyle(35, 1.0);
    _pMenuDateTime->setOnClickListener(this);

    _pMenuMisc = new Button(this, Point(80, 231), Size(240, 72), Color_Black, Color_Grey_1);
    _pMenuMisc->SetText((UINT8 *)"System", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pMenuMisc->SetStyle(35, 1.0);
    _pMenuMisc->setOnClickListener(this);
}

Menu_P2_Fragment::~Menu_P2_Fragment()
{
    delete _pMenuMisc;
    delete _pMenuDateTime;
    delete _pMenuGauges;
    delete _pMenuStorage;
}

void Menu_P2_Fragment::onClick(Control *widget)
{
    if (widget == _pMenuStorage) {
        this->GetOwner()->StartWnd(WINDOW_storagemenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pMenuGauges) {
        this->GetOwner()->StartWnd(WINDOW_gauges_preferences, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pMenuDateTime) {
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

    _pViewPager = new ViewPager("SettingsViewPager",
        _pPanel, this, Point(0, 0), Size(400, 400), 20);

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

#if 1
    _pViewPager->getCurFragment()->onResume();
    _pIndicator->setHighlight(_indexFocus);
#else
    _pViewPager->setFocus(0);
    _pViewPager->getCurFragment()->onResume();
    _pIndicator->setHighlight(0);
#endif
}

void SettingsGaugesWnd::onPause()
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
                this->Close(Anim_Top2BottomExit, false);
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

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 100), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);


    _pResolutionBtnFake = new Button(_pPanel, Point(30, 120), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pResolutionBtnFake->SetStyle(20);
    _pResolutionBtnFake->setOnClickListener(this);

    _pResolution = new StaticText(_pPanel, Point(40, 140), Size(200, 40));
    _pResolution->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pResolution->SetText((UINT8 *)"Resolution");

    strcpy(_txtResolution, "");
    _pResolutionResult = new StaticText(_pPanel, Point(240, 140), Size(120, 40));
    _pResolutionResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pResolutionResult->SetText((UINT8 *)_txtResolution);


    _pQualityBtnFake = new Button(_pPanel, Point(30, 200), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pQualityBtnFake->SetStyle(20);
    _pQualityBtnFake->setOnClickListener(this);

    _pQuality = new StaticText(_pPanel, Point(40, 220), Size(120, 40));
    _pQuality->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pQuality->SetText((UINT8 *)"Quality");

    strcpy(_txtQuality, "");
    _pQualityResult = new StaticText(_pPanel, Point(160, 220), Size(200, 40));
    _pQualityResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pQualityResult->SetText((UINT8 *)_txtQuality);
}

VideoMenuWnd::~VideoMenuWnd()
{
    delete _pResolutionResult;
    delete _pResolution;
    delete _pResolutionBtnFake;

    delete _pQualityResult;
    delete _pQuality;
    delete _pQualityBtnFake;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void VideoMenuWnd::_updateStatus()
{
    agcmd_property_get(PropName_rec_quality, _txtQuality, Quality_Normal);

    agcmd_property_get(PropName_rec_resoluton, _txtResolution, "1080p30");
    if (strcasecmp(_txtResolution, "1080p23976") == 0) {
        snprintf(_txtResolution, sizeof(_txtResolution), "1080p24");
    }
}

void VideoMenuWnd::onMenuResume()
{
    _updateStatus();
    _pPanel->Draw(true);
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
    } else if (widget == _pQualityBtnFake) {
        this->StartWnd(WINDOW_videoquality, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pResolutionBtnFake) {
        this->StartWnd(WINDOW_videoresolution, Anim_Bottom2TopCoverEnter);
    }
}

bool VideoMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_resolution) {
                    _updateStatus();
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
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 42));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Quality");

    _pLine = new UILine(_pPanel, Point(105, 106), Size(190, 4));
    _pLine->setLine(Point(105, 100), Point(295, 100), 4, Color_Grey_2);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)Quality_Normal;
    _pMenus[_numMenus++] = (char *)Quality_High;

    UINT16 itemheight = 60;
    UINT16 padding = 20;
    _pickerSize.width = 240;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding;
    _pPickers = new Pickers(_pPanel,
        Point(80, 220 - _pickerSize.height / 2),
        _pickerSize,
        _pickerSize);
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
    agcmd_property_get(PropName_rec_quality, resolution, Quality_Normal);
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
        agcmd_property_set(PropName_rec_quality, _pMenus[_targetIndex]);
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
        updateStatus();
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
                agcmd_property_set(PropName_rec_quality, _pMenus[indexFocus]);
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


VideoResolutionWnd::VideoResolutionWnd(
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
        "btn-menu_down-up.bmp",
        "btn-menu_down-down.bmp");
    _pReturnBtn->setOnClickListener(this);

    _pTitle = new StaticText(_pPanel, Point(100, 60), Size(200, 42));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Resolution");

    _pLine = new UILine(_pPanel, Point(90, 106), Size(220, 4));
    _pLine->setLine(Point(90, 100), Point(310, 100), 4, Color_Grey_2);

    bool bSupport2397 = is2397Enabled();

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)"1080p60";
    _pMenus[_numMenus++] = (char *)"1080p30";
    if (bSupport2397) {
        _pMenus[_numMenus++] = (char *)"1080p23976";
    }
    _pMenus[_numMenus++] = (char *)"720p120";
    _pMenus[_numMenus++] = (char *)"720p60";
    _pMenus[_numMenus++] = (char *)"720p30";

    char quality[32];
    agcmd_property_get(PropName_rec_quality, quality, Quality_Normal);
    int index = 0;
    if (strcasecmp(quality, Quality_Normal) == 0) {
        _pResolutions[index++] = (char *)"1080p60 28Mbps";
        _pResolutions[index++] = (char *)"1080p30 18Mbps";
        if (bSupport2397) {
            _pResolutions[index++] = (char *)"1080p24 18Mbps";
        }
        _pResolutions[index++] = (char *)"720p120 28Mbps";
        _pResolutions[index++] = (char *)"720p60 18Mbps";
        _pResolutions[index++] = (char *)"720p30 12Mbps";
    } else {
        _pResolutions[index++] = (char *)"1080p60 38Mbps";
        _pResolutions[index++] = (char *)"1080p30 22Mbps";
        if (bSupport2397) {
            _pResolutions[index++] = (char *)"1080p24 22Mbps";
        }
        _pResolutions[index++] = (char *)"720p120 38Mbps";
        _pResolutions[index++] = (char *)"720p60 22Mbps";
        _pResolutions[index++] = (char *)"720p30 14Mbps";
    }

    UINT16 itemheight = 60;
    UINT16 padding = 2;
    _pickerSize.width = 320;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding + itemheight / 3;
    _pPickers = new Pickers(_pPanel, Point(40, 120), Size(320, 260), _pickerSize);
    _pPickers->setItems(_pResolutions, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);
}

VideoResolutionWnd::~VideoResolutionWnd()
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

bool VideoResolutionWnd::is2397Enabled()
{
    bool support = false;

    char tmp[64];
    agcmd_property_get(PropName_support_2397_fps, tmp, Disabled_2397_fps);
    if (strcasecmp(tmp, Enabled_2397_fps) == 0) {
        support = true;
    }

    return support;
}

int VideoResolutionWnd::updateStatus()
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

void VideoResolutionWnd::showDialogue()
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

void VideoResolutionWnd::hideDialogue()
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

void VideoResolutionWnd::onMenuResume()
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

void VideoResolutionWnd::onMenuPause()
{
}

void VideoResolutionWnd::onClick(Control *widget)
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
        updateStatus();
        _pPanel->Draw(true);
    }
}

void VideoResolutionWnd::onPickersFocusChanged(Control *picker, int indexFocus)

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

bool VideoResolutionWnd::OnEvent(CEvent *event)
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

    _pReturnBtnFake = new Button(_pPanel, Point(150, 0), Size(100, 60), Color_Black, Color_Black);
    _pReturnBtnFake->setOnClickListener(this);

    _pReturnBtn = new ImageButton(
        _pPanel,
        Point(176, 10), Size(48, 48),
        "/usr/local/share/ui/BMP/dash_menu/",
        "btn-menu_cancel-up.bmp",
        "btn-menu_cancel-down.bmp");
    _pReturnBtn->setOnClickListener(this);


    _pSwitchBtnFake = new Button(_pPanel, Point(30, 60), Size(340, 70), Color_Black, Color_Grey_1);
    _pSwitchBtnFake->SetStyle(20);
    _pSwitchBtnFake->setOnClickListener(this);

    _pStatus = new StaticText(_pPanel, Point(40, 75), Size(210, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"Speaker");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(304, 79), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");


    _pVolumeBtnFake = new Button(_pPanel, Point(30, 130), Size(340, 70), Color_Black, Color_Grey_1);
    _pVolumeBtnFake->SetStyle(20);
    _pVolumeBtnFake->setOnClickListener(this);

    _pVolume = new StaticText(_pPanel, Point(40, 145), Size(250, 40));
    _pVolume->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pVolume->SetText((UINT8 *)"Speaker Volume");

    snprintf(_txtVolume, 16, "0");
    _pVolumeResult = new StaticText(_pPanel, Point(290, 145), Size(70, 40));
    _pVolumeResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pVolumeResult->SetText((UINT8 *)_txtVolume);


    _pMicSwitchBtnFake = new Button(_pPanel, Point(30, 200), Size(340, 70), Color_Black, Color_Grey_1);
    _pMicSwitchBtnFake->SetStyle(20);
    _pMicSwitchBtnFake->setOnClickListener(this);

    _pMicStatus = new StaticText(_pPanel, Point(40, 215), Size(210, 40));
    _pMicStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pMicStatus->SetText((UINT8 *)"Microphone");

    _pMicSwitch = new BmpImage(
        _pPanel,
        Point(304, 219), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");

    _pMicVolumeBtnFake = new Button(_pPanel, Point(30, 270), Size(340, 70), Color_Black, Color_Grey_1);
    _pMicVolumeBtnFake->SetStyle(20);
    _pMicVolumeBtnFake->setOnClickListener(this);

    _pMicVolume = new StaticText(_pPanel, Point(40, 285), Size(250, 40));
    _pMicVolume->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pMicVolume->SetText((UINT8 *)"Mic. Volume");

    snprintf(_txtMicVolume, 16, "0");
    _pMicVolumeResult = new StaticText(_pPanel, Point(290, 285), Size(70, 40));
    _pMicVolumeResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pMicVolumeResult->SetText((UINT8 *)_txtMicVolume);
}

AudioMenuWnd::~AudioMenuWnd()
{
    delete _pMicVolumeResult;
    delete _pMicVolume;
    delete _pMicVolumeBtnFake;

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
    if ((strcasecmp(tmp, "Normal") == 0) ||
        (strcasecmp(tmp, "Manual") == 0))
    {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
        _pVolumeResult->SetColor(Color_White);
    } else if (strcasecmp(tmp, "Mute") == 0) {
        _pSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
        _pVolumeResult->SetColor(Color_Grey_4);
    }

    agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
    if ((strcasecmp(tmp, "Normal") == 0) ||
        (strcasecmp(tmp, "Manual") == 0))
    {
        _pMicSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
        _pMicVolumeResult->SetColor(Color_White);
    } else if (strcasecmp(tmp, "Mute") == 0) {
        _pMicSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
        _pMicVolumeResult->SetColor(Color_Grey_4);
    }

    volume = 0;
    agcmd_property_get(Prop_Mic_Volume, tmp, DEFAULT_MIC_VOLUME);
    volume = atoi(tmp);
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }
    snprintf(_txtMicVolume, 16, "%d", volume * 10);

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
        if ((strcasecmp(tmp, "Normal") == 0) ||
            (strcasecmp(tmp, "Manual") == 0))
        {
            agcmd_property_get(Prop_Spk_Volume, tmp, DEFAULT_SPK_VOLUME);
            if (strcasecmp(tmp, "0") == 0) {
                snprintf(tmp, sizeof(tmp), "%s", DEFAULT_SPK_VOLUME);
            }
            agcmd_property_set(Settings_Spk_Volume, tmp);
            _cp->SetSpkVolume(0);
        } else if (strcasecmp(tmp, "Mute") == 0) {
            agcmd_property_get(Settings_Spk_Volume, tmp, DEFAULT_SPK_VOLUME);
            _cp->SetSpkVolume(atoi(tmp));
        }
    } else if (widget == _pVolumeBtnFake) {
        char tmp[256];
        agcmd_property_get(Prop_Spk_Status, tmp, "Normal");
        if (strcasecmp(tmp, "Mute") != 0) {
            this->StartWnd(WINDOW_spkvolume, Anim_Bottom2TopCoverEnter);
        }
    } else if (widget == _pMicSwitchBtnFake) {
        char tmp[256];
        agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
        if ((strcasecmp(tmp, "Normal") == 0) ||
            (strcasecmp(tmp, "Manual") == 0))
        {
            agcmd_property_get(Prop_Mic_Volume, tmp, DEFAULT_MIC_VOLUME);
            if (strcasecmp(tmp, "0") == 0) {
                snprintf(tmp, sizeof(tmp), "%s", DEFAULT_MIC_VOLUME);
            }
            agcmd_property_set(Settings_Mic_Volume, tmp);
            _cp->SetMicVolume(0);
        } else if (strcasecmp(tmp, "Mute") == 0) {
            agcmd_property_get(Settings_Mic_Volume, tmp, DEFAULT_MIC_VOLUME);
            _cp->SetMicVolume(atoi(tmp));
        }
    } else if (widget == _pMicVolumeBtnFake) {
        char tmp[256];
        agcmd_property_get(Prop_Mic_Status, tmp, "Normal");
        if (strcasecmp(tmp, "Mute") != 0) {
            this->StartWnd(WINDOW_micvolume, Anim_Bottom2TopCoverEnter);
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

    _pInfo = new StaticText(_pPanel, Point(50, 300), Size(300, 40));
    _pInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pInfo->SetText((UINT8 *)"Default 80");
}

SpeakerVolumeMenuWnd::~SpeakerVolumeMenuWnd()
{
    delete _pInfo;
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
            char vol_bak[32];
            if (newvolume == 0) {
                snprintf(vol_bak, sizeof(vol_bak), "%s", DEFAULT_SPK_VOLUME);
            } else {
                snprintf(vol_bak, sizeof(vol_bak), "%d", newvolume);
            }
            agcmd_property_set(Settings_Spk_Volume, vol_bak);
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
            if ((strcasecmp(tmp, "Normal") == 0) ||
                (strcasecmp(tmp, "Manual") == 0))
            {
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


MicVolumeMenuWnd::MicVolumeMenuWnd(
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

    _pTitle = new StaticText(_pPanel, Point(0, 60), Size(400, 42));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)"Mic. Volume");

    _pLine = new UILine(_pPanel, Point(80, 106), Size(240, 4));
    _pLine->setLine(Point(80, 100), Point(320, 100), 4, Color_Grey_2);

    snprintf(_slVal, 16, "%d", 80);
    _pSlVal = new StaticText(_pPanel, Point(100, 140), Size(200, 88));
    _pSlVal->SetStyle(64, FONT_ROBOTOMONO_Light, Color_White, CENTER, MIDDLE);
    _pSlVal->SetText((UINT8 *)_slVal);

    _pSlider = new Slider(_pPanel, Point(40, 250), Size(320, 10));
    _pSlider->setStepper(10);
    _pSlider->setColor(Color_Grey_2, Color_White);
    _pSlider->setListener(this);

    _pInfo = new StaticText(_pPanel, Point(50, 300), Size(300, 40));
    _pInfo->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_4, CENTER, MIDDLE);
    _pInfo->SetText((UINT8 *)"Default 80");
}

MicVolumeMenuWnd::~MicVolumeMenuWnd()
{
    delete _pInfo;
    delete _pSlider;
    delete _pSlVal;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void MicVolumeMenuWnd::updateMicVolumeDisplay()
{
    char tmp[256];
    int volume = 0;
    agcmd_property_get(Prop_Mic_Volume, tmp, DEFAULT_MIC_VOLUME);
    volume = atoi(tmp);
    snprintf(_slVal, 16, "%d", volume * 10);
    _pSlider->setValue(volume * 10);
}

void MicVolumeMenuWnd::onMenuResume()
{
    updateMicVolumeDisplay();
    _pPanel->Draw(true);
}

void MicVolumeMenuWnd::onMenuPause()
{
}

void MicVolumeMenuWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void MicVolumeMenuWnd::onSliderChanged(Control *slider, int percentage)
{
    if (_pSlider == slider) {
        char tmp[256];
        int volume = 0;
        agcmd_property_get(Prop_Mic_Volume, tmp, DEFAULT_MIC_VOLUME);
        volume = atoi(tmp);
        int newvolume = percentage / 10;
        if (volume != newvolume) {
            _cp->SetMicVolume(newvolume);
            char vol_bak[32];
            if (newvolume == 0) {
                snprintf(vol_bak, sizeof(vol_bak), "%s", DEFAULT_MIC_VOLUME);
            } else {
                snprintf(vol_bak, sizeof(vol_bak), "%d", newvolume);
            }
            agcmd_property_set(Settings_Mic_Volume, vol_bak);
            snprintf(_slVal, 16, "%d", newvolume * 10);
            _pPanel->Draw(true);
        }
    }
}

void MicVolumeMenuWnd::onSliderTriggered(Control *slider, bool triggered)
{
}

bool MicVolumeMenuWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_property_update) {
                if (event->_paload == PropertyChangeType_audio) {
                    updateMicVolumeDisplay();
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
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 6);
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


AutoRecordWnd::AutoRecordWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
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

    _pInfo = new StaticText(_pPanel, Point(40, 80), Size(320, 200));
    _pInfo->SetStyle(28, FONT_ROBOTO_Regular, Color_Grey_4, CENTER, TOP);
    _pInfo->SetLineHeight(40);
    snprintf(_txt, sizeof(_txt),
        "Automatically start"
        "\n"
        "recording when Waylens"
        "\n"
        "camera powered on"
        "\n"
        "through car mount.");
    _pInfo->SetText((UINT8 *)_txt);

    _pSwitchFake = new Button(
        _pPanel,
        Point(0, 240), Size(400, 80),
        Color_Black, Color_Grey_1);
    _pSwitchFake->SetStyle(0, 0.0);
    _pSwitchFake->setOnClickListener(this);

    _pStatus = new StaticText(
        _pPanel,
        Point(40, 260), Size(200, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"Auto Record");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(304, 264), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");
}

AutoRecordWnd::~AutoRecordWnd()
{
    delete _pSwitch;
    delete _pStatus;
    delete _pSwitchFake;

    delete _pInfo;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void AutoRecordWnd::updateStatus()
{
    char auto_record[256];
    agcmd_property_get(PropName_Auto_Record, auto_record, Auto_Record_Enabled);
    if (strcasecmp(auto_record, Auto_Record_Enabled) == 0) {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }
}

void AutoRecordWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void AutoRecordWnd::onMenuPause()
{
}

void AutoRecordWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pSwitchFake) {
        char auto_record[256];
        agcmd_property_get(PropName_Auto_Record, auto_record, Auto_Record_Enabled);
        if (strcasecmp(auto_record, Auto_Record_Enabled) == 0) {
            agcmd_property_set(PropName_Auto_Record, Auto_Record_Disabled);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_off.bmp");
        } else {
            agcmd_property_set(PropName_Auto_Record, Auto_Record_Enabled);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_on.bmp");
        }
        _pPanel->Draw(true);
    }
}

bool AutoRecordWnd::OnEvent(CEvent *event)
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
    _pTotalResult->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pTotalResult->SetText((UINT8 *)_txtTotle);

    _pHighlight = new StaticText(_pPanel, Point(40, 160), Size(160, 40));
    _pHighlight->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pHighlight->SetText((UINT8 *)"Highlight");

    strcpy(_txtHighlight, " ");
    _pHighlightResult = new StaticText(_pPanel, Point(200, 160), Size(160, 40));
    _pHighlightResult->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pHighlightResult->SetText((UINT8 *)_txtHighlight);

    _pLoop = new StaticText(_pPanel, Point(40, 240), Size(180, 40));
    _pLoop->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pLoop->SetText((UINT8 *)"Loop Space");

    strcpy(_txtLoop, " ");
    _pLoopResult = new StaticText(_pPanel, Point(220, 240), Size(140, 40));
    _pLoopResult->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pLoopResult->SetText((UINT8 *)_txtLoop);

    _pFormatBtnFake = new Button(_pPanel,
        Point(30, 280), Size(340, 120),
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


    _visibleSizeScroller.width = 400;
    _visibleSizeScroller.height = 312;
    _pScroller = new Scroller(
        _pPanel,
        this,
        Point(0, 88),
        _visibleSizeScroller,
        Size(_visibleSizeScroller.width, 680),
        22);


    _pName = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 20), Size(80, 40));
    _pName->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pName->SetText((UINT8 *)"Name");

    char nam[AGCMD_PROPERTY_SIZE];
    memset(nam, 0x00, sizeof(nam));
    agcmd_property_get(PropName_camera_name, nam, Camera_name_default);
    snprintf(_txtName, sizeof(_txtName), "%s", nam);
    _pNameValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(120, 16), Size(240, 40));
    _pNameValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_Grey_3, RIGHT, MIDDLE);
    _pNameValue->SetText((UINT8 *)_txtName);


    _pVerBtnFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 80), Size(340, 80), Color_Black, Color_Grey_1);
    _pVerBtnFake->SetStyle(20);
    _pVerBtnFake->setOnClickListener(this);

    _pVersion = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 100), Size(160, 40));
    _pVersion->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pVersion->SetText((UINT8 *)"Version");

    snprintf(_txtVersionSys, sizeof(_txtVersionSys), "%s", SERVICE_UPNP_VERSION);
    _pVersionValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(200, 100), Size(160, 40));
    _pVersionValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pVersionValue->SetText((UINT8 *)_txtVersionSys);


    _pUnitsFake = new Button(
        _pScroller->getViewRoot(),
        Point(50, 160), Size(340, 80), Color_Black, Color_Black);
    _pUnitsFake->setOnClickListener(this);

    _pUnits = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 180), Size(160, 60));
    _pUnits->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pUnits->SetText((UINT8 *)"Units");

    snprintf(_txtUnits, sizeof(_txtUnits), " ");
    _pUnitsValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(200, 188), Size(160, 40));
    _pUnitsValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pUnitsValue->SetText((UINT8 *)_txtUnits);


    _pAutoOffFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 240), Size(340, 80), Color_Black, Color_Black);
    _pAutoOffFake->setOnClickListener(this);

    _pAutoOff = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 260), Size(160, 60));
    _pAutoOff->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pAutoOff->SetText((UINT8 *)"Auto Off");

    snprintf(_txtAutoOff, sizeof(_txtAutoOff), " ");
    _pAutoOffValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(200, 268), Size(160, 40));
    _pAutoOffValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pAutoOffValue->SetText((UINT8 *)_txtAutoOff);


    _pAutoRecordFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 320), Size(340, 80), Color_Black, Color_Black);
    _pAutoRecordFake->setOnClickListener(this);

    _pAutoRecord = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 340), Size(220, 60));
    _pAutoRecord->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pAutoRecord->SetText((UINT8 *)"Auto Record");

    snprintf(_txtAutoRecord, sizeof(_txtAutoOff), " ");
    _pAutoRecordValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(260, 348), Size(100, 40));
    _pAutoRecordValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pAutoRecordValue->SetText((UINT8 *)_txtAutoRecord);


    _pAutoMarkFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 400), Size(340, 80), Color_Black, Color_Black);
    _pAutoMarkFake->setOnClickListener(this);

    _pAutoMark = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 420), Size(250, 60));
    _pAutoMark->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pAutoMark->SetText((UINT8 *)"Auto Highlight");

    snprintf(_txtAutoMark, sizeof(_txtAutoMark), " ");
    _pAutoMarkValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(290, 434), Size(70, 40));
    _pAutoMarkValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pAutoMarkValue->SetText((UINT8 *)_txtAutoMark);


    _pResetBtnFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 480), Size(340, 80), Color_Black, Color_Black);
    _pResetBtnFake->setOnClickListener(this);

    _pFactoryReset = new Button(
        _pScroller->getViewRoot(),
        Point(50, 500), Size(300, 60), Color_Black, Color_Grey_1);
    _pFactoryReset->SetText((UINT8 *)"Reset Camera", 32, Color_White, FONT_ROBOTOMONO_Bold);
    _pFactoryReset->SetTextAlign(CENTER, MIDDLE);
    _pFactoryReset->SetStyle(25);
    _pFactoryReset->setOnClickListener(this);


    _pPowerOffBtnFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 560), Size(340, 80), Color_Black, Color_Grey_1);
    _pPowerOffBtnFake->setOnClickListener(this);

    _pPowerOff = new Button(
        _pScroller->getViewRoot(),
        Point(50, 580), Size(300, 60), Color_Black, Color_Grey_1);
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

    delete _pAutoMarkValue;
    delete _pAutoMark;
    delete _pAutoMarkFake;

    delete _pAutoRecordValue;
    delete _pAutoRecord;
    delete _pAutoRecordFake;

    delete _pAutoOffValue;
    delete _pAutoOff;
    delete _pAutoOffFake;

    delete _pUnitsValue;
    delete _pUnits;
    delete _pUnitsFake;

    delete _pVersionValue;
    delete _pVersion;
    delete _pVerBtnFake;

    delete _pNameValue;
    delete _pName;

    delete _pScroller;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void AdvancedMenuWnd::onMenuResume()
{
    agcmd_property_get(PropName_Units_System, _txtUnits, Imperial_Units_System);
    agcmd_property_get(PropName_Udc_Power_Delay, _txtAutoOff, Udc_Power_Delay_Default);
    agcmd_property_get(PropName_Auto_Record, _txtAutoRecord, Auto_Record_Enabled);
    agcmd_property_get(PropName_Auto_Mark, _txtAutoMark, Auto_Mark_Disabled);
    _pPanel->Draw(true);
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
    } else if (widget == _pAutoOffFake) {
        this->StartWnd(WINDOW_autooffmenu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pAutoRecordFake) {
        this->StartWnd(WINDOW_auto_record_menu, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pUnitsFake) {
        this->StartWnd(WINDOW_units_test, Anim_Bottom2TopCoverEnter);
    } else if (widget == _pAutoMarkFake) {
        this->StartWnd(WINDOW_auto_mark, Anim_Bottom2TopCoverEnter);
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
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                int x_pos = (position >> 16) & 0xFFFF;
                if ((y_pos < 110) || (x_pos < 100) || (x_pos > 300)) {
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


VersionMenuWnd::VersionMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
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
    _pTitle->SetText((UINT8 *)"Version");

    _pLine = new UILine(_pPanel, Point(105, 106), Size(190, 4));
    _pLine->setLine(Point(105, 100), Point(295, 100), 4, Color_Grey_2);

    _visibleSizeScroller.width = 400;
    _visibleSizeScroller.height = 290;
    _scrollableSize.width = 400;
    _scrollableSize.height = 400;
    _pScroller = new Scroller(
        _pPanel,
        this,
        Point(0, 110),
        _visibleSizeScroller,
        _scrollableSize,
        10);


    _pBSP = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 0), Size(60, 80));
    _pBSP->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pBSP->SetText((UINT8 *)"HW");

    memset(_vBsp, 0x00, sizeof(_vBsp));
    agcmd_property_get(PropName_prebuild_bsp, _vBsp, "V0B");
    _pBSPValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(100, 0), Size(260, 80));
    _pBSPValue->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pBSPValue->SetText((UINT8 *)_vBsp);


    _pSN = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 80), Size(60, 80));
    _pSN->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pSN->SetText((UINT8 *)"SN");

    memset(_vSN, 0x00, sizeof(_vSN));
    agcmd_property_get(PropName_SN_SSID, _vSN, "Unknown");
    _pSNValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(100, 80), Size(260, 80));
    _pSNValue->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pSNValue->SetText((UINT8 *)_vSN);


    _pVAPI = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 160), Size(60, 80));
    _pVAPI->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pVAPI->SetText((UINT8 *)"API");

    memset(_vAPI, 0x00, sizeof(_vAPI));
    snprintf(_vAPI, sizeof(_vAPI), "%s", SERVICE_UPNP_VERSION);
    _pVAPIValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(100, 160), Size(260, 80));
    _pVAPIValue->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pVAPIValue->SetText((UINT8 *)_vAPI);


    _pVSystem = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 240), Size(60, 80));
    _pVSystem->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pVSystem->SetText((UINT8 *)"OS");

    memset(_vSys, 0x00, sizeof(_vSys));
    agcmd_property_get("prebuild.system.version", _vSys, "Unknown");
    _pVSystemValue = new StaticText(
        _pScroller->getViewRoot(),
        Point(100, 240), Size(260, 80));
    _pVSystemValue->SetStyle(28, FONT_ROBOTOMONO_Regular, Color_Grey_3, RIGHT, MIDDLE);
    _pVSystemValue->SetText((UINT8 *)_vSys);
}

VersionMenuWnd::~VersionMenuWnd()
{
    delete _pVSystemValue;
    delete _pVSystem;
    delete _pVAPIValue;
    delete _pVAPI;
    delete _pSNValue;
    delete _pSN;
    delete _pBSPValue;
    delete _pBSP;
    delete _pScroller;
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
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                //int x_pos = (position >> 16) & 0xFFFF;
                if (y_pos < 110) {
                    this->Close(Anim_Top2BottomExit);
                    b = false;
                }
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


    _pBtnTimeFake = new Button(_pPanel, Point(30, 120), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pBtnTimeFake->SetStyle(20);
    _pBtnTimeFake->setOnClickListener(this);

    _pTime = new StaticText(_pPanel, Point(40, 140), Size(120, 40));
    _pTime->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pTime->SetText((UINT8 *)"Time");

    strcpy(_txtTime, "00:00");
    _pTimeResult = new StaticText(_pPanel, Point(160, 140), Size(200, 40));
    _pTimeResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pTimeResult->SetText((UINT8 *)_txtTime);


    _pBtnDateFake = new Button(_pPanel, Point(30, 200), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pBtnDateFake->SetStyle(20);
    _pBtnDateFake->setOnClickListener(this);

    _pDate = new StaticText(_pPanel, Point(40, 220), Size(100, 40));
    _pDate->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pDate->SetText((UINT8 *)"Date");

    strcpy(_txtDate, "Jan 01, 2015");
    _pDateResult = new StaticText(_pPanel, Point(140, 220), Size(220, 40));
    _pDateResult->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pDateResult->SetText((UINT8 *)_txtDate);
}

TimeDateMenuWnd::~TimeDateMenuWnd()
{
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
    gtime = (*localtime(&now_val.tv_sec));

    //CAMERALOG("%02d:%02d %s",
    //    gtime.tm_hour > 12 ? gtime.tm_hour - 12 : gtime.tm_hour,
    //    gtime.tm_min,
    //    gtime.tm_hour > 12 ? "PM" : "AM");
    char format[32];
    agcmd_property_get(PropName_Time_Format, format, "12h");
    if (strcasecmp(format, "12h") == 0) {
        strftime(_txtTime, sizeof(_txtTime),
            "%l:%M %p", &gtime);
    } else {
        strftime(_txtTime, sizeof(_txtTime),
            "%H:%M", &gtime);
    }

//--------------------------------
// yy/mm/dd: 2017-01-17
// mm/dd/yy: Jan 17, 2017
// dd/mm/yy: 17 Jan 2017
//--------------------------------
    agcmd_property_get(PropName_Date_Format, format, "mm/dd/yy");
    if (strcasecmp(format, "mm/dd/yy") == 0) {
        strftime(_txtDate, sizeof(_txtDate),
            "%b %d, %Y", &gtime);
    } else if (strcasecmp(format, "dd/mm/yy") == 0) {
        strftime(_txtDate, sizeof(_txtDate),
            "%d %b %Y", &gtime);
    } else if (strcasecmp(format, "yy/mm/dd") == 0) {
        strftime(_txtDate, sizeof(_txtDate),
            "%Y-%m-%d", &gtime);
    }

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
    gtime = (*localtime(&now_val.tv_sec));

    char format[32];
    agcmd_property_get(PropName_Time_Format, format, "12h");
    if (strcasecmp(format, "12h") == 0) {
        strftime(_txtTime, sizeof(_txtTime),
            "%l:%M %p", &gtime);
    } else {
        strftime(_txtTime, sizeof(_txtTime),
            "%H:%M", &gtime);
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

    strcpy(_txtDate, "Jan 01, 2015");
    _pDate = new StaticText(_pPanel, Point(0, 140), Size(400, 100));
    _pDate->SetStyle(52, FONT_ROBOTOMONO_Light, Color_Grey_3, CENTER, MIDDLE);
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
        Point(80, 240 - _pickerSize.height / 2),
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
    gtime = (*localtime(&now_val.tv_sec));

    char format[32];
    agcmd_property_get(PropName_Date_Format, format, "mm/dd/yy");
    strcpy(_dateFormat, format);

//--------------------------------
// yy/mm/dd: 2017-01-17
// mm/dd/yy: Jan 17, 2017
// dd/mm/yy: 17 Jan 2017
//--------------------------------
    if (strcasecmp(format, "mm/dd/yy") == 0) {
        strftime(_txtDate, sizeof(_txtDate),
            "%b %d, %Y", &gtime);
    } else if (strcasecmp(format, "dd/mm/yy") == 0) {
        strftime(_txtDate, sizeof(_txtDate),
            "%d %b %Y", &gtime);
    } else if (strcasecmp(format, "yy/mm/dd") == 0) {
        strftime(_txtDate, sizeof(_txtDate),
            "%Y-%m-%d", &gtime);
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


OBDGaugesWnd::OBDGaugesWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 10);
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

    _visibleSizeScroller.width = 400;
    _visibleSizeScroller.height = 312;
    _pScroller = new Scroller(
        _pPanel,
        this,
        Point(0, 88),
        _visibleSizeScroller,
        Size(_visibleSizeScroller.width, 480),
        20);

    _initScroller();
}

OBDGaugesWnd::~OBDGaugesWnd()
{
    delete _pSpeedSwitch;
    delete _pSpeedStatus;
    delete _pSpeedFake;

    delete _pRPMSwitch;
    delete _pRPMStatus;
    delete _pRPMFake;

    delete _pMAFSwitch;
    delete _pMAFStatus;
    delete _pMAFFake;

    delete _pBoostSwitch;
    delete _pBoostStatus;
    delete _pBoostFake;

    delete _pTemperatureSwitch;
    delete _pTemperatureStatus;
    delete _pTemperatureFake;

    delete _pScroller;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void OBDGaugesWnd::_initScroller()
{
    _pSpeedFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 0), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pSpeedFake->SetStyle(20);
    _pSpeedFake->setOnClickListener(this);

    _pSpeedStatus = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 20), Size(260, 40));
    _pSpeedStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pSpeedStatus->SetText((UINT8 *)"Speed");

    _pSpeedSwitch = new BmpImage(
        _pScroller->getViewRoot(),
        Point(304, 24), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");


    _pRPMFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 80), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pRPMFake->SetStyle(20);
    _pRPMFake->setOnClickListener(this);

    _pRPMStatus = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 100), Size(260, 40));
    _pRPMStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pRPMStatus->SetText((UINT8 *)"RPM");

    _pRPMSwitch = new BmpImage(
        _pScroller->getViewRoot(),
        Point(304, 104), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");


    _pTemperatureFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 160), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pTemperatureFake->SetStyle(20);
    _pTemperatureFake->setOnClickListener(this);

    _pTemperatureStatus = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 180), Size(200, 40));
    _pTemperatureStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pTemperatureStatus->SetText((UINT8 *)"Engine Temp.");

    _pTemperatureSwitch = new BmpImage(
        _pScroller->getViewRoot(),
        Point(304, 184), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");


    _pBoostFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 240), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pBoostFake->SetStyle(20);
    _pBoostFake->setOnClickListener(this);

    _pBoostStatus = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 260), Size(200, 40));
    _pBoostStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pBoostStatus->SetText((UINT8 *)"Boost");

    _pBoostSwitch = new BmpImage(
        _pScroller->getViewRoot(),
        Point(304, 264), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");


    _pMAFFake = new Button(
        _pScroller->getViewRoot(),
        Point(30, 320), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pMAFFake->SetStyle(20);
    _pMAFFake->setOnClickListener(this);

    _pMAFStatus = new StaticText(
        _pScroller->getViewRoot(),
        Point(40, 340), Size(260, 40));
    _pMAFStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pMAFStatus->SetText((UINT8 *)"Mass Air Flow");

    _pMAFSwitch = new BmpImage(
        _pScroller->getViewRoot(),
        Point(304, 344), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");
}

void OBDGaugesWnd::_updateStatus()
{
    char tmp[256];
    agcmd_property_get(PropName_Settings_Temp_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        _pTemperatureSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pTemperatureSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }

    agcmd_property_get(PropName_Settings_Boost_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        _pBoostSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pBoostSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }

    agcmd_property_get(PropName_Settings_MAF_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        _pMAFSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pMAFSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }

    agcmd_property_get(PropName_Settings_RPM_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        _pRPMSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pRPMSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }

    agcmd_property_get(PropName_Settings_Speed_Gauge, tmp, SHOW_GAUGE);
    if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
        _pSpeedSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pSpeedSwitch->setSource(
            "/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }
}

void OBDGaugesWnd::onMenuResume()
{
    _updateStatus();
    _pPanel->Draw(true);
}

void OBDGaugesWnd::onMenuPause()
{
}

void OBDGaugesWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pTemperatureFake) {
        char tmp[256];
        agcmd_property_get(PropName_Settings_Temp_Gauge, tmp, SHOW_GAUGE);
        if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
            agcmd_property_set(PropName_Settings_Temp_Gauge, HIDE_GAUGE);
        } else {
            agcmd_property_set(PropName_Settings_Temp_Gauge, SHOW_GAUGE);
        }
        _updateStatus();
        _pPanel->Draw(true);
    } else if (widget == _pBoostFake) {
        char tmp[256];
        agcmd_property_get(PropName_Settings_Boost_Gauge, tmp, SHOW_GAUGE);
        if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
            agcmd_property_set(PropName_Settings_Boost_Gauge, HIDE_GAUGE);
        } else {
            agcmd_property_set(PropName_Settings_Boost_Gauge, SHOW_GAUGE);
        }
        _updateStatus();
        _pPanel->Draw(true);
    } else if (widget == _pMAFFake) {
        char tmp[256];
        agcmd_property_get(PropName_Settings_MAF_Gauge, tmp, SHOW_GAUGE);
        if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
            agcmd_property_set(PropName_Settings_MAF_Gauge, HIDE_GAUGE);
        } else {
            agcmd_property_set(PropName_Settings_MAF_Gauge, SHOW_GAUGE);
        }
        _updateStatus();
        _pPanel->Draw(true);
    } else if (widget == _pRPMFake) {
        char tmp[256];
        agcmd_property_get(PropName_Settings_RPM_Gauge, tmp, SHOW_GAUGE);
        if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
            agcmd_property_set(PropName_Settings_RPM_Gauge, HIDE_GAUGE);
        } else {
            agcmd_property_set(PropName_Settings_RPM_Gauge, SHOW_GAUGE);
        }
        _updateStatus();
        _pPanel->Draw(true);
    } else if (widget == _pSpeedFake) {
        char tmp[256];
        agcmd_property_get(PropName_Settings_Speed_Gauge, tmp, SHOW_GAUGE);
        if (strcasecmp(tmp, SHOW_GAUGE) == 0) {
            agcmd_property_set(PropName_Settings_Speed_Gauge, HIDE_GAUGE);
        } else {
            agcmd_property_set(PropName_Settings_Speed_Gauge, SHOW_GAUGE);
        }
        _updateStatus();
        _pPanel->Draw(true);
    }
}

bool OBDGaugesWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                //int x_pos = (position >> 16) & 0xFFFF;
                if (y_pos < 110) {
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


UnitsWnd::UnitsWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
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
    _pTitle->SetText((UINT8 *)"Units");

    _pLine = new UILine(_pPanel, Point(130, 106), Size(140, 4));
    _pLine->setLine(Point(130, 100), Point(270, 100), 4, Color_Grey_2);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)Imperial_Units_System;
    _pMenus[_numMenus++] = (char *)Metric_Units_System;

    UINT16 itemheight = 60;
    UINT16 padding = 40;
    _pickerSize.width = 240;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding;
    _pPickers = new Pickers(_pPanel,
        Point(80, 240 - _pickerSize.height / 2),
        _pickerSize,
        _pickerSize);
    _pPickers->setItems(_pMenus, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);
}

UnitsWnd::~UnitsWnd()
{
    delete _pPickers;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void UnitsWnd::updateStatus()
{
    char tmp[256];
    agcmd_property_get(PropName_Units_System, tmp, Imperial_Units_System);
    if (strcasecmp(tmp, Imperial_Units_System) == 0) {
        _pPickers->setIndex(0);
    } else {
        _pPickers->setIndex(1);
    }
}

void UnitsWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void UnitsWnd::onMenuPause()
{
}

void UnitsWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void UnitsWnd::onPickersFocusChanged(Control *picker, int indexFocus)
{
    if (_pPickers == picker) {
        if ((indexFocus >= 0) && (indexFocus < _numMenus)) {
            if (indexFocus == 0) {
                agcmd_property_set(PropName_Units_System, Imperial_Units_System);
                usleep(100 * 1000);
                this->Close(Anim_Top2BottomExit);
            } else if (indexFocus == 1) {
                agcmd_property_set(PropName_Units_System, Metric_Units_System);
                usleep(100 * 1000);
                this->Close(Anim_Top2BottomExit);
            }
        }
    }
}

bool UnitsWnd::OnEvent(CEvent *event)
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


AutoMarkWnd::AutoMarkWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
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

    _pSwitchFake = new Button(
        _pPanel,
        Point(0, 120), Size(400, 80),
        Color_Black, Color_Grey_1);
    _pSwitchFake->SetStyle(0, 0.0);
    _pSwitchFake->setOnClickListener(this);

    _pStatus = new StaticText(
        _pPanel,
        Point(40, 140), Size(264, 40));
    _pStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pStatus->SetText((UINT8 *)"Auto Highlight");

    _pSwitch = new BmpImage(
        _pPanel,
        Point(304, 144), Size(56, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "toggle_off.bmp");

    _pSensitivityFake = new Button(_pPanel, Point(30, 200), Size(340, 80),
        Color_Black, Color_Grey_1);
    _pSensitivityFake->SetStyle(20);
    _pSensitivityFake->setOnClickListener(this);

    _pSensitivityStatus = new StaticText(_pPanel, Point(40, 220), Size(200, 40));
    _pSensitivityStatus->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_2, LEFT, MIDDLE);
    _pSensitivityStatus->SetText((UINT8 *)"Sensitivity");

    strcpy(_txtSensitivity, " ");
    _pSensitivityValue = new StaticText(_pPanel, Point(200, 220), Size(160, 40));
    _pSensitivityValue->SetStyle(28, FONT_ROBOTOMONO_Bold, Color_White, RIGHT, MIDDLE);
    _pSensitivityValue->SetText((UINT8 *)_txtSensitivity);
}

AutoMarkWnd::~AutoMarkWnd()
{
    delete _pSensitivityValue;
    delete _pSensitivityStatus;
    delete _pSensitivityFake;

    delete _pSwitch;
    delete _pStatus;
    delete _pSwitchFake;

    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void AutoMarkWnd::updateStatus()
{
    char auto_mark[256];
    agcmd_property_get(PropName_Auto_Mark, auto_mark, Auto_Mark_Disabled);
    if (strcasecmp(auto_mark, Auto_Mark_Enabled) == 0) {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_on.bmp");
    } else {
        _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
            "toggle_off.bmp");
    }

    agcmd_property_get(PropName_Auto_Mark_Sensitivity, _txtSensitivity, Sensitivity_Normal);
}

void AutoMarkWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void AutoMarkWnd::onMenuPause()
{
}

void AutoMarkWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    } else if (widget == _pSwitchFake) {
        char auto_mark[256];
        agcmd_property_get(PropName_Auto_Mark, auto_mark, Auto_Mark_Disabled);
        if (strcasecmp(auto_mark, Auto_Mark_Enabled) == 0) {
            agcmd_property_set(PropName_Auto_Mark, Auto_Mark_Disabled);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_off.bmp");
        } else {
            agcmd_property_set(PropName_Auto_Mark, Auto_Mark_Enabled);
            _pSwitch->setSource("/usr/local/share/ui/BMP/dash_menu/",
                "toggle_on.bmp");
        }
        _pPanel->Draw(true);
    } else if (widget == _pSensitivityFake) {
        this->StartWnd(WINDOW_mark_sensitivity, Anim_Bottom2TopCoverEnter);
    }
}

bool AutoMarkWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                UINT32 position = event->_paload1;
                int y_pos = position & 0xFFFF;
                //int x_pos = (position >> 16) & 0xFFFF;
                if ((y_pos < 110) || (y_pos >280)) {
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


SensitivityWnd::SensitivityWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
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
    _pTitle->SetText((UINT8 *)"Sensitivity");

    _pLine = new UILine(_pPanel, Point(95, 106), Size(210, 4));
    _pLine->setLine(Point(95, 100), Point(305, 100), 4, Color_Grey_2);

    _numMenus = 0;
    _pMenus[_numMenus++] = (char *)"H(0.75g,90ms)";
    _pMenus[_numMenus++] = (char *)"N(1.0g,100ms)";
    _pMenus[_numMenus++] = (char *)"L(1.5g,150ms)";

    UINT16 itemheight = 60;
    UINT16 padding = 20;
    _pickerSize.width = 300;
    _pickerSize.height = (itemheight + padding) * _numMenus - padding;
    _pPickers = new Pickers(_pPanel,
        Point(50, 240 - _pickerSize.height / 2),
        _pickerSize,
        _pickerSize);
    _pPickers->setItems(_pMenus, _numMenus);
    _pPickers->setStyle(Color_Black,
        Color_Black, Color_White,
        32, FONT_ROBOTOMONO_Bold,
        itemheight, padding);
    _pPickers->setListener(this);
}

SensitivityWnd::~SensitivityWnd()
{
    delete _pPickers;
    delete _pLine;
    delete _pTitle;
    delete _pReturnBtn;
    delete _pReturnBtnFake;
    delete _pPanel;
}

void SensitivityWnd::updateStatus()
{
    char tmp[256];
    agcmd_property_get(PropName_Auto_Mark_Sensitivity, tmp, Sensitivity_Normal);
    if (strcasecmp(tmp, Sensitivity_High) == 0) {
        _pPickers->setIndex(0);
    } else if (strcasecmp(tmp, Sensitivity_Normal) == 0) {
        _pPickers->setIndex(1);
    } else if (strcasecmp(tmp, Sensitivity_Low) == 0) {
        _pPickers->setIndex(2);
    }
}

void SensitivityWnd::onMenuResume()
{
    updateStatus();
    _pPanel->Draw(true);
}

void SensitivityWnd::onMenuPause()
{
}

void SensitivityWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        this->Close(Anim_Top2BottomExit);
    }
}

void SensitivityWnd::onPickersFocusChanged(Control *picker, int indexFocus)
{
    if (_pPickers == picker) {
        if ((indexFocus >= 0) && (indexFocus < _numMenus)) {
            if (indexFocus == 0) {
                agcmd_property_set(PropName_Auto_Mark_Sensitivity, Sensitivity_High);
                _cp->setSensitivity(gsensor_sensitive_high);
                usleep(100 * 1000);
                this->Close(Anim_Top2BottomExit);
            } else if (indexFocus == 1) {
                agcmd_property_set(PropName_Auto_Mark_Sensitivity, Sensitivity_Normal);
                _cp->setSensitivity(gsensor_sensitive_middle);
                usleep(100 * 1000);
                this->Close(Anim_Top2BottomExit);
            } else if (indexFocus == 2) {
                agcmd_property_set(PropName_Auto_Mark_Sensitivity, Sensitivity_Low);
                _cp->setSensitivity(gsensor_sensitive_low);
                usleep(100 * 1000);
                this->Close(Anim_Top2BottomExit);
            }
        }
    }
}

bool SensitivityWnd::OnEvent(CEvent *event)
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

