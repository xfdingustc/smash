
#ifndef __AVF_UTIL_H__
#define __AVF_UTIL_H__

int avf_snprintf(char *buf, int size, const char *fmt, ...);

#define HASH_INIT_VAL		0
#define HASH_NEXT_VAL(_hash_val, _c) \
	do { _hash_val = (_c) + (((_hash_val) << 5) - (_hash_val)); } while (0)

extern "C" avf_u32_t avf_round_to_power_of_2(avf_u32_t size);

extern "C" avf_hash_val_t avf_calc_hash(const avf_u8_t *value);

extern "C" int avf_load_text_file(const char *filename, char **pbuffer);
extern "C" void avf_free_text_buffer(char *buffer);

extern "C" int avf_get_word(const char *ptr, char *buffer, int buffer_len, int sep_char);

extern "C" avf_u64_t avf_get_curr_tick(void);
extern "C" avf_time_t avf_get_curr_time(int incre, int *gmt_off = NULL);
extern "C" avf_time_t avf_time_add(avf_time_t time, int incre);
extern "C" sys_time_t avf_get_sys_time(int *gmt_off);
extern "C" sys_time_t avf_time_to_sys_time(avf_time_t time);
extern "C" avf_time_t avf_sys_time_to_time(sys_time_t sys_time, int *gmt_off);
extern "C" avf_time_t avf_mktime(struct tm *tm);
extern "C" void avf_change_file_time(const char *filename, avf_time_t gen_time);
extern "C" void avf_local_time(const time_t *timep, struct tm *result);

extern "C" avf_u32_t avf_crc_ieee(avf_u32_t crc, const avf_u8_t *buffer, avf_size_t length);

extern "C" const char *avf_extract_filename(const char *fullname);

extern "C" INLINE bool avf_is_parent_dir(const char *dir)
{
	if (dir[0] == '.') {
		if (dir[1] == 0 || (dir[1] == '.' && dir[2] == 0))
			return true;
	}
	return false;
}

extern "C" int avf_open_file(const char *filename, int flag, int mode);
extern "C" int avf_close_file(int fd);
extern "C" int avf_pipe_created(int fd[2]);
extern "C" void avf_file_info(void);

typedef struct avf_dir_s {
	void *_dir;	// DIR *
	struct dirent *entp;
#ifdef MINGW
	char *path;
#endif
} avf_dir_t;

extern "C" bool avf_path_is_dir(avf_dir_t *adir, struct dirent *ent);

extern "C" bool avf_open_dir(avf_dir_t *adir, const char *path);
extern "C" void avf_close_dir(avf_dir_t *adir);
extern "C" struct dirent *avf_read_dir(avf_dir_t *adir);

extern "C" void avf_sync(void);
extern "C" bool avf_mkdir(const char *path);
extern "C" bool avf_safe_mkdir(const char *path);
extern "C" bool avf_rm_dir_recursive(const char *path, avf_size_t *num_files, bool bKeepPath);
extern "C" bool avf_remove_dir_recursive(const char *path, bool bPrint = true);
extern "C" bool avf_remove_file(const char *filename, bool bShowError = true);
extern "C" bool avf_symlink(const char *target, const char *path);
extern "C" bool avf_set_file_size(int fd, avf_u64_t size);
extern "C" bool avf_rename_file(const char *filename, const char *new_filename);
extern "C" bool avf_mkdir_for_file(const char *filename);

extern "C" bool avf_file_exists(const char *name);
extern "C" bool avf_touch_file(const char *name);
extern "C" avf_u64_t avf_get_file_size(const char *name, bool bShowError = true);
extern "C" avf_u64_t avf_get_filesize(int fd);
extern "C" const char *avf_get_file_ext(const char *filename);

extern "C" avf_s64_t avf_seek(int fd, avf_u64_t offset, int whence);

extern "C" void avf_msleep(int ms);

extern "C" const char *avf_get_framerate(int framerate);

// b57fedfd-fd36-4993-ae4f-0e4617ce7d41
extern "C" void avf_gen_uuid(avf_u8_t uuid[UUID_LEN+1]);

INLINE void bset_set(avf_u8_t *bset, avf_size_t index)
{
	bset[index >> 3] |= (1 << (index & 7));
}

INLINE bool bset_contains(const avf_u8_t *bset, avf_size_t index)
{
	return (bset[index >> 3] & (1 << (index & 7))) != 0;
}

INLINE avf_u32_t load_be_16(const avf_u8_t *p)
{
	return (p[0] << 8) | p[1];
}

INLINE avf_u32_t load_be_32(const avf_u8_t *p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

INLINE avf_u32_t load_le_16(const avf_u8_t *p)
{
	return (p[1] << 8) | p[0];
}

INLINE avf_u32_t load_le_32(const avf_u8_t *p)
{
	return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
}

#define SAVE_FCC(_p, _v) \
	do { \
		(_p)[0] = (_v)[0]; \
		(_p)[1] = (_v)[1]; \
		(_p)[2] = (_v)[2]; \
		(_p)[3] = (_v)[3]; \
		(_p) += 4; \
	} while (0)

#define SAVE_BE_16(_p, _v) \
	do { \
		(_p)[0] = (_v) >> 8; \
		(_p)[1] = (avf_u8_t)(_v); \
		(_p) += 2; \
	} while (0)

#define SAVE_BE_24(_p, _vv) \
	do { \
		avf_u32_t _v = _vv; \
		(_p)[0] = (_v) >> 16; \
		(_p)[1] = (_v) >> 8; \
		(_p)[2] = (_v); \
		(_p) += 3; \
	} while (0)
		
#define SAVE_BE_32(_p, _vv) \
	do { \
		avf_u32_t _v = _vv; \
		(_p)[0] = (_v) >> 24; \
		(_p)[1] = (_v) >> 16; \
		(_p)[2] = (_v) >> 8; \
		(_p)[3] = (_v); \
		(_p) += 4; \
	} while (0)

#define SAVE_LE_16(_p, _vv) \
	do { \
		avf_u32_t _v = _vv; \
		(_p)[0] = _v; \
		(_p)[1] = _v >> 8; \
		(_p) += 2; \
	} while (0)

#define SAVE_LE_32(_p, _vv) \
	do { \
		avf_u32_t _v = _vv; \
		(_p)[0] = _v; \
		(_p)[1] = _v >> 8; \
		(_p)[2] = _v >> 16; \
		(_p)[3] = _v >> 24; \
		(_p) += 4; \
	} while (0)

INLINE void set_upload_end_header(upload_header_t *header, avf_size_t size)
{
	header->data_type = __RAW_DATA_END;
	header->data_size = size;
	header->timestamp = 0;
	header->stream = 0;
	header->duration = 0;
	header->extra.version = UPLOAD_VERSION_v2;
	header->extra.reserved1 = 0;
	header->extra.reserved2 = 0;
	header->extra.reserved3 = 0;
}

INLINE void set_upload_end_header_v2(upload_header_v2_t *header, avf_size_t size)
{
	header->u_data_type = __RAW_DATA_END;
	header->u_data_size = size;
	header->u_timestamp = 0;
	header->u_stream = 0;
	header->u_duration = 0;
	header->u_version = UPLOAD_VERSION_v2;
	header->u_flags = 0;
	header->u_param1 = 0;
	header->u_param2 = 0;
}

INLINE upload_header_t *go_next_header(upload_header_t *header)
{
	return (upload_header_t*)((avf_u8_t*)(header + 1) + header->data_size);
}

struct tick_info_s
{
#ifdef DEBUG_AVSYNC
	avf_u64_t start_ticks;
	avf_u64_t last_ticks;
	avf_u32_t min_ticks;
	avf_u32_t max_ticks;
	int count;
#endif
};

#ifdef DEBUG_AVSYNC

extern "C" void tick_info_init(tick_info_s *info);
extern "C" int __tick_info_update(tick_info_s *info, int max_count, const char *tag);
#define tick_info_update(_info, _max) __tick_info_update(_info, _max, LOG_TAG)

#else

#define tick_info_init(_info)
#define tick_info_update(_info, _max) 0

#endif

#endif

