
#ifndef __AVF_MALLOC_H__
#define __AVF_MALLOC_H__

#if !defined(IOS_OS) && !defined(MAC_OS)

#include "rbtree.h"

class CDefaultMalloc;
class CMmapMalloc;
class CRingMalloc;

void *mmap_AllocMem(avf_size_t size);
void mmap_FreeMem(void *ptr, avf_size_t size);

//-----------------------------------------------------------------------
//
// CDefaultMalloc
//
//-----------------------------------------------------------------------
class CDefaultMalloc: public CObject, public IAllocMem
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

public:
	static CDefaultMalloc *Create();

protected:
	CDefaultMalloc();
	virtual ~CDefaultMalloc();

// IAllocMem
public:
	virtual void *AllocMem(avf_size_t size);
	virtual void FreeMem(void *ptr, avf_size_t size);
};

//-----------------------------------------------------------------------
//
// CMmapMalloc
//
//-----------------------------------------------------------------------
class CMmapMalloc: public CDefaultMalloc
{
	typedef CDefaultMalloc inherited;

public:
	static CMmapMalloc *Create();

private:
	CMmapMalloc();
	virtual ~CMmapMalloc();

// IAllocMem
public:
	virtual void *AllocMem(avf_size_t size);
	virtual void FreeMem(void *ptr, avf_size_t size);
};

//-----------------------------------------------------------------------
//
// CRingMalloc: ring buffer memory allocator using mmap/munmap
//
//-----------------------------------------------------------------------
class CRingMalloc: public CDefaultMalloc
{
	typedef CDefaultMalloc inherited;

public:
	enum {
		MAX_NODE_CACHE = 256,
	};

public:
	static CRingMalloc *Create(int tag, avf_size_t mem_max);

private:
	CRingMalloc(int tag, avf_size_t mem_max);
	virtual ~CRingMalloc();

// IAllocMem
public:
	virtual void *AllocMem(avf_size_t size);
	virtual void FreeMem(void *ptr, avf_size_t size);

private:
	struct node_s {
		struct rb_node rbnode;
		avf_u8_t *ptr;
		avf_size_t size;

		static INLINE node_s *from_node(struct rb_node *node) {
			return container_of(node, node_s, rbnode);
		}

		INLINE node_s *get_next() {
			struct rb_node *next = rb_next(&rbnode);
			return next ? from_node(next) : NULL;
		}
	};

private:
	node_s *GetNodeAfter(avf_u8_t *ptr);
	void *Insert(avf_u8_t *ptr, avf_size_t size);
	bool ExpandMem(avf_size_t inc_size);
	void *AllocFromRange(avf_size_t size, avf_u8_t *end_ptr);

private:
	CMutex mMutex;
	int mTag;
	avf_size_t m_mem_max;
	struct rb_root allocated_set;

	node_s *m_free_node;
	avf_size_t m_num_free_node;

	avf_u8_t *map_base;
	avf_size_t mapped_size;
	avf_size_t unmapped_size;
	avf_u8_t *malloc_ptr;
	avf_size_t alloc_size;
	avf_size_t alloc_count;

#ifdef AVF_DEBUG
	// for debug
	avf_u64_t last_tick;
	avf_size_t max_mapped_size;
	avf_size_t max_alloc_size;
	avf_size_t max_alloc_size_0;
	avf_size_t max_alloc_count;
#endif
};

#endif

#endif

