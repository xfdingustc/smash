
#define LOG_TAG "http_mjpeg"

// implementation note
//	* server is created with the filter
//	* server is started/stopped when the filter is started/stopped

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_socket.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_config.h"
#include "avf_tcp_server.h"

#include "avf_util.h"
#include "avf_media_format.h"
#include "http_mjpeg.h"

#define BOUNDARY_TAG \
	"avfmjpegserver"

//-----------------------------------------------------------------------
//
//  CDataQueue
//
//-----------------------------------------------------------------------
CBufferQueue::CBufferQueue()
{
	mbDisabled = false;
	m_frame_length = 0;
	m_last_tick = avf_get_curr_tick();
	m_total_buffers = 0;
	m_dropped_buffers = 0;
	m_read_index = 0;
	m_write_index = 0;
	m_num_buffers = 0;
}

CBufferQueue::~CBufferQueue()
{
	AVF_LOGD("total: %d, dropped: %d", m_total_buffers, m_dropped_buffers);
}

void CBufferQueue::SetFPS(int fps)
{
	AUTO_LOCK(mMutex);
	if (fps <= 0) {
		m_frame_length = 0;
	} else {
		m_frame_length = 1000 / fps;
		if (m_frame_length <= 0)
			m_frame_length = 1;
	}
}

void CBufferQueue::PushBuffer(const BufferItem *pBufferItem, avf_u64_t tick)
{
	AUTO_LOCK(mMutex);

	if (mbDisabled)
		return;

	if (m_frame_length > 0) {
		if (m_last_tick + m_frame_length > tick)
			return;
		m_last_tick = tick;
	}

	pBufferItem->pBuffer->AddRef();
	m_total_buffers++;

	// drop oldest
	if (m_num_buffers >= MAX_BUFFERS) {
		mBuffers[m_read_index++].pBuffer->Release();
		m_read_index %= MAX_BUFFERS;
		m_num_buffers--;
		m_dropped_buffers++;
	}

	mBuffers[m_write_index++] = *pBufferItem;
	m_write_index %= MAX_BUFFERS;
	m_num_buffers++;

	mReadSem.Wakeup();
}

bool CBufferQueue::PopBuffer(BufferItem *pBufferItem, bool *pbTimeout)
{
	AUTO_LOCK(mMutex);

	while (true) {
		if (mbDisabled)
			return false;

		if (m_num_buffers > 0) {
			*pBufferItem = mBuffers[m_read_index++];
			m_read_index %= MAX_BUFFERS;
			m_num_buffers--;

			*pbTimeout = false;
			return true;
		}

		if (!mReadSem.TimedWait(mMutex, TIMEOUT)) {
			*pbTimeout = true;
			return true;
		}
	}
}

void CBufferQueue::Enable(bool bEnabled)
{
	AUTO_LOCK(mMutex);
	mbDisabled = !bEnabled;
	mReadSem.Wakeup();
}

void CBufferQueue::ReleaseAll()
{
	AUTO_LOCK(mMutex);
	for (; m_num_buffers > 0; m_num_buffers--) {
		mBuffers[m_read_index++].pBuffer->Release();
		m_read_index %= MAX_BUFFERS;
	}
}

//-----------------------------------------------------------------------
//
//  CTransaction
//
//-----------------------------------------------------------------------
CTransaction* CTransaction::Create(CTransactionList *pList, CSocketEvent *pSocketEvent, 
		CSocket **ppSocket, const server_config_t *pConfig)
{
	CTransaction *result = avf_new CTransaction(pList, pSocketEvent, ppSocket, pConfig);
	if (result) {
		result->RunThread();
	}
	CHECK_STATUS(result);
	return result;
}

CTransaction::CTransaction(CTransactionList *pList, CSocketEvent *pSocketEvent, 
		CSocket **ppSocket, const server_config_t *pConfig):
	m_server_config(pConfig),

	mpList(pList),
	mpNext(NULL),

	mpSocketEvent(pSocketEvent),
	mpSocket(*ppSocket),

	mpThread(NULL)
{
	*ppSocket = NULL;
#ifdef AVF_DEBUG
	total_bytes = 0;
	start_tick = avf_get_curr_tick();
#endif
}

void CTransaction::RunThread()
{
	// we create a detached thread so it does not need join()
	CREATE_OBJ(mpThread = CThread::Create("mjpeg-conn", ThreadEntry, this, CThread::DETACHED));

//	if (m_server_config->realtime) {
		mpThread->SetThreadPriority(CThread::PRIO_LOWER);
//	}
}

CTransaction::~CTransaction()
{
	avf_safe_release(mpThread);
	avf_safe_release(mpSocket);
	mBufferQueue.ReleaseAll();

#ifdef AVF_DEBUG
	avf_u64_t tick = avf_get_curr_tick();
	if (tick > start_tick) {
		tick -= start_tick;
		AVF_LOGD(C_WHITE "mjpeg bitrate: " LLD C_NONE,
			total_bytes * 8000 / tick);
	}
#endif
}

void CTransaction::ThreadEntry(void *p)
{
	CTransaction *pTrans = (CTransaction*)p;
	pTrans->TransactionLoop();
	pTrans->mpList->RemoveTransaction(pTrans); // detached thread
}

void CTransaction::TransactionLoop()
{
	int buffer_timeouts = 0;

	int fps = 0;
	if (ReadRequest(&fps) < 0)
		return;

	// no interesting for input
	mpSocket->IgnoreInput(true);

	mBufferQueue.SetFPS(fps);

	if (WriteResponse() < 0)
		return;

	while (true) {
		// wait until got a buffer
		CBufferQueue::BufferItem bufferItem = {NULL};
		bool bTimeout;

		if (!PopBuffer(&bufferItem, &bTimeout))
			break;

		if (bTimeout) {
			if (!mpSocket->TestConnected())
				break;
			if (++buffer_timeouts == 20) {
				buffer_timeouts = 0;
				AVF_LOGD("PopBuffer timeout");
			}
		} else {
			buffer_timeouts = 0;
			if (WriteBuffer(&bufferItem) != E_OK) {
				bufferItem.pBuffer->Release();
				break;
			}
			bufferItem.pBuffer->Release();
		}
	}
}

avf_status_t CTransaction::WriteMem(const avf_u8_t *buffer, avf_size_t size)
{
	avf_status_t status = mpSocketEvent->TCPSend(mpSocket,
		m_server_config->write_timeout*1000,
		buffer, size);
	if (status != E_OK) {
		AVF_LOGD("WriteMem failed");
	}
	return status;
}

avf_status_t CTransaction::WriteResponse()
{
	return WriteMem((const avf_u8_t*)STR_WITH_SIZE("HTTP/1.1 200 OK\r\n"
		"Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY_TAG "\r\n"
		"Cache-Control: no-cache\r\n"
		"\r\n"));
}

avf_status_t CTransaction::WriteBuffer(CBufferQueue::BufferItem *pBufferItem)
{
	char buffer[128];
	avf_status_t status;
	CBuffer *pBuffer = pBufferItem->pBuffer;
	CBuffer *pNext = pBuffer->mpNext;
	avf_size_t size1 = pBuffer->GetDataSize();
	avf_size_t size2 = pNext ? pNext->GetDataSize() : 0;

	::sprintf(buffer,
		"\r\n--%s\r\n"
		"Content-Type: image/jpeg\r\n"
		"Content-Length: %d\r\n"
		"\r\n", BOUNDARY_TAG, size1 + size2);

	if ((status = WriteMem((const avf_u8_t*)buffer, strlen(buffer))) != E_OK)
		return status;

	if ((status = WriteMem(pBuffer->GetData(), size1)) != E_OK)
		return status;

	if (size2) {
		if ((status = WriteMem(pNext->GetData(), size2)) != E_OK)
			return status;
	}

	return E_OK;
}

// GET /?fps=30
// GET / HTTP/1.1
// Host: 10.0.0.22:8081
// Connection: keep-alive
// Cache-Control: max-age=0
// User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.4 (KHTML, like Gecko) Chrome/22.0.1229.79 Safari/537.4
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
// Accept-Encoding: gzip,deflate,sdch
// Accept-Language: en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4
// Accept-Charset: GBK,utf-8;q=0.7,*;q=0.3
avf_status_t CTransaction::ReadRequest(int *fps)
{
	avf_status_t rval = E_OK;

	char *buffer;
	char *ptr;
	int len = 0;
	int remain = 1024;

	if ((buffer = (char*)avf_malloc(remain)) == NULL)
		return E_NOMEM;

	ptr = buffer;
	while (true) {

		int nread;
		avf_status_t status = mpSocketEvent->TCPReceiveAny(mpSocket, m_server_config->read_timeout*1000,
			(avf_u8_t*)ptr, remain, &nread);

		if (status != E_OK) {
			rval = status;
			break;
		}

		ptr += nread;
		len += nread;
		remain -= nread;

		if (len >= 4) {
			if (::strncmp(ptr - 4, "\r\n\r\n", 4) == 0) {
				break;
			}

			if (remain < 4) {
				AVF_LOGE("header incomplete");
				rval = E_ERROR;
				break;
			}
		}
	}

	if (rval == E_OK) {
		ptr = buffer;
		if (::strncasecmp(ptr, "GET ", 4) == 0) {
			ptr += 4;
			while (*ptr != '?') {
				if (ptr[0] == 0 || (ptr[0] == '\r' && ptr[1] == '\n'))
					goto Done;
				ptr++;
			}
			ptr++;
			int val = 0;
			if (::sscanf(ptr, "fps=%d", &val) == 1) {
				*fps = val;
			}
		}
	}

Done:
	avf_free(buffer);
	return rval;
}

//-----------------------------------------------------------------------
//
//  CTransactionList
//
//-----------------------------------------------------------------------
CTransactionList* CTransactionList::Create(CSocketEvent *pSocketEvent)
{
	CTransactionList *result = avf_new CTransactionList(pSocketEvent);
	CHECK_STATUS(result);
	return result;
}

CTransactionList::CTransactionList(CSocketEvent *pSocketEvent):
	mpSocketEvent(pSocketEvent),
	mNumWaitTrans(0),
	mNumTransactions(0),
	mbStopped(false),
	mpFirstTransaction(NULL)
{
}

CTransactionList::~CTransactionList()
{
//	StopAllTransactions();
}

avf_status_t CTransactionList::NewTransaction(CSocket **ppSocket, const server_config_t *pConfig)
{
	AUTO_LOCK(mMutex);

	if (mbStopped) {
		AVF_LOGE("CTransactionList is stopped!");
		return E_PERM;
	}

	if (mNumTransactions >= MAX_CONNECTIONS) {
		AVF_LOGE("Connection limit reaches: %d", MAX_CONNECTIONS);
		return E_IO;
	}

	CTransaction *pTrans = CTransaction::Create(this, mpSocketEvent, ppSocket, pConfig);
	if (pTrans == NULL) {
		return E_NOMEM;
	}

	if (mpFirstTransaction == NULL) {
		mpFirstTransaction = pTrans;
	} else {
		pTrans->mpNext = mpFirstTransaction;
		mpFirstTransaction = pTrans;
	}

	mNumTransactions++;
	//AVF_LOGI("created transaction [%d], total %d", pTrans->m_fd, mNumTransactions);

	return E_OK;
}

void CTransactionList::PushBuffer(const CBufferQueue::BufferItem *pBufferItem)
{
	AUTO_LOCK(mMutex);

	if (mbStopped)
		return;

	avf_u64_t tick = avf_get_curr_tick();
	for (CTransaction *pTrans = mpFirstTransaction; pTrans; pTrans = pTrans->mpNext) {
		pTrans->PushBuffer(pBufferItem, tick);
	}
}

// called by the transaction
avf_status_t CTransactionList::RemoveTransaction(CTransaction *pTrans)
{
	AUTO_LOCK(mMutex);

	if (pTrans == mpFirstTransaction) {
		mpFirstTransaction = pTrans->mpNext;
	} else {
		CTransaction *pPrev = mpFirstTransaction;
		while (pPrev && pPrev->mpNext != pTrans)
			pPrev = pPrev->mpNext;
		if (pPrev == NULL) {
			AVF_LOGE("transaction not found!");
			return E_ERROR;
		}
		pPrev->mpNext = pTrans->mpNext;
	}

	//AVF_LOGI("transaction [%d] removed, now %d", pTrans->m_fd, mNumTransactions - 1);

	avf_delete pTrans;

	mNumTransactions--;
	mCondWaitDone.WakeupAll();

	return E_OK;
}

// note: the pipe event must be signalled before this call
void CTransactionList::StopAllTransactions()
{
	AUTO_LOCK(mMutex);

	// 1. set flag
	mbStopped = true;

	// 2. disable all transaction's queue
	for (CTransaction *pTrans = mpFirstTransaction; pTrans; pTrans = pTrans->mpNext) {
		pTrans->DisableBufferQueue();
	}

	// 3. wait all threads exits
	while (true) {
		if (mNumTransactions == 0)
			break;
		mCondWaitDone.Wait(mMutex);
	}
}

//-----------------------------------------------------------------------
//
//  CMjpegServer
//
//-----------------------------------------------------------------------

CMjpegServer* CMjpegServer::Create(const server_config_t *pConfig)
{
	CMjpegServer *result = avf_new CMjpegServer(pConfig);
	CHECK_STATUS(result);
	return result;
}

CMjpegServer::CMjpegServer(const server_config_t *pConfig):
	mpNetworkService(NULL),
	m_server_config(pConfig),
	mpSocketEvent(NULL),
	mpTransList(NULL)
{
	CREATE_OBJ(mpSocketEvent = CSocketEvent::Create("MjpegServer"));
	CREATE_OBJ(mpTransList = CTransactionList::Create(mpSocketEvent));
}

CMjpegServer::~CMjpegServer()
{
	StopServer();
	avf_safe_release(mpTransList);
	avf_safe_release(mpSocketEvent);
}

avf_status_t CMjpegServer::StartServer(const server_config_t *pConfig)
{
	m_server_config = pConfig;

	mpNetworkService = CNetworkService::Create();
	if (mpNetworkService == NULL)
		return E_ERROR;

	mpTransList->StartTransaction();

	avf_status_t status = mpNetworkService->StartServer(
		pConfig->port, "MjpegServer", OnConnected, (void*)this);
	if (status != E_OK) {
		avf_safe_release(mpNetworkService);
		return status;
	}

	return E_OK;
}

avf_status_t CMjpegServer::StopServer()
{
	if (mpNetworkService) {
		mpNetworkService->StopServer(m_server_config->port);
		avf_safe_release(mpNetworkService);

		mpSocketEvent->Interrupt();
		mpTransList->StopAllTransactions();
		mpSocketEvent->Reset();
	}
	return E_OK;
}

void CMjpegServer::OnConnected(void *context, int port, CSocket **ppSocket)
{
	CMjpegServer *self = (CMjpegServer*)context;
	self->mpTransList->NewTransaction(ppSocket, self->m_server_config);
}

//-----------------------------------------------------------------------
//
//  CHttpMjpegFilter
//
//-----------------------------------------------------------------------
IFilter* CHttpMjpegFilter::Create(IEngine *pEngine, 
	const char *pName, CConfigNode *pFilterConfig)
{
	CHttpMjpegFilter *result = avf_new CHttpMjpegFilter(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CHttpMjpegFilter::CHttpMjpegFilter(IEngine *pEngine, 
		const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpInput(NULL),
	mpServer(NULL)
{
	if (pFilterConfig) {
		CConfigNode *pAttr;
		if ((pAttr = pFilterConfig->FindChild("port")) != NULL)
			mServerConfig.port = ::atoi(pAttr->GetValue());
//		if ((pAttr = pFilterConfig->FindChild("rd-timeout")) != NULL)
//			mServerConfig.read_timeout = ::atoi(pAttr->GetValue());
//		if ((pAttr = pFilterConfig->FindChild("wr-timeout")) != NULL)
//			mServerConfig.write_timeout = ::atoi(pAttr->GetValue());
	}
}

CHttpMjpegFilter::~CHttpMjpegFilter()
{
	__avf_safe_release(mpServer);
	__avf_safe_release(mpInput);
}

avf_status_t CHttpMjpegFilter::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	if ((mpServer = CMjpegServer::Create(&mServerConfig)) == NULL)
		return E_NOMEM;

	mpInput = avf_new CHttpMjpegFilterInput(this);
	CHECK_STATUS(mpInput);
	if (mpInput == NULL)
		return E_NOMEM;

	mpServer->StartServer(&mServerConfig);

	return E_OK;
}

avf_status_t CHttpMjpegFilter::PreRunFilter()
{
	inherited::PreRunFilter();

	fps_nominator = 1;
	fps_denominator = 1;

	char buffer[REG_STR_MAX];
	mpEngine->ReadRegString(NAME_MJPEG_FPS, 0, buffer, VALUE_MJPEG_FPS);
	if (buffer[0] != 0) {
		const char *p = buffer;
		char *pend;
		int nominator = ::strtoul(p, &pend, 10);
		if (pend != p) {
			if (*pend == ':') pend++;
			p = pend;
			int denominator = ::strtoul(p, &pend, 10);
			if (pend != p) {
				if (nominator <= 0) nominator = 1;
				if (denominator <= 0) denominator = 1;
				fps_nominator = nominator;
				fps_denominator = denominator;
			}
		}
	}

	m_fps_filter = fps_denominator - 1;
	AVF_LOGD(C_GREEN "fps is %d:%d" C_NONE, fps_nominator, fps_denominator);

	return E_OK;
}

IPin *CHttpMjpegFilter::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

//-----------------------------------------------------------------------
//
//  CHttpMjpegFilterInput
//
//-----------------------------------------------------------------------
CHttpMjpegFilterInput::CHttpMjpegFilterInput(CHttpMjpegFilter *pFilter):
	inherited(pFilter, 0, "CHttpMjpegInput")
{
}

CHttpMjpegFilterInput::~CHttpMjpegFilterInput()
{
}

void CHttpMjpegFilterInput::ReceiveBuffer(CBuffer *pBuffer)
{
	CMjpegServer *pServer = ((CHttpMjpegFilter*)mpFilter)->mpServer;

	IncBuffersReceived();

	switch (pBuffer->mType) {
	case CBuffer::DATA:
#if 0
		{
			avf_u8_t *p = pBuffer->GetData();
			avf_size_t len = pBuffer->GetDataSize();
			if (p[0] != 0xff || p[1] != 0xd8 || p[len-2] != 0xff || p[len-1] != 0xd9) {
				AVF_LOGE("bad jpeg frame");
			}
		}
#endif
		if (pServer) {
			CHttpMjpegFilter *pFilter = (CHttpMjpegFilter*)GetFilter();
			pFilter->m_fps_filter += pFilter->fps_nominator;
			if (pFilter->m_fps_filter >= pFilter->fps_denominator) {
				pFilter->m_fps_filter -= pFilter->fps_denominator;
				CBufferQueue::BufferItem bufferItem;
				bufferItem.pBuffer = pBuffer;
				pServer->PushBuffer(&bufferItem);
			}
		}
		break;

	case CBuffer::EOS:
		SetEOS();
		mpFilter->PostEosMsg();
		break;

	default:
		break;
	}
}

avf_status_t CHttpMjpegFilterInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (pMediaFormat->mFormatType != MF_MJPEGVideoFormat && pMediaFormat->mFormatType != MF_NullFormat) {
		AVF_LOGE("Can only accept mjpeg type");
		return E_ERROR;
	}

	return E_OK;
}


