
#ifndef __LIBIPC_H__
#define __LIBIPC_H__

#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "ipc_drv.h"

extern int g_ipc_fd;

int ipc_init(void);
void ipc_deinit(void);

#if 0
#define check_rval(_rval) \
	do { \
		if (_rval < 0) \
			perror(__func__); \
	} while (0)
#else
#define check_rval(_rval) \
	do { _rval = _rval; } while (0)
#endif

static inline int __ipc_register_service(ipc_service_t *service)
{
	int rval = ioctl(g_ipc_fd, IPC_REGISTER_SERVICE, service);
	check_rval(rval);
	return rval;
}

static inline int ipc_register_service(const char *name, ipc_cid_t *cid)
{
	ipc_service_t service;
	::memset(&service, 0, sizeof(service));
	strcpy(service.name, name);
	if (__ipc_register_service(&service) < 0)
		return -1;
	*cid = service.service_cid;
	return 0;
}

static inline int __ipc_query_service(ipc_service_t *service)
{
	int rval = ioctl(g_ipc_fd, IPC_QUERY_SERVICE, service);
	check_rval(rval);
	return rval;
}

static inline int ipc_query_service(const char *name, ipc_cid_t *cid)
{
	ipc_service_t service;
	strcpy(service.name, name);
	if (__ipc_query_service(&service) < 0)
		return -1;
	*cid = service.service_cid;
	return 0;
}

static inline int ipc_setup_channel(ipc_write_read_t *iwr)
{
	int rval;
	iwr->write_val = 0;
	iwr->write_size = 0;
	rval = ioctl(g_ipc_fd, IPC_SETUP_CHANNEL, iwr);
	check_rval(rval);
	return rval;
}

static inline int ipc_close_channel(ipc_cid_t cid)
{
	int rval = ioctl(g_ipc_fd, IPC_CLOSE_CHANNEL, cid);
	check_rval(rval);
	return rval;
}

static inline int ipc_send_cmd(ipc_write_read_t *iwr)
{
	while (1) {
		int rval = ioctl(g_ipc_fd, IPC_SEND_CMD, iwr);
		if (rval < 0 && errno == EINTR)
			continue;
		return rval;
	}
}

static inline int ipc_write_read(ipc_write_read_t *iwr)
{
	while (1) {
		int rval = ioctl(g_ipc_fd, IPC_WRITE_READ, iwr);
		if (rval < 0 && errno == EINTR)
			continue;
		return rval;
	}
}

static inline int ipc_read_notification(ipc_cid_t cid, unsigned long *values)
{
	ipc_notify_t notify;
	int rval;
	notify.cid = cid;
	notify.values = 0;
	while (1) {
		rval = ioctl(g_ipc_fd, IPC_READ_NOTIFICATION, &notify);
		if (rval < 0 && errno == EINTR)
			continue;
		*values = notify.values;
		return rval;
	}
}

static inline int ipc_write_notification(ipc_cid_t cid, unsigned long values)
{
	ipc_notify_t notify;
	int rval;
	notify.cid = cid;
	notify.values = values;
	rval = ioctl(g_ipc_fd, IPC_WRITE_NOTIFICATION, &notify);
	check_rval(rval);
	return rval;
}

void ipc_no_buffer(void);
void ipc_dump_data(void *buffer, int size);

// init iwr for IPC_SEND_CMD
static inline void ipc_init_cmd_iwr(ipc_write_read_t *iwr, ipc_cid_t cid, 
	int cmd, void *buffer, size_t size, void *read_ptr, size_t read_size)
{
	iwr->input_cid[0] = cid;
	iwr->output_cid[0] = 0;
	iwr->flags = 0;
	iwr->write_val = cmd;
	iwr->write_ptr = buffer;
	iwr->write_size = size;
	iwr->read_ptr = read_ptr;
	iwr->read_size = read_size;
}

// init iwr for IPC_WRITE_READ
static inline void ipc_init_service_iwr(ipc_write_read_t *iwr, ipc_cid_t service_cid,
	char *write_buffer, char *read_buffer, size_t read_buffer_size)
{
	iwr->input_cid[0] = service_cid;
	iwr->output_cid[0] = 0;
	iwr->rid[0] = 0;
	iwr->error = 0;

	iwr->flags = IPC_FLAG_READ;

	iwr->write_val = 0;
	iwr->write_ptr = write_buffer;
	iwr->write_size = 0;

	iwr->read_val = 0;
	iwr->read_ptr = read_buffer;
	iwr->read_size = read_buffer_size;
	iwr->bytes_read = 0;
}


#define __ipc_check_size(_size, _size_needed) \
	do { \
		if ((int)_size < (int)_size_needed) \
			ipc_no_buffer(); \
	} while (0)

#define __ipc_inc_pos(_size, _ptr, _inc) \
	do { \
		int __inc = ((_inc) + 3) & ~3; \
		_size -= __inc; \
		_ptr = (void*)((char*)(_ptr) + __inc); \
	} while (0)

#define __ipc_write_int(_size, _ptr, _val) \
	do { \
		__ipc_check_size(_size, (int)sizeof(int)); \
		*(int*)(_ptr) = (_val); \
		__ipc_inc_pos(_size, _ptr, (int)sizeof(int)); \
	} while (0)

#define __ipc_write_int32(_size, _ptr, _val) \
	do { \
		__ipc_check_size(_size, (int)sizeof(int32_t)); \
		*(int*)(_ptr) = (_val); \
		__ipc_inc_pos(_size, _ptr, (int)sizeof(int32_t)); \
	} while (0)

#define __ipc_write_int64(_size, _ptr, _val) \
	do { \
		__ipc_check_size(_size, (int)sizeof(int64_t)); \
		*(int*)(_ptr) = (_val) >> 32; \
		*(int*)((char*)(_ptr) + 4) = (_val); \
		__ipc_inc_pos(_size, _ptr, (int)sizeof(int64_t)); \
	} while (0)

#define __ipc_write_data(_size, _ptr, _pdata, _datasize) \
	do { \
		__ipc_check_size(_size, _datasize); \
		memcpy(_ptr, _pdata, _datasize); \
		__ipc_inc_pos(_size, _ptr, _datasize); \
	} while (0)

// size+string+'\0'
#define __ipc_write_str(_size, _ptr, _str) \
	do { \
		int len = strlen(_str) + 1; \
		__ipc_write_int(_size, _ptr, len); \
		__ipc_write_data(_size, _ptr, _str, len); \
	} while (0)

#define __init_write_buffer(_wbuf, _wbuf_size) \
	char *__ipc_wbuf = _wbuf; \
	size_t __ipc_wsize = _wbuf_size; \
	void *__ipc_wptr = __ipc_wbuf

#define define_write_buffer(_size) \
	char __ipc_wbuf[_size]; \
	size_t __ipc_wsize = _size; \
	void *__ipc_wptr = __ipc_wbuf

#define __ipc_wbuf_size \
	((char*)__ipc_wptr - (char*)__ipc_wbuf)

#define __ipc_wbuf_and_size \
	__ipc_wbuf, __ipc_wbuf_size

#define ipc_inc_pos(_n) \
	__ipc_inc_pos(__ipc_wsize, __ipc_wptr, _n);

#define ipc_write_int(_val) \
	__ipc_write_int(__ipc_wsize, __ipc_wptr, _val)

#define ipc_write_int32(_val) \
	__ipc_write_int32(__ipc_wsize, __ipc_wptr, _val)

#define ipc_write_int64(_val) \
	__ipc_write_int64(__ipc_wsize, __ipc_wptr, _val)

#define ipc_write_data(_pdata, _datasize) \
	__ipc_write_data(__ipc_wsize, __ipc_wptr, _pdata, _datasize)

#define ipc_write_str(_str) \
	do { \
		const char *_ipc_str = (_str); \
		__ipc_write_str(__ipc_wsize, __ipc_wptr, _ipc_str); \
	} while (0)

void *__ipc_write_strlist(size_t *size, void *ptr, int count, const char **strlist);

#define ipc_write_strlist(_count, _strlist) \
	__ipc_wptr = __ipc_write_strlist(&__ipc_wsize, __ipc_wptr, _count, (const char **)_strlist)

#define ipc_dump_wbuf() \
	ipc_dump_data((void*)__ipc_wbuf, sizeof(__ipc_wbuf))

struct ipc_client_s;

typedef int (*ipc_client_constructor)(struct ipc_client_s *self, void *args);
typedef void (*ipc_client_destructor)(struct ipc_client_s *self);
typedef void (*ipc_client_notify_proc)(struct ipc_client_s *self, int index);
typedef void (*ipc_client_no_disconnected_proc)(struct ipc_client_s *self, int error);

typedef struct ipc_client_s {
	ipc_cid_t cid;
	pthread_t tid_notify;
	char disconnected;
	ipc_client_destructor dtor;
	ipc_client_notify_proc notify_proc;
	ipc_client_no_disconnected_proc on_disconnected;
} ipc_client_t;

ipc_client_t *__ipc_new_client_ex(ipc_cid_t cid, size_t object_size, 
	ipc_client_constructor ctor, void *ctor_args, ipc_client_destructor dtor,
	ipc_client_notify_proc notify_proc, ipc_client_no_disconnected_proc on_disconnected);

static inline ipc_client_t *__ipc_new_client(ipc_cid_t cid, size_t object_size, 
	ipc_client_constructor ctor, void *ctor_args, ipc_client_destructor dtor,
	ipc_client_notify_proc notify_proc)
{
	return __ipc_new_client_ex(cid, object_size, ctor, ctor_args, dtor, notify_proc, NULL);
}

ipc_client_t *__ipc_create_client(size_t object_size, const char *service_name, 
	int cmd, void *cmd_buffer, size_t buffer_size, 
	ipc_client_constructor ctor, void *ctor_args,
	ipc_client_destructor dtor, ipc_client_notify_proc notify_proc,
	ipc_client_no_disconnected_proc on_disconnected);

#define ipc_create_client(_object_type, _service_name, _cmd, _ctor, _ctor_args, _dtor, _notify_proc) \
	(ipc_libvlc_instance_t*)__ipc_create_client(sizeof(_object_type), \
	_service_name, _cmd, __ipc_wbuf_and_size, \
	_ctor, _ctor_args, _dtor, _notify_proc)

#define ipc_create_client_np(_object_type, _service_name, _cmd, _ctor, _dtor, _notify_proc) \
	__ipc_create_client(sizeof(_object_type), _service_name, _cmd, NULL, 0, _ctor, _dtor, _notify_proc)

void __ipc_destroy_client(ipc_client_t *client);

#define ipc_destroy_client(_instance) \
	__ipc_destroy_client(&(_instance)->super)

int __ipc_call_remote(ipc_client_t *client, int cmd, void *cmd_buffer, size_t buffer_size,
	void *read_buffer, size_t read_size);

int __ipc_call_remote_read(ipc_client_t *client, int cmd, void *cmd_buffer, size_t buffer_size,
	void *read_buffer, size_t read_size, int *bytes_read);

#define ipc_call_remote(_instance, _cmd) \
	__ipc_call_remote(&(_instance)->super, _cmd, NULL, 0, NULL, 0)

#define ipc_call_remote_write(_instance, _cmd) \
	__ipc_call_remote(&(_instance)->super, _cmd, __ipc_wbuf_and_size, NULL, 0)
	
#define ipc_call_remote_read(_instance, _cmd) \
	__ipc_call_remote_read(&(_instance)->super, _cmd, NULL, 0, __ipc_rbuf_and_size, &__ipc_rsize)

#define ipc_call_remote_write_read(_instance, _cmd) \
	__ipc_call_remote_read(&(_instance)->super, _cmd, __ipc_wbuf_and_size, __ipc_rbuf_and_size, &__ipc_rsize)

struct ipc_server_s;

typedef int (*ipc_server_constructor)(struct ipc_server_s *self, void *args);
typedef void (*ipc_server_destructor)(struct ipc_server_s *self);
typedef int (*ipc_server_proc)(struct ipc_server_s *self, ipc_write_read_t *iwr);

typedef struct ipc_server_s {
	ipc_cid_t cid;
	ipc_server_destructor dtor;
	ipc_server_proc server_proc;
	pthread_t tid_server;
	int error;
	char *write_buffer;
	char *read_buffer;
	size_t write_buffer_size;
	size_t read_buffer_size;
} ipc_server_t;

ipc_server_t *__ipc_create_server(size_t object_size, size_t max_read_size, size_t max_write_size,
	ipc_server_constructor ctor, void *ctor_args, ipc_server_destructor dtor, ipc_server_proc server_proc);

#define ipc_create_server(_object_type, _max_read_size, _max_write_size, _ctor, _ctor_args, _dtor, _server_proc) \
	((_object_type*)__ipc_create_server(sizeof(_object_type), _max_read_size, _max_write_size, \
		_ctor, _ctor_args, _dtor, _server_proc))

int __ipc_destroy_server(ipc_server_t *server);

#define ipc_destroy_server(_server) \
	__ipc_destroy_server(&(_server)->super)


int __ipc_run_server(ipc_server_t *server, ipc_write_read_t *iwr);

// run the server. if failed, server will be destroyed
static inline int ipc_run_server(ipc_server_t *server, ipc_write_read_t *iwr)
{
	int rval = __ipc_run_server(server, iwr);
	if (rval < 0)
		__ipc_destroy_server(server);
	return rval;
}

#define __ipc_read_int(_status, _ptr, _size, _val) \
	do { \
		if (_status == 0) { \
			if ((int)(_size) < (int)sizeof(int)) { \
				_status = -1; \
			} else { \
				_val = *(int*)(_ptr); \
				__ipc_inc_pos(_size, _ptr, (int)sizeof(int)); \
			} \
		} \
	} while (0)

#define __ipc_read_int32(_status, _ptr, _size, _val) \
	do { \
		if (_status == 0) { \
			if ((int)(_size) < (int)sizeof(int32_t)) { \
				_status = -1; \
			} else { \
				_val = *(int32_t*)(_ptr); \
				__ipc_inc_pos(_size, _ptr, (int)sizeof(int32_t)); \
			} \
		} \
	} while (0)

#define __ipc_read_int64(_status, _ptr, _size, _val) \
	do { \
		if (_status == 0) { \
			if ((int)(_size) < (int)sizeof(int64_t)) { \
				_status = -1; \
			} else { \
				_val = ((int64_t)(*(uint32_t*)(_ptr)) << 32) | (*(uint32_t*)((char*)(_ptr) + 4)); \
				__ipc_inc_pos(_size, _ptr, (int)sizeof(int64_t)); \
			} \
		} \
	} while (0)

#define __ipc_read_data(_status, _ptr, _size, _pdata, _datasize) \
	do { \
		if (_status == 0) { \
			if ((int)(_size) < (int)(_datasize)) { \
				_status = -1; \
			} else { \
				_pdata = (char*)(_ptr); \
				__ipc_inc_pos(_size, _ptr, _datasize); \
			} \
		} \
	} while (0)

#define __ipc_copy_data(_status, _ptr, _size, _pdata, _datasize) \
	do { \
		if (_status == 0) { \
			if ((int)(_size) < (int)(_datasize)) { \
				_status = -1; \
			} else { \
				memcpy((void*)_pdata, _ptr, _datasize); \
				__ipc_inc_pos(_size, _ptr, _datasize); \
			} \
		} \
	} while (0)

#define __ipc_read_str(_status, _ptr, _size, _str) \
	do { \
		int len = 0; \
		__ipc_read_int(_status, _ptr, _size, len); \
		_str = NULL; \
		if (len > 0) { \
			__ipc_read_data(_status, _ptr, _size, _str, len); \
			_str[len - 1] = 0; \
		} \
	} while (0)

#define ipc_init_read(_read_buffer, _read_size) \
	void *__ipc_rptr = (void*)(_read_buffer); \
	int __ipc_rsize = (int)(_read_size); \
	int __ipc_read_status = 0

#define define_read_buffer(_size) \
	char __ipc_rbuf[_size]; \
	ipc_init_read(__ipc_rbuf, _size)

#define __ipc_rbuf_and_size \
	__ipc_rbuf, sizeof(__ipc_rbuf)

#define ipc_init_read_iwr(_iwr) \
	ipc_init_read((_iwr)->read_ptr, (_iwr)->bytes_read)

#define ipc_read_int(_val) \
	__ipc_read_int(__ipc_read_status, __ipc_rptr, __ipc_rsize, _val)

#define ipc_read_int32(_val) \
	__ipc_read_int32(__ipc_read_status, __ipc_rptr, __ipc_rsize, _val)

#define ipc_read_int64(_val) \
	__ipc_read_int64(__ipc_read_status, __ipc_rptr, __ipc_rsize, _val)

#define ipc_read_str(_str) \
	__ipc_read_str(__ipc_read_status, __ipc_rptr, __ipc_rsize, _str)

#define ipc_read_data(_data, _datasize) \
	__ipc_read_data(__ipc_read_status, __ipc_rptr, __ipc_rsize, _data, _datasize)

#define ipc_copy_data(_data, _datasize) \
	__ipc_copy_data(__ipc_read_status, __ipc_rptr, __ipc_rsize, _data, _datasize)

#define ipc_read_failed() \
	(__ipc_read_status < 0)

void *__ipc_read_strlist(int *status, size_t *size, void *ptr, int *count, char ***pstrlist);

#define ipc_read_strlist(_count, _strlist) \
	__ipc_rptr = __ipc_read_strlist(&__ipc_read_status, &__ipc_rsize, __ipc_rptr, &(_count), &(_strlist))


#define server_cmd_done(_iwr, _rval) \
	do { \
		(_iwr)->write_val = _rval; \
		(_iwr)->write_size = 0; \
		(_iwr)->flags = IPC_FLAG_WRITE | IPC_FLAG_READ; \
	} while (0)

#define server_cmd_done_write(_iwr, _rval) \
	do { \
		(_iwr)->write_val = _rval; \
		(_iwr)->write_ptr = __ipc_wbuf; \
		(_iwr)->write_size = __ipc_wbuf_size; \
		(_iwr)->flags = IPC_FLAG_WRITE | IPC_FLAG_READ; \
	} while (0)

#define server_read_fail(_iwr) \
	do { \
		(_iwr)->write_val = -1; \
		(_iwr)->write_size = 0; \
		(_iwr)->flags = IPC_FLAG_WRITE | IPC_FLAG_READ; \
	} while (0)

//===========================================================================================================

static inline ipc_reg_id_t ipc_reg_query_id(int type, const char *name, int index, int options)
{
	ipc_query_reg_id_t param;
	param.type = type;
	param.name = (char*)name;
	param.name_length = name ? strlen(name) : 0;
	param.index = index;
	param.options = options;
	param.reg_id = INVALID_IPC_REG_ID;
	return ioctl(g_ipc_fd, IPC_REG_QUERY_ID, &param) == 0 ? param.reg_id : INVALID_IPC_REG_ID;
}

static inline ipc_reg_id_t ipc_reg_create_for_read(int type, const char *name, int index, int no_updated)
{
	int options = no_updated ? (REG_ID_OPT_CREATE|REG_ID_OPT_NO_UPDATED) : REG_ID_OPT_CREATE;
	return ipc_reg_query_id(type, name, index, options);
}

static inline ipc_reg_id_t ipc_reg_create_for_write(int type, const char *name, int index)
{
	return ipc_reg_query_id(type, name, index, REG_ID_OPT_CREATE|REG_ID_OPT_WRONLY);
}

static inline int ipc_reg_remove_id(ipc_reg_id_t reg_id)
{
	ipc_reg_action_t param;
	param.reg_id = reg_id;
	param.options = 0;
	return ioctl(g_ipc_fd, IPC_REG_REMOVE_ID, &param);
}

static inline int ipc_reg_write(ipc_reg_read_write_t *param)
{
	return ioctl(g_ipc_fd, IPC_REG_WRITE, param);
}

static inline int ipc_reg_read(ipc_reg_read_write_t *param)
{
	return ioctl(g_ipc_fd, IPC_REG_READ, param);
}

static inline int ipc_reg_read_any(ipc_reg_read_write_t *param)
{
	return ioctl(g_ipc_fd, IPC_REG_READ_ANY, param);
}

static inline int ipc_reg_info(ipc_query_reg_id_t *info)
{
	return ioctl(g_ipc_fd, IPC_REG_ID_INFO, info);
}

static inline int ipc_dmesg(void)
{
	ipc_reg_action_t param;
	param.reg_id = INVALID_IPC_REG_ID;	// printk all
	param.options = 0;
	return ioctl(g_ipc_fd, IPC_REG_DMESG, &param);
}

// return number of items filled, or -1 if failed
static inline int ipc_get_all(ipc_reg_id_item_t *id_items, int length)
{
	ipc_reg_get_all_t param;

	param.num_entries = length;
	param.total = 0;
	param.items = id_items;

	if (ioctl(g_ipc_fd, IPC_REG_GET_ALL, &param) < 0)
		return -1;

	return param.total;
}

#ifdef __cplusplus
}
#endif

#endif

