#ifndef __Camera_Dialogues_h__
#define __Camera_Dialogues_h__

#include <stdint.h>

#include "GobalMacro.h"
#include "Window.h"

class CCameraProperties;

namespace ui{

class PowerOffWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    PowerOffWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~PowerOffWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    CCameraProperties *_cp;

    Panel           *_pPanel;
    BmpImage        *_pAnimation;
    StaticText      *_pTxt;
    ImageButton     *_pYes;
    ImageButton     *_pNo;

    int             _poweroffCnt;
};

class USBStorageWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    USBStorageWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~USBStorageWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    enum {
        USB_NotIn = 0,
        USB_Enabling,
        USB_Working,
        USB_Exit_NeedCorfirm,
        USB_Exiting,
    };
    bool _shouldExitUSBStorage();
    bool _isAlreadyUSBStorage();
    bool _getOriginalWifiMode();
    bool _getOriginalBTMode();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    BmpImage        *_pBG;
    BmpImage        *_pLaptop;
    StaticText      *_pTxt;
    ImageButton     *_pYes;
    ImageButton     *_pNo;
    ImageButton     *_pExit;

    bool            _bInUSBStorage;
    int             _workStep;
    int             _originWifiMode;
    bool            _bOriginBTOn;
    int             _counter;
};

class CarMountUnplugWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    CarMountUnplugWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~CarMountUnplugWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    int _getAutoOffTime();

    bool _shouldContinueCountdown();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    Button          *_pRing;
    StaticText      *_pTitle;
    Button          *_pTime;
    ImageButton     *_pYes;
    ImageButton     *_pNo;
    BmpImage        *_pAnimation;
    int             _poweroffCnt;

    char            _txt[64];
    int             _counter;
};

class SpaceFullWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    SpaceFullWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~SpaceFullWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    CCameraProperties *_cp;

    Panel           *_pPanel;
    FanRing         *_pRing;
    BmpImage        *_pIcon;
    StaticText      *_pTxtError;
    StaticText      *_pTxtHint;
    Button          *_pOK;
    Button          *_pOKFake;
};

class CardMissingWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    CardMissingWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~CardMissingWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    CCameraProperties *_cp;

    Panel           *_pPanel;
    FanRing         *_pRing;
    BmpImage        *_pIcon;
    StaticText      *_pTxtError;
    StaticText      *_pTxtHint;
    Button          *_pOK;
    Button          *_pOKFake;
};

class WriteDiskErrorWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    WriteDiskErrorWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WriteDiskErrorWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    CCameraProperties *_cp;

    Panel           *_pPanel;
    FanRing         *_pRing;
    BmpImage        *_pIcon;
    StaticText      *_pTxtError;
    StaticText      *_pTxtHint;
    Button          *_pOK;
    Button          *_pOKFake;
};

class UnknowSDFormatWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    UnknowSDFormatWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~UnknowSDFormatWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    enum {
        FormatStep_Idle = 0,
        FormatStep_NeedCorfirm,
        FormatStep_Formatting,
    };

    void updateStorageStatus();

    CCameraProperties *_cp;

    Panel           *_pPanel;

    Circle          *_pRing;
    BmpImage        *_pIcon;

    Button          *_pFormat;
    StaticText      *_pFormatInfo;
    StaticText      *_pFormatHint;

    ImageButton     *_pYes;
    ImageButton     *_pNo;

    BmpImage        *_pAnimation;

    int             _formatStep;

    char            _txt[64];
    int             _counter;
    int             _delayClose;
};

class HighlightWnd : public Window
{
public:
    typedef Window inherited;
    HighlightWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~HighlightWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    CCameraProperties *_cp;

    Panel           *_pPanel;
    BmpImage        *_pIcon;
    int             _counter;
};

class HighlightFailedWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    HighlightFailedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~HighlightFailedWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    void updateStatus();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    Circle          *_pRing;
    BmpImage        *_pIcon;
    StaticText      *_pTxtTitle;
    StaticText      *_pTxtHint;
    Button          *_pButton;
    Button          *_pButtonFake;
    char            _txt[16];
    int             _counter;
};

class BTNotifyWnd : public Window, public OnClickListener
{
public:
    enum Notification_Type {
        Notification_Type_NULL = 0,
        Notification_Type_OBD_Connected,
        Notification_Type_OBD_Disconnected,
        Notification_Type_HID_Connected,
        Notification_Type_HID_Disconnected,
    };

    typedef Window inherited;
    BTNotifyWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop,
        Notification_Type type);
    virtual ~BTNotifyWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateStatus();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    Circle          *_pRing;
    BmpImage        *_pIcon;
    StaticText      *_pTxtHint;

    Button          *_pBtnOK;

    StaticText      *_pInfo;
    ImageButton     *_pBtnYes;
    ImageButton     *_pBtnNo;
    int             _batt_level;

    int             _counter;

    Notification_Type _type;
};

class OBDConnectedWnd : public BTNotifyWnd
{
public:
    OBDConnectedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~OBDConnectedWnd();
};

class OBDDisconnectedWnd : public BTNotifyWnd
{
public:
    OBDDisconnectedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~OBDDisconnectedWnd();
};

class HIDConnectedWnd : public BTNotifyWnd
{
public:
    HIDConnectedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~HIDConnectedWnd();
};

class HIDDisconnectedWnd : public BTNotifyWnd
{
public:
    HIDDisconnectedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~HIDDisconnectedWnd();
};

class BTDefectWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    BTDefectWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~BTDefectWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateStatus();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    Circle          *_pRing;

    StaticText      *_pTxtHint;
    StaticText      *_pTxtDefect;
    char            _txtDefect[64];

    Button          *_pBtnClear;
    Button          *_pBtnIgnore;

    List            *_pList;
    TextListAdapter *_pAdapter;
#define MAX_CODES   28
    char            *_dtc_codes[MAX_CODES];
    int             _num;
};

class BTUpgradingWnd : public Window
{
public:
    enum Upgrading_Type {
        Upgrading_Type_NULL = 0,
        Upgrading_Type_OBD,
        Upgrading_Type_HID,
    };

    typedef Window inherited;
    BTUpgradingWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop,
        Upgrading_Type type);
    virtual ~BTUpgradingWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    void updateStatus();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    Circle          *_pRing;
    BmpImage        *_pIcon;
    StaticText      *_pTxtHint;
    StaticText      *_pTxtUpgrading;
    char            _txtUpgrading[32];

    int             _counter;
    Upgrading_Type  _type;
};

class HIDUpgradingWnd : public BTUpgradingWnd
{
public:
    HIDUpgradingWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~HIDUpgradingWnd();
};

class OBDUpgradingWnd : public BTUpgradingWnd
{
public:
    OBDUpgradingWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~OBDUpgradingWnd();
};


class OverheatWnd : public Window
{
public:
    typedef Window inherited;
    OverheatWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~OverheatWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    bool _isInAWeek();
    void _updateStatus();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    BmpImage        *_pBG;
    BmpImage        *_pIcon;
    StaticText      *_pTxt;

    StaticText      *_pTemp;
    char            _txtTemp[128];

    int             _origin_state;
    int             _counter;
    bool            _bInAWeek;
};

class BTNoBindWnd : public Window, public OnClickListener, public OnListClickListener
{
public:
    typedef Window inherited;

    enum EBTType {
        BTType_OBD = 0,
        BTType_HID,
    };

    BTNoBindWnd(CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop,
        EBTType type);
    virtual ~BTNoBindWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onListClicked(Control *list, int index);

private:
    void parseBT(char *json, int type);
    bool isOBDBinded();
    bool isHIDBinded();

    enum State {
        State_Wait_Confirm_To_Scan = 0,
        State_Wait_Confirm_To_Exit,
        State_Scanning,
        State_Scan_Timeout,
        State_Got_List,
        State_Wait_To_Bind_OBD,
        State_Wait_To_Bind_HID,
        State_Bind_Timeout,
    };

    enum EBTErr {
        BTErr_OK,
        BTErr_TryLater,
        BTErr_Err,
    };

    EBTType           _type;
    CCameraProperties *_cp;

    Panel           *_pPanel;
    StaticText      *_pTxt;
    ImageButton     *_pIconYes;
    ImageButton     *_pIconNo;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;
    StaticText      *_pTitle;
    UILine          *_pLine;
    List            *_pList;
    TextImageAdapter *_pAdapter;
    char            **_pBTList;
    int             _numBTs;

    Button          *_pHelp;
    ImageButton     *_pReScan;
    StaticText      *_pHelpInfo;

    StaticText      *_pInfo;
    BmpImage        *_pAnimation;
    int             _cntAnim;
    BmpImage        *_pBindResult;

    int             _state;
    int             _cntDelayClose;

    int             _counter;
};

class OBDNotBindedWnd : public BTNoBindWnd
{
public:
    OBDNotBindedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~OBDNotBindedWnd();
};

class HIDNotBindedWnd : public BTNoBindWnd
{
public:
    HIDNotBindedWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~HIDNotBindedWnd();
};

class WrongWifiPwdWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    WrongWifiPwdWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WrongWifiPwdWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    CCameraProperties *_cp;

    Panel           *_pPanel;
    FanRing         *_pRing;
    BmpImage        *_pIcon;
    StaticText      *_pTxtHint;
    char            _txtHint[128];
    Button          *_pOK;
    Button          *_pOKFake;
};

class WifiAddSuccessWnd : public Window
{
public:
    typedef Window inherited;
    WifiAddSuccessWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WifiAddSuccessWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);

private:
    CCameraProperties *_cp;

    Panel           *_pPanel;
    FanRing         *_pRing;
    BmpImage        *_pIcon;
    StaticText      *_pTxtHint;
    char            _txtHint[128];
    int             _counter;
};

class LoopSpaceWarningWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    LoopSpaceWarningWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~LoopSpaceWarningWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    void showDialogue();
    void checkSpaceWarning();

    CCameraProperties *_cp;

    Panel           *_pPanel;
    FanRing         *_pRing;
    BmpImage        *_pIcon;
    StaticText      *_pTxtHint;

    Button          *_pBtnMore;
    Button          *_pOK;

    Scroller        *_pScroller;
    Size            _visibleSizeScroller;

    // the following widgets are added to Scroller
    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;
    StaticText      *_pTitle;
    UILine          *_pLine;
    StaticText      *_pGuide;
    char            _txt[256];
    Button          *_pClose;

    int             _counter;
};

class LoopSpaceWarningDetailWnd : public Window, public OnClickListener
{
public:
    typedef Window inherited;
    LoopSpaceWarningDetailWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~LoopSpaceWarningDetailWnd();

    virtual void onResume();
    virtual void onPause();
    virtual bool OnEvent(CEvent *event);
    virtual void onClick(Control *widget);

private:
    void showDialogue();
    CCameraProperties *_cp;

    Panel           *_pPanel;

    Scroller        *_pScroller;
    Size            _visibleSizeScroller;

    // the following widgets are added to Scroller
    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;
    StaticText      *_pTitle;
    UILine          *_pLine;
    StaticText      *_pGuide;
    char            _txt[256];
    Button          *_pClose;
};

};

#endif
