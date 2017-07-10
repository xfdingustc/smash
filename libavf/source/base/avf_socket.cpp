
#define LOG_TAG "socket"

#include <fcntl.h>

#ifdef WIN32_OS
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

#include "avf_common.h"
#include "avf_new.h"
#include "avf_util.h"
#include "avf_socket.h"

//-----------------------------------------------------------------------
//
//  CSocketBuffer - receives input bytes from a socket
//
//-----------------------------------------------------------------------
CSocketBuffer* CSocketBuffer::Create(avf_size_t block_size, avf_size_t limit)
{
	CSocketBuffer *result = avf_new CSocketBuffer(block_size, limit);
	return result;
}

CSocketBuffer::CSocketBuffer(avf_size_t block_size, avf_size_t limit):
	m_block_size(block_size),
	m_limit(limit),
	m_total_size(0),
	mb_ignore_input(false),

	m_first_block(NULL),
	m_first_ptr(NULL),
	m_first_size(0),

	m_last_block(NULL),
	m_last_ptr(NULL),
	m_last_room(0)
{
}

CSocketBuffer::~CSocketBuffer()
{
	block_t *block = m_first_block;
	while (block) {
		block_t *next = block->GetNext();
		avf_free(block);
		block = next;
	}
}

// returns NULL if out of memory
avf_u8_t *CSocketBuffer::BeginWrite(avf_size_t *size)
{
	if (m_last_room == 0) {
		block_t *block = (block_t*)avf_malloc(sizeof(block_t) + m_block_size);
		if (block == NULL)
			return NULL;

		block->next = NULL;

		if (m_last_block) {
			m_last_block->next = block;
		} else {
			m_first_block = block;
			m_first_ptr = block->GetMem();
			m_first_size = 0;
		}

		m_last_block = block;
		m_last_ptr = block->GetMem();
		m_last_room = m_block_size;
	}

	*size = m_last_room;
	return m_last_ptr;
}

// should be called after BeginWrite()
// returns false: bytes buffered exceeds limit
bool CSocketBuffer::EndWrite(avf_size_t bytes_read)
{
	AVF_ASSERT(m_last_ptr);
	AVF_ASSERT(bytes_read <= m_last_room);

	if (mb_ignore_input) {
		AVF_LOGD("EndWrite: %d bytes ignored", bytes_read);
	} else {
		m_last_ptr += bytes_read;
		m_last_room -= bytes_read;

		if (m_last_block == m_first_block) {
			m_first_size += bytes_read;
		}

		m_total_size += bytes_read;

		if (m_total_size > m_limit) {
			AVF_LOGW("socket buffer exceeds limit %d", m_limit);
			return false;
		}
	}

	return true;
}

void CSocketBuffer::IgnoreInput(bool bIgnore)
{
	mb_ignore_input = bIgnore;
	if (bIgnore && m_first_block) {
		AVF_LOGD("IgnoreInput: %d bytes in buffer", m_total_size);
		m_total_size = 0;

		block_t *block = m_first_block->GetNext();
		while (block) {
			block_t *next = block->GetNext();
			avf_free(block);
			block = next;
		}

		m_first_block->next = NULL;
		m_first_ptr = m_first_block->GetMem();
		m_first_size = 0;

		m_last_block = m_first_block;
		m_last_ptr = m_first_ptr;
		m_last_room = m_block_size;
	}
}

// read up to 'size' bytes
// return number of bytes read (might be 0)
avf_size_t CSocketBuffer::Read(avf_u8_t *buffer, avf_size_t size)
{
	avf_size_t total = 0;

	while (m_first_size > 0 && size > 0) {
		avf_size_t tocopy = MIN(size, m_first_size);

		total += tocopy;
		m_total_size -= tocopy;

		::memcpy(buffer, m_first_ptr, tocopy);
		buffer += tocopy;
		size -= tocopy;

		m_first_ptr += tocopy;
		m_first_size -= tocopy;

		if (m_first_size == 0) {
			if (m_first_block == m_last_block) {
				AVF_ASSERT(m_total_size == 0);
				m_first_ptr = m_first_block->GetMem();
				m_last_ptr = m_first_ptr;
				m_last_room = m_block_size;
			} else {
				block_t *next = m_first_block->GetNext();
				avf_free(m_first_block);
				m_first_block = next;
				m_first_ptr = next->GetMem();
				if (next == m_last_block) {
					m_first_size = m_block_size - m_last_room;
				} else {
					m_first_size = m_block_size;
				}
			}
		}
	}

	return total;
}

//-----------------------------------------------------------------------
//
//  CSocket
//
//-----------------------------------------------------------------------

static avf_atomic_t g_num_sockets = 0;
static avf_atomic_t g_num_events = 0;

INLINE static int get_socket_error(void)
{
#ifdef WIN32_OS
	return WSAGetLastError();
#else
	return errno;
#endif
}

extern "C" void avf_socket_info(void)
{
	int num_sockets = g_num_sockets;
	if (num_sockets == 0) {
		AVF_LOGI(C_YELLOW "there are no open sockets" C_NONE);
	} else {
		AVF_LOGE("there are %d sockets not closed", num_sockets);
	}

	int num_events = g_num_events;
	if (num_events == 0) {
		AVF_LOGI(C_YELLOW "there are no open events" C_NONE);
	} else {
		AVF_LOGE("there are %d events not closed", num_events);
	}
}

#ifdef WIN32_OS

CSocket::CSocket(int type, avf_socket_t socket, const char *pName, WSAEVENT event):
	m_type(type),
	m_socket(socket),
	mpName(NULL),
	mEvent(event)
{
	InitSocket(pName);
}

#else

CSocket::CSocket(int type, avf_socket_t socket, const char *pName, CSocketBuffer *pBuffer):
	m_type(type),
	m_socket(socket),
	mpName(NULL),
	mpReadBuffer(pBuffer)
{
	InitSocket(pName);
}

#endif

void CSocket::InitSocket(const char *pName)
{
	mpName = avf_strdup(pName);
	CSocketEvent::SetBlocking(m_socket, false);
	avf_atomic_inc(&g_num_sockets);
}

CSocket::~CSocket()
{
	avf_atomic_dec(&g_num_sockets);
	avf_safe_free(mpName);

#ifdef WIN32_OS
	WSACloseEvent(mEvent);
	avf_atomic_dec(&g_num_events);
#else
	avf_safe_release(mpReadBuffer);
#endif
}

void CSocket::Release()
{
	DestroySocket(m_socket);
	avf_delete this;
}

bool CSocket::CreateSocket(avf_socket_t *psocket)
{
#ifdef WIN32_OS

	*psocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*psocket == INVALID_SOCKET) {
		if (get_socket_error() == WSANOTINITIALISED) {
			AVF_LOGE("WSA not initialized, please call avf_WSAStartup() first");
		} else {
			AVF_LOGE("socket error %d", get_socket_error());
		}
		return false;
	}

#else

	*psocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (*psocket < 0) {
		AVF_LOGE("socket error %d", get_socket_error());
		return false;
	}

#endif

	AVF_LOGD(C_GREEN "create socket %d" C_NONE, *psocket);
	return true;
}

void CSocket::DestroySocket(avf_socket_t socket)
{
	AVF_LOGI(C_GREEN "close socket %d" C_NONE, socket);
#ifdef WIN32_OS
	::closesocket(socket);
#else
	::close(socket);
#endif
}

CSocket *CSocket::CreateSocket(int type, avf_socket_t socket_fd, const char *pName)
{
#ifdef WIN32_OS

	WSAEVENT event = WSACreateEvent();
	if (event == WSA_INVALID_EVENT) {
		AVF_LOGP("WSAEventCreate");
		return NULL;
	}
	avf_atomic_inc(&g_num_events);

	CSocket *socket = avf_new CSocket(type, socket_fd, pName, event);

	if (socket == NULL) {
		WSACloseEvent(event);
		avf_atomic_dec(&g_num_events);
	} else {
		socket->mEvent = event;
	}

#else

	CSocketBuffer *pBuffer = CSocketBuffer::Create(4*KB, 64*KB); // TODO:
	if (pBuffer == NULL) {
		return NULL;
	}

	CSocket *socket = avf_new CSocket(type, socket_fd, pName, pBuffer);

	if (socket == NULL) {
		avf_safe_release(pBuffer);
	}

#endif

	return socket;
}

CSocket *CSocket::CreateTCPServerSocket(int port, int backlog, const char *pName)
{
	struct sockaddr_in server_addr;
	const char *func = NULL;
	avf_socket_t socket_fd;
	CSocket *socket;

	if (!CreateSocket(&socket_fd))
		return NULL;

	::memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	int flag = 1;
	if (::setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag, sizeof(flag)) < 0) {
		func = "setsockopt";
		goto Fail;
	}

	if (::bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		func = "bind";
		goto Fail;
	}

	if (::listen(socket_fd, backlog) < 0) {
		func = "listen";
		goto Fail;
	}

	socket = CreateSocket(TYPE_TCP_SERVER, socket_fd, pName);
	if (socket == NULL) {
		func = "new CSocket";
		goto Fail;
	}

	return socket;

Fail:
	AVF_LOGE("%s error %d", func, get_socket_error());
	DestroySocket(socket_fd);
	return NULL;
}

CSocket *CSocket::CreateTCPClientSocket(const char *pName)
{
	avf_socket_t socket_fd;

	if (!CreateSocket(&socket_fd))
		return NULL;

	CSocket *socket = CreateSocket(TYPE_TCP_CLIENT, socket_fd, pName);

	if (socket == NULL) {
		DestroySocket(socket_fd);
	}

	return socket;
}

CSocket *CSocket::Accept(const char *pName)
{
	struct sockaddr_storage peer_addr;
	socklen_t addr_size = sizeof(peer_addr);

#ifdef WIN32_OS

	avf_socket_t socket_fd = ::accept(m_socket, (struct sockaddr *)&peer_addr, &addr_size);
	if (socket_fd == INVALID_SOCKET) {
		AVF_LOGE("accept error %d", get_socket_error());
		return NULL;
	}

#else

	avf_socket_t socket_fd = ::accept(m_socket, (struct sockaddr *)&peer_addr, &addr_size);
	if (socket_fd < 0) {
		AVF_LOGP("accept");
		return NULL;
	}

	if (peer_addr.ss_family == AF_INET) {
		const avf_u8_t *p = (const avf_u8_t*)&peer_addr;
		AVF_LOGI("socket %d addr %d.%d.%d.%d, port %d",
			socket_fd, p[4], p[5], p[6], p[7], (p[2]<<8)|p[3]);
	}

#endif

	AVF_LOGI(C_YELLOW "accept socket %d : %s" C_NONE, socket_fd, pName);

	CSocket *socket = CreateSocket(TYPE_TCP_REMOTE, socket_fd, pName);

	if (socket == NULL) {
		DestroySocket(socket_fd);
	}

	return socket;
}

bool CSocket::TestConnected()
{
#ifdef WIN32_OS

	// If the socket is connection oriented and the remote side has shut down the connection gracefully, 
	// and all data has been received, a recv will complete immediately with zero bytes received. 
	// If the connection has been reset, a recv will fail with the error WSAECONNRESET.
	int rval = ::recv(m_socket, NULL, 0, 0);
	if (rval == 0 || (rval == SOCKET_ERROR && get_socket_error() == WSAECONNRESET)) {
		AVF_LOGD(C_YELLOW "socket %d is closed, rval = %d" C_NONE, m_socket, rval);
		return false;
	}
	return true;

#else

	avf_status_t status = ReadBuffer();
	return status == E_OK;

#endif
}

// recv()

// These calls return the number of bytes received, or -1 if an error occurred. 
// The return value will be 0 when the peer has performed an orderly shutdown.

// If no messages are available at the socket, the receive calls wait for a message to arrive,
// unless the socket is nonblocking (see fcntl(2)), in which case the value -1 is returned and 
// the external variable errno is set to EAGAIN or EWOULDBLOCK. 

// POSIX.1-2001 allows either error to be returned for this case, and does not require these 
// constants to have the same value, so a portable application should check for both possibilities.

avf_status_t CSocket::ReceiveNoWait(avf_u8_t *buffer, int size, int *nread)
{
	int rval = ::recv(m_socket, (char*)buffer, size, 0);

	if (rval < 0) {
		int error = get_socket_error();

#ifdef WIN32_OS
		if (error == WSAEWOULDBLOCK) {
			*nread = 0;
			return E_OK;
		}
#else
		if (error == EAGAIN || error == EWOULDBLOCK) {
			*nread = 0;
			return E_OK;
		}
#endif

		AVF_LOGW("socket %d ReceiveNoWait returns error %d", m_socket, error);
		return E_IO;
	}

	if (rval == 0) {
		AVF_LOGI(C_YELLOW "socket %d closed by remote" C_NONE, m_socket);
		return E_CLOSE;
	}

	*nread = rval;
	return E_OK;
}

// send()

// On success, these calls return the number of characters sent. 
// On error, -1 is returned, and errno is set appropriately.

// EAGAIN or EWOULDBLOCK: The socket is marked nonblocking and the requested operation would block. 
// POSIX.1-2001 allows either error to be returned for this case, and does not require these constants to 
// have the same value, so a portable application should check for both possibilities.

avf_status_t CSocket::SendNoWait(const avf_u8_t *buffer, int size, int *nwrite)
{
#if defined(WIN32_OS) || defined(IOS_OS)
	int flags = 0;
#elif defined(MAC_OS)
	int flags = SO_NOSIGPIPE;
#else
	int flags = MSG_NOSIGNAL;
#endif

	int rval = ::send(m_socket, (const char *)buffer, size, flags);

	if (rval < 0) {
		int error = get_socket_error();

#ifdef WIN32_OS
		if (error == WSAEWOULDBLOCK) {
			*nwrite = 0;
			return E_OK;
		}
#else
		if (error == EAGAIN || error == EWOULDBLOCK) {
			*nwrite = 0;
			return E_OK;
		}
#endif

		AVF_LOGI("socket %d SendNoWait returns error %d", m_socket, error);
		return E_IO;
	}

	*nwrite = rval;
	return E_OK;
}

#ifndef WIN32_OS

// pull all input bytes into socket buffer, until it would block
// returns error if disconnected or error happens
avf_status_t CSocket::ReadBuffer()
{
	avf_size_t size = 0;
	avf_u8_t *buf = NULL;

	while (true) {
		if (size == 0) {
			if ((buf = mpReadBuffer->BeginWrite(&size)) == NULL)
				return E_NOMEM;
		}

		int nread;
		avf_status_t status = ReceiveNoWait(buf, size, &nread);
		if (status != E_OK)
			return status;

		if (nread == 0) {
			// read nothing
			return E_OK;
		}

		buf += nread;
		size -= nread;

		if (!mpReadBuffer->EndWrite(nread)) {
			// exceeds limit
			return E_OVERFLOW;
		}
	}
}

#endif

avf_status_t CSocket::GetServerIP(char *buffer, int size)
{
	struct sockaddr_in server_addr;
	socklen_t server_len = sizeof(server_addr);

	if (::getsockname(m_socket, (struct sockaddr *)&server_addr, &server_len) < 0) {
		AVF_LOGP("getsockname");
		buffer[0] = 0;
		return E_OK;
	}

#ifdef WIN32_OS
	char *addr = ::inet_ntoa(server_addr.sin_addr);
	avf_snprintf(buffer, size, "%s", addr);
	buffer[size-1] = 0;
#else
	::inet_ntop(AF_INET, &server_addr.sin_addr, buffer, size);
#endif

	return E_OK;
}

avf_status_t CSocket::Shutdown()
{
#ifdef LINUX_OS
	AVF_LOGD("shutdown socket %d", m_socket);
	if (::shutdown(m_socket, SHUT_RDWR) < 0) {
		AVF_LOGP("shutdown socket %d", m_socket);
		return E_ERROR;
	}
#endif
	return E_OK;
}

avf_status_t CSocket::KeepAlive(int keep_idle, int keep_interval, int keep_count)
{
#ifdef LINUX_OS
	int keep_alive = 1;

	if (::setsockopt(m_socket, SOL_SOCKET, SO_KEEPALIVE, (void*)&keep_alive, sizeof(keep_alive)) < 0) {
		AVF_LOGP("SO_KEEPALIVE failed");
		return E_ERROR;
	}

	if (::setsockopt(m_socket, SOL_TCP, TCP_KEEPIDLE, (void*)&keep_idle, sizeof(keep_idle)) < 0) {
		AVF_LOGP("TCP_KEEPIDLE");
		return E_ERROR;
	}

	if (::setsockopt(m_socket, SOL_TCP, TCP_KEEPINTVL, (void*)&keep_interval, sizeof(keep_interval)) < 0) {
		AVF_LOGP("TCP_KEEPINTVL");
		return E_ERROR;
	}

	if (::setsockopt(m_socket, SOL_TCP, TCP_KEEPCNT, (void*)&keep_count, sizeof(keep_count)) < 0) {
		AVF_LOGP("TCP_KEEPCNT");
		return E_ERROR;
	}
#endif
	return E_OK;
}

avf_status_t CSocket::SetSendBufferSize(int size)
{
	if (::setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(int)) < 0) {
		AVF_LOGE("SetSendBufferSize failed, size=%d, error=%d", size, get_socket_error());
		return E_ERROR;
	}
	return E_OK;
}

avf_status_t CSocket::SetReceiveBufferSize(int size)
{
	if (::setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(int)) < 0) {
		AVF_LOGE("SetReceiveBufferSize failed, size=%d, error=%d", size, get_socket_error());
		return E_ERROR;
	}
	return E_OK;
}

extern "C" int avf_WSAStartup(int major, int minor)
{
#ifdef WIN32_OS
	WORD wVersionRequested = MAKEWORD(minor, major);
	WSADATA wsaData;

	int err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		AVF_LOGE("WSAStartup failed: err = %d", err);
		return E_ERROR;
	}
#endif
	return E_OK;
}

extern "C" int avf_WSACleanup(void)
{
#ifdef WIN32_OS
	WSACleanup();
#endif
	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CSocketEvent
//
//-----------------------------------------------------------------------

#ifdef MINGW
#define pipe(_fds) _pipe(_fds, 4096, _O_BINARY)
#endif

CSocketEvent* CSocketEvent::Create(const char *pName)
{
	CSocketEvent *result = avf_new CSocketEvent(pName);
	CHECK_STATUS(result);
	return result;
}

CSocketEvent::CSocketEvent(const char *pName)
{
	mpName = avf_strdup(pName);

#ifdef WIN32_OS

	mEvent = WSACreateEvent();
	if (mEvent == WSA_INVALID_EVENT) {
		AVF_LOGP("WSAEventCreate");
		return;
	}
	avf_atomic_inc(&g_num_events);

#else

	m_read_fd = -1;
	m_write_fd = -1;

	int fd[2];
	if (::pipe(fd) < 0) {
		AVF_LOGP("pipe");
		return;
	}

	avf_pipe_created(fd);

	m_read_fd = fd[0];
	SetBlocking(m_read_fd, false);

	m_write_fd = fd[1];

#endif
}

CSocketEvent::~CSocketEvent()
{
#ifdef WIN32_OS

	if (mEvent != WSA_INVALID_EVENT) {
		WSACloseEvent(mEvent);
		avf_atomic_dec(&g_num_events);
	}

#else

	avf_safe_close(m_read_fd);
	avf_safe_close(m_write_fd);

#endif

	avf_safe_free(mpName);
}

void CSocketEvent::Interrupt()
{
#ifdef WIN32_OS

	if (!WSASetEvent(mEvent)) {
		AVF_LOGE("WSASetEvent error: %d", get_socket_error());
	}

#else

	char buf = 0;
	int rval = ::write(m_write_fd, &buf, 1);
	AVF_UNUSED(rval);

#endif
}

void CSocketEvent::Reset()
{
#ifdef WIN32_OS

	if (!WSAResetEvent(mEvent)) {
		AVF_LOGE("WSAResetEvent error: %d", get_socket_error());
	}

#else

	char buffer[16];
	while (::read(m_read_fd, buffer, sizeof(buffer)) > 0)
		continue;

#endif
}

INLINE static void ms_to_timeval(int timeout_ms, struct timeval *tv)
{
	tv->tv_sec = timeout_ms / 1000;
	tv->tv_usec = (timeout_ms % 1000) * 1000;
}

bool CSocketEvent::SetBlocking(avf_socket_t fd, bool bBlocking)
{
#ifdef WIN32_OS

	u_long mode = bBlocking ? 0 : 1;
	if (::ioctlsocket(fd, FIONBIO, &mode) != 0) {
		AVF_LOGE("ioctlsocket failed, error=%d", get_socket_error());
		return false;
	}
	return true;

#else

	int flags = ::fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return false;

	if (bBlocking) {
		if (test_flag(flags, O_NONBLOCK))
			flags &= ~O_NONBLOCK;
		else
			return true;
	} else {
		if (!test_flag(flags, O_NONBLOCK))
			flags |= O_NONBLOCK;
		else
			return true;
	}

	if (::fcntl(fd, F_SETFL, flags) < 0) {
		AVF_LOGE("fcntl %d failed, error=%d", flags, get_socket_error());
		return false;
	}

	return true;

#endif
}

avf_status_t CSocketEvent::Connect(CSocket *socket, const struct sockaddr_in *addr, int timeout_ms)
{
	return _Connect(socket, (const struct sockaddr *)addr, sizeof(struct sockaddr_in), timeout_ms);
}

#ifdef WIN32_OS

avf_status_t CSocketEvent::WaitSingle(CSocket *socket, int timeout_ms, int fd, int fd_bit)
{
	while (true) {
		WSAEVENT events[2];

		events[0] = this->mEvent;

		int rval = ::WSAEventSelect(socket->m_socket, socket->mEvent, fd | FD_CLOSE);
		if (rval != 0) {
			AVF_LOGE("WSAEventSelect failed: error = %d", get_socket_error());
			return E_IO;
		}
		events[1] = socket->mEvent;

		DWORD result = WSAWaitForMultipleEvents(2, events, FALSE, 
			timeout_ms < 0 ? WSA_INFINITE : timeout_ms, FALSE);

		if (result == WSA_WAIT_TIMEOUT) {
			AVF_LOGW("connect timeout (%d seconds), socket %d", timeout_ms/1000, socket->m_socket);
			return E_TIMEOUT;
		}

		if (result < WSA_WAIT_EVENT_0 || result >= WSA_WAIT_EVENT_0 + 2) {
			AVF_LOGE("WSAWaitForMultipleEvents unexpected result: %d", (int)result);
			return E_ERROR;
		}

		if (result == WSA_WAIT_EVENT_0) {
			// socket event is signalled, should give up.
			AVF_LOGW("WaitSingle cancelled, socket %d", socket->m_socket);
			return E_CANCEL;
		}

		WSANETWORKEVENTS nw_event;
		result = WSAEnumNetworkEvents(socket->m_socket, socket->mEvent, &nw_event);
		if (result != 0) {
			AVF_LOGE("WSAEnumNetworkEvents failed, error=%d", get_socket_error());
			return E_ERROR;
		}

		if (nw_event.lNetworkEvents & fd) {
			if (nw_event.iErrorCode[fd_bit] == 0) {
				return E_OK;
			} else {
				AVF_LOGE("event %d error occured %d", fd, nw_event.iErrorCode[fd_bit]);
				return E_ERROR;
			}
		} else if (nw_event.lNetworkEvents & FD_CLOSE) {
			if (nw_event.iErrorCode[FD_CLOSE_BIT] == 0) {
				return E_CLOSE;
			} else {
				AVF_LOGE("event close error occured %d", nw_event.iErrorCode[FD_CLOSE_BIT]);
				return E_ERROR;
			}
		}

		AVF_LOGW("unexpected network event on '%s', events: %x", socket->GetName(), (int)nw_event.lNetworkEvents);
		// avf_msleep(100);
	}
}

#define NUM_EVENTS_MAX	16

// timeout_ms = -1 : wait forever
// return E_ERROR on pipe event or timeout
avf_status_t CSocketEvent::WaitMultiple(CSocket *sockets[], int num_fd, 
	int timeout_ms, int *index_out, int fd, int fd_bit)
{
	while (true) {
		WSAEVENT events[1 + NUM_EVENTS_MAX];
		int indices[1 + NUM_EVENTS_MAX];
		int nevents = 0;

		if (num_fd > NUM_EVENTS_MAX) {
			AVF_LOGE("too many sockets %d", num_fd);
			return E_FATAL;
		}

		events[nevents] = this->mEvent;
		indices[nevents] = -1;
		nevents++;

		for (int i = 0; i < num_fd; i++) {
			if (sockets[i] != NULL) {
				int rval = ::WSAEventSelect(sockets[i]->m_socket, sockets[i]->mEvent, fd | FD_CLOSE);
				if (rval != 0) {
					AVF_LOGE("WSAEventSelect failed, error = %d", get_socket_error());
					return E_IO;
				}
				events[nevents] = sockets[i]->mEvent;
				indices[nevents] = i;
				nevents++;
			}
		}

		DWORD result = WSAWaitForMultipleEvents(nevents, events, FALSE, 
			timeout_ms < 0 ? WSA_INFINITE : timeout_ms, FALSE);

		if (result == WSA_WAIT_TIMEOUT) {
			AVF_LOGW("connect timeout (%d seconds)", timeout_ms/1000);
			return E_TIMEOUT;
		}

		if (result < WSA_WAIT_EVENT_0 || result >= WSA_WAIT_EVENT_0 + nevents) {
			AVF_LOGE("WSAWaitForMultipleEvents unexpected result: %d", (int)result);
			return E_ERROR;
		}

		int index = result - WSA_WAIT_EVENT_0;
		if (index == 0) {
			// socket event is signalled, should give up.
			//AVF_LOGW("WaitMultiple cancelled");
			return E_CANCEL;
		}

		int tmp = indices[index];
		if (index_out != NULL) {
			*index_out = tmp;
		}
		CSocket *s = sockets[tmp];

		WSANETWORKEVENTS nw_event;
		result = WSAEnumNetworkEvents(s->m_socket, s->mEvent, &nw_event);
		if (result != 0) {
			AVF_LOGE("WSAEnumNetworkEvents failed, error=%d", get_socket_error());
			return E_ERROR;
		}

		if (nw_event.lNetworkEvents & fd) {
			if (nw_event.iErrorCode[fd_bit] == 0) {
				return E_OK;
			} else {
				AVF_LOGE("event %d error occured %d", fd, nw_event.iErrorCode[fd_bit]);
				return E_ERROR;
			}
		} else if (nw_event.lNetworkEvents & FD_CLOSE) {
			if (nw_event.iErrorCode[FD_CLOSE_BIT] == 0) {
				return E_CLOSE;
			} else {
				AVF_LOGE("event close error occured %d", nw_event.iErrorCode[FD_CLOSE_BIT]);
				return E_ERROR;
			}
		}

		AVF_LOGW("unexpected network event on '%s', events: %x", s->GetName(), (int)nw_event.lNetworkEvents);
		//avf_msleep(100);
	}
}

avf_status_t CSocketEvent::_Connect(CSocket *socket, const struct sockaddr *addr, int addrlen, int timeout_ms)
{
	int rval = ::connect(socket->m_socket, addr, addrlen);

	if (rval == 0) {
		return E_OK;
	}

	if (rval == SOCKET_ERROR && get_socket_error() == WSAEWOULDBLOCK) {
		return WaitSingle(socket, timeout_ms, FD_WRITE, FD_WRITE_BIT);
	}

	AVF_LOGW("unexpected connect error, rval=%d, error=%d", rval, get_socket_error());
	return E_ERROR;
}

#else

int CSocketEvent::Select(int max, fd_set *read_fds, fd_set *write_fds, int timeout_ms)
{
	if (timeout_ms >= 0) {
		struct timeval tv;
		ms_to_timeval(timeout_ms, &tv);
		return ::select(max + 1, read_fds, write_fds, NULL, &tv);
	} else {
		return ::select(max + 1, read_fds, write_fds, NULL, NULL);
	}
}

avf_status_t CSocketEvent::_Connect(CSocket *socket, const struct sockaddr *addr, int addrlen, int timeout_ms)
{
	if (::connect(socket->m_socket, addr, addrlen) == 0) {
		// in case: connected immediately
		AVF_LOGD("socket %d connect() returns immediately", socket->m_socket);
		return E_OK;
	}

	if (get_socket_error() != EINPROGRESS) {
		AVF_LOGE("connect returns error %d", get_socket_error());
		return E_IO;
	}

	// EINPROGRESS: the socket is non-blocking and the connection could not be established immediately.
	while (true) {
		fd_set read_fds;
		fd_set write_fds;

		// read FDs - socket & pipe
		FD_ZERO(&read_fds);
		FD_SET(socket->m_socket, &read_fds);
		FD_SET(m_read_fd, &read_fds);

		// write FDs - socket
		FD_ZERO(&write_fds);
		FD_SET(socket->m_socket, &write_fds);

		int max = MAX(socket->m_socket, m_read_fd);
		int rval = Select(max, &read_fds, &write_fds, timeout_ms);

		if (rval < 0) {
			AVF_LOGP("connect,select");
			return E_ERROR;
		}

		if (rval == 0) {
			AVF_LOGW("connect timeout (%d seconds), socket %d", timeout_ms/1000, socket->m_socket);
			return E_TIMEOUT;
		}

		if (FD_ISSET(socket->m_socket, &write_fds)) {
			if (FD_ISSET(socket->m_socket, &read_fds)) {
				// write-able and read-able: error, or server just sends us something
				if (::connect(socket->m_socket, addr, addrlen) < 0 && get_socket_error() != EISCONN) {
					AVF_LOGW("connect error %d, socket %d", get_socket_error(), socket->m_socket);
					return E_ERROR;
				}
			}
			// 1. write-able and not read-able: connection is successful
			// 2. write-able and read-able, and connect() returns EISCONN
			return E_OK;
		}

		if (FD_ISSET(m_read_fd, &read_fds)) {
			// cancelled by pipe
			AVF_LOGE("connect cancelled");
			return E_CANCEL;
		}

		AVF_LOGE("connect: unknown select");
		avf_msleep(100);
	}
}

#endif

#ifdef WIN32_OS

avf_status_t CSocketEvent::WaitAcceptMultiple(CSocket *socket[], int num_fd, int timeout_ms, int *index_out)
{
	return WaitMultiple(socket, num_fd, timeout_ms, index_out, FD_ACCEPT, FD_ACCEPT_BIT);
}

avf_status_t CSocketEvent::WaitInputMultiple(CSocket *socket[], int num_fd, int timeout_ms, int *index_out)
{
	return WaitMultiple(socket, num_fd, timeout_ms, index_out, FD_READ, FD_READ_BIT);
}

#else

// timeout_ms = -1 : wait forever
// return E_ERROR on pipe event or timeout
avf_status_t CSocketEvent::WaitAcceptMultiple(CSocket *socket[], int num_fd, int timeout_ms, int *index)
{
	return WaitInputMultiple(socket, num_fd, timeout_ms, index);
}

// for linux use only
avf_status_t CSocketEvent::WaitFileInput(int input_fd, int timeout_ms)
{
	fd_set read_fds;
	int max;
	int rval;

	while (true) {
		FD_ZERO(&read_fds);
		FD_SET(m_read_fd, &read_fds);
		FD_SET(input_fd, &read_fds);
		max = MAX(m_read_fd, input_fd);

		rval = Select(max, &read_fds, NULL, timeout_ms);

		if (rval < 0) {
			AVF_LOGP("WaitFileInput,select");
			return E_ERROR;
		}

		if (rval == 0) {
			AVF_LOGW("WaitFileInput timeout (%d seconds)", timeout_ms/1000);
			return E_TIMEOUT;
		}

		if (FD_ISSET(input_fd, &read_fds)) {
			return E_OK;
		}

		if (FD_ISSET(m_read_fd, &read_fds)) {
			return E_CANCEL;
		}

		AVF_LOGE("WaitFileInput: unknown select");
	}
}

avf_status_t CSocketEvent::WaitSingleFileInput(int input_fd, int timeout_ms)
{
	fd_set read_fds;
	int rval;

	while (true) {
		FD_ZERO(&read_fds);
		FD_SET(input_fd, &read_fds);

		rval = Select(input_fd + 1, &read_fds, NULL, timeout_ms);

		if (rval < 0) {
			AVF_LOGP("WaitSingleFileInput,select");
			return E_ERROR;
		}

		if (rval == 0) {
			AVF_LOGW("WaitSingleFileInput timeout (%d seconds)", timeout_ms/1000);
			return E_TIMEOUT;
		}

		if (FD_ISSET(input_fd, &read_fds)) {
			return E_OK;
		}

		AVF_LOGE("WaitSingleFileInput: unknown select");
	}
}

// timeout_ms = -1 : wait forever
// return E_ERROR on pipe event or timeout
avf_status_t CSocketEvent::WaitInputMultiple(CSocket *socket[], int num_fd, int timeout_ms, int *index)
{
	fd_set read_fds;
	int max;
	int rval;

	while (true) {
		FD_ZERO(&read_fds);
		FD_SET(m_read_fd, &read_fds);
		max = m_read_fd;

		for (int i = 0; i < num_fd; i++) {
			if (socket[i] != NULL) {
				int fd = socket[i]->m_socket;
				if (fd >= 0) {
					FD_SET(fd, &read_fds);
					if (max < fd)
						max = fd;
				}
			}
		}

		rval = Select(max, &read_fds, NULL, timeout_ms);

		if (rval < 0) {
			AVF_LOGP("WaitInputMultiple,select");
			for (int i = 0; i < num_fd; i++) {
				AVF_LOGD("fd[%d]: %d", i, socket[i] ? socket[i]->m_socket : 0);
			}
			return E_ERROR;
		}

		if (rval == 0) {
			//AVF_LOGW("WaitInputMultiple timeout (%d seconds)", timeout_ms/1000);
			return E_TIMEOUT;
		}

		for (int i = 0; i < num_fd; i++) {
			if (socket[i] != NULL) {
				int fd = socket[i]->m_socket;
				if (fd >= 0 && FD_ISSET(fd, &read_fds)) {
					if (index != NULL) {
						*index = i;
					}
					return E_OK;
				}
			}
		}

		if (FD_ISSET(m_read_fd, &read_fds)) {
			// cancelled by pipe
			// AVF_LOGW("WaitInputMultiple cancelled");
			return E_CANCEL;
		}

		AVF_LOGE("WaitInputMultiple: unknown select");
	}
}

#endif

#ifdef WIN32_OS

// timeout_ms = -1 : wait forever
// return E_ERROR on pipe event or timeout
avf_status_t CSocketEvent::WaitOutput(CSocket *socket, int timeout_ms, bool bCheckClose)
{
	return WaitSingle(socket, timeout_ms, FD_WRITE, FD_WRITE_BIT);
}

#else

// timeout_ms = -1 : wait forever
// return E_ERROR on pipe event or timeout
avf_status_t CSocketEvent::WaitOutput(CSocket *socket, int timeout_ms, bool bCheckClose)
{
	fd_set read_fds;
	fd_set write_fds;
	int max;
	int rval;

	int socket_fd = socket->m_socket;
	while (true) {
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);

		// read: socket & pipe
		FD_SET(m_read_fd, &read_fds);

		if (bCheckClose) {
			FD_SET(socket_fd, &read_fds);
		}

		// write: socket
		FD_SET(socket_fd, &write_fds);

		// max
		max = m_read_fd;
		if (max < socket_fd)
			max = socket_fd;

		rval = Select(max, &read_fds, &write_fds, timeout_ms);

		if (rval < 0) {
			AVF_LOGP("socket %d WaitOutput,select", socket->m_socket);
			return E_ERROR;
		}

		if (rval == 0) {
			// timeout
			AVF_LOGW("WaitOutput timeout (%d seconds), socket %d", timeout_ms/1000, socket->m_socket);
			return E_TIMEOUT;
		}

		if (FD_ISSET(socket_fd, &write_fds)) {
			// write-able
			return E_OK;
		}

		if (FD_ISSET(m_read_fd, &read_fds)) {
			// aborted
			AVF_LOGW("WaitOutput cancelled, socket %d", socket->m_socket);
			return E_CANCEL;
		}

		if (FD_ISSET(socket_fd, &read_fds)) {
			avf_status_t status = socket->ReadBuffer();
			if (status != E_OK) {
				return status;
			}
			continue;
		}

		AVF_LOGE("WaitOutput: unknown select");
	}
}

#endif

avf_status_t CSocketEvent::TCPSend(CSocket *socket, int timeout_ms, 
	const avf_u8_t *buffer, avf_size_t size, bool bCheckClose)
{
	while (size > 0) {
		avf_status_t status;
		int nwrite;

		if ((status = socket->SendNoWait(buffer, size, &nwrite)) != E_OK)
			return status;

		if (nwrite == 0) {
			if ((status = WaitOutput(socket, timeout_ms, bCheckClose)) != E_OK)
				return status;
			continue;
		}

		buffer += nwrite;
		size -= nwrite;
	}

	return E_OK;
}

avf_status_t CSocketEvent::TCPReceive(CSocket *socket, int timeout_ms, avf_u8_t *buffer, avf_size_t size)
{
#ifndef WIN32_OS

	avf_size_t tmp = socket->mpReadBuffer->Read(buffer, size);
	buffer += tmp;
	size -= tmp;

#endif

	int wait = -1;
	while (size > 0) {
		avf_status_t status;
		int nread;

		if ((status = socket->ReceiveNoWait(buffer, size, &nread)) != E_OK)
			return status;

		if (nread == 0) {
			if ((status = WaitInput(socket, wait)) != E_OK) {
				AVF_LOGD("WaitInput returns %d, socket %d", status, socket->m_socket);
				return status;
			}
			continue;
		}

		buffer += nread;
		size -= nread;

		wait = timeout_ms;
	}

	return E_OK;
}

avf_status_t CSocketEvent::TCPReceiveAny(CSocket *socket, int timeout_ms, avf_u8_t *buffer, avf_size_t size, int *nread)
{
#ifndef WIN32_OS

	avf_size_t tmp = socket->mpReadBuffer->Read(buffer, size);
	if (tmp > 0) {
		*nread = tmp;
		return E_OK;
	}

#endif

	while (true) {
		avf_status_t status;
		int _nread;

		if ((status = socket->ReceiveNoWait(buffer, size, &_nread)) != E_OK)
			return status;

		if (_nread == 0) {
			if ((status = WaitInput(socket, timeout_ms)) != E_OK)
				return status;
			continue;
		}

		*nread = _nread;
		return E_OK;
	}
}

