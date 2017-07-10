/*****************************************************************************
 * MotorController.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2013, linnsong.
 *
 *
 *****************************************************************************/

#ifndef __H_motor_controller__
#define __H_motor_controller__

#include "GobalMacro.h"
#include "ClinuxThread.h"
#include "class_TCPService.h"
#include "class_ipc_rec.h"

#define seccode (0x010203ff)

struct MotoCmdData
{
	int scode;
	int cmd;
	int para1;
	int para2;
	int para3;
};

enum MotorCmd
{
	MotorCmd_step = 0,
	MotorCmd_angle,
	MotorCmd_SetSpeed,
	MotorCmd_autoRotate,
	MotorCmd_autoRotate_absolute,
	MotorCmd_autoRotate_RecWhenStop,
	MotorCmd_scriptExecute,
	MotorCmd_Rec,
	MotorCmd_Sleep,
	MotorCmd_updown_up,
	MotorCmd_updown_down,
	MotorCmd_updown_stop,
	MotorCmd_updown_irSwitch,
	MotorCmd_stop,
};

enum MotorSpeed
{
	MotorSpeed_full = 1,
	MotorSpeed_half,
	MotorSpeed_1_4,
	MotorSpeed_1_8,
};

#define motor_cmd_buffer_size 1024
#define motor_TCP_PORT 10186
#define motor_max_connection 24

class MotorCtrl;
class MotoController;
class MotoControllerUart;

class MotorControlService : public TCPRcvDelegate, public CThread
{
public:
	static void Create();
	static void Destroy();
	static MotorControlService* getInstance(){return _pInstance;};
	
	virtual void OnRecievedBuffer(char* buffer, int len, int fd);
	virtual void onFdNew(int fd);
	virtual void onFdClose(int fd);

	virtual void main(void *);

	void SetSpeed(int speed);
	
private:
	StandardTCPService *_pTCPService;
	MotoController*	_pController;

	CSemaphore		*_pSemphore;
	int				_remainSteps;
	int		_MotorSpeed;

	CSemaphore 		*_pTimerWait;
	int				_cmd;
	int				_angle;
	int				_autoRotateStopNum;
	int				_autoRotateStopSeconds;
	int				_autoRotateDirection;
	int				_executedAngle;
	int 			_sign;
	int				_returncount;
	int				_stepPerRound;

	CFile			*_pScriptFile;
	bool			_inlineExecute;
	char			*_lineBuffer;
	int				_offset;
	int				_scriptCmd;
	int				_rotateDirection;
	int				_num;
	bool 			_bAbsolute;
	
	bool			_MuteWhenRotate;
	bool 			_bMute;
	int				_updownCmd;
	bool			_irswitch;

	MotoControllerUart *_uartSteperMotor;
	
	
private:
	static MotorControlService* _pInstance;
	MotorControlService();
	~MotorControlService();

	bool ParseCmd();
	bool ReadLine();
	void Mute();
	void Unmute();
};

class MotorCtrl
{
public:
	MotorCtrl(){};
	virtual ~MotorCtrl(){};
	virtual void RotateAbsolutley(int angle) = 0;
	virtual void RotateRelatively(int angle) = 0;
	virtual void Step(int steps, int speed) = 0;

	virtual void UpdownMotion(int cmd) = 0;
	virtual void IRSwitchMotion(bool b) = 0;
};

#define motoNode "/proc/agb_irm"
#define backlash 5
#define stepsPerROUND 64000
class MotoController :public MotorCtrl
{
public:
	MotoController(const char* node);
	virtual ~MotoController();

	virtual void RotateAbsolutley(int angle);
	virtual void RotateRelatively(int angle);
	virtual void Step(int steps, int speed);

	virtual void UpdownMotion(int cmd);
	virtual void IRSwitchMotion(bool b);
	
private:
	int _lastMovementAmount;
	int _fd;
	bool _bMuteWhenRotate;
	bool _bmute;
};


class MotoControllerUart : public MotorCtrl, public CThread
{
public:
	MotoControllerUart(char* node);
	~MotoControllerUart();

	virtual void RotateAbsolutley(int angle);
	virtual void RotateRelatively(int angle);
	virtual void Step(int steps, int speed);
	virtual void Stop();

	virtual void UpdownMotion(int cmd);
	virtual void IRSwitchMotion(bool b);
	void RotateRelatively(int angle, int speed);

	virtual void main(void *);

	void waitRotating();
	void Prepare();
	void ResetPosition();
	
	int writeToUart(char* tmp, int len);
private:
	int 			uart_fd;
	CSemaphore		*_pSemphoreWaitPrepare;
	CSemaphore		*_pSemphoreWaitCmd;
	char			_readBuffer[64];
	bool			_bReady;
	bool 			_bFinish;
private:
	int uart_config(int fd, int speed, int flow);
	
	int MoveCmd
		(unsigned char motorIndex
		, unsigned char  direction
		, unsigned char  speed
		, int angle
		, unsigned char reset);
};

#endif
