
#ifndef __AVF_TCP_SERVER_H__
#define __AVF_TCP_SERVER_H__

#include "avf_socket.h"

class CNetworkService;
class CTCPServer;
class CTCPConnection;
class CSocket;
class CSocketEvent;

extern CNetworkService *g_tcp_server;
typedef void (*tcp_server_callback)(void *context, int port, CSocket **ppSocket);

//-----------------------------------------------------------------------
//
//  CNetworkService
//
//-----------------------------------------------------------------------
class CNetworkService: public CObject
{
public:
	static CNetworkService *Create();
	virtual void Release();

private:
	CNetworkService();
	virtual ~CNetworkService();
	void RunThread();

private:
	enum {
		MAX_SERVER = 6,
	};

	enum {
		CMD_Null,
		CMD_Exit,
		CMD_StartServer,
		CMD_StopServer,
	};

	typedef struct server_entry_s {
		int port;
		const char *pName;
		tcp_server_callback callback;
		void *context;
	} server_entry_t;

public:
	avf_status_t CmdServer(int cmd, int port, const char *pName, 
		tcp_server_callback callback, void *context);

	INLINE avf_status_t StartServer(int port, const char *pName, 
			tcp_server_callback callback, void *context) {
		return CmdServer(CMD_StartServer, port, pName, callback, context);
	}

	INLINE avf_status_t StopServer(int port) {
		return CmdServer(CMD_StopServer, port, NULL, NULL, NULL);
	}

private:
	int m_cmd;
	avf_status_t m_result;
	server_entry_t m_param;

private:
	static void ThreadEntry(void *p);
	void ServerLoop();

private:
	void ProcessConnection(int index);
	avf_status_t ProcessCmd();
	avf_status_t ProcessStartServerL();
	avf_status_t ProcessStopServerL();

private:
	static CNetworkService *m_instance;
	static CMutex mMutex;

private:
	CThread *mpThread;
	CSocketEvent *mpSocketEvent;
	CCondition mCondRequest;
	CCondition mCondAck;
	CSocket *m_server_socket[MAX_SERVER];
	server_entry_t m_entry[MAX_SERVER];
};

//-----------------------------------------------------------------------
//
//  CTCPServer
//
//-----------------------------------------------------------------------
class CTCPServer: public CObject
{
	friend class CTCPConnection;

protected:
	CTCPServer(const char *pName);
	virtual ~CTCPServer();

protected:
	avf_status_t RunServer(int port);
	avf_status_t StopServer();

	INLINE CSocketEvent *GetSocketEvent() {
		return mpSocketEvent;
	}

	virtual CTCPConnection *CreateConnection(CSocket **ppSocket) = 0;

protected:
	CMutex mMutex;
	CTCPConnection *mpFirstConn;

private:
	static void OnCreateConnection(void *context, int port, CSocket **ppSocket);
	void OnDestroyConnection(CTCPConnection *pConnection);

private:
	CNetworkService *mpNetworkService;
	CSocketEvent *mpSocketEvent;
	CCondition mCondWaitDone;
	int m_num_conn;
	int m_port;
	char *mpName;

public:
	INLINE int GetNumConnections() {
		// no lock; so it is an estimation
		return m_num_conn;
	}
};

//-----------------------------------------------------------------------
//
//  CTCPConnection
//
//-----------------------------------------------------------------------
class CTCPConnection: public CObject
{
	typedef CObject inherited;
	friend class CTCPServer;

protected:
	CTCPConnection(CTCPServer *pServer, CSocket **ppSocket);
	virtual ~CTCPConnection();
	void RunThread(const char *pName);

protected:
	virtual void ThreadLoop() = 0;

private:
	static void ThreadEntry(void *p);

public:
	INLINE CTCPConnection *_GetNext() {
		return mpNext;
	}

	INLINE const char *GetIP() {
		return m_ip_addr;
	}

protected:
	avf_status_t TCPSend(int timeout_ms, const avf_u8_t *buffer, avf_uint_t size, bool bCheckClose = true);

	INLINE avf_status_t TCPReceive(int timeout_ms, avf_u8_t *buffer, avf_size_t size) {
		return mpSocketEvent->TCPReceive(mpSocket, timeout_ms, buffer, size);
	}

	INLINE avf_status_t TCPReceiveAny(int timeout_ms, avf_u8_t *buffer, avf_size_t size, int *nread) {
		return mpSocketEvent->TCPReceiveAny(mpSocket, timeout_ms, buffer, size, nread);
	}

protected:
	CTCPConnection *mpNext;
	CTCPServer *mpServer;
	CSocket *mpSocket;
	CSocketEvent *mpSocketEvent;
	bool mbMissed;
	avf_status_t mStatusSend;
	CThread *mpThread;
	char m_ip_addr[20];
};

#endif

