
#define LOG_TAG "avf_malloc"

#include <sys/mman.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_malloc.h"

#define MAP_PAGE_SIZE	(4096)

static avf_u8_t *my_mmap(avf_size_t size, int prot, void *ptr = NULL)
{
	int flags = MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED|MAP_POPULATE;
	if (ptr) flags |= MAP_FIXED;
	void *result = ::mmap(ptr, size, prot, flags, -1, 0);
	if (result == MAP_FAILED || (ptr && result != ptr)) {
		AVF_LOGP("mmap() failed, size=%d, prot=0x%x, ptr=%p, result=%p", size, prot, ptr, result);
		*(int*)0=0;
		AVF_DIE;
	}
	return (avf_u8_t*)result;
}

void *mmap_AllocMem(avf_size_t size)
{
	size = ROUND_UP(size, MAP_PAGE_SIZE);
	return my_mmap(size, PROT_READ|PROT_WRITE);
}

void mmap_FreeMem(void *ptr, avf_size_t size)
{
	if (ptr) {
		size = ROUND_UP(size, MAP_PAGE_SIZE);
		int result = ::munmap(ptr, size);
		if (result < 0) {
			AVF_LOGE("munmap failed");
		}
	}
}

//-----------------------------------------------------------------------
//
// CDefaultMalloc
//
//-----------------------------------------------------------------------
CDefaultMalloc *CDefaultMalloc::Create()
{
	CDefaultMalloc *result = avf_new CDefaultMalloc();
	CHECK_STATUS(result);
	return result;
}

CDefaultMalloc::CDefaultMalloc()
{
}

CDefaultMalloc::~CDefaultMalloc()
{
}

void *CDefaultMalloc::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IAllocMem)
		return static_cast<IAllocMem*>(this);
	return inherited::GetInterface(refiid);
}

void *CDefaultMalloc::AllocMem(avf_size_t size)
{
	return avf_malloc(size);
}

void CDefaultMalloc::FreeMem(void *ptr, avf_size_t size)
{
	avf_free(ptr);
}

//-----------------------------------------------------------------------
//
// CMmapMalloc
//
//-----------------------------------------------------------------------
CMmapMalloc *CMmapMalloc::Create()
{
	CMmapMalloc *result = avf_new CMmapMalloc();
	CHECK_STATUS(result);
	return result;
}

CMmapMalloc::CMmapMalloc()
{
}

CMmapMalloc::~CMmapMalloc()
{
}

void *CMmapMalloc::AllocMem(avf_size_t size)
{
	return mmap_AllocMem(size);
}

void CMmapMalloc::FreeMem(void *ptr, avf_size_t size)
{
	mmap_FreeMem(ptr, size);
}

//-----------------------------------------------------------------------
//
// CRingMalloc
//
//-----------------------------------------------------------------------
CRingMalloc *CRingMalloc::Create(int tag, avf_size_t mem_max)
{
	CRingMalloc *result = avf_new CRingMalloc(tag, mem_max);
	CHECK_STATUS(result);
	return result;
}

CRingMalloc::CRingMalloc(int tag, avf_size_t mem_max)
{
	mTag = tag;
	m_mem_max = mem_max;
	allocated_set.rb_node = NULL;

	m_free_node = NULL;
	m_num_free_node = 0;

	map_base = my_mmap(m_mem_max, PROT_NONE);
	mapped_size = 0;
	unmapped_size = m_mem_max;
	malloc_ptr = map_base;
	alloc_size = 0;
	alloc_count = 0;

	CHECK_OBJ(map_base);

//	AVF_LOGD(C_CYAN "mmap[%d]: %p - %p" C_NONE,
//		mTag, map_base, map_base + m_mem_max);

#ifdef AVF_DEBUG
	last_tick = 0;
	max_mapped_size = 0;
	max_alloc_size = 0;
	max_alloc_size_0 = 0;
	max_alloc_count = 0;
#endif
}

CRingMalloc::~CRingMalloc()
{
	if (alloc_count || alloc_size || allocated_set.rb_node) {
		// all memory should be freed at this point
		AVF_DIE;
	}
	mmap_FreeMem(map_base, m_mem_max);
	node_s *node = m_free_node;
	while (node) {
		node_s *next = (node_s*)node->ptr;
		avf_free(node);
		node = next;
	}
}

// find the nearest node that's after ptr
CRingMalloc::node_s *CRingMalloc::GetNodeAfter(avf_u8_t *ptr)
{
	struct rb_node *n = allocated_set.rb_node;
	node_s *min_ge = NULL;

	while (n) {
		node_s *node = node_s::from_node(n);
		if (ptr <= node->ptr) {
			min_ge = node;
			n = n->rb_left;
		} else {
			n = n->rb_right;
		}
	}

	return min_ge;
}

// insert a node that describes the memory block in use
void *CRingMalloc::Insert(avf_u8_t *ptr, avf_size_t size)
{
	struct rb_node **p = &allocated_set.rb_node;
	struct rb_node *parent = NULL;
	node_s *tmp;

	while (*p) {
		parent = *p;
		tmp = node_s::from_node(parent);

		if (ptr < tmp->ptr) {
			p = &parent->rb_left;
		} else if (ptr > tmp->ptr) {
			p = &parent->rb_right;
		} else {
			AVF_DIE;
		}
	}

	if (m_free_node) {
		tmp = m_free_node;
		m_free_node = (node_s*)m_free_node->ptr;
		m_num_free_node--;
	} else {
		tmp = avf_malloc_type(node_s);
	}

	tmp->ptr = ptr;
	tmp->size = size;

	rb_link_node(&tmp->rbnode, parent, p);
	rb_insert_color(&tmp->rbnode, &allocated_set);

	alloc_size += size;
	alloc_count++;

#ifdef AVF_DEBUG

	if (alloc_size > max_alloc_size) {
		max_alloc_size = alloc_size;
	}
	if (alloc_size > max_alloc_size_0) {
		max_alloc_size_0 = alloc_size;
	}
	if (alloc_count > max_alloc_count) {
		max_alloc_count = alloc_count;
	}

	avf_u64_t tick = avf_get_curr_tick();
	if (last_tick == 0) {
		last_tick = tick;
	} else {
		if (tick - last_tick >= 30*1000) {
			last_tick = tick;
			AVF_LOGV(C_LIGHT_GRAY "mmap[%d]: %d/%d/%d/%d KB, count: %d/%d" C_NONE, mTag, 
				alloc_size>>10, max_alloc_size>>10, max_alloc_size_0>>10, max_mapped_size>>10,
				alloc_count, max_alloc_count);
			max_alloc_size_0 = 0;
		}
	}
#endif

	malloc_ptr = ptr + size;
	return ptr;
}

bool CRingMalloc::ExpandMem(avf_size_t inc_size)
{
	if (mapped_size + inc_size > m_mem_max) {
		AVF_LOGE("exceeds max mem %d, maped_size: %d, inc_size: %d",
			m_mem_max, mapped_size, inc_size);
		return false;
		//AVF_DIE;
	}

	avf_u8_t *ptr = map_base + mapped_size;
	my_mmap(inc_size, PROT_READ|PROT_WRITE, ptr);

	mapped_size += inc_size;
	unmapped_size -= inc_size;

#ifdef AVF_DEBUG
	if (mapped_size > max_mapped_size) {
		max_mapped_size = mapped_size;
	}
#endif

	return true;
}

void *CRingMalloc::AllocFromRange(avf_size_t size, avf_u8_t *end_ptr)
{
	avf_size_t avail_size;
	node_s *node = GetNodeAfter(malloc_ptr);
	while (true) {
		if (node == NULL) {
			avail_size = mapped_size - (malloc_ptr - map_base);
			if (avail_size >= size)
				return Insert(malloc_ptr, size);
			return NULL;
		}

		avail_size = node->ptr - malloc_ptr;
		if (avail_size >= size)
			return Insert(malloc_ptr, size);

		malloc_ptr = node->ptr + node->size;
		if (malloc_ptr >= end_ptr)
			return NULL;

		node = node->get_next();
	}
}

void *CRingMalloc::AllocMem(avf_size_t size)
{
	AUTO_LOCK(mMutex);

	size = ROUND_UP(size, MAP_PAGE_SIZE);
	void *result;

	avf_u8_t *orig_ptr = malloc_ptr;
	if ((result = AllocFromRange(size, map_base + mapped_size)) != NULL)
		return result;

	avf_u8_t *tail_ptr = malloc_ptr;
	malloc_ptr = map_base;

	if ((result = AllocFromRange(size, orig_ptr)) != NULL)
		return result;

	// have to expand
	malloc_ptr = tail_ptr;
	avf_size_t avail_size = mapped_size - (malloc_ptr - map_base);
	if (!ExpandMem(size - avail_size))
		return NULL;

	return Insert(malloc_ptr, size);
}

void CRingMalloc::FreeMem(void *ptr, avf_size_t size)
{
	AUTO_LOCK(mMutex);

	size = ROUND_UP(size, MAP_PAGE_SIZE);
	node_s *node = GetNodeAfter((avf_u8_t*)ptr);
	if (node == NULL || node->ptr != ptr || node->size != size) {
		// must exactly match, otherwise it is a bad free
		AVF_DIE;
		return;
	}

	avf_u8_t *last_ptr = node->ptr;
	bool is_last = node->get_next() == NULL;

	alloc_size -= size;
	alloc_count--;

	rb_erase(&node->rbnode, &allocated_set);

	if (m_num_free_node >= MAX_NODE_CACHE) {
		avf_free(node);
	} else {
		// cache for later use
		node->ptr = (avf_u8_t*)m_free_node;
		m_free_node = node;
		m_num_free_node++;
	}

	avf_size_t reserved_mem = m_mem_max / 8;

	// release memory if too much
	if (is_last && mapped_size > reserved_mem) {
		avf_size_t dec_size = mapped_size - (last_ptr - map_base);
		avf_size_t can_free = mapped_size - reserved_mem;
		if (dec_size > can_free)
			dec_size = can_free;

		avf_u8_t *release_ptr = map_base + (mapped_size - dec_size);

		//AVF_LOGI(C_WHITE "shrink %d" C_NONE, dec_size);
		my_mmap(dec_size, PROT_NONE, release_ptr);

		mapped_size -= dec_size;
		unmapped_size += dec_size;

		if (malloc_ptr > last_ptr) {
			malloc_ptr = last_ptr;
		}
	}
}

