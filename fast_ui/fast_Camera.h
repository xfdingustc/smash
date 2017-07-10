/*******************************************************************************
**       fast_Camera.h
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
#ifndef __INFast_Camera_h
#define __INFast_Camera_h

#include "linux.h"
#include <linux/input.h>
#include <linux/fb.h>
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
#include "CLanguage.h"
#include "Control.h"
#include "Style.h"
#include "Wnd.h"
#include "App.h"
#include "Camera.h"
#include "FontLibApi.h"
#include "CKeyEventProcessThread.h"
#include "CLanguage.h"
#include "class_camera_property.h"
#include "class_TCPService.h"
#include "CCameraBitUI.h"
#include "class_gsensor_thread.h"

namespace ui{

class CTouchThread : public CThread
{
public:
	enum TouchState
	{
		TouchState_Null = 0,
		TouchState_Press,
		TouchState_Pressed,		
	};
	
	CTouchThread(void *p);
	~CTouchThread();
protected:
	virtual void main(void *);
	void TouchEventProcess(input_event* pE);
	void Calibrate();
private:
	int				_touchDev;

	TouchState		_touchState;
	int				_curX;
	int				_curY;
	int				_screenX;
	int				_screenY;
	
	CAppl			*_app;

	CPoint			_cali_TopLeft;
	CPoint			_cali_TopRight;
	CPoint			_cali_BottomLeft;
	CPoint			_cali_BottomRight;
	int				_screenWi;
	int				_screenHi;
};

/*
class CKeyThread : public CThread
{
public:
	CKeyThread(void *p);
	~CKeyThread();
protected:
	virtual void main(void *);
private:
	int	_keyDev;	
};*/

class CBitCanvasService;

class CFastCamera : public KeyEventProcessor, public PropertiesObserver, public ShakeObserver
{
public:
	CFastCamera(::CCameraProperties* pp);
	~CFastCamera();
	static void FB565DrawHandler();
	static void	BitCanvasDrawHandler();
	void Start();
	void Stop();

	virtual bool event_process(struct input_event* ev);
	virtual void OnPropertieChange(void* p, PropertyChangeType type);
	virtual void OnShake();
	bool SetOLEDBrightness(unsigned char v);

private:
	bool OpenFb();
	static void poweroff(void*);

private:
	
	int				_keyEvent;
	int				_touchEvent;	
	
	CameraWnd		*_pCamWnd;
	CTouchThread	*_pTouchInputThread;
	//CKeyThread		*_pKeyInputThread;

	CWnd			*_currentWnd;

	//CLanguageLoader	*_pLanguageLoader;

	static int 			_fb;
	static char*		_fbm;
	UINT32 			_screensize;
	static struct fb_var_screeninfo 	_vinfo;
	struct fb_fix_screeninfo 	_finfo;
	struct timeval _last_sensor_key_time;
	int    _last_sensor_event_value;
	struct timeval _sensor_key_time;

	struct timeval _press_power_key_time;

	CCanvas* _pCanvas565;

	
	CBitCanvas *_pCanvasBit;
	CWndManager* _pBitWndManager;
	CBitRecWnd* _pBitWnd;
	
	static unsigned char *bitCavasMem;//128*64 /8
	static CBitCanvasService *_pCanvasService;

	bool _bShakeProcess;
	bool _bShakeEvent;

	static bool _bWaitTimerPoweroff;
	static bool _bPoweroffPress;
	static int	_poweroffcount;
	static CMutex* _lockForPoweroff;

};

//CBitCanvasService
class CBitCanvasService : public StandardTcpClient
{
public:
	CBitCanvasService();
	~CBitCanvasService();
private:
	
};

}
#endif
