
#ifndef __TEST_SINK_H__
#define __TEST_SINK_H__

struct CConfigNode;
class CTestSink;
class CTestInput;

//-----------------------------------------------------------------------
//
//  CTestSink
//
//-----------------------------------------------------------------------
class CTestSink: public CActiveFilter
{
	typedef CActiveFilter inherited;
	friend class CTestInput;

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CTestSink(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CTestSink();

// IFilter
public:
	//virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumInput() { return 2; }
	virtual IPin* GetInputPin(avf_uint_t index);

// IActiveFilter
public:
	virtual void FilterLoop();

private:
	CTestInput *mpPin0;
	CTestInput *mpPin1;
};

//-----------------------------------------------------------------------
//
//  CTestInput
//
//-----------------------------------------------------------------------
class CTestInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CTestSink;

protected:
	CTestInput(CTestSink *pFilter, avf_uint_t index);
	virtual ~CTestInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat, IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);
};

#endif

