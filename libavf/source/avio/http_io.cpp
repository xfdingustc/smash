
#define LOG_TAG "http_io"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_socket.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "mem_stream.h"
#include "avio.h"

#include "http_io.h"

//-----------------------------------------------------------------------
//
//  CHttpIO
//
//-----------------------------------------------------------------------

IAVIO* CHttpIO::Create()
{
	CHttpIO *result = avf_new CHttpIO();
	CHECK_STATUS(result);
	return result;
}

CHttpIO::CHttpIO():
	mpSocketEvent(NULL),
	mpSocket(NULL),
	mState(STATE_IDLE),
	pm(NULL)
{
	CREATE_OBJ(mpSocketEvent = CSocketEvent::Create("HttpIO"));

	CREATE_OBJ(mpSocket = CSocket::CreateTCPClientSocket("HttpIO"));

	mpSocket->SetReceiveBufferSize(40*KB);

	CREATE_OBJ(pm = CMemBuffer::Create(1*1024));
}

CHttpIO::~CHttpIO()
{
	AUTO_LOCK(mMutex);
	DisconnectLocked();
	avf_safe_release(mpSocket);
	avf_safe_release(mpSocketEvent);
	avf_safe_release(pm);
}

avf_status_t CHttpIO::CreateFile(const char *url)
{
	// not supported
	return E_ERROR;
}

// http://192.168.110.1:8085/buffer/filename.mp4
avf_status_t CHttpIO::OpenRead(const char *url)
{
	AUTO_LOCK(mMutex);
	avf_status_t status;

	if (mState != STATE_IDLE) {
		AVF_LOGE("bad state %d", mState);
		return E_PERM;
	}

	if ((status = ParseUrl(url)) != E_OK)
		return status;

	mState = STATE_CONNECTING;

	// connect
	mMutex.Unlock();
	status = ConnectWithServer();
	mMutex.Lock();

	if (status != E_OK) {
		mState = STATE_ERROR;
		return E_ERROR;
	}

	if (CheckDisconnected())
		return E_ERROR;

	// send request
	mMutex.Unlock();
	status = SendRequest();
	mMutex.Lock();

	if (status != E_OK) {
		mState = STATE_ERROR;
		return E_ERROR;
	}

	if (CheckDisconnected())
		return E_ERROR;

	// receive response
	mMutex.Unlock();
	status = ReceiveResponse();
	mMutex.Lock();

	if (CheckDisconnected())
		return E_ERROR;

	if (status == E_OK) {
		mState = STATE_CONNECTED;
	} else {
		mState = STATE_ERROR;
	}

	return status;
}

avf_status_t CHttpIO::OpenWrite(const char *url)
{
	// not supported
	return E_ERROR;
}

avf_status_t CHttpIO::OpenExisting(const char *url)
{
	return E_ERROR;
}

avf_status_t CHttpIO::Close()
{
	AUTO_LOCK(mMutex);
	return CloseLocked();
}

bool CHttpIO::IsOpen()
{
	AUTO_LOCK(mMutex);
	return mState == STATE_IDLE;
}

int CHttpIO::Read(void *buffer, avf_size_t size)
{
	AUTO_LOCK(mMutex);
	avf_size_t orig_size = size;
	avf_status_t status;

	if (mState != STATE_CONNECTED) {
		AVF_LOGE("not connected");
		return E_PERM;
	}

	mState = STATE_BUSY;

	mMutex.Unlock();
	if (mLineRemain > 0) {
		avf_size_t toread = mLineRemain;
		if (toread > size)
			toread = size;
		::memcpy(buffer, mLinePtr, toread);
		mLinePtr += toread;
		mLineRemain -= toread;
		buffer = (void*)((avf_u8_t*)buffer + toread);
		size -= toread;
	}
	if (size == 0) {
		status = E_OK;
	} else {
		status = mpSocketEvent->TCPReceive(mpSocket, -1, (avf_u8_t*)buffer, size);
	}
	mMutex.Lock();

	if (CheckDisconnected())
		return E_ERROR;

	mState = STATE_CONNECTED;
	if (status != E_OK)
		return status;

	mFileOffset += orig_size;
	return orig_size;
}

int CHttpIO::Write(const void *buffer, avf_size_t size)
{
	AVF_LOGE("Write() not supported");
	return E_UNIMP;
}

avf_status_t CHttpIO::Flush()
{
	// todo
	return E_OK;
}

bool CHttpIO::CanSeek()
{
	// todo
	return false;
}

avf_status_t CHttpIO::Seek(avf_s64_t pos, int whence)
{
	// todo
	return E_ERROR;
}

avf_u64_t CHttpIO::GetSize()
{
	AUTO_LOCK(mMutex);
	return mFileSize;
}

avf_u64_t CHttpIO::GetPos()
{
	AUTO_LOCK(mMutex);
	return mFileOffset;
}

avf_status_t CHttpIO::Disconnect()
{
	AUTO_LOCK(mMutex);
	DisconnectLocked();
	return E_OK;
}

void CHttpIO::DisconnectLocked()
{
	bool bNeedAbort = mState == STATE_CONNECTING || mState == STATE_BUSY;
	mState = STATE_DISCONNECTED;
	if (bNeedAbort) {
		mpSocketEvent->Interrupt();
		mAbortCondition.Wait(mMutex);
		mpSocketEvent->Reset();
	}
}

avf_status_t CHttpIO::CloseLocked()
{
	DisconnectLocked();
	avf_safe_release(mpSocket);
	return E_OK;
}

// http://192.168.110.1:8085/xxx.ts
avf_status_t CHttpIO::ParseUrl(const char *url)
{
	if (::strncasecmp("http://", url, 7) != 0) {
		AVF_LOGE("bad address");
		return E_INVAL;
	}
	url += 7;

	ap<string_t> host;
	ap<string_t> path;
	int port;

	const char *slash = ::strchr(url, '/');
	if (slash == NULL) {
		host = url;
		path = "/";
	} else {
		host = ap<string_t>(url, slash - url);
		path = slash;
	}

	const char *colon = ::strchr(host->string(), ':');
	if (colon == NULL) {
		port = 80;
	} else {
		char *end;
		long tmp = ::strtol(colon + 1, &end, 10);

		if (end <= colon + 1) {
			AVF_LOGE("bad port");
			return E_INVAL;
		}
		if (tmp < 0 || tmp >= 65536) {
			AVF_LOGE("bad port %ld", tmp);
			return E_INVAL;
		}

		port = (int)tmp;
		host = ap<string_t>(host->string(), colon - host->string());
	}

	mHost = host;
	mPort = port;
	mPath = path;

	return E_OK;
}

avf_status_t CHttpIO::ConnectWithServer()
{
	struct sockaddr_in sockaddr;

	::memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(mPort);
	inet_aton(mHost->string(), &sockaddr.sin_addr);

	return mpSocketEvent->Connect(mpSocket, &sockaddr, 30*1000);
}

avf_status_t CHttpIO::SendRequest()
{
	avf_status_t status = E_OK;
	pm->Clear();

	pm->write_string(STR_WITH_SIZE("GET "));
	pm->write_string(mPath->string(), mPath->size());
	pm->write_string(STR_WITH_SIZE(" HTTP/1.1\r\n"));
	pm->write_string(STR_WITH_SIZE("Host: "));
	pm->write_string(mHost->string(), mHost->size());
	char buffer[32];
	int len = sprintf(buffer, ":%d\r\n", mPort);
	pm->write_string(buffer, len);
	pm->write_string(STR_WITH_SIZE("Connection: keep-alive\r\n"));
	pm->write_string(STR_WITH_SIZE("Accept-Encoding: gzip,deflate\r\n"));
	pm->write_string(STR_WITH_SIZE("\r\n"));

	CMemBuffer::block_t *block = pm->GetFirstBlock();
	for (; block; block = block->GetNext()) {
		status = mpSocketEvent->TCPSend(mpSocket, 10*1000, block->GetMem(), block->GetSize());
		if (status != E_OK)
			break;
	}

	pm->Clear();
	return status;
}

#define HTTP_200_OK_STR \
	"HTTP/1.1 200 OK"

#define HTTP_206_OK_STR \
	"HTTP/1.1 206 OK"

#define CONTENT_LENGTH \
	"Content-Length: "

avf_status_t CHttpIO::ReceiveResponse()
{
	avf_status_t status = E_OK;

	mLinePtr = mLineBuffer;
	mLineRemain = 0;

	bool bStatusOK = false;

	while (true) {
		status = ReceiveLine();
		if (status != E_OK)
			break;

		CMemBuffer::block_t *block = pm->GetFirstBlock();
		//AVF_LOGI("line: %s", block->GetMem());
		if (block->GetSize() <= 1)
			break;

		const char *line = (const char *)block->GetMem();
		if (::strncasecmp(line, STR_WITH_SIZE(HTTP_200_OK_STR)) == 0) {
			bStatusOK = true;
			continue;
		}

		if (::strncasecmp(line, STR_WITH_SIZE(CONTENT_LENGTH)) == 0) {
			avf_u64_t length = ::strtoull(line + STR_SIZE(CONTENT_LENGTH), NULL, 10);
			AUTO_LOCK(mMutex);
			mFileSize = length;
			mFileOffset = 0;
			continue;
		}
	}

	pm->Clear();

	if (!bStatusOK) {
		AVF_LOGE("no 200 OK");
		return E_ERROR;
	}

	return status;
}

avf_status_t CHttpIO::ReceiveLine()
{
	pm->Clear();
	while (true) {
		avf_uint_t c;
		avf_status_t status;

		status = GetChar(&c);
		if (status != E_OK)
			return status;

		if (c == '\r') {
			status = GetChar(&c);
			if (status != E_OK)
				return status;
			if (c == '\n') {
				pm->write_char(0);
				return E_OK;
			}
		}

		pm->write_char(c);
	}
}

avf_status_t CHttpIO::FillLineBuffer()
{
	int nbytes;
	avf_status_t status = mpSocketEvent->TCPReceiveAny(mpSocket, -1,
		mLineBuffer, sizeof(mLineBuffer), &nbytes);
	if (status != E_OK)
		return status;
	mLinePtr = mLineBuffer;
	mLineRemain = nbytes;
	return E_OK;
}

