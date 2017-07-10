
#ifndef __AVF_NEW_H__
#define __AVF_NEW_H__

#if defined(AVF_DEBUG) && defined(LINUX_OS)
#define MEM_DEBUG
#endif

#define avf_malloc	malloc
#define avf_strdup	strdup
#define avf_free	free

#define avf_malloc_bytes(_size) \
	((avf_u8_t*)avf_malloc(_size))

#define avf_malloc_type(_type) \
	((_type*)avf_malloc(sizeof(_type)))

#define avf_malloc_type_extra(_type, _extra_size) \
	((_type*)avf_malloc(sizeof(_type) + _extra_size))

#define avf_malloc_array(_type, _count) \
	((_type*)avf_malloc((_count) * sizeof(_type)))

#define __avf_safe_free(_ptr) \
	do { \
		if (_ptr) { \
			avf_free(_ptr); \
		} \
	} while (0)

#define avf_safe_free(_ptr) \
	do { \
		if (_ptr) { \
			avf_free(_ptr); \
			_ptr = NULL; \
		} \
	} while (0)

#define avf_new	new
#define avf_new_array	new[]

#define avf_delete	delete
#define avf_delete_array	delete

#define avf_safe_delete(_obj) \
	do { \
		if (_obj) { \
			avf_delete _obj; \
			_obj = NULL; \
		} \
	} while (0)

#endif

