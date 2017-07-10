
#ifndef __FILEMAP_H__
#define __FILEMAP_H__

//-----------------------------------------------------------------------
//
//  CFileMap - read file by mmap()/munmap()
//
//-----------------------------------------------------------------------
class CFileMap: public CObject
{
	typedef CObject inherited;

public:
	static CFileMap* Create();

private:
	CFileMap();
	virtual ~CFileMap();

public:
	avf_status_t OpenFileRead(const char *filename);

	INLINE avf_u64_t GetFileSize() {
		return m_filesize;
	}

	INLINE bool IsOpen() {
		return m_fd >= 0;
	}

	INLINE void Close() {
		if (m_fd >= 0) {
			DoClose();
		}
	}

	INLINE void Unmap() {
		if (m_map_ptr) {
			DoUnmap();
		}
	}

	const uint8_t *Map(uint64_t offset, uint32_t size);

	avf_u64_t UpdateSize();

private:
	void DoClose();
	void DoUnmap();

private:
	int m_fd;
	uint64_t m_filesize;
	const uint32_t page_size;

	const uint8_t *m_map_ptr;
	uint64_t m_map_offset;
	uint32_t m_map_size;
};

#endif

