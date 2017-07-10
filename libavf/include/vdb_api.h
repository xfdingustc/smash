
#ifndef __VDB_API_H__
#define __VDB_API_H__


extern "C" {

typedef struct VDB_s VDB;

#define CAMERA_ID_SZ	64
typedef char camera_id_t[CAMERA_ID_SZ];
typedef uint64_t vdb_clip_id_t;
typedef void *vdb_clip_reader_t;

#define MAKE_VDB_CLIP_ID(_dirname, _clip_id) \
	MK_U64(_clip_id, _dirname)

#define GET_VDB_CLIP_ID(_vdb_clip_id, _dirname, _clip_id) \
	do { \
		_clip_id = U64_LO(_vdb_clip_id); \
		_dirname = U64_HI(_vdb_clip_id); \
	} while (0)


enum {
	VDB_CB_Null,
	VDB_CB_ClipCreated = 1,
	VDB_CB_ClipFinished = 2,
	VDB_CB_ClipRemoved = 3,
};

typedef struct gps_range_s {
	double latitude_min;
	double latitude_max;
	double longitude_min;
	double longitude_max;
	double altitude_min;
	double altitude_max;
	int range_is_valid;
} gps_range_t;

typedef struct VDB_ClipCreatedMsg_s {
	camera_id_t camera_id;
	vdb_clip_id_t clip_id;
	uint32_t clip_date;	// seconds since 1970, local time
	int32_t gmtoff;		// GMT offset
} VDB_ClipCreatedMsg_t;

typedef struct VDB_ClipFinishedMsg_s {
	camera_id_t camera_id;
	vdb_clip_id_t clip_id;
	uint32_t clip_date;	// seconds since 1970, local time
	int32_t gmtoff;		// GMT offset
	uint32_t duration_ms;
	gps_range_t gps_range;
	// TODO: - clip size info
} VDB_ClipFinishedMsg_t;

typedef struct VDB_ClipRemovedMsg_s {
	camera_id_t camera_id;
	vdb_clip_id_t clip_id;
} VDB_ClipRemovedMsg_t;

typedef struct vdbs_clip_info_s {
	uint32_t clip_date;			// seconds from 1970 (when the clip was created)
	int32_t gmtoff;
	uint32_t clip_duration_ms;
	uint64_t clip_start_time_ms;
	uint16_t num_streams;
	uint16_t reserved;
	avf_stream_attr_t stream_info[2];
} vdbs_clip_info_t;


typedef void (*vdb_cb)(void *ctx, int msg, void *msg_data);

VDB *VDB_Open(const char *path, int create);
void VDB_SetCallback(VDB *vdb, vdb_cb cb, void *ctx, const char *camera_id);
void VDB_Close(VDB *vdb);
//int VDB_Delete(const char *path);

int VDB_StartRecord(VDB *vdb, int has_video, int has_picture); // same with VDB_StartRecordEx(ch=0);
int VDB_StopRecord(VDB *vdb); // same with VDB_StopRecordEx(ch=0);
int VDB_AddData(VDB *vdb, const uint8_t *data, int size); // same with VDB_AddDataEx(ch=0);

// ch: 0, or 1 that can be used for live recording & uploading, respectively
int VDB_StartRecordEx(VDB *vdb, int has_video, int has_picture, int ch);
int VDB_StopRecordEx(VDB *vdb, int ch);
int VDB_AddDataEx(VDB *vdb, const uint8_t *data, int size, int ch);

int VDB_GetDataSize(VDB *vdb, uint64_t *video_size, uint64_t *picture_size, uint64_t *raw_data_size);
int VDB_GetTotalClips(VDB *vdb);

int VDB_RemoveClip(VDB *vdb, vdb_clip_id_t clip_id);

int VDB_GetClipInfo(VDB *vdb, vdb_clip_id_t clip_id, vdbs_clip_info_t *info);

int VDB_GetIndexPicture(VDB *vdb, vdb_clip_id_t clip_id, void **pdata, int *psize);
int VDB_GetIndexPictureAtPos(VDB *vdb, vdb_clip_id_t clip_id, uint64_t clip_time_ms,
	void **pdata, int *psize, uint64_t *clip_time_ms_out);
void VDB_FreeIndexPicture(void *data);

typedef int (*vdb_enum_raw_data_proc)(void *ctx,
	uint64_t seg_stms, int num_items,
	const raw_dentry_t *dentries, const void *data, uint32_t data_size);

// data_type: avf_std_media.h, RAW_DATA_GPS, RAW_DATA_OBD, RAW_DATA_ACC
int VDB_EnumRawData(VDB *vdb, vdb_enum_raw_data_proc proc, void *ctx,
	vdb_clip_id_t clip_id, int data_type, 
	uint64_t clip_time_ms, uint32_t length_ms);

// result is allocated by vdb, should call VDB_FreeRawData() to free it
// data_type: avf_std_media.h, RAW_DATA_GPS, RAW_DATA_OBD, RAW_DATA_ACC
int VDB_ReadRawData(VDB *vdb, vdb_clip_id_t clip_id, int data_type, 
	uint64_t clip_time_ms, uint32_t length_ms, vdb_raw_data_t *result);
void VDB_FreeRawData(vdb_raw_data_t *result);

// return playback address.m3u8
// full address is like http://192.168.110.1:8085/[camera_id]/address.m3u8
int VDB_GetPlaybackUrl(VDB *vdb, vdb_clip_id_t clip_id, uint64_t clip_time_ms, char address[200]);

// get playback url of stream 2
int VDB_GetPlaybackUrlEx(VDB *vdb, vdb_clip_id_t clip_id, uint64_t clip_time_ms,
	int stream, int b_mute_audio, uint32_t param1, uint32_t param2,
	char address[200]);

// content of .m3u8 file - buffer is allocated by vdb
int VDB_ReadHLS(VDB *vdb, const char *address, void **pdata, int *psize);
void VDB_ReleaseHLSData(VDB *vdb, void *data);

// for reading .ts
vdb_clip_reader_t VDB_CreateClipReader(VDB *vdb, const char *url, uint64_t start_offset);
void VDB_ReleaseClipReader(VDB *vdb, vdb_clip_reader_t reader);

uint64_t VDB_ClipReader_GetSize(vdb_clip_reader_t reader);
uint64_t VDB_ClipReader_GetPos(vdb_clip_reader_t reader);

// return value:
//		>= 0 : bytes read
//		< 0 : error code
int VDB_ClipReader_ReadClip(vdb_clip_reader_t reader, uint8_t *buffer, int size);

// 0: SEEK_SET; 1: SEEK_CUR; 2: SEEK_END
int VDB_ClipReader_Seek(vdb_clip_reader_t reader, int64_t pos, int whence);


// for debug use
int VDB_DumpData(VDB *vdb, const char *filename);

void VDB_EnableLogs(const char *logs);

struct clip_list_s;
clip_list_s *VDB_GetAllClipsInfo(VDB *vdb);


};

#endif

