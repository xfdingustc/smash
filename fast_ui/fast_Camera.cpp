/*******************************************************************************
**       fast_Camera.cpp
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a  7-14-2011 linnsong CREATE AND MODIFY
**
*******************************************************************************/

#include <netinet/in.h>

#include "fast_Camera.h"
#include "agbox.h"
#include "GobalMacro.h"
#define ui_standalong 0
//namespace ui{

const static char* fontFile = "/usr/local/share/ui/arialuni.ttf";
//static char* fontFile = "/usr/local/bin/ui/SIMYOU.TTF";
const static char* languageTxtDir = "/usr/local/share/ui/";
const static char* ControlResourceDir = "/usr/local/share/ui/";

ui::CTouchThread::CTouchThread
	(
		void *p
	) : CThread("touchInputThread",CThread::NORMAL , 0, p)
	,_app((CAppl*)p)
{
	//_touchDev = open("/dev/input/event2", O_RDONLY);
	_touchDev = open("/dev/mice", O_RDONLY);
	if(_touchDev == -1)
	{
		printf("touch dev open failed");			
	}
	_screenWi = 320;
	_screenHi = 240;
	_cali_TopLeft.x = 0;
	_cali_TopLeft.y = 0;
	_cali_BottomRight.x = 320;
	_cali_BottomRight.y = 240;
}

ui::CTouchThread::~CTouchThread()
{
	if(_touchDev > 0)
	{
		close(_touchDev);
	}
}

void ui::CTouchThread::main(void *p)
{
	input_event event;
	_touchState = TouchState_Null;
	while(1)
	{
		if(read(_touchDev, &event, sizeof(event)) > 0)
		{
			TouchEventProcess(&event);
		}
		else
		{
		 	sleep(10);
			printf("read touch error \n");
		}
	}	
}

void ui::CTouchThread::TouchEventProcess(input_event* pE)
{
	switch(pE->type)
	{	
		case  EV_ABS:
			switch(pE->code)
			{
				case ABS_PRESSURE:
					if(pE->value > 0)
						_touchState = TouchState_Press;
					else
					{
						_touchState = TouchState_Null;
					}
					break;
				case ABS_X:
					if(_touchState != TouchState_Null)
						_curX = pE->value;
					break;
				case ABS_Y:
					if(_touchState != TouchState_Null)
						_curY = pE->value;
					break;
				default:
					break;
			}
			break;
		case  EV_SYN:
			switch(pE->code)
			{
				case SYN_MT_REPORT:
					//send msg;
					Calibrate();
					switch(_touchState)
					{
						case TouchState_Null:
							{
								_app->PushEvent(eventType_touch, TouchEvent_release, _screenX, _screenY);
							}
							break;
						case TouchState_Press:
							{
								_app->PushEvent(eventType_touch, TouchEvent_press, _screenX, _screenY);
								_touchState = TouchState_Pressed;
							}
							break;
						case TouchState_Pressed:
							{
								_app->PushEvent(eventType_touch, TouchEvent_overon, _screenX, _screenY);
							}
							break;
					}					
					break;
				case SYN_REPORT:
					break;					
			}			
			break;
	}
}
void ui::CTouchThread::Calibrate()
{
	_screenX = _screenWi * (_curX - _cali_TopLeft.x) / (_cali_BottomRight.x -_cali_TopLeft.x);
	_screenY = _screenHi * (_curY - _cali_TopLeft.y) / (_cali_BottomRight.y -_cali_TopLeft.y);
}


int ui::CFastCamera::_fb= -1;
char* ui::CFastCamera::_fbm = 0;
fb_var_screeninfo 	ui::CFastCamera::_vinfo;
static char *_fbbbb;//[1024];
void ui::CFastCamera::FB565DrawHandler()
{
	// notice FB to update here
	ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
	//printf("--fb infor after pan : \n\tYoffset:%d\n\tXoffset:%d\n",  _vinfo.yoffset, _vinfo.xoffset);
}

void ui::CFastCamera::BitCanvasDrawHandler()
{
	if(_pCanvasService)
	{
		if(!_pCanvasService->isConnected())
		{
			_pCanvasService->Connect("10.0.1.10", 30004);
		}
		if(_pCanvasService->isConnected())
		{
			_pCanvasService->Send(_fbm, 1024);
		}
	}
#if 1
	if(_fb >0)
	{

		unsigned char mmm[8];
		unsigned char ppp;
		unsigned char* tt = (unsigned char*)_fbm;
		unsigned char *p;
		unsigned char ttt;
		//m_rcvLocker.Take();
		//memcpy(_pBitTransBuffer,_pBitBuffer,1024);
		//m_rcvLocker.Give();

		for(int i = 0; i < 64 ; i ++)
		{
			for(int j = 0; j < 16; j++)
			{	
				tt[i*16 + j] = _fbbbb[((i / 8) * 8 + (j%8))*16 + ((i%8)*2 + j/8)];
			}
		}
		
		
		for(int i = 0; i < 1024; i += 8)
		{
			p = tt + i;
			memcpy(mmm, p, 8);
			ttt = 0x80;
			for(int j = 0; j < 8; j++)
			{
				ppp = 0x80;
				for(int k = 7; k >= 0; k--)
				{
					if(mmm[k]&ttt)
					{
						*(p + j) |= ppp;
					}
					else
					{
						*(p + j) &= ~ppp;
					}
					ppp = ppp >> 1;
				}
				ttt = ttt >> 1;
			}
		}
		//unsigned short tmp = 0x1234;
		//_serialObj.SendData((const char*)&tmp, 2);
		//_serialObj.SendData((const char*)tt,1024);

	}
	//printf("--- fb pan\n");
#endif
	if(_fb >0)
	 ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
}
bool ui::CFastCamera::_bWaitTimerPoweroff = false;
bool ui::CFastCamera::_bPoweroffPress = false;
int ui::CFastCamera::_poweroffcount = 5;
const static char* poweroffTimer = "poweroff Timer";

CMutex* ui::CFastCamera::_lockForPoweroff = NULL;
ui::CBitCanvasService* ui::CFastCamera::_pCanvasService = NULL;
ui::CFastCamera::CFastCamera
	(::CCameraProperties* pp)
	:KeyEventProcessor(NULL)
	,_bShakeProcess(false)
	,_bShakeEvent(false)
{
	//if(OpenFb())
	_lockForPoweroff = new CMutex("power off");
	OpenFb();
	{
		pp->SetObserver(this);
		CFontLibApi::Create(fontFile);
		CStyle::Create();
		char tmp[256];
		memset(tmp, 0, sizeof(tmp));
		agcmd_property_get("system.ui.language", tmp, "English");
		CLanguageLoader::Create(languageTxtDir, tmp);
		CMenu::LoadSymbles(ControlResourceDir);
		CPanel::SetStyle(BSL_PANEL_NORMAL, BSL_PANEL_NORMAL);	
		CBtn::SetBtnStyle(BSL_BUTTON_NORMAL, FSL_BUTTON_NORMAL, BSL_BUTTON_HILIGHT, FSL_BUTTON_HILIGHT);
#ifdef RGB656UI
		_pCanvas565 = new CCanvas
			((COLOR*)_fbm + _vinfo.yoffset * _finfo.line_length /sizeof(COLOR)
			, FB565DrawHandler
			,_vinfo.xres
			,_vinfo.yres
			,false);
		CWndManager::Create(_pCanvas565);
		_pCamWnd = new CameraWnd(pp);
#endif
#ifdef  BitUI
		
		_fbbbb = new char[1024];
		
		_pCanvasBit = new CBitCanvas((UINT8*)_fbbbb, BitCanvasDrawHandler, 128, 64);//_fbm
		
		//_pBitWndManager = new CWndManager(_pCanvasBit);
		CWndManager::Create(_pCanvasBit);
		_pBitWnd = new CBitRecWnd(CWndManager::getInstance(), pp);
		_pCanvasBit->FillAll(0);
		/*int y = 36, x = 15, w = 10;
		for(int i = 0; i  < 4; i++)
		{
			_pCanvasBit->DrawRow(CPoint(x, y), x + w, true);
			x += 11;
		}
		_pCanvasBit->Update();*/
		//_pCanvasService = new CBitCanvasService();
		//_pCanvasService->Go();
		//_pCanvasService->Connect("10.0.1.10", 30004);
		//CameraAppl::Create(_pBitWndManager);
		CTimerThread::GetInstance()->ApplyTimer
					(10, CFastCamera::poweroff, this, true, poweroffTimer);
		
#endif
		CameraAppl::Create(CWndManager::getInstance());

		EventProcessThread::getEPTObject()->AddProcessor(this);
		_last_sensor_key_time.tv_sec = 0;
		_last_sensor_key_time.tv_usec = 0;
		_last_sensor_event_value = agsys_input_read_switch(SW_ROTATE_LOCK);
		agcmd_property_set("system.input.key_mute_diff", "10000");

		
	}
	//else
	//{
	//	printf("_fb open failed!! %d \n", _fb);	
	//}
}


ui::CFastCamera::~CFastCamera()
{
	//if(_fb > 0)
	{
		//printf("--RemoveProcessor\n");
		EventProcessThread::getEPTObject()->RemoveProcessor(this);
#ifdef  BitUI
		//printf("--CancelTimer\n");
		CTimerThread::GetInstance()->CancelTimer(poweroffTimer);
		CWndManager::Destroy();	
		delete _pCanvasBit;
		delete[] _fbbbb;

#endif	

#ifdef  RGB656UI
		CWndManager::Destroy();	
		delete _pCanvas565;	
#endif
		CameraAppl::Destroy();
		CMenu::DeleteSymbles();
		CLanguageLoader::Destroy();			
		CFontLibApi::Destroy();
		CStyle::Destroy();

		munmap(_fbm, _screensize);
		if(_fb > 0)
			close(_fb);
	}
	delete _lockForPoweroff;
}

bool ui::CFastCamera::OpenFb()
{
	_fb = open("/dev/fb0", O_RDWR);
	if (!_fb) 
	{
		printf("Error: cannot open framebuffer device.\n");
	}
	else
	{
       printf("The framebuffer device was opened successfully.\n");
	}
	if(_fb > 0)
	{
		// Get fixed screen information
		if (ioctl(_fb, FBIOGET_FSCREENINFO, &_finfo)) 
		{
			printf("Error reading fixed information.\n");
		}
		
		if (ioctl(_fb, FBIOGET_VSCREENINFO, &_vinfo))
		{
			printf("Error reading variable information.\n");
		}
		//printf("vinfo.xoffset : %d,  vinfo.yoffset : %d\n", _vinfo.xoffset,  _vinfo.yoffset);
		_screensize = _vinfo.xres * _vinfo.yres * _vinfo.bits_per_pixel / 8;
		_fbm = (char *)mmap(0, _finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, _fb, 0);
		memset(_fbm, 0xff, _screensize);
		ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
		printf("open fb : %d, %d, %d\n", _vinfo.xres, _vinfo.yres, _screensize);
		return true;
	}
	else
		return false;
}

bool ui::CFastCamera::SetOLEDBrightness(unsigned char v)
{
	if(_fb)
	{
//		ioctl(_fb, AGB_SSD1308_IOCTL_SET_CONTRAST, &v);
	}
}

void ui::CFastCamera::Start()
{
#ifdef  RGB656UI
	_pCamWnd->GetManager()->GetCanvas()->FillAll(0);
	_pCamWnd->showDefaultWnd();
#endif
	//printf("---_pBitWnd: 0x%x\n", _pBitWnd);
	
#ifdef  BitUI
	//printf("---_pBitWnd: 0x%x\n", _pBitWnd);
	_pBitWnd->Show(false);
	
#endif
}

void ui::CFastCamera::Stop()
{
#ifdef  RGB656UI
	_pCamWnd->GetManager()->GetCanvas()->FillAll(0);
#endif

#ifdef  BitUI
	if(_fb > 0)
	{
		//memset(_fbm, 0, 1024);
		//_pCanvasBit->FillAll(1);
		//ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
	}
#endif
}

void ui::CFastCamera::OnPropertieChange(void* p, PropertyChangeType type)
{
	if(CameraAppl::getInstance() != NULL)
		CameraAppl::getInstance()->PushEvent(eventType_internal, InternalEvent_app_state_update, type, 0);
}

void ui::CFastCamera::OnShake()
{
	if(_bShakeProcess)
	{
		printf("CFastCamera::OnShake()\n");
		if(CameraAppl::getInstance() != NULL)
		{
			CameraAppl::getInstance()->PushEvent(eventType_Gesnsor, GsensorShake, 0, 0);
			_bShakeEvent = true;
		}
	}
}


void ui::CFastCamera::poweroff(void* p)
{
	//_lockForPoweroff->Take();
	if(_bPoweroffPress)
	{
		_poweroffcount --;
		if(_poweroffcount <= 0)
		{
			//printf("-- key power off\n");
			//_poweroffcount = 5;
			const char *tmp_args[8];
			tmp_args[0] = "agsh";
			tmp_args[1] = "poweroff";
			tmp_args[2] = NULL;
			agbox_sh(2, (const char *const *)tmp_args);
		}
		else if(_poweroffcount <= 3)
		{
			//push power off msg
			CameraAppl::getInstance()->PushEvent(eventType_key, key_poweroff_prepare, 0, 0);
		}
	}
	//_bWaitTimerPoweroff = false;
	//_lockForPoweroff->Give();
}

#ifdef  BitUI

bool ui::CFastCamera::event_process
	(struct input_event* ev)
{
	int event;
	if((ev->type == 1))//key
	{
		if(ev->value == 0) // release
		{
			//printf("--- ev->code : %d\n", ev->code);
			event = key_unknow;	
			switch( ev->code)
			{
				case 108:
					event= key_down;	
					break;
				case 116:
					_bShakeProcess = false;
					if(!_bShakeEvent)
//#ifdef DiabloBoardVersion2
//						event = key_ok;
//#else
						event = key_right;
//#endif
					_bShakeEvent = false;
					break;
				case 200:
				case 201:
				case 167:
				case 212:
#ifdef DiabloBoardVersion2
						//if(ev->time.tv_sec - _press_power_key_time.tv_sec > 3)
						//{
							//event = key_power;
							// power off;
							//printf("+++-- key release\n");
							//CTimerThread::GetInstance()->CancelTimer(poweroffTimer);
						//}
						//else
						_bPoweroffPress = false;
						_poweroffcount = 5;
		
						//CameraAppl::getInstance()->PushEvent(eventType_key, key_poweroff_cancel, 0, 0);
						if(_poweroffcount > 3)
							event = key_ok; //key_right;
#else
						event = key_ok;
#endif
					break;
				case 128:
					event = key_test;
					break;
				case 113:
					_bShakeProcess = true;
					break;
				case 103:
					event= key_up;
					break;
				default:				
					break;
			}
			if((CameraAppl::getInstance() != NULL)&&(event != key_unknow))
			{
				CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
			}
		}
		else
		{
			if(ev->code == 108)
			{
				_bShakeProcess = true;
			}
#ifdef DiabloBoardVersion2
			else if(ev->code == 212)
			{
				//_press_power_key_time = ev->time;
				//_lockForPoweroff->Take();
				/*if(_bWaitTimerPoweroff)
				{
					CTimerThread::GetInstance()->CancelTimer(poweroffTimer);
				}
				_bWaitTimerPoweroff = true;
				CTimerThread::GetInstance()->ApplyTimer
					(40, CFastCamera::poweroff, this, false, poweroffTimer);*/
				_bPoweroffPress = true;
				//_lockForPoweroff->Give();
			}
#endif
		}
	}
	else if((ev->type == 5))
	{
		switch( ev->code)
		{
			case 12: //HDR switch
			if (((ev->time.tv_sec - _last_sensor_key_time.tv_sec) < 5)
				||(_last_sensor_event_value == ev->value)){
				printf("skip hdr switch \n");
			}
			else
			{
				_last_sensor_key_time = ev->time;
				_last_sensor_event_value = ev->value;
				event = key_sensorModeSwitch;
				CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
			}
			break;
			
		}
	}
	return false;
}
#else
bool ui::CFastCamera::event_process
	(struct input_event* ev)
{
	//printf("--CFastCamera key, code:%d, value : %d \n", ev->code, ev->value);
	//bool rt = true;
	int event;
	if((ev->type == 1))//key
	{
		if(ev->value == 1) // press
		{
			//printf("--- ev->code : %d\n", ev->code);
			switch( ev->code)
			{
				case 103:
					event= key_up;	
					break;
				case 108:
					event = key_down;				
					break;
				case 105:
					event = key_left;				
					break;
				case 106:
					event = key_right;			
					break;
				case 200:
				case 201:
				case 28:
				case 167:
					event = key_ok;
					break;
				case 128:
					event = key_test;
					break;
				case 113:
					_bShakeProcess = true;
					break;
				case 116:
					
					break;
				default:
					event = key_unknow;				
					break;
			}
			if(CameraAppl::getInstance() != NULL)
			{
				CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
			}
		}
		else
		{
			if(ev->code == 113)
			{
				_bShakeProcess = false;
			}
		}
	}
	else if((ev->type == 5))
	{
		switch( ev->code)
		{
			case 12: //HDR switch
			if (((ev->time.tv_sec - _last_sensor_key_time.tv_sec) < 5)
				||(_last_sensor_event_value == ev->value)){
				printf("skip hdr switch \n");
			}
			else
			{
				_last_sensor_key_time = ev->time;
				_last_sensor_event_value = ev->value;
				event = key_sensorModeSwitch;
				CameraAppl::getInstance()->PushEvent(eventType_key, event, 0, 0);
			}
			break;
			
		}
	}
	return false;
}

#endif

#if ui_standalong
int main(int argc, char **argv)
{
	bool bBkgrd = false;
	ui::CFastCamera *pFC = new ui::CFastCamera(NULL);
	pFC->Start();

	if(argc >= 1)
	{
		for(int j = 1; j < argc; j++)
		{
			if(strcmp(argv[j], "-bkgrd") == 0)
			{	
				bBkgrd = true;
			}
		}
	}

	if(bBkgrd)
	{
		CSemaphore sem(0, 1, "ui main sem");
		sem.Take(-1);
	}
	else
	{	
		char ch[100];
		char tmp;
		int i;
		while(1)
		{
			i = 0;
			memset(ch, 0, sizeof(ch));
			do
			{
				tmp = getchar();
				ch[i] = tmp;
				i++;
			}while((tmp!= '\n')&&(i < 10));		

			if(0 == strcmp(ch, "exit\n"))	
			{
				break;
			}
		}
	}
	pFC->Stop();
	delete pFC;
}
#endif


ui::CBitCanvasService::CBitCanvasService
	():StandardTcpClient(1024, "bitcanvs")
{
}

ui::CBitCanvasService::~CBitCanvasService()
{
}


/*
ui::CKeyThread::CKeyThread(
		void *p
	) : CThread("Key Input Thread",CThread::NORMAL , 0, p)
{
	_keyDev = open("/dev/input/event0", O_RDONLY);
	_keyDev = open("/dev/event1", O_RDONLY);
	if(_keyDev == -1)
	{
		printf("touch dev open failed");			
	}	
}

ui::CKeyThread::~CKeyThread()
{
	if(_keyDev > 0)
	{
		close(_keyDev);
	}
}

void ui::CKeyThread::main(void *p)
{
	CFastCamera* pFC = (CFastCamera*)p;
	char bb[256];
	memset(bb, 0, 256);
	while(1)
	{
		if(read(_keyDev,bb, 23) > 0)
		{
			printf("read key ok :%s \n", bb);
			//pars and send event;
		}
		else
			printf("read key error \n");
	}	
}*/
