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
#include "CCameraBitUI.h"
#include "class_camera_property.h"
#include <stdlib.h>
#include <unistd.h>
#include "Camera.h"
#include "agbox.h"


/*******************************************************************************
*   CBitRecWnd
*   
*/
ui::CBitRecWnd::CBitRecWnd
	(CWndManager *pMgr
	, CCameraProperties* cp
	):inherited("BitRecWnd", pMgr)
	,_cp(cp)
	,_bStopRec(true)
	,_bGpsFullInfor(false)
	,_bScreenSaver(false)
	,_screenSaverCount(screenSaveTime)
{
	_pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
	this->SetMainObject(_pPannel);
	_pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

	_pRecSymble = new CBitRecSybl(_pPannel, CPoint(0, 0), CSize(128, 24),true ,cp);
	_pRecSymble->SetStyle
		(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text
		,BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text);
	_pRecSymble->SetHAlign(CENTER);
	_pRecSymble->SetVAlign(TOP);
	_pRecSymble->AdjustVertical(-2);

	_pStorageSpace = new CPosiBarSegH
		(_pPannel
		,CPoint(26, 26)
		,CSize(98, 12)
		,CPoint(2, 2)
		,CSize(94, 8)
		,19
		,1
		);
	_pStorageSpace->SetStyle
		(BSL_Bit_White_border,
		BSL_Bit_White_Pannel
		);
	_pStorageSpace->SetPosi(0);
	_pStorageSpace->setLength(100, 0);

	/*_pBatteryV = new CPosiBarV1
		(_pPannel
		, CPoint(0, 25)
		, CSize(24,36)
		, CPoint(5, 6)
		, CSize(14, 26)
		);
	_pBatteryV->SetStyle
		(BSL_Bit_battery_bmp,
		BSL_Bit_White_Pannel
		);
	_pBatteryV->SetPosi(60);
	_pBatteryV->setLength(100, 40);*/

	_pBatterySymble = new CBitBatterySybl
		(_pPannel
		,CPoint(0, 25)
		,CSize(24,36)
		,true
		,_cp);
	
	_pAudioSymble = new CBitAudioSybl
		(_pPannel
		,CPoint(24, 40)
		,CSize(20, 20)
		,true
		,_cp
		);

	_pWifiSymble = new CBitWifiSybl
		(_pPannel
		,CPoint(45, 40)
		,CSize(20, 20)
		,true
		,_cp);

	_pBtSymble = new CBitBtSybl
		(_pPannel
		,CPoint(66, 40)
		,CSize(18, 20)
		,true
		,_cp);

	_pGpsSymble = new CBitGPSSybl
		(_pPannel
		,CPoint(85, 40)
		,CSize(20, 20)
		,true
		,_cp);

	_pInternetSymble = new CBitInternetSybl
		(_pPannel
		,CPoint(106, 40)
		,CSize(20, 20)
		,true
		,_cp);

	_pLanguageSymble = new CBitLanguageSybl
		(NULL
		,CPoint(106, 40)
		,CSize(20, 20)
		,true);

	_pInforWnd = new CBitInforWnd(pMgr, _cp);
	_pInforWnd->SetSymble(_pLanguageSymble, CBitInforWnd::infor_language);
	_pInforWnd->SetSymble(_pWifiSymble, CBitInforWnd::infor_wifi);
	_pInforWnd->SetSymble(_pBtSymble, CBitInforWnd::infor_bt);
	_pInforWnd->SetSymble(_pGpsSymble, CBitInforWnd::infor_gps);
	_pInforWnd->SetSymble(_pInternetSymble, CBitInforWnd::infor_internet);
	_pInforWnd->SetSymble(_pAudioSymble, CBitInforWnd::infor_audio);
	//_pInforWnd->SetSymble(_pWifiSymble, CBitInforWnd::infor_battery);
	//BSL_Bit_battery_bmp

	//_pQRWnd = new CBitQRCodeWnd(pMgr, _cp);
	_pBrightWnd = new COLEDBrightnessWnd(pMgr, _cp);


	_pPowerOffWnd = new CBitPowerOffPreapreWnd(pMgr, _cp);
	
}

ui::CBitRecWnd::~CBitRecWnd()
{
	delete _pPannel;
}

void ui::CBitRecWnd::willHide()
{
	//CameraAppl::getInstance()->StopUpdateTimer();
	if(_bStopRec)
		_cp->endRecMode();
}

void ui::CBitRecWnd::willShow()
{
	//CameraAppl::getInstance()->StartUpdateTimer();
	CameraAppl::getInstance()->StartUpdateTimer();
	CameraAppl::getInstance()->PushEvent(eventType_internal, InternalEvent_app_state_update, 0, 0);
	
	//printf("--- startRecMode\n");
	if(_bStopRec)
		_cp->startRecMode();

}	

bool ui::CBitRecWnd::OnEvent(CEvent *event)
{
	bool b = true;
	switch(event->_type)
	{
		case eventType_internal:
			if((event->_event == InternalEvent_app_timer_update))
			{
				//_cp->
				//this->Draw(false);
				if(!_bScreenSaver)
				{
					int space = 0;
					storage_State st = _cp->getStorageState();
					if(st == storage_State_ready)
					{
						space = _cp->getStorageFreSpcByPrcn();
					}
					if(space > 100)
					{
						space = 100;
					}
					if(space < 0)
					{
						space = 0;
					}
					_pStorageSpace->setLength(100, 100 - space);
					_pStorageSpace->Draw(false);

					/*bool bIncharge;
					int batteryV = _cp->GetBatteryVolume(bIncharge);
					//printf("--battery : %d, tf space: %d\n", batteryV, space);
					_pBatteryV->setLength(100, batteryV);
					_pBatteryV->SetPosi(100-batteryV);
					_pBatteryV->Draw(false);*/
					
					_screenSaverCount--;
					if(_screenSaverCount <= 0)
					{
						_bScreenSaver = true;
						ScreenSave(true);
					}
				}
				if(_bScreenSaver)
				{
					_screenSaverCount++;
					int tmp = _screenSaverCount / 5;
					if(_screenSaverCount % 5 == 0)
					{
						if(tmp % 2 == 0)
						{
							_pRecSymble->SetRltvPosi(CPoint(0,0));
						}
						else
						{
							_pRecSymble->SetRltvPosi(CPoint(0,40));
						}
						_pPannel->Draw(true);
					}
					
				}
			}
			break;
		case eventType_key:
			if(_bScreenSaver)
			{
				_bScreenSaver = false;
				_screenSaverCount = screenSaveTime;
				ScreenSave(false);
				b = false;
			}
			else
			{
				switch(event->_event)
				{
					case key_right:
						_bStopRec = false;
						_pInforWnd->preSetIndex(CBitInforWnd::infor_wifi);
						this->PopWnd(_pInforWnd);
						b = false;
						break;
					case key_ok:
						_cp->OnRecKeyProcess();
						b = false;
						break;
					case key_up:
						_bStopRec = false;
						this->PopWnd(_pBrightWnd);
						b = false;
						break;
					case key_down:
						_bStopRec = false;
#if DownKeyForGPSTest
						//this->PopWnd(_pBrightWnd);
						_cp->ShowGPSFullInfor(!_bGpsFullInfor);
						_bGpsFullInfor = !_bGpsFullInfor;
#else
						_pInforWnd->preSetIndex(CBitInforWnd::infor_qrcode);
						this->PopWnd(_pInforWnd);
#endif
						b = false;
						break;
					case key_poweroff_prepare:
						_bStopRec = false;
						this->PopWnd(_pPowerOffWnd);
						b = false;
						break;
					default:
						printf("---key :%d\n", event->_event);
						break;
				}
			}
			break;
		case eventType_Gesnsor:
			_bStopRec = false;
			this->PopWnd(_pInforWnd);
			break;
	}
	if(b)
		b = inherited::OnEvent(event);
	return b;
}

void ui::CBitRecWnd::ScreenSave(bool bSave)
{
	if(bSave)
	{
		printf("--hide\n");
		_pAudioSymble->SetHiden();
		_pStorageSpace->SetHiden();
		_pBatterySymble->SetHiden();
		_pWifiSymble->SetHiden();
		_pBtSymble->SetHiden();
		_pInternetSymble->SetHiden();
		_pGpsSymble->SetHiden();
		_pLanguageSymble->SetHiden();
		_pPannel->Draw(true);
	}
	else
	{
		printf("--show\n");
		_pAudioSymble->SetShow();
		_pStorageSpace->SetShow();
		_pBatterySymble->SetShow();
		_pWifiSymble->SetShow();
		_pBtSymble->SetShow();
		_pInternetSymble->SetShow();
		_pGpsSymble->SetShow();
		_pLanguageSymble->SetShow();
		_pRecSymble->SetRltvPosi(CPoint(0,0));
		_pPannel->Draw(true);
	}
}

/*******************************************************************************
*   CBitInforSybl
*/ 
ui::CBitInforSybl::CBitInforSybl(
		CContainer* pParent
		,CPoint leftTop
		, CSize CSize
		, bool left
		, CCameraProperties* p
		):inherited(pParent, leftTop, CSize, left)
		,_cp(p)	
{
	CBmp** symbles = new CBmp*;
	samplebmp = new CBmp(WndResourceDir,"icon_ap.bmp"); // as sample size;
	symbles[0] = samplebmp;
	SetSymbles(1, symbles);

	this->SetText((UINT8*)NULL);
	SetTitles(0, NULL);		
}
ui::CBitInforSybl::~CBitInforSybl()
{
	delete samplebmp;
	SetCurrentSymbleBmp(NULL);
}

/*******************************************************************************
*   CBitInforWnd
*/  

ui::CBitInforWnd::CBitInforWnd
	(CWndManager *pMgr
	, CCameraProperties* cp
	):inherited("BitInforWnd", pMgr)
	,_cp(cp)
	,_currentInfor(infor_wifi)
	,_pQrCode(NULL)
	,_pLanCtrlWnd(NULL)
{
	_pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
	this->SetMainObject(_pPannel);
	_pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

	_pBitInforSymble = new CBitInforSybl(_pPannel, CPoint(0, 0), CSize(128, 24),true ,cp);
	_pBitInforSymble->SetStyle
		(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small);
	_pBitInforSymble->SetHAlign(CENTER);
	_pBitInforSymble->SetVAlign(TOP);
	_pBitInforSymble->AdjustVertical(0);

	_pRow = new CBlock(_pPannel, CPoint(2, 24), CSize(124, 2), BSL_Bit_White_Pannel);
	
	_textInfor = new CStaticText(_pPannel, CPoint(0, 26), CSize(128, 38));
	_textInfor->SetStyle(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text
		,BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text);
	_textInfor->SetVAlign(MIDDLE);
	_textInfor->SetHAlign(CENTER);

	_ppLinkSymble = new CSymbleWithTxt*[infor_num];
	for(int i = 0; i < infor_num; i++)
	{
		_ppLinkSymble[i] = NULL;
	}

	_pBatteryBmp = new CBmp(WndResourceDir,"icon_battery_full.bmp");


	if(_pQrCode == NULL)
	{
		//char tmp[256];
		//memset(tmp, 0, sizeof(tmp));
		//agcmd_property_get("system.boot.enc_code", tmp, " ");
		//sprintf(tmp, "<a>ABCDEFGH</a><p>12345678<p>");
		_pQrCode = new CBitQrCodeControl(_pPannel,CPoint(32,0), CSize(64,64), NULL);
	}
	_pQrCode->SetHiden();

	_pWifiSetWnd = new CWifiControWnd(pMgr, _cp);

	_pMicVolumeWnd = new CMicVolumeWnd(pMgr, _cp);
	
}


ui::CBitInforWnd::~CBitInforWnd()
{
	delete _pBatteryBmp;
	delete[] _ppLinkSymble;
	delete _pPannel;
}


void ui::CBitInforWnd::willHide()
{
	//_currentInfor = 0;
}

void ui::CBitInforWnd::willShow()
{
	updateCurrentInfor();
}

bool ui::CBitInforWnd::OnEvent(CEvent *event)
{
	//printf("--event: %d\n", event->_event);
	bool b = true;
	switch(event->_type)
	{
		case eventType_internal:
			if((event->_event == InternalEvent_app_timer_update))
			{
				//printf("-- timer update \n");
				//if(_ppLinkSymble[_currentInfor])
				if(_ppLinkSymble[_currentInfor]!=NULL)
				{	
					//printf("-- timer update :%d\n", _currentInfor);
					_ppLinkSymble[_currentInfor]->OnEvent(event);
				}
				updateCurrentInfor();
				
			}
			break;
		case eventType_key:
			switch(event->_event)
			{
				case key_right:
					popConfigWnd();
					//this->PopWnd(_pWifiSetWnd);
					break;
				case key_ok:
					this->Close();
					b = false;
					break;
				case key_up:
					//printf("--key up\n");
					_currentInfor--;
					if(_currentInfor < 0)
						_currentInfor = infor_num - 1;
					updateCurrentInfor();
					b = false;
					break;
				case key_down:
					//printf("--key down\n");
					_currentInfor ++;
					if(_currentInfor >= infor_num)
						_currentInfor = 0;
					updateCurrentInfor();
					b = false;
					break;
				default:
					printf("---key :%d\n", event->_event);
					break;
			}
			break;
		case eventType_Gesnsor:
			{
				_currentInfor = infor_qrcode;
				updateCurrentInfor();
				b = false;
			}
			break;
	}
	if(b)
		b = inherited::OnEvent(event);
	return b;
}


void ui::CBitInforWnd::SetSymble
	(CSymbleWithTxt* p
	, int index)
{
	_ppLinkSymble[index] = p;
}

void ui::CBitInforWnd::updateCurrentInfor()
{
	int i = -1;
	//printf("updata: 0x%x\n",_ppLinkSymble[_currentInfor]);
	if(_ppLinkSymble[_currentInfor]!=NULL)
	{
		i = _ppLinkSymble[_currentInfor]->getCurrentSymble();
		_pBitInforSymble->SetCurrentSymbleBmp(_ppLinkSymble[_currentInfor]->getCurrentSymbleBmp());
	}else
	{
		if(_currentInfor == infor_battery)
		{
			_pBitInforSymble->SetCurrentSymbleBmp(_pBatteryBmp);
		}
	}
	_pBitInforSymble->SetShow();
	_textInfor->SetShow();
	_pRow->SetShow();
	_pQrCode->SetHiden();
	switch(_currentInfor)
	{
		case infor_language:
			{
				sprintf(tmp1, "%s", LoadTxtItem("Language")->getContent());
				_cp->GetSystemLanugage(tmp2);
				_pBitInforSymble->SetText((UINT8*)tmp1);
				_textInfor->SetText((UINT8*)tmp2);
			}
			break;
		case infor_wifi:
			//printf("infor_wifi : %d\n", i);
				switch(i)
				{
					case CBitWifiSybl::symble_ap:
						{
							_cp->GetWifiAPinfor(tmp1, tmp2);
							_pBitInforSymble->SetText((UINT8*)tmp1);
							_textInfor->SetText((UINT8*)tmp2);
						}
						break;
					case CBitWifiSybl::symble_clt:
						{
							wifi_State state = _cp->getWifiState();
							switch(state)
							{
								case wifi_State_ready:				
									sprintf(tmp1, "Connected");
									break;
								case wifi_State_prepare:				
									sprintf(tmp1, "Prepare");
									break;
								case wifi_State_error:
									sprintf(tmp1, "No Connect");
									break;
								default:
									break;
							}
							
							_cp->GetWifiCltinfor(tmp2);
							_pBitInforSymble->SetText((UINT8*)(LoadTxtItem(tmp1))->getContent());
							_textInfor->SetText((UINT8*)tmp2);
						}
						break;
					case CBitWifiSybl::symble_off:
						{
							_pBitInforSymble->SetText((UINT8*)"Off");
							_textInfor->SetText((UINT8*)"Off");
						}
						break;
					default:
						break;
				}
			break;
		case infor_bt:
			switch(i)
				{
					case CBitBtSybl::symble_bt_connect:
						{
							_pBitInforSymble->SetText((UINT8*)"Connected");
							_textInfor->SetText((UINT8*)"--TBD--");
						}
						break;
					case CBitBtSybl::symble_bt_disconnect:
						{
							_pBitInforSymble->SetText((UINT8*)"No Device");
							_textInfor->SetText((UINT8*)"");
						}
						break;
		
					default:
						break;
				}
			break;
		case infor_gps:
			switch(i)
			{
				case CBitGPSSybl::symble_gps_ready:
					{
						_pBitInforSymble->SetText((UINT8*)"Position");
						_textInfor->SetText((UINT8*)"--TBD--");
					}
					break;
				case CBitGPSSybl::symble_gps_offline:
					{
						_pBitInforSymble->SetText((UINT8*)"Offline");
						_textInfor->SetText((UINT8*)"--TBD--");
					}
					break;
	
				default:
					break;
			}
			break;
		case infor_internet:
			switch(i)
			{
				case CBitInternetSybl::symble_internet_online:
					{
						_pBitInforSymble->SetText((UINT8*)"Online");
						_textInfor->SetText((UINT8*)"Not support");
					}
					break;
				case CBitInternetSybl::symble_internet_offline:
					{
						_pBitInforSymble->SetText((UINT8*)"Offline");
						_textInfor->SetText((UINT8*)"--TBD--");
					}
					break;
	
				default:
					break;
			}
			break;
		case infor_audio:
			switch(i)
			{
				case CBitAudioSybl::symble_audioOn:
					{
						_pBitInforSymble->SetText((UINT8*)"Audio");
						//char tmp3;
						int volume = 0;
						agcmd_property_get(audioGainPropertyName, tmp3, "NA");
						if(strcmp(tmp3, "NA")==0)
						{
							volume = defaultMicGain;
						}
						else
							volume = atoi(tmp3);
						sprintf(tmp3,"%d", volume);
						_textInfor->SetText((UINT8*)tmp3);
					}
					break;
				case CBitAudioSybl::symble_audioOff:
					{
						_pBitInforSymble->SetText((UINT8*)"Audio");
						_textInfor->SetText((UINT8*)"Mute");
					}
					break;
	
				default:
					break;
			}
			break;
		case infor_battery:
			{
				bool bIncharge;
				int batteryV = _cp->GetBatteryVolume(bIncharge);
				sprintf(_pBatteryVolume, "%d", batteryV);
				_pBitInforSymble->SetText((UINT8*)_pBatteryVolume);
				_textInfor->SetText((UINT8*)"--TBD--");
			}
			break;
		case infor_qrcode:
			{
				_pBitInforSymble->SetHiden();
				_textInfor->SetHiden();
				_pRow->SetHiden();
				{
					char tmp[256];
					GetWifiInfor(tmp);
					_pQrCode->SetString(tmp);
				}
				_pQrCode->SetShow();
			}
			break;
		default:
			break;
	}
	this->GetMainObject()->Draw(false);
	//this->GetManager()->GetCanvas()->Update();
}

void ui::CBitInforWnd::GetWifiInfor(char *tmp)
{
	char tmp1[64];
	char tmp2[64];
	wifi_mode mode = _cp->getWifiMode();
	switch(mode)
	{
		case wifi_mode_AP:
			{
				_cp->GetWifiAPinfor(tmp1, tmp2);
			}
			break;
		case wifi_mode_Client:
			{
				_cp->GetWifiCltinfor(tmp1);
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

void ui::CBitInforWnd::popConfigWnd()
{
	switch(_currentInfor)
	{
		case infor_language:
			if(_pLanCtrlWnd == NULL)
			{
				//printf("-- %x\n",  _ppLinkSymble[infor_language]);
				_pLanCtrlWnd = new CLanguageControlWnd(this->GetManager(), _cp);
			}
			this->PopWnd(_pLanCtrlWnd);
			break;
		case infor_wifi:
			this->PopWnd(_pWifiSetWnd);
			break;
		case infor_bt:
			break;
		case infor_gps:
			break;
		case infor_internet:
			break;
		case infor_audio:
			this->PopWnd(_pMicVolumeWnd);
			break;
		case infor_battery:
			break;
		case infor_qrcode:
			break;

	}
}


/*******************************************************************************
*   CBitQrCodeControl
*   
*/
ui::CBitQrCodeControl::CBitQrCodeControl
		(CContainer* pParent
		,CPoint leftTop
		,CSize size
		,char* string
		) : inherited(pParent, leftTop, size) 
		,_bmpBuf(0)
		,_pBmp(0)
		,_qrcode(NULL)
{
	SetString(string);
    /*_code_width = _qrcode->width + 2;
	printf("--qr code size: %d\n", _code_width);
	int code_pitch = (_code_width / 8)+((_code_width % 8) > 0 ? 1 : 0);
	code_pitch = ((code_pitch / 4) + ((code_pitch%4) > 0 ? 1 : 0)) * 4; 
	int m, n; // k, l;
	float f;
	if((_code_width < size.width) && (_code_width < size.height)&&(size.height == size.width))
	{
		_bmpBuf = new unsigned char [code_pitch * _code_width];
		memset((char*)_bmpBuf, 0xff, code_pitch * _code_width);
		m = size.width / _code_width; n = size.width % _code_width;
		f = (float)n / _code_width;
		int bits;
		int rows;
		printf("------fill qr code buffer : %d %d %f\n", m, n, f);
		int pp;
		for(int i = 1, k = 0; i < _code_width - 1 ; i ++)
		{
			if(((i-1) * f - 1) >= k)
			{
				rows = m + 1;
			}
			else
				rows = m;
			for(int j = 1, l = 0; j < _code_width - 1 ; j ++)
			{
				if(((j -1 ) * f - 1) > l)
				{
					bits = m + 1;					
				}
				else
					bits = m;
				if((_qrcode->data[(i-1)*(_code_width - 2) + (j - 1)]&0x01))
				{
					for(int s = 0; s < rows; s++)
					{
						pp = (size.height - 1 - (i* m + k + s)) * size.width + j * m + l;
						memset((char*)&(_bmpBuf[pp]), 0 , sizeof(COLOR)*bits);
					}
				}
				if(bits == m + 1)
					l ++;
				//else
				//printf("1");
			}
			//printf("\n");
			if(rows == m + 1)
				k++;
		}
		_pBmp = new CBmp(GetSize().width, GetSize().height, sizeof(COLOR)*8, GetSize().width*sizeof(COLOR), (BYTE*)_bmpBuf);
	}
	else
	{
		printf("--too small qrcode control size set to fill the code\n");
	}	*/
	
}

ui::CBitQrCodeControl::~CBitQrCodeControl()
{
	if(_qrcode)
	{
		QRcode_free(_qrcode);
		_qrcode = NULL;
	}
	if(_pBmp != 0)
	{
		delete _pBmp;
		_pBmp = NULL;
	}
}

void ui::CBitQrCodeControl::SetString(char* string)
{
	if(string != NULL)
	{
		if(_qrcode)
		{
			QRcode_free(_qrcode);
			_qrcode = NULL;
		}
		if(_pBmp != 0)
		{
			delete _pBmp;
			_pBmp = NULL;
		}
		QRecLevel level = QR_ECLEVEL_L;
		_qrcode = QRcode_encodeString8bit(string, 0, level);
	}
}

void ui::CBitQrCodeControl::Draw(bool bHilight)
{
	
	//if(_bmpBuf != 0)
	{
		CPoint tplft = GetAbsPos();
		CSize size = this->GetSize();
		CCanvas* pCav = this->GetOwner()->GetManager()->GetCanvas(); 
		CBrush *whiteBackground = CStyle::GetInstance()->GetBrush(BSL_Bit_White_Pannel);
		whiteBackground->Draw(this);
		if(pCav->isBitCanvas())
		{
			int BitMapWidth = _qrcode->width;
			int BitMapHeight = _qrcode->width;
			int ActuralWidth = _qrcode->width;
			int X = tplft.x + (size.width - BitMapWidth) / 2;
			int Y = tplft.y + (size.height- BitMapHeight) / 2;
			bool bDoubleSize = false;
			if(size.width > 2*BitMapWidth)
				bDoubleSize = true;
			if(bDoubleSize)
			{
				
				X = tplft.x + (size.width - 2*BitMapWidth) / 2;
				Y = tplft.y + (size.height - 2*BitMapHeight) / 2;
			}
			int  height, width;
    		UINT8 *canvas;
    		UINT8  bit=0;
		    UINT32 byte_in_bmpbuffer=0;
		    UINT8* pMem = (UINT8*)pCav->GetCanvas();
			UINT16 memWidth = pCav->GetSize().width / 8;
			UINT16 memHeight = pCav->GetSize().height;
		    register UINT8* memlimit = pMem + memWidth * memHeight;
		    UINT8* ref = pMem + (memWidth * Y) + X / 8;
		    int r = X % 8;
			//printf("--x: %d, r: %d\n",X, r);
			if(bDoubleSize)
			{
				//printf("--double size\n");
				UINT8* p1, *p2, *p3, *p4;
				UINT8 s1, s2;
				for(height=0 ; height < BitMapHeight;height++)
			    {
			        canvas = height * 2 * memWidth + ref;
			        for(width=0; width < ActuralWidth; width++)
			        {
			        	
						p1 = canvas + (r + width*2)/8;
						p2 = canvas + ((r + width*2)+1)/8;
						p3 = p1 + memWidth;
						p4 = p2 + memWidth;
	
						if((p1 >= memlimit)||(p1 < pMem))
						{
							//printf("--y: %d\n", Y + height);
			                continue;
						}
						else
						{
				            byte_in_bmpbuffer=(height*BitMapWidth+width)/8;
				            bit=(_qrcode->data[height *(_qrcode->width) + width]&0x01);
							s1 = 0x80>>((r + width*2)%8);
							s2 = 0x80>>(((r + width*2)+1)%8);

							if(bit)
							{
								*p1 = *p1&(~s1);
								*p3 = *p3&(~s1);

								*p2 = *p2&(~s2);
								*p4 = *p4&(~s2);
							}
							else
							{
								*p1 = *p1|s1;
								*p3 = *p3|s1;
								
								*p2 = *p2|s2;
								*p4 = *p4|s2;
							}
						}
			        }
			    }
			}
			else
			{
				UINT8* p;
				UINT8 s;
			    for(height=0 ; height < BitMapHeight;height++)
			    {
			        canvas = height * memWidth + ref;
			        for(width=0;width < ActuralWidth; width++)
			        {
			        	
						p = canvas + (r + width)/8;
						if((p >= memlimit)||(p < pMem))
						{
							//printf("--y: %d\n", Y + height);
			                continue;
						}
			            byte_in_bmpbuffer=(height*BitMapWidth+width)/8;
			            bit=(_qrcode->data[height *(_qrcode->width) + width]&0x01);
						s = 0x80>>((r + width)%8);

						if(bit)
							*p = *p|s;
						else
							*p = *p&(~s);
			        }
			    }
			}
		}
		//pCav->DrawRect(tplft, size, _pBmp, CCanvas::BmpFillMode_repeat, 0);
	}
}
/*******************************************************************************
*   CBitQRCodeWnd
*   
*/
ui::CBitQRCodeWnd::CBitQRCodeWnd
	(CWndManager *pMgr, CCameraProperties* cp
	):inherited("BitRecWnd", pMgr)
	,_cp(cp)
{
	_pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
	this->SetMainObject(_pPannel);
	_pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);
	//_qrcode = new CBitQrCodeControl();
}

ui::CBitQRCodeWnd::~CBitQRCodeWnd()
{
	delete _pPannel;
}

void ui::CBitQRCodeWnd::willHide()
{

}

void ui::CBitQRCodeWnd::willShow()
{
	if(_qrcodeC == NULL)
	{
		char tmp[256];
		//memset(tmp, 0, sizeof(tmp));
		//agcmd_property_get("system.boot.enc_code", tmp, " ");
		sprintf(tmp, "<a>ABCDEFGH</a><p>12345678<p>");
		_qrcodeC = new CBitQrCodeControl(_pPannel,CPoint(32,0),CSize(64,64),tmp);
	}
}

bool ui::CBitQRCodeWnd::OnEvent(CEvent *event)
{
	//printf("--event: %d\n", event->_event);
	bool b = true;
	switch(event->_type)
	{
		case eventType_internal:
			if((event->_event == InternalEvent_app_timer_update))
			{
				
			}
			break;
		case eventType_key:
			switch(event->_event)
			{
				case key_right:
					//this->PopWnd(); pop setup
					break;
				case key_ok:
					this->Close();
					b = false;
					break;
				case key_up:
					
					break;
				case key_down:
					
					break;
				default:
					printf("---key :%d\n", event->_event);
					break;
			}
			break;

	}
	if(b)
		b = inherited::OnEvent(event);
	return b;
}

/*******************************************************************************
*   CBitPowerOffPreapreWnd
*   
*/
ui::CBitPowerOffPreapreWnd::CBitPowerOffPreapreWnd
	(CWndManager *pMgr
	, CCameraProperties* cp)
	:inherited("BitRecWnd", pMgr)
	,_cp(cp)
	,_counter(3)
{
	_pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
	this->SetMainObject(_pPannel);
	_pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

	_pNotice= new CStaticText(_pPannel, CPoint(0, 0), CSize(128, 24));
	_pNotice->SetStyle(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_White_Pannel
		,FSL_Bit_Black_Text_small
		,0
		,0
		,0
		,0);
	_pNotice->SetLanguageItem(LoadTxtItem("Power off!"));
	_pNotice->SetHAlign(CENTER);
	_pNotice->SetVAlign(MIDDLE);

	_pRow = new CBlock(_pPannel, CPoint(2, 24), CSize(124, 2), BSL_Bit_White_Pannel);

	_pT = _pNotice= new CStaticText(_pPannel, CPoint(0, 26), CSize(128, 38));
	_pNotice->SetStyle(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_White_Pannel
		,FSL_Bit_Black_Text_small
		,0
		,0
		,0
		,0);
	_pNotice->SetText((UINT8*)_tt);
	_pNotice->SetHAlign(CENTER);
	_pNotice->SetVAlign(MIDDLE);	
}


ui::CBitPowerOffPreapreWnd::~CBitPowerOffPreapreWnd()
{
	delete _pPannel;
}


void ui::CBitPowerOffPreapreWnd::willHide()
{

}

void ui::CBitPowerOffPreapreWnd::willShow()
{
	_counter = 3;
	sprintf(_tt,"%d", _counter);
}

bool ui::CBitPowerOffPreapreWnd::OnEvent(CEvent *event)
{
	bool b = true;
	switch(event->_type)
	{
		case eventType_key:
			switch(event->_event)
			{
				case key_poweroff_prepare:
					_counter --;
					printf("--%d\n", _counter);
					sprintf(_tt,"%d", _counter);
					_pPannel->Draw(true);
					//_pT->Draw(true);
					b = false;
					break;
				default:
					//printf("---key :%d\n", event->_event);
					this->Close();
					b = false;
					break;
			}
			break;
		case eventType_Gesnsor:
			break;
	}
	if(b)
		b = inherited::OnEvent(event);
	return b;
}


/*******************************************************************************
*   CBitAudioSybl
*   
*/
ui::CWifiControWnd::CWifiControWnd
	(CWndManager *pMgr
	, CCameraProperties* cp
	):inherited("BitRecWnd", pMgr)
	,_cp(cp)
{
	_pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
	this->SetMainObject(_pPannel);
	_pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

	_pSymble = new CBitWifiSybl(_pPannel, CPoint(0, 0), CSize(128, 24),true ,cp);
	_pSymble->SetStyle
		(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small);
	_pSymble->SetHAlign(CENTER);
	_pSymble->SetVAlign(TOP);
	_pSymble->AdjustVertical(0);
	_pSymble->SetDisabled();

	CLanguageItem *pItem = LoadTxtItem("Setup");
	_pSymble->SetLanguageItem(pItem);
	
	_pRow = new CBlock(_pPannel, CPoint(2, 24), CSize(124, 2), BSL_Bit_White_Pannel);
	_pRow->SetDisabled();
	
	_pSelector = new CHSelector(_pPannel, NULL, CPoint(0, 26), CSize(128, 38), 2, 5, 42);
	_pSelector->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);
	_pAP = new CStaticText(_pSelector, CPoint(0, 0), CSize(40, 24));
	_pAP->SetStyle(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_White_Pannel
		,FSL_Bit_Black_Text_small
		,0
		,0
		,0
		,0);
	_pAP->SetHAlign(CENTER);
	_pAP->SetVAlign(MIDDLE);
	pItem = LoadTxtItem("AP");
	_pAP->SetLanguageItem(pItem);

	_pCLT= new CStaticText(_pSelector, CPoint(0, 0), CSize(40, 24));
	_pCLT->SetStyle(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_White_Pannel
		,FSL_Bit_Black_Text_small
		,0
		,0
		,0
		,0);
	pItem = LoadTxtItem("CLT");
	_pCLT->SetLanguageItem(pItem);
	_pCLT->SetHAlign(CENTER);
	_pCLT->SetVAlign(MIDDLE);

	_pOFF= new CStaticText(_pSelector, CPoint(0, 0), CSize(40, 24));
	_pOFF->SetStyle(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_White_Pannel
		,FSL_Bit_Black_Text_small
		,0
		,0
		,0
		,0);
	pItem = LoadTxtItem("Off");
	_pOFF->SetLanguageItem(pItem);
	_pOFF->SetHAlign(CENTER);
	_pOFF->SetVAlign(MIDDLE);
	
}
ui::CWifiControWnd::~CWifiControWnd()
{

}

void ui::CWifiControWnd::willHide()
{

}

void ui::CWifiControWnd::willShow()
{
	int i = 0;
	char tmp[256];
	agcmd_property_get(WIFI_KEY, tmp, "");
	_pPannel->SetCurrentHilight(2);
	if(strcmp(tmp, WIFI_AP_KEY) == 0)
	{
		_pSelector->SetCurrentHilight(0);
	}
	else if(strcmp(tmp, WIFI_CLIENT_KEY) == 0)
	{
		_pSelector->SetCurrentHilight(1);
	}
	else
	{
		_pSelector->SetCurrentHilight(2);
	}
}

bool ui::CWifiControWnd::OnEvent(CEvent *event)
{
	bool b = true;
	switch(event->_type)
	{
		case eventType_internal:
			if((event->_event == InternalEvent_app_timer_update))
			{
				
			}
			break;
		case eventType_key:
			switch(event->_event)
			{
				case key_right:
					{
						_cp->SetWifiMode(_pSelector->GetCurrentHilight());
						this->Close();
						b = false;
					}
					break;
				case key_ok:
					this->Close();
					b = false;
					break;
				default:
					//printf("---key :%d\n", event->_event);
					break;
			}
			break;

	}
	if(b)
		b = inherited::OnEvent(event);
	return b;
}

/*******************************************************************************
*   CBitAudioSybl
*   
*/
ui::CLanguageControlWnd::CLanguageControlWnd
	(CWndManager *pMgr
	, CCameraProperties* cp
	//, CSymbleWithTxt * lanSymb
	):inherited("BitRecWnd", pMgr)
	,_cp(cp)
{
	_pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
	this->SetMainObject(_pPannel);
	_pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

	//_pSymble = lanSymb;
	_pSymble = new CBitLanguageSybl(_pPannel, CPoint(0, 0), CSize(128, 24),true);
	_pSymble->SetStyle
		(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small);
	_pSymble->SetHAlign(CENTER);
	_pSymble->SetVAlign(TOP);
	_pSymble->AdjustVertical(0);
	_pSymble->SetDisabled();

	CLanguageItem *pItem = LoadTxtItem("Language");
	_pSymble->SetLanguageItem(pItem);
	_pSymble->SetDisabled();
	
	_pRow = new CBlock(_pPannel, CPoint(2, 24), CSize(124, 2), BSL_Bit_White_Pannel);
	_pRow->SetDisabled();
	
	_pSelector = new CHSelector(_pPannel, NULL, CPoint(0, 26), CSize(128, 38), 2, 5, 128);
	_pSelector->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);

	_num;
	char** ppList;
	ppList = CLanguageLoader::getInstance()->GetLanguageList(_num);
	_pLanguages = new CStaticText*[_num];
	//printf("--- lang num : %d\n", _num);
	for(int i = 0; i < _num; i++)
	{
		
		_pLanguages[i] = new CStaticText(_pSelector, CPoint(0, 0), CSize(110, 24));
		_pLanguages[i]->SetStyle(BSL_Bit_Black_Pannel
			,FSL_Bit_White_Text_small
			,BSL_Bit_White_Pannel
			,FSL_Bit_Black_Text_small
			,0
			,0
			,0
			,0);
		_pLanguages[i]->SetHAlign(CENTER);
		_pLanguages[i]->SetVAlign(MIDDLE);
		_pLanguages[i]->SetText((UINT8*)ppList[i]);
		_pLanguages[i]->SetLanguageItem(pItem);
	}
}
ui::CLanguageControlWnd::~CLanguageControlWnd()
{

}

void ui::CLanguageControlWnd::willHide()
{

}

void ui::CLanguageControlWnd::willShow()
{
	int i = 0;
	char tmp[256];
	_cp->GetSystemLanugage(tmp);
	_pPannel->SetCurrentHilight(2);
	for(int i = 0; i < _num; i++)
	{
		if(strcmp(tmp, (char*)_pLanguages[i]->GetText()) == 0)
		{
			_pSelector->SetCurrentHilight(i);
			break;
		}
	}
}

bool ui::CLanguageControlWnd::OnEvent(CEvent *event)
{
	bool b = true;
	switch(event->_type)
	{
		case eventType_internal:
			if((event->_event == InternalEvent_app_timer_update))
			{
				
			}
			break;
		case eventType_key:
			switch(event->_event)
			{
				case key_right:
					{
						char* language = (char*)_pLanguages[_pSelector->GetCurrentHilight()]->GetText();
						//printf("--set language: %s\n", language);
						CLanguageLoader::getInstance()->LoadLanguage(language);
						_cp->SetSystemLanguage(language);
						this->Close();
						b = false;
					}
					break;
				case key_ok:
					this->Close();
					b = false;
					break;
				default:
					//printf("---key :%d\n", event->_event);
					break;
			}
			break;

	}
	if(b)
		b = inherited::OnEvent(event);
	return b;
}

/*******************************************************************************
*   CBitAudioSybl
*   
*/
ui::CBitAudioSybl::CBitAudioSybl(
		CContainer* pParent
		,CPoint leftTop
		, CSize CSize
		, bool left
		, CCameraProperties* p
		):inherited(pParent, leftTop, CSize, left)
		,_cp(p)	
{
	CBmp** symbles = new CBmp*[symble_num];
	symbles[symble_audioOn] = new CBmp(WndResourceDir,"icon_audio.bmp");
	symbles[symble_audioOff] = new CBmp(WndResourceDir,"icon_mute.bmp");
	SetSymbles(symble_num, symbles);

	//_pVolumeInfo = new char[20];
	//memset(_pVolumeInfo, 0, 20);
	this->SetText((UINT8*)NULL);
	SetTitles(0, NULL);	
}

ui::CBitAudioSybl::~CBitAudioSybl()
{
	//delete[] _pVolumeInfo;
}

bool ui::CBitAudioSybl::OnEvent(CEvent *event)
{
	if((event->_type == eventType_internal)&&((event->_event == InternalEvent_app_state_update) ||(event->_event == InternalEvent_app_timer_update)))
	{
		/*audio_State state = _cp->getAudioState();
		switch(state)
		{
			case audio_State_off:
					this->SetCurrentSymble(symble_audioOff);
				break;
			case audio_State_on:
				{
					this->SetCurrentSymble(symble_audioOn);
				}
				break;
			default:
				break;
		}*/
		char tmp[256];
		int volume = 0;
		agcmd_property_get(audioGainPropertyName, tmp, "NA");
		if(strcmp(tmp, "NA")==0)
		{
			volume = defaultMicGain;
		}
		else
			volume = atoi(tmp);
		if(volume == 0)
			this->SetCurrentSymble(symble_audioOff);
		else
			this->SetCurrentSymble(symble_audioOn);
		this->Draw(false);
	}
	return true;
}


/*******************************************************************************
*   CBitAudioSybl
*   
*/
ui::CBitWifiSybl::CBitWifiSybl(
		CContainer* pParent
		,CPoint leftTop
		, CSize CSize
		, bool left
		, CCameraProperties* p
		):inherited(pParent, leftTop, CSize, left)
		,_cp(p)	
{
	CBmp** symbles = new CBmp*[symble_num];
	symbles[symble_ap] = new CBmp(WndResourceDir,"icon_ap.bmp");
	symbles[symble_clt] = new CBmp(WndResourceDir,"icon_wifi_clt.bmp");
	symbles[symble_off] = new CBmp(WndResourceDir,"icon_wifi_off.bmp");
	SetSymbles(symble_num, symbles);

	//_pVolumeInfo = new char[20];
	//memset(_pVolumeInfo, 0, 20);
	this->SetText((UINT8*)NULL);
	SetTitles(0, NULL);	
}

ui::CBitWifiSybl::~CBitWifiSybl()
{


}

bool ui::CBitWifiSybl::OnEvent(CEvent *event)
{
	if((event->_type == eventType_internal)&&((event->_event == InternalEvent_app_state_update) ||(event->_event == InternalEvent_app_timer_update)))
	{
		
		wifi_mode mode = _cp->getWifiMode();
		//printf("--ui::WifiSymble::OnEvent\n");
		switch(mode)
		{
			case wifi_mode_AP:				
				this->SetCurrentSymble(symble_ap);
				break;
			case wifi_mode_Client:				
				this->SetCurrentSymble(symble_clt);
				break;
			case wifi_mode_Off:
				this->SetCurrentSymble(symble_off);
				break;
			default:
				break;
		}
		/*wifi_State state = _cp->getWifiState();
		switch(state)
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
		}*/
		this->Draw(false);
	}
	return true;
}


/*******************************************************************************
*   CBitBtSybl
*   
*/
ui::CBitBtSybl::CBitBtSybl(
		CContainer* pParent
		,CPoint leftTop
		, CSize CSize
		, bool left
		, CCameraProperties* p
		):inherited(pParent, leftTop, CSize, left)
		,_cp(p)	
{
	CBmp** symbles = new CBmp*[symble_num];
	symbles[symble_bt_connect] = new CBmp(WndResourceDir,"icon_bt_c.bmp");
	symbles[symble_bt_disconnect] = new CBmp(WndResourceDir,"icon_bt.bmp");
	SetSymbles(symble_num, symbles);

	//_pVolumeInfo = new char[20];
	//memset(_pVolumeInfo, 0, 20);
	this->SetText((UINT8*)NULL);
	SetTitles(0, NULL);	
}

ui::CBitBtSybl::~CBitBtSybl()
{


}

bool ui::CBitBtSybl::OnEvent(CEvent *event)
{
	return true;
}

/*******************************************************************************
*   CBitBatterySybl
*   
*/
ui::CBitBatterySybl::CBitBatterySybl(
		CContainer* pParent
		,CPoint leftTop
		, CSize CSize
		, bool left
		, CCameraProperties* p
		):inherited(pParent, leftTop, CSize, left)
		,_cp(p)	
{
	CBmp** symbles = new CBmp*[symble_num];
	symbles[symble_charge_full] = new CBmp(WndResourceDir,"icon_battery_full_charge.bmp");
	symbles[symble_charge_high] = new CBmp(WndResourceDir,"icon_battery_high_charge.bmp");
	symbles[symble_charge_normal] = new CBmp(WndResourceDir,"icon_battery_normal_charge.bmp");
	symbles[symble_charge_low] = new CBmp(WndResourceDir,"icon_battery_low_charge.bmp");
	symbles[symble_charge_warnning] = new CBmp(WndResourceDir,"icon_battery_warnning_charge.bmp");
	symbles[symble_full] = new CBmp(WndResourceDir,"icon_battery_full.bmp");
	symbles[symble_high] = new CBmp(WndResourceDir,"icon_battery_high.bmp");
	symbles[symble_normal] = new CBmp(WndResourceDir,"icon_battery_normal.bmp");
	symbles[symble_low] = new CBmp(WndResourceDir,"icon_battery_low.bmp");
	symbles[symble_warnning] = new CBmp(WndResourceDir,"icon_battery_warnning.bmp");

	SetSymbles(symble_num, symbles);

	//_pVolumeInfo = new char[20];
	//memset(_pVolumeInfo, 0, 20);
	this->SetText((UINT8*)NULL);
	SetTitles(0, NULL);	
}

ui::CBitBatterySybl::~CBitBatterySybl()
{

}

bool ui::CBitBatterySybl::OnEvent(CEvent *event)
{
	if((event->_type == eventType_internal)&&(event->_event == InternalEvent_app_timer_update))
	{
		bool bIncharge;
		int batteryV = _cp->GetBatteryVolume(bIncharge);
		//printf("batt v:%d\n", batteryV);
		if (bIncharge) {
			if (batteryV >= PROP_POWER_SUPPLY_CAPACITY_FULL) {
				this->SetCurrentSymble(symble_charge_full);
			} else if(batteryV >= PROP_POWER_SUPPLY_CAPACITY_HIGH) {
				this->SetCurrentSymble(symble_charge_high);
			} else if(batteryV >= PROP_POWER_SUPPLY_CAPACITY_NORMAL) {
				this->SetCurrentSymble(symble_charge_normal);
			} else if(batteryV >= PROP_POWER_SUPPLY_CAPACITY_LOW) {
				this->SetCurrentSymble(symble_charge_low);
			} else {//PROP_POWER_SUPPLY_CAPACITY_CRITICAL
				this->SetCurrentSymble(symble_charge_warnning);
			}
		} else {
			if(batteryV >= PROP_POWER_SUPPLY_CAPACITY_FULL) {
				this->SetCurrentSymble(symble_full);
			} else if(batteryV >= PROP_POWER_SUPPLY_CAPACITY_HIGH) {
				this->SetCurrentSymble(symble_high);
			} else if(batteryV >= PROP_POWER_SUPPLY_CAPACITY_NORMAL) {
				this->SetCurrentSymble(symble_normal);
			} else if(batteryV >= PROP_POWER_SUPPLY_CAPACITY_LOW) {
				this->SetCurrentSymble(symble_low);
			} else {//PROP_POWER_SUPPLY_CAPACITY_CRITICAL
				this->SetCurrentSymble(symble_warnning);
			}
		}
		this->Draw(false);
	}
	return true;
}

/*******************************************************************************
*   CBitGPSSybl
*   
*/
ui::CBitGPSSybl::CBitGPSSybl(
		CContainer* pParent
		,CPoint leftTop
		, CSize CSize
		, bool left
		, CCameraProperties* p
		):inherited(pParent, leftTop, CSize, left)
		,_cp(p)	
{
	CBmp** symbles = new CBmp*[symble_num];
	symbles[symble_gps_ready] = new CBmp(WndResourceDir,"icon_gps_on.bmp");
	symbles[symble_gps_offline] = new CBmp(WndResourceDir,"icon_gps.bmp");
	SetSymbles(symble_num, symbles);

	//_pVolumeInfo = new char[20];
	//memset(_pVolumeInfo, 0, 20);
	this->SetText((UINT8*)NULL);
	SetTitles(0, NULL);	
}

ui::CBitGPSSybl::~CBitGPSSybl()
{


}

bool ui::CBitGPSSybl::OnEvent(CEvent *event)
{
	if((event->_type == eventType_internal)&&(event->_event == InternalEvent_app_timer_update))
	{
		gps_state state = _cp->getGPSState();
		switch(state)
		{
			case gps_state_on:				
				this->SetCurrentSymble(symble_gps_offline);
				break;
			case gps_state_ready:				
				this->SetCurrentSymble(symble_gps_ready);
				break;
			case gps_state_off:				
				this->SetCurrentSymble(symble_gps_offline);
				break;
			default:
				break;
		}
		this->Draw(false);
	}
	return true;
}

/*******************************************************************************
*   CBitInternetSybl
*   
*/
ui::CBitInternetSybl::CBitInternetSybl(
		CContainer* pParent
		,CPoint leftTop
		, CSize CSize
		, bool left
		, CCameraProperties* p
		):inherited(pParent, leftTop, CSize, left)
		,_cp(p)	
{
	CBmp** symbles = new CBmp*[symble_num];
	symbles[symble_internet_online] = new CBmp(WndResourceDir,"icon_internet.bmp");
	symbles[symble_internet_offline] = new CBmp(WndResourceDir,"icon_internet_off.bmp");
	SetSymbles(symble_num, symbles);

	//_pVolumeInfo = new char[20];
	//memset(_pVolumeInfo, 0, 20);
	this->SetText((UINT8*)NULL);
	SetTitles(0, NULL);	
}

ui::CBitInternetSybl::~CBitInternetSybl()
{


}

bool ui::CBitInternetSybl::OnEvent(CEvent *event)
{
	return true;
}


/*******************************************************************************
*   CBitRecSybl
*   
*/
ui::CBitRecSybl::CBitRecSybl
	(CContainer* pParent
	,CPoint leftTop
	, CSize CSize
	, bool left
	, CCameraProperties* p
	):inherited(pParent, leftTop, CSize, left)
	,_cp(p)
{
	CBmp** symbles = new CBmp*[symble_num];
	symbles[symble_stop] = new CBmp(WndResourceDir,"icon_stop.bmp");
	symbles[symble_record] = new CBmp(WndResourceDir,"icon_rec.bmp");
	symbles[symble_record1] = new CBmp(WndResourceDir,"icon_rec_1.bmp");
	symbles[symble_mark] = new CBmp(WndResourceDir,"icon_mark.bmp");
	symbles[symble_play] = new CBmp(WndResourceDir,"icon_play.bmp");
	symbles[symble_pause] = new CBmp(WndResourceDir,"icon_pause.bmp");	
	SetSymbles(symble_num, symbles);

	_pTimeInfo = new char[maxMenuStrinLen];
	this->SetText((UINT8*)_pTimeInfo);
	SetTitles(0, NULL);	
	resetState();
	//count = 0;
}

ui::CBitRecSybl::~CBitRecSybl()
{
	delete[] _pTimeInfo;
}

void ui::CBitRecSybl::resetState()
{
	memset(_pTimeInfo, 0, maxMenuStrinLen);
	sprintf(_pTimeInfo, "000:00:00");
	this->SetCurrentSymble(symble_stop);
}
bool ui::CBitRecSybl::OnEvent(CEvent *event)
{
	if((event->_type == eventType_internal)&&(event->_event == InternalEvent_app_timer_update))
	{
		//count ++;
		//if(count % 10 == 0)
		//printf("--ui::CRecordSymble::OnEvent : %lld\n", count);
		
		camera_State state = _cp->getRecState();
		switch(state)
		{
			case camera_State_stop:
				this->SetCurrentSymble(symble_stop);
				break;
			case camera_State_record:
				if(getCurrentSymble() == camera_State_record)
					this->SetCurrentSymble(symble_record1);
				else
					this->SetCurrentSymble(symble_record);
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
		storage_State st = _cp->getStorageState();
		switch(st)
		{
			case storage_State_ready:
				{
					UINT64 tt = _cp->getRecordingTime();
					memset(_pTimeInfo, 0, maxMenuStrinLen);
					UINT64 hh =  tt /3600;
					UINT64 mm = (tt % 3600) /60;
					UINT64 ss = (tt % 3600) %60;
					int j = 0;
					j = sprintf( _pTimeInfo, "%03lld:", hh);
   					j += sprintf( _pTimeInfo + j,"%02lld:", mm);
   					j += sprintf( _pTimeInfo + j,"%02lld", ss);
				}
				break;
			case storage_State_error:
				sprintf(_pTimeInfo, "TF ERR");
				break;
			case storage_State_prepare:
				sprintf(_pTimeInfo, "LOADING");
				break;
			case storage_State_full:
			sprintf(_pTimeInfo, "TF FULL");
				break;
			case storage_State_noMedia:
				sprintf(_pTimeInfo, "NO TF");
				break;
		}
		
		
		this->Draw(false);
	}
	return true;
	
}
/*******************************************************************************
*   CBrightSymble
*   
*/
ui::CBrightSymble::CBrightSymble(
		CContainer* pParent
		,CPoint leftTop
		, CSize CSize
		, bool left
		):inherited(pParent, leftTop, CSize, left)
{
	CBmp** symbles = new CBmp*[symble_num];
	symbles[symble_bright] = new CBmp(WndResourceDir,"icon_brightness.bmp");
	SetSymbles(symble_num, symbles);

	this->SetText((UINT8*)NULL);
	SetTitles(0, NULL);	
}

ui::CBrightSymble::~CBrightSymble()
{

}

bool ui::CBrightSymble::OnEvent(CEvent *event)
{

}

/*******************************************************************************
*   CBitLanguageSybl
*   
*/
ui::CBitLanguageSybl::CBitLanguageSybl(
		CContainer* pParent
		,CPoint leftTop
		, CSize CSize
		, bool left
		):inherited(pParent, leftTop, CSize, left)
{
	CBmp** symbles = new CBmp*[symble_num];
	symbles[symble_language] = new CBmp(WndResourceDir,"icon_brightness.bmp");
	SetSymbles(symble_num, symbles);

	this->SetText((UINT8*)NULL);
	SetTitles(0, NULL);	
}

ui::CBitLanguageSybl::~CBitLanguageSybl()
{

}

bool ui::CBitLanguageSybl::OnEvent(CEvent *event)
{

}




/*******************************************************************************
*   COLEDBrightnessWnd
*   
*/
ui::COLEDBrightnessWnd::COLEDBrightnessWnd
	(CWndManager *pMgr
	, CCameraProperties* cp
	):inherited("BitRecWnd", pMgr)
	,_cp(cp)
{
	_pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
	this->SetMainObject(_pPannel);
	_pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);
	
	_pSymbleTxt = new CBrightSymble(_pPannel, CPoint(0, 0), CSize(128, 24),true);
	_pSymbleTxt->SetStyle
		(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small);
	_pSymbleTxt->SetHAlign(CENTER);
	_pSymbleTxt->SetVAlign(TOP);
	_pSymbleTxt->AdjustVertical(0);
	_pSymbleTxt->SetDisabled();

	//_pBrightBmp = new CBmp(WndResourceDir,"icon_brightness.bmp");  // brightness
	//_pSymbleTxt->SetCurrentSymbleBmp(_pBrightBmp);
	_pSymbleTxt->SetText((UINT8*)_pBrightTxt);

	_pRow = new CBlock(_pPannel, CPoint(2, 24), CSize(124, 2), BSL_Bit_White_Pannel);
	_pRow->SetDisabled();

	_pBrightness = new CPosiBarSegH
		(_pPannel
		,CPoint(13, 36)
		,CSize(102, 12)
		,CPoint(2, 2)
		,CSize(98, 8)
		,9
		,1
		);
	_pBrightness->SetStyle
		(BSL_Bit_White_border,
		BSL_Bit_White_Pannel
		);
	_pBrightness->SetPosi(0);
	_pBrightness->setLength(256, 128);
	
}

ui::COLEDBrightnessWnd::~COLEDBrightnessWnd()
{
	delete _pPannel;
}

void ui::COLEDBrightnessWnd::willHide()
{
	
}

void ui::COLEDBrightnessWnd::willShow()
{
	int b = _cp->GetBrightness();
	sprintf(_pBrightTxt, "%d", b/28);
	_pBrightness->setLength(252, b);
}

bool ui::COLEDBrightnessWnd::OnEvent(CEvent *event)
{
	bool b = true;
	int bright = 0;
	switch(event->_type)
	{
		case eventType_key:
			switch(event->_event)
			{
				case key_right:
					this->Close();
					b = false;
					break;
				case key_ok:
					this->Close();
					b = false;
					break;
				case key_up:
					bright = _cp->SetDisplayBrightness(1);
					sprintf(_pBrightTxt, "%d", bright/28);
					_pBrightness->setLength(252, bright);
					this->GetMainObject()->Draw(false);
					b = false;
					break;
				case key_down:
					bright = _cp->SetDisplayBrightness(-1);
					sprintf(_pBrightTxt, "%d", bright/28);
					_pBrightness->setLength(252, bright);
					this->GetMainObject()->Draw(false);
					b = false;
					break;
				default:
					printf("---key :%d\n", event->_event);
					break;
			}
			break;
	}
	if(b)
		b = inherited::OnEvent(event);
	return b;
}


/*******************************************************************************
*   CMicVolumeWnd
*   
*/
ui::CMicVolumeWnd::CMicVolumeWnd
	(CWndManager *pMgr
	, CCameraProperties* cp
	):inherited("BitRecWnd", pMgr)
	,_cp(cp)
{
	_pPannel = new CPanel(NULL, this, CPoint(0, 0), CSize(128, 64), 10);
	this->SetMainObject(_pPannel);
	_pPannel->SetStyle(BSL_Bit_Black_Pannel, BSL_Bit_Black_Pannel);
	
	_pSymbleTxt = new CBitAudioSybl(_pPannel, CPoint(0, 0), CSize(128, 24),true, 
		_cp);
	_pSymbleTxt->SetStyle
		(BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small
		,BSL_Bit_Black_Pannel
		,FSL_Bit_White_Text_small);
	_pSymbleTxt->SetHAlign(CENTER);
	_pSymbleTxt->SetVAlign(TOP);
	_pSymbleTxt->AdjustVertical(0);
	_pSymbleTxt->SetDisabled();

	//_pBrightBmp = new CBmp(WndResourceDir,"icon_brightness.bmp");  // brightness
	//_pSymbleTxt->SetCurrentSymbleBmp(_pBrightBmp);
	_pSymbleTxt->SetText((UINT8*)_pVolumeTxt);

	_pRow = new CBlock(_pPannel, CPoint(2, 24), CSize(124, 2), BSL_Bit_White_Pannel);
	_pRow->SetDisabled();

	_pVolume = new CPosiBarSegH
		(_pPannel
		,CPoint(13, 36)
		,CSize(102, 12)
		,CPoint(2, 2)
		,CSize(98, 8)
		,9
		,1
		);
	_pVolume->SetStyle
		(BSL_Bit_White_border,
		BSL_Bit_White_Pannel
		);
	_pVolume->SetPosi(0);
	_pVolume->setLength(256, 128);
	
}

ui::CMicVolumeWnd::~CMicVolumeWnd()
{
	delete _pPannel;
}

void ui::CMicVolumeWnd::willHide()
{
	_cp->SetMicVolume(_volume);
}

void ui::CMicVolumeWnd::willShow()
{
	char tmp[256];
	agcmd_property_get(audioGainPropertyName, tmp, "NA");
	if(strcmp(tmp, "NA")==0)
	{
		_volume = defaultMicGain;
	}
	else
		_volume = atoi(tmp);
	updateVolumeInfor();
}

bool ui::CMicVolumeWnd::OnEvent(CEvent *event)
{
	bool b = true;
	int bright = 0;
	switch(event->_type)
	{
		case eventType_key:
			switch(event->_event)
			{
				case key_right:
					this->Close();
					b = false;
					break;
				case key_ok:
					this->Close();
					b = false;
					break;
				case key_up:
					//bright = _cp->SetDisplayBrightness(1);
					if(_volume == 0)
						_volume = 1;
					else
						_volume += 1;
					if(_volume > 9)
					{
						_volume = 9;
					}
					updateVolumeInfor();
					this->GetMainObject()->Draw(false);
					b = false;
					break;
				case key_down:
					_volume -= 1;
					if(_volume <= 0)
					{
						_volume = 0;
					}
					updateVolumeInfor();
					this->GetMainObject()->Draw(false);
					b = false;
					break;
				default:
					printf("---key :%d\n", event->_event);
					break;
			}
			break;
	}
	if(b)
		b = inherited::OnEvent(event);
	return b;
}

void ui::CMicVolumeWnd::updateVolumeInfor()
{
	if(_volume == 0)
	{
		sprintf(_pVolumeTxt, "Mute");
		_pVolume->setLength(252, 0);
	}
	else
	{
		sprintf(_pVolumeTxt, "%d", _volume);
		_pVolume->setLength(252, 28*_volume);
	}
}

