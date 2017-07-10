
#define LOG_TAG	"mm_server"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_socket.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_config.h"

#include "avf_util.h"
#include "avio.h"
#include "sys_io.h"

#include "avf_media_format.h"
#include "writer_list.h"
#include "mm_server.h"

//-----------------------------------------------------------------------
//
//  CModeService
//
//-----------------------------------------------------------------------
CModeService* CModeService::Create(CMMServer *pFilter, IEngine *pEngine, avf_uint_t index, int mode)
{
	CModeService *result = avf_new CModeService(pFilter, pEngine, index, mode);
	CHECK_STATUS(result);
	return result;
}

CModeService::CModeService(CMMServer *pFilter, IEngine *pEngine, avf_uint_t index, int mode):
	mpFilter(pFilter),
	mpEngine(pEngine),
	m_index(index),
	m_mode(mode),
	mpWriter(NULL),
	mpIO(NULL),
	mp_filename(NULL),
	mp_jpg_buffer(NULL),
	mp_file_path(NULL)
{
	if (mode == MODE_TS) {
		CREATE_OBJ(mpWriter = avf_create_file_writer(mpEngine, "ts", NULL, m_index, "mmap"));
	} else {
		CREATE_OBJ(mpIO = CSysIO::Create());
	}
	m_counter = 1;
	m_last_size = 0;
}

CModeService::~CModeService()
{
	avf_safe_release(mpWriter);
	avf_safe_release(mpIO);
	avf_safe_release(mp_filename);
	avf_safe_release(mp_jpg_buffer);
	avf_safe_free(mp_file_path);
}

void CModeService::ResetState()
{
	m_opt = 0;
//	m_counter = 0;
	m_filenum = 0;
	m_frame_size = 0;
	m_frame_count = 0;
}

void CModeService::AcceptMedia(CESFormat *format)
{
	m_stream_attr = format->stream_attr;
	if (m_mode == MODE_TS) {
		for (avf_uint_t i = 0; i < format->es_formats.count; i++) {
			CMediaFormat *es_format = *format->es_formats.Item(i);
			mpWriter->SetMediaFormat(i, es_format);
		}
	}
}

void CModeService::CheckService(CBuffer *pBuffer)
{
	const char *name = m_mode == MODE_TS ? NAME_MMS_OPT_MODE1 : NAME_MMS_OPT_MODE2;
	int opt = mpEngine->ReadRegInt64(name, m_index, 0);

	if (opt) {

		if (m_opt) {
			EndService(pBuffer);
			opt = m_opt;	// **
		} else {
			AVF_LOGD("--- Start MMS %s ---", GetModeName());

			//m_counter = 1;
			m_start_ms = pBuffer->pts_to_ms(pBuffer->m_pts);
			name = m_mode == MODE_TS ? NAME_MMS_NUMFILES_MODE1 : NAME_MMS_NUMFILES_MODE2;
			m_filenum = mpEngine->ReadRegBool(name, m_index, 5);
			avf_safe_free(mp_file_path);

			if (m_mode == MODE_TS) {
				mpWriter->StartWriter(true);
			}
		}

		StartService(pBuffer);

	} else {
		if (m_opt) {
			EndService(NULL);
		}
	}

	m_opt = opt;
}

void CModeService::DoProcessBuffer(CBuffer *pBuffer)
{
	if ((pBuffer->mFlags & CBuffer::F_JPEG)) {
		if (m_opt & UPLOAD_OPT_JPEG) {
			if (m_frame_size == 0 || m_mode == MODE_FRAME) {
				WritePicture(pBuffer);
			} else {
				avf_assign(mp_jpg_buffer, pBuffer);
			}
		}
		return;
	}

	if (pBuffer->mpFormat->IsVideo()) {
		EndFrame();
		StartFrame(pBuffer);
		if (m_opt & UPLOAD_OPT_VIDEO) {
			WriteAVBuffer(pBuffer);
		}
		return;
	}

	if (pBuffer->mpFormat->IsAudio()) {
		if (m_opt & UPLOAD_OPT_AUDIO) {
			WriteAVBuffer(pBuffer);
		}
		return;
	}
}

void CModeService::StartService(CBuffer *pBuffer)
{
	char filename[256];
	GetFilename(with_size(filename), m_counter);

	avf_safe_release(mp_filename);
	mp_filename = string_t::CreateFrom(filename);
	m_frame_count = 0;

	if (m_mode == MODE_TS) {
		avf_status_t status = mpWriter->StartFile(mp_filename, AVF_INVALID_TIME, NULL, AVF_INVALID_TIME, NULL);

		if (status != E_OK) {
			avf_safe_release(mp_filename);
			mpIO = NULL;
		} else {
			mpIO = mpWriter->GetIO();
		}
	} else {
		avf_status_t status = mpIO->CreateFile(mp_filename->string());
		if (status != E_OK) {
			avf_safe_release(mp_filename);
		}

		upload_header_v2_t header;

		header.u_data_type = VDB_STREAM_ATTR;
		header.u_data_size = sizeof(m_stream_attr);
		header.u_timestamp = 0;
		header.u_stream = 0;
		header.u_duration = 0;
		header.u_version = UPLOAD_VERSION_v2;
		header.u_flags = 0;
		header.u_param1 = 0;
		header.u_param2 = 0;

		mpIO->Write((const void*)&header, sizeof(header));
		mpIO->Write((const void*)&m_stream_attr, sizeof(m_stream_attr));
	}
}

void CModeService::EndService(CBuffer *pBuffer)
{
	EndFrame();

	if (m_mode == MODE_TS) {
		mpWriter->EndFile(NULL, pBuffer == NULL, 0);
		mpIO = NULL;
	} else {
		mpIO->Close();
	}

	avf_safe_release(mp_filename);

	if (m_counter > m_filenum) {
		char filename[256];
		GetFilename(with_size(filename), m_counter - m_filenum);
		if (!avf_remove_file(filename, false)) {
			AVF_LOGD(C_CYAN "cannot remove %s" C_NONE, filename);
		} else {
			AVF_LOGD("removed %s", filename);
		}
	}

	if (pBuffer == NULL) {
		if (m_mode == MODE_TS) {
			mpWriter->EndWriter();
		}
		m_opt = 0;
		avf_safe_free(mp_file_path);
		AVF_LOGD("--- End MMS %s --", GetModeName());
	}

	m_counter++;
	m_last_size = 0;
}

void CModeService::StartFrame(CBuffer *pBuffer)
{
	if (mpIO && m_mode == MODE_TS) {
		m_header_fpos = mpIO->GetPos();
		m_frame_size = 0;

		m_frame_header.u_data_type = VDB_TS_FRAME;
		m_frame_header.u_data_size = 0; // fixed later
		m_frame_header.u_timestamp = CalcTimeStamp(pBuffer);
		m_frame_header.u_stream = 0;
		m_frame_header.u_duration = 0;	// todo
		m_frame_header.u_version = UPLOAD_VERSION_v2;
		m_frame_header.u_flags = 0;
		m_frame_header.u_param1 = 0;
		m_frame_header.u_param2 = 0;

		mpIO->Write((const void*)&m_frame_header, sizeof(m_frame_header));
	}
}

void CModeService::DoEndFrame()
{
	if (m_mode == MODE_TS) {
		if (mpIO == NULL)
			return;

		if (m_frame_size > 0) {
			avf_u32_t pos = mpIO->GetPos();
			m_frame_header.u_data_size = pos - m_header_fpos - sizeof(m_frame_header);
			mpIO->WriteAt((const void*)&m_frame_header, sizeof(m_frame_header), m_header_fpos);
			m_frame_size = 0;
		}

		if (mp_jpg_buffer) {
			WritePicture(mp_jpg_buffer);
			mp_jpg_buffer->Release();
		}
	}

	m_frame_count++;
	mpIO->Flush();

	const char *event = m_mode == MODE_TS ? NAME_MMS_STATE_MODE1 : NAME_MMS_STATE_MODE2;
	avf_uint_t msg = m_mode == MODE_TS ? IEngine::MSG_MMS_MODE1 : IEngine::MSG_MMS_MODE2;
	avf_u32_t filesize = (avf_u32_t)mpIO->GetSize();
	if (filesize <= m_last_size) {
		AVF_LOGW("bad filesize: counter=%d, last=%d, this=%d, frames=%d",
			m_counter, m_last_size, filesize, m_frame_count);
	}
	m_last_size = filesize;
	mpFilter->UpdateState(event, msg, m_index, m_counter, filesize, m_frame_count);
}

void CModeService::WriteAVBuffer(CBuffer *pBuffer)
{
	if (m_mode == MODE_TS) {
		if (mpIO) {
			mpWriter->WriteSample(-1, pBuffer);
			m_frame_size += pBuffer->GetDataSize();
		}
	} else {
		upload_header_v2_t header;

		header.u_data_type = pBuffer->mpFormat->IsVideo() ? VDB_VIDEO_FRAME : VDB_AUDIO_FRAME;
		header.u_data_size = pBuffer->GetDataSize();
		header.u_timestamp = CalcTimeStamp(pBuffer);
		header.u_stream = 0;
		header.u_duration = 0;	// todo
		header.u_version = UPLOAD_VERSION_v2;
		header.u_flags = 0;
		header.u_param1 = 0;
		header.u_param2 = 0;
		mpIO->Write((const void*)&header, sizeof(header));

		mpIO->Write((const void*)pBuffer->GetData(), pBuffer->GetDataSize());
	}
}

void CModeService::WritePicture(CBuffer *pBuffer)
{
	if (mpIO) {
		upload_header_v2_t header;

		header.u_data_type = VDB_DATA_JPEG;
		header.u_data_size = pBuffer->GetDataSize();
		header.u_timestamp = CalcTimeStamp(pBuffer);
		header.u_stream = 0;
		header.u_duration = 0;	// todo
		header.u_version = UPLOAD_VERSION_v2;
		header.u_flags = 0;
		header.u_param1 = 0;
		header.u_param2 = 0;
		mpIO->Write((const void*)&header, sizeof(header));

		mpIO->Write((const void*)pBuffer->GetData(), pBuffer->GetDataSize());
	}
}

void CModeService::GetFilename(char *buffer, int len, int index)
{
	if (mp_file_path == NULL) {
		char path[REG_STR_MAX];
		const char *name = m_mode == MODE_TS ? NAME_MMS_PATH_MODE1 : NAME_MMS_PATH_MODE2;
		const char *value = m_mode == MODE_TS ? VALUE_MMS_PATH_MODE1 : VALUE_MMS_PATH_MODE2;
		mpEngine->ReadRegString(name, m_index, path, value);
		mp_file_path = avf_strdup(path);
	}
	avf_snprintf(buffer, len, "%s%d.mmf", mp_file_path, index);	// /tmp/1.mmf, 2.mmf, ...
	buffer[len-1] = 0;
}

avf_u64_t CModeService::CalcTimeStamp(CBuffer *pBuffer)
{
	avf_u64_t timestamp_ms = pBuffer->pts_to_ms(pBuffer->m_pts);
	return timestamp_ms > m_start_ms ? timestamp_ms - m_start_ms : 0;
}

//-----------------------------------------------------------------------
//
//  CMMServer
//
//-----------------------------------------------------------------------
IFilter* CMMServer::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CMMServer *result = avf_new CMMServer(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CMMServer::CMMServer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName)
{
	SET_ARRAY_NULL(mp_av_input);
}

CMMServer::~CMMServer()
{
	SAFE_RELEASE_ARRAY(mp_av_input);
}

avf_status_t CMMServer::InitFilter()
{
	for (avf_uint_t i = 0; i < NUM_AV; i++) {
		mp_av_input[i] = avf_new CMMServerAVInput(this, i);
		CHECK_STATUS(mp_av_input[i]);
		if (mp_av_input[i] == NULL)
			return E_NOMEM;
	}

	return E_OK;
}

avf_status_t CMMServer::PreRunFilter()
{
	inherited::PreRunFilter();
	for (avf_uint_t i = 0; i < NUM_AV; i++) {
		avf_status_t status = mp_av_input[i]->CheckStreams();
		if (status != E_OK) {
			//AVF_LOGE("PreRunFilter failed");
			//return status;
		}
	}
	return E_OK;
}

IPin *CMMServer::GetInputPin(avf_uint_t index)
{
	return index < NUM_AV ? mp_av_input[index] : NULL;
}

void CMMServer::OnEOS(avf_uint_t pin_index)
{
	AUTO_LOCK(mMutex);

	if (pin_index < NUM_AV) {
		mp_av_input[pin_index]->SetEOS();
	}

	for (avf_uint_t i = 0; i < NUM_AV; i++) {
		if (!mp_av_input[i]->GetEOS())
			return;
	}

	PostEosMsg();
}

//-----------------------------------------------------------------------
//
//  CMMServerAVInput
//
//-----------------------------------------------------------------------

CMMServerAVInput::CMMServerAVInput(CMMServer *pFilter, avf_uint_t index):
	inherited(pFilter, index, "MMS-av-Input"),
	mpService1(NULL),
	mpService2(NULL)
{
	CREATE_OBJ(mpService1 = CModeService::Create(pFilter, pFilter->mpEngine, index, CModeService::MODE_TS));
	CREATE_OBJ(mpService2 = CModeService::Create(pFilter, pFilter->mpEngine, index, CModeService::MODE_FRAME));
}

CMMServerAVInput::~CMMServerAVInput()
{
	avf_safe_release(mpService1);
	avf_safe_release(mpService2);
}

avf_status_t CMMServerAVInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (pMediaFormat->mFormatType != MF_AVES) {
		AVF_LOGE("cannot accept aves format");
		return E_ERROR;
	}

	CESFormat *format = (CESFormat*)pMediaFormat;
	mpService1->AcceptMedia(format);
	mpService2->AcceptMedia(format);

	return E_OK;
}

void CMMServerAVInput::SetMediaFormat(CMediaFormat *pMediaFormat)
{
	inherited::SetMediaFormat(pMediaFormat);
	if (pMediaFormat == NULL) {
		mpService1->ClearMediaFormat();
		mpService2->ClearMediaFormat();
	}
}

avf_status_t CMMServerAVInput::CheckStreams()
{
	avf_status_t status;

	if ((status = mpService1->CheckStreams()) != E_OK)
		return status;

	if ((status = mpService2->CheckStreams()) != E_OK)
		return status;

	return E_OK;
}

void CMMServerAVInput::ResetState()
{
	inherited::ResetState();
	mpService1->ResetState();
	mpService2->ResetState();
}

void CMMServerAVInput::ReceiveBuffer(CBuffer *pBuffer)
{
	switch (pBuffer->mType) {
	case CBuffer::DATA:
		if (pBuffer->mpFormat->mFormatType == MF_H264VideoFormat && (pBuffer->mFlags & CBuffer::F_AVC_IDR)) {
			mpService1->CheckService(pBuffer);
			mpService2->CheckService(pBuffer);
		}

		mpService1->ProcessBuffer(pBuffer);
		mpService2->ProcessBuffer(pBuffer);

		break;

	case CBuffer::EOS:
		mpService1->OnEOS();
		mpService2->OnEOS();
		((CMMServer*)mpFilter)->OnEOS(mPinIndex);
		break;

	default:
		break;
	}
}


