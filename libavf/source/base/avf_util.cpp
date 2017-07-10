
#define LOG_TAG "avf_util"

#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#include "avf_new.h"
#include "avf_common.h"

#include "avf_util.h"

#if defined(IOS_OS) || defined(MAC_OS)
#include <assert.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#ifdef WIN32_OS
#include <windows.h>
#endif

int avf_snprintf(char *buf, int size, const char *fmt, ...)
{
	if (size <= 0) {
		return 0;
	}

	int rval;

	va_list arg;
	va_start(arg, fmt);
	rval = vsnprintf(buf, size, fmt, arg);
	va_end(arg);

	return rval < size ? rval : size;
}

#if defined(WIN32_OS) || defined(MAC_OS)

static void default_print_proc(const char *msg)
{
	printf("%s\n", msg);
}

void (*avf_print_proc)(const char *) = default_print_proc;

#endif

#ifdef LINUX_OS

int (*avf_printf)(const char *fmt, ...) = printf;

#endif

extern "C" int avf_load_text_file(const char *filename, char **pbuffer)
{
	int fd;
	char *buffer;
	int size;

	if ((fd = avf_open_file(filename, O_RDONLY, 0)) < 0) {
		AVF_LOGP("cannot open %s", filename);
		return fd;
	}

	size = (int)::lseek(fd, 0, SEEK_END);
	::lseek(fd, 0, SEEK_SET);

	if ((buffer = (char*)avf_malloc(size + 1)) == NULL) {
		avf_close_file(fd);
		return -1;
	}

	if (::read(fd, buffer, size) != size) {
		avf_free(buffer);
		avf_close_file(fd);
		return -1;
	}

	avf_close_file(fd);

	buffer[size] = 0;
	*pbuffer = buffer;
	return 0;
}

extern "C" void avf_free_text_buffer(char *buffer)
{
	avf_free(buffer);
}

extern "C" int avf_get_word(const char *ptr, char *buffer, int buffer_len, int sep_char)
{
	const char *p;
	int size;

	// find 'sep_char' or EOS
	for (p = ptr; *p; p++) {
		if (*p == sep_char)
			break;
	}

	size = p - ptr;
	if (size >= buffer_len)
		return -1;

	::strncpy(buffer, ptr, size);
	buffer[size] = 0;

	if (*p == sep_char)
		size++;

	return size;
}

// return is ms
extern "C" avf_u64_t avf_get_curr_tick(void)
{
#if defined(IOS_OS) || defined(MAC_OS)

    static mach_timebase_info_data_t    sTimebaseInfo;
    uint64_t        startNano;
    uint64_t start = mach_absolute_time();
    if ( sTimebaseInfo.denom == 0 ) {
        (void) mach_timebase_info(&sTimebaseInfo);
    }
    
    startNano = start * sTimebaseInfo.numer / sTimebaseInfo.denom;
    return startNano / 1000000;

#else
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000;

#endif

}

extern "C" avf_time_t avf_get_curr_time(int incre, int *gmt_off)
{
	time_t curr_time = time(NULL);
	if (curr_time < AVF_MIN_TIME)
		curr_time = AVF_MIN_TIME;

	curr_time += incre;
	return avf_sys_time_to_time(curr_time, gmt_off);
}

extern "C" avf_time_t avf_time_add(avf_time_t time, int incre)
{
	sys_time_t sys_time = avf_time_to_sys_time(time);
	sys_time += incre;
	return avf_sys_time_to_time(sys_time, NULL);
}

extern "C" avf_time_t avf_mktime(struct tm *tm)
{
	return ((tm->tm_year + 1900 - AVF_MIN_YEAR) << YEAR_SHIFT) |
		((tm->tm_mon + 1) << MONTH_SHIFT) |
		(tm->tm_mday << DAY_SHIFT) |
		(tm->tm_hour << HH_SHIFT) |
		(tm->tm_min << MM_SHIFT) |
		(tm->tm_sec << SS_SHIFT);
}

extern "C" sys_time_t avf_get_sys_time(int *gmt_off)
{
	time_t curr_time = time(NULL);

	struct tm tm;
	time_t tmp = curr_time;
	avf_local_time(&tmp, &tm);
#ifndef MINGW
	curr_time += tm.tm_gmtoff;
#endif

	if (gmt_off) {
#ifndef MINGW
		*gmt_off = tm.tm_gmtoff;
#else
		*gmt_off = 0;
#endif
	}

	return curr_time;
}

extern "C" avf_time_t avf_sys_time_to_time(sys_time_t sys_time, int *gmt_off)
{
	time_t tmp = sys_time;
	struct tm tm;
	avf_local_time(&tmp, &tm);

	if (gmt_off) {
#ifndef MINGW
		*gmt_off = tm.tm_gmtoff;
#else
		*gmt_off = 0;
#endif
	}

	return avf_mktime(&tm);
}

extern "C" sys_time_t avf_time_to_sys_time(avf_time_t time)
{
	struct tm tm;
	::memset(&tm, 0, sizeof(tm));

	tm.tm_year = GET_YEAR(time) + AVF_MIN_YEAR - 1900;
	tm.tm_mon = GET_MONTH(time) - 1;
	tm.tm_mday = GET_DAY(time);
	tm.tm_hour = GET_HH(time);
	tm.tm_min = GET_MM(time);
	tm.tm_sec = GET_SS(time);
	tm.tm_isdst = -1;	// let mktime decide

	return mktime(&tm);
}

// change file access time and modification time, not creation time
extern "C" void avf_change_file_time(const char *filename, avf_time_t gen_time)
{
	if (gen_time != 0) {
		sys_time_t sys_time = avf_time_to_sys_time(gen_time);
		struct utimbuf utimbuf = {sys_time, sys_time};
		utime(filename, &utimbuf);
	}
}

// 1. localtime() is not thread-safe.
// 2. localtime_r() does not check timezone change.
CMutex g_time_lock;
extern "C" void avf_local_time(const time_t *timep, struct tm *result)
{
	AUTO_LOCK(g_time_lock);
	const struct tm *tm = localtime(timep);
	*result = *tm;
}

extern "C" void avf_dump_mem(const void *p, avf_size_t size)
{
	int i, j, c;
	const char *ptr = (const char*)p;

	c = size / 16;
	for (i = 0; i < c; i++, ptr += 16) {
		for (j = 0; j < 16; j++) {
			printf("%02x ", ptr[j]);
		}
		printf("  ");
		for (j = 0; j < 16; j++) {
			if (isprint(ptr[j])) printf("%c", ptr[j]);
			else printf(".");
		}
		printf("\n");
	}

	c = size % 16;
	if (c > 0) {
		for (j = 0; j < c; j++) {
			printf("%02x ", ptr[j]);
		}
		for (; j < 16; j++) {
			printf("   ");
		}
		for (j = 0; j < c; j++) {
			if (isprint(ptr[j])) printf("%c", ptr[j]);
			else printf(".");
		}
		printf("\n");
	}
}

static avf_u32_t g_crc_32_ieee[] = {
    0x00000000, 0xB71DC104, 0x6E3B8209, 0xD926430D, 0xDC760413, 0x6B6BC517,
    0xB24D861A, 0x0550471E, 0xB8ED0826, 0x0FF0C922, 0xD6D68A2F, 0x61CB4B2B,
    0x649B0C35, 0xD386CD31, 0x0AA08E3C, 0xBDBD4F38, 0x70DB114C, 0xC7C6D048,
    0x1EE09345, 0xA9FD5241, 0xACAD155F, 0x1BB0D45B, 0xC2969756, 0x758B5652,
    0xC836196A, 0x7F2BD86E, 0xA60D9B63, 0x11105A67, 0x14401D79, 0xA35DDC7D,
    0x7A7B9F70, 0xCD665E74, 0xE0B62398, 0x57ABE29C, 0x8E8DA191, 0x39906095,
    0x3CC0278B, 0x8BDDE68F, 0x52FBA582, 0xE5E66486, 0x585B2BBE, 0xEF46EABA,
    0x3660A9B7, 0x817D68B3, 0x842D2FAD, 0x3330EEA9, 0xEA16ADA4, 0x5D0B6CA0,
    0x906D32D4, 0x2770F3D0, 0xFE56B0DD, 0x494B71D9, 0x4C1B36C7, 0xFB06F7C3,
    0x2220B4CE, 0x953D75CA, 0x28803AF2, 0x9F9DFBF6, 0x46BBB8FB, 0xF1A679FF,
    0xF4F63EE1, 0x43EBFFE5, 0x9ACDBCE8, 0x2DD07DEC, 0x77708634, 0xC06D4730,
    0x194B043D, 0xAE56C539, 0xAB068227, 0x1C1B4323, 0xC53D002E, 0x7220C12A,
    0xCF9D8E12, 0x78804F16, 0xA1A60C1B, 0x16BBCD1F, 0x13EB8A01, 0xA4F64B05,
    0x7DD00808, 0xCACDC90C, 0x07AB9778, 0xB0B6567C, 0x69901571, 0xDE8DD475,
    0xDBDD936B, 0x6CC0526F, 0xB5E61162, 0x02FBD066, 0xBF469F5E, 0x085B5E5A,
    0xD17D1D57, 0x6660DC53, 0x63309B4D, 0xD42D5A49, 0x0D0B1944, 0xBA16D840,
    0x97C6A5AC, 0x20DB64A8, 0xF9FD27A5, 0x4EE0E6A1, 0x4BB0A1BF, 0xFCAD60BB,
    0x258B23B6, 0x9296E2B2, 0x2F2BAD8A, 0x98366C8E, 0x41102F83, 0xF60DEE87,
    0xF35DA999, 0x4440689D, 0x9D662B90, 0x2A7BEA94, 0xE71DB4E0, 0x500075E4,
    0x892636E9, 0x3E3BF7ED, 0x3B6BB0F3, 0x8C7671F7, 0x555032FA, 0xE24DF3FE,
    0x5FF0BCC6, 0xE8ED7DC2, 0x31CB3ECF, 0x86D6FFCB, 0x8386B8D5, 0x349B79D1,
    0xEDBD3ADC, 0x5AA0FBD8, 0xEEE00C69, 0x59FDCD6D, 0x80DB8E60, 0x37C64F64,
    0x3296087A, 0x858BC97E, 0x5CAD8A73, 0xEBB04B77, 0x560D044F, 0xE110C54B,
    0x38368646, 0x8F2B4742, 0x8A7B005C, 0x3D66C158, 0xE4408255, 0x535D4351,
    0x9E3B1D25, 0x2926DC21, 0xF0009F2C, 0x471D5E28, 0x424D1936, 0xF550D832,
    0x2C769B3F, 0x9B6B5A3B, 0x26D61503, 0x91CBD407, 0x48ED970A, 0xFFF0560E,
    0xFAA01110, 0x4DBDD014, 0x949B9319, 0x2386521D, 0x0E562FF1, 0xB94BEEF5,
    0x606DADF8, 0xD7706CFC, 0xD2202BE2, 0x653DEAE6, 0xBC1BA9EB, 0x0B0668EF,
    0xB6BB27D7, 0x01A6E6D3, 0xD880A5DE, 0x6F9D64DA, 0x6ACD23C4, 0xDDD0E2C0,
    0x04F6A1CD, 0xB3EB60C9, 0x7E8D3EBD, 0xC990FFB9, 0x10B6BCB4, 0xA7AB7DB0,
    0xA2FB3AAE, 0x15E6FBAA, 0xCCC0B8A7, 0x7BDD79A3, 0xC660369B, 0x717DF79F,
    0xA85BB492, 0x1F467596, 0x1A163288, 0xAD0BF38C, 0x742DB081, 0xC3307185,
    0x99908A5D, 0x2E8D4B59, 0xF7AB0854, 0x40B6C950, 0x45E68E4E, 0xF2FB4F4A,
    0x2BDD0C47, 0x9CC0CD43, 0x217D827B, 0x9660437F, 0x4F460072, 0xF85BC176,
    0xFD0B8668, 0x4A16476C, 0x93300461, 0x242DC565, 0xE94B9B11, 0x5E565A15,
    0x87701918, 0x306DD81C, 0x353D9F02, 0x82205E06, 0x5B061D0B, 0xEC1BDC0F,
    0x51A69337, 0xE6BB5233, 0x3F9D113E, 0x8880D03A, 0x8DD09724, 0x3ACD5620,
    0xE3EB152D, 0x54F6D429, 0x7926A9C5, 0xCE3B68C1, 0x171D2BCC, 0xA000EAC8,
    0xA550ADD6, 0x124D6CD2, 0xCB6B2FDF, 0x7C76EEDB, 0xC1CBA1E3, 0x76D660E7,
    0xAFF023EA, 0x18EDE2EE, 0x1DBDA5F0, 0xAAA064F4, 0x738627F9, 0xC49BE6FD,
    0x09FDB889, 0xBEE0798D, 0x67C63A80, 0xD0DBFB84, 0xD58BBC9A, 0x62967D9E,
    0xBBB03E93, 0x0CADFF97, 0xB110B0AF, 0x060D71AB, 0xDF2B32A6, 0x6836F3A2,
    0x6D66B4BC, 0xDA7B75B8, 0x035D36B5, 0xB440F7B1, 0x00000001
};

extern "C" avf_u32_t avf_crc_ieee(avf_u32_t crc, const avf_u8_t *buffer, avf_size_t length)
{
	while (length-- > 0)
		crc = g_crc_32_ieee[((avf_u8_t)crc) ^ *buffer++] ^ (crc >> 8);
	return crc;
}

extern "C" bool avf_path_is_dir(avf_dir_t *adir, struct dirent *ent)
{
#ifdef MINGW
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", adir->path, ent->d_name);
	DWORD res = GetFileAttributesA(path);
	if (res == INVALID_FILE_ATTRIBUTES)
		return false;
	return (res & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	return ent->d_type == DT_DIR;
#endif
}

static avf_atomic_t g_num_files;

extern "C" int avf_open_file(const char *filename, int flag, int mode)
{
	if (mode == 0) {
		mode = AVF_FILE_MODE;
	}

#ifdef WIN32_OS
	int fd = ::open(filename, flag|_O_BINARY, mode);
#else
	int fd = ::open(filename, flag, mode);
#endif

	if (fd >= 0) {
		avf_atomic_inc(&g_num_files);
	} else {
		AVF_LOGP("cannot open %s", filename);
	}

	return fd;
}

extern "C" int avf_close_file(int fd)
{
	if (fd >= 0) {
		::close(fd);
		avf_atomic_dec(&g_num_files);
	}
	return E_OK;
}

extern "C" int avf_pipe_created(int fd[2])
{
	avf_atomic_add(&g_num_files, 2);
	return E_OK;
}

extern "C" void avf_file_info(void)
{
	if (g_num_files == 0) {
		AVF_LOGI(C_YELLOW "there are no open files" C_NONE);
	} else {
		AVF_LOGE("there are %d files not closed", g_num_files);
	}
}

extern "C" const char *avf_extract_filename(const char *fullname)
{
	const char *p = fullname + strlen(fullname);
	while (p > fullname) {
		if (*p == '/')
			return p + 1;
		p--;
	}
	return fullname;
}

extern "C" void avf_sync(void)
{
#ifdef LINUX_OS
	AVF_LOGI(C_CYAN "call sync()" C_NONE);
	sync();
#endif
}

extern "C" bool avf_mkdir(const char *path)
{
#ifdef MINGW
	if (::mkdir(path) < 0) {
		AVF_LOGE("mkdir %d: %s", errno, path);
		return false;
	}
#else
	if (::mkdir(path, AVF_DIR_MODE) < 0) {
		AVF_LOGE("mkdir %d: %s", errno, path);
		return false;
	}
#endif
	return true;
}

extern "C" bool avf_safe_mkdir(const char *path)
{
	if (!avf_file_exists(path)) {
		return avf_mkdir(path);
	}
	return true;
}

extern "C" bool avf_open_dir(avf_dir_t *adir, const char *path)
{
	adir->_dir = NULL;
	adir->entp = NULL;

#ifdef MINGW
	adir->path = avf_strdup(path);
	if (adir->path == NULL) {
		return false;
	}
#endif

	if ((adir->_dir = (DIR*)::opendir(path)) == NULL) {
#ifdef MINGW
		avf_safe_free(adir->path);
#endif
		return false;
	}

	int size = offsetof(struct dirent, d_name) + 1024 + 1;
	if ((adir->entp = (struct dirent*)::avf_malloc(size)) == NULL) {
		::closedir((DIR*)adir->_dir);
		adir->_dir = NULL;
#ifdef MINGW
		avf_safe_free(adir->path);
#endif
		return false;
	}

	return true;
}

extern "C" void avf_close_dir(avf_dir_t *adir)
{
#ifdef MINGW
	avf_safe_free(adir->path);
#endif
	avf_safe_free(adir->entp);
	::closedir((DIR*)adir->_dir);
}

extern "C" struct dirent *avf_read_dir(avf_dir_t *adir)
{
#ifdef MINGW
	return ::readdir((DIR*)adir->_dir);
#else
	struct dirent *result = NULL;
	if (::readdir_r((DIR*)adir->_dir, adir->entp, &result) != 0) {
		return NULL;
	}
	return result;
#endif
}

// should be '/' ended
extern "C" bool avf_rm_dir_recursive(const char *path, avf_size_t *num_files, bool bKeepPath)
{
	avf_dir_t adir;
	struct dirent *ent;

	if (!avf_open_dir(&adir, path))
		return false;

	while ((ent = avf_read_dir(&adir)) != NULL) {
		if (avf_is_parent_dir(ent->d_name))
			continue;

		if (avf_path_is_dir(&adir, ent)) {
			ap<string_t> path_dir(path, ent->d_name);
			path_dir += PATH_SEP;
			//printf("-- remove %s\n", path_dir->string());
			avf_remove_dir_recursive(path_dir->string(), false);
		} else {
			ap<string_t> path_dir(path, ent->d_name);
			//printf("remove %s\n", path_dir->string());
			::remove(path_dir->string());
			(*num_files)++;
		}
	}

	avf_close_dir(&adir);

	if (!bKeepPath) {
		::remove(path);
	}

	return true;
}

extern "C" bool avf_remove_dir_recursive(const char *path, bool bPrint)
{
	avf_size_t num_files = 0;
	bool result = avf_rm_dir_recursive(path, &num_files, false);
	if (bPrint) {
		AVF_LOGD(C_YELLOW "rmdir %s : %d files" C_NONE, path, num_files);
	}
	return result;
}

extern "C" bool avf_remove_file(const char *filename, bool bShowError)
{
	if (::remove(filename)) {
		if (bShowError) {
			AVF_LOGP("cannot remove %s", filename);
		}
		return false;
	}
	return true;
}

extern "C" bool avf_symlink(const char *target, const char *path)
{
#ifndef WIN32_OS
	if (::symlink(target, path)) {
		AVF_LOGP("symlink failed: %s -> %s", target, path);
		return false;
	}
	return true;
#else
	AVF_LOGE("symlink not exist on win32");
	return false;
#endif
}

extern "C" bool avf_file_exists(const char *name)
{
	return ::access(name, F_OK) >= 0;
}

extern "C" bool avf_touch_file(const char *name)
{
	int fd = avf_open_file(name, O_RDWR|O_CREAT|O_TRUNC, 0);
	if (fd < 0)
		return false;
	avf_close_file(fd);
	return true;
}

extern "C" bool avf_set_file_size(int fd, avf_u64_t size)
{
#ifdef ANDROID_OS
	AVF_LOGW("ftruncate not implemented on Android");
	return false;
#elif defined(MAC_OS) || defined(IOS_OS)
	if (::ftruncate(fd, size) < 0) {
		AVF_LOGP("truncate");
		return false;
	}
#else
	if (::ftruncate64(fd, size) < 0) {
		AVF_LOGP("truncate");
		return false;
	}
#endif
	return true;
}

extern "C" avf_s64_t avf_seek(int fd, avf_u64_t offset, int whence)
{
#if defined(MAC_OS) || defined(IOS_OS)
	return ::lseek(fd, offset, whence);
#else
	return ::lseek64(fd, offset, whence);
#endif
}

extern "C" bool avf_rename_file(const char *filename, const char *new_filename)
{
	if (::rename(filename, new_filename) < 0) {
		AVF_LOGE("rename %s -> %s", filename, new_filename);
		AVF_LOGP("rename");
		return false;
	}
	return true;
}

extern "C" bool avf_mkdir_for_file(const char *filename)
{
	const char *ptr = filename;
	for (;;) {
		const char *pend = ::strchr(ptr, '/');
		if (pend == NULL)
			break;

		if (pend != ptr) {
			string_t *path = string_t::CreateFrom(filename, pend - filename);

			if (!avf_safe_mkdir(path->string())) {
				path->Release();
				return false;
			}

			path->Release();
		}

		ptr = pend + 1;
	}

	return true;
}

extern "C" avf_u64_t avf_get_file_size(const char *name, bool bShowError)
{
#if defined(MAC_OS) || defined(IOS_OS)
	struct stat buf;
	if (::stat(name, &buf) < 0) {
		if (bShowError) {
			AVF_LOGE("%s", name);
			AVF_LOGP("get_file_size");
		}
		return 0;
	}
#else
    struct stat64 buf;
    if (::stat64(name, &buf) < 0) {
        if (bShowError) {
            AVF_LOGE("%s", name);
            AVF_LOGP("get_file_size");
        }
        return 0;
    }
#endif
	return buf.st_size;
}

extern "C" avf_u64_t avf_get_filesize(int fd)
{
#if defined(MAC_OS) || defined(IOS_OS)
	struct stat stat;
	if (::fstat(fd, &stat) < 0) {
		AVF_LOGE("fstat failed");
		return 0;
	}
#else
    struct stat64 stat;
    if (::fstat64(fd, &stat) < 0) {
        AVF_LOGE("fstat failed");
        return 0;
    }
#endif
	return stat.st_size;
}

extern "C" const char *avf_get_file_ext(const char *filename)
{
	int len = (int)::strlen(filename);
	const char *ptr = filename + len;

	while (ptr > filename) {
		int c = *ptr;
		if (c == '.')
			return ptr + 1;
		if (c == '\\' || c == '/')
			break;
		ptr--;
	}

	return NULL;
}

extern "C" avf_u32_t avf_round_to_power_of_2(avf_u32_t size)
{
	avf_u32_t result = 1;
	while (1) {
		if (result >= size)
			return result;
		result <<= 1;
	}
}

extern "C" avf_hash_val_t avf_calc_hash(const avf_u8_t *value)
{
	avf_hash_val_t hash_val = HASH_INIT_VAL;
	avf_hash_val_t c;
	for (; (c = *value) != 0; value++) {
		HASH_NEXT_VAL(hash_val, c);
	}
	return hash_val;
}

extern "C" void avf_msleep(int ms)
{
	::usleep(ms * 1000);
}

extern "C" const char *avf_get_framerate(int framerate)
{
	switch (framerate) {
	case FrameRate_12_5: return "12.25";
	case FrameRate_6_25: return "6.25";
	case FrameRate_23_976: return "23.976";
	case FrameRate_24: return "24";
	case FrameRate_25: return "25";
	case FrameRate_29_97: return "29.97";
	case FrameRate_30: return "30";
	case FrameRate_50: return "50";
	case FrameRate_59_94: return "59.94";
	case FrameRate_60: return "60";
	case FrameRate_120: return "120";
	case FrameRate_240: return "240";
	case FrameRate_20: return "20";
	case FrameRate_15: return "15";
	case FrameRate_14_985: return "14.985";
	default: return "unknown";
	}
}

// b57fedfd-fd36-4993-ae4f-0e4617ce7d41
extern "C" void avf_gen_uuid(avf_u8_t uuid[UUID_LEN+1])
{
#ifdef WIN32_OS
	GUID guid;
	CoCreateGuid(&guid);
	sprintf((char*)uuid, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		(avf_u32_t)guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], 
		guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], 
		guid.Data4[6], guid.Data4[7]);
#else
	const char *filename = "/proc/sys/kernel/random/uuid";
	int fd = avf_open_file(filename, O_RDONLY, 0);
	if (fd < 0) {
		AVF_LOGE("cannot open %s", filename);
		::memset(uuid, 0, UUID_LEN+1);
		return;
	}
	int nread = ::read(fd, uuid, UUID_LEN);
	if (nread != UUID_LEN) {
		AVF_LOGE("read uuid failed, nread=%d", nread);
		::memset(uuid, 0, UUID_LEN+1);
	}
	uuid[UUID_LEN] = 0;
	//AVF_LOGD("uuid: %s", uuid);
	avf_close_file(fd);
#endif
}

#ifdef DEBUG_AVSYNC
extern "C" void tick_info_init(tick_info_s *info)
{
	info->last_ticks = info->start_ticks = avf_get_curr_tick();
	info->min_ticks = -1U;
	info->max_ticks = 0;
	info->count = 0;
}

extern "C" int __tick_info_update(tick_info_s *info, int max_count, const char *tag)
{
	avf_u64_t curr_ticks = avf_get_curr_tick();
	avf_u32_t ticks = (avf_u32_t)(curr_ticks - info->last_ticks);
	info->last_ticks = curr_ticks;

	if (info->min_ticks > ticks)
		info->min_ticks = ticks;

	if (info->max_ticks < ticks)
		info->max_ticks = ticks;

	if (++info->count == max_count) {
		int avg100 = (int)(curr_ticks - info->start_ticks);
		avg100 = avg100 * 100 / max_count;
		AVF_LOGI(C_YELLOW "%s  " C_CYAN "min %d max %d avg %d.%02d" C_NONE, tag,
			info->min_ticks, info->max_ticks, avg100/100, avg100%100);

		info->last_ticks = info->start_ticks = curr_ticks;
		info->min_ticks = -1U;
		info->max_ticks = 0;
		info->count = 0;

		return 1;
	}

	return 0;
}
#endif

