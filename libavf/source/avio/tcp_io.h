
#ifndef __TCP_IO_H__
#define __TCP_IO_H__

#include "avio.h"
#include "avf_tcp_server.h"

class TCTPWriteServer;
class CTCPWriter;
class CSocket;

//-----------------------------------------------------------------------
//
//  TCTPWriteServer
//
//-----------------------------------------------------------------------
class TCTPWriteServer: public CTCPServer
{
	typedef CTCPServer inherited;

public:
	enum {
		MAX_CONN = 4,
	};

public:
	static TCTPWriteServer *Create();

protected:
	TCTPWriteServer();
	virtual ~TCTPWriteServer();

public:
	avf_status_t StartServer(int port);
	void SendBuffer(const avf_u8_t *buffer, avf_size_t bytes);

protected:
	virtual CTCPConnection *CreateConnection(CSocket **ppSocket);

private:
	void CloseAll();
	void OnConnection(int port, CSocket *socket);
	static void OnConnectionCB(void *context, int port, CSocket *socket);

private:
	void StartWrite(CSocket *socket[], bool closed[]);
	void EndWrite(CSocket *socket[], bool closed[]);

private:
	bool mb_closed;
	CSocket *mpSocket[MAX_CONN];
};

//-----------------------------------------------------------------------
//
//  CTCPWriter
//
//-----------------------------------------------------------------------

class CTCPWriter: public CAVIO
{
public:
	static IAVIO *Create(int port, avf_size_t buffer_size);

private:
	CTCPWriter(int port, avf_size_t buffer_size);
	virtual ~CTCPWriter();

// IAVIO
public:
	virtual avf_status_t CreateFile(const char *url);
	virtual avf_status_t OpenRead(const char *url);
	virtual avf_status_t OpenWrite(const char *url);
	virtual avf_status_t OpenExisting(const char *url);
	virtual avf_status_t Close();
	virtual bool IsOpen() { return true; }
	virtual const char *GetIOType() { return "CTCPWriter"; }
	virtual int Read(void *buffer, avf_size_t size) { return E_ERROR; }
	virtual int Write(const void *buffer, avf_size_t size);
	virtual avf_status_t Flush() { return E_OK; }
	//virtual int ReadAt(void *buffer, avf_size_t size, avf_u64_t pos);
	//virtual int WriteAt(const void *buffer, avf_size_t size, avf_u64_t pos);
	virtual bool CanSeek() { return false; }
	virtual avf_status_t Seek(avf_s64_t pos, int whence) { return E_ERROR; }
	virtual avf_u64_t GetSize() { return 0; }
	virtual avf_u64_t GetPos() { return 0; }

private:
	TCTPWriteServer *mpServer;
	avf_u8_t *mpBuffer;
	avf_size_t mPos;
	avf_size_t mRemain;
};

#endif

