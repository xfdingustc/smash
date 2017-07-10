
#ifndef __LIST_H__
#define __LIST_H__

//-----------------------------------------------------------------------
//
// circular doubley linked list
//
//-----------------------------------------------------------------------

typedef struct list_head_s {
	struct list_head_s *next;
	struct list_head_s *prev;
} list_head_t;

#define __list_init(__list) \
	do { \
		__list->next = __list; \
		__list->prev = __list; \
	} while (0)

#define list_init(_list) \
	do { \
		list_head_t *__list = &(_list); \
		__list->next = __list; \
		__list->prev = __list; \
	} while (0)

#define __list_add(__node, __list) \
	do { \
		__node->prev = __list; \
		__node->next = __list->next; \
		__list->next->prev = __node; \
		__list->next = __node; \
	} while (0)

#define list_add(_node, _list)	\
	do { \
		list_head_t *__node = (_node); \
		list_head_t *__list = &(_list); \
		__list_add(__node, __list); \
	} while (0)

#define __list_add_tail(__node, __list) \
	do { \
		__node->next = __list; \
		__node->prev = __list->prev; \
		__list->prev->next = __node; \
		__list->prev = __node; \
	} while (0)

#define list_add_tail(_node, _list) \
	do { \
		list_head_t *__node = (_node); \
		list_head_t *__list = &(_list); \
		__list_add_tail(__node, __list); \
	} while (0)

// append _node after _prev
// _prev should be in the list
#define list_append(_node, _prev) \
	do { \
		list_head_t *__prev = &(_prev); \
		list_head_t *__node = &(_node); \
		__list_add(__node, __prev); \
	} while (0)

// insert _node before _next
// _next is in the list
#define list_insert(_node, _next) \
	do { \
		list_head_t *__node = &(_node); \
		list_head_t *__next = &(_next); \
		__list_add_tail(__node, __next); \
	} while (0)

#define __list_del(_node) \
	do { \
		list_head_t *__node = (_node); \
		__node->prev->next = __node->next; \
		__node->next->prev = __node->prev; \
	} while (0)

// delete and init
#define list_del(_node) \
	do { \
		list_head_t *_list = (_node); \
		__list_del(_list); \
		__list_init(_list); \
	} while (0)

#define list_head_node(_list)		(_list).next
#define list_tail_node(_list)		(_list).prev

#define list_entry(_node, _type, _member) \
	((_type*)((const unsigned char *)(_node) - OFFSET(_type, _member)))

#define list_for_each(_node, _list) \
	for (_node = (_list).next; _node != &(_list); _node = _node->next)

#define list_for_each_backward(_node, _list) \
	for (_node = (_list).prev; _node != &(_list); _node = _node->prev)

#define list_for_each_del_backward(_node, _prev, _list) \
	for (_node = (_list).prev; _prev = (_node)->prev, _node != &(_list); _node = _prev)

// _list is pointer
#define list_for_each_p(_node, _list) \
	for (_node = (_list)->next; _node != (_list); _node = _node->next)

// _node can be removed from the list
#define list_for_each_del(_node, _next, _list) \
	for (_node = (_list).next; _next = (_node)->next, _node != &(_list); _node = _next)

#define list_for_each_next(_node, _list) \
	for (_node = _node->next; _node != &(_list); _node = _node->next)

#define list_for_each_from(_node, _list) \
	for (; _node != &(_list); _node = _node->next)

#define list_is_empty(_list) \
	((_list).next == &(_list))

#define list_is_first(_node, _list) \
	((_node) == (_list).next)

#define list_is_last(_node, _list) \
	((_node)->next == &(_list))

#define LIST_NODE			{NULL, NULL}
#define LIST_HEAD(_list)	{&_list, &_list}

//-----------------------------------------------------------------------
//
// linked list
//
//-----------------------------------------------------------------------

#define DEFINE_HLIST_NODE(_type) \
	_type* mpNext; \
	_type** mppPrev

#define HLIST_INSERT(_first, _node) \
	do { \
		(_node)->mpNext = (_first); \
		(_node)->mppPrev = &(_first); \
		if (_first) \
			(_first)->mppPrev = &(_node)->mpNext; \
		(_first) = (_node); \
	} while (0)

#define HLIST_REMOVE(_node) \
	do { \
		*(_node)->mppPrev = (_node)->mpNext; \
		if ((_node)->mpNext) \
			(_node)->mpNext->mppPrev = (_node)->mppPrev; \
	} while (0)

#endif

