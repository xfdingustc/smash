
#ifndef __CLIP_READER_IO_H__
#define __CLIP_READER_IO_H__

//-----------------------------------------------------------------------
//
//  CClipReaderIO
//
//-----------------------------------------------------------------------
class CClipReaderIO: public CAVIO
{
	typedef CAVIO inherited;

public:
	static CClipReaderIO *Create(IVdbInfo *pVdbInfo);
	virtual void *GetInterface(avf_refiid_t refiid);

private:
	CClipReaderIO(IVdbInfo *pVdbInfo);
	virtual ~CClipReaderIO();

// IAVIO
public:
	virtual avf_status_t CreateFile(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenRead(const char *url);
	virtual avf_status_t OpenWrite(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenExisting(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t Close();
	virtual bool IsOpen() {
		return mpClipReader != NULL;
	}

	virtual const char *GetIOType() { return "CVdbReaderIO"; }

	virtual int Read(void *buffer, avf_size_t size);

	virtual int Write(const void *buffer, avf_size_t size) {
		return -1;
	}
	virtual avf_status_t Flush() {
		return E_ERROR;
	}

	virtual int ReadAt(void *buffer, avf_size_t size, avf_u64_t pos) {
		return -1;
	}
	virtual int WriteAt(const void *buffer, avf_size_t size, avf_u64_t pos) {
		return -1;
	}

	virtual bool CanSeek() { return true; }
	virtual avf_status_t Seek(avf_s64_t pos, int whence);

	virtual avf_u64_t GetSize();
	virtual avf_u64_t GetPos();

public:
	static void CreateUrl(char *url, int length, int clip_type, clip_id_t clip_id,
		int stream, avf_u64_t start_time_ms, avf_u32_t length_ms);

private:
	avf_status_t OpenClipReader(int clip_type, clip_id_t clip_id,
		int stream, avf_u64_t start_time_ms, avf_u32_t length_ms);

private:
	IVdbInfo *mpVdbInfo;
	IClipReader *mpClipReader;
};

#endif

