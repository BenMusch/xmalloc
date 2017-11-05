#ifndef BARRIER_H
#define BARRIER_H

#include <p_thread.h>

typedef struct barrier {
    int count;
    int seen;
} barrier;


barrier* make_barrier(int nn);
void barrier_wait(barrier* bb);
void free_barrier(barrier* bb);

#endif
