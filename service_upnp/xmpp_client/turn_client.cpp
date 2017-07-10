#include "ClinuxThread.h"
#include <unistd.h>
#include <openssl/err.h>
#include "turn_client.h"

CTURN_Client::CTURN_Client
	(bool use_tcp
	,const char* serverAddr
	,UINT16 serverPort
	,const char* ifname
	,const char *local_address
	): CThread("turn client thread", CThread::NORMAL, 0, 0)
	,_state(client_state_none)
{
	use_short_term = false;
	_name = (char*)"noname";
	_pwd = (char*)"";

	memset(_ifname, 0, sizeof(_ifname));
	memset(_ipAddr, 0, sizeof(_ipAddr));
	if(ifname)
		strcpy(_ifname, ifname);
	if(local_address)
		strcpy(_ipAddr, local_address);
	
	memset((void *) &_server_addr, 0, sizeof(struct sockaddr_storage));
	if (make_ioa_addr((const UINT8*)serverAddr, serverPort,	&_server_addr) < 0)
	{
		printf("init server address failed\n");
	}
	memset((void *) &_local_addr, 0, sizeof(struct sockaddr_storage));
	memset((void *) &_relay_addr, 0, sizeof(struct sockaddr_storage));
	_pP2pSem = new CSemaphore(0, 1, "p2p");
	
	_state = client_state_initOk;
}

CTURN_Client::~CTURN_Client()
{
	close(_control_fd);
	close(_data_fd);
	if(_state >= client_state_Serverconnected)
	{
		this->Stop();
	}
	delete _pP2pSem;
}

int CTURN_Client::SyncPrePareTURN()
{
	int rt = 0;
	int counter = 0;
	while((_state < client_state_Serverconnected) && (counter < 100))
	{
		rt = this->ConnectToServer();
		counter++;
	}
	
	if(_state < client_state_WaitAllocation)
	{
		rt = turn_address_alloc(0);
		if(rt >= 0)
		{
			waitServerEcho();
			if(_state != client_state_Allocated)
				rt = -1;
		}
	}
	return rt;
}

int CTURN_Client::SyncAddPermission(char* ipaddress, char* port)
{
	int rt;
	//ioa_addr addr;
	if((rt = make_ioa_addr((const UINT8*)ipaddress, atoi(port), &_peerAddr)) < 0)
	{
		printf("init remote address error\n");
		return -1;
	}
	rt = turn_create_permission(&_peerAddr);
	if(rt >= 0)
	{
		waitServerEcho();
		if(_state != client_state_gotPermission)
			rt = -1;
	}
	return rt;
}

int CTURN_Client::SyncStartConnect(bool bPassive)
{
	int rt = 0;
	if(!bPassive)
	{
		if(_state == client_state_gotPermission)
			rt = turn_tcp_connect(&_peerAddr);
		else
			rt = -1;
	}
	else
	{
		_state = client_state_waitConnect;
	}
	if(rt >= 0)
		waitServerEcho();
	if(_state < client_state_ConnectOk)
		rt = -1;
	return rt;
}

int CTURN_Client::GetMappedAddress(char* addr, int addrLen, char* port, int portLen)
{
	int rt = 0;
	if(_state >= client_state_Allocated)
	{
		inet_ntop(AF_INET, &(_relay_addr.s4.sin_addr), addr, addrLen);
		sprintf(port, "%d", nswap16(_relay_addr.s4.sin_port));
	}
	else
		rt = -1;
	return rt;
}

int CTURN_Client::waitServerEcho()
{
	_pP2pSem->Take(-1);
	return 0;
}

int CTURN_Client::ConnectToServer()
{
	int rt = 0;
	if(_state != client_state_initOk)
		return -1;

	_control_fd = socket(_server_addr.ss.ss_family, SOCK_STREAM, 0);
	if (_control_fd < 0) {
		printf("init _control_fd failed\n");
		return -1;
	}

	if(strlen(_ifname) > 0)
	{
		sock_bind_to_device(_control_fd, (UINT8*)_ifname);
	}

	set_sock_buf_size(_control_fd, UR_CLIENT_SOCK_BUF_SIZE);

	if((strlen(_ipAddr) > 0)&&(make_ioa_addr((const UINT8*)_ipAddr, 0, &_local_addr) >= 0))
	{ 
	  	rt = addr_bind(_control_fd, &_local_addr);
	}else
	{
		struct sockaddr_in s4;
		s4.sin_family = AF_INET;
		s4.sin_port = 0;
		s4.sin_addr.s_addr = INADDR_ANY;
		rt = bind(_control_fd,(const struct sockaddr*)&s4, sizeof(struct sockaddr_in));
	}
	if(rt < 0)
	{
		printf("bind error\n");
		close(_control_fd);
		return rt;
	}
	addr_get_from_sock(_control_fd, &_local_addr);
	if (addr_connect(_control_fd, &_server_addr, &_connect_err) < 0)
	{
		if(_connect_err == EADDRINUSE)
			rt = -2;
		else
			rt = -1;
		printf("----CTURN_Client cannot connect to remote addr\n");
		close(_control_fd);
		return rt;
	}
	printf("Connected to server: \n");
	//usleep(50000);
	_state = client_state_Serverconnected;
	this->Go();
	return 0;
}

int CTURN_Client::SendMsg(stun_buffer* message)
{
	bool bSend = 0;
	int counter = 0;
	int len;
	while (!bSend && (counter < 100)) {
		len = send_buffer(message, 0);
		if (len > 0) {
			bSend = true;
		} else {
			counter++;
		}
	}
	if(bSend)
		return 0;
	else
		return -1;
}

int CTURN_Client::turn_address_alloc(int af)
{
	stun_buffer message;
	af = STUN_ATTRIBUTE_REQUESTED_ADDRESS_FAMILY_VALUE_DEFAULT;
	stun_set_allocate_request(&message, 1800, af, STUN_ATTRIBUTE_TRANSPORT_TCP_VALUE);
	if(add_integrity(&message)<0) return -1;
	stun_attr_add_fingerprint_str(message.buf,(size_t*)&(message.len));

	int rt = SendMsg(&message);
	if(rt == 0)
		_state = client_state_WaitAllocation;
	return rt;
}

int CTURN_Client::turn_create_permission(ioa_addr *peer_addr)
{
	stun_buffer message;
	stun_init_request(STUN_METHOD_CREATE_PERMISSION, &message);
	stun_attr_add_addr(&message, STUN_ATTRIBUTE_XOR_PEER_ADDRESS, peer_addr);
	if(add_integrity(&message)<0) return -1;
	stun_attr_add_fingerprint_str(message.buf,(size_t*)&(message.len));

	int rt = SendMsg(&message);
	if(rt == 0)
		_state = client_state_waitAddPermission;
	return rt;
}

int CTURN_Client::turn_tcp_connect(ioa_addr *peer_addr)
{
	stun_buffer message;
	stun_init_request(STUN_METHOD_CONNECT, &message);
	stun_attr_add_addr(&message, STUN_ATTRIBUTE_XOR_PEER_ADDRESS, peer_addr);
	if(add_integrity(&message)<0) return -1;
	stun_attr_add_fingerprint_str(message.buf,(size_t*)&(message.len));

	int rt = SendMsg(&message);
	if(rt == 0)
		_state = client_state_waitConnect;
	return rt;
}

int CTURN_Client::Init_Data_Connect(UINT32 cid)
{
	_data_cid = cid;
	 //_server_addr _relay_addr;
	int clnet_fd = socket(_server_addr.ss.ss_family, SOCK_STREAM, 0);
	if (clnet_fd < 0) {
		printf("socket Init_Data_Connect error\n");
		return -1;
	}

	socket_set_reusable(clnet_fd);
	if (sock_bind_to_device(clnet_fd, (const unsigned char*)_ifname) < 0) 
	{
		printf("Cannot bind client socket to device %s\n", _ifname);
	}
	set_sock_buf_size(clnet_fd, UR_CLIENT_SOCK_BUF_SIZE);
	addr_cpy(&_date_localaddr, &_local_addr);
	addr_set_port(&_date_localaddr,0);

	addr_bind(clnet_fd, &_date_localaddr);
	addr_get_from_sock(clnet_fd,&_date_localaddr);

	int err;
	char tmp[50];
	printf("data connect to : %s, %d\n", inet_ntop(AF_INET, &(_server_addr.s4.sin_addr), tmp, 50), nswap16(_server_addr.s4.sin_port));
	if (addr_connect(clnet_fd, &_server_addr,&err) < 0)
	{
		if(err == EADDRINUSE)
		{
			close(clnet_fd);
	    }else {
			printf("connect error : %d\n", err);
		}
	}
	else
	{
		//try bind
		_data_fd = clnet_fd;
		if(turn_tcp_connection_bind() >= 0)
		{
			_pP2pSem->Give();
		}else
		{
			printf(": cannot BIND to tcp connection\n");	
		}
	}
	return 0;
}

int CTURN_Client::turn_tcp_connection_bind()
{
	{
		int cb_sent = 0;
		stun_buffer message;
		stun_init_request(STUN_METHOD_CONNECTION_BIND, &message);
		stun_attr_add(&message, STUN_ATTRIBUTE_CONNECTION_ID, (const char*)&(_data_cid),4);
		//if(add_integrity(clnet_info, &message)<0) return -1;
		add_integrity(&message);
		stun_attr_add_fingerprint_str(message.buf,(size_t*)&(message.len));
		while (!cb_sent) {
			int len = send_buffer(&message, 1);
			if (len > 0) {
				cb_sent = 1;
			} else {
				printf("turn_tcp_connection_bind send error\n");
				return -1;
			}
		}
	}
	{
		int cb_received = 0;
		stun_buffer message;
		while (!cb_received)
		{
			int len = recv_buffer(&message, 1, 1);
			if (len > 0) {
				printf("connect bind response received: \n");
				int err_code = 0;
				UINT8 err_msg[129];
				if (stun_is_success_response(&message)) {
					if(stun_get_method(&message)!=STUN_METHOD_CONNECTION_BIND)
						continue;
					cb_received = 1;
					printf("turn_tcp_connection_bind success\n");
					_state=client_state_PeerConnected;
				} else if (stun_is_challenge_response_str(message.buf, (size_t)message.len,
										&err_code,err_msg,sizeof(err_msg),
										realm, nonce)) {
					return -2;
				} else if (stun_is_error_response(&message, &err_code,err_msg,sizeof(err_msg))) {
					cb_received = 1;
					printf( "connection bind error %d (%s)\n", err_code,(char*)err_msg);
					return -1;
				} else {
					printf("unknown connection bind response\n");
				}
			} else {
				printf("recv error\n");
				return -1;
			}
		}
	}

	return 0;
}

int CTURN_Client::send_buffer(stun_buffer* message, int data_connection)//, app_tcp_conn_info *atc)
{
	int rc = 0;
	int ret = -1;

	char *buffer = (char*) (message->buf);
	ioa_socket_raw fd;
	if(data_connection == 0)
		fd = _control_fd;
	else
		fd = _data_fd;

	if (fd >= 0)
	{
		size_t left = (size_t) (message->len);
		while (left > 0) {
			do {
				rc = send(fd, buffer, left, MSG_NOSIGNAL);
			} while (rc < 0 && ((errno == EINTR) || (errno == ENOBUFS) || (errno
							== EAGAIN)));
			if (rc > 0) {
				left -= (size_t) rc;
				buffer += rc;
			} else {
				break;
			}
		}
		if (left > 0)
			return -1;
		ret = (int) message->len;
	}
	return ret;
}


int CTURN_Client::recv_buffer(stun_buffer* message, int data_connection, int  sync)//, app_tcp_conn_info *atc) 
{
	int rc = 0;
	ioa_socket_raw fd;
	if(data_connection == 0)
		fd = _control_fd;
	else
		fd = _data_fd;
	
	do{
		rc = recv(fd, message->buf, sizeof(message->buf) - 1, MSG_PEEK | MSG_NOSIGNAL);
		if((rc<0) && (errno==EAGAIN) && sync)
		{
	      errno=EINTR;
	    }
	} while (rc < 0 && (errno == EINTR));
	    
	if(rc>0)
	{
		int mlen = rc;
		mlen = stun_get_message_len_str(message->buf, rc);
		if(mlen>0) 
		{
		  int rcr = 0;
		  int rsf = 0;
		  int cycle = 0;
		  while(rsf<mlen && cycle++<128)
		  {
			do 
			{
				rcr = recv(fd, message->buf+rsf, (size_t)mlen-(size_t)rsf, MSG_NOSIGNAL);
				if(rcr<0 && errno==EAGAIN && sync)
					errno=EINTR;
			} while (rcr < 0 && (errno == EINTR));
		    printf("---rcr : %d\n",rcr);
			if (rcr > 0)
				rsf+= rcr;
		  }
		  if(rsf<1)
			return -1;
		  message->len = rsf;
		  rc = rsf;
		} else {
		  rc = 0;
		}
	}
	return rc;
}

void CTURN_Client::main(void *)
{
	printf("main 0\n");
	int nRcv;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000000;
	fd_set Socks;
	FD_ZERO(&Socks);

	_rcvHead = _message.buf;
	_remainlen = sizeof(_message.buf);
	_message.len = 0;
	if(_control_fd > 0)
		FD_SET(_control_fd, &Socks);
	
	while(1)
	{
		int rt = ::select(_control_fd + 1, &Socks, NULL, NULL, NULL);//&tv); 
		if(rt > 0)
		{
			if(FD_ISSET(_control_fd, &Socks))
			{
				nRcv = recv(_control_fd, _rcvHead, _remainlen, MSG_NOSIGNAL);
				if (nRcv > 0)
				{	
					//printf("rcv : %d\n", nRcv);
					OnRecvMsg(nRcv);
				}
				else
				{
					printf("socket break\n");
					break;
				}
			}
		}
		else if(rt == 0)
		{
			// time out
			// check refresh time, send refresh request. TBD
			printf("--rcv time out\n");
		}
		else
		{
			printf("-- _control_fd select return error\n");
			break;
		}
	}
};

int CTURN_Client::OnRecvMsg(int len)
{
	_message.len += len;
	int mlen = stun_get_message_len_str(_message.buf, _message.len);
	if(mlen < 0)
	{
		printf("stun_get_message_len_str error : %d\n", mlen);
		_rcvHead = _message.buf;
		_remainlen = sizeof(_message.buf);
		_message.len = 0;
		return -1;
	}
	if(mlen <= _message.len)
	{
		int err_code = 0;
		UINT8 err_msg[129];
		if (stun_is_success_response(&_message)) {
			UINT16 method = stun_get_method(&_message);
			printf("success_response received : %d\n", method);
			
			if((method == STUN_METHOD_ALLOCATE)&&(_state == client_state_WaitAllocation))
			{
				if(stun_attr_get_first_addr(&_message, STUN_ATTRIBUTE_XOR_RELAYED_ADDRESS, &_relay_addr, NULL) < 0) 
				{
					printf("!!!: relay addr cannot be received\n");
					//return -1;
				}
				else
				{
					char tmp[50];
					printf("_relay_addr : %s:%d\n",inet_ntop(AF_INET, &(_relay_addr.s4.sin_addr), tmp, 50), nswap16(_relay_addr.s4.sin_port));
					_state = client_state_Allocated;
				}
				stun_attr_ref rt_sar = stun_attr_get_first_by_type(&_message, STUN_ATTRIBUTE_RESERVATION_TOKEN);
				UINT64 rtv = stun_attr_get_reservation_token_value(rt_sar);
				_current_reservation_token = rtv;
				printf("rtv=%llu\n",rtv);
				_pP2pSem->Give();
			}
			else if((method == STUN_METHOD_CONNECT)&&(_state == client_state_waitConnect))
			{
				// connect success recieved
				stun_attr_ref sar = stun_attr_get_first(&_message);
				if(sar) {
					UINT32 cid = *((const UINT32*)stun_attr_get_value(sar));
					printf("connect success return :%d\n", cid);
					Init_Data_Connect(cid);
				}
			}
			else if(method == STUN_METHOD_CREATE_PERMISSION)
			{
				printf("creat permission ok\n");
				_state = client_state_gotPermission;
				_pP2pSem->Give();
			}
			else
			{
				printf("unknow success_response\n");
			}
		} 
		else if(stun_is_indication(&_message))
		{
			UINT16 method = stun_get_method(&_message);
			printf("indication recieved : %d\n", method);
			if(method == STUN_METHOD_CONNECTION_ATTEMPT)
			{
			  stun_attr_ref sar = stun_attr_get_first(&_message);
			  if(sar) {
			    UINT32 cid = *((const UINT32*)stun_attr_get_value(sar));
				printf("STUN_METHOD_CONNECTION_ATTEMPT get :%d\n", cid);
			    Init_Data_Connect(cid);
			  }
			} 
		}
		else if(stun_is_challenge_response_str(_message.buf, (size_t)_message.len,
						&err_code, err_msg, sizeof(err_msg),
						realm, nonce))
		{
			//goto beg_allocate; //send allocate again.
		}else if(stun_is_error_response(&_message, &err_code,err_msg,sizeof(err_msg))) {
			printf("error %d (%s)\n", err_code,(char*)err_msg);
			if (err_code != 437) {
				_current_reservation_token = 0;
			} else {
				printf("errorn: %d, trying allocate again...\n", err_code);
				sleep(5);
			}
		}
		else{
			printf("unknown allocate response\n");
		}
		if(mlen < _message.len)
		{
			memcpy(_message.buf, _message.buf + mlen, _message.len - mlen);
		}
		_message.len = (_message.len - mlen);
		_rcvHead = _message.buf + _message.len;
		_remainlen = sizeof(_message.buf)- _message.len;
	}
	else
	{
		_rcvHead += len;
		_remainlen -= len;
	}
	return 0;
}

int CTURN_Client::add_integrity(stun_buffer *message)
{
	if(use_short_term)
	{
		if(stun_attr_add_integrity_by_user_short_term_str(message->buf, (size_t*)&(message->len), (UINT8*)_name, (UINT8*)_pwd)<0)
		{
			return -1;
		}
	} else if(nonce[0]) {
		if(stun_attr_add_integrity_by_user_str(message->buf, (size_t*)&(message->len), (UINT8*)_name, realm, (UINT8*)_pwd, nonce)<0)
		{
			return -1;
		}
	}
	return 0;
}

#if 0
// Channel bind for udp only, no use for tcp
int CTURN_Client::ChannelBind(UINT16 *chn, ioa_addr *peer_addr)
{
beg_bind:
	{
		int cb_sent = 0;
		stun_buffer message;
		*chn = stun_set_channel_bind_request(&message, peer_addr, *chn);

		if(add_integrity(&message)<0) return -1;
		stun_attr_add_fingerprint_str(message.buf,(size_t*)&(message.len));

		while (!cb_sent) {
			int len = send_buffer(&message, 0);
			if (len > 0) {
				printf("channel bind sent\n");
				cb_sent = 1;
			} else {
				printf("channel bind send error\n");
				return -1;
			}
		}
	}
	{
		int cb_received = 0;
		stun_buffer message;
		while (!cb_received) {

			int len = recv_buffer(&message, 0, 1);
			if (len > 0) {
				printf("cb response received: \n");
				int err_code = 0;
				UINT8 err_msg[129];
				if (stun_is_success_response(&message))
				{
					cb_received = 1;

					printf("success: 0x%x\n",(int) (*chn));
				} else if (stun_is_challenge_response_str(message.buf, (size_t)message.len,
										&err_code,err_msg,sizeof(err_msg),
										realm, nonce)) {
										goto beg_bind;
				} else if (stun_is_error_response(&message, &err_code,err_msg,sizeof(err_msg))) {
					cb_received = 1;
					printf("channel bind: error %d (%s)\n",err_code,(char*)err_msg);
					return -1;
				} else {
					printf("unknown channel bind response\n");
				}
			} else {
				printf("recv error \n");
				return -1;
				break;
			}
		}
	}
	return 0;
}
#endif

