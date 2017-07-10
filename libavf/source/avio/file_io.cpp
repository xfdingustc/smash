
#define LOG_TAG "file_io"

#include <sys/stat.h>
#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avio.h"

#include "file_io.h"

//-----------------------------------------------------------------------
//
//  CFileIO
//
//-----------------------------------------------------------------------

CFileIO* CFileIO::Create()
{
	CFileIO *result = avf_new CFileIO();
	CHECK_STATUS(result);
	return result;
}

CFileIO::CFileIO(): mfp(NULL)
{
}

CFileIO::~CFileIO()
{
	Close();
}

avf_status_t CFileIO::CreateFile(const char *url)
{
	Close();
	::remove(url);
	return OpenWrite(url) == E_OK ? E_OK : E_IO_CANNOT_CREATE_FILE;
}

avf_status_t CFileIO::DoOpen(const char *url, const char *flag)
{
	Close();

	if ((mfp = fopen(url, flag)) == NULL) {
		AVF_LOGE("fopen %s failed", url);
		return E_IO;
	}

	if (::setvbuf(mfp, NULL, _IOFBF, 16*1024) < 0) {
		AVF_LOGP("setvbuf");
	}

//--- default st_blksize: 64 KB
//	struct stat buf;
//	stat(url, &buf);
//	printf("%s: st_block=%d, st_blksize=%d\n", 
//		url, (int)buf.st_blocks, (int)buf.st_blksize);

	return E_OK;
}

avf_status_t CFileIO::OpenRead(const char *url)
{
	return DoOpen(url, "rb") == E_OK ? E_OK : E_IO_CANNOT_OPEN_FILE;
}

avf_status_t CFileIO::OpenWrite(const char *url)
{
	return DoOpen(url, "wb") == E_OK ? E_OK : E_IO_CANNOT_OPEN_FILE;
}

avf_status_t CFileIO::OpenExisting(const char *url)
{
	// TODO
	return E_UNIMP;
}

avf_status_t CFileIO::Close()
{
	if (mfp) {
		::fclose(mfp);
		mfp = NULL;
	}
	return E_OK;
}

int CFileIO::Read(void *buffer, avf_size_t size)
{
	if (mfp == NULL)
		return -1;

	int result = ::fread(buffer, 1, size, mfp);
	if (result < 0) {
		AVF_LOGE("Read failed, size: %d, return: %d", size, result);
		return E_IO;
	}

	return result;
}

int CFileIO::Write(const void *buffer, avf_size_t size)
{
	if (mfp == NULL)
		return -1;

	size_t wsize = ::fwrite(buffer, size, 1, mfp);
	if (wsize != 1) {
		AVF_LOGE("Write failed " FMT_ZD, wsize);
		return E_IO;
	}

	return size;
}

avf_status_t CFileIO::Flush()
{
	if (mfp == NULL)
		return E_IO;

	if (::fflush(mfp) < 0) {
		AVF_LOGE("Flush failed");
		return E_IO;
	}

	return E_OK;
}

avf_status_t CFileIO::Seek(avf_s64_t pos, int whence)
{
	if (mfp == NULL)
		return E_IO;

	switch (whence) {
	case kSeekSet:
		return ::fseeko(mfp, pos, SEEK_SET) < 0 ? E_IO : E_OK;
	case kSeekCur:
		return ::fseeko(mfp, pos, SEEK_CUR) < 0 ? E_IO : E_OK;
	case kSeekEnd:
		return ::fseeko(mfp, pos, SEEK_END) < 0 ? E_IO : E_OK;
	default:
		AVF_LOGE("Bad seek %d", whence);
		return E_INVAL;
	}
}

avf_u64_t CFileIO::GetSize()
{
	avf_u64_t pos = ::ftello(mfp);
	::fseeko(mfp, 0, SEEK_END);
	avf_u64_t size = ::ftello(mfp);
	::fseeko(mfp, pos, SEEK_SET);
	return size;
}

avf_u64_t CFileIO::GetPos()
{
	return ::ftello(mfp);
}

