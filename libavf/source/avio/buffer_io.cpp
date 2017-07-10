
#define LOG_TAG "buffer_io"

#include <fcntl.h>
#ifdef LINUX_OS
#include <sys/mman.h>
#endif

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_malloc.h"
#include "avio.h"

#include "buffer_io.h"

//-----------------------------------------------------------------------
//
//  CBufferIO
//
//-----------------------------------------------------------------------

CBufferIO* CBufferIO::Create(int type)
{
	CBufferIO *result = avf_new CBufferIO(type);
	CHECK_STATUS(result);
	return result;
}

CBufferIO::CBufferIO(int type)
{
	if (type == TYPE_FILEMAP_IO) {
		m_block_size = FILEMAP_SIZE;
	} else {
		m_block_size = BUFFER_SIZE;
	}

	m_fd = -1;
	m_read_fd = -1;
	m_filename = NULL;

	m_buffer_base = NULL;
	m_buffer = NULL;

	mb_dirty = false;
	mb_error = false;

#ifndef LINUX_OS

	mb_direct_io = false;
	mb_filemapping = (type == TYPE_FILEMAP_IO);
	m_align_size = 0;

#else

#ifdef DIRECT_IO
	mb_direct_io = (type == TYPE_DIRECT_IO);
#else
	mb_direct_io = false;
#endif

	mb_filemapping = (type == TYPE_FILEMAP_IO);
	m_align_size = mb_direct_io ? ALIGN_SIZE : 0;

#endif
}

CBufferIO::~CBufferIO()
{
	MyClose();
	ReleaseBuffer();
}

void *CBufferIO::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IBlockIO)
		return static_cast<IBlockIO*>(this);
	return inherited::GetInterface(refiid);
}

void CBufferIO::ReleaseBuffer()
{
	if (mb_direct_io) {
#ifdef USE_MMAP
		mmap_FreeMem(m_buffer_base, m_block_size);
#else
		avf_safe_free(m_buffer_base);
#endif
	} else if (mb_filemapping) {
		Unmap();
	} else {
		avf_safe_free(m_buffer);
	}

	m_buffer_base = NULL;
	m_buffer = NULL;
}

avf_status_t CBufferIO::CreateFile(const char *url)
{
	AUTO_LOCK(mMutex);

	MyClose();

	int o_flags = O_CREAT|O_RDWR|O_TRUNC;

	if (mb_direct_io) {
#ifdef LINUX_OS
		//AVF_LOGD(C_YELLOW "use O_DIRECT for %s" C_NONE, url);
		o_flags |= O_DIRECT;
#endif
	}

	if ((m_fd = avf_open_file(url, o_flags, 0)) < 0) {
		AVF_LOGP("open %s failed, flags: %x", url, o_flags);
		DiskError();
		return E_IO_CANNOT_CREATE_FILE;
	}

	avf_status_t status = InitBuffer();
	if (status != E_OK) {
		avf_safe_close(m_fd);
		return status;
	}

	m_filename = avf_strdup(url);

	return E_OK;
}

// called by CreateFile
avf_status_t CBufferIO::InitBuffer()
{
	m_file_pos = 0;
	m_file_write_pos = 0;
	m_file_size = 0;
	m_buffer_addr = 0;

	if (mb_direct_io) {

#ifdef USE_MMAP
		if ((m_buffer_base = (avf_u8_t*)mmap_AllocMem(m_block_size)) == NULL)
			return E_NOMEM;

		m_buffer = m_buffer_base;
#else

		if ((m_buffer_base = avf_malloc_bytes(m_block_size + PAGE_ALIGN)) == NULL)
			return E_NOMEM;

		avf_size_t offset = (avf_size_t)(avf_u64_t)m_buffer_base & (PAGE_ALIGN-1);
		m_buffer = m_buffer_base + (PAGE_ALIGN - offset);

#endif

	} else if (mb_filemapping) {

		if (!Map()) {
			return E_ERROR;
		}

	} else {

		if ((m_buffer = avf_malloc_bytes(m_block_size)) == NULL)
			return E_NOMEM;
	}

	m_buffer_ptr = m_buffer;
	m_buffer_write_start = m_buffer_write_end = m_buffer;

	m_buffer_room = m_block_size;
	mb_dirty = false;

	return E_OK;
}

bool CBufferIO::Map()
{
#ifdef LINUX_OS
	Unmap();

	avf_u64_t file_end = m_buffer_addr + m_block_size;
	if (file_end > m_file_size) {
		avf_set_file_size(m_fd, file_end);
	}

	m_buffer_base = (avf_u8_t*)::mmap(NULL, m_block_size, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_LOCKED | MAP_POPULATE, m_fd, m_buffer_addr);

	if (m_buffer_base == MAP_FAILED) {
		AVF_LOGP("mmap failed, offset: " LLD ", size: %d", 0, m_block_size);
		return false;
	}

	m_buffer = m_buffer_base;

	return true;
#else
	return false;
#endif
}

void CBufferIO::DoUnmap()
{
#ifdef LINUX_OS
	if (::munmap((void*)m_buffer, m_block_size) != 0) {
		AVF_LOGP("munmap failed, addr: %p, size: %d", m_buffer, m_block_size);
	}
	m_buffer_base = NULL;
	m_buffer = NULL;
#endif
}

avf_status_t CBufferIO::DoWriteBuffer()
{
	avf_size_t block_offset;
	avf_size_t size;

	if (m_align_size > 0) {
		// buffered bytes
		if (m_buffer_addr + m_block_size <= m_file_size) {
			// write the whole block
			size = m_block_size;
		} else {
			// the tail block
			size = (avf_size_t)(m_file_size - m_buffer_addr);
			if (size == 0) {
				AVF_LOGV("DoWriteBuffer: empty");
				return E_OK;
			}

			// align the file to ALIGN_SIZE
			avf_size_t tail = size % m_align_size;
			if (tail) {
				avf_size_t tofill = m_align_size - tail;
				::memset(m_buffer + size, 0, tofill);
				size += tofill;
				m_file_size += tofill;
			}
		}

		// for O_DIRECT
		if (mb_direct_io) {
			size = ROUND_UP(size, PAGE_ALIGN);
		}

		block_offset = 0;

	} else {

		AVF_ASSERT(m_buffer_write_start >= m_buffer);
		AVF_ASSERT(m_buffer_write_start <= m_buffer_write_end);

		block_offset = m_buffer_write_start - m_buffer;
		size = m_buffer_write_end - m_buffer_write_start;

		if (size == 0) {
			AVF_LOGV("DoWriteBuffer: empty range");
			return E_OK;
		}
	}

	return WriteFileAt(m_buffer + block_offset, size, m_buffer_addr + block_offset);
}

// read at m_buffer_addr
avf_status_t CBufferIO::ReadBuffer()
{
	if (mb_filemapping) {

		if (!Map()) {
			return E_ERROR;
		}

	} else {

		avf_size_t size;
		
		if (m_file_size >= m_buffer_addr + m_block_size) {
			size = m_block_size;
		} else {
			AVF_ASSERT(m_file_size >= m_buffer_addr);
			size = (avf_size_t)(m_file_size - m_buffer_addr);
			if (size == 0) {
				AVF_LOGV("ReadBuffer: empty");
				return E_OK;
			}
		}

		avf_seek(m_fd, m_buffer_addr, SEEK_SET);

		int rval = ::read(m_fd, m_buffer, size);
		if (rval != (int)size) {
			AVF_LOGP("read %d at %p, size %d returns %d", m_fd, m_buffer, size, rval);
			return E_IO;
		}
	}

	return E_OK;
}

avf_status_t CBufferIO::MyClose()
{
	MyFlush();

	if (m_fd >= 0) {

		if (mb_filemapping) {
			avf_set_file_size(m_fd, m_file_size);
		}

		avf_safe_close(m_fd);
	}

	if (mb_direct_io) {
		avf_safe_close(m_read_fd);
	} else {
		// same with m_fd
		m_read_fd = -1;
	}

	avf_safe_free(m_filename);

	return E_OK;
}

avf_status_t CBufferIO::Close()
{
	AUTO_LOCK(mMutex);
	return MyClose();
}

int CBufferIO::Read(void *buffer, avf_size_t size)
{
	// not supported
	return E_UNIMP;
}

avf_status_t CBufferIO::_GetBlockDesc(block_desc_s& desc)
{
	AUTO_LOCK(mMutex);

	if (m_fd < 0) {
		return E_ERROR;
	}

	desc.file_remain = 0;
	desc.align_size = m_align_size;
	desc.block_size = m_block_size;
	desc.ptr = m_buffer_ptr;
	desc.remain = m_buffer_room;

	if (m_align_size > 0) {
		avf_u32_t tmp = (avf_u32_t)m_file_size % m_align_size;
		if (tmp) {
			desc.file_remain = m_align_size - tmp;
		}
	}

	return E_OK;
}

avf_status_t CBufferIO::_EndWrite()
{
	AUTO_LOCK(mMutex);

	if (mb_dirty) {
		AVF_LOGW("buffer is dirty");
	}

	ReleaseBuffer();
	m_block_size = 0;

	return E_OK;
}

avf_status_t CBufferIO::_UpdateWrite(block_desc_s& desc, const void *buf, avf_size_t size)
{
	AUTO_LOCK(mMutex);

	if (m_fd < 0) {
		return E_ERROR;
	}

	avf_size_t towrite = desc.ptr - m_buffer_ptr;

	if (towrite) {
		UpdateBuffer(towrite);
	}

	if (buf && size > 0) {
		MyWrite(buf, size);
	}

	desc.ptr = m_buffer_ptr;
	desc.remain = m_buffer_room;

	return E_OK;
}

// if buffer == NULL, write 0
int CBufferIO::MyWrite(const void *buffer, avf_size_t size)
{
	const avf_size_t total = size;

	if (m_fd < 0) {
		return -1;
	}

	if (total == 0)
		return 0;

	while (true) {
		avf_size_t towrite = MIN(size, m_buffer_room);

		if (buffer) {
			::memcpy(m_buffer_ptr, buffer, towrite);
		} else {
			::memset(m_buffer_ptr, 0, towrite);
		}

		UpdateBuffer(towrite);

		if ((size -= towrite) == 0)
			return total;

		if (buffer) {
			buffer = (const avf_u8_t*)buffer + towrite;
		}
	}
}

void CBufferIO::UpdateBuffer(avf_size_t towrite)
{
	mb_dirty = true;

	if (m_buffer_write_start > m_buffer_ptr) {
		m_buffer_write_start = m_buffer_ptr;
	}

	m_buffer_ptr += towrite;

	if (m_buffer_write_end < m_buffer_ptr) {
		m_buffer_write_end = m_buffer_ptr;
	}

	// pos & size
	if ((m_file_pos += towrite) > m_file_size) {
		m_file_size = m_file_pos;
	}

	m_buffer_room -= towrite;
	if (m_buffer_room == 0) {	// the block buffer is full
		WriteBuffer();
		mb_dirty = false;
		m_buffer_addr += m_block_size;
		m_buffer_ptr = m_buffer;
		m_buffer_write_start = m_buffer_write_end = m_buffer;
		m_buffer_room = m_block_size;
		if (mb_filemapping || m_buffer_addr < m_file_size) {
			ReadBuffer();
		}
	}
}

int CBufferIO::Write(const void *buffer, avf_size_t size)
{
	AUTO_LOCK(mMutex);
	return MyWrite(buffer, size);
}

avf_status_t CBufferIO::MyFlush()
{
	if (mb_dirty) {
		avf_status_t status = WriteBuffer();
		if (status != E_OK)
			return status;
		m_buffer_write_start = m_buffer_write_end = m_buffer_ptr;
		mb_dirty = false;
	}
	return E_OK;
}

avf_status_t CBufferIO::Flush()
{
	AUTO_LOCK(mMutex);
	if (m_fd < 0) {
		AVF_LOGE("file not open");
		return E_PERM;
	}
	return MyFlush();
}

avf_status_t CBufferIO::OpenForRead()
{
	if (m_read_fd < 0) {
		if (mb_direct_io) {
			if (m_filename == NULL) {
				AVF_LOGE("no filename");
				return E_ERROR;
			}
			m_read_fd = avf_open_file(m_filename, O_RDONLY, 0);
			if (m_read_fd < 0) {
				AVF_LOGE("cannot open %s", m_filename);
				return E_ERROR;
			}
		} else {
			m_read_fd = m_fd;
		}
	}
	return E_OK;
}

avf_status_t CBufferIO::ReadFileAt(int fd, void *buffer, avf_size_t size, avf_u64_t pos)
{
	avf_seek(fd, pos, SEEK_SET);

	int rval = ::read(fd, buffer, size);
	if (rval != (int)size) {
		AVF_LOGP("read %d at %p, size %d returns %d", fd, buffer, size, rval);
		return E_IO;
	}

	return E_OK;
}

avf_status_t CBufferIO::WriteFileAt(const void *buffer, avf_size_t size, avf_u64_t pos)
{
	if (m_file_write_pos != pos) {
		avf_seek(m_fd, pos, SEEK_SET);
		m_file_write_pos = pos;
	}

	do {
		__LOCK_IO__(LOCK_FOR_WRITE, "write");
		int rval = ::write(m_fd, buffer, size);

		if (rval != (int)size) {
			if (!mb_error) {
				AVF_LOGP("write %d at %p, size %d returns %d", m_fd, buffer, size, rval);
			}
			DiskError();
			return E_IO;
		}

		m_file_write_pos += rval;
	} while (0);

	return E_OK;
}

int CBufferIO::ReadAt(void *buffer, avf_size_t size, avf_u64_t pos)
{
	AUTO_LOCK(mMutex);

	if (m_fd < 0) {
		AVF_LOGE("file not open");
		return E_PERM;
	}

	avf_size_t total = size;
	avf_u64_t pos_end = pos + size;

	if (pos_end > m_file_size) {
		AVF_LOGE("ReadAt " LLD ", size %d beyond file range " LLD, pos, size, m_file_size);
		return E_INVAL;
	}

	// seg 1 - file
	if (pos < m_buffer_addr) {
		avf_size_t toread = (pos_end <= m_buffer_addr) ? size : (avf_size_t)(m_buffer_addr - pos);

		if (m_read_fd < 0 && OpenForRead() != E_OK)
			return E_ERROR;

		if (ReadFileAt(m_read_fd, buffer, toread, pos) != E_OK)
			return E_IO;

		pos += toread;
		buffer = (avf_u8_t*)buffer + toread;
		size -= toread;
	}

	if (size > 0) {

		// seg 2 - buffer
		avf_u64_t buffer_end = m_buffer_addr + m_block_size;
		if (pos < buffer_end) {
			avf_size_t buffer_offset = (pos <= m_buffer_addr) ? 0 : (avf_size_t)(pos - m_buffer_addr);
			avf_size_t tocopy = (buffer_offset + size <= m_block_size) ? size : (m_block_size - buffer_offset);

			AVF_LOGD(C_CYAN "copy from buffer %d" C_NONE, tocopy);
			::memcpy(buffer, m_buffer + buffer_offset, tocopy);

			buffer = (avf_u8_t*)buffer + tocopy;
			size -= tocopy;
		}

		// seg 3 - file
		if (pos_end > buffer_end) {
			avf_u64_t s = MAX(pos, buffer_end);
			avf_size_t toread = (avf_size_t)(pos_end - s);

			if (m_read_fd < 0 && OpenForRead() != E_OK)
				return E_ERROR;

			if (ReadFileAt(m_read_fd, buffer, toread, s) != E_OK)
				return E_IO;
		}
	}

	return total;
}

int CBufferIO::WriteAt(const void *buffer, avf_size_t size, avf_u64_t pos)
{
	AUTO_LOCK(mMutex);

	if (m_fd < 0) {
		AVF_LOGE("file not open");
		return E_PERM;
	}

	if (m_align_size != 0) {
		AVF_LOGE("WriteAt: align size must be 0");
		return E_PERM;
	}

	avf_size_t total = size;
	avf_u64_t pos_end = pos + size;

	if (pos_end > m_file_size) {
		AVF_LOGE("WriteAt " LLD ", size %d beyond file range " LLD, pos, size, m_file_size);
		return E_INVAL;
	}

	// seg 1 - file
	if (pos < m_buffer_addr) {
		avf_size_t towrite = (pos_end <= m_buffer_addr) ? size : (avf_size_t)(m_buffer_addr - pos);

		if (WriteFileAt(buffer, towrite, pos) != E_OK)
			return E_IO;

		pos += towrite;
		buffer = (avf_u8_t*)buffer + towrite;
		size -= towrite;
	}

	if (size > 0) {

		// seg 2 - buffer
		avf_u64_t buffer_end = m_buffer_addr + m_block_size;
		if (pos < buffer_end) {
			avf_size_t buffer_offset = (pos <= m_buffer_addr) ? 0 : (avf_size_t)(pos - m_buffer_addr);
			avf_size_t tocopy = (buffer_offset + size <= m_block_size) ? size : (m_block_size - buffer_offset);
			avf_u8_t *ptr = m_buffer + buffer_offset;

			if (m_buffer_write_start > ptr) {
				m_buffer_write_start = ptr;
			}

			::memcpy(ptr, buffer, tocopy);

			ptr += tocopy;
			if (m_buffer_write_end < ptr) {
				m_buffer_write_end = ptr;
			}

			mb_dirty = true;

			pos += tocopy;
			buffer = (avf_u8_t*)buffer + tocopy;
			size -= tocopy;
		}

		// seg 3 - file
		if (size > 0) {
			if (WriteFileAt(buffer, size, pos) != E_OK)
				return E_IO;
			pos += size;
		}

		if (pos > m_file_size) {
			m_file_size = pos;
		}
	}

	return total;
}

avf_status_t CBufferIO::Seek(avf_s64_t pos, int whence)
{
	AUTO_LOCK(mMutex);

	if (m_fd < 0) {
		AVF_LOGE("file not open");
		return E_PERM;
	}

	avf_u64_t new_file_pos;

	switch (whence) {
	case kSeekSet:
		new_file_pos = pos;
		break;
	case kSeekCur:
		new_file_pos = m_file_pos + pos;
		break;
	case kSeekEnd:
		new_file_pos = m_file_size + pos;
		break;
	default:
		AVF_LOGE("Bad seek %d", whence);
		return E_INVAL;
	}

	// if not changed
	if (new_file_pos == m_file_pos)
		return E_OK;

	// the address and offset of the new_file_pos
	avf_size_t new_block_offset = new_file_pos % m_block_size;
	avf_u64_t new_block_addr = new_file_pos - new_block_offset;

	// tail block address and offset
	avf_size_t tail_block_offset = m_file_size % m_block_size;
	avf_u64_t tail_block_addr = m_file_size - tail_block_offset;

	// 1. seek to a past block
	if (new_block_addr < tail_block_addr) {

		if (new_block_addr == m_buffer_addr) {	// seek in this buffer; not tail
			m_file_pos = new_file_pos;
			// m_file_size not changed
			// m_buffer_addr not changed
			m_buffer_ptr = m_buffer + new_block_offset;
			m_buffer_room = m_block_size - new_block_offset;
			// mb_dirty not changed
		} else {								// seek in other buffer; not tail
			MyFlush();
			m_file_pos = new_file_pos;
			// m_file_size not changed
			m_buffer_addr = new_block_addr;
			if (ReadBuffer() != E_OK)
				AVF_LOGE("ReadBuffer failed 1");
			m_buffer_ptr = m_buffer + new_block_offset;
			m_buffer_write_start = m_buffer_write_end = m_buffer_ptr;
			m_buffer_room = m_block_size - new_block_offset;
			// mb_dirty = false;
		}

	// 2. seek to the tail block
	} else if (new_block_addr == tail_block_addr) {

		bool bOtherBlock = false;

		if (m_buffer_addr != new_block_addr) {
			MyFlush();
			m_buffer_addr = new_block_addr;
			if (ReadBuffer() != E_OK)
				AVF_LOGE("ReadBuffer failed 2");
			bOtherBlock = true;
		}
		// now m_buffer_addr == new_block_addr

		if (new_block_offset > tail_block_offset) {
			m_file_size = new_file_pos;
			// m_buffer_addr not changed

			avf_size_t size = new_block_offset - tail_block_offset;
			avf_u8_t *ptr = m_buffer + tail_block_offset;
			AVF_LOGV("seek increases %d bytes", size);
			::memset(ptr, 0, size);
			mb_dirty = true;
		}

		m_file_pos = new_file_pos;
		m_buffer_ptr = m_buffer + new_block_offset;
		if (bOtherBlock) {
			m_buffer_write_start = m_buffer_write_end = m_buffer_ptr;
		}
		m_buffer_room = m_block_size - new_block_offset;

	// 3. seek to a future block
	} else {

		AVF_LOGV("seek to future block");

		if (m_buffer_addr != tail_block_addr) {
			MyFlush();
			m_buffer_addr = tail_block_addr;
			if (ReadBuffer() != E_OK)
				AVF_LOGE("ReadBuffer failed 3");
		}
		// now m_buffer_addr == tail_block_addr

		// write tail block
		avf_size_t size = m_block_size - tail_block_offset;
		avf_u8_t *ptr = m_buffer + tail_block_offset;
		if (size > 0) {
			::memset(ptr, 0, size);
			mb_dirty = true;
		}
		m_file_size = new_file_pos;
		MyFlush();

		// other blocks
		avf_u64_t block_addr = tail_block_addr + m_block_size;
		if (block_addr != new_block_addr) {
			::memset(m_buffer, 0, m_block_size);
			do {
				m_buffer_addr = block_addr;
				WriteBuffer();
				block_addr += m_block_size;
			} while (block_addr != new_block_addr);
		}

		m_file_pos = new_file_pos;
		// m_file_size was set
		m_buffer_addr = new_block_addr;
		m_buffer_ptr = m_buffer + new_block_offset;
		m_buffer_write_start = m_buffer_write_end = m_buffer_ptr;
		m_buffer_room = m_block_size - new_block_offset;
		mb_dirty = false;

		if (new_block_offset > 0) {
			::memset(m_buffer, 0, new_block_offset);
			mb_dirty = true;
		}
	}

	return E_OK;
}

avf_u64_t CBufferIO::GetSize()
{
	AUTO_LOCK(mMutex);
	return m_fd >= 0 ? m_file_size : 0;
}

avf_u64_t CBufferIO::GetPos()
{
	AUTO_LOCK(mMutex);
	return m_fd >= 0 ? m_file_pos : 0;
}

bool CBufferIO::IsOpen()
{
	AUTO_LOCK(mMutex);
	return m_fd >= 0;
}

