
#define LOG_TAG "media_writer"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avio.h"
#include "file_io.h"
#include "buffer_io.h"
#include "sys_io.h"

#include "avf_util.h"
#include "avf_config.h"

#include "writer_list.h"
#include "media_writer.h"

//-----------------------------------------------------------------------
//
//  CMediaWriter
//
//-----------------------------------------------------------------------

IFilter *CMediaWriter::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CMediaWriter *result = avf_new CMediaWriter(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CMediaWriter::CMediaWriter(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpInput(NULL),
	mpObserver(NULL),
	mpWriter(NULL),
	mpPosterIO(NULL)
{
	if (pFilterConfig == NULL) {
		mWriterFormat = "mp4";
	} else {
		CConfigNode *pAttr = pFilterConfig->FindChild("index");
		mTypeIndex = pAttr ? ::atoi(pAttr->GetValue()) : 0;

		char buffer[REG_STR_MAX];
		mpEngine->ReadRegString(NAME_MEDIAWRITER_FORMAT, mTypeIndex, buffer, "");
		if (buffer[0]) {
			mWriterFormat = buffer;
		} else {
			pAttr = pFilterConfig->FindChild("format");
			mWriterFormat = pAttr ? pAttr->GetValue() : "mp4";
		}

		pAttr = pFilterConfig->FindChild("vdb");
		if (pAttr && ::atoi(pAttr->GetValue())) {
			mpObserver = GetRecordObserver();
		}
	}

	CREATE_OBJ(mpWriter = avf_create_file_writer(mpEngine, mWriterFormat->string(),
		pFilterConfig, mTypeIndex, NULL));
}

CMediaWriter::~CMediaWriter()
{
	avf_safe_release(mpPosterIO);
	avf_safe_release(mpInput);
	avf_safe_release(mpWriter);
	//avf_safe_release(mpObserver);
}

avf_status_t CMediaWriter::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	SetRTPriority(CThread::PRIO_LOW);

	mpInput = avf_new CMediaWriterInput(this);
	CHECK_STATUS(mpInput);
	return mpInput ? E_OK : E_NOMEM;
}

avf_status_t CMediaWriter::PreRunFilter()
{
	avf_status_t status = inherited::PreRunFilter();
	if (status != E_OK)
		return status;

	i_enable = mpEngine->ReadRegInt32(NAME_RECORD, mTypeIndex, -1);
	if (i_enable < 0) {
		if (mTypeIndex == 0) {
			i_enable = 0;
		} else {
			i_enable = mpEngine->ReadRegInt32(NAME_RECORD, 0, 0);
		}
	}

	AVF_LOGD("i_enable[%d]: %d", mTypeIndex, i_enable);

	return i_enable == 1 ? mpWriter->CheckStreams() : E_OK;
}

IPin* CMediaWriter::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

void CMediaWriter::FilterLoop()
{
	CMediaFormat *pFormat = mpInput->GetUsingMediaFormat();
	if (pFormat == NULL || pFormat->mFormatType != MF_AVES) {
		ReplyCmd(E_ERROR);
		return;
	}

	if (i_enable == 1) {
		avf_stream_attr_t& stream_attr = ((CESFormat*)pFormat)->stream_attr;
		mpWriter->SetStreamAttr(stream_attr);

		if (mpObserver) {
			int video_type = (mWriterFormat == "mp4") ? VIDEO_TYPE_MP4 : VIDEO_TYPE_TS;
			mpObserver->InitVideoStream(&stream_attr, video_type, mTypeIndex);
		}

		avf_status_t status = mpWriter->StartWriter(true);
		if (status != E_OK) {
			ReplyCmd(status);
			return;
		}
	}

	avf_safe_release(mpPosterIO);

	ReplyCmd(E_OK);

	bool bEOS = false;

	while (true) {
		if (i_enable == 1) {
			if (mpObserver) {
				GenerateFileName_vdb();
			} else {
				GenerateFileName();
			}

			mpWriter->StartFile(mVideoFileName.get(), mGenTime,
				mPictureFileName.get(), mPictureGenTime, mBaseName.get());

			if (mpObserver) {
				mpObserver->StartSegment(mGenTime, mPictureGenTime, mTypeIndex);
			}
		}

		bool bEnd = MuxOneFile(&bEOS);

		if (mpPosterIO) {
			avf_safe_release(mpPosterIO);
			avf_change_file_time(mPosterFileName->string(), mPosterGenTime);
		}

		if (i_enable == 1) {
			avf_u32_t fsize = 0;
			mpWriter->EndFile(&fsize, bEnd, m_nextseg_ms);
			if (mpObserver) {
				mpObserver->EndSegment(m_nextseg_ms, fsize, mTypeIndex, bEnd);
			}
		}

		if (bEnd)
			break;
	}

	if (i_enable == 1) {
		mpWriter->EndWriter();
	}

	if (bEOS) {
		PostEosMsg();
	}
}

void CMediaWriter::GenerateFileName_vdb()
{
	string_t *name;

	mGenTime = AVF_INVALID_TIME;
	mPictureGenTime = AVF_INVALID_TIME;

	const char *ext = mWriterFormat == "mp4" ? ".mp4" : NULL;
	name = mpObserver->CreateVideoFileName(&mGenTime, mTypeIndex, ext);
	mVideoFileName = name;
	name->Release();

	if (mTypeIndex == 0) {
		name = mpObserver->CreatePictureFileName(&mPictureGenTime, mTypeIndex);
		mPictureFileName = name;
		name->Release();
	}
}

// mVideoFileName - /tmp/2004-10-11-16:01:00
// mPictureFileName - same
// mGenTime
void CMediaWriter::GenerateFileName()
{
	char buffer[REG_STR_MAX];

	mpEngine->ReadRegString(NAME_MUXER_FILE, mTypeIndex, buffer, "");
	if (buffer[0] != 0) {
		mVideoFileName = buffer;
		mGenTime = avf_get_curr_time(0);
	} else {
		mpEngine->ReadRegString(NAME_MUXER_PATH, mTypeIndex, buffer, VALUE_MUXER_PATH);
		mVideoFileName = buffer;

		mGenTime = avf_get_curr_time(0);
		::sprintf(buffer, "%04d-%02d-%02d-%02d-%02d-%02d", SPLIT_TIME(mGenTime));
		mBaseName = buffer;

		mVideoFileName += mBaseName;
		mPictureFileName = mVideoFileName;
		mPictureGenTime = mGenTime;
	}
}

int CMediaWriter::GetStreamIndex(CMediaFormat *format)
{
	CESFormat *pESFormat = (CESFormat*)mpInput->GetUsingMediaFormat();
	for (avf_uint_t i = 0; i < pESFormat->es_formats.count; i++) {
		if (format == *pESFormat->es_formats.Item(i))
			return i;
	}
	return -1;
}

void CMediaWriter::WritePoster(CBuffer *pBuffer)
{
	if (mpObserver) {
		mpObserver->AddPicture(pBuffer, mTypeIndex);
	} else {
		if (mpPosterIO == NULL) {
			if ((mpPosterIO = CBufferIO::Create()) != NULL) {
				mPosterGenTime = mGenTime;
				mPosterFileName = ap<string_t>(mPictureFileName.get(), ".jpg");
				mpPosterIO->CreateFile(mPosterFileName->string());
				AVF_LOGD("create poster file: %s", mPosterFileName->string());
			}
		}
		if (mpPosterIO) {
			mpPosterIO->Write(pBuffer->GetData(), pBuffer->GetDataSize());
			if (pBuffer->mpNext) {
				mpPosterIO->Write(pBuffer->mpNext->GetData(), pBuffer->mpNext->GetDataSize());
			}
		}
	}
}

// return true: end
bool CMediaWriter::MuxOneFile(bool *pbEOS)
{
	bool bAlignToVideo = mpEngine->ReadRegBool(NAME_ALIGN_TO_VIDEO, 0, VALUE_ALIGN_TO_VIDEO);
	bool bVideoEOS = false;
	avf_u64_t pts_90k_video = -1ULL;

	while (true) {
		CQueueInputPin *pInPin;
		CBuffer *pInBuffer;

		if (ReceiveInputBuffer(pInPin, pInBuffer) != E_OK)
			return true;

		switch (pInBuffer->mType) {
		case CBuffer::DATA:
			if (i_enable > 0) {
				if (bAlignToVideo && bVideoEOS && pInBuffer->pts_90k(pInBuffer->m_pts) >= pts_90k_video) {
					// discard
				} else if (pInBuffer->mFlags & CBuffer::F_JPEG) {
					if (mTypeIndex == 0 && i_enable == 1) {
						// todo: upload pictures
						WritePoster(pInBuffer);
					}
				} else {
					int stream = GetStreamIndex(pInBuffer->mpFormat);
					if (stream >= 0 && i_enable == 1) {
						mpWriter->WriteSample(stream, pInBuffer);
					}
				}

				if (pInBuffer->mpFormat->IsVideo()) {
					avf_u64_t next_pts = pInBuffer->m_pts + pInBuffer->m_duration;
					pts_90k_video = pInBuffer->pts_90k(next_pts);
					m_nextseg_ms = pInBuffer->pts_to_ms(next_pts);
				}
			}
			break;

		case CBuffer::EOS:
			mpInput->SetEOS();
			pInBuffer->Release();
			*pbEOS = true;
			return true;

		case CBuffer::STREAM_EOS:
			if (pInBuffer->mpFormat->IsVideo()) {
				bVideoEOS = true;
			}
			break;

		case CBuffer::NEW_SEG:
			m_nextseg_ms = pInBuffer->pts_to_ms(pInBuffer->m_pts);
			pInBuffer->Release();
			*pbEOS = false;
			return false;

		default:
			break;
		}

		pInBuffer->Release();
	}
}

//-----------------------------------------------------------------------
//
//  CMediaWriterInput
//
//-----------------------------------------------------------------------
CMediaWriterInput::CMediaWriterInput(CMediaWriter *pFilter):
	inherited(pFilter, 0, "MediaWriterInput")
{
}

CMediaWriterInput::~CMediaWriterInput()
{
}

avf_status_t CMediaWriterInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (pMediaFormat->mFormatType != MF_AVES) {
		AVF_LOGW("can only accept aves");
		return E_ERROR;
	}

	CESFormat *format = (CESFormat*)pMediaFormat;

	IMediaFileWriter *writer = ((CMediaWriter*)mpFilter)->mpWriter;
	for (avf_uint_t i = 0; i < format->es_formats.count; i++) {
		CMediaFormat *es_format = *format->es_formats.Item(i);
		writer->SetMediaFormat(i, es_format);
	}

	return E_OK;
}

void CMediaWriterInput::SetMediaFormat(CMediaFormat *pMediaFormat)
{
	inherited::SetMediaFormat(pMediaFormat);

	if (pMediaFormat == NULL) {
		IMediaFileWriter *writer = ((CMediaWriter*)mpFilter)->mpWriter;
		writer->ClearMediaFormats();
	}
}

