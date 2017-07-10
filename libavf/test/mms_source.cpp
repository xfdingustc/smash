
#define LOG_TAG	"mms_source"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_socket.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "filemap.h"

#include "mms_if.h"

//{8789E4F8-6618-4B8A-8281-752420D53895}
AVF_DEFINE_IID(IID_IMMSource,
0x8789E4F8, 0x6618, 0x4B8A, 0x82, 0x81, 0x75, 0x24, 0x20, 0xD5, 0x38, 0x95);

//-----------------------------------------------------------------------
//
// CMMSource
//
//-----------------------------------------------------------------------
class CMMSource: public CObject, public IMMSource
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

public:
	static CMMSource* Create(const char *path, int mode);

private:
	CMMSource(const char *path, int mode);
	virtual ~CMMSource();

// IMMSource
public:
	virtual int GetFileNo() {
		return mpEvent->GetFileNo();
	}
	virtual int Read(uint8_t *buf, uint32_t size, FrameInfo *pFrameInfo);
	virtual void Close();
	virtual avf_status_t UpdateWrite(int write_index, uint32_t write_pos, uint32_t framecount);

private:
	void Reset();

	INLINE void WakeUp() {
		if (m_mode == MODE_TS) {
			if (!mbEventSet) {
				mbEventSet = true;
				mpEvent->Interrupt();
			}
		} else {	// MODE_FRAME
			mCondData.Wakeup();
		}
	}

	INLINE void ClearEvent() {
		if (mbEventSet) {
			mbEventSet = false;
			mpEvent->Reset();
		}
	}

private:
	avf_status_t GotoLastFile();
	avf_status_t GotoNextFrame();

private:
	const char *const mp_path;
	const int m_mode;

private:
	bool mbClosed;
	CMutex mMutex;
	CCondition mCondData;	// data read condition, for MODE_FRAME
	CSocketEvent *mpEvent;	// for MODE_TS
	bool mbEventSet;

private:
	CFileMap *mpFileMap;
	const uint8_t *mpFilePtr;
	uint32_t m_map_end;

private:
	ap<string_t> mFileName;
	uint32_t m_filesize;
	uint32_t m_frame_nread;		// 
	uint32_t m_frame_remain;	// to be read
	bool mb_started;

	int m_read_index;	// 1,2,...
	uint32_t m_read_pos;

	int m_write_index;	// 1,2,...
	uint32_t m_write_pos;
	uint32_t m_frame_count;

	upload_header_v2_t* mp_frame_header;
};

//-----------------------------------------------------------------------

extern "C" IMMSource* mms_create_source(const char *path, int mode)
{
	return CMMSource::Create(path, mode);
}

//-----------------------------------------------------------------------
//
// CMMSource
//
//-----------------------------------------------------------------------

void *CMMSource::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMMSource)
		return static_cast<IMMSource*>(this);
	return inherited::GetInterface(refiid);
}

CMMSource* CMMSource::Create(const char *path, int mode)
{
	CMMSource *result = avf_new CMMSource(path, mode);
	CHECK_STATUS(result);
	return result;
}

CMMSource::CMMSource(const char *path, int mode):
	mp_path(path),
	m_mode(mode),
	mbClosed(false),
	mpEvent(NULL),
	mbEventSet(false),
	mpFileMap(NULL),
	mpFilePtr(NULL),
	m_map_end(0)
{
	if (m_mode == MODE_TS) {
		CREATE_OBJ(mpEvent = CSocketEvent::Create("mmsource"));
	}
	CREATE_OBJ(mpFileMap = CFileMap::Create());
	Reset();
}

CMMSource::~CMMSource()
{
	avf_safe_release(mpFileMap);
	avf_safe_release(mpEvent);
}

void CMMSource::Reset()
{
	AUTO_LOCK(mMutex);

	mpFileMap->Close();
	mpFilePtr = NULL;
	m_map_end = 0;

	m_filesize = 0;
	m_frame_nread = 0;
	m_frame_remain = 0;
	mb_started = false;

	m_read_index = 1;
	m_read_pos = 0;

	m_write_index = 0;
	m_write_pos = 0;
	m_frame_count = 0;
}

// unblock the reader, let subsequent Read() fail
void CMMSource::Close()
{
	AUTO_LOCK(mMutex);
	if (!mbClosed) {
		mbClosed = true;
		WakeUp();
	}
}

avf_status_t CMMSource::UpdateWrite(int write_index, uint32_t write_pos, uint32_t framecount)
{
	AUTO_LOCK(mMutex);

	m_write_index = write_index;
	m_write_pos = write_pos;
	m_frame_count = framecount;

	WakeUp();

	return E_OK;
}

// MODE_TS: read as many bytes as possible, do not block
// MODE_FRAME: return >= 0 on success
int CMMSource::Read(uint8_t *buf, uint32_t size, FrameInfo *pFrameInfo)
{
	AUTO_LOCK(mMutex);

Start:
	if (mbClosed) {
		AVF_LOGD("Read after Close()");
		return E_CLOSE;
	}

	avf_status_t status;
	uint32_t filesize;

ThisFrame:
	if (m_frame_remain > 0) {
		if (m_mode == MODE_TS) {
			uint32_t toread = MIN(m_frame_remain, size);
			if (m_mode == MODE_TS) {
				toread = toread / 188 * 188;	// packet aligned
			}

			if (toread > 0) {
				::memcpy(buf, mpFilePtr, toread);
				mpFilePtr += toread;

				m_frame_nread += toread;
				m_frame_remain -= toread;

				pFrameInfo->timestamp_ms = mp_frame_header->u_timestamp;
				pFrameInfo->duration_ms = mp_frame_header->u_duration;
				pFrameInfo->new_frame = m_frame_nread == 0;
				pFrameInfo->data_type = mp_frame_header->u_data_type;

				return (int)toread;
			}

		} else {	// MODE_FRAME

			pFrameInfo->timestamp_ms = mp_frame_header->u_timestamp;
			pFrameInfo->duration_ms = mp_frame_header->u_duration;
			pFrameInfo->new_frame = 1;
			pFrameInfo->data_type = mp_frame_header->u_data_type;
			pFrameInfo->pframe = mpFilePtr;
			pFrameInfo->frame_size = m_frame_remain;

			mpFilePtr += m_frame_remain;
			m_frame_nread += m_frame_remain;
			m_frame_remain = 0;

			return (int)pFrameInfo->frame_size;
		}
	}

ThisFile:
	if (m_read_pos < m_filesize) {
		if ((status = GotoNextFrame()) == E_OK) {
			goto ThisFrame;
		}
		// no valid frame
	}

ExtendFile:
	if (mpFileMap->IsOpen()) {
		if (m_write_index == m_read_index && m_write_pos > m_filesize) {
			// this file increased
			m_filesize = m_write_pos;
			goto ThisFile;
		}

		if (m_write_index > m_read_index) {
			filesize = mpFileMap->UpdateSize();
			if (m_filesize < filesize) {
				m_filesize = filesize;
				goto ThisFile;
			}
		} else {
			// wait this file
			goto NoData;
		}

		// this file is done, goto next
		mpFileMap->Close();

		AVF_LOGD(C_GREEN "remove %s" C_NONE, mFileName->string());
		if (!avf_remove_file(mFileName->string(), false)) {
			AVF_LOGD("cannot remove %s", mFileName->string());
		}
	}

	// incoming data in next file
	if ((status = GotoLastFile()) != E_OK) {
		// wait next file
		goto NoData;
	}

	goto ExtendFile;

NoData:
	if (m_mode == MODE_TS) {
		ClearEvent();
		return 0;
	} else {
		mCondData.Wait(mMutex);
		goto Start;
	}
}

avf_status_t CMMSource::GotoLastFile()
{
	if (m_write_index == 0) {
		//AVF_LOGD("no file");
		return E_ERROR;
	}

	if (!mb_started && m_frame_count != 1) {
		// start from the last I frame to reduce latency
		return E_ERROR;
	}

	char filename[256];
	avf_snprintf(filename, sizeof(filename), "%s%d.mmf", mp_path, m_write_index);	// todo
	filename[sizeof(filename)-1] = 0;

	mFileName = filename;
	avf_status_t status = mpFileMap->OpenFileRead(filename);

	if (status != E_OK) {
		AVF_LOGD("cannot open %s, write_index: %d", filename, m_write_index);
		return E_ERROR;
	}

	mpFilePtr = NULL;
	m_map_end = 0;

	AVF_LOGD("%s is open", filename);

	m_read_index = m_write_index;
	m_filesize = 0;
	m_read_pos = 0;
	m_frame_nread = 0;
	m_frame_remain = 0;
	mb_started = true;

	return E_OK;
}

avf_status_t CMMSource::GotoNextFrame()
{
	// map to file end
	if (m_map_end < m_filesize) {
//		AVF_LOGI("map %d - %d", m_read_pos, m_filesize);
		mpFilePtr = mpFileMap->Map(m_read_pos, m_filesize - m_read_pos);
		if (mpFilePtr == NULL) {
			AVF_LOGE("map failed, skip file %s", mFileName->string());
			m_frame_nread = m_frame_remain;
			m_frame_remain = 0;
			m_read_pos = INT32_MAX;
			return E_IO;
		}
		m_map_end = m_filesize;
	}

	// search for a valid frame
	while (m_read_pos < m_filesize) {
		avf_uint_t size = sizeof(upload_header_v2_t);

		mp_frame_header = (upload_header_v2_t*)mpFilePtr;
		mpFilePtr += size;

		m_read_pos += size + mp_frame_header->u_data_size;

		int data_type = mp_frame_header->u_data_type;
		if (m_mode == MODE_TS) {
			if (data_type == VDB_TS_FRAME) {
				if ((mp_frame_header->u_data_size % 188) != 0) {
					AVF_LOGW("bad frame size %d", m_frame_remain);
				}
				m_frame_nread = 0;
				m_frame_remain = mp_frame_header->u_data_size;
				return E_OK;
			}
		} else {	// MODE_FRAME
			m_frame_nread = 0;
			m_frame_remain = mp_frame_header->u_data_size;
			return E_OK;
		}
	}

	// no valid data
	return E_ERROR;
}

