#include <sys/mman.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "bens/hw07/hmalloc.h"
#include "xmalloc.h"

/*
  typedef struct hm_stats {
  long pages_mapped;
  long pages_unmapped;
  long chunks_allocated;
  long chunks_freed;
  long free_length;
  } hm_stats;
*/

typedef struct mem_node {
    size_t size;
    struct mem_node* next;
} mem_node;

const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.

const size_t NUM_BINS = 10;
const size_t BIN_SIZES[] = {4, 8, 32, 64, 128, 256, 512, 1024, 2048, 4096};
static mem_node* bins[10];

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

long
bins_list_length(int bin)
{
    long len = 0;

	mem_node* cur = bins[bin];
	while (cur != NULL) {
		if (cur->size != BIN_SIZES[bin]) {
			printf("\nERROR: %lu should be %lu\n", cur->size, BIN_SIZES[bin]);
		}
		len++;
		cur = cur->next;
	}

    return len;
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

// Initializes pages new memory of as a mem_node
mem_node*
mem_node_init(size_t pages)
{
    stats.pages_mapped += pages;
    size_t size = pages * PAGE_SIZE;
    mem_node* n = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    n->size = size - sizeof(mem_node);
    n->next = NULL;
    return n;
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

void
binstatus() {
	printf("===========================================\n");
	for (int i=0; i < NUM_BINS; i++) {
		printf("%lu: %lu\n", BIN_SIZES[i], bins_list_length(i));
	}
	printf("===========================================\n");
}

mem_node*
split_node(mem_node* node, size_t size)
{
    pthread_mutex_lock(&mutex);
	binstatus();
	size_t leftover_size = node->size - size;
	size_t next_bin = 9;

	//printf("NODE SIZE: %lu | REQUESTED SIZE: %lu | LEFTOVER SIZE: %lu\n", node->size, size, leftover_size);

	mem_node* return_node = node;
	return_node->size = size;

	if (leftover_size > BIN_SIZES[0]) {
		node = (mem_node*) (((void*) node) + size);
		node->size = leftover_size;

		while (next_bin >= 0) {
			size_t to_insert_size = BIN_SIZES[next_bin];

			//printf("CUR BIN: %lu | CUR LEFTOVER: %lu\n", to_insert_size, leftover_size);

			if (leftover_size < to_insert_size) {
				next_bin -= 1;
				//printf("skipping\n");
				continue;
			}

			//printf("inserting\n");

			mem_node* to_insert = node;
			to_insert->size = to_insert_size;
			to_insert->next = bins[next_bin];
			bins[next_bin] = to_insert;
			leftover_size -= to_insert_size;
			
			node = (mem_node*) (((void*) node) + to_insert_size);
			node->size = leftover_size;
		}
	}

	pthread_mutex_unlock(&mutex);
	return return_node;
}

// "pops" a mem_node with size bytes available off of the bins
mem_node*
bins_list_pop(size_t size)
{
	size_t rounded_size = get_rounded_size(size);
	//printf("POP SIZE %lu | ROUNDED: %lu\n", size, rounded_size);
	int bin_number;
	
	for (int i = 0; i < NUM_BINS; i++) {
		if (BIN_SIZES[i] >= rounded_size && bins[i] != NULL) {
			mem_node* node = split_node(bins[i], rounded_size);	
		}
	}
	
    return NULL;
}

void
mem_node_merge(mem_node* left, mem_node* right)
{
    size_t size = left->size + right->size + sizeof(mem_node);
    left->size = size;
    left->next = right->next;
}

// Inserts into the free list, maintaining sorted order
void
bins_insert(mem_node* node)
{
    pthread_mutex_lock(&mutex);
	int bin_number = get_bin_number(node->size);

	node->next = bins[bin_number];
	bins[bin_number] = node;
    pthread_mutex_unlock(&mutex);
}

void*
xmalloc(size_t size)
{
    stats.chunks_allocated += 1;
    size += sizeof(size_t);

    mem_node* to_alloc = NULL;
    mem_node* new_node = NULL;
    size_t pages = div_up(size, PAGE_SIZE);

    if (pages == 1) {
		size = get_rounded_size(size);
        to_alloc = bins_list_pop(size);
    } else {
        size = pages * PAGE_SIZE;
    }

    if (to_alloc == NULL) {
        to_alloc = mem_node_init(pages);
    }

    void* item = ((void*) to_alloc) + sizeof(size_t);
    set_size(item, size);

    return item;
}

void
xfree(void* item)
{
    stats.chunks_freed += 1;

    size_t size = get_size(item);
    item = item - sizeof(size_t);

    if (size >= PAGE_SIZE) {
        munmap(item, size);
        stats.pages_unmapped += size / PAGE_SIZE;
    } else {
        mem_node* new_node = (mem_node*) (item);
        new_node->size = size;
		//printf("FREE: %lu\n", size);
        bins_insert(new_node);
    }
}

void*
xrealloc(void* ptr, size_t size)
{
    void* new = xmalloc(size);

    memcpy(new, ptr, get_size(ptr));
    xfree(ptr);

    return new;
}
