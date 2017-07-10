
/*******************************************************************************
**      Camera.cpp
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Description:
**
**      Revision History:
**      -----------------
**      01a 27- 8-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/

/*******************************************************************************
* <Includes>
*******************************************************************************/
#ifdef WIN32
#include <windows.h>
#include "debug.h"
#include "CWinThread.h"
#include "CEvent.h"
#include "CFile.h"
#include "CBmp.h"
#include "Canvas.h"
#include "Control.h"
#include "Wnd.h"
#include "App.h"
#include "CameraAppl.h"
#include "CameraUI.h"
#else

#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
#include "CLanguage.h"
#include "Control.h"
#include "Wnd.h"
#include "App.h"
#include "Style.h"
#include "CameraAppl.h"
#include "CameraUI.h"
#include "class_camera_property.h"
#include <stdlib.h>
#include <unistd.h>

#include "agbox.h"
#include "agcmd.h"
#include <linux/input.h>
#endif

#define screen_saver_prop_name "system.ui.screensaver"
#define screen_saver_prop_default "15"


const static char* no_LANGUAGE_SETTING = "NONE";
ui::CameraWnd::CameraWnd(CCameraProperties* pp)
    : CWnd("main wnd")
    , _cp(pp)
{
    _pSetupWnd = new SetupWnd(_cp);
    _pQRWnd = new QRcodeWnd(_cp);
    _pRecWnd = new CRecWnd(_cp);
    _pWifiInfo = new CWifiInfo();

    _ptitle = LoadTxtItem("Main Menu");
    _ppItem[0] = LoadTxtItem("Record");
    _ppItem[0]->setWnd(_pRecWnd);
    //_ppItem[1] = LoadTxtItem("Playback");
    //_ppItem[1]->setWnd(_pDirWnd);
    _ppItem[1] = LoadTxtItem("QRCode");
    _ppItem[1]->setWnd(_pQRWnd);
    _ppItem[2] = LoadTxtItem("Wifi infor");
    _ppItem[2]->setWnd(_pWifiInfo);
    _ppItem[3] = LoadTxtItem("Setup");
    _ppItem[3]->setWnd(_pSetupWnd);

    _pMainMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pMainMenu->SetTitleItem(_ptitle);
    _pMainMenu->setItemList(_ppItem, 4);
    this->SetMainObject(_pMainMenu);
}

ui::CameraWnd::~CameraWnd()
{
    delete _pMainMenu;
    //delete _pSetupWnd;
    //delete _pQRWnd;
}

void ui::CameraWnd::CloseWnd(bool bResetFocus )
{
    //this->GetMainObject->Draw(true);
}

void ui::CameraWnd::ShowWnd(bool bResetFocus )
{

}

void ui::CameraWnd::showDefaultWnd()
{
    char tmp[256];
    memset(tmp, 0, 256);
    agcmd_property_get(PropName_Language, tmp, no_LANGUAGE_SETTING);
    if (strcmp(tmp, no_LANGUAGE_SETTING) == 0) {
        _pSetupWnd->SetPreWnd(this); // for first time
        _pSetupWnd->ShowLanugageSetting();
    } else {
        _pRecWnd->SetPreWnd(this); // for default wnd
        _pRecWnd->Show(false);
    }
    _cp->LCDOnOff(true);
}

//#ifdef WIN32
//};
//#endif
bool ui::CameraWnd::OnEvent(CEvent* event)
{
    bool b = inherited::OnEvent(event);
    if (b) {
        switch (event->_type)
        {
            case eventType_internal:
                switch (event->_event)
                {
                    case InternalEvent_app_state_update:
                        if ((int)event->_paload == this->GetID()) {
                            if (event->_paload1 == start_record) {
                                //_pTim->Start();
                            } else {
                                //_pTim->End();
                            }
                        }
                        break;
                    default:
                        break;
                }
                break;
            case eventType_key:
                switch (event->_event)
                {
                    case key_left:
                        b = false;
                        break;
                    default:
                        break;
                }
                break;
        }
    }
    return b;
}


/*******************************************************************************
*   ActionSetLanguage
*/
void ui::ActionSetScreenSaver::Action(void* para)
{
    if (para != NULL) {
        CMenuItem *pItem = (CMenuItem*)para;
        //
        char* saverTime =  (char*)pItem->GetText();
        //save property TBD
        agcmd_property_set(screen_saver_prop_name, saverTime);
        pItem->GetOwner()->Close();
    }
}


/*******************************************************************************
*   CScreenSaverWnd
*/
ui::CScreenSaverWnd::CScreenSaverWnd(): CWnd("screen saver wnd")
{
    _pAction = new ActionSetScreenSaver();
    _ptitle = LoadTxtItem("Screen saver");
    _ppItem[0] = LoadTxtItem("off");
    _ppItem[1] = LoadTxtItem("5 sec.");
    _ppItem[2] = LoadTxtItem("15 sec.");
    _ppItem[3] = LoadTxtItem("30 sec.");
    _ppItem[4] = LoadTxtItem("60 sec.");

    _pScreenSaverMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pScreenSaverMenu->SetTitle(_title);
    _pScreenSaverMenu->setItemList(_ppItem, 5);
    _pScreenSaverMenu->setAction(_pAction);

    this->SetMainObject(_pScreenSaverMenu);
}
ui::CScreenSaverWnd::~CScreenSaverWnd()
{
    delete _pScreenSaverMenu;
    delete _pAction;
}
void ui::CScreenSaverWnd::willHide()
{

}
void ui::CScreenSaverWnd::willShow()
{
    char tmp[128];
    agcmd_property_get(screen_saver_prop_name, tmp, screen_saver_prop_default);
    sprintf(_title,"%s:%s",_ptitle->getContent(), tmp);
}

/*******************************************************************************
*   CGSensorTestWnd
*/
ui::CGSensorTestWnd::CGSensorTestWnd(CCameraProperties* pp)
    : CWnd("GSensor Test")
    , _cp(pp)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 2);
    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _pInfor = new CStaticText(_pPanel, CPoint(0, 30), CSize(320, 210));
    _pInfor->SetStyle(BSL_NOTICE_BAKC, FSL_NOTICE_BLACK, BSL_NOTICE_BAKC, FSL_NOTICE_BLACK, 30, 30, 6, 20);
    //_title = LoadTxtItem("GSensor Test");
    _pTitle->SetText((UINT8*)"GSensor Test");
    this->SetMainObject(_pPanel);
};

ui::CGSensorTestWnd::~CGSensorTestWnd()
{
    delete _pPanel;
}

void ui::CGSensorTestWnd::willHide()
{
    // stop gsensor test thread;
    CameraAppl::getInstance()->StopUpdateTimer();
}

void ui::CGSensorTestWnd::willShow()
{
    // start gsensor test thread,
    _cp->SetGSensorOutputBuf(_GsensorTex, sizeof(_GsensorTex), 2);
    _pInfor->SetText((UINT8*)_GsensorTex);
    CameraAppl::getInstance()->StartUpdateTimer();
}

bool ui::CGSensorTestWnd::OnEvent(CEvent * event)
{
    bool b = true;
    if (b) {
        switch (event->_type)
        {
            case eventType_internal:
                if (event->_event == InternalEvent_app_timer_update) {
                    //CAMERALOG("-- gsensor wnd update : --\n%s---", _GsensorTex);
                    _pInfor->Draw(false);
                }
                break;
            default:
                break;
        }
    }
    if (b) {
        b = inherited::OnEvent(event);
    }
    return b;
}

/*******************************************************************************
*   ActionSetStamp
*/
void ui::ActionSetStamp::Action(void * para)
{
    if (para != NULL) {
        CMenuItem *pItem = (CMenuItem*)para;
        CMenu *menu = (CMenu*)pItem->GetParent();
        int index = menu->getCurIndAtFullList();
        if (index == 0) {
            agcmd_property_set(_propName, "On");
        } else {
            agcmd_property_set(_propName, "Off");
        }
        _cp->UpdateSubtitleConfig();
        pItem->GetOwner()->Close();
    }
}

ui::SetStampWnd::SetStampWnd
    (CLanguageItem* pTitle
    , char* PropertyName
    , CCameraProperties* pp
    ):CWnd(PropertyName)
    ,_ptitle(pTitle)
    ,_propName(PropertyName)
    ,_cp(pp)
{
    _pAction = new ActionSetStamp(_propName, _cp);
    //_ptitle = pTitle; //LoadTxtItem("Time stamp");
    _ppItem[0] = LoadTxtItem("On");
    _ppItem[1] = LoadTxtItem("Off");
    _pMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pMenu->SetTitle(_title);
    _pMenu->setItemList(_ppItem, 2);
    _pMenu->setAction(_pAction);
    this->SetMainObject(_pMenu);
}

ui::SetStampWnd::~SetStampWnd()
{
    delete _pMenu;
    delete _pAction;
}

void ui::SetStampWnd::willHide()
{

}

void ui::SetStampWnd::willShow()
{
    char tmp[128];
    agcmd_property_get(_propName, tmp, "On");
    if (strcmp(tmp,"On") == 0) {
        sprintf(_title,"%s:%s",_ptitle->getContent(), _ppItem[0]->getContent());
    } else {
        sprintf(_title,"%s:%s",_ptitle->getContent(), _ppItem[1]->getContent());
    }
}


/*******************************************************************************
*   StampSetupWnd
*/
ui::StampSetupWnd::StampSetupWnd(CCameraProperties* pp)
	: CWnd("Stamp Setup")
{
    _ptitle = LoadTxtItem("Video stamp");
    _ppItem[0] = LoadTxtItem("Time stamp");
    _ppItem[1] = LoadTxtItem("Speed stamp");
    _ppItem[2] = LoadTxtItem("Coord stamp");
    _ppItem[3] = LoadTxtItem("Overspeed");
    _ppItem[4] = LoadTxtItem("Speed roof");

    SetStampWnd* pStampWnd = new SetStampWnd(_ppItem[0], (char*)"system.ui.timestamp", pp);
    _ppItem[0]->setWnd(pStampWnd);

    pStampWnd = new SetStampWnd(_ppItem[1], (char*)"system.ui.speedstamp", pp);
    _ppItem[1]->setWnd(pStampWnd);

    pStampWnd = new SetStampWnd(_ppItem[2], (char*)"system.ui.coordstamp", pp);
    _ppItem[2]->setWnd(pStampWnd);

    pStampWnd = new SetStampWnd(_ppItem[3], (char*)"system.ui.overspeed", pp);
    _ppItem[3]->setWnd(pStampWnd);

    COverSpeedWnd *pWnd = new COverSpeedWnd(pp);
    _ppItem[4]->setWnd(pWnd);

    int num = 5;
#ifdef GPSManualFilterOn
    _ppItem[num ] = LoadTxtItem("GPS Filter");
    CGPSManualSet *pWnd1 = new CGPSManualSet(pp);
    _ppItem[num ]->setWnd(pWnd1);
    num++;
#endif

    _pMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pMenu ->SetTitleItem(_ptitle);
    _pMenu->setItemList(_ppItem, num);
    this->SetMainObject(_pMenu);
}

ui::StampSetupWnd::~StampSetupWnd()
{
    delete _pMenu;
}


/*******************************************************************************
*   ActionAutoMarkSetup
*/
#define gsensor_low_sens 1200
#define gsensor_mid_sens 800
#define gsensor_hi_sens 400
void ui::ActionAutoMarkSetup::Action(void *para)
{
    if (para != NULL) {
        CMenuItem *pItem = (CMenuItem*)para;
        CMenu *menu = (CMenu*)pItem->GetParent();
        int index = menu->getCurIndAtFullList();
        switch (index)
        {
            case 0:
                _cp->SetGSensorSensitivity(-1);
                _cp->SetGSensorSensitivity(-1, -1, -1);
                pItem->GetOwner()->Close();
                break;
            case 1:
                _cp->SetGSensorSensitivity(gsensor_low_sens); //low sensitive
                _cp->SetGSensorSensitivity(gsensor_low_sens, gsensor_low_sens, gsensor_low_sens);
                pItem->GetOwner()->Close();
                break;
            case 2:
                _cp->SetGSensorSensitivity(gsensor_mid_sens);
                _cp->SetGSensorSensitivity(gsensor_mid_sens, gsensor_mid_sens, gsensor_mid_sens);
                pItem->GetOwner()->Close();
                break;
            case 3:
                _cp->SetGSensorSensitivity(gsensor_hi_sens);
                _cp->SetGSensorSensitivity(gsensor_hi_sens, gsensor_hi_sens, gsensor_hi_sens);
                pItem->GetOwner()->Close();
                break;
            case 5:
                pItem->GetOwner()->PopWnd(_pWnd);
                break;
            case 4:
                pItem->GetOwner()->PopWnd(_pManualSet);
        }
    }
}

ui::AutoMarkSetupWnd::AutoMarkSetupWnd(CCameraProperties* pp)
    : CWnd("gsensor Setup")
    , _cp(pp)
{
    _ptitle = LoadTxtItem("Auto mark");
    _ppItem[0] = LoadTxtItem("Off");
    _ppItem[1] = LoadTxtItem("Low sensitive");
    _ppItem[2] = LoadTxtItem("Middle sensitive");
    _ppItem[3] = LoadTxtItem("High sensitive");
    _ppItem[4] = LoadTxtItem("Manual sensitive");
    _ppItem[5] = LoadTxtItem("GSensor test");

    _pMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);

    _pMenu->setItemList(_ppItem, 6);
    this->SetMainObject(_pMenu);

    _pGsensorTest = new CGSensorTestWnd(pp);
    _pGSensorManualSet = new CGSensorManualSet(pp);
    _pAction = new ActionAutoMarkSetup(pp, _pGsensorTest, _pGSensorManualSet);
    _pMenu->setAction(_pAction);
}

ui::AutoMarkSetupWnd::~AutoMarkSetupWnd()
{
    delete _pMenu;
    delete _pAction;
}

void ui::AutoMarkSetupWnd::willHide()
{

}

void ui::AutoMarkSetupWnd::willShow()
{
    _cp->GetGSensorSensitivity(_value, sizeof(_value));
    snprintf(_title, sizeof(_title), "%s:%s", _ptitle->getContent(), _value);
    _pMenu->SetTitle(_title);
}

/*******************************************************************************
*   SetupWnd
*/
ui::SetupWnd::SetupWnd(CCameraProperties* pp)
    : CWnd("setup wnd")
{
    _ptitle = LoadTxtItem("Setup");
    _ppItem[0] = LoadTxtItem("Language");
    _ppItem[1] = LoadTxtItem("Infor");
    _ppItem[2] = LoadTxtItem("Time&Date");
    _ppItem[3] = LoadTxtItem("Format");
    _ppItem[4] = LoadTxtItem("Screen saver");
    _ppItem[5] = LoadTxtItem("Video stamp");
    _ppItem[6] = LoadTxtItem("Mic volume");
    _ppItem[7] = LoadTxtItem("Auto mark");

    _pLanguageWnd = new LanguageWnd(0);
    _ppItem[0]->setWnd(_pLanguageWnd);

    _pInforWnd = new CInfoWnd();
    _ppItem[1]->setWnd(_pInforWnd);

    _pTimeWnd = new CTimeWnd(pp);
    _ppItem[2]->setWnd(_pTimeWnd);

    _pFormatWnd = new CFormatWnd(pp);
    _ppItem[3]->setWnd(_pFormatWnd);

    _pScreenSaverWnd = new CScreenSaverWnd();
    _ppItem[4]->setWnd(_pScreenSaverWnd);

    StampSetupWnd *_pWnd = new StampSetupWnd(pp);
    _ppItem[5]->setWnd(_pWnd);

    _ppItem[6]->setWnd(new CRecVolumeWnd(pp));

    _ppItem[7]->setWnd(new AutoMarkSetupWnd(pp));

    int num = 8;

#ifdef QRScanEnable
    _ppItem[num] = LoadTxtItem("QRScan");
    _pScanWnd = new CScanWnd(pp);
    _ppItem[num]->setWnd(_pScanWnd);
    num++;
#endif

#ifdef SupportBlueTooth
    _ppItem[num] = LoadTxtItem("BlueTooth");
    CBlueToothWnd *pBlueToothWnd = new CBlueToothWnd();
    _ppItem[num]->setWnd(pBlueToothWnd);
    num++;
#endif

    _pSetupMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, num, 5, 30);
    _pSetupMenu->SetTitleItem(_ptitle);

    _pSetupMenu->setItemList(_ppItem, num);

    this->SetMainObject(_pSetupMenu);
}

ui::SetupWnd::~SetupWnd()
{
    //delete _pLanguageWnd;
    delete _pSetupMenu;
}

bool ui::SetupWnd::GetCalibState()
{
    char tmp[256];
    agcmd_property_get("temp.calibration.status",tmp,"0");
    bool rt =(strcmp(tmp, "1")== 0);
    //CAMERALOG("---calibed : %d", rt);
    return rt;
}

void ui::SetupWnd::ShowLanugageSetting()
{
    _pLanguageWnd->SetPreWnd(this); // for fist time bringup
    _pLanguageWnd->Show(false);
}

ui::ActionSetLanguage::ActionSetLanguage()
    :CUIAction()
{
}

ui::ActionSetLanguage::~ActionSetLanguage()
{
}

void ui::ActionSetLanguage::Action(void* para)
{
    if (para != NULL) {
        CMenuItem *pItem = (CMenuItem*)para;
        //
        char* language =  (char*)pItem->GetText();
        CLanguageLoader::getInstance()->LoadLanguage(language);
        //save property TBD
        agcmd_property_set(PropName_Language, language);
        pItem->GetOwner()->Close();
    }
}
/*******************************************************************************
*   LanguageWnd
*
*/
ui::LanguageWnd::LanguageWnd(int id)
    : CWnd("lang wnd")
{
    _ptitle = LoadTxtItem("Language");
    _pAction = new ActionSetLanguage();
    _pLanguageMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pLanguageMenu->SetTitleItem(_ptitle);
    _pLanguageMenu->setAction(_pAction);
    int num;
    char** ppList;
    ppList = CLanguageLoader::getInstance()->GetLanguageList(num);
    _pLanguageMenu->setStringList(ppList, num);
    this->SetMainObject(_pLanguageMenu);
}

ui::LanguageWnd::~LanguageWnd()
{
    delete _pLanguageMenu;
    delete _pAction;
}

/*******************************************************************************
*   ActionSetWifiMode
*
*/

void ui::ActionSetWifiMode::Action(void* para)
{
    if (para != NULL) {
        CMenuItem *pItem = (CMenuItem*)para;
        CMenu *menu = (CMenu*)pItem->GetParent();
        int index = menu->getCurIndAtFullList();
        if (index == 0) {
            agcmd_property_set(WIFI_MODE_KEY, WIFI_AP_KEY);
        } else {
            agcmd_property_set(WIFI_MODE_KEY, WIFI_CLIENT_KEY);
        }
        const char *tmp_args[8];
        // close then open
        tmp_args[0] = "agsh";
        tmp_args[1] = "castle";
        tmp_args[2] = "wifi_stop";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
        int ret_val = agsys_input_read_switch(SW_RFKILL_ALL);
        CAMERALOG(">>>wifi switch state: %d", ret_val);
        if (ret_val  == 1) {
            sleep(1);
            tmp_args[2] = "wifi_start";
            tmp_args[3] = NULL;
            agbox_sh(3, (const char *const *)tmp_args);
        }
        pItem->GetOwner()->Close();
    }
}
ui::WifiModeWnd::WifiModeWnd()
    : CWnd("wifi mode wnd")
{
    _pMode[0] = LoadTxtItem("AP mode");
    _pMode[1] = LoadTxtItem("Client mode");
    _ptitle = LoadTxtItem("Wifi mode");
    _pAction = new ActionSetWifiMode();
    _pWifiModeMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    //_pWifiModeMenu->SetTitleItem(_ptitle);
    _pWifiModeMenu->setAction(_pAction);
    _pWifiModeMenu->setItemList(_pMode, 2);
    this->SetMainObject(_pWifiModeMenu);
}

ui::WifiModeWnd::~WifiModeWnd()
{
    delete _pWifiModeMenu;
    delete _pAction;
}

void ui::WifiModeWnd::willHide()
{
    delete[] _tmp;
}
void ui::WifiModeWnd::willShow()
{
    _tmp = new char[128];
    //int i = 0;
    char tmp[256];
    agcmd_property_get(WIFI_MODE_KEY, tmp, "");
    memset(_tmp, 0, 128);
    if (strcmp(tmp, WIFI_AP_KEY) == 0) {
        sprintf(_tmp, "%s:%s",_ptitle->getContent(), "AP");
    } else if(strcmp(tmp, WIFI_CLIENT_KEY) == 0) {
        sprintf(_tmp, "%s:%s",_ptitle->getContent(), "CLT");
    } else {
        sprintf(_tmp, "%s:%s",_ptitle->getContent(), "Unknow");
    }
    _pWifiModeMenu->SetTitle(_tmp);
}


/*******************************************************************************
*   WifiSwitchWnd
tmp_args[0] = "agsh";
tmp_args[1] = "castle";
tmp_args[2] = "wifi_start";
tmp_args[3] = NULL;
agbox_sh(5, (const char *const *)tmp_args);




ÕâÐ©ÃüÁîÊÇÎÞÊÓ¿ª¹Ø×´Ì¬µÄ£¬ÄãUIÐèÒª×Ô¼ºÅÐ¶Ï¿ª¹ØµÄ´æÔÚ¡£
ret_val = agsys_input_read_switch(SW_RFKILL_ALL);
0±íÊ¾¹Ø£¬1±íÊ¾¿ª¡£-1±íÊ¾¿ª¹Ø²»´æÔÚ¡£
²»´æÔÚµÄÊ±ºò£¬¶Áprop
"system.boot.wifi_sw"
»áÓÐ£º
"ON"/"OFF"Á½ÖÖ×´Ì¬¡
*/
void ui::ActionWifiSwitch::Action(void* para)
{
    const char *tmp_args[8];
    if (para != NULL) {
        CMenuItem *pItem = (CMenuItem*)para;
        CMenu *menu = (CMenu*)pItem->GetParent();
        int index = menu->getCurIndAtFullList();
        if (index == 0) {
            tmp_args[0] = "agsh";
            tmp_args[1] = "castle";
            tmp_args[2] = "wifi_start";
            tmp_args[3] = NULL;
            agbox_sh(3, (const char *const *)tmp_args);
        } else {
            tmp_args[0] = "agsh";
            tmp_args[1] = "castle";
            tmp_args[2] = "wifi_stop";
            tmp_args[3] = NULL;
            agbox_sh(3, (const char *const *)tmp_args);
        }
        pItem->GetOwner()->Close();
    }
}
ui::WifiSwitchWnd::WifiSwitchWnd()
    : CWnd("wifi switch wnd")
{
    _pMode[0] = LoadTxtItem("Switch on");
    _pMode[1] = LoadTxtItem("Switch off");
    _ptitle = LoadTxtItem("Wifi state");
    _pAction = new ActionWifiSwitch();
    _pWifiModeMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    //_pWifiModeMenu->SetTitleItem(_ptitle);
    _pWifiModeMenu->setAction(_pAction);
    _pWifiModeMenu->setItemList(_pMode, 2);
    this->SetMainObject(_pWifiModeMenu);
}

ui::WifiSwitchWnd::~WifiSwitchWnd()
{
    delete _pWifiModeMenu;
    delete _pAction;
}

void ui::WifiSwitchWnd::willHide()
{
    delete[] _tmp;
}
void ui::WifiSwitchWnd::willShow()
{
    _tmp = new char[128];
    //int i = 0;
    char tmp[256];
    agcmd_property_get("system.boot.wifi_sw", tmp, "");
    memset(_tmp, 0, 128);
    sprintf(_tmp, "%s:%s",_ptitle->getContent(), tmp);
    _pWifiModeMenu->SetTitle(_tmp);
}


/*******************************************************************************
*   CQrCodeControl
*
*/
ui::CQrCodeControl::CQrCodeControl
    (CContainer* pParent
    ,CPoint leftTop
    ,CSize size
    ,char* string
    ) : inherited(pParent, leftTop, size)
    ,_bmpBuf(0)
    ,_pBmp(0)
{
    QRecLevel level = QR_ECLEVEL_L;
    _qrcode = QRcode_encodeString8bit(string, 0, level);
    _code_width = _qrcode->width + 2;
    CAMERALOG("--qr code size: %d", _code_width);
    int m, n; // k, l;
    float f;
    if ((_code_width < size.width) && (_code_width < size.height)&&(size.height == size.width))
    {
        _bmpBuf = new COLOR [size.width * size.height];
        //CAMERALOG("------fill qr buf size : %d", size.width * size.height* sizeof(COLOR));
        memset((char*)_bmpBuf, 0xff, size.width * size.height* sizeof(COLOR));
        m = size.width / _code_width; n = size.width % _code_width;
        f = (float)n / _code_width;
        int bits;
        int rows;
        CAMERALOG("------fill qr code buffer : %d %d %f", m, n, f);
        int pp;
        for (int i = 1, k = 0; i < _code_width - 1 ; i ++) {
            if (((i-1) * f - 1) >= k) {
                rows = m + 1;
            } else {
                rows = m;
            }
            for (int j = 1, l = 0; j < _code_width - 1 ; j ++) {
                if (((j -1 ) * f - 1) > l) {
                    bits = m + 1;
                } else {
                    bits = m;
                }
                if ((_qrcode->data[(i-1)*(_code_width - 2) + (j - 1)] & 0x01)) {
                    for (int s = 0; s < rows; s++) {
                        pp = (size.height - 1 - (i* m + k + s)) * size.width + j * m + l;
                        memset((char*)&(_bmpBuf[pp]), 0 , sizeof(COLOR)*bits);
                    }
                }
                if (bits == m + 1) {
                    l ++;
                }
                //else
                //CAMERALOG("1");
            }
            //CAMERALOG("\n");
            if (rows == m + 1) {
                k++;
            }
        }
        _pBmp = new CBmp(GetSize().width, GetSize().height, sizeof(COLOR)*8, GetSize().width*sizeof(COLOR), (BYTE*)_bmpBuf);
    } else {
        CAMERALOG("--too small qrcode control size set to fill the code");
    }
}

ui::CQrCodeControl::~CQrCodeControl()
{
    QRcode_free(_qrcode);
    if (_pBmp != 0) {
        delete _pBmp;
    }
}
void ui::CQrCodeControl::Draw(bool bHilight)
{
    if (_bmpBuf != 0) {
        CPoint tplft = GetAbsPos();
        CSize size = this->GetSize();
        CCanvas* pCav = this->GetOwner()->GetManager()->GetCanvas();
        pCav->DrawRect(tplft, size, _pBmp, CCanvas::BmpFillMode_repeat, 0);
    }
}


/*******************************************************************************
*   QRcodeWnd
*
*/

ui::QRcodeWnd::QRcodeWnd
    (CCameraProperties* p)
    : CWnd("qr wnd")
    ,_cp(p)
    ,_code(NULL)
{
    _pPanel = new CPanel(NULL, this, CPoint(0,0),CSize(320, 240), 1);

    this->SetMainObject(_pPanel);
}

ui::QRcodeWnd::~QRcodeWnd()
{
    delete _pPanel;
}

bool ui::QRcodeWnd::OnEvent(CEvent * event)
{
    //CAMERALOG("--ui::QRcodeWnd::OnEvent");
    bool b = inherited::OnEvent(event);
    if (b) {
        switch (event->_type)
        {
            case eventType_key:
                switch (event->_event)
                {
                    case key_right:
                        b = false;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    return b;
}

void ui::QRcodeWnd::willHide()
{

}
void ui::QRcodeWnd::willShow()
{
    if (_code == NULL) {
        char tmp[256];
        memset(tmp, 0, sizeof(tmp));
        GetWifiInfor(tmp);
        CAMERALOG("wifi info: %s", tmp);
        _code = new CQrCodeControl(_pPanel,CPoint(40,0),CSize(240,240),tmp);
    }
}

void ui::QRcodeWnd::GetWifiInfor(char *tmp)
{
    char tmp1[64];
    char tmp2[64];
    wifi_mode mode = _cp->getWifiMode();
    switch (mode)
    {
        case wifi_mode_AP:
            {
                _cp->GetWifiAPinfor(tmp1, tmp2);
            }
            break;
        case wifi_mode_Client:
            {
                _cp->GetWifiCltinfor(tmp1, 64);
                sprintf(tmp2, "NA");
            }
            break;
        case wifi_mode_Off:
            {
                sprintf(tmp1, "NA");
                sprintf(tmp2, "OFF");
            }
            break;
    }
    sprintf(tmp, "<a>%s</a><p>%s<p>", tmp1, tmp2);
}


/*******************************************************************************
*   CRecordSymble
*
*/
ui::CRecordSymble::CRecordSymble
    (CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    , CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_stop] = new CBmp(WndResourceDir,"stop.bmp");
    symbles[symble_record] = new CBmp(WndResourceDir,"recording.bmp");
    symbles[symble_record1] = new CBmp(WndResourceDir,"recording1.bmp");
    symbles[symble_mark] = new CBmp(WndResourceDir,"mark.bmp");
    symbles[symble_play] = new CBmp(WndResourceDir,"play.bmp");
    symbles[symble_pause] = new CBmp(WndResourceDir,"pause.bmp");
    SetSymbles(symble_num, symbles);

    _pTimeInfo = new char[maxMenuStrinLen];
    this->SetText((UINT8*)_pTimeInfo);
    SetTitles(0, NULL);
    resetState();
    //count = 0;
}

ui::CRecordSymble::~CRecordSymble()
{
    delete[] _pTimeInfo;
}

void ui::CRecordSymble::resetState()
{
    memset(_pTimeInfo, 0, maxMenuStrinLen);
    sprintf(_pTimeInfo, "000:00:00");
    this->SetCurrentSymble(symble_stop);
}
bool ui::CRecordSymble::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal) && (event->_event == InternalEvent_app_timer_update))
    {
        camera_State state = _cp->getRecState();
        switch (state)
        {
            case camera_State_stop:
                this->SetCurrentSymble(symble_stop);
                break;
            case camera_State_record:
                if (getCurrentSymble() == camera_State_record) {
                    this->SetCurrentSymble(symble_record1);
                } else {
                    this->SetCurrentSymble(symble_record);
                }
                break;
            case camera_State_marking:
                this->SetCurrentSymble(symble_mark);
                break;
            case camera_State_play:
                this->SetCurrentSymble(symble_play);
                break;
            case camera_State_Pause:
                this->SetCurrentSymble(symble_pause);
                break;
            default:
                break;
        }
        UINT64 tt = _cp->getRecordingTime();
        memset(_pTimeInfo, 0, maxMenuStrinLen);
        UINT64 hh =  tt / 3600;
        UINT64 mm = (tt % 3600) / 60;
        UINT64 ss = (tt % 3600) % 60;
        int j = 0;
        j = sprintf( _pTimeInfo, "%03lld:", hh);
        j += sprintf( _pTimeInfo + j,"%02lld:", mm);
        j += sprintf( _pTimeInfo + j,"%02lld", ss);
        this->Draw(false);
    }
    return true;
}


/*******************************************************************************
*   WifiSymble
*
*/
ui::WifiSymble::WifiSymble
    (CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    ,CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_Ready] = new CBmp(WndResourceDir,"wifi_ready.bmp");
    symbles[symble_Prepare1] = new CBmp(WndResourceDir,"wifi_prepare1.bmp");
    symbles[symble_Prepare2] = new CBmp(WndResourceDir,"wifi_prepare2.bmp");
    symbles[symble_error] = new CBmp(WndResourceDir,"wifi_off.bmp");
    SetSymbles(symble_num, symbles);

    CLanguageItem** titles = new CLanguageItem*[Title_num];
    titles[Title_AP] = LoadTxtItem("AP");
    titles[Title_Client] = LoadTxtItem("Client");
    titles[Title_off] = LoadTxtItem("Off");
    SetTitles(Title_num, titles);
}
ui::WifiSymble::~WifiSymble()
{

}

void ui::WifiSymble::updateWifiSymble() {
    wifi_mode mode = _cp->getWifiMode();
    switch (mode)
    {
        case wifi_mode_AP:
            this->SetCurrentTitle(Title_AP);
            break;
        case wifi_mode_Client:
            this->SetCurrentTitle(Title_Client);
            break;
        case wifi_mode_Off:
            this->SetCurrentTitle(Title_off);
            break;
        default:
            break;
    }
    wifi_State state = _cp->getWifiState();
    switch (state)
    {
        case wifi_State_ready:
            this->SetCurrentSymble(symble_Ready);
            break;
        case wifi_State_prepare:
            this->SetCurrentSymble(symble_Prepare1);
            break;
        case wifi_State_error:
            this->SetCurrentSymble(symble_error);
            break;
        default:
            break;
    }
    CAMERALOG("--ui::WifiSymble::updateWifiSymble, mode = %d, state = %d", mode, state);
    this->Draw(false);
}

bool ui::WifiSymble::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal) &&
        (event->_event == InternalEvent_app_property_update) &&
        (event->_paload == PropertyChangeType_Wifi))
    {
        CAMERALOG("--ui::WifiSymble::OnEvent");
        wifi_mode mode = _cp->getWifiMode();
        switch (mode)
        {
            case wifi_mode_AP:
                this->SetCurrentTitle(Title_AP);
                break;
            case wifi_mode_Client:
                this->SetCurrentTitle(Title_Client);
                break;
            case wifi_mode_Off:
                this->SetCurrentTitle(Title_off);
                break;
            default:
                break;
        }
        wifi_State state = _cp->getWifiState();
        switch (state)
        {
            case wifi_State_ready:
                this->SetCurrentSymble(symble_Ready);
                break;
            case wifi_State_prepare:
                this->SetCurrentSymble(symble_Prepare1);
                break;
            case wifi_State_error:
                this->SetCurrentSymble(symble_error);
                break;
            default:
                break;
        }
        this->Draw(false);
    }
    return true;
}

/*******************************************************************************
*   AudioSymble
*
*/
ui::AudioSymble::AudioSymble
    (CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    ,CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
    ,_bPlay(false)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_mic_on] = new CBmp(WndResourceDir,"mic.bmp");
    symbles[symble_mic_mute] = new CBmp(WndResourceDir,"mic_mute.bmp");
    symbles[symble_speaker] = new CBmp(WndResourceDir,"speaker.bmp");
    symbles[symble_speaker_mute] = new CBmp(WndResourceDir,"speaker_mute.bmp");
    SetSymbles(symble_num, symbles);

    _pVolumeInfo = new char[20];
    memset(_pVolumeInfo, 0, 20);
    this->SetText((UINT8*)_pVolumeInfo);

    SetTitles(0, NULL);
}
ui::AudioSymble::~AudioSymble()
{
    delete[] _pVolumeInfo;
}
bool ui::AudioSymble::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal) &&
        (event->_event == InternalEvent_app_property_update) &&
        (event->_paload == PropertyChangeType_audio))
    {
        char tmp[256];
        int volume = 0;
        agcmd_property_get(audioGainPropertyName, tmp, "NA");
        if (strcmp(tmp, "NA")==0) {
            volume = defaultMicGain;
        } else {
            volume = atoi(tmp);
        }
        agcmd_property_get(audioStatusPropertyName, tmp, "NA");
        if ((strcmp(tmp, "mute") == 0) || (strcmp(tmp, "Mute") == 0) || (volume == 0)) {
            this->SetCurrentSymble(symble_mic_mute);
        } else {
            this->SetCurrentSymble(symble_mic_on);
        }

        this->Draw(false);
    }
    return true;
}


/*******************************************************************************
*   GPSSymble
*
*/
ui::GPSSymble::GPSSymble
    (CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    , CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_gps_on] = new CBmp(WndResourceDir,"gps.bmp");
    symbles[symble_gps_off] = new CBmp(WndResourceDir,"gps_off.bmp");
    symbles[symble_gps_ready] = new CBmp(WndResourceDir,"gps_ready.bmp");
    SetSymbles(symble_num, symbles);

    //_pVolumeInfo = new char[20];
    //memset(_pVolumeInfo, 0, 20);
    //this->SetText((UINT8*)_pVolumeInfo);
    SetTitles(0, NULL);
}
ui::GPSSymble::~GPSSymble()
{

}

bool ui::GPSSymble::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal) && (event->_event == InternalEvent_app_timer_update))
    {
        gps_state state = _cp->getGPSState();
        switch (state)
        {
            case gps_state_on:
                this->SetCurrentSymble(symble_gps_on);
                break;
            case gps_state_ready:
                this->SetCurrentSymble(symble_gps_ready);
                break;
            case gps_state_off:
                this->SetCurrentSymble(symble_gps_off);
                break;
            default:
                break;
        }
        this->Draw(false);
    }
    return true;
}

/*******************************************************************************
*   TFSymble
*
*/
ui::TFSymble::TFSymble
    (CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    ,CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_tf_ready] = new CBmp(WndResourceDir,"tf_ready.bmp");
    symbles[symble_tf_load] = new CBmp(WndResourceDir,"tf_load.bmp");
    symbles[symble_tf_error] = new CBmp(WndResourceDir,"tf_error.bmp");
    symbles[symble_tf_warnning] = new CBmp(WndResourceDir,"tf_warnning.bmp");
    symbles[symble_tf_nocard] = new CBmp(WndResourceDir,"tf_nocard.bmp");
    SetSymbles(symble_num, symbles);

    CLanguageItem** titles = new CLanguageItem*[Title_num];
    titles[Title_loading] = LoadTxtItem("Loading");
    titles[Title_ready] = LoadTxtItem("Ready");
    titles[Title_nocard] = LoadTxtItem("No card");
    titles[Title_error] = LoadTxtItem("Error");
    titles[Title_full] = LoadTxtItem("Full");
    titles[Title_warnning] = LoadTxtItem("Error");
    SetTitles(Title_num, titles);
    _count = 0;
}
ui::TFSymble::~TFSymble()
{

}

bool ui::TFSymble::OnEvent(CEvent *event)
{
    if ((event->_event == InternalEvent_app_property_update)) {
        //CAMERALOG("--ui::TFSymble::OnEvent");
        if (event->_paload == PropertyChangeType_FileWriteWarnning) {
            this->SetCurrentSymble(symble_tf_warnning);
            this->SetCurrentTitle(Title_warnning);
        } else if(event->_paload == PropertyChangeType_FileWriteError) {
            this->SetCurrentSymble(symble_tf_error);
            this->SetCurrentTitle(Title_error);
        }
        _count = 5;
        this->Draw(false);
    } else if ((event->_type == eventType_internal)) {
        if (_count > 0) {
            _count --;
        } else {
            storage_State state = _cp->getStorageState();
            switch (state)
            {
                case storage_State_error:
                    this->SetCurrentSymble(symble_tf_error);
                    this->SetCurrentTitle(Title_error);
                    break;
                case storage_State_ready:
                    this->SetCurrentSymble(symble_tf_ready);
                    this->SetCurrentTitle(Title_ready);
                    break;
                case storage_State_prepare:
                    this->SetCurrentSymble(symble_tf_load);
                    this->SetCurrentTitle(Title_loading);
                    break;
                case storage_State_noMedia:
                    this->SetCurrentSymble(symble_tf_nocard);
                    this->SetCurrentTitle(Title_nocard);
                    break;
                case storage_State_full:
                    this->SetCurrentSymble(symble_tf_error);
                    this->SetCurrentTitle(Title_full);
                    break;
                default:
                    break;
            }
            this->Draw(false);
        }
    }
    return true;
}

/*******************************************************************************
*   RotationSymble
*
*/
ui::RotationSymble::RotationSymble
    (CContainer* pParent
    ,CPoint leftTop
    , CSize CSize
    , bool left
    ,CCameraProperties* p
    ):inherited(pParent, leftTop, CSize, left)
    ,_cp(p)
{
    CBmp** symbles = new CBmp*[symble_num];
    symbles[symble_rotate] = new CBmp(WndResourceDir,"rotate.bmp");
    symbles[symble_mirror] = new CBmp(WndResourceDir,"mirror.bmp");
    symbles[symble_hdr] = new CBmp(WndResourceDir,"hdr.bmp");
    SetSymbles(symble_num, symbles);

    CLanguageItem** titles = new CLanguageItem*[Title_num];
    titles[Title_180] = LoadTxtItem("180");
    titles[Title_0] = LoadTxtItem("0");
    titles[Title_mirror] = LoadTxtItem("mirror");
    titles[Title_on] = LoadTxtItem("On");
    titles[Title_off] = LoadTxtItem("Off");
    SetTitles(Title_num, titles);
    SetCurrentSymble(1);
    SetCurrentTitle(1);
}
ui::RotationSymble::~RotationSymble()
{

}

bool ui::RotationSymble::OnEvent(CEvent *event)
{
    if ((event->_type == eventType_internal) && (event->_event == InternalEvent_app_state_update))
    {
        //CAMERALOG("--ui::RotationSymble::OnEvent");
        rotate_State state = _cp->getRotationState();
        switch (state)
        {
            case rotate_State_0:
                this->SetCurrentSymble(symble_rotate);
                this->SetCurrentTitle(Title_0);
                break;
            case rotate_State_180:
                this->SetCurrentSymble(symble_rotate);
                this->SetCurrentTitle(Title_180);
                break;
            case rotate_State_mirror_0:
                this->SetCurrentSymble(symble_mirror);
                this->SetCurrentTitle(Title_0);
                break;
            case rotate_State_mirror_180:
                this->SetCurrentSymble(symble_mirror);
                this->SetCurrentTitle(Title_180);
                break;
            case rotate_State_hdr_on:
                this->SetCurrentSymble(symble_hdr);
                this->SetCurrentTitle(Title_on);
                break;
            case rotate_State_hdr_off:
                this->SetCurrentSymble(symble_hdr);
                this->SetCurrentTitle(Title_off);
                break;
            default:
                break;
        }
        this->Draw(false);
    }
    return true;
}

/*******************************************************************************
*   CScanWnd
*
*/
#ifdef QRScanEnable
ui::CScanWnd::CScanWnd
    (CCameraProperties* p
    ):inherited("scan wnd")
    ,_cp(p)
{
    _pPanel = new CPanel(NULL, this, CPoint(0,0),CSize(320, 240), 7);
    _pPanel->SetStyle(BSL_CLEAR, BSL_CLEAR);

    _cusor = new CBmpControl(_pPanel,CPoint(110,70), CSize(100,100),WndResourceDir, "scancusor.bmp");
    this->SetMainObject(_pPanel);

    _pFinishNotice = new CNoticeWnd(this, "scan finish notice");
    _pFinishNotice->DisableOk();

    _title = LoadTxtItem("Wifi Infor");
    _apName = LoadTxtItem(" Wifi hot point:\\n");
    _apKey = LoadTxtItem("\\n Key:\\n");
}
ui::CScanWnd::~CScanWnd()
{
    delete _pPanel;
}
void ui::CScanWnd::willHide()
{
    _cp->stopScanMode();
}
void ui::CScanWnd::willShow()
{
    _cp->startScanMode();
    // start qr code analyze thread
}
bool ui::CScanWnd::OnEvent(CEvent* event)
{
    bool b = true;
    if (b) {
        switch (event->_type)
        {
            case eventType_internal:
                switch(event->_event)
                {
                    case InternalEvent_app_property_update:
                        if (event->_paload == PropertyChangeType_scanOk) {
                            // pop ok notice
                            this->PopWnd(_pFinishNotice);
                            b = false;
                        } else if (event->_paload == PropertyChangeType_scanFail) {
                            this->PopWnd(_pFinishNotice);
                            b = false;
                        }
                        break;
                }
            case eventType_key:
                switch(event->_event)
                {
                case key_ok:
                    b = false;
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
        }
    }
    if (b) {
        inherited::OnEvent(event);
    }
    return b;
}

void ui::CScanWnd::CallBack(int btnIndex)
{
    if (btnIndex == CNoticeWnd::BtnIndex_OK) {
    } else {
        CameraAppl::getInstance()->PushEvent
            (eventType_internal,
            InternalEvent_wnd_force_close,
            (UINT32)this, 0);
    }
}
const char* ui::CScanWnd::NoticeTitle()
{
    return "Scan Result";
}
const char* ui::CScanWnd::Notice()
{
    return _cp->GetScanResult();
}
const char* ui::CScanWnd::CancelLable()
{
    return "OK";
}
const char* ui::CScanWnd::OkLable()
{
    return "OK";
}

#endif


ui::CPowerOffWnd::CPowerOffWnd(CCameraProperties* cp)
    :inherited("PowerOffWnd")
    ,_cp(cp)
{
    _pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 10);
    this->SetMainObject(_pPannel);
    _pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

    _pText = new CStaticText(_pPannel, CPoint(0, 0), CSize(320, 240));
    _pText->SetStyle(BSL_Bit_Black_Pannel
        ,FSL_TEXT_NORMAL
        ,BSL_Bit_Black_Pannel
        ,FSL_TEXT_NORMAL
        ,0
        ,0
        ,0
        ,0);
    _pText->SetHAlign(CENTER);
    _pText->SetVAlign(MIDDLE);
    memset(txt, 0x00, 32);
    CLanguageItem *item = LoadTxtItem("Shutting down..");
    _pText->SetLanguageItem(item);
}


ui::CPowerOffWnd::~CPowerOffWnd()
{
    delete _pText;
    delete _pPannel;
}

void ui::CPowerOffWnd::TimerCallback(void *para)
{
    CPowerOffWnd *wnd = (CPowerOffWnd *)para;
    if (wnd) {
        CAMERALOG("-- power off\n");
        const char *tmp_args[8];
        tmp_args[0] = "agsh";
        tmp_args[1] = "poweroff";
        tmp_args[2] = NULL;
        agbox_sh(2, (const char *const *)tmp_args);
    }
}


void ui::CPowerOffWnd::willHide()
{

}

void ui::CPowerOffWnd::willShow()
{
    CTimerThread::GetInstance()->ApplyTimer(
        20/* 2s */, ui::CPowerOffWnd::TimerCallback, this, false, "CPowerOffWnd Timer");
}

bool ui::CPowerOffWnd::OnEvent(CEvent *event)
{
    return true;
}

/*******************************************************************************
*   CRecWnd
*
*/
ui::CRecWnd::CRecWnd
    (CCameraProperties* p
    ):inherited("rec wnd")
    ,_cp(p)
    ,_bStopRec(true)
    ,_bGpsFullInfor(false)
    ,_bLCDBackLight(true)
{
    _pPanel = new CPanelWithTopBtmBar(NULL, this, CPoint(0,0),CSize(320, 240), 10);
    _pPanel->SetBarStyle(30, 30, BSL_BLACK);
    _pPanel->SetStyle(BSL_CLEAR, BSL_CLEAR);

    _recSym = new CRecordSymble(_pPanel, CPoint(0, 0), CSize(150, 30), true, _cp);
    _gpsSym = new GPSSymble(_pPanel, CPoint(150, 0), CSize(30, 30), true, _cp);
    _wifiSym = new WifiSymble(_pPanel, CPoint(180, 0), CSize(140, 30), false ,_cp);
    _audioSym = new AudioSymble(_pPanel, CPoint(0, 210), CSize(100, 30), true, _cp);
    _tfSym = new TFSymble(_pPanel, CPoint(110, 210), CSize(110, 30), true, _cp);
    _rotateSym = new RotationSymble(_pPanel, CPoint(220, 210), CSize(100, 30), false, _cp);

    this->SetMainObject(_pPanel);

    _pStopNotice = new CNoticeWnd(this, "rec stop notice");

    // Check wifi mode status
    int wifi_mode = agsys_input_read_switch(SW_RFKILL_ALL);
    if (wifi_mode == 1) {
        CAMERALOG("Set AP mode");
        agcmd_property_set(WIFI_MODE_KEY, WIFI_AP_KEY);
    } else if (wifi_mode == 0) {
        CAMERALOG("Set Client mode");
        agcmd_property_set(WIFI_MODE_KEY, WIFI_CLIENT_KEY);
    }

    // Check wifi switcher status
    int wifi_status = agsys_input_read_switch(SW_ROTATE_LOCK);
    const char *tmp_args[8];
    if (wifi_status == 0) {
        CAMERALOG("Turn off wifi");
        tmp_args[0] = "agsh";
        tmp_args[1] = "castle";
        tmp_args[2] = "wifi_stop";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
    } else if (wifi_status == 1) {
        CAMERALOG("Turn on wifi");
        tmp_args[0] = "agsh";
        tmp_args[1] = "castle";
        tmp_args[2] = "wifi_start";
        tmp_args[3] = NULL;
        agbox_sh(3, (const char *const *)tmp_args);
    }

    _pPowerOffWnd = new CPowerOffWnd(p);
}

ui::CRecWnd::~CRecWnd()
{
    delete _pPanel;
}

void ui::CRecWnd::willShow()
{
    _wifiSym->updateWifiSymble();
    _recSym->resetState();
    CameraAppl::getInstance()->StartUpdateTimer();
    CameraAppl::getInstance()->PushEvent(eventType_internal, InternalEvent_app_state_update, 0, 0);
    if (_bStopRec) {
        _cp->startRecMode();
    }
    char tmp[128];
    _screenSavercounter = 0;
    agcmd_property_get(screen_saver_prop_name, tmp, screen_saver_prop_default);
    if (strcmp(tmp,"off") == 0) {
        _autoClose = -1;
    } else {
        _autoClose = atoi(tmp);
    }
    _bWriteError = false;
    //CAMERALOG("-- screen saver : %d", _autoClose);
}

void ui::CRecWnd::willHide()
{
    CameraAppl::getInstance()->StopUpdateTimer();
    if(_bStopRec) {
        _cp->endRecMode();
    }
}

void ui::CRecWnd::CloseLCD()
{
    this->GetManager()->GetCanvas()->FillAll(0x0841);
    _cp->LCDOnOff(false);
    _bLCDBackLight = false;
}

void ui::CRecWnd::OpenLCD()
{
    _screenSavercounter = 0;
    this->GetMainObject()->Draw(true);
    _cp->LCDOnOff(true);
    _bLCDBackLight = true;
}

void ui::CRecWnd::LCDBurnTest()
{
    this->GetManager()->GetCanvas()->FillAll(0x0841);
    this->GetMainObject()->Draw(true);
}

bool ui::CRecWnd::OnEvent(CEvent * event)
{
    bool b = true;
    if (b) {
        switch (event->_type)
        {
            case eventType_internal:
                switch (event->_event)
                {
                    case InternalEvent_app_timer_update:
                    {
                        if ((_autoClose > 0)&&(_bLCDBackLight )&& (event->_event == InternalEvent_app_timer_update))
                        {
                            _screenSavercounter ++;
                            if (_screenSavercounter > _autoClose) {
                                CloseLCD();
                                b = false;
                            }
                        } else if (!_bLCDBackLight) {
                            b = false;
                        }
                    }
                    break;
                    case InternalEvent_app_property_update:
                        if (event->_paload == PropertyChangeType_FileWriteWarnning) {
                            //CAMERALOG("++----++ ui process warnning");
                        } else if(event->_paload == PropertyChangeType_FileWriteError) {
                            //CAMERALOG("++----++ ui process error");
                            // pop error msg
                            _bWriteError = true;
                            _bStopRec = false;
                            _pStopNotice->DisableOk();
                            this->PopWnd(_pStopNotice);
                            b = false;
                        }
                    break;
                }
                break;
            case eventType_key:
                if (!_bLCDBackLight) {
                    //set lcd on,
                    OpenLCD();
                    b = false;
                    break;
                }
                switch (event->_event)
                {
                    case key_right:
#if 1 // GPS debug
                    {
                        // show gps full infor
                        //CAMERALOG("-- on rec wnd key right");
                        _cp->ShowGPSFullInfor(!_bGpsFullInfor);
                        _bGpsFullInfor = !_bGpsFullInfor;
                    }
                    b = false;
#else

#endif
                    break;
                    case key_hid:
                    case key_ok:
                        //b = false;
                        // record process
                        _cp->OnRecKeyProcess();
                        b = false;
                        break;
                    case key_sensorModeSwitch:
                        _cp->OnSensorSwitchKey();
                        break;
                    case key_switch_top:
                        {
                            int wifi_mode = agsys_input_read_switch(SW_RFKILL_ALL);
                            if (wifi_mode == 1) {
                                CAMERALOG("Set AP mode");
                                agcmd_property_set(WIFI_MODE_KEY, WIFI_AP_KEY);
                            } else if (wifi_mode == 0) {
                                CAMERALOG("Set Client mode");
                                agcmd_property_set(WIFI_MODE_KEY, WIFI_CLIENT_KEY);
                            }
                            char tmp[AGCMD_PROPERTY_SIZE];
                            agcmd_property_get(WIFI_SWITCH_STATE, tmp, "NA");
                            if (strcasecmp(tmp, "ON") == 0) {
                                const char *tmp_args[8];
                                tmp_args[0] = "agsh";
                                tmp_args[1] = "castle";
                                // stop
                                tmp_args[2] = "wifi_stop";
                                tmp_args[3] = NULL;
                                agbox_sh(3, (const char *const *)tmp_args);
                                sleep(1);
                                // then start
                                tmp_args[2] = "wifi_start";
                                tmp_args[3] = NULL;
                                agbox_sh(3, (const char *const *)tmp_args);
                            }
                        }
                        break;
                    case key_switch_bottom:
                        {
                            int wifi_status = agsys_input_read_switch(SW_ROTATE_LOCK);
                            const char *tmp_args[8];
                            if (wifi_status == 0) {
                                CAMERALOG("Turn off wifi");
                                tmp_args[0] = "agsh";
                                tmp_args[1] = "castle";
                                tmp_args[2] = "wifi_stop";
                                tmp_args[3] = NULL;
                                agbox_sh(3, (const char *const *)tmp_args);
                            } else if (wifi_status == 1) {
                                CAMERALOG("Turn on wifi");
                                tmp_args[0] = "agsh";
                                tmp_args[1] = "castle";
                                tmp_args[2] = "wifi_start";
                                tmp_args[3] = NULL;
                                agbox_sh(3, (const char *const *)tmp_args);
                            }
                        }
                        break;
                    case key_left:
                        {
                            int state = _cp->getRecState();
                            if ((state == camera_State_record) || (state == camera_State_marking)) {
                                _bStopRec = false;
                                _pStopNotice->EnableOk();
                                this->PopWnd(_pStopNotice);
                                b = false;
                            } else {
                                _bStopRec = true;
                            }
                        }
                        break;
                    case key_down:
                        if (_bLCDBackLight) {
                            CloseLCD();
                            b = false;
                            break;
                        }
                        break;
                    case key_poweroff:
                        {
                            this->PopWnd(_pPowerOffWnd);
                        }
                        break;
                    case key_mute:
                        {
                            char tmp[256];
                            int volume = 0;
                            agcmd_property_get(audioGainPropertyName, tmp, "NA");
                            if (strcmp(tmp, "NA")==0) {
                                volume = defaultMicGain;
                            } else {
                                volume = atoi(tmp);
                            }
                            agcmd_property_get(audioStatusPropertyName, tmp, "NA");
                            if ((strcmp(tmp, "mute") == 0) ||
                                (strcmp(tmp, "Mute") == 0) ||
                                (volume == 0))
                            {
                                if(volume > 9) {
                                    volume = 9;
                                }
                                if(volume <= 0) {
                                    volume = defaultMicGain;
                                }
                                _cp->SetMicVolume(volume);
                            } else {
                                _cp->SetMicVolume(0);
                            }
                        }
                        break;
                    default:
                        break;
                    }
                break;
            default:
                break;
        }
    }
    if (b) {
        b = inherited::OnEvent(event);
    }
    return b;
}

void ui::CRecWnd::CallBack(int btnIndex)
{
    if (btnIndex == CNoticeWnd::BtnIndex_OK) {
        //CAMERALOG("btn Ok");
        _bStopRec = true;
        CameraAppl::getInstance()->PushEvent
            (eventType_internal,
            InternalEvent_wnd_force_close,
            (UINT32)this, 0);
    } else {
        //CAMERALOG("btn Cancel");
        _bWriteError = false;
    }
}
const char* ui::CRecWnd::NoticeTitle()
{
    if (_bWriteError) {
        return "Error!!";
    } else {
        return "Stop?";
    }
}
const char* ui::CRecWnd::Notice()
{
    if (_bWriteError) {
        return "TF card write error, record stopped.";
    } else {
        return "Stop Current Recording?";
    }
}
const char* ui::CRecWnd::CancelLable()
{
    if (_bWriteError) {
        return "OK";
    } else {
        return "No";
    }
}
const char* ui::CRecWnd::OkLable()
{
    return "Yes";
}


/*******************************************************************************
*   CWifiInfo
*/
ui::CWifiInfo::CWifiInfo
    ():inherited("wifi info wnd")
    , _pPanel(0)
    , _pInfor(0)
    , _pTitle(0)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 2);
    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _pInfor = new CStaticText(_pPanel, CPoint(0, 30), CSize(320, 210));
    _pInfor->SetStyle(BSL_NOTICE_BAKC, FSL_NOTICE_BLACK, BSL_NOTICE_BAKC, FSL_NOTICE_BLACK, 30, 30, 6, 20);
    _title = LoadTxtItem("Wifi Infor");
    _apName = LoadTxtItem(" Wifi hot point:\\n");
    _apKey = LoadTxtItem("\\n Key:\\n");
    this->SetMainObject(_pPanel);
}
ui::CWifiInfo::~CWifiInfo()
{
    delete _pPanel;
}
void ui::CWifiInfo::willHide()
{
    delete[] _tmp;
}
void ui::CWifiInfo::willShow()
{
    char ssid_name_prop[AGCMD_PROPERTY_SIZE];
    char ssid_key_prop[AGCMD_PROPERTY_SIZE];
    char tmp_prop[AGCMD_PROPERTY_SIZE];

    _tmp = new char[AGCMD_PROPERTY_SIZE * 2];
    memset(_tmp, 0, AGCMD_PROPERTY_SIZE * 2);
    _pTitle->SetText((UINT8*)_title->getContent());

    agcmd_property_get("temp.earlyboot.ssid_name", ssid_name_prop, "");
    agcmd_property_get("temp.earlyboot.ssid_key", ssid_key_prop, "");
    agcmd_property_get(ssid_name_prop, tmp_prop, "NA");
    sprintf(_tmp, "%s    %s ", _apName->getContent(), tmp_prop);
    agcmd_property_get(ssid_key_prop, tmp_prop, "NA");
    sprintf(_tmp + strlen(_tmp), "%s    %s", _apKey->getContent(), tmp_prop);
    _pInfor->SetText((UINT8*)_tmp);
}

/*******************************************************************************
*   ActionSelectRecMode
*/
void ui::ActionSelectRecMode::Action(void *para)
{
    CMenuItem *pItem = (CMenuItem*)para;
    //char* name =  (char*)pItem->GetText();
    CMenu *menu = (CMenu*)pItem->GetParent();
    int index = menu->getCurIndAtFullList();
    if (index == 0) {
        _cp->setRecMode(camera_mode_car);
    } else {
        _cp->setRecMode(camera_mode_normal);
    }
    pItem->GetOwner()->Close();
}

/*******************************************************************************
*   CRecModeWnd
*/
ui::CRecModeWnd::CRecModeWnd
    (CCameraProperties* p
    ):inherited("recmode set wnd")
    ,_cp(p)
{
    //_ptitle = LoadTxtItem("Setup");
    _pMode[0] = LoadTxtItem("Car Mode");
    _pMode[1] = LoadTxtItem("Normal Mode");

    _pCurMdHint = LoadTxtItem("Current:");
    _pModeMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pModeMenu->setItemList(_pMode, 2);
    this->SetMainObject(_pModeMenu);

    _pAction = new ActionSelectRecMode(p);
    _pModeMenu->setAction(_pAction);

}

ui::CRecModeWnd::~CRecModeWnd()
{
    delete _pModeMenu;
    delete _pAction;
}

void ui::CRecModeWnd::willHide()
{
    delete[] _tmp;
}
void ui::CRecModeWnd::willShow()
{
    _tmp = new char[512];
    int i = 0;
    camera_mode md = _cp->getRecMode();
    if (md != camera_mode_car) {
        i = 1;
    }

    memset(_tmp, 0, 512);
    sprintf(_tmp, "%s %s",_pCurMdHint->getContent(), _pMode[i]->getContent());
    _pModeMenu->SetTitle(_tmp);
}

/*******************************************************************************
*   CRecSegWnd
*/
void ui::ActionSelectRecSeg::Action(void *para)
{
    CMenuItem *pItem = (CMenuItem*)para;
    //char* name =  (char*)pItem->GetText();
    CMenu *menu = (CMenu*)pItem->GetParent();
    int index = menu->getCurIndAtFullList();
    int t = 60;
    switch (index)
    {
        case 0:
            t = 60;
            break;
        case 1:
            t = 180;
            break;
        case 2:
            t = 300;
            break;
        case 3:
            t = 600;
            break;
        default:
            t = 60;
            break;
    }
    _cp->setRecSegLength(t);
    //agcmd_property_set("upnp.record.segLength", tmp);
    pItem->GetOwner()->Close();
}

ui::CRecSegWnd::CRecSegWnd
    (CCameraProperties* p
    ):inherited("recmode set wnd")
    ,_cp(p)
{
    _pSegLen[0] = LoadTxtItem("1 minute");
    _pSegLen[1] = LoadTxtItem("3 minutes");
    _pSegLen[2] = LoadTxtItem("5 minutes");
    _pSegLen[3] = LoadTxtItem("10 minutes");

    _pCurMdHint = LoadTxtItem("Movie minutes");

    _pSegMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pSegMenu->setItemList(_pSegLen, 4);
    this->SetMainObject(_pSegMenu);

    _pAction = new ActionSelectRecSeg(p);
    _pSegMenu->setAction(_pAction);
}
ui::CRecSegWnd::~CRecSegWnd()
{
    delete _pSegMenu;
    delete _pAction;
}

void ui::CRecSegWnd::willHide()
{
    delete[] _tmp;
}

void ui::CRecSegWnd::willShow()
{
    _tmp = new char[128];
    int i = 1;
    char tmp[10];
    agcmd_property_get("upnp.record.segLength", tmp, "1min");
    if (strcmp(tmp, "1min") == 0) {
        i = 1;
    } else if(strcmp(tmp, "3min") == 0) {
        i = 3;
    } else if(strcmp(tmp, "5min") == 0) {
        i = 5;
    } else if(strcmp(tmp, "10min") == 0) {
        i = 10;
    }
    memset(_tmp, 0, sizeof(tmp));
    sprintf(_tmp, "%s:%d",_pCurMdHint->getContent(), i);
    _pSegMenu->SetTitle(_tmp);
}


/*******************************************************************************
*   PopNoticeWnd
*/
ui::CNoticeWnd* ui::CNoticeWnd::PopNoticeWnd
    (CNoticCallBack* pDedegate
    , const char* wndName)
{
    CNoticeWnd *p = new CNoticeWnd(pDedegate, wndName);
    return  p;
}

ui::CNoticeWnd::CNoticeWnd
    (CNoticCallBack* pDedegate
    ,const char* wndName
    ):inherited(wndName)
    , _pDelegate(pDedegate)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 5);
    _pPanel->SetPrivateBrush(CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL));

    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _pTitle->SetDisabled();

    _pNotice = new CStaticText(_pPanel, CPoint(0, 30), CSize(320, 120));
    _pNotice->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, 30, 30, 6, 6);
    _pNotice->SetDisabled();

    _pOK = new CBtn(_pPanel, CPoint(10, 160), CSize(300, 30), BtnIndex_OK);
    _pCancel= new CBtn(_pPanel, CPoint(10, 200), CSize(300, 30), BtnIndex_Cancel);
    this->SetMainObject(_pPanel);
}

ui::CNoticeWnd::~CNoticeWnd()
{
    delete _pPanel;
}

void ui::CNoticeWnd::willHide()
{

}

void ui::CNoticeWnd::willShow()
{
    _title = LoadTxtItem(_pDelegate->NoticeTitle());
    _ok = LoadTxtItem(_pDelegate->OkLable());
    _cancel = LoadTxtItem(_pDelegate->CancelLable());
    _notice = LoadTxtItem(_pDelegate->Notice());
    _pTitle->SetText((UINT8*)_title->getContent());
    _pNotice->SetText((UINT8*)_notice->getContent());
    _pOK->SetText((UINT8*)_ok->getContent());
    _pCancel->SetText((UINT8*)_cancel->getContent());

    _pPanel->SetCurrentHilight(_pCancel);
    this->SetApp(CameraAppl::getInstance());
}

bool ui::CNoticeWnd::OnEvent(CEvent* event)
{
    bool rt = inherited::OnEvent(event);

    if (rt && (event->_type == eventType_internal)) {
        switch (event->_event) {
            case InternalEvent_wnd_button_click:
                this->Close();
                _pDelegate->CallBack(event->_paload);
                break;
            default:
                break;
        }
    }
    return rt;
}

/*******************************************************************************
*   CInfoWnd
*
*/
ui::CInfoWnd::CInfoWnd
    ():inherited("infor wnd")
    , _pPanel(0)
    , _pInfor(0)
    , _pTitle(0)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 2);
    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _pInfor = new CStaticText(_pPanel, CPoint(0, 30), CSize(320, 210));
    _pInfor->SetStyle(BSL_NOTICE_BAKC, FSL_NOTICE_BLACK, BSL_NOTICE_BAKC, FSL_NOTICE_BLACK, 30, 30, 6, 20);
    _title = LoadTxtItem("Infor");
    _notice = LoadTxtItem("Version");
    this->SetMainObject(_pPanel);

    _pTitle->SetDisabled();
    _pInfor->SetDisabled();
}
ui::CInfoWnd::~CInfoWnd()
{
    delete _pPanel;
}

void ui::CInfoWnd::willHide()
{

}

void ui::CInfoWnd::willShow()
{
    _pTitle->SetText((UINT8*)_title->getContent());
    sprintf(_versionTex,"%s: %s \\n",_notice->getContent(),APPLICATION_VERSION);
    // get version
#ifdef IPInforTest
    char tmp[256];
    memset(tmp, 0, sizeof(tmp));
    agcmd_property_get("temp.uap0.ip", tmp, "0");
    sprintf(_versionTex +strlen(_versionTex),"%s: %s \\n","uap0",tmp);
    agcmd_property_get("temp.usb0.ip", tmp, "0");
    sprintf(_versionTex +strlen(_versionTex),"%s: %s \\n","usb0",tmp);
    agcmd_property_get("temp.mlan0.ip", tmp, "0");
    sprintf(_versionTex +strlen(_versionTex),"%s: %s \\n","mlan0",tmp);
    agcmd_property_get("temp.eth0.ip", tmp, "0");
    sprintf(_versionTex +strlen(_versionTex),"%s: %s \\n","eth0",tmp);
    agcmd_property_get("temp.wlan0.ip", tmp, "0");
    sprintf(_versionTex +strlen(_versionTex),"%s: %s \\n","wlan0",tmp);
#endif

    _pInfor->SetText((UINT8*)_versionTex);
}

/*******************************************************************************
*   COverSpeedWnd
*
*/
ui::COverSpeedWnd::COverSpeedWnd
    (CCameraProperties* p
    ):inherited("over speed")
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 11);
    _pPanel->SetPrivateBrush(CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL));

    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _title = LoadTxtItem("Overspeed");

    _pNotice= new CStaticText(_pPanel, CPoint(20, 40), CSize(280, 120));
    _pNotice->SetStyle(BSL_BLACK, FSL_MENU_TITLE, BSL_BLACK, FSL_MENU_TITLE, 0, 4, 0, 0);
    _pNotice->SetHAlign(CENTER);
    _Notice = LoadTxtItem("Set following value as roof limitation of speed.");

    _pSpeed = new CDigEdit(_pPanel, CPoint(110, 180), CSize(100,32), 60, 250, 3, 120);
    this->SetMainObject(_pPanel);
    _pSpeed->setStep(5);
    _pTitle->SetDisabled();
    _pNotice->SetDisabled();
    _pPanel->SetCurrentHilight(2);
}

ui::COverSpeedWnd::~COverSpeedWnd()
{

}

void ui::COverSpeedWnd::willHide()
{
    int speed = _pSpeed->getCurrentValue();
    char tmp[256];
    sprintf(tmp, "%d", speed);
    agcmd_property_set("system.ui.speedroof", tmp);
}

void ui::COverSpeedWnd::willShow()
{
    char tmp[256];
    agcmd_property_get("system.ui.speedroof", tmp, "120");
    CAMERALOG("---get speed : %s", tmp);
    int speed = atoi(tmp);
    _pSpeed->SetCurrentValue(speed);
    _pTitle->SetText((UINT8*)_title->getContent());
    _pNotice->SetText((UINT8*)_Notice->getContent());
}

bool ui::COverSpeedWnd::OnEvent(CEvent* event)
{
    bool rt = inherited::OnEvent(event);
    return rt;
}

/*******************************************************************************
*   CTimeWnd
*
*/
extern int agclkd_set_rtc(time_t utc);
//const static char* timezongPropety = "system.time.zone";
//const static char* timezongLink = "/castle/localtime";
//const static char* timezongDir = "/usr/share/zoneinfo/TS/";
ui::CTimeWnd::CTimeWnd
    (CCameraProperties* p
    ):inherited("timer wnd")
    , _pPanel(0)
    , _pTitle(0)
    ,_cp(p)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 11);
    _pPanel->SetPrivateBrush(CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL));

    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _title = LoadTxtItem("Time&Date");

    _pBmpDate = new CBmpControl(_pPanel,CPoint(32,52), CSize(48,48),WndResourceDir,"date.bmp");
#ifdef DateFormat_Euro
    _pDay = new CDigEdit(_pPanel, CPoint(90, 60), CSize(50,32), 1, 30, 2, 6);
    _pMon = new CDigEdit(_pPanel, CPoint(150, 60), CSize(50,32), 1, 12, 2, 12);
    _pYear = new CDigEdit(_pPanel, CPoint(210, 60), CSize(80,32), 2000, 9999, 4, 2012);
#else
    _pYear = new CDigEdit(_pPanel, CPoint(90, 60), CSize(80,32), 2000, 9999, 4, 2012);
    _pMon = new CDigEdit(_pPanel, CPoint(180, 60), CSize(50,32), 1, 12, 2, 12);
    _pDay = new CDigEdit(_pPanel, CPoint(240, 60), CSize(50,32), 1, 30, 2, 6);
#endif
    _pPanel->SetCurrentHilight(2);
    _pBmpTime = new CBmpControl(_pPanel,CPoint(32,112), CSize(48,48),WndResourceDir,"time.bmp");
    _pHH = new CDigEdit(_pPanel, CPoint(90, 120), CSize(50,32), 0, 23, 2, 15);
    _pmm = new CDigEdit(_pPanel, CPoint(150, 120), CSize(50,32), 0, 59, 2, 05);
    _psec = new CDigEdit(_pPanel, CPoint(210, 120), CSize(50,32), 0, 59, 2, 00);

    _pBmpZone = new CBmpControl(_pPanel,CPoint(32,172), CSize(48,48),WndResourceDir,"timezone.bmp");
    _pTimeZone = new CDigEdit(_pPanel, CPoint(90, 180), CSize(70,32), -12, +12, 3, +8);
    _pTimeZone->setSign(true);
    this->SetMainObject(_pPanel);

    _pTitle->SetDisabled();
    _pBmpZone->SetDisabled();
    _pBmpDate->SetDisabled();
    _pBmpTime->SetDisabled();

    CDigEdit::SetIndication(CMenu::_up_symble
        , CMenu::_down_symble
        , ((CBrushFlat*)CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL))->getMainColor());
}

ui::CTimeWnd::~CTimeWnd()
{
    delete _pPanel;
}

void ui::CTimeWnd::willHide()
{
    time_t timep, timec;
    time(&timep);
    struct tm *p;
    p = localtime(&timep);
    p->tm_year = _pYear->getCurrentValue() - 1900;
    p->tm_mon = _pMon->getCurrentValue() - 1;
    p->tm_mday = _pDay->getCurrentValue();
    p->tm_hour = _pHH->getCurrentValue();
    p->tm_min = _pmm->getCurrentValue();
    p->tm_sec = _psec->getCurrentValue();

    timec = mktime(p);

    struct timeval tmv;
    struct timezone tz;

    int tt = _pTimeZone->getCurrentValue();
    tmv.tv_sec = timec;
    tmv.tv_usec = 0;
    tz.tz_dsttime = 0;
    tz.tz_minuteswest = (int)(tt*60);

    settimeofday(&tmv, &tz);
    //CAMERALOG("--settime return : %d", rt);
    agclkd_set_rtc(timec);

    setTimeZone(tt);
    if (abs(timec - timep) > 300) {
        _cp->OnSystemTimeChange();
    }
}

void ui::CTimeWnd::willShow()
{
    _pTitle->SetText((UINT8*)_title->getContent());

    time_t timep;
    time(&timep);
    struct tm *p;
    p = localtime(&timep);

    int year = p->tm_year + 1900;
    int mon = p->tm_mon + 1;
    int day = p->tm_mday;
    //CAMERALOG("%d/%d/%d, %02d:%02d:%02d", year, mon, day, p->tm_hour,p->tm_min,p->tm_sec );

    _pYear->SetCurrentValue(year);
    _pMon->SetCurrentValue(mon);
    _pDay->SetCurrentValue(day);

    _pHH->SetCurrentValue(p->tm_hour);
    _pmm->SetCurrentValue(p->tm_min);
    _psec->SetCurrentValue(p->tm_sec);

    int tt = getTimeZone();
    _pTimeZone->SetCurrentValue(tt);
}

bool ui::CTimeWnd::OnEvent(CEvent* event)
{
    if ((event->_type == eventType_key)) {
        switch (event->_event)
        {
            case key_right:
                event->_event = key_down;
                break;
            case key_left:
                if (_pPanel->GetCurrentHilight() != 2) {
                    event->_event = key_up;
                }
                break;
        }
    }
    bool rt = inherited::OnEvent(event);
    if (!rt) {
        fixValue();
    }
    return rt;
}

int ui::CTimeWnd::getTimeZone()
{
    int rt = 0;
    char tmp[256];
    memset(tmp, 0, sizeof(tmp));
	/*agcmd_property_get(timezongPropety, tmp, "CST+08");
	CAMERALOG("--time zone : %s", tmp);
	char*p = strchr(tmp, '+');
	if(p == 0)
		p = strchr(tmp, '-');
	if(p != 0)
		rt = atol(p);*/

    readlink(timezongLink, tmp, sizeof(tmp));
    if (strstr(tmp, timezongDir) > 0)
    {
        char*p = strchr(tmp, '+');
        if (p == 0){
            p = strchr(tmp, '-');
        }
        if (p != 0) {
            rt = atol(p);
        } else {
            rt = 8;
        }
    } else {
        rt = 8;
    }

    return rt;
}

int ui::CTimeWnd::setTimeZone(int tz)
{
    char tmp[256];
    memset(tmp, 0, sizeof(tmp));
    sprintf(tmp, "%sGMT%+d",timezongDir,tz);
    //agcmd_property_set(timezongPropety, tmp);
    int rt = remove(timezongLink);
    //if(rt = 0)
    rt = symlink(tmp, timezongLink);
    return rt;
}

bool ui::CTimeWnd::leapYear(int y)
{
    return (((y % 4== 0) && (y % 100)) || (y % 400 == 0));
}

void ui::CTimeWnd::fixValue()
{
    CControl* p = _pPanel->GetControl(_pPanel->GetCurrentHilight());
    if ((p == _pYear) || (p == _pMon)) {
        //bool rt = false;
        int year = _pYear->getCurrentValue();
        int days = 0;
        switch (_pMon->getCurrentValue())
        {
            case 1:
            case 3:
            case 5:
            case 7:
            case 8:
            case 10:
            case 12:
                days = 31;
                break;
            case 4:
            case 6:
            case 9:
            case 11:
                days = 30;
                break;
            case 2:
                if (leapYear(year)) {
                    days = 29;
                } else {
                    days = 28;
                }
                break;
            default:
                break;
        }
        if (_pDay->setMaxLmitation(days)) {
            _pPanel->Draw(true);
        }
    }
}


/*******************************************************************************
*   CFormatWnd
*
*/
ui::CFormatThread::CFormatThread
    (CFormatWnd* formatWnd
    ): CThread("format_working", CThread::NORMAL, 0, NULL)
    ,_wnd(formatWnd)
{

}

ui::CFormatThread::~CFormatThread()
{

}

void ui::CFormatThread::main(void * p)
{
    // check tf status, if ready , unmount,
    if (_wnd->_cp->FormatStorage()) {
        _wnd->ShowSuccess();
    } else {
        _wnd->ShowFailed();
    }
}

/*******************************************************************************
*   CFormatWnd
*
*/
ui::CFormatWnd::CFormatWnd(CCameraProperties* p)
    : inherited("format wnd")
    , _cp(p)
    , _pFormatWork(NULL)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 5);
    _pPanel->SetPrivateBrush(CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL));

    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _pTitle->SetDisabled();

    _pNotice = new CStaticText(_pPanel, CPoint(0, 30), CSize(320, 120));
    _pNotice->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, 30, 30, 6, 6);
    _pNotice->SetDisabled();

    _pOK = new CBtn(_pPanel, CPoint(10, 160), CSize(300, 30), BtnIndex_OK);
    _pCancel= new CBtn(_pPanel, CPoint(10, 200), CSize(300, 30), BtnIndex_Cancel);
    this->SetMainObject(_pPanel);
}
ui::CFormatWnd::~CFormatWnd()
{
    delete _pPanel;
}

void ui::CFormatWnd::willHide()
{
    if (_pFormatWork != NULL) {
        _pFormatWork->Stop();
        delete _pFormatWork;
    }
}

void ui::CFormatWnd::willShow()
{
    storage_State state = _cp->getStorageState();
    // if tf card ok, show
    _title = LoadTxtItem("Format TF");
    //CAMERALOG("--p: %s", (char*)_title->getContent());
    _pTitle->SetText((UINT8*)_title->getContent());
    _ok = LoadTxtItem("Confirm");
    _pOK->SetText((UINT8*)_ok->getContent());
    _cancel = LoadTxtItem("Cancel");
    _pCancel->SetText((UINT8*)_cancel->getContent());
    _pCancel->SetShow();
    if ((state == storage_State_noMedia)||(state == storage_State_prepare)) {
        _notice = LoadTxtItem("No tf card found!");
        _pOK->SetHiden();
    } else {
        _notice = LoadTxtItem("Notice that you will lost all your content in the TF card!");
        _pOK->SetShow();
    }
    _pNotice->SetText((UINT8*)_notice->getContent());
    _pPanel->SetCurrentHilight(_pCancel);
    this->SetApp(CameraAppl::getInstance());
    _pFormatWork = NULL;
}

void ui::CFormatWnd::ShowFailed()
{
    _notice = LoadTxtItem("Format task failed. Please format this with a PC.");
    _pNotice->SetText((UINT8*)_notice->getContent());
    _pPanel->SetCurrentHilight(_pCancel);
    _pCancel->SetShow();
    _pOK->SetHiden();
    this->GetMainObject()->Draw(true);
}

void ui::CFormatWnd::ShowSuccess()
{
    _notice = LoadTxtItem("Format accomplished.");
    _pNotice->SetText((UINT8*)_notice->getContent());
    _pPanel->SetCurrentHilight(_pCancel);
    _pCancel->SetShow();
    _pOK->SetHiden();
    this->GetMainObject()->Draw(true);
}

bool ui::CFormatWnd::OnEvent(CEvent* event)
{
    bool rt = inherited::OnEvent(event);

    if (rt && (event->_event == InternalEvent_app_state_update)) {
        if (event->_paload == camera_State_FormatSD_result) {
            if (event->_paload1 != 0) {
                ShowSuccess();
            } else {
                ShowFailed();
            }
            this->GetMainObject()->Draw(false);
            rt = false;
        }
    } else if (rt && (event->_type == eventType_internal)) {
        switch (event->_event)
        {
            case InternalEvent_wnd_button_click:
                if (event->_paload == BtnIndex_OK)
                {
                    _notice = LoadTxtItem("Please Wait....");
                    _pNotice->SetText((UINT8*)_notice->getContent());
                    _pPanel->SetCurrentHilight(-1);
                    _pCancel->SetHiden();
                    _pOK->SetHiden();
                    this->GetMainObject()->Draw(true);
                    _cp->FormatStorage();
                } else if (event->_paload == BtnIndex_Cancel) {
                    this->Close();
                }
                rt = false;
                break;
            default:
                break;
        }
    }
    return rt;
}

/*******************************************************************************
*   CBlueToothWnd
*
*/
ui::CBlueToothWnd::CBlueToothWnd
    (
    ):inherited("BlueToothWnd")
{
    _pItem[0] = LoadTxtItem("Scan");
    _pItem[1] = LoadTxtItem("Paired devices");
    _ptitle = LoadTxtItem("Bluetooth");
    //_pCurMdHint = LoadTxtItem("Current:");

    _pBTMenu = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pBTMenu->setItemList(_pItem, 2);
    _pBTMenu->SetTitleItem(_ptitle);
    this->SetMainObject(_pBTMenu);

    //_pAction = new ActionSelectRecMode(p);
    //_pModeMenu->setAction(_pAction);
    _pBTA = new BlueToothAPI();
    _pScanWnd = new CBlueToothScanWnd(_pBTA);
    _pItem[0]->setWnd(_pScanWnd);

    _pPairedWnd = new CBlueToothPairedListWnd(_pBTA);
    _pItem[1]->setWnd(_pPairedWnd);
}

ui::CBlueToothWnd::~CBlueToothWnd()
{
    delete _pBTMenu;
    delete _pBTA;
}

void ui::CBlueToothWnd::willHide()
{

}

void ui::CBlueToothWnd::willShow()
{

}

/*******************************************************************************
*   CBTScanThread
*
*/
ui::CBTScanThread::CBTScanThread
    (CBlueToothScanWnd* btWnd
    ):CThread("CBTScanThread", CThread::NORMAL, 0, NULL)
    ,_wnd(btWnd)
{

}
ui::CBTScanThread::~CBTScanThread()
{

}

void ui::CBTScanThread::main(void * p)
{
    CAMERALOG("start BT scan");
    if (_wnd->_pBTA->ScanDevices()) {
        _wnd->ShowItmeList();
    } else {
        _wnd->ShowFailedInfor();
    }
}

/*******************************************************************************
*   ActionSelectMovie
*
*/
void ui::ActionBTItemSelect::Action(void *para)
{
    if (para != NULL) {
        CMenuItem *pItem = (CMenuItem*)para;
        CMenu *menu = (CMenu*)pItem->GetParent();
        int index = menu->getCurIndAtFullList();
        CAMERALOG("-- action item :%d", index);
        //_pWnd->SetSelectedFile(name, index);
        //pItem->GetOwner()->PopWnd(_pWnd);
        _pWnd->SetIndex(index);
        pItem->GetOwner()->PopWnd(_pWnd);
    }
}

/*******************************************************************************
*   CBlueToothScanWnd
*
*/
ui::CBlueToothScanWnd::CBlueToothScanWnd(BlueToothAPI* p)
    :inherited("scan wnd")
    ,_ppList(NULL)
    ,_pListBuffer(NULL)
    ,_pBTScanThread(NULL)
    ,_pBTA(p)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 5);
    _pPanel->SetPrivateBrush(CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL));

    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _pTitle->SetDisabled();

    _pNotice = new CStaticText(_pPanel, CPoint(0, 30), CSize(320, 120));
    _pNotice->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, 30, 30, 6, 6);
    _pNotice->SetDisabled();

    //_pOK = new CBtn(_pPanel, CPoint(10, 160), CSize(300, 30), BtnIndex_OK);
    _pCancel= new CBtn(_pPanel, CPoint(10, 200), CSize(300, 30), BtnIndex_Cancel);
    this->SetMainObject(_pPanel);

    _pPairWnd = new CBlueToothPairWnd(_pBTA);
    _pAction = new ActionBTItemSelect(_pPairWnd);
    _pItemList = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pItemList->setAction(_pAction);
}
ui::CBlueToothScanWnd::~CBlueToothScanWnd()
{
    delete _pAction;
    delete _pPanel;
    delete _pItemList;
}

void ui::CBlueToothScanWnd::willHide()
{
    if (_pBTScanThread != NULL) {
        _pBTScanThread->Stop();
        delete _pBTScanThread;
        _pBTScanThread = NULL;
    }

    if (_ppList != NULL) {
        delete[] _ppList;
    }
    if (_pListBuffer != NULL) {
        delete[] _pListBuffer;
    }
    _ppList = NULL;
    _pListBuffer = NULL;
}
void ui::CBlueToothScanWnd::willShow()
{
    this->SetMainObject(_pPanel);
    CLanguageItem *_title = LoadTxtItem("Scan Bluetooth");
    CLanguageItem *_cancel = LoadTxtItem("Cancel");
    CLanguageItem *_notice = LoadTxtItem("Scan Bluetooth device now...");
    _pTitle->SetText((UINT8*)_title->getContent());
    _pNotice->SetText((UINT8*)_notice->getContent());
    _pCancel->SetText((UINT8*)_cancel->getContent());
    _pCancel->SetShow();
    _pPanel->SetCurrentHilight(2);
    if (_pBTScanThread == NULL) {
        _pBTScanThread = new CBTScanThread(this);
    }
    _pBTScanThread->Go();
    this->SetApp(CameraAppl::getInstance());
}

bool ui::CBlueToothScanWnd::OnEvent(CEvent* event)
{
    bool rt = inherited::OnEvent(event);
    if (rt && (event->_type == eventType_internal)) {
        switch (event->_event)
        {
            case InternalEvent_wnd_button_click:
                if (event->_paload == BtnIndex_Cancel) {
                    this->Close();
                }
                rt = false;
                break;
            default:
                break;
        }
    }
    return rt;
}

void ui::CBlueToothScanWnd::ShowItmeList()
{
    _num = _pBTA->getScanedItemNum();
    if (_num <=0) {
        //_pTxt->SetText((UINT8 *)_notice->getContent());
        //this->SetMainObject(_pPanel);
    } else {
        if (_ppList != NULL) {
            delete[] _ppList;
        }
        if (_pListBuffer != NULL) {
            delete[] _pListBuffer;
        }

        _pListBuffer = new char[maxMenuStrinLen*_num];
        _ppList = new char*[_num];
        for (int i = 0; i < _num; i++) {
            _ppList[i] = _pListBuffer + maxMenuStrinLen*i;
        }
        //_pItemList->SetTitle();
        _pBTA->getItemList(_ppList, maxMenuStrinLen, _num);
        _pItemList->setStringList(_ppList, _num);
        _pItemList->resetFocus();
        this->SetMainObject(_pItemList);
    }
    this->GetMainObject()->Draw(true);
}

void ui::CBlueToothScanWnd::ShowFailedInfor()
{
    //_num = _cp->getFileNum(_currentDir, _groupIndex);
    CLanguageItem *_notice = LoadTxtItem("No device found.");
    _pNotice->SetText((UINT8*)_notice->getContent());
    this->SetMainObject(_pPanel);
    // updata ui
    this->GetMainObject()->Draw(true);
}

/*******************************************************************************
*   BlueToothAPI
*
*/
ui::BlueToothAPI::BlueToothAPI()
{
    //_pScan_list = (struct agbt_scan_list_s*)malloc(sizeof(struct agbt_scan_list_s));
}

ui::BlueToothAPI::~BlueToothAPI()
{
    //mfree(_pScan_list);
}
bool ui::BlueToothAPI::ScanDevices()
{
    int ret_val;
    int i;

    CAMERALOG("Scaning...");
    ret_val = agbt_scan_devices(&_scan_list, 10);
    if (ret_val) {
        CAMERALOG("agbt_scan_devices() = %d", ret_val);
        return false;
    }

    CAMERALOG("Found %d devices:", _scan_list.total_num);
    for (i = 0; i < _scan_list.total_num; i++) {
        CAMERALOG("%d:\t%s\t%s", i,
            _scan_list.result[i].mac,
            _scan_list.result[i].name);
    }

    return true;
}
void ui::BlueToothAPI::getItemList
    (char** pList
    , int stringLen
    , int itemNum)
{
    int i;
    CAMERALOG("getItemList");
    for (i = 0; i < _scan_list.total_num; i++)
    {
        snprintf(pList[i], stringLen, "%s",
            _scan_list.result[i].name);
    }
    //CAMERALOG("---getItemList");
}
int ui::BlueToothAPI::getScanedItemNum()
{
    CAMERALOG("getScanedItemNum");
    return _scan_list.total_num;
}

bool ui::BlueToothAPI::pairItem
    (int index
    , char* pin)
{
    CAMERALOG("pair itme: %d,  %s", index, pin);
    agbt_pin_set(_scan_list.result[index].mac,
        _scan_list.result[index].name, pin);

    agcmd_property_set("agbt.uinput.bdaddr",
        _scan_list.result[index].mac);
    return true;
}

int ui::BlueToothAPI::getPairedItemNum()
{
    CAMERALOG("getPairedItemNum");
    return 3;
}

void ui::BlueToothAPI::getPairedItemList
    (char** pList
    , int stringLen
    , int itemNum)
{
    CAMERALOG("getPairedItemList");
    for (int i = 0; i < 3; i++)
    {
        snprintf(pList[i], stringLen, "pitem%d", i);
    }
}

void ui::BlueToothAPI::demontItme(int index)
{
    CAMERALOG("demount: %d", index);
}

/*******************************************************************************
*   CBlueToothPairWnd
*
*/
ui::CBlueToothPairWnd::CBlueToothPairWnd
    (BlueToothAPI* p)
    :inherited("pair")
    ,_pBTA(p)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 8);
    _pPanel->SetPrivateBrush(CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL));

    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _pTitle->SetDisabled();

    _pNotice = new CStaticText(_pPanel, CPoint(50, 50), CSize(140, 32));

    _pNotice->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, 30, 30, 6, 6);
    _pNotice->SetDisabled();

    _pDig0 = new CDigEdit(_pPanel, CPoint(160, 60), CSize(30,32), 0, 9, 1, 0);
    _pDig1 = new CDigEdit(_pPanel, CPoint(160+30, 60), CSize(30,32), 0, 9, 1, 0);
    _pDig2 = new CDigEdit(_pPanel, CPoint(160+60, 60), CSize(30,32), 0, 9, 1, 0);
    _pDig3 = new CDigEdit(_pPanel, CPoint(160+90, 60), CSize(30,32), 0, 9, 1, 0);

    _pOK = new CBtn(_pPanel, CPoint(10, 160), CSize(300, 30), BtnIndex_OK);
    _pCancel= new CBtn(_pPanel, CPoint(10, 200), CSize(300, 30), BtnIndex_Cancel);
    this->SetMainObject(_pPanel);
}

ui::CBlueToothPairWnd::~CBlueToothPairWnd()
{
    delete  _pPanel;
}

void ui::CBlueToothPairWnd::willHide()
{

}

void ui::CBlueToothPairWnd::willShow()
{
    this->SetMainObject(_pPanel);
    CLanguageItem *_title = LoadTxtItem("Pair Device");
    CLanguageItem *_OK = LoadTxtItem("Ok");
    CLanguageItem *_cancel = LoadTxtItem("Cancel");
    CLanguageItem *_notice = LoadTxtItem("pin:");
    _pTitle->SetText((UINT8*)_title->getContent());
    _pNotice->SetText((UINT8*)_notice->getContent());
    _pOK->SetText((UINT8*)_OK->getContent());
    _pCancel->SetText((UINT8*)_cancel->getContent());
    //_pCancel->SetShow();
    this->SetApp(CameraAppl::getInstance());
    _pPanel->SetCurrentHilight(2);
}

bool ui::CBlueToothPairWnd::OnEvent(CEvent* event)
{
    bool rt = true;
    if ((event->_type == eventType_key))
    {
        switch (event->_event)
        {
            case key_right:
                event->_event = key_down;
                break;
            case key_left:
                //if(_pPanel->GetCurrentHilight() != 2)
                event->_event = key_up;
                break;
        }
    } else if (rt && (event->_type == eventType_internal)) {
        switch (event->_event)
        {
            case InternalEvent_wnd_button_click:
                if (event->_paload == BtnIndex_Cancel)
                {
                    this->Close();
                } else if (event->_paload == BtnIndex_OK) {
                    //int pin = _pDig0->getCurrentValue() * 1000
                    //	+_pDig1->getCurrentValue() * 100
                    //	+_pDig2->getCurrentValue() * 10
                    //	+_pDig3->getCurrentValue() * 1;
                    char pin[8];
                    sprintf(pin,"%d%d%d%d"
                        ,_pDig0->getCurrentValue()
                        ,_pDig1->getCurrentValue()
                        ,_pDig2->getCurrentValue()
                        ,_pDig3->getCurrentValue()
                    );
                    if (_pBTA->pairItem(_pairIndex, pin)) {
                        this->Close();
                    } else {
                        // show input error;
                    }
                }
                rt = false;
                break;
            default:
                break;
        }
    }
    if (rt) {
        //CAMERALOG("-- on event : %d", event->_event);
        rt = inherited::OnEvent(event);
    }
    return rt;
}

/*******************************************************************************
*   CGSensorManualSet
*/
ui::CGSensorManualSet::CGSensorManualSet
    (CCameraProperties* p
    ):inherited("sesitive")
    ,_cp(p)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 10);
    _pPanel->SetPrivateBrush(CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL));

    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _pTitle->SetDisabled();

    /*_pNotice = new CStaticText(_pPanel, CPoint(20, 40), CSize(200, 32));
    _pNotice->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, 30, 30, 6, 6);
    _pNotice->SetDisabled();
    _pNotice->SetHAlign(RIGHT);
    _pNotice->SetVAlign(MIDDLE);*/

    _pNotice0 = new CStaticText(_pPanel, CPoint(20, 50), CSize(80, 32));
    _pNotice0->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, 30, 30, 6, 6);
    _pNotice0->SetDisabled();
    _pNotice0->SetHAlign(CENTER);
    _pNotice0->SetText((UINT8*)"x");
    _pNotice0->SetVAlign(MIDDLE);

    _pNotice1 = new CStaticText(_pPanel, CPoint(100, 50), CSize(120, 32));
    _pNotice1->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, 30, 30, 6, 6);
    _pNotice1->SetDisabled();
    _pNotice1->SetHAlign(CENTER);
    _pNotice1->SetText((UINT8*)"y");
    _pNotice1->SetVAlign(MIDDLE);

    _pNotice2 = new CStaticText(_pPanel, CPoint(220, 50), CSize(80, 32));
    _pNotice2->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, 30, 30, 6, 6);
    _pNotice2->SetDisabled();
    _pNotice2->SetHAlign(CENTER);
    _pNotice2->SetText((UINT8*)"z");
    _pNotice2->SetVAlign(MIDDLE);

    _pDig0 = new CDigEdit(_pPanel, CPoint(45, 100), CSize(30,32), 1, 19, 2, 1);
    _pDig1 = new CDigEdit(_pPanel, CPoint(145, 100), CSize(30,32), 1, 19, 2, 1);
    _pDig2 = new CDigEdit(_pPanel, CPoint(245, 100), CSize(30,32), 1, 19, 2, 1);

    _pOK = new CBtn(_pPanel, CPoint(10, 160), CSize(300, 30), BtnIndex_OK);
    _pCancel= new CBtn(_pPanel, CPoint(10, 200), CSize(300, 30), BtnIndex_Cancel);
    this->SetMainObject(_pPanel);

}

ui::CGSensorManualSet::~CGSensorManualSet()
{
    delete  _pPanel;
}

void ui::CGSensorManualSet::willHide()
{


}

void ui::CGSensorManualSet::willShow()
{
    this->SetMainObject(_pPanel);
    CLanguageItem *_titleTxt = LoadTxtItem("Threshold:");
    CLanguageItem *_OK = LoadTxtItem("OK");
    CLanguageItem *_cancel = LoadTxtItem("Cancel");
    //_pTitle->SetText((UINT8*)_title->getContent());
    //_pNotice->SetText((UINT8*)_notice->getContent());
    _pOK->SetText((UINT8*)_OK->getContent());
    _pCancel->SetText((UINT8*)_cancel->getContent());
    this->SetApp(CameraAppl::getInstance());
    _pPanel->SetCurrentHilight(5);
    //char tt[40];
    int x, y, z;
    _cp->GetGSensorSensitivity(x, y, z);
    _pDig0->SetCurrentValue(x / 100);
    _pDig1->SetCurrentValue(y / 100);
    _pDig2->SetCurrentValue(z / 100);

    snprintf(_title, sizeof(_title), "%s", _titleTxt->getContent());
    _pTitle->SetText((UINT8*)_title);
}

bool ui::CGSensorManualSet::OnEvent(CEvent* event)
{
    bool rt = true;
    if ((event->_type == eventType_key)) {
        switch (event->_event)
        {
            case key_right:
                event->_event = key_down;
                break;
            case key_left:
                event->_event = key_up;
                break;
        }
    } else if (rt && (event->_type == eventType_internal)) {
        switch (event->_event)
        {
            case InternalEvent_wnd_button_click:
                if (event->_paload == BtnIndex_Cancel) {
                    this->Close();
                } else if (event->_paload == BtnIndex_OK) {
                    int x = _pDig0->getCurrentValue();
                    int y = _pDig1->getCurrentValue();
                    int z = _pDig2->getCurrentValue();
                    _cp->SetGSensorSensitivity(x*100);
                    _cp->SetGSensorSensitivity(x*100, y*100, z*100);
                    this->Close();
                }
                rt = false;
                break;
            default:
                break;
        }
    }
    if (rt) {
        //CAMERALOG("-- on event : %d", event->_event);
        rt = inherited::OnEvent(event);
    }
    return rt;
}
/*******************************************************************************
*   CBlueToothPairedListWnd
*

void ui::ActionPairedSelect::Action(void *para)
{
	if(para != NULL)
	{
		CMenuItem *pItem = (CMenuItem*)para;
		char* name =  (char*)pItem->GetText();
		CMenu *menu = (CMenu*)pItem->GetParent();
		int index = menu->getCurIndAtFullList();
		//CAMERALOG("-- action item :%d", index);
		//_pWnd->SetSelectedFile(name, index);
		//pItem->GetOwner()->PopWnd(_pWnd);
		//_pWnd->SetIndex(index);
		//pItem->GetOwner()->PopWnd(_pWnd);
	}
}*/
/*******************************************************************************
*   CGPSManualSet
*/
ui::CGPSManualSet::CGPSManualSet
    (CCameraProperties* p
    ):inherited("sesitive")
    ,_cp(p)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 10);
    _pPanel->SetPrivateBrush(CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL));

    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE, BSL_MENU_TITLE, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _pTitle->SetDisabled();

    _pNotice = new CStaticText(_pPanel, CPoint(20, 40), CSize(280, 32));
    _pNotice->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL, 30, 30, 6, 6);
    _pNotice->SetDisabled();
    _pNotice->SetHAlign(LEFT);
    _pNotice->SetVAlign(MIDDLE);

    _pDig0 = new CDigEdit(_pPanel, CPoint(145, 100), CSize(30,32), 1, 99, 2, 1);

    _pOK = new CBtn(_pPanel, CPoint(10, 160), CSize(300, 30), BtnIndex_OK);
    _pCancel= new CBtn(_pPanel, CPoint(10, 200), CSize(300, 30), BtnIndex_Cancel);
    this->SetMainObject(_pPanel);
}

ui::CGPSManualSet::~CGPSManualSet()
{
    delete  _pPanel;
}

void ui::CGPSManualSet::willHide()
{

}

void ui::CGPSManualSet::willShow()
{
    this->SetMainObject(_pPanel);
    CLanguageItem *_titleTxt = LoadTxtItem("HDOP:");
    CLanguageItem *_OK = LoadTxtItem("OK");
    CLanguageItem *_cancel = LoadTxtItem("Cancel");
    CLanguageItem *_notice = LoadTxtItem("Tolerance(m):");

    _pNotice->SetText((UINT8*)_notice->getContent());
    _pOK->SetText((UINT8*)_OK->getContent());
    _pCancel->SetText((UINT8*)_cancel->getContent());
    this->SetApp(CameraAppl::getInstance());
    _pPanel->SetCurrentHilight(2);
    char tt[40];
    agcmd_property_get(GPSManualFilterProp, tt, "20");
    _pDig0->SetCurrentValue(atoi(tt) / 100);

    snprintf(_title, sizeof(_title), "%s", _titleTxt->getContent());
    _pTitle->SetText((UINT8*)_title);
}

bool ui::CGPSManualSet::OnEvent(CEvent* event)
{
    bool rt = true;
    if ((event->_type == eventType_key)) {
        switch (event->_event)
        {
            case key_right:
                event->_event = key_down;
                break;
            case key_left:
                event->_event = key_up;
                break;
        }
    } else if (rt && (event->_type == eventType_internal)) {
        switch (event->_event)
        {
            case InternalEvent_wnd_button_click:
                if (event->_paload == BtnIndex_Cancel) {
                    this->Close();
                } else if (event->_paload == BtnIndex_OK) {
                    int value = _pDig0->getCurrentValue();
                    char tt[40];
                    sprintf(tt,"%d", value*100);
                    agcmd_property_set(GPSManualFilterProp, tt);
                    this->Close();
                }
                rt = false;
                break;
            default:
                break;
        }
    }
    if (rt) {
        //CAMERALOG("-- on event : %d", event->_event);
        rt = inherited::OnEvent(event);
    }
    return rt;
}
/*******************************************************************************
*   CBlueToothPairedListWnd
*
*/
ui::CBlueToothPairedListWnd::CBlueToothPairedListWnd
    (BlueToothAPI* p
    ):inherited("paired list")
    ,_ppList(NULL)
    ,_pListBuffer(NULL)
    ,_pBTA(p)
{
    CLanguageItem *_title = LoadTxtItem("Paired Devices");
    _pItemList = new CMenu(NULL, this, CPoint(0, 0), 320, 42, 10, 5, 30);
    _pItemList->SetTitleItem(_title);
    this->SetMainObject(_pItemList);

    _pDemountNotice = new CNoticeWnd(this, "Demount selected bluetooth device.");
}

ui::CBlueToothPairedListWnd::~CBlueToothPairedListWnd()
{
    delete _pItemList;
}


void ui::CBlueToothPairedListWnd::willHide()
{
    if (_ppList != NULL) {
        delete[] _ppList;
    }
    if (_pListBuffer != NULL) {
        delete[] _pListBuffer;
    }
    _ppList = NULL;
    _pListBuffer = NULL;
}

void ui::CBlueToothPairedListWnd::willShow()
{
    int _num = _pBTA->getPairedItemNum();
    if (_ppList != NULL) {
        delete[] _ppList;
    }
    if (_pListBuffer != NULL) {
        delete[] _pListBuffer;
    }

    _pListBuffer = new char[maxMenuStrinLen*_num];
    _ppList = new char*[_num];
    for (int i = 0; i < _num; i++) {
        _ppList[i] = _pListBuffer + maxMenuStrinLen*i;
    }
    _pBTA->getPairedItemList(_ppList, maxMenuStrinLen, _num);
    _pItemList->setStringList(_ppList, _num);
    _pItemList->resetFocus();
}

bool ui::CBlueToothPairedListWnd::OnEvent(CEvent* event)
{
    bool rt = true;
    if ((event->_type == eventType_key)) {
        switch (event->_event)
        {
            case key_ok:
            case key_right:
                _itmeIndex = _pItemList->getCurIndAtFullList();
                this->PopWnd(_pDemountNotice);
                rt = false;
                break;
        }
    }
    if (rt) {
        rt = inherited::OnEvent(event);
    }
    return rt;
}

void ui::CBlueToothPairedListWnd::CallBack(int btnIndex)
{
    if (btnIndex == CNoticeWnd::BtnIndex_OK) {
        _pBTA->demontItme(_itmeIndex);
    } else {
    }
}

const char* ui::CBlueToothPairedListWnd::NoticeTitle()
{
    return "Demount device";
}

const char* ui::CBlueToothPairedListWnd::Notice()
{
    return "OK to demount device. Cancel to quit.";
}

const char* ui::CBlueToothPairedListWnd::CancelLable()
{
    return "Cancel";
}

const char* ui::CBlueToothPairedListWnd::OkLable()
{
    return "OK";
}



/*******************************************************************************
*   CRecVolumeWnd
*
*/
ui::CRecVolumeWnd::CRecVolumeWnd
    (CCameraProperties* p
    ):inherited("mic volume")
    ,_cp(p)
{
    _pPanel = new CPanel(NULL, this, CPoint(0, 0), CSize(320, 240), 11);
    _pPanel->SetPrivateBrush(CStyle::GetInstance()->GetBrush(BSL_MENU_ITEM_NORMAL));

    _pTitle = new CStaticText(_pPanel, CPoint(0, 0), CSize(320, 30));
    _pTitle->SetStyle(BSL_BLACK, FSL_MENU_TITLE, BSL_BLACK, FSL_MENU_TITLE, 0, 0, 0, 0);
    _pTitle->SetHAlign(CENTER);
    _title = LoadTxtItem("Mic volume");

    _pNotice= new CStaticText(_pPanel, CPoint(20, 40), CSize(280, 120));
    _pNotice->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_TITLE, BSL_MENU_ITEM_NORMAL, FSL_MENU_TITLE, 0, 4, 0, 0);
    _pNotice->SetHAlign(CENTER);
    _Notice = LoadTxtItem("Set following value as microphone's sensitivity.");

    _pVolume = new CDigEdit(_pPanel, CPoint(110, 180), CSize(100,32), -5, 5, 2, 36);
    this->SetMainObject(_pPanel);
    _pVolume->setStep(1);
    _pVolume->setSign(true);
    _pTitle->SetDisabled();
    _pNotice->SetDisabled();
    _pPanel->SetCurrentHilight(2);
}

ui::CRecVolumeWnd::~CRecVolumeWnd()
{

}

void ui::CRecVolumeWnd::willHide()
{
    int volume = _pVolume->getCurrentValue();
    //char tmp[256];
    //sprintf(tmp, "%d", volume);
    //agcmd_property_set("system.ui.speedroof", tmp);
    _cp->SetMicVolume(volume + 34);
}

void ui::CRecVolumeWnd::willShow()
{
    char tmp[256];
    agcmd_property_get(audioGainPropertyName, tmp, "5");
    //CAMERALOG("---get capvo : %s", tmp);
    int volume = atoi(tmp);
    _pVolume->SetCurrentValue(volume);
    _pTitle->SetText((UINT8*)_title->getContent());
    _pNotice->SetText((UINT8*)_Notice->getContent());
}
