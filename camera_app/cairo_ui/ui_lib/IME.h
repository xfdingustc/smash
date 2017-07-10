#ifndef __UI_LIB_IME_H__
#define __UI_LIB_IME_H__

#include "Widgets.h"

namespace ui{

class EditTextWatcher
{
public:
    EditTextWatcher() {}
    virtual ~EditTextWatcher() {}
    virtual void onEditTextChanged() = 0;
};

class TextWatcher
{
public:
    TextWatcher() {}
    virtual ~TextWatcher() {}
    virtual void onTextChanged(const char *characters, bool bReDraw) = 0;
    virtual void onInputStatusChanged(bool bInputing) = 0;
};

class EditText : public Control, public TextWatcher
{
public:
    typedef Control inherited;
    EditText(Container* pParent, const Point& leftTop, const Size& CSize);
    virtual ~EditText();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    virtual void onTextChanged(const char *characters, bool bReDraw = true);
    virtual void onInputStatusChanged(bool bInputing);

    void setOnClickListener(OnClickListener *listener) { _listener = listener; }
    void setWatcher(EditTextWatcher *watcher) { _watcher = watcher; }

    char* GetText() { return _pText; }

    void SetHintText(const char *hint)
    {
        snprintf(_pText, sizeof(_pText), hint);
        _bHasHint = true;
    }

    void SetTextStyle(UINT16 size, UINT16 fonttype, UINT16 color)
    {
        _txtSize = size;
        _fonttype = fonttype;
        _txtColor = color;
    }

    void SetTextColor(UINT16 color) { _txtColor = color; }
    void SetFont(UINT16 fonttype) { _fonttype = fonttype; }
    void SetTextSize(UINT16 size) { _txtSize = size; }

    void SetBGColor(UINT16 color) { _bgColor = color; }

    void SetFocus(bool focus)
    {
        _bFocus = focus;
        if (_bHasHint) {
            _bHasHint = false;
            memset(_pText, 0x00, sizeof(_pText));
        }
    }

    void SetPassword(bool bpwd) { _bPassword = bpwd; }

    void reset() { memset(_pText, 0x00, sizeof(_pText)); }

private:
    CMutex          *_pLock;

    OnClickListener *_listener;
    EditTextWatcher *_watcher;

    char    _pText[64];
    UINT16  _fonttype;
    UINT16  _txtSize;
    UINT16  _txtColor;

    UINT16  _bgColor;
    bool    _bFocus;
    bool    _bPassword;

    bool    _bHasHint;

    bool    _bIMEInputing;
    bool    _bShowCursor;

    UINT8   _counterET;
};

class IMEStatusWatcher
{
public:
    IMEStatusWatcher() {}
    ~IMEStatusWatcher() {}
    virtual void onHideIME() = 0;
};

#define KEYBOARD_LINES      3
#define KEYBOARD_COLUMNS    5
class SimpleIME : public Container, OnClickListener
{
public:
    typedef Container inherited;
    SimpleIME(Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size);
    virtual ~SimpleIME();

    virtual void onClick(Control *widget);

    void setTextWatcher(TextWatcher *watcher) { _pTextWatcher = watcher; }
    void setStatusWatcher(IMEStatusWatcher *watcher) { _pStatusWatcher = watcher; }

    void reset();

    void switchDeleteStatus(bool enable);
    void switchEnterStatus(bool enable);

private:
    bool _startTimer();
    bool _cancelTimer();
    void _onTimerEvent();

    void _switchUpperCase(bool bUpper);
    void _switchNumBoard();
    void _switchSymbolBoard(int index);

    static void TimerCallback(void*);

    TextWatcher         *_pTextWatcher;
    IMEStatusWatcher    *_pStatusWatcher;

    ImageButton         *_pKeys[KEYBOARD_LINES][KEYBOARD_COLUMNS];
    int                 _indexPage;
    const char*         (*_alphabet)[KEYBOARD_LINES][KEYBOARD_COLUMNS];

    int                 _clickCnt;
    ImageButton         *_btnInputing;

    bool                _bDelEnabled;
    bool                _bEnterEnabled;
};

};

#endif
