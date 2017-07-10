#ifndef __IME_WND_H__
#define __IME_WND_H__

#include <stdint.h>

#include "GobalMacro.h"
#include "Window.h"
#include "IME.h"

#include "CCameraCircularUI.h"

namespace ui{

class IMEWnd : public MenuWindow,
                    public OnClickListener,
                    public IMEStatusWatcher,
                    public EditTextWatcher
{
public:
    typedef MenuWindow inherited;
    IMEWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~IMEWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

    virtual void onHideIME();

    virtual void onEditTextChanged();

    void setInitValue(char *title, bool bpassword);

private:
    void _exit(bool gotInput);

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    char            _txtTitle[64];
    BmpImage        *_pIconPwd;
    EditText        *_pEditText;
    SimpleIME       *_pIME;

    bool            _bPassword;
};

};

#endif
