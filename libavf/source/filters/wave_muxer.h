
#ifndef __WAVE_MUXER_H__
#define __WAVE_MUXER_H__

#include "avio.h"

struct CConfigNode;
class CWaveMuxer;
class CWaveMuxerInput;

//-----------------------------------------------------------------------
//
//  CWaveMuxer
//
//-----------------------------------------------------------------------
class CWaveMuxer: public CActiveFilter
{
	typedef CActiveFilter inherited;
	friend class CWaveMuxerInput;

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CWaveMuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CWaveMuxer();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumInput() { return 1; }
	virtual IPin* GetInputPin(avf_uint_t index);

// IActiveObject
public:
	virtual void FilterLoop();

private:
	CWaveMuxerInput *mpInput;
	IAVIO *mpIO;
	ap<string_t> mFileName;

private:
	void CompleteFile();
};

//-----------------------------------------------------------------------
//
//  CWaveMuxerInput
//
//-----------------------------------------------------------------------
class CWaveMuxerInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CWaveMuxer;

private:
	CWaveMuxerInput(CWaveMuxer *pFilter);
	virtual ~CWaveMuxerInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);
};

#endif

