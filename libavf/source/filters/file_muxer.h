
#ifndef __FILE_MUXER_H__
#define __FILE_MUXER_H__

struct CConfigNode;
struct avf_stream_attr_s;
class CFileMuxer;
class CFileMuxerInput;
class CSampleWriter;

//-----------------------------------------------------------------------
//
//  CFileMuxer
//
//-----------------------------------------------------------------------
class CFileMuxer: public CActiveFilter, public ISegmentSaver
{
	typedef CActiveFilter inherited;
	friend class CFileMuxerInput;
	friend class CSampleWriter;
	DEFINE_INTERFACE;

	enum {
		CMD_SAVE_CURRENT = inherited::CMD_LAST,
		CMD_LAST
	};

	enum {
		FLAG_SAMPLE_WRITER = inherited::FLAG_LAST,
	};

	enum {
		MAX_INPUTS = 4,
	};

	struct save_current_t {
		avf_size_t before;
		avf_size_t after;
		const char *filename;
	};

public:
	static IFilter *Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CFileMuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CFileMuxer();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_size_t GetNumInput() { return MAX_INPUTS; }
	virtual IPin* GetInputPin(avf_uint_t index);
	virtual avf_status_t PreRunFilter();

// IActiveObject
public:
	virtual void FilterLoop();
	virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);
	virtual bool ProcessFlag(avf_int_t flag);

// ISegmentSaver
public:
	virtual bool CanSave();
	virtual avf_status_t SaveCurrent(avf_size_t before, avf_size_t after, const char *filename) {
		save_current_t param = {before, after, filename};
		return mpWorkQ->SendCmd(CMD_SAVE_CURRENT, &param);
	}

private:
	avf_status_t HandleSaveCurrent(const CMD& cmd);
	avf_status_t DoSaveCurrent(avf_size_t before, avf_size_t after, const ap<string_t>& filename);
	INLINE void DoneSaveCurrent() {
		SetEventFlag(FLAG_SAMPLE_WRITER);
	}

private:
	CFileMuxerInput *FindPinToReceive(bool *bVideoEOS, avf_u64_t *length_90k_video);
	CFileMuxerInput *GetMaxInputPin();
	bool MuxOneFile(bool *pbEOS);
	void GenerateFileName();
	void GenerateFileName_vdb();
	void CalcStreamAttr(avf_stream_attr_s& stream_attr);

private:
	CFileMuxerInput *mpInputs[MAX_INPUTS];
	avf_time_t mGenTime;
	avf_time_t mPictureGenTime;
	bool mbEnableRecord;
	bool mbEnableAVSync;
	avf_u64_t m_nextseg_ms;
	ap<string_t> mBaseName;
	bool mbFileNameSet;
	ap<string_t> mVideoFileName;
	ap<string_t> mPictureFileName;
	ap<string_t> mMuxerType;
	IMediaFileWriter *mpWriter;
	CSampleWriter *mpSampleWriter;
	IRecordObserver *mpObserver;
	avf_stream_attr_s m_stream_attr;
	avf_size_t m_sync_counter;
	avf_size_t m_sync_counter2;
	avf_size_t m_frame_counter;
	bool mb_avsync_error_sent;
};

//-----------------------------------------------------------------------
//
//  CFileMuxerInput
//
//-----------------------------------------------------------------------
class CFileMuxerInput: public CQueueInputPin
{
	typedef CQueueInputPin inherited;
	friend class CFileMuxer;

protected:
	CFileMuxerInput(CFileMuxer* pFilter, avf_uint_t index);
	virtual ~CFileMuxerInput();

// IPin
protected:
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

	virtual void ResetState() {
		inherited::ResetState();
		m_length_90k = 0;
		mbNewSeg = false;
		mbStarted = false;
	}

// CPin
public:
	virtual void SetMediaFormat(CMediaFormat *pMediaFormat);

private:
	void UpdateLength(CBuffer *pBuffer);

private:
	avf_u64_t m_start_pts;
	avf_u64_t m_length_90k;
	avf_u64_t m_buffer_pts;
	avf_u32_t m_buffer_len;
	bool mbNewSeg;
	bool mbStarted;
	avf_u64_t m_newseg_ms;
};

//-----------------------------------------------------------------------
//
//  CSampleWriter
//
//-----------------------------------------------------------------------
class CSampleWriter: public CObject
{
	typedef CObject inherited;
	friend class CFileMuxer;

public:
	static CSampleWriter* Create(CFileMuxer *pMuxer, const ap<string_t>& filename, avf_time_t gen_time,
		IMediaFileWriter *pWriter, ISampleProvider *pProvider);

public:
	CSampleWriter(CFileMuxer *pMuxer, const ap<string_t>& filename, avf_time_t gen_time,
		IMediaFileWriter *pWriter, ISampleProvider *pProvider);
	virtual ~CSampleWriter();
	void RunThread();

private:
	static void ThreadEntry(void *p);
	void ThreadLoop();

private:
	CFileMuxer *mpMuxer;
	avf_time_t mGenTime;
	ap<string_t> mFileName;
	IMediaFileWriter *mpWriter;
	ISampleProvider *mpProvider;
	CThread *mpThread;
	avf_status_t m_result;
};

#endif

