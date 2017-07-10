
#ifndef __SAMPLE_QUEUE_H__
#define __SAMPLE_QUEUE_H__

class CSampleQueue;
class CSampleProvider;

//-----------------------------------------------------------------------
//
//  CSampleQueue
//
//-----------------------------------------------------------------------
class CSampleQueue: public CObject
{
	typedef CObject inherited;

private:
	enum {
		BLOCK_INIT_SIZE = 80,	// todo
		BLOCK_INC = 8,
	};

public:
	static CSampleQueue* Create(avf_size_t max_blocks);

protected:
	CSampleQueue(avf_size_t max_blocks);
	virtual ~CSampleQueue();

public:
	INLINE void SetFileName(const ap<string_t>& filename) {
		mFileName = filename;
	}

	avf_status_t PushRawData(avf_size_t index, raw_data_t *raw);
	avf_status_t PushSample(avf_size_t index, CBuffer *pBuffer, const media_sample_t *pSample);
	void FinishBlock();

	void UpdateQueue(avf_u64_t fpos);
	void FlushQueue(bool bEnd);

	avf_status_t InitSampleProvider(CSampleProvider *pProvider, avf_size_t before, avf_size_t after);

private:
	avf_status_t CreateCurrentBlock();
	void ModifyLists();
	INLINE avf_size_t GetPendingSamples() {
		return mp_sample_ptr ? mp_sample_ptr - mp_curr_block->mpSamples : 0;
	}

private:
	ap<string_t> mFileName;
	CSampleProvider *mpProvider;
	avf_size_t m_provider_remain;
	avf_size_t m_provider_skip;

	// can be used
	avf_size_t m_max_blocks;
	avf_size_t m_num_blocks;
	list_head_t m_block_list;

	// not written to FS
	avf_size_t m_max_num_tmp;
	avf_size_t m_num_tmp_blocks;
	list_head_t m_tmp_block_list;

	// max samples per block
	avf_size_t m_max_block_size;

	// current block
	avf_size_t m_block_remain;
	raw_sample_t *mp_sample_ptr;
	media_block_t *mp_curr_block;
};

//-----------------------------------------------------------------------
//
//  CSampleProvider
//
//-----------------------------------------------------------------------
class CSampleProvider: public CObject, public ISampleProvider
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

	enum {
		MAX_STREAMS = 4,
	};

public:
	static CSampleProvider* Create();

private:
	CSampleProvider();
	virtual ~CSampleProvider();

// ISampleProvider
public:
	virtual avf_size_t GetNumStreams() {
		return ARRAY_SIZE(mpFormats);
	}
	virtual CMediaFormat *QueryMediaFormat(avf_size_t index);
	virtual media_block_t *PopMediaBlock();

public:
	avf_status_t SetMediaFormat(avf_size_t index, CMediaFormat *pFormat);
	avf_status_t PushMediaBlock(media_block_t *pBlock);
	INLINE avf_status_t PushEOS() {
		return PushMediaBlock(NULL);
	}

private:
	CMsgQueue *mpQueue;
	CMediaFormat *mpFormats[MAX_STREAMS];
};

#endif

