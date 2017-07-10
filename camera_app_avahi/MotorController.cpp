/*****************************************************************************
 * MotorController.cpp:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2013, linnsong.
 *
 *
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <math.h>
#include "MotorController.h"
#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"

/***************************************************************
MotoController::MotoController()
*/
MotoController::MotoController
	(const char* node
	):_lastMovementAmount(0)
{
	_fd = open(motoNode, O_RDWR);
	if(_fd < 0)
	{
		CAMERALOG("open %s failed", motoNode);
	}
}

MotoController::~MotoController()
{
	if(_fd > 0)
	{
		close(_fd);
	}
}

void MotoController::RotateAbsolutley(int angle)
{
	char tmp[32];
	memset(tmp, 0, sizeof(tmp));
	if(angle == 0)
	{
		snprintf(tmp, 32, "s0 ");
		write(_fd, tmp, sizeof(tmp));
	}
	else
	{
		snprintf(tmp, 32, "A%d ", angle);
		write(_fd, tmp, sizeof(tmp));
	}
}

void MotoController::RotateRelatively(int angle)
{
	char tmp[32];
	bzero(tmp, sizeof(tmp));
	snprintf(tmp, 32, "a%d ", angle);
	write(_fd, tmp, sizeof(tmp));
}

void MotoController::Step(int steps, int speed)
{
	if(_lastMovementAmount*steps < 0)
	{
		if(steps > 0)
			steps += backlash;
		else
			steps -= backlash;
	}
	_lastMovementAmount = steps;
	char tmp[32];
	bzero(tmp, sizeof(tmp));
	snprintf(tmp, 32, "s%d ", steps);
	//CAMERALOG("---%s----", tmp);
	write(_fd, tmp, sizeof(tmp));
}

void MotoController::UpdownMotion(int cmd)
{
	if(cmd == 0)
	{
		CAMERALOG("-- UpdownMotion up");
		agsys_gpio_output(61, 1);
		agsys_gpio_output(38, 1);
	}
	else if(cmd == 1)
	{
		agsys_gpio_output(61, 0);
		agsys_gpio_output(38, 1);
	}
	else if(cmd == -1)
	{
		agsys_gpio_output(61, 1);
		agsys_gpio_output(38, 0);
	}
}

void MotoController::IRSwitchMotion(bool b)
{
	if(b)
	{
		agsys_gpio_output(29, 0);
		agsys_gpio_output(32, 1);
		sleep(1);
		agsys_gpio_output(29, 1);
		agsys_gpio_output(32, 1);
	}else
	{
		agsys_gpio_output(29, 1);
		agsys_gpio_output(32, 0);
		sleep(1);
		agsys_gpio_output(29, 1);
		agsys_gpio_output(32, 1);
	}
}

/***************************************************************
MotoControllerUart
*/
#define UARTSPEED "B9600"
MotoControllerUart::MotoControllerUart
	(char* node
	):CThread("MotorControlUart", CThread::NORMAL, 0, NULL)
	,uart_fd(-1)
	,_bReady(false)
	,_bFinish(true)
{
	int speed = B9600;
	int flowctrl = 0;
	uart_fd = open(node, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (uart_fd < 0) {
		CAMERALOG("Can't open serial port");
	}
	uart_config(uart_fd, speed, flowctrl);
	_pSemphoreWaitPrepare = new CSemaphore(0, 1, "stepmotorPrepare");
	_pSemphoreWaitCmd =  new CSemaphore(0, 1, "stepmotorCmd");
}

MotoControllerUart::~MotoControllerUart()
{
	if(uart_fd > 0)
	{	
		close(uart_fd);
		uart_fd = -1;
	}
	delete _pSemphoreWaitPrepare;
}

void MotoControllerUart::Prepare()
{
	//if(!_bReady)
	{
		char t = 0x23;
		writeToUart(&t, 1);
		_pSemphoreWaitPrepare->Take(-1);
	}
}

void MotoControllerUart::RotateAbsolutley(int angle)
{
	MoveCmd(0, (angle > 0)? 0 : 1, 1, abs(angle), 0);
}
void MotoControllerUart::RotateRelatively(int angle)
{
	MoveCmd(0, (angle > 0)? 0 : 1, 1, abs(angle), 0);
}
void MotoControllerUart::RotateRelatively(int angle, int speed)
{
	MoveCmd(0, (angle > 0)? 0 : 1, (speed%0x08), abs(angle), 0);
}
void MotoControllerUart::Step(int steps, int speed)
{
	//RotateRelatively(step*360/)
	//Prepare();
	int angle = 0;
	//if(steps >= 0)
		angle = steps / (3000/360) ;
	//else
	//	angle = -steps*360 / 3000;
	if(abs(angle) <= 360)
		MoveCmd(0, (steps > 0)? 1 : 0,  (speed%0x08), abs(angle), 0);
	else
		MoveCmd(0, (steps > 0)? 0 : 1,  (speed%0x08), abs(angle), 0);
	//_pSemphoreWaitCmd->Take(-1);
	CAMERALOG("step executed : %d @%d", steps, speed);
}

void MotoControllerUart::Stop()
{
	//Prepare();
	MoveCmd(0, 0, 1, 0, 0);
	//_pSemphoreWaitCmd->Take(-1);
	//CAMERALOG("stop : %d");
}

void MotoControllerUart::ResetPosition()
{
	MoveCmd(0, 0, 1, 1, 1);
	_pSemphoreWaitCmd->Take(-1);
}

void MotoControllerUart::waitRotating()
{
	_pSemphoreWaitCmd->Take(-1);
}

void MotoControllerUart::UpdownMotion(int cmd)
{


}

void MotoControllerUart::IRSwitchMotion(bool b)
{


}

void MotoControllerUart::main(void *)
{
    char buf_rd[8];
    int rval = 0;
    if (uart_fd > 0)
    {
        while (uart_fd > 0)
        {
            memset(buf_rd, 0x00, 8);
            rval = read(uart_fd, buf_rd, 1);
            if (rval < 0) {
                if (errno == EAGAIN) {
                    usleep(100000);
                    continue;
                } else {
                    break;
                }
            }
            if(buf_rd[0] == 'V')
            {
                CAMERALOG("motor rotate cmd excuting");
                _bReady = true;
                //_pSemphoreWaitPrepare->Give();
            } else if(buf_rd[0] == '*') {
                CAMERALOG("motor rotate cmd finish");
                _bFinish = true;
                _pSemphoreWaitCmd->Give();
            } else {
                CAMERALOG("return : %s", buf_rd);
            }
        }
    }
}

int MotoControllerUart::writeToUart(char* buf, int len)
{
    int rval = write(uart_fd, buf, len);
    if (rval < 0)
    {
        CAMERALOG("write error : %d", rval);
    }
    CAMERALOG("write to uart : %x", *buf);

    return 0;
}

int MotoControllerUart::MoveCmd(unsigned char motorIndex, unsigned char  direction, unsigned char  speed, int  angle, unsigned char reset)
{
    //CAMERALOG("rotate angle: %d", angle);

    unsigned char cmd[4];
    cmd[0]= '#';
    cmd[1]= '#';
    cmd[2] = 0;
    cmd[3] = 0;
    if (angle == 0)
    {
        CAMERALOG("stop cmd: %d", angle);
    } else {
        motorIndex = motorIndex << 6;
        speed = (speed&0x07) << 3;
        direction = (direction & 0x01) << 1;
        reset = (reset > 0)? 0x04 : 0x00;
        CAMERALOG("angles: %d", angle);
        if (angle > 360)
        {
            CAMERALOG("no stop rotate: %d", angle);
            cmd[2] = motorIndex | speed | reset | direction | 0x01;
            cmd[3] = 0xff;
        } else {
            cmd[2] = motorIndex | speed | reset | direction | ((abs(angle) / 256) > 0 ? 0x01 : 0x00);
            cmd[3] = abs(angle) % 256;
            CAMERALOG("rotate: %d, %d", angle, cmd[3]);
        }
    }
    for (int i = 0; i < 4; i++)
    {
        writeToUart((char*)(cmd + i), 1);
    }

    return 0;
}

int MotoControllerUart::uart_config(int fd, int speed, int flow)
{
    static struct termios ti;

    tcflush(fd, TCIOFLUSH);

    if (tcgetattr(fd, &ti) < 0) {
        CAMERALOG("Can't get port settings");
        return -1;
    }

    cfmakeraw(&ti);

    ti.c_cflag |= CLOCAL;
    if (flow) {
        ti.c_cflag |= CRTSCTS;
    } else {
        ti.c_cflag &= ~CRTSCTS;
    }

    cfsetospeed(&ti, speed);
    cfsetispeed(&ti, speed);

    if (tcsetattr(fd, TCSANOW, &ti) < 0) {
        CAMERALOG("Can't set port settings");
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    return 0;
}
	
/***************************************************************
MotorControlService::MotorControlService()
*/
#define linebufferlength 256

MotorControlService* MotorControlService::_pInstance = NULL;
void MotorControlService::Create()
{
	if(_pInstance == NULL)
	{
		_pInstance = new MotorControlService();
		_pInstance->Go();
	}
}
void MotorControlService::Destroy()
{
	if(_pInstance != NULL)
	{
		_pInstance->Stop();
		delete _pInstance;
		_pInstance = NULL;
	}
}

MotorControlService::MotorControlService
	(): CThread("MotorControl", CThread::NORMAL, 0, NULL)
	,_remainSteps(0)
	,_MotorSpeed(MotorSpeed_full)
	,_stepPerRound(stepsPerROUND)
	,_pScriptFile(NULL)
	,_bAbsolute(false)
	,_MuteWhenRotate(true)
	,_bMute(false)
{
	_lineBuffer = new char[linebufferlength];
	_pController = new MotoController(motoNode);
	_pTCPService = new StandardTCPService
		(motor_TCP_PORT 
		, motor_cmd_buffer_size
		, motor_max_connection);
	_pTCPService->SetDelegate(this);
	_pSemphore = new CSemaphore(0, 1, "motorThread");
	_pTimerWait = new CSemaphore(0, 1, "motorTimer");
	CAMERALOG("--- init MotorControlService");
	_pTCPService->Go();
#ifdef support_steper_motor
	_uartSteperMotor = new MotoControllerUart("/dev/ttyS1");
	_uartSteperMotor->Go();
	//_uartSteperMotor->Prepare();
#endif
	
}

MotorControlService::~MotorControlService()
{

#ifdef support_steper_motor
	_uartSteperMotor->Stop();
	delete _uartSteperMotor;
#endif

	_pTCPService->Stop();
	delete _pSemphore;
	delete _pController;
	delete _pTCPService;
	delete _pTimerWait;
	delete[] _lineBuffer;
}

void MotorControlService::main(void *)
{
	int steps = 60;
	int anglestep = 60;
	int c = 0;
	while(1)
	{
		_pSemphore->Take(-1);
		switch(_cmd)
		{
			case MotorCmd_step:
#ifdef support_steper_motor
				_uartSteperMotor->Step(_remainSteps, _MotorSpeed);
				_remainSteps = 0;
				//_cmd = MotorCmd_stop;
#else
				if(abs(_remainSteps) > steps)
				{
					_pController->Step((_remainSteps/abs(_remainSteps))*steps, _MotorSpeed);
					_remainSteps -= (_remainSteps/abs(_remainSteps))*steps;
					_pSemphore->Give();
				}
				else
				{
					_pController->Step(_remainSteps, _MotorSpeed);
					_remainSteps = 0;
					_cmd = MotorCmd_stop;
				}
#endif
				break;
			case MotorCmd_stop:
#ifdef support_steper_motor
				_uartSteperMotor->Stop();
#endif
				Unmute();
				_remainSteps = 0;
				c = 0;
				break;
			case MotorCmd_angle:
#ifdef support_steper_motor
				_uartSteperMotor->RotateRelatively(_angle);
				//_remainSteps = 0;
				//_cmd = MotorCmd_stop;
#else
				if(abs(_angle) > anglestep)
				{
					_pController->RotateRelatively((_angle/abs(_angle))*anglestep);
					_angle -= (_angle/abs(_angle))*anglestep;
					_pSemphore->Give();
				}
				else
				{
					_pController->RotateRelatively(_angle);
					_angle = 0;
					_cmd = MotorCmd_stop;
					}
#endif
				break;
			case MotorCmd_autoRotate:
#ifdef support_steper_motor
				Mute();
				switch(_autoRotateDirection)
				{
					case 2:
						{
							_uartSteperMotor->RotateRelatively(-_angle, _MotorSpeed);
						}
						break;
					case 0:
						if(c >= _returncount)
						{
							CAMERALOG("-- c : %d, %d", c, _returncount);
							_sign = _sign * -1;
							c = 0;
						}
						_uartSteperMotor->RotateRelatively(_sign * _angle, _MotorSpeed);
						break;
					case 1:
						_uartSteperMotor->RotateRelatively(_angle, _MotorSpeed);
						break;
				}
				_uartSteperMotor->waitRotating();
				{
					Unmute();
					_pTimerWait->Take(_autoRotateStopSeconds * 1000000);
					//_executedAngle = 0;
					c++;
				}
				_pSemphore->Give();
#else
				{
					Mute();
					int s = anglestep;
					if(_angle - _executedAngle < anglestep)
						s = _angle - _executedAngle;
					int s1 = s*9/10;
					switch(_autoRotateDirection)
					{
						case 2:
							{

								_pController->Step(-s1, _MotorSpeed);
							}
							break;
						case 0:
							if(c >= _returncount)
							{
								_sign = _sign * -1;
								c = 0;
							}
							_pController->Step(_sign * s1, _MotorSpeed);
							break;
						case 1:
							_pController->Step(s1, _MotorSpeed);
							break;
					}
					_executedAngle += s;
					if((_executedAngle) >= _angle)
					{
						Unmute();
						_pTimerWait->Take(_autoRotateStopSeconds * 1000000);
						_executedAngle = 0;
						c++;
					}
					_pSemphore->Give();
				}
#endif
				break;
			case MotorCmd_autoRotate_absolute:
				{
#ifdef support_steper_motor
					Mute();
					_uartSteperMotor->ResetPosition();
					_cmd = MotorCmd_autoRotate;
					_pSemphore->Give();
#else
					Mute();
					int r = (_angle*c) % 360;
					if(r > 180)
						r = r - 360;
					switch(_autoRotateDirection)
					{
						case 2:
							_pController->RotateAbsolutley(-r);
							break;
						case 0:
							{
								int j = c % (4 * _autoRotateStopNum);
								if(j < _autoRotateStopNum)
								{
									r = j * _angle / 4;
								}
								else if(j < 2 * _autoRotateStopNum)
								{
									r = 90 - (j * _angle / 4) % 90;
								}
								else if(j < 3 * _autoRotateStopNum)
								{
									r = - (j * _angle / 4) % 180;
								}
								else
								{
									r = - 90 + (j * _angle / 4) % 270;
								}
								CAMERALOG("rotate: %d", r);
								_pController->RotateAbsolutley(r);
							}
							break;
						case 1:
							_pController->RotateAbsolutley(r);
							break;
					}
					// unmute
					Unmute();
					_pTimerWait->Take(_autoRotateStopSeconds * 1000000);
					c++;
					_pSemphore->Give();

#endif
					//mute
					
				}
				break;
			case MotorCmd_autoRotate_RecWhenStop:
				{
					switch(_autoRotateDirection)
					{
						case 2:
							_pController->Step(-anglestep, _MotorSpeed);
							break;
						case 0:
							_pController->Step(_sign * anglestep, _MotorSpeed);
							if(c >= _returncount)
							{
								_sign = _sign * -1;
								c = 0;
							}
							break;
						case 1:
							_pController->Step(anglestep, _MotorSpeed);
							break;
					}
					_executedAngle += anglestep;
					if((_executedAngle) > _angle)
					{
						// start record
						_pTimerWait->Take(_autoRotateStopSeconds * 1000000);
						// stop record
						_executedAngle = 0;
						c++;
					}
					_pSemphore->Give();
				}
				break;
			case MotorCmd_scriptExecute:
				{
					if(_pScriptFile == NULL)
					{
						_pScriptFile = new CFile("/tmp/mmcblk0p1/cmds.txt", CFile::FILE_READ);
						if(_pScriptFile->IsValid())
						{	
							CAMERALOG("got file, start execute");
							_inlineExecute = false;
							
							_pSemphore->Give();
						}
						else
						{
							CAMERALOG("--/tmp/mmcblk0p1/cmds.txt not found!");
							delete _pScriptFile;
							_pScriptFile = NULL;
						}
					}else
					{
						if(_inlineExecute) //execute line not finish, execute,
						{
							switch(_scriptCmd)
							{
								case MotorCmd_step:
									if(abs(_remainSteps) > steps)
									{
										_pController->Step((_remainSteps/abs(_remainSteps))*steps, _MotorSpeed);
										_remainSteps -= (_remainSteps/abs(_remainSteps))*steps;
									}
									else
									{
										_pController->Step(_remainSteps, _MotorSpeed);
										_remainSteps = 0;
										_inlineExecute = false;
									}
									break;
								case MotorCmd_Sleep:
									if(_num > 0)
									{
										_pTimerWait->Take(1000000);
										_num --;
									}
									else
									{
										_inlineExecute = false;
									}
									break;
								case MotorCmd_Rec:
									if(_num > 0)
									{
										// start record
										IPC_AVF_Client::getObject()->StartRecording();
									}
									else
									{
										// stop record
										IPC_AVF_Client::getObject()->StopRecording();
									}
									_inlineExecute = false;
									break;
								case MotorCmd_stop:
									_cmd = MotorCmd_stop;
									{
										CAMERALOG("stop script execute");
										delete _pScriptFile;
										_pScriptFile = NULL;
									}
									break;
								case MotorCmd_updown_up:
									_pController->UpdownMotion(1);
									_inlineExecute =false;
									break;
								case MotorCmd_updown_down:
									_pController->UpdownMotion(-1);
									_inlineExecute =false;
									break;
								case MotorCmd_updown_stop:
									_pController->UpdownMotion(0);
									_inlineExecute =false;
									break;
								case MotorCmd_updown_irSwitch:
									_pController->IRSwitchMotion(_irswitch);
									_inlineExecute =false;
									break;
							}
							_pSemphore->Give();
						}
						else
						{
							if(ReadLine())
							{
								_pScriptFile->seek(0);
							}
							if(ParseCmd())
							{
								_inlineExecute = true;
							}
							else
							{
								CAMERALOG("invalid line, skip");
							}
							_pSemphore->Give();
						}
					}
				}
				break;
			case MotorCmd_updown_up:
				CAMERALOG("move up");
				_pController->UpdownMotion(1);
				break;
			case MotorCmd_updown_down:
				CAMERALOG("move down");
				_pController->UpdownMotion(-1);
				break;
			case MotorCmd_updown_stop:
				CAMERALOG("up down stop");
				_pController->UpdownMotion(0);
				break;
			case MotorCmd_updown_irSwitch:
				CAMERALOG("ir switch");
				_pController->IRSwitchMotion(_irswitch);
				break;
		}
	}
}

bool MotorControlService::ReadLine()
{
	_offset = 0;
	memset(_lineBuffer, 0, linebufferlength);
	int rt;
	char p;
	bool bFileEnd = false;
	while(1)
	{
		rt = _pScriptFile->readfile(&p, 1);
		if(rt <= 0)
		{
			bFileEnd = true;;
			break;
		}
		if(p == '\r')
		{
			// do nothing	
		}else if(p == '\n')
		{	
			CAMERALOG("\n");
			break;
		}
		else
		{
			_lineBuffer[_offset] = p;
			//CAMERALOG("%c", _lineBuffer[_offset]);
			_offset++;
		}	
	}
	CAMERALOG("read line :%s", _lineBuffer);
	return bFileEnd;
}

bool MotorControlService::ParseCmd()
{
	bool rt = false;
	int num;
	switch(_lineBuffer[0])
	{
		case 'd': // direction
			CAMERALOG("--set direction");
			num = atoi(_lineBuffer + 1);
			if(num <= 0)
			{
				_rotateDirection = -1;
			}
			else
			{
				_rotateDirection = 1;
			}
			break;
		case 'p': // progress steps
			_scriptCmd = MotorCmd_step;
			_remainSteps = atoi(_lineBuffer + 1)*_rotateDirection;
			rt = true;
			break;
		case 's': // sleep seconds
			_scriptCmd = MotorCmd_Sleep;
			_num = atoi(_lineBuffer + 1);
			rt = true;
			break;
		case 'r': // record
			_scriptCmd = MotorCmd_Rec;
			_num = atoi(_lineBuffer + 1);
			rt = true;
			break;
		case 'e': // record
			_scriptCmd = MotorCmd_stop;
			rt = true;
			break;
		case 'u':
			_scriptCmd = MotorCmd_updown_up;
			rt = true;
			break;
		case 'n':
			_scriptCmd = MotorCmd_updown_down;
			rt = true;
			break;
		case 't':
			_scriptCmd = MotorCmd_updown_stop;
			rt = true;
			break;
		case 'i':
			_scriptCmd = MotorCmd_updown_irSwitch;
			rt = true;
			break;
		default:
			break;
	}
	return rt;
}

void MotorControlService::SetSpeed(int speed)
{
	_MotorSpeed = speed;
}

void MotorControlService::OnRecievedBuffer
	(char* buffer, int len, int fd)
{
	MotoCmdData* cmddata = (MotoCmdData*)buffer;
	if(cmddata->scode != seccode)
		return;
	_cmd = cmddata->cmd;
	CAMERALOG("--- motor control cmd : %d, %d", _cmd, cmddata->para1);
	switch(cmddata->cmd)
	{
		case MotorCmd_step:
			{
				_remainSteps += cmddata->para1;
			}
			break;
		case MotorCmd_stop:
			//_pTimerWait->Give();
			break;
		case MotorCmd_angle:
			_angle = cmddata->para1;
			break;
		case MotorCmd_autoRotate:
			{
				_autoRotateStopNum = cmddata->para1;
				_autoRotateStopSeconds = cmddata->para2;
				_autoRotateDirection = cmddata->para3 & 0x3;
				_MuteWhenRotate = cmddata->para3 & 0x4;
				_executedAngle = 0;
				_sign = 1;
#ifdef support_steper_motor
				_angle = 360 / _autoRotateStopNum;
#else
				_angle = _stepPerRound / _autoRotateStopNum; //360
#endif
				_returncount = _autoRotateStopNum / 2;
			}
			break;
		case MotorCmd_autoRotate_absolute:
			{
				_autoRotateStopNum = cmddata->para1;
				_autoRotateStopSeconds = cmddata->para2;
				_autoRotateDirection = cmddata->para3 & 0x3;
				_MuteWhenRotate = cmddata->para3 & 0x4;
				_executedAngle = 0;
				_sign = 1;
				_angle = 360 / _autoRotateStopNum; //360
				_returncount = _autoRotateStopNum / 2;
			}
			break;
		case MotorCmd_SetSpeed:
			{
				SetSpeed(cmddata->para1);
			}
			break;
		case MotorCmd_updown_up:
			CAMERALOG("-- try move up");
			_updownCmd = 1;
			break;
		case MotorCmd_updown_down:
			_updownCmd = -1;
			break;
		case MotorCmd_updown_stop:
			_updownCmd = 0;
			break;
		case MotorCmd_updown_irSwitch:
			_irswitch = cmddata->para1;
			break;
		default:
			break;
	}
	_pSemphore->Give();
}

void MotorControlService::onFdNew(int fd)
{
	CAMERALOG("- motor control connected : %d", fd);
}

void MotorControlService::onFdClose(int fd)
{
	CAMERALOG("- motor control disconnected : %d", fd);
}


void MotorControlService::Mute()
{
    if ((_MuteWhenRotate) && (!_bMute))
    {
        CAMERALOG("--mute");
        int ret_val = 0;
        const char *tmp_args[16];

        tmp_args[0] = "/usr/sbin/agboard_helper";
        tmp_args[1] = "--audio_mute";
        tmp_args[2] = NULL;
        ret_val = agcmd_agexecvp((const char *)tmp_args[0],
            (char * const *)tmp_args);
        if (ret_val) {
            CAMERALOG("agcmd_agexecvp(agboard_helper) = %d.", ret_val);
        }
        _bMute = true;
    }
}

void MotorControlService::Unmute()
{
    if (_bMute)
    {
        CAMERALOG("--normal");
        int ret_val = 0;
        const char *tmp_args[16];

        tmp_args[0] = "/usr/sbin/agboard_helper";
        tmp_args[1] = "--audio_normal";
        tmp_args[2] = NULL;
        ret_val = agcmd_agexecvp((const char *)tmp_args[0],
            (char * const *)tmp_args);
        if (ret_val) {
            CAMERALOG("agcmd_agexecvp(agboard_helper) = %d.", ret_val);
        }
        _bMute = false;
    }
}

//void MotorControlService::OnScriptRotate(char* file)
//{
	// read line, judge direction, step
	// execute .
	// wait finish.
	// 
//
//}
