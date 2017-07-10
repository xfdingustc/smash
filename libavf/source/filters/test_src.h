
#ifndef __TEST_SRC_H__
#define __TEST_SRC_H__

struct CConfigNode;
class CTestSrc;
class CTestOutput;

//-----------------------------------------------------------------------
//
//  CTestSrc
//
//-----------------------------------------------------------------------
class CTestSrc: public CActiveFilter
{
	typedef CActiveFilter inherited;
	friend class CTestOutput;

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CTestSrc(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CTestSrc();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumOutput() { return 2; }
	virtual IPin* GetOutputPin(avf_uint_t index);

// IActiveObject
public:
	virtual void FilterLoop();

private:
	CTestOutput *mpPin0;
	CTestOutput *mpPin1;
};

//-----------------------------------------------------------------------
//
//  CTestOutput
//
//-----------------------------------------------------------------------
class CTestOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CTestSrc;

protected:
	CTestOutput(CTestSrc *pFilter, avf_uint_t index);
	virtual ~CTestOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat();
	virtual IBufferPool* QueryBufferPool();
};

#endif

