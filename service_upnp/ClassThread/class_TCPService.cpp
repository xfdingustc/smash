
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
	for(int i = 0; i < _maxConnection; i++)
	{
		_connectedSockets[i] = -1;
	}
}

StandardTCPService::~StandardTCPService()
{
	for(int i = 0; i < _maxConnection; i++)
	{
		if(_connectedSockets[i] > 0)
		close(_connectedSockets[i]);
	}
	delete[] _connectedSockets;
	delete[] _buffer;
}

int StandardTCPService::BroadCast(char* buffer, int len)
{
	for(int i = 0; i < _maxConnection; i++)
	{
		if(_connectedSockets[i] > 0)
		{
			//printf(">>>>>>broad cast:%d, %s\n", _connectedSockets[i], buffer);
			::send(_connectedSockets[i], buffer, len, MSG_NOSIGNAL);
		}
	}
	return 0;
}

void StandardTCPService::onFdNew(int fd)
{
	//printf("StandardTCPService::onFdNew %d\n", fd);
	if(_delegate)
	{
		_delegate->onFdNew(fd);
	}
	else
		printf("accept connection: %d\n", fd);
}

void StandardTCPService::onFdClose(int fd)
{
	if(_delegate)
	{
		_delegate->onFdClose(fd);
	}
	else
		printf("close socket: %d\n", fd);
}

void StandardTCPService::onReadBufferProcess(char* buffer, int len, int fd)
{
	if(_delegate)
	{
		_delegate->OnRecievedBuffer(buffer, len, fd);
	}
	else
		printf("%d, content:%s\n",fd ,buffer);
	//StringEnvelope ev(buffer ,len);	
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
	if(_listenSocket < 0)
	{
		return;
	}
	int flag = 1;
	if (::setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0)
	{
		printf("--!!!!--setopt error\n");
		close(_listenSocket);
		return;
	}
	if(bind(_listenSocket, (const sockaddr*)&addr, sizeof(addr)) < 0)
	{
		printf("--!!!!--bind error\n");
		close(_listenSocket);
		return;
	}
	if(listen(_listenSocket, 3) < 0)
	{
		printf("listen error\n");
		close(_listenSocket);
		return;
	}
	for(int i = 0; i < _maxConnection; i++)
		_connectedSockets[i] = 0;

	fd_set readfds;
	int Maxfd;
	int newFd;
	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(_listenSocket, &readfds);
		Maxfd = _listenSocket + 1;
		for(int i = _connectedNum - 1; i >= 0; i--)
		{
			if(_connectedSockets[i] == -1)
			{
				for(int j = i; j < _connectedNum - 1; j++)
				{
					_connectedSockets[j] = _connectedSockets[j + 1];
				}
				_connectedSockets[_connectedNum - 1] = 0;
				_connectedNum --;
			}
			else
			{
				FD_SET(_connectedSockets[i], &readfds);
				Maxfd = (_connectedSockets[i] > (Maxfd - 1))? (_connectedSockets[i] + 1) : Maxfd;
			}
		}
		for(int i = 0; i < _connectedNum; i++)
		{
			//printf("socket at index %d, %d\n", i, _connectedSockets[i]);
		}
		//printf("--select Maxfd:%d, _connectedNum: %d\n", Maxfd, _connectedNum);
		if(select(Maxfd, &readfds, NULL, NULL, NULL))
		{
			if(FD_ISSET(_listenSocket, &readfds))
			{
				newFd = accept(_listenSocket, (sockaddr*)&addr, (socklen_t*)&addr_len);
				if((newFd)&& (_connectedNum < _maxConnection))
				{
					//printf("---new connect : %d\n", newFd);
					_connectedSockets[_connectedNum] = newFd;
					_connectedNum++;
					FD_SET(newFd, &readfds);
					//Maxfd = (newFd > (Maxfd - 1))? (newFd + 1) : Maxfd;
					this->onFdNew(newFd);
				}
				else if(newFd > 0)
				{
					close(newFd);
					printf("too much client connection: %d\n", _connectedNum);
				}
			}
			else
			{
				for(int i = 0; i < _connectedNum; i++)
				{
					if(FD_ISSET(_connectedSockets[i], &readfds))
					{
						bzero(_buffer, _rcvBufferLen);
						int len = (int)recv(_connectedSockets[i], _buffer, (size_t)_rcvBufferLen, MSG_DONTWAIT | MSG_NOSIGNAL);
						if(len > 0)
						{
							this->onReadBufferProcess(_buffer, len, _connectedSockets[i]);
						}
						else
						{
							//printf("close fd : %d\n", _connectedSockets[i]);
							//close(_connectedSockets[i]);
							printf("onFdClose fd : %d >>>\n", _connectedSockets[i]);
							this->onFdClose(_connectedSockets[i]);
							printf("onFdClose fd : %d <<<\n", _connectedSockets[i]);
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
	//printf("--->>>> connect:%s:%d\n", address, port);
	if(_socket > 0)
	{
		if(connect(_socket, (sockaddr*)&addr, sizeof(addr)) < 0)
		{
            //printf("--- connect failed : %s:%d\n", address, port);
			close(_socket);
			_bConnected = false;
		}
		else
		{
            //printf("------ +++StandardTcpClient connect ok:%d\n", _socket);
			_bConnected = true;
            // set socket
            //int flags = fcntl(_socket, F_GETFL, 0);
            //fcntl(_socket, F_SETFL, flags|O_NONBLOCK);
			onFdNew(_socket);
			this->Go();
		}
	}
	else
	{
		_bConnected = false;
	}
    return _bConnected;
}

void StandardTcpClient::Disconnect()
{
	if(_bConnected)
	{
        printf("---StandardTcpClient::Disconnect : %d\n", _socket);
        _bConnected = false;
        onFdClose(_socket);
		close(_socket);
        CThread::Stop();
    }
}

void StandardTcpClient::Stop()
{
    //printf("----->>>StandardTcpClient::Stop()\n");
    _pStopSem->Take(-1);
    CThread::Stop();
}

int StandardTcpClient::Send(char* buffer, int len)
{
	if(_bConnected)
	{
		return send(_socket, buffer, len, MSG_NOSIGNAL);
	}
	return 0;
}

void StandardTcpClient::main(void * p)
{
	fd_set readfds;
	int Maxfd;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    //printf("-------+>>>>>> start StandardTcpClient::main : %s \n", this->getName());
    //_bRcvBool = false;
	while(_bConnected)
	{
		FD_ZERO(&readfds);
		FD_SET(_socket, &readfds);
		Maxfd = _socket + 1;
        int rt = select(Maxfd, &readfds, NULL, NULL, NULL);//&timeout);
		if(rt > 0) // &timeout))
		{
			if((FD_ISSET(_socket, &readfds)))
			{
                //printf("---selected");
                int len = (int)recv(_socket, _buffer, _rcvBufferLen, MSG_DONTWAIT | MSG_NOSIGNAL);
                //printf("---recv %d\n", len);
                if(len > 0)
				{
					onReadBufferProcess(_buffer, (int)len, _socket);
				}
				else
				{
                    FD_CLR(_socket, &readfds);
					Disconnect();
                    //printf("----should auto disconnect?\n");
                    _pStopSem->Give();
				}
			}
		}else
        {
            //printf("--- SELECT RETURN : %d---\n", rt);
            if (rt == 0) {
                // time out;
                // count to 3  as disconnect;?
            }
            else
            {
                //printf("--- SELECT RETURN : %d---disconnect\n", rt);
                FD_CLR(_socket, &readfds);
                Disconnect();
                //printf("----should auto disconnect?\n");
                _pStopSem->Give();
            }
        }
	}
}

/*
 int keepalive = 1; // 开启keepalive属性
 int keepidle = 5; // 如该连接在60秒内没有任何数据往来,则进行探测
 int keepinterval = 1; // 探测时发包的时间间隔为5 秒
 int keepcount = 3; // 探测尝试的次数.如果第1次探测包就收到响应了,则后2次的不再发.
 setsockopt(_socket, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive ));
 setsockopt(_socket, IPPROTO_TCP, TCP_KEEPALIVE, (void*)&keepidle , sizeof(keepidle ));
 setsockopt(_socket, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keepinterval , sizeof(keepinterval ));
 setsockopt(_socket, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keepcount , sizeof(keepcount ));
NOT WORK
*/

void StandardTcpClient::onFdNew(int fd)
{
	if(_delegate)
	{
		_delegate->onFdNew(fd);
	}
	else
		printf("accept connection: %d\n", fd);
}

void StandardTcpClient::onFdClose(int fd)
{
	if(_delegate)
	{
		_delegate->onFdClose(fd);
	}
	else
		printf("close socket: %d\n", fd);
}
void StandardTcpClient::onReadBufferProcess(char* buffer, int len, int fd)
{
	if(_delegate)
	{
		_delegate->OnRecievedBuffer(buffer, len, fd);
	}
	else
		printf("%d, content:%s\n",fd ,buffer);
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
	for(i = 0; i < _addedNum; i++)
	{
		if(_ppSessionList[i]->fdMatch(fd))
			break;
	}
	if(i < _addedNum)
		_ppSessionList[i]->OnRecievedBuffer(buffer, len);
	_lock->Give();
}

void TCPSessionService::onFdNew(int fd)
{
	_lock->Take();
	printf("TCPSessionService::onFdNew : %d\n", fd);
	_ppSessionList[_addedNum] = new StandardTcpSession(fd, _sessionBufferLen, this);
	_ppSessionList[_addedNum]->SetDelegate(_pDelegate);
	_addedNum++;
	_lock->Give();
}

void TCPSessionService::onFdClose(int fd)
{
	int i = 0;
	_lock->Take();
	printf("TCPSessionService::onFdClose : %d\n", fd);
	for(i = 0; i < _addedNum; i++)
	{
		if(_ppSessionList[i]->fdMatch(fd))
			break;
	}
	if(i < _addedNum)
	{
		_ppSessionList[i]->onSessionBreak();
		delete _ppSessionList[i];
		for(int j = i; j < _addedNum -1; j++)
		{
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
	for(int i = 0; i < _addedNum; i++)
	{
		if(_ppSessionList[i])
			_ppSessionList[i]->SendBack(data, len);
	}
	_lock->Give();
}

int TCPSessionService::sendSessionBack(SessionResponseSendBack* session, char* data, int len)
{
	_lock->Take();
	bool done = false;
	for(int i = 0; i < _addedNum; i++)
	{
		if(_ppSessionList[i] == session)
		{
			_ppSessionList[i]->SendBack(data, len);
			done = true;
			break;
		}
	}
	if (!done)
	{
		printf("\n@@@@@sendSessionBack, but session is not exist!!@@@@@\n\n");
	}
	_lock->Give();
}
void TCPSessionService::SetDelegate(SessionRcvDelegate* dele)
{
	int i = 0;
	_lock->Take();
	_pDelegate = dele;
	for(i = 0; i < _addedNum; i++)
	{
		_ppSessionList[i]->SetDelegate(_pDelegate);
	}
	_lock->Give();
}

/***************************************************************
StandardTcpSession
*/

void StandardTcpSession::onSessionBreak()
{
	if (_pDelegate) {
		_pDelegate->onSessionBreak(this);
	}
	close(_fd);
	printf("--onSessionBreak() close fd: %d\n", _fd);
}
void StandardTcpSession::OnRecievedBuffer(char* buffer, int len)
{
	if(_buffSize - _rcvedLen < len)
	{
		printf("--session rcv buffer overflow\n");
		// error process.
	}
	memcpy(_buffer + _rcvedLen, buffer, len);
	_rcvedLen += len;
	while(_rcvedLen >= (int)sizeof(SessionDataHead))
	{
		SessionDataHead* pHead = (SessionDataHead*)_buffer;
		if(_rcvedLen >= pHead->length)
		{
			if(_pDelegate)
				_pDelegate->OnRcvData(pHead->data, sizeof(pHead->data) + pHead->appendLength, this);
			int removelength = pHead->length;
			memcpy(_buffer, _buffer + pHead->length, _rcvedLen - pHead->length);
			_rcvedLen -= removelength;
		}
		else
		{
			printf("---wait more:%d, %d, %s\n", _rcvedLen, pHead->length, pHead->data);
			break;
		}
	}
};

int StandardTcpSession::SendBack(char* data, int len)
{
	
    SessionDataHead head;
    if(len > sizeof(head.data))
    {
        memcpy(head.data, data, sizeof(head.data));
        head.appendLength = (len - sizeof(head.data));
        head.length = sizeof(head) + head.appendLength;
    }
    else
    {
        memset(head.data, 0, sizeof(head.data));
        memcpy(head.data, data, len);
        head.appendLength = 0;
        head.length = sizeof(head);
    }
	//if(_fd)
	//{
	    int rt = send(_fd, &head, sizeof(head), MSG_NOSIGNAL);
	    if(len > sizeof(head.data))
	    {
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
    //printf("--send cmd: %s\n", buffer);
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
    if(_pSession)
        _pSession->SetDelegate(_pDelegate);
    _lock->Give();
}
void TCPSessionClient::onReadBufferProcess
	(char* buffer
	, int len
	, int fd)
{
    _lock->Take();
	if((_pSession !=NULL)&&(_pSession->fdMatch(fd)))
		_pSession->OnRecievedBuffer(buffer, len);
    _lock->Give();
}

void TCPSessionClient::onFdNew(int fd)
{
    _lock->Take();
	if(_pSession != NULL )
		delete _pSession;
	_pSession = new StandardTcpSession(fd, _sessionBufferLen,NULL);
	_pSession->SetDelegate(_pDelegate);
    _lock->Give();
    _pDelegate->OnConnect();
}

void TCPSessionClient::onFdClose(int fd)
{
    _pDelegate->OnDisconnect();
    _lock->Take();
	if((_pSession)&&(_pSession->fdMatch(fd)))
	{
		delete _pSession;
		_pSession = NULL;
	}
    _lock->Give();
}
