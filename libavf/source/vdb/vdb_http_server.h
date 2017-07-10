
#ifndef __VDB_HTTP_SERVER_H__
#define __VDB_HTTP_SERVER_H__

#include "avf_tcp_server.h"

class CVDB;
class CVdbHttpConnection;
class CVdbHttpServer;
class CNetworkService;
class CTCPServer;
class CMemBuffer;
class CSocket;
class CSocketEvent;
class CBufferIO;
struct vdb_set_s;

//-----------------------------------------------------------------------
//
//  CVdbHttpConnection
//
//-----------------------------------------------------------------------
class CVdbHttpConnection: public CTCPConnection
{
	typedef CTCPConnection inherited;

	enum {
		SEND_TIMEOUT = -1,
	};

public:
	static CVdbHttpConnection* Create(CVdbHttpServer *pServer, CSocket **ppSocket, 
		const vdb_set_s *vdb_set, void *context);

private:
	enum {
		TS_PACKET_SIZE = 188,
		REQ_BUFFER_SIZE = 2048,
		FILE_BUFFER_SIZE = 32*1024,
	};

	enum {
		HTTP_GET = 0,
		HTTP_HEAD,
		HTTP_PUT,
		HTTP_DELETE,
	};

	enum {
		ERROR_OK = 0,
		ERROR_BadRequest,
		ERROR_NotFound,
	};

	enum {
		RANGE_NONE = 0,
		RANGE_FROM_TO,
		RANGE_FROM,
		RANGE_TO,
	};

	typedef struct request_s {
		int code;	// HTTP_GET, etc
		char error;	// ERROR_OK, etc
		char keep_alive;
		char mute_audio;
		char fix_pts;
		char full_path;

		int url_type;
		int range_type;

		uint32_t param1;
		uint32_t param2;
		uint32_t upload_opt;
		uint64_t addr_from;
		uint64_t addr_to;
		uint64_t content_length;

		IVdbInfo::range_s range;
	} request_t;

	enum {
		CLIP_NONE,
		CLIP_INDEX,
		CLIP_POSTER,
		CLIP_PICTURE,
	};

	typedef struct clip_request_s {
		int code;
		uint32_t dirname;
		uint32_t clip_id;
		uint64_t clip_time_ms;
	} clip_request_t;

	// total 64 bytes
	typedef struct playlist_file_info_s {
		avf_u64_t filesize;
		avf_u64_t reserved[7];
	} playlist_file_info_t;
	
	typedef struct vdb_ack_mem_s {
		uint8_t *data;
		uint32_t size;
	} vdb_ack_mem_t;

	typedef struct clip_info_s {
		uint32_t clip_id;			// unique clip id
		uint32_t clip_date; 		// seconds from 1970 (when the clip was created)
		uint32_t clip_duration_ms;
		uint64_t clip_start_time_ms;
		uint16_t num_streams;
		uint16_t flags; 			// is 0; used by VDB_CMD_GetClipSetInfoEx
		/////
		avf_stream_attr_t stream_attr[2];
		/////
		int clip_type;
		uint8_t uuid[UUID_LEN];
		uint32_t ref_clip_date;
		int32_t gmtoff;
		uint32_t real_clip_id;
		/////
		uint32_t clip_attr;
		/////
		uint64_t clip_size;
	} clip_info_t;

private:
	CVdbHttpConnection(CVdbHttpServer *pServer, CSocket **ppSocket, 
		const vdb_set_s *vdb_set, void *context);
	virtual ~CVdbHttpConnection();

private:
	virtual void ThreadLoop();

private:
	avf_status_t ReadRequest();
	const char *ParseRequest(request_t *request);
	const char *ParseClipRequest(const char *url, clip_request_t *request);

private:
	avf_status_t SendResponseHeader(request_t *request, avf_u64_t offset, 
		avf_u64_t size, avf_u64_t file_size, int data_type);
	avf_status_t SendMemBuffer(CMemBuffer *pmb);
	avf_status_t SendMemBuffer(CMemBuffer *pmb, int data_type);

private:
	avf_status_t ServiceTS(request_t *request);
	avf_status_t ServiceHLS(request_t *request);
	avf_status_t ServiceClip(request_t *request);
	avf_status_t ServiceMP4(request_t *request);
	void ServiceFile(CVDB *pvdb, request_t *request, ap<string_t>& name);
	void ServicePlaylistFile(CVDB *pvdb, request_t *request, const char *name);
	void GetFile(request_t *request, ap<string_t>& filename);
	void PutFile(request_t *request, ap<string_t>& filename);
	void DeleteFile(request_t *request, ap<string_t>& filename);
	void DeleteAllFiles(CVDB *pvdb, request_t *request, avf_u32_t id);

private:
	static void StartScript(CMemBuffer *pmb);
	static void EndScript(CMemBuffer *pmb);
	void WriteHeader(CMemBuffer *pmb);
	void WriteNavBar(CMemBuffer *pmb);
	void WriteFooter(CMemBuffer *pmb);

private:
	void GenerateSpaceInfo(CVDB *pvdb, CMemBuffer *pmb);
	const uint8_t *GenerateStreamAttr(CMemBuffer *pmb, char *buffer, const uint8_t *ptr);
	const uint8_t *GenerateOneClip(CMemBuffer *pmb, int index, char *buffer, const uint8_t *ptr);
	void GenerateAllClips(CVDB *vdb, CMemBuffer *pmb);
	void FetchClipSet(CVDB *vdb, int clip_type, vdb_ack_mem_t& mem);

	void GenerateClipPoster(CVDB *vdb, CMemBuffer *pmb, const char *ptr);
	void GenerateClipPlaybackPage(CVDB *vdb, CMemBuffer *pmb, const char *ptr);

	void AppendFile(CMemBuffer *pmb, const char *filename);

private:
	static void ConvertToAckMem(int rval, CMemBuffer *pmb, vdb_ack_mem_t& mem);
	static void FreeAckMem(vdb_ack_mem_t& mem);

private:
	static const avf_u8_t *NextLine(const avf_u8_t *ptr);
	static int GetRange(const avf_u8_t *ptr, avf_u64_t *from, avf_u64_t *to);

private:
	void OK();
	void BadRequest();
	void NotFound();

private:
	void BufferString(const avf_u8_t *str, int len);
	INLINE void BufferString(const char *str, int len) {
		BufferString((const avf_u8_t*)str, len);
	}

private:
	const vdb_set_s *mp_vdb_set;
	void *mp_vdb_context;
	char *mp_vdb_id;
	bool mb_has_vhs;

	avf_uint_t m_buffer_len;	// not including tail 0
	avf_uint_t m_buffer_pos;
	avf_uint_t m_buffer_remain;
	avf_u8_t m_buffer[REQ_BUFFER_SIZE];

private:
	INLINE CVDB *GetVDB() {
		return mp_vdb_set->get_vdb(mp_vdb_context, mp_vdb_id);
	}
	INLINE void ReleaseVDB(CVDB *pvdb) {
		if (pvdb != NULL) {
			mp_vdb_set->release_vdb(mp_vdb_context, mp_vdb_id, pvdb);
		}
	}
};

//-----------------------------------------------------------------------
//
//  CVdbHttpServer
//
//-----------------------------------------------------------------------
class CVdbHttpServer: public CTCPServer
{
	friend class CVdbHttpConnection;
	typedef CTCPServer inherited;

public:
	static CVdbHttpServer* Create(const vdb_set_s *vdb_set, void *context);

private:
	CVdbHttpServer(const vdb_set_s *vdb_set, void *context);
	//virtual ~CVdbHttpServer();

public:
	INLINE avf_status_t Start() {
		return RunServer(VDB_HTTP_PORT);
	}

protected:
	virtual CTCPConnection *CreateConnection(CSocket **ppSocket) {
		return CVdbHttpConnection::Create(this, ppSocket, &m_vdb_set, mp_vdb_context);
	}

private:
	vdb_set_s m_vdb_set;
	void *mp_vdb_context;
};

#endif

