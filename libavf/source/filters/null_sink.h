
#ifndef __NULL_SINK_H__
#define __NULL_SINK_H__

struct CConfigNode;
class CNullSink;
class CNullSinkInput;

//-----------------------------------------------------------------------
//
//  CNullSink
//
//-----------------------------------------------------------------------
class CNullSink: public CFilter
{
	typedef CFilter inherited;
	friend class CNullSinkInput;

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CNullSink(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CNullSink();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_status_t RunFilter() { return E_OK; }
	virtual avf_status_t StopFilter() { return E_OK; }
	virtual avf_size_t GetNumInput() { return 1; }
	virtual IPin *GetInputPin(avf_uint_t index);

private:
	CNullSinkInput *mpInput;
};

//-----------------------------------------------------------------------
//
//  CNullSinkInput
//
//-----------------------------------------------------------------------
class CNullSinkInput: public CInputPin
{
	typedef CInputPin inherited;
	friend class CNullSink;

private:
	CNullSinkInput(CNullSink *pFilter);
	virtual ~CNullSinkInput();

// IPin
protected:
	virtual void ReceiveBuffer(CBuffer *pBuffer);
	virtual void PurgeInput() {}
	avf_status_t AcceptMedia(CMediaFormat *pMediaFormat, IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool) {
		return E_OK;
	}
};

#endif

