/*****************************************************************************
 * class_gsensor_thread.cpp:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 -, linnsong.
 *
 *
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include <string>
#include "class_ipc_rec.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/select.h">
#include <sys/inotify.h>
#include <sys/time.h>

#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"
#include "agsys_iio.h"
//#include "agsys_iio_private.h"
#include "class_gsensor_thread.h"
#include "math.h"

static struct agsys_iio_lis3dh_event_setting_s agsys_iio_lis3dh_event_setting = {
	2,
	100,
	100, //threshood to generate signal
	100000,
	1,
	1,
	1,
};
#ifdef DEBUG
static char* html_head ="<html lang=\"en\" dir=\"ltr\" >" \
    "<head>" \
        "<title>Local video</title>" \
    "</head><body>";
static char* html_rear = "</body></html>";
#endif


CGsensorThread*	CGsensorThread::_pInstance = NULL;

void CGsensorThread::destroy()
{
	if(_pInstance != NULL)
	{
		_pInstance->Stop();
		delete _pInstance;
		_pInstance = NULL;
	}
}

void CGsensorThread::create()
{
	if(_pInstance == NULL)
	{
		_pInstance = new CGsensorThread();
		_pInstance->Go();
	}
}

CGsensorThread::CGsensorThread
	():CThread("GsensorThread", CThread::NORMAL, 0, NULL)
	,_piio_info(NULL)
	,_buf(NULL)
	,_lineNum(5)
	,_pShakeObserver(0)
{
	_piio_info = agsys_iio_lis3dh_open();
	if (_piio_info == NULL) {
		printf("--- open gsensor device failed\n");
		return;
	}
	int ret_val = agsys_iio_lis3dh_event_set(_piio_info,
		&agsys_iio_lis3dh_event_setting);
	if (ret_val) {
		printf("--- set gsensor device config failed\n");
		return;
	}
	char value[256];
	agcmd_property_get("system.gsensor.sensitivity", value, "-1");
	_sensitivity = atoi(value);

	_x_sensitivity = -1;
	_y_sensitivity = -1;
	_z_sensitivity = -1;
	agcmd_property_get("system.gsensor.sensitivityXYZ", value, "x:-1 y:-1 z:-1");
	char* ch;
	ch = strstr(value, "x:");
	if(ch > 0)
	{
		_x_sensitivity = atoi(ch + 2);
	}
	ch = strstr(value, "y:");
	if(ch > 0)
	{
		_y_sensitivity = atoi(ch + 2);
	}
	ch = strstr(value, "z:");
	if(ch > 0)
	{
		_z_sensitivity = atoi(ch + 2);
	}
	printf("---gsensor sensitive, x:%d y:%d z:%d\n", _x_sensitivity, _y_sensitivity, _z_sensitivity);


#ifdef DEBUG
	//remove("/tmp/mmcblk0p1/gsensor.txt");
	//_logfile = new CFile("/tmp/mmcblk0p1/gsensor.txt", CFile::FILE_WRITE);
	//_logfile->write(BYTE * buffer, int length)
#endif
}

CGsensorThread::~CGsensorThread()
{
	if(_piio_info != NULL)
	{
		agsys_iio_lis3dh_event_setting.start_accel_x = 0;
		agsys_iio_lis3dh_event_setting.start_accel_y = 0;
		agsys_iio_lis3dh_event_setting.start_accel_z = 0;
		int ret_val = agsys_iio_lis3dh_event_set(_piio_info,
			&agsys_iio_lis3dh_event_setting);
		agsys_iio_lis3dh_close(_piio_info);
	}
#ifdef DEBUG
	//printf("close gsensor log file\n");
	delete _logfile;
#endif
}

bool CGsensorThread::Read(int& x, int& y, int& z)
{
	struct agsys_iio_lis3dh_accel_xyz_s accel_xyz;
	int ret_val = agsys_iio_lis3dh_read_accel_xyz(_piio_info,
			&accel_xyz);
	if (ret_val < 0) {
		printf("--- read accel infor failed.");
		return false;
	}

	x = (accel_xyz.accel_x * _piio_info->accel_scale);
	y = (accel_xyz.accel_y * _piio_info->accel_scale);
	z = (accel_xyz.accel_z * _piio_info->accel_scale);

	return true;
}

void CGsensorThread::setTestBuffer(char* buf, int len, int lineLimit)
{
	_buf = buf;
	_len = len;
	_lineNum = lineLimit;
	//memset(buf, 0, len);
}

void CGsensorThread::setSensitivity(int st)
{
	_sensitivity = st;
	char value[256];
	sprintf(value, "%d", _sensitivity);
	agcmd_property_set("system.gsensor.sensitivity", value);

}

void CGsensorThread::setSensitivity(int x, int y, int z)
{
	_x_sensitivity = x;
	_y_sensitivity = y;
	_z_sensitivity = z;
	char value[256];
	sprintf(value, "x:%d y:%d z:%d", _x_sensitivity, _y_sensitivity, _z_sensitivity);
	agcmd_property_set("system.gsensor.sensitivityXYZ", value);

}

int CGsensorThread::getSensitivity(int& x, int& y, int& z)
{
	x = _x_sensitivity;
	y = _y_sensitivity;
	z = _z_sensitivity;
	return x;
}

void CGsensorThread::main(void * p)
{
	int ret_val;
	int x, y, z;
	char tmp[1024];
	memset(tmp, 0, sizeof(tmp));
	int line = 0;
	char* posi;
	double r;
	int shakecounter = 0;
	char fileTmp[256];

	while(_piio_info)
	{
		ret_val = agsys_iio_lis3dh_event_wait(_piio_info);
		if (ret_val < 0) {
			//printf("--- wait failed\n");
			sleep(1);
			continue;
		}
		Read(x, y, z);
		r = sqrt(x*x + y*y + z*z);
#ifdef DEBUG
		sprintf(fileTmp, "<p>%5.2f\n<p>", r);
		//_logfile->write((BYTE*)fileTmp, strlen(fileTmp));
#endif
		if((_sensitivity > 0)&&((x >= _x_sensitivity)||(y >= _y_sensitivity)||(z >= _z_sensitivity)))
		{
			IPC_AVF_Client::getObject()->GsensorSignalProcess();
			if(_buf)
			{
				if(line > _lineNum)
				{
					posi = strstr(_buf, "\\n");
					if(posi)
					{
						memset(tmp, 0, sizeof(tmp));
						strcpy(tmp, posi + 2);
					}
				}
				else
					line++;

				//if(x < _x_sensitivity)
				//{
				//	x = -1;
				//}
				//if(y < _y_sensitivity)
				//{
				//	y = -1;
				//}
				//if(z < _z_sensitivity)
				//{
				//	z = -1;
				//
				//}
				sprintf(tmp + strlen(tmp),"x:%d,y:%d,z:%d \\n", x, y, z);
				//sprintf(tmp + strlen(tmp),"x:%d\n", x);
				strcpy(_buf,tmp);
				printf("Gr:%f----%s\n", r,_buf);
			}
		}
		if(r > 1200)
		{
			shakecounter ++;
			if(shakecounter >= 5)
			{
				if(_pShakeObserver)
					_pShakeObserver->OnShake();
				shakecounter = 0;
			}
		}
		else
		{
			shakecounter = 0;
		}
	}
}

/***************************************************************
COBDServiceThread::()
*/
void COBDServiceThread::destroy()
{
	if(_pInstance != NULL)
	{
		printf("COBDServiceThread::destroy \n");
		_pInstance->Stop();
		delete _pInstance;
		_pInstance = NULL;
	}
}

void COBDServiceThread::create()
{
	if(_pInstance == NULL)
	{
		_pInstance = new COBDServiceThread();
		_pInstance->Go();
	}
}

void COBDServiceThread::OnRecievedBuffer
	(char* buffer
	, int len
	, int fd)
{
}

void COBDServiceThread::onFdNew(int fd)
{

}
void COBDServiceThread::onFdClose(int fd)
{
}

void COBDServiceThread::BroadCast(char* buffer, int len)
{
	if(_pOBDSocketService)
		_pOBDSocketService->BroadCast(buffer, len);
}

void COBDServiceThread::main(void * p)
{
	//int offset = 0;
	struct agobd_pid_info_s *ppid_info;
	uint8_t *ppid_data;
	uint32_t pid_info_size;
	uint8_t pid_index;
	int ret_val = 0;
	_bRun = true;
	OBDdata data;
	data.checkTAG = OBDdataCheckTAG;
	while(_bRun)
	{
		data.speed = 0;
		data.rotateSPM = 0;
		data.temp = 0;
		ret_val = 0;
		if (obd_read() < 0) {
			printf("--- obd read error\n");
		}
		else{

			ppid_info = pagobd_client->pobd_info->pid_info;
			ppid_data = pagobd_client->pobd_info->pid_data;
			pid_info_size = (pagobd_client->pobd_info->pid_data_size /
				sizeof(struct agobd_pid_info_s));
			//Engine Coolant Temperature
			pid_index = 0x05;
			if ((pid_info_size > pid_index) &&
				((ppid_info[pid_index].flag & AGOBDD_PID_FLAG_VALID) ==
				AGOBDD_PID_FLAG_VALID)) {
				int tmp_data;

				tmp_data = ppid_data[ppid_info[pid_index].offset];
				tmp_data -= 40;
				data.temp = tmp_data;
			} else {
				data.temp = 0;
			}
			//Engine RPM
			pid_index = 0x0C;
			if ((pid_info_size > pid_index) &&
				((ppid_info[pid_index].flag & AGOBDD_PID_FLAG_VALID) ==
				AGOBDD_PID_FLAG_VALID)) {
				uint16_t tmp_data;

				tmp_data = uint8_to_uint16(
					&ppid_data[ppid_info[pid_index].offset]);
				tmp_data >>= 2;
				data.rotateSPM = tmp_data;
			} else {
				data.rotateSPM = 0;
			}
			//Vehicle Speed Sensor
			pid_index = 0x0D;
			{
				if ((pid_info_size > pid_index) &&
					((ppid_info[pid_index].flag & AGOBDD_PID_FLAG_VALID) ==
					AGOBDD_PID_FLAG_VALID)) {
					data.speed = ppid_data[ppid_info[pid_index].offset];
				} else {
					data.speed = 0;
				}
			}
			//printf("-- get OBD date: speed : %d, spm : %d, temp: %d\n", data.speed, data.rotateSPM, data.temp);
			//send buffer here.\
			BroadCast((char*)&data, sizeof(OBDdata));
		}
		sleep(1);
	}
	_pSem->Give();
}

COBDServiceThread* COBDServiceThread::_pInstance = NULL;

COBDServiceThread::COBDServiceThread
	():CThread("OBDService", CThread::NORMAL, 0, NULL)
	,pagobd_client(NULL)
	,agobd_client_polling(1)
	,_pOBDSocketService(NULL)
{
	_pOBDSocketService = new COBDRemoteService();
	//_pOBDSocketService->SetDelegate(this);
	_pSem = new CSemaphore(0, 1, "OBD");
	_pOBDSocketService->Go();
}
COBDServiceThread::~COBDServiceThread()
{
	_bRun = false;
	_pOBDSocketService->Stop();
	delete _pSem;
	delete _pOBDSocketService;
	obd_dev_close();
}

void COBDServiceThread::Stop()
{
	_pSem->Take(2000000);
	CThread::Stop();
}
int COBDServiceThread::set_obd_polling()
{
	int ret_val = -1;
	struct agobd_pid_info_s *ppid_info;
	uint32_t pid_info_size;
	uint8_t pid_index;

	if (pagobd_client == NULL) {
		goto set_obd_polling_flag_all_exit;
	}

	ret_val = agobd_client_get_info(pagobd_client);
	if (ret_val) {
		printf("agobd_client_get_info() = %d\n", ret_val);
		goto set_obd_polling_flag_all_exit;
	}

	ppid_info = pagobd_client->pobd_info->pid_info;
	pid_info_size = (pagobd_client->pobd_info->pid_data_size /
		sizeof(struct agobd_pid_info_s));
	if (pid_info_size) {
		printf("pid_info_size = %d\n", pid_info_size);
	} else {
		printf("pid_info_size = %d\n", pid_info_size);
	}

	pid_index = 0x05;
	if (pid_info_size > pid_index) {
		ppid_info[pid_index].flag = AGOBDD_PID_FLAG_POLLING;
	}
	pid_index = 0x0C;
	if (pid_info_size > pid_index) {
		ppid_info[pid_index].flag = AGOBDD_PID_FLAG_POLLING;
	}
	pid_index = 0x0D;
	if (pid_info_size > pid_index) {
		ppid_info[pid_index].flag = AGOBDD_PID_FLAG_POLLING;
	}

	ret_val = agobd_client_set_info_flag(pagobd_client);
	if (ret_val) {
		printf("agobd_client_set_info_flag() = %d\n", ret_val);
	}

set_obd_polling_flag_all_exit:
	return ret_val;
}

int COBDServiceThread::obd_dev_open()
{
	if (pagobd_client != NULL)
		return 0;
	pagobd_client = agobd_open_client();
	if (pagobd_client == NULL) {
		return -1;
	}
	return 0;
}

int COBDServiceThread::obd_dev_close()
{
	if (pagobd_client == NULL)
		return 0;

	agobd_close_client(pagobd_client);
	pagobd_client = NULL;

	return 0;
}

int COBDServiceThread::obd_read()
{
	int ret_val = -1;

	if (pagobd_client == NULL) {
		printf("obd_dev_open\n");
		if(obd_dev_open() < 0)
			return -1;
	}
	else if (agobd_client_polling > 0) {
		printf("set_obd_polling\n");
		ret_val = set_obd_polling();
		if (ret_val) {
			return ret_val;
		}
		agobd_client_polling--;
	} else {
		ret_val = agobd_client_get_info(pagobd_client);
		if (ret_val) {
			printf("agobd_client_get_info() = %d\n", ret_val);
			//goto read_data_exit;
		}
		return ret_val;
	}

//read_data_exit:
	return ret_val;
}

uint16_t COBDServiceThread::uint8_to_uint16(uint8_t *pdata)
{
	uint16_t tmp_data;
	tmp_data = pdata[0];
	tmp_data <<= 8;
	tmp_data |= pdata[1];
	return tmp_data;
}

COBDRemoteService::COBDRemoteService
	(): StandardTCPService(OBDServicePort, 1024, maxConnection)
{
}

COBDRemoteService::~COBDRemoteService()
{

}

void COBDRemoteService::onReadBufferProcess
	(char* buffer, int len, int fd)
{
	printf("onReadBufferProcess\n");
}

void COBDRemoteService::onFdNew(int fd)
{
	// add fd to set
	printf("new client connected\n");
}

void COBDRemoteService::onFdClose(int fd)
{
	printf("onFdClose\n");
}

