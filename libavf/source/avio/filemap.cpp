
#define LOG_TAG "filemap"

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "filemap.h"

//-----------------------------------------------------------------------
//
//  CFileMap
//
//-----------------------------------------------------------------------
CFileMap* CFileMap::Create()
{
	CFileMap *result = avf_new CFileMap();
	CHECK_STATUS(result);
	return result;
}

CFileMap::CFileMap():
	m_fd(-1),
	m_filesize(0),
	page_size(4*1024),	// todo
	m_map_ptr(NULL),
	m_map_offset(0),
	m_map_size(0)
{
}

CFileMap::~CFileMap()
{
	Close();
}

avf_status_t CFileMap::OpenFileRead(const char *filename)
{
	Close();

	m_fd = avf_open_file(filename, O_RDONLY, 0);
	if (m_fd < 0) {
		AVF_LOGE("Cannot open %s", filename);
		return E_IO_CANNOT_OPEN_FILE;
	}

	m_filesize = avf_get_filesize(m_fd);

	return E_OK;
}

const uint8_t *CFileMap::Map(uint64_t offset, uint32_t size)
{
	if (m_fd < 0) {
		AVF_LOGE("file not open");
		return NULL;
	}

	Unmap();

	uint32_t off = (uint32_t)offset & (page_size - 1);
	offset -= off;
	size += off;

	int prot = PROT_READ;
	int flags = MAP_SHARED | MAP_LOCKED | MAP_POPULATE;

	uint8_t *ptr = (uint8_t*)::mmap(NULL, size, prot, flags, m_fd, offset);
	if (ptr == MAP_FAILED) {
		AVF_LOGP("mmap failed, offset: " LLD ", size: %d", offset, size);
		return NULL;
	}

	m_map_ptr = ptr;
	m_map_offset = offset;
	m_map_size = size;

	return ptr + off;
}

void CFileMap::DoClose()
{
	Unmap();
	avf_safe_close(m_fd);
}

void CFileMap::DoUnmap()
{
	if (::munmap((void*)m_map_ptr, m_map_size) != 0) {
		AVF_LOGP("munmap failed, addr: %p, size: %d", m_map_ptr, m_map_size);
	}
	m_map_ptr = NULL;
	m_map_offset = 0;
	m_map_size = 0;
}

avf_u64_t CFileMap::UpdateSize()
{
	m_filesize = avf_get_filesize(m_fd);
	return m_filesize;
}

