
#ifndef __AVF_COMMON_H__
#define __AVF_COMMON_H__

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "avf_error_code.h"
#include "avf_configs.h"
#include "avf_std_media.h"

#ifndef UINT64_C
#define UINT64_C(value) __CONCAT(value, ULL)
#endif

//-----------------------------------
// supported OS types:
//-----------------------------------

//	LINUX_OS
//	WIN32_OS
//	MAC_OS
//	ANDROID_OS
//	IOS_OS
//	LINUX_SERVER

//-----------------------------------------------------------------------
//
//
//-----------------------------------------------------------------------

#define AVF_VERSION	20140711

#define AVF_DIR_MODE	0777
#define AVF_FILE_MODE	0644

//-----------------------------------------------------------------------
//
//	common defines
//
//-----------------------------------------------------------------------

#define KB		(1024)
#define MB		(1024*1024)

#define INLINE			inline
#define LIKELY(_x)		(_x)
#define UNLIKELY(_x)		(_x)

// _align must be power of 2
#ifndef ROUND_UP
#define ROUND_UP(_size, _align) \
	(((_size) + (_align - 1)) & ~((_align) - 1))
#endif

// _align must be power of 2
#ifndef ROUND_DOWN
#define ROUND_DOWN(_size, _align) \
	((_size) & ~((_align) - 1))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_array) \
	(sizeof(_array) / sizeof((_array)[0]))
#endif

#ifndef MIN
#define MIN(_a, _b) \
	((_a) < (_b) ? (_a) : (_b))
#endif

#ifndef MAX
#define MAX(_a, _b) \
	((_a) > (_b) ? (_a) : (_b))
#endif

#ifndef OFFSET
#define OFFSET(type, member) \
	(unsigned int)(unsigned long long)(&((type *)0)->member)
#endif

#ifndef container_of
#define container_of(_ptr, _type, _member) \
	((_type*)((const unsigned char *)(_ptr) - OFFSET(_type, _member)))
#endif

#define with_size(_array) \
	(_array), ARRAY_SIZE(_array)

#define MASK(_nbits) \
	((1 << (_nbits)) - 1)

#define EOL	"\n"

#ifdef WIN32_OS
#define PATH_SEP	"\\"
#else
#define PATH_SEP	"/"
#endif

#define STR_WITH_SIZE(_str) \
	(_str), (sizeof(_str)-1)

#define STR_SIZE(_str) \
	(sizeof(_str)-1)

//-----------------------------------------------------------------------
//
//	types
//
//-----------------------------------------------------------------------

typedef int avf_int_t;
typedef unsigned int avf_uint_t;

typedef uint8_t avf_u8_t;
typedef uint16_t avf_u16_t;
typedef uint32_t avf_u32_t;

typedef int8_t avf_s8_t;
typedef int16_t avf_s16_t;
typedef int32_t avf_s32_t;

typedef uint64_t avf_u64_t;
typedef int64_t avf_s64_t;

typedef unsigned int avf_size_t;

typedef unsigned long long int avf_pts_t;
typedef volatile int avf_atomic_t;

#ifdef OS_64BIT
typedef long long int avf_intptr_t;
#else
typedef long int avf_intptr_t;
#endif

typedef unsigned char avf_sbool_t;	// short bool
typedef unsigned int avf_enum_t;

typedef unsigned int avf_hash_val_t;

// use standard C++ bool type

typedef struct name_value_s {
	const char *name;
	const char *value;
} name_value_t;

//-----------------------------------------------------------------------
//
//	time definition
//		time cannot be less than 2012/Jan/1 00:00:00
//
//-----------------------------------------------------------------------

// bits 0-5   : second (6 bits)
// bits 6-11  : minute (6 bits)
// bits 12-16 : hour (5 bits)
// bits 17-21 : day (5 bits)
// bits 22-25 : month (4 bits)
// bits 26-31 : year (6 bits)

#define SS_SHIFT		0
#define MM_SHIFT		6
#define HH_SHIFT		12
#define DAY_SHIFT		17
#define MONTH_SHIFT		22
#define YEAR_SHIFT		26

typedef uint32_t avf_time_t;
#define AVF_INVALID_TIME	((avf_time_t)(-1))
#define AVF_MIN_TIME		(1325376000)	// 2012/1/1 00:00:00, local time
#define AVF_MIN_YEAR		(2010)

#define GET_YEAR(_time)		((avf_uint_t)(((_time) >> YEAR_SHIFT) & MASK(6)))
#define GET_MONTH(_time)		((avf_uint_t)(((_time) >> MONTH_SHIFT) & MASK(4)))
#define GET_DAY(_time)		((avf_uint_t)(((_time) >> DAY_SHIFT) & MASK(5)))
#define GET_HH(_time)		((avf_uint_t)(((_time) >> HH_SHIFT) & MASK(5)))
#define GET_MM(_time)		((avf_uint_t)(((_time) >> MM_SHIFT) & MASK(6)))
#define GET_SS(_time)		((avf_uint_t)(((_time) >> SS_SHIFT) & MASK(6)))

#define SPLIT_TIME(_time) \
	GET_YEAR(_time) + AVF_MIN_YEAR, GET_MONTH(_time), \
	GET_DAY(_time), GET_HH(_time), GET_MM(_time), GET_SS(_time)

typedef time_t sys_time_t;

//-----------------------------------------------------------------------
//
//	fourcc
//
//-----------------------------------------------------------------------

typedef avf_u32_t avf_fcc_t;

#define MKFCC(_a, _b, _c, _d) \
	(((_a) << 24) | ((_b) << 16) | ((_c) << 8) | (_d))

#define MKFCC_LE(_a, _b, _c, _d) \
	(((_d) << 24) | ((_c) << 16) | ((_b) << 8) | (_a))

#define FCC_VAL(_name) \
	(((_name)[0] << 24) | ((_name)[1] << 16) | ((_name)[2] << 8) | ((_name)[3]))

#define FCC_CHARS(_fcc) \
	((_fcc)>>24), (((_fcc)>>16)&0xFF), (((_fcc)>>8)&0xFF), ((_fcc)&0xFF)

//-----------------------------------------------------------------------
//
//	guid
//
//-----------------------------------------------------------------------

struct avf_guid_t {
	avf_u32_t x;
	avf_u16_t s1;
	avf_u16_t s2;
	avf_u8_t c[8];
};

typedef const avf_guid_t avf_iid_t;
typedef const avf_guid_t avf_clsid_t;

typedef const avf_guid_t& avf_refguid_t;
typedef const avf_iid_t& avf_refiid_t;
typedef const avf_clsid_t& avf_refclsid_t;

INLINE bool operator==(avf_refguid_t guid1, avf_refguid_t guid2)
{
	const avf_u32_t *p1 = (avf_u32_t*)&guid1;
	const avf_u32_t *p2 = (avf_u32_t*)&guid2;
	return (p1[0] == p2[0] && p1[1] == p2[1] && p1[2] == p2[2] && p1[3] == p2[3]);
}

INLINE bool operator!=(avf_refguid_t guid1, avf_refguid_t guid2)
{
	return !(guid1 == guid2);
}

#define AVF_DEFINE_IID(name, x, s1, s2, c0, c1, c2, c3, c4, c5, c6, c7) \
	extern avf_iid_t name = {x, s1, s2, {c0, c1, c2, c3, c4, c5, c6, c7}}

#define AVF_DEFINE_GUID(name, x, s1, s2, c0, c1, c2, c3, c4, c5, c6, c7) \
	extern const avf_guid_t name = {x, s1, s2, {c0, c1, c2, c3, c4, c5, c6, c7}}

//-----------------------------------------------------------------------
//
//	
//
//-----------------------------------------------------------------------

#define MK_U64(_lo, _hi) \
	(((avf_u64_t)(_hi) << 32) | (_lo))

#define U64_LO(_u64) \
	((avf_u32_t)(_u64))

#define U64_HI(_u64) \
	((avf_u32_t)((_u64) >> 32))

#define U32_LO(_u32) \
	((_u32) & 0xFFFF)

#define U32_HI(_u32) \
	(((_u32)>>16) & 0xFFFF)

#define MK_U32(_lo, _hi) \
	(((avf_u32_t)(_hi) << 16) | (_lo))

//-----------------------------------------------------------------------
//
//	registry defines
//
//-----------------------------------------------------------------------

// registry
#define REG_WRITE		0x40000000
#define REG_STR_MAX		256

//-----------------------------------------------------------------------
//
//	log & assertion
//
//-----------------------------------------------------------------------

#define AVF_LOG_E		(1 << 0)	// error
#define AVF_LOG_W		(1 << 1)	// warning
#define AVF_LOG_I		(1 << 2)	// info
#define AVF_LOG_V		(1 << 3)	// verbose
#define AVF_LOG_D		(1 << 4)	// debug
#define AVF_LOG_P		(1 << 5)	// perror

#define ALL_LOGS		(AVF_LOG_E|AVF_LOG_W|AVF_LOG_I|AVF_LOG_D|AVF_LOG_P)
#define BASE_LOGS		(AVF_LOG_E|AVF_LOG_W|AVF_LOG_P|AVF_LOG_I)
#define ERROR_LOGS		(AVF_LOG_E)
#define NO_LOGS			(0)

#if defined(ANDROID_OS) || defined(WIN32_OS)

#define C_NONE

#define C_BLACK	
#define C_WHITE	

#define C_GRAY
#define C_LIGHT_GRAY

#define C_RED
#define C_LIGHT_RED

#define C_GREEN
#define C_LIGHT_GREEN

#define C_BLUE
#define C_LIGHT_BLUE

#define C_CYAN
#define C_LIGHT_CYAN

#define C_PURPLE
#define C_LIGHT_PURPLE

#define C_BROWN
#define C_YELLOW

#else

#define C_NONE			"\033[0m"

#define C_BLACK			"\033[0;30m"
#define C_WHITE			"\033[1;37m"

#define C_GRAY			"\033[1;30m"
#define C_LIGHT_GRAY	"\033[0;37m"

#define C_RED			"\033[0;31m"
#define C_LIGHT_RED		"\033[1;31m"

#define C_GREEN			"\033[0;32m"
#define C_LIGHT_GREEN	"\033[1;32m"

#define C_BLUE			"\033[0;34m"
#define C_LIGHT_BLUE	"\033[1;34m"

#define C_CYAN			"\033[0;36m"
#define C_LIGHT_CYAN	"\033[1;36m"

#define C_PURPLE		"\033[0;35m"
#define C_LIGHT_PURPLE	"\033[1;35m"

#define C_BROWN			"\033[0;33m"
#define C_YELLOW		"\033[1;33m"

#endif

extern avf_u32_t g_avf_log_flag;

#define AVF_LOG_ALL	"ewidp"		// "ewidpv"

void avf_set_logs(const char *logs);

#ifndef LOG_TAG
#define LOG_TAG	""
#endif

#ifdef MINGW
#define LLD	"%I64d"
#else
#define LLD	"%" PRIi64
#endif

#ifdef WIN32_OS
#define FMT_ZD	"%d"
#else
#define FMT_ZD	"%zd"
#endif

#ifdef ANDROID_OS

#include <android/log.h>

// =================================================
//	Android
// =================================================

#define AVF_LOG(fmt, _logf, _alog, args...) \
	do { \
		if (test_flag(g_avf_log_flag, _logf)) { \
			__android_log_print(_alog, LOG_TAG, fmt, ##args); \
		} \
	} while (0)

#define AVF_LOGE(fmt, args...)	AVF_LOG(fmt, AVF_LOG_E, ANDROID_LOG_ERROR, ##args)
#define AVF_LOGW(fmt, args...)	AVF_LOG(fmt, AVF_LOG_W, ANDROID_LOG_WARN, ##args)
#define AVF_LOGI(fmt, args...)	AVF_LOG(fmt, AVF_LOG_I, ANDROID_LOG_INFO, ##args)
#define AVF_LOGV(fmt, args...)	AVF_LOG(fmt, AVF_LOG_V, ANDROID_LOG_VERBOSE, ##args)
#define AVF_LOGD(fmt, args...)	AVF_LOG(fmt, AVF_LOG_D, ANDROID_LOG_DEBUG, ##args)
#define AVF_LOGDW(fmt, args...)	AVF_LOG(fmt, AVF_LOG_D, ANDROID_LOG_DEBUG, ##args)
#define AVF_LOGP(fmt, args...)	AVF_LOG(fmt " (line %d): %s", AVF_LOG_P, ANDROID_LOG_ERROR, ##args, __LINE__, strerror(errno))

#elif defined(WIN32_OS) || defined(MAC_OS)

// =================================================
//	Windows/MAC
// =================================================

extern "C" void (*avf_print_proc)(const char *);

#define AVF_LOG(fmt, _logf, _logs, args...) \
	do { \
		if (test_flag(g_avf_log_flag, _logf)) { \
			char _b_[256]; \
			sprintf(_b_, _logs LOG_TAG " - " fmt, ##args); \
			(avf_print_proc)(_b_); \
		} \
	} while (0)

#define AVF_LOGE(fmt, args...)	AVF_LOG(fmt " (line %d)", AVF_LOG_W, "E/", ##args, __LINE__)
#define AVF_LOGW(fmt, args...)	AVF_LOG(fmt, AVF_LOG_W, "W/", ##args)
#define AVF_LOGI(fmt, args...)	AVF_LOG(fmt, AVF_LOG_I, "I/", ##args)
#define AVF_LOGV(fmt, args...)	AVF_LOG(fmt, AVF_LOG_V, "V/", ##args)
#define AVF_LOGD(fmt, args...)	AVF_LOG(fmt, AVF_LOG_D, "D/", ##args)
#define AVF_LOGDW(fmt, args...)	AVF_LOG(fmt, AVF_LOG_D, "D/", ##args)
#define AVF_LOGP(fmt, args...)	AVF_LOG(fmt " (line %d): %s", AVF_LOG_P, "P/", ##args, __LINE__, strerror(errno))

#else

// =================================================
//	Others
// =================================================

extern int (*avf_printf)(const char *fmt, ...);

#define AVF_LOG(fmt, _logf, _logs, _cbegin, _cend, args...) \
	do { \
		if (test_flag(g_avf_log_flag, _logf)) { \
			avf_printf(_cbegin _logs LOG_TAG " - " fmt "\n" _cend, ##args); \
		} \
	} while (0)

#define AVF_LOGE(fmt, args...)	AVF_LOG(fmt " (line %d)", AVF_LOG_W, "E/", C_RED, C_NONE, ##args, __LINE__)
#define AVF_LOGW(fmt, args...)	AVF_LOG(fmt, AVF_LOG_W, "W/", C_PURPLE, C_NONE, ##args)
#define AVF_LOGI(fmt, args...)	AVF_LOG(fmt, AVF_LOG_I, "I/", , , ##args)
#define AVF_LOGV(fmt, args...)	AVF_LOG(fmt, AVF_LOG_V, "V/", , , ##args)
#define AVF_LOGD(fmt, args...)	AVF_LOG(fmt, AVF_LOG_D, "D/", , , ##args)
#define AVF_LOGDW(fmt, args...)	AVF_LOG(fmt, AVF_LOG_D, "D/", C_PURPLE, C_NONE, ##args)
#define AVF_LOGP(fmt, args...)	AVF_LOG(fmt " (line %d): %s", AVF_LOG_P, "P/", C_RED, C_NONE, ##args, __LINE__, strerror(errno))

#endif



#define AVF_UNUSED(_expr) \
	do { \
		if (0) { (void)(_expr); } \
	} while (0)

extern void avf_die(const char *file, int line);

#ifdef AVF_DEBUG
#define AVF_DIE \
	avf_die(__FILE__, __LINE__)
#else
#define AVF_DIE \
	avf_die(NULL, __LINE__)
#endif

#ifdef AVF_DEBUG
#define AVF_ASSERT(_expr) \
	do { \
		if (UNLIKELY(!(_expr))) { \
			AVF_DIE; \
		} \
	} while (0)
#else
#define AVF_ASSERT(_expr) \
	AVF_UNUSED(_expr);
#endif

#define AVF_ASSERT_OK(_status) \
	AVF_ASSERT((_status) == E_OK)

//-----------------------------------------------------------------------
//
//	common utils
//
//-----------------------------------------------------------------------

#define CHECK_STATUS(_object) \
	do { \
		if ((_object) && (_object)->Status() != E_OK) { \
			avf_delete (_object); \
			(_object) = NULL; \
		} \
	} while (0)

#define CREATE_OBJ(_expr) \
	do { \
		if (!(_expr)) { \
			mStatus = E_NOMEM; \
			return; \
		} \
	} while (0)

#define CHECK_OBJ(_obj) \
	do { \
		if (!(_obj)) { \
			mStatus = E_NOMEM; \
			return; \
		} \
	} while (0)

#define set_flag(_flags, _f) \
	do { (_flags) |= (_f); } while (0)

#define clear_flag(_flags, _f) \
	do { (_flags) &= ~(_f); } while (0)

#define test_flag(_flags, _f) \
	((_flags) & (_f))

#define set_or_clear_flag(_cond, _flags, _f) \
	do { \
		if (_cond) set_flag(_flags, _f); \
		else clear_flag(_flags, _f); \
	} while (0)

#define __avf_safe_release(_obj) \
	do { \
		if (_obj) { \
			(_obj)->Release(); \
		} \
	} while (0)

#define avf_safe_release(_obj) \
	do { \
		if (_obj) { \
			(_obj)->Release(); \
			_obj = NULL; \
		} \
	} while (0)

#define avf_safe_close(_fd) \
	do { \
		if (_fd >= 0) { \
			avf_close_file(_fd); \
			_fd = -1; \
		} \
	} while (0)

#define avf_assign(_to, _from) \
	do { \
		if (_from) (_from)->AddRef(); \
		if (_to) (_to)->Release(); \
		_to = _from; \
	} while (0)

#define SET_ARRAY_NULL_N(_array, _size) \
	do { \
		for (avf_size_t _i = 0; _i < _size; _i++) \
			(_array)[_i] = NULL; \
	} while (0)

#define SET_ARRAY_NULL(_array) \
	SET_ARRAY_NULL_N(_array, ARRAY_SIZE(_array))

#define SAFE_RELEASE_ARRAY_N(_array, _size) \
	do { \
		for (avf_size_t _i = 0; _i < (_size); _i++) \
			avf_safe_release((_array)[_i]); \
	} while (0)

#define SAFE_RELEASE_ARRAY(_array) \
	SAFE_RELEASE_ARRAY_N(_array, ARRAY_SIZE(_array))

#define ADJUST_RANGE(_v, _min, _max) \
	do { \
		if ((_v) < (_min)) \
			(_v) = (_min); \
		else if ((_v) > (_max)) \
			(_v) = (_max); \
	} while (0)

//-----------------------------------------------------------------------
//
//	byte ordering
//
//-----------------------------------------------------------------------
static INLINE avf_u32_t avf_read_be_32(avf_u8_t *ptr) {
	return (avf_u32_t)(ptr[0] << 24) | 
			(avf_u32_t)(ptr[1] << 16) | 
			(avf_u32_t)(ptr[2] << 8) | 
			(avf_u32_t)ptr[3];
}

static INLINE avf_u32_t avf_read_be_16(avf_u8_t *ptr) {
	return (ptr[0] << 8) | ptr[1];
}

static INLINE void avf_write_be_32(avf_u8_t *ptr, avf_u32_t value) {
	ptr[0] = value >> 24;
	ptr[1] = value >> 16;
	ptr[2] = value >> 8;
	ptr[3] = value;
}

#define AVF_HIGH(_value) \
	(((_value) >> 16) & 0xFFFF)

#define AVF_LOW(_value) \
	((_value) & 0xFFFF)

#define AVF_MK64(_high, _low) \
	(((avf_u64_t)(_high) << 32) | (_low))

//-----------------------------------------------------------------------
//
//	utilities
//
//-----------------------------------------------------------------------
extern "C" void avf_dump_mem(const void *p, avf_size_t size);

//-----------------------------------------------------------------------
//
//	global constants
//
//-----------------------------------------------------------------------
#define AVF_POLL_DELAY		10	// ms

#include "avf_osal.h"
#include "avf_string.h"

#endif

