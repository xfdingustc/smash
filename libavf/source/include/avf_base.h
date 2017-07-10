
#ifndef __AVF_BASE_H__
#define __AVF_BASE_H__

class CObject;
class CRawDataCache;
class CBufferPool;
class CProxyBufferPool;
class CMemoryBufferPool;
class CWorkQueue;
class CFilter;
class CPin;
class CInputPin;
class COutputPin;
class CQueueInputPin;
class CActiveFilter;
class CActiveRenderer;
class CFilterGraph;
class CMediaFileReader;
class CMediaTrack;
class CMediaFileWriter;
class IAVIO;

#include "avf_new.h"
#include "avf_media_format.h"

//#define MONITOR_BUFFERS

//-----------------------------------------------------------------------
//
//  CObject - IInterface implementation
//
//-----------------------------------------------------------------------
class CObject: public IInterface
{
public:
	virtual void *GetInterface(avf_refiid_t refiid);
	virtual void AddRef() {
		avf_atomic_inc(&m_ref_count);
	}
	virtual void Release();

protected:
	CObject(): mStatus(E_OK), m_ref_count(1) {}
	virtual ~CObject() {}

protected:
	avf_status_t mStatus;	// keeps tracking the result of constructor
	avf_atomic_t m_ref_count;	// reference counter

public:
	avf_status_t Status() { return mStatus; }
	int _GetRefCount() { return m_ref_count; }
};

//-----------------------------------------------------------------------
//
//  CRawDataCache
//
//-----------------------------------------------------------------------
class CRawDataCache: public CObject
{
	enum {
		CACHE_MAX = 128,
	};

public:
	static CRawDataCache* Create();

private:
	CRawDataCache();
	virtual ~CRawDataCache();

public:
	raw_data_t *AllocRawData(avf_size_t size, avf_u32_t fcc);

private:
	static void ReleaseRawData(raw_data_t *raw);

private:
	CMutex mMutex;
	raw_data_t *mpList;
	avf_size_t mCount;
	avf_size_t mDataSize;
	avf_size_t mUnmatchCount;	// debug
};

//-----------------------------------------------------------------------
//
//  CBufferPool: base class
//
//-----------------------------------------------------------------------
class CBufferPool: public CObject, public IBufferPool
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

protected:
	CBufferPool(const char *pName, avf_uint_t maxBuffers);
	virtual ~CBufferPool();

// IBufferPool
public:
	virtual const char *GetName() { return mName->string(); }
	//virtual avf_u32_t GetFlags() = 0;
	virtual avf_size_t GetMaxNumBuffers() { return mMaxBuffers; }
	virtual CMsgQueue* GetBufferQueue() { return mpBufferQ; }
	//virtual bool AllocBuffer(CBuffer*& pBuffer, CMediaFormat *pFormat, avf_size_t size) = 0;
	//virtual avf_status_t AllocMem(CBuffer *pBuffer, avf_size_t size) = 0;
	virtual void __ReleaseBuffer(CBuffer *pBuffer);	// called by CBuffer
	virtual avf_u32_t GetAllocatedMem() {
		return 0;
	}

protected:
	avf_size_t mMaxBuffers;	// can grow in sub class
	CMsgQueue *mpBufferQ;
	ap<string_t> mName;
};

//-----------------------------------------------------------------------
//
//  CProxyBufferPool: CBuffer without mem
//
//-----------------------------------------------------------------------
class CProxyBufferPool: public CBufferPool
{
	typedef CBufferPool inherited;

public:
	static CProxyBufferPool* Create(const char *pName, 
		avf_size_t maxBuffers, avf_size_t objectSize = sizeof(CBuffer));

protected:
	CProxyBufferPool(const char *pName, 
		avf_size_t maxBuffers, avf_size_t objectSize);
	virtual ~CProxyBufferPool();

// IBufferPool
public:
	virtual avf_u32_t GetFlags() { return 0; }
	virtual bool AllocBuffer(CBuffer*& pBuffer, CMediaFormat *pFormat, avf_size_t size);
	virtual avf_status_t AllocMem(CBuffer *pBuffer, avf_size_t size);

protected:
	avf_size_t mObjectSize;
	avf_u8_t *mpObjectMemory;
};

//-----------------------------------------------------------------------
//
//  CMemoryBufferPool: buffers are pre-allocated in memory by new()
//
//-----------------------------------------------------------------------
class CMemoryBufferPool: public CProxyBufferPool
{
	typedef CProxyBufferPool inherited;

public:
	static CMemoryBufferPool* Create(const char *pName, 
		avf_size_t maxBuffers, avf_size_t objectSize, avf_size_t bufferSize);

protected:
	CMemoryBufferPool(const char *pName, 
		avf_size_t maxBuffers, avf_size_t objectSize, avf_size_t bufferSize);
	virtual ~CMemoryBufferPool();

// IBufferPool
public:
	virtual avf_u32_t GetFlags() { return F_HAS_MEMORY; }
	virtual bool AllocBuffer(CBuffer*& pBuffer, CMediaFormat *pFormat, avf_size_t size);
	//virtual avf_status_t AllocMem(CBuffer *pBuffer, avf_size_t size);

protected:
	avf_u8_t *mpDataMemory;
};

//-----------------------------------------------------------------------
//
//  CDynamicBufferPool - dynamically allocate a buffer by new()
//
//-----------------------------------------------------------------------
class CDynamicBufferPool: public CProxyBufferPool
{
	typedef CProxyBufferPool inherited;

public:
	static CDynamicBufferPool* DoCreate(const char *pName, IAllocMem *pMalloc,
		avf_size_t maxBuffers, avf_size_t objectSize, bool bNonBlock);

	// will block when no buffer
	static INLINE CDynamicBufferPool* CreateBlock(const char *pName, IAllocMem *pMalloc,
			avf_size_t maxBuffers) {
		return DoCreate(pName, pMalloc, maxBuffers, sizeof(CBuffer), false);
	}

	// will allocate new buffer when no buffer
	static INLINE CDynamicBufferPool* CreateNonBlock(const char *pName, IAllocMem *pMalloc,
			avf_size_t maxBuffers) {
		return DoCreate(pName, pMalloc, maxBuffers, sizeof(CBuffer), true);
	}

protected:
	CDynamicBufferPool(const char *pName, IAllocMem *pMalloc,
		avf_size_t maxBuffers, avf_size_t objectSize, bool bNonBlock);
	virtual ~CDynamicBufferPool();

// IBufferPool
public:
	virtual avf_u32_t GetFlags() {
		return F_HAS_MEMORY | (mbNonBlock ? F_HAS_BUFFER : 0);
	}
	virtual bool AllocBuffer(CBuffer*& pBuffer, CMediaFormat *pFormat, avf_size_t bufSize);
	virtual avf_status_t AllocMem(CBuffer *pBuffer, avf_size_t size);
	virtual void __ReleaseBuffer(CBuffer *pBuffer);	// calle by CBuffer
	virtual avf_u32_t GetAllocatedMem() {
		return avf_atomic_get(&m_total_mem);
	}

private:
	void FreeBufferData(CBuffer *pBuffer);
	void ReleaseOneBuffer(CBuffer *pBuffer);
	avf_u8_t *AllocData(avf_size_t size) {
		return mpMalloc ? (avf_u8_t*)mpMalloc->AllocMem(size) : 
			(avf_u8_t*)avf_malloc_bytes(size);
	}
	INLINE void FreeData(void *data, avf_size_t size) {
		if (mpMalloc) mpMalloc->FreeMem(data, size);
		else avf_free(data);
	}

private:
	bool mbNonBlock;		// if CBuffer can be increased
	avf_size_t m_allocs;
	avf_atomic_t m_total_mem;
	IAllocMem *mpMalloc;
};

//-----------------------------------------------------------------------
//
//  CWorkQueue
//		When created, this object is in ThreadLoop() state,
//			and can respond to CMD_RUN, CMD_TERMINATE
//		When CMD_RUN is received, it enters AOLoop() state,
//			and can respond to CMD_STOP.
//		other commands are processed by calling the ActiveObject's
//			AOCmdProc(cmd, bInAOLoop).
//
//-----------------------------------------------------------------------
class CWorkQueue
{
	typedef IActiveObject AO;

public:
	static CWorkQueue* Create(AO *pAO);
	void Release();

private:
	CWorkQueue(AO *pAO);
	~CWorkQueue();
	void RunThread();

public:
	// 
	INLINE void SetEventFlag(avf_enum_t flag) {
		mpMsgHub->SetEventFlag(flag);
	}

	INLINE void ClearAllEvents() {
		mpMsgHub->ClearAllEvents();
	}

	INLINE avf_size_t RemoveAllCmds(avf_queue_remove_msg_proc proc, void *context) {
		return mpCmdQ->ClearMsgQ(proc, context);
	}

	// return receiver's ReplyCmd()
	INLINE avf_status_t SendCmd(avf_uint_t code, void *pExtra = NULL) {
		AO::CMD cmd(code, pExtra);
		return mpCmdQ->SendMsg(&cmd, sizeof(cmd));
	}

	INLINE avf_status_t SendCmd(avf_intptr_t sid, avf_uint_t code, void *pExtra) {
		AO::CMD cmd(code, pExtra);
		cmd.sid = sid;
		return mpCmdQ->SendMsg(&cmd, sizeof(cmd));
	}

	INLINE void PostCmd(AO::CMD& cmd) {
		avf_status_t status = mpCmdQ->PostMsg(&cmd, sizeof(cmd));
		AVF_ASSERT_OK(status);
	}

	// post a cmd to the thread without waiting
	INLINE void PostCmd(avf_uint_t code, void *pExtra = NULL) {
		AO::CMD cmd(code, pExtra);
		avf_status_t status = mpCmdQ->PostMsg(&cmd, sizeof(cmd));
		AVF_ASSERT_OK(status);
	}

	// is it the cmd queue?
	INLINE bool IsCmdQ(CMsgQueue *pMsgQ) {
		return pMsgQ == mpCmdQ;
	}

	// peek a cmd (without waiting)
	INLINE bool PeekCmd(AO::CMD& cmd) {
		return mpCmdQ->PeekMsg(&cmd, sizeof(cmd));
	}

	// get a cmd (with waiting)
	INLINE void GetCmd(AO::CMD& cmd) {
		mpCmdQ->GetMsg(&cmd, sizeof(cmd));
	}

	// command the AO to enter AOLoop() from ThreadLoop()
	INLINE avf_status_t Run() {
		return SendCmd(AO::CMD_RUN);
	}

	// command the AO to exit AOLoop() to ThreadLoop()
	INLINE avf_status_t Stop() {
		return SendCmd(AO::CMD_STOP);
	}

	// 
	INLINE void ReplyCmd(avf_status_t result) {
		mpCmdQ->ReplyMsg(result);
	}

	// attach the msg queue to our hub
	INLINE avf_status_t AttachMsgQueue(CMsgQueue *pMsgQ, int type) {
		return pMsgQ->AttachToHub(mpMsgHub, type);
	}

	// detach the msg queue from our hub
	INLINE avf_status_t DetachMsgQueue(CMsgQueue *pMsgQ) {
		return pMsgQ->DetachFromHub();
	}

	// detach all the msg queues from our hub
	INLINE void DetachAllMsgQueues() {
		mpMsgHub->DetachAllMsgQueues();
	}

	// wait until any msg queue has a msg (including the cmd queue)
	// return false: got nothing
	INLINE bool WaitMsg(CMsgHub::MsgResult& result, int types, avf_int_t timeout = -1) {
		return mpMsgHub->WaitMsg(result, mpCmdQ, types, timeout);
	}

	// wait until pQ or my 'mpCmdQ' has msgs
	INLINE void WaitMsgQueues(CMsgHub::MsgResult& result, CMsgQueue *pQ, bool bGetFull) {
		mpMsgHub->WaitMsgQueues(result, mpCmdQ, pQ, bGetFull);
	}

	INLINE void SetRTPriority(avf_enum_t priority) {
		mpThread->SetThreadPriority(priority);
	}

private:
	static void ThreadEntry(void *p);
	void ThreadLoop();

	// command the ThreadLoop() to exit
	INLINE void Terminate() {
		PostCmd(AO::CMD_TERMINATE);
	}

private:
	INLINE avf_status_t Status() {
		return mStatus;
	}
	avf_status_t mStatus;
	AO *mpAO;		// the active object
	CMsgQueue *mpCmdQ;	// the command queue for the AO
	CMsgHub *mpMsgHub;	// the msg hub for the AO
	CThread *mpThread;	// my working thread
};

//-----------------------------------------------------------------------
//
//  CFilter
//
//-----------------------------------------------------------------------
class CFilter: public CObject, public IFilter
{
	typedef CObject inherited;
	friend class CPin;
	DEFINE_INTERFACE;

protected:
	CFilter(IEngine *pEngine, const char *pName):
		mpEngine(pEngine), mFilterIndex(-1), mTypeIndex(0), mName(pName) {
	}
	virtual ~CFilter() {}

// IFilter
public:
	virtual avf_status_t InitFilter() { return E_OK; }

	virtual avf_status_t PreRunFilter();
	//virtual avf_status_t RunFilter();
	//virtual avf_status_t StopFilter();
	virtual avf_status_t PostStopFilter() { return E_OK; }

	virtual avf_status_t ForceEOS() {
		AVF_LOGW("%s ForceEOS", GetFilterName());
		return E_ERROR;
	}

	virtual void SetFilterIndex(int index) {
		mFilterIndex = index;
	}
	virtual void SetTypeIndex(int index) {
		mTypeIndex = index;
	}
	virtual int GetFilterIndex() { return mFilterIndex; }
	virtual int GetTypeIndex() { return mTypeIndex; }
	virtual const char *GetFilterName() { return mName->string(); }

	virtual avf_size_t GetNumInput() { return 0; }
	virtual avf_size_t GetNumOutput() { return 0; }
	virtual IPin *GetInputPin(avf_uint_t index) { return NULL; }
	virtual IPin *GetOutputPin(avf_uint_t index) { return NULL; }

public:
	INLINE void PostFilterMsg(avf_uint_t code, avf_intptr_t p1 = 0) {
		avf_msg_t msg;
		msg.code = code;
		msg.p0 = (avf_intptr_t)static_cast<IFilter*>(this);
		msg.p1 = p1;
		mpEngine->PostFilterMsg(msg);
	}

	INLINE void PostErrorMsg(avf_status_t error) {
		AVF_LOGD("PostErrorMsg %d", error);
		PostFilterMsg(IEngine::MSG_ERROR, error);
	}

	INLINE void PostPreparedMsg() {
		PostFilterMsg(IEngine::MSG_PREPARED, 0);
	}

	INLINE void PostStartedMsg() {
		PostFilterMsg(IEngine::MSG_STARTED, 0);
	}

	INLINE void PostEosMsg() {
		PostFilterMsg(IEngine::MSG_EOS, 0);
	}

protected:
	INLINE IRecordObserver *GetRecordObserver() {
		return (IRecordObserver*)mpEngine->FindComponent(IID_IRecordObserver);
	}

	INLINE void SetEOS(CPin *pPin, CBuffer *pBuffer);

protected:
	IEngine *mpEngine;	// the engine
	avf_int_t mFilterIndex;	// in filter graph
	avf_int_t mTypeIndex;	// index of the same type
	ap<string_t> mName;
};

//-----------------------------------------------------------------------
//
//  CPin
//
//-----------------------------------------------------------------------
class CPin: public CObject, public IPin
{
	typedef CObject inherited;
	DEFINE_INTERFACE;
	friend class CFilter;

protected:
	CPin(CFilter *pFilter, avf_uint_t index, const char *pName);
	virtual ~CPin();

// IPin
public:
	virtual const char *GetPinName() { return mName->string(); }
	virtual IFilter *GetFilter() { return mpFilter; }
	virtual avf_uint_t GetPinIndex() { return mPinIndex; }

	//virtual void ReceiveBuffer(CBuffer *pBuffer) = 0;
	//virtual void PurgeInput() = 0;
	virtual void ResetState() {
		mbEOS = false;
#ifdef MONITOR_BUFFERS
		for (unsigned i = 0; i < ARRAY_SIZE(mBuffers); i++) {
			mBuffers[i].num = 0;
			mBuffers[i].num_last = 0;
		}
#endif
	}

	//virtual bool CanConnect();
	//virtual int Connected();
	virtual bool NeedReconnect() { return false; }

	//virtual CMediaFormat* QueryMediaFormat() = 0;
	//virtual IBufferPool *QueryBufferPool() = 0;
	//virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
	//		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool) = 0;

	//virtual int Connect(IPin *pPeer, IBufferPool *pBufferPool, CMediaFormat *pMediaFormat);
	//virtual void Disconnect(IPin *pPeer);
	//virtual CMsgQueue* GetBufferQueue() = 0;

	virtual bool GetEOS() {
		return mbEOS;
	}

	virtual avf_int_t GetBuffers(avf_enum_t type);
	virtual avf_int_t GetBuffersDiff(avf_enum_t type);

	virtual CMediaFormat *GetUsingMediaFormat() {
		return mUsingMediaFormat.get();
	}
	virtual IBufferPool *GetUsingBufferPool() {
		return mUsingBufferPool.get();
	}

protected:
	void SetEOS() {
		mbEOS = true;
	}

	INLINE void IncBuffers(avf_enum_t type, int value) {
#ifdef MONITOR_BUFFERS
		AVF_ASSERT(type < ARRAY_SIZE(mBuffers));
		avf_atomic_add(&mBuffers[type].num, value);
#endif
	}
	INLINE void DecBuffers(avf_enum_t type, int value) {
#ifdef MONITOR_BUFFERS
		AVF_ASSERT(type < ARRAY_SIZE(mBuffers));
		avf_atomic_sub(&mBuffers[type].num, value);
#endif
	}

	INLINE void IncBuffersSent(int value = 1) {
		IncBuffers(BUFFERS_SENT, value);
	}
	INLINE void IncBuffersReceived(int value = 1) {
		IncBuffers(BUFFERS_RECEIVED, value);
	}
	INLINE void IncBuffersDropped(int value = 1) {
		IncBuffers(BUFFERS_DROPPED, value);
	}

	INLINE void IncBuffersStored(int value = 1) {
		IncBuffers(BUFFERS_STORED, value);
	}
	INLINE void DecBuffersStored(int value = 1) {
		DecBuffers(BUFFERS_STORED, value);
	}

	INLINE void PostFilterMsg(avf_uint_t code) {
		mpFilter->PostFilterMsg(code, 0);
	}

protected:
	CFilter *mpFilter;
	avf_uint_t mPinIndex;
	ap<string_t> mName;

	ap<IBufferPool> mUsingBufferPool;	// the buffer pool in using after connection
	ap<CMediaFormat> mUsingMediaFormat;	// the media format in using after connection

public:
	virtual void SetMediaFormat(CMediaFormat *pMediaFormat);
	virtual void SetBufferPool(IBufferPool *pBufferPool);

	INLINE avf_u32_t GetBufferPoolFlags() {
		return mUsingBufferPool.get() ? mUsingBufferPool->GetFlags() : 0;
	}

	INLINE bool IsVideo() {
		CMediaFormat *format = mUsingMediaFormat.get();
		return format ? format->IsVideo() : false;
	}

	INLINE bool IsAudio() {
		CMediaFormat *format = mUsingMediaFormat.get();
		return format ? format->IsAudio() : false;
	}

	const char *GetMediaTypeName();

public:
	avf_pts_t ms_to_pts(avf_u32_t ms);

private:
	typedef struct buffer_s {
		avf_atomic_t num;
		avf_atomic_t num_last;
	} buffer_t;

private:
	bool mbEOS;
#ifdef MONITOR_BUFFERS
	buffer_t mBuffers[_NUM_BUFFER_TYPES];
#endif
};

INLINE void CFilter::SetEOS(CPin *pPin, CBuffer *pBuffer)
{
	pPin->SetEOS();
	pBuffer->SetEOS();
}

//-----------------------------------------------------------------------
//
//  CInputPin
//
//-----------------------------------------------------------------------
class CInputPin: public CPin
{
	typedef CPin inherited;

protected:
	CInputPin(CFilter *pFilter, avf_uint_t index, const char *pName): 
		inherited(pFilter, index, pName), mpPeer(NULL) {}
	virtual ~CInputPin();

// IPin
public:
	//virtual void ReceiveBuffer(CBuffer *pBuffer);
	//virtual void PurgeInput();

	virtual bool CanConnect() { return mpPeer == NULL; }
	virtual int Connected() { return mpPeer ? 1 : 0; }

	virtual int Connect(IPin *pPeer, IBufferPool *pBufferPool, CMediaFormat *pMediaFormat);
	virtual void Disconnect(IPin *pPeer);

	virtual CMediaFormat* QueryMediaFormat() {
		AVF_LOGE("Input pin does not provide media format");
		return NULL;
	}

	virtual IBufferPool *QueryBufferPool() {
		AVF_LOGE("QueryBufferPool() should not be called on input pin");
		return NULL;
  	}

  	//virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
  	//		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool) = 0;

	virtual CMsgQueue* GetBufferQueue() {
		AVF_LOGE("GetBufferQueue() called on input pin");
		return NULL;
	}

	virtual void ForceEOS() {
		//AVF_LOGD("%s[%d] ForceEOS %s", GetPinName(), GetPinIndex(), mpFilter->GetFilterName());
		// TODO: only force EOS when this is the only input pin
		mpFilter->ForceEOS();
	}

private:
	IPin *mpPeer;
};

//-----------------------------------------------------------------------
//
//  COutputPin
//
//-----------------------------------------------------------------------
class COutputPin: public CPin
{
	typedef CPin inherited;

protected:
	COutputPin(CFilter *pFilter, avf_uint_t index, const char *pName): 
			inherited(pFilter, index, pName) {
		mPeerPins._Init();
	}
	virtual ~COutputPin();

// IPin
public:
	virtual bool CanConnect() { return true; }
	virtual int Connected() { return mPeerPins.count; }

	virtual int Connect(IPin *pPeer, IBufferPool *pBufferPool, CMediaFormat *pMediaFormat);
	virtual void Disconnect(IPin *pPeer);

	virtual void ReceiveBuffer(CBuffer *pBuffer) {
		AVF_LOGE("ReceiveBuffer() called on output pin!");
		AVF_ASSERT(0);
	}

	virtual void PurgeInput() {
		AVF_LOGE("PurgeInput() called on on output pin!");
		AVF_ASSERT(0);
	}

	//virtual CMediaFormat* QueryMediaFormat() = 0;
	//virtual IBufferPool *QueryBufferPool() = 0;

	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
			IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool) {
		AVF_LOGE("Output pin does not accept any media format");
		return E_BADCALL;
	}

	virtual CMsgQueue* GetBufferQueue() {
		return mUsingBufferPool.get() ? mUsingBufferPool->GetBufferQueue() : NULL;
	}

	virtual void ForceEOS();

public:
	INLINE bool AllocBuffer(CBuffer*& pBuffer, avf_uint_t size = 0) {
		AVF_ASSERT(mUsingBufferPool.get());
		return mUsingBufferPool->AllocBuffer(pBuffer, GetUsingMediaFormat(), size);
	}

	void PostBuffer(CBuffer *pBuffer);

private:
	TDynamicArray<IPin*> mPeerPins;
};

//-----------------------------------------------------------------------
//
//  CQueueInputPin: input pin with a buffer queue
//
//-----------------------------------------------------------------------
class CQueueInputPin: public CInputPin
{
	typedef CInputPin inherited;
	friend class CActiveFilter;

protected:
	CQueueInputPin(CFilter *pFilter, avf_uint_t index, const char *pName, avf_size_t queueSize = 32);
	virtual ~CQueueInputPin();

// IPin
public:
	virtual void ReceiveBuffer(CBuffer *pBuffer) {
		IncBuffersReceived();
		IncBuffersStored();
		pBuffer->AddRef();
		avf_status_t status = mpBufferQueue->PostMsg(&pBuffer, sizeof(pBuffer));
		AVF_ASSERT_OK(status);
	}

	virtual void PurgeInput();

	//virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
	//		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool) = 0;

	virtual CMsgQueue* GetBufferQueue() {
		return mpBufferQueue;
	}

	virtual int Connect(IPin *pPeer, IBufferPool *pBufferPool, CMediaFormat *pMediaFormat);

public:
	INLINE bool PeekBuffer(CBuffer*& pBuffer) {
		bool rval = mpBufferQueue->PeekMsg(&pBuffer, sizeof(pBuffer));
		DecBuffersStored();
		return rval;
	}

	// pending buffers
	INLINE avf_size_t GetNumInputBuffers() {
		return mpBufferQueue ? mpBufferQueue->GetNumMsgs() : 0;
	}

protected:
	CMsgQueue *mpBufferQueue;
	bool mbCheckFull;
};

//-----------------------------------------------------------------------
//
//  CActiveFilter
//
//-----------------------------------------------------------------------
class CActiveFilter: public CFilter, public IActiveObject
{
protected:
	typedef CFilter inherited;
	typedef IActiveObject AO;
	DEFINE_INTERFACE;

public:
	enum {
		CMD_FORCE_EOS = AO::CMD_LAST,
		CMD_LAST,
	};

protected:
	CActiveFilter(IEngine *pEngine, const char *pName);
	virtual ~CActiveFilter();

// IFilter
public:
	virtual avf_status_t RunFilter() {
		return mpWorkQ->Run();
	}

	virtual avf_status_t StopFilter() {
		return mpWorkQ->Stop();
	}

	virtual avf_status_t ForceEOS() {
		mpWorkQ->PostCmd(CMD_FORCE_EOS);
		return E_OK;
	}

// IActiveObject
public:
	virtual const char *GetAOName() {
		return mName->string();
	}

	virtual void AOLoop();
	virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);

protected:
	INLINE void ReplyCmd(avf_status_t result) {
		mpWorkQ->ReplyCmd(result);
	}

	INLINE void SetEventFlag(avf_uint_t flag) {
		mpWorkQ->SetEventFlag(flag);
	}

	avf_status_t __AttachPin(IPin *pPin, bool bAttach, int type);

	INLINE avf_status_t AttachInputPin(IPin *pPin, bool bAttach) {
		return __AttachPin(pPin, bAttach, CMsgQueue::Q_INPUT);
	}
	INLINE avf_status_t AttachOutputPin(IPin *pPin, bool bAttach) {
		return __AttachPin(pPin, bAttach, CMsgQueue::Q_OUTPUT);
	}

	virtual void OnAttachAllPins(bool bAttach) {}
	avf_status_t AttachBufferPool(IBufferPool *pBufferPool, bool bAttach, int type);

	// attach/detach all input/output pins automatically
	// when entering/leaving FilterLoop()
	void __AttachAllPins(bool bAttach);
	INLINE void __AttachAllPins() { __AttachAllPins(true); }
	INLINE void __DetachAllPins() { __AttachAllPins(false); }

	INLINE void AttachAllPins() {
		if (!mbPinsAttached) {
			OnAttachAllPins(true);
			__AttachAllPins();
			mbPinsAttached = true;
		}
	}

	INLINE void DetachAllPins() {
		if (mbPinsAttached) {
			__DetachAllPins();
			OnAttachAllPins(false);
			mbPinsAttached = false;
		}
	}

	avf_status_t GetPinWithMsg(CQueueInputPin*& pPin);
	INLINE CBuffer *ReceiveBufferFrom(CQueueInputPin *pPin) {
		CBuffer *pBuffer;
		return pPin->PeekBuffer(pBuffer) ? pBuffer : NULL;
	}

	avf_status_t ReceiveInputBuffer(CQueueInputPin*& pPin, CBuffer*& pBuffer);
	bool PeekInputBuffer(CQueueInputPin*& pPin, CBuffer*& pBuffer, bool& bStopped);
	bool AllocAnyOutput(COutputPin*& pPin, CBuffer*& pBuffer, avf_size_t size = 0);
	bool AllocOutputBuffer(IBufferPool *pBufferPool, COutputPin *pOutput, CBuffer*& pBuffer, avf_size_t size = 0);
	INLINE bool AllocOutputBuffer(COutputPin *pPin, CBuffer*& pBuffer, avf_size_t size = 0) {
		return AllocOutputBuffer(pPin->GetUsingBufferPool(), pPin, pBuffer, size);
	}
	bool ProcessAnyCmds(avf_int_t timeout);
	bool ProcessOneCmd(avf_int_t timeout = -1);
	bool GenerateEOS(COutputPin *pPin, bool bToEngine = true);
	bool Sync(const char *my_pos, avf_s32_t my_index, const char *other_pos, avf_s32_t other_index);

	void SetRTPriority(avf_enum_t priority);

protected:
	virtual void FilterLoop() = 0;
	virtual bool ProcessFlag(avf_int_t flag);

	virtual IPin *GetInputMaster() { return NULL; }
	virtual IPin *GetOutputMaster() { return NULL; }

private:
	bool HandleCmdQInLoop();

protected:
	CWorkQueue *mpWorkQ;
	bool mbAttachInput;
	bool mbAttachOutput;
	bool mbPinsAttached;
	bool mbStopAckNeeded;
};

//-----------------------------------------------------------------------
//
//  CActiveRenderer
//
//-----------------------------------------------------------------------
class CActiveRenderer: public CActiveFilter, public IMediaRenderer
{
	typedef CActiveFilter inherited;
	DEFINE_INTERFACE;

protected:
	enum {
		CMD_INIT_RENDERER = inherited::CMD_LAST,
		CMD_PAUSE,
		CMD_RESUME,
		CMD_LAST,
	};

	struct init_renderer_t {
		avf_u32_t start_ms;
	};

protected:
	CActiveRenderer(IEngine *pEngine, const char *pName):
		inherited(pEngine, pName),
		mbStarting(false),
		mbPaused(false) {}

// IMediaRenderer
public:
	virtual avf_status_t InitRenderer(avf_u32_t start_ms) {
		init_renderer_t param = {start_ms};
		return mpWorkQ->SendCmd(CMD_INIT_RENDERER, &param);
	}
	virtual avf_status_t PauseRenderer() {
		return mpWorkQ->SendCmd(CMD_PAUSE);
	}
	virtual avf_status_t ResumeRenderer() {
		return mpWorkQ->SendCmd(CMD_RESUME);
	}

// IActiveObject
public:
	virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);

// for derived class
protected:
	virtual void OnStopRenderer() = 0;
	virtual bool OnPauseRenderer() = 0;

protected:
	avf_u32_t m_start_ms;
	bool mbStarting;	// the renderer is preparing to run
	bool mbPaused;		// the renderer is paused
};

//-----------------------------------------------------------------------
//
//  CMediaFileReader
//
//-----------------------------------------------------------------------
class CMediaFileReader: public CObject, public IMediaFileReader
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

protected:
	CMediaFileReader(IEngine *pEngine):
		mpEngine(pEngine),
		mbReportProgress(false),
		mPercent(0),
		m_total_bytes(0),
		m_remain_bytes(0)
	{
	}

public:
	virtual avf_status_t SetReportProgress(bool bReport);

protected:
	virtual void ReportProgress() {}
	void DoReportProgress(avf_u64_t total, avf_u64_t remain);
	void ReadProgress(avf_u64_t *total, avf_u64_t *remain);

protected:
	IEngine *mpEngine;
	bool mbReportProgress;
	avf_size_t mPercent;

private:
	CMutex mProgressMutex;
	avf_u64_t m_total_bytes;
	avf_u64_t m_remain_bytes;
};

//-----------------------------------------------------------------------
//
//  CMediaFileWriter
//
//-----------------------------------------------------------------------
class CMediaFileWriter: public CObject, public IMediaFileWriter
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

protected:
	CMediaFileWriter(IEngine *pEngine):
		mpEngine(pEngine), mbEnableRecord(false) {}

// IMediaFileWriter
public:
	virtual avf_status_t StartWriter(bool bEnableRecord) {
		mbEnableRecord = bEnableRecord;
		return E_OK;
	}
	virtual avf_status_t EndWriter() {
		mbEnableRecord = false;
		return E_OK;
	}

protected:
	void ModifyFileTime(const char *filename, avf_time_t time);

	INLINE void PostFilterMsg(avf_uint_t code, avf_intptr_t p1 = 0) {
		avf_msg_t msg;
		msg.code = code;
		msg.p0 = 0;
		msg.p1 = p1;
		mpEngine->PostFilterMsg(msg);
	}

	INLINE IRecordObserver *GetRecordObserver() {
		return (IRecordObserver*)mpEngine->FindComponent(IID_IRecordObserver);
	}

	void SetLargeFileFlag(int flag);

protected:
	IEngine *mpEngine;
	bool mbEnableRecord;
	bool mb_file_too_large;
};

//-----------------------------------------------------------------------
//
//  CMediaTrack
//
//-----------------------------------------------------------------------
class CMediaTrack: public CObject, public IMediaTrack
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

protected:
	CMediaTrack():
		mbTrackEnabled(true),
		m_start_dts(0),
		m_dts_offset(0),
		m_last_dts(0)
	{
	}

// IMediaTrack
public:
	virtual avf_status_t EnableTrack(bool bEnable) {
		mbTrackEnabled = bEnable;
		return E_OK;
	}
	virtual avf_pts_t GetDtsOffset() {
		return m_dts_offset + (m_last_dts - m_start_dts);
	}
	virtual void SetDtsOffset(avf_pts_t offset) {
		m_dts_offset = offset;
	}

protected:
	bool mbTrackEnabled;
	avf_pts_t m_start_dts;
	avf_pts_t m_dts_offset;
	avf_pts_t m_last_dts;
};

//-----------------------------------------------------------------------
//
//  CFilterGraph - manages filters and connections
//
//-----------------------------------------------------------------------
class CFilterGraph
{
public:
	CFilterGraph();
	~CFilterGraph();

public:
	// filter flags
	enum {
		FF_EOS = (1 << 0),		// end of stream
		FF_PREPARED = (1 << 1),	// this filter is prepared
		FF_STARTED = (1 << 2),	// this filter is started
	};

	struct Filter {
		IFilter *pFilter;
		avf_u32_t flags;
	};

	struct Connection {
		IPin *pOutputPin;
		IPin *pInputPin;
		bool mbReconnecting;
	};

public:
	void ClearGraph();
	avf_status_t PreRunAllFilters();
	avf_status_t RunAllFilters();
	void StopAllFilters();
	void PurgeAllFilters();
	void DeleteAllConnections();
	void DestroyAllFilters();
	avf_status_t RunFilter(IFilter *pFilter);

public:
	INLINE void __ClearFilterFlag(avf_size_t index, avf_u32_t flag) {
		Filter *pf = mFilters.Item(index);
		clear_flag(pf->flags, flag);
	}
	INLINE void __SetFilterFlag(avf_size_t index, avf_u32_t flag) {
		Filter *pf = mFilters.Item(index);
		set_flag(pf->flags, flag);
	}
	INLINE avf_u32_t __TestFilterFlag(avf_size_t index, avf_u32_t flag) {
		Filter *pf = mFilters.Item(index);
		return test_flag(pf->flags, flag);
	}

public:
	void ClearAllFiltersFlag(avf_u32_t flag);
	bool TestAllFiltersFlag(avf_u32_t flag);
	bool TestAllEOS();
	void SetFilterFlag(IFilter *pFilter, avf_u32_t flag);
	void ClearFilterFlag(IFilter *pFilter, avf_u32_t flag);
	bool FilterHasInput(avf_size_t index);

public:
	avf_status_t AddFilter(IFilter *pFilter);
	avf_status_t Connect(IFilter *pUpFilter, avf_uint_t indexUp,
		IFilter *pDownFilter, avf_uint_t indexDown);
	avf_status_t CreateConnection(IPin *pOutputPin, IPin *pInputPin);
	avf_status_t UpdateAllConnections();

	IFilter *__GetFilter(avf_size_t index) {
		Filter *pf = mFilters.Item(index);
		IFilter *pFilter = pf->pFilter;
		AVF_ASSERT(pFilter);
		return pFilter;
	}

public:
	INLINE avf_size_t GetNumFilters() {
		return mFilters.count;
	}
	IFilter *FindFilterByName(const char *pName);
	void *FindFilterByRefiid(avf_refiid_t refiid);

public:
	void PurgeInputPins(IFilter *pFilter);

private:
	TDynamicArray<Filter> mFilters;
	TDynamicArray<Connection> mConnections;
};

#endif

