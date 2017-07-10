
#ifndef __AVF_MP4I_H__
#define __AVF_MP4I_H__

typedef struct mp4i_info_s {
	///
	uint32_t clip_date;	// start time of this downloaded clip (local time of camera)
	uint32_t clip_length_ms;
	uint32_t clip_created_date;	// time of downloading (local time of phone)
	///
	uint32_t stream_version;
	uint8_t video_coding;
	uint8_t video_frame_rate;
	uint16_t video_width;
	uint16_t video_height;
	uint8_t audio_coding;
	uint8_t audio_num_channels;
	uint32_t audio_sampling_freq;
	///
	uint8_t has_gps;		// output of mp4i_read_info()
	uint8_t has_acc;		// output of by mp4i_read_info()
	uint8_t has_obd;		// output of by mp4i_read_info()
} mp4i_info_t;

struct mp4i_raw_data_s;
typedef struct mp4i_raw_data_s mp4i_raw_data_t;

// append info to the mp4 file
int mp4i_write_info(const char *mp4_filename,
	const mp4i_info_t *info,
	uint32_t jpg_size, const uint8_t *jpg_data,
	const uint8_t *gps_ack_data,	// vdb_ack_GetRawDataBlock_t
	const uint8_t *acc_ack_data,	// vdb_ack_GetRawDataBlock_t
	const uint8_t *obd_ack_data		// vdb_ack_GetRawDataBlock_t
	);

// read info from the mp4 file
int mp4i_read_info(const char *mp4_filename, mp4i_info_t *info);

// read poster jpg,
// returns byte array; jpg data size is returned by psize
// the data must be freed by mp4i_free_poster() when not needed
uint8_t *mp4i_read_poster(const char *mp4_filename, uint32_t *psize);

void mp4i_free_poster(uint8_t *poster_data);

// read and return all raw data
// the result should be freed by mp4i_free_raw_data() when not needed
mp4i_raw_data_t *mp4i_read_raw_data(const char *mp4_filename);

// free raw data
void mp4i_free_raw_data(mp4i_raw_data_t *data);

typedef struct mp4i_ack_s {
	uint8_t *data;
	uint32_t size;
} mp4i_ack_t;

void mp4i_free_ack(mp4i_ack_t *ack);

// ------------------------------------------------------------------------------------------
// get all raw data (multiple types) at time point 'clip_time_ms'
// data_types: (1<<kRawData_GPS) | (1<<kRawData_ACC) | (1<<kRawData_OBD)
// return result in ack->data as vdb_ack_GetRawData_t; return value < 0 if failed
//	vdb_ack_GetRawData_t:
//		clip_type is 0
//		clip_id is 0
// ack should be freed by mp4i_free_ack() when not needed
// ------------------------------------------------------------------------------------------
int mp4i_get_raw_data(mp4i_raw_data_t *data, uint32_t clip_time_ms, uint32_t data_types, mp4i_ack_t *ack);

// ------------------------------------------------------------------------------------------
// get all raw data (one type) at range: clip_time_ms..clip_time_ms+length_ms
// data_type: kRawData_GPS, kRawData_ACC, or kRawData_OBD
// return result in ack->data as vdb_ack_GetRawDataBlock_t; return value < 0 if failed
//	vdb_ack_GetRawDataBlock_t:
//		clip_type is 0
//		clip_id is 0
// ack should be freed by mp4i_free_ack() when not needed
// ------------------------------------------------------------------------------------------
int mp4i_get_raw_data_block(mp4i_raw_data_t *data, uint32_t clip_time_ms, uint32_t length_ms,
	uint32_t data_type, mp4i_ack_t *ack);

#endif

