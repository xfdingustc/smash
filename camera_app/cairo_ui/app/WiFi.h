#ifndef __WIFI_Settings_H__
#define __WIFI_Settings_H__

#include <stdint.h>

#include "GobalMacro.h"
#include "Window.h"
#include "IME.h"

#include "CCameraCircularUI.h"

class CCameraProperties;

namespace ui{

class WifiMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    WifiMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WifiMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void initScroller();
    void updateWifi();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    Scroller        *_pScroller;
    Size            _visibleSizeScroller;
    Size            _scrollableSize;

    // the following widgets are added to Scroller
    StaticText      *_pStatus;
    Button          *_pSwitchFake;
    BmpImage        *_pSwitch;

    Button          *_pModeFake;
    StaticText      *_pMode;
    StaticText      *_pModeResult;
    char            _txtMode[16];

    StaticText      *_pSSID;
    StaticText      *_pSSIDValue;
    char            _txtSSID[16];

    StaticText      *_pPWD;
    StaticText      *_pPWDValue;
    char            _txtPWD[16];

    StaticText      *_pClient;
    StaticText      *_pWifiInfo;
    Button          *_pClientInfoFake;
    char            _txtWifiInfo[256];

    UIRectangle     *_pQRBG;
    QrCodeControl   *_pQrCode;

    Button          *_pAutoAPFake;
    StaticText      *_pAutoAP;
    StaticText      *_pAutoAPInfo;
    char            _txtAutoAP[16];

    int             _counter;

    BmpImage        *_pAnimation;
    int             _cntTimeout;
    int             _targetMode;
};

class WifiModeMenuWnd : public MenuWindow,
                    public OnClickListener,
                    public OnPickersListener
{
public:
    typedef MenuWindow inherited;
    WifiModeMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WifiModeMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onPickersFocusChanged(Control *picker, int indexFocus);

private:
    void updateWifi();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pTitle;
    UILine          *_pLine;

    Pickers         *_pPickers;
    Size            _pickerSize;
    char            *_pMenus[3];
    UINT8           _numMenus;

    BmpImage        *_pAnimation;
    BmpImage        *_pIconResult;
    StaticText      *_pTxtResult;

    int             _counter;
    int             _targetMode;
    int             _delayClose;
};

class AutoAPMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    AutoAPMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~AutoAPMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateStatus();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pInfo;
    char            _txt[256];

    StaticText      *_pStatus;
    Button          *_pSwitchFake;
    BmpImage        *_pSwitch;
};

class WifiNoConnectWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    WifiNoConnectWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WifiNoConnectWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void initScroller();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    Scroller        *_pScroller;
    Size            _visibleSizeScroller;

    // the following widgets are added to Scroller
    StaticText      *_pTitle;
    UILine          *_pLine;
    StaticText      *_pGuide;
    char            _txt[256];
    Button          *_pHotspot;

    int             _counter;

    BmpImage        *_pAnimation;
    int             _cntTimeout;
    int             _targetMode;
};

class WifiDetailWnd : public MenuWindow,
                    public OnClickListener, public OnListClickListener
{
public:
    typedef MenuWindow inherited;
    WifiDetailWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WifiDetailWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onListClicked(Control *list, int index);

private:
    void updateWifi();
    void parseWifi(char *json);

    enum {
        State_Show_Detail = 0,
        State_Scan_Wifi,
        State_Scan_Wifi_Timeout,
        State_Show_Wifi_List,
        State_Connecting,
        State_Connect_Success,
        State_Connect_Failed,
        State_Connect_Timeout,
    };

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    StaticText      *_pSSID;
    char            _txt[256];
    Button          *_pDisconnect;

    StaticText      *_pTitle;
    UILine          *_pLine;
    List            *_pList;
    TextListAdapter *_pAdapter;
    char            **_wifiList;
    int             _wifiNum;
    char            _ssid[256];

    StaticText      *_pInfo;
    char            _txtInfo[256];
    BmpImage        *_pAnimation;
    int             _cntAnim;
    BmpImage        *_pConnectResult;

    int             _state;
    int             _counter;
    int             _cntDelayClose;
    int             _cntTimeout;
};

typedef struct WifiItem {
    WifiItem()
        : ssid(NULL)
        , signal_level(-100)
        , added(false)
        , current(false)
        , secured(false)
    {
    }
    ~WifiItem()
    {
        if (ssid) {
            delete[] ssid;
            ssid = NULL;
        }
    }
    char    *ssid;
    int     signal_level;
    bool    added;
    bool    current;
    bool    secured;
} WifiItem;

class WifiListAdapter : public ListAdaptor
{
public:
    WifiListAdapter();
    virtual ~WifiListAdapter();
    virtual UINT16 getCount();
    virtual Control* getItem(UINT16 position, Container* pParent, const Size& size);
    virtual Control* getItem(UINT16 position, Container* pParent);
    virtual Size& getItemSize() { return _itemSize; }

    void setItemSize(const Size& size) { _itemSize = size; }

    void setWifiList(WifiItem **pList, UINT16 num);

private:
    WifiItem    **_pList;
    UINT16      _num;

    Size        _itemSize;
};

class WifiClientWnd : public MenuWindow,
                    public OnClickListener,
                    public OnListClickListener
{
public:
    typedef MenuWindow inherited;
    WifiClientWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~WifiClientWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onListClicked(Control *list, int index);

private:
    enum {
        State_Scan_Wifi = 0,
        State_Scan_Wifi_Timeout,
        State_Show_Wifi_List,
        State_Waiting_Input,
        State_Waiting_To_Connect,
        State_Show_Forget,
        State_Connecting,
        State_Connect_Success,
        State_Connect_Failed,
        State_Connect_Timeout,
    };

    void _getInitWifi();

    void _updateWifi();
    void _parseWifi(char *json);
    void _connectWifi(char *ssid, char *pwd);

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

// wifi list:
    StaticText      *_pTitle;
    UILine          *_pLine;
    List            *_pList;
    WifiListAdapter *_pAdapter;
    WifiItem        **_wifisFound;
    int             _wifiNum;

// dialogue to connect or forget:
    StaticText      *_pSSID;
    char            _txtSSID[256];
    Button          *_pConnect;
    Button          *_pForget;

// animation:
    BmpImage        *_pAnimation;
    int             _cntAnim;

// help info:
    StaticText      *_pInfo;
    char            _txtInfo[256];
    BmpImage        *_pConnectResult;
    Button          *_pReset;
    Button          *_pCancel;

    int             _state;
    int             _counter;
    int             _cntDelayClose;
    int             _cntTimeout;
};

};
#endif
