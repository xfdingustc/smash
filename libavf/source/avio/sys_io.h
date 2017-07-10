
#ifndef __SYS_IO_H__
#define __SYS_IO_H__

class CSysIO;

//-----------------------------------------------------------------------
//
//  CSysIO
//
//-----------------------------------------------------------------------

class CSysIO: public CAVIO
{
public:
	static IAVIO* Create();

protected:
	CSysIO();
	virtual ~CSysIO();

public:
	virtual avf_status_t CreateFile(const char *url);
	virtual avf_status_t OpenRead(const char *url);
	virtual avf_status_t OpenWrite(const char *url);
	virtual avf_status_t OpenExisting(const char *url);
	virtual avf_status_t Close();
	virtual bool IsOpen() { return fd >= 0; }
	virtual const char *GetIOType() { return "CSysIO"; }
	virtual int Read(void *buffer, avf_size_t size);
	virtual int Write(const void *buffer, avf_size_t size);
	virtual avf_status_t Flush();
	virtual bool CanSeek() { return true; }
	virtual avf_status_t Seek(avf_s64_t pos, int whence);
	virtual avf_u64_t GetSize();
	virtual avf_u64_t GetPos() {
		return m_pos;
	}
	virtual avf_u64_t UpdateSize() {
		return GetSize();
	}

private:
	void MyClose();

private:
	int fd;
	avf_u64_t m_pos;

private:
	avf_status_t DoOpen(const char *url, const char *flag);
};

#endif

