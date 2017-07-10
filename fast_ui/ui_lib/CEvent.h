#ifndef __INCEvent_h
#define __INCEvent_h
//#ifdef WIN32
namespace ui{
//#endif

enum evenType
{
	eventType_key=0,
	eventType_touch,
	eventType_internal,
	eventType_external,
	eventType_Gesnsor,
};

enum TouchEvent
{
	TouchEvent_press=0,
	TouchEvent_overon,
	TouchEvent_release,
};

enum InternalEvent
{
	InternalEvent_wnd_button_click = 0,
	InternalEvent_app_timer_update,
	InternalEvent_app_state_update,
	InternalEvent_wnd_force_close,
};

enum KeyEvent
{
	key_up = 0,
	key_down,
	key_left,
	key_right,
	key_ok,
	key_sensorModeSwitch,
	key_test,
	key_unknow,
	key_poweroff_prepare,
	key_poweroff_cancel,
	key_poweroff,
	key_number,
};

enum GsensorEvent
{
	GsensorShake = 0,
};

class CEvent
{
public:
	CEvent(){};
	~CEvent(){};	

public:
	UINT16	_type;
	UINT16	_event; 
	UINT32	_paload;
	UINT32	_paload1;
	bool		_bProcessed;
};

//#ifdef WIN32
}
//#endif

#endif