/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "visha",
    /* First member's full name */
    "Aleksandra Shanina",
    /* First member's email address */
    "ashanina@u.rochester.edu",
    /* Second member's full name (leave blank if none) */
    "Vishnu Chittari",
    /* Second member's email address (leave blank if none) */
    "vchittar@u.rochester.edu"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE       8       /* Word and header/footer size (bytes) */
#define DSIZE       16       /* Double word size (bytes) */
#define CHUNKSIZE  (256)  /* Extend heap by this amount (bytes) */
#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define FTRP_S(bp, size) ((char *)(bp) + size - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr, read/write the next and previous free blocks */
#define PUT_PREV_FREE(bp, prev) PUT((bp), (unsigned long)prev)
#define PUT_NEXT_FREE(bp, next) PUT((bp) + WSIZE, (unsigned long)next)
#define GET_PREV_FREE(bp)       (void *)GET(bp)
#define GET_NEXT_FREE(bp)       (void *)GET((bp) + WSIZE)

static void * heap_listp;

static void * free_headp;
static void * free_tailp;

extern int verbose;

// debug mode
#if 0

#define mm_log printf
#define ASSERT {if (mm_check() < 0) exit(0);}

#else

#define mm_log(...)
#define ASSERT

#endif

/*
 * mm_check - TODO
 */
static int mm_check()
{    
    /* Check that the blocks are properly organized & that epilogue is present */
    void *ptr = heap_listp;
    int prev_alloced;
    int freecount = 0;
    while (ptr < mem_heap_hi())
    {
        size_t size = GET_SIZE(HDRP(ptr));
        size_t size2 = GET_SIZE(FTRP(ptr));
        int allocated = GET_ALLOC(HDRP(ptr));
        int allocated2 = GET_ALLOC(FTRP(ptr));

        if (size == 0)
        {
            if (ptr != mem_heap_hi() - WSIZE)
            {
                mm_log("mm_check error: ran into size 0 (epilogue) at %p, which is not heap_hi().\n",ptr);
                return -1;
            }
            if (!allocated)
            {
                mm_log("mm_check error: epilogue at %p is marked as free\n",ptr);
                return -1;
            }
        }

        if (size != size2)
        {
            mm_log("mm_check error: %p size in header %d != footer %d\n", ptr, (int)size, (int)size2);
            return -1;
        }

        if (allocated != allocated2)
        {
            mm_log("mm_check error: %p alloc in header %d != footer %d\n", ptr, allocated, allocated2);
            return -1;
        }


        if (!allocated && !prev_alloced)
        {
            mm_log("mm_check error: two contiguous free blocks that should be coalesced. %p and prev.\n",ptr);
        }

        if (allocated)
        {
            //mm_log("block %p (%d,%d)\n", ptr, (int)size, allocated);
        }
        else
        {
            mm_log("block %p (%d,%d). prev=%p next=%p\n", ptr, (int)size, allocated, GET_PREV_FREE(ptr), GET_NEXT_FREE(ptr));
            freecount++;
        }

        prev_alloced = allocated;
        ptr += size;
    }

    /* Check the free list */
    int count = 0;
    ptr = free_headp;
    void *prev = NULL;
    while (ptr != NULL)
    {
        count++;
        size_t bsize = GET_SIZE(HDRP(ptr));

        if (GET_NEXT_FREE(ptr) && GET_PREV_FREE(GET_NEXT_FREE(ptr)) != ptr)
        {
            mm_log("mm_check error: prev of next of %p is %p\n", ptr, GET_PREV_FREE(GET_NEXT_FREE(ptr)));
            return -1;
        }

        if (GET_PREV_FREE(ptr) && GET_NEXT_FREE(GET_PREV_FREE(ptr)) != ptr)
        {
            mm_log("mm_check error: next of prev of %p is %p\n", ptr, GET_NEXT_FREE(GET_PREV_FREE(ptr)));
            return -1;
        }

        if (bsize == 0)
        {
            mm_log("mm_check error: block size 0 at %p\n", ptr);
            return -1;
        }
        if (GET_ALLOC(HDRP(ptr)) || GET_ALLOC(FTRP(ptr)))
        {
            mm_log("mm_check error: block at %p on the freelist is marked allocated\n", ptr);
            return -1;
        }
        if (GET_PREV_FREE(ptr) != prev)
        {
            mm_log("mm_check error: block at %p does not point to prev %p (points to %p)\n", ptr, prev, GET_PREV_FREE(ptr));
            return -1;
        }

        prev = ptr;
        ptr = GET_NEXT_FREE(ptr);
    }

    if (prev != free_tailp)
    {
        mm_log("mm_check error: last free block (%p) should == free_tailp (%p)\n", prev, free_tailp);
        return -1;
    }

    if (freecount != count)
    {
        mm_log("mm_check error: found %d free blocks, but only %d on the freelist.\n",freecount, count);
        return -1;
    }

    mm_log("mm_check says %d free block(s)\n",count);

    return 0;
}

static void delete_free_block(void *ptr)
{
    void *next_free = GET_NEXT_FREE(ptr);
    void *prev_free = GET_PREV_FREE(ptr);

    if (ptr == free_headp && ptr == free_tailp)
    {
        free_headp = NULL;
        free_tailp = NULL;
    }
    else if (ptr == free_headp)
    {
        free_headp = next_free;
        PUT_PREV_FREE(free_headp, NULL);
    }
    else if (ptr == free_tailp)
    {
        free_tailp = prev_free;
        PUT_NEXT_FREE(free_tailp, NULL);
    }
    else
    {
        PUT_NEXT_FREE(prev_free, next_free);
        PUT_PREV_FREE(next_free, prev_free);
    }
}

static void append_free_block(void *bp)
{
    if (free_tailp)
        PUT_NEXT_FREE(free_tailp, bp);

    PUT_PREV_FREE(bp, free_tailp);
    PUT_NEXT_FREE(bp, NULL);
    free_tailp = bp;

    if (!free_headp)
        free_headp = bp;
}

static void *init_free_block(void *bp, size_t size)
{
    mm_log("\ninit free block %p of size %d\n", bp, (int)size);

    void *next = bp + size;
    void *prev = bp - GET_SIZE(bp - DSIZE);

    int prev_alloc = GET_ALLOC(HDRP(prev));
    int next_alloc = GET_ALLOC(HDRP(next));

    size_t prev_size = GET_SIZE(HDRP(prev));
    size_t next_size = GET_SIZE(HDRP(next));

    mm_log("prev = %p (%d,%d); next = %p (%d,%d)\n", prev, (int)prev_size, prev_alloc, next, (int)next_size, next_alloc);

    if (prev_alloc && next_alloc) /* Case 1 */
    {
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP_S(bp, size), PACK(size, 0));

        append_free_block(bp);
    }
    else if (prev_alloc && !next_alloc)  /* Case 2 */
    {
        int new_size = size + next_size;

        mm_log("coalesce next %p (%d) to make %d\n", next, (int)next_size, (int)new_size);

        delete_free_block(next);

        append_free_block(bp);

        PUT(HDRP(bp),            PACK(new_size, 0));
        PUT(FTRP_S(bp, new_size), PACK(new_size, 0));
    }
    else if (!prev_alloc && next_alloc) /* Case 3 */
    {
        size_t new_size = size + prev_size;

        mm_log("coalesce prev %p (%d) to make %d\n", prev, (int)prev_size, (int)new_size);

        PUT(HDRP(prev),             PACK(new_size, 0)); /* Resize header of prev */
        PUT(FTRP_S(prev, new_size), PACK(new_size, 0)); /* Resize footer */

        bp = prev;
    }
    else /* Case 4 */
    {
        size_t new_size = size + prev_size + next_size;

        mm_log("coalesce both prev %p (%d) and next %p (%d) to make %d\n", prev, (int)prev_size, next, (int)next_size, (int)new_size);

        /* Delete both prev blocks off the list; then append this new block */
        delete_free_block(prev);
        delete_free_block(next);

        mm_log("deleted %p and %p. tail=%p\n",prev,next,free_tailp);

        PUT(HDRP(prev),              PACK(new_size, 0));
        PUT(FTRP_S(next, next_size), PACK(new_size, 0));

        bp = prev;

        append_free_block(bp);
    }

    ASSERT;
    
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(bp + size - WSIZE, PACK(0, 1)); /* New epilogue header */
    mm_log("setting up epilogue at %p\n", bp + size);

    return init_free_block(bp, size);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += (2*WSIZE);

    mm_log("\n\n*** init ***\n");

    free_tailp = NULL;
    free_headp = NULL;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    ASSERT;
    
    return 0;
}

static void *find_fit(size_t size)
{
    void * ptr = free_headp;
    while (ptr)
    {
        size_t bsize = GET_SIZE(HDRP(ptr));
        mm_log("considering %p (%d)\n", ptr, (int)bsize);
        if (bsize >= size)
            return ptr;
        
        ptr = GET_NEXT_FREE(ptr);
    }
    return NULL;
}

static void place(void *ptr, size_t size)
{
    mm_log("placing at %p, size %d\n", ptr, (int)size);

    size_t free_size = GET_SIZE(HDRP(ptr));
    size_t rem = free_size - size;
    if (rem < DSIZE+WSIZE*2)
    {
        size = free_size;
    }

    /* Delete from the freelist */
    delete_free_block(ptr);

    PUT(HDRP(ptr), PACK(size, 1));
    PUT(FTRP(ptr), PACK(size, 1));

    mm_log("header = (%d,%d)\n", GET_SIZE(HDRP(ptr)), GET_ALLOC(HDRP(ptr)));
    mm_log("footer = (%d,%d)\n", GET_SIZE(FTRP(ptr)), GET_ALLOC(FTRP(ptr)));

    /* See if we should split */
    if (size != free_size)
    {
        void *split_pt = ptr + size;

        init_free_block(split_pt, free_size-size);
    }
}

static size_t adjust_requested_block_size(size_t size)
{
    size_t asize;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
    {
        //MIN
        //needs enough for header, footer, prev_ptr, next_ptr (all words = 4*w = 2*d)
        asize = 2*DSIZE;
    }
    else
    {
        //add enough for header, footer (we have enough for ptr since it's > DSIZE)
        size += WSIZE*2;
        asize = DSIZE * ((size + (DSIZE-1)) / DSIZE); //padding
    }

    return asize;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    mm_log("\nmalloc(%d)\n\n", (int)size);

    ASSERT;
        
    char *bp;
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    size_t asize = adjust_requested_block_size(size);
    
    /* Search the free list for a fit */
    bp = find_fit(asize);
    if (!bp)
    {
        mm_log("requesting more memory...\n");
        /* No fit found. Get more memory and place the block */
        size_t extendsize = MAX(asize,CHUNKSIZE);
        if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
            return NULL;
    }
    
    place(bp, asize);
    
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    mm_log("\nfree(%p)\n", ptr);

    init_free_block(ptr, size);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t req_size)
{
    size_t new_size = adjust_requested_block_size(req_size);

    size_t size = GET_SIZE(HDRP(ptr));

    mm_log("\nrealloc(%p, %d => %d)\noriginal=%d\n\n", ptr, (int)req_size, (int)new_size, (int)size);

    if (new_size < size)
    {
        mm_log("newsize < original! keep pointer.\n");
        size_t remain = size - new_size;
        if (remain >= DSIZE+WSIZE*2)
        {
            PUT(HDRP(ptr), PACK(new_size, 1));
            PUT(FTRP(ptr), PACK(new_size, 1));

            mm_log("have some leftover (%d), which is enough for a new block.\n",(int)remain);
            //split
            void *split_pt = ptr + new_size;

            init_free_block(split_pt, remain);
        }
        return ptr;
    }

    /* Okay, we need to find some new space. See if there are adjacent blocks we can coalesce into */
    void *new_ptr;

    void *prev_ftr = ptr - DSIZE;
    void *next_hdr = ptr + size - WSIZE;

    size_t extra_space = 0;
    char c_prev = 0;
    char c_next = 0;

    if (!GET_ALLOC(prev_ftr))
    {
        extra_space += GET_SIZE(prev_ftr);
        c_prev = 1;
    }
    if (!GET_ALLOC(next_hdr))
    {
        extra_space += GET_SIZE(next_hdr);
        c_next = 1;
    }

    size_t coalesced_size = extra_space + size;

    if (coalesced_size >= new_size)
    {
        mm_log("prev open = %d; next open = %d. together with %d, we have %d total space.\n",c_prev,c_next,(int)size,(int)coalesced_size);

        new_ptr = ptr;
        if (c_prev)
        {
            new_ptr -= GET_SIZE(prev_ftr); //move ptr back to prev block
            delete_free_block(new_ptr);

            memcpy(new_ptr, ptr, size);
        }
        if (c_next)
        {
            delete_free_block(ptr + size);
        }

        /* We can now see if there's enough room to split another block */
        size_t remain = coalesced_size - new_size;
        if (remain >= DSIZE+WSIZE*2)
        {
            PUT(HDRP(new_ptr), PACK(new_size, 1));
            PUT(FTRP(new_ptr), PACK(new_size, 1));

            mm_log("have some leftover (%d), which is enough for a new block.\n",(int)remain);
            //split
            void *split_pt = new_ptr + new_size;

            init_free_block(split_pt, remain);
        }
        else
        {
            PUT(HDRP(new_ptr), PACK(coalesced_size, 1));
            PUT(FTRP(new_ptr), PACK(coalesced_size, 1));
        }

        return new_ptr;
    }

    mm_log("can't coalesce. have to malloc new & free this thing\n");

    /* Can't coalesce. Have to malloc new & free this block :( */
    new_ptr = mm_malloc(req_size);
    memcpy(new_ptr, ptr, size);

    init_free_block(ptr, size);
    
    return new_ptr;
}

