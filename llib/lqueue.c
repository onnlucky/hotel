// ** a lock free fifo queue **
//
// author: Onne Gorter <onne@onnlucky.com>
//
// Notice the queue->head and entry->next pointers are authorative.
// The queue->tail pointer is just a cache and may point to random entries;
// this implies the use of something like a gc.
//
//
// Notice it might appear there is an ABA problem, but there is not. If a
// thread tries to find the real tail, but is stalled for a long time, the tail
// it finds might be an entry about to be put in the queue again. Adding as the
// next element to this entry is ok, since it will be put in the queue.
//
// However, if this second thread is stalled for a long time too, unable to put
// its two sized entry in the queue. And our first thread inserts yet another
// entry. Now this second entry might appear in the queue before the first.
// This is a breach of the fifo contract. So this is a mostly fifo queue.
//
// A possible solution is to hold on to the previous element too, and disalow
// longer sized chains. But then tail should point to second to last element.
// And a zero or one sized queue is special.
// Never re-using entries is another way.
// Another solution is to use a thread local variable that verifies:
// if (tls_last_entry->next == owner(q) && tls_last_entry != tail) goto retry;
//
// TODO investigate using the queue as a special node to remove some spacial
// cases of empty list
// TODO add memory barriers and explicit load where needed

#define _GNU_SOURCE
#include <stdlib.h>

#include "atomic_ops.h"
#include "lqueue.h"

static int lqueue_cas(void* addr, const void* nval, const void* oval) {
    return AO_compare_and_swap(addr, (AO_t)oval, (AO_t)nval);
}


// ** implementation **

lqentry* lqentry_init(lqentry* i) { i->next = 0; return i; }
lqueue* lqueue_init(lqueue* q) { q->head = 0; q->tail = 0; return q; }

lqentry* lqentry_new() { return lqentry_init(malloc(sizeof(lqentry))); }
lqueue* lqueue_new() { return lqueue_init(malloc(sizeof(lqueue))); }

// TODO add volatile
static lqentry* load(lqentry* i) { return i; }
static lqentry* owner(lqueue* q) { return (lqentry* )q; }

void lqueue_put(lqueue* q, lqentry* e) {
    assert(q); assert(e); assert(e->next == 0);

    // make this node owned by the queue
    e->next = owner(q);
    while (1) {
    retry:;
        lqentry* h = load(q->head);
        if (h == 0) {                  // if the queue is empty
            if (lqueue_cas(&q->head, e, 0)) { // try to update the head
                q->tail = e;           // update the tail hint
                return;
            }
            continue;                  // failed to update head; retry
        }

        lqentry* t = load(q->tail);
        if (t == 0) t = h;                  // if tail hint is obviously stale (and wrong)

        while (t->next != owner(q)) {       // try to get to the real tail
            t = t->next;
            if (t == 0) goto retry;         //continue; invalid tail hint
            if (t == owner(q)) goto retry;  //continue; invalid tail hint
        }

        if (lqueue_cas(&t->next, e, owner(q))) {   // try to update the last node
            q->tail = e;                    // update the tail hint
            return;
        }
    }
}

lqentry* lqueue_get(lqueue* q) {
    while (1) {
        lqentry* h = load(q->head);
        if (h == 0) return 0;

        // there are actually items in the queue
        lqentry* n = h->next;
        if (lqueue_cas(&h->next, 0, n)) {           // take ownership of head node
            if (n == owner(q)) {             // if it is a single element queue
                assert(lqueue_cas(&q->head, 0, h)); // update head to zero; we have ownership so it must succeed
                q->tail = 0;                 // update tail to zero too
            } else {
                assert(lqueue_cas(&q->head, n, h)); // advance head to next; we have ownership so it must succeed
            }
            return h;
        }
    }
}

lqentry* lqueue_peek(lqueue* q) {
    return load(q->head);
}

