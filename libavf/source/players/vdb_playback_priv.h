
#ifndef __VDB_PLAYBACK_PRIV_H__
#define __VDB_PLAYBACK_PRIV_H__

//-----------------------------------------------------------------------
//
//  CVdbIO
//
//-----------------------------------------------------------------------
class CVdbIO: public CAVIO
{
	typedef CAVIO inherited;

public:
	static CVdbIO *Create(IVdbInfo *pVdbInfo);
	virtual void *GetInterface(avf_refiid_t refiid);

private:
	CVdbIO(IVdbInfo *pVdbInfo);
	virtual ~CVdbIO();

public:
	avf_status_t OpenClip(IVdbInfo::range_s *range);
	avf_u32_t GetLengthMs();

// IAVIO
public:
	virtual avf_status_t CreateFile(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenRead(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenWrite(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenExisting(const char *url) {
		return E_ERROR;
	}

	virtual avf_status_t Close();

	virtual bool IsOpen() { return mpClipReader != NULL; }

	virtual const char *GetIOType() { return "CVdbIO"; }

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

private:
	IVdbInfo *mpVdbInfo;
	IClipReader *mpClipReader;
};

#endif

