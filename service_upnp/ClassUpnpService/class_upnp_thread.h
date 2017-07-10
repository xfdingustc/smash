/*****************************************************************************
 * class_upnp_thread.h:
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

#ifndef __H_class_upnp_thread__
#define __H_class_upnp_thread__

#include "GobalMacro.h"
#include "ClinuxThread.h"
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ixml.h>
#include <upnp/ithread.h>
#include "Class_PropsEventThread.h"

//
#if UPNP_VERSION < 10800
#define UpnpString_get_String(str) (str)

typedef struct Upnp_Action_Request UpnpActionRequest;
#define UpnpActionRequest_set_ErrCode(pAR, var) \
	do { pAR->ErrCode = var; } while(0)
#define UpnpActionRequest_get_ErrCode(pAR) (pAR->ErrCode)
#define UpnpActionRequest_set_ActionResult(pAR, var) \
	do { pAR->ActionResult = var; } while(0)
#define UpnpActionRequest_get_ActionRequest(pAR) (pAR->ActionRequest)
#define UpnpActionRequest_strcpy_ErrStr(pAR, var) \
	do { snprintf(pAR->ErrStr, sizeof(pAR->ErrStr), "%s", var); } while(0)
#define UpnpActionRequest_get_DevUDN(pAR) (pAR->DevUDN)
#define UpnpActionRequest_get_ServiceID(pAR) (pAR->ServiceID)
#define UpnpActionRequest_get_ActionName(pAR) (pAR->ActionName)

typedef struct Upnp_State_Var_Request UpnpStateVarRequest;
#define UpnpStateVarRequest_strcpy_ErrStr(pSVR, var) \
	do { snprintf(pSVR->ErrStr, sizeof(pSVR->ErrStr), "%s", var); } while(0)
#define UpnpStateVarRequest_get_DevUDN(pSVR) (pSVR->DevUDN)
#define UpnpStateVarRequest_get_ServiceID(pSVR) (pSVR->ServiceID)
#define UpnpStateVarRequest_get_StateVarName(pSVR) (pSVR->StateVarName)
#define UpnpStateVarRequest_set_ErrCode(pSVR, var) \
	do { pSVR->ErrCode = var; } while(0)
#define UpnpStateVarRequest_set_CurrentVal(pSVR, var) \
	do { pSVR->CurrentVal = ixmlCloneDOMString(var); } while(0)

typedef struct File_Info UpnpFileInfo;
#define UpnpFileInfo_set_LastModified(pUFI, var) \
	do { pUFI->last_modified = var; } while(0)
#define UpnpFileInfo_set_IsDirectory(pUFI, var) \
	do { pUFI->is_directory = var; } while(0)
#define UpnpFileInfo_set_IsReadable(pUFI, var) \
	do { pUFI->is_readable = var; } while(0)
#define UpnpFileInfo_set_FileLength(pUFI, var) \
	do { pUFI->file_length = var; } while(0)
#define UpnpFileInfo_set_ContentType(pUFI, var) \
	do { pUFI->content_type = ixmlCloneDOMString(var); } while(0)
#define UpnpFileInfo_get_ContentType(pUFI) (pUFI->content_type)

typedef struct Upnp_Subscription_Request UpnpSubscriptionRequest;
#define UpnpSubscriptionRequest_get_UDN_cstr(pUSR) (pUSR->UDN)
#define UpnpSubscriptionRequest_get_ServiceId(pUSR) (pUSR->ServiceId)
#define UpnpSubscriptionRequest_get_SID_cstr(pUSR) (pUSR->Sid)

#endif /* UPNP_VERSION < 10800 */


/***************************************************************
CUpnpEventHandler
*/
class CUpnpEventHandler : public CPropertyEventHandler
{
public:
	CUpnpEventHandler(){};
	virtual ~CUpnpEventHandler(){};
	virtual int upnp_action_request_handler(UpnpActionRequest *par_event) = 0;
	virtual int upnp_getvar_request_handler(UpnpStateVarRequest *pvr_event) = 0;
	virtual int upnp_subscription_request_handler(UpnpSubscriptionRequest *psr_event) = 0;
	virtual int upnp_event_handler(Upnp_EventType EventType, const void *Event) = 0;

	void setUpnpDeviceHandle(UpnpDevice_Handle handle){_upnpDevice_handle = handle;};
	UpnpDevice_Handle getUpnpDeviceHandle(){return _upnpDevice_handle;};

	virtual void SendHeartBeat() = 0;
	//virtual void EventPropertyUpdate(changed_property_type state) = 0;
private:
	UpnpDevice_Handle 	_upnpDevice_handle;
};

/***************************************************************
CUpnpThread
*/
class CUpnpThread : public CThread, public CPropertyEventHandler
{
public:
	static void Create(char* root, char* description, CUpnpEventHandler *peventProcessor);
	static void Destroy();
	static CUpnpThread* getInstance();
	
	static int upnp_event_handler(Upnp_EventType EventType, void *Event, void *Cookie);
	static int FilePropertyWaitIP(const char *key, const char *value);

	void ipChanged();
	virtual void EventPropertyUpdate(changed_property_type state);
	UpnpDevice_Handle deviceHandle(){return _upnpDevice_handle;};
	void RequireUpnpReboot();

	int CheckIpStatus();
	virtual void Stop();
	//void AddPEH(CPropertyEventHandler* pHandle);
protected:
	virtual void main(void *);

private:
	static CUpnpThread* _pInstance;
	CUpnpThread(const char* upnpWebSvrRoot
					,const char* device_description
					,CUpnpEventHandler *peventProcessor);
	~CUpnpThread();
	int initUpnp();
	void upnp_deinit();
	void UpnpCheck();
	void RebootUpnp();
private:
	CMutex 				*_mutex;
	UpnpDevice_Handle 	_upnpDevice_handle;
	const char*			_svrRoot;
	const char*			_device_description;
	CUpnpEventHandler		*_pEventProcessor;
	bool 				_bInited;
	CSemaphore			*_pTmpPropertyChanged;
	UINT32				_CurrentIP;
	struct agcmd_property_wait_info_s *_pwait_ip;

	bool						_bNeedReboot;

	char 						_wait_ip_prop_name[256];
	bool						_bRun;
	char 						_wifi_prop[256];
};


class movieFileNode
{
public:
	movieFileNode(){memset(name,0,sizeof(name));};
	~movieFileNode(){};
	char	name[NameLength];
	long long size;

	int compareName(char* pname);
	void cpy(movieFileNode* p);

	void setSize(long long s){size = s;};
	void setName(char* nameWithExtention);
	void getShortName(char* tmp, int length);
	bool checkShortName(char* tmp, int length);
	
	static bool isMovieFile(char*);
	static void getShortName(char* shortname, int length, char* fullPath);
	/*
	void removeMovie();
	void mvMovieToMarked();
	void mvMovieToCircle();
	void cpyMovieToInternal();*/
};


class FileQueueControl
{
public:
	FileQueueControl
		(const char* path
		, unsigned long MaxFileNumber
		, unsigned long long maxSize);
	~FileQueueControl();

	int addToQueue(char* fileName);
	int removeFromQueue(char* fileName);
	void reset();
	int ChectSizeAndNumber(char*);

	bool PreReadDir(char* forFile);
	void SetCheckFreeSpace(bool b){_bCheckFreeSpace = b;};
	//static initFreeSpace();
	static long long GetStorageFreeSpace();
	static void setFreeSpace(long long space){_freesize = space;};

	int getFileNumberInQueue()
		{return _addedNodeNumber;};
	movieFileNode* getFileNodeList
		(int& head, int& queueNumber)
	{
		head = _headIndex;
		queueNumber = _nodeQueueDeepth;
		return _pMoviefileList;
	};

	void SetSizeLimitateion(long long Limitation)
		{
			_nodeQueueStorageLimitation = Limitation;
		};
	char* getBasePath(){return _basePath;};

	char* getNextFile(char* path);

	//int getFilePath(char* path, int len, char* shortName);
	int GetFullPath(char* shortName
		, int startSearch
		, int range
		, char* path
		, int len);
	int RemoveMovie(char* shortName, int startSearch, int range);
	int mvToMarkDir(char* shortName, int startSearch, int range);
	int mvToCircleDir(char* shortName, int startSearch, int range);
	int cpyToInternalDir(char* shortName, int startSearch, int range);
	UINT64 fileSize(char* shortName, int startSearch, int range);
	bool HasFile(char* shortName);

	double getCpyPercentage(){return _cppercentage;};
	
private:
	char _basePath[MaxPathStringLength];
	unsigned long _nodeQueueDeepth;
	unsigned long long _nodeQueueStorageLimitation;

	movieFileNode* 		_pMoviefileList;
	int 				_addedNodeNumber;
	int					_headIndex;
	long long			_queuedStorageSize;

	bool				_bCheckFreeSpace;
	CMutex				*_locker;
	static long long 	_freesize;

	char _tmp[256];
	double 				_cppercentage;

private:
	int ShotNameIndex(char* shortName, int startSearch, int range);

};


/***************************************************************
CWatchThread
*/
class CWatchThread : public CThread
{
public:
	CWatchThread(const char* watchPath, UINT32 mask);
	virtual ~CWatchThread();
	virtual void ToStart();
	virtual void ToStop();
protected:	
	virtual void main(void * p);	
	virtual void inotify_event_handle(struct inotify_event *event);
	const char*			_watchPath;
private:
	int					_inotify_fd;
	int					_watch_d;	
	unsigned char 		_buf[1024];
	UINT32				_mask;
};
/***************************************************************
CDirWatchThread
*/
class CDirWatchThread : public CWatchThread
{
public:
	CDirWatchThread(const char* watchPath);
	virtual ~CDirWatchThread();
	FileQueueControl* getQueue(){return _pFileNameQueue;};

	void ResetQueue();
	
protected:
	virtual void main(void *);
	virtual void SetStorageSize(long long size) = 0;
	virtual void inotify_event_handle(struct inotify_event *event);
	FileQueueControl	*_pFileNameQueue;
	CMutex*				_lock;	
	
private:
	int					_inotify_fd;
	int					_watch_d;
	unsigned char 		_buf[1024];

	
};

/***************************************************************
CCircleWatchThread
*/
class CCircleWatchThread : public CDirWatchThread
{
public:
	CCircleWatchThread
		() : CDirWatchThread(POOL_PATH)
		,_markCount(0)
		{};
	~CCircleWatchThread()
		{};
	virtual void SetStorageSize(long long size);
	void OnMarkEvent(bool bManual);
	bool inMarking();
	void ClearMarkState();
	void main(void *);
protected:
	virtual void inotify_event_handle(struct inotify_event *event);
	int MoveFile(char *filename);
	
private:
	char _LastFile[256];
	int _markCount;
	bool _bManualMark;
};

/***************************************************************
CDirWatchThread
*/
class CMarkWatchThread : public CDirWatchThread
{
public:
	CMarkWatchThread() : CDirWatchThread(MARKED_PATH)
		{};
	~CMarkWatchThread()
		{};

	virtual void SetStorageSize(long long size);
protected:
	virtual void inotify_event_handle(struct inotify_event *event);

private:
};

/***************************************************************
CInternalWatchThread

class CInternalWatchThread : public CDirWatchThread
{
public:
	CInternalWatchThread() : CDirWatchThread(MARKED_PATH)
		{};
	~CInternalWatchThread()
		{};

	virtual void SetStorageSize(long long size);
protected:
	virtual void inotify_event_handle(struct inotify_event *event);

private:
};*/
class CBackupDirManager
{
public:
	static void Create()
	{
		if(!_pInstance)
		{
			_pInstance = new CBackupDirManager((char*)Internal_PATH);
		}
	};
	static void Destroy()
	{
		if(_pInstance)
		{
			delete _pInstance;
		}
	}
	static CBackupDirManager* getInstance(){return _pInstance;};
	

	int GetBackupDirSize();
	int GetBackupDirFileNum();
	int GetBackupDirMovieNum();
	int GetBackupDirFileList(char** ppList, int num);
	FileQueueControl* getQueue();//{return _fileQueue;};
	UINT64 getFreeSpace();
private:
	CBackupDirManager(char* path);
	~CBackupDirManager();
	
private:
	static CBackupDirManager* _pInstance;
	char* _path;
	FileQueueControl *_fileQueue;
	UINT64	_total;
	UINT64 	_freeSpace;
};

/***************************************************************
SDWatchThread
*/
/*enum SDWatchEvent
{
	SDWatchEvent_mount = 0,
	SDWatchEvent_unmount,
};

class SDWatchObeserver
{
public:
	SDWatchObeserver(){};
	~SDWatchObeserver(){};
	onWatchEvent(SDWatchEvent event);	
};*/

class SDWatchThread : public CWatchThread
{
public:
	
	static SDWatchThread* getInstance(){return _pInstance;};
	static void destroy();
	static void create();
	virtual void ToStop();
	//static int propertyWaitCallBack(const char *key, const char *value);
	bool inMarking();
	void OnMarkEvent(bool bManual);
	void CancelMarkState();
	bool GetSpaceInfor(UINT64 *total, UINT64 *free);
		
	FileQueueControl* GetCircleQueue(){return _pCircleWatch->getQueue();};
	FileQueueControl* GetMarkedQueue(){return _pMarkedWatch->getQueue();};

	char* GetNextFile(char *currentMovie);
	
protected:
	
	void main(void *);	
	virtual void inotify_event_handle(struct inotify_event *event);
	
private:
	
	SDWatchThread();
	virtual ~SDWatchThread();
	void StartDirWatch();
	void StopDirWatch();
		
	//void propertyChanged();

	bool ChectDirExist();
	bool CheckSDStatus();
	
	static SDWatchThread* 	_pInstance;
	bool					_bStorageReady;
	CSemaphore				*_pTmpPropertyChanged;
	
	unsigned long long		_storageSize;
	unsigned long long 		_freesize;
	unsigned long long		_Limitation;

	CCircleWatchThread* 	_pCircleWatch;
	CMarkWatchThread* 		_pMarkedWatch;

	bool _bWaitMount;
};

#endif

#if 0
/***************************************************************
CMarkThread
*/
class CMarkThread : public CThread
{
public:

	static CMarkThread* getInstance(){return pInstance;};
	static void create();
	static void destroy();

	void MarkEvent();
	void FileEvent();
	bool inMarking();

protected:
	void main(void *);

private:
	CMarkThread();
	~CMarkThread();
	int MoveFile();
	
	
private:
	static CMarkThread* pInstance;
	
	int		_markCount;
	bool	_bInMarkWait;
	bool	_blive;
	CSemaphore	*_signalEvent;
	CSemaphore  *_signalProperty;
	CMutex		*_mutex;	
};

/***************************************************************
CMarkFileQueueThread 512
*/


/***************************************************************
CMarkFileQueueThread
*/
#define lastfileNumber 3
class CMarkFileQueueThread : public CThread
{
public:
	static CMarkFileQueueThread* getInstance();
	static void destroy();

	//bool checkModifiedFile();
	bool checkToQueue();
	char* getLastSavedMp4();

	bool AddToMarkedQueue(char* name);
	bool addToCircleQueue(char* name);
	//bool PreReadMarkedDir();
	void SetStorageLimitation(long long lim){_limitation = lim;};
	
	FileQueueControl* GetMarkedQueue(){return _pFileNameQueueForMarked;};
	FileQueueControl* GetCircleQueue(){return _pFileNameQueueForCircle;};
	
protected:
	void main(void *);

private:
	CMarkFileQueueThread();
	~CMarkFileQueueThread();
	static int propertyWaitCallBack(const char *key, const char *value);
	void propertyChanged();
	bool CheckSDStatus();

private:
	static 				CMarkFileQueueThread* pInstance;
	CSemaphore			*_pTmpPropertyChanged;
	char				lastmovies[lastfileNumber][256];
	int					num;
	int					index;

	movieFileNode 		_MoviefileList[MAXFILE_IN_MARKED_LIST];
	int 				_NodeNumber;
	int					_headIndex;
	long long			_queuedSize;
	long long			_limitation;

	CMutex				*_locker;

	FileQueueControl	*_pFileNameQueueForCircle;
	FileQueueControl	*_pFileNameQueueForMarked;

	bool 				_bStorageReady;

};
#endif 
