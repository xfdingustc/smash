
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <getopt.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GobalMacro.h"
#include "class_TCPService.h"

/***************************************************************
StandardTCPService::StandardTCPService()
*/
StandardTCPService::StandardTCPService
    (UINT16 lisenPort
     ,int rcvBufferLen
     ,int maxConnection
    ):CThread("CCamService", CThread::NORMAL, 0, NULL)
    ,_lisenPort(lisenPort)
    ,_rcvBufferLen(rcvBufferLen)
    ,_maxConnection(maxConnection)
    ,_delegate(NULL)
{
    _buffer = new char[_rcvBufferLen];
    _connectedSockets = new int[_maxConnection];
    for (int i = 0; i < _maxConnection; i++) {
        _connectedSockets[i] = -1;
    }
}

StandardTCPService::~StandardTCPService()
{
    for (int i = 0; i < _maxConnection; i++) {
        if (_connectedSockets[i] > 0) {
            close(_connectedSockets[i]);
        }
    }
    delete[] _connectedSockets;
    delete[] _buffer;
    CAMERALOG("destroyed");
}

int StandardTCPService::BroadCast(char* buffer, int len)
{
    for (int i = 0; i < _maxConnection; i++) {
        if (_connectedSockets[i] > 0) {
            //CAMERALOG(">>>>>>broad cast:%d, %s", _connectedSockets[i], buffer);
            ::send(_connectedSockets[i], buffer, len, MSG_NOSIGNAL);
        }
    }
    return 0;
}

void StandardTCPService::onFdNew(int fd)
{
    //CAMERALOG("StandardTCPService::onFdNew %d", fd);
    if (_delegate) {
        _delegate->onFdNew(fd);
    } else {
        CAMERALOG("accept connection: %d", fd);
    }
}

void StandardTCPService::onFdClose(int fd)
{
    if (_delegate) {
        _delegate->onFdClose(fd);
    } else {
        CAMERALOG("close socket: %d", fd);
    }
}

void StandardTCPService::onReadBufferProcess(char* buffer, int len, int fd)
{
    if (_delegate) {
        _delegate->OnRecievedBuffer(buffer, len, fd);
    } else {
        CAMERALOG("%d, content:%s", fd, buffer);
    }
}
void StandardTCPService::main(void * p)
{
    // init socket, listen
    int _listenSocket;
    int _connectedNum = 0;

    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    memset(&addr,0 ,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_lisenPort);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);

    _listenSocket = socket(AF_INET, SOCK_STREAM,0);
    if (_listenSocket < 0) {
        return;
    }
    int flag = 1;
    if (::setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        CAMERALOG("--!!!!--setopt error");
        close(_listenSocket);
        return;
    }
    if (bind(_listenSocket, (const sockaddr*)&addr, sizeof(addr)) < 0) {
        CAMERALOG("--!!!!--bind error");
        close(_listenSocket);
        return;
    }
    if (listen(_listenSocket, 3) < 0) {
        CAMERALOG("listen error");
        close(_listenSocket);
        return;
    }
    for (int i = 0; i < _maxConnection; i++) {
        _connectedSockets[i] = 0;
    }

    fd_set readfds;
    int Maxfd;
    int newFd;
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(_listenSocket, &readfds);
        Maxfd = _listenSocket + 1;
        for (int i = _connectedNum - 1; i >= 0; i--) {
            if (_connectedSockets[i] == -1) {
                for (int j = i; j < _connectedNum - 1; j++) {
                    _connectedSockets[j] = _connectedSockets[j + 1];
                }
                _connectedSockets[_connectedNum - 1] = 0;
                _connectedNum --;
            } else {
                FD_SET(_connectedSockets[i], &readfds);
                Maxfd = (_connectedSockets[i] > (Maxfd - 1))? (_connectedSockets[i] + 1) : Maxfd;
            }
        }
        for (int i = 0; i < _connectedNum; i++) {
            //CAMERALOG("socket at index %d, %d", i, _connectedSockets[i]);
        }
        //CAMERALOG("--select Maxfd:%d, _connectedNum: %d", Maxfd, _connectedNum);
        if (select(Maxfd, &readfds, NULL, NULL, NULL)) {
            if (FD_ISSET(_listenSocket, &readfds)) {
                newFd = accept(_listenSocket, (sockaddr*)&addr, (socklen_t*)&addr_len);
                if ((newFd) && (_connectedNum < _maxConnection)) {
                    int keepAlive = 1; // ¿ªÆôkeepaliveÊôÐÔ
                    int keepIdle = 60; // Èç¸ÃÁ¬½ÓÔÚ60ÃëÄÚÃ»ÓÐÈÎºÎÊý¾ÝÍùÀ´,Ôò½øÐÐÌ½²â
                    int keepInterval = 5; // Ì½²âÊ±·¢°üµÄÊ±¼ä¼ä¸ôÎª5 Ãë
                    int keepCount = 3; // Ì½²â³¢ÊÔµÄ´ÎÊý.Èç¹ûµÚ1´ÎÌ½²â°ü¾ÍÊÕµ½ÏìÓ¦ÁË,Ôòºó2´ÎµÄ²»ÔÙ·¢.
                    setsockopt(newFd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
                    setsockopt(newFd, SOL_TCP, TCP_KEEPIDLE, (void*)&keepIdle, sizeof(keepIdle));
                    setsockopt(newFd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
                    setsockopt(newFd, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));

                    // set socket to non-block:
                    int flags = fcntl(newFd, F_GETFL, 0);
                    fcntl(newFd, F_SETFL, flags | O_NONBLOCK);

                    //CAMERALOG("---new connect : %d", newFd);
                    _connectedSockets[_connectedNum] = newFd;
                    _connectedNum++;
                    FD_SET(newFd, &readfds);
                    //Maxfd = (newFd > (Maxfd - 1))? (newFd + 1) : Maxfd;
                    this->onFdNew(newFd);
                } else if (newFd > 0) {
                    close(newFd);
                    CAMERALOG("too much client connection: %d", _connectedNum);
                }
            } else {
                for (int i = 0; i < _connectedNum; i++) {
                    if (FD_ISSET(_connectedSockets[i], &readfds)) {
                        bzero(_buffer, _rcvBufferLen);
                        int len = (int)recv(_connectedSockets[i], _buffer, (size_t)_rcvBufferLen, MSG_DONTWAIT | MSG_NOSIGNAL);
                        if (len > 0) {
                            this->onReadBufferProcess(_buffer, len, _connectedSockets[i]);
                        } else {
                            //CAMERALOG("close fd : %d", _connectedSockets[i]);
                            //close(_connectedSockets[i]);
                            CAMERALOG("onFdClose fd : %d >>>", _connectedSockets[i]);
                            this->onFdClose(_connectedSockets[i]);
                            CAMERALOG("onFdClose fd : %d <<<", _connectedSockets[i]);
                            FD_CLR(_connectedSockets[i],&readfds);
                            _connectedSockets[i] = -1;
                        }
                    }
                }
            }
        }
    }
}

/***************************************************************
StandardTcpClient
*/
StandardTcpClient::StandardTcpClient
    (int rcvBufferLen
     ,char* threadName
    ):CThread(threadName, CThread::NORMAL, 0, NULL)
    ,_rcvBufferLen(rcvBufferLen)
    ,_delegate(NULL)
{
    _buffer = new char[_rcvBufferLen];
    _pStopSem = new CSemaphore(0, 1,"tcpClient");
}

StandardTcpClient::~StandardTcpClient()
{
    delete[] _buffer;
    delete _pStopSem;
}

bool StandardTcpClient::Connect(char* address, UINT16 port)
{
    struct sockaddr_in addr;
    //int addr_len = sizeof(addr);
    memset(&addr,0 ,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr=inet_addr(address);
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    //CAMERALOG("--->>>> connect:%s:%d", address, port);
    if (_socket > 0) {
        if (connect(_socket, (sockaddr*)&addr, sizeof(addr)) < 0) {
            //CAMERALOG("--- connect failed : %s:%d", address, port);
            close(_socket);
            _bConnected = false;
        } else {
            //CAMERALOG("------ +++StandardTcpClient connect ok:%d", _socket);
            _bConnected = true;
            // set socket
            //int flags = fcntl(_socket, F_GETFL, 0);
            //fcntl(_socket, F_SETFL, flags|O_NONBLOCK);
            onFdNew(_socket);
            this->Go();
        }
    } else {
        _bConnected = false;
    }
    return _bConnected;
}

void StandardTcpClient::Disconnect()
{
    if (_bConnected) {
        CAMERALOG("---StandardTcpClient::Disconnect : %d", _socket);
        _bConnected = false;
        onFdClose(_socket);
        close(_socket);
        CThread::Stop();
    }
}

void StandardTcpClient::Stop()
{
    //CAMERALOG("----->>>StandardTcpClient::Stop()");
    _pStopSem->Take(-1);
    CThread::Stop();
}

int StandardTcpClient::Send(char* buffer, int len)
{
    if (_bConnected) {
        return send(_socket, buffer, len, MSG_NOSIGNAL);
    }
    return 0;
}

void StandardTcpClient::main(void * p)
{
    fd_set readfds;
    int Maxfd;
    //CAMERALOG("-------+>>>>>> start StandardTcpClient::main : %s", this->getName());
    //_bRcvBool = false;
    while (_bConnected)
    {
        FD_ZERO(&readfds);
        FD_SET(_socket, &readfds);
        Maxfd = _socket + 1;
        int rt = select(Maxfd, &readfds, NULL, NULL, NULL);
        if (rt > 0) {
            if ((FD_ISSET(_socket, &readfds))) {
                //CAMERALOG("---selected");
                int len = (int)recv(_socket, _buffer, _rcvBufferLen, MSG_DONTWAIT | MSG_NOSIGNAL);
                //CAMERALOG("---recv %d", len);
                if (len > 0) {
                    onReadBufferProcess(_buffer, (int)len, _socket);
                } else {
                    FD_CLR(_socket, &readfds);
                    Disconnect();
                    //CAMERALOG("----should auto disconnect?");
                    _pStopSem->Give();
                }
            }
        } else {
            //CAMERALOG("--- SELECT RETURN : %d---", rt);
            if (rt == 0) {
                // time out;
                // count to 3  as disconnect;?
            } else {
                //CAMERALOG("--- SELECT RETURN : %d---disconnect", rt);
                FD_CLR(_socket, &readfds);
                Disconnect();
                //CAMERALOG("----should auto disconnect?");
                _pStopSem->Give();
            }
        }
    }
}

/*
 int keepalive = 1; // å¼€å¯keepaliveå±žæ€§
 int keepidle = 5; // å¦‚è¯¥è¿žæŽ¥åœ¨60ç§’å†…æ²¡æœ‰ä»»ä½•æ•°æ®å¾€æ¥,åˆ™è¿›è¡ŒæŽ¢æµ‹
 int keepinterval = 1; // æŽ¢æµ‹æ—¶å‘åŒ…çš„æ—¶é—´é—´éš”ä¸º5 ç§’
 int keepcount = 3; // æŽ¢æµ‹å°è¯•çš„æ¬¡æ•°.å¦‚æžœç¬¬1æ¬¡æŽ¢æµ‹åŒ…å°±æ”¶åˆ°å“åº”äº†,åˆ™åŽ2æ¬¡çš„ä¸å†å‘.
 setsockopt(_socket, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive ));
 setsockopt(_socket, IPPROTO_TCP, TCP_KEEPALIVE, (void*)&keepidle , sizeof(keepidle ));
 setsockopt(_socket, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keepinterval , sizeof(keepinterval ));
 setsockopt(_socket, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keepcount , sizeof(keepcount ));
NOT WORK
*/

void StandardTcpClient::onFdNew(int fd)
{
    if (_delegate) {
        _delegate->onFdNew(fd);
    } else {
        CAMERALOG("accept connection: %d", fd);
    }
}

void StandardTcpClient::onFdClose(int fd)
{
    if (_delegate) {
        _delegate->onFdClose(fd);
    } else {
        CAMERALOG("close socket: %d", fd);
    }
}
void StandardTcpClient::onReadBufferProcess(char* buffer, int len, int fd)
{
    if (_delegate) {
        _delegate->OnRecievedBuffer(buffer, len, fd);
    } else {
        CAMERALOG("%d, content:%s", fd, buffer);
    }
}

/***************************************************************
TCPSessionService
*/
void TCPSessionService::onReadBufferProcess
    (char* buffer
     , int len
     , int fd)
{
    _lock->Take();
    int i = 0;
    for (i = 0; i < _addedNum; i++) {
        if (_ppSessionList[i]->fdMatch(fd)) {
            break;
        }
    }
    if (i < _addedNum) {
        _ppSessionList[i]->OnRecievedBuffer(buffer, len);
    }
    _lock->Give();
}

void TCPSessionService::onFdNew(int fd)
{
    _lock->Take();
    CAMERALOG("TCPSessionService::onFdNew : %d", fd);
    _ppSessionList[_addedNum] = new StandardTcpSession(fd, _sessionBufferLen, this);
    _ppSessionList[_addedNum]->SetDelegate(_pDelegate);
    _pDelegate->OnConnect(_ppSessionList[_addedNum]);
    _addedNum++;
    _lock->Give();
}

void TCPSessionService::onFdClose(int fd)
{
    int i = 0;
    _lock->Take();
    CAMERALOG("TCPSessionService::onFdClose : %d", fd);
    for (i = 0; i < _addedNum; i++) {
        if (_ppSessionList[i]->fdMatch(fd)) {
            break;
        }
    }
    if (i < _addedNum) {
        _pDelegate->OnDisconnect(_ppSessionList[i]);
        delete _ppSessionList[i];
        for (int j = i; j < _addedNum -1; j++) {
            _ppSessionList[j] = _ppSessionList[j + 1];
        }
        _ppSessionList[_addedNum -1] = NULL;
        _addedNum--;
    }
    _lock->Give();
}

void TCPSessionService::BroadCast(char* data, int len)
{
    _lock->Take();
    for (int i = 0; i < _addedNum; i++) {
        if (_ppSessionList[i]) {
            _ppSessionList[i]->SendBack(data, len);
        }
    }
    _lock->Give();
}

int TCPSessionService::sendSessionBack(SessionResponseSendBack* session, char* data, int len)
{
    _lock->Take();
    bool done = false;
    for (int i = 0; i < _addedNum; i++) {
        if (_ppSessionList[i] == session) {
            _ppSessionList[i]->SendBack(data, len);
            done = true;
            break;
        }
    }
    if (!done) {
        CAMERALOG("@@@@@sendSessionBack, but session is not exist!!@@@@@\n");
    }
    _lock->Give();

    return 0;
}
void TCPSessionService::SetDelegate(SessionRcvDelegate* dele)
{
    int i = 0;
    _lock->Take();
    _pDelegate = dele;
    for (i = 0; i < _addedNum; i++) {
        _ppSessionList[i]->SetDelegate(_pDelegate);
    }
    _lock->Give();
}

/***************************************************************
StandardTcpSession
*/
void StandardTcpSession::OnRecievedBuffer(char* buffer, int len)
{
    if (_buffSize - _rcvedLen < len) {
        CAMERALOG("--session rcv buffer overflow!");
        // error process.
        return;
    }
    memcpy(_buffer + _rcvedLen, buffer, len);
    _rcvedLen += len;
    while (_rcvedLen >= (int)sizeof(SessionDataHead)) {
        SessionDataHead* pHead = (SessionDataHead*)_buffer;
        if (_rcvedLen >= pHead->length) {
            if(_pDelegate) {
                _pDelegate->OnRcvData(pHead->data, sizeof(pHead->data) + pHead->appendLength, this);
            }
            int removelength = pHead->length;
            memmove(_buffer, _buffer + pHead->length, _rcvedLen - pHead->length);
            _rcvedLen -= removelength;
        } else {
            CAMERALOG("---wait more:%d, %d, %s", _rcvedLen, pHead->length, pHead->data);
            break;
        }
    }
};

int StandardTcpSession::SendBack(char* data, int len)
{
    SessionDataHead head;
    if (len > (int)sizeof(head.data)) {
        memcpy(head.data, data, sizeof(head.data));
        head.appendLength = (len - sizeof(head.data));
        head.length = sizeof(head) + head.appendLength;
    } else {
        memset(head.data, 0, sizeof(head.data));
        memcpy(head.data, data, len);
        head.appendLength = 0;
        head.length = sizeof(head);
    }
    //if(_fd)
    //{
        int rt = send(_fd, &head, sizeof(head), MSG_NOSIGNAL);
        if (len > (int)sizeof(head.data)) {
            rt += send(_fd, data + sizeof(head.data), head.appendLength, MSG_NOSIGNAL);
        }
    //}
    return rt;
}


/***************************************************************
StandardTcpSession
*/
TCPSessionClient::TCPSessionClient
    (int rcvBufferLen
     ,int sessionBufferLen
     ,char* name
    ): StandardTcpClient(rcvBufferLen, name)
    ,_pSession(0)
    ,_pDelegate(0)
    ,_sessionBufferLen(sessionBufferLen)
{
    _lock = new CMutex("TCPSessionClient lock");
};
TCPSessionClient::~TCPSessionClient()
{
    delete _lock;
};

int TCPSessionClient::Send(char* buffer, int len)
{
    //CAMERALOG("--send cmd: %s", buffer);
    int rt = 0;
    _lock->Take();
    if (_pSession) {
        rt = _pSession->SendBack(buffer, len);
    }
    _lock->Give();
    return rt;
}

void TCPSessionClient::SetDelegate(SessionRcvDelegate* dele)
{
    _pDelegate = dele;
    _lock->Take();
    if (_pSession) {
        _pSession->SetDelegate(_pDelegate);
    }
    _lock->Give();
}
void TCPSessionClient::onReadBufferProcess
    (char* buffer
    , int len
    , int fd)
{
    _lock->Take();
    if ((_pSession != NULL) && (_pSession->fdMatch(fd))) {
        _pSession->OnRecievedBuffer(buffer, len);
    }
    _lock->Give();
}

void TCPSessionClient::onFdNew(int fd)
{
    _lock->Take();
    if (_pSession != NULL ) {
        delete _pSession;
    }
    _pSession = new StandardTcpSession(fd, _sessionBufferLen,NULL);
    _pSession->SetDelegate(_pDelegate);
    _lock->Give();
    _pDelegate->OnConnect(_pSession);
}

void TCPSessionClient::onFdClose(int fd)
{
    _pDelegate->OnDisconnect(_pSession);
    _lock->Take();
    if ((_pSession) && (_pSession->fdMatch(fd))) {
        delete _pSession;
        _pSession = NULL;
    }
    _lock->Give();
}
