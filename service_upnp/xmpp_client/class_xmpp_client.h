/*****************************************************************************
 * class_xmpp_clent.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - , linnsong.
 *
 *
 *****************************************************************************/
#ifndef __H_class_xmpp_client__
#define __H_class_xmpp_client__


#include "GobalMacro.h"
#include "ClinuxThread.h"
#include "gloox/client.h"
#include "gloox/messagesessionhandler.h"
#include "gloox/messageeventhandler.h"
#include "gloox/messageeventfilter.h"
#include "gloox/chatstatehandler.h"
#include "gloox/chatstatefilter.h"
#include "gloox/connectionlistener.h"
#include "gloox/disco.h"
#include "gloox/message.h"
#include "gloox/gloox.h"
#include "gloox/lastactivity.h"
#include "gloox/loghandler.h"
#include "gloox/logsink.h"
#include "gloox/connectiontcpclient.h"
#include "gloox/connectionsocks5proxy.h"
#include "gloox/connectionhttpproxy.h"
#include "gloox/rosterlistener.h"
#include "gloox/rostermanager.h"
#include "gloox/messagehandler.h"
#include "gloox/presence.h"
#include "class_ipc_rec.h"
#include "sd_general_description.h"

class StringCMD;
class StringEnvelope;
//class StringCMDProcessor;
class XMPP_Client;

class RosterMsgProcessor : public gloox::RosterListener
{
public:
    RosterMsgProcessor(XMPP_Client* pClient);
    ~RosterMsgProcessor();
    void handleItemAdded( const gloox::JID& jid );
    void handleItemSubscribed( const gloox::JID& jid );
    void handleItemRemoved( const gloox::JID& jid );
    void handleItemUpdated( const gloox::JID& jid );
    void handleItemUnsubscribed( const gloox::JID& jid );
    void handleRoster( const gloox::Roster& roster );
    void handleRosterPresence( const gloox::RosterItem& item, const std::string& resource, gloox::Presence::PresenceType presence, const std::string& msg );
    void handleSelfPresence( const gloox::RosterItem& item, const std::string& resource, gloox::Presence::PresenceType presence, const std::string& msg );
    bool handleSubscriptionRequest( const gloox::JID& jid, const std::string& msg );
    bool handleUnsubscriptionRequest( const gloox::JID& jid, const std::string& msg );
    void handleNonrosterPresence( const gloox::Presence& presence );
    void handleRosterError( const gloox::IQ& iq );
private:
    XMPP_Client                *_pClient;
    //NSMutableArray              *_pFriends;
};

class XMPP_Connect_thread : public CThread
{
public:
	
	XMPP_Connect_thread(XMPP_Client* client);
	~XMPP_Connect_thread();

protected:
	virtual void main(void *);
	
private:
	//gloox::Client *j;
    //gloox::TLSBase* m_tls;
    //const gloox::JID rcpt;
    //std::string m_send;
	XMPP_Client *_client;
};

class StringCMDProcessor;
class StringRemoteCamera;
#define Max_xmpp_client_name_len 256
#define Max_xmpp_client_pwd_len 128
class XMPP_Client : public gloox::MessageSessionHandler, gloox::ConnectionListener, gloox::LogHandler,
                    gloox::MessageEventHandler, gloox::MessageHandler, gloox::ChatStateHandler
{
public:
	enum Status
	{
		Status_idle = 0,
		Status_connected,
		Status_num,
	};
	static void Create();
	static void Destroy();
	static XMPP_Client* getInstance(){return _pInstance;};
	
	void SetProcessor(StringCMDProcessor* p);

	void On_Msg(char* msg){};
	void On_P2P_require(){};
	void On_P2P_Msg(){};
	char* getID(){return _jid;};
	char* getPwd(){return _pwd;};

	void prepareConnect();
	bool connect();
	void disconnect();
	int whitespacePing(); // keep online signal
	
	void OnOnline(char* name);
	void OnOffline(char* name);
	
	gloox::ConnectionError recv(int microsecTimeout);

	virtual void onConnect();
    virtual void onDisconnect( gloox::ConnectionError e );
    virtual bool onTLSConnect( const gloox::CertInfo& info );
    virtual void handleMessage( const gloox::Message& msg, gloox::MessageSession * /*session*/ );
    virtual void handleMessageEvent( const gloox::JID& from, gloox::MessageEventType event );
    virtual void handleChatState( const gloox::JID& from, gloox::ChatStateType state );
    virtual void handleMessageSession( gloox::MessageSession *session );
    virtual void handleLog( gloox::LogLevel level, gloox::LogArea area, const std::string& message );

	void SendMsg(char* buff, char* sub, char* target);
	gloox::Client* getClientJ(){return j;};
private:
	XMPP_Client(char* jid = NULL, char* pwd = NULL);
	~XMPP_Client();
	static XMPP_Client* _pInstance;
	static StringRemoteCamera *_pSRC;
	
	XMPP_Connect_thread *_connectThread;
	
	char _jid[Max_xmpp_client_name_len];
	char _pwd[Max_xmpp_client_pwd_len];

	Status _status;
	gloox::Client 				*j;
    gloox::MessageEventFilter 	*m_messageEventFilter;
    gloox::ChatStateFilter 		*m_chatStateFilter;
	gloox::MessageSession 		*m_session;
	StringCMDProcessor* 			_processor;
	RosterMsgProcessor*			_pRoster;
};


/*
class XMPP_Camera_end : public XMPP_Client
{
public:
	XMPP_Camera_end(char* jid, char* pwd){};
	~XMPP_Camera_end(){};

	void On_Command(char* msg){}; // start record, stop record, set rec config
	void On_StorageReady(){};
	
private:
};

class XMPP_Controller_end : public XMPP_Client
{
public:
	XMPP_Controller_end(){};
	~XMPP_Controller_end(){};
	void On_CameraOnline(char* msg){};
	void On_CameraStatusUpdate(){};
};

class XMPP_StorageServer_end : public XMPP_Client
{
public:
	XMPP_StorageServer_end(){};
	~XMPP_StorageServer_end(){};
	void On_StorageSize_Get(){};
	void On_FileNumber_Get(){};
	void On_DeleteFile_Require(){};
	
private:

};*/	

/***************************************************************
StringCMDProcessor
*/
#define MaxXmppQueueDeapth 20
class StringCMDProcessor
{
public:
	StringCMDProcessor();
	~StringCMDProcessor();

	int ProecessEvelope(StringEnvelope* pEv, char* from);
	int ProcessCmd();
	
	//void OnDisconnect(char* id);
	void setID(char* id){_id = id;};
	char* getID(){return _id;};
	virtual int OnDisconnect(char* name) = 0;
	
protected:
	virtual int inProcessCmd(StringCMD* cmd) = 0;
	UINT32		_currentControlID;
	StringCMD*	_cmdQueue[MaxXmppQueueDeapth];
	int			_head;
	int			_num;
	CMutex*		_lock;
	CSemaphore* _pSem;
	char* 		_id;
	
private: 
	int PostProcessCmd();	
	int removeCmdsInQfor(SessionResponseSendBack* sendback);
	int appendCMD(StringCMD* cmd);
};


class StringP2PSession;
class StringP2PSessionDelegate
{
public:
	enum ConnectionState
	{
		ConnectionState_RegistOk = 0,
		ConnectionState_RegistFailed,
		ConnectionState_ConnectOk,
		ConnectionState_ConnectFailed,
		ConnectionState_timeOut,
		ConnectionState_ready, 
		ConnectionState_idle,
	};
	StringP2PSessionDelegate(){};
	~StringP2PSessionDelegate(){};
	virtual int onStateChange(ConnectionState state) = 0;
	virtual int onFailed() = 0;
	virtual int onData(char* buffer, int len) = 0;

	void SetOnstatePara(char* para1, char* para2)
	{
		_para1 = para1;
		_para2 = para2;
	};
	
	char* _para1;
	char* _para2;
};

enum CameraInforType
{
	CameraInforType_GPS = 0,
	CameraInforType_VDB,
};
class TCP_VDB_Session;
class StringRemoteCamera : public StringCMDProcessor, public CThread, public StringP2PSessionDelegate
{
public:
	StringRemoteCamera(XMPP_Client* pClient);
	~StringRemoteCamera();
	
	virtual int onStateChange(ConnectionState state);
	virtual int onFailed();
	virtual int onData(char* buffer, int len);
	virtual int OnDisconnect(char* name);

	void OnCameraInformation(CameraInforType type, char* para);
	
protected:
	virtual void main(void * p);
private:
	virtual int inProcessCmd(StringCMD* cmd);
	int AddMsgLisener(char* id);
	int DelMsgLisener(char* id);

	StringP2PSession*	_pConnectSession;
	XMPP_Client*		_pClient;
	char				_from[256];
	TCP_VDB_Session*	_pVDBSession;

	char				_msgLisenerList[10][256];
	int					_LisenerNum;
};



#define maxNumInFileQueue 15
class XSTUNT_client;
class CTURN_Client;
class StringP2PSession : public CThread, public IPC_AVF_Delegate
{
public:
	enum ConnectionMode
	{
		ConnectionMode_XSTUNTserver = 0,
		ConnectionMode_XSTUNTclient,
		ConnectionMode_TURNRelay, 
		ConnectionMode_num,
	};
	
	StringP2PSession
		(StringP2PSessionDelegate *delegate
		, char* id
		, char* target);
	~StringP2PSession();
	void SetMode(ConnectionMode mode);
	int getMode(){return _mode;};
	char* getTarget(){return _target;};
	bool isIdle(){return _bIdle;};
	void SetPara(char* para1, char*para2);

	void onNewFileReady(char* path);
protected:
	virtual void main(void * p);

private:
	int tryXSTUNTsereverMode();
	int tryXSTUNTClientMode();
	int tryTURNRelaySession();
	void moveHead();
	
private:
	StringP2PSessionDelegate* _delegate;
	StringP2PSessionDelegate::ConnectionState _state;
	char			_target[256];
	char			_id[256];
	char			_para1[256];
	char			_para2[256];
	XSTUNT_client*	_pCLient ;
	ConnectionMode	_mode;
	bool 			_bIdle;
	int				_sessionSocket;

	CTURN_Client* _turnClient;

	bool	_bStartStream;
	char	_fileQueue[maxNumInFileQueue][256];
	int 	_inqueueNum;
	int		_head;
	CSemaphore	*_pFileReady;
	CMutex		*_pFileQueueLock;
	int 		_sendSpeed;
	int			_generateCounter;
	
};




#endif