
#ifndef __IPC_DRV_H__
#define __IPC_DRV_H__

#include <linux/ioctl.h>

typedef unsigned int ipc_cid_t;	// channel id
typedef unsigned long ipc_rid_t;	// request id

#define IPC_INVALID_CID	0
#define IPC_INVALID_RID	0

#define MAX_SERVICE_NAME	64	// bytes

typedef struct ipc_service_s {
	ipc_cid_t service_cid;		// out - service channel's id
	char name[MAX_SERVICE_NAME];	// in - service name
} ipc_service_t;

#define IPC_FLAG_WRITE	(1 << 0)
#define IPC_FLAG_READ		(1 << 1)
#define IPC_FLAG_DENY_CMD	(1 << 2)
#define IPC_FLAG_NOWAIT	(1 << 3)
#define IPC_NUM_NOTIF		4	// total 32*4 = 128

typedef struct ipc_write_read_s {
	ipc_cid_t	input_cid[1];	// in (which channel to operate)
	ipc_cid_t	output_cid[1];	// out (returns a new channel; for both client and server)
	ipc_rid_t	rid[1];		// in/out (returns to server the request id; server uses it to reply to)
	int error;			// return to client

	unsigned long	flags;		// IPC_FLAG_WRITE, IPC_FLAG_READ (for server)

	int		write_val;	// in (for client, it's command id; for server, it's return value)
	const void	*write_ptr;	// in (for client, it's command parameters; for server, it's return data)
	size_t		write_size;	// in

	int		read_val;	// out (for client, it's return value; for server, it's command id)
	void		*read_ptr;	// in (for client, it's return data; for server, it's command parameters)
	size_t		read_size;	// in
	size_t		bytes_read;	// out (bytes actually read)
} ipc_write_read_t;

typedef struct ipc_notify_s {
	ipc_cid_t	cid;		// channel id
	unsigned long	values;		// one per bit
} ipc_notify_t;

typedef struct ipc_notify_ex_s {
	ipc_cid_t	cid;
	unsigned long	values[IPC_NUM_NOTIF];
} ipc_notify_ex_t;

typedef struct ipc_single_notify_s {
	ipc_cid_t	cid;
	unsigned long	value;
} ipc_single_notify_t;

typedef struct ipc_timeout_s {
	ipc_cid_t	cid;
	unsigned long	value;
} ipc_timeout_t;

#define IPC_MAGIC 'i'

#define IPC_REGISTER_SERVICE		_IOWR(IPC_MAGIC, 1, ipc_service_t)	// server; register a service
#define IPC_QUERY_SERVICE		_IOWR(IPC_MAGIC, 2, ipc_service_t)	// client; query the service

#define IPC_SETUP_CHANNEL		_IOWR(IPC_MAGIC, 3, ipc_write_read_t)// server; create a new channel
#define IPC_CLOSE_CHANNEL		_IOWR(IPC_MAGIC, 4, ipc_cid_t)	// server/client; close a channel

#define IPC_SEND_CMD			_IOWR(IPC_MAGIC, 5, ipc_write_read_t)	// client; send a command and return reply
#define IPC_WRITE_READ		_IOWR(IPC_MAGIC, 6, ipc_write_read_t)	// server; write result and then read the next command

#define IPC_READ_NOTIFICATION	_IOR(IPC_MAGIC, 7, ipc_notify_t)	// client; wait and read notifications
#define IPC_WRITE_NOTIFICATION	_IOW(IPC_MAGIC, 8, ipc_notify_t)	// server; send notification (no waiting)

#define IPC_SET_TIMEOUT		_IOW(IPC_MAGIC, 9, ipc_timeout_t)	// client;
#define IPC_SHUTDOWN		_IOWR(IPC_MAGIC, 10, ipc_cid_t)				// client & server

#define IPC_SET_NOTIFICATION_MASK	_IOW(IPC_MAGIC, 11, ipc_notify_ex_t)	// client
#define IPC_GET_NOTIFICATION_MASK	_IOR(IPC_MAGIC, 12, ipc_notify_ex_t)	// client

#define IPC_READ_NOTIFICATION_EX	_IOR(IPC_MAGIC, 13, ipc_notify_ex_t)	// client
#define IPC_WRITE_NOTIFICATION_EX	_IOW(IPC_MAGIC, 14, ipc_single_notify_t)	// server

// ================================================================================================
// reg[type,name,index] = value
// ================================================================================================

typedef unsigned long ipc_reg_id_t;
#define INVALID_IPC_REG_ID	0

#define MAX_REG_NAME	64			// max name length, including 0
#define MAX_REG_DATA	(4*1024)	// large data should not use this driver
#define MAX_REG_NUM		1024		// max number of registry items

// options for IPC_REG_QUERY_ID
#define REG_ID_OPT_NONE			0
#define REG_ID_OPT_CREATE		(1 << 0)	// create the id if it does not exist
#define REG_ID_OPT_NO_UPDATED	(1 << 1)	// when creating pnode, set updated to 0
#define REG_ID_OPT_WRONLY		(1 << 2)	// create for write-only

typedef struct ipc_query_reg_id_s {
	int type;
	char *name;
	int name_length;
	int index;
	int options;
	//-------------------
	ipc_reg_id_t reg_id;
} ipc_query_reg_id_t;

// options for read/write
#define REG_OPT_NONE			0
#define REG_OPT_IGNORE_UPDATED	(1 << 0)	// read even 'updated' flag is 0
#define REG_OPT_KEEP_UPDATED	(1 << 1)	// keep updated flag unchanged after read
#define REG_OPT_WRITE_TIMESTAMP	(1 << 2)	// write timestamp, for IPC_REG_WRITE
#define REG_OPT_READ_ALL		(1 << 3)	// read all registry, otherwise read the created ids only. for IPC_REG_READ_ANY
#define REG_OPT_DONT_READ		(1 << 4)	// just wait for update. for IPC_REG_READ_ANY and REG_OPT_READ_ALL
#define REG_OPT_REMOVE_READALL	(1 << 5)	// remove from readall list before read. for IPC_REG_READ_ANY and REG_OPT_READ_ALL

#define REG_OPT_QUICK_READ		(REG_OPT_IGNORE_UPDATED|REG_OPT_KEEP_UPDATED)

typedef struct ipc_reg_read_write_s {
	ipc_reg_id_t reg_id;	// in; out for IPC_REG_READ_ANY
	void *ptr;				// in
	size_t size;			// bytes pointed by ptr; in/out
	int options;			// REG_OPT_NONE etc
	long timeout;			// 0 : no wait; < 0: wait forever; > 0 : time out value in ms
	int type;				// out: for read
	int index;				// out: for read
	unsigned long seqnum;	// readonly
	struct timespec ts_write;	// in/out; timestamp when write
	struct timespec ts_read;	// out; timestamp when read
} ipc_reg_read_write_t;

typedef struct ipc_reg_action_s {
	ipc_reg_id_t reg_id;
	int options;	// must be 0 for now
} ipc_reg_action_t;

typedef struct ipc_reg_id_item_s {
	int type;
	ipc_reg_id_t reg_id;
} ipc_reg_id_item_t;

typedef struct ipc_reg_get_all_s {
	int num_entries;			// in: number of items
	int total;					// out: number of filled
	ipc_reg_id_item_t *items;	// buffer to receive items
} ipc_reg_get_all_t;

#define REG_MAGIC 'r'

#define _REG_QUERY_ID		1
#define _REG_REMOVE_ID		2
#define _REG_ID_INFO		3

#define _REG_READ			4
#define _REG_WRITE			5
#define _REG_RESET			6
#define _REG_READ_ANY		7

#define _REG_DMESG			8
#define _REG_GET_ALL		9
#define _REG_ENABLE			10

#define IPC_REG_QUERY_ID	_IOWR(REG_MAGIC, _REG_QUERY_ID, ipc_query_reg_id_t)
#define IPC_REG_REMOVE_ID	_IOW(REG_MAGIC, _REG_REMOVE_ID, ipc_reg_action_t)
#define IPC_REG_ID_INFO		_IOR(REG_MAGIC, _REG_ID_INFO, ipc_query_reg_id_t)

#define IPC_REG_READ		_IOWR(REG_MAGIC, _REG_READ, ipc_reg_read_write_t)
#define IPC_REG_WRITE		_IOWR(REG_MAGIC, _REG_WRITE, ipc_reg_read_write_t)
#define IPC_REG_RESET		_IOW(REG_MAGIC, _REG_RESET, ipc_reg_action_t)
#define IPC_REG_READ_ANY	_IOWR(REG_MAGIC, _REG_READ_ANY, ipc_reg_read_write_t)

#define IPC_REG_DMESG		_IOW(REG_MAGIC, _REG_DMESG, ipc_reg_action_t)
#define IPC_REG_GET_ALL		_IOR(REG_MAGIC, _REG_GET_ALL, ipc_reg_get_all_t)
#define IPC_REG_ENABLE		_IOR(REG_MAGIC, _REG_ENABLE, int32_t*)

#endif

