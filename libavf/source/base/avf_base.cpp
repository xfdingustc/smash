
#define LOG_TAG "avf_base"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avio.h"

#ifdef AVF_DEBUG
avf_u32_t g_avf_log_flag = ALL_LOGS;
#else
avf_u32_t g_avf_log_flag = BASE_LOGS;
#endif

void avf_set_logs(const char *logs)
{
	int result = 0;
	while (true) {
		int c = *logs++;
		if (c == 0)
			break;
		switch (c) {
		case 'e': result |= AVF_LOG_E; break;
		case 'w': result |= AVF_LOG_W; break;
		case 'i': result |= AVF_LOG_I; break;
		case 'v': result |= AVF_LOG_V; break;
		case 'd': result |= AVF_LOG_D; break;
		case 'p': result |= AVF_LOG_P; break;
		}
	}
	g_avf_log_flag = result;
}

//-----------------------------------------------------------------------
//
//  CObject
//
//-----------------------------------------------------------------------
void *CObject::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IInterface)
		return (IInterface*)this;
	return NULL;
}

void CObject::Release() {
	if (avf_atomic_dec(&m_ref_count) == 1) {
		avf_delete this;
	}
}

//-----------------------------------------------------------------------
//
//  CRawDataCache
//
//-----------------------------------------------------------------------
CRawDataCache* CRawDataCache::Create()
{
	CRawDataCache *result = avf_new CRawDataCache();
	CHECK_STATUS(result);
	return result;
}

CRawDataCache::CRawDataCache():
	mpList(NULL),
	mCount(0),
	mDataSize(0),
	mUnmatchCount(0)
{
}

CRawDataCache::~CRawDataCache()
{
	raw_data_t *raw = mpList;
	while (raw) {
		raw_data_t *next = (raw_data_t*)raw->owner;
		avf_free(raw);
		raw = next;
	}
	AVF_LOGD("data_size: %d, unmatched: %d", mDataSize, mUnmatchCount);
}

raw_data_t *CRawDataCache::AllocRawData(avf_size_t size, avf_u32_t fcc)
{
	AUTO_LOCK(mMutex);

	if (mpList && size == mDataSize) {
		raw_data_t *result = mpList;
		raw_data_t *next = (raw_data_t*)mpList->owner;
		mpList = next;
		mCount--;

		avf_atomic_set(&result->mRefCount, 1);
		result->mSize = size;
		result->fcc = fcc;
		result->owner = (void*)this;
		result->DoRelease = ReleaseRawData;

		return result;
	}

	if (size == mDataSize || mDataSize == 0) {
		mDataSize = size;

		raw_data_t *result = (raw_data_t*)avf_malloc(sizeof(raw_data_t) + size);
		avf_atomic_set(&result->mRefCount, 1);
		result->mSize = size;
		result->fcc = fcc;
		result->owner = (void*)this;
		result->DoRelease = ReleaseRawData;

		return result;
	}

	mUnmatchCount++;
	return avf_alloc_raw_data(size, fcc);
}

void CRawDataCache::ReleaseRawData(raw_data_t *raw)
{
	CRawDataCache *self = (CRawDataCache*)raw->owner;
	AUTO_LOCK(self->mMutex);

	if (self->mCount < CACHE_MAX) {
		raw->owner = (void*)self->mpList;
		self->mpList = raw;
		self->mCount++;
	} else {
		avf_free(raw);
	}
}

//-----------------------------------------------------------------------
//
//  CBufferPool
//
//-----------------------------------------------------------------------
CBufferPool::CBufferPool(const char *pName, avf_uint_t maxBuffers):
	mMaxBuffers(maxBuffers),
	mName(pName)
{
	CREATE_OBJ(mpBufferQ = CMsgQueue::Create(pName, sizeof(CBuffer*), maxBuffers));
}

CBufferPool::~CBufferPool()
{
	__avf_safe_release(mpBufferQ);
}

void *CBufferPool::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IBufferPool)
		return static_cast<IBufferPool*>(this);
	return inherited::GetInterface(refiid);
}

void CBufferPool::__ReleaseBuffer(CBuffer *pBuffer)
{
	CBuffer *pNextBuffer = pBuffer->mpNext;

	// return the buffer to its pool
	avf_status_t status = mpBufferQ->PostMsg(&pBuffer, sizeof(CBuffer*));
	AVF_ASSERT_OK(status);

	if (pNextBuffer) {
		status = mpBufferQ->PostMsg(&pNextBuffer, sizeof(CBuffer*));
		AVF_ASSERT_OK(status);
	}
}

//-----------------------------------------------------------------------
//
//  CProxyBufferPool
//
//-----------------------------------------------------------------------
CProxyBufferPool* CProxyBufferPool::Create(const char *pName, avf_size_t maxBuffers, avf_size_t objectSize)
{
	CProxyBufferPool* result = avf_new CProxyBufferPool(pName, maxBuffers, objectSize);
	CHECK_STATUS(result);
	return result;
}

CProxyBufferPool::CProxyBufferPool(const char *pName, avf_size_t maxBuffers, avf_size_t objectSize):
	inherited(pName, maxBuffers),
	mpObjectMemory(NULL)
{
	AVF_ASSERT(maxBuffers > 0);
	AVF_ASSERT(objectSize >= sizeof(CBuffer));

	mObjectSize = ROUND_UP(objectSize, sizeof(void*));
	CREATE_OBJ(mpObjectMemory = avf_malloc_bytes(mObjectSize * maxBuffers));

	CBuffer *pBuffer = (CBuffer*)mpObjectMemory;
	for (avf_size_t i = maxBuffers; i; i--) {
		//pBuffer->mpNext = NULL;
		//pBuffer->mpFormat = NULL;
		pBuffer->mbNeedFree = 0;
		pBuffer->mpData = NULL;
		pBuffer->mSize = 0;
		pBuffer->Init(static_cast<IBufferPool*>(this), NULL);
		avf_status_t status = mpBufferQ->PostMsg(&pBuffer, sizeof(pBuffer));
		AVF_ASSERT_OK(status);
		pBuffer = (CBuffer*)((avf_u8_t*)pBuffer + mObjectSize);
	}

	// all messages must be returned to the pool
	mpBufferQ->SetMaxMsgNum(maxBuffers);
}

CProxyBufferPool::~CProxyBufferPool()
{
	avf_safe_free(mpObjectMemory);
}

bool CProxyBufferPool::AllocBuffer(CBuffer*& pBuffer, CMediaFormat *pFormat, avf_size_t size)
{
	if (!mpBufferQ->PeekMsg(&pBuffer, sizeof(pBuffer)))
		return false;

	pBuffer->mpData = NULL;
	pBuffer->mSize = 0;
	pBuffer->Init(static_cast<IBufferPool*>(this), pFormat);

	return true;
}

avf_status_t CProxyBufferPool::AllocMem(CBuffer *pBuffer, avf_size_t size)
{
	if (size <= pBuffer->mSize)
		return E_OK;
	AVF_LOGE("don't know how to alloc mem");
	return E_ERROR;
}

//-----------------------------------------------------------------------
//
//  CMemoryBufferPool
//
//-----------------------------------------------------------------------
CMemoryBufferPool* CMemoryBufferPool::Create(const char *pName, 
	avf_size_t maxBuffers, avf_size_t objectSize, avf_size_t bufferSize)
{
	CMemoryBufferPool* result = avf_new CMemoryBufferPool(pName, maxBuffers, objectSize, bufferSize);
	CHECK_STATUS(result);
	return result;
}

CMemoryBufferPool::CMemoryBufferPool(const char *pName, avf_size_t maxBuffers, 
		avf_size_t objectSize, avf_size_t bufferSize):
	inherited(pName, maxBuffers, objectSize),
	mpDataMemory(NULL)
{
	AVF_ASSERT(objectSize >= sizeof(CBuffer));
	AVF_ASSERT(bufferSize > 0);

	bufferSize = ROUND_UP(bufferSize, sizeof(void*));

	CREATE_OBJ(mpDataMemory = avf_malloc_bytes(bufferSize * maxBuffers));

	CBuffer *pBuffer = (CBuffer*)mpObjectMemory;
	avf_u8_t *pData = mpDataMemory;

	for (avf_size_t i = 0; i < maxBuffers; i++) {
		pBuffer->mpData = pData;
		pBuffer->mSize = bufferSize;
		pBuffer = (CBuffer*)((avf_u8_t*)pBuffer + mObjectSize);
		pData += bufferSize;
	}
}

CMemoryBufferPool::~CMemoryBufferPool()
{
	avf_free(mpDataMemory);
}

bool CMemoryBufferPool::AllocBuffer(CBuffer*& pBuffer, CMediaFormat *pFormat, avf_size_t size)
{
	if (!mpBufferQ->PeekMsg(&pBuffer, sizeof(pBuffer)))
		return false;

	pBuffer->Init(static_cast<IBufferPool*>(this), pFormat);
	return true;
}

//-----------------------------------------------------------------------
//
//  CDynamicBufferPool - dynamically allocate a buffer by avf_new()
//
//-----------------------------------------------------------------------
CDynamicBufferPool* CDynamicBufferPool::DoCreate(const char *pName, IAllocMem *pMalloc,
	avf_size_t maxBuffers, avf_size_t objectSize, bool bNonBlock)
{
	CDynamicBufferPool* result = avf_new CDynamicBufferPool(
		pName, pMalloc, maxBuffers, objectSize, bNonBlock);
	CHECK_STATUS(result);
	return result;
}

CDynamicBufferPool::CDynamicBufferPool(const char *pName, IAllocMem *pMalloc,
	avf_size_t maxBuffers, avf_size_t objectSize, bool bNonBlock):
	inherited(pName, maxBuffers, objectSize),
	mbNonBlock(bNonBlock),
	m_allocs(0),
	m_total_mem(0),
	mpMalloc(pMalloc)
{
	if (pMalloc) {
		pMalloc->AddRef();
	}
}

CDynamicBufferPool::~CDynamicBufferPool()
{
	if (mpBufferQ) {
		CBuffer *pBuffer;
		avf_int_t total = 0;
		while (mpBufferQ->PeekMsg(&pBuffer, sizeof(pBuffer))) {
			if (pBuffer->mpData) {
				FreeBufferData(pBuffer);
			}
			if (pBuffer->mbNeedFree) {
				avf_free(pBuffer);
			}
			total++;
		}

		mpBufferQ->SetMaxMsgNum(0);	// tell Q all msgs are consumed
	}

	avf_safe_release(mpMalloc);
}

// true - got buffer
// false - no buffer
// if no memory, pBuffer->mpData is NULL
bool CDynamicBufferPool::AllocBuffer(CBuffer*& pBuffer, CMediaFormat *pFormat, avf_size_t bufSize)
{
	if (!inherited::AllocBuffer(pBuffer, pFormat, bufSize)) {
		if (!mbNonBlock)
			return false;

		pBuffer = (CBuffer*)avf_malloc(mObjectSize);
		pBuffer->Init(static_cast<IBufferPool*>(this), pFormat);
		pBuffer->mbNeedFree = 1;
		m_allocs++;
	}

	// allocate data if neccessary
	pBuffer->mpData = NULL;
	if (bufSize > 0) {
		if ((pBuffer->mpData = AllocData(bufSize)) == NULL) {
			pBuffer->Release();
			return false;
		}
	}
	pBuffer->mSize = bufSize;
	avf_atomic_add(&m_total_mem, bufSize);

	return true;
}

avf_status_t CDynamicBufferPool::AllocMem(CBuffer *pBuffer, avf_size_t size)
{
	if (size <= pBuffer->mSize)
		return E_OK;

	avf_u8_t *mem = AllocData(size);
	if (mem == NULL) {
		AVF_LOGE("AllocMem failed");
		return E_NOMEM;
	}

	if (pBuffer->mpData) {
		avf_atomic_sub(&m_total_mem, pBuffer->mSize);
		FreeData(pBuffer->mpData, pBuffer->mSize);
	}

	pBuffer->mpData = mem;
	pBuffer->mSize = size;
	avf_atomic_add(&m_total_mem, size);

	return E_OK;
}

void CDynamicBufferPool::__ReleaseBuffer(CBuffer *pBuffer)
{
	CBuffer *pNext = pBuffer->mpNext;
	pBuffer->mpNext = NULL;

	ReleaseOneBuffer(pBuffer);
	if (pNext) {
		ReleaseOneBuffer(pNext);
	}
}

void CDynamicBufferPool::ReleaseOneBuffer(CBuffer *pBuffer)
{
	if (pBuffer->mpData) {
		FreeBufferData(pBuffer);
	}

	if (!pBuffer->mbNeedFree || m_allocs < 256) {
		pBuffer->mpNext = NULL;
		inherited::__ReleaseBuffer(pBuffer);
	} else {
		avf_free(pBuffer);
		m_allocs--;
	}
}

void CDynamicBufferPool::FreeBufferData(CBuffer *pBuffer)
{
	avf_atomic_sub(&m_total_mem, pBuffer->mSize);
	FreeData(pBuffer->mpData, pBuffer->mSize);
	pBuffer->mpData = NULL;
}

//-----------------------------------------------------------------------
//
//  CWorkQueue
//
//-----------------------------------------------------------------------

CWorkQueue* CWorkQueue::Create(AO *pAO)
{
	CWorkQueue *result = avf_new CWorkQueue(pAO);
	if (result) {
		result->RunThread();
	}
	CHECK_STATUS(result);
	return result;
}

void CWorkQueue::Release()
{
	avf_delete this;
}

CWorkQueue::CWorkQueue(AO *pAO):
	mStatus(E_OK),
	mpAO(NULL),
	mpCmdQ(NULL),
	mpMsgHub(NULL),
	mpThread(NULL)
{
	mpAO = pAO;

	CREATE_OBJ(mpCmdQ = CMsgQueue::Create(mpAO->GetAOName(), sizeof(AO::CMD), 4));

	CREATE_OBJ(mpMsgHub = CMsgHub::Create(mpAO->GetAOName()));

	avf_status_t status = mpCmdQ->AttachToHub(mpMsgHub, CMsgQueue::Q_CMD);
	AVF_ASSERT_OK(status);
}

void CWorkQueue::RunThread()
{
	CREATE_OBJ(mpThread = CThread::Create(mpAO->GetAOName(), ThreadEntry, this));
}

CWorkQueue::~CWorkQueue()
{
	if (mpThread) {
		Terminate();
		mpThread->Release();
	}

	if (mpMsgHub) {
		mpMsgHub->DetachAllMsgQueues();
		mpMsgHub->Release();
	}

	if (mpCmdQ) {
		mpCmdQ->Release();
	}
}

void CWorkQueue::ThreadEntry(void *p)
{
	((CWorkQueue*)p)->ThreadLoop();
}

void CWorkQueue::ThreadLoop()
{
	AO::CMD cmd;

	while (true) {

		GetCmd(cmd);

		switch (cmd.code) {
		case AO::CMD_TERMINATE:
			// exits the loop, and the thread is done!
			return;

		case AO::CMD_RUN:
			// enter the ActiveObject's loop
			// ReplyCmd() should be called inside
			mpAO->AOLoop();
			break;

		case AO::CMD_STOP:
			// this should be received inside AOLoop().
			// However, we got it here
			//AVF_LOGE("The workQ %s is not started yet", mpAO->GetAOName());
			ReplyCmd(E_STATE);
			break;

		default:
			mpAO->AOCmdProc(cmd, false);	// false: in thread loop
			break;
		}
	}
}

//-----------------------------------------------------------------------
//
//  CFilter
//
//-----------------------------------------------------------------------
void *CFilter::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IFilter)
		return static_cast<IFilter*>(this);
	return inherited::GetInterface(refiid);
}

avf_status_t CFilter::PreRunFilter()
{
	avf_size_t n = GetNumInput();
	for (avf_size_t i = 0; i < n; i++) {
		IPin *pInput = GetInputPin(i);
		if (pInput) {
			pInput->ResetState();
		}
	}

	n = GetNumOutput();
	for (avf_size_t i = 0; i < n; i++) {
		IPin *pOutput = GetOutputPin(i);
		if (pOutput) {
			pOutput->ResetState();
		}
	}

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CPin
//
//-----------------------------------------------------------------------
CPin::CPin(CFilter *pFilter, avf_uint_t index, const char *pName):
	mpFilter(pFilter),
	mPinIndex(index),
	mName(pName)
{
	ResetState();
}

CPin::~CPin()
{
	// should be released in Disconnect()
	AVF_ASSERT(GetUsingMediaFormat() == NULL);
	AVF_ASSERT(GetUsingMediaFormat() == NULL);
}

void *CPin::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IPin)
		return static_cast<IPin*>(this);
	return inherited::GetInterface(refiid);
}

void CPin::SetMediaFormat(CMediaFormat *pMediaFormat)
{
	mUsingMediaFormat = pMediaFormat;
}

void CPin::SetBufferPool(IBufferPool *pBufferPool)
{
	mUsingBufferPool = pBufferPool;
}

avf_int_t CPin::GetBuffers(avf_enum_t type)
{
#ifdef MONITOR_BUFFERS
	AVF_ASSERT(type < ARRAY_SIZE(mBuffers));
	return mBuffers[type].num;
#else
	return 0;
#endif
}

avf_int_t CPin::GetBuffersDiff(avf_enum_t type)
{
#ifdef MONITOR_BUFFERS
	AVF_ASSERT(type < ARRAY_SIZE(mBuffers));
	int num = mBuffers[type].num;
	int num_last = mBuffers[type].num_last;
	mBuffers[type].num_last = num;
	return num - num_last;
#else
	return 0;
#endif
}

avf_pts_t CPin::ms_to_pts(avf_u32_t ms)
{
	if (GetUsingMediaFormat() == NULL) {
		AVF_LOGE("mpUsingMediaFormat: no fmt");
		return 0;
	}
	return (avf_pts_t)ms * mUsingMediaFormat->mTimeScale / 1000;
}

// for debug
const char *CPin::GetMediaTypeName()
{
	CMediaFormat *pMF = mUsingMediaFormat.get();
	if (pMF == NULL)
		return "[no format]";
	switch (pMF->mMediaType) {
	case MEDIA_TYPE_NULL: return "null";
	case MEDIA_TYPE_VIDEO: return "video";
	case MEDIA_TYPE_AUDIO: return "audio";
	case MEDIA_TYPE_SUBTITLE: return "subtitle";
	case MEDIA_TYPE_AVES: return "aves";
	default: return "[unknown]";
	}
}

//-----------------------------------------------------------------------
//
//  CInputPin
//
//-----------------------------------------------------------------------
CInputPin::~CInputPin()
{
}

int CInputPin::Connect(IPin *pPeer, IBufferPool *pBufferPool, CMediaFormat *pMediaFormat)
{
	SetMediaFormat(pMediaFormat);
	SetBufferPool(pBufferPool);
	mpPeer = pPeer;
	return 0;
}

void CInputPin::Disconnect(IPin *pPeer)
{
	SetMediaFormat(NULL);
	SetBufferPool(NULL);
	mpPeer = NULL;
}

//-----------------------------------------------------------------------
//
//  COutputPin
//
//-----------------------------------------------------------------------
COutputPin::~COutputPin()
{
	mPeerPins._Release();
}

// return index of the connection
int COutputPin::Connect(IPin *pPeer, IBufferPool *pBufferPool, CMediaFormat *pMediaFormat)
{
	if (mPeerPins._Append(&pPeer, 4) == NULL) {
		// TODO
		AVF_LOGE("cannot append");
		return -1;
	}

	if (mPeerPins.count == 1) {
		SetMediaFormat(pMediaFormat);
		SetBufferPool(pBufferPool);
	}

	return mPeerPins.count - 1;
}

void COutputPin::Disconnect(IPin *pPeer)
{
	int index = -1;

	for (avf_size_t i = 0; i < mPeerPins.count; i++) {
		if (pPeer == *mPeerPins.Item(i)) {
			index = i;
			break;
		}
	}

	if (index < 0) {
		AVF_LOGE("cannot disconnect %s : %s", GetPinName(), pPeer->GetPinName());
		return;
	}

	mPeerPins._Remove(index);

	if (mPeerPins.count == 0) {
		SetMediaFormat(NULL);
		SetBufferPool(NULL);
	}
}

void COutputPin::ForceEOS()
{
	SetEOS();
	for (avf_size_t i = 0; i < mPeerPins.count; i++) {
		IPin *peer = *mPeerPins.Item(i);
		peer->ForceEOS();
	}
}

void COutputPin::PostBuffer(CBuffer *pBuffer)
{
	for (avf_size_t i = 0; i < mPeerPins.count; i++) {
		IPin *peer = *mPeerPins.Item(i);
		IncBuffersSent();
		peer->ReceiveBuffer(pBuffer);
	}
	pBuffer->Release();
}

//-----------------------------------------------------------------------
//
//  string_t
//
//-----------------------------------------------------------------------

string_t *string_t::CreateFrom(const char *string, avf_size_t len)
{
	string_t *self = (string_t*)avf_malloc(sizeof(string_t) + len + 1);
	self->mRefCount = 1;
	self->mSize = len;
	char *buffer = (char*)(self + 1);
	::memcpy(buffer, string, len);
	buffer[len] = 0;
	return self;
}

string_t *string_t::Add(
	const char *a, avf_size_t size_a,
	const char *b, avf_size_t size_b,
	const char *c, avf_size_t size_c,
	const char *d, avf_size_t size_d)
{
	avf_size_t size = size_a + size_b + size_c + size_d;

	string_t *self = (string_t*)avf_malloc(sizeof(string_t) + 1 + size);
	char *buffer;

	self->mRefCount = 1;
	self->mSize = size;

	buffer = (char*)(self + 1);

	::memcpy(buffer, a, size_a);
	buffer += size_a;

	::memcpy(buffer, b, size_b);
	buffer += size_b;

	if (c) {
		::memcpy(buffer, c, size_c);
		buffer += size_c;
	}

	if (d) {
		::memcpy(buffer, d, size_d);
		buffer += size_d;
	}

	*buffer = 0;

	return self;
}

void string_t::DoRelease()
{
	avf_free(this);
}

void ap<string_t>::clear()
{
	m_ptr->Release();
	m_ptr = string_t::CreateFrom("", 0);
}

ap<string_t>& ap<string_t>::operator=(const char *other)
{
	string_t *pNew = string_t::CreateFrom(other, ::strlen(other));
	m_ptr->Release();
	m_ptr = pNew;
	return *this;
}

ap<string_t>& ap<string_t>::operator=(string_t *other)
{
	other->AddRef();
	m_ptr->Release();
	m_ptr = other;
	return *this;
}

ap<string_t>& ap<string_t>::operator=(const ap<string_t>& other)
{
	other.m_ptr->AddRef();
	m_ptr->Release();
	m_ptr = other.m_ptr;
	return *this;
}

ap<string_t>& ap<string_t>::operator+=(const char *other)
{
	string_t *pNew = string_t::Add(
		m_ptr->string(), m_ptr->size(),
		other, ::strlen(other), NULL, 0, NULL, 0);
	m_ptr->Release();
	m_ptr = pNew;
	return *this;
}

void ap<string_t>::Append(string_t *other)
{
	string_t *pNew = string_t::Add(
		m_ptr->string(), m_ptr->size(),
		other->string(), other->size(),
		NULL, 0, NULL, 0);
	m_ptr->Release();
	m_ptr = pNew;
}

//-----------------------------------------------------------------------
//
//  raw data
//
//-----------------------------------------------------------------------

static void __avf_free_raw_data(raw_data_t *self)
{
	avf_free(self);
}

raw_data_t *avf_alloc_raw_data(avf_size_t size, avf_u32_t fcc)
{
	raw_data_t *self = (raw_data_t*)avf_malloc(sizeof(raw_data_t) + size);
	avf_atomic_set(&self->mRefCount, 1);
	self->mSize = size;
	self->fcc = fcc;
	self->owner = NULL;
	self->DoRelease = __avf_free_raw_data;
	return self;
}

//-----------------------------------------------------------------------
//
//  media format
//
//-----------------------------------------------------------------------

static void __avf_free_media_format(CMediaFormat *self)
{
	avf_free(self);
}

void *__avf_alloc_media_format(avf_size_t size)
{
	CMediaFormat *self = (CMediaFormat*)avf_malloc(size);
	avf_atomic_set(&self->mRefCount, 1);
	self->mFlags = 0;
	self->DoRelease = __avf_free_media_format;
	return reinterpret_cast<void*>(self);
}

//-----------------------------------------------------------------------
//
//  CQueueInputPin
//
//-----------------------------------------------------------------------
CQueueInputPin::CQueueInputPin(CFilter *pFilter, 
	avf_uint_t index, const char *pName, avf_size_t queueSize):
	inherited(pFilter, index, pName),
	mpBufferQueue(NULL),
	mbCheckFull(false)
{
	CREATE_OBJ(mpBufferQueue = CMsgQueue::Create(pName, sizeof(CBuffer*), queueSize));
}

CQueueInputPin::~CQueueInputPin()
{
	avf_safe_release(mpBufferQueue);
}

void CQueueInputPin::PurgeInput()
{
	// release all buffers in the queue
	CBuffer *pBuffer;
	avf_size_t num = 0;
	while (mpBufferQueue->PeekMsg(&pBuffer, sizeof(pBuffer))) {
		pBuffer->Release();
		DecBuffersStored();
		num++;
	}
	if (num > 0) {
		AVF_LOGD(C_GREEN "total %d buffers purged for %s[%d]" C_NONE,
			num, mpFilter->GetFilterName(), mPinIndex);
	}
}

int CQueueInputPin::Connect(IPin *pPeer, IBufferPool *pBufferPool, CMediaFormat *pMediaFormat)
{
	int index = inherited::Connect(pPeer, pBufferPool, pMediaFormat);
	if (mbCheckFull && pBufferPool) {
		avf_size_t maxBuffers = pBufferPool->GetMaxNumBuffers();
		mpBufferQueue->SetFullMsgNum(maxBuffers);
	}
	return index;
}

//-----------------------------------------------------------------------
//
//  CActiveFilter
//
//-----------------------------------------------------------------------
CActiveFilter::CActiveFilter(IEngine *pEngine, const char *pName):
	inherited(pEngine, pName),
	mpWorkQ(NULL),
	mbAttachInput(true),
	mbAttachOutput(true),
	mbPinsAttached(false),
	mbStopAckNeeded(false)
{
	CREATE_OBJ(mpWorkQ = CWorkQueue::Create(static_cast<IActiveObject*>(this)));
}

CActiveFilter::~CActiveFilter()
{
	avf_safe_release(mpWorkQ);
}

void *CActiveFilter::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IActiveObject)
		return static_cast<IActiveObject*>(this);
	return inherited::GetInterface(refiid);
}

void CActiveFilter::SetRTPriority(avf_enum_t priority)
{
	if (mpEngine->IsRealTime()) {
		mpWorkQ->SetRTPriority(priority);
	}
}

avf_status_t CActiveFilter::AttachBufferPool(IBufferPool *pBufferPool, bool bAttach, int type)
{
	if (pBufferPool == NULL)
		return E_OK;

	CMsgQueue *pBufferQueue = pBufferPool->GetBufferQueue();
	if (pBufferQueue == NULL)
		return E_OK;

	avf_status_t status;

	if (bAttach) {
		status = mpWorkQ->AttachMsgQueue(pBufferQueue, type);
	} else {
		status = mpWorkQ->DetachMsgQueue(pBufferQueue);
	}

	if (status != E_OK)
		return status;

	pBufferQueue->SetOwner(NULL);
	return E_OK;
}

avf_status_t CActiveFilter::__AttachPin(IPin *pPin, bool bAttach, int type)
{
	if (pPin == NULL)
		return E_OK;

	CMsgQueue *pBufferQueue = pPin->GetBufferQueue();
	if (pBufferQueue == NULL)
		return E_OK;

	avf_status_t status;

	if (bAttach) {
		status = mpWorkQ->AttachMsgQueue(pBufferQueue, type);
	} else {
		status = mpWorkQ->DetachMsgQueue(pBufferQueue);
	}

	if (status != E_OK)
		return status;

	// note: the IPin must be casted from CQueueInputPin
	pBufferQueue->SetOwner(pPin);
	return E_OK;
}

void CActiveFilter::__AttachAllPins(bool bAttach)
{
	avf_size_t numPins;
	IPin *pMasterPin;
	avf_size_t i;

	if (mbAttachInput) {
		// master input
		pMasterPin = GetInputMaster();
		if (pMasterPin) {
			AttachInputPin(pMasterPin, bAttach);
		}

		// input pins
		numPins = GetNumInput();
		for (i = 0; i < numPins; i++) {
			IPin *pInput = GetInputPin(i);
			if (pInput && pInput != pMasterPin) {
				AttachInputPin(pInput, bAttach);
			}
		}
	}

	if (mbAttachOutput) {
		// master output
		pMasterPin = GetOutputMaster();
		if (pMasterPin) {
			AttachOutputPin(pMasterPin, bAttach);
		}

		// output pins
		numPins = GetNumOutput();
		for (i = 0; i < numPins; i++) {
			IPin *pOutput = GetOutputPin(i);
			if (pOutput && pOutput != pMasterPin) {
				AttachOutputPin(pOutput, bAttach);
			}
		}
	}
}

bool CActiveFilter::ProcessFlag(avf_int_t flag)
{
	AVF_LOGW("%s: flag %d not handled", mName->string(), flag);
	return true;
}

// try to get and process one cmd
// return false to indicate loop end
bool CActiveFilter::HandleCmdQInLoop()
{
	AO::CMD cmd;

	if (mpWorkQ->PeekCmd(cmd)) {
		if (!AOCmdProc(cmd, true))
			return false;
	}

	return true;
}

// false : stopped
// true : timedout, or a cmd is processed
bool CActiveFilter::ProcessAnyCmds(avf_int_t timeout)
{
	while (true) {
		CMsgHub::MsgResult result;

		if (!mpWorkQ->WaitMsg(result, CMsgQueue::Q_CMD, timeout)) {
			// timeout
			return true;
		}

		if (result.flag >= 0) {
			ProcessFlag(result.flag);
			continue;
		}

		if (result.pMsgQ) {
			if (mpWorkQ->IsCmdQ(result.pMsgQ)) {
				if (!HandleCmdQInLoop())
					return false;
				return true;
			}
		}
	}
}

// true : time out, or nothing done
// false : stopped
bool CActiveFilter::ProcessOneCmd(avf_int_t timeout)
{
	CMsgHub::MsgResult result;

	if (!mpWorkQ->WaitMsg(result, CMsgQueue::Q_CMD, timeout)) {
		// timeout
		return true;
	}

	if (result.flag >= 0) {
		ProcessFlag(result.flag);
		return true;
	}

	if (result.pMsgQ) {
		if (mpWorkQ->IsCmdQ(result.pMsgQ)) {
			if (!HandleCmdQInLoop())
				return false;
		}
	}

	return true;
}

bool CActiveFilter::Sync(const char *my_pos, avf_s32_t my_index, 
	const char *other_pos, avf_s32_t other_index)
{
	avf_s32_t my_time = mpEngine->ReadRegInt32(my_pos, my_index, -1);
	if (my_time >= 0) {
		avf_s32_t other_time = mpEngine->ReadRegInt32(other_pos, other_index, -1);
		if (other_time >= 0 && my_time > other_time) {
			return ProcessAnyCmds(my_time - other_time);
		}
	}
	return true;
}

avf_status_t CActiveFilter::GetPinWithMsg(CQueueInputPin*& pPin)
{
	while (true) {
		CMsgHub::MsgResult result;
		mpWorkQ->WaitMsgQueues(result, pPin->GetBufferQueue(), true);

		if (result.flag >= 0) {
			if (!ProcessFlag(result.flag))
				return E_INTER;
			continue;
		}

		if (result.pMsgQ) {
			if (mpWorkQ->IsCmdQ(result.pMsgQ)) {
				if (!HandleCmdQInLoop())
					return E_ERROR;
			} else {
				// pPin is the given pin, or a pin that's full with buffers
				pPin = (CQueueInputPin*)(IPin*)result.pMsgQ->GetOwner();
				return E_OK;
			}
		}
	}
}

// This should be called from FilterLoop().
// It tries to receive an input buffer from all attached CQueueInputPin's.
// return E_OK - got buffer
// return E_ERROR - FilterLoop() is stopped
// return E_INTER - interrupted by event flag
avf_status_t CActiveFilter::ReceiveInputBuffer(CQueueInputPin*& pPin, CBuffer*& pBuffer)
{
	while (true) {

		CMsgHub::MsgResult result;
		mpWorkQ->WaitMsg(result, CMsgQueue::Q_CMD | CMsgQueue::Q_INPUT);

		if (result.flag >= 0) {
			if (!ProcessFlag(result.flag))
				return E_INTER;
			continue;
		}

		if (result.pMsgQ) {
			if (mpWorkQ->IsCmdQ(result.pMsgQ)) {
				if (!HandleCmdQInLoop())
					return E_ERROR;
			} else {
				pPin = (CQueueInputPin*)(IPin*)result.pMsgQ->GetOwner();
				if (pPin->PeekBuffer(pBuffer))
					return E_OK;
				AVF_LOGW("%s: no msg", mName->string());
			}
		}
	}
}

bool CActiveFilter::PeekInputBuffer(CQueueInputPin*& pPin, CBuffer*& pBuffer, bool& bStopped)
{
	while (true) {

		CMsgHub::MsgResult result;

		if (!mpWorkQ->WaitMsg(result, CMsgQueue::Q_CMD | CMsgQueue::Q_INPUT, 0)) {
			bStopped = false;
			return false;
		}

		if (result.flag >= 0) {
			ProcessFlag(result.flag);
			continue;
		}

		if (result.pMsgQ) {
			if (mpWorkQ->IsCmdQ(result.pMsgQ)) {
				if (!HandleCmdQInLoop()) {
					bStopped = true;
					return false;
				}
			} else {
				pPin = (CQueueInputPin*)(IPin*)result.pMsgQ->GetOwner();
				if (pPin->PeekBuffer(pBuffer))
					return true;

				bStopped = false;
				return false;
			}
		}
	}
}

bool CActiveFilter::AllocAnyOutput(COutputPin*& pPin, CBuffer*& pBuffer, avf_size_t size)
{
	while (true) {

		CMsgHub::MsgResult result;

		if (!mpWorkQ->WaitMsg(result, CMsgQueue::Q_CMD | CMsgQueue::Q_OUTPUT))
			continue;

		if (result.flag >= 0) {
			ProcessFlag(result.flag);
			continue;
		}

		if (result.pMsgQ) {
			if (mpWorkQ->IsCmdQ(result.pMsgQ)) {
				if (!HandleCmdQInLoop())
					return false;
			} else {
				pPin = (COutputPin*)(IPin*)result.pMsgQ->GetOwner();
				if (pPin->AllocBuffer(pBuffer, size))
					return true;

				AVF_LOGW("%s: AllocBuffer failed 1", mName->string());
				avf_msleep(100);
			}
		}
	}
}

bool CActiveFilter::AllocOutputBuffer(IBufferPool *pBufferPool, COutputPin *pOutput, CBuffer*& pBuffer, avf_size_t size)
{
	if (pBufferPool == NULL)
		return false;

	CMediaFormat *pFormat = pOutput ? pOutput->GetUsingMediaFormat() : NULL;

	if (pBufferPool->GetFlags() & IBufferPool::F_HAS_BUFFER) {

		while (true) {

			CMsgHub::MsgResult result;
			if (mpWorkQ->WaitMsg(result, CMsgQueue::Q_CMD, 0)) {
				if (result.flag >= 0) {
					ProcessFlag(result.flag);
					continue;
				}

				if (!HandleCmdQInLoop())
					return false;
			}

			if (pBufferPool->AllocBuffer(pBuffer, pFormat, size))
				return true;

			AVF_LOGW("%s: AllocBuffer failed 2", mName->string());
			avf_msleep(100);
		}

	} else {

		CMsgQueue *mpBufferQueue = pBufferPool->GetBufferQueue();
		AVF_ASSERT(mpBufferQueue != NULL);

		while (true) {

			CMsgHub::MsgResult result;
			mpWorkQ->WaitMsgQueues(result, mpBufferQueue, false);

			// likely it is a buffer msg
			if (result.pMsgQ == mpBufferQueue) {
				if (pBufferPool->AllocBuffer(pBuffer, pFormat, size))
					return true;

				AVF_LOGW("%s: AllocBuffer failed 3", mName->string());
				avf_msleep(100);
				continue;
			}

			// unlikely a flag
			if (result.flag >= 0) {
				ProcessFlag(result.flag);
				continue;
			}

			// try cmd Q
			if (!HandleCmdQInLoop())
				return false;
		}
	}
}

// both to downstream and engine
bool CActiveFilter::GenerateEOS(COutputPin *pPin, bool bToEngine)
{
	CBuffer *pBuffer;

	if (!AllocOutputBuffer(pPin, pBuffer))
		return false;

	SetEOS(pPin, pBuffer);
	pPin->PostBuffer(pBuffer);

	if (bToEngine) {
		PostEosMsg();
	}

	return true;
}

void CActiveFilter::AOLoop()
{
	mbStopAckNeeded = false;

	AttachAllPins();
	FilterLoop();
	DetachAllPins();

	//AVF_LOGD("[%d] %s exits loop", GetFilterIndex(), GetFilterName());

	if (mbStopAckNeeded) {
		mbStopAckNeeded = false;
		ReplyCmd(E_OK);
	}
}

bool CActiveFilter::AOCmdProc(const CMD& cmd, bool bInAOLoop)
{
	switch (cmd.code) {
	case AO::CMD_STOP:
		mbStopAckNeeded = true;
		return false;

	case CMD_FORCE_EOS:
		if (bInAOLoop) {
			avf_size_t numPins = GetNumOutput();
			for (avf_size_t i = 0; i < numPins; i++) {
				IPin *pOutput = GetOutputPin(i);
				if (pOutput) {
					pOutput->ForceEOS();
				}
			}
			PostEosMsg();
		}
		return false;

	default:
		// should be overwritten by derived class
		AVF_LOGW("command %d not handled by %s", cmd.code, mName->string());
		ReplyCmd(E_OK);
		return true;
	}
}

//-----------------------------------------------------------------------
//
//  CActiveRenderer
//
//-----------------------------------------------------------------------
void *CActiveRenderer::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMediaRenderer)
		return static_cast<IMediaRenderer*>(this);
	return inherited::GetInterface(refiid);
}

bool CActiveRenderer::AOCmdProc(const CMD& cmd, bool bInAOLoop)
{
	switch (cmd.code) {
	case inherited::CMD_STOP:
		if (!bInAOLoop) {
			OnStopRenderer();
		}
		break;

	case CMD_INIT_RENDERER: {
			init_renderer_t *param = (init_renderer_t*)cmd.pExtra;
			m_start_ms = param->start_ms;
		}
		ReplyCmd(E_OK);
		return true;

	case CMD_PAUSE:
		if (mbStarting || mbPaused) {
			// 1. video not started yet;
			// 2. started, but already paused
			ReplyCmd(E_OK);
		} else {
			ReplyCmd(E_OK);
			mbPaused = true;
			if (bInAOLoop && !OnPauseRenderer()) {
				// in pause, and player wants to stop
				return false;
			}
		}
		return true;

	case CMD_RESUME:
		mbPaused = false;
		ReplyCmd(E_OK);
		return true;

	default:
		break;
	}

	return inherited::AOCmdProc(cmd, bInAOLoop);
}

//-----------------------------------------------------------------------
//
//  CMediaFileReader
//
//-----------------------------------------------------------------------
void *CMediaFileReader::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMediaFileReader)
		return static_cast<IMediaFileReader*>(this);
	return inherited::GetInterface(refiid);
}

avf_status_t CMediaFileReader::SetReportProgress(bool bReport)
{
	mbReportProgress = bReport;
	if (bReport) {
		ReportProgress();
	}
	return E_OK;
}

void CMediaFileReader::DoReportProgress(avf_u64_t total, avf_u64_t remain)
{
	{
		AUTO_LOCK(mProgressMutex);
		m_total_bytes = total;
		m_remain_bytes = remain;
	}

	avf_u64_t addr = total - remain;
	avf_size_t percent = (avf_size_t)(addr * 100 / total);
	if (percent != mPercent) {
		mPercent = percent;
		avf_msg_t msg;
		msg.code = IEngine::MSG_MEDIA_SOURCE_PROGRESS;
		msg.p0 = 0;
		msg.p1 = percent;
		mpEngine->PostFilterMsg(msg);
	}
}

void CMediaFileReader::ReadProgress(avf_u64_t *total, avf_u64_t *remain)
{
	AUTO_LOCK(mProgressMutex);
	*total = m_total_bytes;
	*remain = m_remain_bytes;
}

//-----------------------------------------------------------------------
//
//  CMediaFileWriter
//
//-----------------------------------------------------------------------
void *CMediaFileWriter::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMediaFileWriter)
		return static_cast<IMediaFileWriter*>(this);
	return inherited::GetInterface(refiid);
}

void CMediaFileWriter::ModifyFileTime(const char *filename, avf_time_t time)
{
	avf_change_file_time(filename, time);
}

void CMediaFileWriter::SetLargeFileFlag(int flag)
{
	mpEngine->WriteRegBool(NAME_MUXER_LARGE_FILE, 0, flag);
}

//-----------------------------------------------------------------------
//
//  CMediaTrack
//
//-----------------------------------------------------------------------
void *CMediaTrack::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMediaTrack)
		return static_cast<IMediaTrack*>(this);
	return NULL;
}

//-----------------------------------------------------------------------
//
//  CFilterGraph
//
//-----------------------------------------------------------------------
CFilterGraph::CFilterGraph()
{
	mFilters._Init();
	mConnections._Init();
}

CFilterGraph::~CFilterGraph()
{
	ClearGraph();
	mConnections._Release();
	mFilters._Release();
}

void CFilterGraph::ClearGraph()
{
	if (mFilters.count > 0) {
		StopAllFilters();
		PurgeAllFilters();
		DeleteAllConnections();
		DestroyAllFilters();
	}
}

avf_status_t CFilterGraph::RunFilter(IFilter *pFilter)
{
	avf_status_t status = pFilter->RunFilter();
	if (status != E_OK) {
		AVF_LOGE("Cannot run %s, code = %d", pFilter->GetFilterName(), status);
		return status;
	}
	AVF_LOGD("[%d] %s is running", pFilter->GetFilterIndex(), pFilter->GetFilterName());
	return E_OK;
}

avf_status_t CFilterGraph::PreRunAllFilters()
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		Filter *pf = mFilters.Item(i);
		IFilter *pFilter = pf->pFilter;
		avf_status_t status = pFilter->PreRunFilter();
		if (status != E_OK)
			return status;
	}

	return E_OK;
}

avf_status_t CFilterGraph::RunAllFilters()
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		Filter *pf = mFilters.Item(i);
		IFilter *pFilter = pf->pFilter;
		avf_status_t status = RunFilter(pFilter);
		if (status != E_OK)
			return status;
	}

	return E_OK;
}

void CFilterGraph::StopAllFilters()
{
	for (avf_size_t i = mFilters.count; i > 0; i--) {
		Filter *pf = mFilters.Item(i - 1);
		IFilter *pFilter = pf->pFilter;
		pFilter->StopFilter();
		AVF_LOGD("[%d] %s stopped", pFilter->GetFilterIndex(), pFilter->GetFilterName());
	}
}

void CFilterGraph::PurgeAllFilters()
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		Filter *pf = mFilters.Item(i);
		pf->pFilter->PostStopFilter();
		PurgeInputPins(pf->pFilter);
	}
}

void CFilterGraph::DeleteAllConnections()
{
	for (avf_size_t i = mConnections.count; i > 0; i--) {
		Connection *c = mConnections.Item(i - 1);
		AVF_LOGD("disconnect [%s] -> [%s]", 
			c->pOutputPin->GetPinName(), c->pInputPin->GetPinName());
		c->pOutputPin->Disconnect(c->pInputPin);
		c->pInputPin->Disconnect(c->pOutputPin);
	}
	mConnections._Clear();
}

void CFilterGraph::DestroyAllFilters()
{
	for (avf_size_t i = mFilters.count; i > 0; i--) {
		Filter *pf = mFilters.Item(i - 1);
		IFilter *pFilter = pf->pFilter;
		AVF_LOGI("[%d] destroy %s", pFilter->GetFilterIndex(), pFilter->GetFilterName());
		pFilter->Release();
	}
	mFilters._Clear();
}

void CFilterGraph::ClearAllFiltersFlag(avf_u32_t flag)
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		__ClearFilterFlag(i, flag);
	}
}

bool CFilterGraph::TestAllFiltersFlag(avf_u32_t flag)
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		if (__TestFilterFlag(i, flag) == 0) {
			return false;
		}
	}
	return true;
}

bool CFilterGraph::TestAllEOS()
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		if (__TestFilterFlag(i, FF_EOS) == 0) {
			if (FilterHasInput(i)) {
				return false;
			}
		}
	}
	return true;
}

bool CFilterGraph::FilterHasInput(avf_size_t index)
{
	Filter *pf = mFilters.Item(index);
	IFilter *pFilter = pf->pFilter;
 	if (pFilter == NULL)
 		return false;
 	avf_size_t ninput = pFilter->GetNumInput();
 	for (avf_size_t i = 0; i < ninput; i++) {
 		IPin *pPin = pFilter->GetInputPin(i);
 		if (pPin && pPin->Connected() > 0)
 			return true;
 	}
 	return false;
}

void CFilterGraph::SetFilterFlag(IFilter *pFilter, avf_u32_t flag)
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		Filter *pf = mFilters.Item(i);
		if (pf->pFilter == pFilter) {
			__SetFilterFlag(i, flag);
			return;
		}
	}
	AVF_LOGE("SetFilterFlag: filter %s not found", pFilter->GetFilterName());
}

void CFilterGraph::ClearFilterFlag(IFilter *pFilter, avf_u32_t flag)
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		Filter *pf = mFilters.Item(i);
		if (pf->pFilter == pFilter) {
			__ClearFilterFlag(i, flag);
			return;
		}
	}
	AVF_LOGE("ClearFilterFlag: filter %s not found", pFilter->GetFilterName());
}

avf_status_t CFilterGraph::AddFilter(IFilter *pFilter)
{
	Filter *pf = mFilters._Append(NULL, 16);
	if (pf == NULL)
		return E_NOMEM;

	int index = mFilters.count - 1;

	pf->pFilter = pFilter;
	pf->flags = 0;

	pFilter->SetFilterIndex(index);

	AVF_LOGI(C_CYAN "[%d]" C_NONE " filter " C_CYAN "%s" C_NONE " added",
		index, pFilter->GetFilterName());

	return E_OK;
}

avf_status_t CFilterGraph::Connect(IFilter *pUpFilter, avf_uint_t indexUp,
		IFilter *pDownFilter, avf_uint_t indexDown)
{
	IPin *pOutput = pUpFilter->GetOutputPin(indexUp);
	if (pOutput == NULL) {
		AVF_LOGE("No such output pin: %s[%d]", pUpFilter->GetFilterName(), indexUp);
		return E_INVAL;
	}

	IPin *pInput = pDownFilter->GetInputPin(indexDown);
	if (pInput == NULL) {
		AVF_LOGE("No such input pin: %s[%d]", pDownFilter->GetFilterName(), indexDown);
		return E_INVAL;
	}

	int index = mConnections.count;
	Connection *c = mConnections._Append(NULL, 16);
	if (c == NULL)
		return E_NOMEM;

	avf_status_t status = CreateConnection(pOutput, pInput);
	if (status != E_OK) {
		mConnections._Remove(index);
		return status;
	}

	c->pOutputPin = pOutput;
	c->pInputPin = pInput;
	c->mbReconnecting = false;

	AVF_LOGD(C_GREEN "connected: %s[%d] -> %s[%d]" C_NONE, 
		pUpFilter->GetFilterName(), indexUp,
		pDownFilter->GetFilterName(), indexDown);

	return E_OK;
}

avf_status_t CFilterGraph::CreateConnection(IPin *pOutput, IPin *pInput)
{
	if (!pOutput->CanConnect()) {
		AVF_LOGE("The pin %s[%d] is already connected", pOutput->GetPinName(), pOutput->GetPinIndex());
		return E_ERROR;
	}

	if (!pInput->CanConnect()) {
		AVF_LOGE("The pin %s[%d] is already connected", pInput->GetPinName(), pInput->GetPinIndex());
		return E_ERROR;
	}

	// step 1. the output pin must give the media format, or already has a media format
	CMediaFormat *pMediaFormat = pOutput->GetUsingMediaFormat();
	if (pMediaFormat) {
		pMediaFormat->AddRef();
	} else {
		pMediaFormat = pOutput->QueryMediaFormat();
		if (pMediaFormat == NULL) {
			AVF_LOGE("No media provied by pin %s[%d]", pOutput->GetPinName(), pOutput->GetPinIndex());
			return E_CONNECT;
		}
	}

	// step 2. the input pin must accept the media format
	// and optionally return a buffer pool for connection
	IBufferPool *pUpStreamBufferPool = pOutput->GetUsingBufferPool();
	IBufferPool *pBufferPool = NULL;
	avf_status_t status = pInput->AcceptMedia(pMediaFormat, pUpStreamBufferPool, &pBufferPool);
	if (status != E_OK) {
		AVF_LOGE("Media format not accepted by pin %s[%d]", pInput->GetPinName(), pInput->GetPinIndex());
		pMediaFormat->Release();
		return E_CONNECT;
	}

	// step 3. buffer pool
	if (pUpStreamBufferPool) {
		// if up stream already has a buffer pool,
		// and down stream provided one, fail
		if (pBufferPool) {
			AVF_LOGE("buffer pool conflict");
			if (pBufferPool) {
				pBufferPool->Release();
			}
			pMediaFormat->Release();
			return E_CONNECT;
		}
		// use up stream buffer pool
		pBufferPool = pUpStreamBufferPool;
		pBufferPool->AddRef();
	}

	// if there's no buffer pool, ask up stream to create
	if (pBufferPool == NULL) {
		pBufferPool = pOutput->QueryBufferPool();
		if (pBufferPool == NULL) {
			AVF_LOGW("no buffer pool: %s[%d] -> %s[%d]", 
				pOutput->GetPinName(), pOutput->GetPinIndex(),
				pInput->GetPinName(), pInput->GetPinIndex());
			pMediaFormat->Release();
			return E_CONNECT;
		}
	}

	// step 4. connect down stream
	pInput->Connect(pOutput, pBufferPool, pMediaFormat);

	// step 5. connect up stream
	pOutput->Connect(pInput, pBufferPool, pMediaFormat);

	// the input/output pins should AddRef() the pBufferPool and pMediaFormat
	// so we should release them here
	pMediaFormat->Release();
	avf_safe_release(pBufferPool);

	return E_OK;
}

avf_status_t CFilterGraph::UpdateAllConnections()
{
	avf_size_t num_reconnected = 0;

	// first disconnect all that needed reconnecting
	for (avf_size_t i = 0; i < mConnections.count; i++) {
		Connection *c = mConnections.Item(i);
		IPin *pOutput = c->pOutputPin;
		if (pOutput->NeedReconnect()) {
			IPin *pInput = c->pInputPin;

			pOutput->Disconnect(pInput);
			pInput->Disconnect(pOutput);

			c->mbReconnecting = true;

			num_reconnected++;
		} else {
			c->mbReconnecting = false;
		}
	}

	// then reconnect all
	if (num_reconnected > 0) {
		avf_status_t status = E_OK;
		for (avf_size_t i = 0; i < mConnections.count; i++) {
			Connection *c = mConnections.Item(i);
			if (c->mbReconnecting) {
				c->mbReconnecting = false;
				if (status == E_OK) {
					IPin *pOutput = c->pOutputPin;
					IPin *pInput = c->pInputPin;
					status = CreateConnection(pOutput, pInput);

					if (status == E_OK) {
						AVF_LOGD(C_GREEN "%s[%d] -> %s[%d] reconnected" C_NONE,
							pOutput->GetPinName(), pOutput->GetPinIndex(),
							pInput->GetPinName(), pInput->GetPinIndex());
					} else {
						AVF_LOGE("%s[%d] -> %s[%d] reconnect failed",
							pOutput->GetPinName(), pOutput->GetPinIndex(),
							pInput->GetPinName(), pInput->GetPinIndex());
					}
				}
			}
		}
		if (status != E_OK)
			return status;
	}

	return E_OK;
}

IFilter *CFilterGraph::FindFilterByName(const char *pName)
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		Filter *pf = mFilters.Item(i);
		IFilter *pFilter = pf->pFilter;
		if (::strcmp(pName, pFilter->GetFilterName()) == 0)
			return pFilter;
	}
	//AVF_LOGI("Filter '%s' not found", pName);
	return NULL;
}

void* CFilterGraph::FindFilterByRefiid(avf_refiid_t refiid)
{
	for (avf_size_t i = 0; i < mFilters.count; i++) {
		Filter *pf = mFilters.Item(i);
		IFilter *pFilter = pf->pFilter;
		void *pInterface = pFilter->GetInterface(refiid);
		if (pInterface)
			return pInterface;
	}
	return NULL;
}

void CFilterGraph::PurgeInputPins(IFilter *pFilter)
{
	avf_size_t numInput = pFilter->GetNumInput();
	for (avf_size_t i = 0; i < numInput; i++) {
		IPin *pPin = pFilter->GetInputPin(i);
		if (pPin) {
			// release all buffers in the pin
			pPin->PurgeInput();
		}
	}
}

