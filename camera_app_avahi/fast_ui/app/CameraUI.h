#ifndef __INCamera_h
#define __INCamera_h
//#ifdef WIN32

#include <stdint.h>

#include "qrencode.h"
#include "GobalMacro.h"
#include "agbt.h"

class CCameraProperties;

namespace ui{
//#endif
class CBrushBmp;


class CNoticCallBack
{
public:
    CNoticCallBack(){};
    ~CNoticCallBack(){};
    virtual void CallBack(int btnIndex) = 0;
    virtual const char* NoticeTitle() = 0;
    virtual const char* Notice() = 0;
    virtual const char* CancelLable() = 0;
    virtual const char* OkLable() = 0;
};


class CNoticeWnd : public CWnd
{
public:
    typedef enum BtnIndex
    {
        BtnIndex_OK = 0,
        BtnIndex_Cancel
    }BtnIndex;
    typedef CWnd inherited;
    static CNoticeWnd* PopNoticeWnd(CNoticCallBack* pDedegate, const char* wndName);
    CNoticeWnd(CNoticCallBack* pDedegate, const char* wndName);
    ~CNoticeWnd();
    virtual bool OnEvent(CEvent* event);
    virtual void willHide();
    virtual void willShow();
    void DisableOk(){_pOK->SetHiden();};
    void EnableOk(){_pOK->SetShow();};
    void DisableCancel(){_pCancel->SetHiden();};
    void EnableCancel(){_pCancel->SetShow();};
private:
    CPanel          *_pPanel;
    CStaticText     *_pNotice;
    CLanguageItem   *_title;
    CLanguageItem   *_ok;
    CLanguageItem   *_cancel;
    CLanguageItem   *_notice;
    CStaticText     *_pTitle;

    CBtn            *_pOK;
    CBtn            *_pCancel;
    CNoticCallBack  *_pDelegate;
};


class SetupWnd;
class QRcodeWnd;
class CRecWnd;
class CWifiInfo;

class CameraWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CameraWnd(::CCameraProperties* pp);
    ~CameraWnd();
    virtual void CloseWnd(bool bResetFocus );
    virtual void ShowWnd(bool bResetFocus );
    virtual bool OnEvent(CEvent* event);

    //void SetCamApp(CameraAppl* p){_pCameraApp = p;};
    void showDefaultWnd();

private:
    CMenu           *_pMainMenu;
    CLanguageItem   *_ppItem[8];
    CLanguageItem   *_ptitle;

    SetupWnd        *_pSetupWnd;
    QRcodeWnd       *_pQRWnd;
    CRecWnd         *_pRecWnd;

    CWifiInfo       *_pWifiInfo;

    ::CCameraProperties *_cp;
};

class CQrCodeControl : public CControl
{
    public:
    typedef CControl inherited;
    CQrCodeControl
        (CContainer* pParent
        ,CPoint leftTop
        ,CSize size
        ,char* string);
    ~CQrCodeControl();
    virtual void Draw(bool bHilight);
private:
    QRcode *_qrcode;
    int _code_width;
    COLOR* _bmpBuf;

    CBmp *_pBmp;
};

class QRcodeWnd : public CWnd
{
public:
    typedef CWnd inherited;
    QRcodeWnd(CCameraProperties* p);
    ~QRcodeWnd();
    virtual bool OnEvent(CEvent* event);
    void willHide();
    void willShow();
private:
    void GetWifiInfor(char *tmp);

    CCameraProperties* _cp;

    CPanel* _pPanel;
    //CBmpControl *_code;
    CQrCodeControl *_code;
};

class ActionSetLanguage : public CUIAction
{
public:
    ActionSetLanguage();
    ~ActionSetLanguage();
    virtual void Action(void *para);
    //char* pWishLanguage;
private:

};

class LanguageWnd : public CWnd
{
public:
    typedef CWnd inherited;
    LanguageWnd(int id);
    ~LanguageWnd();
private:
    CMenu *_pLanguageMenu;
    CLanguageItem* _ptitle;
    ActionSetLanguage* _pAction;
};
/*******************************************************************************
*   WifiModeWnd
*/
class ActionSetWifiMode : public CUIAction
{
public:
    ActionSetWifiMode():CUIAction(){};
    ~ActionSetWifiMode(){};
    virtual void Action(void *para);
    //char* pWishLanguage;
private:

};

class WifiModeWnd : public CWnd
{
public:
    typedef CWnd inherited;
    WifiModeWnd();
    ~WifiModeWnd();
    virtual void willHide();
    virtual void willShow();
private:
    CMenu               *_pWifiModeMenu;
    CLanguageItem       *_ptitle;
    ActionSetWifiMode   *_pAction;
    CLanguageItem       *_pMode[2];
    char* _tmp;
};
/*******************************************************************************
*   WifiSwitchWnd
*/
class ActionWifiSwitch : public CUIAction
{
public:
    ActionWifiSwitch():CUIAction(){};
    ~ActionWifiSwitch(){};
    virtual void Action(void *para);
    //char* pWishLanguage;
private:

};

class WifiSwitchWnd : public CWnd
{
public:
    typedef CWnd inherited;
    WifiSwitchWnd();
    ~WifiSwitchWnd();
    virtual void willHide();
    virtual void willShow();
private:
    CMenu *_pWifiModeMenu;
    CLanguageItem* _ptitle;
    ActionWifiSwitch* _pAction;
    CLanguageItem*	_pMode[2];
    char* _tmp;
};


/*******************************************************************************
*   CScreenSaverWnd
*/
class ActionSetScreenSaver : public CUIAction
{
public:
    ActionSetScreenSaver(){};
    ~ActionSetScreenSaver(){};
    virtual void Action(void *para);
private:

};
class CScreenSaverWnd  : public CWnd
{
public:
    typedef CWnd inherited;
    CScreenSaverWnd();
    ~CScreenSaverWnd();
    virtual void willHide();
    virtual void willShow();
private:
    CMenu *_pScreenSaverMenu;
    CLanguageItem* _ptitle;
    CLanguageItem* _ppItem[8];
    ActionSetScreenSaver* _pAction;
    char _title[256];
};

/*******************************************************************************
*   CGSensorTestWnd
*/

class CGSensorTestWnd  : public CWnd
{
public:
    typedef CWnd inherited;
    CGSensorTestWnd(CCameraProperties* pp);
    ~CGSensorTestWnd();

    virtual void willHide();
    virtual void willShow();
    bool OnEvent(CEvent * event);

private:
    CPanel        *_pPanel;
    CStaticText   *_pInfor;
    CStaticText   *_pTitle;
    char          _GsensorTex[512];
    CCameraProperties *_cp;
};

/*******************************************************************************
*   SetTimeStamp
*/
class ActionSetStamp : public CUIAction
{
public:
    ActionSetStamp
        (char* propName, CCameraProperties *pp
        ):_propName(propName)
        ,_cp(pp){};
    ~ActionSetStamp(){};
    virtual void Action(void *para);
private:
    char* _propName;
    CCameraProperties *_cp;
};

/*******************************************************************************
*   SetStampWnd
*/
class SetStampWnd  : public CWnd
{
public:
    typedef CWnd inherited;
    SetStampWnd(CLanguageItem* pTitle
        , char* PropertyName
        , CCameraProperties* pp);
    ~SetStampWnd();
    virtual void willHide();
    virtual void willShow();
private:
    CMenu *_pMenu;
    CLanguageItem* _ptitle;
    CLanguageItem* _ppItem[8];
    ActionSetStamp* _pAction;
    char _title[256];
    char* _propName;
    CCameraProperties *_cp;
};

/*******************************************************************************
*   StampSetupWnd
*/

class StampSetupWnd : public CWnd
{
public:
    typedef CWnd inherited;
    StampSetupWnd(CCameraProperties* p);
    ~StampSetupWnd();

private:
    CMenu *_pMenu;
    CLanguageItem* _ppItem[8];
    CLanguageItem* _ptitle;

};

/*******************************************************************************
*   AutoMarkSetupWnd
*/
class CGSensorManualSet;
class ActionAutoMarkSetup : public CUIAction
{
public:
    ActionAutoMarkSetup
        (CCameraProperties* pp
        , CGSensorTestWnd* pWnd
        , CGSensorManualSet* pWnd2)
    {
        _cp = pp;
        _pWnd = pWnd;
        _pManualSet = pWnd2;
    };
    ~ActionAutoMarkSetup(){};

    virtual void Action(void *para);
private:
    CGSensorTestWnd* _pWnd;
    CGSensorManualSet* _pManualSet;
    CCameraProperties* _cp;
};


class AutoMarkSetupWnd : public CWnd
{
public:
    typedef CWnd inherited;
    AutoMarkSetupWnd(CCameraProperties* p);
    ~AutoMarkSetupWnd();
    virtual void willHide();
    virtual void willShow();

private:
    CMenu *_pMenu;
    CLanguageItem* _ppItem[8];
    CLanguageItem* _ptitle;
    ActionAutoMarkSetup* _pAction;
    CGSensorTestWnd* _pGsensorTest;
    CGSensorManualSet* _pGSensorManualSet;
    char _title[40];
    CCameraProperties* _cp;
    char _value[40];
};

/*******************************************************************************
*   SetupWnd
*/
class CRecModeWnd;
class CInfoWnd;
class CTimeWnd;
class CFormatWnd;
class CScanWnd;
class CBlueToothWnd;
class CRecSegWnd;
class SetupWnd : public CWnd
{
public:
    typedef CWnd inherited;
    SetupWnd(CCameraProperties* p);
    ~SetupWnd();
    void ShowLanugageSetting();
    bool GetCalibState();

private:
    CMenu             *_pSetupMenu;
    CLanguageItem     *_ppItem[14];
    CLanguageItem     *_ptitle;

    LanguageWnd       *_pLanguageWnd;
    CRecModeWnd       *_pRecMode;
    CRecSegWnd        *_pSegWnd;
    CInfoWnd          *_pInforWnd;
    CTimeWnd          *_pTimeWnd;
    CFormatWnd        *_pFormatWnd;
    CScreenSaverWnd   *_pScreenSaverWnd;
    AutoMarkSetupWnd  *_pAutoMarkWnd;
#ifdef QRScanEnable
    CScanWnd          *_pScanWnd;
#endif
};

class CRecordSymble : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_stop = 0,
        symble_record,
        symble_record1,
        symble_mark,
        symble_play,
        symble_pause,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CRecordSymble(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CRecordSymble();
    virtual bool OnEvent(CEvent *event);
    char* getCharBuffer(){return _pTimeInfo;};
    void resetState();
protected:
    char* _pTimeInfo;
    CCameraProperties* _cp;
    long long count;
};

class WifiSymble : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_Ready = 0,
        symble_Prepare1,
        symble_Prepare2,
        symble_error,
        symble_num,
    };

    enum Title
    {
        Title_AP = 0,
        Title_Client,
        Title_off,
        Title_num,
    };
    typedef CSymbleWithTxt inherited;
    WifiSymble(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~WifiSymble();
    virtual bool OnEvent(CEvent *event);
    void updateWifiSymble();
private:
    CCameraProperties* _cp;
};

class AudioSymble : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_mic_on = 0,
        symble_mic_mute,
        symble_speaker,
        symble_speaker_mute,
        symble_num,
    };

    typedef CSymbleWithTxt inherited;
    AudioSymble(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~AudioSymble();
    virtual bool OnEvent(CEvent *event);
    char* getCharBuffer(){return _pVolumeInfo;};
    void setPlay(bool b){_bPlay = b;};
private:
    char*	_pVolumeInfo;
    CCameraProperties* _cp;
    bool 	_bPlay;
};

class GPSSymble : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_gps_on = 0,
        symble_gps_off,
        symble_gps_ready,
        symble_num,
    };

    typedef CSymbleWithTxt inherited;
    GPSSymble(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~GPSSymble();
    virtual bool OnEvent(CEvent *event);
private:
    CCameraProperties *_cp;
};

class TFSymble : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_tf_ready = 0,
        symble_tf_load,
        symble_tf_error,
        symble_tf_warnning,
        symble_tf_nocard,
        symble_num,
    };

    enum Title
    {
        Title_loading = 0,
        Title_ready,
        Title_nocard,
        Title_error,
        Title_full,
        Title_warnning,
        Title_num,
    };
    typedef CSymbleWithTxt inherited;
    TFSymble(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~TFSymble();
    virtual bool OnEvent(CEvent *event);
private:
    CCameraProperties  *_cp;
    int                _count;
};

class RotationSymble : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_rotate = 0,
        symble_mirror,
        symble_hdr,
        symble_num,
    };

    enum Title
    {
        Title_180 = 0,
        Title_0,
        Title_mirror,
        Title_on,
        Title_off,
        Title_num,
    };
    typedef CSymbleWithTxt inherited;
    RotationSymble(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~RotationSymble();
    virtual bool OnEvent(CEvent *event);

private:
    CCameraProperties* _cp;
};

#ifdef QRScanEnable
class CScanWnd:public CWnd, public CNoticCallBack
{
public:
    typedef CWnd inherited;
    CScanWnd(CCameraProperties* p);
    ~CScanWnd();
    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent* event);

    void CallBack(int btnIndex);
    const char* NoticeTitle();
    const char* Notice();
    const char* CancelLable();
    const char* OkLable();
private:
    CPanel              *_pPanel;
    CCameraProperties   *_cp;
    CNoticeWnd          *_pFinishNotice;

    CLanguageItem       *_title;
    CLanguageItem       *_apName;
    CLanguageItem       *_apKey;

    CBmpControl         *_cusor;
};
#endif

class CPowerOffWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CPowerOffWnd(CCameraProperties* cp);
    ~CPowerOffWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

    static void TimerCallback(void*);

private:
    CCameraProperties   *_cp;
    CPanel              *_pPannel;

    CStaticText         *_pText;
    char                txt[32];
};

class CRecWnd : public CWnd, public CNoticCallBack
{
public:
    typedef CWnd inherited;
    CRecWnd(CCameraProperties* p);
    ~CRecWnd();
    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent* event);

    CCameraProperties* getCamera(){return _cp;};

    // notice delegate
    void CallBack(int btnIndex);
    const char* NoticeTitle();
    const char* Notice();
    const char* CancelLable();
    const char* OkLable();

    void CloseLCD();
    void OpenLCD();
    void LCDBurnTest();

private:
    CRecordSymble        *_recSym;
    WifiSymble           *_wifiSym;
    AudioSymble          *_audioSym;
    TFSymble             *_tfSym;
    RotationSymble       *_rotateSym;
    GPSSymble            *_gpsSym;

    CCameraProperties    *_cp;

    CPanelWithTopBtmBar  *_pPanel;

    CNoticeWnd           *_pStopNotice;
    bool                 _bStopRec;
    bool                 _bGpsFullInfor;
    bool                 _bLCDBackLight;

    int                  _autoClose;
    int                  _screenSavercounter;

    bool                 _bWriteError;

    CPowerOffWnd   *_pPowerOffWnd;
};


/*******************************************************************************
*   CWifiInfo
*/
class CWifiInfo : public CWnd
{
public:
    typedef CWnd inherited;
    CWifiInfo();
    ~CWifiInfo();
    virtual void willHide();
    virtual void willShow();
private:
    CPanel          *_pPanel;
    CStaticText     *_pInfor;
    CStaticText     *_pTitle;
    CLanguageItem   *_title;
    CLanguageItem   *_apName;
    CLanguageItem   *_apKey;
    char            *_tmp;
};

/*******************************************************************************
*   CRecModeWnd
*/
class ActionSelectRecMode : public CUIAction
{
public:
    ActionSelectRecMode(CCameraProperties* p) : _cp(p) {}
    ~ActionSelectRecMode(){}
    virtual void Action(void *para);
private:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CRecModeWnd
*/
class CRecModeWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CRecModeWnd(CCameraProperties* p);
    ~CRecModeWnd();

    virtual void willHide();
    virtual void willShow();
private:
    CMenu                *_pModeMenu;
    CLanguageItem        *_pMode[2];
    CLanguageItem        *_pCurMdHint;
    ActionSelectRecMode  *_pAction;

    int                  _num;
    char                 *_tmp;
    CCameraProperties    *_cp;
};

/*******************************************************************************
*   CRecSegWnd
*/
class ActionSelectRecSeg : public CUIAction
{
public:
    ActionSelectRecSeg(CCameraProperties* p) : _cp(p) {}
    ~ActionSelectRecSeg(){}
    virtual void Action(void *para);
private:
    CCameraProperties* _cp;
};

class CRecSegWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CRecSegWnd(CCameraProperties* p);
    ~CRecSegWnd();

    virtual void willHide();
    virtual void willShow();
private:
    CMenu               *_pSegMenu;
    CLanguageItem       *_pSegLen[4];
    CLanguageItem       *_pCurMdHint;
    ActionSelectRecSeg  *_pAction;
    int                 _num;
    char                *_tmp;
    CCameraProperties   *_cp;
};

/*******************************************************************************
*   CInfoWnd
*/
class CInfoWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CInfoWnd();
    ~CInfoWnd();
    virtual void willHide();
    virtual void willShow();
private:
    CPanel          *_pPanel;
    CStaticText     *_pInfor;
    CStaticText     *_pTitle;
    CLanguageItem   *_title;
    CLanguageItem   *_notice;
    char            _versionTex[512];
};

class CFormatWnd;
class CFormatThread : public CThread
{
public:
    CFormatThread(CFormatWnd* formatWnd);
    ~CFormatThread();

protected:
    virtual void main(void * p);
private:
    CFormatWnd* _wnd;
};

class CFormatWnd : public CWnd
{
public:
    typedef enum BtnIndex
    {
        BtnIndex_OK = 0,
        BtnIndex_Cancel,
    }BtnIndex;
    typedef CWnd inherited;
    CFormatWnd(CCameraProperties* p);
    ~CFormatWnd();
    virtual void willHide();
    virtual void willShow();
    bool OnEvent(CEvent* event);
    void ShowFailed();
    void ShowSuccess();
private:
    friend class CFormatThread;
    CPanel          *_pPanel;
    CStaticText     *_pNotice;
    CStaticText     *_pTitle;
    CLanguageItem   *_title;
    CLanguageItem   *_notice;
    CLanguageItem   *_ok;
    CLanguageItem   *_cancel;

    CBtn            *_pOK;
    CBtn            *_pCancel;

    CCameraProperties   *_cp;
    CFormatThread       *_pFormatWork;
};


class COverSpeedWnd:public CWnd
{
public:
    typedef CWnd inherited;
    COverSpeedWnd(CCameraProperties* p);
    ~COverSpeedWnd();
    virtual void willHide();
    virtual void willShow();

    virtual bool OnEvent(CEvent* event);
private:
    CPanel  *_pPanel;
    CStaticText *_pTitle;
    CLanguageItem *_title;
    CStaticText *_pNotice;
    CLanguageItem *_Notice;
    CDigEdit *_pSpeed;
};


class CTimeWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CTimeWnd(CCameraProperties* p);
    ~CTimeWnd();
    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent* event);

private:
    int getTimeZone();
    int setTimeZone(int tz);
    bool leapYear(int y);
    void fixValue();
private:
    CPanel  *_pPanel;
    CStaticText *_pTitle;
    CLanguageItem *_title;

    CStaticText *_pDateNotice;
    CStaticText *_pTimeNotice;
    CStaticText *_pTimeZoneNotice;
    CLanguageItem *_tzN;
    CLanguageItem *_dtN;
    CLanguageItem *_tmN;
    CBmpControl *_pBmpDate;
    CBmpControl *_pBmpTime;
    CBmpControl *_pBmpZone;

    CDigEdit *_pYear;
    CDigEdit *_pMon;
    CDigEdit *_pDay;
    CDigEdit *_pTimeZone;
    CDigEdit *_pHH;
    CDigEdit *_pmm;
    CDigEdit *_psec;

    CCameraProperties *_cp;
};

class BlueToothAPI
{
public:
    BlueToothAPI();
    ~BlueToothAPI();
    bool ScanDevices();
    void getItemList(char** pList, int stringLen, int itemNum);
    int getScanedItemNum();
    bool pairItem(int index, char* pin);
    int getPairedItemNum();
    void getPairedItemList(char** pList, int stringLen, int itemNum);
    void demontItme(int index);
private:
    struct agbt_scan_list_s _scan_list;
};

class CBlueToothScanWnd;
class CBlueToothPairWnd;
class CBTScanThread : public CThread
{
public:
    CBTScanThread(CBlueToothScanWnd* formatWnd);
    ~CBTScanThread();
protected:
    virtual void main(void * p);
private:
    CBlueToothScanWnd* _wnd;
};

class ActionBTItemSelect : public CUIAction
{
public:
    ActionBTItemSelect(CBlueToothPairWnd* pWnd){_pWnd = pWnd;};
    ~ActionBTItemSelect(){};
    virtual void Action(void *para);
private:
    CBlueToothPairWnd* _pWnd;
};

class CBlueToothScanWnd : public CWnd
{
public:
    typedef enum BtnIndex
    {
        BtnIndex_OK = 0,
        BtnIndex_Cancel,
    }BtnIndex;
    typedef CWnd inherited;
    CBlueToothScanWnd(BlueToothAPI* p);
    ~CBlueToothScanWnd();

    virtual void willHide();
    virtual void willShow();

    void ShowItmeList();
    void ShowFailedInfor();
    virtual bool OnEvent(CEvent* event);

private:
    friend class CBTScanThread;
    ActionBTItemSelect  *_pAction;
    CMenu               *_pItemList;
    char                **_ppList;
    char                *_pListBuffer;
    CLanguageItem       *_pItem[2];
    int                 _num;
    char                *_tmp;
    CPanel              *_pPanel;
    CStaticText         *_pTitle;
    CStaticText         *_pNotice;
    CBtn                *_pOK;
    CBtn                *_pCancel;

    CBTScanThread       *_pBTScanThread;
    BlueToothAPI        *_pBTA;
    CBlueToothPairWnd   *_pPairWnd;
};

class CBlueToothPairWnd : public CWnd
{
public:
    typedef enum BtnIndex
    {
        BtnIndex_OK = 0,
        BtnIndex_Cancel,
    }BtnIndex;
    typedef CWnd inherited;
    CBlueToothPairWnd(BlueToothAPI* p);
    ~CBlueToothPairWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent* event);
    void SetIndex(int i){_pairIndex = i;};

private:
    char           *_tmp;
    int            _pairIndex;
    CPanel         *_pPanel;
    CStaticText    *_pTitle;
    CStaticText    *_pNotice;
    CDigEdit       *_pDig0;
    CDigEdit       *_pDig1;
    CDigEdit       *_pDig2;
    CDigEdit       *_pDig3;

    CBtn           *_pOK;
    CBtn           *_pCancel;
    BlueToothAPI   *_pBTA;
};

class CGSensorManualSet : public CWnd
{
public:
    typedef enum BtnIndex
    {
        BtnIndex_OK = 0,
        BtnIndex_Cancel,
    }BtnIndex;
    typedef CWnd inherited;
    CGSensorManualSet(CCameraProperties* p);
    ~CGSensorManualSet();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent* event);

private:
    char            *_tmp;
    int             _pairIndex;
    CPanel          *_pPanel;
    CStaticText     *_pTitle;
    CStaticText     *_pNotice;
    CStaticText     *_pNotice0;
    CStaticText     *_pNotice1;
    CStaticText     *_pNotice2;
    CDigEdit        *_pDig0;
    CDigEdit        *_pDig1;
    CDigEdit        *_pDig2;

    CBtn            *_pOK;
    CBtn            *_pCancel;
    CCameraProperties   *_cp;
    char                _title[40];
};


class CGPSManualSet : public CWnd
{
public:
    typedef enum BtnIndex
    {
        BtnIndex_OK = 0,
        BtnIndex_Cancel,
    }BtnIndex;
    typedef CWnd inherited;
    CGPSManualSet(CCameraProperties* p);
    ~CGPSManualSet();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent* event);

private:
    char            *_tmp;
    CPanel          *_pPanel;
    CStaticText     *_pTitle;
    CStaticText     *_pNotice;
    CDigEdit        *_pDig0;
    CDigEdit        *_pDig1;

    CBtn            *_pOK;
    CBtn            *_pCancel;
    CCameraProperties   *_cp;
    char                _title[40];
};

/*class ActionPairedSelect : public CUIAction
{
public:
	ActionPairedSelect(){};
	~ActionPairedSelect(){};
	virtual void Action(void *para);
private:

};*/
/*******************************************************************************
*   CBlueToothPairedListWnd
*
*/
class CBlueToothPairedListWnd : public CWnd, public CNoticCallBack
{
public:
    typedef CWnd inherited;
    CBlueToothPairedListWnd(BlueToothAPI* p);
    ~CBlueToothPairedListWnd();

    virtual void willHide();
    virtual void willShow();
    bool OnEvent(CEvent* event);

    // notice call back
    void CallBack(int btnIndex);
    const char* NoticeTitle();
    const char* Notice();
    const char* CancelLable();
    const char* OkLable();

private:
    char            *_tmp;
    CMenu           *_pItemList;
    char            **_ppList;
    char            *_pListBuffer;
    BlueToothAPI    *_pBTA;
    //ActionPairedSelect    *_pAction;
    CNoticeWnd      *_pDemountNotice;
    int             _itmeIndex;
};
/*******************************************************************************
*   CBlueToothWnd
*
*/
class CBlueToothWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CBlueToothWnd();
    ~CBlueToothWnd();

    virtual void willHide();
    virtual void willShow();
private:
    CMenu               *_pBTMenu;
    //char                **_ppList;
    CLanguageItem       *_pItem[2];
    CLanguageItem       *_ptitle;
    int                 _num;
    char                *_tmp;
    BlueToothAPI        *_pBTA;
    CBlueToothScanWnd   *_pScanWnd;
    CBlueToothPairedListWnd *_pPairedWnd;
};

/*******************************************************************************
*   CRecVolumeWnd
*
*/
class CRecVolumeWnd:public CWnd
{
public:
    typedef CWnd inherited;
    CRecVolumeWnd(CCameraProperties* p);
    ~CRecVolumeWnd();
    virtual void willHide();
    virtual void willShow();

    //virtual bool OnEvent(CEvent* event);
private:
    CPanel  *_pPanel;
    CStaticText *_pTitle;
    CLanguageItem *_title;
    CStaticText *_pNotice;
    CLanguageItem *_Notice;
    CDigEdit *_pVolume;
    CCameraProperties* _cp;
};

//#ifdef WIN32
}
//#endif

#endif
