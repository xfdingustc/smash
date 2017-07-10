
#ifndef __HTTP_IO_H__
#define __HTTP_IO_H__

class CMemBuffer;
class CHttpIO;
class CSocket;
class CSocketEvent;

//-----------------------------------------------------------------------
//
//  CHttpIO: HTTP client, readonly
//
//-----------------------------------------------------------------------

class CHttpIO: public CAVIO
{
	typedef CAVIO inherited;

public:
	static IAVIO* Create();

private:
	CHttpIO();
	virtual ~CHttpIO();

// IAVIO
public:
	virtual avf_status_t CreateFile(const char *url);
	virtual avf_status_t OpenRead(const char *url);
	virtual avf_status_t OpenWrite(const char *url);
	virtual avf_status_t OpenExisting(const char *url);
	virtual avf_status_t Close();
	virtual bool IsOpen();
	virtual const char *GetIOType() { return "CHttpIO"; }
	virtual int Read(void *buffer, avf_size_t size);
	virtual int Write(const void *buffer, avf_size_t size);
	virtual avf_status_t Flush();
	virtual bool CanSeek();
	virtual avf_status_t Seek(avf_s64_t pos, int whence);
	virtual avf_u64_t GetSize();
	virtual avf_u64_t GetPos();
	virtual avf_status_t Disconnect();

private:
	enum {
		STATE_IDLE,		// disconnected
		STATE_CONNECTING,	// connecting
		STATE_CONNECTED,	// connected with server
		STATE_BUSY,		// reading
		STATE_DISCONNECTED,	// internal use
		STATE_ERROR,
	};

private:
	avf_status_t CloseLocked();
	void DisconnectLocked();

	INLINE bool CheckDisconnected() {
		if (mState == STATE_DISCONNECTED) {
			mAbortCondition.WakeupAll();
			return true;
		}
		return false;
	}

private:
	avf_status_t ParseUrl(const char *url);
	avf_status_t ConnectWithServer();
	avf_status_t SendRequest();
	avf_status_t ReceiveResponse();
	avf_status_t ReceiveLine();
	avf_status_t FillLineBuffer();

	INLINE avf_status_t GetChar(avf_uint_t *c) {
		if (mLineRemain == 0) {
			avf_status_t status = FillLineBuffer();
			if (status != E_OK)
				return status;
		}
		mLineRemain--;
		*c = *mLinePtr++;
		return E_OK;
	}

private:
	CMutex mMutex;
	CSocketEvent *mpSocketEvent;
	CSocket *mpSocket;
	CCondition mAbortCondition;
	int mState;

	ap<string_t> mHost;
	int mPort;
	ap<string_t> mPath;

	avf_u64_t mFileSize;
	avf_u64_t mFileOffset;

private:
	CMemBuffer *pm;
	avf_u8_t mLineBuffer[256];
	avf_u8_t *mLinePtr;
	avf_u8_t mLineRemain;
};

#endif

