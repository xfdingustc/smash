
#ifndef __VDB_LOCAL_H__
#define __VDB_LOCAL_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vdb_local_s vdb_local_t;
typedef struct vdb_local_server_s vdb_server_t;
typedef struct vdb_local_http_server_s vdb_http_server_t;
typedef void *vdb_local_clip_reader_t;
typedef void *vdb_local_reader_t;

typedef struct vdb_local_ack_s {
	uint8_t *data;
	uint32_t size;
} vdb_local_ack_t;

// vdb_server should be created before the vdb,
// and released after all vdb objects are destroyed
// vdb_server can be NULL, in this case, no notifications can be sent.
vdb_local_t *vdb_local_create(vdb_server_t *vdb_server);
int vdb_local_destroy(vdb_local_t *vdb);

// vdb_root: "/mnt/sdcard/100TRANS/"
int vdb_local_load(vdb_local_t *vdb, const char *vdb_root, int b_create, const char *id);
int vdb_local_unload(vdb_local_t *vdb);

// total_space - 0
// used_space - 0
// marked_space - used by non-deletable clips
// clip_space - all clips
int vdb_local_get_space_info(vdb_local_t *vdb, uint64_t *total_space, uint64_t *used_space, 
	uint64_t *marked_space, uint64_t *clip_space);


void vdb_local_free_ack(vdb_local_t *vdb, vdb_local_ack_t *ack);

// example:
/*
	vdb_local_ack_t ack;
	vdb_local_get_clip_set_info(vdb, CLIP_TYPE_BUFFER, &ack);
	// parse ack
	vdb_local_free_ack(vdb, &ack);
*/

// ack - vdb_ack_GetClipSetInfoEx_t
// should call vdb_local_free_ack() to free ack
// flags: GET_CLIP_EXTRA | GET_CLIP_OBDVIN
int vdb_local_get_clip_set_info(vdb_local_t *vdb, int clip_type, int flags, vdb_local_ack_t *ack);

// ack - vdb_clip_info_ex_t
int vdb_local_get_clip_info(vdb_local_t *vdb, int clip_type, uint32_t clip_id, 
	int flags, vdb_local_ack_t *ack);

// ack - vdb_ack_GetIndexPicture_t
// should call vdb_local_free_ack() to free ack
int vdb_local_get_index_picture(vdb_local_t *vdb, int clip_type, 
	uint32_t clip_id, uint64_t clip_time_ms, vdb_local_ack_t *ack);

// ack - vdb_ack_GetRawData_t
// should call vdb_local_free_ack() to free ack
// data_types: (1<<kRawData_GPS) | (1<<kRawData_ACC) | (1<<kRawData_OBD)
int vdb_local_get_raw_data(vdb_local_t *vdb, int clip_type, uint32_t clip_id, 
	uint64_t clip_time_ms, int data_types, vdb_local_ack_t *ack);

// return raw data size int the specified range of the clip
// if returns *size == 0, then no raw data exists
int vdb_local_get_raw_data_size(vdb_local_t *vdb, int clip_type, uint32_t clip_id, 
	uint64_t clip_time_ms, uint32_t length_ms, uint32_t data_type, uint32_t *size);

int vdb_local_get_raw_data_block(vdb_local_t *vdb, int clip_type, uint32_t clip_id,
	uint64_t clip_time_ms, uint32_t length_ms, uint32_t data_type, vdb_local_ack_t *ack);

int vdb_local_get_playback_info(vdb_local_t *vdb,
	int clip_type, uint32_t clip_id, int stream, int url_type,
	uint64_t clip_time_ms, uint32_t length_ms,
	uint64_t *real_time_ms, uint32_t *real_length_ms);

// url_type: URL_TYPE_TS (.ts) or URL_TYPE_HLS (.m3u8)
int vdb_local_get_playback_url(const char *vdb_id,
	int clip_type, uint32_t clip_id, int stream, int url_type, 
	uint64_t clip_time_ms, uint32_t length_ms,
	char url[256]);

// if (url_type & _URL_TYPE_MASK) == URL_TYPE_HLS, param1 specifies segment length
// if url_type & URL_FULL_PATH, the m3u8 of url will contain full path of ts file names
// if url_type & URL_SPEC_TIME, param2 specifies the init start time
int vdb_local_get_playback_url_ex(const char *vdb_id,
	int clip_type, uint32_t clip_id, int stream, int url_type, 
	uint64_t clip_time_ms, uint32_t length_ms, uint32_t param1, uint32_t param2,
	char url[256]);

// return values: e_DeleteClip_OK, e_DeleteClip_NotFound, e_DeleteClip_NotPermitted
// returns < 0 when other error happens
int vdb_local_delete_clip(vdb_local_t *vdb, int clip_type, uint32_t clip_id);

// return values: e_MarkClip_OK, e_MarkClip_BadParam
// p_clip_type, p_clip_id: return the created clip's type and id
int vdb_local_mark_clip(vdb_local_t *vdb, int clip_type, uint32_t clip_id,
	uint64_t clip_time_ms, uint32_t length_ms, int *p_clip_type, uint32_t *p_clip_id);

// set scene data to the specified clip
int vdb_local_set_clip_scene_data(vdb_local_t *vdb, int clip_type, uint32_t clip_id,
	const uint8_t *data, uint32_t data_size);

// ---------------------------------------------------------------
//
//	clip transfer APIs
//
// ---------------------------------------------------------------

#define VDB_LOCAL_TRANSFER_SCENE_DATA	(1 << 0)

// a ref clip to be transfered
typedef struct vdb_local_item_s {
	int clip_type;
	uint32_t clip_id;
	uint64_t start_time_ms;
	uint64_t end_time_ms;
	uint32_t flags;		// VDB_LOCAL_TRANSFER_SCENE_DATA
	uint32_t reserved;	// must be 0
} vdb_local_item_t;

// note: use vdb_local_item_t instead of vdb_transfer_item_t
typedef vdb_local_item_t vdb_transfer_item_t;

typedef struct vdb_local_size_s {
	uint64_t picture_size;
	uint64_t video_size;
	uint64_t index_size;
} vdb_local_size_t;

typedef struct vdb_transfer_info_s {
	vdb_local_size_t total;
	vdb_local_size_t transfered;
	int b_finished;
} vdb_transfer_info_t;

int vdb_transfer_init(vdb_local_t *vdb, const vdb_local_item_t *items, int nitems);
int vdb_transfer_start(vdb_local_t *vdb, vdb_local_t *dest_vdb);	// vdb -> dest_vdb
int vdb_transfer_cancel(vdb_local_t *vdb);
int vdb_transfer_get_info(vdb_local_t *vdb, vdb_transfer_info_t *info);

// info of transfered clips (buffered+marked) will be returned in ack.
// ack is vdb_ack_GetClipSetInfo_t with clip_type == CLIP_TYPE_UNSPECIFIED
//
// after this function is called, the result will be cleared from vdb
// should call vdb_local_free_ack() to free ack
int vdb_transfer_get_result(vdb_local_t *vdb, vdb_local_ack_t *ack);

// ---------------------------------------------------------------
//
//	remux APIs
//
// ---------------------------------------------------------------
int vdb_remux_set_audio(vdb_local_t *vdb, int bDisableAudio,
	const char *pAudioFilename, const char *format);

typedef struct vdb_remux_info_s {
	int percent; // TODO:
	int error;
	int b_running;
	uint64_t total_bytes;
	uint64_t remain_bytes;
} vdb_remux_info_t;

// estimate clip size
int vdb_remux_get_clip_size(vdb_local_t *vdb, const vdb_local_item_t *item,
	int stream, uint64_t *size_bytes);

// stream 0: 1080p
// stream 1: 512x288
// output_format should be "mp4"
// in case of error, caller should delete the (partial) output_filename
int vdb_remux_clip(vdb_local_t *vdb, const vdb_local_item_t *item,
	int stream, const char *output_filename, const char *output_format);

// stop remuxer before it finishes
int vdb_remux_stop(vdb_local_t *vdb, int b_delete_output_file);

int vdb_remux_get_info(vdb_local_t *vdb, vdb_remux_info_t *info);

// ---------------------------------------------------------------
//
//	server APIs
//
// ---------------------------------------------------------------

#ifdef WIN32_OS
// major 2, minor 2
extern "C" int avf_WSAStartup(int major, int minor);
extern "C" int avf_WSACleanup(void);
#endif

typedef struct local_vdb_set_s
{
	// request the vdb_local_t object, given its vdb_id; ref count should be increased
	vdb_local_t *(*get_vdb)(void *context, const char *vdb_id);

	// release vdb_local_t object; ref count should be decreased
	void (*release_vdb)(void *context, const char *vdb_id, vdb_local_t *vdb);

	// get all clips in one vdb; return 0 on sucess, return -1 on error
	int (*get_all_clips)(void *context, uint32_t *p_num_clips, vdb_clip_desc_t **p_clips);

	// release memory for get_all_clips
	void (*release_clips)(void *context, uint32_t num_clips, vdb_clip_desc_t *clips);

	// get poster jpeg data of the specified give clip
	int (*get_clip_poster)(void *context, const char *vdb_id,
		int clip_type, uint32_t clip_id, uint32_t *p_size, void **p_data);

	// release memory for get_clip_poster
	void (*release_clip_poster)(void *context, const char *vdb_id, uint32_t size, void *data);

	// callback when a clip is created/deleted (probably by another client)
	void (*on_clip_created)(void *context, const char *vdb_id, int clip_type, uint32_t clip_id);
	void (*on_clip_deleted)(void *context, const char *vdb_id, int clip_type, uint32_t clip_id);

} local_vdb_set_t;


vdb_server_t *vdb_create_server(const local_vdb_set_t *vdb_set, void *vdb_context);
int vdb_destroy_server(vdb_server_t *server);
int vdb_run_server(vdb_server_t *server);


vdb_http_server_t *vdb_create_http_server(const local_vdb_set_t *vdb_set, void *vdb_context);
int vdb_destroy_http_server(vdb_http_server_t *server);
int vdb_run_http_server(vdb_http_server_t *server);

// ---------------------------------------------------------------
//
//	read clip data APIs - obsolete, use vdb_local_create_reader()
//	create a clip reader to read the specified range of data sequentially
//	data format: see upload_header_t for details
//
// ---------------------------------------------------------------

// return NULL on error
vdb_local_clip_reader_t vdb_local_create_clip_reader(vdb_local_t *vdb,
	const vdb_local_item_t *item);

void vdb_local_destroy_clip_reader(vdb_local_t *vdb, vdb_local_clip_reader_t reader);

// data_type: VDB_DATA_VIDEO, VDB_DATA_JPEG
// stream: set to 0 or 1 when data_type is VDB_DATA_VIDEO; otherwise set to 0
// return S_END on done (no more data)
// E_OK: if no data and no error, file is not created
int vdb_local_read_clip(vdb_local_clip_reader_t reader, int data_type, int stream, const char *filename);

// data_types: RAW_DATA_GPS_FLAG | RAW_DATA_OBD_FLAG | RAW_DATA_ACC_FLAG
// return S_END on done (no more data)
// E_OK: if no data and no error, *pdata is NULL
int vdb_local_read_clip_raw_data(vdb_local_clip_reader_t reader, int data_types, void **pdata, uint32_t *data_size);

int vdb_local_free_clip_raw_data(vdb_local_clip_reader_t reader, void *data);

// ---------------------------------------------------------------
//
//	clip reader for upload
//
// ---------------------------------------------------------------

// return NULL on error
// upload_opt: see vdb_cmd.h, UPLOAD_GET_V0 etc
vdb_local_reader_t vdb_local_create_reader(vdb_local_t *vdb,
	const vdb_local_item_t *item, int b_mute_audio, int upload_opt);

void vdb_local_destroy_reader(vdb_local_t *vdb, vdb_local_reader_t reader);

// len: size of buffer
// return value > 0 (may not equal len) on success
// return 0 on done
// return < 0 on error
int vdb_local_read(vdb_local_reader_t reader, uint8_t *buffer, int len);

typedef struct vdb_local_reader_info_s {
	uint64_t clip_time_ms;	// start time in the clip (absolute)
	uint32_t length_ms;		//
	uint64_t size;			// size in bytes
	uint64_t pos;			// current position
} vdb_local_reader_info_t;

int vdb_local_get_reader_info(vdb_local_reader_t reader, vdb_local_reader_info_t *info);

// ---------------------------------------------------------------
//
//	debug APIs
//
// ---------------------------------------------------------------

int vdb_local_clear_cache(vdb_local_t *vdb);

// logs: "ewivdp"
void vdb_local_enable_logs(const char *logs);
void set_print_proc(void (*p)(const char*));

#ifdef __cplusplus
}
#endif

#endif

