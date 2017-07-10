
#define LOG_TAG "avf_registry"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_config.h"
#include "avf_util.h"

#include "avf_registry.h"

//
//	GLOBAL_START(index, mpGlobalRegistry)
//	return mpGlobalRegistry->ReadRegxxx...
//	GLOBAL_END;

#define GLOBAL_START(_index, _pGlobalReg) \
	do { \
		if ((_index) & IRegistry::GLOBAL) { \
			_index &= ~IRegistry::GLOBAL; \
			if (_pGlobalReg) {

#define GLOBAL_END \
			} \
		} \
	} while (0)

#if 0
//-----------------------------------------------------------------------
//
//  CRegistry
//
//-----------------------------------------------------------------------

CRegistry* CRegistry::Create(IRegistry *pGlobalRegistry)
{
	CRegistry* result = avf_new CRegistry(pGlobalRegistry);
	CHECK_STATUS(result);
	return result;
}

CRegistry::CRegistry(IRegistry *pGlobalRegistry):
	mpGlobalRegistry(pGlobalRegistry)
{
	mRoot.parent = NULL;
	mRoot.prev = NULL;
	mRoot.next = NULL;
	mRoot.first_child = NULL;
	mRoot.last_child = NULL;
	mRoot.name = (char*)"";
	mRoot.index = 0;
	mRoot.flags = 0;
	mRoot.type = TYPE_NULL;
}

CRegistry::~CRegistry()
{
	FreeAllNodes();
}

void *CRegistry::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IRegistry)
		return static_cast<IRegistry*>(this);
	return inherited::GetInterface(refiid);
}

void CRegistry::ClearRegistry()
{
	AUTO_LOCK(mMutex);
	FreeAllNodes();
	mRoot.first_child = NULL;
	mRoot.last_child = NULL;
}

avf_status_t CRegistry::LoadConfig(CConfig *doc, const char *root)
{
	CConfigNode* pConfigs;

	if ((pConfigs = doc->FindChild(root)) == NULL) {
		AVF_LOGE("LoadConfig: %s does not exist", root);
		return E_ERROR;
	}

	for (CConfigNode *pc = pConfigs->GetFirst(); pc; pc = pc->GetNext()) {
		WriteRegString(pc->GetName(), pc->GetIndex(), pc->GetValue());
	}

	return E_OK;
}

avf_s32_t CRegistry::ReadRegInt32(const char *name, avf_u32_t index, avf_s32_t default_value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->ReadRegInt32(name, index, default_value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	if ((node = GetNode(name, index)) == NULL) {
		return default_value;
	}

	switch (node->type) {
	default:
		return default_value;

	case TYPE_NULL:		// created
		node->type = TYPE_INT32;
		node->value.u.int32_value = default_value;
		return default_value;

	case TYPE_INT32:
		return node->value.u.int32_value;

	case TYPE_INT64:
		return (avf_s32_t)node->value.u.int64_value;

	case TYPE_BOOL:
		return (avf_s32_t)node->value.u.int32_value;

	case TYPE_STRING:
		return (avf_s32_t)atoi(node->value.u.string_value);
	}
}

avf_s64_t CRegistry::ReadRegInt64(const char *name, avf_u32_t index, avf_s64_t default_value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->ReadRegInt64(name, index, default_value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	if ((node = GetNode(name, index)) == NULL) {
		return default_value;
	}

	switch (node->type) {
	default:
		return default_value;

	case TYPE_NULL:		// created
		node->type = TYPE_INT64;
		node->value.u.int64_value = default_value;
		return default_value;

	case TYPE_INT32:
		return node->value.u.int32_value;

	case TYPE_INT64:
		return node->value.u.int64_value;

	case TYPE_BOOL:
		return node->value.u.int32_value;

	case TYPE_STRING:
		return (avf_s64_t)atoll(node->value.u.string_value);
	}
}

avf_int_t CRegistry::ReadRegBool(const char *name, avf_u32_t index, avf_int_t default_value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->ReadRegBool(name, index, default_value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	if ((node = GetNode(name, index)) == NULL) {
		return default_value;
	}

	switch (node->type) {
	default:
		return default_value;

	case TYPE_NULL:		// created
		node->type = TYPE_BOOL;
		node->value.u.int32_value = default_value;
		return default_value;

	case TYPE_INT32:
		return node->value.u.int32_value != 0;

	case TYPE_INT64:
		return node->value.u.int64_value != 0;

	case TYPE_BOOL:
		return node->value.u.int32_value != 0;

	case TYPE_STRING: {
			// "1", "yes" and "true" are treated as true
			// others are treated as false
			const char *value = node->value.u.string_value;
			if (value[0] == '1' && value[1] == 0) return 1;
			if (value[0] == '0' && value[1] == 0) return 0;
			if (strcmp(value, "yes") == 0) return 1;
			if (strcmp(value, "true") == 0) return 1;
			return 0;
		}
	}
}

void CRegistry::ReadRegString(const char *name, avf_u32_t index, char *value, const char *default_value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->ReadRegString(name, index, value, default_value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	if ((node = GetNode(name, index)) == NULL) {
		return CopyValue(value, default_value);
	}

	switch (node->type) {
	default:
		return CopyValue(value, default_value);

	case TYPE_NULL: 	// created
		node->type = TYPE_STRING;
		node->value.u.string_value = avf_strdup(default_value);
		if (node->value.u.string_value == NULL) {
			node->value.u.string_value = (char*)"0";	// 
		} else {
			set_flag(node->flags, FREE_VALUE);
		}
		return CopyValue(value, default_value);

	case TYPE_INT32:
		sprintf(value, "%d", (int32_t)node->value.u.int32_value);
		return;

	case TYPE_INT64:
		sprintf(value, LLD, node->value.u.int64_value);
		return;

	case TYPE_BOOL:
		strcpy(value, node->value.u.int32_value ? "true" : "false");
		return;

	case TYPE_STRING:
		return CopyValue(value, node->value.u.string_value);
	}
}

void CRegistry::ReadRegMem(const char *name, avf_u32_t index, avf_u8_t *mem, avf_size_t size)
{
	GLOBAL_START(index, mpGlobalRegistry);
	return mpGlobalRegistry->ReadRegMem(name, index, mem, size);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	const avf_u8_t *psrc = NULL;
	avf_size_t ssize = 0;

	if ((node = GetNode(name, index)) != NULL) {
		switch (node->type) {
		default:
		case TYPE_NULL:
			break;

		case TYPE_BOOL:
		case TYPE_INT32:
			psrc = (const avf_u8_t*)&node->value.u.int32_value;
			ssize = sizeof(avf_u32_t);
			break;

		case TYPE_INT64:
			psrc = (const avf_u8_t*)&node->value.u.int64_value;
			ssize = sizeof(avf_u64_t);
			break;

		case TYPE_STRING:
			psrc = (const avf_u8_t*)node->value.u.string_value;
			ssize = ::strlen((const char*)psrc);
			break;

		case TYPE_MEM:
			psrc = node->value.u.mem.ptr;
			ssize = node->value.u.mem.size;
			break;
		}
	}

	if (ssize > 0) {
		avf_size_t tocopy = MIN(ssize, size);
		::memcpy(mem, psrc, tocopy);
		mem += tocopy;
		size -= tocopy;
	}

	if (size > 0) {
		::memset(mem, 0, size);
	}
}

avf_status_t CRegistry::WriteRegInt32(const char *name, avf_u32_t index, avf_s32_t value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->WriteRegInt32(name, index, value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	if ((node = GetNode(name, REG_WRITE|index)) == NULL) {
		return E_NOMEM;
	}

	FreeNodeMem(node);

	node->type = TYPE_INT32;
	node->value.u.int32_value = value;

	return E_OK;
}

avf_status_t CRegistry::WriteRegInt64(const char *name, avf_u32_t index, avf_s64_t value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->WriteRegInt64(name, index, value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	if ((node = GetNode(name, REG_WRITE|index)) == NULL) {
		return E_NOMEM;
	}

	FreeNodeMem(node);

	node->type = TYPE_INT64;
	node->value.u.int64_value = value;

	return E_OK;
}

avf_status_t CRegistry::WriteRegBool(const char *name, avf_u32_t index, avf_int_t value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->WriteRegBool(name, index, value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	if ((node = GetNode(name, REG_WRITE|index)) == NULL) {
		return E_NOMEM;
	}

	FreeNodeMem(node);

	node->type = TYPE_BOOL;
	node->value.u.int32_value = value;

	return E_OK;
}

avf_status_t CRegistry::WriteRegString(const char *name, avf_u32_t index, const char *value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->WriteRegString(name, index, value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	if ((node = GetNode(name, REG_WRITE|index)) == NULL) {
		return E_NOMEM;
	}

	FreeNodeMem(node);

	node->type = TYPE_STRING;
	node->value.u.string_value = avf_strdup(value);
	if (node->value.u.string_value == NULL) {
		node->value.u.string_value = (char*)"0";	// 
		return E_NOMEM;
	}

	set_flag(node->flags, FREE_VALUE);
	return E_OK;
}

avf_status_t CRegistry::WriteRegMem(const char *name, avf_u32_t index, const avf_u8_t *mem, avf_size_t size)
{
	GLOBAL_START(index, mpGlobalRegistry);
	return mpGlobalRegistry->WriteRegMem(name, index, mem, size);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	node_t *node;

	if ((node = GetNode(name, REG_WRITE|index)) == NULL) {
		return E_NOMEM;
	}

	if (node->type == TYPE_MEM && node->value.u.mem.size == size) {
		::memcpy(node->value.u.mem.ptr, mem, size);
		return E_OK;
	}

	FreeNodeMem(node);

	node->type = TYPE_MEM;

	avf_u8_t *ptr = avf_malloc_bytes(size);
	if (ptr == NULL) {
		node->type = TYPE_NULL;
		return E_NOMEM;
	}

	::memcpy(ptr, mem, size);

	node->value.u.mem.size = size;
	node->value.u.mem.ptr = ptr;

	set_flag(node->flags, FREE_VALUE);
	return E_OK;
}

//
// e.g. config.vin.resolution
//
// find the node of (name, index)
// when index & REG_WRITE, create the node if it does not exist (type set to TYPE_NULL)
//
CRegistry::node_t *CRegistry::GetNode(const char *name, avf_u32_t index)
{
	node_t *node = &mRoot;
	const char *p = name;
	avf_s32_t create = test_flag(index, REG_WRITE);
	clear_flag(index, REG_WRITE);

Next:
	while (*p) {
		const char *pend = p + 1;
		unsigned int c = *pend++;
		node_t *child;
		avf_size_t size;

		// find name
		while (c != '.' && c != 0)
			c = *pend++;

		// p...pend-2 is node name
		// pend-1 is '.' or EOS

		for (child = node->first_child; child; child = child->next) {

			// index and name must match
			if (index != child->index) {
				// only check the last node's index - todo
				if (c == 0)
					continue;
			}

			int len = pend - p - 1;
			if (::strncasecmp(p, child->name, len) || child->name[len] != 0)
				continue;

			// now the names equal
			node = child;

			if (c == 0)
				goto End;

			p = pend;
			goto Next;
		}

		// no match
		if (!create)
			return NULL;

		// now create all the nodes
		while (true) {
			if ((child = avf_malloc_type(node_t)) == NULL)
				return NULL;

			size = pend - p - 1;
			if ((child->name = (char*)avf_malloc(size + 1)) == NULL) {
				avf_free(child);
				return NULL;
			}

			::strncpy(child->name, p, size);
			child->name[size] = 0;

			child->index = 0;	// fixed later
			child->flags = FREE_NODE | FREE_NAME;
			child->type = TYPE_NULL;

			// setup link
			child->parent = node;
			child->first_child = NULL;
			child->last_child = NULL;
			if (node->first_child == NULL) {
				node->first_child = child;
				node->last_child = child;
				child->prev = NULL;
				child->next = NULL;
			} else {
				child->prev = node->last_child;
				child->next = NULL;
				node->last_child->next = child;
				node->last_child = child;
			}

			if (c == 0) {
				child->index = index;
				return child;
			}

			// get next node name
			p = pend;

			c = *pend++;
			if (c == 0) {
				child->index = index;
				return child;
			}

			while (c != '.' && c != 0)
				c = *pend++;

			node = child;

		} // end while
	} // end while

End:
	return node;
}

void CRegistry::FreeAllChildren(node_t *node)
{
	node_t *child = node->first_child;
	while (child) {
		node_t *next = child->next;
		FreeAllChildren(child);
		child = next;
	}
	FreeNode(node);
}

void CRegistry::FreeNode(node_t *node)
{
	if (test_flag(node->flags, FREE_NAME))
		avf_free(node->name);

	if (test_flag(node->flags, FREE_VALUE))
		avf_free(node->value.u.string_value);

	if (test_flag(node->flags, FREE_NODE))
		avf_free(node);
}
#endif

//-----------------------------------------------------------------------
//
//  CRegistry2
//
//-----------------------------------------------------------------------
CRegistry2* CRegistry2::Create(IRegistry *pGlobalRegistry)
{
	CRegistry2 *result = avf_new CRegistry2(pGlobalRegistry);
	CHECK_STATUS(result);
	return result;
}

CRegistry2::CRegistry2(IRegistry *pGlobalRegistry):
	mpGlobalRegistry(pGlobalRegistry)
{
	mRegSet.rb_node = NULL;
}

CRegistry2::~CRegistry2()
{
	SafeFreeNode(mRegSet.rb_node);
}

void CRegistry2::FreeNode(struct rb_node *rbnode)
{
	SafeFreeNode(rbnode->rb_left);
	SafeFreeNode(rbnode->rb_right);
	reg_node_s *node = reg_node_s::from_node(rbnode);
	FreeNodeMem(node);
	avf_free(node->name);
	avf_free(node);
}

void *CRegistry2::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IRegistry)
		return static_cast<IRegistry*>(this);
	return inherited::GetInterface(refiid);
}

avf_s32_t CRegistry2::ReadRegInt32(const char *name, avf_u32_t index, avf_s32_t default_value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->ReadRegInt32(name, index, default_value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, false)) == NULL) {
		return default_value;
	}

	switch (node->type) {
	default:
	case TYPE_NULL:
		return default_value;

	case TYPE_INT32:
		return node->value.u.int32_value;

	case TYPE_INT64:
		return (avf_s32_t)node->value.u.int64_value;

	case TYPE_BOOL:
		return (avf_s32_t)node->value.u.int32_value;

	case TYPE_STRING:
		return (avf_s32_t)::atoi(node->value.u.string_value);
	}
}

avf_s64_t CRegistry2::ReadRegInt64(const char *name, avf_u32_t index, avf_s64_t default_value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->ReadRegInt64(name, index, default_value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, false)) == NULL) {
		return default_value;
	}

	switch (node->type) {
	default:
	case TYPE_NULL:
		return default_value;

	case TYPE_INT32:
		return node->value.u.int32_value;

	case TYPE_INT64:
		return node->value.u.int64_value;

	case TYPE_BOOL:
		return node->value.u.int32_value;

	case TYPE_STRING:
		return (avf_s64_t)::atoll(node->value.u.string_value);
	}
}

avf_int_t CRegistry2::ReadRegBool(const char *name, avf_u32_t index, avf_int_t default_value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->ReadRegBool(name, index, default_value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, false)) == NULL) {
		return default_value;
	}

	switch (node->type) {
	default:
	case TYPE_NULL:
		return default_value;

	case TYPE_INT32:
		return node->value.u.int32_value != 0;

	case TYPE_INT64:
		return node->value.u.int64_value != 0;

	case TYPE_BOOL:
		return node->value.u.int32_value != 0;

	case TYPE_STRING: {
			// "1", "yes" and "true" are treated as true
			// others are treated as false
			const char *value = node->value.u.string_value;
			if (value[0] == '1' && value[1] == 0) return 1;
			if (value[0] == '0' && value[1] == 0) return 0;
			if (::strcmp(value, "yes") == 0) return 1;
			if (::strcmp(value, "true") == 0) return 1;
			return 0;
		}
	}
}

void CRegistry2::ReadRegString(const char *name, avf_u32_t index, char *value, const char *default_value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->ReadRegString(name, index, value, default_value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, false)) == NULL) {
		return CopyValue(value, default_value);
	}

	switch (node->type) {
	default:
	case TYPE_NULL:
		return CopyValue(value, default_value);

	case TYPE_INT32:
		::sprintf(value, "%d", (avf_s32_t)node->value.u.int32_value);
		return;

	case TYPE_INT64:
		::sprintf(value, LLD, node->value.u.int64_value);
		return;

	case TYPE_BOOL:
		::strcpy(value, node->value.u.int32_value ? "true" : "false");
		return;

	case TYPE_STRING:
		return CopyValue(value, node->value.u.string_value);
	}
}

void CRegistry2::ReadRegMem(const char *name, avf_u32_t index, avf_u8_t *mem, avf_size_t size)
{
	GLOBAL_START(index, mpGlobalRegistry);
	return mpGlobalRegistry->ReadRegMem(name, index, mem, size);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, false)) == NULL || node->type != TYPE_MEM) {
		::memset(mem, 0, size);
		return;
	}

	avf_size_t nsize = node->value.u.mem.size;
	avf_size_t tocopy = MIN(nsize, size);

	::memcpy(mem, node->value.u.mem.ptr, tocopy);
	if (size > tocopy) {
		::memset(mem + tocopy, 0, size - tocopy);
	}
}

avf_status_t CRegistry2::WriteRegInt32(const char *name, avf_u32_t index, avf_s32_t value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->WriteRegInt32(name, index, value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, true)) == NULL) {
		return E_NOMEM;
	}

	FreeNodeMem(node);

	node->type = TYPE_INT32;
	node->value.u.int32_value = value;

	return E_OK;
}

avf_status_t CRegistry2::WriteRegInt64(const char *name, avf_u32_t index, avf_s64_t value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->WriteRegInt64(name, index, value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, true)) == NULL) {
		return E_NOMEM;
	}

	FreeNodeMem(node);

	node->type = TYPE_INT64;
	node->value.u.int64_value = value;

	return E_OK;
}

avf_status_t CRegistry2::WriteRegBool(const char *name, avf_u32_t index, avf_int_t value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->WriteRegBool(name, index, value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, true)) == NULL) {
		return E_NOMEM;
	}

	FreeNodeMem(node);

	node->type = TYPE_BOOL;
	node->value.u.int32_value = value;

	return E_OK;
}

avf_status_t CRegistry2::WriteRegString(const char *name, avf_u32_t index, const char *value)
{
	GLOBAL_START(index, mpGlobalRegistry)
	return mpGlobalRegistry->WriteRegString(name, index, value);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, true)) == NULL) {
		return E_NOMEM;
	}

	FreeNodeMem(node);

	node->type = TYPE_STRING;
	node->value.u.string_value = avf_strdup(value);
	if (node->value.u.string_value == NULL) {
		node->type = TYPE_NULL;
		return E_NOMEM;
	}

	return E_OK;
}

avf_status_t CRegistry2::WriteRegMem(const char *name, avf_u32_t index, const avf_u8_t *mem, avf_size_t size)
{
	GLOBAL_START(index, mpGlobalRegistry);
	return mpGlobalRegistry->WriteRegMem(name, index, mem, size);
	GLOBAL_END;

	AUTO_LOCK(mMutex);
	reg_node_s *node;

	if ((node = FindNodeLocked(name, index, true)) == NULL) {
		return E_NOMEM;
	}

	if (node->type == TYPE_MEM && node->value.u.mem.size == size) {
		::memcpy(node->value.u.mem.ptr, mem, size);
		return E_OK;
	}

	FreeNodeMem(node);

	node->type = TYPE_MEM;
	avf_u8_t *ptr = avf_malloc_bytes(size);

	if (ptr == NULL) {
		node->type = TYPE_NULL;
		return E_NOMEM;
	}

	::memcpy(ptr, mem, size);

	node->value.u.mem.size = size;
	node->value.u.mem.ptr = ptr;

	return E_OK;
}

void CRegistry2::ClearRegistry()
{
	AUTO_LOCK(mMutex);
	SafeFreeNode(mRegSet.rb_node);
	mRegSet.rb_node = NULL;
}

avf_status_t CRegistry2::LoadConfig(CConfig *doc, const char *root)
{
	CConfigNode* pConfigs;

	if ((pConfigs = doc->FindChild(root)) == NULL) {
		AVF_LOGE("LoadConfig: %s does not exist", root);
		return E_ERROR;
	}

	for (CConfigNode *pc = pConfigs->GetFirst(); pc; pc = pc->GetNext()) {
		WriteRegString(pc->GetName(), pc->GetIndex(), pc->GetValue());
	}

	return E_OK;
}

CRegistry2::reg_node_s *CRegistry2::FindNodeLocked(const char *name, avf_u32_t index, bool bCreate)
{
	hash_val_t hash_val = avf_calc_hash((const avf_u8_t*)name) + index;

	struct rb_node **p = &mRegSet.rb_node;
	struct rb_node *parent = NULL;
	reg_node_s *node;

	while (*p) {
		parent = *p;
		node = reg_node_s::from_node(parent);

		if (hash_val < node->hash_val) {
			p = &parent->rb_left;
		} else if (hash_val > node->hash_val) {
			p = &parent->rb_right;

		} else {
			int rval = ::strcmp(name, node->name);
			if (rval == 0) {
				// index should be equal
				return node;
			}
			p = rval < 0 ? &parent->rb_left : &parent->rb_right;
		}
	}

	if (!bCreate)
		return NULL;

	if ((node = avf_malloc_type(reg_node_s)) == NULL)
		return NULL;

	node->hash_val = hash_val;
	node->index = index;
	node->type = TYPE_NULL;
	if ((node->name = avf_strdup(name)) == NULL) {
		avf_free(node);
		return NULL;
	}

	rb_link_node(&node->rbnode, parent, p);
	rb_insert_color(&node->rbnode, &mRegSet);

	return node;
}

