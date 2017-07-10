
#ifndef __AVF_REGISTRY_H__
#define __AVF_REGISTRY_H__

#include "rbtree.h"

//class CRegistry;
class CRegistry2;
class CConfig;

#if 0
//-----------------------------------------------------------------------
//
//  CRegistry
//
//-----------------------------------------------------------------------
class CRegistry: public CObject, public IRegistry
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

	enum {
		ALIGN_SIZE = 8,
	};

public:
	static CRegistry* Create(IRegistry *pGlobalRegistry);

private:
	CRegistry(IRegistry *pGlobalRegistry);
	virtual ~CRegistry();

// IRegistry
public:
	virtual avf_s32_t ReadRegInt32(const char *name, avf_u32_t index, avf_s32_t default_value);
	virtual avf_s64_t ReadRegInt64(const char *name, avf_u32_t index, avf_s64_t default_value);
	virtual avf_int_t ReadRegBool(const char *name, avf_u32_t index, avf_int_t default_value);
	virtual void ReadRegString(const char *name, avf_u32_t index, char *value, const char *default_value);
	virtual void ReadRegMem(const char *name, avf_u32_t index, avf_u8_t *mem, avf_size_t size);

	virtual avf_status_t WriteRegInt32(const char *name, avf_u32_t index, avf_s32_t value);
	virtual avf_status_t WriteRegInt64(const char *name, avf_u32_t index, avf_s64_t value);
	virtual avf_status_t WriteRegBool(const char *name, avf_u32_t index, avf_int_t value);
	virtual avf_status_t WriteRegString(const char *name, avf_u32_t index, const char *value);
	virtual avf_status_t WriteRegMem(const char *name, avf_u32_t index, const avf_u8_t *mem, avf_size_t size);

	virtual void ClearRegistry();
	virtual avf_status_t LoadConfig(CConfig *doc, const char *root);

private:
	enum {
		TYPE_NULL,
		TYPE_BOOL,
		TYPE_INT32,
		TYPE_INT64,
		TYPE_STRING,
		TYPE_MEM,
	};

	struct value_t {
		union {
			avf_s32_t int32_value;
			avf_s64_t int64_value;
			char *string_value;		// not NULL
			struct {
				avf_size_t size;
				avf_u8_t *ptr;
			} mem;
		} u;
	};

	enum {
		FREE_NAME = (1 << 0),		// name need free
		FREE_VALUE = (1 << 1),		// string_value need free
		FREE_NODE = (1 << 2),		// node need free
	};

	struct node_t {
		node_t *parent;
		node_t *prev;
		node_t *next;
		node_t *first_child;
		node_t *last_child;

		char *name;			// not NULL
		avf_u32_t index;
		avf_u8_t flags;	// FREE_NAME etc
		avf_u8_t type;

		value_t value;
	};

private:
	CMutex mMutex;
	node_t mRoot;
	IRegistry *const mpGlobalRegistry;

private:
	node_t *GetNode(const char *name, avf_u32_t index);

	static void FreeAllChildren(node_t *node);
	static void FreeNode(node_t *node);

	void INLINE FreeAllNodes() {
		FreeAllChildren(&mRoot);
	}

	static INLINE void CopyValue(char *value, const char *v) {
		::strncpy(value, v, REG_STR_MAX);
		value[REG_STR_MAX-1] = 0;
	}

	static INLINE void FreeNodeMem(node_t *node) {
		if (test_flag(node->flags, FREE_VALUE)) {
			clear_flag(node->flags, FREE_VALUE);
			if (node->type == TYPE_STRING) {
				avf_free(node->value.u.string_value);
			} else if (node->type == TYPE_MEM) {
				avf_free(node->value.u.mem.ptr);
			}
		}
	}
};
#endif

//-----------------------------------------------------------------------
//
//  CRegistry2
//
//-----------------------------------------------------------------------
class CRegistry2: public CObject, public IRegistry
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

	enum {
		ALIGN_SIZE = 8,
	};

public:
	static CRegistry2* Create(IRegistry *pGlobalRegistry);

private:
	CRegistry2(IRegistry *pGlobalRegistry);
	virtual ~CRegistry2();

// IRegistry
public:
	virtual avf_s32_t ReadRegInt32(const char *name, avf_u32_t index, avf_s32_t default_value);
	virtual avf_s64_t ReadRegInt64(const char *name, avf_u32_t index, avf_s64_t default_value);
	virtual avf_int_t ReadRegBool(const char *name, avf_u32_t index, avf_int_t default_value);
	virtual void ReadRegString(const char *name, avf_u32_t index, char *value, const char *default_value);
	virtual void ReadRegMem(const char *name, avf_u32_t index, avf_u8_t *mem, avf_size_t size);

	virtual avf_status_t WriteRegInt32(const char *name, avf_u32_t index, avf_s32_t value);
	virtual avf_status_t WriteRegInt64(const char *name, avf_u32_t index, avf_s64_t value);
	virtual avf_status_t WriteRegBool(const char *name, avf_u32_t index, avf_int_t value);
	virtual avf_status_t WriteRegString(const char *name, avf_u32_t index, const char *value);
	virtual avf_status_t WriteRegMem(const char *name, avf_u32_t index, const avf_u8_t *mem, avf_size_t size);

	virtual void ClearRegistry();
	virtual avf_status_t LoadConfig(CConfig *doc, const char *root);

private:
	typedef avf_u32_t hash_val_t;

	enum {
		TYPE_NULL,
		TYPE_INT32,
		TYPE_INT64,
		TYPE_BOOL,
		TYPE_STRING,
		TYPE_MEM,
	};

	struct value_t {
		union {
			avf_s32_t int32_value;
			avf_s64_t int64_value;
			char *string_value;		// not NULL for TYPE_STRING
			struct {
				avf_size_t size;
				avf_u8_t *ptr;
			} mem;
		} u;
	};

	struct reg_node_s {
		struct rb_node rbnode;
		hash_val_t hash_val;		// hash of pKey & index
		avf_s32_t index;
		char *name;				// not NULL

		avf_u8_t type;
		value_t value;

		static reg_node_s *from_node(struct rb_node *node) {
			return container_of(node, reg_node_s, rbnode);
		}
	};

private:
	reg_node_s *FindNodeLocked(const char *name, avf_u32_t index, bool bCreate);

private:
	static INLINE void CopyValue(char *value, const char *v) {
		::strncpy(value, v, REG_STR_MAX);
		value[REG_STR_MAX-1] = 0;
	}

	static INLINE void FreeNodeMem(reg_node_s *node) {
		if (node->type == TYPE_STRING) {
			node->type = TYPE_NULL;
			avf_free(node->value.u.string_value);
		} else if (node->type == TYPE_MEM) {
			node->type = TYPE_NULL;
			avf_free(node->value.u.mem.ptr);
		}
	}

	static void FreeNode(struct rb_node *rbnode);

	static INLINE void SafeFreeNode(struct rb_node *rbnode) {
		if (rbnode) {
			FreeNode(rbnode);
		}
	}

private:
	CMutex mMutex;
	struct rb_root mRegSet;
	IRegistry *const mpGlobalRegistry;
};

#endif

