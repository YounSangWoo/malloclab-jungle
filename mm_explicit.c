
/*@author :- SHALIN SHAH @DAIICT ID :- 201101179*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "Ateam",
    /* First member's full name */
    "Youn Sang Woo",
    /* First member's email address */
    "tkddn0811@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*Additional Macros defined*/
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define OVERHEAD 16
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(size_t *)(p))
#define PUT(p, value) (*(size_t *)(p) = (value))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((void *)(bp)-WSIZE)
#define FTRP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((void *)(bp)-GET_SIZE(HDRP(bp) - WSIZE))
#define NEXT_FREEP(bp) (*(void **)(bp + WSIZE))
#define PREV_FREEP(bp) (*(void **)(bp))

static char *heap_listp = 0;
static char *head = 0;

static void *extend_heap(size_t words);
static void *place(void *bp, size_t size);
static void *find_fit(size_t size);
static void *coalesce(void *bp);
static void mm_insert(void *bp);
static void mm_remove(void *bp);

/*  Initialize the heap  */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(2 * OVERHEAD)) == NULL)
    {
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(OVERHEAD, 1));//프롤로그 헤더
    PUT(heap_listp + (2 + WSIZE), 0);//PREV_FREEP를 위한 주소 저장공간
    PUT(heap_listp + (3 + WSIZE), 0);//NEXT_FREEP를 위한 주소 저장공간
    PUT(heap_listp + (4 + WSIZE), PACK(OVERHEAD, 1));//프롤로그푸터
    PUT(heap_listp + (5 + WSIZE), PACK(0, 1));//에필로그헤더
    head = heap_listp + DSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) 
    //초기 가용메모리 공간 확보
    {
        return -1;
    }

    return 0;
}
//Here the bp is given to the user if he requests for space in heap.
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendedsize;
    char *bp;

    if (size <= 0)
    {
        return NULL;
    }

    asize = MAX(ALIGN(size) + DSIZE, OVERHEAD);

    if ((bp = find_fit(asize)))
    {
        bp = place(bp, asize);
        return bp;
    }

    extendedsize = MAX(asize, CHUNKSIZE);

    if ((bp = extend_heap(extendedsize / WSIZE)) == NULL)
    {
        return NULL;
    }

    bp = place(bp, asize);
    return bp;
}

//This thing frees the block and passes the control to coalesce where add/del of free list is done
void mm_free(void *bp)
{
    if (bp == NULL)
    {
        return;
    }

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);//반환된 메모리는 최대 공간 확보를 위해 coalesce함수에서 가용리스트 끼리 합쳐줍니다.
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

//Extend the heap in case when the heap size is not sufficient to hold the block
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    size = MAX(size, OVERHEAD);

    if ((int)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0)); 
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t previous_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
    size_t next__alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (previous_alloc && !next__alloc)
    { /* 앞 할당 뒤 free인 경우*/
            size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        mm_remove(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!previous_alloc && next__alloc)
    { /* 앞 free 뒤 할당 인 경우 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        mm_remove(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!previous_alloc && !next__alloc)
    { /*  앞 뒤 모두 free의 경우 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        mm_remove(PREV_BLKP(bp));
        mm_remove(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    mm_insert(bp);
    return bp;
}

//I inserted the new block at the beginning of the free list
static void mm_insert(void *bp)
{
    NEXT_FREEP(bp) = head;
    PREV_FREEP(head) = bp;
    PREV_FREEP(bp) = NULL;
    head = bp;
}

static void mm_remove(void *bp)
{
    if (PREV_FREEP(bp) != NULL)
    {
        NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp);
    }
    else
    {
        head = NEXT_FREEP(bp);
    }

    PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp);
}

static void *find_fit(size_t size)
{
    void *bp;

    for (bp = head; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FREEP(bp))
    {
        if (size <= GET_SIZE(HDRP(bp)))
        {
            return bp;
        }
    }

    return NULL;
}

static void *place(void *bp, size_t size)
{
    size_t totalsize = GET_SIZE(HDRP(bp));
    mm_remove(bp);
    if ((totalsize - size) >= OVERHEAD)
    {
        if (size >= 100)
        {
            PUT(HDRP(bp), PACK(totalsize - size, 0));
            PUT(FTRP(bp), PACK(totalsize - size, 0));
            bp = NEXT_BLKP(bp);
            PUT(HDRP(bp), PACK(size, 1));
            PUT(FTRP(bp), PACK(size, 1));

            coalesce(PREV_BLKP(bp));
            return bp;
        }
        else
        {
            PUT(HDRP(bp), PACK(size, 1));
            PUT(FTRP(bp), PACK(size, 1));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(totalsize - size, 0));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(totalsize - size, 0));
            coalesce(NEXT_BLKP(bp));
        }
        return bp;
    }

    else
    {
        PUT(HDRP(bp), PACK(totalsize, 1));
        PUT(FTRP(bp), PACK(totalsize, 1));
    }
    return bp;
}
