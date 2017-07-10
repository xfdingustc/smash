
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/ctype.h>

#include "ipc_drv.h"
#include "event.c"
//#include "event.h"

#define IPC_ENABLE_DEBUG	1	// enable debug printk
#define IPC_TRACE_MEM		1	// tracing kmalloc, kfree

#define __ipc_printk(fmt, ...) \
	printk("[%s] %s - " fmt "\n", current->comm, __FUNCTION__, ##__VA_ARGS__)

#if IPC_ENABLE_DEBUG
#define ipc_printk __ipc_printk
#else
#define ipc_printk(fmt, ...)
#endif


#define IPC_MAX_CHANNEL		32
#define IPC_MAX_WRITE_BYTES		(8*1024)
#define IPC_MAX_READ_BYTES		(8*1024)

// request state
enum {
	REQUEST_STATE_FREE,		// not used
	REQUEST_STATE_CMD,		// waiting for cmd to be read (in cmd_list)
	REQUEST_STATE_REPLY,		// waiting for reply (in reply_list)
	REQUEST_STATE_REPLIED,	// replied
};

// client request
typedef struct ipc_request_s {
	struct ipc_client_s *client;	// the client
	struct list_head list;		// in free_request_list/cmd_list/reply_list
	struct rb_node rb_node;	// in reply_tree (also in reply_list)
	event_t event_reply;

	int state;			// REQUEST_STATE_XXX
	int denied;			// this request is denied by server
	struct ipc_channel_s *new_channel;	// if server created a new channel

	int write_val;			// cmd number to server
	char *write_ptr;		// cmd data to server
	size_t write_size;

	int reply_val;			// return value from server
	char *reply_ptr;		// return data from server
	size_t reply_size;
} ipc_request_t;

#define __list_to_request(_node) \
	list_entry(_node, ipc_request_t, list)

#define __rbnode_to_request(_node) \
	rb_entry(_node, ipc_request_t, rb_node)

// client
typedef struct ipc_client_s {
	int is_channel;			// 0 for client; must be first

	struct	ipc_channel_s *channel;
	struct list_head list_channel;	// link to ipc_channel_s.client_list

	struct list_head cmd_list;	// requests waiting to be read
	struct list_head reply_list;	// requests waiting to be replied

	unsigned long notifications[IPC_NUM_NOTIF];
	unsigned long notif_mask[IPC_NUM_NOTIF];	// if 1, disable
	event_t event_notify;

	unsigned long timeout;

	int num_client_threads;		// number of client threads using this client channel
	int client_closing;
	event_t event_client_closing;
} ipc_client_t;

// from list_channel to ipc_client_t
#define __node_to_client(_node) \
	list_entry(_node, ipc_client_t, list_channel)

// channel
typedef struct ipc_channel_s {
	int is_channel;				// 1 for server; must be first

	struct ipc_service_desc_s *sdesc;
	struct list_head client_list;		// clients list
	int num_clients;			// number of clients

	struct list_head free_request_list;	// free request node
	struct rb_root reply_tree;		// all requests waiting to be replied

	int num_server_threads;			// number of server threads using this channel
	event_t event_read;

	int channel_closed;
	event_t event_server_close;
} ipc_channel_t;

// per file (/dev/ipc)
typedef struct ipc_proc_s {
	struct list_head list_procs;
	pid_t pid;
	int closing_file;
	void *channels[IPC_MAX_CHANNEL];	// ipc_channel_t or ipc_client_t
	////
	struct task_struct *task;
	int enabled;
	int num_readany_threads;	// for reg_read_any
	event_t event_close;
	event_t event_readany;		// for reg_read_any
	struct list_head reg_list;
	struct list_head list_readall;	// g_reg_readall_list
} ipc_proc_t;

#define __list_to_proc(_node) \
	list_entry(_node, ipc_proc_t, list_procs)

typedef struct ipc_service_desc_s {
	struct rb_node rb_node;		// link to g_all_services
	ipc_channel_t *channel;		// the service channel
	unsigned long hash_val;		// hash value of name
	char name[MAX_SERVICE_NAME];		// service name
} ipc_service_desc_t;

#define __rbnode_to_sdesc(_node) \
	rb_entry(_node, ipc_service_desc_t, rb_node)

static DEFINE_MUTEX(g_ipc_mutex);
static struct rb_root g_all_services = RB_ROOT;
static LIST_HEAD(g_all_procs);
static atomic_t g_total_alloc = {0};

static void ipc_wakeup_server(ipc_channel_t *channel);
static int ipc_close_channel(ipc_proc_t *proc, ipc_cid_t cid);
static int ipc_create_client(ipc_proc_t *proc, ipc_channel_t *channel, ipc_cid_t *cid);

static inline void ipc_lock(void)
{
	mutex_lock(&g_ipc_mutex);
}

static inline void ipc_unlock(void)
{
	mutex_unlock(&g_ipc_mutex);
}

static inline void *ipc_zalloc(size_t size)
{
#if IPC_TRACE_MEM
	atomic_inc(&g_total_alloc);
#endif
	return kzalloc(size, GFP_KERNEL);
}

static inline void *ipc_malloc(size_t size)
{
#if IPC_TRACE_MEM
	atomic_inc(&g_total_alloc);
#endif
	return kmalloc(size, GFP_KERNEL);
}

static inline void ipc_free(void *ptr)
{
	kfree(ptr);
#if IPC_TRACE_MEM
	atomic_dec(&g_total_alloc);
#endif
}

// value is ms
static inline long get_timeout_value(long value)
{
	if (value < 0)
		return MAX_SCHEDULE_TIMEOUT;

	if (value == 0)
		return 0;

	return msecs_to_jiffies(value);
}

static inline ipc_channel_t *__ipc_get_channel(ipc_proc_t *proc, ipc_cid_t cid)
{
	return (--cid >= IPC_MAX_CHANNEL) ? NULL : proc->channels[cid];
}

static ipc_channel_t *ipc_get_channel(ipc_proc_t *proc, ipc_cid_t cid)
{
	ipc_channel_t *channel;

	if (--cid >= IPC_MAX_CHANNEL) {
		ipc_printk("bad channel id %d", cid + 1);
		return NULL;
	}

	if ((channel = proc->channels[cid]) == NULL) {
		ipc_printk("channel is invalid %d", cid + 1);
		return NULL;
	}

	if (!channel->is_channel) {
		ipc_printk("not a server channel");
		return NULL;
	}

	return channel;
}

static ipc_client_t *ipc_get_client(ipc_proc_t *proc, ipc_cid_t cid)
{
	ipc_client_t *client;

	if (--cid >= IPC_MAX_CHANNEL) {
		ipc_printk("bad channel id %d", cid + 1);
		return NULL;
	}

	if ((client = proc->channels[cid]) == NULL) {
		ipc_printk("channel is invalid %d", cid + 1);
		return NULL;
	}

	if (client->is_channel) {
		ipc_printk("not a client channel");
		return NULL;
	}

	return client;
}

static inline void ipc_set_channel(ipc_proc_t *proc, ipc_cid_t cid, void *channel)
{
	proc->channels[cid - 1] = channel;
}

static int ipc_get_free_cid(ipc_proc_t *proc)
{
	int i;

	for (i = 0; i < IPC_MAX_CHANNEL; i++) {
		if (proc->channels[i] == NULL)
			return i + 1;
	}

	ipc_printk("no free cid");
	return IPC_INVALID_CID;
}

// server creates a channel
static ipc_channel_t *ipc_create_channel(ipc_service_desc_t *sdesc)
{
	ipc_channel_t *channel;

	if ((channel = ipc_malloc(sizeof(*channel))) == NULL)
		return NULL;

	channel->is_channel = 1;

	channel->sdesc = sdesc;
	INIT_LIST_HEAD(&channel->client_list);
	channel->num_clients = 0;

	INIT_LIST_HEAD(&channel->free_request_list);
	channel->reply_tree = RB_ROOT;

	channel->num_server_threads = 0;
	event_init(&channel->event_read);

	channel->channel_closed = 0;
	event_init(&channel->event_server_close);

	return channel;
}

static inline void ipc_remove_service(ipc_service_desc_t *sdesc)
{
	rb_erase(&sdesc->rb_node, &g_all_services);
}

static inline void ipc_destroy_request(ipc_request_t *request)
{
	ipc_free(request);
}

static void ipc_destroy_channel(ipc_channel_t *channel)
{
	struct list_head *node;
	struct list_head *tmp;

	list_for_each_safe(node, tmp, &channel->free_request_list) {
		ipc_request_t *request = __list_to_request(node);
		ipc_destroy_request(request);
	}

	if (channel->sdesc) {
		// service channel
		ipc_remove_service(channel->sdesc);
		ipc_free(channel->sdesc);
	}

	ipc_free(channel);
}

// client released channel
static void ipc_release_client_channel(ipc_channel_t *channel)
{
	if (--channel->num_clients == 0) {
		if (channel->channel_closed) {
			ipc_destroy_channel(channel);
		} else {
			ipc_wakeup_server(channel);
		}
	}
}

// get or create a new request node
static ipc_request_t *ipc_create_request(ipc_client_t *client)
{
	struct list_head *node;
	ipc_request_t *request;

	// check the free list (cache)
	node = client->channel->free_request_list.next;
	if (node != &client->channel->free_request_list) {
		list_del(node);
		request = __list_to_request(node);
		goto Done;
	}

	// create a new one
	if ((request = ipc_malloc(sizeof(*request))) == NULL)
		return NULL;

	event_init(&request->event_reply);

Done:
	request->client = client;
	request->state = REQUEST_STATE_FREE;
	return request;
}

static inline void ipc_release_request(ipc_client_t *client, ipc_request_t *request)
{
	// save to the free_request_list so we can use it later
	list_add(&request->list, &client->channel->free_request_list);
}

// save to the cmd_list (waiting to be read)
static inline void ipc_push_request(ipc_client_t *client, ipc_request_t *request)
{
	list_add_tail(&request->list, &client->cmd_list);
}

// remove from the cmd_list
static inline void ipc_pop_request(ipc_request_t *request)
{
	list_del(&request->list);
}

// get a request from the cmd_list
static ipc_request_t *ipc_get_request(ipc_channel_t *channel)
{
	struct list_head *client_node;
	struct list_head *request_node;

	list_for_each(client_node, &channel->client_list) {
		ipc_client_t *client = __node_to_client(client_node);
		request_node = client->cmd_list.next;
		if (request_node != &client->cmd_list) {
			list_del(request_node);
			return __list_to_request(request_node);
		}
	}

	return NULL;
}

// insert the request to reply_list (and the tree)
static void ipc_insert_request(ipc_client_t *client, ipc_request_t *request)
{
	ipc_channel_t *channel = client->channel;
	struct rb_node **p = &channel->reply_tree.rb_node;
	struct rb_node *parent = NULL;
	ipc_request_t *tmp;

	while (*p) {
		parent = *p;
		tmp = __rbnode_to_request(parent);

		if (request < tmp)
			p = &parent->rb_left;
		else
			p = &parent->rb_right;
	}

	rb_link_node(&request->rb_node, parent, p);
	rb_insert_color(&request->rb_node, &channel->reply_tree);

	// add to the reply_list
	list_add(&request->list, &client->reply_list);
}

// find the request
static inline ipc_request_t *ipc_lookup_request(ipc_channel_t *channel, ipc_request_t *request)
{
	struct rb_node *n = channel->reply_tree.rb_node;

	while (n) {
		ipc_request_t *tmp = __rbnode_to_request(n);

		if (request < tmp)
			n = n->rb_left;
		else if (request > tmp)
			n = n->rb_right;
		else
			return request;
	}

	ipc_printk("request 0x%lx not found", (ipc_rid_t)request);
	return NULL;
}

// remove the request from reply_list and reply_tree
static inline void ipc_delete_request(ipc_request_t *request)
{
	ipc_channel_t *channel = request->client->channel;
	rb_erase(&request->rb_node, &channel->reply_tree);
	list_del(&request->list);
}

static void ipc_clear_request(ipc_client_t *client, ipc_request_t *request)
{
	switch (request->state) {
	case REQUEST_STATE_FREE:
		break;

	case REQUEST_STATE_CMD:
		ipc_pop_request(request);
		break;

	case REQUEST_STATE_REPLY:
		ipc_delete_request(request);
		break;

	case REQUEST_STATE_REPLIED:
		break;

	default:
		break;
	}

	if (request->write_ptr) {
		ipc_free(request->write_ptr);
		request->write_ptr = NULL;
	}

	if (request->reply_ptr) {
		ipc_free(request->reply_ptr);
		request->reply_ptr = NULL;
	}

	if (request->new_channel) {
		ipc_channel_t *channel = request->new_channel;
		request->new_channel = NULL;
		ipc_release_client_channel(channel);
	}
}

// send a command and wait for reply
static int ipc_send_cmd(ipc_proc_t *proc, ipc_write_read_t *iwr)
{
	ipc_client_t *client;
	ipc_request_t *request;
	char *write_ptr;
	size_t write_size;
	int rval;

	// get the channel
	if ((client = ipc_get_client(proc, iwr->input_cid[0])) == NULL)
		return -EINVAL;

	// allocate a request node
	if ((request = ipc_create_request(client)) == NULL)
		return -ENOMEM;

	if (iwr->write_size > 0) {

		if ((write_size = iwr->write_size) > IPC_MAX_WRITE_BYTES) {
			ipc_printk("bad write_size: %d", write_size);
			ipc_release_request(client, request);
			return -EINVAL;
		}

		// allocate the buffer to save the command data
		if ((write_ptr = ipc_malloc(write_size)) == NULL) {
			ipc_release_request(client, request);
			return -ENOMEM;
		}

		// copy from user
		if (copy_from_user(write_ptr, iwr->write_ptr, write_size)) {
			ipc_free(write_ptr);
			ipc_release_request(client, request);
			return -EFAULT;
		}

	} else {

		write_ptr = NULL;
		write_size = 0;
	}

	// setup the node
	request->new_channel = NULL;

	request->write_val = iwr->write_val;
	request->write_ptr = write_ptr;
	request->write_size = write_size;

	request->reply_val = 0;
	request->reply_ptr = NULL;
	request->reply_size = 0;

	// append to the cmd list
	request->denied = 0;
	request->state = REQUEST_STATE_CMD;
	ipc_push_request(client, request);

	// wake up any reader(server)
	event_signal(&client->channel->event_read);

	client->num_client_threads++;

	// wait for reply
	while (1) {

		// if closing
		if (client->client_closing) {
			ipc_printk("client_closing");
			rval = -EPIPE;
			goto Fail;
		}

		// if replied
		if (request->state == REQUEST_STATE_REPLIED)
			break;

		// if server is alive
		if (client->channel->channel_closed) {
			ipc_printk("no server");
			rval = -EPIPE;
			goto Fail;
		}

		// otherwise, release lock and wait
		rval = wait_for_event_interruptible_timeout(&g_ipc_mutex, &request->event_reply, client->timeout);

		// if interrupted
		if (rval < 0) {
			//ipc_printk("interrupted");
			goto Fail;
		}
	}

	// if denied by server
	if (request->denied) {
		ipc_printk("request denied by server");
		rval = -EAGAIN;
		goto Fail;
	}

	// if server created a new channel for this request
	if (request->new_channel) {
		ipc_cid_t cid;

		if ((rval = ipc_create_client(proc, request->new_channel, &cid)) < 0)
			goto Fail;

		request->new_channel = NULL;
		iwr->output_cid[0] = cid;
	}

	// replied, copy the reply data
	if (iwr->read_ptr) {
		if (iwr->read_size < request->reply_size) {
			ipc_printk("bad read_size: %d", iwr->read_size);
			rval = -EINVAL;
			goto Fail;
		}

		if (request->reply_size > 0)
			if (copy_to_user(iwr->read_ptr, request->reply_ptr, request->reply_size)) {
				rval = -EPERM;
				goto Fail;
			}
	}

	iwr->read_val = request->reply_val;
	iwr->bytes_read = request->reply_size;

	rval = 0;

Fail:
	ipc_clear_request(client, request);
	ipc_release_request(client, request);

	client->num_client_threads--;
	event_signal(&client->event_client_closing);

	return rval;
}

// some problem happens when read/replied by server
// so deny the request
static void ipc_deny_request(ipc_request_t *request)
{
	request->denied = 1;
	if (request->state == REQUEST_STATE_REPLY)
		ipc_delete_request(request);
	request->state = REQUEST_STATE_REPLIED;
	event_signal(&request->event_reply);
}

// server reads a command
static int ipc_read_cmd(ipc_channel_t *channel, ipc_write_read_t *iwr)
{
	ipc_request_t *request;
	int rval;

	channel->num_server_threads++;

	while (1) {
		// if closing
		if (channel->channel_closed) {
			ipc_printk("server closed");
			rval = -EPIPE;
			goto Fail;
		}

		// if no client
		if (channel->num_clients == 0) {
			ipc_printk("no client");
			rval = -EPIPE;
			goto Fail;
		}

		// try to get a request node from the list
		if ((request = ipc_get_request(channel)) != NULL)
			break;

		// no request, wait on the list
		rval = wait_for_event_interruptible(&g_ipc_mutex, &channel->event_read);

		// if interrupted
		if (rval < 0) {
			//ipc_printk("interrupted");
			goto Fail;
		}
	}

	// check if the buffer is large enought to read the request data
	if (iwr->read_size < request->write_size) {
		ipc_printk("read_size (%d) < write_size (%d)", iwr->read_size, iwr->write_size);
		ipc_deny_request(request);
		rval = -EINVAL;
		goto Fail;
	}

	iwr->read_val = request->write_val;

	// copy the request data
	iwr->bytes_read = min(iwr->read_size, request->write_size);
	if (iwr->bytes_read > 0) {
		if (copy_to_user(iwr->read_ptr, request->write_ptr, iwr->bytes_read)) {
			ipc_deny_request(request);
			rval = -EFAULT;
			goto Fail;
		}
	}

	// add to the tree
	request->state = REQUEST_STATE_REPLY;
	ipc_insert_request(request->client, request);

	iwr->rid[0] = (ipc_rid_t)request;
	rval = 0;

Fail:
	channel->num_server_threads--;
	event_signal(&channel->event_server_close);

	return rval;
}

// server writes reply
static int ipc_write_reply(ipc_channel_t *channel, 
	ipc_write_read_t *iwr, ipc_channel_t *new_channel)
{
	ipc_request_t *request;

	// find the request node
	request = ipc_lookup_request(channel, (ipc_request_t*)iwr->rid[0]);
	if (request == NULL)
		return -EINVAL;

	// reply msg - can be 0 bytes (reply without data)
	if (iwr->write_size > 0) {
		if (iwr->write_size > IPC_MAX_READ_BYTES) {
			ipc_deny_request(request);
			return -EINVAL;
		}

		if ((request->reply_ptr = ipc_malloc(iwr->write_size)) == NULL) {
			ipc_deny_request(request);
			return -ENOMEM;
		}

		if (copy_from_user(request->reply_ptr, iwr->write_ptr, iwr->write_size)) {
			ipc_free(request->reply_ptr);
			request->reply_ptr = NULL;
			ipc_deny_request(request);
			return -EFAULT;
		}

		request->reply_size = iwr->write_size;
	}

	// if a new channel is created by server
	if (new_channel) {
		new_channel->num_clients++;
		request->new_channel = new_channel;
	}

	// delete the node from the tree, and wake up client
	request->reply_val = iwr->write_val;
	request->state = REQUEST_STATE_REPLIED;
	ipc_delete_request(request);
	event_signal(&request->event_reply);

	return 0;
}

// server write and then read
static int ipc_write_read(ipc_proc_t *proc, ipc_write_read_t *iwr)
{
	ipc_channel_t *channel;
	int rval;

	if ((channel = ipc_get_channel(proc, iwr->input_cid[0])) == NULL)
		return -EINVAL;

	if ((iwr->flags & (IPC_FLAG_WRITE|IPC_FLAG_DENY_CMD)) != 0) {
		if ((iwr->flags & IPC_FLAG_DENY_CMD) != 0) {
			iwr->write_size = 0;
		}
		if ((rval = ipc_write_reply(channel, iwr, NULL)) < 0)
			return rval;
	}

	if ((iwr->flags & IPC_FLAG_READ) != 0) {
		if ((rval = ipc_read_cmd(channel, iwr)) < 0)
			return rval;
	}

	return 0;
}

// ipc_setup_channel
// ipc_send_cmd
// ipc_write_read
static int ipc_process_write_read(ipc_proc_t *proc, void __user *ubuf, int (*fn)(ipc_proc_t*, ipc_write_read_t*))
{
	ipc_write_read_t iwr;
	int rval;

	if (copy_from_user(&iwr, ubuf, sizeof(iwr)))
		return -EFAULT;

	iwr.output_cid[0] = IPC_INVALID_CID;

	if ((rval = (*fn)(proc, &iwr)) == 0) {
		if (copy_to_user(ubuf, &iwr, sizeof(iwr))) {
			if (iwr.output_cid[0] != IPC_INVALID_CID) {
				ipc_close_channel(proc, iwr.output_cid[0]);
			}
			rval = -EFAULT;
		}
	}

	return rval;
}

static int ipc_create_client(ipc_proc_t *proc, ipc_channel_t *channel, ipc_cid_t *cid)
{
	ipc_client_t *client;

	if ((*cid = ipc_get_free_cid(proc)) == IPC_INVALID_CID)
		return -EMFILE;

	if ((client = ipc_malloc(sizeof(*client))) == NULL)
		return -ENOMEM;

	client->is_channel = 0;

	client->channel = channel;
	list_add_tail(&client->list_channel, &channel->client_list);

	INIT_LIST_HEAD(&client->cmd_list);
	INIT_LIST_HEAD(&client->reply_list);

	memset(client->notifications, 0, sizeof(client->notifications));
	memset(client->notif_mask, 0, sizeof(client->notif_mask));
	event_init(&client->event_notify);

	client->timeout = MAX_SCHEDULE_TIMEOUT;

	client->num_client_threads = 0;
	client->client_closing = 0;
	event_init(&client->event_client_closing);

	ipc_set_channel(proc, *cid, client);

	return 0;
}

static inline void ipc_destroy_client(ipc_client_t *client)
{
	ipc_free(client);
}

// server creates a new channel in replying a request
static int ipc_setup_channel(ipc_proc_t *proc, ipc_write_read_t *iwr)
{
	ipc_channel_t *channel;
	ipc_cid_t new_cid;
	ipc_channel_t *new_channel;
	int rval;

	if ((channel = ipc_get_channel(proc, iwr->input_cid[0])) == NULL)
		return -EINVAL;

	if ((new_cid = ipc_get_free_cid(proc)) == IPC_INVALID_CID)
		return -EMFILE;

	if ((new_channel = ipc_create_channel(NULL)) == NULL)
		return -ENOMEM;

	rval = ipc_write_reply(channel, iwr, new_channel);
	if (rval < 0) {
		ipc_destroy_channel(new_channel);
		return rval;
	}

	ipc_set_channel(proc, new_cid, new_channel);
	iwr->output_cid[0] = new_cid;

	return 0;
}

// wake up the threads waiting on 'client'
static void ipc_wakeup_client(ipc_client_t *client)
{
	struct list_head *node;
	struct list_head *tmp;
	ipc_request_t *request;

	list_for_each_safe(node, tmp, &client->cmd_list) {
		request = __list_to_request(node);
		ipc_pop_request(request);
		request->state = REQUEST_STATE_REPLIED;
		event_signal(&request->event_reply);
	}

	list_for_each_safe(node, tmp, &client->reply_list) {
		request = __list_to_request(node);
		ipc_delete_request(request);
		request->state = REQUEST_STATE_REPLIED;
		event_signal(&request->event_reply);
	}

	event_signal_all(&client->event_notify);
}

// wake up all client threads waiting on the channel
static void ipc_wakeup_all_clients(ipc_channel_t *channel)
{
	struct list_head *client_node;
	list_for_each(client_node, &channel->client_list) {
		ipc_client_t *client = __node_to_client(client_node);
		ipc_wakeup_client(client);
	}
}

// wake up all server threads waiting on the channel
static void ipc_wakeup_server(ipc_channel_t *channel)
{
	event_signal_all(&channel->event_read);
}

// called by server
static void __ipc_close_channel(ipc_channel_t *channel)
{
	channel->channel_closed = 1;

	if (channel->num_server_threads > 0) {
		ipc_wakeup_server(channel);

		// wait for all server threads to quit
		while (channel->num_server_threads > 0) {
			wait_for_event(&g_ipc_mutex, &channel->event_server_close);
		}
	}

	if (channel->num_clients == 0) {
		ipc_destroy_channel(channel);
	} else {
		ipc_wakeup_all_clients(channel);
	}	
}

// called by client
static void ipc_close_client(ipc_client_t *client)
{
	client->client_closing = 1;

	if (client->num_client_threads > 0) {
		ipc_wakeup_client(client);

		// wait for all client threads to quit
		while (client->num_client_threads > 0) {
			wait_for_event(&g_ipc_mutex, &client->event_client_closing);
		}
	}

	ipc_destroy_client(client);
}

static int ipc_close_channel(ipc_proc_t *proc, ipc_cid_t cid)
{
	ipc_channel_t *channel;
	ipc_client_t *client;

	if ((channel = __ipc_get_channel(proc, cid)) == NULL) {
		ipc_printk("no such channel %d", cid);
		return -EINVAL;
	}

	ipc_set_channel(proc, cid, NULL);

	if (channel->is_channel) {
		if (channel->sdesc)
			channel->num_clients--;
		__ipc_close_channel(channel);
	} else {
		client = (ipc_client_t*)channel;
		channel = client->channel;

		list_del(&client->list_channel);
		ipc_close_client(client);

		ipc_release_client_channel(channel);
	}

	return 0;
}

static unsigned long ipc_calc_hash(const char *name)
{
	unsigned long hash_val;
	for (hash_val = 0; *name; name++)
		hash_val = *(unsigned char *)name + 31 * hash_val;
	return hash_val;
}

static ipc_service_desc_t *ipc_lookup_service(const char *name, unsigned long hash_val)
{
	struct rb_node *n = g_all_services.rb_node;

	while (n) {
		ipc_service_desc_t *sdesc = __rbnode_to_sdesc(n);

		if (hash_val < sdesc->hash_val)
			n = n->rb_left;
		else if (hash_val > sdesc->hash_val)
			n = n->rb_right;
		else {
			int val = strcmp(name, sdesc->name);
			if (val == 0)
				return sdesc;
			n = (val < 0) ? n->rb_left : n->rb_right;
		}
	}

	return NULL;
}

static void ipc_insert_service(ipc_service_desc_t *sdesc)
{
	struct rb_node **p = &g_all_services.rb_node;
	struct rb_node *parent = NULL;
	ipc_service_desc_t *tmp;

	while (*p) {
		parent = *p;
		tmp = __rbnode_to_sdesc(parent);

		if (sdesc->hash_val < tmp->hash_val)
			p = &parent->rb_left;
		else if (sdesc->hash_val > tmp->hash_val)
			p = &parent->rb_right;
		else {
			if (strcmp(sdesc->name, tmp->name) < 0)
				p = &parent->rb_left;
			else
				p = &parent->rb_right;
		}
	}

	rb_link_node(&sdesc->rb_node, parent, p);
	rb_insert_color(&sdesc->rb_node, &g_all_services);
}

static int ipc_register_service(ipc_proc_t *proc, ipc_service_t *service, unsigned long hash_val)
{
	ipc_service_desc_t *sdesc;
	ipc_channel_t *channel;
	ipc_cid_t cid;

	if ((sdesc = ipc_lookup_service(service->name, hash_val)) != NULL) {
		ipc_printk("service %s is already registered", service->name);
		return -EINVAL;
	}

	if ((cid = ipc_get_free_cid(proc)) == IPC_INVALID_CID)
		return -EMFILE;

	if ((sdesc = ipc_malloc(sizeof(*sdesc))) == NULL)
		return -ENOMEM;

	if ((channel = ipc_create_channel(sdesc)) == NULL) {
		ipc_free(sdesc);
		return -ENOMEM;
	}

	channel->num_clients++;	// to avoid free
	ipc_set_channel(proc, cid, channel);

	// the service
	sdesc->channel = channel;
	sdesc->hash_val = hash_val;
	memcpy(sdesc->name, service->name, MAX_SERVICE_NAME);
	ipc_insert_service(sdesc);

	// return the channel id
	service->service_cid = cid;

	return 0;
}

static int ipc_query_service(ipc_proc_t *proc, ipc_service_t *service, unsigned long hash_val)
{
	ipc_service_desc_t *sdesc;
	ipc_cid_t cid;
	int rval;

	if ((sdesc = ipc_lookup_service(service->name, hash_val)) == NULL) {
		ipc_printk("no such service: \"%s\"", service->name);
		service->service_cid = IPC_INVALID_CID;
		return 0;
	}

	if ((rval = ipc_create_client(proc, sdesc->channel, &cid)) < 0) {
		service->service_cid = IPC_INVALID_CID;
		return 0;
	}

	sdesc->channel->num_clients++;

	// return the channel id
	service->service_cid = cid;

	return 0;
}

// ipc_register_service
// ipc_query_service
static int ipc_process_service(ipc_proc_t *proc, void __user *ubuf, 
	int (*fn)(ipc_proc_t*, ipc_service_t*, unsigned long))
{
	ipc_service_t service;
	unsigned long hash_val;
	int rval;

	if (copy_from_user(&service, ubuf, sizeof(service)))
		return -EFAULT;

	service.service_cid = IPC_INVALID_CID;
	service.name[MAX_SERVICE_NAME-1] = 0;	// in case
	hash_val = ipc_calc_hash(service.name);

	if ((rval = (*fn)(proc, &service, hash_val)) == 0) {
		if (copy_to_user(ubuf, &service, sizeof(service))) {
			ipc_close_channel(proc, service.service_cid);
			rval = -EFAULT;
		}
	}

	return rval;
}

// client wait notification
static int ipc_read_notification(ipc_proc_t *proc, ipc_notify_t *notify)
{
	ipc_client_t *client;
	ipc_channel_t *channel;
	int rval;

	if ((client = ipc_get_client(proc, notify->cid)) == NULL)
		return -EINVAL;

	channel = client->channel;

	client->num_client_threads++;

	while (1) {
		
		// if closing
		if (client->client_closing) {
			rval = -EPIPE;
			goto Fail;
		}

		// if notification is ready
		if (client->notifications[0])
			break;

		// if server is alive
		if (channel->channel_closed) {
			ipc_printk("no server");
			rval = -EPIPE;
			goto Fail;
		}

		// otherwise release lock and wait
		rval = wait_for_event_interruptible(&g_ipc_mutex, &client->event_notify);
		
		// if interrupted
		if (rval < 0) {
			//ipc_printk("interrupted");
			goto Fail;
		}
	}

	notify->values = client->notifications[0];
	client->notifications[0] = 0;

	rval = 0;

Fail:
	client->num_client_threads--;
	event_signal(&client->event_client_closing);

	return rval;
}

static int ipc_set_notification_mask(ipc_proc_t *proc, ipc_notify_ex_t *notify)
{
	ipc_client_t *client;

	if ((client = ipc_get_client(proc, notify->cid)) == NULL)
		return -EINVAL;

	memcpy(client->notif_mask, notify->values, sizeof(client->notif_mask));

	return 0;
}

static int ipc_get_notification_mask(ipc_proc_t *proc, ipc_notify_ex_t *notify)
{
	ipc_client_t *client;

	if ((client = ipc_get_client(proc, notify->cid)) == NULL)
		return -EINVAL;

	memcpy(notify->values, client->notif_mask, sizeof(notify->values));

	return 0;
}

static int ipc_read_notification_ex(ipc_proc_t *proc, ipc_notify_ex_t *notify)
{
	ipc_client_t *client;
	ipc_channel_t *channel;
	int rval;
	int i, n;

	if ((client = ipc_get_client(proc, notify->cid)) == NULL)
		return -EINVAL;

	channel = client->channel;

	client->num_client_threads++;

	while (1) {
		
		// if closing
		if (client->client_closing) {
			rval = -EPIPE;
			goto Fail;
		}

		// if notification is ready
		n = 0;
		for (i = 0; i < IPC_NUM_NOTIF; i++) {
			if ((notify->values[i] = client->notifications[i]) != 0) {
				client->notifications[i] = 0;
				n++;
			}
		}
		if (n > 0)
			break;

		// if server is alive
		if (channel->channel_closed) {
			ipc_printk("no server");
			rval = -EPIPE;
			goto Fail;
		}

		// otherwise release lock and wait
		rval = wait_for_event_interruptible(&g_ipc_mutex, &client->event_notify);
		
		// if interrupted
		if (rval < 0) {
			//ipc_printk("interrupted");
			goto Fail;
		}
	}

	rval = 0;

Fail:
	client->num_client_threads--;
	event_signal(&client->event_client_closing);

	return rval;
}

// server send notification
static int ipc_write_notify(ipc_proc_t *proc, ipc_notify_t *notify)
{
	ipc_channel_t *channel;
	struct list_head *client_node;

	if ((channel = ipc_get_channel(proc, notify->cid)) == NULL)
		return -EINVAL;

	list_for_each(client_node, &channel->client_list) {
		ipc_client_t *client = __node_to_client(client_node);
		if (((~client->notif_mask[0]) & notify->values) != 0) {
			client->notifications[0] |= notify->values;
			event_signal_all(&client->event_notify);
		}
	}

	return 0;
}

static int ipc_write_notify_ex(ipc_proc_t *proc, ipc_single_notify_t *notify)
{
	ipc_channel_t *channel;
	struct list_head *client_node;
	int i, j;

	if (notify->value >= IPC_NUM_NOTIF * sizeof(unsigned long)) {
		return -EINVAL;
	}

	i = notify->value / sizeof(unsigned long);
	j = 1 << (notify->value % sizeof(unsigned long));

	if ((channel = ipc_get_channel(proc, notify->cid)) == NULL)
		return -EINVAL;

	list_for_each(client_node, &channel->client_list) {
		ipc_client_t *client = __node_to_client(client_node);
		if (((~client->notif_mask[i]) & j) != 0) {
			client->notifications[i] |= j;
			event_signal_all(&client->event_notify);
		}
	}

	return 0;
}

// only for client
static int ipc_set_timeout(ipc_proc_t *proc, ipc_timeout_t *timeout)
{
	ipc_client_t *client;

	if ((client = ipc_get_client(proc, timeout->cid)) == NULL)
		return -EINVAL;

	client->timeout = timeout->value;

	return 0;
}

static int ipc_shutdown(ipc_proc_t *proc, ipc_cid_t cid)
{
	return 0;
}

static inline long do_ipc_ioctl(ipc_proc_t *proc, unsigned int cmd, unsigned long arg)
{
	void __user *ubuf = (void __user *)arg;
	int rval;

	ipc_lock();
/*
	if (proc->closing_file) {
		ipc_printk("the file is being closed but received cmd 0x%x", cmd);
		ipc_unlock();
		return -EPERM;
	}
*/
	switch (cmd) {
	case IPC_REGISTER_SERVICE:
		rval = ipc_process_service(proc, ubuf, ipc_register_service);
		break;

	case IPC_QUERY_SERVICE:
		rval = ipc_process_service(proc, ubuf, ipc_query_service);
		break;

	case IPC_SETUP_CHANNEL:
		rval = ipc_process_write_read(proc, ubuf, ipc_setup_channel);
		break;

	case IPC_CLOSE_CHANNEL:
		rval = ipc_close_channel(proc, (ipc_cid_t)arg);
		break;

	case IPC_SEND_CMD:
		rval = ipc_process_write_read(proc, ubuf, ipc_send_cmd);
		break;

	case IPC_WRITE_READ:
		rval = ipc_process_write_read(proc, ubuf, ipc_write_read);
		break;

	case IPC_READ_NOTIFICATION: {
			ipc_notify_t notify;
			if (copy_from_user(&notify, ubuf, sizeof(notify))) {
				rval = -EFAULT;
			} else {
				rval = ipc_read_notification(proc, &notify);
				if (rval == 0) {
					if (copy_to_user(ubuf, &notify, sizeof(notify)))
						rval = -EFAULT;
				}
			}
		}
		break;

	case IPC_WRITE_NOTIFICATION: {
			ipc_notify_t notify;
			if (copy_from_user(&notify, ubuf, sizeof(notify)))
				rval = -EFAULT;
			else
				rval = ipc_write_notify(proc, &notify);
		}
		break;

	case IPC_SET_TIMEOUT: {
			ipc_timeout_t timeout;
			if (copy_from_user(&timeout, ubuf, sizeof(timeout))) {
				rval = -EFAULT;
			} else {
				rval = ipc_set_timeout(proc, &timeout);
			}
		}
		break;

	case IPC_SHUTDOWN:
		rval = ipc_shutdown(proc, (ipc_cid_t)arg);
		break;

	case IPC_READ_NOTIFICATION_EX: {
			ipc_notify_ex_t notify;
			if (copy_from_user(&notify, ubuf, sizeof(notify))) {
				rval = -EFAULT;
			} else {
				rval = ipc_read_notification_ex(proc, &notify);
				if (rval == 0) {
					if (copy_to_user(ubuf, &notify, sizeof(notify)))
						rval = -EFAULT;
				}
			}
		}
		break;

	case IPC_SET_NOTIFICATION_MASK: {
			ipc_notify_ex_t notify;
			if (copy_from_user(&notify, ubuf, sizeof(notify))) {
				rval = -EFAULT;
			} else {
				rval = ipc_set_notification_mask(proc, &notify);
			}
		}
		break;

	case IPC_GET_NOTIFICATION_MASK: {
			ipc_notify_ex_t notify;
			if (copy_from_user(&notify, ubuf, sizeof(notify))) {
				rval = -EFAULT;
			} else {
				rval = ipc_get_notification_mask(proc, &notify);
				if (rval == 0) {
					if (copy_to_user(ubuf, &notify, sizeof(notify)))
						rval = -EFAULT;
				}
			}
		}
		break;

	case IPC_WRITE_NOTIFICATION_EX: {
			ipc_single_notify_t notify;
			if (copy_from_user(&notify, ubuf, sizeof(notify)))
				rval = -EFAULT;
			else
				rval = ipc_write_notify_ex(proc, &notify);
		}
		break;

	default:
		rval = -ENOIOCTLCMD;
		break;
	}

	ipc_unlock();

	if (rval < 0) {
		ipc_printk("ipc cmd %x returns %d", cmd, rval);
	}

	return rval;
}

// ================================================================================================

// ipc_reg_id_t is address of reg_node_t
typedef struct reg_node_s {
	struct rb_node rb_node_name;	// g_reg_name_tree
	struct rb_node rb_node_id;		// g_reg_id_tree
	struct list_head proc_list;		// p_node_t
	//--------------------------
	unsigned long hash_val;
	int type;
	int index;
	char *name;
	void *data;
	int data_size;
	unsigned long seqnum;
	struct timespec ts_write;
} reg_node_t;

typedef struct p_node_s {
	struct list_head list_reg;	// link to reg_node_t->proc_list
	struct list_head list_proc;	// link to ipc_proc_t->reg_list
	ipc_proc_t *proc;			// belongs to this file
	reg_node_t *rnode;			// points to this node
	int updated;
	int num_read_threads;
	event_t event;
} p_node_t;

static DEFINE_MUTEX(g_reg_mutex);
static int g_num_reg_items = 0;
static struct rb_root g_reg_name_tree = RB_ROOT;
static struct rb_root g_reg_id_tree = RB_ROOT;
static LIST_HEAD(g_reg_readall_list);

static inline void reg_lock(void)
{
	mutex_lock(&g_reg_mutex);
}

static inline void reg_unlock(void)
{
	mutex_unlock(&g_reg_mutex);
}

static reg_node_t *reg_find_by_id(ipc_reg_id_t id)
{
	struct rb_node *n = g_reg_id_tree.rb_node;

	while (n) {
		reg_node_t *rnode = rb_entry(n, reg_node_t, rb_node_id);
		if (id < (ipc_reg_id_t)rnode)
			n = n->rb_left;
		else if (id > (ipc_reg_id_t)rnode)
			n = n->rb_right;
		else
			return rnode;
	}

	return NULL;
}

static inline void reg_insert_by_id(reg_node_t *rnode)
{
	struct rb_node **p = &g_reg_id_tree.rb_node;
	struct rb_node *parent = NULL;
	reg_node_t *tmp;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, reg_node_t, rb_node_id);

		if ((ipc_reg_id_t)rnode < (ipc_reg_id_t)tmp) {
			p = &parent->rb_left;
		} else if ((ipc_reg_id_t)rnode > (ipc_reg_id_t)tmp) {
			p = &parent->rb_right;
		} else {
			// should not happen
			ipc_printk("there's a bug on line %d", __LINE__);
			return;
		}
	}

	rb_link_node(&rnode->rb_node_id, parent, p);
	rb_insert_color(&rnode->rb_node_id, &g_reg_id_tree);
}

static int add_to_all_readall_proc(reg_node_t *rnode);

static inline reg_node_t *reg_find_by_name(const char *name, ipc_query_reg_id_t *param)
{
	unsigned long hash_val = ipc_calc_hash(name) + param->type + param->index;

	struct rb_node **p = &g_reg_name_tree.rb_node;
	struct rb_node *parent = NULL;
	reg_node_t *tmp;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, reg_node_t, rb_node_name);

		if (hash_val < tmp->hash_val) {
			p = &parent->rb_left;
		} else if (hash_val > tmp->hash_val) {
			p = &parent->rb_right;
		} else {
			if (param->type < tmp->type) {
				p = &parent->rb_left;
			} else if (param->type > tmp->type) {
				p = &parent->rb_right;
			} else {
				int r = strcmp(name, tmp->name);
				if (r == 0) {
					// index should be equal
					param->options &= ~REG_ID_OPT_CREATE;
					return tmp;
				}
				p = r < 0 ? &parent->rb_left : &parent->rb_right;
			}
		}
	}

	if (!(param->options & REG_ID_OPT_CREATE))
		return NULL;

	if (g_num_reg_items >= MAX_REG_NUM) {
		ipc_printk("reg exceeds max %d", g_num_reg_items);
		return NULL;
	}

	tmp = ipc_malloc(sizeof(reg_node_t));
	if (tmp == NULL)
		return NULL;

	INIT_LIST_HEAD(&tmp->proc_list);
	tmp->hash_val = hash_val;
	tmp->type = param->type;
	tmp->index = param->index;

	if (param->name_length > 0) {
		tmp->name = ipc_malloc(param->name_length + 1);
		if (tmp->name == NULL) {
			ipc_free(tmp);
			return NULL;
		}
		memcpy(tmp->name, name, param->name_length + 1);
	} else {
		tmp->name = "";
	}

	tmp->data = NULL;
	tmp->data_size = 0;
	tmp->seqnum = 0;
	tmp->ts_write.tv_sec = 0;
	tmp->ts_write.tv_nsec = 0;

	rb_link_node(&tmp->rb_node_name, parent, p);
	rb_insert_color(&tmp->rb_node_name, &g_reg_name_tree);

	reg_insert_by_id(tmp);

	g_num_reg_items++;

	if (!list_empty(&g_reg_readall_list)) {
		add_to_all_readall_proc(tmp);
	}

	return tmp;
}

static inline int reg_id_info(ipc_proc_t *proc, ipc_query_reg_id_t *param)
{
	reg_node_t *rnode;
	int length;

	if (param->name_length <= 0)
		return -EINVAL;

	rnode = reg_find_by_id(param->reg_id);
	if (rnode == NULL)
		return -EINVAL;

	param->type = rnode->type;

	length = strlen(rnode->name) + 1;
	if (length > param->name_length)
		length = param->name_length;

	if (copy_to_user((void*)param->name, rnode->name, length))
		return -EFAULT;

	param->index = rnode->index;
	param->options = 0;

	return 0;
}

// is rnode already added to proc
static inline int rnode_added(ipc_proc_t *proc, reg_node_t *rnode)
{
	struct list_head *lnode;
	list_for_each(lnode, &rnode->proc_list) {
		p_node_t *pnode = list_entry(lnode, p_node_t, list_reg);
		if (pnode->proc == proc) {
			// exists
			return 1;
		}
	}
	return 0;
}

// add rnode to proc
static int add_rnode(ipc_proc_t *proc, reg_node_t *rnode, int options)
{
	p_node_t *pnode = ipc_malloc(sizeof(p_node_t));
	if (pnode == NULL) {
		return -ENOMEM;
	}

	pnode->proc = proc;
	pnode->rnode = rnode;
	pnode->updated = (rnode->data_size > 0) && !(options & REG_ID_OPT_NO_UPDATED);
	pnode->num_read_threads = 0;
	event_init(&pnode->event);

	list_add_tail(&pnode->list_reg, &rnode->proc_list);
	list_add_tail(&pnode->list_proc, &proc->reg_list);

	return 0;
}

static int add_to_all_readall_proc(reg_node_t *rnode)
{
	struct list_head *lnode;
	list_for_each(lnode, &g_reg_readall_list) {
		ipc_proc_t *proc = list_entry(lnode, ipc_proc_t, list_readall);
		int rval = add_rnode(proc, rnode, 0);
		if (rval < 0)
			return rval;
	}
	return 0;
}

static inline int reg_query_id(ipc_proc_t *proc, ipc_query_reg_id_t *param)
{
	char name[MAX_REG_NAME];
	reg_node_t *rnode;
	int options;

	if ((unsigned)param->name_length >= MAX_REG_NAME)
		return -EINVAL;

	if (param->name_length > 0) {
		if (copy_from_user(name, param->name, param->name_length))
			return -EFAULT;

		name[param->name_length] = 0;
		if ((param->name_length = strlen(name)) == 0)
			return -EINVAL;

	} else {
		name[0] = 0;
	}

	options = param->options;	// save

	if ((rnode = reg_find_by_name(name, param)) == NULL) {
		if (options & REG_ID_OPT_CREATE) {
			// cannot create
			return -ENOENT;
		} else {
			// just query if it exists
			param->reg_id = INVALID_IPC_REG_ID;
			return 0;
		}
	}

	param->reg_id = (ipc_reg_id_t)rnode;

	if (!(options & REG_ID_OPT_CREATE) || (options & REG_ID_OPT_WRONLY)) {
		// don't create or create for write-only
		return 0;
	}

	if (rnode_added(proc, rnode))
		return 0;

	// todo - if failed, should remove rnode?
	return add_rnode(proc, rnode, param->options);
}

// add all registry nodes to proc
static inline int reg_add_all(ipc_proc_t *proc, ipc_reg_read_write_t *param)
{
	struct rb_node *node;

	if (!list_empty(&proc->list_readall))	// already in readall list
		return 0;

	// add all reg node to this file
	node = rb_first(&g_reg_id_tree);
	for (; node; node = rb_next(node)) {
		reg_node_t *rnode = rb_entry(node, reg_node_t, rb_node_id);
		if (!rnode_added(proc, rnode)) {
			int rval = add_rnode(proc, rnode, param->options);
			if (rval < 0)
				return rval;
		}
	}

	// add to readall list
	list_add_tail(&proc->list_readall, &g_reg_readall_list);

	return 0;
}

static inline void update_pnode(p_node_t *pnode, ipc_reg_read_write_t *param)
{
	if (!(param->options & REG_OPT_KEEP_UPDATED)) {
		pnode->updated = 0;
	}
}

// user must pass a buffer big enough
static int do_read_reg(reg_node_t *rnode, ipc_reg_read_write_t *param)
{
	if (rnode->data == NULL) {
		param->size = 0;	// nothing to read
	} else {
		if (param->size < rnode->data_size) {
			param->size = rnode->data_size;
			return -ETOOSMALL;	// buffer is too small
		}

		param->size = rnode->data_size;
		if (copy_to_user(param->ptr, rnode->data, rnode->data_size))
			return -EFAULT;
	}

	param->type = rnode->type;
	param->index = rnode->index;
	param->seqnum = rnode->seqnum;
	param->ts_write = rnode->ts_write;
	ktime_get_ts(&param->ts_read);

	return 0;
}

static int reg_read(ipc_proc_t *proc, ipc_reg_read_write_t *param)
{
	reg_node_t *rnode;
	struct list_head *lnode;
	p_node_t *pnode;
	int rval;

	rnode = reg_find_by_id(param->reg_id);
	if (rnode == NULL)
		return -EINVAL;

	if ((param->options & REG_OPT_QUICK_READ) == REG_OPT_QUICK_READ) {
		// quick read: ignore pnode
		return do_read_reg(rnode, param);
	}

	// find my pnode
	list_for_each(lnode, &rnode->proc_list) {
		pnode = list_entry(lnode, p_node_t, list_reg);
		if (pnode->proc == proc) {

			pnode->num_read_threads++;

			for (;;) {
				if (!proc->enabled) {
					rval = -EPIPE;
					goto Fail;
				}

				if (pnode->updated || (param->options & REG_OPT_IGNORE_UPDATED)) {
					// updated, or we accept old data
					if ((rval = do_read_reg(rnode, param)) == 0) {
						update_pnode(pnode, param);
					}
					break;
				}

				rval = wait_for_event_interruptible_timeout(&g_reg_mutex, &pnode->event, get_timeout_value(param->timeout));
				if (rval < 0) {
					goto Fail;
				}
			}

Fail:
			pnode->num_read_threads--;
			event_signal(&proc->event_close);
			return rval;
		}
	}

	return -ENOENT;
}

static inline int reg_read_any(ipc_proc_t *proc, ipc_reg_read_write_t *param)
{
	struct list_head *lnode;
	int rval = 0;

	proc->num_readany_threads++;

	for (;;) {
		if (!proc->enabled) {
			rval = -EPIPE;
			goto Fail;
		}

		if (!(param->options & REG_OPT_READ_ALL) && list_empty(&proc->reg_list)) {
			// only read created ids but the list is empty
			rval = -ENOENT;
			goto Fail;
		}

		// find updated node - may be slow
		list_for_each(lnode, &proc->reg_list) {
			p_node_t *pnode = list_entry(lnode, p_node_t, list_proc);
			if (pnode->updated) {
				param->reg_id = (ipc_reg_id_t)pnode->rnode;
				if ((rval = do_read_reg(pnode->rnode, param)) == 0) {
					update_pnode(pnode, param);
				}
				goto Done;
			}
		}

		rval = wait_for_event_interruptible_timeout(&g_reg_mutex, &proc->event_readany, get_timeout_value(param->timeout));
		if (rval < 0) {
			goto Fail;
		}
	}

Done:
Fail:
	proc->num_readany_threads--;
	event_signal(&proc->event_close);
	return rval;
}

static inline int reg_write(ipc_proc_t *proc, ipc_reg_read_write_t *param)
{
	reg_node_t *rnode;
	struct list_head *lnode;
	void *mem;

	if (param->size <= 0 || param->size > MAX_REG_DATA)
		return -EINVAL;

	if ((rnode = reg_find_by_id(param->reg_id)) == NULL)
		return -EINVAL;

	if ((mem = ipc_malloc(param->size)) == NULL)
		return -ENOMEM;

	if (copy_from_user(mem, param->ptr, param->size)) {
		ipc_free(mem);
		return -EFAULT;
	}

	if (rnode->data) {
		ipc_free(rnode->data);
	}

	rnode->data = mem;
	rnode->data_size = param->size;
	rnode->seqnum++;

	if (param->options & REG_OPT_WRITE_TIMESTAMP) {
		rnode->ts_write = param->ts_write;
	} else {
		ktime_get_ts(&rnode->ts_write);
	}

	// wake up all clients
	list_for_each(lnode, &rnode->proc_list) {
		p_node_t *pnode = list_entry(lnode, p_node_t, list_reg);
		if (!pnode->updated) {
			pnode->updated = 1;
			event_signal_all(&pnode->event);
			event_signal_all(&pnode->proc->event_readany);
		}
	}

	return 0;
}

// free node data and reset its state
static inline int reg_reset(ipc_proc_t *proc, ipc_reg_action_t *param)
{
	reg_node_t *rnode;

	rnode = reg_find_by_id(param->reg_id);
	if (rnode == NULL)
		return -EINVAL;

	if (rnode->data) {
		ipc_free(rnode->data);
		rnode->data = NULL;
	}

	rnode->data_size = 0;
	rnode->seqnum = 0;
	rnode->ts_write.tv_sec = 0;
	rnode->ts_write.tv_nsec = 0;

	return 0;
}

// remove the client node only
static inline int reg_remove(ipc_proc_t *proc, ipc_reg_action_t *param)
{
	struct list_head *lnode;

	list_for_each(lnode, &proc->reg_list) {
		p_node_t *pnode = list_entry(lnode, p_node_t, list_proc);
		if (param->reg_id == (ipc_reg_id_t)pnode->rnode) {
			if (pnode->num_read_threads > 0) {
				// todo - notify the threads to exit ?
				ipc_printk("num_thread=%d", pnode->num_read_threads);
				return -EPERM;
			}

			__list_del_entry(&pnode->list_reg);
			__list_del_entry(&pnode->list_proc);
			ipc_free(pnode);

			if (proc->num_readany_threads > 0 && list_empty(&proc->reg_list)) {
				// notify the threads that list is empty
				event_signal_all(&proc->event_readany);
			}

			return 0;
		}
	}

	return -ENOENT;
}

static int is_string(char *ptr, int size)
{
	for (; size > 1; size--, ptr++) {
		if (!isprint(*ptr))
			return 0;
	}
	return *ptr == 0;
}

static void reg_print_rnode(ipc_proc_t *proc, reg_node_t *rnode, int index)
{
	printk("%d: reg[%d, %s, %d] { value: ",
		index, rnode->type, rnode->name, rnode->index);

	if (rnode->data && rnode->data_size > 0) {
		if (is_string(rnode->data, rnode->data_size)) {
			printk("%s", (char*)rnode->data);
		} else {
			// todo
			printk("[%d bytes]", rnode->data_size);
		}
	} else {
		printk("[empty]");
	}

	printk(", size: %d, seqnum: %lu, ts_write: %ld.%06d }\n",
		rnode->data_size, rnode->seqnum,
		rnode->ts_write.tv_sec, (int)(rnode->ts_write.tv_nsec/1000));

	if (!list_empty(&rnode->proc_list)) {
		struct list_head *lnode;
		list_for_each(lnode, &rnode->proc_list) {
			p_node_t *pnode = list_entry(lnode, p_node_t, list_reg);
			printk("    %s { updated: %d, num_read_threads: %d }\n",
				pnode->proc->task->comm, pnode->updated, pnode->num_read_threads);
		}
	}
}

static int reg_print(ipc_proc_t *proc, ipc_reg_action_t *param)
{
	if (param->reg_id == INVALID_IPC_REG_ID) {
		struct rb_node *node = rb_first(&g_reg_id_tree);
		if (node == NULL) {
			ipc_printk("registry is empty");
		} else {
			int index = 0;
			printk("\n");
			for (; node; node = rb_next(node), index++) {
				reg_node_t *rnode = rb_entry(node, reg_node_t, rb_node_id);
				reg_print_rnode(proc, rnode, index);
			}
			printk("%d blocks of memory allocated\n", atomic_read(&g_total_alloc));
			printk("\n");
		}
	} else {
		reg_node_t *rnode = reg_find_by_id(param->reg_id);
		if (rnode == NULL) {
			ipc_printk("reg_id not found");
			return -EINVAL;
		}
		reg_print_rnode(proc, rnode, 0);
	}
	return 0;
}

static inline int reg_get_all(ipc_proc_t *proc, ipc_reg_get_all_t *param)
{
	int remain = param->num_entries;

	ipc_reg_id_item_t *u_item = param->items;
	ipc_reg_id_item_t item;
	struct rb_node *node;

	param->total = 0;

	node = rb_first(&g_reg_id_tree);
	for (; node; node = rb_next(node)) {
		param->total++;
		if (remain > 0) {
			reg_node_t *rnode = rb_entry(node, reg_node_t, rb_node_id);

			item.type = rnode->type;
			item.reg_id = (ipc_reg_id_t)rnode;

			if (copy_to_user(u_item, &item, sizeof(item)))
				return -EFAULT;

			u_item++;
			remain--;
		}
	}

	return 0;
}

static void reg_notify_all_readers(ipc_proc_t *proc, int wait);

static inline int reg_enable(ipc_proc_t *proc, int enable)
{
	proc->enabled = enable;
	if (!enable) {
		// notify but don't wait
		reg_notify_all_readers(proc, 0);
	}
	return 0;
}

static inline long do_reg_ioctl(ipc_proc_t *proc, unsigned int cmd, unsigned long arg)
{
	void __user *ubuf = (void __user *)arg;
	int rval;

	reg_lock();

/*
	if (!proc->enabled) {
		ipc_printk("reg file is being closed but received cmd 0x%x", cmd);
		reg_unlock();
	}
*/

	switch (_IOC_NR(cmd)) {
	case _REG_QUERY_ID: {
			ipc_query_reg_id_t param;
			if (copy_from_user(&param, ubuf, sizeof(param))) {
				rval = -EFAULT;
			} else {
				if ((rval = reg_query_id(proc, &param)) == 0) {
					if (copy_to_user(ubuf, &param, sizeof(param)))
						rval = -EFAULT;
				}
			}
		}
		break;

	case _REG_REMOVE_ID: {
			ipc_reg_action_t param;
			if (copy_from_user(&param, ubuf, sizeof(param))) {
				rval = -EFAULT;
			} else {
				rval = reg_remove(proc, &param);
			}
		}
		break;

	case _REG_READ: {
			ipc_reg_read_write_t param;
			if (copy_from_user(&param, ubuf, sizeof(param))) {
				rval = -EFAULT;
			} else {
				if ((rval = reg_read(proc, &param)) == 0) {
					if (copy_to_user(ubuf, &param, sizeof(param)))
						rval = -EFAULT;
				}
			}
		}
		break;

	case _REG_WRITE: {
			ipc_reg_read_write_t param;
			if (copy_from_user(&param, ubuf, sizeof(param))) {
				rval = -EFAULT;
			} else {
				rval = reg_write(proc, &param);
			}
		}
		break;

	case _REG_RESET: {
			ipc_reg_action_t param;
			if (copy_from_user(&param, ubuf, sizeof(param))) {
				rval = -EFAULT;
			} else {
				rval = reg_reset(proc, &param);
			}
		}
		break;

	case _REG_READ_ANY: {
			ipc_reg_read_write_t param;
			if (copy_from_user(&param, ubuf, sizeof(param))) {
				rval = -EFAULT;
			} else {
				if (param.options & REG_OPT_REMOVE_READALL) {
					// remove from readall list
					list_del_init(&proc->list_readall);
					rval = 0;
				} else if (param.options & REG_OPT_READ_ALL) {
					if ((rval = reg_add_all(proc, &param)) < 0)
						break;
					if (param.options & REG_OPT_DONT_READ)
						break;
				}
				if ((rval = reg_read_any(proc, &param)) == 0) {
					if (copy_to_user(ubuf, &param, sizeof(param)))
						rval = -EFAULT;
				}
			}
		}
		break;

	case _REG_ID_INFO: {
			ipc_query_reg_id_t param;
			if (copy_from_user(&param, ubuf, sizeof(param))) {
				rval = -EFAULT;
			} else {
				if ((rval = reg_id_info(proc, &param)) == 0) {
					if (copy_to_user(ubuf, &param, sizeof(param)))
						rval = -EFAULT;
				}
			}
		}
		break;

	case _REG_DMESG: {
			ipc_reg_action_t param;
			if (copy_from_user(&param, ubuf, sizeof(param))) {
				rval = -EFAULT;
			} else {
				rval = reg_print(proc, &param);
			}
		}
		break;

	case _REG_GET_ALL: {
			ipc_reg_get_all_t param;
			if (copy_from_user(&param, ubuf, sizeof(param))) {
				rval = -EFAULT;
			} else {
				if ((rval = reg_get_all(proc, &param)) == 0) {
					if (copy_to_user(ubuf, &param, sizeof(param)))
						rval = -EFAULT;
				}
			}
		}
		break;

	case _REG_ENABLE: {
			int32_t enable;
			if (get_user(enable, (__user int32_t*)arg)) {
				rval = -EFAULT;
			} else {
				rval = reg_enable(proc, enable);
			}
		}
		break;

	default:
		rval = -ENOIOCTLCMD;
		break;
	}

	reg_unlock();

	if (rval < 0) {
		ipc_printk("reg cmd %x (nr: %d) returns %d", cmd, _IOC_NR(cmd), rval);
	}

	return rval;
}

static void reg_notify_all_readers(ipc_proc_t *proc, int wait)
{
	struct list_head *node;

	// force all read threads to exit
	list_for_each(node, &proc->reg_list) {
		p_node_t *pnode = list_entry(node, p_node_t, list_proc);
		if (pnode->num_read_threads > 0) {
			event_signal_all(&pnode->event);
			if (wait) {
				while (pnode->num_read_threads > 0) {
					wait_for_event(&g_reg_mutex, &proc->event_close);
				}
			}
		}
	}

	// threads doing reg_read_any()
	if (proc->num_readany_threads > 0) {
		event_signal_all(&proc->event_readany);
		if (wait) {
			while (proc->num_readany_threads > 0) {
				wait_for_event(&g_reg_mutex, &proc->event_close);
			}
		}
	}
}

// before closing the file
static inline void reg_free_all(ipc_proc_t *proc)
{
	struct list_head *node;
	struct list_head *tmp;

	reg_lock();

	proc->enabled = 0;
	reg_notify_all_readers(proc, 1);

	// free all nodes
	list_for_each_safe(node, tmp, &proc->reg_list) {
		p_node_t *pnode = list_entry(node, p_node_t, list_proc);

		BUG_ON(pnode->num_read_threads != 0);
		__list_del_entry(node);
		__list_del_entry(&pnode->list_reg);	// remove from reg list

		ipc_free(pnode);
	}
	BUG_ON(proc->num_readany_threads != 0);

	// remove from readall list
	list_del_init(&proc->list_readall);

	reg_unlock();
}

// ================================================================================================

static long ipc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	ipc_proc_t *proc = filp->private_data;

	switch (_IOC_TYPE(cmd)) {
	case IPC_MAGIC:
		return do_ipc_ioctl(proc, cmd, arg);

	case REG_MAGIC:
		return do_reg_ioctl(proc, cmd, arg);

	default:
		ipc_printk("cmd not handled, type %d", _IOC_TYPE(cmd));
		return -ENOIOCTLCMD;
	}
}

static int ipc_open(struct inode *nodp, struct file *filp)
{
	ipc_proc_t *proc;

	if ((proc = ipc_zalloc(sizeof(*proc))) == NULL)
		return -ENOMEM;

	proc->pid = current->group_leader->pid;
	proc->closing_file = 0;
	filp->private_data = proc;

	ipc_lock();
	list_add_tail(&proc->list_procs, &g_all_procs);
	ipc_unlock();

	proc->task = current;
	proc->enabled = 1;
	proc->num_readany_threads = 0;

	event_init(&proc->event_close);
	event_init(&proc->event_readany);

	INIT_LIST_HEAD(&proc->reg_list);
	INIT_LIST_HEAD(&proc->list_readall);

	return 0;
}

static int ipc_release(struct inode *nodp, struct file *filp)
{
	ipc_proc_t *proc = filp->private_data;
	int i;

	ipc_lock();

	proc->closing_file = 1;
	for (i = 0; i < IPC_MAX_CHANNEL; i++) {
		if (proc->channels[i]) {
			ipc_close_channel(proc, i + 1);
		}
	}

	list_del(&proc->list_procs);

	ipc_unlock();

	reg_free_all(proc);

	ipc_free(proc);

	return 0;
}

static const struct file_operations ipc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ipc_ioctl,
	.open = ipc_open,
	.release = ipc_release,
};

static struct miscdevice ipc_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ipc",
	.fops = &ipc_fops,
};


#define IPC_DEBUG_ENTRY(name) \
static int ipc_##name##_show(struct seq_file *, void *); \
static int ipc_##name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, ipc_##name##_show, inode->i_private); \
} \
static const struct file_operations ipc_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = ipc_##name##_open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

static struct dentry *g_ipc_dir_entry_root;
IPC_DEBUG_ENTRY(procs);

static void ipc_print_channel(struct seq_file *m, ipc_channel_t *channel)
{
}

static void ipc_print_client(struct seq_file *m, ipc_client_t *client)
{
}

static int ipc_procs_show(struct seq_file *m, void *unused)
{
	struct list_head *node;

	ipc_lock();

	list_for_each(node, &g_all_procs) {
		ipc_proc_t *proc = __list_to_proc(node);
		int i;

		for (i = 0; i < IPC_MAX_CHANNEL; i++) {
			ipc_channel_t *channel = proc->channels[i];
			if (channel) {
				if (channel->is_channel)
					ipc_print_channel(m, channel);
				else
					ipc_print_client(m, (ipc_client_t*)channel);
			}
		}
	}

	ipc_unlock();

	return 0;
}

static int __init ipc_init(void)
{
	int rval;

	if ((rval = misc_register(&ipc_miscdev)) < 0)
		return rval;

	g_ipc_dir_entry_root = debugfs_create_dir("ipc", NULL);
	if (g_ipc_dir_entry_root) {
		debugfs_create_file("procs", S_IRUGO, g_ipc_dir_entry_root,
			NULL, &ipc_procs_fops);
	}

	return 0;
}

static void __exit ipc_exit(void)
{
	if (g_ipc_dir_entry_root) {
		debugfs_remove_recursive(g_ipc_dir_entry_root);
	}
	misc_deregister(&ipc_miscdev);
}

module_init(ipc_init);
module_exit(ipc_exit);

MODULE_LICENSE("GPL v2");

