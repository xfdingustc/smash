
#ifndef __RAW_WRITER_H__
#define __RAW_WRITER_H__

class CRawWriter;
struct CConfigNode;

//-----------------------------------------------------------------------
//
//  CRawWriter
//
//-----------------------------------------------------------------------
class CRawWriter: public CMediaFileWriter
{
	typedef CMediaFileWriter inherited;

public:
	static IMediaFileWriter* Create(IEngine *pEngine, CConfigNode *node, int index, const char *avio);

private:
	CRawWriter(IEngine *pEngine, CConfigNode *node, int index, const char *avio);
	virtual ~CRawWriter();

// IMediaFileWriter
public:
	//virtual avf_status_t StartWriter(bool bEnableRecord);
	//virtual avf_status_t EndWriter();

	virtual avf_status_t StartFile(string_t *filename, avf_time_t video_time,
		string_t *picture_name, avf_time_t picture_time, string_t *basename);
	virtual IAVIO *GetIO() { return mpIO; }
	virtual avf_status_t EndFile(avf_u32_t *fsize, bool bLast, avf_u64_t nextseg_ms);
	virtual const char *GetCurrentFile();

	virtual avf_status_t SetRawData(avf_size_t stream, raw_data_t *raw);

	virtual avf_status_t ClearMediaFormats() { return E_OK; }
	virtual avf_status_t CheckMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat);
	virtual avf_status_t SetMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat);
	virtual avf_status_t CheckStreams();
	virtual avf_status_t SetStreamAttr(avf_stream_attr_s& stream_attr);

	virtual avf_status_t WriteSample(int stream, CBuffer* pBuffer);
	virtual avf_status_t WritePoster(string_t *filename, avf_time_t gen_time, CBuffer* pBuffer) {
		return E_UNIMP;
	}

	virtual bool ProvideSamples() {
		return false;
	}
	virtual ISampleProvider *CreateSampleProvider(avf_size_t before, avf_size_t after) {
		return NULL;
	}

private:
	ap<string_t> mFileName;
	IAVIO *mpIO;
};

#endif

