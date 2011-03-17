#include "lqueue.h"
#include <pthread.h>
#include <unistd.h>

#define HAVE_DEBUG

#include "debug.h"

static lqueue *q;

struct count {
    lqentry entry;
    int i;
};

#define COUNT 10000

void * reader(void *data) {
    for (int i = 0; i < 3 * COUNT; i++) {
        if (i % 73 == 0) usleep(5000);
        struct count *c;
        int yielded = 0;
        while ((c = (struct count *)lqueue_get(q)) == 0) {
            if (yielded++ == 0) print("reader: yield");
            //assert(yielded < 10000000);
            yielded++;
            sched_yield();
        }

        print("reading: %d", c->i);
        //assert(c->i == i);
    }
    return 0;
}

void * writer(void *data) {
    for (int i = 0; i < COUNT; i++) {
        print("writing: %d", i);

        if (i % 33 == 0) usleep(5000);

        struct count *c = malloc(sizeof(struct count));
        lqentry_init((lqentry *)c);
        c->i = i;
        lqueue_put(q, (lqentry *)c);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (1 == 0) fatal("oeps");

    q = lqueue_new();

    pthread_t treader;
    pthread_create(&treader, 0, reader, 0);

    pthread_t twriter1;
    pthread_create(&twriter1, 0, writer, 0);
    pthread_t twriter2;
    pthread_create(&twriter2, 0, writer, 0);
    pthread_t twriter3;
    pthread_create(&twriter3, 0, writer, 0);

    pthread_join(treader, 0);

    return 0;
}

