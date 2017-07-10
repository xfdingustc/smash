
#ifndef __RAW_MUXER_H__
#define __RAW_MUXER_H__

struct CConfigNode;
class CRawMuxer;
class CRawMuxerInput;

//-----------------------------------------------------------------------
//
//  CRawMuxer
//
//-----------------------------------------------------------------------
class CRawMuxer: public CActiveFilter
{
	typedef CActiveFilter inherited;
	friend class CRawMuxerInput;

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CRawMuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CRawMuxer();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumInput() { return 1; }
	virtual IPin* GetInputPin(avf_uint_t index);

// IActiveObject
public:
	virtual void FilterLoop();

private:
	CRawMuxerInput *mpInput;
	IAVIO *mpIO;
	ap<string_t> mFileName;
};

//-----------------------------------------------------------------------
//
//  CRawMuxerInput
//
//-----------------------------------------------------------------------
class CRawMuxerInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CRawMuxer;

private:
	CRawMuxerInput(CRawMuxer *pFilter);
	virtual ~CRawMuxerInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat, IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);
};

#endif

