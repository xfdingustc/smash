
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


#define VDB_VERSION_MAJOR	1
#define VDB_VERSION_MINOR	8

#define VDB_CMD_PORT		8083
#define VDB_HTTP_PORT		8085

#define CLIPINFO_FILE_EXT	".clip"
#define INDEX_FILE_EXT		".idx"
#define VIDEO_FILE_TS_EXT	".ts"
#define VIDEO_FILE_MP4_EXT	".mp4"
#define PICTURE_FILE_EXT	".jpg"
#define HLS_FILE_EXT		".m3u8"
#define CLIP_FILE_EXT		".clip"

// little endian format

#define VDB_CMD_SIZE		160
#define VDB_ACK_SIZE		160
#define VDB_MSG_SIZE		VDB_ACK_SIZE

#define VDB_ACK_MAGIC		0xFAFBFCFF

// clip types
#define CLIP_TYPE_UNSPECIFIED		(-1UL)	// for vdb_clip_t, used by VDB_CMD_GetIndexPicture
#define CLIP_TYPE_BUFFER		0		// buffered clip list
#define CLIP_TYPE_MARKED		1		// marked clip list
// 2-0xFF: reserved for now
#define CLIP_TYPE_PLIST0	0x100	// the first playlist
#define NUM_PLISTS			5

#define IS_PLAYLIST(_clip_type) \
	((_clip_type) >= CLIP_TYPE_PLIST0)
//	((_clip_type) >= CLIP_TYPE_PLIST0 && (_clip_type) < (CLIP_TYPE_PLIST0 + NUM_PLISTS))

#define SAVE_IN_CLIPINFO(_clip_type) \
	((_clip_type) == CLIP_TYPE_BUFFER || (_clip_type) == CLIP_TYPE_MARKED)

#define CLIP_TYPE_FIRST_PLIST	CLIP_TYPE_PLIST0
#define CLIP_TYPE_LAST_PLIST	(CLIP_TYPE_PLIST0 + NUM_PLISTS - 1)
#define PLAYLIST_ID_START		(CLIP_TYPE_PLIST0 + NUM_PLISTS)

// url type
#define URL_TYPE_TS		0
#define URL_TYPE_HLS	1
#define URL_TYPE_CLIP	2
#define URL_TYPE_MP4	3

#define _URL_TYPE_MASK	0x000000FF

#define URL_MUTE_AUDIO	(1 << 31)
#define URL_FULL_PATH	(1 << 30)	// use full path for .ts in m3u8
#define URL_SPEC_TIME	(1 << 29)	// specify start time

// streams
enum {
	STREAM_MAIN = 0,
	STREAM_SUB_1 = 1,
	MAX_VDB_STREAMS
};

// no vdb_id: VDB_CMD_GetVersionInfo, VDB_CMD_SetRawDataOption

enum {
	VDB_CMD_Null,

	// commands
	VDB_CMD_GetVersionInfo = 1,	// replace VDB_CMD_GetDiscInfo

	VDB_CMD_GetClipSetInfo = 2,	// obsolete; use VDB_CMD_GetClipSetInfoEx
	VDB_CMD_GetIndexPicture = 3,
	VDB_CMD_GetPlaybackUrl = 4,
		VDB_CMD_GetDownloadUrl = 5,	// obsolete; use VDB_CMD_GetDownloadUrlEx
	VDB_CMD_MarkClip = 6,
		//VDB_CMD_GetCopyState = 7,	// obsolete
	VDB_CMD_DeleteClip = 8,
	VDB_CMD_GetRawData = 9,
	VDB_CMD_SetRawDataOption = 10,
	VDB_CMD_GetRawDataBlock = 11,
	VDB_CMD_GetDownloadUrlEx = 12,

	// commands: playlist
	VDB_CMD_GetAllPlaylists = 13,
	VDB_CMD_GetPlaylistIndexPicture = 14,
	VDB_CMD_ClearPlaylist = 15,
	VDB_CMD_InsertClip = 16,
		//VDB_CMD_RemoveClip: same with VDB_CMD_DeleteClip
	VDB_CMD_MoveClip = 17,
	VDB_CMD_GetPlaylistPlaybackUrl = 18,

	//-----------------------
	// since version 1.4
	//-----------------------
	VDB_CMD_GetUploadUrl = 19,
	VDB_CMD_SetOptions = 20,

	//-----------------------
	// since version 1.5
	//-----------------------
	VDB_CMD_GetSpaceInfo = 21,

	//-----------------------
	// since version 1.6
	//-----------------------
	VDB_CMD_SetClipAttr = 22,

	//-----------------------
	// since version 1.2
	//-----------------------
	VDB_CMD_GetClipExtent = 32,
	VDB_CMD_SetClipExtent = 33,
	VDB_CMD_GetClipSetInfoEx = 34,
	VDB_CMD_GetAllClipSetInfo = 35,
	VDB_CMD_GetClipInfo = 36,

	//-----------------------
	// since version 1.3
	//-----------------------
	VDB_CMD_GetRawDataSize = 37,		// obsolete
	VDB_CMD_GetPlaybackUrlEx = 38,

	//-----------------------
	// since version 1.3
	//	for still pictures
	//-----------------------
	VDB_CMD_GetPictureList = 39,
	VDB_CMD_ReadPicture = 40,
	VDB_CMD_RemovePicture = 41,

	//-----------------------
	// since version 1.6
	//-----------------------
	VDB_CMD_CreatePlaylist = 50,
	VDB_CMD_DeletePlaylist = 51,
	VDB_CMD_InsertClipEx = 52,	// supersedes VDB_CMD_InsertClip
	VDB_CMD_GetPlaylistPath = 53,

	//-----------------------
	// since version 1.7
	//-----------------------
	VDB_CMD_GetRawDataBlockEx = 54,

	//-----------------------
	// since version 1.8
	//-----------------------
	VDB_CMD_GetIFrame = 55,

	// -----------------------------------------------
	// msgs
	// -----------------------------------------------

	_VDB_MSG_START = 0x1000,
	VDB_MSG_VdbReady = _VDB_MSG_START + 0,
	VDB_MSG_VdbUnmounted = _VDB_MSG_START + 1,

	VDB_MSG_ClipInfo = _VDB_MSG_START + 2,
	VDB_MSG_ClipRemoved = _VDB_MSG_START + 3,

	VDB_MSG_BufferSpaceLow = _VDB_MSG_START + 4,
	VDB_MSG_BufferFull = _VDB_MSG_START + 5,
		//VDB_MSG_CopyState = _VDB_MSG_START + 6,	// obsolete
	VDB_MSG_RawData = _VDB_MSG_START + 7,

	VDB_MSG_PlaylistCleared = _VDB_MSG_START + 8,

	//-----------------------
	// since version 1.6
	//-----------------------
	VDB_MSG_PlaylistCreated = _VDB_MSG_START + 9,
	VDB_MSG_PlaylistDeleted = _VDB_MSG_START + 10,

	//-----------------------
	// since version 1.2
	//-----------------------
	VDB_MSG_MarkLiveClipInfo = _VDB_MSG_START + 32,

	//-----------------------
	// since version 1.5
	//-----------------------
	VDB_MSG_SpaceInfo = _VDB_MSG_START + 40,

	//-----------------------
	// since version 1.3
	//	for still pictures
	//-----------------------
	VDB_MSG_RefreshPictureList = _VDB_MSG_START + 64,
	VDB_MSG_PictureTaken = _VDB_MSG_START + 65,
	VDB_MSG_PictureRemoved = _VDB_MSG_START + 66,

	//-----------------------
	// since version 1.6
	//-----------------------
	VDB_MSG_CLIP_ATTR = _VDB_MSG_START + 80,
};

// cmd_flags
#define VDB_CMD_DEFLATE		(1 << 0)
#define VDB_CMD_FLAGS		(VDB_CMD_DEFLATE)

// 16 bytes
typedef struct vdb_cmd_header_s {
	uint16_t cmd_code;		// command code
	uint16_t cmd_flags;		// command flags: VDB_CMD_DEFLATE
	uint32_t cmd_tag;		// will be returned in ack
	uint32_t user1;			// will be returned in ack
	uint32_t user2;			// will be returned in ack
} vdb_cmd_header_t;

// 32-byte ack header
// for each ACK and MSG
typedef struct vdb_ack_s {
	uint32_t magic;			// VDB_ACK_MAGIC
	uint32_t seqid;			// sequence id

	uint32_t user1;			// return user1 in vdb_cmd_header_t
	uint32_t user2;			// return user2 in vdb_cmd_header_t
	uint16_t cmd_code;		// return cmd_code in vdb_cmd_header_t
	uint16_t cmd_flags;		// return cmd_flags in vdb_cmd_header_t
	uint32_t cmd_tag;		// return cmd_tag in vdb_cmd_header_t

	int32_t ret_code;		// 0: success
	uint32_t extra_bytes;	// bytes after the 128-byte header
} vdb_ack_t;

typedef struct vdb_id_s {
	uint32_t id_size;	// including trailing 0
//	uint8_t id[id_size];	// vdb-id, should be null-terminated
//	if ((id_size % 4) != 0) {
//		uint8_t padding[4 - (id_size%4)];	// padding for 4-byte aligned
//	}

} vdb_id_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetVersionInfo:
//	this cmd need not be sent; the ack will be sent be vdb-server automatically
//	as the first packet
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetVersionInfo_s {
	vdb_cmd_header_t header;	// must be first
} vdb_cmd_GetVersionInfo_t;

enum {
	OS_LINUX = 0,
	OS_ANDROID = 1,
	OS_IOS = 2,
	OS_WINDOWS = 3,
	OS_MAC = 4,
};

#define VDB_HAS_ID	(1 << 0)

typedef struct vdb_ack_GetVersionInfo_s {
	uint16_t major;		// VDB_VERSION_MAJOR
	uint16_t minor;		// VDB_VERSION_MINOR
	uint16_t os_type;	// OS_LINUX etc
	uint16_t flags;		// VDB_HAS_ID
} vdb_ack_GetVersionInfo_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetClipSetInfo - get clip set info
//	this cmd is obsolete; please use VDB_CMD_GetClipSetInfoEx
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetClipSetInfo_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;		// CLIP_TYPE_BUFFER, CLIP_TYPE_MARKED
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetClipSetInfo_t;

typedef struct vdb_clip_info_s {
	uint32_t clip_id;			// unique clip id
	uint32_t clip_date;			// seconds from 1970 (when the clip was created)
	uint32_t clip_duration_ms;
	uint32_t clip_start_time_ms_lo;
	uint32_t clip_start_time_ms_hi;
	uint16_t num_streams;
	uint16_t flags;				// is 0; used by VDB_CMD_GetClipSetInfoEx

//	avf_stream_attr_t stream_attr[num_streams];

} vdb_clip_info_t;

typedef struct vdb_ack_GetClipSetInfo_s {
	uint32_t clip_type;			// may be CLIP_TYPE_UNSPECIFIED
	uint32_t total_clips;		// 
	uint32_t total_length_ms;	// TBD, current is 0
	uint32_t live_clip_id;		// INVALID_CLIP_ID

//	vdb_clip_info_t clip_info[total_clips];

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

//	if (more_data_follows) {
//		uint32_t vdb_no_delete;
//	}

} vdb_ack_GetClipSetInfo_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetIndexPicture - get index picture in a clip
//	note: for VDB_HAS_ID, if set header.cmd_tag = 1, then clip_time_ms
//		is not used, and clip poster is returned.
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetIndexPicture_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t user_data;
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
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

//	uint8_t jpeg_data[picture_size];

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetIndexPicture_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetPlaylistIndexPicture
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetPlaylistIndexPicture_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t list_type;		// must be CLIP_TYPE_PLIST0 etc
	uint32_t flags;			// TBD
} vdb_cmd_GetPlaylistIndexPicture_t;

typedef struct vdb_ack_GetPlaylistIndexPicture_s {
	uint32_t list_type;
	uint32_t flags;			// TBD

	uint32_t clip_date;
	uint32_t clip_time_ms_lo;
	uint32_t clip_time_ms_hi;

	uint32_t picture_size;
//	uint8_t jpeg_data[picture_size];

} vdb_ack_GetPlaylistIndexPicture_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetIFrame
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_GetIFrame_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t user_data;
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;
	uint16_t stream;			// 0 or 1
	uint16_t flags;				// must be 0
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetIFrame_t;

typedef struct vdb_ack_GetIFrame_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_date;		// seconds from 1970 (when the clip was created)
	uint32_t user_data;
	uint32_t requested_time_ms_lo;	// requested
	uint32_t requested_time_ms_hi;
	uint32_t clip_time_ms_lo;		// real
	uint32_t clip_time_ms_hi;
	uint32_t duration;

	//----------------------

	uint16_t stream;
	uint16_t flags;
	uint16_t pic_type;
	uint16_t reserved;

	//----------------------

	uint32_t iframe_size;
	uint32_t data_offset;	// iframe_data pos. if 0, then iframe_data follows immediately

//	uint8_t iframe_data[iframe_size];

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_ack_GetIFrame_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetPlaybackUrl - get clip playback url
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetPlaybackUrl_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t stream;			// STREAM_MAIN, STREAM_SUB_1
	uint32_t url_type;			// URL_TYPE_TS, URL_TYPE_HLS; bit-31: mute audio
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetPlaybackUrl_t;

typedef struct vdb_ack_GetPlaybackUrl_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t stream;			// STREAM_MAIN, STREAM_SUB_1
	uint32_t url_type;			// URL_TYPE_TS, URL_TYPE_HLS; bit-31: mute audio
	uint32_t real_time_ms_lo;
	uint32_t real_time_ms_hi;
	uint32_t length_ms;
	uint32_t has_more;			// TBD
	uint32_t url_size;			// including trailing 0

//	uint8_t url[url_size];

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetPlaybackUrl_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetDownloadUrl - get clip download url for main stream
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetDownloadUrl_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;
	uint32_t clip_length_ms;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
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

//	uint8_t url[url_size];

//	uint32_t picture_size;
//	uint8_t jpeg_data[picture_size];

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetDownloadUrl_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetDownloadUrlEx - get clip download url
//
//-----------------------------------------------------------------------

#define DOWNLOAD_OPT_MAIN_STREAM	(1 << 0)
#define DOWNLOAD_OPT_SUB_STREAM_1	(1 << 1)
#define DOWNLOAD_OPT_INDEX_PICT		(1 << 2)
#define DOWNLOAD_OPT_PLAYLIST		(1 << 3)	// download the playlist
#define DOWNLOAD_OPT_MUTE_AUDIO		(1 << 4)
#define DOWNLOAD_OPT_MAIN_MP4		(1 << 5)	// get mp4 url if possible
#define DOWNLOAD_OPT_SUB_MP4		(1 << 6)	// 

// same with vdb_cmd_GetDownloadUrl_t
typedef struct vdb_cmd_GetDownloadUrlEx_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;	//	OPT_PLAYLIST: start offset from playlist begin 
	uint32_t clip_length_ms;		//  OPT_PLAYLIST: set to 0 to download the whole playlist
	uint32_t download_opt;		// extra option
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetDownloadUrlEx_t;

// keep same with vdb_ack_GetDownloadUrl_t
typedef struct vdb_stream_url_s {
	uint32_t clip_date;			// seconds from 1970 (when the clip was created)
	uint32_t clip_time_ms_lo;
	uint32_t clip_time_ms_hi;
	uint32_t length_ms;
	uint32_t size_lo;
	uint32_t size_hi;
	uint32_t url_size;	// including trailing 0

//	uint8_t url[url_size];

} vdb_stream_url_t;

typedef struct vdb_ack_GetDownloadUrlEx_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t download_opt;		// returned value: STREAM, STREAM_1, INDEX_PIC

//	if (download_opt & DOWNLOAD_OPT_MAIN_STREAM) {
//		vdb_stream_url_t stream_url;	// for main stream
//	}

//	if (download_opt & DOWNLOAD_OPT_SUB_STREAM_1) {
//		vdb_stream_url_t stream_url;	// for sub stream
//	}

//	if (download_opt & DOWNLOAD_OPT_INDEX_PICT) {
//		uint32_t picture_size;
//		uint8_t jpeg_data[picture_size];
//	}

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetDownloadUrlEx_t;

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
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_MarkClip_t;

#define e_MarkClip_OK		0
#define e_MarkClip_BadParam	1

typedef struct vdb_ack_MarkClip_s {
	uint32_t status;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_ack_MarkClip_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_DeleteClip
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_DeleteClip_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_DeleteClip_t;

#define e_DeleteClip_OK				0
#define e_DeleteClip_NotFound		1
#define e_DeleteClip_NotPermitted	2

typedef struct vdb_ack_DeleteClip_s {
	uint32_t result;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
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
	kRawData_OBD = 3,
};

typedef struct vdb_cmd_GetRawData_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;
	uint32_t data_types;		// (1<<kRawData_GPS) | (1<<kRawData_ACC) | (1<<kRawData_OBD)
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetRawData_t;

typedef struct vdb_ack_GetRawData_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_date;		// seconds from 1970 (when the clip was created)

//	while (true) {
//		uint32_t data_type;
//		if (data_type == kRawData_NULL)
//			break;
//		uint32_t clip_time_ms_lo;	// the item's real position on time line
//		uint32_t clip_time_ms_hi;
//		uint32_t data_size;
//		uint8 data[data_size];
//	}

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetRawData_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_SetRawDataOption
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_SetRawDataOption_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t data_types;		// (1<<kRawData_GPS) | (1<<kRawData_ACC) | (1<<kRawData_OBD)
} vdb_cmd_SetRawDataOption_t;

typedef struct vdb_ack_SetRawDataOption_s {
	uint32_t dummy;
} vdb_ack_SetRawDataOption_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetRawDataBlock & VDB_CMD_GetRawDataBlockEx
//
//-----------------------------------------------------------------------

#define DATA_OPTION_NO_INDEX		(1 << 0)
#define DATA_OPTION_NO_DATA			(1 << 1)
#define DATA_OPTION_DOWNSAMPLING	(1 << 2)

typedef struct vdb_cmd_GetRawDataBlock_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t clip_type;
	uint32_t clip_id;			// INVALID_CLIP_ID: clip_type is playlist id
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;
	uint32_t length_ms;

	uint16_t data_type;			// kRawData_GPS, kRawData_ACC, kRawData_OBD
	uint16_t data_options;		// DATA_OPTION_NO_DATA|DATA_OPTION_NO_INDEX, do not set DATA_OPTION_DOWNSAMPLING

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetRawDataBlock_t;

typedef struct vdb_cmd_GetRawDataBlockEx_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t clip_type;
	uint32_t clip_id;			// INVALID_CLIP_ID: clip_type is playlist id
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;
	uint32_t length_ms;

	uint16_t data_type;			// kRawData_GPS, kRawData_ACC, kRawData_OBD
	uint16_t data_options;		// DATA_OPTION_NO_DATA|DATA_OPTION_NO_INDEX|DATA_OPTION_DOWNSAMPLING

	uint32_t interval_ms;	// read data every 'interval_ms', set to 0 to read all
	uint32_t reserved1;		// must be 0
	uint32_t reserved2;		// must be 0

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetRawDataBlockEx_t;

typedef struct vdb_raw_data_index_s {
	int32_t time_offset_ms;		// may be negative
								// requested_time_ms + time_offset_ms is 
								// the time point in the clip
	uint32_t data_size;
} vdb_raw_data_index_t;

typedef struct vdb_ack_GetRawDataBlock_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_date;		// seconds from 1970 (when the clip was created)

	uint16_t data_type;		// same as cmd
	uint16_t data_options;	// same as cmd

	uint32_t requested_time_ms_lo;	// requested
	uint32_t requested_time_ms_hi;
	uint32_t num_items;
	uint32_t data_size;		// total

//	if ((data_options & DATA_OPTION_NO_INDEX) == 0) {
//		vdb_raw_data_index_t index[num_items];
//	}

//	if ((data_options & DATA_OPTION_NO_DATA) == 0) {
//		for (i = 0; i < num_items; i++) {
//			uint8_t item_data[index[i].data_size];
//		}
//	}

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetRawDataBlock_t;

typedef vdb_ack_GetRawDataBlock_t vdb_ack_GetRawDataBlockEx_t; 	// same

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetAllPlaylists
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetAllPlaylists_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t flags;			// not used now
} vdb_cmd_GetAllPlaylists_t;

typedef struct vdb_playlist_info_s {
	uint32_t list_type;		// CLIP_TYPE_PLIST0, 1, 2
	uint32_t properties;		// TBD
	uint32_t num_clips;
	uint32_t total_length_ms;
} vdb_playlist_info_t;

typedef struct vdb_ack_GetAllPlaylist_s {
	uint32_t flags;			// not used now
	uint32_t num_playlists;

//	vdb_playlist_info_t playlist_info[num_playlists];

} vdb_ack_GetAllPlaylist_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_ClearPlaylist
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_ClearPlaylist_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t list_type;		// must be CLIP_TYPE_PLIST0, CLIP_TYPE_PLIST0 etc
} vdb_cmd_ClearPlaylist_t;

#define e_ClearPlaylist_OK				0
#define e_ClearPlaylist_NotFound		1	// playlist not found
#define e_ClearPlaylist_NotPermitted	2	// cannot clear playlist

typedef struct vdb_ack_ClearPlaylist_s {
	uint32_t status;
} vdb_ack_ClearPlaylist_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_InsertClip
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_InsertClip_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;		// source type: buffer, marked
	uint32_t clip_id;		// source clip id
	uint32_t start_time_ms_lo;	// source clip start time
	uint32_t start_time_ms_hi;
	uint32_t end_time_ms_lo;	// source clip end time
	uint32_t end_time_ms_hi;
	uint32_t list_type;		// dst type, must be CLIP_TYPE_PLIST0, 1, 2
	uint32_t list_pos;		// -1U: append at tail
} vdb_cmd_InsertClip_t;

#define e_InsertClip_OK				0
#define e_InsertClip_BadParam		1
#define e_InsertClip_NotFound		2	// clip or playlist not found
#define e_InsertClip_TooMany		3	// too many clips in the list
#define e_InsertClip_UnknownStream	4	// clip attribute unknown
#define e_InsertClip_StreamNotMatch	5	// clip attribute does not match other clips

typedef struct vdb_ack_InsertClip_s {
	uint32_t status;
} vdb_ack_InsertClip_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_MoveClip
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_MoveClip_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t new_clip_pos;	// 0-based
} vdb_cmd_MoveClip_t;

#define e_MoveClip_OK			0
#define e_MoveClip_BadParam	1

typedef struct vdb_ack_MoveClip_s {
	uint32_t status;
} vdb_ack_MoveClip_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetPlaylistPlaybackUrl
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetPlaylistPlaybackUrl_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t list_type;
	uint32_t playlist_start_ms;		// relative to clip start
	uint32_t stream;					// STREAM_MAIN, STREAM_SUB_1
	uint32_t url_type;				// URL_TYPE_TS, URL_TYPE_HLS; bit-31: mute audio
} vdb_cmd_GetPlaylistPlaybackUrl_t;

typedef struct vdb_ack_GetPlaylistPlaybackUrl_s {
	uint32_t list_type;
	uint32_t playlist_start_ms;		// relative to playlist start, may be different
	uint32_t stream;					// STREAM_MAIN, STREAM_SUB_1
	uint32_t url_type;				// URL_TYPE_TS, URL_TYPE_HLS; bit-31: mute audio
	///
	uint32_t length_ms;
	uint32_t has_more;				// TBD
	uint32_t url_size;				// including trailing 0
//	uint8_t url[url_size];

} vdb_ack_GetPlaylistPlaybackUrl_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetUploadUrl
//
//-----------------------------------------------------------------------

#define UPLOAD_GET_V0	(1 << 0)	// video stream 0
#define UPLOAD_GET_V1	(1 << 1)	// video stream 1
#define UPLOAD_GET_PIC	(1 << 2)	// picture
#define UPLOAD_GET_GPS	(1 << 3)	// gps
#define UPLOAD_GET_OBD	(1 << 4)	// obd
#define UPLOAD_GET_ACC	(1 << 5)	// acc

#define UPLOAD_GET_RAW			(UPLOAD_GET_GPS|UPLOAD_GET_OBD|UPLOAD_GET_ACC)
#define UPLOAD_GET_PIC_RAW		(UPLOAD_GET_PIC|UPLOAD_GET_RAW)
#define UPLOAD_GET_VIDEO		(UPLOAD_GET_V0|UPLOAD_GET_V1)

#define UPLOAD_GET_STREAM_0		(UPLOAD_GET_V0|UPLOAD_GET_PIC_RAW)
#define UPLOAD_GET_STREAM_1		(UPLOAD_GET_V1)

typedef struct vdb_cmd_GetUploadUrl_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t b_playlist;			// is playlist
	uint32_t clip_type;			// CLIP_TYPE_BUFFER/CLIP_TYPE_MARKED; >= 0x100 for playlist
	uint32_t clip_id;			// not used if b_playlist != 0
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;	//	for playlist, they are offset from start the playlist
	uint32_t length_ms;			// length; 0 for all remain
	uint32_t upload_opt;			// UPLOAD_GET_V0 etc
	uint32_t reserved1;			// set to 0
	uint32_t reserved2;			// set to 0
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetUploadUrl_t;

typedef struct vdb_ack_GetUploadUrl_s {
	uint32_t b_playlist;		// return the cmd param
	uint32_t clip_type;		// return the cmd param
	uint32_t clip_id;		// return the cmd param
	uint32_t real_time_ms_lo;
	uint32_t real_time_ms_hi;
	uint32_t length_ms;		// real length
	uint32_t upload_opt;		// return the cmd param
	uint32_t reserved1;		// return the cmd param
	uint32_t reserved2;		// return the cmd param

	uint32_t url_size;		// including trailing 0
//	uint8_t url[url_size];

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_ack_GetUploadUrl_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_SetOptions
//
//-----------------------------------------------------------------------

enum {
	VDB_OPTION_NONE = 0,
	VDB_OPTION_HLS_SEGMENT_LENGTH,	// param[0] is hls segment length
};

typedef struct vdb_cmd_SetOptions_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t option;
	uint32_t params[4];
} vdb_cmd_SetOptions_t;

typedef struct vdb_ack_SetOption_s {
	uint32_t dummy;	// not used
} vdb_ack_SetOption_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetSpaceInfo
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetSpaceInfo_s {
	vdb_cmd_header_t header;		// must be first
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetSpaceInfo_t;

typedef struct vdb_space_info_s {
	uint32_t total_space_lo;		// total space in bytes
	uint32_t total_space_hi;	
	uint32_t used_space_lo;		// used space in bytes (all clips and other files)
	uint32_t used_space_hi;
	uint32_t protected_space_lo;	// protected clips (none-buffered, and none-auto-delete buffered clips) in bytes
	uint32_t protected_space_hi;
	uint32_t clip_space_lo;		// all clips (buffered + marked); 0 if not set
	uint32_t clip_space_hi;		
	uint32_t reserved[4];		// 0
} vdb_space_info_t;

typedef struct vdb_ack_GetSpaceInfo_s {
	vdb_space_info_t space_info;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_ack_GetSpaceInfo_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_SetClipAttr
//		e.g: attr_mask = CLIP_ATTR_AUTO, attr_value = CLIP_ATTR_AUTO will
//			set CLIP_ATTR_AUTO
//		attr_mask = CLIP_ATTR_AUTO, attr_value = 0 will
//			clear CLIP_ATTR_AUTO
//
//	CLIP_ATTR_LIVE & CLIP_ATTR_LIVE_MARK cannot be changed
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_SetClipAttr_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t attr_mask;			// bits set to 1: set as attr_value
	uint32_t attr_value;			// 
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_SetClipAttr_t;

typedef struct vdb_ack_SetClipAttr_s {

	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t attr_mask;
	uint32_t attr_value;

	int32_t result;			// 0 for OK
	uint32_t new_attr;		// after the cmd

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_ack_SetClipAttr_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetClipExtent
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetClipExtent_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t clip_type;
	uint32_t clip_id;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetClipExtent_t;

typedef struct vdb_ack_GetClipExtent_s {
	uint32_t clip_type;
	uint32_t clip_id;

	uint32_t real_clip_id;			// don't use
	uint32_t buffered_clip_id;

	uint32_t min_clip_start_tims_ms_lo;	// min clip_start_time_ms
	uint32_t min_clip_start_time_ms_hi;

	uint32_t max_clip_end_time_ms_lo;	// max clip_end_time_ms
	uint32_t max_clip_end_time_ms_hi;

	uint32_t clip_start_time_ms_lo;		// curr clip_start_time_ms
	uint32_t clip_start_time_ms_hi;

	uint32_t clip_end_time_ms_lo;		// curr clip_end_time_ms
	uint32_t clip_end_time_ms_hi;

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetClipExtent_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_SetClipExtent
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_SetClipExtent_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t new_clip_start_time_ms_lo;	// new clip_start_time_ms
	uint32_t new_clip_start_time_ms_hi;
	uint32_t new_clip_end_time_ms_lo;	// new clip_end_time_ms
	uint32_t new_clip_end_time_ms_hi;	
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_SetClipExtent_t;

typedef struct vdb_ack_SetClipExtent_s {
	uint32_t status;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_ack_SetClipExtent_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetClipSetInfoEx - get clip set info
//	inherites VDB_CMD_GetClipSetInfo
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_GetClipSetInfoEx_s {
	vdb_cmd_GetClipSetInfo_t inherited;
	uint32_t flags;		// GET_CLIP_EXTRA
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetClipSetInfoEx_t;

// vdb_clip_info_ex_t.inherited.flags
#define GET_CLIP_EXTRA		(1 << 0)
#define GET_CLIP_VDB_ID		(1 << 1)
#define GET_CLIP_DESC		(1 << 2)
#define GET_CLIP_ATTR		(1 << 3)
#define GET_CLIP_SIZE		(1 << 4)
#define GET_CLIP_SCENE_DATA	(1 << 5)
#define GET_CLIP_RAW_FREQ	(1 << 6)

// clip attributes flags (max is 16)
#define CLIP_ATTR_LIVE				(1 << 0)	// live clip
#define CLIP_ATTR_AUTO				(1 << 1)	// auto generated clip
#define CLIP_ATTR_MANUALLY			(1 << 2)	// manually generated clip
#define CLIP_ATTR_UPLOADED			(1 << 3)	// clip has been uploaded
#define CLIP_ATTR_LIVE_MARK			(1 << 4)	// created by avf_camera_mark_live_clip()
#define CLIP_ATTR_NO_AUTO_DELETE	(1 << 5)	// do not auto delete the clip is space is low
#define CLIP_ATTR_NEED_UPLOAD		(1 << 6)	// this clip need to be uploaded
#define CLIP_ATTR_HAS_EX_RAW_DATA	(1 << 7)	// this clip has extra raw data

// inherites vdb_clip_info_t
typedef struct vdb_clip_info_ex_s {
	vdb_clip_info_t inherited;

	int32_t clip_type;
	uint32_t extra_size;		// sum size of the following

//	if (inherited.flags & GET_CLIP_EXTRA) {
//		uint8_t uuid[UUID_LEN];
//		uint32_t ref_clip_date;	// same with clip_date
//		int32_t gmtoff;
//		uint32_t real_clip_id;
//	}

//	if (inherited.flags & GET_CLIP_DESC) {
//		for (;;) {
//			uint32_t fcc;
//			if (fcc == 0)
//				break;
//			uint32_t data_size
//			uint8_t data[data_size];
//			uint8_t padding_0[align_to_4n_bytes];
//		}
//	}

//	if (inherited.flags & GET_CLIP_ATTR) {
//		uint32_t	clip_attr;	// CLIP_ATTR_LIVE etc
//	}

//	if (inherited.flags & GET_CLIP_SIZE) {
//		uint32_t clip_size_lo;
//		uint32_t clip_size_hi;
//	}

//	if (inherited.flags & GET_CLIP_SCENE_DATA) {
//		uint32_t scene_data_size;
//		uint8_t scene_data[scene_data_size];
//	}

//	if (inherited.flags & GET_CLIP_RAW_FREQ) {
//		uint32_t interval_gps;
//		uint32_t interval_acc;
//		uint32_t interval_obd;
//		uint32_t reserved;
//	}

} vdb_clip_info_ex_t;

typedef struct vdb_ack_GetClipSetInfoEx_s {
	/// same with vdb_ack_GetClipSetInfo_t
	uint32_t clip_type;			// may be CLIP_TYPE_UNSPECIFIED
	uint32_t total_clips;		// 
	uint32_t total_length_ms;	// TBD, current is 0
	uint32_t live_clip_id;		// INVALID_CLIP_ID

//	vdb_clip_info_ex_t clip_info[total_clips];

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

//	if (more_data_follows) {
//		uint32_t vdb_no_delete;
//	}

} vdb_ack_GetClipSetInfoEx_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetAllClipSetInfo - get all buffered & marked clips
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_GetAllClipSetInfo_s {
	vdb_cmd_header_t header;	// must be first
	// uint32_t clip_type;		// CLIP_TYPE_BUFFER, CLIP_TYPE_MARKED
	uint32_t flags;			// reserved; 0
} vdb_cmd_GetAllClipSetInfo_t;

typedef struct vdb_ack_GetAllClipSetInfo_s {
	uint32_t total_clips;		// 

//	vdb_clip_info_ex_t clip_info[total_clips];

} vdb_ack_GetAllClipSetInfo_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetClipInfo
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_GetClipInfo_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t clip_type;		// CLIP_TYPE_BUFFER, CLIP_TYPE_MARKED
	uint32_t clip_id;
	uint32_t flags;		// GET_CLIP_EXTRA | GET_CLIP_DESC
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetClipInfo_t;

typedef struct vdb_ack_GetClipInfo_s {
	vdb_clip_info_ex_t info;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_ack_GetClipInfo_t;

// ack: vdb_clip_info_ex_t

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetRawDataSize - not used
//
//-----------------------------------------------------------------------

typedef vdb_cmd_GetRawDataBlock_t vdb_cmd_GetRawDataSize_t;

typedef struct vdb_ack_GetRawDataSize_s {
	uint32_t size;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_ack_GetRawDataSize_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetPlaybackUrlEx
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetPlaybackUrlEx_s {
	vdb_cmd_GetPlaybackUrl_t inherited;
	uint32_t length_ms;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetPlaybackUrlEx_t;

typedef vdb_ack_GetPlaybackUrl_t vdb_ack_GetPlaybackUrlEx_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetPictureList
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_GetPictureList_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t options;	// be 0 now
	uint32_t reserved1;	// 0
	uint32_t reserved2;	// 0
} vdb_cmd_GetPictureList_t;

typedef struct vdb_picture_name_s {
	uint32_t filename_size; // including trailing 0
//	uint8_t filename[filename_size];
//	if ((filename_size % 4) != 0) {
//		uint8_t padding[4 - (filename_size % 4)];	// padding for 4-byte aligned
//	}
} vdb_picture_name_t;

typedef struct vdb_picture_info_s {
	uint32_t info_size;	// size of this struct, including this field
	uint32_t picture_date;	// date since 1970-1-1
	uint32_t reserved1;
	uint32_t reserved2;
//	vdb_picture_name_t picture_name;
} vdb_picture_info_t;

typedef struct vdb_ack_GetPictureList_s {
	uint32_t options;
	uint32_t reserved1;
	uint32_t reserved2;
	int32_t gmt_off;
	uint32_t num_pictures;
//
//	vdb_picture_info_t picture_info[num_pictures];
//
} vdb_ack_GetPictureList_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_ReadPicture
//
//-----------------------------------------------------------------------

#define PIC_OPT_THUMBNAIL	(1 << 0)

typedef struct vdb_cmd_ReadPicture_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t options;	// PIC_OPT_THUMBNAIL
	uint32_t reserved1;	// 0
	uint32_t reserved2;	// 0
//	vdb_picture_name_t picture_name;
} vdb_cmd_ReadPicture_t;

// should check read_status to see if the data is correct
typedef struct vdb_ack_ReadPicture_s {
	uint32_t options;
	uint32_t reserved1;
	uint32_t reserved2;
//	vdb_picture_name_t picture_name;
//	uint32_t picture_size;
//	uint8_t picture_data[picture_size];	// jpeg data
//	int32_t read_status;	// 0: OK; else error code
} vdb_ack_ReadPicture_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_RemovePicture
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_RemovePicture_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t reserved1;	// 0
	uint32_t reserved2;	// 0
//	vdb_picture_name_t picture_name;
} vdb_cmd_RemovePicture_t;

typedef struct vdb_ack_RemovePicture_s {
	uint32_t result;		// 0: OK, otherwise error
//	vdb_picture_name_t picture_name;
} vdb_ack_RemovePicture_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_CreatePlaylist
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_CreatePlaylist_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t reserved0;		// must be 0
	uint32_t reserved1;		// must be 0
} vdb_cmd_CreatePlaylist_t;

typedef struct vdb_ack_CreatePlaylist_s {
	int32_t result;		// 0: OK
	uint32_t list_id;
	uint32_t reserved0;
	uint32_t reserved1;
} vdb_ack_CreatePlaylist_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_DeletePlaylist
//
//-----------------------------------------------------------------------

typedef struct vdb_cmd_DeletePlaylist_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t list_id;
	uint32_t reserved;
} vdb_cmd_DeletePlaylist_t;

typedef struct vdb_ack_DeletePlaylist_s {
	int32_t result;		// 0: OK
	uint32_t list_id;
	uint32_t reserved;
} vdb_ack_DeletePlaylist_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_InsertClipEx - supersedes VDB_CMD_InsertClip
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_InsertClipEx_s {
	vdb_cmd_InsertClip_t inherited;
	uint32_t check_stream_attr;
	uint32_t reserved;
} vdb_cmd_InsertClipEx_t;

typedef struct vdb_ack_InsertClipEx_s {
	vdb_ack_InsertClip_t inherited;
} vdb_ack_InsertClipEx_t;

//-----------------------------------------------------------------------
//
//  VDB_CMD_GetPlaylistPath
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetPlaylistPath_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t list_id;
	uint32_t reserved;
} vdb_cmd_GetPlaylistPath_t;

typedef struct vdb_ack_GetPlaylistPath_s {
	int32_t result;		// 0: OK
	uint32_t path_size;	// including trailing 0
//	uint8_t path[path_size];
} vdb_ack_GetPlaylistPath_t;

//-----------------------------------------------------------------------
//
//	VDB_MSG_VdbReady
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_VdbReady_s {
	vdb_ack_t header;	// must be first
	int32_t status;		// 0: OK, other: TBD
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_VdbReady_t;

//-----------------------------------------------------------------------
//
//	VDB_MSG_VdbUnmounted
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_VdbUnmounted_s {
	vdb_ack_t header;	// must be first
	uint32_t dummy;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_VdbUnmounted_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_ClipInfo
//
//-----------------------------------------------------------------------

enum {
	CLIP_ACTION_CREATED = 1,
	CLIP_ACTION_CHANGED = 2,
	CLIP_ACTION_FINISHED = 3,
	CLIP_ACTION_INSERTED = 4,
	CLIP_ACTION_MOVED = 5,
};

typedef struct vdb_msg_ClipInfo_s {
	vdb_ack_t header;	// must be first
	///
	uint16_t action;		// CLIP_ACTION_CREATED, etc
	uint16_t flags;		// CLIP_IS_LIVE
	uint32_t list_pos;	// for CLIP_ACTION_INSERTED, CLIP_ACTION_MOVED
	///
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_date;
	uint32_t clip_duration_ms;
	uint32_t clip_start_time_ms_lo;
	uint32_t clip_start_time_ms_hi;
	///
	uint16_t num_streams;
	uint16_t reserved;
	avf_stream_attr_s stream_info[MAX_VDB_STREAMS];

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

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
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_ClipRemoved_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_BufferSpaceLow
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_BufferSpaceLow_s {
	vdb_ack_t header;	// must be first
	uint32_t dummy;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_BufferSpaceLow_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_BufferFull
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_BufferFull_s {
	vdb_ack_t header;	// must be first
	uint32_t dummy;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_BufferFull_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_RawData
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_RawData_s {
	vdb_ack_t header;	// must be first
	uint32_t data_type;
	uint32_t data_size;
//	uint8_t data[data_size];
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_RawData_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_PlaylistCleared
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_PlaylistCleared_s {
	vdb_ack_t header;	// must be first
	uint32_t list_type;
} vdb_msg_PlaylistCleared_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_PlaylistCreated
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_PlaylistCreated_s {
	vdb_ack_t header;	// must be first
	uint32_t list_type;
} vdb_msg_PlaylistCreated_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_PlaylistDeleted
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_PlaylistDeleted_s {
	vdb_ack_t header;	// must be first
	uint32_t list_type;
} vdb_msg_PlaylistDeleted_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_MarkLiveClipInfo
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_MarkLiveClipInfo_s {
	vdb_msg_ClipInfo_t super;
	uint32_t flags;	// not used
	int32_t delay_ms;
	int32_t before_live_ms;
	int32_t after_live_ms;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_MarkLiveClipInfo_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_SpaceInfo
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_SpaceInfo_s {
	vdb_ack_t header;	// must be first
	vdb_space_info_t space_info;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_SpaceInfo_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_RefreshPictureList
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_RefreshPictureList_s {
	vdb_ack_t header;	// must be first
} vdb_msg_RefreshPictureList_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_PictureTaken
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_PictureTaken_s {
	vdb_ack_t header;	// must be first
	uint32_t pic_date;
//	vdb_picture_name_t picture_name;
} vdb_msg_PictureTaken_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_PictureRemoved
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_PictureRemoved_s {
	vdb_ack_t header;	// must be first
//	vdb_picture_name_t picture_name;
} vdb_msg_PictureRemoved_t;

//-----------------------------------------------------------------------
//
//  VDB_MSG_CLIP_ATTR
//
//-----------------------------------------------------------------------
typedef struct vdb_msg_ClipAttr_s {
	vdb_ack_t header;	// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_attr;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_ClipAttr_t;


#endif

