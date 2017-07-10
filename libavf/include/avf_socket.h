
#ifndef __AVF_SOCKET_H__
#define __AVF_SOCKET_H__

class CSocketBuffer;
class CSocket;
class CSocketEvent;

//-----------------------------------------------------------------------
//
//  CSocketBuffer - receives input bytes from a socket
//
//-----------------------------------------------------------------------
class CSocketBuffer
{
public:
	static CSocketBuffer* Create(avf_size_t block_size, avf_size_t limit);
	void Release() {
		avf_delete this;
	}

public:
	avf_u8_t *BeginWrite(avf_size_t *size);
	bool EndWrite(avf_size_t bytes_read);
	avf_size_t Read(avf_u8_t *buffer, avf_size_t size);
	void IgnoreInput(bool bIgnore);

private:
	CSocketBuffer(avf_size_t block_size, avf_size_t limit);
	~CSocketBuffer();

private:
	struct block_t {
		struct block_t *next;
		// follows memory
		INLINE avf_u8_t *GetMem() {
			return (avf_u8_t*)(this + 1);
		}
		INLINE struct block_t *GetNext() {
			return next;
		}
	};

private:
	const avf_size_t m_block_size;
	const avf_size_t m_limit;
	avf_size_t m_total_size;
	bool mb_ignore_input;

	block_t *m_first_block;
	avf_u8_t *m_first_ptr;
	avf_uint_t m_first_size;

	block_t *m_last_block;
	avf_u8_t *m_last_ptr;
	avf_uint_t m_last_room;
};

//-----------------------------------------------------------------------
//
//  CSocket - non-blocking
//
//-----------------------------------------------------------------------

#ifdef WIN32_OS
//#include <winsock2.h>
//typedef SOCKET avf_socket_t;
typedef unsigned int avf_socket_t;
typedef void *WSAEVENT;
#ifndef WSA_INVALID_EVENT
#define WSA_INVALID_EVENT 0
#endif
#else
typedef int avf_socket_t;
#endif

class CSocket
{
	friend class CSocketEvent;

public:
	static CSocket *CreateTCPServerSocket(int port, int backlog, const char *pName);
	static CSocket *CreateTCPClientSocket(const char *pName);
	void Release();

private:
	static CSocket *CreateSocket(int type, avf_socket_t socket_fd, const char *pName);

public:
	CSocket *Accept(const char *pName);
	bool TestConnected();
	avf_status_t ReceiveNoWait(avf_u8_t *buffer, int size, int *nread);
	avf_status_t SendNoWait(const avf_u8_t *buffer, int size, int *nwrite);
	avf_status_t GetServerIP(char *buffer, int size);
	avf_status_t Shutdown();
	INLINE const char *GetName() { return mpName; }
	INLINE avf_socket_t GetSocket() { return m_socket; }
	avf_status_t KeepAlive(int keep_idle, int keep_interval, int keep_count);

public:
	avf_status_t SetSendBufferSize(int size);
	avf_status_t SetReceiveBufferSize(int size);

private:
	enum {
		TYPE_TCP_SERVER = 0,
		TYPE_TCP_CLIENT = 1,
		TYPE_TCP_REMOTE = 2,	// accepted by server
	};

private:
#ifdef WIN32_OS
	CSocket(int type, avf_socket_t socket, const char *pName, WSAEVENT event);
#else
	CSocket(int type, avf_socket_t socket, const char *pName, CSocketBuffer *pBuffer);
#endif
	void InitSocket(const char *pName);
	~CSocket();

private:
	static bool CreateSocket(avf_socket_t *psocket);
	static void DestroySocket(avf_socket_t socket);

public:
#ifdef WIN32_OS
	INLINE void IgnoreInput(bool bIgnore) {}
#else
	INLINE void IgnoreInput(bool bIgnore) {
		mpReadBuffer->IgnoreInput(bIgnore);
	}
#endif

private:
	int m_type;
	avf_socket_t m_socket;
	char *mpName;

#ifdef WIN32_OS

private:
	WSAEVENT mEvent;

#else

private:
	avf_status_t ReadBuffer();

private:
	CSocketBuffer *mpReadBuffer;

#endif

public:
	INLINE static int GetNumSockets() {
		return m_num_sockets;
	}

private:
	static avf_atomic_t m_num_sockets;
};

// for WIN32, major 2, minor 2
extern "C" int avf_WSAStartup(int major, int minor);
extern "C" int avf_WSACleanup(void);

//-----------------------------------------------------------------------
//
//  CSocketEvent
//
//-----------------------------------------------------------------------

struct sockaddr;
struct sockaddr_in;

class CSocketEvent
{
public:
	static CSocketEvent* Create(const char *pName);
	void Release() { avf_delete this; }

private:
	CSocketEvent(const char *pName);
	~CSocketEvent();

#ifdef WIN32_OS

	INLINE avf_status_t Status() {
		return mEvent == WSA_INVALID_EVENT ? E_ERROR : E_OK;
	}

#else

	INLINE avf_status_t Status() {
		return m_read_fd < 0 ? E_ERROR : E_OK;
	}

#endif

public:
	void Interrupt();
	void Reset();

#ifndef WIN32_OS
	int GetFileNo() { return m_read_fd; }
#endif

	avf_status_t _Connect(CSocket *socket, const struct sockaddr *addr, int addrlen, int timeout_ms);
	avf_status_t Connect(CSocket *socket, const struct sockaddr_in *addr, int timeout_ms);

#ifdef WIN32_OS
	avf_status_t WaitSingle(CSocket *socket, int timeout_ms, int fd, int fd_bit);
	avf_status_t WaitMultiple(CSocket *sockets[], int num_fd, int timeout_ms, int *index_out,
		int fd, int fd_bit);
#else
	avf_status_t ReadBuffer(CSocket *socket);
#endif

	avf_status_t WaitAcceptMultiple(CSocket *socket[], int num_fd, int timeout_ms, int *index);
	avf_status_t WaitInputMultiple(CSocket *socket[], int num_fd, int timeout_ms, int *index);
	INLINE avf_status_t WaitInput(CSocket *socket, int timeout_ms) {
		return WaitInputMultiple(&socket, 1, timeout_ms, NULL);
	}
	avf_status_t WaitOutput(CSocket *socket, int timeout_ms, bool bCheckClose);

	avf_status_t TCPSend(CSocket *socket, int timeout_ms, const avf_u8_t *buffer, avf_size_t size, bool bCheckClose = true);
	avf_status_t TCPReceive(CSocket *socket, int timeout_ms, avf_u8_t *buffer, avf_size_t size);
	avf_status_t TCPReceiveAny(CSocket *socket, int timeout_ms, avf_u8_t *buffer, avf_size_t size, int *nread);

public:
	avf_status_t WaitFileInput(int input_fd, int timeout_ms);
	static avf_status_t WaitSingleFileInput(int input_fd, int timeout_ms);

public:
	static bool SetBlocking(avf_socket_t fd, bool bBlocking);
	INLINE const char *GetName() { return mpName; }

#ifndef WIN32_OS
private:
	static int Select(int max, fd_set *read_fds, fd_set *write_fds, int timeout_ms);
#endif

private:
	char *mpName;

#ifdef WIN32_OS

private:
	WSAEVENT mEvent;

#else

private:
	int m_read_fd;
	int m_write_fd;

#endif
};

#endif

