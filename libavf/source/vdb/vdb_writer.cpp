
#define LOG_TAG "vdb_writer"

#include <fcntl.h>
#include <sys/stat.h>

#ifndef MINGW
#ifdef MAC_OS
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#endif
#endif

#include <dirent.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "rbtree.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avio.h"
#include "buffer_io.h"
#include "sys_io.h"
#include "file_io.h"
#include "mpeg_utils.h"

#include "avf_util.h"
#include "mem_stream.h"

#include "vdb_cmd.h"
#include "vdb_util.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_format.h"
#include "vdb_if.h"
#include "vdb.h"
#include "vdb_server.h"

//-----------------------------------------------------------------------
//
//  CClipWriter
//
//-----------------------------------------------------------------------

IClipWriter *CVDB::CreateClipWriter()
{
	AUTO_LOCK(mMutex);
	IClipWriter *writer = CClipWriter::Create(this);
	return writer;
}

CClipWriter *CClipWriter::Create(CVDB *pVDB)
{
	CClipWriter *result = avf_new CClipWriter(pVDB);
	CHECK_STATUS(result);
	return result;
}

CClipWriter::CClipWriter(CVDB *pVDB):
	mpVDB(pVDB),
	mbClosed(true),
	mState(STATE_IDLE),
	m_cb_proc(NULL),
	m_v_length(2*1000*60),
	m_p_length(10*1000*60),
	m_r_length(30*1000*60),
	mp_picture_io(NULL),
	mp_raw_mem(NULL),
	mpFormat(NULL)
{
	mpVDB->AddRef();
	SET_ARRAY_NULL(mp_video_io);
	HLIST_INSERT(mpVDB->mpFirstClipWriter, this);
	mbClosed = false;

	// for jpeg
	CREATE_OBJ(mpFormat = avf_alloc_media_format(CMJPEGVideoFormat));
	mpFormat->mMediaType = MEDIA_TYPE_VIDEO;
	mpFormat->mFormatType = MF_MJPEGVideoFormat;
	mpFormat->mFrameDuration = 0;
	mpFormat->mTimeScale = 1000;
	mpFormat->mFrameRate = FrameRate_Unknown;
	mpFormat->mWidth = 0;	// todo
	mpFormat->mHeight = 0;	// todo
}

CClipWriter::~CClipWriter()
{
	avf_safe_free(mp_raw_mem);
	avf_safe_release(mp_picture_io);
	SAFE_RELEASE_ARRAY(mp_video_io);
	mpVDB->Release();
	avf_safe_release(mpFormat);
}

void CClipWriter::CloseL()
{
	if (!mbClosed) {
		HLIST_REMOVE(this);
		mbClosed = true;
	}
}

void CClipWriter::Release()
{
	if (avf_atomic_dec(&m_ref_count) == 1) {
		AUTO_LOCK(mpVDB->mMutex);
		CloseL();
		avf_delete this;
	}
}

void *CClipWriter::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IClipWriter)
		return static_cast<IClipWriter*>(this);
	return inherited::GetInterface(refiid);
}

void CClipWriter::SetCallback(void (*cb_proc)(void *context, int cbid, 
	void *param, int size), void *context)
{
	AUTO_LOCK(mMutex);
	m_cb_proc = cb_proc;
	m_cb_context = context;
}

void CClipWriter::SetSegmentLength(avf_uint_t v_length, avf_uint_t p_length, avf_uint_t r_length)
{
	AUTO_LOCK(mMutex);
	m_v_length = v_length;
	m_p_length = p_length;
	m_r_length = r_length;
}

avf_status_t CClipWriter::StartRecord(int ch)
{
	AUTO_LOCK(mMutex);

	if (mState != STATE_IDLE) {
		AVF_LOGE("bad state %d", mState);
		return E_STATE;
	}

	CVDB::vdb_record_info_t info;

	avf_status_t status = mpVDB->StartRecord(&info, ch);
	if (status != E_OK)
		return status;

	mbEnd = false;
	m_ch = ch;
	m_num_items = 0;
	NextItem();

	m_video_fpos[0] = 0;
	m_video_fpos[1] = 0;

	m_seg_length[0] = 0;
	m_seg_length[1] = 0;

	m_video_samples[0] = 0;
	m_video_samples[1] = 0;
	m_data_samples = 0;

	if (info.valid && m_cb_proc) {
		m_cb_proc(m_cb_context, CB_ID_StartRecord, (void*)&info, sizeof(info));
	}

	return E_OK;
}

avf_status_t CClipWriter::StopRecord()
{
	AUTO_LOCK(mMutex);

	if (mState == STATE_IDLE)
		return E_OK;

	if (mState != STATE_END) {
		if (mState != STATE_WAIT_HEADER || m_remain_bytes != 0) {
			AVF_LOGW("StopRecord: not finished, state: %d, read: %d, remain: %d",
				mState, m_read_bytes, m_remain_bytes);
		}
	}

	CVDB::vdb_record_info_t info;

	avf_status_t status = mpVDB->StopRecord(&info, m_ch);
	if (status != E_OK) {
		AVF_LOGE("StopRecord failed");
	}

	avf_safe_release(mp_video_io[0]);
	avf_safe_release(mp_video_io[1]);

	if (info.valid && m_cb_proc) {
		m_cb_proc(m_cb_context, CB_ID_StopRecord, (void*)&info, sizeof(info));
	}

	mState = STATE_IDLE;
	return E_OK;
}

avf_status_t CClipWriter::WriteData(const avf_u8_t *data, avf_size_t size)
{
	AUTO_LOCK(mMutex);

	while (size > 0) {
		switch (mState) {
		case STATE_IDLE:
			AVF_LOGE("bad state %d", mState);
			return E_STATE;

		case STATE_WAIT_HEADER: {
				avf_uint_t tocopy = MIN(m_remain_bytes, size);
				avf_u8_t *ptr = (avf_u8_t*)&m_header + (sizeof(upload_header_v2_t) - m_remain_bytes);
				::memcpy(ptr, data, tocopy);

				data += tocopy;
				size -= tocopy;

				m_read_bytes += tocopy;
				m_remain_bytes -= tocopy;

				if (m_remain_bytes == 0) {
					// check version
					if (m_header.u_version != UPLOAD_VERSION && m_header.u_version != UPLOAD_VERSION_v2) {
						AVF_LOGE("unknown version %d", m_header.u_version);
						mState = STATE_ERROR;
						return E_ERROR;
					}

					if (m_header.u_version == UPLOAD_VERSION) {
						if (m_header.u_data_type == VDB_DATA_VIDEO || m_header.u_data_type == VDB_DATA_JPEG) {
							m_header.u_timestamp /= 90;
							m_header.u_duration /= 90;
						}
					}

					m_read_bytes = 0;
					m_remain_bytes = m_header.u_data_size;

					switch (m_header.u_data_type) {
					case VDB_DATA_VIDEO:
						if (m_header.u_stream > 1) {
							AVF_LOGE("bad stream %d", m_header.u_stream);
							mState = STATE_ERROR;
							return E_ERROR;
						}
						mState = STATE_WRITE_STREAM_ATTR;
						break;

					case VDB_DATA_JPEG:
						mState = STATE_WRITE_PICTURE;
						break;

					case RAW_DATA_GPS:
					case RAW_DATA_OBD:
					case RAW_DATA_ACC:
						mState = STATE_WRITE_RAW;
						break;

					case __UPLOAD_END:
						mpVDB->SegVideoLength(m_header.u_timestamp, m_video_fpos[0], 0, m_ch);
						mpVDB->SegVideoLength(m_header.u_timestamp, m_video_fpos[1], 1, m_ch);
						mState = m_remain_bytes ? STATE_WRITE_OTHER : STATE_END;
						mbEnd = true;
						break;

					case __UPLOAD_INFO:
					default:
						mState = STATE_WRITE_OTHER;
						break;
					}
				}
			}
			break;

		case STATE_WRITE_STREAM_ATTR: {
				avf_uint_t tocopy = sizeof(avf_stream_attr_t) - m_read_bytes;
				if (tocopy > size)
					tocopy = size;

				::memcpy((avf_u8_t*)&m_stream_attr, data, tocopy);
				data += tocopy;
				size -= tocopy;

				m_read_bytes += tocopy;
				m_remain_bytes -= tocopy;

				if (m_read_bytes == sizeof(avf_stream_attr_t)) {
					mState = STATE_WRITE_VIDEO;
				}
			}
			break;

		case STATE_WRITE_VIDEO: {
				int n = write_video(data, size);
				if (n < 0) {
					mState = STATE_ERROR;
					return (avf_status_t)n;
				}
				data += n;
				size -= n;
			}
			break;

		case STATE_WRITE_PICTURE: {
				int n = write_picture(data, size);
				if (n < 0) {
					mState = STATE_ERROR;
					return (avf_status_t)n;
				}
				data += n;
				size -= n;
			}
			break;

		case STATE_WRITE_RAW: {
				int n = write_raw(data, size);
				if (n < 0) {
					mState = STATE_ERROR;
					return (avf_status_t)n;
				}
				data += n;
				size -= n;
			}
			break;

		case STATE_WRITE_OTHER: {
				int n = MIN(size, m_remain_bytes);
				data += n;
				size -= n;
				m_read_bytes += n;
				m_remain_bytes -= n;
				if (m_remain_bytes == 0) {
					NextItem();
					if (mbEnd) {
						mState = STATE_END;
					}
				}
			}
			break;

		case STATE_END:
			AVF_LOGW("record already end");
			return E_ERROR;

		default:
			AVF_LOGE("bad state %d", mState);
			return E_ERROR;
		}
	}

	return E_OK;
}

IAVIO *CClipWriter::StartVideo()
{
	avf_status_t status = mpVDB->InitVideoStream(&m_stream_attr, VIDEO_TYPE_TS, m_header.u_stream, m_ch);
	if (status != E_OK)
		return NULL;

	string_t *video_name;
	avf_time_t video_gen_time;

	video_name = mpVDB->CreateVideoFileName(&video_gen_time, m_header.u_stream, false, m_ch);
	if (video_name == NULL)
		return NULL;

	IAVIO *io = CBufferIO::Create();
	if (io == NULL)
		goto Error;

	status = io->CreateFile(video_name->string());
	if (status != E_OK)
		goto Error;

	status = mpVDB->StartVideo(video_gen_time, m_header.u_stream, m_ch);
	if (status != E_OK)
		goto Error;

	video_name->Release();
	return io;

Error:
	avf_remove_file(video_name->string(), false);
	video_name->Release();
	avf_safe_release(io);
	return io;
}

avf_status_t CClipWriter::FinishSegment(int stream)
{
	avf_status_t status;

	status = mpVDB->FinishSegment(0, 0, stream, true, m_ch);
	if (status != E_OK)
		return status;

	m_video_fpos[stream] = 0;
	m_seg_length[stream] = 0;
	m_video_samples[stream] = 0;

	avf_safe_release(mp_video_io[stream]);

	if (stream == 0) {
		m_data_samples = 0;
		avf_safe_release(mp_picture_io);
	}

	return E_OK;
}

// return bytes written, or < 0 if error
int CClipWriter::write_video(const avf_u8_t *data, avf_size_t size)
{
	int stream = m_header.u_stream;
	IAVIO *io;

Start:
	io = mp_video_io[stream];
	if (io == NULL) {
		io = mp_video_io[stream] = StartVideo();
		if (io == NULL) {
			return E_IO;
		}
		m_video_fpos[stream] = 0;
	} else if (m_read_bytes == sizeof(avf_stream_attr_t)) {
		if (m_video_fpos[stream] > 1500*MB || m_seg_length[stream] >= m_v_length) {
			mpVDB->SegVideoLength(m_header.u_timestamp, m_video_fpos[stream], stream, m_ch);
			avf_status_t status = FinishSegment(stream);
			if (status != E_OK) {
				return status;
			}
			goto Start;
		}
	}

	avf_uint_t towrite = MIN(size, m_remain_bytes);
	int rval = io->Write(data, towrite);
	if (rval != (int)towrite) {
		return rval;
	}

	m_read_bytes += towrite;
	m_remain_bytes -= towrite;

	if (m_remain_bytes == 0) {
		avf_status_t status = mpVDB->AddVideo(m_header.u_timestamp, 1000, 
			m_video_fpos[stream], stream, m_ch, m_seg_length + stream);
		if (status != E_OK) {
			return E_ERROR;
		}
		m_video_samples[stream]++;
		NextItem();
		m_video_fpos[stream] = io->GetSize();
	}

	return towrite;
}

IAVIO* CClipWriter::StartPicture()
{
	string_t *picture_name;
	avf_time_t picture_gen_time;
	avf_status_t status;

	picture_name = mpVDB->CreatePictureFileName(&picture_gen_time, 0, false, m_ch);
	if (picture_name == NULL)
		return NULL;

	IAVIO *io = CBufferIO::Create();
	if (io == NULL)
		goto Error;

	status = io->CreateFile(picture_name->string());
	if (status != E_OK)
		goto Error;

	status = mpVDB->StartPicture(picture_gen_time, 0, m_ch);
	if (status != E_OK)
		goto Error;

	picture_name->Release();
	return io;

Error:
	avf_remove_file(picture_name->string(), false);
	picture_name->Release();
	avf_safe_release(io);
	return NULL;
}

// return bytes written, or < 0 if error
int CClipWriter::write_picture(const avf_u8_t *data, avf_size_t size)
{
	IAVIO *io;

Start:
	io = mp_picture_io;
	if (io == NULL) {
		io = mp_picture_io = StartPicture();
		if (io == NULL) {
			return E_OK;
		}
	} else if (m_read_bytes == 0) {
		if (m_seg_length[0] >= m_p_length) {
			if (m_video_samples[0] > 0) {
				AVF_LOGE("error");
				return E_ERROR;
			}
			avf_status_t status = FinishSegment(0);
			if (status != E_OK) {
				return status;
			}
			goto Start;
		}
	}

	avf_uint_t towrite = MIN(size, m_remain_bytes);
	int rval = io->Write(data, towrite);
	if (rval != (int)towrite) {
		return rval;
	}

	m_read_bytes += towrite;
	m_remain_bytes -= towrite;

	if (m_remain_bytes == 0) {
		CBuffer buffer;

		buffer.mRefCount = 1;
		buffer.mpPool = NULL;
		buffer.mpNext = NULL;
		buffer.mpFormat = mpFormat;

		buffer.mbNeedFree = 0;
		buffer.mType = CBuffer::DATA;
		buffer.frame_type = 0;
		buffer.reserved = 0;
		buffer.mFlags = 0;

		buffer.mpData = NULL;
		buffer.mSize = m_header.u_data_size;

		buffer.m_offset = 0;
		buffer.m_data_size = buffer.mSize;

		buffer.m_dts = buffer.m_pts = m_header.u_timestamp;
		buffer.m_duration = 0;

		avf_status_t status = mpVDB->AddPicture(&buffer, 0, m_ch, m_seg_length);
		if (status != E_OK) {
			return E_ERROR;
		}

		m_data_samples++;
		NextItem();
	}

	return towrite;
}

// return bytes written, or < 0 if error
int CClipWriter::write_raw(const avf_u8_t *data, avf_size_t size)
{
	if (m_read_bytes == 0) {
		if (mp_raw_mem == NULL || m_raw_size < m_remain_bytes) {
			avf_u8_t *mem = avf_malloc_bytes(m_remain_bytes);
			if (mem == NULL) {
				return E_NOMEM;
			}
			avf_safe_free(mp_raw_mem);
			mp_raw_mem = mem;
			m_raw_size = m_header.u_data_size;
		}

		if (m_seg_length[0] >= m_r_length) {
			if (m_video_samples[0] > 0) {
				AVF_LOGE("error");
				return E_ERROR;
			}
			avf_status_t status = FinishSegment(0);
			if (status != E_OK) {
				return status;
			}
		}
	}

	avf_uint_t tocopy = MIN(size, m_remain_bytes);
	::memcpy(mp_raw_mem + m_read_bytes, data, tocopy);

	m_read_bytes += tocopy;
	m_remain_bytes -= tocopy;

	if (m_remain_bytes == 0) {

		avf_u32_t fcc = 0;
		switch (m_header.u_data_type) {
		case RAW_DATA_GPS: fcc = IRecordObserver::GPS_DATA; break;
		case RAW_DATA_OBD: fcc = IRecordObserver::OBD_DATA; break;
		case RAW_DATA_ACC: fcc = IRecordObserver::ACC_DATA; break;
		default: break;
		}

		if (fcc) {
			raw_data_t *raw = avf_alloc_raw_data(m_header.u_data_size, fcc);
			if (raw == NULL) {
				return E_NOMEM;
			}

			raw->ts_ms = m_header.u_timestamp;
			::memcpy(raw->GetData(), mp_raw_mem, m_header.u_data_size);
			avf_status_t status = mpVDB->AddRawData(raw, 0, m_ch, m_seg_length);
			raw->Release();
			if (status != E_OK) {
				return status;
			}

			m_data_samples++;

			if (m_cb_proc && m_header.u_data_type == RAW_DATA_GPS) {
				m_cb_proc(m_cb_context, CB_ID_GPS_DATA, (void*)mp_raw_mem, m_header.u_data_size);
			}
		}

		NextItem();
	}

	return tocopy;
}

