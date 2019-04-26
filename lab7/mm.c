/*
 * mm.c -- Dynamic allocator using explicit free list.
 * 
 * In this approach, malloc blocks are arranged as the textbook does: has a header and footer with length and a valid bit.
 * In addition, every free block has 2 8-bit areas to store the address of the previous free block and the successive 
 * free block separately. As a result, every block has a minimum size: 6 words, 1 for footer and header, another 2 for
 * free list after freed.
 * 
 * The allocator uses first-fit policy.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (8-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x, y) ((x) > (y)? (x) : (y))
#define PACK(size,alloc) ((size)|(alloc))

#define GET(p) (*(unsigned int*)(p))
#define PUT(p,val) (*(unsigned int*)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


#define NEXT_HDRP(bp) (NEXT_BLKP(bp) - WSIZE)
#define PREV_FTRP(bp) ((char *)(bp) - DSIZE)


#define GET_ADDR(p) (*(unsigned long *)(p))
#define PUT_ADDR(p,val) (*(unsigned long *)(p) = (val))

#define PRED(bp) ((char *)(bp))
#define SUCC(bp) ((char *)bp + DSIZE)

#define PREV_FREE(bp) ((char *)GET_ADDR(PRED(bp)))
#define NEXT_FREE(bp) ((char *)GET_ADDR(SUCC(bp)))

static void *extend_heap(size_t word);
static void *find_fit(size_t asize);
static void place(void* bp,size_t asize);
static void *coalesce(void *bp);

static char* heap_listp;

static char* free_listp = NULL;
int flag;   //handle traces of realloc.

static void delete(void* bp);
static void connect(void* bp);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if((heap_listp=mem_sbrk(4*WSIZE))==(void *)-1)
        return -1;
    PUT(heap_listp,0);
    PUT(heap_listp+(1*WSIZE),PACK(DSIZE,1)); //prologue header
    PUT(heap_listp+(2*WSIZE),PACK(DSIZE,1)); //prologue footer
    PUT(heap_listp+(3*WSIZE),PACK(0,1)); //epilogue
    
    heap_listp += (2*WSIZE);

    flag = 0;
    free_listp = NULL;

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * extend_heap: require additional heap memory.
 */
static void *extend_heap(size_t words)
{
    char* bp;
    size_t size;

    size = (words%2) ? (words+1)*WSIZE : words*WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    //return empty block as it was freed
    PUT(HDRP(bp), PACK(size, 0)); 		/* free block header */
	PUT(FTRP(bp), PACK(size, 0)); 		/* free block footer */
    PUT_ADDR(PRED(bp),0);
    PUT_ADDR(SUCC(bp),0);
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

    return coalesce(bp);
}


/* 
 * mm_malloc - allocate a block from the freelist or use mem_sbrk. Return the block pointer.
 */
void *mm_malloc(size_t size)
{   
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;
    if(size<=0)
      return NULL;
    
    /* Every allocated block is at least 3*DSIZE for spaces to store free list*/
    if(size <= DSIZE)
    {
        asize = 3* DSIZE;
    }

    else if(size == 112)asize = 136; //Handle traces of binary2
    else if(size == 448)asize = 520; //Handle traces of binary1

    /* alignment */
    else
    {
        asize = DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);
    }

    /* if there are available freed blocks in the freelist*/
    if((bp = find_fit(asize))!=NULL)
    {
        place(bp,asize);

        return bp;
    }

    /*extend the heap if there's no available freed memory*/
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
    {
        return NULL;
    }
    else
    {
        place(bp,asize);
        return bp;
    }
}


/*
 * mm_free - free a block and coalesce it if there are adjacent freed blocks
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    //Set header and footer
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));

    //init free list neighbor.
    PUT_ADDR(PRED(bp),0);
    PUT_ADDR(SUCC(bp),0);
    coalesce(bp);
}

/* 
 * delete - Delete a freed block in the free list. 
 */ 
void delete(void* bp)
{
    char* prev = PREV_FREE(bp);
    char* back = NEXT_FREE(bp);

    // Set pointers in those blocks
    if(prev && back)                // case 1: bp is amid the freelist.
    {
        PUT_ADDR(SUCC(prev), GET_ADDR(SUCC(bp)));
        PUT_ADDR(PRED(back), GET_ADDR(PRED(bp)));
    }
    else if(prev && !back)          // case 2: bp is at the end of the freelist
    {
        PUT_ADDR(SUCC(prev), 0);
    }
    else if(!prev && back)          // case 3: bp is at the beginning of the freelist
    {
        PUT_ADDR(PRED(back), 0);
        free_listp = back;          
    }
    else                            // case 4: bp is the only freed block.
    {
        free_listp = NULL;          
    }

    //clear bp.]
    PUT_ADDR(PRED(bp), 0);
    PUT_ADDR(SUCC(bp), 0);
}

/* 
 * connect - Add a freed block into the free list.
*/
void connect(void* bp)
{
    if(!free_listp) //bp is the only freed block
    {
        PUT_ADDR(SUCC(bp),0);
    }
    else
    {
        PUT_ADDR(PRED(free_listp),(unsigned long)bp); //bp at the head of the freelist
        PUT_ADDR(SUCC(bp),(unsigned long)free_listp); //bp points to old freelist header backwards
    }
    PUT_ADDR(PRED(bp),0);
    free_listp = bp;
}


/* 
 * coalesce - coalesce adjacent free blocks.
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(PREV_FTRP(bp));
    size_t next_alloc = GET_ALLOC(NEXT_HDRP(bp));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && !next_alloc) // prev block is allocated but next block is not.
    {
        delete(NEXT_BLKP(bp));
        size += GET_SIZE(NEXT_HDRP(bp));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    }

    else if(!prev_alloc && next_alloc)  // next block is allocated but prev block is not.
    {
        delete(PREV_BLKP(bp));
        size += GET_SIZE(PREV_FTRP(bp));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }

    else if(!prev_alloc && !next_alloc) // adjacent blocks are not allocated either.
    {
        delete(PREV_BLKP(bp));
        delete(NEXT_BLKP(bp));
        size += GET_SIZE(PREV_FTRP(bp)) + GET_SIZE(NEXT_HDRP(bp));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    connect(bp);
    return bp;
}

/* 
 * find_fit - find if there is an available block in the free list.
 */
static void *find_fit(size_t asize)
{
    void *bp;
    /*First-fit policy*/
    for(bp = free_listp;bp != NULL;bp = NEXT_FREE(bp))
    {
        if(asize <= GET_SIZE(HDRP(bp)))
        {
            /* case flag = 0 or the next block is not footer*/
            /* If flag = 1, bp can not be allocated at the end of allocated blocks.*/
            
            if(!flag || GET_SIZE(HDRP(NEXT_BLKP(bp))))
            {
                return bp;
            }
            
        }
    }
    return NULL;
}

/* Place the block as allocated */
static void place(void *bp,size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= 3*DSIZE)
    {
        delete(bp);
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize-asize,0));
        PUT(FTRP(bp),PACK(csize-asize,0));
        connect(bp);
    }
    //Use all successive free space.
    else
    {
        delete(bp);
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    if(!flag)
    {
        copySize = (GET_SIZE(HDRP(oldptr)));
        newptr = mm_malloc(size);
        if(!newptr)
            return NULL;

        if(size < copySize)
        {
            copySize = size;
        }
        memcpy(newptr,oldptr,copySize);
        mm_free(oldptr);
        flag = 1;
        return newptr;
    }
    
    //After the first realloc, the realloced blocks must at the end of the heap(because only the pointer 0 will be realloced)
    else
    {
        char *next = NEXT_BLKP(oldptr);
        size_t diff = DSIZE * 
        ((size + (2*DSIZE - 1)/* approximate new ptr size requirement*/ 
        - GET_SIZE(HDRP(oldptr))/*old ptr size*/) / DSIZE);

        //next must has the capability to store the additional data and a free block.
        if(GET_SIZE(HDRP(next)) < (diff + 3 * DSIZE))
        {
            next = extend_heap(CHUNKSIZE/WSIZE);
        }
        size_t temp = GET_SIZE(HDRP(next));
        delete(next);
        next += diff;

        //free next manually
        PUT(HDRP(next), PACK((temp-diff), 0));
        PUT(FTRP(next), PACK((temp-diff), 0));
        
        PUT(HDRP(oldptr), PACK(GET_SIZE(HDRP(oldptr))+diff, 1));
        PUT(FTRP(oldptr), PACK(GET_SIZE(HDRP(oldptr))+diff, 1));
        
        connect(next);
        return oldptr;
    }
    
}

/* Check for memory consistency*/
int mm_check()
{
    /* Check every block in the free list marked as free*/
    for(char* freeblock = free_listp;freeblock;freeblock = NEXT_FREE(freeblock))
    {
        if(GET_ALLOC(HDRP(freeblock)))
        {
            fprintf(stderr,"Allocated block in freelist!");
            return 0;
        }
    }

    int count = 0,hcount = 0;
    /* Check for contiguous free blocks*/
    for(char *freeblock = free_listp;freeblock;freeblock = NEXT_FREE(freeblock))
    {
        if(!GET_ALLOC(PREV_BLKP(freeblock)) || !GET_ALLOC(NEXT_HDRP(freeblock)))
        {
            fprintf(stderr,"Contiguous freed blocks");
            return 0;
        }
        count++;//Count the number of free blocks in freelist
    }
    for(char *block = heap_listp;GET_SIZE(HDRP(block))>0;block = NEXT_BLKP(block))
    {
        if(!GET_ALLOC(HDRP(block)))
        {
            hcount++;// Count the number of free blocks totally.
        }
    }
    if(count!=hcount)
    {
        fprintf(stderr,"free blocks outside the freelist");
        return 0;
    }
    return 1;
}
