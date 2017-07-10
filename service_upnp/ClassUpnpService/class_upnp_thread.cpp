/*****************************************************************************
 * class_upnp_thread.cpp:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

#include "agbox.h"

#include "class_upnp_thread.h"
#include "class_upnp_description.h"

#ifdef XMPPENABLE
#include "../xmpp_client/class_xmpp_client.h"
#endif

#define DMC_UPNP_MAX_VSIZE			(4096)
#define DMC_UPNP_MAX_EXPIRATION		(100)

/***************************************************************
CUpnpThread
*/

CUpnpThread* CUpnpThread::_pInstance = NULL;

void CUpnpThread::Create(char* root, char* description, CUpnpEventHandler* _pCamera)
{
	if(_pInstance == NULL)
	{
		_pInstance = new CUpnpThread(root, description, _pCamera);
		_pInstance->Go();
	}
}

void CUpnpThread::Destroy()
{
	if(_pInstance)
	{
		_pInstance->Stop();
		delete _pInstance;
	}
}

int CUpnpThread::FilePropertyWaitIP(const char *key, const char *value)
{
	CUpnpThread::getInstance()->ipChanged();
	return 0;
}

CUpnpThread* CUpnpThread::getInstance()
{
	return _pInstance;
}

CUpnpThread::CUpnpThread
	(const char* upnpWebSvrRoot
	,const char* device_description
	,CUpnpEventHandler *peventProcessor
	) : CThread("CUpnpThread", CThread::NORMAL, 0, NULL)
	,_svrRoot(upnpWebSvrRoot)
	,_device_description(device_description)
	,_pEventProcessor(peventProcessor)
	,_bInited(false)
	,_CurrentIP(0)
	,_pwait_ip(NULL)
	,_bNeedReboot(false)
{
	_mutex = new CMutex("CUpnpThread mutex");
	_pTmpPropertyChanged = new CSemaphore(0, 1, "upnp prop sem");
	
	CPropsEventThread::getInstance()->AddPEH(this);
}

CUpnpThread::~CUpnpThread()
{
	if(_bInited)
		upnp_deinit();
	delete _mutex;
	delete _pTmpPropertyChanged;
}

void CUpnpThread::EventPropertyUpdate(changed_property_type state)
{
	if(state == property_type_wifi)
	{
		_pTmpPropertyChanged->Give();
	}
	_pEventProcessor->EventPropertyUpdate(state);
}

void CUpnpThread::ipChanged()
{
	_pTmpPropertyChanged->Give();
}

int CUpnpThread::CheckIpStatus()
{
	//printf("----CheckIpStatus\n");
	
	char ip_prop_name[AGCMD_PROPERTY_SIZE];
	char prebuild_prop[AGCMD_PROPERTY_SIZE];

	agcmd_property_get(wifiStatusPropertyName, _wifi_prop, "NA");
	if (strcasecmp(_wifi_prop, "on") == 0) {
	} else {
		return 0;
	}

	agcmd_property_get(wifiModePropertyName, _wifi_prop, "NA");
	if (strcasecmp(_wifi_prop, "AP") == 0) {
		agcmd_property_get("prebuild.board.if_ap",
			prebuild_prop, "NA");
		agcmd_property_get("temp.earlyboot.if_ap",
			_wifi_prop, prebuild_prop);
	} else {
		agcmd_property_get("prebuild.board.if_wlan",
			prebuild_prop, "NA");
		agcmd_property_get("temp.earlyboot.if_wlan",
			_wifi_prop, prebuild_prop);
	}
	sprintf(ip_prop_name, "temp.%s.ip", _wifi_prop);
	agcmd_property_get(ip_prop_name, _wifi_prop, "NA");
	if(strcmp(_wait_ip_prop_name, ip_prop_name) != 0)
	{
		printf("----wait new interface: %s\n", ip_prop_name);
		if (_pwait_ip) {
			agcmd_property_wait_stop(_pwait_ip);
			_pwait_ip = NULL;
		}
		_pwait_ip = agcmd_property_wait_start(ip_prop_name,
			CUpnpThread::FilePropertyWaitIP);
		strcpy(_wait_ip_prop_name, ip_prop_name);
	}

	UINT32 rt = inet_addr(_wifi_prop);
	if ((rt != (UINT32)-1)&&(rt != 0)) {
		if(_CurrentIP != rt) {
			_CurrentIP = rt;
			return 2;
		} else {
			return 1;
		}
	}

	return 0;
}

void CUpnpThread::RequireUpnpReboot()
{
	_bNeedReboot = true;
	_pTmpPropertyChanged->Give();
}

void CUpnpThread::RebootUpnp()
{
	if(_bInited)
	{
		printf("---RebootUpnp-upnp_deinit()\n");
		upnp_deinit();
		_bInited = false;

	}
	if(initUpnp() < 0)
	{
		printf("----initUpnp failed\n");
	}
	else
	{
		_bInited = true;
		_pEventProcessor->setUpnpDeviceHandle(_upnpDevice_handle);
	}

}

void CUpnpThread::UpnpCheck()
{
	printf("----UpnpCheck\n");
	int ck = CheckIpStatus();
	if(ck == 1)
	{
		if(!_bInited)
		{
			if(initUpnp() < 0)
			{
				printf("----initUpnp failed\n");
			}
			else
			{
				_bInited = true;
				_pEventProcessor->setUpnpDeviceHandle(_upnpDevice_handle);
			}
		}
	}
	else if(ck == 0) 
	{
		if(_bInited)
		{
			printf("----upnp_deinit()\n");
			upnp_deinit();
			_bInited = false;
		}
	}
	else if(ck == 2) 
	{
		RebootUpnp();				
	}

}

void CUpnpThread::Stop()
{
	_bRun = false;
	CThread::Stop();
}

void CUpnpThread::main(void *)
{	
	
	printf("--CUpnpThread thread start!!\n");
	
	struct timeval tv, lasttv;
	struct timezone tz;	
	int ms;
	memset(&lasttv, 0, sizeof(&lasttv));
	
	_bRun = true;
	while(_bRun)
	{	
#ifndef noupnp
		//gettimeofday(&tv, &tz);
		//ms = (tv.tv_sec - lasttv.tv_sec)*1000 + (tv.tv_usec - lasttv.tv_usec)/1000;
		//if(ms >= 1000)
		//_pTmpPropertyChanged->Take(-1);	
		{
			if(_bNeedReboot)
			{
				RebootUpnp();
				_bNeedReboot = false;
			}
			UpnpCheck();
		}
#endif
		lasttv = tv;
		if(_bInited)
			_pTmpPropertyChanged->Take(-1);	
		else
			sleep(1);
	}
}

int CUpnpThread::initUpnp()
{
	int ret_val = 0;
	int rt = 0;

	char					pdmc_upnp_address[IP_MAX_SIZE];
	unsigned short			dmc_upnp_port = 80;
	char* 					ptmp_str;
	
	ptmp_str = new char[DMC_UPNP_MAX_VSIZE];	
	_mutex->Take();
	memset(pdmc_upnp_address, 0, IP_MAX_SIZE);
	ret_val = -1;
	int c = 0;
	
	//check if wifi status ready.
	
	while(ret_val != 0)
	{	
		printf("upnp current ip : %s\n", _wifi_prop);
		ret_val = UpnpInit(_wifi_prop, dmc_upnp_port); 
		c ++;
		if(ret_val != 0)
		{
			sleep(1);
			printf("try init Upnp :%d times.\n", c);
		}
	}
	if(ret_val == UPNP_E_SUCCESS)
	{
		strncpy(pdmc_upnp_address, UpnpGetServerIpAddress(), IP_MAX_SIZE);
		dmc_upnp_port = UpnpGetServerPort();
		printf("UPnP register at @ %s:%u\n", pdmc_upnp_address, dmc_upnp_port);
		
		ret_val = UpnpSetWebServerRootDir(_svrRoot);
		if (ret_val != UPNP_E_SUCCESS) {
			printf("UpnpSetWebServerRootDir(%s) = %d\n", _svrRoot, ret_val);
			rt = -1;
		}
		//else
		{	
			memset(ptmp_str, 0, DMC_UPNP_MAX_VSIZE);
			sprintf(ptmp_str, "http://%s:%u%s",pdmc_upnp_address, dmc_upnp_port, _device_description);
			//printf("---url: %s", ptmp_str);
			//ret_val = UpnpRegisterRootDevice( ptmp_str, upnp_event_handler, (void*)_pEventProcessor, &_upnpDevice_handle);
			
			//if (ret_val != UPNP_E_SUCCESS)
			//{
			//	printf("UpnpRegisterRootDevice2(0) = %d\n", ret_val);
			//	rt = -1;
			//}
			//else
			{
				//UpnpUnRegisterRootDevice(_upnpDevice_handle);
				//ret_val = UpnpRegisterRootDevice2(UPNPREG_BUF_DESC,	ptmp_str, strlen(ptmp_str), 1,
				//			upnp_event_handler, (void*)_pEventProcessor, &_upnpDevice_handle);
				ret_val = UpnpRegisterRootDevice( ptmp_str, CUpnpThread::upnp_event_handler, (void*)_pEventProcessor, &_upnpDevice_handle);
				if (ret_val != UPNP_E_SUCCESS)
				{
					printf("UpnpRegisterRootDevice2(1) = %d\n", ret_val);
					rt = -1;
				}else{
					//ret_val = UpnpSetMaxSubscriptionTimeOut(_upnpDevice_handle, DMC_UPNP_MAX_EXPIRATION);
					//if (ret_val != UPNP_E_SUCCESS) {
					//	printf("UpnpSetMaxSubscriptionTimeOut(0x%08x, %d) = %d\n", _upnpDevice_handle, DMC_UPNP_MAX_EXPIRATION, ret_val);
					//}
					ret_val = UpnpSendAdvertisement(_upnpDevice_handle, DMC_UPNP_MAX_EXPIRATION);
					if (ret_val != UPNP_E_SUCCESS) {
						printf("UpnpSendAdvertisement(0x%08x, %d) = %d\n", _upnpDevice_handle, DMC_UPNP_MAX_EXPIRATION, ret_val);
					}
				}
			}
		}		
	}	
	else{
		printf("UpnpInit(%s, %d) = %d\n", pdmc_upnp_address, dmc_upnp_port, ret_val);
		rt = -1;	
	}
	_mutex->Give();

	delete[] ptmp_str;

#ifdef XMPPENABLE
	XMPP_Client::Create();
#endif
	
	return rt;
}

void CUpnpThread::upnp_deinit()
{
	_mutex->Take();
	UpnpUnRegisterRootDevice(_upnpDevice_handle);
	UpnpFinish();
	_mutex->Give();
	
#ifdef XMPPENABLE
	XMPP_Client::Destroy();
#endif
}

#if UPNP_VERSION < 10800
int CUpnpThread::upnp_event_handler(enum Upnp_EventType_e EventType,
	void *Event, void *Cookie)
#else
int CUpnpThread::upnp_event_handler(Upnp_EventType EventType,
	const void *Event, void *Cookie)
#endif
{
	//int					ret_val = UPNP_E_SUCCESS;

	CUpnpEventHandler* pEventHandler =(CUpnpEventHandler*)Cookie;
	return pEventHandler->upnp_event_handler(EventType, Event);
}

/***************************************************************
FileQueueControl
*/
long long FileQueueControl::_freesize = -2;
FileQueueControl::FileQueueControl
	(const char* path
	, unsigned long MaxFileNumber
	, unsigned long long maxSize
	):_nodeQueueDeepth(MaxFileNumber)
	,_nodeQueueStorageLimitation(maxSize)
	,_pMoviefileList(NULL)
	,_bCheckFreeSpace(FALSE)
	,_locker(NULL)
{
	_addedNodeNumber = 0;
	_headIndex = 0;
	_queuedStorageSize = 0;
	if(strlen(path) > MaxPathStringLength)
	{
		printf("-- error ,set path strlen : %d bigger than defined : %d \n",strlen(path) ,MaxPathStringLength);
	}
	else
	{
		strcpy(_basePath, path);
		_pMoviefileList = new movieFileNode[_nodeQueueDeepth];
		
		if(_pMoviefileList == NULL)
		{
			printf("-- error ,FileQueueControl init memory not enought.\n");
		}
		else
		{
			_locker = new CMutex("FileQueueControl mutex");
			// read path fill queue
		}
	}
	
}

/***************************************************************
FileQueueControl::~FileQueueControl()
*/
FileQueueControl::~FileQueueControl()
{
	if(_pMoviefileList != NULL)
	{
		delete[] _pMoviefileList;
		_pMoviefileList = NULL;
		
	}
	if(_locker)
	{
		delete _locker;
		_locker = NULL;
	}
}

/***************************************************************
FileQueueControl::PreReadDir() 
*/
bool FileQueueControl::PreReadDir(char* forFile)
{	
	
	reset();
	DIR *dir;
	struct dirent *ptr;
	dir = opendir(_basePath);
	if(dir != NULL)
	{
		while((ptr = readdir(dir)) != NULL)
		{
			if(!movieFileNode::isMovieFile(ptr->d_name))
				continue;
			if((forFile !=NULL)&&(strcmp(forFile, ptr->d_name) == 0))
				continue;
			addToQueue(ptr->d_name);			
		}
		closedir(dir);
	}
	else
	{
		// can't open dir
		printf("-- cant open _basePath : %s\n", _basePath);
	}
	//printf("---pre read : %d\n", _addedNodeNumber);
	return true;
}


/***************************************************************
int FileQueueControl::addToQueue(char* fileName)
*/
int FileQueueControl::addToQueue
	(char* fileName)
{
	struct stat stat0, stat1;
	_locker->Take();
		
	int i = 0;  
	while(i < _addedNodeNumber)
	{
		if(_pMoviefileList[(i + _headIndex)% _nodeQueueDeepth].compareName(fileName) == 0)
		{
			_locker->Give();
			return 0;
		}
		i++;
	}	

	char tmp[256];	
	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%s%s", _basePath, fileName);
	if(stat(tmp, &stat0)!= 0)
	{
		_locker->Give();
		return false;
	}

	if(stat0.st_size > _nodeQueueStorageLimitation)
	{
		printf("-- file size bigger than max queue size : %d, %lld\n",stat0.st_size , _nodeQueueStorageLimitation);
		_locker->Give();
		return false;
	}

	unsigned long long freesize = 0, total = 0;
	SDWatchThread::getInstance()->GetSpaceInfor(&total, &freesize);
	
	if(IPC_AVF_Client::getObject()->isCircleMode())
	{
		//int pp = _headIndex;
#if 1

		int i = 0;
		while((_addedNodeNumber > 0)
			&&((_queuedStorageSize + stat0.st_size > _nodeQueueStorageLimitation)
			||(_addedNodeNumber >= _nodeQueueDeepth)
			||((_bCheckFreeSpace)&&(freesize < FreeSpaceThreshold)))
			&&(i < _addedNodeNumber))
		{
			memset(tmp, 0, sizeof(tmp));
			sprintf(tmp, "%s%s%s", _basePath, _pMoviefileList[(_headIndex + i) % _nodeQueueDeepth].name, MovieFileExtend);				
			stat(tmp, &stat1);
			int s = 0;
			s = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
			if((stat1.st_mode & 0x1ff) == s)
			{
				// skip locked file;
				printf("skip read only file: %s\n", tmp);
				i++;
				continue;
			}
			int ret = remove(tmp);	
			if(ret == 0)
			{		
				memcpy(tmp + strlen(tmp) - 4,".jpg", 4);
				ret = remove(tmp);
				printf("--remove :%s, size limit: %lld\n",tmp, _queuedStorageSize);
				if(i == 0)
				{
					_headIndex = (_headIndex + 1)% _nodeQueueDeepth;
					i = 0;
				}
				else
				{
					for(int j = 0; j < _addedNodeNumber - i - 1; j++)
					{
						memcpy(&(_pMoviefileList[(_headIndex + i + j) % _nodeQueueDeepth])
							,&(_pMoviefileList[(_headIndex + i + j + 1) % _nodeQueueDeepth])
							,sizeof(movieFileNode));
					}
				}
				_addedNodeNumber --;
				_queuedStorageSize -= stat1.st_size;
			}
			else
			{	
				printf("--remove :%s failed, : %d \n",tmp, ret);
				_locker->Give();
				return false;
			}

		}
#else		
		while((_addedNodeNumber > 0)&&((_queuedStorageSize + stat0.st_size > _nodeQueueStorageLimitation)
			||(_addedNodeNumber >= _nodeQueueDeepth)
			||((_bCheckFreeSpace)&&(freesize < FreeSpaceThreshold))))
		{
			memset(tmp, 0, sizeof(tmp));
			sprintf(tmp, "%s%s%s", _basePath, _pMoviefileList[_headIndex].name, MovieFileExtend);				
			stat(tmp, &stat1);
			int ret = remove(tmp);	
			if(ret == 0)
			{		
				//memset(tmp, 0, sizeof(tmp));
				//sprintf(tmp, "%s%s%s", _basePath, _pMoviefileList[_headIndex].name, PosterFileExtend);
				memcpy(tmp + strlen(tmp) - 4,".jpg", 4);
				ret = remove(tmp);
				printf("--remove :%s, size limit: %lld\n",tmp, _queuedStorageSize);
				_headIndex = (_headIndex + 1)% _nodeQueueDeepth;
				_addedNodeNumber --;
				_queuedStorageSize -= stat1.st_size;
			}
			else
			{	
				printf("--remove :%s failed, : %d \n",tmp, ret);
				_locker->Give();
				return false;
			}
		}
#endif
	//resequency
	}
	else
	{
		// check space if need auto stop.?
	}

	i = 0;  
	while(i < _addedNodeNumber)
	{
		
		if(_pMoviefileList[(i + _headIndex)% _nodeQueueDeepth].compareName(fileName) > 0)
			break;
		i++;
	}
	if(i < _addedNodeNumber)
	{
		for(int j = _addedNodeNumber; j > i; j--)
		{
			_pMoviefileList[(j + _headIndex)% _nodeQueueDeepth].cpy(&_pMoviefileList[(j + _headIndex -1)% _nodeQueueDeepth]);
		}
		
	}
	_pMoviefileList[(i + _headIndex)% _nodeQueueDeepth].setName(fileName);
	_pMoviefileList[(i + _headIndex)% _nodeQueueDeepth].setSize(stat0.st_size);
	_addedNodeNumber++;
	_queuedStorageSize += stat0.st_size;
	_locker->Give();
	return TRUE;
}

int FileQueueControl::removeFromQueue
	(char* fileName)
{
	int i = 0;  
	while(i < _addedNodeNumber)
	{
		if(_pMoviefileList[(i + _headIndex)% _nodeQueueDeepth].compareName(fileName) == 0)
			break;
		i++;
	}
	
	if(i < _addedNodeNumber)
	{
		_queuedStorageSize -= _pMoviefileList[(i + _headIndex)% _nodeQueueDeepth].size;
		for(int j = i; j < _addedNodeNumber - 1; j++)
		{
			_pMoviefileList[(j + _headIndex)% _nodeQueueDeepth].cpy(&_pMoviefileList[(j + _headIndex + 1)% _nodeQueueDeepth]);
		}
		_addedNodeNumber --;
	}
	return _addedNodeNumber;
}

/***************************************************************
FileCircleControl::getLatestFileName

char* FileQueueControl::getLatestFileName()
{
	return _pMoviefileList[(_addedNodeNumber + _headIndex)% _nodeQueueDeepth].name;
}*/

/***************************************************************
FileQueueControl::getLatestFileFullPath

void FileQueueControl::getLatestFileFullPath(char* pathBuffer, int len)
{
	
}*/

/***************************************************************
FileQueueControl::getOldestFileName

char* FileQueueControl::getOldestFileName()
{
	return _pMoviefileList[_headIndex].name;
}*/

/***************************************************************
FileQueueControl::getOldestFileFullPath

char* FileQueueControl::getOldestFileFullPath()
{
	
}*/

void FileQueueControl::reset()
{
	_addedNodeNumber = 0;
	_queuedStorageSize = 0;
	_headIndex = 0;
}

/***************************************************************
FileQueueControl::ChectSizeAndNumber
*/
int FileQueueControl::ChectSizeAndNumber(char* forFile)
{
	IPC_AVF_Client::getObject()->CheckFreeAutoStop();
	//_addedNodeNumber = 0;
	//_queuedStorageSize = 0;
	//_headIndex = 0;
	//PreReadDir(forFile);
	return addToQueue(forFile);
}
long long FileQueueControl::GetStorageFreeSpace()
{
	if(_freesize == -2)
	{
		char tmp[256];
		memset(tmp, 0, sizeof(tmp));
		agcmd_property_get(SotrageStatusPropertyName, tmp, "default");
		if(strcmp(tmp, "mount_ok") == 0)
		{
			memset(tmp, 0, sizeof(tmp));
			int rt = agcmd_property_get(SotrageFreePropertyName, tmp, "0");
			if(rt == 0)
				_freesize = atoll(tmp);
			else
				_freesize = -1;
		}
		else
			_freesize = 0;
	}
	return _freesize;
};


char* FileQueueControl::getNextFile(char* path)
{
	if(strstr(path, _basePath) > 0)
	{
		char* name = strstr(path, _basePath) + strlen(_basePath);
		//printf("file name : %s\n", name);
		int i = 0;  
		while(i < _addedNodeNumber)
		{
			if(_pMoviefileList[(i + _headIndex)% _nodeQueueDeepth].compareName(name) == 0)
				break;
			i++;
		}
	
		if(i < _addedNodeNumber - 1)
		{
			i++;
			sprintf(_tmp, "%s%s%s", _basePath, _pMoviefileList[(i + _headIndex)% _nodeQueueDeepth].name,MovieFileExtend);
			return _tmp;
		}
		else
			return NULL;
	}
	else
		return NULL;
}

int FileQueueControl::GetFullPath(char* shortName, int startSearch, int range, char* path, int len)
{	
	int index;
	if(( index = ShotNameIndex(shortName, startSearch, range)) >= 0)
	{
		memset(path, 0, len);
		sprintf(path,"%s%s%s", getBasePath(), _pMoviefileList[index].name,MovieFileExtend);
		printf("full path : %s\n", path);
	}
	return 0;
}

int FileQueueControl::ShotNameIndex(char* shortName, int startSearch, int range)
{
	int index;
	int i;
	for(i = 0; i < range; i++)
	{
		index = (_headIndex+ startSearch + i) % _nodeQueueDeepth;
		if(_pMoviefileList[index].checkShortName(shortName, strlen(shortName)))
		{
			printf("---fileNOde : %d \n", index);
			break;
		}		
	}
	if(i < range)
		return index;
	else
		return -1;
}

int FileQueueControl::RemoveMovie(char* shortName, int startSearch, int range)
{
	int index = ShotNameIndex(shortName, startSearch, range);
	printf("--remove index:%d\n", index);
	if( index >= 0)
	{
		char tmp[256];
		sprintf(tmp,"%s%s%s",_basePath,_pMoviefileList[index].name, MovieFileExtend);
		printf("movieFileNode::removeMovie() %s\n", tmp);
		remove(tmp);
		sprintf(tmp,"%s%s%s",_basePath,_pMoviefileList[index].name, PosterFileExtend);
		remove(tmp);
	}
	return 0;
}

int FileQueueControl::mvToMarkDir(char* shortName, int startSearch, int range)
{
	int index;
	if(( index = ShotNameIndex(shortName, startSearch, range)) >= 0)
	{
		char tmp[256];
		char tmp1[256];
		sprintf(tmp,"%s%s%s",_basePath,_pMoviefileList[index].name, MovieFileExtend);
		sprintf(tmp1,"%s%s%s",MARKED_PATH,_pMoviefileList[index].name, MovieFileExtend);
		rename(tmp, tmp1);
		int r = chmod(tmp1, S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
		sprintf(tmp,"%s%s%s",_basePath,_pMoviefileList[index].name, PosterFileExtend);
		sprintf(tmp1,"%s%s%s",MARKED_PATH,_pMoviefileList[index].name, PosterFileExtend);
		rename(tmp, tmp1);
		
	}
	return 0;
}

int FileQueueControl::mvToCircleDir(char* shortName, int startSearch, int range)
{
	int index;
	if(( index = ShotNameIndex(shortName, startSearch, range)) >= 0)
	{
		char tmp[256];
		char tmp1[256];
		sprintf(tmp,"%s%s%s",POOL_PATH,_pMoviefileList[index].name, MovieFileExtend);
		sprintf(tmp1,"%s%s%s",_basePath,_pMoviefileList[index].name, MovieFileExtend);
		rename(tmp1, tmp);
		int r = chmod(tmp, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		sprintf(tmp,"%s%s%s",POOL_PATH,_pMoviefileList[index].name, PosterFileExtend);
		sprintf(tmp1,"%s%s%s",_basePath,_pMoviefileList[index].name, PosterFileExtend);
		rename(tmp1, tmp);
	}
	return 0;
}

int FileQueueControl::cpyToInternalDir(char* shortName, int startSearch, int range)
{
	int index;
	
	if(( index = ShotNameIndex(shortName, startSearch, range)) >= 0)
	{
		char tmp[256];
		char tmp1[256];
		char buf[10240];
		sprintf(tmp,"%s%s%s",_basePath,_pMoviefileList[index].name, MovieFileExtend);
		sprintf(tmp1,"%s%s%s",Internal_PATH,_pMoviefileList[index].name, MovieFileExtend);
		//rename(tmp1, tmp);
		CFile source(tmp, CFile::FILE_READ);
		CFile dest(tmp1, CFile::FILE_WRITE);
		
		UINT64 len = 0, wlen = 0;
		int rlen = 0, wwlen = 0;
		_cppercentage = 0;
		if(!source.IsValid())
			return -1;
		len = source.GetLength();
		while(wlen < len)
		{
			rlen = source.readfile(buf, 10240);
			wwlen = dest.writefile(buf, rlen);
			wlen =  wlen + rlen;
			_cppercentage = wlen;
			_cppercentage = _cppercentage* 100 / len;
			//printf("cpy per: %%%3.2f\n", percentage);
		}
		sprintf(tmp,"%s%s%s",_basePath,_pMoviefileList[index].name, PosterFileExtend);
		sprintf(tmp1,"%s%s%s",Internal_PATH,_pMoviefileList[index].name, PosterFileExtend);
		//rename(tmp1, tmp);
		CFile source1(tmp, CFile::FILE_READ);
		CFile dest1(tmp1, CFile::FILE_WRITE);
		len = 0;
		wlen = 0;
		rlen = 0;
		if(!source1.IsValid())
			return -2;
		len = source1.GetLength();
		while(wlen < len)
		{
			rlen = source1.readfile(buf, 10240);
			dest1.writefile(buf, rlen);
			wlen =  wlen + rlen;
		}
	}
	return 0;
}

UINT64 FileQueueControl::fileSize(char* shortName, int startSearch, int range)
{
	int index;
	if(( index = ShotNameIndex(shortName, startSearch, range)) >= 0)
	{
		return (UINT64)_pMoviefileList[index].size;
	}
}

bool FileQueueControl::HasFile(char* shortName)
{
	return(ShotNameIndex(shortName, 0, _addedNodeNumber) >= 0);	
}

/*
void FileQueueControl::initFreeSpace()
{
	char tmp[256];
	memset(tmp, 0, sizeof(tmp));
	agcmd_property_get(SotrageStatusPropertyName, tmp, "default");
	if(strcmp(tmp, "mount_ok") == 0)
	{
		memset(tmp, 0, sizeof(tmp));
		int rt = agcmd_property_get(SotrageFreePropertyName, tmp, "0");
		if(rt == 0)
			_freesize = atoll(tmp);
		else
			_freesize = -1;
	}
	else
		_freesize = 0;
}*/

/***************************************************************
movieFileNode::isBeforeThan
*/
void movieFileNode::cpy(movieFileNode* p)
{
	memcpy(name, p->name,NameLength);
	size = p->size;
}

/***************************************************************
movieFileNode::isBeforeThan 
return : 0 equal
<0 this file node earler than pname
>0 this file node later than pname;
*/ 
int movieFileNode::compareName(char* pname)
{
	return strncmp(this->name, pname, strlen(this->name));
}

/***************************************************************
movieFileNode::setName
*/
void movieFileNode::setName(char* nameWithExtention)
{	
	if(nameWithExtention == NULL)
		return;
	int len = strstr(nameWithExtention, MovieFileExtend) - nameWithExtention;
	if (len <=0)
		return ;
	memset(name, 0, NameLength);
	strncpy(name, nameWithExtention, len);
}

/***************************************************************
movieFileNode::isMovieFile
*/
bool movieFileNode::isMovieFile(char *namep)
{
	return (namep != NULL)&&(strstr(namep, MovieFileExtend) > 0);	
}

void movieFileNode::getShortName(char* shortname, int length, char* fullPath)
{
	char* name = fullPath;
	char* p = NULL;
	while((p = strstr(name, "/")) > 0)
	{
		name = p + 1;
	}
	if(name > 0)
	{
		char* tt;
		if(strlen(name) > 0)
		{
			if(strlen(name) > 10)
			{
				strncpy(shortname, name + 2, length);
				tt = index(shortname, '-');
				if(tt > 0)
					*tt = '/';
				tt = index(shortname, '-');
				if(tt > 0)
					*tt = '/';
				tt = index(shortname, '-');
				if(tt > 0)
					*tt = ' ';
				tt = index(shortname, '-');
				if(tt > 0)
					*tt = ':';
				tt = index(shortname, '-');
				if(tt > 0)
					*tt = ':';
			}
			else
			{
				strncpy(shortname, name, length);
			}
			
		}
		if((p = strstr(shortname, MovieFileExtend)) > 0)
		{
			//printf("---cut \n");
			p[0] = 0;
		}
	}
}

void movieFileNode::getShortName
	(char* tmp
	, int length)
{
	//skip year, got lenth
	char* tt;
	if(strlen(name) > 0)
	{
		if(strlen(name) > 10)
		{
			strncpy(tmp, name + 2, length);
			tt = index(tmp, '-');
			if(tt > 0)
				*tt = '/';
			tt = index(tmp, '-');
			if(tt > 0)
				*tt = '/';
			tt = index(tmp, '-');
			if(tt > 0)
				*tt = ' ';
			tt = index(tmp, '-');
			if(tt > 0)
				*tt = ':';
			tt = index(tmp, '-');
			if(tt > 0)
				*tt = ':';
		}
		else
		{
			strncpy(tmp, name, length);
		}
		
	}
	
	//snprintf(tmp,length,"%s",name + 5);
}

bool movieFileNode::checkShortName
	(char* tmp
	, int length)
{
	char tmp1[length+1];
	char* tt;
	memset(tmp1, 0, length+1);

	if(strlen(name) > 0)
	{
		if(strlen(name) > 10)
		{
			strncpy(tmp1, name + 2, length);
			tt = index(tmp1, '-');	
			if(tt > 0)
				*tt = '/';
			tt = index(tmp1, '-');
			if(tt > 0)
				*tt = '/';	
			tt = index(tmp1, '-');
			if(tt > 0)
				*tt = ' ';
			tt = index(tmp1, '-');
			if(tt > 0)
				*tt = ':';
			tt = index(tmp1, '-');
			if(tt > 0)
				*tt = ':';
		}
		else
		{
			strncpy(tmp1, name, length);
		}
		
	}
	
	return (strncmp(tmp, tmp1, length) == 0);
}

/*
void movieFileNode::removeMovie()
{
	char tmp[256];
	sprintf(tmp,"%s%s",name, MovieFileExtend);
	printf("movieFileNode::removeMovie() %s\n", tmp);
	remove(tmp);
	sprintf(tmp,"%s%s",name, PosterFileExtend);
	remove(tmp);
}


void movieFileNode::mvMovieToMarked()
{
	char* pFileName = strstr(name, POOL_PATH);
	if(pFileName > 0)
	{
		pFileName += strlen(POOL_PATH);
	}
	else
		return;
	char tmp[256];
	char tmp1[256];
	sprintf(tmp,"%s%s",name, MovieFileExtend);
	sprintf(tmp,"%s%s%s",MARKED_PATH,pFileName, MovieFileExtend);
	rename(tmp, tmp1);
	sprintf(tmp,"%s%s",name, PosterFileExtend);
	sprintf(tmp,"%s%s%s",MARKED_PATH,pFileName, PosterFileExtend);
	rename(tmp, tmp1);
}

void movieFileNode::mvMovieToCircle()
{
	char* pFileName = strstr(name, MARKED_PATH);
	if(pFileName > 0)
	{
		pFileName += strlen(MARKED_PATH);
	}
	else
		return;
	char tmp[256];
	char tmp1[256];
	sprintf(tmp,"%s%s",name, MovieFileExtend);
	sprintf(tmp,"%s%s%s",POOL_PATH,pFileName, MovieFileExtend);
	rename(tmp, tmp1);
	sprintf(tmp,"%s%s",name, PosterFileExtend);
	sprintf(tmp,"%s%s%s",POOL_PATH,pFileName, PosterFileExtend);
	rename(tmp, tmp1);
}

void movieFileNode::cpyMovieToInternal()
{
	char* pFileName = strstr(name, MARKED_PATH);
	if(pFileName > 0)
	{
		pFileName += strlen(MARKED_PATH);
	}
	else
	{
		pFileName = strstr(name, POOL_PATH);
		if(!(pFileName > 0))
			return;
		else
		{
			pFileName += strlen(POOL_PATH);
		}
	}
		
	char tmp[256];
	char tmp1[256];
	sprintf(tmp,"%s%s",name, MovieFileExtend);
	sprintf(tmp,"%s%s%s",Internal_PATH,pFileName, MovieFileExtend);
	rename(tmp, tmp1);
	sprintf(tmp,"%s%s",name, PosterFileExtend);
	sprintf(tmp,"%s%s%s",Internal_PATH,pFileName, PosterFileExtend);
	rename(tmp, tmp1);
}
*/

/***************************************************************
CWatchThread::CWatchThread
*/

CWatchThread::CWatchThread
	(const char* watchPath
	,UINT32 mask
	):CThread(watchPath, CThread::NORMAL, 0, NULL)
	,_watchPath(watchPath)
	,_mask(mask)
{	
	memset(_buf, 0, sizeof(_buf));
}
CWatchThread::~CWatchThread()
{

};

void CWatchThread::ToStart()
{
//#ifndef noupnp
	_inotify_fd = inotify_init();
	_watch_d = inotify_add_watch(_inotify_fd, _watchPath ,_mask);
	//struct inotify_event *event = NULL;
	this->Go();
//#endif
}
void CWatchThread::ToStop()
{
//#ifndef noupnp
	this->Stop();
	inotify_rm_watch(_inotify_fd, _watch_d);
//#endif
}

void CWatchThread::main(void *)
{
	//_inotify_fd = inotify_init();
	//_watch_d = inotify_add_watch(_inotify_fd, _watchPath ,_mask);
	struct inotify_event *event = NULL;
	while(1)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(_inotify_fd, &fds);
		if(select(_inotify_fd + 1, &fds, NULL, NULL, NULL) > 0)
		{
			int len, index = 0;
			while(((len = read(_inotify_fd, _buf, sizeof(_buf))) < 0)&&(errno == EINTR));
			while(index < len)
			{
				event = (struct inotify_event*)(_buf + index);
				inotify_event_handle(event);
				index += sizeof(struct inotify_event)+event->len;
			}
		}
	}
}
void CWatchThread::inotify_event_handle
	(struct inotify_event *event
	)
{};

/***************************************************************
CDirWatchThread::CDirWatchThread
*/
void CDirWatchThread::main(void * p)
{
	_pFileNameQueue->PreReadDir(0);
	CWatchThread::main(p);	
}

CDirWatchThread::CDirWatchThread
	(const char* watchPath
	): CWatchThread(watchPath, IN_CLOSE_WRITE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM)
	,_pFileNameQueue(0)
{
	_pFileNameQueue = new FileQueueControl
		(_watchPath
		, MAXFILE_IN_TMP_LIST
		, FreeSpaceThreshold);
	_lock = new CMutex("CDirWatchThread mutex");
}
CDirWatchThread::~CDirWatchThread()
{
	delete _pFileNameQueue;
	delete _lock;
}

void CDirWatchThread::ResetQueue()
{
	_pFileNameQueue->reset();
}
void CDirWatchThread::inotify_event_handle
		(struct inotify_event *event)
{
	if((event->mask & IN_CLOSE_WRITE) > 0)
	{
		printf("close write : %s\n", event->name);
	}
	if((event->mask & IN_DELETE) > 0)
	{
		printf("delete : %s\n", event->name);
	}
	if((event->mask & IN_MOVED_TO) > 0)
	{
		printf("move in : %s\n", event->name);
	}
	if((event->mask & IN_MOVED_FROM) > 0)
	{
		printf("move out : %s\n", event->name);
	}
}


/***************************************************************
CCircleWatchThread
*/
void CCircleWatchThread::OnMarkEvent(bool bManual)
{
	_lock->Take();
	if((movieFileNode::isMovieFile(_LastFile))&&(_markCount == 0))
	{	
		_lock->Give();
		MoveFile(_LastFile);
		memset(_LastFile, 0, sizeof(_LastFile));
		_lock->Take();
	}
	_markCount = 2;
	_bManualMark = bManual;
	_lock->Give();
}

bool CCircleWatchThread::inMarking()
{
	_lock->Take();
	bool rt = (_markCount > 0);
	_lock->Give();
	return rt;
};

void CCircleWatchThread::ClearMarkState()
{
	_lock->Take();
	_markCount  = 0;
	_lock->Give();
}

void CCircleWatchThread::SetStorageSize(long long size)
{
	printf("--- setCircleStorageSize: %lld\n", size);
	_pFileNameQueue->SetSizeLimitateion(size);
}

void CCircleWatchThread::main(void * p)
{
	memset(_LastFile, 0, sizeof(_LastFile));
	CDirWatchThread::main(p);
}

void CCircleWatchThread::inotify_event_handle
	(struct inotify_event *event)
{
	printf("CCircleWatchThread::inotify_event_handle  :%d\n", event->mask);
	if(movieFileNode::isMovieFile(event->name))
	{
		if((event->mask & IN_CLOSE_WRITE) > 0)
		{
			if(_markCount > 0)
			{
				_lock->Take();
				_markCount --;
				_lock->Give();
				MoveFile(event->name);
			}
			else
			{
				memset(_LastFile, 0, sizeof(_LastFile));
				strcpy(_LastFile, event->name);
				_pFileNameQueue->addToQueue(event->name);
			}
		}
		if((event->mask & IN_DELETE) > 0)
		{
			_pFileNameQueue->removeFromQueue(event->name);
		}
		if((event->mask & IN_MOVED_TO) > 0)
		{
			_pFileNameQueue->addToQueue(event->name);
		}
		if((event->mask & IN_MOVED_FROM) > 0)
		{
			_pFileNameQueue->removeFromQueue(event->name);
		}
	}
	//CDirWatchThread::inotify_event_handle(event);
}

int CCircleWatchThread::MoveFile(char *filename)
{	
	//char *filename;
	char tmp[1024];
	char tmp1[1024];
	
	//lename = CMarkFileQueueThread::getInstance()->getLastSavedMp4();
	if(filename == NULL)
		return -2;
	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%s%s", POOL_PATH, filename);
	
	memset(tmp1, 0, sizeof(tmp1));
	sprintf(tmp1, "%s%s", MARKED_PATH, filename);

	if(::rename(tmp, tmp1) < 0)
	{
		printf("---move to marked dir error: %s\n", tmp);
		return -1;
	}
	else
	{
#ifdef GSensorTestThread
		if(_bManualMark)
		{
			//int fd = open(tmp1, O_RDONLY);
			int r = chmod(tmp1, S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
			//close(fd);
			//printf("---fchmod \"%s\"rt : %d, %d\n", tmp1, r, errno);
			_bManualMark = false;
		}
#endif
		memcpy(tmp + strlen(tmp) - 4,".jpg", 4);
		memcpy(tmp1 + strlen(tmp1) - 4,".jpg", 4);
		if(::rename(tmp, tmp1) < 0)
		{
			printf("---move poster to marked dir error: %s\n", tmp);
		}else
		{
			printf("---move poster to marked dir: %s\n", filename);
		}
		return 0;
	}
}

/***************************************************************
CMarkWatchThread
*/
void CMarkWatchThread::SetStorageSize(long long size)
{
	//printf("--- setMarkedStorageSize: %lld", size);
	_pFileNameQueue->SetSizeLimitateion(size);
}

void CMarkWatchThread::inotify_event_handle
	(struct inotify_event *event)
{
	printf("CMarkWatchThread::inotify_event_handle  :%d\n", event->mask);
	if(movieFileNode::isMovieFile(event->name))
	{
		if((event->mask & IN_CLOSE_WRITE) > 0)
		{
		}
		if((event->mask & IN_DELETE) > 0)
		{
			_pFileNameQueue->removeFromQueue(event->name);
		}
		if((event->mask & IN_MOVED_TO) > 0)
		{
			_pFileNameQueue->addToQueue(event->name);
		}
		if((event->mask & IN_MOVED_FROM) > 0)
		{
			_pFileNameQueue->removeFromQueue(event->name);
		}
	}
}

/***************************************************************
SDWatch thread
*/
SDWatchThread* SDWatchThread::_pInstance = NULL;
static const char* tfMountPoint = "mmcblk0p1";
void SDWatchThread::destroy()
{
	if(_pInstance)
	{	
		_pInstance->ToStop();	
		delete _pInstance;
		_pInstance = NULL;
	}
}
void SDWatchThread::create()
{
	if(!_pInstance)
	{
		_pInstance = new SDWatchThread(); 
		_pInstance->ToStart();		
	}
}

void SDWatchThread::ToStop()
{
	StopDirWatch();
	CWatchThread::ToStop();
	_bStorageReady = false;
}

/*
int SDWatchThread::propertyWaitCallBack(const char *key, const char *value)
{
	_pInstance->propertyChanged();
	CUpnpThread* p = CUpnpThread::getInstance();
	if(p != NULL)
	{
		p->propertyChanged();
	}
	return 0;
}

void SDWatchThread::propertyChanged()
{
	_pTmpPropertyChanged->Give();
} */

void SDWatchThread::OnMarkEvent(bool bManual)
{	
	if(_pCircleWatch)
	{
		_pCircleWatch->OnMarkEvent(bManual);		
	}
}

void SDWatchThread::CancelMarkState()
{
	if(_pCircleWatch)
	{
		_pCircleWatch->ClearMarkState();		
	}
}

bool SDWatchThread::inMarking()
{	
	if(_pCircleWatch)
	{
		return _pCircleWatch->inMarking();		
	}
	return false;
}

void SDWatchThread::inotify_event_handle(struct inotify_event *event)
{
	if(((event->mask & IN_CREATE) > 0)&&(strcmp(event->name, tfMountPoint) == 0))
	{
		// need judge sd mounted, wait at mount status.
		char tmp[256];
		int count = 0;
		while(count < 10)
		{
			memset(tmp, 0, sizeof(tmp));
			agcmd_property_get(SotrageStatusPropertyName, tmp, "default");
			if(strcmp(tmp, "mount_ok") == 0)
				break;
			else
			{
				count++;
				sleep(1);				
			}
		}
		if(count >= 10)
		{
			printf("wait mount time out : %d seconds \n", count);
			return;
		}		
		if(CheckSDStatus())
		{
			if(!_bStorageReady)
			{
				StartDirWatch();
				_bStorageReady = true;
				IPC_AVF_Client::getObject()->onTFReady(_bStorageReady);
			}
		}						
	}
	if(((event->mask & IN_DELETE) > 0)&&(strcmp(event->name, tfMountPoint) == 0))
	{
		printf("------ tf IN_DELETE \n");
		if(_bStorageReady)
		{
			StopDirWatch();
			_bStorageReady = false;
			IPC_AVF_Client::getObject()->onTFReady(_bStorageReady);
		}
	}
	if(((event->mask & IN_UNMOUNT) > 0)&&(strcmp(event->name, tfMountPoint) == 0))
	{
		printf("------ tf UNMOUNT \n");
	}
	
}

void SDWatchThread::main(void *p)
{	
	if(CheckSDStatus())
	{
		if(!_bStorageReady)
		{
			StartDirWatch();
			_bStorageReady = true;
		}	
	}
	IPC_AVF_Client::getObject()->onTFReady(_bStorageReady);
	CWatchThread::main(p);
}

void SDWatchThread::StartDirWatch()
{
	if(_pCircleWatch)
	{	
		_pCircleWatch->SetStorageSize(_storageSize*CircleDirPercentage/100);
		_pCircleWatch->ToStart();
	}
	if(_pMarkedWatch)
	{
		_pMarkedWatch->SetStorageSize(_storageSize*MarkDirPercentage/100);
		_pMarkedWatch->ToStart();
	}
}

void SDWatchThread::StopDirWatch()
{
	if(_pCircleWatch)
	{
		_pCircleWatch->ResetQueue();
		_pCircleWatch->ToStop();		
	}
	if(_pMarkedWatch)
	{
		_pMarkedWatch->ResetQueue();
		_pMarkedWatch->ToStop();		
	}
}

bool SDWatchThread::ChectDirExist()
{
	bool  rt = true;
	DIR *dir;
	dir = opendir(MARKED_PATH);
	if(dir == NULL)
	{		
		dir = opendir(POOL_PATH);
		if(dir != NULL)
		{
			closedir(dir);
			int status;
			status = mkdir(MARKED_PATH, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
			dir = opendir(MARKED_PATH);
			if(dir == NULL)
			{
				// show ui erro TBD
				printf("--creat mark dir failed:%d\n",status);
				rt = false;
			}
			else
				closedir(dir);
		}
		else
		{
			printf("---ChectDirExist : can't open %s, \n", POOL_PATH);
			rt = false;
		}
	}
	else
		closedir(dir);
	return rt;
}

bool SDWatchThread::GetSpaceInfor(UINT64 *total, UINT64 *free)
{
	char tmp[256];
	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%s%s", _watchPath, tfMountPoint);
	int					ret_val;
    struct statfs		fs;
	ret_val = statfs(tmp, &fs);
	//printf("statfs(%s)=%d.\n", dir, ret_val);
	if (ret_val >= 0) {
		*total = fs.f_blocks;
		*total *= fs.f_bsize;
		*free = fs.f_bfree;
		*free *= fs.f_bsize; 
		//printf("--total : %lld, free : % lld\n", *total, *free);
	} else {
		*total = 0;
		*free = 0;
	}
	return (ret_val >= 0);
}

char* SDWatchThread::GetNextFile(char *currentMovie)
{
	char* rt = NULL;
	FileQueueControl *pQueue = _pCircleWatch->getQueue();
	if( pQueue != NULL)
		rt = pQueue->getNextFile(currentMovie);
	if(rt == NULL)
	{
		pQueue = _pMarkedWatch->getQueue();
		if( pQueue != NULL)
			rt = pQueue->getNextFile(currentMovie);
	}
	//printf("---next file :%s \n", rt);
	return rt;
}

bool SDWatchThread::CheckSDStatus()
{
	bool rt = true;
	char tmp[256];
	memset(tmp, 0, sizeof(tmp));
	rt = ChectDirExist();
	printf("++++++check dir exist : %d\n", rt);
	if(rt)
	{
		//unsigned long long limitation;
		memset(tmp, 0, sizeof(tmp));
		agcmd_property_get(SotrageSizePropertyName, tmp, "0");
  		_storageSize = atoll(tmp);
		while(_storageSize == 0)
		{
			sleep(1);
			agcmd_property_get(SotrageSizePropertyName, tmp, "0");
  			_storageSize = atoll(tmp);
		}
		if(_storageSize < StorageSizeLimitateion)
		{
			// UI notify TBD
			printf("--- insert a too small storage, less than 1GB.\n");
		}
		memset(tmp, 0, sizeof(tmp));
		agcmd_property_get(SotrageFreePropertyName, tmp, "0");
		_freesize = atoll(tmp);
	}			
	return rt;
}

SDWatchThread::SDWatchThread
	(): CWatchThread("/tmp/",IN_UNMOUNT|IN_CREATE|IN_DELETE)
	,_bStorageReady(false)
	,_pCircleWatch(NULL)
	,_pMarkedWatch(NULL)
{
#ifndef noupnp
	_pCircleWatch = new CCircleWatchThread; 
	_pMarkedWatch = new CMarkWatchThread;
#endif
}

SDWatchThread::~SDWatchThread()
{
#ifndef noupnp
	delete _pMarkedWatch;
	delete _pCircleWatch;
#endif
}



/***************************************************************
CBackupDirManager
*/
CBackupDirManager* CBackupDirManager::_pInstance = NULL;
CBackupDirManager::CBackupDirManager
	(char* path):_path(path)
{
	GetBackupDirSize();
	_fileQueue = new FileQueueControl(_path, 512, _total);
	_fileQueue->PreReadDir(0);
}

CBackupDirManager::~CBackupDirManager()
{

}

int CBackupDirManager::GetBackupDirSize()
{
	int					ret_val;
    struct statfs		fs;
	ret_val = statfs(_path, &fs);
	if (ret_val >= 0) {
		/*printf("f_type = %x\n", fs.f_type);
		printf("f_bsize = %ld\n", fs.f_bsize);
		printf("f_blocks = %ld\n", fs.f_blocks);
		printf("f_bfree = %ld\n", fs.f_bfree);
		printf("f_bavail = %ld\n", fs.f_bavail);
		printf("f_files = %ld\n", fs.f_files);
		printf("f_ffree = %ld\n", fs.f_ffree);
		printf("f_fsid = %d\n", fs.f_fsid);
		printf("f_namelen = %ld\n", fs.f_namelen);*/
		_total = fs.f_blocks;
		_total *= fs.f_bsize;
		_freeSpace = fs.f_bfree;
		_freeSpace *= fs.f_bsize; 
		//printf("--backup dir, total : %lld, free : % lld\n", _total, _freeSpace);
	} else {
		_total = 0;
		_freeSpace = 0;
	}
	return (ret_val >= 0);
}

UINT64 CBackupDirManager::getFreeSpace()
{
	GetBackupDirSize();
	return _freeSpace;
}
FileQueueControl* CBackupDirManager::getQueue()
{
	_fileQueue->PreReadDir(0);
	return _fileQueue;
}
int CBackupDirManager::GetBackupDirMovieNum()
{
	return _fileQueue->getFileNumberInQueue();
}

int CBackupDirManager::GetBackupDirFileList(char** ppList, int num)
{

}


/*
	struct stat stat0, stat1;
	_locker->Take();
	if(!_limitation)
	{
		_locker->Give();
		return false;
	}
		
	int i = 0;  
	while(i < _NodeNumber)
	{
		int ret = strcmp(name, _MoviefileList[(i + _headIndex)% MAXFILE_IN_MARKED_LIST].name);
		if(ret == 0)
		{
			_locker->Give();
			return false;
		}
		i++;
	}	
	
	char tmp[512];
	while(_NodeNumber >= MAXFILE_IN_MARKED_LIST)
	{
		//remove oldest file;		
		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "%s%s", MARKED_PATH, _MoviefileList[_headIndex].name);				
		stat(tmp, &stat1);
		int ret = remove(tmp);
		//printf("remove oldest marked file, because file number overflow : %d\n", ret);
		if(ret == 0)
		{
			_headIndex = (_headIndex + 1)% MAXFILE_IN_MARKED_LIST;
			_NodeNumber --;	

			_queuedSize -= stat1.st_size;
		}
	}
	
	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%s%s", MARKED_PATH, name);
	if(stat(tmp, &stat0)!= 0)
	{
		_locker->Give();
		return false;
	}

	if(stat0.st_size > _limitation)
	{
		_locker->Give();
		return false;
	}
	
	while(_queuedSize + stat0.st_size > _limitation)
	{
		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "%s%s", MARKED_PATH, _MoviefileList[_headIndex].name);				
		stat(tmp, &stat1);
		int ret = remove(tmp);	
		//printf("remove :%s, : %d\n",tmp, ret);
		if(ret == 0)
		{		
			_headIndex = (_headIndex + 1)% MAXFILE_IN_MARKED_LIST;
			_NodeNumber --;	
			_queuedSize -= stat1.st_size;
		}
		else
		{
			_locker->Give();
			return false;
		}
	}
	
	i = 0;  
	while(i < _NodeNumber)
	{
		int ret = strcmp(name, _MoviefileList[(i + _headIndex)% MAXFILE_IN_MARKED_LIST].name);
		if(ret < 0)
			break;
		i++;
	}
	
	if(i < _NodeNumber)
	{
		for(int j = _NodeNumber; j > i; j--)
		{
			memcpy(_MoviefileList[(j + _headIndex)% MAXFILE_IN_MARKED_LIST].name, _MoviefileList[(j + _headIndex -1)% MAXFILE_IN_MARKED_LIST].name, NameLength);
		}
	}	
	memset(_MoviefileList[(i + _headIndex)% MAXFILE_IN_MARKED_LIST].name, 0, NameLength);
	memcpy(_MoviefileList[(i + _headIndex)% MAXFILE_IN_MARKED_LIST].name, name, strlen(name));

	_NodeNumber++;
	_queuedSize += stat0.st_size;
	_locker->Give();
	return TRUE;

}*/


/*
int CUpnpThread::upnp_handle_subscription_request(UpnpSubscriptionRequest *psr_event)
{
	int					ret_val = -1;
	uint32_t				i = 0;
	int					cmp1 = 0;
	int					cmp2 = 0;
	struct dmc_upnp_service_info		*pdmc_sinfo;
	const char				*devUDN = NULL;
	const char				*serviceID = NULL;
	const char				*Sid = NULL;


	_mutex->Take();

	devUDN = UpnpSubscriptionRequest_get_UDN_cstr(psr_event);
	serviceID = UpnpString_get_String(	UpnpSubscriptionRequest_get_ServiceId(psr_event));
	Sid = UpnpSubscriptionRequest_get_SID_cstr(psr_event);
	for (i = 0; i < DMC_MODE_NUM; ++i) {
		//pdmc_sinfo = &pupnp->service[i];
		cmp1 = strcmp(devUDN, pdmc_sinfo->UDN);
		cmp2 = strcmp(serviceID, pdmc_sinfo->id);
		if (cmp1 == 0 && cmp2 == 0) {
			UpnpAcceptSubscription(_upnpDevice_handle,
				devUDN, serviceID,
				pdmc_sinfo->variable_name,
				(const char **)
				pdmc_sinfo->variable_val,
				pdmc_sinfo->variable_count,
				Sid);
			ret_val = 0;
			break;
		}
	}
	_mutex->Give();
	
	return ret_val;
}*/
#if 0
/***************************************************************
CMarkThread
*/
CMarkThread* CMarkThread::pInstance = NULL;
void CMarkThread::create()
{
	if(pInstance == NULL)
	{
		pInstance = new CMarkThread();
		pInstance->Go();
	}
}

void CMarkThread::destroy()
{
	if(pInstance != NULL)
	{
		//_blive = false;
		pInstance->Stop();
		delete pInstance;
	}
}
void CMarkThread::MarkEvent()
{
	_mutex->Take();
	if(_markCount > 0)
	{
		_markCount = 2;
	}
	else
	{	
		_mutex->Give();
		MoveFile();
		_mutex->Take();
		_markCount = 2;
		_signalEvent->Give();
	}
	_mutex->Give();
}

bool CMarkThread::inMarking()
{
	return _markCount;
}

void CMarkThread::FileEvent()
{
	_signalProperty->Give();
}

void CMarkThread::main(void *)
{
	_blive = true;
	//SDWatchThread::create();
	//CMarkFileQueueThread::getInstance();
	while(_blive)
	{
		_signalEvent->Take(-1);
		_mutex->Take();
		while(_markCount > 0)
		{
			_mutex->Give();
			_signalProperty->Take(-1);
			if(MoveFile() == 0)
			{
				_mutex->Take();
				_markCount--;
				_mutex->Give();
			}
			_mutex->Take();
		}
		_mutex->Give();
	}
	//CMarkFileQueueThread::destroy();
	//SDWatchThread::destroy();
}

int CMarkThread::MoveFile()
{
	
	char *filename;
	char tmp[1024];
	char tmp1[1024];
	
	filename = CMarkFileQueueThread::getInstance()->getLastSavedMp4();
	if(filename == NULL)
		return -2;
	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%s%s", POOL_PATH, filename);
	
	memset(tmp1, 0, sizeof(tmp1));
	sprintf(tmp1, "%s%s", MARKED_PATH, filename);

	if(::rename(tmp, tmp1) < 0)
	{
		printf("---move to marked dir error: %s\n", tmp);
		return -1;
	}
	else
	{
		
		//printf("---move to marked dir: %s\n", filename);

		// move poster file
		memcpy(tmp + strlen(tmp) - 4,".jpg", 4);
		memcpy(tmp1 + strlen(tmp1) - 4,".jpg", 4);
		if(::rename(tmp, tmp1) < 0)
		{
			printf("---move poster to marked dir error: %s\n", tmp);
		}else
		{
			printf("---move poster to marked dir: %s\n", filename);
		}		
		return 0;
	}
}

CMarkThread::CMarkThread
	(): CThread("CMarkThread", CThread::NORMAL, 0, NULL)
{
	
	_signalEvent = new CSemaphore(0, 1);
	_signalProperty = new CSemaphore(0, 1);
	_mutex = new CMutex();
}

CMarkThread::~CMarkThread()
{
	delete	_signalEvent;
	delete  _signalProperty;
	delete	_mutex;
}

/***************************************************************
CMarkFileQueueThread
*/
CMarkFileQueueThread* CMarkFileQueueThread::pInstance = NULL;
int CMarkFileQueueThread::propertyWaitCallBack(const char *key, const char *value)
{
	pInstance->propertyChanged();
	CUpnpThread* p = CUpnpThread::getInstance();
	if(p != NULL)
	{
		p->propertyChanged();
	}
	return 0;
}

void CMarkFileQueueThread::propertyChanged()
{
	_pTmpPropertyChanged->Give();
} 

CMarkFileQueueThread* CMarkFileQueueThread::getInstance()
{
	if(pInstance == NULL)
	{
		pInstance = new CMarkFileQueueThread();
		pInstance->Go();
	}
	return pInstance;
}
void CMarkFileQueueThread::destroy()
{
	if(pInstance != NULL)
	{
		pInstance->Stop();
		delete pInstance;
	}
}

void CMarkFileQueueThread::main(void *)
{
	
	CheckSDStatus();	
	printf("--check sd status : %lld\n", FileQueueControl::GetStorageFreeSpace());
	struct agcmd_property_wait_info_s *pwait_info;
	pwait_info = agcmd_property_wait_start(LastFilePropertyName, CMarkFileQueueThread::propertyWaitCallBack);
	while(1)
	{
		_pTmpPropertyChanged->Take(-1);		
		if(CheckSDStatus())
		{
			if(checkToQueue())
			{
				if(CMarkThread::getInstance()->inMarking())
					CMarkThread::getInstance()->FileEvent();
				else
				{
					addToCircleQueue(getLastSavedMp4());
				}
			}
		}
	}
	agcmd_property_wait_stop(pwait_info);
}
bool CMarkFileQueueThread::AddToMarkedQueue(char* name)
{
	if(movieFileNode::isMovieFile(name))
	{
		return _pFileNameQueueForMarked->ChectSizeAndNumber(name);
	}
	else 
		return false;	
}



bool CMarkFileQueueThread::CheckSDStatus()
{
	//read sd property
	char tmp[256];
	memset(tmp, 0, sizeof(tmp));
	agcmd_property_get(SotrageStatusPropertyName, tmp, "default");
	if(strcmp(tmp, "mount_ok") != 0)
	{
		if(_bStorageReady)
		{
			if(_pFileNameQueueForCircle)
				delete _pFileNameQueueForCircle;
			if(_pFileNameQueueForMarked)
				delete _pFileNameQueueForMarked;
			_pFileNameQueueForCircle = NULL;
			_pFileNameQueueForMarked = NULL;
		}
		_bStorageReady = false;
	}
	else
	{	
		if(!_bStorageReady)
		{
			unsigned long long limitation;
			memset(tmp, 0, sizeof(tmp));
			agcmd_property_get(SotrageSizePropertyName, tmp, "0");
	  		limitation = atoll(tmp);
			limitation = (limitation / 2);

			

			DIR *dir;
			dir = opendir(MARKED_PATH);
			if(dir == NULL)
			{		
				dir = opendir(POOL_PATH);
				if(dir != NULL)
				{
					closedir(dir);
					int status;
					status = mkdir(MARKED_PATH, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
					dir = opendir(MARKED_PATH);
					if(dir == NULL)
					{
						printf("--creat dir failed:%d\n",status);
						_bStorageReady = false;
					}
					else
						closedir(dir);
				}
			}
			else
				closedir(dir);
			_bStorageReady = TRUE;
			
			memset(tmp, 0, sizeof(tmp));
			agcmd_property_get(SotrageSizePropertyName, tmp, "0");
			limitation = atoll(tmp);
			limitation = (limitation / 2);
			printf("--- limitation: %lld.\n", limitation);
			if(limitation < FreeSpaceThreshold)
			{
				printf("--- insert a too small storage.\n");
				//return;
			}
			_pFileNameQueueForCircle = new FileQueueControl(POOL_PATH, MAXFILE_IN_TMP_LIST, limitation - FreeSpaceThreshold);
			_pFileNameQueueForCircle->SetCheckFreeSpace(TRUE);
			_pFileNameQueueForCircle->PreReadDir(0);

			_pFileNameQueueForMarked = new FileQueueControl(MARKED_PATH, MAXFILE_IN_MARKED_LIST, limitation);	
			_pFileNameQueueForMarked->PreReadDir(0);
			
		}
		unsigned long long freesize;
		memset(tmp, 0, sizeof(tmp));
		int rt = agcmd_property_get(SotrageFreePropertyName, tmp, "0");
		if(rt == 0)
			freesize = atoll(tmp);
		else
			freesize = -1;
		FileQueueControl::setFreeSpace(freesize);
	}	
	return _bStorageReady;

}

bool CMarkFileQueueThread::addToCircleQueue(char* name)
{
	//printf("-- add to circle queue: %s\n", name);
	if(movieFileNode::isMovieFile(name))
	{
		return _pFileNameQueueForCircle->ChectSizeAndNumber(name);
	}
	else 
		return false;
}


bool CMarkFileQueueThread::checkToQueue()
{
	char filename[256];
	memset(filename, 0, sizeof(filename));
	agcmd_property_get(LastFilePropertyName, filename, "default");
	if(strstr(filename, MovieFileExtend) != 0)
	{
		int i;
		for(i = 0; i < lastfileNumber ; i++)
		{
			if(strcmp(lastmovies[i], filename) == 0)
				break;
		}
		
		if(i >= lastfileNumber)
		{
			for(int j = lastfileNumber -1; j >0  ; j--)
			{
				memcpy(lastmovies[j], lastmovies[j - 1], 256);
			}
			memcpy(lastmovies[0], filename, 256);
			num ++;
			if(num > lastfileNumber)
				num = lastfileNumber;
			
			return TRUE;
		}
	}
	return FALSE;
}


char* CMarkFileQueueThread::getLastSavedMp4()
{
	if(num >= 2)
	{
		/*for(int j = 0; j  <num; j++)
		{	
			printf("in buffer: %d : %s\n", j, lastmovies[j]);
		}*/
		return lastmovies[1];
	}
	else
		return NULL;
}
/***************************************************************
CMarkFileQueueThread::CMarkFileQueueThread
*/
CMarkFileQueueThread::CMarkFileQueueThread
	():CThread("CMarkQueueThread", CThread::NORMAL, 0, NULL)
{
	_pTmpPropertyChanged = new CSemaphore(0, 1);
	memset(lastmovies, 0, sizeof(lastmovies));
	num = 0;
	index = 0;
	_queuedSize = 0;
	_limitation = 0;
	_NodeNumber = 0;
	_headIndex = 0;
	_locker = new CMutex();
	_pFileNameQueueForCircle = NULL;
	_pFileNameQueueForMarked = NULL;
	_bStorageReady = false;
}

/***************************************************************
CMarkFileQueueThread::~CMarkFileQueueThread
*/
CMarkFileQueueThread::~CMarkFileQueueThread()
{
	if(_pFileNameQueueForCircle)
		delete _pFileNameQueueForCircle;
	if(_pFileNameQueueForMarked)
		delete _pFileNameQueueForMarked;
	delete _pTmpPropertyChanged;
	delete _locker;
}
///backup
	switch (EventType) {
	case UPNP_EVENT_SUBSCRIPTION_REQUEST:	
		printf("-- event subscription!!!\n");
		//ret_val = pUpnpThread->upnp_handle_subscription_request((UpnpSubscriptionRequest *)Event);
		break;

	case UPNP_CONTROL_GET_VAR_REQUEST:
		/*ret_val = dmc_upnp_handle_getvar_request(
			(struct dmc_upnp_info *)Cookie,
			(UpnpStateVarRequest *)Event);*/
		break;

	case UPNP_CONTROL_ACTION_REQUEST:
		/*ret_val = dmc_upnp_handle_action_request(
			(struct dmc_upnp_info *)Cookie,
			(UpnpActionRequest *)Event);*/
		break;

	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	case UPNP_DISCOVERY_SEARCH_RESULT:
	case UPNP_DISCOVERY_SEARCH_TIMEOUT:
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	case UPNP_CONTROL_ACTION_COMPLETE:
	case UPNP_CONTROL_GET_VAR_COMPLETE:
	case UPNP_EVENT_RECEIVED:
	case UPNP_EVENT_RENEWAL_COMPLETE:
	case UPNP_EVENT_SUBSCRIBE_COMPLETE:
	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
		/*AG_LOG_ERR("%s(%d, %p, %p) not supported!\n",
			__func__, EventType, Event, Cookie);*/
		break;

	default:
		/*AG_LOG_ERR("%s(%d, %p, %p) unknown!\n",
			__func__, EventType, Event, Cookie);*/
		break;
	}

	return ret_val;
			/*while(_queueNum > 0)
		{
			_mutex->Take();
			type = _typeQueue[_queueIndex];
			_queueNum--;
			_queueIndex++;
			if(_queueIndex >= 10)
				_queueIndex = 0;
			_mutex->Give();
			//if(type == property_type_wifi)*/
#endif
