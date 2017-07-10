
#ifndef __AVF_QUEUE_H__
#define __AVF_QUEUE_H__

//#define DEBUG_QUEUE

class CMsgQueue;
class CMsgHub;

#include "avf_new.h"

typedef bool (*avf_queue_remove_msg_proc)(void *context, void *pMsg, avf_size_t size);

//-----------------------------------------------------------------------
//
// CMsgQueue
//
//-----------------------------------------------------------------------
class CMsgQueue
{
	friend class CMsgHub;

public:
	enum {
		Q_FREE = 0,			// this q is not attached to a msg hub
		Q_CMD = 1 << 0,		// this q is attached as the cmd queue
		Q_INPUT = 1 << 1,	// this q is attached as an input queue
		Q_OUTPUT = 1 << 2,	// this q is attached as an output queue
		Q_ALL = Q_CMD | Q_INPUT | Q_OUTPUT,
	};

	enum {
		MSG_LIMIT = 256,
	};

	enum {
		QF_HAS_MSG = (1 << 0),
		QF_IS_FULL = (1 << 1),
	};

public:
	static CMsgQueue* Create(const char *pName, avf_size_t msgSize, avf_size_t numMsgs);
	void Release();

private:
	CMsgQueue(const char *pName, avf_size_t msgSize, avf_size_t numMsgs);
	~CMsgQueue();

public:
	avf_status_t AttachToHub(CMsgHub *pHub, int type);
	avf_status_t DetachFromHub();

public:
	avf_status_t PostMsg(const void *pMsg, avf_size_t size);
	avf_status_t SendMsg(const void *pMsg, avf_size_t size);	// will block

	void GetMsg(void *pMsg, avf_size_t size);	// will block
	bool PeekMsg(void *pMsg, avf_size_t size);

	bool GetMsgInterruptible(void *pMsg, avf_size_t size);
	void Interrupt();

	void ReplyMsg(avf_status_t result);

	avf_size_t ClearMsgQ(avf_queue_remove_msg_proc proc, void *context);

	INLINE void SetFullMsgNum(avf_size_t numMsgs) {
		AUTO_LOCK(mMutex);
		mFullNumMsgs = numMsgs;
	}

	INLINE void SetOwner(void *pOwner) {
		mpOwner = pOwner;
	}

	INLINE void *GetOwner() {
		return mpOwner;
	}

	INLINE avf_int_t GetId() {
		return mId;
	}

public:
	// when the Q is destroyed, it will check whether
	//	(1) all msgs have been read, or
	//	(2) all msgs have NOT been read (typically buffer queues)
	// so here is an API to set the checking target
	INLINE void SetMaxMsgNum(avf_size_t maxNum) {
		mNumMaxMsgs = maxNum;
	}
	INLINE avf_size_t GetNumMsgs() {
		AUTO_LOCK(mMutex);
		return mNumMsgs;
	}

private:
	struct List {
		List *pNext;		// must be first
		bool bAllocated;	// this node is dynamically allocated, need to free
		// msg data follows

		void Release();

		INLINE void *GetBuffer() {
			return reinterpret_cast<avf_u8_t*>(this + 1);
		}
	};

public:
	avf_status_t Status() { return mStatus; }
	const char *GetName() { return mName->string(); }

private:
	avf_status_t mStatus;
	ap<string_t> mName;
	void *mpOwner;

	CMutex mMutex;		// protecte this msg queue
	CConditionBase mCondReplyMsg;	// the condition used for waiting reply of SendMsg()
	CCondition mCondSendMsg;	// the condition used to wake up waiters of SendMsg()
	CCondition mCondGetMsg;	// the condition used to wake up waiters of GetMsg()

	bool bInterrupted;
	int mType;			// Q_CMD, Q_INPUT, Q_OUTPUT etc
	CMsgHub *mpHub;		// the msg hub. Currently a msgQ can only be attached to one/no hub
	avf_int_t mId;		// in the hub
	avf_int_t mFlags;

private:
	avf_size_t mMsgSize;	// number of bytes of each msg
	avf_size_t mNumMsgs;	// number of msgs currently in the Q
	avf_size_t mNumMaxMsgs;
	avf_size_t mFullNumMsgs;
	avf_size_t mNodeAlloc;

	List *mpSendBuffer;		// for SendMsg
	avf_status_t *mpMsgResult;	// used to receive the reply of SendMsg()

	avf_u8_t *mpReservedMemory;

	List *mpListHead;
	List *mpListTail;
	List *mpFreeList;

private:
	INLINE avf_size_t GetNodeSize() {
		return sizeof(List) + mMsgSize;
	}

	INLINE static void Copy(void *to, const void *from, avf_size_t size) {
		if (size == sizeof(void*)) {
			*(void**)to = *(void**)from;
		} else {
			::memcpy(to, from, size);
		}
	}

	List *AllocNode();

	void WriteMsgLocked(List *pNode, const void *pMsg, avf_size_t size);
	void ReadMsgLocked(void *pMsg, avf_size_t size);

	INLINE void ReleaseMsgNode(List *pNode) {
		if (pNode != mpSendBuffer) {
			if (!pNode->bAllocated || mNodeAlloc < MSG_LIMIT) {
				// preserved node, or allocated too many: add to free list
				pNode->pNext = mpFreeList;
				mpFreeList = pNode;
			} else {
				// too many allocated nodes, free
				avf_free(pNode);
				mNodeAlloc--;
			}
		}
	}
};

//-----------------------------------------------------------------------
//
// CMsgHub
//
//-----------------------------------------------------------------------
class CMsgHub
{
	friend class CMsgQueue;

	enum {
		MAX_MSG_Q = 8,
	};

public:
	static CMsgHub* Create(const char *pName);
	void Release();

protected:
	CMsgHub(const char *pName);
	~CMsgHub();

public:
	avf_status_t _AttachMsgQueue(CMsgQueue *pMsgQ, int type, bool bHasMsg, bool bIsFull);
	avf_status_t _DetachMsgQueue(CMsgQueue *pMsgQ);
	void DetachAllMsgQueues();

public:
	struct MsgResult {
		CMsgQueue *pMsgQ;	// this Q has msg or full
		avf_int_t flag;	// this flag (in mEventFlags) is set
	};

	bool WaitMsg(MsgResult& result, CMsgQueue *pQ, int types, avf_int_t timeout = -1);
	void WaitMsgQueues(MsgResult& result, CMsgQueue *pQ1, CMsgQueue *pQ2, bool bGetFull);
	void SetEventFlag(avf_uint_t flag);

	INLINE void ClearAllEvents() {
		AUTO_LOCK(mMutex);
		mEventFlags = 0;
	}

protected:
	INLINE void SetMsgFlag(CMsgQueue *pMsgQ) {	// called by CMsgQueue
		SetFlag(&mMsgQFlags, pMsgQ);
	}
	INLINE void ClearMsgFlag(CMsgQueue *pMsgQ) {	// called by CMsgQueue
		ClearFlag(&mMsgQFlags, pMsgQ);
	}

	INLINE void MsgQIsFull(CMsgQueue *pMsgQ) {	// called by CMsgQueue
		SetFlag(&mMsgQFullFlags, pMsgQ);
	}
	INLINE void MsgQIsNotFull(CMsgQueue *pMsgQ){	// called by CMsgQueue
		ClearFlag(&mMsgQFullFlags, pMsgQ);
	}

private:
	void SetFlag(avf_u32_t *pFlags, CMsgQueue *pMsgQ);
	void ClearFlag(avf_u32_t *pFlags, CMsgQueue *pMsgQ);

private:
	INLINE void GetEvent(MsgResult& result) {
		int index = avf_ctz(mEventFlags);
		clear_flag(mEventFlags, (1 << index));	// clear the event flag
		result.pMsgQ = NULL;
		result.flag = index;
	}

	CMsgQueue *GetMsgQFromFlags(avf_u32_t flags, int types);

private:
	CMutex mMutex;
	CSemaphore mReadSem;
	ap<string_t> mName;

	avf_u32_t mMsgQFlags;		// each bit represents a msgQ. if set, the Q (may) has msg
	avf_u32_t mMsgQFullFlags;	// each bit represents a msgQ. if set, the Q (may) be full
	avf_u32_t mEventFlags;		// user defined events. one event per bit

#ifdef DEBUG_QUEUE
	struct info {
		avf_u64_t tick;
		avf_atomic_t counter;
		avf_atomic_t counter_save;
	};
	static info m_info_1;
	static info m_info_2;
	static void CheckPerformance(CMsgHub::info *info);
#endif

	CMsgQueue *mpMsgQs[MAX_MSG_Q];
};

#endif

