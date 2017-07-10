
#ifndef __BUFFER_IO_H__
#define __BUFFER_IO_H__

class CBufferIO;

// use mmap() to allocate memory
#ifdef LINUX_OS
#define USE_MMAP	// use mmap to allocate memory
#else
#undef USE_MMAP
#endif

#define DIRECT_IO

#define PAGE_ALIGN		(4*1024)	// page size, do not change

#ifdef LINUX_OS

#define BUFFER_SIZE		(256*1024)	// be multiple of ALIGN_SIZE
#define ALIGN_SIZE		(256*1024)	// file size alignment, be multiple of PAGE_ALIGN

#else

#define BUFFER_SIZE		(64*1024)	// 
#define ALIGN_SIZE		0

#endif

#define FILEMAP_SIZE	(64*1024)

//-----------------------------------------------------------------------
//
//  CBufferIO
//
//-----------------------------------------------------------------------

class CBufferIO: public CAVIO, public IBlockIO
{
	typedef CAVIO inherited;
	DEFINE_INTERFACE;

public:
	enum {
		TYPE_NORMAL,
		TYPE_DIRECT_IO,
		TYPE_FILEMAP_IO,
	};

public:
	static CBufferIO* Create(int type = TYPE_DIRECT_IO);

private:
	CBufferIO(int type);
	virtual ~CBufferIO();

// IAVIO
public:
	virtual avf_status_t CreateFile(const char *url);
	virtual avf_status_t OpenRead(const char *url) { return E_ERROR; }
	virtual avf_status_t OpenWrite(const char *url) { return E_ERROR; }
	virtual avf_status_t OpenExisting(const char *url) { return E_ERROR; }
	virtual avf_status_t Close();
	virtual bool IsOpen();
	virtual const char *GetIOType() { return "CBufferIO"; }
	virtual int Read(void *buffer, avf_size_t size);
	virtual int Write(const void *buffer, avf_size_t size);
	virtual avf_status_t Flush();
	virtual int ReadAt(void *buffer, avf_size_t size, avf_u64_t pos);
	virtual int WriteAt(const void *buffer, avf_size_t size, avf_u64_t pos);
	virtual bool CanSeek() { return true; }
	virtual avf_status_t Seek(avf_s64_t pos, int whence = kSeekSet);
	virtual avf_u64_t GetSize();
	virtual avf_u64_t GetPos();

// IBlockIO
public:
	virtual avf_status_t _GetBlockDesc(block_desc_s& desc);
	virtual avf_status_t _EndWrite();
	virtual avf_status_t _UpdateWrite(block_desc_s& desc, const void *buf, avf_size_t size);

private:
	avf_status_t MyClose();
	avf_status_t MyFlush();
	int MyWrite(const void *buffer, avf_size_t size);
	avf_status_t OpenForRead();
	avf_status_t ReadFileAt(int fd, void *buffer, avf_size_t size, avf_u64_t pos);
	avf_status_t WriteFileAt(const void *buffer, avf_size_t size, avf_u64_t pos);
	void UpdateBuffer(avf_size_t towrite);

private:
	bool Map();
	void DoUnmap();
	void Unmap() {
		if (m_buffer_base) {
			DoUnmap();
		}
	}

private:
	avf_u32_t m_block_size;			// must be 2^^n

	int m_fd;
	int m_read_fd;					// for read in case of O_DIRECT
	char *m_filename;

	avf_u8_t *m_buffer_base;			// start of buffer
	avf_u8_t *m_buffer;

	avf_u64_t m_file_pos;				// current file position
	avf_u64_t m_file_write_pos;		// for seek
	avf_u64_t m_file_size;			// file size

	avf_u64_t m_buffer_addr;			// buffer address (block aligned)
	avf_u8_t *m_buffer_ptr;			// current pointer in buffer
	avf_u8_t *m_buffer_write_start;
	avf_u8_t *m_buffer_write_end;	// [start..end) - to write
	avf_size_t m_buffer_room;			// room in the buffer
	bool mb_dirty;					// if buffer is dirty
	bool mb_error;					// disk error

private:
	INLINE void DiskError() {
		if (!mb_error) {
			mb_error = true;
			__IO_ERROR("");
		}
	}

private:
	CMutex mMutex;
	bool mb_direct_io;
	bool mb_filemapping;
	avf_size_t m_align_size;	// for direct io

private:
	avf_status_t InitBuffer();
	void ReleaseBuffer();
	avf_status_t DoWriteBuffer();
	INLINE avf_status_t WriteBuffer() {
		return mb_filemapping ? E_OK : DoWriteBuffer();
	}
	avf_status_t ReadBuffer();
	void FillTailBlock(avf_size_t size);
};

#endif

