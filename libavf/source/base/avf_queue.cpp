
#define LOG_TAG "avf_queue"

#include <limits.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"

#include "avf_queue.h"

//-----------------------------------------------------------------------
//
// CMsgQueue
//
//-----------------------------------------------------------------------

CMsgQueue* CMsgQueue::Create(const char *pName, avf_size_t msgSize, avf_size_t numMsgs)
{
	CMsgQueue *result = avf_new CMsgQueue(pName, msgSize, numMsgs);
	CHECK_STATUS(result);
	return result;
}

void CMsgQueue::Release()
{
	avf_delete this;
}

CMsgQueue::CMsgQueue(const char *pName, avf_size_t msgSize, avf_size_t numMsgs):
	mStatus(E_OK),
	mName(pName),
	mpOwner(NULL)
{
	bInterrupted = false;
	mType = Q_FREE;
	mpHub = NULL;
	mId = UINT_MAX;
	mFlags = 0;

	mMsgSize = ROUND_UP(msgSize, sizeof(void*));
	mNumMsgs = 0;
	mNumMaxMsgs = 0;
	mFullNumMsgs = 0;
	mNodeAlloc = 0;

	mpSendBuffer = NULL;
	mpMsgResult = NULL;

	mpReservedMemory = NULL;
	mpListHead = NULL;
	mpListTail = (List*)&mpListHead;
	mpFreeList = NULL;

	// the memory for msgs.
	// mpSendBuffer + [node + node + ... + node]
	CREATE_OBJ(mpReservedMemory = avf_malloc_bytes(GetNodeSize() * (1 + numMsgs)));

	// the SendMsg buffer
	mpSendBuffer = (List*)mpReservedMemory;
	mpSendBuffer->pNext = NULL;
	mpSendBuffer->bAllocated = false;

	// reserved nodes, keep in free-list
	List *pNode = (List*)(mpReservedMemory + GetNodeSize());
	for (int i = numMsgs; i > 0; i--) {
		pNode->bAllocated = false;
		pNode->pNext = mpFreeList;
		mpFreeList = pNode;
		pNode = (List*)((avf_u8_t*)pNode + GetNodeSize());
	}
}

CMsgQueue::~CMsgQueue()
{
	AVF_ASSERT(mpHub == NULL);

	if (mNumMsgs != mNumMaxMsgs) {
		AVF_LOGE("%s: msg mismatch: %d %d",
			mName->string(), mNumMsgs, mNumMaxMsgs);
	}

	__avf_safe_release(mpListHead);
	__avf_safe_release(mpFreeList);

	avf_safe_free(mpReservedMemory);
}

avf_status_t CMsgQueue::AttachToHub(CMsgHub *pHub, int type)
{
	AUTO_LOCK(mMutex);

	bool bHasMsg = mNumMsgs > 0;
	bool bIsFull = mFullNumMsgs > 0 && mNumMsgs >= mFullNumMsgs;

	avf_status_t status = pHub->_AttachMsgQueue(this, type, bHasMsg, bIsFull);
	if (status != E_OK)
		return status;

	set_or_clear_flag(bHasMsg, mFlags, QF_HAS_MSG);
	set_or_clear_flag(bIsFull, mFlags, QF_IS_FULL);

	return E_OK;
}

avf_status_t CMsgQueue::DetachFromHub()
{
	AUTO_LOCK(mMutex);
	if (mpHub == NULL)
		return E_INVAL;
	return mpHub->_DetachMsgQueue(this);
}

void CMsgQueue::List::Release()
{
	List *pNode = this;
	while (pNode) {
		List *pNext = pNode->pNext;
		if (pNode->bAllocated)
			avf_free(pNode);
		pNode = pNext;
	}
}

// write a msg and return
// always returns E_OK (ignore E_NOMEM)
avf_status_t CMsgQueue::PostMsg(const void *pMsg, avf_size_t size)
{
	AUTO_LOCK(mMutex);
	WriteMsgLocked(AllocNode(), pMsg, size);
	return E_OK;
}

// send a msg and wait for a reply
// wait if any other one is sending
avf_status_t CMsgQueue::SendMsg(const void *pMsg, avf_size_t size)
{
	AUTO_LOCK(mMutex);

	while (true) {

		if (mpMsgResult == NULL) {	// no other one is sending

			// write the msg
			WriteMsgLocked(mpSendBuffer, pMsg, size);

			// wait for result
			avf_status_t status;
			mpMsgResult = &status;

			mCondReplyMsg.Wait(mMutex);

			mpMsgResult = NULL;

			// if any other is waiting to send msg
			mCondSendMsg.Wakeup();

			return status;
		}

		// someone is sending, we have to wait
		mCondSendMsg.Wait(mMutex);
	}
}

// reply to SendMsg()
void CMsgQueue::ReplyMsg(avf_status_t result)
{
	AUTO_LOCK(mMutex);
	AVF_ASSERT(mpMsgResult);
	*mpMsgResult = result;
	mCondReplyMsg.Signal();
}

avf_size_t CMsgQueue::ClearMsgQ(avf_queue_remove_msg_proc proc, void *context)
{
	AUTO_LOCK(mMutex);

	avf_size_t count = 0;
	List *pPrev = (List*)&mpListHead;
	List *pCurr = mpListHead;

	while (pCurr != NULL) {
		if (proc == NULL || (proc)(context, pCurr->GetBuffer(), mMsgSize)) {
			// caller agrees to remove this msg

			if (mpListTail == pCurr) {
				mpListTail = (List*)&mpListHead;
			}

			List *pNext = pCurr->pNext;

			ReleaseMsgNode(pCurr);

			AVF_ASSERT(mNumMsgs > 0);
			mNumMsgs--;

			pPrev->pNext = pNext;
			pCurr = pNext;

		} else {

			pPrev = pCurr;
			pCurr = pCurr->pNext;
		}
	}

	if (mpHub) {
		if (mNumMsgs == 0) {
			if (test_flag(mFlags, QF_HAS_MSG)) {
				clear_flag(mFlags, QF_HAS_MSG);
				mpHub->ClearMsgFlag(this);
			}
		}
		if (mNumMsgs < mFullNumMsgs) {
			if (test_flag(mFlags, QF_IS_FULL)) {
				clear_flag(mFlags, QF_IS_FULL);
				mpHub->MsgQIsNotFull(this);
			}
		}
	}

	return count;
}

// wait until a message is received
void CMsgQueue::GetMsg(void *pMsg, avf_size_t size)
{
	AUTO_LOCK(mMutex);

	while (true) {

		if (mNumMsgs > 0) {
			ReadMsgLocked(pMsg, size);
			return;
		}

		// no msg, wait
		mCondGetMsg.Wait(mMutex);
	}
}

// wait until a message is received, or interrupted
bool CMsgQueue::GetMsgInterruptible(void *pMsg, avf_size_t size)
{
	AUTO_LOCK(mMutex);

	while (true) {

		if (bInterrupted) {
			bInterrupted = false;
			return false;
		}

		if (mNumMsgs > 0) {
			ReadMsgLocked(pMsg, size);
			return true;
		}

		// no msg, wait
		mCondGetMsg.Wait(mMutex);
	}
}

void CMsgQueue::Interrupt()
{
	AUTO_LOCK(mMutex);
	bInterrupted = true;
	mCondGetMsg.WakeupAll();
}

CMsgQueue::List* CMsgQueue::AllocNode()
{
	// try to get a node from the free list
	if (mpFreeList) {
		List *pNode = mpFreeList;
		mpFreeList = mpFreeList->pNext;
		pNode->pNext = NULL;
		return pNode;
	}

	// allocate a new node, ignore E_NOMEM
	List *pNode = (List*)avf_malloc(GetNodeSize());
	pNode->pNext = NULL;
	pNode->bAllocated = true;
	mNodeAlloc++;

	return pNode;
}

// if there's a msg, read and return true
// if there's no msg, return false
bool CMsgQueue::PeekMsg(void *pMsg, avf_size_t size)
{
	AUTO_LOCK(mMutex);

	if (mNumMsgs == 0)
		return false;

	ReadMsgLocked(pMsg, size);

	return true;
}

void CMsgQueue::WriteMsgLocked(List *pNode, const void *pMsg, avf_size_t size)
{
	Copy(pNode->GetBuffer(), pMsg, MIN(size, mMsgSize));

	// append to list tail
	pNode->pNext = NULL;
	mpListTail->pNext = pNode;
	mpListTail = pNode;

	mNumMsgs++;
	mCondGetMsg.Wakeup();

	if (mpHub) {
		if (!test_flag(mFlags, QF_HAS_MSG)) {
			set_flag(mFlags, QF_HAS_MSG);
			mpHub->SetMsgFlag(this);
		}
		if (mFullNumMsgs > 0 && mNumMsgs >= mFullNumMsgs) {
			if (!test_flag(mFlags, QF_IS_FULL)) {
				set_flag(mFlags, QF_IS_FULL);
				mpHub->MsgQIsFull(this);
			}
		}
	}
}

void CMsgQueue::ReadMsgLocked(void *pMsg, avf_size_t size)
{
	List *pNode = mpListHead;
	AVF_ASSERT(pNode);

	// remove from the list head
	mpListHead = mpListHead->pNext;
	if (mpListHead == NULL) {
		mpListTail = (List*)&mpListHead;
	}

	Copy(pMsg, pNode->GetBuffer(), MIN(size, mMsgSize));

	ReleaseMsgNode(pNode);

	AVF_ASSERT(mNumMsgs > 0);
	mNumMsgs--;

	if (mpHub) {
		if (mNumMsgs == 0) {
			if (test_flag(mFlags, QF_HAS_MSG)) {
				clear_flag(mFlags, QF_HAS_MSG);
				mpHub->ClearMsgFlag(this);
			}
		}
		if (mNumMsgs < mFullNumMsgs) {
			if (test_flag(mFlags, QF_IS_FULL)) {
				clear_flag(mFlags, QF_IS_FULL);
				mpHub->MsgQIsNotFull(this);
			}
		}
	}
}

//-----------------------------------------------------------------------
//
// CMsgHub
//
//-----------------------------------------------------------------------

#ifdef DEBUG_QUEUE
#include "avf_util.h"
CMsgHub::info CMsgHub::m_info_1;
CMsgHub::info CMsgHub::m_info_2;
void CMsgHub::CheckPerformance(CMsgHub::info *info)
{
	int c = avf_atomic_inc(&info->counter);
	if ((c % 200) == 0) {
		avf_u64_t tick = avf_get_curr_tick();
		if (info->tick == 0) {
			info->tick = tick;
		} else {
			avf_int_t len = (int)(tick - info->tick);
			info->tick = tick;
			AVF_LOGD("total %d, %d/s", c, (c - info->counter_save) * 1000 / len);
		}
		info->counter_save = c;
	}
}
#endif

CMsgHub* CMsgHub::Create(const char *pName)
{
	return avf_new CMsgHub(pName);
}

void CMsgHub::Release()
{
	avf_delete this;
}

CMsgHub::CMsgHub(const char *pName):
	mName(pName)
{
	mMsgQFlags = 0;
	mMsgQFullFlags = 0;
	mEventFlags = 0;
	SET_ARRAY_NULL(mpMsgQs);
}

CMsgHub::~CMsgHub()
{
}

// first lock pMsgQ
// then lock the hub
avf_status_t CMsgHub::_AttachMsgQueue(CMsgQueue *pMsgQ, int type, bool bHasMsg, bool bIsFull)
{
	CAutoLock lock2(mMutex);

	if (pMsgQ->mType != CMsgQueue::Q_FREE) {
		AVF_LOGE("MsgQ %s already attached", pMsgQ->mName->string());
		return E_INVAL;
	}

	for (int i = 0; i < MAX_MSG_Q; i++) {
		if (mpMsgQs[i] == NULL) {
			mpMsgQs[i] = pMsgQ;

			pMsgQ->mpHub = this;
			pMsgQ->mId = i;
			pMsgQ->mType = type;

			set_or_clear_flag(bHasMsg, mMsgQFlags, (1 << i));
			set_or_clear_flag(bIsFull, mMsgQFullFlags, (1 << i));

			return E_OK;
		}
	}

	AVF_LOGE("MsgHub is full");
	return E_NOENT;
}

// first lock pMsgQ
// then lock the hub
avf_status_t CMsgHub::_DetachMsgQueue(CMsgQueue *pMsgQ)
{
	CAutoLock lock2(mMutex);

	if (pMsgQ->mType == CMsgQueue::Q_FREE) {
		AVF_LOGE("MsgQ %s already detached", pMsgQ->mName->string());
		return E_INVAL;
	}

	int id = pMsgQ->mId;
	if ((avf_uint_t)id >= MAX_MSG_Q) {
		AVF_LOGE("bad msgQ id %d", id);
		return E_INVAL;
	}

	if (mpMsgQs[id] != pMsgQ) {
		AVF_LOGE("msgQ not in hub[%d]", id);
		return E_INVAL;
	}

	clear_flag(mMsgQFlags, (1 << id));
	clear_flag(mMsgQFullFlags, (1 << id));
	mpMsgQs[id] = NULL;

	pMsgQ->mpHub = NULL;
	pMsgQ->mId = UINT_MAX;
	pMsgQ->mType = CMsgQueue::Q_FREE;

	return E_OK;
}

void CMsgHub::DetachAllMsgQueues()
{
	for (int i = 0; i < MAX_MSG_Q; i++) {
		CMsgQueue *pMsgQ = mpMsgQs[i];
		if (pMsgQ) {
			avf_status_t status = _DetachMsgQueue(pMsgQ);
			AVF_ASSERT_OK(status);
		}
	}
}

void CMsgHub::WaitMsgQueues(MsgResult& result, CMsgQueue *pQ1, CMsgQueue *pQ2, bool bGetFull)
{
	AUTO_LOCK(mMutex);

#ifdef DEBUG_QUEUE
	CheckPerformance(&m_info_1);
#endif

	while (true) {

		if (mEventFlags) {
			GetEvent(result);
			return;
		}

		// pQ1 is more important (Q_CMD)
		if ((mMsgQFlags & (1 << pQ1->mId)) != 0) {
			result.pMsgQ = pQ1;
			result.flag = -1;
			return;
		}

		// any Q is full
		if (bGetFull && mMsgQFullFlags) {
			CMsgQueue *pMsgQ = GetMsgQFromFlags(mMsgQFullFlags, CMsgQueue::Q_ALL);
			if (pMsgQ) {
				result.pMsgQ = pMsgQ;
				result.flag = -1;
				return;
			}
		}

		// pQ2
		if ((mMsgQFlags & (1 << pQ2->mId)) != 0) {
			result.pMsgQ = pQ2;
			result.flag = -1;
			return;
		}

		mReadSem.Wait(mMutex);
	}
}

// return false: got nothing
// timeout:  -1: infinite; 0: no wait; >0: wait
bool CMsgHub::WaitMsg(CMsgHub::MsgResult& result, CMsgQueue *pQ, int types, avf_int_t timeout)
{
	AUTO_LOCK(mMutex);

#ifdef DEBUG_QUEUE
	CheckPerformance(&m_info_2);
#endif

	while (true) {

		// check if any event flag is set, from LSB to MSB
		if (mEventFlags) {
			GetEvent(result);
			return true;
		}

		// pQ may be Q_CMD, more important
		if (pQ && (mMsgQFlags & (1 << pQ->mId)) != 0) {
			result.pMsgQ = pQ;
			result.flag = -1;
			return true;
		}

		// any Q that is full
		if (mMsgQFullFlags) {
			CMsgQueue *pMsgQ = GetMsgQFromFlags(mMsgQFullFlags, types);
			if (pMsgQ != NULL) {
				result.pMsgQ = pMsgQ;
				result.flag = -1;
				return true;
			}
		}

		// any Q that has msg
		if (mMsgQFlags) {
			CMsgQueue *pMsgQ = GetMsgQFromFlags(mMsgQFlags, types);
			if (pMsgQ != NULL) {
				result.pMsgQ = pMsgQ;
				result.flag = -1;
				return true;
			}
		}

		if (timeout == 0)
			return false;

		// wait
		if (timeout < 0) {
			mReadSem.Wait(mMutex);
		} else {
			if (!mReadSem.TimedWait(mMutex, timeout))
				return false;
		}
	}
}

CMsgQueue *CMsgHub::GetMsgQFromFlags(avf_u32_t flags, int types)
{
	avf_u32_t tmp = flags;
	while (true) {
		int index = avf_ctz(tmp);
		AVF_ASSERT(index < MAX_MSG_Q);
		CMsgQueue *pMsgQ = mpMsgQs[index];
		if (pMsgQ == NULL) {
			clear_flag(flags, (1 << index));
		} else {
			// do not clear the flag since we don't know how many msgs in the Q
			// CMsgQueue will clear it
			if (test_flag(pMsgQ->mType, types)) {
				return pMsgQ;
			}
		}
		clear_flag(tmp, (1 << index));
		if (tmp == 0)
			return NULL;
	}
}

void CMsgHub::SetEventFlag(avf_uint_t flag)
{
	AVF_ASSERT(flag < sizeof(mEventFlags)*8);
	AUTO_LOCK(mMutex);
	if (!test_flag(mEventFlags, (1 << flag))) {
		set_flag(mEventFlags, (1 << flag));
		mReadSem.Wakeup();
	}
}

void CMsgHub::SetFlag(avf_u32_t *pFlags, CMsgQueue *pMsgQ)
{
	AUTO_LOCK(mMutex);
	AVF_ASSERT((avf_uint_t)pMsgQ->mId < MAX_MSG_Q);
	set_flag(*pFlags, (1 << pMsgQ->mId));
	mReadSem.Wakeup();
}

void CMsgHub::ClearFlag(avf_u32_t *pFlags, CMsgQueue *pMsgQ)
{
	AUTO_LOCK(mMutex);
	AVF_ASSERT((avf_uint_t)pMsgQ->mId < MAX_MSG_Q);
	clear_flag(*pFlags, (1 << pMsgQ->mId));
}

