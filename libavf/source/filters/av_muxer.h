
#ifndef __AV_MUXER_H__
#define __AV_MUXER_H__

class CAVMuxer;
class CAVMuxerInput;
class CAVMuxerOutput;

//-----------------------------------------------------------------------
//
//  CAVMuxer
//
//-----------------------------------------------------------------------
class CAVMuxer: public CActiveFilter
{
	typedef CActiveFilter inherited;
	friend class CAVMuxerInput;
	friend class CAVMuxerOutput;

public:
	static IFilter *Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CAVMuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CAVMuxer();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumInput() { return 2; }
	virtual avf_size_t GetNumOutput() { return 1; }
	virtual IPin* GetInputPin(avf_uint_t index);
	virtual IPin *GetOutputPin(avf_uint_t index);

// IActiveObject
public:
	virtual void FilterLoop();
	//virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);
	//virtual bool ProcessFlag(avf_int_t flag);

private:
	CAVMuxerInput *FindPinToReceive();
	CAVMuxerInput *GetMaxInputPin();
	bool PostBuffer(CAVMuxerInput *pInput, CBuffer *pBuffer);
	bool AllEOSReceived();

private:
	CMediaFormat* QueryMediaFormat();

private:
	CAVMuxerInput *mpInput[2];	// video + audio
	CAVMuxerOutput *mpOutput;

private:
	bool mbEnableMuxer;

private:
	int m_sync_counter;
	int m_sync_counter2;
	int m_frame_counter;
	bool mb_avsync_error_sent;
	bool mb_enable_avsync;
};

//-----------------------------------------------------------------------
//
//  CAVMuxerInput
//
//-----------------------------------------------------------------------
class CAVMuxerInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CAVMuxer;

protected:
	CAVMuxerInput(CAVMuxer* pFilter, avf_uint_t index);
	virtual ~CAVMuxerInput();

// IPin
public:
	virtual void ResetState() {
		inherited::ResetState();
		m_length_90k = 0;
		mbStarted = false;
	}

	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

private:
	void UpdateLength(CBuffer *pBuffer);

private:
	avf_u64_t m_start_pts;
	avf_u64_t m_length_90k;
	avf_u64_t m_buffer_pts;
	avf_u32_t m_buffer_len;
	bool mbStarted;
};

//-----------------------------------------------------------------------
//
//  CAVMuxerOutput
//
//-----------------------------------------------------------------------
class CAVMuxerOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CAVMuxer;

protected:
	CAVMuxerOutput(CAVMuxer *pFilter);
	virtual ~CAVMuxerOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat();
	virtual IBufferPool* QueryBufferPool();
	virtual bool NeedReconnect() { return true; }

private:
	IBufferPool *mpBufferPool;
};

#endif

