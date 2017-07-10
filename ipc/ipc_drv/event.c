
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include "event.h"

typedef struct event_waiter_s {
	struct list_head list;
	struct task_struct *task;
	int up;
} event_waiter_t;

static inline int __wait_common(event_t *event, long state, long timeout)
{
	struct task_struct *task = current;
	event_waiter_t waiter;
	int rval;

	list_add_tail(&waiter.list, &event->wait_list);
	waiter.task = task;
	waiter.up = 0;

	for (;;) {
		if (signal_pending_state(state, task)) {
			rval = -EINTR;
			goto Fail;
		}

		if (timeout <= 0) {
			rval = -ETIME;
			goto Fail;
		}

		__set_task_state(task, state);
		spin_unlock_irq(&event->lock);
		timeout = schedule_timeout(timeout);
		spin_lock_irq(&event->lock);

		if (waiter.up)
			return 0;
	}

Fail:
	event->num_signals--;	// (1)
	list_del(&waiter.list);
	return rval;
}

// num_waiters + num_signals == num_actual_waiters
// num_waiters >= 0
// num_signals may < 0

static inline void __adjust_event_counter(event_t *event)
{
	if (unlikely(event->num_signals < 0)) {
		event->num_waiters += event->num_signals;
		event->num_signals = 0;
	}
}

int __wait_for_event(struct mutex *lock, event_t *event, long state, long timeout)
{
	unsigned long flags;
	int rval;

	event->num_waiters++;
	mutex_unlock(lock);

	spin_lock_irqsave(&event->lock, flags);

	if (unlikely(event->num_signals > 0)) {
		event->num_signals--;
		rval = 0;
	} else {
		rval = __wait_common(event, state, timeout);
		// if rval == 0 (succes), it's waken up by server, and num_signals was decreased (3)
		// if rval < 0 (failed), num_signals was also decreased (1)
	}

	spin_unlock_irqrestore(&event->lock, flags);

	mutex_lock(lock);

	if (unlikely(rval < 0)) {
		// need adjust num_waiters and num_signals
		// because __event_signal() may make num_waiters-- and num_signals++,
		// we cannot adjust them accurately
		spin_lock_irqsave(&event->lock, flags);
		__adjust_event_counter(event);
		spin_unlock_irqrestore(&event->lock, flags);
	}

	return rval;
}

void __event_signal(event_t *event, int wakeup_one)
{
	unsigned long flags;

	spin_lock_irqsave(&event->lock, flags);

	__adjust_event_counter(event);

	// now num_waiters is the real number,
	// and num_signals >= 0

	if (likely(event->num_waiters > 0)) {
		struct list_head *curr;
		struct list_head *tmp;

		int count = wakeup_one ? 1 : event->num_waiters;
		event->num_waiters -= count;	// (2)
		event->num_signals += count;

		// wake up at most 'num_signals' waiters
		// after this, num_signals is the number of clients not awaken
		list_for_each_safe(curr, tmp, &event->wait_list) {
			event_waiter_t *waiter = list_entry(curr, event_waiter_t, list);
			list_del(curr);
			waiter->up = 1;
			wake_up_process(waiter->task);
			if (--event->num_signals == 0)	// (3)
				break;
		}
	}

	spin_unlock_irqrestore(&event->lock, flags);
}

