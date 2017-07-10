
#define LOG_TAG "clip_reader_io"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "rbtree.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "mem_stream.h"

#include "vdb_cmd.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_util.h"
#include "vdb_if.h"
#include "vdb.h"

#include "avio.h"
#include "clip_reader_io.h"

//-----------------------------------------------------------------------
//
//  CClipReaderIO
//
//-----------------------------------------------------------------------
CClipReaderIO* CClipReaderIO::Create(IVdbInfo *pVdbInfo)
{
	CClipReaderIO *result = avf_new CClipReaderIO(pVdbInfo);
	CHECK_STATUS(result);
	return result;
}

CClipReaderIO::CClipReaderIO(IVdbInfo *pVdbInfo):
	mpVdbInfo(pVdbInfo),
	mpClipReader(NULL)
{
}

CClipReaderIO::~CClipReaderIO()
{
	avf_safe_release(mpClipReader);
}

void *CClipReaderIO::GetInterface(avf_refiid_t refiid)
{
	return inherited::GetInterface(refiid);
}

void CClipReaderIO::CreateUrl(char *url, int length, int clip_type, clip_id_t clip_id,
		int stream, avf_u64_t start_time_ms, avf_u32_t length_ms)
{
	avf_snprintf(url, length, VDB_CLIPS_PROT "/%d/%x/%d/" LLD "_%d",
		clip_type, clip_id, stream, start_time_ms, length_ms);
	url[length-1] = 0;
}

// vdb:clip/[clip_type]/[clip_id]/[stream]/[start_time_ms]_[length_ms]
avf_status_t CClipReaderIO::OpenRead(const char *url)
{
	const char *p = url;
	char *end;

	if (::strncasecmp(p, VDB_CLIPS_PROT, sizeof(VDB_CLIPS_PROT)-1) != 0) {
		AVF_LOGE("url must be started with " VDB_CLIPS_PROT);
		return E_ERROR;
	}
	p += sizeof(VDB_CLIPS_PROT)-1;
	if (*p == '/') p++;

	// ----------------------------
	// clip_type
	// ----------------------------
	int clip_type = ::strtoul(p, &end, 10);
	if (p == end) {
		AVF_LOGE("no clip_type %s", p);
		return E_ERROR;
	}
	p = end;
	if (*p == '/') p++;

	// ----------------------------
	// clip_id
	// ----------------------------
	clip_id_t clip_id = ::strtoul(p, &end, 16);
	if (p == end) {
		AVF_LOGE("no clip_id %s", p);
		return E_ERROR;
	}
	p = end;
	if (*p == '/') p++;

	// ----------------------------
	// stream
	// ----------------------------
	int stream = ::strtoul(p, &end, 10);
	if (p == end) {
		AVF_LOGE("no stream %s", p);
		return E_ERROR;
	}
	p = end;
	if (*p == '/') p++;

	// ----------------------------
	// start_time_ms
	// ----------------------------
	avf_u64_t start_time_ms = ::strtoull(p, &end, 10);
	if (p == end) {
		AVF_LOGE("no start_time_ms %s", p);
		return E_ERROR;
	}
	p = end;
	if (*p == '_') p++;

	// ----------------------------
	// length_ms
	// ----------------------------
	avf_u32_t length_ms = ::strtoul(p, &end, 10);
	if (p == end) {
		AVF_LOGE("no length_ms %s", p);
		return E_ERROR;
	}

	return OpenClipReader(clip_type, clip_id, stream, start_time_ms, length_ms);
}

avf_status_t CClipReaderIO::OpenClipReader(int clip_type, clip_id_t clip_id, int stream,
	avf_u64_t start_time_ms, avf_u32_t length_ms)
{
	IVdbInfo::range_s range;
	::memset(&range, 0, sizeof(range));

	range.clip_type = clip_type;
	range._stream = stream;
	range.b_playlist = 0;
	range.ref_clip_id = clip_id;
	range.length_ms = length_ms;
	range.clip_time_ms = start_time_ms;

	IClipReader *reader = mpVdbInfo->CreateClipReaderOfTime(&range, false, false, 0, 0);
	if (reader == NULL) {
		AVF_LOGE("Cannot create IClipReader");
		return E_ERROR;
	}

	avf_safe_release(mpClipReader);
	mpClipReader = reader;

	return E_OK;
}

avf_status_t CClipReaderIO::Close()
{
	avf_safe_release(mpClipReader);
	return E_OK;
}

int CClipReaderIO::Read(void *buffer, avf_size_t size)
{
	if (mpClipReader == NULL) {
		AVF_LOGE("CClipReaderIO::Reader: no clip reader");
		return -1;
	}
	return mpClipReader->ReadClip((avf_u8_t*)buffer, size);
}

avf_status_t CClipReaderIO::Seek(avf_s64_t pos, int whence)
{
	return mpClipReader ? mpClipReader->Seek(pos, whence) : E_ERROR;
}

avf_u64_t CClipReaderIO::GetSize()
{
	return mpClipReader ? mpClipReader->GetSize() : 0;
}

avf_u64_t CClipReaderIO::GetPos()
{
	return mpClipReader ? mpClipReader->GetPos() : 0;
}

