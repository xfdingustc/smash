
#ifndef __HTTP_MJPEG_H__
#define __HTTP_MJPEG_H__

struct CConfigNode;
class CBufferQueue;
class CSocketEvent;
class CTransaction;
class CTransactionList;
class CMjpegServer;
class CNetworkService;
class CHttpMjpegFilter;
class CHttpMjpegFilterInput;
class CSocket;

typedef struct server_config_s {
	int port;
	int read_timeout;
	int write_timeout;

	server_config_s() {
		port = 8081;
		read_timeout = 10;	// seconds
		write_timeout = 10;	// seconds
	}
} server_config_t;

//-----------------------------------------------------------------------
//
//  CDataQueue
//
//-----------------------------------------------------------------------
class CBufferQueue
{
public:
	CBufferQueue();
	~CBufferQueue();

public:
	struct BufferItem {
		CBuffer *pBuffer;
	};

public:
	void SetFPS(int fps);
	void PushBuffer(const BufferItem *pBufferItem, avf_u64_t tick);
	bool PopBuffer(BufferItem *pBufferItem, bool *pbTimeout);
	void Enable(bool bEnabled = true);
	void Disable() { Enable(false); }
	void ReleaseAll();

private:
	enum { MAX_BUFFERS = 1 };
	enum { TIMEOUT = 100 };

	CMutex mMutex;
	CSemaphore mReadSem;

	bool mbDisabled;
	int m_frame_length;
	avf_u64_t m_last_tick;
	int m_total_buffers;
	int m_dropped_buffers;
	BufferItem mBuffers[MAX_BUFFERS];
	int m_read_index;
	int m_write_index;
	int m_num_buffers;
};

//-----------------------------------------------------------------------
//
//  CTransaction - can only be used by CTransationList
//
//-----------------------------------------------------------------------
class CTransaction: public CObject
{
	friend class CTransactionList;

private:
	static CTransaction* Create(CTransactionList *pList, CSocketEvent *pSocketEvent, 
		CSocket **ppSocket, const server_config_t *pConfig);

private:
	CTransaction(CTransactionList *pList, CSocketEvent *pSocketEvent, 
		CSocket **ppSocket, const server_config_t *pConfig);
	virtual ~CTransaction();
	void RunThread();

private:
	INLINE void DisableBufferQueue() {
		mBufferQueue.Disable();
	}
	INLINE void PushBuffer(const CBufferQueue::BufferItem *pBufferItem, avf_u64_t tick) {
#ifdef AVF_DEBUG
		CBuffer *pBuffer = pBufferItem->pBuffer;
		total_bytes += pBuffer->GetDataSize();
		if ((pBuffer = pBuffer->mpNext) != NULL) {
			total_bytes += pBuffer->GetDataSize();
		}
#endif
		mBufferQueue.PushBuffer(pBufferItem, tick);
	}
	INLINE bool PopBuffer(CBufferQueue::BufferItem *pBufferItem, bool *pbTimeout) {
		return mBufferQueue.PopBuffer(pBufferItem, pbTimeout);
	}

private:
	void TransactionLoop();
	static void ThreadEntry(void *p);

private:
	avf_status_t WriteMem(const avf_u8_t *buffer, avf_size_t size);
	avf_status_t WriteResponse();
	avf_status_t WriteBuffer(CBufferQueue::BufferItem *pBufferItem);
	avf_status_t ReadRequest(int *fps);

private:
	const server_config_t *m_server_config;

	CTransactionList *mpList;
	CTransaction *mpNext;

	CSocketEvent *mpSocketEvent;
	CSocket *mpSocket;
	CThread *mpThread;
	CBufferQueue mBufferQueue;

#ifdef AVF_DEBUG
	avf_u64_t total_bytes;
	avf_u64_t start_tick;
#endif
};

//-----------------------------------------------------------------------
//
//  CTransactionList
//
//-----------------------------------------------------------------------
class CTransactionList: public CObject
{
	friend class CTransaction;

public:
	static CTransactionList* Create(CSocketEvent *pSocketEvent);

private:
	CTransactionList(CSocketEvent *pSocketEvent);
	virtual ~CTransactionList();

public:
	avf_status_t NewTransaction(CSocket **ppSocket, const server_config_t *pConfig);
	void PushBuffer(const CBufferQueue::BufferItem *pBufferItem);
	void StopAllTransactions();
	void StartTransaction() { mbStopped = false; }

public:
	enum { MAX_CONNECTIONS = 8 };

private:
	avf_status_t RemoveTransaction(CTransaction *pTrans);

private:
	CSocketEvent *mpSocketEvent;
	CMutex mMutex;
	CCondition mCondWaitDone;
	avf_uint_t mNumWaitTrans;
	avf_uint_t mNumTransactions;
	bool mbStopped;
	CTransaction *mpFirstTransaction;
};

//-----------------------------------------------------------------------
//
//  CMjpegServer
//
//-----------------------------------------------------------------------
class CMjpegServer: public CObject
{
public:
	static CMjpegServer* Create(const server_config_t *pConfig);

private:
	CMjpegServer(const server_config_t *pConfig);
	virtual ~CMjpegServer();

public:
	avf_status_t StartServer(const server_config_t *pConfig);
	avf_status_t StopServer();

	void PushBuffer(const CBufferQueue::BufferItem *pBufferItem) {
		mpTransList->PushBuffer(pBufferItem);
	}

private:
	static void OnConnected(void *context, int port, CSocket **ppSocket);

private:
	CNetworkService *mpNetworkService;
	const server_config_t *m_server_config;
	CSocketEvent *mpSocketEvent;
	CTransactionList *mpTransList;
};

//-----------------------------------------------------------------------
//
//  CHttpMjpegFilter
//
//-----------------------------------------------------------------------
class CHttpMjpegFilter: public CFilter
{
	typedef CFilter inherited;
	friend class CHttpMjpegFilterInput;

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CHttpMjpegFilter(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CHttpMjpegFilter();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_status_t PreRunFilter();
	virtual avf_status_t RunFilter() { return E_OK; }
	virtual avf_status_t StopFilter() { return E_OK; }
	virtual avf_size_t GetNumInput() { return 1; }
	virtual IPin *GetInputPin(avf_uint_t index);

private:
	CHttpMjpegFilterInput *mpInput;
	CMjpegServer *mpServer;
	server_config_t mServerConfig;
	int m_fps_filter;
	int fps_nominator;
	int fps_denominator;
};

//-----------------------------------------------------------------------
//
//  CHttpMjpegFilterInput
//
//-----------------------------------------------------------------------
class CHttpMjpegFilterInput: public CInputPin
{
	typedef CInputPin inherited;
	friend class CHttpMjpegFilter;

private:
	CHttpMjpegFilterInput(CHttpMjpegFilter *pFilter);
	virtual ~CHttpMjpegFilterInput();

// IPin
public:
	virtual void ReceiveBuffer(CBuffer *pBuffer);
	virtual void PurgeInput() {}

	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);
};

#endif

