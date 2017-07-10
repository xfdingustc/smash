
#ifndef __AVF_BASE_IF_H__
#define __AVF_BASE_IF_H__

#include "list.h"
#include "avf_new.h"

// interface IDs
extern avf_iid_t IID_IAllocMem;
extern avf_iid_t IID_IActiveObject;
extern avf_iid_t IID_IBufferPool;
extern avf_iid_t IID_IEngine;
extern avf_iid_t IID_ITimerManager;
extern avf_iid_t IID_ITimerTarget;
extern avf_iid_t IID_ITimerReceiver;
extern avf_iid_t IID_IMediaPlayer;
extern avf_iid_t IID_IPlayback;
extern avf_iid_t IID_IFilter;
extern avf_iid_t IID_IPin;
extern avf_iid_t IID_IMediaRenderer;

extern avf_iid_t IID_IAVIO;
extern avf_iid_t IID_IBlockIO;
extern avf_iid_t IID_ITSMapInfo;

extern avf_iid_t IID_IMediaFileReader;
extern avf_iid_t IID_IReadProgress;
extern avf_iid_t IID_IMediaTrack;
extern avf_iid_t IID_IRecordObserver;
extern avf_iid_t IID_IVdbInfo;
extern avf_iid_t IID_IClipReader;
extern avf_iid_t IID_IClipWriter;
extern avf_iid_t IID_ISeekToTime;
extern avf_iid_t IID_IClipData;

extern avf_iid_t IID_ISegmentSaver;
extern avf_iid_t IID_IMediaFileWriter;
extern avf_iid_t IID_ISampleProvider;

// interfaces
class IAllocMem;
class IActiveObject;
class IBufferPool;
class IEngine;
class ITimerManager;
class ITimerTarget;
class ITimerReceiver;
class IMediaPlayer;
class IFilter;
class IPin;
class IAVIO;
class CMemBuffer;

class IMediaFileReader;
class IReadProgress;
class IMediaTrack;
class IRecordObserver;
class IVdbInfo;
class IClipReader;
class IClipWriter;
class ISeekToTime;
class IClipData;
class ITSMapInfo;

class ISegmentSaver;
class IMediaFileWriter;
class ISampleProvider;

class CMsgQueue;

// structs
class CBuffer;
class CMediaFormat;

struct raw_data_t;
struct media_sample_t;
struct raw_sample_t;
struct media_block_t;
struct avf_stream_attr_s;

// 1. for vdb clip, id is the clipinfo filename (e.g. 0x14a48a1c)
// 2. buffered ref clip id is same with the vdb clip id (server mode ony)
// 3. for marked/playlist ref clip, id is its clip's id + ref_start_time + incre (e.g. 0x54e454d8)
typedef avf_time_t clip_id_t;
#define INVALID_CLIP_ID	0

//-----------------------------------------------------------------------
//
// TDynamicArray
//
//-----------------------------------------------------------------------
#define ARRAY_MAGIC	0xFEFDFCFB
template <typename ET>
struct TDynamicArray
{
	ET *elems;
	avf_size_t count;
	avf_size_t _cap;

	INLINE void _Init() {
		elems = NULL;
		count = 0;
		_cap = 0;
	}

	INLINE void _Release() {
		if (elems) {
#ifdef AVF_DEBUG
			ET *tail = elems + _cap;
			if (*(uint32_t*)tail != ARRAY_MAGIC) {
				AVF_LOGE("array corrupted");
			}
#endif
			avf_free(elems);
			_Init();
		}
	}

	INLINE void _Clear() {
		count = 0;
	}

	INLINE avf_size_t _Avail() {
		return _cap - count;
	}

	INLINE ET *Item(int index) {
		return elems + index;
	}

	INLINE ET *TailItem() {
		return elems + count;
	}

	bool _DoResize(avf_size_t inc, avf_size_t extra) {
		if (inc == 0) {
			inc = _cap / 2;
			if (inc == 0) {
				inc = 2;
			}
		}
		avf_size_t new_cap = _cap + inc + extra;
#ifdef AVF_DEBUG
		ET *new_elems = avf_malloc_array(ET, new_cap + 1);
		ET *tail = new_elems + new_cap;
		*(uint32_t*)tail = ARRAY_MAGIC;
#else
		ET *new_elems = avf_malloc_array(ET, new_cap);
#endif
		if (new_elems == NULL) {
			return false;
		}
		if (elems) {
#ifdef AVF_DEBUG
			ET *tail = elems + _cap;
			if (*(uint32_t*)tail != ARRAY_MAGIC) {
				AVF_LOGE("array corrupted");
			}
#endif
			::memcpy(new_elems, elems, count * sizeof(ET));
			avf_free(elems);
		}
		_cap = new_cap;
		elems = new_elems;
		return true;
	}

	INLINE bool _MakeMinSize(avf_size_t min_size) {
		return _cap >= min_size ? true : _DoResize(min_size - _cap, 0);
	}

	INLINE bool _IncOne(avf_size_t inc, avf_size_t extra) {
		return _cap >= 1 + extra + count ? true : _DoResize(inc, 1 + extra);
	}

	// append at the tail of array, resize the array if needed
	// extra > 0: always reserve extra items at the tail
	ET* _Append(const ET *elem, avf_size_t inc = 0, avf_size_t extra = 0) {
		if (!_IncOne(inc, extra)) {
			return NULL;
		}
#ifdef AVF_DEBUG
		ET *tail = elems + _cap;
		if (*(uint32_t*)tail != ARRAY_MAGIC) {
			AVF_LOGE("array corrupted");
		}
#endif
		ET *this_elem = elems + count;
		count++;
		if (elem) {
			*this_elem = *elem;
		}
		return this_elem;
	}

	ET *_Insert(const ET *elem, int index, avf_size_t inc = 0, avf_size_t extra = 0) {
		if (!_IncOne(inc, extra)) {
			return NULL;
		}
		for (int j = count; j > index; j--) {
			elems[j] = elems[j - 1];
		}
		count++;
		ET *this_elem = elems + index;
		if (elem) {
			*this_elem = *elem;
		}
		return this_elem;
	}

	void _Remove(avf_uint_t i) {
		count--;
		for (avf_size_t j = i; j < count; j++) {
			elems[j] = elems[j + 1];
		}
	}

	// remove the elem from array
	bool _Remove(const ET& elem) {
		for (avf_size_t i = 0; i < count; i++) {
			if (elem == elems[i]) {
				_Remove(i);
				return true;
			}
		}
		return false;
	}
};

//-----------------------------------------------------------------------
//
// IAllocMem
//
//-----------------------------------------------------------------------
class IAllocMem: public IInterface
{
public:
	DECLARE_IF(IAllocMem, IID_IAllocMem);

	virtual void *AllocMem(avf_size_t size) = 0;
	virtual void FreeMem(void *ptr, avf_size_t size) = 0;
};

//-----------------------------------------------------------------------
//
// raw data
//
//-----------------------------------------------------------------------
struct raw_data_t
{
	avf_atomic_t mRefCount;
	avf_size_t mSize;
	avf_u32_t fcc;

	void *owner;
	void (*DoRelease)(raw_data_t *self);
	// follows data

	list_head_t node;
	uint64_t ts_ms;

	static raw_data_t *from_node(list_head_t *n) {
		return list_entry(n, raw_data_t, node);
	}

	INLINE void AddRef() {
		avf_atomic_inc(&mRefCount);
	}

	INLINE void Release() {
		if (avf_atomic_dec(&mRefCount) == 1) {
			this->DoRelease(this);
		}
	}

	INLINE avf_u32_t GetFCC() {
		return fcc;
	}

	INLINE void SetFCC(avf_u32_t fcc) {
		this->fcc = fcc;
	}

	INLINE avf_u8_t *GetData() {
		return (avf_u8_t*)(this + 1);
	}

	INLINE avf_size_t GetSize() {
		return mSize;
	}

	INLINE raw_data_t* acquire() {
		AddRef();
		return this;
	}
};

raw_data_t *avf_alloc_raw_data(avf_size_t size, avf_u32_t fcc);

//-----------------------------------------------------------------------
//
// structs
//
//-----------------------------------------------------------------------

struct media_sample_t
{
	avf_u64_t	m_pos;	// pos in the file
	avf_u32_t	m_size; // size written
	avf_u32_t	m_duration;
	avf_u32_t	m_flags;
	avf_u32_t	m_pts_diff; // pts - dts
};

struct raw_sample_t
{
	// stream index
	avf_size_t m_index;

	// position in file
	avf_size_t m_size;
	avf_u64_t m_pos;

	avf_u8_t frame_type;
	avf_u32_t mFlags;

	avf_pts_t m_dts;
	avf_pts_t m_pts;
	avf_pts_t m_duration;
};

struct stream_raw_data_t
{
	avf_size_t m_index;
	raw_data_t *m_data;
};

struct media_block_t
{
	list_head_t		list_block;	// used only by the creater

	avf_u32_t		m_sample_count;
	avf_u32_t		m_block_size;

	avf_atomic_t	mRefCount;
	void (*DoRelease)(media_block_t *self);

	string_t *filename;
	raw_sample_t *mpSamples;

	// raw data array
	avf_size_t m_num_raw;
	avf_size_t m_max_raw;
	stream_raw_data_t *m_stream_raw_data;

	INLINE void AddRef() {
		avf_atomic_inc(&mRefCount);
	}

	INLINE void Release() {
		if (avf_atomic_dec(&mRefCount) == 1) {
			this->DoRelease(this);
		}
	}

	static INLINE media_block_t *FromNode(list_head_t *node) {
		return list_entry(node, media_block_t, list_block);
	}
};


//-----------------------------------------------------------------------
//
// IActiveObject
//	note: if a class wants to has a thread and an associated cmd queue,
//		it should derive from IActiveObject, and create a CWorkQueue
//
//-----------------------------------------------------------------------
class IActiveObject: public IInterface
{
public:
	enum {
		CMD_TERMINATE,	// terminate the working thread
		CMD_RUN,		// ask the thread to enter OnStart()
		CMD_STOP,		// ask the thread to leave OnStart()
		CMD_LAST
	};

	enum {
		FLAG_LAST
	};

	struct CMD {
		avf_intptr_t sid;	// sender id
		avf_uint_t code;
		void *pExtra;
		avf_intptr_t param;

		CMD() {
			this->sid = 0;
			this->code = 0;
			this->pExtra = NULL;
			this->param = 0;
		}

		CMD(avf_uint_t code, void *pExtra) {
			this->sid = 0;
			this->code = code;
			this->pExtra = pExtra;
			this->param = 0;
		}

		CMD(avf_uint_t code, void *pExtra, avf_intptr_t param) {
			this->sid = 0;
			this->code = code;
			this->pExtra = pExtra;
			this->param = param;
		}
	};

public:
	DECLARE_IF(IActiveObject, IID_IActiveObject);
	virtual const char *GetAOName() = 0;

	// this is called when CMD_RUN is received
	virtual void AOLoop() = 0;

	// other cmd is received (>= CMD_LAST)
	// bInAOLoop = true: it is called in AOLoop, returns false to exit
	virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop) = 0;
};

//-----------------------------------------------------------------------
//
// IBufferPool
//
//-----------------------------------------------------------------------
class IBufferPool: public IInterface
{
public:
	enum {
		// buffers in this pool provide memory
		F_HAS_MEMORY = (1 << 0),

		// this pool always has buffers
		F_HAS_BUFFER = (1 << 1),
	};

public:
	DECLARE_IF(IBufferPool, IID_IBufferPool);

	virtual const char *GetName() = 0;
	virtual avf_u32_t GetFlags() = 0;
	virtual avf_size_t GetMaxNumBuffers() = 0;

	virtual CMsgQueue* GetBufferQueue() = 0;
	virtual bool AllocBuffer(CBuffer*& pBuffer, CMediaFormat *pFormat, avf_size_t size) = 0;
	virtual avf_status_t AllocMem(CBuffer *pBuffer, avf_size_t size) = 0;
	virtual void __ReleaseBuffer(CBuffer *pBuffer) = 0; // called by CBuffer

	// memory
	virtual avf_u32_t GetAllocatedMem() = 0;
};

//-----------------------------------------------------------------------
//
// IEngine - for player and filters use
//
//-----------------------------------------------------------------------
class IEngine: public IInterface
{
public:
	// filters msg to player
	enum {
		MSG_NULL = 0,
		MSG_ERROR,			// filter gets error, p0 == IFilter (or NULL), p1 == error code (avf_status_t)
		MSG_PREPARED,		// filter is prepared
		MSG_STARTED,		// filter is started
		MSG_EOS,			// filter EOS, p0 = IFilter
		MSG_EARLY_EOS,		// 

		// todo - remove
		MSG_FILE_START = 101,	// muxer started a file, filename in "state.file.current"
		MSG_FILE_END,			// muxer has done a file, filename in "state.file.previous"
		MSG_DONE_SAVING,			// current segment is saved
		MSG_DONE_WRITE_NEXT_PICTURE,
		MSG_MEDIA_SOURCE_PROGRESS,

		// todo - remove
		MSG_UPLOAD_VIDEO = 201,
		MSG_UPLOAD_PICTURE,
		MSG_UPLOAD_RAW,

		// todo - remove
		MSG_MMS_MODE1 = 301,
		MSG_MMS_MODE2,
	};

	// players event to control
	enum {
		EV_NULL = 0,
		EV_PLAYER_ERROR = 1,			// player got an error. p0: error code
		EV_OPEN_MEDIA_DONE = 2,		// player->OpenMedia() is done
		EV_PREPARE_MEDIA_DONE = 3,	// player->PrepareMedia() is done
		EV_RUN_MEDIA_DONE = 4,		// player->RunMedia() is done
		EV_STOP_MEDIA_DONE = 5,		// player->StopMedia() is done
		EV_CHANGE_STATE = 6,			// player requires to change the state
		EV_APP_EVENT = 7,			// player sends msg to app
	};

public:
	DECLARE_IF(IEngine, IID_IEngine);

	virtual void PostFilterMsg(const avf_msg_t& msg) = 0;	// for filters
	virtual void PostPlayerMsg(const avf_msg_t& msg) = 0;	// for player
	virtual void ClearFilterMsgs() = 0;						// for player

public:
	virtual void *FindComponent(avf_refiid_t refiid) = 0;

// IRegistry
public:
	virtual IRegistry *GetRegistry() = 0;
	virtual avf_s32_t ReadRegInt32(const char *name, avf_u32_t index, avf_s32_t default_value) = 0;
	virtual avf_s64_t ReadRegInt64(const char *name, avf_u32_t index, avf_s64_t default_value)  = 0;
	virtual avf_int_t ReadRegBool(const char *name, avf_u32_t index, avf_int_t default_value)  = 0;
	virtual void ReadRegString(const char *name, avf_u32_t index, char *value, const char *default_value)  = 0;
	virtual void ReadRegMem(const char *name, avf_u32_t index, avf_u8_t *mem, avf_size_t size) = 0; 

	virtual avf_status_t WriteRegInt32(const char *name, avf_u32_t index, avf_s32_t value)  = 0;
	virtual avf_status_t WriteRegInt64(const char *name, avf_u32_t index, avf_s64_t value)  = 0;
	virtual avf_status_t WriteRegBool(const char *name, avf_u32_t index, avf_int_t value)  = 0;
	virtual avf_status_t WriteRegString(const char *name, avf_u32_t index, const char *value)  = 0;
	virtual avf_status_t WriteRegMem(const char *name, avf_u32_t index, const avf_u8_t *mem, avf_size_t size) = 0;

// misc
public:
	virtual bool IsRealTime() = 0;
};

class IDiskNotify
{
public:
	virtual void OnDiskError() = 0;
	virtual void OnDiskSlow() = 0;
};

//-----------------------------------------------------------------------
//
// ITimerManager
//
//-----------------------------------------------------------------------
class ITimerManager: public IInterface
{
public:
	DECLARE_IF(ITimerManager, IID_ITimerManager);
	virtual avf_status_t SetTimer(IFilter *pFilter, 
		avf_enum_t id, avf_uint_t ms, bool bPeriodic) = 0;
	virtual avf_status_t KillTimer(IFilter *pFilter, avf_enum_t id) = 0;
};

//-----------------------------------------------------------------------
//
// ITimerTarget
//
//-----------------------------------------------------------------------
class ITimerTarget: public IInterface
{
public:
	DECLARE_IF(ITimerTarget, IID_ITimerTarget);
	virtual avf_status_t TimerTriggered(avf_enum_t id) = 0;
};

//-----------------------------------------------------------------------
//
// ITimerReceiver
//
//-----------------------------------------------------------------------
class ITimerReceiver: public IInterface
{
public:
	DECLARE_IF(ITimerReceiver, IID_ITimerReceiver);
	virtual void OnTimer(void *param1, avf_enum_t param2) = 0;
};

//-----------------------------------------------------------------------
//
// IMediaPlayer
//
//-----------------------------------------------------------------------
class IMediaPlayer: public IInterface
{
public:
	DECLARE_IF(IMediaPlayer, IID_IMediaPlayer);

	virtual avf_status_t OpenMedia(bool *pbDone) = 0;
	virtual avf_status_t PrepareMedia(bool *pbDone) = 0;
	virtual avf_status_t RunMedia(bool *pbDone) = 0;
	virtual avf_status_t StopMedia(bool *pbDone) = 0;

	virtual void SetState(avf_enum_t state) = 0;	// see IMediaControl
	virtual void CancelActionAsync() = 0;

	virtual void ProcessFilterMsg(const avf_msg_t& msg) = 0;
	virtual avf_status_t DispatchTimer(IFilter *pFilter, avf_enum_t id) = 0;
	virtual avf_status_t Command(avf_enum_t cmdId, avf_intptr_t arg) = 0;

	virtual avf_int_t GetID() = 0;
	virtual avf_status_t DumpState() = 0;
};

//-----------------------------------------------------------------------
//
// IFilter
//
//-----------------------------------------------------------------------
class IFilter: public IInterface
{
public:
	DECLARE_IF(IFilter, IID_IFilter);

	virtual avf_status_t InitFilter() = 0;
	virtual avf_status_t PreRunFilter() = 0;
	virtual avf_status_t RunFilter() = 0;
	virtual avf_status_t StopFilter() = 0;
	virtual avf_status_t PostStopFilter() = 0;
	virtual avf_status_t ForceEOS() = 0;

	virtual void SetFilterIndex(int index) = 0;
	virtual void SetTypeIndex(int index) = 0;
	virtual int GetFilterIndex() = 0;
	virtual int GetTypeIndex() = 0;
	virtual const char *GetFilterName() = 0;

	virtual avf_size_t GetNumInput() = 0;
	virtual avf_size_t GetNumOutput() = 0;
	virtual IPin *GetInputPin(avf_uint_t index) = 0;
	virtual IPin *GetOutputPin(avf_uint_t index) = 0;
};

//-----------------------------------------------------------------------
//
// IPin : 1 output to * inputs
//
//-----------------------------------------------------------------------
class IPin: public IInterface
{
public:
	enum {
		BUFFERS_SENT,
		BUFFERS_RECEIVED,
		BUFFERS_STORED,
		BUFFERS_DROPPED,
		_NUM_BUFFER_TYPES
	};

public:
	DECLARE_IF(IPin, IID_IPin);

	virtual const char *GetPinName() = 0;	// get pin name
	virtual IFilter *GetFilter() = 0;		// get owner filter
	virtual avf_uint_t GetPinIndex() = 0;

	virtual void ReceiveBuffer(CBuffer *pBuffer) = 0;	// for input pin
	virtual void ForceEOS() = 0;
	virtual void PurgeInput() = 0;			// for input pin
	virtual void ResetState() = 0;

	virtual bool CanConnect() = 0;
	virtual int Connected() = 0;
	virtual bool NeedReconnect() = 0;		// for output pin

	virtual int Connect(IPin *pPeer, IBufferPool *pBufferPool, CMediaFormat *pMediaFormat) = 0;
	virtual void Disconnect(IPin *pPeer) = 0;

	virtual CMediaFormat* QueryMediaFormat() = 0;	// for output pin
	virtual IBufferPool* QueryBufferPool() = 0;		// for output pin
	virtual avf_status_t AcceptMedia(CMediaFormat *pMediaFormat, 
		IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool) = 0; // for input pin

	virtual CMsgQueue* GetBufferQueue() = 0;

	virtual bool GetEOS() = 0;
	virtual avf_int_t GetBuffers(avf_enum_t type) = 0;
	virtual avf_int_t GetBuffersDiff(avf_enum_t type) = 0;

	virtual CMediaFormat *GetUsingMediaFormat() = 0;
	virtual IBufferPool *GetUsingBufferPool() = 0;

public:
	INLINE avf_int_t GetBuffersSent() {
		return GetBuffers(BUFFERS_SENT);
	}
	INLINE avf_int_t GetBuffersReceived() {
		return GetBuffers(BUFFERS_RECEIVED);
	}
	INLINE avf_int_t GetBuffersStored() {
		return GetBuffers(BUFFERS_STORED);
	}

	INLINE avf_int_t GetBuffersSentDiff() {
		return GetBuffersDiff(BUFFERS_SENT);
	}
	INLINE avf_int_t GetBuffersReceivedDiff() {
		return GetBuffersDiff(BUFFERS_RECEIVED);
	}
	INLINE avf_int_t GetBuffersStoredDiff() {
		return GetBuffersDiff(BUFFERS_STORED);
	}
};

//-----------------------------------------------------------------------
//
// IMediaRenderer
//
//-----------------------------------------------------------------------
class IMediaRenderer: public IInterface
{
public:
	DECLARE_IF(IMediaRenderer, IID_IMediaRenderer);

	virtual avf_status_t InitRenderer(avf_u32_t start_ms) = 0;
	virtual avf_status_t PauseRenderer() = 0;
	virtual avf_status_t ResumeRenderer() = 0;
};

//-----------------------------------------------------------------------
//
// ISegmentSaver
//
//-----------------------------------------------------------------------
class ISegmentSaver: public IInterface
{
public:
	DECLARE_IF(ISegmentSaver, IID_ISegmentSaver);

	virtual bool CanSave() = 0;
	virtual avf_status_t SaveCurrent(avf_size_t before, avf_size_t after, const char *filename) = 0;
};

//-----------------------------------------------------------------------
//
// IRecordObserver
//
//-----------------------------------------------------------------------
class IRecordObserver: public IInterface
{
public:
	enum {
		GPS_DATA = MKFCC('g','p','s','0'),
		ACC_DATA = MKFCC('a','c','c','0'),
		OBD_DATA = MKFCC('o','d','b','0'),
		VMEM_DATA = MKFCC('V','M','E','M'),
		///
		VIN_DATA = MKFCC('v','i','n','0'),
		VIN_CONFIG = MKFCC('V','I','N','C'),
	};

public:
	DECLARE_IF(IRecordObserver, IID_IRecordObserver);

	virtual int GetSegmentLength() = 0;
	virtual avf_status_t StartRecord(bool bEnableRecord) = 0;
	virtual void StopRecord() = 0;

	virtual string_t *CreateVideoFileName(avf_time_t *gen_time, int stream, const char *ext) = 0;
	virtual string_t *CreatePictureFileName(avf_time_t *gen_time, int stream) = 0;

	virtual void InitVideoStream(const avf_stream_attr_s *stream_attr, int video_type, int stream) = 0;
	virtual void PostIO(IAVIO *io, ITSMapInfo *map_info, int stream) = 0;
	virtual void StartSegment(avf_time_t video_time, avf_time_t picture_time, int stream) = 0;
	virtual void EndSegment(avf_u64_t next_start_ms, avf_u32_t v_fsize, int stream, bool bLast) = 0;

	virtual void AddVideo(avf_u64_t pts, avf_u32_t timescale, avf_u32_t fpos, int stream) = 0;
	virtual void AddPicture(CBuffer *pBuffer, int stream) = 0;
	virtual void AddRawData(raw_data_t *raw, int stream) = 0;
	virtual void SetConfigData(raw_data_t *raw) = 0;
	virtual void AddRawDataEx(raw_data_t *raw) = 0;

	virtual void PostRawDataFreq(int interval_gps, int interval_acc, int interval_obd) = 0;
};

//-----------------------------------------------------------------------
//
// IMediaFileReader
//
//-----------------------------------------------------------------------
class IMediaFileReader: public IInterface
{
public:
	DECLARE_IF(IMediaFileReader, IID_IMediaFileReader);

	virtual avf_status_t SetReportProgress(bool bReport) = 0;
	virtual avf_status_t OpenMediaSource() = 0;

	virtual avf_uint_t GetNumTracks() = 0;
	virtual IMediaTrack* GetTrack(avf_uint_t index) = 0;

	virtual avf_status_t GetMediaInfo(avf_media_info_t *info) = 0;
	virtual avf_status_t GetVideoInfo(avf_uint_t index, avf_video_info_t *info) = 0;
	virtual avf_status_t GetAudioInfo(avf_uint_t index, avf_audio_info_t *info) = 0;
	virtual avf_status_t GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info) = 0;

	virtual bool IsStreamFormat() = 0;
	virtual bool CanFastPlay() = 0;

	virtual avf_status_t SeekToTime(avf_u32_t ms, avf_u32_t *out_ms) = 0;	// IsStreamFormat(), CanFastPlay()
	virtual avf_status_t FetchSample(avf_size_t *pTrackIndex) = 0;	// IsStreamFormat()
};

//-----------------------------------------------------------------------
//
// IReadProgress
//
//-----------------------------------------------------------------------
class IReadProgress: public IInterface
{
public:
	struct progress_t {
		avf_u64_t total_bytes;
		avf_u64_t remain_bytes;
	};

public:
	DECLARE_IF(IReadProgress, IID_IReadProgress);
	virtual avf_status_t GetReadProgress(progress_t *progress) = 0;
};

//-----------------------------------------------------------------------
//
// IMediaFileWriter
//
//-----------------------------------------------------------------------
class IMediaFileWriter: public IInterface
{
public:
	DECLARE_IF(IMediaFileWriter, IID_IMediaFileWriter);

	virtual avf_status_t StartWriter(bool bEnableRecord) = 0;
	virtual avf_status_t EndWriter() = 0;

	// picture_name might be NULL
	virtual avf_status_t StartFile(string_t *video_name, avf_time_t video_time,
		string_t *picture_name, avf_time_t picture_time, string_t *basename) = 0;
	virtual IAVIO *GetIO() = 0;

	// fsize may be NULL
	// nextseg_ms: valid when bLast == false
	virtual avf_status_t EndFile(avf_u32_t *fsize, bool bLast, avf_u64_t nextseg_ms) = 0;
	virtual const char *GetCurrentFile() = 0;

	virtual avf_status_t SetRawData(avf_size_t stream, raw_data_t *raw) = 0;

	virtual avf_status_t ClearMediaFormats() = 0;
	virtual avf_status_t CheckMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat) = 0;
	virtual avf_status_t SetMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat) = 0;
	virtual avf_status_t CheckStreams() = 0;
	virtual avf_status_t SetStreamAttr(avf_stream_attr_s& stream_attr) = 0;

	virtual avf_status_t WriteSample(int stream, CBuffer* pBuffer) = 0;
	virtual avf_status_t WritePoster(string_t *filename, avf_time_t gen_time, CBuffer* pBuffer) = 0;

	virtual bool ProvideSamples() = 0;
	virtual ISampleProvider *CreateSampleProvider(avf_size_t before, avf_size_t after) = 0;
};

//-----------------------------------------------------------------------
//
// ISampleProvider
//
//-----------------------------------------------------------------------
class ISampleProvider: public IInterface
{
public:
	DECLARE_IF(ISampleProvider, IID_ISampleProvider);

	virtual avf_size_t GetNumStreams() = 0;
	virtual CMediaFormat *QueryMediaFormat(avf_size_t index) = 0;
	virtual media_block_t *PopMediaBlock() = 0;
};

//-----------------------------------------------------------------------
//
// IMediaTrack
//
//-----------------------------------------------------------------------
class IMediaTrack: public IInterface
{
public:
	DECLARE_IF(IMediaTrack, IID_IMediaTrack);

	virtual avf_status_t EnableTrack(bool bEnable) = 0;
	virtual CMediaFormat* QueryMediaFormat() = 0;
	virtual avf_status_t SeekToTime(avf_u32_t ms) = 0;
	virtual avf_status_t ReadSample(CBuffer *pBuffer, bool& bEOS) = 0;
	virtual avf_pts_t GetDtsOffset() = 0;
	virtual void SetDtsOffset(avf_pts_t offset) = 0;
};

//-----------------------------------------------------------------------
//
// IVdbInfo
//
//-----------------------------------------------------------------------
class IVdbInfo: public IInterface
{
public:
	// describes a range in a clip/playlist
	struct range_s {
		avf_u32_t clip_type;		// buffered, marked, or playlist
		clip_id_t ref_clip_id;
		avf_s8_t _stream;		// 0 or 1; -1 for upload
		avf_u8_t b_playlist;		// is a range in a playlist
		avf_u32_t length_ms;		// duration; 0: get all remain
		avf_u64_t clip_time_ms;	// playlist: relative to playlist start
	};

	// range info of clip or cliplist
	// if stream is 0 or 1, then [0] are used
	// if stream < 0, then both are used
	// for playlist: total info
	struct clip_info_s {
		clip_id_t clip_id;			// *clip* id; cliplist: INVALID_CLIP_ID
		avf_u32_t clip_date;			// date when the clip was created; cliplist: of the first clip

		avf_u64_t clip_addr[2];		// absolute pos in clip; cliplist: 0
		avf_u64_t clip_time_ms;		// absolute pos in clip; cliplist: relative to playlist start

		avf_u64_t v_size[2];			// video size in bytes; cliplist: total video size
		avf_u64_t ex_size;			// upload: all upload_header_v2_t + pic + raw
		avf_u32_t _length_ms;		// clip range length; cliplist: total length

		avf_u8_t b_cliplist;			
		avf_u8_t stream;				// video stream, 0 or 1; -1 for upload
		avf_u8_t video_type[2];
	};

	// clip_info_s + cliplist_info_s : describes playlist range info
	struct cliplist_info_s: public TDynamicArray<clip_info_s> {
		// nothing else
	};

	// used by IPC
	struct clip_set_info_s {
		clip_id_t ref_clip_id;
		avf_u32_t ref_clip_date;
		avf_u64_t clip_time_ms;
		avf_u32_t length_ms;
	};

public:
	DECLARE_IF(IVdbInfo, IID_IVdbInfo);

	virtual avf_status_t DeleteClip(int clip_type, avf_u32_t clip_id) = 0;

	// used by IPC
	virtual avf_status_t GetNumClips(int clip_type, avf_u32_t *num_clips, avf_u32_t *sid) = 0;
	virtual avf_status_t GetClipSetInfo(int clip_type, clip_set_info_s *info, avf_u32_t start, avf_u32_t *num, avf_u32_t *sid) = 0;

	virtual IClipReader *CreateClipReaderOfTime(const range_s *range, bool bMuteAudio,
		bool bFixPTS, avf_u32_t init_time_ms, avf_u32_t upload_opt) = 0;

	// if upload_opt != 0, then offset must be 0
	virtual IClipReader *CreateClipReaderOfAddress(const clip_info_s *clip_info,
		cliplist_info_s *cliplist_info, avf_u64_t offset, bool bMuteAudio,
		bool bFixPTS, avf_u32_t init_time_ms, avf_u32_t upload_opt) = 0;

	virtual IClipData *CreateClipDataOfTime(range_s *range) = 0;

	virtual IClipWriter *CreateClipWriter() = 0;
};

//-----------------------------------------------------------------------
//
// IClipReader
//
//-----------------------------------------------------------------------
class IClipReader: public IInterface
{
public:
	DECLARE_IF(IClipReader, IID_IClipReader);

	// read should be packet aligned (188B)
	virtual int ReadClip(avf_u8_t *buffer, avf_size_t size) = 0;
	virtual avf_u64_t GetStartTimeMs() = 0;
	virtual avf_u32_t GetLengthMs() = 0;
	virtual avf_u64_t GetSize() = 0;
	virtual avf_u64_t GetPos() = 0;
	virtual avf_status_t Seek(avf_s64_t offset, int whence) = 0;
};

//-----------------------------------------------------------------------
//
// IClipWriter
//
//-----------------------------------------------------------------------
class IClipWriter: public IInterface
{
public:
	enum {
		CB_ID_NULL = 0,
		CB_ID_StartRecord,	// CVDB::vdb_record_info_t
		CB_ID_StopRecord,		// CVDB::vdb_record_info_t
		CB_ID_GPS_DATA
	};

public:
	DECLARE_IF(IClipWriter, IID_IClipWriter);

	virtual void SetCallback(void (*cb_proc)(void *context,
		int cbid, void *param, int size), void *context) = 0;
	virtual void SetSegmentLength(avf_uint_t v_length, avf_uint_t p_length, avf_uint_t r_length) = 0;
	virtual avf_status_t StartRecord(int ch) = 0;
	virtual avf_status_t StopRecord() = 0;
	virtual avf_status_t WriteData(const avf_u8_t *data, avf_size_t size) = 0;
};

//-----------------------------------------------------------------------
//
// ISeekToTime
//
//-----------------------------------------------------------------------
class ISeekToTime: public IInterface
{
public:
	DECLARE_IF(ISeekToTime, IID_ISeekToTime);

	virtual avf_u32_t GetLengthMs() = 0;
	virtual avf_status_t SeekToTime(avf_u32_t time_ms) = 0;
};

//-----------------------------------------------------------------------
//
// IClipData - read clip data sequentially
//	data/file format defined as upload_header_t + data
//
//-----------------------------------------------------------------------
class IClipData: public IInterface
{
public:
	struct desc_s {
		int data_type;	// input;
		int stream;		// input;
		avf_u64_t seg_time_ms;	// output;
		avf_u64_t clip_time_ms;	// output;
		avf_u32_t length_ms;		// output;
		avf_u32_t num_entries;	// output;
		avf_u32_t total_size;	// output;
	};

public:
	DECLARE_IF(IClipData, IID_IClipData);

	virtual avf_status_t ReadClipData(desc_s *desc, void **pdata, avf_size_t *psize) = 0;
	virtual avf_status_t ReadClipDataToFile(desc_s *desc, const char *filename) = 0;
	virtual avf_status_t ReadClipRawData(int data_types, void **pdata, avf_size_t *psize) = 0;
	virtual avf_status_t ReadClipRawDataToFile(int data_types, const char *filename) = 0;
	virtual void ReleaseData(void *data) = 0;
};

//-----------------------------------------------------------------------
//
// CMediaFormat - can be derived
//	content cannot be changed after creation and being used
//
//-----------------------------------------------------------------------
struct CMediaFormat
{
	enum {
		F_HAS_EXTRA_DATA = (1 << 0),
	};

	avf_atomic_t mRefCount;
	void (*DoRelease)(CMediaFormat *self);
	avf_u32_t mFlags;			// F_HAS_EXTRA_DATA

	////////////////////////

	int mMediaType;			// MEDIA_TYPE_VIDEO, MEDIA_TYPE_AUDIO
	avf_fcc_t mFormatType;

	avf_u32_t mFrameDuration;	// frame length (in mTimeScale)
	avf_u32_t mTimeScale;		// 'mTimeScale' units == 1 second

	////////////////////////

	INLINE void AddRef() {
		avf_atomic_inc(&mRefCount);
	}

	INLINE void Release() {
		if (avf_atomic_dec(&mRefCount) == 1) {
			this->DoRelease(this);
		}
	}

	INLINE bool IsVideo();
	INLINE bool IsAudio();
	INLINE bool IsMediaType(int media_type);

	INLINE bool HasExtraData() {
		return (this->mFlags & F_HAS_EXTRA_DATA) != 0;
	}

	INLINE avf_u64_t ms_to_pts(avf_u32_t ms) {
		return (avf_u64_t)ms * mTimeScale / 1000;
	}

	INLINE avf_u32_t pts_to_ms(avf_u64_t pts) {
		return pts * 1000 / mTimeScale;
	}

	INLINE CMediaFormat* acquire() {
		AddRef();
		return this;
	}
};

#include "avf_media_format.h"

bool CMediaFormat::IsVideo() {
	return mMediaType == MEDIA_TYPE_VIDEO;
}

bool CMediaFormat::IsAudio() {
	return mMediaType == MEDIA_TYPE_AUDIO;
}

bool CMediaFormat::IsMediaType(int media_type) {
	return mMediaType == media_type;
}

void *__avf_alloc_media_format(avf_size_t size);
#define avf_alloc_media_format(type) \
	(type*)__avf_alloc_media_format(sizeof(type))

//-----------------------------------------------------------------------
//
// CBuffer - managed by its buffer pool, may be derived
//
//-----------------------------------------------------------------------
struct CBuffer
{
	// mType : 0-255
	enum Type {
		DATA = 1,		// carries media data
		EOS = 2,		// End Of Stream
		NEW_SEG = 3,	// new segment is started
		START_DTS = 4,	// carries only dts
		STREAM_EOS = 5,	// end of sub stream
	};

	// mFlags : 32 bits
	enum {
		F_KEYFRAME = (1 << 0),
		F_AVC_IDR = (1 << 1),	// for H.264
		F_JPEG = (1 << 2),		// poster image
		F_RAW = (1 << 3),		// raw data without header
		// bits 16..31 are not used
		F_USER_START = (1 << 16),
	};

	//====================================

	avf_atomic_t mRefCount;
	IBufferPool *mpPool;
	CBuffer *mpNext;			// next half or NULL
	CMediaFormat *mpFormat;		// valid for DATA; no need to release

	//====================================

	avf_u8_t mbNeedFree;		// this needs to be freed
	avf_u8_t mType;				// Type
	avf_u8_t frame_type;		// I, P, B: not used now
	avf_u8_t reserved;

	avf_u32_t mFlags;			// F_KEYFRAME etc

	avf_u8_t *mpData;			// raw data base address
	avf_size_t mSize;			// size of data

	avf_size_t m_offset;		// offset of data start
	avf_size_t m_data_size;		// the total buffer's size

	avf_pts_t m_dts;			// Decoding Time Stamp
	avf_pts_t m_pts;			// Presentation Time Stamp
	avf_u32_t m_duration;		// duration

	//====================================

	INLINE avf_u8_t *GetData() {
		return mpData + m_offset;
	}

	INLINE avf_size_t GetDataSize() {
		return m_data_size;
	}

	INLINE avf_u8_t *GetMem() {
		return mpData;
	}

	INLINE avf_size_t GetMemSize() {
		return mSize;
	}

	INLINE void Init(IBufferPool *pPool, CMediaFormat *pMediaFormat) {
		avf_atomic_set(&mRefCount, 1);
		mpPool = pPool;
		mpNext = NULL;
		mpFormat = pMediaFormat;
	}

	INLINE void SetEOS() {
		mType = EOS;
		mFlags = 0;
	}

	INLINE void SetNewSeg(avf_u64_t next_pts) {
		mType = NEW_SEG;
		mFlags = 0;
		m_pts = next_pts;
	}

	INLINE void SetStartDts() {
		mType = START_DTS;
		mFlags = 0;
	}

	INLINE void SetStreamEOS() {
		mType = STREAM_EOS;
		mFlags = 0;
	}

	INLINE bool IsEOS() {
		return mType == EOS;
	}

	INLINE void AddRef() {
		avf_atomic_inc(&mRefCount);
	}

	INLINE void Release() {
		if (avf_atomic_dec(&mRefCount) == 1) {
			mpPool->__ReleaseBuffer(this);
		}
	}

	INLINE CBuffer* acquire() {
		AddRef();
		return this;
	}

	INLINE avf_status_t AllocMem(avf_size_t size) {
		return mpPool->AllocMem(this, size);
	}

	INLINE avf_uint_t GetTimeScale() {
		return mpFormat->mTimeScale;
	}

	INLINE avf_u64_t pts_90k(avf_u64_t pts) {
		avf_u32_t timescale = mpFormat->mTimeScale;
		return timescale == _90kHZ ? pts : (pts * _90kHZ / timescale);
	}

	INLINE avf_u64_t pts_to_ms(avf_u64_t pts) {
		return pts * 1000 / mpFormat->mTimeScale;
	}
};


#endif

