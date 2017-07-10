

//
// event is like the user space 'condition variable', plus a waiter counter (num_waiters)
// when calling the APIs, the caller should hold the lock first
//

typedef struct event_s {
	spinlock_t lock;
	int num_signals;
	int num_waiters;
	struct list_head wait_list;
} event_t;

#define __EVENT_INITIALIZER(name) \
{ \
	.lock = __SPIN_LOCK_UNLOCKED((name).lock), \
	.num_signals = 0, \
	.num_waiters = 0, \
	.wait_list = LIST_HEAD_INIT((name).wait_list), \
}

#if 0
#define DEFINE_EVENT(name) \
	event_t name = __EVENT_INITIALIZER(name)
#endif

static inline void event_init(event_t *event)
{
	static struct lock_class_key __key;
	*event = (event_t)__EVENT_INITIALIZER(*event);
	lockdep_init_map(&event->lock.dep_map, "event->lock", &__key, 0);
}

extern int __wait_for_event(struct mutex *lock, event_t *event, long state, long timeout);
extern void __event_signal(event_t *event, int wakeup_one);

static inline void wait_for_event(struct mutex *lock, event_t *event)
{
	__wait_for_event(lock, event, TASK_UNINTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}

static inline int wait_for_event_interruptible(struct mutex *lock, event_t *event)
{
	return __wait_for_event(lock, event, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}

static inline int wait_for_event_interruptible_timeout(struct mutex *lock, event_t *event, unsigned long timeout)
{
	return __wait_for_event(lock, event, TASK_INTERRUPTIBLE, timeout);
}

static inline void event_signal(event_t *event)
{
	if (event->num_waiters > 0) {
		__event_signal(event, 1);
	}
}

static inline void event_signal_all(event_t *event)
{
	if (event->num_waiters > 0) {
		__event_signal(event, 0);
	}
}

