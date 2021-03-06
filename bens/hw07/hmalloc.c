#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "hmalloc.h"

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
static mem_node* free_list;
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

// "pops" a mem_node with size bytes available off of the free_list
mem_node*
free_list_pop(size_t size)
{
	printf("About to lock!");
	pthread_mutex_lock(&mutex);
	printf("After lock");
	mem_node* prev = NULL;
	mem_node* cur = free_list;

	while (cur != NULL) {
		if (cur->size > size) {
			if (prev != NULL) {
				prev->next = cur->next;
			} else {
				// if prev is null, cur == free_list
				free_list = cur->next;
			}

			cur->next = NULL;
			printf("About to unlock! [found]");
			pthread_mutex_unlock(&mutex);
			printf("unlocked! [found]");
			return cur;
		}

		prev = cur;
		cur = cur->next;
	}
	printf("About to unlock!");
	pthread_mutex_unlock(&mutex);
	printf("unlocked!");
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
free_list_insert(mem_node* node)
{
	printf("About to lock [insert]");
	pthread_mutex_lock(&mutex);
	printf("locked [insert]");
	if (free_list == NULL) {
		free_list = node;
		return;
	}

	mem_node* insertion_start = NULL;
	mem_node* insertion_end = free_list;	

	while (insertion_end != NULL && node > insertion_end) {
		if (insertion_start && (insertion_end - insertion_start) == 1) {
			mem_node_merge(insertion_start, insertion_end);
			insertion_end = insertion_start->next;
		} else {
			insertion_start = insertion_end;
			insertion_end = insertion_end->next;
		}
	}

	if (insertion_start != NULL) {
		insertion_start->next = node;
	}
	node->next = insertion_end;
	printf("About to unlock [insert]");
	pthread_mutex_unlock(&mutex);
	printf("unlocked [insert]");
}

long
free_list_length()
{
	mem_node* cur = free_list;
	long len = 0;

	while (cur != NULL) {
		len++;
		cur = cur->next;
	}

	return len;
}

hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

void*
hmalloc(size_t size)
{
    stats.chunks_allocated += 1;
    size += sizeof(size_t);

	mem_node* to_alloc = NULL;
	mem_node* new_node = NULL;
	size_t pages = div_up(size, PAGE_SIZE);

	if (pages == 1) {
		to_alloc = free_list_pop(size);
	} else {
		size = pages * PAGE_SIZE;	
	}

	if (to_alloc == NULL) {
		to_alloc = mem_node_init(pages);
	}

	int new_node_size = to_alloc->size - size;	

	// I know this looks redundant, but for some reason the > 0
	// conditional prevents a segfault on one of my test machines
	if (new_node_size > 0 && new_node_size > sizeof(mem_node)) {
		new_node = ((void*) to_alloc) + size;
		new_node->size = (size_t) new_node_size;
		free_list_insert(new_node);
	}

	void* item = ((void*) to_alloc) + sizeof(size_t);
	set_size(item, size);

	return item;
}

void
hfree(void* item)
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
		free_list_insert(new_node);
	}
}

void*
hrealloc(void* ptr, size_t size)
{
    void* new = hmalloc(size);

	memcpy(new, ptr, get_size(ptr));
    hfree(ptr);

    return new;
}
