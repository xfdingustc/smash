
#ifndef __VDB_SERVER_H__
#define __VDB_SERVER_H__

#include "avf_tcp_server.h"
#include "timer_manager.h"
#include "picture_list.h"

class CTimerManager;
class CVDBConnection;
class CVDBServer;
class CVDBBroadcast;
class CVDB;
class CMemBuffer;
class CSocket;
class CSocketEvent;
struct vdb_set_s;

class CPictureList;

//-----------------------------------------------------------------------
//
//  CVDBConnection
//
//-----------------------------------------------------------------------
class CVDBConnection: public CTCPConnection
{
	typedef CTCPConnection inherited;
	friend class CVDBServer;

	enum {
#if defined(WIN32_OS) || defined(MAC_OS)
		MAX_PLAYBACK_LENGTH = 100*60*60*1000,	// 100 hours
#else
		MAX_PLAYBACK_LENGTH = 30*60*1000,	// 30 minutes
#endif
	};

public:
	static CVDBConnection *Create(CVDBServer *pServer, CSocket **ppSocket,
		const vdb_set_s *vdb_set, void *context);

private:
	CVDBConnection(CVDBServer *pServer, CSocket **ppSocket,
		const vdb_set_s *vdb_set, void *context);
	virtual ~CVDBConnection();

private:
	virtual void ThreadLoop();
	avf_status_t ProcessCmd();

private:
	avf_status_t DeflateAckLocked();
	avf_status_t InflateAckLocked();
	avf_status_t DeflateMemBufferLocked(void *_stream, CMemBuffer *from, int last, 
		CMemBuffer *to, avf_u8_t **pptr, avf_size_t *remain_bytes);
	avf_status_t InflateMemBufferLocked(void *_stream, CMemBuffer *from, CMemBuffer *to);

private:
	avf_status_t WriteAckLocked(const vdb_cmd_header_t *cmd, avf_status_t ret_code);
	avf_status_t WriteBaseAckLocked(const vdb_cmd_header_t *cmd, avf_status_t ret_code);
	avf_status_t WriteBufferLocked(const avf_u8_t *buffer, avf_uint_t size, bool bCheckClose);
	avf_status_t WritePaddingLocked(avf_uint_t size, bool bCheckClose);

private:
	avf_status_t BroadcastMsg(void *buf, int size, const avf_u8_t *p_vdb_id, int len);
	avf_status_t BroadcastRawData(vdb_msg_RawData_t *param, raw_data_t *raw,
		int size, const avf_u8_t *p_vdb_id, int len);
	avf_status_t BroadcastPictureRemoved(string_t *picname);
	avf_status_t BroadcastPictureTaken(string_t *picname, avf_u32_t pic_date);
	avf_status_t BroadcastSpaceInfo(vdb_space_info_t *info);

private:
	avf_status_t HandleGetPlaybackUrl(CVDB *pvdb, vdb_cmd_GetPlaybackUrl_t *cmd, avf_u32_t length_ms);
	avf_status_t HandleGetPlaylistPlaybackUrl(CVDB *pvdb, vdb_cmd_GetPlaylistPlaybackUrl_t *cmd);
	avf_status_t HandleGetUploadUrl(CVDB *pvdb, vdb_cmd_GetUploadUrl_t *cmd);
	avf_status_t HandleGetDownloadUrl(CVDB *pvdb, vdb_cmd_GetDownloadUrl_t *cmd);
	avf_status_t HandleGetDownloadUrlEx(CVDB *pvdb, vdb_cmd_GetDownloadUrlEx_t *cmd);

private:
	avf_status_t HandleDownloadStreamUrl(CVDB *pvdb, vdb_cmd_GetDownloadUrlEx_t *cmd, 
		int stream, bool bDownloadPlaylist, uint32_t *download_opt);
	void WriteDownloadUrl(int clip_type, avf_u32_t clip_id, CVDB::clip_info_s *info, int stream, 
		int b_playlist, int b_mute_audio, int url_type);
	avf_status_t GetDownloadIndexPicture(CVDB *pvdb, int clip_type, avf_u32_t clip_id,
		avf_u32_t clip_time_ms_lo, avf_u32_t clip_time_ms_hi, bool bPlaylist);

private:
	avf_status_t HandleGetAllClipSetInfo();
	avf_status_t HandleGetClipPoster(vdb_cmd_GetIndexPicture_t *cmd);

private:
	CPictureList *GetPictureList();
	static const char *GetPictureFileName(vdb_picture_name_t *pname);
	avf_status_t ReadPictureData(vdb_cmd_ReadPicture_t *cmd, CPictureList *piclist, const char *picname);

private:
	avf_u32_t m_seqid;
	const vdb_set_s *mp_vdb_set;
	void *mp_vdb_context;
	avf_u32_t mRawDataFlags;
	avf_uint_t m_hls_seg_length;

// per connection
private:
	CMutex mWriteMutex;
	CMemBuffer *mpMemBuffer;
	CMemBuffer *mpData;
	vdb_ack_t *mpAck;
	char *mp_vdb_id;	// in m_cmd_buffer
	CVDB *mp_vdb;
	avf_u8_t m_cmd_buffer[VDB_CMD_SIZE];

private:
	CVDB *GetVDB(const avf_u8_t *base, const avf_u8_t *ptr, int line);
	avf_status_t WriteVdbId(CMemBuffer *pmb);
	INLINE void ReleaseVDB() {
		if (mp_vdb != NULL) {
			mp_vdb_set->release_vdb(mp_vdb_context, mp_vdb_id, mp_vdb);
			mp_vdb = NULL;
		}
	}
};

//-----------------------------------------------------------------------
//
//  CVDBServer
//
//-----------------------------------------------------------------------
class CVDBServer: public CTCPServer, public IVDBBroadcast, public IPictureListener
{
	typedef CTCPServer inherited;
	friend class CVDBConnection;
	DEFINE_INTERFACE;

public:
	static CVDBServer *Create(const vdb_set_s *vdb_set, void *context);

protected:
	CVDBServer(const vdb_set_s *vdb_set, void *context);
	virtual ~CVDBServer();

private:
	enum {
		MAX_CONNECTION = 4,
		TIMEOUT = 3000, // ms
	};

	enum {
		INFO_Null = 0,

		INFO_VdbReady = 1,
		INFO_VdbUnmounted = 2,

		INFO_ClipInfo = 3,
		INFO_ClipRemoved = 4,

		INFO_BufferSpaceLow = 5,
		INFO_BufferFull = 6,

		INFO_SendRawData = 7,
		INFO_PlaylistCleared = 8,
		INFO_PlaylistCreated = 9,
		INFO_PlaylistDeleted = 10,

		INFO_RefreshPictureList = 11,
		INFO_PictureTaken = 12,
		INFO_PictureRemoved = 13,

		INFO_SpaceInfo = 14,
		INFO_ClipAttrChanged = 15,
	};

	typedef struct cmd_s {
		int cmd_code;
		string_t *vdb_id;

		union {
			// INFO_VdbReady
			struct {
				int32_t status;
			} VdbReady;

			// INFO_VdbUnmounted

			// INFO_ClipInfo
			struct {
				avf_u16_t action;
				avf_u16_t clip_attr;
				avf_u32_t list_pos;
				///
				avf_u32_t clip_type;
				avf_u32_t clip_id;
				avf_u32_t clip_date;
				avf_u32_t clip_duration_ms;
				avf_u64_t clip_start_time_ms;
				///
				avf_u32_t num_streams;
				avf_stream_attr_s stream_info[MAX_VDB_STREAMS];

				// for MarkClipInfo
				avf_u8_t mark_info_valid;
				avf_s32_t delay_ms;
				avf_s32_t before_live_ms;
				avf_s32_t after_live_ms;
			} ClipInfo;

			// INFO_ClipRemoved
			struct {
				avf_u32_t clip_type;
				avf_u32_t clip_id;
			} ClipRemoved;

			// INFO_BufferSpaceLow
			// INFO_BufferFull

			// INFO_SendRawData
			struct {
				raw_data_t *raw;
			} SendRawData;

			// INFO_PlaylistCleared
			// INFO_PlaylistCreated
			// INFO_PlaylistDeleted
			struct {
				avf_u32_t list_type;
			} PlaylistParam;

			// INFO_RefreshPictureList

			// INFO_PictureTaken
			struct {
				string_t *picname;
				avf_u32_t pic_date;
			} PictureTaken;

			// INFO_PictureRemoved
			struct {
				string_t *picname;
			} PictureRemoved;

			// INFO_SpaceInfo
			struct {
				avf_u64_t total_bytes;
				avf_u64_t used_bytes;
				avf_u64_t marked_bytes;
			} SpaceInfo;

			// INFO_ClipAttrChanged
			struct {
				avf_u32_t clip_type;
				avf_u32_t clip_id;
				avf_u32_t clip_attr;
			} ClipAttrChanged;
			
		} u;

		cmd_s(int cmd_code, string_t *vdb_id) {
			this->cmd_code = cmd_code;
			this->vdb_id = vdb_id ? vdb_id->acquire() : NULL;
		}

		cmd_s() {
			this->cmd_code = 0;
			this->vdb_id = NULL;
		}
	} cmd_t;

// CTCPServer
protected:
	virtual CTCPConnection *CreateConnection(CSocket **ppSocket) {
		return CVDBConnection::Create(this, ppSocket, &m_vdb_set, mp_vdb_context);
	}

// IVDBBroadcast
public:
	virtual avf_status_t VdbReady(string_t *vdb_id, int status);
	virtual avf_status_t VdbUnmounted(string_t *vdb_id);
	virtual avf_status_t ClipInfo(string_t *vdb_id, int action, ref_clip_t *ref_clip, mark_info_s *mark_info);
	virtual avf_status_t ReportSpaceInfo(string_t *vdb_id, avf_u64_t total_bytes, avf_u64_t used_bytes, avf_u64_t marked_bytes);
	virtual avf_status_t ClipRemoved(string_t *vdb_id, avf_u32_t clip_type, avf_u32_t clip_id);
	virtual avf_status_t BufferSpaceLow(string_t *vdb_id);
	virtual avf_status_t BufferFull(string_t *vdb_id);
	virtual avf_status_t SendRawData(string_t *vdb_id, raw_data_t *raw);
	virtual avf_status_t PlaylistCleared(string_t *vdb_id, avf_u32_t list_type);
	virtual avf_status_t PlaylistCreated(string_t *vdb_id, avf_u32_t list_type);
	virtual avf_status_t PlaylistDeleted(string_t *vdb_id, avf_u32_t list_type);
	virtual avf_status_t ClipAttrChanged(string_t *vdb_id, avf_u32_t clip_type, avf_u32_t clip_id, avf_u32_t clip_attr);

private:
	avf_status_t PlaylistAction(string_t *vdb_id, int action, avf_u32_t list_type);

// IPictureListener
public:
	virtual void OnListScaned(int total);
	virtual void OnListCleared();
	virtual void OnPictureTaken(string_t *picname, avf_u32_t pic_date);
	virtual void OnPictureRemoved(string_t *picname);

public:
	avf_status_t Start();
	CPictureList *CreatePictureList(const char *picture_path);
	CPictureList *GetPictureList();

public:
	avf_status_t RefreshPictureList();
	avf_status_t PictureTaken(string_t *picname, avf_u32_t pic_date);
	avf_status_t PictureRemoved(string_t *picname);

private:
	void BroadcastMsg(string_t *vdb_id, int code, void *buf, int size);
	void BroadcastRawData(string_t *vdb_id, raw_data_t *raw);
	void BroadcastPictureTaken(string_t *picname, avf_u32_t pic_date);
	void BroadcastPictureRemoved(string_t *picname);
	void BroadcastSpaceInfo(string_t *vdb_id, avf_u64_t total_bytes, avf_u64_t used_bytes, avf_u64_t marked_bytes);

private:
	void CopyVdbId(string_t *vdb_id, avf_u8_t **p_vdb_id, int *len);

private:
	void ServerLoop();
	static void ThreadEntry(void *p);

private:
	vdb_set_s m_vdb_set;
	void *mp_vdb_context;
	avf_u8_t m_vdb_id_buf[VDB_ACK_SIZE];

private:
	CMsgQueue *mpMsgQueue;
	CThread *mpThread;
	CPictureList *mpPictureList;
};

#endif

