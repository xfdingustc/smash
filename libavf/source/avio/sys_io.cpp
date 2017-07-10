
#define LOG_TAG "sys_io"

#include <fcntl.h>
#include <sys/stat.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avio.h"

#include "sys_io.h"

//-----------------------------------------------------------------------
//
//  CSysIO
//
//-----------------------------------------------------------------------

IAVIO* CSysIO::Create()
{
	CSysIO *result = avf_new CSysIO();
	CHECK_STATUS(result);
	return result;
}

CSysIO::CSysIO():
	fd(-1),
	m_pos(0)
{
}

CSysIO::~CSysIO()
{
	MyClose();
}

void CSysIO::MyClose()
{
	avf_safe_close(fd);
	m_pos = 0;
}

avf_status_t CSysIO::CreateFile(const char *url)
{
	MyClose();
	fd = avf_open_file(url, O_RDWR|O_CREAT|O_TRUNC, 0);
	if (fd < 0) {
		AVF_LOGE("Cannot create %s", url);
		return E_IO_CANNOT_CREATE_FILE;
	}
	return E_OK;
}

avf_status_t CSysIO::OpenRead(const char *url)
{
	MyClose();
	fd = avf_open_file(url, O_RDONLY, 0);
	if (fd < 0) {
		AVF_LOGE("Cannot open %s", url);
		return E_IO_CANNOT_OPEN_FILE;
	}
	return E_OK;
}

avf_status_t CSysIO::OpenWrite(const char *url)
{
	AVF_LOGE("CSysIO::OpenWrite not implemented");
	return E_UNIMP;
}

avf_status_t CSysIO::OpenExisting(const char *url)
{
	MyClose();
	fd = avf_open_file(url, O_WRONLY, 0);
	if (fd < 0) {
		AVF_LOGE("cannot open %s", url);
		return E_IO_CANNOT_OPEN_FILE;
	}
	return E_OK;
}

avf_status_t CSysIO::Close()
{
	MyClose();
	return E_OK;
}

int CSysIO::Read(void *buffer, avf_size_t size)
{
	if (fd < 0) {
		AVF_LOGE("File not open");
		return -1;
	}
	int rval = ::read(fd, buffer, size);
	if (rval >= 0)
		m_pos += rval;
	return rval;
}

int CSysIO::Write(const void *buffer, avf_size_t size)
{
	if (fd < 0) {
		AVF_LOGE("File not open");
		return -1;
	}
	int rval = ::write(fd, buffer, size);
	if (rval >= 0)
		m_pos += rval;
	return rval;
}

avf_status_t CSysIO::Flush()
{
	// todo
	return E_OK;
}

avf_status_t CSysIO::Seek(avf_s64_t pos, int whence)
{
	if (fd < 0) {
		AVF_LOGE("File not open");
		return E_IO;
	}

	int lseek_whence;
	switch (whence) {
	case kSeekSet:
		lseek_whence = SEEK_SET;
		break;
	case kSeekCur:
		lseek_whence = SEEK_CUR;
		break;
	case kSeekEnd:
		lseek_whence = SEEK_END;
		break;
	default:
		AVF_LOGE("bad seek %d", whence);
		return E_BADCALL;
	}

	avf_s64_t rval = avf_seek(fd, pos, lseek_whence);

	if (rval < 0)
		return E_IO;

	m_pos = rval;

	return E_OK;
}

avf_u64_t CSysIO::GetSize()
{
	struct stat stat;
	if (fd < 0) {
		AVF_LOGE("file not open");
		return 0;
	}
	if (fstat(fd, &stat) < 0) {
		AVF_LOGE("fstat failed");
		return 0;
	}
	return stat.st_size;
}

