/*****************************************************************************
 * VDBClient.h
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 - 2013, linnsong.
 *
 *
 *****************************************************************************/

#ifndef __H_class_vdb_client__
#define __H_class_vdb_client__

#include "vdb_cmd.h"
#include "ClinuxThread.h"
#include "class_TCPService.h"

class StringRemoteCamera;

/******************************************************************************
class TCP_VDB_SessionClient
*
*
*****************************************************************************/
class TCP_VDB_SessionClient : public StandardTcpClient
{
public:
	TCP_VDB_SessionClient
    (int rcvBufferLen
     );
	~TCP_VDB_SessionClient();
    virtual int Send(char* buffer, int len);
    bool isFd(int fd){return _fd == fd;};
    bool isConnected(){return _fd > 0;};
protected:
	virtual void onReadBufferProcess(char* buffer, int len, int fd);
	virtual void onFdNew(int fd);
	virtual void onFdClose(int fd);
    
private:
    int _fd;
};



/******************************************************************************
 class TCP_VDB_SessionClient
 *
 *
 *****************************************************************************/
#define VDB_Data_BufferLen 102400
#define VDB_Msg_BufferLen 1024
class TCP_VDB_Session : public TCPRcvDelegate
{
public:
    TCP_VDB_Session(char* address, StringRemoteCamera* camera);
    ~TCP_VDB_Session();
    
    void OnRecievedBuffer(char* buffer, int len, int fd);
	void onFdNew(int fd){};
	void onFdClose(int fd){};
    
    //int GetVDBInfor(int dir);
    //void GetSubNail(int dir, unsigned int clip, double timePoint, unsigned int sequence);
    //void GetAdditonalData(int dir, unsigned int clip, double timePoint);
    //void GetGpsData(int dir, unsigned int clip, double timePoint);
    //void GetGsensorBlock(int dir, unsigned int clip, double timePoint, double range);
    //void GetFirstUrl(int dir, unsigned int clip, double timePoint, char* buffer, int len);
    
    //NSArray* getClips(){return _pClips;};
    
    //void SetDelegate(id<DVB_Session_Delegate> dele){_pDelegate = dele;};
    void SwitchRawDataMsgOn(bool bOn);
private:
    void onCMDResult(char* buffer, int len);
    void onMsg(char* buffer, int len);
    
private:
    TCP_VDB_SessionClient* _pVDBCmdSession;
    //TCP_VDB_SessionClient* _pVDBMsgSession;
    bool  _bMoreNeedRcv;
    char*   _dataBuffer;
    int     _buffSize;
    int     _rcvedLen;
    
    StringRemoteCamera*	_pCamera;
    
    unsigned int _cmdSequence;
};

#endif
