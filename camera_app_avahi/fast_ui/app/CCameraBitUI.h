#ifndef __INCameraBit_h
#define __INCameraBit_h

#include <stdint.h>

#include "qrencode.h"
#include "GobalMacro.h"
#include "agbt.h"

class CCameraProperties;

namespace ui{
/*******************************************************************************
*   CBitAudioSybl
*   
*/
class CBitAudioSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_audioOn = 0,
        symble_audioOff,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CBitAudioSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CBitAudioSybl();
    virtual bool OnEvent(CEvent *event);
protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CBitBatterySybl
*   
*/
class CBitBatterySybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_charge_full = 0,
        symble_charge_high,
        symble_charge_normal,
        symble_charge_low,
        symble_charge_warnning,
        symble_full,
        symble_high,
        symble_normal,
        symble_low,
        symble_warnning,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CBitBatterySybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CBitBatterySybl();
    virtual bool OnEvent(CEvent *event);
protected:
    CCameraProperties* _cp;
};


/*******************************************************************************
*   CBitLanguageSybl
*   
*/
class CBitLanguageSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_language = 0,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CBitLanguageSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left);
    ~CBitLanguageSybl();
    virtual bool OnEvent(CEvent *event);
protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CBitWifiSybl
*   
*/
class CBitWifiSybl : public CSymbleWithTxt
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
    CBitWifiSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CBitWifiSybl();
    virtual bool OnEvent(CEvent *event);

    void updateSymble();
protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CBitBtSybl
*   
*/
class CBitBtSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_bt_disconnect = 0,
        symble_bt_connect,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CBitBtSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CBitBtSybl();
    virtual bool OnEvent(CEvent *event);
protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CBitBtSybl
*   
*/
class CBitGPSSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_gps_offline = 0,
        symble_gps_ready,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CBitGPSSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CBitGPSSybl();
    virtual bool OnEvent(CEvent *event);
protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CBitInternetSybl
*   
*/
class CBitInternetSybl : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_internet_online = 0,
        symble_internet_offline,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CBitInternetSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CBitInternetSybl();
    virtual bool OnEvent(CEvent *event);
protected:
    CCameraProperties* _cp;
};

/*******************************************************************************
*   CBitRecSybl
*   
*/
class CBitRecSybl : public CSymbleWithTxt
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
    CBitRecSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CBitRecSybl();
    virtual bool OnEvent(CEvent *event);
    //char* getCharBuffer(){return _pTimeInfo;};
    void resetState();
protected:
    char* _pTimeInfo;
    CCameraProperties* _cp;
    //long long count;
};
/*******************************************************************************
*   CBitRecWnd
*   
*/
class CBitInforWnd;
class CBitQRCodeWnd;
class COLEDBrightnessWnd;
class CBitPowerOffPreapreWnd;

class CBitRecWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CBitRecWnd(CWndManager *pMgr, CCameraProperties* cp);
    ~CBitRecWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);
private:
    void ScreenSave(bool bSave);

private:
    CCameraProperties  *_cp;
    CPanel             *_pPannel;
    CLanguageItem      *_Infor;
    CBitAudioSybl      *_pAudioSymble;
    CBitRecSybl        *_pRecSymble;
    CPosiBarSegH       *_pStorageSpace;

    CBitBatterySybl    *_pBatterySymble;

    CBitWifiSybl       *_pWifiSymble;
    CBitBtSybl         *_pBtSymble;
    CBitInternetSybl   *_pInternetSymble;
    CBitGPSSybl        *_pGpsSymble;
    CBitLanguageSybl   *_pLanguageSymble;

    CBitInforWnd       *_pInforWnd;
    CBitQRCodeWnd      *_pQRWnd;

    COLEDBrightnessWnd *_pBrightWnd;
    bool               _bGpsFullInfor;

    bool               _bScreenSaver;
    int                _screenSaverCount;

    CBitPowerOffPreapreWnd *_pPowerOffWnd;
};

/*******************************************************************************
*   CBitInforSybl
*   
*/
class CBitInforSybl : public CSymbleWithTxt
{
public:
    typedef CSymbleWithTxt inherited;
    CBitInforSybl(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        , CCameraProperties* p);
    ~CBitInforSybl();

    //virtual bool OnEvent(CEvent *event);
    //void resetState();
protected:
    char* _textInfor;
    CCameraProperties* _cp;
    CBmp* samplebmp;
    //long long count;
};
class CBitQrCodeControl;
class CWifiControWnd;
class CLanguageControlWnd;
class CMicVolumeWnd;
/*******************************************************************************
*   CBitInforWnd
*   
*/
class CBitInforWnd : public CWnd
{
public:
    typedef enum infor
    {
        infor_wifi = 0,
        infor_audio,
        infor_qrcode,
        infor_num,
        infor_language,
        infor_bt,
        infor_internet,
        infor_battery,
        infor_gps,
    }infor;
    typedef CWnd inherited;
    CBitInforWnd(CWndManager *pMgr, CCameraProperties* cp);
    ~CBitInforWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);
    void updateCurrentInfor();
    void popConfigWnd();
    void SetSymble(CSymbleWithTxt* p, int index);
    void preSetIndex(infor i){_currentInfor = i;};

private:
    void GetWifiInfor(char *tmp);

private:
    CPanel              *_pPannel;
    CCameraProperties   *_cp;
    CBitInforSybl       *_pBitInforSymble;
    CStaticText         *_textInfor;
    int                 _currentInfor;
    CBlock              *_pRow;

    CSymbleWithTxt      **_ppLinkSymble;
    CBmp                *_pBatteryBmp;
    char                _pBatteryVolume[10];

    CBitQrCodeControl   *_pQrCode;

    CWifiControWnd      *_pWifiSetWnd;
    char                tmp1[128];
    char                tmp2[128];

    CMicVolumeWnd       *_pMicVolumeWnd;
    char                tmp3[8];

    CLanguageControlWnd *_pLanCtrlWnd;

    int _rollpos;
};


/*******************************************************************************
*   CBitQrCodeControl
*   
*/
class CBitQrCodeControl : public CControl
{
public:
    typedef CControl inherited;
    CBitQrCodeControl
        (CContainer* pParent
        ,CPoint leftTop
        ,CSize size
        ,char* string);
    ~CBitQrCodeControl();
    virtual void Draw(bool bHilight);
    void SetString(char* string);
private:
    QRcode *_qrcode;
    int    _code_width;
    COLOR  *_bmpBuf;

    CBmp   *_pBmp;
};


/*******************************************************************************
*   CBitQRCodeWnd
*   
*/
class CBitQRCodeWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CBitQRCodeWnd(CWndManager *pMgr, CCameraProperties* cp);
    ~CBitQRCodeWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);
private:
    CCameraProperties   *_cp;
    CPanel              *_pPannel;
    CBitQrCodeControl   *_qrcodeC;

};


/*******************************************************************************
*   CBitPowerOffPreapreWnd
*   
*/
class CBitPowerOffPreapreWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CBitPowerOffPreapreWnd(CWndManager *pMgr, CCameraProperties* cp);
    ~CBitPowerOffPreapreWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);
private:
    CCameraProperties   *_cp;
    CPanel              *_pPannel;

    CStaticText         *_pNotice;
    CBlock              *_pRow;
    CStaticText         *_pT;
    char                _tt[4];
    int                 _counter;
};

/*******************************************************************************
*   CWifiControWnd
*   
*/
class CWifiControWnd : public CWnd
{
public:
    typedef CWnd inherited;
    CWifiControWnd(CWndManager *pMgr, CCameraProperties* cp);
    ~CWifiControWnd();

    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);
private:
    CCameraProperties   *_cp;
    CPanel              *_pPannel;
    CSymbleWithTxt      *_pSymble;
    CBlock              *_pRow;
    CHSelector          *_pSelector;
    CStaticText         *_pAP;
    CStaticText         *_pCLT;
    CStaticText         *_pOFF;
};

/*******************************************************************************
*   CLanguageControlWnd
*   
*/
class CLanguageControlWnd : public CWnd
{
public:
	typedef CWnd inherited;
	CLanguageControlWnd(
		CWndManager *pMgr
		,CCameraProperties* cp
		//,CSymbleWithTxt* lanSymb
		);
	~CLanguageControlWnd();

	virtual void willHide();
	virtual void willShow();
	virtual bool OnEvent(CEvent *event);
private:
	CCameraProperties   *_cp;
	CPanel              *_pPannel;
	CSymbleWithTxt      *_pSymble;
	CBlock              *_pRow;
	CHSelector          *_pSelector;
	CStaticText         **_pLanguages;
    CStaticText         *_pHint;
    int                 _numLang;
    int                 _hintCounter;
    char                _pHintSelect[4][maxMenuStrinLen];
    char                _pHintSet[4][maxMenuStrinLen];
};

/*******************************************************************************
*   CBrightSymble
*   
*/
class CBrightSymble : public CSymbleWithTxt
{
public:
    enum symbles
    {
        symble_bright = 0,
        symble_num,
    };
    typedef CSymbleWithTxt inherited;
    CBrightSymble(
        CContainer* pParent
        ,CPoint leftTop
        , CSize CSize
        , bool left
        //, CCameraProperties* p
        );
    ~CBrightSymble();
    virtual bool OnEvent(CEvent *event);
protected:
    CCameraProperties* _cp;
};
/*******************************************************************************
*   COLEDBrightnessWnd
*   
*/
class COLEDBrightnessWnd  : public CWnd
{
public:
    typedef CWnd inherited;
    COLEDBrightnessWnd
        (CWndManager *pMgr
        , CCameraProperties* cp);
    ~COLEDBrightnessWnd();
    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

private:
    CCameraProperties   *_cp;
    CPanel              *_pPannel;
    CSymbleWithTxt      *_pSymbleTxt;
    CBlock				*_pRow;

    CBmp                *_pBrightBmp;
    char                _pBrightTxt[10];
    CPosiBarSegH        *_pBrightness;
};



/*******************************************************************************
*   CMicVolumeWnd
*   
*/
class CMicVolumeWnd  : public CWnd
{
public:
    typedef CWnd inherited;
    CMicVolumeWnd
        (CWndManager *pMgr
        , CCameraProperties* cp);
    ~CMicVolumeWnd();
    virtual void willHide();
    virtual void willShow();
    virtual bool OnEvent(CEvent *event);

    void updateVolumeInfor();

private:
    CCameraProperties   *_cp;
    CPanel              *_pPannel;
    CSymbleWithTxt      *_pSymbleTxt;
    CBlock              *_pRow;

    CBmp                *_pVolumeBmp;
    char                _pVolumeTxt[10];
    CPosiBarSegH        *_pVolume;
    int                 _volume;
};

}
#endif
