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

<<<<<<< HEAD
const size_t PAGE_SIZE = 4096;
const int free_freq = 1000;
const long SIZE_CAP = 1024;
static hm_stats stats; // This initializes the stats to 0.
static mem_node* get_freed_things;
__thread int frees = 0;
=======
static const size_t PAGE_SIZE = 4096;
static const size_t NUM_BINS = 8;
static const size_t BIN_SIZES[] = {32, 64, 128, 256, 516, 1024, 2048, 4096};
static mem_node* bins[8];

>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static __thread mem_node* free_list;

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
<<<<<<< HEAD
=======
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
>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc
}

// Initializes pages new memory of as a mem_node
mem_node*
mem_node_init(size_t pages)
{
<<<<<<< HEAD
	stats.pages_mapped += pages;
=======
>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc
	size_t size = pages * PAGE_SIZE;
	mem_node* n = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	n->size = size;
	n->next = NULL;
	return n;
}

<<<<<<< HEAD
mem_node*
freed_things_list_pop(size_t size)
{
	pthread_mutex_lock(&mutex);
	mem_node* prev = NULL;
	mem_node* cur = get_freed_things;

	while (cur != NULL) {
		if (cur->size > size) {
			if (prev != NULL) {
				prev->next = cur->next;
			} else {
				// if prev is null, cur == free_list
				get_freed_things = cur->next;
			}

			cur->next = NULL;
			pthread_mutex_unlock(&mutex);
			return cur;
		}

		prev = cur;
		cur = cur->next;
	}
	pthread_mutex_unlock(&mutex);
	return NULL;
}


// "pops" a mem_node with size bytes available off of the free_list
=======
// "pops" the first element off of the passed bin
>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc
mem_node*
bin_pop(int bin)
{
<<<<<<< HEAD
	//pthread_mutex_lock(&mutex);
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
			//pthread_mutex_unlock(&mutex);
			return cur;
		}

		prev = cur;
		cur = cur->next;
	}
	//pthread_mutex_unlock(&mutex);
	return freed_things_list_pop(size);
	
=======
    pthread_mutex_lock(&mutex);
	mem_node* node = bins[bin];
	if (node != NULL) {
		bins[bin] = node->next;
	}
    pthread_mutex_unlock(&mutex);
	return node;
>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc
}

// "pops" a mem_node from the smallest bin that can fulfill size
mem_node*
bins_pop(size_t size)
{
<<<<<<< HEAD
	size_t size = left->size + right->size + sizeof(mem_node);
	left->size = size;
	left->next = right->next;
=======
	size_t rounded_size = get_rounded_size(size);
    for (int i = 0; i < NUM_BINS; i++) {
        if (BIN_SIZES[i] >= rounded_size && bins[i] != NULL) {
            return bin_pop(i);
        }
    }
	return NULL;
>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc
}

// Splits a node into two nodes, with the first node beings the passed size
// and the second node being the remainder
//
// Returns node2, since the caller of split_node aready has a pointer to node
mem_node*
split_node(mem_node* node, size_t size)
{
<<<<<<< HEAD
	//pthread_mutex_lock(&mutex);

	if (free_list == NULL || node < free_list) {
		node->next = free_list;
		free_list = node;
		//pthread_mutex_unlock(&mutex);
		return;
	}

	mem_node* insertion_start = NULL;
	mem_node* insertion_end = free_list;	

	while (insertion_end != NULL && node > insertion_end) {
		if (insertion_start && (insertion_end - insertion_start) == 1) {
			mem_node_merge(insertion_start, insertion_end);
			insertion_end = insertion_start->next;
		} else {
			insertion_end = insertion_end->next;
		}
	}

	if (insertion_start != NULL) {
		insertion_start->next = node;
	}
	node->next = insertion_end;
	//pthread_mutex_unlock(&mutex);
}

// Inserts into the global free, maintaining sorted order
void
freed_things_list_insert( mem_node* node)
{
	//pthread_mutex_lock(&mutex);

	if (get_freed_things == NULL || node < get_freed_things) {
		node->next = get_freed_things;
		get_freed_things = node;
		//pthread_mutex_unlock(&mutex);
		return;
	}

	mem_node* insertion_start = NULL;
	mem_node* insertion_end = get_freed_things;	

	while (insertion_end != NULL && node > insertion_end) {
		if (insertion_start && (insertion_end - insertion_start) == 1) {
			mem_node_merge(insertion_start, insertion_end);
			insertion_end = insertion_start->next;
		} else {
			insertion_end = insertion_end->next;
		}
	}

	if (insertion_start != NULL) {
		insertion_start->next = node;
	}
	node->next = insertion_end;
	//pthread_mutex_unlock(&mutex);
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

long
list_total_size(mem_node* alist)
{
    mem_node* cur = alist;
    long size = 0;
    
    while (cur != NULL) {
        size += cur->size;
        cur = cur->next;
    }
    
    return size;
=======
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
    pthread_mutex_lock(&mutex);
    if (bin_number >= NUM_BINS || node->size != BIN_SIZES[bin_number]) {
        //printf("ERROR: Invalid insert of %lu into bin %lu\n", node->size, BIN_SIZES[bin_number]);
    } else {
        node->next = bins[bin_number];
        bins[bin_number] = node;
    }
    pthread_mutex_unlock(&mutex);
>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc
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

void
full_coalesce(mem_node* alist)
{
    void* start;
    void* stop;
    mem_node* cur = alist;
    while (cur != NULL && cur->next != NULL) {
	stop = (void*)cur + cur->size;
	start = cur->next;
	if (stop == start) {
	    cur->size += cur->next->size;
	    cur->next = cur->next->next;
	} 
	cur = cur->next;
		
    }  

}

void
add_all(mem_node* get_freed_things, mem_node* free_list) 
{
    pthread_mutex_lock(&mutex);
    if (get_freed_things == NULL) {
	    get_freed_things = free_list;
    } else {
    	mem_node* cur = free_list;
    	while (cur != NULL) {
    	    freed_things_list_insert(cur);
    	    cur = cur->next;
    	}
    }
    
    full_coalesce(get_freed_things);
    pthread_mutex_unlock(&mutex);
}


void*
xmalloc(size_t size)
{
    size += sizeof(size_t);

	mem_node* to_alloc = NULL;
	mem_node* new_node = NULL;
	size_t pages = div_up(size, PAGE_SIZE);

<<<<<<< HEAD
	if (pages == 1) {
		to_alloc = free_list_pop(size);
	} else {
		size = pages * PAGE_SIZE;	
	}

	if (to_alloc == NULL) {
		to_alloc = mem_node_init(pages);
	}

	size_t new_node_size = to_alloc->size - size;	

	// I know this looks redundant, but for some reason the > 0
	// conditional prevents a segfault on one of my test machines
	if (new_node_size > 0 && new_node_size > sizeof(mem_node)) {
		new_node = ((void*) to_alloc) + size;
		new_node->size = new_node_size;
		free_list_insert(new_node);
=======
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
>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc
	}

	void* item = ((void*) to_alloc) + sizeof(size_t);
	set_size(item, size);
<<<<<<< HEAD
=======
	//binstatus();
>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc

	return item;
}



void
xfree(void* item)
{
<<<<<<< HEAD
    stats.chunks_freed += 1;
    frees += 1;
    

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
    if (frees >= free_freq && list_total_size(free_list) > SIZE_CAP) {
    
        add_all(get_freed_things, free_list);
    }
=======
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
>>>>>>> 9e32750d660ab4614c0c116f6e871a1e91bd70dc
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
