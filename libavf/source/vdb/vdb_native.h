
#ifndef __VDB_NATIVE_H__
#define __VDB_NATIVE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vdb_native_s vdb_native_t;

typedef uint32_t vdb_native_time_t;
#define INVALID_VDB_NATIVE_TIME	((uint32_t)-1)

vdb_native_t *vdb_native_create(void);
int vdb_native_destroy(vdb_native_t *vdb);

// vdb_root: "/mnt/sdcard/100TRANS/"
int vdb_native_load(vdb_native_t *vdb, const char *vdb_root);
int vdb_native_unload(vdb_native_t *vdb);

int vdb_native_start_record(vdb_native_t *vdb, int has_video, int has_picture);
int vdb_native_stop_record(vdb_native_t *vdb);

enum {
	VDB_NATIVE_LOADED = (1 << 0),
	VDB_NATIVE_RECORDING = (1 << 1),
};

// returns VDB_NATIVE_LOADED | VDB_NATIVE_RECORDING
int vdb_native_get_state(vdb_native_t *vdb);

// return value should be released by vdb_native_free_filename()
char *vdb_native_create_video_filename(vdb_native_t *vdb, vdb_native_time_t *time, int stream);

// return value should be released by vdb_native_free_filename()
char *vdb_native_create_picture_filename(vdb_native_t *vdb, vdb_native_time_t *time, int stream);

// free memory returned by vdb_native_create_xxx_filename()
void vdb_native_free_filename(vdb_native_t *vdb, char *filename);

int vdb_native_init_clip(vdb_native_t *vdb, int stream);
int vdb_native_start_clip(vdb_native_t *vdb, const avf_stream_attr_t *stream_attr, int stream);
int vdb_native_end_clip(vdb_native_t *vdb, int stream);

int vdb_native_start_segment(vdb_native_t *vdb, vdb_native_time_t video_time, vdb_native_time_t picture_time, int stream);
int vdb_native_end_segment(vdb_native_t *vdb, uint32_t duration_ms, uint32_t video_fsize, int stream, int is_last_segment);

int vdb_native_add_video(vdb_native_t *vdb, uint32_t time_ms, uint32_t file_pos, int stream);
int vdb_native_video_update(vdb_native_t *vdb, uint32_t duration_ms, int stream);

int vdb_native_add_picture(vdb_native_t *vdb, const uint8_t *buffer, int size, uint64_t pts, uint32_t timescale, int stream);

// type: RAW_DATA_GPS, RAW_DATA_OBD, or RAW_DATA_ACC
int vdb_native_add_raw_data(vdb_native_t *vdb, int type, const uint8_t *buffer, int size, uint64_t timestamp_ms, int stream);

// helper 
uint64_t vdb_native_get_timestamp(void);

// logs: "ewivdp"
void vdb_native_enable_logs(const char *logs);

#ifdef __cplusplus
}
#endif

#endif

