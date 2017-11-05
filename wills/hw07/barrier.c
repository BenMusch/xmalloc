#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include "barrier.h"

pthread_mutex_t mutex;
pthread_cond_t condv;

barrier*
make_barrier(int nn) {
    barrier bb = malloc(sizeof(barrier));
    assert(bb != 0);
    
    bb->count = nn;
    bb->seen = 0;
    pthread_mutex_init(&mutex, 0);
    pthread_cond_init_(&condv, 0);
    return bb;
}

void
barrier_wait(barrier* bb)
{
    pthread_mutex_lock(&mutex);
    bb->seen +=1;
    while(bb->seen < bb->count) {
        pthread_cond_wait(&condv, &mutex);
    }
    
    pthread_mutex_unlock(&mutex);
    pthread_cond_broadcast(&condv);    
}


void
free_barrier(barrier* bb) 
{
    free(bb);
}
