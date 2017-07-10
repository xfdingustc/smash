
#ifndef __MP3_DECODER_H__
#define __MP3_DECODER_H__

class CMp3Decoder;
class CMp3DecoderInput;
class CMp3DecoderOutput;

struct CConfigNode;
struct ffmpeg_decoder_s;

//-----------------------------------------------------------------------
//
//  CMp3Decoder
//
//-----------------------------------------------------------------------
class CMp3Decoder: public CActiveFilter
{
	typedef CActiveFilter inherited;
	friend class CMp3DecoderInput;
	friend class CMp3DecoderOutput;

public:
	static avf_status_t RecognizeFormat(CMediaFormat *pFormat);
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

protected:
	CMp3Decoder(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CMp3Decoder();

// IFilter
public:
	//virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumInput() { return 1; }
	virtual avf_size_t GetNumOutput() { return 1; }
	virtual IPin* GetInputPin(avf_uint_t index);
	virtual IPin* GetOutputPin(avf_uint_t index);

// IActiveObject
public:
	virtual void FilterLoop();

private:
	bool GetInput(ffmpeg_decoder_s *decoder);
	bool RefillData(ffmpeg_decoder_s *decoder);
	bool DecodeData(ffmpeg_decoder_s *decoder, int *error);

private:
	CMp3DecoderInput *mpInput;
	CMp3DecoderOutput *mpOutput;

	CBuffer *mpInBuffer;
	const avf_u8_t *mpInDataPtr;
	avf_size_t mInDataRemain;
	bool mbEOS;
};

//-----------------------------------------------------------------------
//
//  CMp3DecoderInput
//
//-----------------------------------------------------------------------
class CMp3DecoderInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CMp3Decoder;

private:
	CMp3DecoderInput(CMp3Decoder *pFilter);
	virtual ~CMp3DecoderInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

private:
	CMp3Decoder *mpDecoder;
};

//-----------------------------------------------------------------------
//
//  CMp3DecoderOutput
//
//-----------------------------------------------------------------------
class CMp3DecoderOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CMp3Decoder;

private:
	CMp3DecoderOutput(CMp3Decoder *pFilter);
	virtual ~CMp3DecoderOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat();
	virtual IBufferPool *QueryBufferPool();

private:
	CAudioFormat *GetInputMediaFormat();

private:
	CMp3Decoder *mpDecoder;
	avf_size_t mSamplesPerChunk;
	avf_u32_t mBytesPerChunk;
	avf_size_t mNumChannels;
};

#endif

