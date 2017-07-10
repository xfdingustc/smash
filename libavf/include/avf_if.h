

#ifndef __AVF_IF_H__
#define __AVF_IF_H__

extern const avf_iid_t IID_IInterface;
extern const avf_iid_t IID_IRegistry;
extern const avf_iid_t IID_IMediaControl;
extern const avf_iid_t IID_IVDBControl;
extern const avf_iid_t IID_IUploadManager;
extern const avf_iid_t IID_IStorageListener;

class CConfig;
class IInterface;
class IRegistry;
class IMediaControl;
class IVDBControl;
class IStorageListener;
class IMediaPlayer;
class IEngine;

//-----------------------------------------------------------------------
//
// message definition
//
//-----------------------------------------------------------------------

struct avf_msg_t
{
	avf_intptr_t sid;	// sender id; 0/NULL is reserved
	avf_uint_t code;		// msg code

	avf_intptr_t p0;		// parameter 0
	avf_intptr_t p1;		// parameter 1

	// how to release this msg object
	void (*__release)(avf_msg_t *self);

	avf_msg_t() {
		this->sid = 0;
		this->__release = NULL;
		this->code = 0;
	}

	avf_msg_t(avf_uint_t code) {
		this->sid = 0;
		this->__release = NULL;
		this->code = code;
	}

	INLINE void Release() {
		if (__release) {
			(__release)(this);
		}
	}
};

//-----------------------------------------------------------------------
//
// media info
//
//-----------------------------------------------------------------------

struct avf_media_info_t
{
	avf_uint_t length_ms;
	avf_int_t num_videos;
	avf_int_t num_audios;
	avf_int_t num_subtitles;
};

struct avf_video_info_t
{
	avf_int_t width;
	avf_int_t height;
	avf_int_t framerate;	// FrameRate_Unknown
};

struct avf_audio_info_t
{
	int dummy;
};

struct avf_subtitle_info_t
{
	int	dummy;
};

//-----------------------------------------------------------------------
//
// IInterface
//
//-----------------------------------------------------------------------
class IInterface
{
public:
	virtual void *GetInterface(avf_refiid_t refiid) = 0;
	virtual void AddRef() = 0;
	virtual void Release() = 0;

protected:
	virtual ~IInterface() {}
};

#define DECLARE_IF(ifName, iid) \
	static ifName* GetInterfaceFrom(IInterface *pInterface) \
	{ \
		return (ifName*)pInterface->GetInterface(iid); \
	} \
	INLINE ifName* acquire() { \
		AddRef(); \
		return this; \
	}

#define DEFINE_INTERFACE \
public: \
	virtual void *GetInterface(avf_refiid_t refiid); \
	virtual void AddRef() { \
		inherited::AddRef(); \
	} \
	virtual void Release() { \
		inherited::Release(); \
	}

//-----------------------------------------------------------------------
//
// IRegistry
//
//-----------------------------------------------------------------------
class IRegistry: public IInterface
{
public:
	DECLARE_IF(IRegistry, IID_IRegistry);

	enum {
		// should keep same with avf_media.h, AVF_GLOBAL_CONFIG
		GLOBAL = 0x80000000,
	};

public:
	virtual avf_s32_t ReadRegInt32(const char *name, avf_u32_t index, avf_s32_t default_value) = 0;
	virtual avf_s64_t ReadRegInt64(const char *name, avf_u32_t index, avf_s64_t default_value) = 0;
	virtual avf_int_t ReadRegBool(const char *name, avf_u32_t index, avf_int_t default_value) = 0;
	virtual void ReadRegString(const char *name, avf_u32_t index, char *value, const char *default_value) = 0;
	virtual void ReadRegMem(const char *name, avf_u32_t index, avf_u8_t *mem, avf_size_t size) = 0; 

	virtual avf_status_t WriteRegInt32(const char *name, avf_u32_t index, avf_s32_t value) = 0;
	virtual avf_status_t WriteRegInt64(const char *name, avf_u32_t index, avf_s64_t value) = 0;
	virtual avf_status_t WriteRegBool(const char *name, avf_u32_t index, avf_int_t value) = 0;
	virtual avf_status_t WriteRegString(const char *name, avf_u32_t index, const char *value) = 0;
	virtual avf_status_t WriteRegMem(const char *name, avf_u32_t index, const avf_u8_t *mem, avf_size_t size) = 0;

	virtual avf_status_t LoadConfig(CConfig *pConfig, const char *root) = 0;
	virtual void ClearRegistry() = 0;
};

//-----------------------------------------------------------------------
//
// IMediaControl - for user to control players (camera, playback, etc)
//
//-----------------------------------------------------------------------
class IMediaControl: public IRegistry
{
public:
	// MediaControl state
	enum {
		STATE_IDLE = 0,		// just created, doing nothing
		STATE_OPENING = 1,	// opening the media source, triggered by OpenMedia()

		STATE_OPEN = 2,		// media source is open, source filter is created
		STATE_STOPPED = STATE_OPEN,
		STATE_PREPARING = 3,	// creating the filter graph, triggered by PrepareMedia()

		STATE_PREPARED = 4,	// filter graph is created, ready to run
		STATE_STARTING = 5,	// start to run the media, triggered by RunMedia()

		STATE_RUNNING = 6,	// running the media, triggered by RunMedia()
		STATE_STOPPING = 7,	// stopping the media, triggered by StopMedia()

		STATE_ERROR = 8,		// error state (from OPENING, PREPARING, RUNNING, STOPPING)

		STATE_NUM
	};

	// msg form MediaControl to app
	enum {
		APP_MSG_STATE_CHANGED,	// MediaControl state has changed

		// todo - remove
		APP_MSG_FILE_START,		// muxer started a new file
		APP_MSG_FILE_END,			// muxer done a file
		APP_MSG_DISK_ERROR,		// 
		APP_MSG_DISK_SLOW,		//
		APP_MSG_DONE_SAVING,		// saving is done
		APP_MSG_DONE_WRITE_NEXT_PICTURE,	// result of CMD_SAVE_NEXT_PICTURE
		APP_MSG_BUFFER_SPACE_LOW,	// VDB
		APP_MSG_BUFFER_SPACE_FULL,	// VDB
		APP_MSG_VDB_STATE_CHANGED,	// VDB
		APP_MSG_MEDIA_SOURCE_PROGRESS,	// for remuxer

		// todo - remove
		APP_MSG_STILL_CAPTURE_STATE_CHANGED,
		APP_MSG_PICTURE_TAKEN,
		APP_MSG_PICTURE_LIST_CHANGED,

		// todo - remove
		APP_MSG_UPLOAD_VIDEO = 100,
		APP_MSG_UPLOAD_PICTURE = 101,
		APP_MSG_UPLOAD_RAW = 102,
		APP_MSG_MMS_MODE1 = 103,
		APP_MSG_MMS_MODE2 = 104,

		APP_MSG_FATAL_ERROR = 200,

		// todo - for JNI
		APP_MSG_ATTACH_THREAD = 300,
		APP_MSG_DETACH_THREAD = 301,

		APP_MAG_LAST,
	};

	struct StateInfo {
		avf_enum_t prevState;		// previous state
		avf_enum_t state;			// current state
		avf_enum_t targetState;	// target state
		avf_status_t error;		// error code, valid when state == STATE_ERROR
	};

public:
	DECLARE_IF(IMediaControl, IID_IMediaControl);
	virtual bool RegisterComponent(IInterface *comp) = 0;
	virtual bool UnregisterComponent(IInterface *comp) = 0;

	// "camera", NULL, pConfig
	// "playback", filename, NULL
	// tag is a non-zero value for callback
	virtual avf_status_t SetMediaSource(const char *pPlayer, const char *pMediaSrc, const char *pConfig, avf_intptr_t tag, int conf) = 0;
	virtual avf_status_t SetMediaState(avf_enum_t targetState, bool bWait = false) = 0;

	// 
	virtual void GetStateInfo(StateInfo& info) = 0;

	// wait until state == curr_state or STATE_ERROR
	virtual avf_enum_t WaitState(avf_enum_t state, avf_status_t *result) = 0;

	// Command(id, arg);
	virtual avf_status_t Command(int cmpid, avf_enum_t cmdId, avf_intptr_t cmdArg) = 0;

	// for debug
	virtual avf_status_t DumpState() = 0;

public:
	INLINE avf_status_t OpenMedia(bool bWait = false) {
		return SetMediaState(STATE_OPEN, bWait);
	}
	INLINE avf_status_t CloseMedia(bool bWait = false) {
		return SetMediaState(STATE_IDLE, bWait);
	}
	INLINE avf_status_t PrepareMedia(bool bWait = false) {
		return SetMediaState(STATE_PREPARED, bWait);
	}
	INLINE avf_status_t RunMedia(bool bWait = false) {
		return SetMediaState(STATE_RUNNING, bWait);
	}
	INLINE avf_status_t StopMedia(bool bWait = false) {
		return SetMediaState(STATE_OPEN, bWait);
	}
};

//-----------------------------------------------------------------------
//
// IVDBControl - for user to control vdb
//
//-----------------------------------------------------------------------
class IVDBControl: public IInterface
{
public:
	struct vdb_info_s {
		avf_u32_t total_space_mb;	// total of tf
		avf_u32_t disk_space_mb;		// free space
		avf_u32_t file_space_mb;		// is 0
		avf_u8_t vdb_ready;		// vdb loaded
		avf_u8_t enough_space;	// enough space for normal record
		avf_u8_t enough_space_autodel;	// enough space for auto-del record
	};

	struct mark_info_s {
		int b_running;
		int b_valid;
		int delay_ms;
		int before_live_ms;
		int after_live_ms;
		avf_u32_t clip_id;
	};

public:
	DECLARE_IF(IVDBControl, IID_IVDBControl);
	virtual void StorageReady(int is_ready) = 0;
	virtual avf_status_t RegisterStorageListener(IStorageListener *listener, bool *pbReady) = 0;
	virtual avf_status_t UnregisterStorageListener(IStorageListener *listener) = 0;
	virtual bool IsStorageReady() = 0;
	virtual bool CheckStorageSpace() = 0;
	virtual void GetVdbInfo(vdb_info_s *info, int free_vdb_space) = 0;
	virtual avf_status_t SetRecordAttr(const char *attr_str) = 0;
	virtual avf_status_t MarkLiveClip(int delay_ms, int before_live_ms, int after_live_ms,
		int b_continue, int live_mark_flag, const char *attr_str) = 0;
	virtual avf_status_t StopMarkLiveClip() = 0;
	virtual avf_status_t GetMarkLiveInfo(mark_info_s& info) = 0;
	virtual avf_status_t EnablePictureService(const char *picture_path) = 0;
	virtual avf_status_t RefreshPictureList() = 0;
	virtual avf_status_t PictureTaken(int code) = 0;
	virtual avf_status_t GetNumPictures(int *num) = 0;
	virtual avf_status_t GetSpaceInfo(avf_u64_t *total_space, avf_u64_t *used_space,
		avf_u64_t *marked_space, avf_u64_t *clip_space) = 0;
	virtual avf_status_t SetClipSceneData(int clip_type, avf_u32_t clip_id, const avf_u8_t *data, avf_size_t data_size) = 0;
};

//-----------------------------------------------------------------------
//
// IStorageListener
//
//-----------------------------------------------------------------------
class IStorageListener: public IInterface
{
public:
	DECLARE_IF(IStorageListener, IID_IStorageListener);
	virtual void NotifyStorageState(int is_ready) = 0;
};

//-----------------------------------------------------------------------
//
// global APIs
//
//-----------------------------------------------------------------------
//extern "C" avf_status_t avf_init(void);
//extern "C" void avf_terminate(void);
typedef void (*MediaControl_MsgProc)(void*, avf_intptr_t, const avf_msg_t&);

extern "C" int avf_module_init();
extern "C" int avf_module_cleanup();

class IRegistry;

extern "C" IMediaControl* avf_create_media_control(IRegistry *pGlobalRegistry, 
	MediaControl_MsgProc MsgProc, void *context, bool bRealTime);
extern "C" const char *avf_get_media_state_name(avf_enum_t media_state);

typedef void (*avf_app_msg_callback)(void *context, avf_intptr_t tag, const avf_msg_t& msg);
extern "C" IVDBControl *avf_create_vdb_control(IRegistry *pRegistry, avf_app_msg_callback callback, void *context);

// known player types
enum {
	CMP_ID_NULL = 0,
	CMP_ID_CAMERA = 1,
	CMP_ID_PLAYBACK = 2,
	CMP_ID_REMUXER = 3,
	CMP_ID_FMT_CONVERTER = 4,
	CMP_ID_VDB = 5,
};

extern "C" IMediaPlayer* avf_create_player(
	const ap<string_t>& player,
	const ap<string_t>& mediaSrc,
	int conf,
	CConfig *pConfig,
	IEngine *pEngine);

//-----------------------------------------------------------------------
//
// utils APIs
//
//-----------------------------------------------------------------------
extern "C" int avf_load_text_file(const char *filename, char **pbuffer);
extern "C" void avf_free_text_buffer(char *buffer);

extern "C" void avf_mem_info(int freeres);

#endif

