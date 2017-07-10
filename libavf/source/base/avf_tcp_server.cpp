
#define LOG_TAG "tcp_server"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_socket.h"
#include "avf_queue.h"
#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"

#include "avf_tcp_server.h"

CNetworkService *CNetworkService::m_instance = NULL;
CMutex CNetworkService::mMutex;

CNetworkService *CNetworkService::Create()
{
	AUTO_LOCK(mMutex);
	if (m_instance) {
		m_instance->AddRef();
	} else {
		CNetworkService *result = avf_new CNetworkService();
		if (result) {
			result->RunThread();
		}
		CHECK_STATUS(result);
		m_instance = result;
	}
	return m_instance;
}

void CNetworkService::Release()
{
	mMutex.Lock();
	bool bRelease = (avf_atomic_dec(&m_ref_count) == 1);
	if (bRelease) {
		m_instance = NULL;
	}
	mMutex.Unlock();

	if (bRelease) {
		avf_delete this;
	}
}

CNetworkService::CNetworkService():
	m_cmd(CMD_Null),
	m_result(E_OK),
	mpThread(NULL),
	mpSocketEvent(NULL)
{
	SET_ARRAY_NULL(m_server_socket);
	CREATE_OBJ(mpSocketEvent = CSocketEvent::Create("tcp_server_manager"));
}

void CNetworkService::RunThread()
{
	CREATE_OBJ(mpThread = CThread::Create("TCP-Server", ThreadEntry, this));
}

CNetworkService::~CNetworkService()
{
	if (mpThread) {
		// exit server thread
		CmdServer(CMD_Exit, 0, NULL, NULL, NULL);
		mpThread->Release();
		// close all connections
		SAFE_RELEASE_ARRAY(m_server_socket);
	}
	avf_safe_release(mpSocketEvent);
}

void CNetworkService::ThreadEntry(void *p)
{
	((CNetworkService*)p)->ServerLoop();
}

void CNetworkService::ServerLoop()
{
	while (true) {
		int index = -1;

		AVF_LOGI("WaitAcceptMultiple");

		if (mpSocketEvent->WaitAcceptMultiple(with_size(m_server_socket), -1, &index) == E_OK) {
			if (index >= 0 && index < MAX_SERVER) {
				if (m_server_socket[index] != NULL) {
					ProcessConnection(index);
				}
			}
			continue;
		}

		if (ProcessCmd() != E_OK) {
			AVF_LOGD("TCP server exited");
			break;
		}
	}
}

void CNetworkService::ProcessConnection(int index)
{
	CSocket *socket = m_server_socket[index]->Accept(m_entry[index].pName);
	if (socket != NULL) {
		if (m_entry[index].callback != NULL) {
			m_entry[index].callback(m_entry[index].context, m_entry[index].port, &socket);
		}
		if (socket) {
			socket->Release();
		}
	}
}

avf_status_t CNetworkService::ProcessCmd()
{
	AUTO_LOCK(mMutex);
	avf_status_t status = E_OK;

	AVF_LOGI("ProcessCmd %d", m_cmd);

	switch (m_cmd) {
	case CMD_Exit:
		status = E_ERROR;
		break;

	case CMD_StartServer:
		m_result = ProcessStartServerL();
		break;

	case CMD_StopServer:
		m_result = ProcessStopServerL();
		break;

	default:
		AVF_LOGE("unknown cmd %d", m_cmd);
		break;
	}

	mpSocketEvent->Reset();
	mCondAck.Wakeup();

	return status;
}

avf_status_t CNetworkService::ProcessStartServerL()
{
	unsigned index = MAX_SERVER;
	unsigned i;

	// find an empty slot
	for (i = 0; i < MAX_SERVER; i++) {
		if (m_server_socket[i] == NULL) {
			index = i;
		} else if (m_param.port == m_entry[i].port) {
			AVF_LOGE("port %d already used", m_param.port);
			return E_ERROR;
		}
	}

	if (index == MAX_SERVER) {
		AVF_LOGE("StartServer: no entries");
		return E_NOENT;
	}

	CSocket *socket = CSocket::CreateTCPServerSocket(m_param.port, 4, m_param.pName);
	if (socket == NULL) {
		AVF_LOGE("cannot create tcp server on port %d", m_param.port);
		return E_ERROR;
	}

	m_server_socket[index] = socket;
	m_entry[index] = m_param;

	AVF_LOGI("start server %s on port [" C_GREEN "%d" C_NONE "]", m_param.pName, m_param.port);
	return E_OK;
}

avf_status_t CNetworkService::ProcessStopServerL()
{
	unsigned i;

	for (i = 0; i < MAX_SERVER; i++) {
		if (m_server_socket[i] != NULL && m_entry[i].port == m_param.port)
			break;
	}

	if (i == MAX_SERVER) {
		AVF_LOGE("no such server, port %d", m_param.port);
		return E_NOENT;
	}

	avf_safe_release(m_server_socket[i]);

	return E_OK;
}

avf_status_t CNetworkService::CmdServer(int cmd,
	int port, const char *pName,
	tcp_server_callback callback, void *context)
{
	AUTO_LOCK(mMutex);

	while (true) {
		if (m_cmd == CMD_Null) {
			m_cmd = cmd;
			m_param.port = port;
			m_param.pName = pName;
			m_param.callback = callback;
			m_param.context = context;

			mpSocketEvent->Interrupt();
			mCondAck.Wait(mMutex);

			avf_status_t result = m_result;
			m_cmd = CMD_Null;
			mCondRequest.Wakeup();

			return result;
		}

		// already has a cmd to be processed
		mCondRequest.Wait(mMutex);
	}
}

//-----------------------------------------------------------------------
//
//  CTCPServer
//
//-----------------------------------------------------------------------
CTCPServer::CTCPServer(const char *pName):
	mpFirstConn(NULL),
	mpNetworkService(NULL),
	mpSocketEvent(NULL),
	m_num_conn(0),
	m_port(-1),
	mpName(NULL)
{
	mpName = avf_strdup(pName);

	CREATE_OBJ(mpSocketEvent = CSocketEvent::Create(mpName));

	CREATE_OBJ(mpNetworkService = CNetworkService::Create());
}

CTCPServer::~CTCPServer()
{
	StopServer();
	avf_safe_free(mpName);
	avf_safe_release(mpSocketEvent);
}

avf_status_t CTCPServer::RunServer(int port)
{
	if (mpNetworkService == NULL)
		return E_ERROR;

	avf_status_t status = mpNetworkService->StartServer(port, mpName, OnCreateConnection, this);
	if (status == E_OK) {
		m_port = port;
	}

	return status;
}

avf_status_t CTCPServer::StopServer()
{
	if (mpNetworkService == NULL)
		return E_ERROR;

	// stop all connections
	mpSocketEvent->Interrupt();

	// stop server
	if (m_port > 0) {
		mpNetworkService->StopServer(m_port);
	}
	avf_safe_release(mpNetworkService);

	mMutex.Lock();
	while (m_num_conn > 0) {
		mCondWaitDone.Wait(mMutex);
	}
	mMutex.Unlock();

	mpSocketEvent->Reset();

	return E_OK;
}

void CTCPServer::OnCreateConnection(void *context, int port, CSocket **ppSocket)
{
	CTCPServer *self = (CTCPServer*)context;
	CTCPConnection *pConnection = self->CreateConnection(ppSocket);
	if (pConnection) {
		AUTO_LOCK(self->mMutex);

		if (pConnection->mbMissed) {
			AVF_LOGW("missed connection, socket %d : %s",
				pConnection->mpSocket->GetSocket(), self->mpName);
			pConnection->Release();
		} else {
			pConnection->mpNext = self->mpFirstConn;
			self->mpFirstConn = pConnection;
			self->m_num_conn++;
		}
	}
}

void CTCPServer::OnDestroyConnection(CTCPConnection *pConnection)
{
	AUTO_LOCK(mMutex);

	CTCPConnection *pPrev = NULL;
	for (CTCPConnection *conn = mpFirstConn; conn; conn = conn->mpNext) {
		if (conn == pConnection) {
			if (pPrev == NULL) {
				mpFirstConn = pConnection->mpNext;
			} else {
				pPrev->mpNext = pConnection->mpNext;
			}

			// TODO - this is called in the connection thread
			// it is a detached thread
			pConnection->Release();

			m_num_conn--;
			mCondWaitDone.Wakeup();

			return;
		}
		pPrev = conn;
	}

	AVF_LOGE("DestroyConnection not found, socket: %d, server: %s %s",
		pConnection->mpSocket->GetSocket(), pConnection->mpServer->mpName, mpName);
	pConnection->mbMissed = true;
}

//-----------------------------------------------------------------------
//
//  CTCPConnection
//
//-----------------------------------------------------------------------

CTCPConnection::CTCPConnection(CTCPServer *pServer, CSocket **ppSocket):
	mpNext(NULL),
	mpServer(pServer),
	mpSocket(*ppSocket),
	mpSocketEvent(pServer->mpSocketEvent),
	mbMissed(false),
	mStatusSend(E_OK),
	mpThread(NULL)
{
	*ppSocket = NULL;
	mpSocket->GetServerIP(with_size(m_ip_addr));
}

CTCPConnection::~CTCPConnection()
{
	avf_safe_release(mpThread);
	avf_safe_release(mpSocket);
}

void CTCPConnection::RunThread(const char *pName)
{
	if (mpThread == NULL) {
		CREATE_OBJ(mpThread = CThread::Create(pName, ThreadEntry, this, CThread::DETACHED));
	}
}

void CTCPConnection::ThreadEntry(void *p)
{
	CTCPConnection *self = (CTCPConnection*)p;
	self->ThreadLoop();

	CTCPServer *pServer = self->mpServer;
	pServer->OnDestroyConnection(self);
}

avf_status_t CTCPConnection::TCPSend(int timeout_ms, const avf_u8_t *buffer, avf_uint_t size, bool bCheckClose)
{
	// if send was error, don't send anymore
	if (mStatusSend == E_OK) {
		mStatusSend = mpSocketEvent->TCPSend(mpSocket, timeout_ms, buffer, size, bCheckClose);
		if (mStatusSend != E_OK) {
			mpSocket->Shutdown();
		}
	}
	return mStatusSend;
}

