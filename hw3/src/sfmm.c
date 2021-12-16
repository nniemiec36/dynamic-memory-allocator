/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"
#include "errno.h"

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       16       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       32       /* Double word size (bytes) */

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc, palloc)  ((size) | (alloc) | (palloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc
#define GET_PALLOC(p)(GET(p) & 0x2)
/* $end mallocmacros */

int splinter = 0;

/* Function prototypes for internal helper routines */
static void *find_fit(size_t asize);
static void *coalesce(void *bp);

void *initialize_heap(){
    int i = 0;
    for(i = 0; i < NUM_FREE_LISTS; i++){
		sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
		sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
	}
	/*create the initial empty heap*/

	void * pp = sf_mem_grow();
	if(pp == NULL){
		sf_errno = ENOMEM;
		return pp; // don't know how to error handle
	}

    char * ptr = sf_mem_start();
	PUT(ptr, 0); /*Alignment heading*/ /*8 bytes */
	PUT(ptr + (sizeof(sf_header)), PACK(DSIZE, 1, 0)); /*Prologue header offset of 8*/
	//sPUT(ptr + DSIZE - sizeof(sf_header), PACK(DSIZE, 1, 0)); /*Prologue footer off set of 24*/
	PUT(sf_mem_end() - sizeof(sf_header), PACK(0, 1, 0)); /*Epilogue header*/ /*should be 8 bytes*/

	ptr += 40; // now pointing to start of wilderness block
	//size_t w_size = 8192 - 48;
	if(sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next
		== sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev){ // if pointing to each other
			sf_block * w_block = (sf_block *)ptr;
			size_t w_size = (size_t)(sf_mem_end() - sf_mem_start()) - 48;

			PUT(ptr, PACK(w_size, 0, 2));         /* Free block header */   //line:vm:mm:freeblockhdr
    		PUT((sf_mem_end() - 2*sizeof(sf_header)), PACK(w_size, 0, 2));         /* Free block footer */   //line:vm:mm:freeblockftr

    		w_block->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
    		w_block->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];

			sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next = (sf_block *)ptr;
			sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];

			sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next->body.links.next
				= sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev;
			PUT(sf_mem_end() - sizeof(sf_header), PACK(0, 1, 0)); /*Epilogue header*/
		}
	return ptr;
}

/*
 * This is your implementation of sf_malloc. It acquires uninitialized memory that
 * is aligned and padded properly for the underlying system.
 *
 * @param size The number of bytes requested to be allocated.
 *
 * @return If size is 0, then NULL is returned without setting sf_errno.
 * If size is nonzero, then if the allocation is successful a pointer to a valid region of
 * memory of the requested size is returned.  If the allocation is not successful, then
 * NULL is returned and sf_errno is set to ENOMEM.
 */
void *sf_malloc(size_t size) {

	char *bp;

	/*return NULL without setting sf_errno*/
	if(size == 0){
		return NULL;
	}

	/*initialize the heap*/
	sf_header * pro = (sf_header *)sf_mem_start();
	sf_header * end = (sf_header *)sf_mem_end();

	if(pro == end){
        initialize_heap();
	}

	size_t newsize = 0;

	if(size <= 24){ // <= 24 --> 32
		newsize = 32;
	} else { // >= 25: x + 8 + however many to multiple of 16
		newsize = size + 8;
		int i = newsize % 16;
		if(i != 0)
			newsize = newsize + (16 - i);
	}


	/*need to search the free list for a fit*/
	if((bp = find_fit(newsize)) != NULL){
		if(splinter == -1){
			PUT(bp, PACK(GET_SIZE(bp), 1, 2));
			splinter = 0;
			((sf_block *)bp)->body.payload[0] = size;
			bp+= sizeof(sf_header);
			return bp;
		}
        PUT(bp, PACK(newsize, 1, 2));
		/* to find padding, we just do newsize - payload*/
		((sf_block *)bp)->body.payload[0] = size;
		bp += sizeof(sf_header);
		return bp;
	}
    return NULL;
}

/*
 * Marks a dynamically allocated region as no longer in use.
 * Adds the newly freed block to the free list.
 *
 * @param ptr Address of memory returned by the function sf_malloc.
 *
 *	👍  You can use casts to convert a generic pointer value to one
	of type sf_block * or sf_header *, in order to make use of the above
	structure definitions to easily access the various fields.  You can even cast
	an integer value to these pointer types; this is sometimes required when
	calculating the locations of blocks in the heap.
 *
 * If ptr is invalid, the function calls abort() to exit the program.
 */
void sf_free(void *pp) {
	sf_header * bp = (sf_header *) pp;
	bp = pp - sizeof(sf_header);

	if(GET_ALLOC(bp) == 0) abort();// if not allocated
	if(bp == NULL) abort(); // if ptr is null
	if(((size_t)pp) % 16 != 0) abort();// if address is not 16 byte aligned
	if(GET_SIZE(bp) < 32) abort();// if size of block is less than the minimum block size
	if(GET_SIZE(bp) % 16 != 0) abort(); // if size of block is not multiple of 16
	if(pp-sizeof(sf_header)+GET_SIZE(bp) > sf_mem_end()) abort();//outside of heap bounds
	if(pp > sf_mem_end() || pp < sf_mem_start()) abort(); //outside of heap bounds
	if(GET_PALLOC(bp) == 0 && GET_ALLOC(bp-sizeof(sf_header)) != 0) abort(); //how to check prev alloc and alloc if not free block footer

	sf_block * cp = coalesce(bp);

	if(pp-sizeof(sf_header)+GET_SIZE(cp) == sf_mem_end() - 8){
		cp->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
		cp->body.links.next = sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next;
		sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = cp;
	} else {
		int i = 0, j = 0;
		int count = 2;
		int less = 1;
		int max = 2;
		if(GET_SIZE(cp) == 32){
			i = 0;
		} else if(GET_SIZE(cp) > 32 * 32){
			i = NUM_FREE_LISTS-2;
		} else {
			for(j = 1; j < NUM_FREE_LISTS-2; j++){
				if((GET_SIZE(cp) > (32 * less)) && (GET_SIZE(cp) <= (32 * max))){
					i = j;
					break;
				} else {
					less *= count;
					max *= count;
				}
			}
		}
		/*Place new free block at beginning of list*/
		cp->body.links.prev = &sf_free_list_heads[i];
		cp->body.links.next = sf_free_list_heads[i].body.links.next;
		sf_free_list_heads[i].body.links.next->body.links.prev = cp;
		sf_free_list_heads[i].body.links.next = cp;
	}

}

static void *coalesce(void *bp){

	/*Case 2 and Case 4 need to deal with if next free block is */

	size_t size = GET_SIZE(bp);
	size_t prev_allocated = GET_PALLOC(bp);
	size_t next_alloc = GET_ALLOC((bp+size));

	if(prev_allocated && next_alloc){ /* Case 1 */
		size_t size = GET_SIZE(bp);
		PUT(bp, PACK(size, 0, 2));
		PUT(bp + size - sizeof(sf_header), PACK(size, 0, 2));

		/* next block change prev alloc to 0*/
		void * bpp = bp;
		sf_header * bpp_h = (sf_header *)(bpp+size);

		if(GET_ALLOC(bpp+size) == 1){
			*bpp_h = *bpp_h ^ PREV_BLOCK_ALLOCATED;
		} else {
			*bpp_h = *bpp_h ^ PREV_BLOCK_ALLOCATED;
			sf_header * bpp_f = (sf_header *)(bpp + GET_SIZE(bpp+size) - sizeof(sf_header));
			*bpp_f = *bpp_f ^ PREV_BLOCK_ALLOCATED;
		}
		return bp; // cannot coalesce
	}

	else if(prev_allocated && !next_alloc){ /* Case 2 */
		/*Going to have to check to see if next block is epilogue :o if it is, then
		we have to set the whole new free block to wilderness*/
		size_t newsize = size + GET_SIZE((bp + size));

		/*next block is free so first we have to take it out of the free lists*/
		((sf_block *)(bp+size))->body.links.next->body.links.prev = ((sf_block *)(bp+size))->body.links.prev;
		((sf_block *)(bp+size))->body.links.prev->body.links.next = ((sf_block *)(bp+size))->body.links.next;

		PUT(bp, PACK(newsize, 0, 2)); /* Free block header*/
		PUT(bp + newsize - sizeof(sf_header), PACK(newsize, 0, 2)); /*Free block footer*/
		return bp;
	}

	else if(!prev_allocated && next_alloc){ /*Case 3*/
		/* need to check if prev_allocated's prev is allocated*/
		size_t newsize = size + GET_SIZE((bp - sizeof(sf_header)));
		bp -= GET_SIZE((bp - sizeof(sf_header)));

		/*removes previous free block from free lists*/
		((sf_block *)bp)->body.links.next->body.links.prev = ((sf_block *)bp)->body.links.prev;
		((sf_block *)bp)->body.links.prev->body.links.next = ((sf_block *)bp)->body.links.next;

		int flag = 0;
		size_t pre_prev = GET_PALLOC(bp);
		if(pre_prev == 2){
			flag = 1;
		}
		PUT(bp, PACK(newsize, 0, 2)); /* Free block header */
		PUT(bp + newsize - sizeof(sf_header), PACK(newsize, 0, 2)); /* Free block footer */
		if(flag == 0){
			return bp;
		}

		/* next block change prev alloc to 0*/
		void * bpp = bp;
		sf_header * bpp_h = (sf_header *)(bpp+newsize);

		if(GET_ALLOC(bpp+newsize) == 1){
			*bpp_h = *bpp_h ^ PREV_BLOCK_ALLOCATED;
		} else {
			*bpp_h = *bpp_h ^ PREV_BLOCK_ALLOCATED;
			sf_header * bpp_f = (sf_header *)(bpp + GET_SIZE(bpp+newsize) - sizeof(sf_header));
			*bpp_f = *bpp_f ^ PREV_BLOCK_ALLOCATED;
		}

		return bp;
	}

	else {
		size_t prev_size = GET_SIZE((bp - sizeof(sf_header)));
		size_t next_size = GET_SIZE((bp + GET_SIZE(bp)));
		size_t newsize = prev_size + size + next_size;

		/*prev block is free so first we have to take it out of the free lists*/
		((sf_block *)(bp-prev_size))->body.links.next->body.links.prev = ((sf_block *)(bp-prev_size))->body.links.prev;
		((sf_block *)(bp-prev_size))->body.links.prev->body.links.next = ((sf_block *)(bp-prev_size))->body.links.next;

		/*next block is free so first we have to take it out of the free lists*/
		((sf_block *)(bp+size))->body.links.next->body.links.prev = ((sf_block *)(bp+size))->body.links.prev;
		((sf_block *)(bp+size))->body.links.prev->body.links.next = ((sf_block *)(bp+size))->body.links.next;

		bp -= prev_size;
		/* need to check if prev_allocated's prev is allocated*/
		int flag = 0;
		size_t pre_prev = GET_PALLOC(bp);
		if(pre_prev == 2){
			flag = 1;
		}
		PUT(bp, PACK(newsize, 0, 2)); /* Free block header */
		PUT(bp + newsize - sizeof(sf_header), PACK(newsize, 0, 2)); /* Free block footer*/
		if(flag == 0){
			return bp;
		}

		/* next block change prev alloc to 0*/
		void * bpp = bp;
		sf_header * bpp_h = (sf_header *)(bpp+newsize);
		if(GET_ALLOC(bpp+newsize) == 1){
			*bpp_h = *bpp_h ^ PREV_BLOCK_ALLOCATED;
		} else {
			*bpp_h = *bpp_h ^ PREV_BLOCK_ALLOCATED;
			sf_header * bpp_f = (sf_header *)(bpp + GET_SIZE(bpp+newsize) - sizeof(sf_header));
			*bpp_f = *bpp_f ^ PREV_BLOCK_ALLOCATED;
		}
		return bp;
	}

	return bp;
}


void *sf_realloc(void *pp, size_t rsize) {
	sf_header * bp = (sf_header *) pp;
	bp = pp - sizeof(sf_header);

	if(GET_ALLOC(bp) == 0) abort();// if not allocated
	if(bp == NULL) abort(); // if ptr is null
	if(((size_t)pp) % 16 != 0) abort();// if address is not 16 byte aligned
	if(GET_SIZE(bp) < 32) abort();// if size of block is less than the minimum block size
	if(GET_SIZE(bp) % 16 != 0) abort(); // if size of block is not multiple of 16
	if(pp-sizeof(sf_header)+GET_SIZE(bp) > sf_mem_end()) abort();//outside of heap bounds
	if(pp > sf_mem_end() || pp < sf_mem_start()) abort(); //outside of heap bounds
	if(GET_PALLOC(bp) == 0 && GET_ALLOC(bp-sizeof(sf_header)) != 0) abort(); //how to check prev alloc and alloc if not free block footer

	if(rsize == 0){ // if pointer is valid but size is 0, free block and return NULL
		sf_free(pp);
		return NULL;
	}

	size_t new_size = 0; // = rsize + 8;
	if(rsize <= 24){ // <= 24 --> 32
		new_size = 32;
	} else { // >= 25: x + 8 + however many to multiple of 16
		new_size = rsize + 8;
		int i = new_size % 16;
		if(i != 0)
			new_size = new_size + (16 - i);
	}

	size_t osize = GET_SIZE(bp);
	if(osize < rsize){ // reallocating to larger size
		void * ptr = sf_malloc(rsize);
		if(ptr == NULL){
			return NULL;
		}
		memcpy(ptr, pp, rsize);
		sf_free(pp);
		return ptr;

	} else if(new_size < osize) { // reallocating to smaller size
		if((osize - rsize - 8) < 32){ // this causes a splinter so we can only update the payload
			((sf_block *)bp)->body.payload[0] = rsize;
			return pp;
		} else { //we have to split the blocks

			/* check if there is wilderness or epilogue*/
			void *x = pp-sizeof(sf_header)+GET_SIZE(pp-sizeof(sf_header));
			void *y = x+GET_SIZE(x);
			if(x == (sf_mem_end() - 8)){
				/*epilogue with no wilderness, so free becomes wilderness*/
				int prev_allocated = GET_PALLOC(bp);
				PUT(bp, PACK(new_size, 1, 0)); /*Adjust block header*/
				if(prev_allocated == 2){
					PUT(bp, PACK(new_size, 1, 2));
				}
				((sf_block *)(bp+sizeof(sf_header)))->body.payload[0] = rsize;
				void * ptr = pp-sizeof(sf_header)+new_size;
				size_t free_size = osize - new_size;
				PUT(ptr, PACK(free_size, 0, 2)); /*Free block header*/
				PUT(ptr + free_size - sizeof(sf_header), PACK(free_size, 0, 2));
				((sf_block *)ptr)->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
				((sf_block *)ptr)->body.links.next = sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next->body.links.prev = (sf_block *)ptr;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = (sf_block *)ptr;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
			} else if(y == (sf_mem_end() - 8)){
				/*wilderness block is next*/
				size_t wsize = osize - new_size + GET_SIZE(x);
				int prev_allocated = GET_PALLOC(bp);
				PUT(pp-sizeof(sf_header), PACK(new_size, 1, 0));
				if(prev_allocated == 2){
					PUT(bp, PACK(new_size, 1, 2));
				}
				((sf_block *)(pp))->body.payload[0] = rsize;
				void * ptr = pp-sizeof(sf_header)+new_size;

				PUT(ptr, PACK(wsize, 0, 2));
				PUT(ptr + wsize - sizeof(sf_header), PACK(wsize, 0, 2));
				((sf_block *)(x))->body.links.next->body.links.prev = ((sf_block *)(x))->body.links.prev;
				((sf_block *)(x))->body.links.prev->body.links.next = ((sf_block *)(x))->body.links.next;
				((sf_block *)ptr)->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
				((sf_block *)ptr)->body.links.next = sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next->body.links.prev = (sf_block *)ptr;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = (sf_block *)ptr;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
			} else {
				int prev_allocated = GET_PALLOC(bp);
				/*pp is what we will return*/
				PUT(bp, PACK(new_size, 1, 0)); /*Adjust block header*/
				if(prev_allocated == 2){
					PUT(bp, PACK(new_size, 1, 2));
				}
				((sf_block *)(pp))->body.payload[0] = rsize;
				void * ptr = pp-sizeof(sf_header)+new_size;
				size_t free_size = osize - new_size;
				PUT(ptr, PACK(free_size, 0, 2)); /*Free block header*/
				PUT(ptr + free_size - sizeof(sf_header), PACK(free_size, 0, 2));

				int i = 0, j = 0;
				int count = 2;
				int less = 1;
				int max = 2;
				if(GET_SIZE((sf_block *)ptr) == 32){
					i = 0;
				} else if(GET_SIZE((sf_block *)ptr) > 32 * 32){
					i = NUM_FREE_LISTS-2;
				} else {
					for(j = 1; j < NUM_FREE_LISTS-2; j++){
						if((GET_SIZE((sf_block *)ptr) > (32 * less)) && (GET_SIZE((sf_block *)ptr) <= (32 * max))){
							i = j;
							break;
						} else {
							less *= count;
							max *= count;
						}
					}
				}

				/* Place the new free block at the beginning of the list */
				((sf_block *)ptr)->body.links.prev = &sf_free_list_heads[i];
				((sf_block *)ptr)->body.links.next = sf_free_list_heads[i].body.links.next;
				sf_free_list_heads[i].body.links.next->body.links.prev = (sf_block *)ptr;
				sf_free_list_heads[i].body.links.next = (sf_block *)ptr;

				size_t next_alloc = GET_ALLOC((ptr+free_size));
				if(next_alloc == 0){
					coalesce(ptr);
				} else {
					PUT(ptr+free_size, PACK(GET_SIZE(ptr+free_size), next_alloc, 0));
				}
			}
			return pp;

		}

	} else { // sizes are the same -- ask abt this case
		return pp;
	}

    return NULL;
}

int ispoweroftwo(size_t n)
{
  if (n == 0) return 0;
  while (n != 1) {
      if (n%2 != 0)
         return 0;
      n = n/2;
  }
  return 1;
}

void *sf_memalign(size_t size, size_t align) {

	if(align < 32){ // check that requested alignment is at least 32
		sf_errno = EINVAL;
		return NULL;
	}

	if(ispoweroftwo(align) == 0){ // check if size is a power of 2
		sf_errno = EINVAL;
		return NULL;
	}

	size_t newsize = size + align + DSIZE; // size + align + 32 + 8
	// check this

	void * block = sf_malloc(newsize);
	size_t osize = GET_SIZE(block-sizeof(sf_header));
	size_t prev_alloc = GET_PALLOC(block-sizeof(sf_header));
	if(block == NULL){
		return NULL;
	}
	/*check to see if payload is aligned*/
	if((size_t)block % align == 0){
		size_t new_size = size + sizeof(sf_header);
		size_t remainder = new_size % 16;
		if(remainder != 0){
			new_size += (16 - remainder);
		}
		size_t leftover = GET_SIZE(block-sizeof(sf_header)) - new_size;
		if(leftover < 32){ /*too small for a split*/;
			return block;
		} else {
			PUT(block-sizeof(sf_header), PACK(new_size, 1, 0)); /*allocated block*/
			void * free_block = block-sizeof(sf_header)+new_size;
			PUT(free_block, PACK(leftover, 0, 2)); /*Free block header*/
			PUT(free_block+leftover-sizeof(sf_header), PACK(leftover, 0, 2)); /*Free block footer*/

			// need to coalesce each and add to free lists
			sf_block * fp = coalesce(free_block);

			if(fp+GET_SIZE(fp) == sf_mem_end() - 8){
				fp->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
				fp->body.links.next = sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = fp;
			} else {
				int i = 0, j = 0;
				int count = 2;
				int less = 1;
				int max = 2;
				if(GET_SIZE(fp) == 32){
					i = 0;
				} else if(GET_SIZE(fp) > 32 * 32){
					i = NUM_FREE_LISTS-2;
				} else {
					for(j = 1; j < NUM_FREE_LISTS-2; j++){
						if((GET_SIZE(fp) >= (32 * less)) && (GET_SIZE(fp) < (32 * max))){
							i = j;
							break;
						} else {
							less *= count;
							max *= count;
						}
					}
				}

				/* Place the new free block at the beginning of the list */
				fp->body.links.prev = &sf_free_list_heads[i];
				fp->body.links.next = sf_free_list_heads[i].body.links.next;
				sf_free_list_heads[i].body.links.next = fp;
				return block;
			}
		}
	}
	else {
		void * ptr = block;
		block -= sizeof(sf_header);

		size_t fsize = DSIZE;
		ptr += fsize; // 32

		int flag = 0;
		while(flag == 0){
			if((size_t)ptr % align == 0 && (size_t)ptr % 16 == 0 && ptr < block+GET_SIZE(block)){
				flag = 1;
			} else{
				fsize += WSIZE; // add 16
				ptr += WSIZE; // add 16 to the pointer
			}
		}

		PUT(ptr-sizeof(sf_header), PACK((osize-fsize), 1, 0)); /*allocated block asjusted*/
		PUT(block, PACK(fsize, 0, GET_PALLOC(block))); /* Free block header*/
		PUT((block+fsize-sizeof(sf_header)), PACK(fsize, 0, GET_PALLOC(block))); /*Free block footer*/
		sf_block * cp = block;
		if(prev_alloc == 0){
			cp = coalesce(block);
		}

		int i = 0, j = 0;
		int count = 2;
		int less = 1;
		int max = 2;
		if(GET_SIZE(cp) == 32){
			i = 0;
		} else if(GET_SIZE(cp) > 32 * 32){
			i = NUM_FREE_LISTS-2;
		} else {
			for(j = 1; j < NUM_FREE_LISTS-2; j++){
				if((GET_SIZE(cp) >= (32 * less)) && (GET_SIZE(cp) < (32 * max))){
					i = j;
					break;
				} else {
					less *= count;
					max *= count;
				}
			}
		}

		/* Place the new free block at the beginning of the list */
		cp->body.links.prev = &sf_free_list_heads[i];
		cp->body.links.next = sf_free_list_heads[i].body.links.next;
		sf_free_list_heads[i].body.links.next = cp;
		/*Need to insert free block into free lists*/

		/*need to check if we can split the bottom extra off*/
		size_t newsize = (size + sizeof(sf_header));
		size_t remainder = newsize % 16;
		if(remainder != 0){
			newsize += (16 - remainder);
		}

		size_t lsize = GET_SIZE(ptr-sizeof(sf_header));
		size_t leftover = (size_t)(lsize - newsize);

		if(leftover < 32){ /*too small for a split*/;
			return ptr;
		} else {

			PUT(ptr-sizeof(sf_header), PACK(newsize, 1, 0)); /*allocated block*/
			void * free_block = ptr-sizeof(sf_header)+newsize;

			PUT(free_block, PACK(leftover, 0, 2)); /*Free block header*/
			PUT(free_block+leftover-sizeof(sf_header), PACK(leftover, 0, 2)); /*Free block footer*/

			// need to coalesce each and add to free lists

			sf_block * fp = coalesce(free_block);

			if(fp+GET_SIZE(fp) == sf_mem_end() - 8){
				fp->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
				fp->body.links.next = sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = fp;
			} else {
				int i = 0, j = 0;
				int count = 2;
				int less = 1;
				int max = 2;
				if(GET_SIZE(fp) == 32){
					i = 0;
				} else if(GET_SIZE(fp) > 32 * 32){
					i = NUM_FREE_LISTS-2;
				} else {
					for(j = 1; j < NUM_FREE_LISTS-2; j++){
						if((GET_SIZE(fp) >= (32 * less)) && (GET_SIZE(fp) < (32 * max))){
							i = j;
							break;
						} else {
							less *= count;
							max *= count;
						}
					}
				}

				/* Place the new free block at the beginning of the list */
				fp->body.links.prev = &sf_free_list_heads[i];
				fp->body.links.next = sf_free_list_heads[i].body.links.next;
				sf_free_list_heads[i].body.links.next = fp;
				return ptr;
			}
		}
	}
    	return NULL;
}

/*
 * find_fit - Find a fit for a block with asize bytes
 */
/* $begin mmfirstfit */
/* $begin mmfirstfit-proto */
static void *find_fit(size_t asize)
/* $end mmfirstfit-proto */
{
	int flag = 0; //use this to check if we need to use wilderness block
	int index = 0;
	//int found = -1;

	if(asize == 32) index = 0;
	else if((asize > 32) && (asize <= 64)) index = 1;
	else if((asize > 64) && (asize <= 128)) index = 2;
	else if((asize > 128) && (asize <= 256)) index = 3;
	else if((asize > 256) && (asize <= 512)) index = 4;
	else if((asize > 512) && (asize <= 1024)) index = 5;
	else if(asize > 1024) index = 6;

	while(index < NUM_FREE_LISTS-1){
		if(sf_free_list_heads[index].body.links.next == &sf_free_list_heads[index] &&
			sf_free_list_heads[index].body.links.prev == &sf_free_list_heads[index]){
			index++; //if list is empty, look in next list
		} else {
			int found_block = -1;
			sf_block * ptr = sf_free_list_heads[index].body.links.next;
			void * ptr1 = sf_free_list_heads[index].body.links.next;
			while(found_block == -1 && ptr != &sf_free_list_heads[index]){

				size_t size = GET_SIZE(ptr);
				if(size >= asize){
					found_block = 0;

					/*check to see if splitting will be an issue*/
					if(((size - asize) == 0) || ((size - asize) < DSIZE)){
						splinter = -1;
						ptr->body.links.next->body.links.prev = ptr->body.links.prev;
						ptr->body.links.prev->body.links.next = ptr->body.links.next;
						size_t x = GET_SIZE(ptr1);
						sf_header * next = (sf_header *)(ptr1+x);
						*next = *next | PREV_BLOCK_ALLOCATED;
						return ptr;
					} else { // need to so some block splitting
						size_t bsize = size - asize;
						void * ptr1 = ptr;
						ptr1 += asize;

						sf_block * block = (sf_block *)ptr1;

						PUT(ptr1, PACK(bsize, 0, 2));         /* Free block header */   //line:vm:mm:freeblockhdr
	   					PUT((ptr1 + bsize - sizeof(sf_header)), PACK(bsize, 0, 2));         /* Free block footer */   //line:vm:mm:freeblockftr

						int new_index = 0;
						if(bsize == 32) new_index = 0;
						else if((bsize > 32) && (bsize <= 64)) new_index = 1;
						else if((bsize > 64) && (bsize <= 128)) new_index = 2;
						else if((bsize > 128) && (bsize <= 256)) new_index = 3;
						else if((bsize > 256) && (bsize <= 512)) new_index = 4;
						else if((bsize > 512) && (bsize <= 1024)) new_index = 5;
						else if((bsize > 1024)) new_index = 6;

						if(new_index != index){
							ptr->body.links.next->body.links.prev = ptr->body.links.prev;
							ptr->body.links.prev->body.links.next = ptr->body.links.next;
							block->body.links.next = sf_free_list_heads[new_index].body.links.next;
							block->body.links.prev = &sf_free_list_heads[new_index];
							sf_free_list_heads[new_index].body.links.next->body.links.prev = block;
							sf_free_list_heads[new_index].body.links.next = block;
							return ptr;
						}

						block->body.links.next = ptr->body.links.next;
						block->body.links.prev = ptr->body.links.prev;
						ptr->body.links.next->body.links.prev = block;
						ptr->body.links.prev->body.links.next = block;
						return ptr;
					}
				} else {
					ptr = ptr->body.links.next;
				}
			}
			index++;

		}
	}

	if(index == NUM_FREE_LISTS-1){
		flag = -1;
	}

	if(flag == -1) { //there is nothing here
		if(sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next == &sf_free_list_heads[NUM_FREE_LISTS-1] &&
			sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev == &sf_free_list_heads[NUM_FREE_LISTS-1]){
			//printf("No block exists\n");
			sf_header * ptr = sf_mem_end() - sizeof(sf_header); // will be start of free block
			sf_header * ep = ptr;
			*ep = *ep - 1; // prev not allocated
			void * pp = sf_mem_grow();
			if(pp == NULL){
				sf_errno = ENOMEM;
				return pp; // don't know how to error handle
			}
			size_t wsize = PAGE_SZ;
			PUT(ptr, PACK(wsize, 0, 2)); /*free block header*/
			PUT((sf_mem_end() - 2*sizeof(sf_header)), PACK(wsize, 0, 2)); /*free block footer*/
			//PUT((sf_mem_end() - sizeof(sf_header)), PACK(0, 1)); /*epilogue header*/
			sf_block * w_block = (sf_block *)ptr;

    			w_block->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
    			w_block->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];

				sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next = (sf_block *)w_block;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];

				sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next->body.links.next
					= sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev;
				PUT((sf_mem_end() - sizeof(sf_header)), PACK(0, 1, 0)); /*Epilogue header*/
		}


		if(sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next !=
			sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev){ // if wilderness block exists
			void * ptr = sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next;
			void * ptr1 = sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next;

			/*need to create new header and footer and adjust size*/
			size_t wsize = GET_SIZE(ptr);
			if(wsize >= asize){ /*if we have enough memory*/
				ptr1 += asize;
				wsize = wsize - asize;        // new wilderness block

				/*need to check this case*/
				if(wsize < 32){ // if splitting leaves a block less than 32, take whole block
					splinter = -1;
					wsize = 0;
				}

				if(wsize == 0){
					sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
					sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
					PUT(sf_mem_end() - sizeof(sf_header), PACK(0, 1, 2)); /*New epilogue header*/
					sf_header * ep = (sf_header *)(sf_mem_end() - sizeof(sf_header));
					*ep = *ep | PREV_BLOCK_ALLOCATED;

					return ptr;
				}

				sf_block * w_block = (sf_block *)ptr1;

				PUT(ptr1, PACK(wsize, 0, 2));         /* Free block header */   //line:vm:mm:freeblockhdr
	   			PUT((sf_mem_end() - 2*sizeof(sf_header)), PACK(wsize, 0, 2));         /* Free block footer */   //line:vm:mm:freeblockftr

	   			w_block->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
	   			w_block->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
				sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next = (sf_block *)w_block;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];

				sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next->body.links.next
					= sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev;
				PUT(sf_mem_end()-sizeof(sf_header), PACK(0, 1, 0)); /*Epilogue header*/
				return ptr;

			} else { /*we need to call mem grow*/
				size_t wild_mem = GET_SIZE(sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next);

				while(wild_mem < asize){
					void * pp = sf_mem_grow();
					if(pp == NULL){
						sf_errno = ENOMEM;
						fprintf(stderr, "Run out of memory... \n");
						char * ptr = (char *)(sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next);
						PUT(ptr, PACK(wild_mem, 0, 2)); /*free block header*/
						PUT((sf_mem_end() - sizeof(sf_header) - sizeof(sf_footer)), PACK(wild_mem, 0, 2));
						PUT(sf_mem_end() - sizeof(sf_header), PACK(0, 1, 0));
						return pp;
					}
					wild_mem += PAGE_SZ;

				}
				char * ptr = (char *)(sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next);
				ptr += asize;
				wsize = wild_mem - asize;

				if(wsize == 0){
					sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
					sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
					PUT(sf_mem_end() - sizeof(sf_header), PACK(0, 1, 2));
					return ptr;
				}

				// find mem st and end and then init header and footer
				PUT(ptr, PACK(wsize, 0, 2)); /*free block header*/
				PUT((sf_mem_end() - sizeof(sf_header) - sizeof(sf_footer)), PACK(wsize, 0, 2)); /*Freeb block footer*/
				PUT(sf_mem_end() - sizeof(sf_header), PACK(0, 1, 0)); /*Epilogue header*/ /*should be 8 bytes*/

				sf_block * w_block = (sf_block *)ptr;
				w_block->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
				w_block->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = w_block;
				sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
				sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next->body.links.next
					= sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev;
				PUT(sf_mem_end()-sizeof(sf_header), PACK(0, 1, 0)); /*Epilogue header*/
			}
			return ptr;
		}
	}
    /* $end mmfirstfit */
    return NULL;
}
/* $end mmfirstfit */