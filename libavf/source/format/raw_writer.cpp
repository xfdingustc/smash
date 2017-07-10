
#define LOG_TAG "raw_writer"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"

#include "avio.h"
#include "buffer_io.h"
#include "sys_io.h"

#include "avf_config.h"
#include "avf_media_format.h"
#include "raw_writer.h"

//-----------------------------------------------------------------------
//
//  CRawWriter
//
//-----------------------------------------------------------------------
IMediaFileWriter* CRawWriter::Create(IEngine *pEngine, CConfigNode *node,
		int index, const char *avio)
{
	CRawWriter *result = avf_new CRawWriter(pEngine, node, index, avio);
	CHECK_STATUS(result);
	return result;
}

CRawWriter::CRawWriter(IEngine *pEngine, CConfigNode *node, int index, const char *avio):
	inherited(pEngine),
	mpIO(NULL)
{
	CREATE_OBJ(mpIO = CBufferIO::Create());
}

CRawWriter::~CRawWriter()
{
	avf_safe_release(mpIO);
}

avf_status_t CRawWriter::StartFile(string_t *filename, avf_time_t video_time,
	string_t *picture_name, avf_time_t picture_time, string_t *basename)
{
	mFileName = filename;

	AVF_LOGD(C_YELLOW "create file %s" C_NONE, mFileName->string());
	avf_status_t status = mpIO->CreateFile(mFileName->string());
	if (status != E_OK) {
		AVF_LOGE("could not create %s", mFileName->string());
	}

	return status;
}

avf_status_t CRawWriter::EndFile(avf_u32_t *fsize, bool bLast, avf_u64_t nextseg_ms)
{
	mpIO->Close();
	return E_OK;
}

const char *CRawWriter::GetCurrentFile()
{
	return mFileName->string();
}

avf_status_t CRawWriter::SetRawData(avf_size_t stream, raw_data_t *raw)
{
	return E_OK;
}

avf_status_t CRawWriter::CheckMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat)
{
	return E_OK;
}

avf_status_t CRawWriter::SetMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat)
{
	return E_OK;
}

avf_status_t CRawWriter::CheckStreams()
{
	return E_OK;
}

avf_status_t CRawWriter::SetStreamAttr(avf_stream_attr_s& stream_attr)
{
	return E_OK;
}

avf_status_t CRawWriter::WriteSample(int stream, CBuffer* pBuffer)
{
	avf_size_t size = pBuffer->GetDataSize();
	//printf("raw write sample %d ===\n", size);
	return mpIO->Write(pBuffer->GetData(), size) == (int)size ? E_OK : E_IO;
}

