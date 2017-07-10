#ifndef __Camera_Vidit_h
#define __Camera_Vidit_h

#include <stdint.h>

#include "GobalMacro.h"
#include "agbt.h"

class CCameraProperties;
class CBitInforWnd;
class CBitQRCodeWnd;
class COLEDBrightnessWnd;
class CBitPowerOffPreapreWnd;

namespace ui{

/*******************************************************************************
*   CViditAudioSybl
*
*/
class CViditAudioSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_audioOn = 0,
        symble_audioOff,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CViditAudioSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CViditAudioSybl();
    virtual bool OnEvent(CEvent *event);
    void updateSymble();

protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CViditBatterySybl
*
*/
class CViditBatterySybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_charge_100 = 0,
        symble_charge_80,
        symble_charge_60,
        symble_charge_40,
        symble_charge_20,
        symble_charge_0,

        symble_100,
        symble_80,
        symble_60,
        symble_40,
        symble_20,
        symble_0,

        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CViditBatterySybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CViditBatterySybl();
    virtual bool OnEvent(CEvent *event);
    void updateSymble();

protected:
    CCameraProperties* _cp;
};

class CViditStorageSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_storage_100 = 0,
        symble_storage_80,
        symble_storage_60,
        symble_storage_40,
        symble_storage_20,
        symble_storage_0,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CViditStorageSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CViditStorageSybl();
    virtual bool OnEvent(CEvent *event);
    void updateSymble();

protected:
    CCameraProperties* _cp;
};


/*******************************************************************************
*   CViditWifiSybl
*
*/
class CViditWifiSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_ap = 0,
        symble_clt,
        symble_off,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CViditWifiSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CViditWifiSybl();
    virtual bool OnEvent(CEvent *event);
    void updateSymble();

protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CViditBtSybl
*
*/
class CViditBtSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_bt_disconnect = 0,
        symble_bt_connect,
        symble_bt_hid_paired,
        symble_bt_obd_paired,
        symble_bt_hid_obd_paried,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CViditBtSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CViditBtSybl();
    virtual bool OnEvent(CEvent *event);
    void updateSymble();

protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CViditGPSSybl
*
*/
class CViditGPSSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_gps_offline = 0,
        symble_gps_ready,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CViditGPSSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CViditGPSSybl();
    virtual bool OnEvent(CEvent *event);
    void updateSymble();

protected:
    CCameraProperties* _cp;
};

class CViditCycleRecSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_cyclerec = 0,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CViditCycleRecSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CViditCycleRecSybl();
    virtual bool OnEvent(CEvent *event);
protected:
    CCameraProperties* _cp;
};

class CViditLanguageSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_language = 0,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CViditLanguageSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left);
    ~CViditLanguageSybl();
    virtual bool OnEvent(CEvent *event);
protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CLanguageSelectionWnd
*
*/
class CLanguageSelectionWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CLanguageSelectionWnd(
        CWndManager *pMgr
        ,CCameraProperties* cp
    );
    ~CLanguageSelectionWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

private:
    enum steps {
        step_language_options = 0,
        step_set_language_1,
        step_set_language_2,
        step_num,
    };

    CCameraProperties   *_cp;
    CPanel              *_pPannel;

    CBmpControl *_pBmp_1;
    CBmpControl *_pBmp_2;
    CBmpControl *_pBmp_3;
    CBmpControl *_pBmp_4;
    CStaticText *_pLanguages[2];

    CBmpControl *_pBmp_logo;

    int         _numLang;
    char        **_ppList;
    char        _pHintConfirm[2][maxMenuStrinLen];
    char        _pHintCancel[2][maxMenuStrinLen];
    int         _currentStep;
};

class WindowLayout
{
public:
    WindowLayout(CWnd *owner, CCameraProperties* cp);
    virtual ~WindowLayout();

    virtual void onFocus() = 0;
    virtual bool OnEvent(CEvent *event) = 0;
    CContainer* getLayoutRoot() { return _pRoot; }

protected:
    CCameraProperties *_cp;
    CPanel            *_pRoot;
};

class StatusLayout : public WindowLayout
{
public:
    StatusLayout(CWnd *owner, CCameraProperties* cp);
    virtual ~StatusLayout();

    virtual void onFocus();
    virtual bool OnEvent(CEvent *event);

private:
    void updateRecUI();
    void updateBatteryUI();
    void updateStorageUI();
    void getLanguageType();
    void getPTLInterval();

    CStaticText     *_pModeSymble;

    CViditGPSSybl   *_pGpsSymble;
    CViditCycleRecSybl   *_pLoopRecSymble;
    CStaticText     *_pText_Res;

    CStaticText     *_pText_Bat;
    CStaticText     *_pText_Storage;

    CViditStorageSybl *_pStorageSymble;
    CViditBatterySybl *_pBatterySymble;

    CBlock          *_pHRow;
    CBlock          *_pVRow;

    CViditAudioSybl   *_pAudioSymble;
    CViditBtSybl      *_pBtSymble;

    CViditWifiSybl    *_pWifiSymble;

    char _pResolution[maxMenuStrinLen];
    char _pTimeInfo[maxMenuStrinLen];
    char _pBatteryInfo[maxMenuStrinLen];
    char _pStorageInfo[maxMenuStrinLen];

    enum StillCap_State {
        StillCap_State_Idle = 0,
        StillCap_State_SINGLE = 1,
        StillCap_State_BURST = 2,
        StillCap_State_STOPPING = 3,
    };
    int  _stillcap_mode;
    int  _cnt_result_showtime;

    float _ptl_interval;

    int _heartBeat;
    int _languageType; // 0:English, 1:Chinese, 2:Russian
};

class WifiInfoLayout : public WindowLayout
{
public:
    WifiInfoLayout(CWnd *owner, CCameraProperties* cp);
    virtual ~WifiInfoLayout();

    virtual void onFocus();
    virtual bool OnEvent(CEvent *event);

private:
    void updateInfo();

    CViditWifiSybl *_pWifiSymble;
    CStaticText    *_pTitle;
    CBlock         *_pRow;
    CStaticText    *_textInfor;

    char           _text_1[128];
    char           _text_2[128];
};

class WifiModeLayout : public WindowLayout
{
public:
    WifiModeLayout(CWnd *owner, CCameraProperties* cp);
    virtual ~WifiModeLayout();

    virtual void onFocus();
    virtual bool OnEvent(CEvent *event);

    void highlightNext();
    int getTargetMode();

private:
    CViditWifiSybl      *_pSymble;
    CBlock              *_pRow;
    CHSelector          *_pSelector;
    CStaticText         *_pAP;
    CStaticText         *_pCLT;
    CStaticText         *_pOFF;
};

class HintLayout : public WindowLayout
{
public:
    enum HintType {
        hint_wifi_long_press = 0,
        hint_changing_wifi_mode,
        hint_changing_wifi_success,
        hint_changing_wifi_timeout,
        hint_wifi_no_change,
        hint_mark,
        hint_mark_car_mode,
        hint_poweroff,
        hint_start_recording,
        hint_stop_recording,
        hint_set_still_in_recording,
        hint_no_TF,
        hint_TF_full,
        hint_TF_Error,
        hint_Change_Mode,
        hint_count_down,
        hint_num,
    };

    HintLayout(CWnd *owner, CCameraProperties* cp);
    virtual ~HintLayout();

    virtual void onFocus();
    virtual bool OnEvent(CEvent *event);

    void setHintType(int type);
    void setText(char *txt);

private:
    CStaticText       *_pNotice;
    char              _pTitleInfo[64];
    CBlock            *_pRow;

    CStaticText       *_textInfor_1;
    char              _pDialogInfo[64];
};

class CBitStorageWnd;
class CBitDialogWnd;

class CBitScreenSaveWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CBitScreenSaveWnd(CWndManager *pMgr, CCameraProperties* cp);
    virtual ~CBitScreenSaveWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

private:
    void updateUI();
    void updateStorageUI();

    CCameraProperties *_cp;
    CPanel            *_pPannel;

    CStaticText  *_pHint_1;
    CStaticText  *_pHint_2;
    int          _screenSaverCount;
    int          _info_display_time;

    char         _pTimeInfo[maxMenuStrinLen];
    char         _pSpaceInfo[maxMenuStrinLen];

    CBitStorageWnd    *_pStorageWnd;
    CBitDialogWnd     *_pDialogWnd;

    int _languageType; // 0:English, 1:Chinese, 2:Russian
};

class CBitStorageWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CBitStorageWnd(CWndManager *pMgr, CCameraProperties* cp);
    virtual ~CBitStorageWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

private:
    enum formatStep {
        formatStep_None = 0,
        formatStep_1 = 1,
        formatStep_2,
    };

    void updateUI();

    CCameraProperties *_cp;
    CPanel            *_pPannel;

    CBmpControl *_pBmp_1;
    CBmpControl *_pBmp_2;
    CBmpControl *_pBmp_3;
    CBmpControl *_pBmp_4;

    CStaticText  *_textInfor_1;

    int   _formatStep;
    char  _pDialogInfo[64];
    int   _displayTime;
};

class CBitDialogWnd : public CWnd
{
public:
    enum DialogType {
        dialog_wifi = 0,
        dialog_changing_wifi_mode,
        dialog_changing_wifi_success,
        dialog_changing_wifi_timeout,
        dialog_wifi_no_change,
        dialog_mark,
        dialog_poweroff,
        dialog_start_recording,
        dialog_stop_recording,
        dialog_language_changed,
        dialog_num,
    };

    typedef CWnd inherited;
    CBitDialogWnd(CWndManager *pMgr, CCameraProperties* cp);
    virtual ~CBitDialogWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

    void setDialogType(int type, int timeShow);

private:
    CCameraProperties *_cp;
    CPanel            *_pPannel;

    CStaticText       *_pNotice;
    CBlock            *_pRow;
    CStaticText       *_textInfor_1;

    int   _timeoutCnt;
    char  _pDialogInfo[64];
};

class CBitPowerOffWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CBitPowerOffWnd(CWndManager *pMgr, CCameraProperties* cp);
    ~CBitPowerOffWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);
private:
    CCameraProperties   *_cp;
    CPanel              *_pPannel;

    CBmpControl         *_pBmp_1;
    CBmpControl         *_pBmp_2;
    CBmpControl         *_pBmp_3;
    CBmpControl         *_pBmp_4;

    CStaticText         *_pConfirm;
    CStaticText         *_pCancel;
    CStaticText         *_pT;
    char                txt[32];
};

class CBitCameraWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CBitCameraWnd(CWndManager *pMgr, CCameraProperties* cp);
    virtual ~CBitCameraWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

    void showDefaultWnd();

private:
    void setCurrentLayout(WindowLayout *layout, bool update);

    CCameraProperties *_cp;
    CWndManager       *_pMgr;
    WindowLayout      *_pCurrentLayout;
    WindowLayout      *_pTargetLayout;
    int               _hintShowTime;

    StatusLayout     *_pStatusLayout;
    WifiInfoLayout   *_pWifiInfoLayout;
    WifiModeLayout   *_pWifiModeLayout;
    HintLayout       *_pHintLayout;

    CBitStorageWnd    *_pStorageWnd;
    CBitScreenSaveWnd *_pScreenSaveWnd;
    CBitDialogWnd     *_pDialogWnd;
    CLanguageSelectionWnd    *_pLanguageWnd;
    CBitPowerOffWnd   *_pPowerOffWnd;

    bool  _bKeyDownLongPressed;

    int   _screenSaverCnt;

    bool  _bChangingWifiMode;
    int   _currentWifiMode;
    int   _targetWifiMode;
    int   _timeoutCnt;

    enum StillCap_State {
        StillCap_State_Idle = 0,
        StillCap_State_Waiting, //(key pressed)
        StillCap_State_Burst,
    };
    int   _stillcap_state;

    bool _bChangingWorkingMode;
    int  _countdown;

    bool _bInPhotoLapse;
};

}
#endif
