
#ifndef __VDB_MANAGER_H__
#define __VDB_MANAGER_H__

class CVDB;
class CVDBServer;
class CVDBBroadcast;
class CVdbHttpServer;

class CVDBManager;
class CConfig;

//-----------------------------------------------------------------------
//
//  CVDBManager
//
//-----------------------------------------------------------------------
class CVDBManager: public CObject,
	public IVDBControl,
	public IActiveObject,
	public IRecordObserver,
	public IPictureListener
{
	typedef CObject inherited;
	typedef IActiveObject AO;
	DEFINE_INTERFACE;

	enum {
		CMD_Stop,
	
		// from IVDBControl
		CMD_MediaStateChanged,
		
		// from IRecordObserver

		CMD_StartRecord,
		CMD_StopRecord,

		CMD_InitVideoStream,
		CMD_PostIO,
		CMD_StartSegment,
		CMD_EndSegment,

		CMD_AddVideo,
		CMD_AddPicture,
		CMD_AddRawData,
		CMD_SetConfigData,
		CMD_AddRawDataEx,
		CMD_PostRawDataFreq,

		CMD_RefreshPictureList,
		CMD_PictureTaken,
	};

	typedef struct vdb_man_cmd_s {
		int cmd_code;
		union {
			// CMD_QUIT

			// CMD_MediaStateChanged
			struct {
				int is_ready;
				int no_delete;
			} MediaStateChanged;

			// CMD_StartRecord
			struct {
				bool bEnableRecord;
			} StartRecord;

			// CMD_StopRecord

			// CMD_InitVideoStream
			struct {
				int stream;
				int video_type;
				avf_stream_attr_s stream_attr;
			} InitVideoStream;

			// CMD_PostIO
			struct {
				IAVIO *io;
				ITSMapInfo *map_info;
				int stream;
			} PostIO;

			// CMD_StartSegment
			struct {
				avf_time_t video_time;
				avf_time_t picture_time;
				int stream;
			} StartSegment;

			// CMD_EndSegment
			struct {
				avf_u64_t next_start_ms;
				avf_u32_t fsize;
				int stream;
				bool bLast;
			} EndSegment;

			// CMD_AddVideo
			struct {
				avf_u64_t pts;
				avf_u32_t timescale;
				avf_u32_t fpos;
				int stream;
			} AddVideo;

			// CMD_AddPicture
			struct {
				CBuffer *pBuffer;
				int stream;
			} AddPicture;

			// CMD_AddRawData
			struct {
				raw_data_t *raw;
				int stream;
			} AddRawData;

			// CMD_SetConfigData
			struct {
				raw_data_t *raw;
			} SetConfigData;

			// CMD_AddRawDataEx
			struct {
				raw_data_t *raw;
			} AddRawDataEx;

			// CMD_PostRawDataFreq
			struct {
				int interval_gps;
				int interval_acc;
				int interval_obd;
			} PostRawDataFreq;

			// CMD_RefreshPictureList

			// CMD_PictureTaken
			struct {
				int code;
			} PictureTaken;

		} u;

		vdb_man_cmd_s(int code): cmd_code(code) {}
		vdb_man_cmd_s() {}

	} vdb_man_cmd_t;

public:
	static CVDBManager* Create(IRegistry *pRegistry, avf_app_msg_callback callback, void *context);

private:
	CVDBManager(IRegistry *pRegistry, avf_app_msg_callback callback, void *context);
	virtual ~CVDBManager();

// IVDBControl
public:
	virtual void StorageReady(int is_ready);
	virtual avf_status_t RegisterStorageListener(IStorageListener *listener, bool *pbReady);
	virtual avf_status_t UnregisterStorageListener(IStorageListener *listener);
	virtual bool IsStorageReady();
	virtual bool CheckStorageSpace();
	virtual void GetVdbInfo(vdb_info_s *info, int free_vdb_space);
	virtual avf_status_t SetRecordAttr(const char *attr_str);
	virtual avf_status_t MarkLiveClip(int delay_ms, int before_live_ms, int after_live_ms, int b_continue, int live_mark_flag, const char *attr_str);
	virtual avf_status_t StopMarkLiveClip();
	virtual avf_status_t GetMarkLiveInfo(mark_info_s& info);
	virtual avf_status_t EnablePictureService(const char *picture_path);
	virtual avf_status_t RefreshPictureList();
	virtual avf_status_t PictureTaken(int code);
	virtual avf_status_t GetNumPictures(int *num);
	virtual avf_status_t GetSpaceInfo(avf_u64_t *total_space, avf_u64_t *used_space, avf_u64_t *marked_space, avf_u64_t *clip_space);
	virtual avf_status_t SetClipSceneData(int clip_type, avf_u32_t clip_id, const avf_u8_t *data, avf_size_t data_size);

// IActiveObject
public:
	virtual const char *GetAOName() {
		return mName->string();
	}
	virtual void AOLoop();
	virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);

// IRecordObserver
public:
	virtual int GetSegmentLength();
	virtual avf_status_t StartRecord(bool bEnableRecord);
	virtual void StopRecord();

	virtual string_t *CreateVideoFileName(avf_time_t *gen_time, int stream, const char *ext);
	virtual string_t *CreatePictureFileName(avf_time_t *gen_time, int stream);

	virtual void InitVideoStream(const avf_stream_attr_s *stream_attr, int video_type, int stream);
	virtual void PostIO(IAVIO *io, ITSMapInfo *map_info, int stream);
	virtual void StartSegment(avf_time_t video_time, avf_time_t picture_time, int stream);
	virtual void EndSegment(avf_u64_t next_start_ms, avf_u32_t fsize, int stream, bool bLast);

	virtual void AddVideo(avf_u64_t pts, avf_u32_t timescale, avf_u32_t fpos, int stream);
	virtual void AddPicture(CBuffer *pBuffer, int stream);
	virtual void AddRawData(raw_data_t *raw, int stream);
	virtual void SetConfigData(raw_data_t *raw);
	virtual void AddRawDataEx(raw_data_t *raw);

	virtual void PostRawDataFreq(int interval_gps, int interval_acc, int interval_obd);

// IPictureListener
public:
	virtual void OnListScaned(int total);
	virtual void OnListCleared();
	virtual void OnPictureTaken(string_t *picname, avf_u32_t picture_date);
	virtual void OnPictureRemoved(string_t *picname);

private:
	bool ShouldAutoDelete();
	void AppCallback(avf_uint_t msg_code);
	void NotifyStorageState(int is_ready);
	avf_u64_t GetAutoDelFree();
	avf_u64_t GetAutoDelMin();

private:
	IRegistry *mpRegistry;
	avf_app_msg_callback m_app_msg_callback;
	void *m_app_context;
	ap<string_t> mName;
	CConfig *mpConfig;

	char m_mount_point[REG_STR_MAX];

	avf_u64_t m_check_space_tick;

	CMutex mMutex;
	bool mbStorageReady;
	IStorageListener *mpStorageListeners[2];

	CVDB *mpVDB;
	CVDBServer *mpVdbServer;
	CVdbHttpServer *mpHttpServer;

	CWorkQueue *mpWorkQ;
	CMsgQueue *mpMsgQ;	// from IRecordObserver & IVDBControl

private:
	static const vdb_set_s m_vdb_set;

private:
	static CVDB *get_vdb(void *context, const char *vdb_id);
	static void release_vdb(void *context, const char *vdb_id, CVDB *vdb);
	void SendPictureTaken(CPictureList *pPictureList, int code);
	int ParseClipAttr(const char *attr_str);

private:
	INLINE void PostCmd(vdb_man_cmd_t& cmd) {
		avf_status_t status = mpMsgQ->PostMsg(&cmd, sizeof(cmd));
		AVF_ASSERT_OK(status);
	}

	INLINE void PostCmd(int code) {
		vdb_man_cmd_t cmd(code);
		PostCmd(cmd);
	}

	INLINE avf_status_t SendCmd(vdb_man_cmd_t& cmd) {
		return mpMsgQ->SendMsg(&cmd, sizeof(cmd));
	}

	INLINE void ReplyCmd(avf_status_t status) {
		mpMsgQ->ReplyMsg(status);
	}

	void HandleCmd(vdb_man_cmd_t& cmd);
};

#endif

