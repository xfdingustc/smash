
#ifndef __AVF_STD_MEDIA_H__
#define __AVF_STD_MEDIA_H__

#define UUID_LEN	36

#define _90kHZ	90000

//-----------------------------------------------------------------------
//
//  video type
//
//-----------------------------------------------------------------------

enum {
	VideoCoding_Unknown = 0,
	VideoCoding_AVC = 1,
	VideoCoding_MJPEG = 2,
};

//-----------------------------------------------------------------------
//
//  video frame rate
//
//-----------------------------------------------------------------------

enum {
	FrameRate_Unknown = 0,
	FrameRate_12_5 = 1,
	FrameRate_6_25 = 2,
	FrameRate_23_976 = 3,
	FrameRate_24 = 4,
	FrameRate_25 = 5,
	FrameRate_29_97 = 6,
	FrameRate_30 = 7,
	FrameRate_50 = 8,
	FrameRate_59_94 = 9,
	FrameRate_60 = 10,
	FrameRate_120 = 11,
	FrameRate_240 = 12,
	FrameRate_20 = 13,
	FrameRate_15 = 14,
	FrameRate_14_985 = 15,
	FrameRate_1 = 16,
	FrameRate_Interlaced = 0x80,	// flag
};

//-----------------------------------------------------------------------
//
//  audio type
//
//-----------------------------------------------------------------------
enum {
	AudioCoding_Unknown = 0,
	AudioCoding_PCM = 1,
	AudioCoding_AAC = 2,	// MPEG2, LC, after-burner=0
	AudioCoding_MP3 = 3,
	AudioCoding_MPEG4_AAC = 4,
};


#define VIDEO_TYPE_TS	0
#define VIDEO_TYPE_MP4	1


//-----------------------------------------------------------------------
//
//  stream info (16 bytes)
//
//-----------------------------------------------------------------------
typedef struct avf_stream_attr_s {
	// flags
#if 0
	uint32_t version;			// 0: invalid stream; should set to CURRENT_STREAM_VERSION
#else
	uint8_t stream_version;		// 0: invalid stream; should set to CURRENT_STREAM_VERSION
	uint8_t video_version;
	uint8_t audio_version;
	uint8_t extra_size;
#endif
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

#define CURRENT_STREAM_VERSION	2

//-----------------------------------------------------------------------
//
//	acc/iio raw data structure
//
//-----------------------------------------------------------------------
typedef struct acc_raw_data_s {
	int32_t accel_x;
	int32_t accel_y;
	int32_t accel_z;
} acc_raw_data_t;


#define IIO_VERSION			1

#define IIO_F_ACCEL			(1 << 0)	// accel_x,y,z are valid
#define IIO_F_GYRO			(1 << 1)	// gyro_x,y,z are valid
#define IIO_F_MAGN			(1 << 2)	// magn_x,y,z are valid
#define IIO_F_EULER			(1 << 3)	// euler are valid
#define IIO_F_QUATERNION	(1 << 4)	// quaternion_w,x,y,z are valid
#define IIO_F_PRESSURE		(1 << 5)	// pressure is valid

typedef struct iio_raw_data_s {

	// accel : g x 1000 = mg
	int32_t accel_x;
	int32_t accel_y;
	int32_t accel_z;

	//---------------------------------------------------
	uint16_t version;	// IIO_VERSION
	uint16_t size;		// sizeof(iio_raw_data_s)
	uint32_t flags;		// IIO_F_ACCEL etc
	//---------------------------------------------------

	// gyro : Dps x 1000 = mDps
	int32_t gyro_x;
	int32_t gyro_y;
	int32_t gyro_z;

	// magn : uT x 1000000
	int32_t magn_x;
	int32_t magn_y;
	int32_t magn_z;

	// Orientation
	// Euler : Degrees x 1000 = mDegrees
	int32_t euler_heading;
	int32_t euler_roll;
	int32_t euler_pitch;

	// Quaternion : Raw, no unit
	int32_t quaternion_w;
	int32_t quaternion_x;
	int32_t quaternion_y;
	int32_t quaternion_z;

	// Pressure: Pa x 1000
	int32_t pressure;

} iio_raw_data_t;

//-----------------------------------------------------------------------
//
//	gps raw data structure
//
//-----------------------------------------------------------------------
#define GPS_F_LATLON	(1 << 0)	// latitude and longitude are valid
#define GPS_F_ALTITUDE	(1 << 1)	// altitude is valid
#define GPS_F_SPEED		(1 << 2)	// speed is valid
#define GPS_F_TIME		(1 << 3)	// time is valid
#define GPS_F_TRACK		(1 << 4)	// track is valid
#define GPS_F_DOP		(1 << 5)	// accuracy is valid

/* obsolete
// size: 32 bytes
typedef struct gps_raw_data_s {
	uint32_t flags;	// GPS_F_LATLON, GPS_F_ALTITUDE, GPS_F_SPEED
	float speed;
	double latitude;
	double longitude;
	double altitude;
} gps_raw_data_t;
*/

/* obsolete
// size: 88 bytes
typedef struct gps_raw_data_v2_s {
	uint32_t flags;	// GPS_F_LATLON, GPS_F_ALTITUDE, GPS_F_SPEED
	float speed;
	double latitude;
	double longitude;
	double altitude;
	///
	uint32_t utc_time;
	struct tm utc_tm;	// 44 bytes
	float track;
	float accuracy;
} gps_raw_data_v2_t;
*/

// size: 48 bytes
typedef struct gps_raw_data_v3_s {
	uint32_t flags;	// GPS_F_LATLON, GPS_F_ALTITUDE, GPS_F_SPEED
	float speed;
	double latitude;
	double longitude;
	double altitude;
	///
	uint32_t utc_time;
	float track;
	float accuracy;
	uint32_t utc_time_usec;
} gps_raw_data_v3_t;



#define OBD_VERSION_1	1
#define OBD_VERSION_2	2

//-----------------------------------------------------------------------
//
//	obd raw data v1
//
//-----------------------------------------------------------------------

typedef struct obd_index_s {
	uint16_t flag;		// 0x1: valid
	uint16_t offset;		// offset into data[]
} obd_index_t;

typedef struct obd_raw_data_s {
	uint32_t revision;	// OBD_VERSION_1
	uint32_t total_size;
	uint32_t pid_info_size;
	uint32_t pid_data_size;
	uint64_t pid_pts;
	uint64_t pid_polling_delay;
	uint64_t pid_polling_diff;
//	obd_index_t index[];	// bytes: pid_info_size
//	uint8_t data[];		// bytes: pid_data_size
} obd_raw_data_t;

//-----------------------------------------------------------------------
//
//	obd raw data v2
//
//-----------------------------------------------------------------------

/*
#define TOTAL_PID	128
static const uint8_t g_pid_data_size_table[TOTAL_PID] =
{
	4,4,2,2,1,1,2,2,		// 00 - 07
	2,2,1,1,2,1,1,1,		// 08 - 0F

	2,1,1,1,2,2,2,2,		// 10 - 17
	2,2,2,2,1,1,1,2,		// 18 - 1F

	4,2,2,2,4,4,4,4,		// 20 - 27
	4,4,4,4,1,1,1,1,		// 28 - 2F

	1,2,2,1,4,4,4,4,		// 30 - 37
	4,4,4,4,2,2,2,2,		// 38 - 3F

	4,4,2,2,2,1,1,1,		// 40 - 47
	1,1,1,1,1,2,2,4,		// 48 - 4F

	4,1,1,2,2,2,2,2,		// 50 - 57
	2,2,1,1,1,2,2,1,		// 58 - 5F

	4,1,1,2,5,2,5,3,		// 60 - 67
	7,7,5,5,5,6,5,3,		// 68 - 6F

	9,5,5,5,5,7,7,5,		// 70 - 77
	9,9,7,7,9,1,1,13,		// 78 - 7F
};
*/

// (pid,data) (pid,data) ... (pid,data) 0
// pid_data is little endian
typedef struct obd_raw_data_v2_s {
	uint8_t revision;	// OBD_VERSION_2
//	for (;;) {
//		uint8_t pid;
//		if (pid == 0)
//			break;
//		uint8_t pid_data[g_pid_data_size_table[pid]];
//	}
//	uint8_t padding_zeros[align_to_4n_byte];
} obd_raw_data_v2_t;

//-----------------------------------------------------------------------
//
//	raw data structure
//
//-----------------------------------------------------------------------
typedef struct raw_dentry_s {
	int32_t time_ms;		// offset relative to seg_start_time or clip_start_time
	uint32_t fpos;		// offset relative to seg_start_addr or raw data block
} raw_dentry_t;

typedef struct vdb_raw_data_s {
	uint64_t clip_start_time_ms; // may diff with param clip_time_ms of VDB_ReadRawData()
	uint32_t num_entries;
	raw_dentry_t *dentries;
	void *data;
	uint32_t data_size;
} vdb_raw_data_t;

//-----------------------------------------------------------------------
//
//	clip desc
//
//-----------------------------------------------------------------------

typedef struct vdb_clip_desc_s {
	uint32_t clip_type;
	uint32_t clip_id;
	uint32_t clip_date;			// seconds from 1970 (when the clip was created)
	uint32_t clip_duration_ms;
	uint64_t clip_start_time;
	uint16_t num_streams;
	uint16_t reserved;
	char *vdb_id;	// 
	avf_stream_attr_t stream_attr[2];
} vdb_clip_desc_t;

//-----------------------------------------------------------------------
//
//	vin config info
//
//-----------------------------------------------------------------------

typedef struct vin_config_info_s {
	uint8_t		version;	// 0

	uint8_t		bits;
	uint8_t		mirror_horizontal;
	uint8_t		mirror_vertical;
	uint8_t		hdr_mode;
	uint8_t		enc_mode;
	uint8_t		anti_flicker;
	uint8_t		exposure;

	uint8_t		dev_name[8];
	uint32_t	source_id;
	uint32_t	video_mode;
	uint32_t	fps_q9;
} vin_config_info_t;

//-----------------------------------------------------------------------
//
//	raw data structure
//
//-----------------------------------------------------------------------
enum {
	__VDB_DATA_UNKNOWN = 0,	// should not be used

	// 0: normal mode
	VDB_DATA_VIDEO = 1,	// ts per second

	// jpeg picture: for all modes
	VDB_DATA_JPEG = 2,

	// raw data types: for all modes
	RAW_DATA_GPS = 3,
	RAW_DATA_OBD = 4,
	RAW_DATA_ACC = 5,
	VDB_RAW_DATA_BLOCK = 6,	// contains gps/obd/acc

	// 1: rtsp mode
	VDB_VIDEO_INFO = 7,	// avf_stream_attr_t
	VDB_TS_FRAME = 8,	// ts_packets

	// 2: rtmp mode
	VDB_VIDEO_FRAME = 9,
	VDB_AUDIO_FRAME = 10,
	VDB_STREAM_ATTR = 11,	// avf_stream_attr_t

	// info
	__UPLOAD_INFO = -3U,
	__UPLOAD_END = -2U,
	__RAW_DATA_END = -1U
};

#define RAW_DATA_GPS_FLAG	(1 << RAW_DATA_GPS)
#define RAW_DATA_OBD_FLAG	(1 << RAW_DATA_OBD)
#define RAW_DATA_ACC_FLAG	(1 << RAW_DATA_ACC)

#define UPLOAD_OPT_VIDEO	(1 << 0)
#define UPLOAD_OPT_AUDIO	(1 << 1)
#define UPLOAD_OPT_JPEG		(1 << 2)
#define UPLOAD_OPT_GPS		(1 << 3)
#define UPLOAD_OPT_OBD		(1 << 4)
#define UPLOAD_OPT_ACC		(1 << 5)

#define UPLOAD_OPT_ALL		0x00FF

//-----------------------------------------------------------------------
// upload data block structure:
//	item + item + ... + item + end
//	item:
//		upload_header_t + data
//		data:
//			VDB_DATA_VIDEO - avf_stream_attr_t + ts_packet + ts_packet + ... + ts_packet
//			VDB_DATA_JPEG - .jpg
//			RAW_DATA_GPS - gps_raw_data_v3_t
//			RAW_DATA_OBD - obd_raw_data_t
//			RAW_DATA_ACC - acc_raw_data_t
//	end:
//		upload_header_t (data_type == __RAW_DATA_END)
//			data_type: __RAW_DATA_END
//			data_size: summary of all previous items
//-----------------------------------------------------------------------

#define UPLOAD_VERSION	1

// total 32 bytes
typedef struct upload_header_s {
	uint32_t data_type;		// VDB_DATA_VIDEO etc
	uint32_t data_size;		// data size, NOT including this header
	uint64_t timestamp;		// 90 khz for video & picture; 1 khz for raw; 0 for others
	uint32_t stream;			// 0 or 1 for VDB_DATA_VIDEO; 0 for others
	uint32_t duration;		// 90 khz for video; for picture & raw, it is 0
	// uint64_t reserved; => extra
	struct extra_s {
		uint8_t version;		// should be UPLOAD_VERSION
		uint8_t reserved1;	// reserved; must be 0
		uint16_t reserved2;	// reserved; must be 0
		uint32_t reserved3;	// reserved; must be 0
	} extra;
} upload_header_t;

//-----------------------------------------------------------------------
// upload data sequence V2:
//	item + item + ... + item + end
//	item:
//		upload_header_t + data
//	end:
//		upload_header_t
//			data_type: __UPLOAD_END
//			data_size: 0
//			timestamp: end time(ms) of the uploaded clip
//			duration: 0
//-----------------------------------------------------------------------

#define UPLOAD_VERSION_v2	2

// total 32 bytes
typedef struct upload_header_v2_s {
	uint32_t u_data_type;	// VDB_DATA_VIDEO etc
	uint32_t u_data_size;	// data size, NOT including this header
	uint64_t u_timestamp;	// unit: ms
	uint32_t u_stream;		// 0 or 1 for VDB_DATA_VIDEO; 0 for others
	uint32_t u_duration;		// unit: ms
	uint8_t u_version;		// UPLOAD_VERSION_v2
	uint8_t u_flags;			// 
	uint16_t u_param1;		//
	uint32_t u_param2;		//
} upload_header_v2_t;

#endif

