
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

#include "IMEWnd.h"

namespace ui {

IMEWnd::IMEWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop)
    : inherited(cp, name, wndSize, leftTop)
    , _bPassword(false)
{
    _pPanel = new Panel(NULL, this, Point(0, 0), Size(400, 400), 10);
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

    memset(_txtTitle, 0x00, sizeof(_txtTitle));
    _pTitle = new StaticText(_pPanel, Point(50, 60), Size(300, 40));
    _pTitle->SetStyle(28, FONT_ROBOTOMONO_Medium, Color_Grey_3, CENTER, MIDDLE);
    _pTitle->SetText((UINT8 *)_txtTitle);

    _pIconPwd = new BmpImage(
        _pPanel,
        Point(50, 108), Size(32, 32),
        "/usr/local/share/ui/BMP/dash_menu/",
        "icn_password.bmp");
    _pIconPwd->SetHiden();

    _pEditText = new EditText(_pPanel, Point(98, 100), Size(252, 56));
    _pEditText->SetTextStyle(28, FONT_ROBOTOMONO_Light, Color_White);
    //_pEditText->SetHintText("Click me!");
    //_pEditText->SetPassword(true);
    _pEditText->setOnClickListener(this);
    _pEditText->setWatcher(this);

    _pIME = new SimpleIME(_pPanel, this, Point(0, 160), Size(400, 240));
    _pIME->setTextWatcher(_pEditText);
    _pIME->setStatusWatcher(this);
}

IMEWnd::~IMEWnd()
{
    delete _pIME;
    delete _pEditText;
    delete _pIconPwd;
    delete _pTitle;

    delete _pReturnBtn;
    delete _pReturnBtnFake;

    delete _pPanel;
}

void IMEWnd::_exit(bool gotInput)
{
    if (gotInput) {
        CameraAppl::getInstance()->updateTextInputed(_pEditText->GetText());
    }

    memset(_txtTitle, 0x00, sizeof(_txtTitle));
    _pEditText->reset();
    _pIME->reset();
    _pPanel->Draw(false);

    this->Close(Anim_Top2BottomExit, false);
}

void IMEWnd::setInitValue(char *title, bool bpassword)
{
    snprintf(_txtTitle, sizeof(_txtTitle), "%s", title);
    _bPassword = bpassword;

    _pIME->reset();
}

void IMEWnd::onMenuResume()
{
    _pEditText->reset();
    _pEditText->SetFocus(true);

    _pIME->reset();

    if (_bPassword) {
        _pIconPwd->SetShow();
    } else {
        _pIconPwd->SetHiden();
    }

    _pPanel->Draw(true);
}

void IMEWnd::onMenuPause()
{
}

void IMEWnd::onHideIME()
{
    _exit(true);
}

void IMEWnd::onEditTextChanged()
{
    char *txt = _pEditText->GetText();
    if (strlen(txt) == 0) {
        _pIME->switchDeleteStatus(false);
    } else {
        _pIME->switchDeleteStatus(true);
    }

    if (strlen(txt) < 8) {
        _pIME->switchEnterStatus(false);
    } else {
        _pIME->switchEnterStatus(true);
    }
}

void IMEWnd::onClick(Control *widget)
{
    if ((widget == _pReturnBtn) ||
        (widget == _pReturnBtnFake))
    {
        _exit(false);
    }
}

bool IMEWnd::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_touch:
            if ((event->_event == TouchEvent_flick) && (event->_paload == TouchFlick_Down)) {
                _exit(false);
                b = false;
            }
            break;
    }

    if (b) {
        b = inherited::OnEvent(event);
    }

    return b;
}

};
