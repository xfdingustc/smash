
#ifndef __MEDIA_WRITER_H__
#define __MEDIA_WRITER_H__

class CMediaWriter;
class CMediaWriterInput;

//-----------------------------------------------------------------------
//
//  CMediaWriter
//
//-----------------------------------------------------------------------
class CMediaWriter: public CActiveFilter
{
	typedef CActiveFilter inherited;
	friend class CMediaWriterInput;

public:
	static IFilter *Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CMediaWriter(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CMediaWriter();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_status_t PreRunFilter();
	virtual avf_size_t GetNumInput() { return 1; }
	virtual IPin* GetInputPin(avf_uint_t index);

// IActiveObject
public:
	virtual void FilterLoop();

private:
	void GenerateFileName_vdb();
	void GenerateFileName();
	int GetStreamIndex(CMediaFormat *format);
	void WritePoster(CBuffer *pBuffer);
	bool MuxOneFile(bool *pbEOS);

private:
	CMediaWriterInput *mpInput;
	IRecordObserver *mpObserver;
	IMediaFileWriter *mpWriter;
	ap<string_t> mWriterFormat;

private:
	avf_time_t mGenTime;
	avf_time_t mPictureGenTime;
	ap<string_t> mBaseName;
	ap<string_t> mVideoFileName;
	ap<string_t> mPictureFileName;
	avf_u64_t m_nextseg_ms;

private:
	IAVIO *mpPosterIO;
	ap<string_t> mPosterFileName;
	avf_time_t mPosterGenTime;

private:
	int i_enable;
};

//-----------------------------------------------------------------------
//
//  CMediaWriterInput
//
//-----------------------------------------------------------------------
class CMediaWriterInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CMediaWriter;

private:
	CMediaWriterInput(CMediaWriter *pFilter);
	virtual ~CMediaWriterInput();

// IPin
public:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

// CPin
public:
	virtual void SetMediaFormat(CMediaFormat *pMediaFormat);
};

#endif

