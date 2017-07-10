#ifndef __Bluetooth_Settings_H__
#define __Bluetooth_Settings_H__

#include <stdint.h>

#include "GobalMacro.h"
#include "Window.h"
#include "aggps.h"
#include "CCameraCircularUI.h"

class CCameraProperties;

namespace ui{

class BluetoothMenuWnd : public MenuWindow,
                    public OnClickListener
{
public:
    typedef MenuWindow inherited;
    BluetoothMenuWnd(CCameraProperties *cp, const char *name, Size wndSize, Point leftTop);
    virtual ~BluetoothMenuWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);

private:
    void updateBluetooth();

    Panel           *_pPanel;

    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    Button          *_pStatusBtnFake;
    StaticText      *_pStatus;
    BmpImage        *_pSwitch;

    Button          *_pHIDBtnFake;
    StaticText      *_pHIDTitle;
    BmpImage        *_pHIDIcon;

    Button          *_pOBDBtnFake;
    StaticText      *_pOBDTitle;
    BmpImage        *_pOBDIcon;

    BmpImage        *_pAnimation;
    int             _cntTimeout;
    int             _targetMode;

    int             _counter;
    bool            _bConnectingOBD;
    bool            _bConnectingRC;
};

class BTDetailWnd : public MenuWindow, public OnClickListener, public OnListClickListener
{
public:
    typedef MenuWindow inherited;

    enum EBTType {
        BTType_OBD = 0,
        BTType_HID,
    };

    BTDetailWnd(CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop,
        EBTType type);
    virtual ~BTDetailWnd();

    virtual void onMenuResume();
    virtual void onMenuPause();
    virtual bool OnEvent(CEvent *event);

    virtual void onClick(Control *widget);
    virtual void onListClicked(Control *list, int index);

private:
    void parseBT(char *json, int type);
    bool isOBDBinded();
    bool isHIDBinded();
    void updateBTDetail();
    void updateConnectingStatus();

    enum State {
        State_Detail_Show_Info = 0,
        State_Detail_No_Device,
        State_Wait_Confirm_To_Remove,
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

    EBTType         _type;

    Panel           *_pPanel;
    Button          *_pReturnBtnFake;
    ImageButton     *_pReturnBtn;

    BmpImage        *_pIconBT;
    StaticText      *_pBTInfoDetail;
    char            _btInfoDetail[64];
    Button          *_pBtnRemove;

    StaticText      *_pTxt;
    ImageButton     *_pIconYes;
    ImageButton     *_pIconNo;

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
    bool            _bConnecting;
};

class OBDDetailWnd : public BTDetailWnd
{
public:
    OBDDetailWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~OBDDetailWnd();
};

class HIDDetailWnd : public BTDetailWnd
{
public:
    HIDDetailWnd(
        CCameraProperties *cp,
        const char *name,
        Size wndSize,
        Point leftTop);
    virtual ~HIDDetailWnd();
};

};

#endif
