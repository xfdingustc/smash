/*****************************************************************************
 * class_xmpp_clent.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - , linnsong.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/select.h">
#include <sys/inotify.h>

#include "agbox.h"
#include "aglog.h"

#include <libxml/parser.h>
#include <libxml/tree.h>


#include "class_xmpp_client.h"


#include "ClientAPI.h"
#include <fcntl.h>
#include "xstunt_client.h"
#include "turn_client.h"
#include "VDBSession.h"

using namespace gloox;

/***************************************************************
XMPP_Connect_thread::XMPP_Connect_thread()
*/
XMPP_Connect_thread::XMPP_Connect_thread
	(XMPP_Client* client
	):CThread("xmpp thread", CThread::NORMAL, 0, NULL)
	,_client(client)
{

}

XMPP_Connect_thread::~XMPP_Connect_thread()
{

}

void XMPP_Connect_thread::main(void *)
{
	_client->prepareConnect();
	bool b = false;
	int c = 0;
	while(!b)
	{
		b = _client->connect();
		AGLOG_ERR("try connect xmpp server result : %d\n", b);
		if(b)
		{
			gloox::ConnectionError ce = gloox::ConnNoError;
			while( ce == gloox::ConnNoError)
			{
				//printf("--recv time out \n");
			  	ce = _client->recv(30000000); //timeout in microseconds to use for select
				_client->whitespacePing();
			}
			b = false;
		}
		else
		{
			c ++;
			AGLOG_ERR( "Connect failed times: %d\n", c);
			sleep(5);
		}
	}
	_client->disconnect();
}

/***************************************************************
XMPP_Client::XMPP_Client()
*/
XMPP_Client* XMPP_Client::_pInstance = NULL;
StringRemoteCamera* XMPP_Client::_pSRC = NULL;
//TCP_VDB_Session* XMPP_Client::_pVDBSession = NULL;
void XMPP_Client::Create()
{
	if(_pInstance == NULL)
	{
		char ssid_name_prop[AGCMD_PROPERTY_SIZE];
		char ssid_key_prop[AGCMD_PROPERTY_SIZE];
		char svr_prop[AGCMD_PROPERTY_SIZE];
		char key_prop[AGCMD_PROPERTY_SIZE];

		agcmd_property_get("temp.earlyboot.ssid_name", ssid_name_prop, "");
		agcmd_property_get("temp.earlyboot.ssid_key", ssid_key_prop, "");
		agcmd_property_get(ssid_name_prop, key_prop, "");
		if (strlen(key_prop) > 1) {
			sprintf(svr_prop, "%s@%s", key_prop, XMPP_DOMAIN_NAME);
			agcmd_property_get(ssid_key_prop, key_prop, "");
			printf("register to server as : %s\n", svr_prop);
			_pInstance = new XMPP_Client(svr_prop, key_prop);
		} else {
			printf("register to server as : %s\n", "linnsong@xmpp.cam2cloud.com");
			_pInstance = new XMPP_Client("linnsong@xmpp.cam2cloud.com","legooken");
		}
		_pSRC = new StringRemoteCamera(_pInstance);
		_pSRC->Go();

		//_pVDBSession = new TCP_VDB_Session();
	}
}

void XMPP_Client::Destroy()
{
	if(_pInstance != NULL)
	{
		_pSRC->Stop();
		delete _pInstance;
		delete _pSRC;
		_pInstance = NULL;
		_pSRC = NULL;
	}
}


XMPP_Client::XMPP_Client
	(char* jid
	, char* pwd
	):_status(Status_idle)
	,m_messageEventFilter(0)
	,m_chatStateFilter(0)
	,m_session(0)
	,_processor(0)	
{
	if(strlen(jid) < Max_xmpp_client_name_len)
		strcpy(_jid, jid);
	else
		printf("--- too long jid for xmpp");
	if(strlen(pwd) < Max_xmpp_client_pwd_len)
		strcpy(_pwd, pwd);
	else
		printf("--- too long pwd for xmpp");

	//LIBXML_TEST_VERSION;
	_pRoster = new RosterMsgProcessor(this);
	_connectThread = new XMPP_Connect_thread(this);
	_connectThread->Go();
}

XMPP_Client::~XMPP_Client()
{
	xmlCleanupParser();
    xmlMemoryDump();
	_connectThread->Stop();
	delete _connectThread;
	if(m_messageEventFilter)
		delete m_messageEventFilter;
	if(m_chatStateFilter)
		delete m_chatStateFilter;
}

void XMPP_Client::SetProcessor(StringCMDProcessor* p)
{
	_processor = p; 
	_processor->setID(_jid);
}

void XMPP_Client::prepareConnect()
{
	gloox::JID jid(_jid);
	j = new gloox::Client( jid, _pwd);
	j->registerConnectionListener(this);
	j->registerMessageSessionHandler(this, 0 );
	j->disco()->setVersion("messageTest", "1.0.0", "Linux");
	j->disco()->setIdentity("client", "bot");
	j->disco()->addFeature(gloox::XMLNS_CHAT_STATES );
	gloox::StringList ca;
	ca.push_back( "/tmp/cacert.crt" );
	j->setCACerts( ca );
	
	j->rosterManager()->registerRosterListener(_pRoster);
	j->logInstance().registerLogHandler( gloox::LogLevelDebug, gloox::LogAreaAll, this);
}

bool XMPP_Client::connect()
{
	return j->connect(false);
}

int XMPP_Client::whitespacePing()
{
	j->whitespacePing();
}

void XMPP_Client::disconnect()
{
	delete j;
}

void XMPP_Client::OnOnline(char* name)
{
	printf("on line: %s\n", name);
}

void XMPP_Client::OnOffline(char* name)
{
	printf("off line: %s\n", name);
	_processor->OnDisconnect(name);
}

ConnectionError XMPP_Client::recv(int microsecTimeout)
{
	return j->recv(microsecTimeout);
}

void XMPP_Client::onConnect()
{
	printf( "xmpp connected!!!\n" );
	_status = Status_connected;
}

void XMPP_Client::onDisconnect( ConnectionError e )
{
	printf( "XMPP_Client: disconnected: %d\n", e );
	if( e == ConnAuthenticationFailed )
		printf( "auth failed. reason: %d\n", j->authError() );
	_status = Status_idle;
}

bool XMPP_Client::onTLSConnect( const CertInfo& info )
{
	time_t from( info.date_from );
	time_t to( info.date_to );

	printf( "status: %d\nissuer: %s\npeer: %s\nprotocol: %s\nmac: %s\ncipher: %s\ncompression: %s\n"
	      "from: %s\nto: %s\n",
	      info.status, info.issuer.c_str(), info.server.c_str(),
	      info.protocol.c_str(), info.mac.c_str(), info.cipher.c_str(),
	      info.compression.c_str(), ctime( &from ), ctime( &to ) );
	return true;
}
static char tt[16];
static void print_element_names(xmlNode * a_node, int level)
{
	memset(tt, 0, sizeof(tt));
	for(int i = 0; i < level; i ++)
	{
		tt[i] = '+';
	}
    xmlNode *cur_node = NULL;
	xmlNode *tmp = NULL;
	xmlAttr *cur_attr = NULL;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            printf("%s--Element name: %s\n", tt, cur_node->name);
			for (cur_attr = cur_node->properties; cur_attr; cur_attr = cur_attr->next) {
				tmp = cur_attr->children;
				printf("%s  attr: %s=%s\n",tt, cur_attr->name, tmp->content);
			}
			print_element_names(cur_node->children, level+1);
		}
		else if(cur_node->type == XML_TEXT_NODE)
		{
			printf("%s  content: %s\n", tt, cur_node->content);
		}
		else
		{
			printf("%s  type: %d\n", tt, cur_node->type);
		}
    }
}

void XMPP_Client::handleMessage( const Message& msg, MessageSession *session )
{
	//printf( "type: %d, subject: %s, message: %s, thread id: %s\n", msg.subtype(), msg.subject().c_str(), msg.body().c_str(), msg.thread().c_str() );
/*
	std::string re = "You said:\n> " + msg.body() + "\nI like that statement.";
	std::string sub;
	if( !msg.subject().empty() )
	sub = "Re: " +  msg.subject();

	m_messageEventFilter->raiseMessageEvent( MessageEventDisplayed );
#if defined( WIN32 ) || defined( _WIN32 )
	Sleep( 1000 );
#else
	sleep( 1 );
#endif
	m_messageEventFilter->raiseMessageEvent( MessageEventComposing );
	m_chatStateFilter->setChatState( ChatStateComposing );
#if defined( WIN32 ) || defined( _WIN32 )
	Sleep( 2000 );
#else
	sleep( 2 );
#endif
	m_session->send( re, sub );

	if( msg.body() == "quit" )
	j->disconnect();/*/
	/*const char* tmp = msg.body().c_str();
	const char* p1 = strstr(tmp, "<cc");
	const char* p2 = strstr(tmp, "/cc>");
	if((p1 > 0)&&( p2 > 0)&&(p2 > p1))
	{
		//printf("%s-- msg is cmd : %s\n", tt, msg.body().c_str());
		//xml parser;
    	xmlDocPtr doc; 
    	doc = xmlReadMemory(p1, p2 + 4 - p1, "noname.xml", NULL, 0);
	    if (doc == NULL) {
	        printf("Failed to parse document\n");
			//return;
	    }
		else
		{
			xmlNode *root_element = NULL;
			root_element = xmlDocGetRootElement(doc);
    		print_element_names(root_element, 0);
			//doc.
    		xmlFreeDoc(doc);
		}
	}
	else
	{
		printf("-- msg is not a valid cmd : %s\n", msg.body().c_str());
	}*/
	//printf("session : %s@%s\n", session->target().username().c_str(), session->target().server().c_str());
	StringEnvelope strEV((char*)msg.body().c_str(), strlen(msg.body().c_str()));
	//str.print();
	char from[256];
	sprintf(from, "%s@%s", session->target().username().c_str(), session->target().server().c_str());
	if(strEV.isNotEmpty()&&(_processor))
	{
		//push cmd to process list.
		_processor->ProecessEvelope(&strEV, from);
	}
}

void XMPP_Client::SendMsg(char* buff, char* sub, char* target)
{
	if(m_session)
	{
		m_session->send(buff, sub);
	}
}

void XMPP_Client::handleMessageEvent( const JID& from, MessageEventType event )
{
	printf( "received event: %d from: %s\n", event, from.full().c_str() );
}

void XMPP_Client::handleChatState( const JID& from, ChatStateType state )
{
	printf( "received state: %d from: %s\n", state, from.full().c_str() );
}

void XMPP_Client::handleMessageSession( MessageSession *session )
{
	printf( "got new session\n");
	// this example can handle only one session. so we get rid of the old session
	j->disposeMessageSession( m_session );
	m_session = session;
	m_session->registerMessageHandler(this);
	m_messageEventFilter = new MessageEventFilter(m_session);
	m_messageEventFilter->registerMessageEventHandler(this);
	m_chatStateFilter = new ChatStateFilter(m_session);
	m_chatStateFilter->registerChatStateHandler(this);
}

void XMPP_Client::handleLog( LogLevel level, LogArea area, const std::string& message )
{
	//printf("log: level: %d, area: %d, %s\n", level, area, message.c_str() );
}


/***************************************************************
StringCMDProcessor::StringCMDProcessor()
*/
StringCMDProcessor::StringCMDProcessor
	(): _head(0)
	,_num(0)
	,_id(NULL)
{
	_lock = new CMutex("xmpp cmd queue lock");
	_pSem = new CSemaphore(0,1, "xmppcmd");
}

StringCMDProcessor::~StringCMDProcessor()
{
	while(_num > 0)
	{
		PostProcessCmd();
	}
	delete _lock;
	delete _pSem;
}

int StringCMDProcessor::ProcessCmd()
{
	int rt;
	while(1)
	{
		_pSem->Take(-1);
		_lock->Take();
		while(_num > 0)
		{
			inProcessCmd(_cmdQueue[_head]);
			rt = PostProcessCmd();
		}
		_lock->Give();
	}
	return rt;
}

int StringCMDProcessor::PostProcessCmd()
{
	StringCMD* cmd = _cmdQueue[_head];
	_head = (_head + 1)%MaxXmppQueueDeapth;
	_num--;
	delete cmd;
	return 0;
}
int StringCMDProcessor::removeCmdsInQfor(SessionResponseSendBack* sendback)
{
	int last = _head + _num - 1;
	for (int i = _head, i < _head + _num; i++) {
		StringCMD* cmd = _cmdQueue[i%MaxCMDQueueDeapth];
		if(cmd->getResponsSendBack() == sendback) {
			if(cmd->isLock()) delete cmd;
			for (int j = i; j < _head + _num - 1; j++) {
				_cmdQueue[j%MaxCMDQueueDeapth] = _cmdQueue[(j+1)%MaxCMDQueueDeapth];
			}
			_num--;
		}
	}
	return 0;
}
int StringCMDProcessor::ProecessEvelope
	(StringEnvelope* pEv
	,char* from)
{
	// if in control state, check sender id, then append cmd
	//TBD
	for(int i = 0; i < pEv->getNum(); i++)
	{
		pEv->GetCmd(i)->copyFromID(from);
		if(appendCMD(pEv->GetCmd(i)) < 0)
			break;
		
	}
	_pSem->Give();
	//if not in control state, check connect cmd, check control id and password
	// if password correct,  set as connect state
	//TBD 
	return 0;
}

int StringCMDProcessor::appendCMD
	(StringCMD* cmd)
{
	int rt = -1;
	_lock->Take();
	if(_num < MaxXmppQueueDeapth)
	{
		_cmdQueue[(_head + _num) % MaxXmppQueueDeapth] = cmd;
		cmd->setLock();
		_num++;
		//printf("append cmd : %s, %d\n",cmd->getName(), _num);
		rt = _num;
	}
	else
	{
		printf("appendCMD overflow\n");
	}
	_lock->Give();
	return rt;
}


/***************************************************************
StringRemoteCamera::StringRemoteCamera()
*/
StringRemoteCamera::StringRemoteCamera
	(XMPP_Client* pClient
	):CThread("RemoteCmdThread", CThread::NORMAL, 0, NULL)
	,_pClient(pClient)
{
	_pClient->SetProcessor(this);
	_pVDBSession = new TCP_VDB_Session("127.0.0.1", this);
}

StringRemoteCamera::~StringRemoteCamera()
{
	if(_pConnectSession)
	{
		_pConnectSession->Stop();
		delete _pConnectSession;
		_pConnectSession = NULL;
	}
	delete _pVDBSession;
}

void StringRemoteCamera::main(void * p)
{
	_pConnectSession = NULL;
	_LisenerNum = 0;
	this->ProcessCmd();
}

int StringRemoteCamera::inProcessCmd(StringCMD* cmd)
{
	if(strcmp(cmd->getName(), "xstunt.p2p") == 0)
	{
		if(_pConnectSession != NULL)
		{
			if(_pConnectSession->isIdle())
				_pConnectSession->Stop();
			else
				return -1;
		}
		else
		{
			_pConnectSession = new StringP2PSession(this,this->getID(),cmd->getPara1());
		}
		_pConnectSession->SetMode(StringP2PSession::ConnectionMode_XSTUNTserver);
		strcpy(_from, cmd->getFromID());
		_pConnectSession->Go();
	}
	else if(strcmp(cmd->getName(), "start.turn.p2p") == 0)
	{
		printf("recieve turn connect from : %s:%s\n", cmd->getPara1(), cmd->getPara2());
		if(_pConnectSession != NULL)
		{
			if(_pConnectSession->isIdle())
				_pConnectSession->Stop();
			else
				return -1;
		}
		else
		{
			_pConnectSession = new StringP2PSession(this,this->getID(),cmd->getPara1());
		}
		_pConnectSession->SetPara(cmd->getPara1(), cmd->getPara2());
		_pConnectSession->SetMode(StringP2PSession::ConnectionMode_TURNRelay);
		strcpy(_from, cmd->getFromID());
		_pConnectSession->Go();
	}
	else if(strcmp(cmd->getName(), "stop.p2p") == 0)
	{
		if(_pConnectSession != NULL)
		{
			printf("delete connect session\n");
			_pConnectSession->Stop();
			delete _pConnectSession;
			_pConnectSession = NULL;
		}
	}
	else if(strcmp(cmd->getName(), "reg.msg") == 0)
	{
		// add to msg list;
		AddMsgLisener(cmd->getFromID());
	}
	else if(strcmp(cmd->getName(), "unreg.msg") == 0)
	{
		// add to msg list;
		DelMsgLisener(cmd->getFromID());
	}
}

int StringRemoteCamera::AddMsgLisener(char* id)
{
	int i = 0;
	for(i = 0; i <_LisenerNum; i++)
	{
		if(strcmp(_msgLisenerList[i], id) == 0)
			break;
	}
	if((i == _LisenerNum)&&(_LisenerNum < 10))
	{
		//printf("-- reg.msg : %s\n", id);
		strcpy(_msgLisenerList[_LisenerNum], id);
		_LisenerNum++;
	}else
	{
		printf("-- too much lisener or lisener has added:%d\n", _LisenerNum);
	}
	return _LisenerNum;
}

int StringRemoteCamera::DelMsgLisener(char* id)
{
	int i = 0;
	for(i = 0; i <_LisenerNum; i++)
	{
		if(strcmp(_msgLisenerList[i], id) == 0)
			break;
	}
	if(i < _LisenerNum)
	{
		for(int j = i; j < _LisenerNum -1; j++)
		{
			strcpy(_msgLisenerList[j], _msgLisenerList[j + 1]);
		}
		_LisenerNum --;
	}
	return _LisenerNum;
}

int StringRemoteCamera::OnDisconnect(char* name)
{
	//printf("on disconnect, close session: %s, %s\n", name, _from);
	DelMsgLisener(name);
	if((strcmp(_from, name) == 0)&&(_pConnectSession != NULL))
	{
		printf("on disconnect, close session\n");
		
		_pConnectSession->Stop();
		delete _pConnectSession;
		memset(_from, 0, sizeof(_from));
		_pConnectSession = NULL;
	}
}

int StringRemoteCamera::onStateChange
	(ConnectionState state)
{
	if(state == StringP2PSessionDelegate::ConnectionState_RegistOk)
	{
		//if server mode send start client. if client mode 
		if(_pConnectSession->getMode() == StringP2PSession::ConnectionMode_XSTUNTserver)
		{
			// send start client cmd.
			StringCMD *cmd = new StringCMD((char*)"resp.p2p", this->getID(), (char*)"serverReady");
			StringEnvelope envelop(&cmd, 1);
			_pClient->SendMsg(envelop.getBuffer(), (char*)"cmd", _pConnectSession->getTarget());
		}
		else if(_pConnectSession->getMode() == StringP2PSession::ConnectionMode_TURNRelay)
		{
			StringCMD *cmd = new StringCMD((char*)"permission.turn.p2p", _para1, _para2);
			StringEnvelope envelop(&cmd, 1);
			_pClient->SendMsg(envelop.getBuffer(), (char*)"cmd", _pConnectSession->getTarget());
		}
	}

	if(state == StringP2PSessionDelegate::ConnectionState_ConnectOk)
	{
		printf("-- state connectOK recieved\n");
	}
	return 0;
}

void StringRemoteCamera::OnCameraInformation
	(CameraInforType type
	, char* para)
{
	StringCMD *cmd = NULL;
	if(_LisenerNum > 0)
	{
		// send start client cmd.
		switch(type)
		{
			case CameraInforType_GPS:
				cmd = new StringCMD((char*)"gps.state", para, _para2);
				break;
			case CameraInforType_VDB:
				cmd = new StringCMD((char*)"obd.state", para, _para2);
				break;
		}
		if(cmd)
		{
			StringEnvelope envelop(&cmd, 1);
			printf("-- send cmd : %s\n", envelop.getBuffer());
			for(int i = 0; i < _LisenerNum; i++)
			{
				_pClient->SendMsg(envelop.getBuffer(), (char*)"cmd", _msgLisenerList[i]);
			}
		}
	}
}

int StringRemoteCamera::onFailed()
{
	return 0;
}
int StringRemoteCamera::onData(char* buffer, int len)
{
	return 0;
}
// p2p use Xstunt as demo should switch to relay server later
//TBD
/***************************************************************
StringP2PSession::StringP2PSession()
*/
#define TURN_SERVER_IP "121.199.26.181"
#define TURN_SERVER_port 3578

#define senbufLen (1*1024)

#define XSTUTNSERVER_IP1 "121.199.26.181"
#define XSTUTNSERVER_IP2 "121.199.26.182"

StringP2PSession::StringP2PSession
	(StringP2PSessionDelegate* delegate
	, char* id
	, char* target
	) : _delegate(delegate)
	,_state(StringP2PSessionDelegate::ConnectionState_idle)
	,_bIdle(true)
	,_sessionSocket(0)
	,_turnClient(0)
	,_bStartStream(false)
	,_inqueueNum(0)
	,_head(0)
{
	if(id)
		strcpy(_id, id);
	if(target)
		strcpy(_target, target);

	_pFileReady = new CSemaphore(0,1,"sendfilequeue");
	_pFileQueueLock = new CMutex("sendfilequeue");
	IPC_AVF_Client::getObject()->setDelegate(this);
}

void StringP2PSession::SetPara(char* para1, char*para2)
{
	if(para1)
		strcpy(_para1, para1);
	if(para2)
		strcpy(_para2, para2);
}

StringP2PSession::~StringP2PSession()
{
	IPC_AVF_Client::getObject()->setDelegate(NULL);
	if(_sessionSocket > 0)
		close(_sessionSocket);
	if(_turnClient)
		delete _turnClient;
	delete _pFileReady;
	delete _pFileQueueLock;
}

void StringP2PSession::main(void * p)
{
	_bIdle = false;
	if(_mode == ConnectionMode_XSTUNTserver)
	{
		_sessionSocket = tryXSTUNTsereverMode();
	}
	else if(_mode == ConnectionMode_XSTUNTclient)
	{
		_sessionSocket = tryXSTUNTClientMode();
	}
	else if(_mode == ConnectionMode_TURNRelay)
	{
		_sessionSocket = tryTURNRelaySession();
	}
	printf("-- get socket : %d\n", _sessionSocket);
	if(_sessionSocket > 0)
	{
		fd_set Socks;
		FD_ZERO(&Socks);
		FD_SET(_sessionSocket, &Socks);
		char* pp = 0;
		int rdlen = 0;
		int sentLen;
		int rt;
		char *buffer = new char[senbufLen];
		_sendSpeed = 0;
		_generateCounter = 0;
		_bStartStream  = true;
		while(_bStartStream)
		{
			_pFileReady->Take(-1);
			while((_inqueueNum > 0)&&(_bStartStream))
			{

				//printf("send file : %s\n", _fileQueue[_head]);
				CFile file(_fileQueue[_head],CFile::FILE_READ);
				if(file.GetLength()> 0)
				{
					sprintf(buffer,"syncStart-%lld", file.GetLength());
					send(_sessionSocket, buffer, 20, MSG_NOSIGNAL);
					sentLen = 0;
					while(1)
					{
						rdlen = file.read(senbufLen, (BYTE*)buffer, senbufLen);
						//printf("read file length: %d \n",rdlen);
						if(rdlen > 0)
						{
							pp = buffer;
							while(rdlen > 0)
							{
								rt = send(_sessionSocket, pp, rdlen, MSG_NOSIGNAL);
								if(rt > 0)
								{
									rdlen -= rt;
									pp += rt;
									sentLen += rt;
									//printf("--- %d/%d--", rt, rdlen);
								}
								//else
									//break;
							}	
						}
						else
							break;
					}
				}
				printf("move head?\n");
				moveHead();
			}
		}
		delete[] buffer;
#if 0
		int rt;
		char filePath[256];
		char *buffer = new char[senbufLen];
		char* pp = 0;
		int nRcv = 0;
		int rdlen = 0;
		int sentLen = 0;
		fd_set Socks;
		FD_ZERO(&Socks);
		FD_SET(_sessionSocket, &Socks);
		for(int i = 0; i  < 10; i++)
		{
			//;
			sprintf(filePath,"/tmp/mmcblk0p1/%s", moviFiles[i]);
			CFile file(filePath ,CFile::FILE_READ);
			if(file.GetLength()> 0)
			{
				sprintf(buffer,"syncStart-%d", file.GetLength());
				printf("send sync : %s\n", buffer);
				send(_sessionSocket, buffer, 20, 0);
				sentLen = 0;
				while(1)
				{
					rdlen = file.read(senbufLen, (BYTE*)buffer, senbufLen);
					//printf("read file length: %d \n",rdlen);
					if(rdlen > 0)
					{
						pp = buffer;
						while(rdlen > 0)
						{
							rt = send(_sessionSocket, pp, rdlen, MSG_NOSIGNAL);
							if(rt > 0)
							{
								rdlen -= rt;
								pp += rt;
								sentLen += rt;
								//printf("--- %d/%d--", rt, rdlen);
							}
							//else
								//break;
						}	
					}
					else
						break;
					
				}
				printf("sent len : %d/%d\n", sentLen, file.GetLength());
			}
		}
#endif
		close(_sessionSocket);
	}
	_bIdle = true;
}

void StringP2PSession::SetMode
	(ConnectionMode mode
	)
{
	_mode = mode;
}

int StringP2PSession::tryXSTUNTsereverMode()
{
	SOCKET s = -2;
	_pCLient = new XSTUNT_client(_id, (char*)XSTUTNSERVER_IP1, (char*)XSTUTNSERVER_IP2);
	if(_delegate != NULL)
	{
		if(_pCLient->isRegisted())
			_delegate->onStateChange(StringP2PSessionDelegate::ConnectionState_RegistOk);
		else
			_delegate->onStateChange(StringP2PSessionDelegate::ConnectionState_RegistFailed);
	}
	if(_pCLient->isRegisted())
	{
		s = _pCLient->tryAsServer(80);
		if(_delegate != NULL)
		{
			if(s > 0)
				_delegate->onStateChange(StringP2PSessionDelegate::ConnectionState_ConnectOk);
			else
				_delegate->onStateChange(StringP2PSessionDelegate::ConnectionState_ConnectFailed);
		}
	}
	delete _pCLient;
	return s;
}

int StringP2PSession::tryXSTUNTClientMode()
{
	_pCLient = new XSTUNT_client(_id, (char*)XSTUTNSERVER_IP1, (char*)XSTUTNSERVER_IP2);
	SOCKET s = _pCLient->tryAsClient(_target, 3);
	delete _pCLient;
	return s;
}

int StringP2PSession::tryTURNRelaySession()
{	
	int rt;
	if(_turnClient)
		delete _turnClient;
	_turnClient = new CTURN_Client
		(true
		,TURN_SERVER_IP
		, TURN_SERVER_port
		,0
		,0);

	rt = _turnClient->SyncPrePareTURN();
	if(rt >= 0)
		rt = _turnClient->SyncAddPermission(_para1, _para2);
	if(rt < 0)
	{
		_delegate->onStateChange(StringP2PSessionDelegate::ConnectionState_RegistFailed);
	}
	else
	{
		char tmp[30];
		char tmp1[8];
		_turnClient->GetMappedAddress(tmp, sizeof(tmp), tmp1, sizeof(tmp1));
		_delegate->SetOnstatePara(tmp, tmp1);	
		_delegate->onStateChange(StringP2PSessionDelegate::ConnectionState_RegistOk);
	}
	rt = _turnClient->SyncStartConnect(true);
	if(rt >= 0)
		return _turnClient->getDataFd();
	else
		return -1;
}

void StringP2PSession::onNewFileReady(char* path)
{
	//printf("on new file ready\n: %d", _bStartStream);
	if(!_bStartStream)
	{
		// remove file
		remove(path);
		return;
	}	
	_generateCounter ++;
	if(_inqueueNum < maxNumInFileQueue)
	{
		_pFileQueueLock->Take();
		strcpy(_fileQueue[(_head + _inqueueNum)%maxNumInFileQueue], path);
		_inqueueNum ++;
		_pFileQueueLock->Give();
		//for(int i = 0; i < _inqueueNum; i++)
		//	printf("file push in queue : %d, %s\n",i, _fileQueue[(_head + i)%maxNumInFileQueue]);
		_pFileReady->Give();	
	}
	else
	{
		// remove head,
		remove(path);
		return;
	}
};

void StringP2PSession::moveHead()
{
	//printf("file num in queu :%d\n", _inqueueNum);
	_pFileQueueLock->Take();
	remove(_fileQueue[_head]);
	_head = (_head + 1) % maxNumInFileQueue;
	_inqueueNum --;
	if(_inqueueNum > 2)
	{
		int num = _inqueueNum;
		for(int i = 0; i < num -1; i++)
		{
			remove(_fileQueue[_head]);
			_head = (_head + 1) % maxNumInFileQueue;
			_inqueueNum --;
		}
		
	}
	_pFileQueueLock->Give();
	

}


RosterMsgProcessor::RosterMsgProcessor
    (XMPP_Client* pClient
     ):_pClient(pClient)
{
    //_pFriends = [[NSMutableArray alloc]init];
}

RosterMsgProcessor::~RosterMsgProcessor()
{
    
}

void RosterMsgProcessor::handleItemAdded( const JID& jid )
{
    printf("RosterMsgProcessor::handleItemAdded\n");
}

void RosterMsgProcessor::handleItemSubscribed( const JID& jid )
{
    printf("RosterMsgProcessor::handleItemSubscribed\n");
}

void RosterMsgProcessor::handleItemRemoved( const JID& jid )
{
    printf("RosterMsgProcessor::handleItemRemoved\n");
}

void RosterMsgProcessor::handleItemUpdated( const JID& jid )
{
    printf("RosterMsgProcessor::handleItemUpdated\n");
}

void RosterMsgProcessor::handleItemUnsubscribed( const JID& jid )
{
    printf("RosterMsgProcessor::handleItemUnsubscribed\n");
}

void RosterMsgProcessor::handleRoster( const Roster& roster )
{
    //printf( "roster arriving\nitems:\n" );
    Roster::const_iterator it = roster.begin();
    for( ; it != roster.end(); ++it )
    {
        // friends 
        //printf( "jid: %s, name: %s, subscription: %d\n", (*it).second->jid().c_str(), (*it).second->name().c_str(), (*it).second->subscription() );
       //[ _pFriends addObject:[NSString stringWithFormat:@"%s",(*it).second->jid().c_str()]];
        /*StringList g = (*it).second->groups();
        StringList::const_iterator it_g = g.begin();
        // group
        for( ; it_g != g.end(); ++it_g )
            printf( "\tgroup: %s\n", (*it_g).c_str() );
        RosterItem::ResourceMap::const_iterator rit = (*it).second->resources().begin();
        for( ; rit != (*it).second->resources().end(); ++rit )
            printf( "resource: %s\n", (*rit).first.c_str() );*/
    }
    //[_pClient OnRoster:_pFriends];
}

void RosterMsgProcessor::handleRosterPresence( const RosterItem& item, const std::string& resource, Presence::PresenceType presence, const std::string& msg )
{
    //printf( "presence received: %s/%s -- %d\n", item.jid().c_str(), resource.c_str(), presence );
    if(presence == Presence::Available)
    {
        _pClient->OnOnline((char*)item.jid().c_str());
    }
    else if(presence == gloox::Presence::Unavailable)
        _pClient->OnOffline((char*)item.jid().c_str());
        
}

void RosterMsgProcessor::handleSelfPresence( const RosterItem& item, const std::string& resource,Presence::PresenceType presence, const std::string& msg )
{
	printf("RosterMsgProcessor::handleSelfPresence\n");
}

bool RosterMsgProcessor::handleSubscriptionRequest( const JID& jid, const std::string& msg )
{
    printf( "subscription: %s\n", jid.bare().c_str() );
    StringList groups;
    JID id(jid);
    _pClient->getClientJ()->rosterManager()->subscribe(id, "", groups, "" );
    return true;
}

bool RosterMsgProcessor::handleUnsubscriptionRequest( const JID& jid, const std::string& msg )
{
    printf("RosterMsgProcessor::handleUnsubscriptionRequest\n");
	JID id(jid);
	_pClient->getClientJ()->rosterManager()->remove(id);
	return true;
}

void RosterMsgProcessor::handleNonrosterPresence( const Presence& presence )
{
    printf("RosterMsgProcessor::handleNonrosterPresence\n");
}

void RosterMsgProcessor::handleRosterError( const IQ& iq)
{
    printf("RosterMsgProcessor::handleRosterError\n");
}

