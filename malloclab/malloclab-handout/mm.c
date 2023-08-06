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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) (x > y ? x : y)
#define MIN(x, y) (x > y ? y : x)

#define PACK(size, alloc) ((size | alloc))

#define GET(p)      (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

#define SET_PRED(bp, val) (*((unsigned int*)(bp)    ) = val)
#define SET_SUCC(bp, val) (*((unsigned int*)(bp) + 1) = val)
#define GET_PRED(bp)      (*(unsigned int *)(bp))
#define GET_SUCC(bp)      (*((unsigned int *)(bp) + 1))

static char* heap_list;
static char* prev_node;
static char* free_list;

static void insert(void* bp)
{
    if (bp == NULL)
        return;

    if (free_list == NULL)
    {
        free_list = bp;
    }
    else
    {
        SET_PRED(free_list, bp);
        SET_SUCC(bp, free_list);
        free_list = bp;
    }
}

static void delete_node(void* bp)
{
    if (bp == NULL)
        return;
    
    void* pred = GET_PRED(bp);
    void* succ = GET_SUCC(bp);

    SET_PRED(bp, 0);
    SET_SUCC(bp, 0);

    if (pred == NULL && succ == NULL)
    {
        free_list = NULL;
    }
    else if ( pred ==  NULL)
    {
        SET_PRED(succ, 0);
        free_list = succ;
    }
    else if (succ == NULL)
    {
        SET_SUCC(pred, 0);
    }
    else
    {
        SET_SUCC(pred, succ);
        SET_PRED(succ, pred);
    }
}

static void* coalesce(void* bp)
{
    size_t prev_alloc = (GET_ALLOC(FTRP(PREV_BLKP(bp))));
    size_t next_alloc = (GET_ALLOC(HDRP(NEXT_BLKP(bp))));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {

    }
    // 合并后面的块
    else if ( prev_alloc && !next_alloc)
    {
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        delete_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        delete_node(NEXT_BLKP(bp));
        delete_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert(bp);
    return bp;
}

static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    size = ((words % 2) ? (words + 1) : words) * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    SET_PRED(bp, 0);
    SET_SUCC(bp, 0);
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 新的序言块

    return coalesce(bp);
}

static void* find_first_fit(size_t asize)
{
    void* bp;

    for (bp = free_list; bp != 0; bp = GET_SUCC(bp))
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
            return bp;
    }
    return NULL;
}

static void* find_next_fit(size_t asize)
{
    void* bp;

    for (bp = prev_node; ; bp = GET_SUCC(bp))
    {
        if (bp && !GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
        {
            prev_node = bp;
            return bp;
        }
    }

    for (bp = heap_list; ; bp = GET_SUCC(bp))
    {
        if (bp && !GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
        {
            prev_node = bp;
            return bp;
        }
    }
    return NULL;
}

static void place(void* bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    size_t remain = csize - asize;
    delete_node(bp);

    if(remain >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(remain, 0));
        PUT(FTRP(bp), PACK(remain, 0));
        SET_PRED(bp, 0);
        SET_SUCC(bp, 0);
        coalesce(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // 申请4个字
    if ((heap_list = mem_sbrk(4 * WSIZE)) == (void*)-1)
        return -1;
    PUT(heap_list, 0);
    PUT(heap_list + 1 * WSIZE, PACK(DSIZE, 1)); // 序言块头
    PUT(heap_list + 2 * WSIZE, PACK(DSIZE, 1)); // 序言块尾
    PUT(heap_list + 3 * WSIZE, PACK(0,     0)); // 结尾块
    heap_list += (2 * WSIZE);
    //prev_node = heap_list;
    free_list = NULL;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendSize;
    char* bp;

    if (size == 0)
        return NULL;
    
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_first_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendSize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendSize / WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    SET_PRED(ptr, 0);
    SET_SUCC(ptr, 0);
    prev_node = coalesce(ptr);

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return mm_malloc(size);
    
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }

    if (size <= DSIZE)
        size = 2 * DSIZE;
    else
        size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    size_t currentSize = GET_SIZE(HDRP(ptr));
    
    if (currentSize >= size)
    {
        if ((currentSize - size) < 2 * DSIZE)
            return ptr;
        else
        {
            PUT(HDRP(ptr), PACK(size, 1));
            PUT(HDRP(ptr), PACK(size, 1));
            void* next = NEXT_BLKP(ptr);
            PUT(HDRP(next), PACK((currentSize - size), 0));
            PUT(FTRP(next), PACK((currentSize - size), 0));
            return ptr;
        }
    }
    else
    {
        void* bp = mm_malloc(size);
        if (bp == NULL) return NULL;
        memcpy(bp, ptr, currentSize);
        mm_free(ptr);
        return bp;
    }
}














