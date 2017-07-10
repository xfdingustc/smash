
#ifndef __VDB_CMD_H__
#define __VDB_CMD_H__

//-----------------------------------------------------------------------
//
//	1. each cmd is 128 bytes (padded with 0);
//	2. for each ack, there's a 16-byte header (vdb_ack_t);
//	3. if ack's size is less than 128 bytes, it's padded to 128 with 0;
//	4. if ack's size is more than 128 bytes, it's sent 128 bytes first, and then
//		a variable size block. The block's size is specified in vdb_ack_t.extra_bytes
//
//-----------------------------------------------------------------------
#ifndef uint32_t
typedef unsigned int uint32_t;
#endif
#ifndef int32_t
typedef int int32_t;
#endif
#ifndef uint64_t
typedef unsigned long long uint64_t;
#endif
#ifndef uint16_t
typedef unsigned short uint16_t;
#endif

#define VDB_CMD_PORT	8083
#define VDB_HTTP_PORT	8085

#define VDB_BUFFER_DIR		"/buffer/"
#define VDB_MARKED_DIR		"/marked/"
#define VDB_HLS_DIR		"hls"
#define VIDEO_FILE_EXT		".ts"
#define HLS_FILE_EXT		".m3u8"
#define PICTURE_FILE_EXT	".jpg"

// little endian format

#define VDB_CMD_SIZE	128
#define VDB_ACK_SIZE	128
#define VDB_MSG_SIZE	VDB_ACK_SIZE

enum {
	VDB_CMD_Null,

	VDB_CMD_GetDiscInfo,
	VDB_CMD_GetClipSetInfo,
	VDB_CMD_GetIndexPicture,
	VDB_CMD_GetPlaybackUrl,
	VDB_CMD_GetDownloadUrl,
	VDB_CMD_MarkClip,
	VDB_CMD_GetCopyState,
	VDB_CMD_DeleteClip,
	VDB_CMD_GetRawData,
	VDB_CMD_SetRawDataOption,
	VDB_CMD_GetRawDataBlock,

	VDB_MSG_VdbReady = 0x1000,
	VDB_MSG_VdbUnmounted,
	VDB_MSG_ClipCreated,
	VDB_MSG_ClipChanged,
	VDB_MSG_ClipRemoved,
	VDB_MSG_BufferSpaceLow,
	VDB_MSG_BufferFull,
	VDB_MSG_CopyState,
	VDB_MSG_RawData,
};

typedef struct vdb_cmd_header_s {
	int32_t cmd_code;
	uint32_t cmd_tag;	// by user
} vdb_cmd_header_t;

// 16-byte ack header
// for each ACK
typedef struct vdb_ack_s {
	int32_t cmd_code;
	uint32_t cmd_tag;
	int32_t ret_code;		// 0: success
	uint32_t extra_bytes;	// bytes after the 128-byte header
} vdb_ack_t;

// clip types
//#define CLIP_TYPE_BUFFER	0
//#define CLIP_TYPE_MARKED	1

// url type
#define URL_TYPE_TS		0
#define URL_TYPE_HLS		1

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetDiscInfo - get disc info
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetDiscInfo_s {
	vdb_cmd_header_t header;	// must be first
} vdb_cmd_GetDiscInfo_t;

typedef struct vdb_clip_set_info_s {
	uint64_t space_bytes_quota;
	uint64_t used_space_bytes;
	uint32_t total_clips;
	uint32_t reserved;
} vdb_clip_set_info_t;

typedef struct vdb_ack_GetDiscInfo_s {
	uint64_t total_space_bytes;
	uint64_t free_space_bytes;
	vdb_clip_set_info_t clip_set_buffer;
	vdb_clip_set_info_t clip_set_marked;
} vdb_ack_GetDiscInfo_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetClipSetInfo - get clip set info
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetClipSetInfo_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
} vdb_cmd_GetClipSetInfo_t;

typedef struct vdb_clip_info_s {
	uint32_t clip_id;
	uint32_t clip_date;	// seconds from 1970 (when the clip was created)
	uint32_t clip_duration_ms;
	uint32_t clip_start_time_ms_lo;
	uint32_t clip_start_time_ms_hi;
} vdb_clip_info_t;

typedef struct vdb_ack_GetClipSetInfo_s {
	uint32_t clip_type;
	uint32_t total_clips;
	uint32_t total_length_ms;
	uint32_t live_clip_id;
	// follows vdb_clip_info_t clip_info[total_clips]
} vdb_ack_GetClipSetInfo_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetIndexPicture - get index picture in a clip
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetIndexPicture_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t user_data;
	uint32_t clip_time_ms_lo;	// clip_start_time + time_offset
	uint32_t clip_time_ms_hi;
} vdb_cmd_GetIndexPicture_t;

typedef struct vdb_ack_GetIndexPicture_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_date;		// seconds from 1970 (when the clip was created)
	uint32_t user_data;
	uint32_t requested_time_ms_lo;	// requested
	uint32_t requested_time_ms_hi;
	uint32_t clip_time_ms_lo;		// real
	uint32_t clip_time_ms_hi;
	uint32_t duration;
	uint32_t picture_size;
	// follows uint8_t jpeg[picture_size]
} vdb_ack_GetIndexPicture_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetPlaybackUrl - get clip playback url
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetPlaybackUrl_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t url_type;			// URL_TYPE_TS, URL_TYPE_HLS
	uint32_t clip_time_ms_lo;	// clip_start_time + time_offset
	uint32_t clip_time_ms_hi;
} vdb_cmd_GetPlaybackUrl_t;

typedef struct vdb_ack_GetPlaybackUrl_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t url_type;			// URL_TYPE_TS, URL_TYPE_HLS
	uint32_t real_time_ms_lo;
	uint32_t real_time_ms_hi;
	uint32_t length_ms;
	uint32_t has_more;			// 
	uint32_t url_size;			// including trailing 0
	// follows uint8_t url[url_size];
} vdb_ack_GetPlaybackUrl_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetDownloadUrl - get clip download url
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetDownloadUrl_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_time_ms_lo;	// clip_start_time + time_offset
	uint32_t clip_time_ms_hi;
	uint32_t clip_length_ms;
} vdb_cmd_GetDownloadUrl_t;

typedef struct vdb_ack_GetDownloadUrl_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_date;		// seconds from 1970 (when the clip was created)
	uint32_t clip_time_ms_lo;
	uint32_t clip_time_ms_hi;
	uint32_t length_ms;
	uint32_t size_lo;
	uint32_t size_hi;	// 
	uint32_t url_size;	// including trailing 0
	// follows uint8_t url[url_size];
} vdb_ack_GetDownloadUrl_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_MarkClip
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_MarkClip_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t start_time_ms_lo;
	uint32_t start_time_ms_hi;
	uint32_t end_time_ms_lo;
	uint32_t end_time_ms_hi;
} vdb_cmd_MarkClip_t;

enum {
	SAVING_STATUS_OK,
	SAVING_STATUS_BUSY,
	SAVING_STATUS_BAD_PARAM,
	SAVING_STATUS_NO_SPACE,
	SAVING_STATUS_ERROR,
};

typedef struct vdb_ack_MarkClip_s {
	uint32_t status;
} vdb_ack_MarkClip_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetCopyState
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetCopyState_s {
	vdb_cmd_header_t header;	// must be first
} vdb_cmd_GetCopyState_t;

enum {
	kCopyStateIdle,
	kCopyStateMarking,
};

typedef struct vdb_ack_CopyState_s {
	uint32_t state;
	uint32_t clip_type;	// being copied
	uint32_t clip_id;	
	uint32_t percent;	// % completed
} vdb_ack_CopyState_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_DeleteClip
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_DeleteClip_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
} vdb_cmd_DeleteClip_t;

typedef struct vdb_ack_DeleteClip_s {
	uint32_t result;	// 0: OK; others: TBD
} vdb_ack_DeleteClip_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetRawData
//
//-----------------------------------------------------------------------

enum {
	kRawData_NULL = 0,
	kRawData_GPS = 1,
	kRawData_ACC = 2,
	kRawData_ODB = 3,
};

typedef struct vdb_cmd_GetRawData_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_time_ms_lo;	// clip_start_time + time_offset
	uint32_t clip_time_ms_hi;
	uint32_t data_types;		// (1<<kRawData_GPS) | (1<<kRawData_ACC) | (1<<kRawData_ODB)
} vdb_cmd_GetRawData_t;

typedef struct vdb_ack_GetRawData_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_date;		// seconds from 1970 (when the clip was created)
	// follows data items
	//	(1)
	//		uint32_t data_type;		// kRawData_GPS
	//		uint32_t clip_time_ms_lo;
	//		uint32_t clip_time_ms_hi;
	//		uint32_t data_size;		// if 0, the data is not available
	//		uint8 data[data_size];
	//	(2)
	//		uint32_t data_type;		// kRawData_ACC
	//		uint32_t clip_time_ms_lo;
	//		uint32_t clip_time_ms_hi;
	//		uint32_t data_size;		// if 0, the data is not available
	//		uint8 data[data_size];
	//	(3)
	//		uint32_t data_type;		// kRawData_ODB
	//		uint32_t clip_time_ms_lo;
	//		uint32_t clip_time_ms_hi;
	//		uint32_t data_size;		// if 0, the data is not available
	//		uint8 data[data_size];
	//	(4)
	//		uint32_t kRawData_NULL;	// end flag
} vdb_ack_GetRawData_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_SetRawDataOption
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_SetRawDataOption_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t data_types;		// (1<<kRawData_GPS) | (1<<kRawData_ACC) | (1<<kRawData_ODB)
} vdb_cmd_SetRawDataOption_t;

typedef struct vdb_ack_SetRawDataOption_s {
	uint32_t dummy;
} vdb_ack_SetRawDataOption_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetRawDataBlock
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetRawDataBlock_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_time_ms_lo;	// clip_start_time + time_offset
	uint32_t clip_time_ms_hi;
	uint32_t length_ms;
	uint32_t data_type;			// kRawData_GPS, kRawData_ACC, kRawData_ODB
} vdb_cmd_GetRawDataBlock_t;

typedef struct vdb_raw_data_index_s {
	int32_t time_offset_ms;		// may be negative!
								// requested_time_ms + time_offset_ms is 
								// the time point in the clip
	uint32_t data_size;
} vdb_raw_data_index_t;

typedef struct vdb_ack_GetRawDataBlock_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_date;		// seconds from 1970 (when the clip was created)
	uint32_t data_type;
	uint32_t requested_time_ms_lo;	// requested
	uint32_t requested_time_ms_hi;
	uint32_t num_items;
	uint32_t data_size;		// total
	// folows
	//	vdb_raw_data_index_t index_array[num_items];
	//	uint8_t item_data[index_array[0].data_size];
	//	uint8_t item_data[index_array[1].data_size];
	//	...
	//	uint8_t item_data[index_array[num_items-1].data_size];
} vdb_ack_GetRawDataBlock_t;

//-----------------------------------------------------------------------
//
//	VDB_MSG_VdbReady
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_VdbReady_s {
	vdb_ack_t header;	// must be first
	int32_t status;		// 0: OK, other: TBD
} vdb_msg_VdbReady_t;

//-----------------------------------------------------------------------
//
//	VDB_MSG_VdbUnmounted
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_VdbUnmounted_s {
	vdb_ack_t header;	// must be first
	uint32_t dummy;
} vdb_msg_VdbUnmounted_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_ClipCreated
//	VDB_MSG_ClipChanged
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_ClipInfo_s {
	vdb_ack_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_duration_ms;
	uint32_t clip_start_time_ms_lo;
	uint32_t clip_start_time_ms_hi;
} vdb_msg_ClipInfo_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_ClipRemoved
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_ClipRemoved_s {
	vdb_ack_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
} vdb_msg_ClipRemoved_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_BufferSpaceLow
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_BufferSpaceLow_s {
	vdb_ack_t header;	// must be first
	uint32_t dummy;
} vdb_msg_BufferSpaceLow_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_BufferFull
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_BufferFull_s {
	vdb_ack_t header;	// must be first
	uint32_t dummy;
} vdb_msg_BufferFull_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_CopyState
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_CopyState_s {
	vdb_ack_t header;	// must be first
	vdb_ack_CopyState_t param;
} vdb_msg_CopyState_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_RawData
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_RawData_s {
	vdb_ack_t header;	// must be first
	uint32_t data_type;
	uint32_t data_size;
	// follows uint8_t data[data_size]
} vdb_msg_RawData_t;



//************************************************************************
//
//	raw data structure
//
//************************************************************************
typedef struct acc_raw_data_s {
	int accel_x;
	int accel_y;
	int accel_z;
} acc_raw_data_t;

#define GPS_F_LATLON	(1 << 0)	// latitude and longitude are valid
#define GPS_F_ALTITUDE	(1 << 1)	// altitude is valid
#define GPS_F_SPEED	(1 << 2)	// speed is valid

typedef struct gps_raw_data_s {
	int flags;	// GPS_F_LATLON, GPS_F_ALTITUDE, GPS_F_SPEED
	float speed;
	double latitude;
	double longitude;
	double altitude;
} gps_raw_data_t;


typedef struct odb_index_s {
	uint16_t flag;		// 0x1: valid
	uint16_t offset;	// offset into data[]
} odb_index_t;

typedef struct odb_raw_data_s {
	uint32_t revision;
	uint32_t total_size;
	uint32_t pid_info_size;
	uint32_t pid_data_size;
	uint64_t pid_pts;
	uint64_t pid_polling_delay;
	uint64_t pid_polling_diff;
	// follows
	//	odb_index_t index[];	// bytes: pid_info_size
	//	uint8_t data[];			// bytes: pid_data_size
} odb_raw_data_t;

#endif

