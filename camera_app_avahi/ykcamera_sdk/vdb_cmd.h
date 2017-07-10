
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
#define VDB_VERSION_MINOR	3

#define VDB_CMD_PORT		8083
#define VDB_HTTP_PORT		8085

#define CLIPINFO_FILE_EXT	".clip"
#define INDEX_FILE_EXT		".idx"
#define VIDEO_FILE_EXT		".ts"
#define PICTURE_FILE_EXT	".jpg"
#define HLS_FILE_EXT		".m3u8"

// little endian format

#define VDB_CMD_SIZE		160
#define VDB_ACK_SIZE		160
#define VDB_MSG_SIZE		VDB_ACK_SIZE

#define VDB_ACK_MAGIC		0xFAFBFCFF

// clip types
#define CLIP_TYPE_REAL		(-1UL)	// for vdb_clip_t, used by VDB_CMD_GetIndexPicture
#define CLIP_TYPE_BUFFER	0		// buffered clip list
#define CLIP_TYPE_MARKED	1		// marked clip list
// 2-0xFF: reserved for now
#define CLIP_TYPE_PLIST0	0x100	// the first playlist
#define NUM_PLISTS			5

#define IS_PLAYLIST(_clip_type) \
	((_clip_type) >= CLIP_TYPE_PLIST0 && (_clip_type) < (CLIP_TYPE_PLIST0 + NUM_PLISTS))

#define SAVE_IN_CLIPINFO(_clip_type) \
	((_clip_type) == CLIP_TYPE_BUFFER || (_clip_type) == CLIP_TYPE_MARKED)

#define CLIP_TYPE_FIRST_PLIST	CLIP_TYPE_PLIST0
#define CLIP_TYPE_LAST_PLIST	(CLIP_TYPE_PLIST0 + NUM_PLISTS - 1)

// url type
#define URL_TYPE_TS		0
#define URL_TYPE_HLS	1

#define _URL_MASK		0x000000FF
#define URL_MUTE_AUDIO	(1 << 31)

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

	VDB_CMD_GetClipSetInfo = 2,
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
	VDB_CMD_GetRawDataSize = 37,
	VDB_CMD_GetPlaybackUrlEx = 38,

	//-----------------------
	// since version 1.3
	//	for still pictures
	//-----------------------
	VDB_CMD_GetPictureList = 39,
	VDB_CMD_ReadPicture = 40,
	VDB_CMD_RemovePicture = 41,

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
	// since version 1.2
	//-----------------------
	VDB_MSG_MarkLiveClipInfo = _VDB_MSG_START + 32,

	//-----------------------
	// since version 1.3
	//	for still pictures
	//-----------------------
	VDB_MSG_RefreshPictureList = _VDB_MSG_START + 64,
	VDB_MSG_PictureTaken = _VDB_MSG_START + 65,
	VDB_MSG_PictureRemoved = _VDB_MSG_START + 66,
};

// 16 bytes
typedef struct vdb_cmd_header_s {
	uint16_t cmd_code;		// command code
	uint16_t cmd_flags;		// command flags (depends on cmd)
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
//  VDB_CMD_GetDownloadUrl - get clip download url
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
//	VDB_CMD_GetRawDataBlock
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetRawDataBlock_s {
	vdb_cmd_header_t header;		// must be first
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_time_ms_lo;	// clip_start_time_ms + time_offset
	uint32_t clip_time_ms_hi;
	uint32_t length_ms;
	uint32_t data_type;			// kRawData_GPS, kRawData_ACC, kRawData_OBD
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetRawDataBlock_t;

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
	uint32_t data_type;
	uint32_t requested_time_ms_lo;	// requested
	uint32_t requested_time_ms_hi;
	uint32_t num_items;
	uint32_t data_size;		// total

//	vdb_raw_data_index_t index[num_items];

//	for (i = 0; i < num_items; i++) {
//		uint8_t item_data[index[i].data_size];
//	}

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetRawDataBlock_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetAllPlaylists
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetAllPlaylists_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t flags;			// not used now
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
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

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetAllPlaylist_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_GetPlaylistIndexPicture
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_GetPlaylistIndexPicture_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t list_type;		// must be CLIP_TYPE_PLIST0 etc
	uint32_t flags;			// TBD
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetPlaylistIndexPicture_t;

typedef struct vdb_ack_GetPlaylistIndexPicture_s {
	uint32_t list_type;
	uint32_t flags;			// TBD

	uint32_t clip_date;
	uint32_t clip_time_ms_lo;
	uint32_t clip_time_ms_hi;

	uint32_t picture_size;
//	uint8_t jpeg_data[picture_size];

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetPlaylistIndexPicture_t;

//-----------------------------------------------------------------------
//
//	VDB_CMD_ClearPlaylist
//
//-----------------------------------------------------------------------
typedef struct vdb_cmd_ClearPlaylist_s {
	vdb_cmd_header_t header;	// must be first
	uint32_t list_type;		// must be CLIP_TYPE_PLIST0, CLIP_TYPE_PLIST0 etc
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_ClearPlaylist_t;

#define e_ClearPlaylist_OK				0
#define e_ClearPlaylist_NotFound		1	// playlist not found
#define e_ClearPlaylist_NotPermitted	2	// cannot clear playlist

typedef struct vdb_ack_ClearPlaylist_s {
	uint32_t status;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
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
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_InsertClip_t;

#define e_InsertClip_OK				0
#define e_InsertClip_BadParam		1
#define e_InsertClip_NotFound		2	// clip or playlist not found
#define e_InsertClip_TooMany		3	// too many clips in the list
#define e_InsertClip_UnknownStream	4	// clip attribute unknown
#define e_InsertClip_StreamNotMatch	5	// clip attribute does not match other clips

typedef struct vdb_ack_InsertClip_s {
	uint32_t status;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
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
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_MoveClip_t;

#define e_MoveClip_OK			0
#define e_MoveClip_BadParam	1

typedef struct vdb_ack_MoveClip_s {
	uint32_t status;
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
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
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
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

//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}

} vdb_ack_GetPlaylistPlaybackUrl_t;

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

	uint32_t real_clip_id;	// use this with CLIP_TYPE_REAL to get index picture
	uint32_t reserved;

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
	uint32_t flags;		// GET_CLIP_EXTRA | GET_CLIP_VDB_ID
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_cmd_GetClipSetInfoEx_t;

// vdb_clip_info_ex_t.inherited.flags
#define GET_CLIP_EXTRA		(1 << 0)
#define GET_CLIP_VDB_ID		(1 << 1)
#define CLIPF_IS_LIVE		(1 << 2)	// this clip is live
#define CLIPF_NO_DELETE		(1 << 3)	// this clip cannot be deleted

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

//	if (inherited.flags & GET_CLIP_VDB_ID) {
//		vdb_id_t vdb_id;
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
	uint32_t flags;
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
//  VDB_CMD_GetRawDataSize
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

enum {
	CLIP_IS_LIVE = (1 << 0),
};

typedef struct avf_stream_attr_s {
	// flags
	uint32_t version;			// 0: invalid stream; should set to CURRENT_STREAM_VERSION
	// video
	uint8_t video_coding;		// VideoCoding_Unknown
	uint8_t video_framerate;		// FrameRate_Unknown
	uint16_t video_width;
	uint16_t video_height;
	// audio
	uint8_t audio_coding;		// AudioCoding_Unknown
	uint8_t audio_num_channels;
	uint32_t audio_sampling_freq;	// 44100
} avf_stream_attr_t;


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
//	if (VDB_HAS_ID) {
//		vdb_id_t vdb_id;
//	}
} vdb_msg_PlaylistCleared_t;

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

#endif

