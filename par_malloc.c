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


/**
 * =======
 * GLOBALS
 * =======
 */
const size_t PAGE_SIZE = 4096;

const size_t NUM_BINS = 10;
const size_t BIN_SIZES[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
static mem_node* bins[10];

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * =================
 * UTILITY FUNCTIONS
 * =================
 */
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

void
binstatus() {
	printf("===========================================\n");
	for (int i=0; i < NUM_BINS; i++) {
		printf("%lu: %lu\n", BIN_SIZES[i], bins_list_length(i));
	}
	printf("===========================================\n");
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

void
mem_node_merge(mem_node* left, mem_node* right)
{
    size_t size = left->size + right->size + sizeof(mem_node);
    left->size = size;
    left->next = right->next;
}

/**
 * ================
 * ALLOCATION LOGIC
 * ================
 */

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

// Inserts into the passed bin, if the size is correct
void
bin_insert(mem_node* node, int bin_number)
{
	if (node == NULL || bin_number >= NUM_BINS || node->size != BIN_SIZES[bin_number]) {
		//printf("ERROR: Invalid insert of %lu into bin %lu\n", node->size, BIN_SIZES[bin_number]);
	} else {
		node->next = bins[bin_number];
		bins[bin_number] = node;
	}
}

// Splits the passed node into two nodes
// node will be size, node2 will be size - node->size
// returns node2
mem_node*
split_node(mem_node* node, size_t size)
{
	//printf("splitting %p from %lu to %lu\n", node, node->size, size);
	if (node->size - size < sizeof(mem_node)) {
		node->size = size;
		node->next = NULL;
		return NULL;
	}
	mem_node* node2 = (mem_node*) (((void*) node) + size);
	node2->size = node->size - size;
	node->size = size;

	//printf("split (%p size=%lu) and (%p size=%lu)\n", node, node->size, node2, node2->size);

	node->next = NULL;
	node2->next = NULL;

	return node2;
}

// Take size from node, then distribute the remainder in the bins
// returns node
mem_node*
distribute_node(mem_node* node, size_t size)
{
	size_t next_bin = 9;
    pthread_mutex_lock(&mutex);

	mem_node* return_node = node;
	node = split_node(return_node, size);
	size_t leftover_size = 0;

	if (node != NULL) {
		leftover_size = node->size;
	}

	if (leftover_size > BIN_SIZES[0]) {
		node = (mem_node*) (((void*) node) + size);
		node->size = leftover_size;

		while (next_bin >= 0 && leftover_size > 0 && node != NULL) {
			size_t to_insert_size = BIN_SIZES[next_bin];

			if (leftover_size < to_insert_size) {
				next_bin -= 1;
				continue;
			}

			mem_node* to_insert = node;
			node = split_node(to_insert, to_insert_size);
			bin_insert(to_insert, next_bin);
			leftover_size -= to_insert_size;
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
	int bin_number;
	
	for (int i = 0; i < NUM_BINS; i++) {
		if (BIN_SIZES[i] >= rounded_size && bins[i] != NULL) {
			mem_node* node = bins[i];
			node = distribute_node(bins[i], rounded_size);
			return node;
		}
	}
	
    return NULL;
}


void*
xmalloc(size_t size)
{
    size += sizeof(size_t);

    mem_node* to_alloc;
    mem_node* new_node;
    size_t pages = div_up(size, PAGE_SIZE);

    if (pages == 1) {
		size = get_rounded_size(size);
        to_alloc = bins_list_pop(size);

		if (to_alloc == NULL)  {
			to_alloc = mem_node_init(1);
			to_alloc = distribute_node(to_alloc, size);
		}
    } else {
        size = pages * PAGE_SIZE;
        to_alloc = mem_node_init(pages);
    }

    void* item = ((void*) to_alloc) + sizeof(size_t);
    set_size(item, size);
	printf("MALLOC %p: %lu\n", item, get_size(item));

    return item;
}

void
xfree(void* item)
{
	if (item == NULL) {
		printf("FREE NULL??\n");
		return;
	}
    size_t size = get_size(item);
	printf("FREE %p: %lu\n", item, size);
    item = item - sizeof(size_t);

    if (size >= PAGE_SIZE) {
        munmap(item, size);
    } else {
        mem_node* new_node = (mem_node*) (item);
        new_node->size = size;
		pthread_mutex_lock(&mutex);
        bin_insert(new_node, get_bin_number(new_node->size));
		pthread_mutex_unlock(&mutex);
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
