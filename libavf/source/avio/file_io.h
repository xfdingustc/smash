
#ifndef __FILE_IO_H__
#define __FILE_IO_H__

class CFileIO;

//-----------------------------------------------------------------------
//
//  CFileIO
//
//-----------------------------------------------------------------------

class CFileIO: public CAVIO
{
public:
	static CFileIO* Create();

private:
	CFileIO();
	virtual ~CFileIO();

public:
	virtual avf_status_t CreateFile(const char *url);
	virtual avf_status_t OpenRead(const char *url);
	virtual avf_status_t OpenWrite(const char *url);
	virtual avf_status_t OpenExisting(const char *url);
	virtual avf_status_t Close();
	virtual bool IsOpen() { return mfp != NULL; }
	virtual const char *GetIOType() { return "CFileIO"; }
	virtual int Read(void *buffer, avf_size_t size);
	virtual int Write(const void *buffer, avf_size_t size);
	virtual avf_status_t Flush();
	virtual bool CanSeek() { return true; }
	virtual avf_status_t Seek(avf_s64_t pos, int whence);
	virtual avf_u64_t GetSize();
	virtual avf_u64_t GetPos();

private:
	FILE *mfp;

private:
	avf_status_t DoOpen(const char *url, const char *flag);
};


#endif

