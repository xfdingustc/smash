
#ifndef __AAC_ENCODER_H__
#define __AAC_ENCODER_H__

struct CConfigNode;
class CAacEncoder;
class CAacEncoderInput;
class CAacEncoderOutput;

struct CAudioFormat;

#ifdef USE_FDK_AAC
#include "aacenc_lib.h"
#endif

//-----------------------------------------------------------------------
//
//  CAacEncoder
//
//-----------------------------------------------------------------------
class CAacEncoder: public CActiveFilter
{
	typedef CActiveFilter inherited;
	friend class CAacEncoderInput;
	friend class CAacEncoderOutput;

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CAacEncoder(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CAacEncoder();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumInput() { return 1; }
	virtual avf_size_t GetNumOutput() { return 1; }
	virtual IPin* GetInputPin(avf_uint_t index);
	virtual IPin* GetOutputPin(avf_uint_t index);

// IActiveObject
public:
	virtual void FilterLoop();

// CActiveFilter
public:
	virtual void OnAttachAllPins(bool bAttach);

private:
	IBufferPool *mpBufferPool;
	CAacEncoderInput *mpInput;
	CAacEncoderOutput *mpOutput;
	bool mbEnabled;

#ifdef USE_FDK_AAC
	HANDLE_AACENCODER m_enc_handle;
	avf_u8_t m_enc_buffer[20*KB];
	avf_u8_t *mp_input_buffer;
	avf_size_t m_input_frame_size;
	avf_size_t m_input_bytes;
	AACENC_InfoStruct m_enc_info;
#else
	avf_u8_t *mp_encode_buffer;
	struct au_aacenc_config_s *mp_encoder_config;
	bool mbEncoderOpen;
#endif

	avf_u64_t m_pts;
	avf_size_t m_num_channels;
	avf_size_t m_sample_rate;
	int m_sample_rate_index;
	avf_u32_t m_bitrate;
	int m_afterburner;
	avf_u32_t m_enc_duration;	// set by remuxer
	avf_u32_t m_fade_ms;
	avf_u32_t m_duration_eos;
	avf_u32_t m_start_fade_pts;
	avf_u32_t m_end_pts;
	avf_u32_t m_input_length;
	tick_info_s m_tick_info;

#ifdef USE_FDK_AAC

private:
	bool FDK_FeedData(avf_u8_t *p, avf_size_t size);
	bool FDK_Encode(bool bEOS);

#endif

private:
	bool ProcessInputBuffer(CBuffer *pInBuffer);
	void zoomSamples(CBuffer *pInBuffer, int zoom100);
	void SendToAllPins(CBuffer *pOutBuffer);
	void SendAllEOS();

private:
	avf_status_t OpenEncoder();
	void CloseEncoder();
};

//-----------------------------------------------------------------------
//
//  CAacEncoderInput
//
//-----------------------------------------------------------------------
class CAacEncoderInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CAacEncoder;
	friend class CAacEncoderOutput;

private:
	CAacEncoderInput(CAacEncoder *pFilter);
	virtual ~CAacEncoderInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

private:
	CAacEncoder *mpEncoder;
};

//-----------------------------------------------------------------------
//
//  CAacEncoderOutput
//
//-----------------------------------------------------------------------
class CAacEncoderOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CAacEncoder;
	friend class CAacEncoderInput;

private:
	CAacEncoderOutput(CAacEncoder *pFilter);
	virtual ~CAacEncoderOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat();
	virtual IBufferPool *QueryBufferPool();

private:
	CAacEncoder *mpEncoder;

private:
	CAudioFormat *GetInputMediaFormat();
};

#endif

