
#ifndef __AAC_DECODER_H__
#define __AAC_DECODER_H__

struct CConfigNode;
class CAacDecoder;
class CAacDecoderInput;
class CAacDecoderOutput;

#ifdef USE_FDK_AAC
#include "aacdecoder_lib.h"
#endif

//-----------------------------------------------------------------------
//
//  CAacDecoder
//
//-----------------------------------------------------------------------
class CAacDecoder: public CActiveFilter
{
	typedef CActiveFilter inherited;
	friend class CAacDecoderInput;
	friend class CAacDecoderOutput;

public:
	static avf_status_t RecognizeFormat(CMediaFormat *pFormat);
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

protected:
	CAacDecoder(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CAacDecoder();

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
	CAacDecoderInput *mpInput;
	CAacDecoderOutput *mpOutput;

#ifdef USE_FDK_AAC
	HANDLE_AACDECODER m_dec_handle;
	bool mbConfigured;
#else
	bool mbDecoderOpen;
	struct au_aacdec_config_s *mp_decoder_config;
	uint8_t *mp_dec_buffer;
	uint32_t m_share_mem[106000];
	uint8_t m_share_mem1[252];
#endif

private:
	avf_status_t OpenDecoder(avf_size_t *buffer_size);
	void CloseDecoder();
};

//-----------------------------------------------------------------------
//
//  CAacDecoderInput
//
//-----------------------------------------------------------------------
class CAacDecoderInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CAacDecoder;
	friend class CAacDecoderOutput;

private:
	CAacDecoderInput(CAacDecoder *pFilter);
	virtual ~CAacDecoderInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

private:
	CAacDecoder *mpDecoder;
};

//-----------------------------------------------------------------------
//
//  CAacDecoderOutput
//
//-----------------------------------------------------------------------
class CAacDecoderOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CAacDecoder;
	friend class CAacDecoderInput;

private:
	CAacDecoderOutput(CAacDecoder *pFilter);
	virtual ~CAacDecoderOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat();
	virtual IBufferPool *QueryBufferPool();

private:
	CAacDecoder *mpDecoder;
	avf_size_t mSamplesPerChunk;
	avf_u32_t mBytesPerChunk;
	avf_size_t mNumChannels;

private:
	CAudioFormat *GetInputMediaFormat();
};

#endif

