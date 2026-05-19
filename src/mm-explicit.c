/*
 * mm-implicit.c - The best malloc package EVAR!
 * TODO (bug): mm_realloc and mm_calloc don't seem to be working...
 * TODO (bug): The allocator doesn't re-use space very well...
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/** The required alignment of heap payloads */
const size_t ALIGNMENT = 2 * sizeof(size_t);
/** The layout of each block allocated on the heap */
typedef struct block_t block_t;

struct block_t {
    /** The size of the block and whether it is allocated (stored in the low bit) */
    size_t header;
    union {
	    struct {
		    block_t *next;
		    block_t *prev;
	    };
    /**
     * We don't know what the size of the payload will be, so we will
     * declare it as a zero-length array.  This allow us to obtain a
     * pointer to the start of the payload.
     */
    uint8_t payload[0];
    };
};

/** The first and last blocks on the heap */
static block_t *mm_heap_first = NULL;
static block_t *mm_heap_last = NULL;
static block_t *search_start = NULL;
static block_t *free_list_head = NULL;
static size_t *get_footer(block_t * block ) {
	return (size_t*) ((uint8_t*)block + get_size(block) - sizeof(size-t);
}
static void set_footer(block_t * block ) {
	*get_footer(block) = block->header;
}

static block_t *get_prev_block ( block_t *block ) {
size_t prev_footer = *(size_t)((uint_t* ) block - sizeof(size_t));
size_t prev_size = prev_footer & ~(ALIGNMNET -1 );
return (block_t* ) ((uint8_t*) block - prev_size);
}
/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}

/** Set's a block's header with the given size and allocation state */
static void set_header(block_t *block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
    set_footer(block);
}

/** Extracts a block's size from its header */
static size_t get_size(block_t *block) {
    return block->header & ~1;
}

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t *block) {
    return block->header & 1;
}

/**
 * Finds the first free block in the heap with at least the given size.
 * If no block is large enough, returns NULL.
 */
static block_t *find_fit(size_t size) {
    // Traverse the blocks in the heap using the implicit list
    for ( block_t * curr = free_list_head;
		    curr != NULL;
		    curr = curr->next)
	    if ( get_size(curr)>= size ) {
		    return curr;
	   }
    return NULL;
}

/** Gets the header corresponding to a given payload pointer */
static block_t *block_from_payload(void *ptr) {
    return (block_t*)((uint8_t*)ptr - offsetof(block_t, payload));
}


/**
 * mm_init - Initializes the allocator state
 */
bool mm_init(void) {
    // We want the first payload to start at ALIGNMENT bytes from the start of the heap
    void *padding = mem_sbrk(ALIGNMENT - sizeof(block_t));
    if (padding == (void *) -1) {
        return false;
    }

    // Initialize the heap with no blocks
    mm_heap_first = NULL;
    mm_heap_last = NULL;
    free_list_head = NULL;
    search_start = mm_heap_first;
    return true;
}

/**
 * mm_malloc - Allocates a block with the given size
 */
void *mm_malloc(size_t size) {
    // The block must have enough space for a header and be 16-byte aligned
    size = round_up( size + 2 * sizeof(size_t), ALIGNMENT);
    if ( size < 32 ) {
	    size = 32;
    }
    // If there is a large enough free block, use it
    block_t *block = find_fit(size);
    list_remove(block);
    if (block != NULL) {
        size_t old_size = get_size(block);
	size_t remaining = old_size - size;
	if ( remaining >= ALIGNMENT ) {
		set_header(block,size,true);
		block_t* new_block = (block_t*)((uint8_t*)block + size );
		set_header(new_block, remaining, false);
		if ( block == mm_heap_last) {
			mm_heap_last = new_block;
		}
		search_start = new_block;
	} else {
		set_header(block,old_size,true);
		search_start = (block_t*) ((uint8_t*)block + old_size);
		if ( search_start > mm_heap_last) {
			search_start = mm_heap_first;
		}
	}
	return block->payload;
}


    // Otherwise, a new block needs to be allocated at the end of the heap
    block = mem_sbrk(size);
    if (block == (void *) -1) {
        return NULL;
    }

    // Update mm_heap_first and mm_heap_last since we extended the heap
    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;

    // Initialize the block with the allocated size
    set_header(block, size, true);
    return block->payload;
}

/**
 * mm_free - Releases a block to be reused for future allocations
 */
void mm_free(void *ptr) {
    // mm_free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }

    // Mark the block as unallocated
    block_t *block = block_from_payload(ptr);
    set_header(block,get_size(block), false);
    block_t *next = get_next_block(block);
    if ( next != NULL && !is_allocated(next)) {
	    list_remove(next);
	    size_t new_size = get_size(block) + get_size(next);
	    if ( next == mm_heap_last) {
		    mm_heap_last = block;
	    }
	    set_header(block,new_size,false);
	  
    }
    if ( block != mm_heap_first ) {
	    block_t *prev = get_prev_block(block);
	    if ( !is_allocated(prev)) {
		    list_remove(prev);
		    size_t new_size = get_size(prev) + get_size(block);
		    if ( block == mm_heap_last ) {
			    mm_heap_last = prev;
		    }
		    block = prev;
		    set_header(block, new_size, false);
	    }
   }
   list_add(block);
}

/**
 * mm_realloc - Change the size of the block by mm_mallocing a new block,
 *      copying its data, and mm_freeing the old block.
 */
void *mm_realloc(void *old_ptr, size_t size) {
    if ( old_ptr == NULL ) {
	    return mm_malloc(size);
    }
    if ( size == 0 ) { mm_free(old_ptr); return NULL; }
    block_t* old_block = block_from_payload(old_ptr);
    size_t old_payload_size = get_size(old_block) - 2*sizeof(size_t);
    void* new_ptr = mm_malloc(size);
    if ( new_ptr == NULL ) {
	    return NULL;
    }
    size_t copy_size = size;
    if ( old_payload_size < copy_size ) {
	    copy_size = old_payload_size;
    }
    memcpy(new_ptr,old_ptr,copy_size);
    mm_free(old_ptr);
    return new_ptr;
}

/**
 * mm_calloc - Allocate the block and set it to zero.
 */
void *mm_calloc(size_t nmemb, size_t size) {
    if ( nmemb != 0 && size > SIZE_MAX /nmemb ) {
	    return NULL;
   }

	size_t total = nmemb * size;
    void* ptr = mm_malloc(total);
    if ( ptr == NULL ) {
	    return NULL;
    }
    memset(ptr,0,total);
    return ptr;
}

/**
 * mm_checkheap - So simple, it doesn't need a checker!
 */
void mm_checkheap(void) {

}

static void list_add(block_t *block) {
	block -> prev = NULL;
	block->next = free_list_head;
	if ( free_list_head != NULL ) {
		free_list_head->prev = block;
	}
	free_list_head = block;
}
static void list_remove ( block_t *block) {
	if ( block->prev != NULL ) {
		block->prev->next = block->next;
	}  else {
		free_list_head = block->next;
	}
	if ( block->next != NULL ) {
		block->next->prev = block->prev;
	}
	block -> next = NULL;
	block -> prev = NULL;
}
