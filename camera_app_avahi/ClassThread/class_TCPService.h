/*****************************************************************************
 * class_TCPService.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2013, linnsong.
 *
 *
 *****************************************************************************/

#ifndef __H_class_TCPService__
#define __H_class_TCPService__

#include "ClinuxThread.h"

#ifdef IOS_OS
using namespace SC;
#endif

class TCPRcvDelegate
{
public:
    TCPRcvDelegate(){};
    ~TCPRcvDelegate(){};
    virtual void OnRecievedBuffer(char* buffer, int len, int fd) = 0;
    virtual void onFdNew(int fd) = 0;
    virtual void onFdClose(int fd) = 0;
private:

};

class StandardTCPService : public CThread
{
public:
    StandardTCPService
        (UINT16 lisenPort
        ,int rcvBufferLen
        ,int maxConnection);
    virtual ~StandardTCPService();

    void SetDelegate(TCPRcvDelegate* dele){_delegate = dele;};
    int BroadCast(char* buffer, int len);
protected:
    virtual void main(void * p);
    virtual void onReadBufferProcess(char* buffer, int len, int fd);
    virtual void onFdNew(int fd);
    virtual void onFdClose(int fd);

private:
    char    *_buffer; //[avahi_cmd_buffer_size];
    UINT16  _lisenPort;
    int     _rcvBufferLen;
    int     _maxConnection;
    int     *_connectedSockets;

    TCPRcvDelegate  *_delegate;
};


class StandardTcpClient : public CThread
{
public:
    StandardTcpClient
        (int rcvBufferLen, char* threadName);
    ~StandardTcpClient();

    bool Connect(char* address, UINT16 port);
    void Disconnect();
    virtual int Send(char* buffer, int len);
    void SetDelegate(TCPRcvDelegate* dele){_delegate = dele;};
    void Stop();
    bool isConnected(){return _bConnected;};

    //void __debugSetBrcv(){_bRcvBool = true;};

protected:
    virtual void main(void * p);
    virtual void onReadBufferProcess(char* buffer, int len, int fd);
    virtual void onFdNew(int fd);
    virtual void onFdClose(int fd);

private:
    char    *_buffer; //[avahi_cmd_buffer_size];
    UINT16  _connectPort;
    int     _rcvBufferLen;
    int     _socket;
    bool    _bConnected;
    TCPRcvDelegate  *_delegate;
    CSemaphore      *_pStopSem;

    //bool            _bRcvBool;
};

class StandardTcpSession;
class TCPSessionService;
/***************************************************************
SessionResponseSendBack
*/
class SessionResponseSendBack
{
public:
    TCPSessionService* pService;
public:
    SessionResponseSendBack(TCPSessionService* service){ pService = service;};
    virtual ~SessionResponseSendBack(){};
    virtual int SendBack(char* data, int len) = 0;
};

/***************************************************************
SessionRcvDelegate
*/
class SessionRcvDelegate
{
public:
    SessionRcvDelegate(){};
    virtual ~SessionRcvDelegate(){};
    virtual void OnRcvData(char* data, int len, SessionResponseSendBack* sendback) = 0;
    virtual void OnDisconnect(SessionResponseSendBack* sendback) = 0;
    virtual void OnConnect(SessionResponseSendBack* sendback) = 0;
};


/***************************************************************
StandardTcpSession
*/
struct SessionDataHead
{
    int length;
    int appendLength;
    char data[120];
};

class StandardTcpSession : public SessionResponseSendBack
{
public:
    StandardTcpSession
        (int fd
        ,int bufferLen
        ,TCPSessionService * service
        ):SessionResponseSendBack(service)
        ,_fd(fd)
        ,_buffSize(bufferLen)
        ,_rcvedLen(0)
        ,_pDelegate(0)
        ,_bSegmentReady(1)
    {
        _buffer = new char[_buffSize];
        _waitLength = sizeof(SessionDataHead);
    };
    virtual ~StandardTcpSession()
    {
        delete[] _buffer;
    };
    void OnRecievedBuffer(char* buffer, int len);
    virtual int SendBack(char* data, int len);

    void SetDelegate(SessionRcvDelegate* dele){_pDelegate = dele;};

    bool fdMatch(int fd){return fd == _fd;};
protected:
    int     _fd;
    char    *_buffer;
    int     _buffSize;
    int     _rcvedLen;
    SessionRcvDelegate  *_pDelegate;
    bool                _bSegmentReady;
    int                 _waitLength;
};


/***************************************************************
TCPSessionService
*/
class TCPSessionService : public StandardTCPService
{
public:
    TCPSessionService
        (UINT16 lisenPort
        ,int rcvBufferLen
        ,int maxConnection
        ,int sessionBufferLen
        //,SessionRcvDelegate* dele
        ):StandardTCPService(lisenPort, rcvBufferLen, maxConnection)
        ,_addedNum(0)
        ,_maxConnection(maxConnection)
        ,_sessionBufferLen(sessionBufferLen)
        ,_pDelegate(0)
    {
        _ppSessionList = new StandardTcpSession*[_maxConnection];
        _lock = new CMutex("TCP session lock");
    };
    ~TCPSessionService()
    {
        for (int i = 0; i < _addedNum; i++) {
            delete _ppSessionList[i];
        }
        delete [] _ppSessionList;
        delete _lock;
    };

    void SetDelegate(SessionRcvDelegate* dele);
    void BroadCast(char* data, int len);
    int sendSessionBack(SessionResponseSendBack* session, char* data, int len);

protected:
    virtual void onReadBufferProcess(char* buffer, int len, int fd);
    virtual void onFdNew(int fd);
    virtual void onFdClose(int fd);

private:
    StandardTcpSession  **_ppSessionList;
    int                 _addedNum;
    int                 _maxConnection;
    int                 _sessionBufferLen;
    SessionRcvDelegate  *_pDelegate;
    CMutex              *_lock;
};

class TCPSessionClient : public StandardTcpClient
{
public:
    TCPSessionClient
        (int rcvBufferLen
        ,int sessionBufferLen
        ,char* name
        );
    ~TCPSessionClient();
    void SetDelegate(SessionRcvDelegate* dele);
    //{_pSession->SetDelegate(dele);};
    virtual int Send(char* buffer, int len);
protected:
    virtual void onReadBufferProcess(char* buffer, int len, int fd);
    virtual void onFdNew(int fd);
    virtual void onFdClose(int fd);


private:
    StandardTcpSession  *_pSession;
    SessionRcvDelegate  *_pDelegate;
    int                 _sessionBufferLen;
    CMutex              *_lock;
};

#endif
