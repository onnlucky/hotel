#ifndef _lqueue_h_
#define _lqueue_h_

/// a single entry that can be put in a queue
typedef struct lqentry lqentry;

/// the type of a queue
typedef struct lqueue lqueue;

/// the implementation of an entry, is given so it can be embedded in larger structs
struct lqentry { lqentry* next; };

/// the implementation of a queue, is given so it can be embedded in larger structs
struct lqueue { lqentry* head; lqentry* tail; };

/// Create a new entry; it is likely more useful to embed the @lqentry in a
/// larger struct and use @lqentry_init instead.
lqentry* lqentry_new();

/// Create a new queue.
lqueue* lqueue_new();

/// Initialize an entry; zeroing an entry is good enough.
lqentry* lqentry_init(lqentry* i);

/// Initialize a queue; zeroing a queue is good enough.
lqueue* lqueue_init(lqueue* q);

/// Add a entry to the queue. It is important the entry is not part of any
/// queue.
void lqueue_put(lqueue* q, lqentry* i);

/// Get the first entry of the queue. Null if no entries exist for this queue.
lqentry* lqueue_get(lqueue* q);

/// Get the first entry of the queue, but not remove it. Null if no entries exist for this queue.
lqentry* lqueue_peek(lqueue* q);

#endif

