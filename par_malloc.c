#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "bens/hw07/hmalloc.h"
#include "xmalloc.h"

typedef struct mem_node {
	size_t size;
	struct mem_node* next;
} mem_node;

static const size_t PAGE_SIZE = 4096;
static const size_t NUM_BINS = 8;
static const size_t BIN_SIZES[] = {32, 64, 128, 256, 516, 1024, 2048, 4096};
static pthread_mutex_t locks[] = {
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER
};
static mem_node* bins[8];

size_t
bin_count(int bin_number)
{
	size_t count = 0;
	mem_node* cur = bins[bin_number];
	while (cur != NULL) {
		count++;
		cur = cur->next;
	}
	return count;
}

void
binstatus() {
    printf("===========================================\n");
    for (int i=0; i < NUM_BINS; i++) {
        printf("%lu: %lu\n", BIN_SIZES[i], bin_count(i));
    }
    printf("===========================================\n");
}

static
size_t
div_up(size_t xx, size_t yy)
{
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    } else {
        return zz + 1;
    }
}

size_t
get_size(void* item)
{
	return *(size_t*)(item - sizeof(size_t));
}

void
set_size(void* item, size_t size)
{
	size_t* alloc_size = (size_t*) (item - sizeof(size_t));
	*alloc_size = size;
}

// Returns the bin number to store a elements of the passed size
int
get_bin_number(size_t size)
{
    for (int i = 0; i < NUM_BINS - 1; i++) {
        if (size <= BIN_SIZES[i]) {
            return i;
        }
    }

    return NUM_BINS - 1;
}

// Rounds the passed size to be a bin size
size_t
get_rounded_size(size_t size)
{
    for (int i = 0; i < NUM_BINS; i++) {
        if (size <= BIN_SIZES[i]) {
            return BIN_SIZES[i];
        }
    }

    return -1;
}

// Initializes pages new memory of as a mem_node
mem_node*
mem_node_init(size_t pages)
{
	size_t size = pages * PAGE_SIZE;
	mem_node* n = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	n->size = size;
	n->next = NULL;
	return n;
}

// "pops" the first element off of the passed bin
mem_node*
bin_pop(int bin)
{
    pthread_mutex_lock(&locks[bin]);
	mem_node* node = bins[bin];
	if (node != NULL) {
		bins[bin] = node->next;
	}
    pthread_mutex_unlock(&locks[bin]);
	return node;
}

// "pops" a mem_node from the smallest bin that can fulfill size
mem_node*
bins_pop(size_t size)
{
	size_t rounded_size = get_rounded_size(size);
    for (int i = 0; i < NUM_BINS; i++) {
        if (BIN_SIZES[i] >= rounded_size && bins[i] != NULL) {
            return bin_pop(i);
        }
    }
	return NULL;
}

// Splits a node into two nodes, with the first node beings the passed size
// and the second node being the remainder
//
// Returns node2, since the caller of split_node aready has a pointer to node
mem_node*
split_node(mem_node* node, size_t size)
{
	size_t node2_size = node->size - size;	
	node->next = NULL;
	node->size = size;

	if (node2_size > sizeof(size_t)) {
		mem_node* node2 = (mem_node*) (((void*) node) + size);
		node2->size = node2_size;
		node2->next = NULL;

		return node2;
	}

	return NULL;
}

// Inserts into the passed bin, if the size is correct
void
bin_insert(mem_node* node, int bin_number)
{
    pthread_mutex_lock(&locks[bin_number]);
    if (bin_number >= NUM_BINS || node->size != BIN_SIZES[bin_number]) {
        //printf("ERROR: Invalid insert of %lu into bin %lu\n", node->size, BIN_SIZES[bin_number]);
    } else {
        node->next = bins[bin_number];
        bins[bin_number] = node;
    }
    pthread_mutex_unlock(&locks[bin_number]);
}


// Inserts into the free list, maintaining sorted order
void
bins_insert(mem_node* node)
{
	int bin_number = get_bin_number(node->size);
	bin_insert(node, bin_number);
}

// Given a node, "distributes" is between bins that it can fit into
void
distribute_node(mem_node* node)
{
	int bin_number = NUM_BINS - 1;
	mem_node* tmp;
	
	while (bin_number >= 0) {
		if (BIN_SIZES[bin_number] > node->size) {
			bin_number--;
			continue;
		}
	
		tmp = split_node(node, BIN_SIZES[bin_number]);
		bin_insert(node, bin_number);
		
		if (tmp == NULL) {
			return;
		}
		
		node = tmp;	
	}
}

void*
xmalloc(size_t size)
{
    size += sizeof(size_t);

	mem_node* to_alloc = NULL;
	mem_node* new_node = NULL;
	size_t pages = div_up(size, PAGE_SIZE);

	//printf("MALLOC: %lu\n", size);

	if (pages == 1) {
		size = get_rounded_size(size);
		to_alloc = bins_pop(size);

		if (to_alloc == NULL) {
			to_alloc = mem_node_init(1);
		}
		
		mem_node* remainder_node = split_node(to_alloc, size);	

		if (remainder_node != NULL) {
			distribute_node(remainder_node);
		}
	} else {
		size = pages * PAGE_SIZE;	
		to_alloc = mem_node_init(pages);
	}

	void* item = ((void*) to_alloc) + sizeof(size_t);
	set_size(item, size);
	//binstatus();

	return item;
}

void
xfree(void* item)
{
	size_t size = get_size(item);
	item = item - sizeof(size_t);

	//printf("FREE: %lu\n", size);
	
	if (size >= PAGE_SIZE) {
		munmap(item, size);
	} else {
		mem_node* new_node = (mem_node*) (item);
		new_node->size = size;
		bins_insert(new_node);
	}
	//binstatus();
}

void*
xrealloc(void* ptr, size_t size)
{
    void* new = xmalloc(size);
	size += sizeof(size_t);

	memcpy(new, ptr, size);
    xfree(ptr);

    return new;
}
