/***********************************************************************
*		 Win32 / Linux Intelx86 C++ Compiler File 		    		   				   
*--------------------------------------------------------------		   
*	[For]				{ XSTUNT Sample Code: Echo Server and Client}				  			   
*	[File name]     	{ Sample.cpp }
*	[Description]
*	Following sample code allows 2 peers behind different NATs do direct TCP connection, 
*	but fail if behind the same NAT. Users should modify the code to prevent this situation.
*	A client here can send message (be care of the message length) to the server and soon get
*	echo message from that. The messeage "exit" will cause both exit the program.
*	Build the program and run on 2 machines behind different NATs. Of course, the server 
*	should run before the client!
*	[Notice]
*	After connecting, the messeage should be sent in 60 seconds or the connection will be closed.
*	[History]
*	2006.04		Kuanyu	First released 
***********************************************************************/
#include "ClinuxThread.h"
#include "ClientAPI.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "xstunt_client.h"


XSTUNT_client::XSTUNT_client
	(char* id
	,char* ip1
	,char* ip2
	): _sConnect(-1)
	,_sServer(-1)
	,_id(id)
	,_ip1(ip1)
	,_ip2(ip2)
	,_registed(false)
{
	INT32 nRet = 0; 
	if ((nRet = XInit((char const*)_ip1, (char const*)_ip2, (long*)&_sServer, (char*)_id, (long*)&nErr)) == ERR_NONE)
	{
		_registed = true;
		sleep(3);
		printf("registed as %s\n", _id);
	}
	else
		printf("registed failed : %d\n", nErr);
}

XSTUNT_client::~XSTUNT_client()
{
	if(_registed)
		::XDeRegister(_sServer, _id, &nErr);	
}

SOCKET XSTUNT_client::tryAsServer(int timeout)
{
	INT32 nErr;
	_sConnect = -1;
	int count = 0;
	_bWait = true;
	printf("Strat listening...: %d\n", _sServer);
	while (_bWait)
	{
		if (XListen(_sServer, &_sConnect, NULL, DEF_TIMEOUT, &nErr) == ERR_NONE)
		{
			printf("One client successfully connected...\n");
			break;
		}
	}	
	return _sConnect;
}

SOCKET XSTUNT_client::tryAsClient
	(char* svrID, int tryTimes)
{
	INT32 nErr;
	int nTry;
	int nRet;
	_sConnect = -1;
	if(_registed)
	{
		for (nTry = 0; (nTry < tryTimes)&&(_bWait); nTry++)
		{
			printf("Try direct TCP Connetion... %d\n", nTry + 1);
			if ((nRet = XConnect(_sServer, svrID, &_sConnect, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
			{
				break;
			}
			else
			{
				printf("failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
			}
		}
		if (nTry == CONNECT_TRY_TIMES)
			printf("failed to connect!\n");
		return _sConnect;
	}
	else
		return -2;
}


#if 0
static SOCKET sConnect = (SOCKET) -1;
static SOCKET sListen = (SOCKET) -1;
static SOCKET sServer = (SOCKET) -1;
static INT32 nErr = 0;
static char* _id;
SOCKET testTCPp2p(bool bServer, char* id, char*svrid, char* ip1, char* ip2)
{
	INT32 nRet = 0;
	char chServerIP1[20];
	char chServerIP2[20]; 
	INT32 nTry = 0;
	SOCKET sReturn = -1;
	strcpy(chServerIP1, ip1);
	strcpy(chServerIP2, ip2);
	
	if ((nRet = XInit(ip1, ip2, &sServer, id, &nErr)) == ERR_NONE)
	{
		sleep(3);

///////////////////////////////////////////////////////////////////////////
//Listen Sample Section////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
		if (bServer)
		{
			printf("Strat listening...\n");
			while (true)
			{
				char chStr[MAX_MSG_LEN];
				char chChar;
				int nRcv = 0;
				sListen = (SOCKET) -1;
				fd_set Socks;

				if (XListen(sServer, &sListen, NULL, DEF_TIMEOUT, &nErr) == ERR_NONE)
				{
					printf("One client successfully connected...\n");
					do
					{
						int j = 0;
						for (int  i = 0; i < MAX_MSG_LEN; i++)
							chStr[i] = '\0';
						do 
						{
							FD_ZERO(&Socks);
							FD_SET(sListen, &Socks);
							//sListen is changed to a non-blocking socket, so we need to block it.
							::select(((INT32)sListen) + 1, &Socks, NULL, NULL, NULL); 
							nRcv = recv(sListen, &chChar, 1, 0);
							chStr[j] = chChar; 
							j++;
						}while(chChar != '\0');
						if (nRcv > 0)
						{
							printf("Msg>> %s\n", chStr);
							send(sListen, chStr, strlen(chStr), 0);
						}
						else
							break;
					}while (strcmp(chStr, "exit") != 0);
					break;
				}
			}//end while
		}//end if
///////////////////////////////////////////////////////////////////////////
//Connect Sample Section///////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
		else
		{
			//try for connection.
			for (nTry = 0; nTry < CONNECT_TRY_TIMES; nTry++)
			{
				char chStr[MAX_MSG_LEN];
				int nStart = 0, nEnd = 0;
				sConnect = (SOCKET) -1;
				printf("Try direct TCP Connetion...\n");
				//XConnect may fail if the listener is in the same NAT.
				//In this situation, local address of the listener will be returned back through error code.
				//Then try to directly connect to the address. The following sample code does not do this action.
#ifdef TEST_TIME
#ifdef _WIN32
				nStart =GetTickCount();
#endif
#endif 
				if ((nRet = XConnect(sServer, svrid, &sConnect, NULL, DEF_TIMEOUT, &nErr)) == ERR_NONE)
				{
#ifdef TEST_TIME
#ifdef _WIN32
					nEnd = GetTickCount();
					printf("Successfully connected after [%d] ms.\n", nEnd - nStart);
#endif
#endif
					do
					{
						chStr[0] = '\0';
						printf("Msg>> ");
						scanf("%s", chStr);
						send(sConnect, chStr, strlen(chStr) + 1, 0);
						chStr[0] = '\0';
						if (recv(sConnect, chStr, MAX_MSG_LEN, 0) > 0)
							printf("Echo>> %s\n", chStr);
					}while (strcmp(chStr, "exit") != 0);
					break;
				}
				else
				{
					printf("failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
				}
			}//end while
			if (nTry == CONNECT_TRY_TIMES)
				printf("failed to connect!\n");
		}//end else if
///////////////////////////////////////////////////////////////////////////
//Deregister Sample Section////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
		XDeRegister(sServer,id, &nErr);
		printf("leave test.\n");
		//end test
	}
	else
	{
		printf("Initialization failed. ErrType(%d) ErrCode(%d)\n", nRet, nErr);
	}
	return -1;
}

void endP2P()
{
	XDeRegister(sServer, _id, &nErr);
}
#endif
