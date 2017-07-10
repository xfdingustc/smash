

#include <math.h>
#include <sys/prctl.h>

#include "ClinuxThread.h"
#include "linux.h"
#include "CEvent.h"
#include "CairoCanvas.h"

#include "Window.h"
#include "WindowManager.h"

#include "IME.h"

namespace ui{

EditText::EditText(Container* pParent, const Point& leftTop, const Size& CSize)
    : inherited(pParent, leftTop, CSize)
    , _listener(NULL)
    , _watcher(NULL)
    , _fonttype(FONT_ROBOTOMONO_Medium)
    , _txtSize(20)
    , _txtColor(0x3187)
    , _bgColor(0x0000)
    , _bFocus(false)
    , _bPassword(false)
    , _bHasHint(false)
    , _bIMEInputing(false)
    , _bShowCursor(false)
    , _counterET(0)
{
    _pLock = new CMutex("EditText mutex");

    memset(_pText, 0x00, sizeof(_pText));
}

EditText::~EditText()
{
    delete _pLock;
}

void EditText::Draw(bool bPost) {
    CairoCanvas* pCav = this->GetCanvas();
    Point lefttop = GetAbsPos();
    Size  areasize = GetSize();

    // reset area
    pCav->DrawRect(lefttop, areasize, _bgColor, 0, 1.0);

    // draw bottom line
    pCav->DrawLine(
        Point(lefttop.x, lefttop.y + areasize.height - 2),
        Point(lefttop.x + areasize.width, lefttop.y + areasize.height - 2),
        2, 0x3187);

    char txtShow[20] = {0x00};
    int txtlen = pCav->GetTextLenInPixel(
            (const char *)_pText, (FONT)_fonttype, _txtSize);
    int charWidth = 0;
    if (strlen(_pText)) {
        charWidth = txtlen / strlen(_pText) + 1;
    }
    unsigned int num = 0;
    if (charWidth > 0) {
        num = areasize.width / charWidth;
    }
    if (strlen(_pText) > num) {
        if (_bPassword) {
            memset(txtShow, 0x00, sizeof(txtShow));
            txtShow[0] = '<';
            txtShow[1] = '-';
            for (int i = 2; i < num - 1; i++) {
                txtShow[i] = '*';
            }
            if (_bIMEInputing) {
                txtShow[num - 1] = _pText[strlen(_pText) - 1];
            } else {
                txtShow[num - 1] = '*';
            }
        } else {
            num = strlen(_pText) - num + 2;
            snprintf(txtShow, sizeof(txtShow), "<-%s", _pText + num);
        }
    } else {
        if (_bPassword && (strlen(_pText) > 0)) {
            memset(txtShow, 0x00, sizeof(txtShow));
            for (int i = 0; i < strlen(_pText) - 1; i++) {
                txtShow[i] = '*';
            }
            if (_bIMEInputing) {
                txtShow[strlen(_pText) - 1] = _pText[strlen(_pText) - 1];
            } else {
                txtShow[strlen(_pText) - 1] = '*';
            }
        } else {
            snprintf(txtShow, sizeof(txtShow), "%s", _pText);
        }
    }

    // draw cursor
    if (_bFocus && _bShowCursor) {
        txtlen = pCav->GetTextLenInPixel(
            (const char *)txtShow, (FONT)_fonttype, _txtSize);
        if (txtlen == 0) {
            txtlen = 1;
        } else if (txtlen >= areasize.width) {
            txtlen = areasize.width - 1;
        }
        //CAMERALOG("######## txtlen = %d, txtShow = %s", txtlen, txtShow);
        if (_bIMEInputing && charWidth) {
            pCav->DrawRect(
                Point(lefttop.x + ((charWidth < txtlen) ? (txtlen - charWidth) : 0), lefttop.y),
                Size((charWidth < txtlen) ? charWidth : txtlen, areasize.height - 6),
                0x3187, 0, 1.0);
        } else {
            pCav->DrawLine(
                Point(lefttop.x + txtlen, lefttop.y),
                Point(lefttop.x + txtlen, lefttop.y + areasize.height - 6),
                2, 0x3187);
        }
    }

    // draw text
    if (_pText && (strlen((char *)_pText) > 0)) {
        UINT16 txtcolor = _txtColor;
        if (_bHasHint) {
            txtcolor = 0x3187;
        }
        pCav->DrawText((const char *)txtShow, (FONT)_fonttype, _txtSize, txtcolor,
            lefttop, areasize, LEFT, MIDDLE);
    }

    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}

bool EditText::OnEvent(CEvent *event)
{
    bool b = true;

    switch (event->_type)
    {
        case eventType_internal:
            if (event->_event == InternalEvent_app_timer_update) {
                _counterET++;
                if (_counterET % 5 == 0) {
                    _pLock->Take();

                    if (_counterET % 10 == 0) {
                        _bShowCursor = true;
                    } else {
                        _bShowCursor = false;
                    }
                    Draw(true);

                    _pLock->Give();
                }
            }
            break;
        case eventType_touch:
            if (TouchEvent_SingleClick == event->_event)
            {
                int x = event->_paload;
                int y = event->_paload1;

                Point pos = GetAbsPos();
                Size size = GetSize();
                /*CAMERALOG("TouchEvent_SingleClick point(%d, %d) is "
                    "clicked, button area: (%d, %d) -> (%d, %d)",
                    x, y,
                    pos.x, pos.y,
                    pos.x +size.width, pos.y + size.height);*/
                if ((x >= pos.x) && (x <= pos.x + size.width)
                    && (y >= pos.y) && (y <= pos.y + size.height))
                {
                    if (_listener) {
                        _listener->onClick(this);
                    }
                    b = false;
                }
            }
            break;
    }

    return b;
}

void EditText::onTextChanged(const char *characters, bool bReDraw)
{
    //CAMERALOG("_pText = %s, characters = %s", _pText, characters);
    if (!characters || (strlen(characters) <= 0) ||
        (strlen(_pText) >= sizeof(_pText)))
    {
        return;
    }

    if (strcmp(characters, "Del") == 0) {
        _bIMEInputing = false;
        if (strlen(_pText) > 0) {
            _pText[strlen(_pText) - 1] = 0x00;
        }
    } else {
        if (_bIMEInputing) {
            if (strlen(_pText) > 0) {
                snprintf(_pText + strlen(_pText) - 1,
                    sizeof(_pText) - (strlen(_pText) - 1), "%s", characters);
            } else {
                snprintf(_pText, sizeof(_pText), "%s", characters);
            }
        } else {
            snprintf(_pText + strlen(_pText),
                sizeof(_pText) - strlen(_pText), "%s", characters);
        }
    }

    if (_watcher) {
        _watcher->onEditTextChanged();
    }

    if (bReDraw) {
        _pLock->Take();
        Draw(true);
        _pLock->Give();
    }
}

void EditText::onInputStatusChanged(bool bInputing)
{
    _pLock->Take();

    _bIMEInputing = bInputing;
    _counterET = 0;
    _bShowCursor = true;

    Draw(true);
    _pLock->Give();
}


#define PAGE_NUM    5
static const char* Alphabet_Pages[PAGE_NUM][KEYBOARD_LINES][KEYBOARD_COLUMNS] = {
        // page 0
        {
            {"Num",   "Symbol", "abc", "def",  "Del"},
            {"Upper", "ghi",    "jkl", "mno",  "Enter"},
            {"Upper", "pqrs",   "tuv", "wxyz", "Enter"},
        },

        // page 1
        {
            {"Num",   "Symbol", "ABC", "DEF",  "Del"},
            {"Lower", "GHI",    "JKL", "MNO",  "Enter"},
            {"Lower", "PQRS",   "TUV", "WXYZ", "Enter"},
        },

        // page 2
        {
            {"Ret", "1", "2", "3", "Del"},
            {"0",   "4", "5", "6", "Enter"},
            {"0",   "7", "8", "9", "Enter"},
        },

        // page 3
        {
            {"Ret",  ".,", "?/", "!@", "Del"},
            {"PgDn", "()", "_-", "+=", "Enter"},
            {"PgDn", "*",  " ",  "#",  "Enter"},
        },

        // page 4
        {
            {"Ret",  ":;", "\"'", "|\\", "Del"},
            {"PgUp", "{}", "[]",  "<>",  "Enter"},
            {"PgUp", "~`", "$%",  "^&",  "Enter"},
        },
    };

SimpleIME::SimpleIME(Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size)
    : inherited(pParent, pOwner, leftTop, size, 20)
    , _pTextWatcher(NULL)
    , _pStatusWatcher(NULL)
    , _indexPage(0)
    , _clickCnt(0)
    , _btnInputing(NULL)
    , _bDelEnabled(false)
    , _bEnterEnabled(false)
{
    _alphabet = Alphabet_Pages + _indexPage;

    // the first column
    _pKeys[0][0] = new ImageButton(
        this,
        Point(0, 0), Size(80, 80),
        "/usr/local/share/ui/BMP/ime/",
        "key-lower-1-1-up.bmp",
        "key-lower-1-1-down.bmp");
    _pKeys[0][0]->setOnClickListener(this);

    _pKeys[1][0] = new ImageButton(
        this,
        Point(0, 80), Size(80, 120),
        "/usr/local/share/ui/BMP/ime/",
        "key-lower-2-1-up.bmp",
        "key-lower-2-1-down.bmp");
    _pKeys[1][0]->setOnClickListener(this);

    _pKeys[2][0] = NULL;


    // the last column
    _pKeys[0][4] = new ImageButton(
        this,
        Point(320, 0), Size(80, 80),
        "/usr/local/share/ui/BMP/ime/",
        "key-all-1-5-disabled.bmp",
        "key-all-1-5-disabled.bmp");
    _pKeys[0][4]->setOnClickListener(this);
    _pKeys[0][4]->SetClickable(false);

    _pKeys[1][4] = new ImageButton(
        this,
        Point(320, 80), Size(80, 120),
        "/usr/local/share/ui/BMP/ime/",
        "key-all-2-5-disabled.bmp",
        "key-all-2-5-disabled.bmp");
    _pKeys[1][4]->setOnClickListener(this);
    _pKeys[1][4]->SetClickable(false);

    _pKeys[2][4] = NULL;


    // other columns
    for (int i = 0; i < 3; i++) { // line
        for (int j = 1; j < 4; j++) { // column
            char name_up[32];
            char name_down[32];
            snprintf(name_up, sizeof(name_up), "key-lower-%d-%d-up.bmp", i + 1, j + 1);
            snprintf(name_down, sizeof(name_down), "key-lower-%d-%d-down.bmp", i + 1, j + 1);
            _pKeys[i][j] = new ImageButton(
                this,
                Point(80 * j, 80 * i), Size(80, 80),
                "/usr/local/share/ui/BMP/ime/",
                name_up,
                name_down);
            _pKeys[i][j]->setOnClickListener(this);
        }
    }
}

SimpleIME::~SimpleIME()
{
    _cancelTimer();

    for (int i = 0; i < KEYBOARD_LINES; i++) { // line
        for (int j = 0; j < KEYBOARD_COLUMNS; j++) { // column
            delete _pKeys[i][j];
            _pKeys[i][j] = NULL;
        }
    }
}

bool SimpleIME::_startTimer()
{
    if (CTimerThread::GetInstance() != NULL) {
        // Time out in 1.5 second
        CTimerThread::GetInstance()->ApplyTimer(
            30, SimpleIME::TimerCallback, this, false, "IME Timer");
    }
    return true;
}

bool SimpleIME::_cancelTimer()
{
    if (CTimerThread::GetInstance() != NULL) {
        CTimerThread::GetInstance()->CancelTimer("IME Timer");
    }
    return true;
}

void SimpleIME::_onTimerEvent()
{
    if (_pTextWatcher) {
        _pTextWatcher->onInputStatusChanged(false);
    }
    _btnInputing = NULL;
    _clickCnt = 0;
}

void SimpleIME::_switchUpperCase(bool bUpper)
{
    _pKeys[0][0]->setSource(
            "/usr/local/share/ui/BMP/ime/",
            "key-lower-1-1-up.bmp",
            "key-lower-1-1-down.bmp");

    char name_up[32];
    char name_down[32];
    snprintf(name_up, sizeof(name_up),
        "key-%s-2-1-up.bmp", bUpper ? "upper" : "lower");
    snprintf(name_down, sizeof(name_down),
        "key-%s-2-1-down.bmp", bUpper ? "upper" : "lower");
    _pKeys[1][0]->setSource(
        "/usr/local/share/ui/BMP/ime/",
        name_up,
        name_down);
    for (int i = 0; i < 3; i++) { // line
        for (int j = 1; j < 4; j++) { // column
            snprintf(name_up, sizeof(name_up),
                "key-%s-%d-%d-up.bmp", bUpper ? "upper" : "lower", i + 1, j + 1);
            snprintf(name_down, sizeof(name_down),
                "key-%s-%d-%d-down.bmp", bUpper ? "upper" : "lower", i + 1, j + 1);
            _pKeys[i][j]->setSource(
                "/usr/local/share/ui/BMP/ime/",
                name_up,
                name_down);
        }
    }
}

void SimpleIME::_switchNumBoard()
{
    _pKeys[0][0]->setSource(
        "/usr/local/share/ui/BMP/ime/",
        "key-num-1-1-up.bmp",
        "key-num-1-1-down.bmp");

    _pKeys[1][0]->setSource(
        "/usr/local/share/ui/BMP/ime/",
        "key-num-2-1-up.bmp",
        "key-num-2-1-down.bmp");

    char name_up[32];
    char name_down[32];
    for (int i = 0; i < 3; i++) { // line
        for (int j = 1; j < 4; j++) { // column
            snprintf(name_up, sizeof(name_up),
                "key-num-%d-%d-up.bmp", i + 1, j + 1);
            snprintf(name_down, sizeof(name_down),
                "key-num-%d-%d-down.bmp", i + 1, j + 1);
            _pKeys[i][j]->setSource(
                "/usr/local/share/ui/BMP/ime/",
                name_up,
                name_down);
        }
    }
}

void SimpleIME::_switchSymbolBoard(int index)
{
    _pKeys[0][0]->setSource(
        "/usr/local/share/ui/BMP/ime/",
        "key-sym-1-1-up.bmp",
        "key-sym-1-1-down.bmp");

    char name_up[32];
    char name_down[32];
    snprintf(name_up, sizeof(name_up),
        "key-sym-2-1-p%01d-up.bmp", index);
    snprintf(name_down, sizeof(name_down),
        "key-sym-2-1-p%01d-down.bmp", index);
    _pKeys[1][0]->setSource(
        "/usr/local/share/ui/BMP/ime/",
        name_up,
        name_down);
    for (int i = 0; i < 3; i++) { // line
        for (int j = 1; j < 4; j++) { // column
            snprintf(name_up, sizeof(name_up),
                "key-sym-%d-%d-p%01d-up.bmp", i + 1, j + 1, index);
            snprintf(name_down, sizeof(name_down),
                "key-sym-%d-%d-p%01d-down.bmp", i + 1, j + 1, index);
            _pKeys[i][j]->setSource(
                "/usr/local/share/ui/BMP/ime/",
                name_up,
                name_down);
        }
    }
}

void SimpleIME::reset()
{
    _indexPage = 0;
    _clickCnt = 0;
    _btnInputing = NULL;
    _switchUpperCase(false);

    _pKeys[0][4]->setSource(
        "/usr/local/share/ui/BMP/ime/",
        "key-all-1-5-disabled.bmp",
        "key-all-1-5-disabled.bmp");
    _pKeys[0][4]->SetClickable(false);
    _bDelEnabled = false;

    _pKeys[1][4]->setSource(
        "/usr/local/share/ui/BMP/ime/",
        "key-all-2-5-disabled.bmp",
        "key-all-2-5-disabled.bmp");
    _pKeys[1][4]->SetClickable(false);
    _bEnterEnabled = false;
}

void SimpleIME::switchDeleteStatus(bool enable)
{
    if (enable) {
        _pKeys[0][4]->setSource(
            "/usr/local/share/ui/BMP/ime/",
            "key-all-1-5-up.bmp",
            "key-all-1-5-down.bmp");
        _pKeys[0][4]->SetClickable(true);
        if (!_bDelEnabled) {
            Draw(true);
            _bDelEnabled = true;
        }
    } else {
        _pKeys[0][4]->setSource(
            "/usr/local/share/ui/BMP/ime/",
            "key-all-1-5-disabled.bmp",
            "key-all-1-5-disabled.bmp");
        _pKeys[0][4]->SetClickable(false);
        if (_bDelEnabled) {
            Draw(true);
            _bDelEnabled = false;
        }
    }
}

void SimpleIME::switchEnterStatus(bool enable)
{
    if (enable) {
        _pKeys[1][4]->setSource(
            "/usr/local/share/ui/BMP/ime/",
            "key-all-2-5-up.bmp",
            "key-all-2-5-down.bmp");
        _pKeys[1][4]->SetClickable(true);
        if (!_bEnterEnabled) {
            Draw(true);
            _bEnterEnabled = true;
        }
    } else {
        _pKeys[1][4]->setSource(
            "/usr/local/share/ui/BMP/ime/",
            "key-all-2-5-disabled.bmp",
            "key-all-2-5-disabled.bmp");
        _pKeys[1][4]->SetClickable(false);
        if (_bEnterEnabled) {
            Draw(true);
            _bEnterEnabled = false;
        }
    }
}

void SimpleIME::onClick(Control *widget)
{
    for (int i = 0; i < KEYBOARD_LINES; i++) {
        for (int j = 0; j < KEYBOARD_COLUMNS; j++) {
            if (widget == _pKeys[i][j]) {
                _cancelTimer();
                if (strcmp(_alphabet[_indexPage][i][j], "Enter") == 0) {
                    if (_pStatusWatcher) {
                        _pStatusWatcher->onHideIME();
                    }
                    if (_pTextWatcher) {
                        _pTextWatcher->onInputStatusChanged(false);
                    }
                    _btnInputing = NULL;
                    _clickCnt = 0;
                } else if (strcmp(_alphabet[_indexPage][i][j], "Del") == 0) {
                    if (_pTextWatcher) {
                        _pTextWatcher->onTextChanged(_alphabet[_indexPage][i][j], true);
                    }
                    _btnInputing = NULL;
                    _clickCnt = 0;
                } else if (strcmp(_alphabet[_indexPage][i][j], "Num") == 0) {
                    // switch to num keyboard
                    _switchNumBoard();
                    _indexPage = 2;
                    Draw(true);

                    if (_pTextWatcher) {
                        _pTextWatcher->onInputStatusChanged(false);
                    }
                    _btnInputing = NULL;
                    _clickCnt = 0;
                } else if (strcmp(_alphabet[_indexPage][i][j], "Symbol") == 0) {
                    // switch to symbol keyboard
                    _switchSymbolBoard(1);
                    _indexPage = 3;
                    Draw(true);

                    if (_pTextWatcher) {
                        _pTextWatcher->onInputStatusChanged(false);
                    }
                    _btnInputing = NULL;
                    _clickCnt = 0;
                } else if (strcmp(_alphabet[_indexPage][i][j], "Upper") == 0) {
                    // switch to upper case keyboard
                    _switchUpperCase(true);
                    _indexPage = 1;
                    Draw(true);

                    if (_pTextWatcher) {
                        _pTextWatcher->onInputStatusChanged(false);
                    }
                    _btnInputing = NULL;
                    _clickCnt = 0;
                } else if (strcmp(_alphabet[_indexPage][i][j], "Lower") == 0) {
                    // switch to lower case keyboard
                    _switchUpperCase(false);
                    _indexPage = 0;
                    Draw(true);

                    if (_pTextWatcher) {
                        _pTextWatcher->onInputStatusChanged(false);
                    }
                    _btnInputing = NULL;
                    _clickCnt = 0;
                } else if (strcmp(_alphabet[_indexPage][i][j], "Ret") == 0) {
                    // return to lower case keyboard
                    _switchUpperCase(false);
                    _indexPage = 0;
                    Draw(true);

                    if (_pTextWatcher) {
                        _pTextWatcher->onInputStatusChanged(false);
                    }
                    _btnInputing = NULL;
                    _clickCnt = 0;
                } else if (strcmp(_alphabet[_indexPage][i][j], "PgDn") == 0) {
                    // switch to symbol keyboard, page 2
                    _switchSymbolBoard(2);
                    _indexPage = 4;
                    Draw(true);

                    if (_pTextWatcher) {
                        _pTextWatcher->onInputStatusChanged(false);
                    }
                    _btnInputing = NULL;
                    _clickCnt = 0;
                } else if (strcmp(_alphabet[_indexPage][i][j], "PgUp") == 0) {
                    // switch to symbol keyboard, page 1
                    _switchSymbolBoard(1);
                    _indexPage = 3;
                    Draw(true);

                    if (_pTextWatcher) {
                        _pTextWatcher->onInputStatusChanged(false);
                    }
                    _btnInputing = NULL;
                    _clickCnt = 0;
                } else {
                    if (_btnInputing) {
                        if (_btnInputing == _pKeys[i][j]) {
                            int txtlen = strlen(_alphabet[_indexPage][i][j]);
                            char input[3] = {0x00};
                            if (txtlen > 1) {
                                // the key contains multiple characters, start a timer to waiting finish
                                _clickCnt++;
                                input[0] = _alphabet[_indexPage][i][j][_clickCnt % txtlen];
                                _startTimer();
                            }
                            if (_pTextWatcher) {
                                _pTextWatcher->onTextChanged(input, true);
                            }
                        } else {
                            if (_pTextWatcher) {
                                // another key pressed, finish the current input
                                _pTextWatcher->onInputStatusChanged(false);
                            }

                            char input[3] = {0x00};
                            input[0] = _alphabet[_indexPage][i][j][0];
                            if (_pTextWatcher) {
                                _pTextWatcher->onTextChanged(input, false);
                            }

                            if (strlen(_alphabet[_indexPage][i][j]) > 1) {
                                // the key contains multiple characters, start a timer to waiting finish
                                if (_pTextWatcher) {
                                    _pTextWatcher->onInputStatusChanged(true);
                                }
                                _btnInputing = _pKeys[i][j];
                                _clickCnt = 0;
                                _startTimer();
                            } else {
                                // the key contains one characters at most, finish input
                                if (_pTextWatcher) {
                                    _pTextWatcher->onInputStatusChanged(false);
                                }
                                _clickCnt = 0;
                                _btnInputing = NULL;
                            }
                        }
                    } else {
                        char input[3] = {0x00};
                        input[0] = _alphabet[_indexPage][i][j][0];
                        if (_pTextWatcher) {
                            _pTextWatcher->onTextChanged(input, false);
                        }

                        if (strlen(_alphabet[_indexPage][i][j]) > 1) {
                            // the key contains multiple characters, start a timer to waiting finish
                            if (_pTextWatcher) {
                                _pTextWatcher->onInputStatusChanged(true);
                            }
                            _btnInputing = _pKeys[i][j];
                            _clickCnt = 0;
                            _startTimer();
                        } else {
                            // the key contains one characters at most, finish input
                            if (_pTextWatcher) {
                                _pTextWatcher->onInputStatusChanged(false);
                            }
                            _clickCnt = 0;
                            _btnInputing = NULL;
                        }
                    }
                }
            }
        }
    }
}

void SimpleIME::TimerCallback(void *para)
{
    SimpleIME *ime = (SimpleIME *)para;
    if (ime) {
        ime->_onTimerEvent();
    }
}

};
