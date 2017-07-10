
#ifndef __H_TURN_client__
#define __H_TURN_client__

#include "apputils.h"
#include "stun_buffer.h"
#include "ns_turn_ioaddr.h"
#include "ns_turn_utils.h"
#include "ns_turn_msg.h"
//#include "session.h"

#define STUN_BUFFER_SIZE (65507)

/*typedef struct _stun_buffer {
  UINT8	buf[STUN_BUFFER_SIZE];
  ssize_t	len;
} stun_buffer;*/

class CTURN_Client : public CThread
{
public:
	enum client_state
	{
		client_state_none = 0,
		client_state_initOk,
		client_state_Serverconnected,
		client_state_WaitAllocation,
		client_state_Allocated,
		client_state_waitAddPermission,
		client_state_gotPermission,
		client_state_waitConnect,
		client_state_ConnectOk,
		client_state_PeerConnected,
		client_state_num,
	};
	CTURN_Client(bool use_tcp
	,const char* serverAddr
	,UINT16 serverPort
	,const char* ifname
	,const char *local_address);
	~CTURN_Client();

	int SyncPrePareTURN();
	int SyncAddPermission(char* ipaddress, char* port);
	int SyncStartConnect(bool bPassive);
	int GetMappedAddress(char* addr, int addrLen, char* port, int portLen);
	
	//ioa_addr* GetMappedAddr();
	//int ConnectTo(ioa_addr* peerAddr);
	//int ChannelBind(UINT16 *chn, ioa_addr *peer_addr);
	
	

	int getControlFd();
	int getDataFd(){return _data_fd;};

protected:
	virtual void main(void *);

private:
	client_state 	_state;
	ioa_addr		_local_addr;
	int				_control_fd;
	int 			_connect_err;
	ioa_addr 		_server_addr;
	ioa_addr 		_relay_addr;
	
	ioa_addr 		_peerAddr;

	int				_connect_fd;

	ioa_addr		_date_localaddr;
	int				_data_fd;
	UINT32			_data_cid;

	unsigned long long _current_reservation_token;
	UINT8 nonce[STUN_MAX_NONCE_SIZE+1];
	UINT8 realm[STUN_MAX_REALM_SIZE+1];

	stun_buffer 	_message;

	UINT8*			_rcvHead;
	int				_remainlen;

	bool			use_short_term;
	char*			_name;
	char* 			_pwd;
	CSemaphore*		_pP2pSem;

	char			_ifname[10];
	char			_ipAddr[32];
#if 1 // for 
	//CThread			*_recvThread;
#else

#endif

private:
	int send_buffer(stun_buffer* message, int data_connection);
	int recv_buffer(stun_buffer* message, int data_connection, int sync);

	int RecvFunction(int len);
	int add_integrity(stun_buffer *message);
	int OnRecvMsg(int len);
	int ConnectToServer();
	
	int SendMsg(stun_buffer* message);
	int turn_address_alloc(int af);
	int turn_create_permission(ioa_addr *peer_addr);
	int turn_tcp_connect(ioa_addr *peer_addr);

	int waitServerEcho();
	int Init_Data_Connect(UINT32 cid);
	int turn_tcp_connection_bind();
};

#endif
