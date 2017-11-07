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

const size_t PAGE_SIZE = 4096;

const size_t NUM_BINS = 1;
const size_t BIN_SIZES[] = {4096};
static mem_node* bins[1];

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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
    pthread_mutex_lock(&mutex);
	mem_node* node = bins[bin];
	if (node != NULL) {
		bins[bin] = node->next;
	}
    pthread_mutex_unlock(&mutex);
	return node;
}

// "pops" a mem_node from the smallest bin that can fulfill size
mem_node*
bins_pop(size_t size)
{
	size_t rounded_size = get_rounded_size(size);
    for (int i = 0; i < NUM_BINS; i++) {
        if (BIN_SIZES[i] >= rounded_size && bins[i] != NULL) {
            return bins_pop(i);
        }
    }
	return NULL;
}

// Inserts into the passed bin, if the size is correct
void
bin_insert(mem_node* node, int bin_number)
{
    pthread_mutex_lock(&mutex);
    if (bin_number >= NUM_BINS || node->size != BIN_SIZES[bin_number]) {
        printf("ERROR: Invalid insert of %lu into bin %lu\n", node->size, BIN_SIZES[bin_number]);
    } else {
        node->next = bins[bin_number];
        bins[bin_number] = node;
    }
    pthread_mutex_unlock(&mutex);
}


// Inserts into the free list, maintaining sorted order
void
bins_insert(mem_node* node)
{
	int bin_number = get_bin_number(node->size);
	bin_insert(node, bin_number);
}

void*
xmalloc(size_t size)
{
    size += sizeof(size_t);

	mem_node* to_alloc = NULL;
	mem_node* new_node = NULL;
	size_t pages = div_up(size, PAGE_SIZE);

	if (pages == 1) {
		size = get_rounded_size(size);
		to_alloc = bins_pop(size);

		if (to_alloc == NULL) {
			to_alloc = mem_node_init(1);
			// TODO: With smaller bins, distribute this node
		}
	} else {
		size = pages * PAGE_SIZE;	
		to_alloc = mem_node_init(pages);
	}

	void* item = ((void*) to_alloc) + sizeof(size_t);
	set_size(item, size);

	return item;
}

void
xfree(void* item)
{
	size_t size = get_size(item);
	item = item - sizeof(size_t);

	if (size >= PAGE_SIZE) {
		munmap(item, size);
	} else {
		mem_node* new_node = (mem_node*) (item);
		new_node->size = size;
		bins_insert(new_node);
	}
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
