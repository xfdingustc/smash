
#define LOG_TAG "avf_new"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_util.h"

#ifdef MEM_DEBUG
#include "rbtree.h"
#include <malloc.h>
#endif

//-----------------------------------------------------------------------
//
// for debug
//
//-----------------------------------------------------------------------

#ifdef MEM_DEBUG

extern "C" void *__libc_malloc(size_t);
extern "C" void *__libc_free(void*);
extern "C" void *__libc_calloc(size_t, size_t);
extern "C" void *__libc_memalign(size_t, size_t);
extern "C" void *__libc_realloc(void*, size_t);
extern "C" void *__libc_valloc(size_t);
extern "C" void *__libc_pvalloc(size_t);
extern "C" void __libc_freeres(void);

enum {
	MEM_MALLOC = 1,
	MEM_CALLOC,
	MEM_ALIGN,
	MEM_REALLOC,
	MEM_VALLOC,
	MEM_PVALLOC
};

typedef struct mem_node_s {
	struct rb_node node;	// must be first
	size_t size;
	char type;	// malloc
	char checked;
	void *ptr;
	const void *caller;
} mem_node_t;

#define CALLER	__builtin_return_address(0)

static CMutex g_mem_lock;
static struct rb_root g_mem_tree;
static int g_alloc_bytes;
static int g_peak_alloc_bytes;
static int g_alloc_count;
static int g_peak_alloc_count;
static int g_nr_allocs;
static int g_nr_frees;

void insert_mem_node(mem_node_t *mnode)
{
	struct rb_node **p = &g_mem_tree.rb_node;
	struct rb_node *parent = NULL;
	mem_node_t *tmp;

	while (*p) {
		parent = *p;
		tmp = (mem_node_t*)parent;

		if (mnode->ptr < tmp->ptr) {
			p = &parent->rb_left;
		} else if (mnode->ptr > tmp->ptr) {
			p = &parent->rb_right;
		} else {
			AVF_DIE;
		}
	}

	rb_link_node(&mnode->node, parent, p);
	rb_insert_color(&mnode->node, &g_mem_tree);
}

static void add_record(size_t size, void *ptr, int type, const void *caller)
{
	AUTO_LOCK(g_mem_lock);

	mem_node_t *mnode;

	mnode = (mem_node_t*)__libc_malloc(sizeof(mem_node_t));
	mnode->size = size;
	mnode->type = type;
	mnode->checked = 0;
	mnode->ptr = ptr;
	mnode->caller = caller;

	insert_mem_node(mnode);

	g_alloc_count++;
	if (g_alloc_count > g_peak_alloc_count)
		g_peak_alloc_count = g_alloc_count;

	g_alloc_bytes += size;
	if (g_alloc_bytes > g_peak_alloc_bytes)
		g_peak_alloc_bytes = g_alloc_bytes;

	g_nr_allocs++;
}

static mem_node_t *find_mem_node(void *ptr)
{
	struct rb_node *n = g_mem_tree.rb_node;

	while (n) {
		mem_node_t *mnode = (mem_node_t*)n;

		if (ptr < mnode->ptr)
			n = n->rb_left;
		else if (ptr > mnode->ptr)
			n = n->rb_right;
		else
			return mnode;
	}

	return NULL;
}

static void remove_record(void *ptr, int keep)
{
	AUTO_LOCK(g_mem_lock);

	mem_node_t *mnode;

	mnode = find_mem_node(ptr);
	if (mnode == NULL) {
		AVF_LOGE("free memory bad pointer %p", ptr);
		AVF_DIE;
		return;
	}

	g_alloc_count--;
	g_alloc_bytes -= mnode->size;
	g_nr_frees++;

	if (!keep) {
		::memset(ptr, 0xEF, mnode->size);
	}

	rb_erase(&mnode->node, &g_mem_tree);
	__libc_free(mnode);
}

#endif

#ifdef MEM_DEBUG
void print_mem_tree(void)
{
	struct rb_node *n;
	mem_node_t *mnode;

	if (g_mem_tree.rb_node == NULL) {
		AVF_LOGI(C_YELLOW "no memory leak (%d)" C_NONE, g_alloc_count);
	} else {
		AVF_LOGI("memory not freed:");

		int i = 0;
		for (n = rb_first(&g_mem_tree); n; n = rb_next(n), i++) {
			mnode = (mem_node_t*)n;
			AVF_LOGI(C_LIGHT_PURPLE "%d [0x%x]" C_NONE ": size " C_LIGHT_PURPLE FMT_ZD C_NONE " type %d, ptr %p",
				i, mnode->caller, mnode->size, mnode->type, mnode->ptr);
			avf_dump_mem(mnode->ptr, MIN(mnode->size, 32));
		}
	}
}
#endif

extern "C" void avf_mem_info(int freeres)
{
#ifdef MEM_DEBUG

	if (freeres) {
		__libc_freeres();
	}

//	AUTO_LOCK(g_mem_lock); - deadlock

	AVF_LOGI(C_LIGHT_GRAY "count: %d/%d, bytes: %d/%d, allocs: %d, frees: %d" C_NONE,
		g_alloc_count, g_peak_alloc_count,
		g_alloc_bytes, g_peak_alloc_bytes,
		g_nr_allocs, g_nr_frees);

	// MinGW does not support
#ifndef WIN32_OS
	struct mallinfo mi = mallinfo();
	AVF_LOGI("mallinfo: %d bytes", mi.uordblks);
#endif

	print_mem_tree();

#endif
}

//-----------------------------------------------------------------------
//
// libc
//
//-----------------------------------------------------------------------

#ifdef MEM_DEBUG

extern "C" void *malloc(size_t size)
{
//	printf("wrap malloc %d\n", size);
	void *ptr = __libc_malloc(size);
	if (ptr) {
		add_record(size, ptr, MEM_MALLOC, CALLER);
	}
	return ptr;
}

extern "C" void free(void *ptr)
{
//	printf("wrap free\n");
	if (ptr) {
		remove_record(ptr, 0);
	}
	__libc_free(ptr);
}

extern "C" void cfree(void *ptr)
{
	printf("wrap cfree\n");
	if (ptr) {
		remove_record(ptr, 0);
	}
	__libc_free(ptr);
}

extern "C" void *calloc(size_t n, size_t elem_size)
{
//	printf("calloc %d %d\n", n, elem_size);
	void *ptr = __libc_calloc(n, elem_size);
	if (ptr) {
		add_record(n * elem_size, ptr, MEM_CALLOC, CALLER);
	}
	return ptr;
}

extern "C" void *memalign(size_t alignment, size_t bytes)
{
//	printf("memalign %d %d\n", alignment, bytes);
	void *ptr = __libc_memalign(alignment, bytes);
	if (ptr) {
		add_record(bytes, ptr, MEM_ALIGN, CALLER);
	}
	return ptr;
}

extern "C" void *realloc(void *oldmem, size_t bytes)
{
//	printf("realloc %d\n", bytes);
	if (oldmem) {
		remove_record(oldmem, 1);
	}
	void *ptr = __libc_realloc(oldmem, bytes);
	if (ptr) {
		add_record(bytes, ptr, MEM_REALLOC, CALLER);
	}
	return ptr;
}

extern "C" void *valloc(size_t bytes)
{
//	printf("valloc %d\n", bytes);
	void *ptr = __libc_valloc(bytes);
	if (ptr) {
		add_record(bytes, ptr, MEM_VALLOC, CALLER);
	}
	return ptr;
}

extern "C" void *pvalloc(size_t bytes)
{
//	printf("pvalloc %d\n", bytes);
	void *ptr = __libc_pvalloc(bytes);
	if (ptr) {
		add_record(bytes, ptr, MEM_PVALLOC, CALLER);
	}
	return ptr;
}

#endif

