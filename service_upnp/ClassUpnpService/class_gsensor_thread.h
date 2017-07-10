/*****************************************************************************
 * class_gsensor_thread.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 -, linnsong.
 *
 *
 *****************************************************************************/
#ifndef __IN_gsensor_thread_h
#define __IN_gsensor_thread_h


#include <agobd.h>
#include "class_TCPService.h"



class ShakeObserver
{
public:
	ShakeObserver(){};
	~ShakeObserver(){};
	virtual void OnShake() = 0;
};

class CGsensorThread : public CThread
{
public:
	enum Sensitivity //mg
	{
		Sensitivity_low = 1200,
		Sensitivity_middle = 800,
		Sensitivity_Hi = 400,
		Sensitivity_off = -1,
	};
	static CGsensorThread* getInstance(){return _pInstance;};
	static void destroy();
	static void create();
	void setTestBuffer(char* buf, int len, int lineLimit);
	void setSensitivity(int st);
	void setSensitivity(int x, int y, int z);
	int getSensitivity(){return _sensitivity;};
	int getSensitivity(int& x, int& y, int& z);
	void SetShakeObserver(ShakeObserver* p){_pShakeObserver = p;};
private:
	static CGsensorThread* 	_pInstance;
	CGsensorThread();
	~CGsensorThread();
	bool Read(int& x, int& y, int& z);
	
protected:	
	virtual void main(void * p);
	
private:
	struct agsys_iio_info_s *_piio_info;
	char *_buf;
	int _len;
	int _lineNum;
	int _sensitivity;
	int _x_sensitivity;
	int _y_sensitivity;
	int _z_sensitivity;

	CFile *_logfile;
	ShakeObserver*	_pShakeObserver;
};


#define OBDServicePort 18891
#define maxConnection 12
class COBDRemoteService: public StandardTCPService
{
public:
	COBDRemoteService();
	~COBDRemoteService();
	virtual void onReadBufferProcess(char* buffer, int len, int fd);
	virtual void onFdNew(int fd);
	virtual void onFdClose(int fd);
	
private:
	//int	_fds[maxConnection];
};

struct OBDdata
{
	int checkTAG;
	int	speed;
	int	rotateSPM;
	int temp;
};
#define OBDdataCheckTAG 0x01020304

class COBDServiceThread : public TCPRcvDelegate, public CThread
{
public:
	static COBDServiceThread* getInstance(){return _pInstance;};
	static void destroy();
	static void create();

	virtual void OnRecievedBuffer(char* buffer, int len, int fd);
	virtual void onFdNew(int fd);
	virtual void onFdClose(int fd);

	void Stop();
	
protected:	
	virtual void main(void * p);
	
private:
	static COBDServiceThread* 	_pInstance;
	COBDServiceThread();
	~COBDServiceThread();

	int set_obd_polling();
	int obd_dev_open();
	int obd_dev_close();
	int obd_read();
	uint16_t uint8_to_uint16(uint8_t *pdata);
	//int gps_dev_read_data(char *buffer, int buffer_size, double *speed);
	void BroadCast(char* buffer, int len);

private:
	int agobd_client_polling;
	int agobd_client_open_counter;	//ODB is slow than GPS
	//char	*_buffer;
	//char	_bufferSize;

	COBDRemoteService* _pOBDSocketService;
	int					_fds[maxConnection];

	bool				_bRun;
	CSemaphore		 	*_pSem;
	struct agobd_client_info_s *pagobd_client;
};

#endif