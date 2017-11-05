
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include "barrier.h"
#include "hmalloc.h"

void free_node_init(void* page);

/*
  typedef struct hm_stats {
  long pages_mapped;
  long pages_unmapped;
  long chunks_allocated;
  long chunks_freed;
  long free_length;
  } hm_stats;
*/

typedef struct node {
    size_t size;
    struct node* next;
}node;

typedef struct header {
    size_t size;
}header;


const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.
node* fl = NULL;
barrier* bb = NULL;



long
free_list_length()
{
    node* cur = fl;
    int nn = 0;
    while (cur != NULL) {
        nn++;
        cur = cur->next;
    }
    return nn;
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

void
full_coalesce()
{
    void* start;
    void* stop;
    node* cur = fl;
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
coalesce(node* n)
{
    if (n->next != NULL) {
        void* this_stop = (void*)n + n->size;
        void* next_start = n->next;
        
        if(this_stop == next_start) {
            n->size += n->next->size;
            n->next = n->next->next;
        }
    }
    full_coalesce();
}

void
insert_node(node* n) {
    if (fl == NULL) {
        fl = n;
	fl->next = NULL;
    } else if (fl->next == NULL && fl < n) {
    	n->next = NULL;
	fl->next = n;
	
    } else {
        node* cur = fl;
        if (n < cur) {
            node* next = fl;
	    fl = n;
	    fl->next = next;
        } else {
	    node* prev = fl;
	    //prev = NULL;
    	    cur = fl;
            while (cur != NULL) {
                if (n < cur) {
		    n->next = cur;
		    prev->next = n;
		    break;
		} else {
		    prev = cur;
		    cur = cur->next;
		}
        
            }
        }
    }    
    coalesce(n);
}



void*
hmalloc(size_t size)
{
    stats.chunks_allocated += 1;
    size += sizeof(size_t);

    if (bb == NULL) {
	bb = make_barrier();
    }    

    // TODO: Actually allocate memory with mmap and a free list.
     
    
    if (size >= PAGE_SIZE) {
    	//calc the number of pages they need in memory, take the extra stuff they dont need and put it onto free list
	size_t pages = div_up(size, PAGE_SIZE);
	void* swath = mmap(NULL, PAGE_SIZE * pages, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	node* big_node = swath;
	big_node->size = pages * PAGE_SIZE;
	stats.pages_mapped += pages;
	return (void*)big_node + sizeof(size_t);
    } else {
        if (fl != NULL) {
	    node* cur = fl;
	    node* prev = fl;

	    while (cur != NULL) {
		if (size <= cur->size) {
		    if (size + sizeof(node) > cur->size) {
			if (fl == cur) {
			    void* ret_val = (void*)fl;
			    fl = fl->next;
			    return ret_val + sizeof(size_t);
			} else {
			    prev->next = cur->next;
			    return (void*)cur + sizeof(size_t);
			}
		     } else {
			//node* new_cause_why = cur;
			//new_cause_why->size = new_cause_why->size - size;
		 	
			cur->size = cur->size - size;
			node* ret_val = (void*)cur + cur->size; 
			ret_val->size = size;
			return (void*)ret_val + sizeof(size_t);
		     }
		} else {
			prev = cur;
			cur = cur->next;
		}

	    }
	    node* new_stuff = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	    stats.pages_mapped += 1;
	    new_stuff->size = size;
	    node* second_half =(void*)new_stuff + size;
	    second_half->size = 4096 - size;
	    second_half->next = NULL; 
	    insert_node(second_half);
	   // prev->next = (node*)second_half;
	   // prev->next->size = 4096 - size;
	   // prev->next->next = NULL;
	    return ((void*)new_stuff) + 8;
	} else {
            //malloc a page and add it to the free list, then do above step
            node* new_block = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
 	    stats.pages_mapped += 1;
	    new_block->size = size;
	    fl = (void*)new_block + size;
            fl->size = 4096 - size;
            fl->next = NULL;
           return (void*)new_block + sizeof(size_t);
        }
    }
}

void
hfree(void* item)
{
        stats.chunks_freed += 1;

    // TODO: Actually free the item.
    node* fnode = (node*)(item - sizeof(size_t));
    if (fnode->size >= PAGE_SIZE) {
	stats.pages_unmapped += div_up(fnode->size, PAGE_SIZE);
	munmap(fnode, fnode->size);
	
    } else {
	insert_node(fnode);
	full_coalesce();
    }

}


void*
realloc(void* ptr, size_t size)
{
    void* new = hmalloc(size);
    for (int ii = 0; ii < size; ++ii) {
        new[ii] = ptr[ii];
    }
    hfree(ptr);
    return new;

}
