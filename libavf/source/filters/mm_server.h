
#ifndef __MM_SERVER_H__
#define __MM_SERVER_H__

class CModeService;
class CMMServer;
class CMMServerAVInput;

//-----------------------------------------------------------------------
//
//  CModeService
//
//-----------------------------------------------------------------------
class CModeService: public CObject
{
	typedef CObject inherited;

public:
	enum {
		MODE_TS,
		MODE_FRAME,
	};

public:
	static CModeService* Create(CMMServer *pFilter, IEngine *pEngine, avf_uint_t index, int mode);

private:
	CModeService(CMMServer *pFilter, IEngine *pEngine, avf_uint_t index, int mode);
	virtual ~CModeService();

public:
	void ResetState();
	void AcceptMedia(CESFormat *format);
	INLINE void ClearMediaFormat() {
		if (mpWriter) {
			mpWriter->ClearMediaFormats();
		}
	}
	INLINE avf_status_t CheckStreams() {
		return mpWriter ? mpWriter->CheckStreams() : E_OK;
	}

public:
	void CheckService(CBuffer *pBuffer);

	INLINE void ProcessBuffer(CBuffer *pBuffer) {
		if (m_opt & (UPLOAD_OPT_VIDEO|UPLOAD_OPT_AUDIO|UPLOAD_OPT_JPEG)) {
			DoProcessBuffer(pBuffer);
		}
	}

	INLINE void OnEOS() {
		if (m_opt) {
			EndService(NULL);
		}
	}

private:
	void StartService(CBuffer *pBuffer);
	void EndService(CBuffer *pBuffer);
	void StartFrame(CBuffer *pBuffer);
	void DoProcessBuffer(CBuffer *pBuffer);
	void DoEndFrame();

	INLINE void EndFrame() {
		if (m_frame_size || m_mode == MODE_FRAME) {
			DoEndFrame();
		}
	}

private:
	void WriteAVBuffer(CBuffer *pBuffer);
	void WritePicture(CBuffer *pBuffer);
	void GetFilename(char *buffer, int len, int index);
	avf_u64_t CalcTimeStamp(CBuffer *pBuffer);

	INLINE const char *GetModeName() {
		return m_mode == MODE_TS ? "ts mode" : "frame mode";
	}

private:
	CMMServer *mpFilter;
	IEngine *mpEngine;
	int m_index;
	const int m_mode;

private:
	IMediaFileWriter *mpWriter;
	IAVIO *mpIO;
	string_t *mp_filename;
	CBuffer *mp_jpg_buffer;
	char *mp_file_path;

private:
	int m_opt;
	int m_counter;
	int m_filenum;
	avf_u32_t m_last_size;
	avf_u64_t m_start_ms;
	avf_u32_t m_header_fpos;
	avf_size_t m_frame_size;
	avf_u32_t m_frame_count;
	upload_header_v2_t m_frame_header;
	avf_stream_attr_t m_stream_attr;
};

//-----------------------------------------------------------------------
//
//  CMMServer
//
//-----------------------------------------------------------------------
class CMMServer: public CFilter
{
	typedef CFilter inherited;
	friend class CModeService;
	friend class CMode2Service;
	friend class CMMServerAVInput;

	enum {
		NUM_AV = 2,
	};

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CMMServer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CMMServer();

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_status_t PreRunFilter();
	virtual avf_status_t RunFilter() { return E_OK; }
	virtual avf_status_t StopFilter() { return E_OK; }
	virtual avf_size_t GetNumInput() { return NUM_AV; }
	virtual IPin *GetInputPin(avf_uint_t index);

private:
	void OnEOS(avf_uint_t pin_index);

	INLINE void UpdateState(const char *event, avf_uint_t msg,
			avf_uint_t pin_index, avf_uint_t fileindex, avf_u32_t filesize, avf_uint_t framecount) {
		avf_u32_t buf[3] = {fileindex, filesize, framecount};
		mpEngine->WriteRegMem(event, pin_index, (const avf_u8_t*)buf, sizeof(buf));
		PostFilterMsg(msg);
	}

private:
	CMMServerAVInput *mp_av_input[NUM_AV];
	CMutex mMutex;
};

//-----------------------------------------------------------------------
//
//  CMMServerAVInput
//
//-----------------------------------------------------------------------
class CMMServerAVInput: public CInputPin
{
	typedef CInputPin inherited;
	friend class CMMServer;

private:
	CMMServerAVInput(CMMServer *pFilter, avf_uint_t index);
	virtual ~CMMServerAVInput();

// IPin
public:
	virtual void ReceiveBuffer(CBuffer *pBuffer);
	virtual void PurgeInput() {}
	virtual void ResetState();

	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat,
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool);

// CPin
public:
	virtual void SetMediaFormat(CMediaFormat *pMediaFormat);

private:
	avf_status_t CheckStreams();

private:
	CModeService *mpService1;
	CModeService *mpService2;
};

#endif

