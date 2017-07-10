
#define LOG_TAG "tcp_io"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_socket.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avio.h"
#include "avf_util.h"

#include "tcp_io.h"

//-----------------------------------------------------------------------
//
//  TCTPWriteServer
//
//-----------------------------------------------------------------------
TCTPWriteServer* TCTPWriteServer::Create()
{
	TCTPWriteServer *result = avf_new TCTPWriteServer();
	CHECK_STATUS(result);
	return result;
}

TCTPWriteServer::TCTPWriteServer():
	inherited("TCPWriterServer")
{
	mb_closed = false;
	SET_ARRAY_NULL(mpSocket);
}

TCTPWriteServer::~TCTPWriteServer()
{
	CloseAll();
	// CTCPServer::~CTCPServer() will be called
}

void TCTPWriteServer::CloseAll()
{
	AUTO_LOCK(mMutex);
	mb_closed = true;
	SAFE_RELEASE_ARRAY(mpSocket);
}

avf_status_t TCTPWriteServer::StartServer(int port)
{
	return RunServer(port);
}

CTCPConnection *TCTPWriteServer::CreateConnection(CSocket **ppSocket)
{
	AUTO_LOCK(mMutex);
	if (!mb_closed) {
		for (unsigned i = 0; i < MAX_CONN; i++) {
			if (mpSocket[i] == NULL) {
				mpSocket[i] = *ppSocket;
				*ppSocket = NULL;
				return NULL;
			}
		}
	}
	return NULL;
}

void TCTPWriteServer::StartWrite(CSocket *socket[], bool closed[])
{
	AUTO_LOCK(mMutex);
	for (unsigned i = 0; i < MAX_CONN; i++) {
		socket[i] = mpSocket[i];
		closed[i] = false;
	}
}

void TCTPWriteServer::EndWrite(CSocket *socket[], bool closed[])
{
	AUTO_LOCK(mMutex);
	for (unsigned i = 0; i < MAX_CONN; i++) {
		if (closed[i]) {
			mpSocket[i] = NULL;
		}
	}
}

void TCTPWriteServer::SendBuffer(const avf_u8_t *buffer, avf_size_t bytes)
{
	CSocket *socket[MAX_CONN];
	bool closed[MAX_CONN];

	StartWrite(socket, closed);

	// todo - consider slow connection
	for (unsigned i = 0; i < MAX_CONN; i++) {
		if (socket[i] != NULL) {
			avf_status_t status = GetSocketEvent()->TCPSend(socket[i], 10*1000, buffer, bytes);
			if (status != E_OK) {
				AVF_LOGD("SendBuffered failed");
				avf_safe_release(socket[i]);
				closed[i] = true;
			}
		}
	}

	EndWrite(socket, closed);
}

//-----------------------------------------------------------------------
//
//  CTCPWriter
//
//-----------------------------------------------------------------------
IAVIO* CTCPWriter::Create(int port, avf_size_t buffer_size)
{
	CTCPWriter *result = avf_new CTCPWriter(port, buffer_size);
	CHECK_STATUS(result);
	return result;
}

CTCPWriter::CTCPWriter(int port, avf_size_t buffer_size):
	mpServer(NULL),
	mpBuffer(NULL),
	mPos(0),
	mRemain(buffer_size)
{
	CREATE_OBJ(mpBuffer = avf_malloc_bytes(buffer_size));

	CREATE_OBJ(mpServer = TCTPWriteServer::Create());

	avf_status_t status = mpServer->StartServer(port);
	if (status != E_OK) {
		mStatus = status;
		return;
	}
}

CTCPWriter::~CTCPWriter()
{
	avf_safe_release(mpServer);
	avf_safe_free(mpBuffer);
}

avf_status_t CTCPWriter::CreateFile(const char *url)
{
	return E_ERROR;
}

avf_status_t CTCPWriter::OpenRead(const char *url)
{
	return E_ERROR;
}

avf_status_t CTCPWriter::OpenWrite(const char *url)
{
	return E_ERROR;
}

avf_status_t CTCPWriter::OpenExisting(const char *url)
{
	return E_ERROR;
}

avf_status_t CTCPWriter::Close()
{
	return E_ERROR;
}

int CTCPWriter::Write(const void *buffer, avf_size_t size)
{
	int total = (int)size;

	while (1) {
		avf_size_t tocopy = MIN(size, mRemain);
		::memcpy(mpBuffer + mPos, buffer, tocopy);
		mPos += tocopy;
		mRemain -= tocopy;

		if (mRemain == 0) {
			mpServer->SendBuffer(mpBuffer, mPos);
			mRemain = mPos;
			mPos = 0;
		}

		size -= tocopy;
		buffer = (const void*)((avf_u8_t*)buffer + tocopy);

		if (size == 0)
			break;
	}

	return total;
}

