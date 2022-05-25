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

#define ALIGNMENT 8
#define LIST_NUM 24  /* list_model:(alignment)2-3,4-7,8-15*/
#define START_OFF (LIST_NUM+1)*8//byte
#define ALIGN(size)  ALIGNMENT* ((size + (ALIGNMENT) + (ALIGNMENT/2-1)) / ALIGNMENT)

#define GET(p) (*(unsigned int*)(p))//cast to 32 and ret a unsigned v  asss
#define PUT(p,val) (*(unsigned int *)(p)=(val))//cast p to 32 and aaaa
#define GET_PTR(p) (*(long*)(p))          //cast to 64aaa
#define PUT_PTR(p,val) (*(long*)(p)=(val))//cast to 64
//define GET_SIZE(p)  (GET(HDRP(p)) & ~0x7) 
#define HDRP(bp)       ((char *)(bp) - ALIGNMENT/2)   //cast to char
#define GET_SIZE(p)  (GET(HDRP(p)) & ~0x7)  
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(bp) - ALIGNMENT) //line:vm:mm:ftrp

                 //cast to uni
#define GET_ALLOC(p) (GET(HDRP(p)) & 0x1)                    //line:vm:mm:getalloc



#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(bp)) //line:vm:mm:nextblkp
#define NEXT_FREE(bp)  (char*)(GET_PTR(bp))
#define PREV_FREE(bp)  (char*)(GET_PTR((char*)bp+ALIGNMENT))

#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst 

static void *extend_heap(size_t aligns);
static int bit_len(unsigned int n);
static void* find_fit(size_t asize);
static void place(void *bp, size_t asize);
inline static char* get_listp(int size);
inline static int get_list_index(int size);
static void unlink_list(char* bp,int size);
static void init_free(char* bp,int size);
static void init_alloc(char* bp,int size);
static void Coalesce();

static char* startp=0;
static int free_time=1;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    
    startp=mem_heap_lo();
    if(mem_sbrk(START_OFF)==(void*)-1)
        return -1;
    memset(startp,0,START_OFF);
    
    if(extend_heap(CHUNKSIZE/ALIGNMENT)==NULL)
        return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      

    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    asize = ALIGN(size)>=3*ALIGNMENT?ALIGN(size):3*ALIGNMENT;

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {  
        place(bp, asize);      
        return bp;
    }
    Coalesce();

    if ((bp = find_fit(asize)) != NULL) {  //line:vm:mm:findfitcall
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = asize > CHUNKSIZE ? asize : CHUNKSIZE;                 //line:vm:mm:growheap1
    if ((bp = extend_heap(extendsize/ALIGNMENT)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    free_time++;
    init_free(ptr,GET_SIZE(ptr));
    if(free_time%100==0) Coalesce();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(ptr);
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}

static void *extend_heap(size_t aligns) 
{
    
    char *bp;
    size_t size=aligns<<3;

    /* Allocate an even number of words to maintain alignment */
    if ((long)(bp = mem_sbrk(size)) == -1)  
        return NULL;
    init_free(bp,size);
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 
    
    return bp;                                        
}

static void* find_fit(size_t asize)
{
    
    int list_index=get_list_index(asize);
    char* listp=get_listp(asize);
    char* checkp=NEXT_FREE(listp);

    if(checkp!=NULL)
    {
        for(;checkp!=NULL;checkp=NEXT_FREE(checkp))
        {
            if(GET_SIZE(checkp)>=asize)
                return checkp;
        }
    }

    while (list_index<LIST_NUM-1)
    {
        list_index++;
        listp+=8;
        checkp=NEXT_FREE(listp);
        if(checkp!=NULL)
            return checkp;
    }

    return NULL;
}

static void place(void *bp, size_t asize)
{
    
    size_t csize = GET_SIZE(bp);   
    //printf("%lx",csize);
    unlink_list(bp,csize);

    if ((csize - asize) >= (3*ALIGNMENT)) { 
        init_alloc(bp,asize);
        init_free(bp+asize,csize-asize);
    }
    else 
        init_alloc(bp,csize);
}

inline static char* get_listp(int size)
{
    return (char*)startp+(bit_len(size>>3)-2)*8;
}

inline static int get_list_index(int size)
{
    return bit_len(size>>3)-2;
}

//clear in free list so cant find in findfit,but still not alloced 
//not change itself,just cant find
static void unlink_list(char* bp,int size)
{
    assert(GET_ALLOC(bp)==0);
    
    //cant find from latter
    if(NEXT_FREE(bp)!=NULL)
        PUT_PTR(NEXT_FREE(bp)+ALIGNMENT,(long)PREV_FREE(bp));
    //cant find from former
    if(PREV_FREE(bp)!=NULL)
        PUT_PTR(PREV_FREE(bp),(long)NEXT_FREE(bp));
}
//init and link it 
static void init_free(char* bp,int size)
{
    
    PUT(HDRP(bp), PACK(size, 0)); //
    /* if(size==4239376) *///printf("%d\n",GET_SIZE(bp));
    PUT(FTRP(bp), PACK(size, 0));
    char *listp=get_listp(size);

    PUT_PTR(bp,GET_PTR(listp));
    PUT_PTR(listp,(long)bp);
    char *prev=bp+8;
    PUT_PTR(prev,(long)listp);//prev is list head
    if(GET_PTR(bp)!=0)
        PUT_PTR(NEXT_FREE(bp)+ALIGNMENT,(long)bp);
}
static void init_alloc(char* bp,int size)
{
    
    PUT(HDRP(bp),PACK(size,1));
}

static void Coalesce()
{
    char* freep=startp+START_OFF,*tmpp;
    int nfsize;
    while (GET_ALLOC(freep)==0||GET_SIZE(freep)!=0)
    {
        
        tmpp=NEXT_BLKP(freep);
        //printf("%lx %lx\n",freep,tmpp);
        if(GET_ALLOC(freep)==0&&GET_ALLOC(tmpp)==0)
        {
            nfsize=GET_SIZE(freep);
            unlink_list(freep,GET_SIZE(freep));
            for(;GET_ALLOC(tmpp)==0&&GET_SIZE(tmpp)!=0;tmpp=NEXT_BLKP(tmpp))
            {
                nfsize+=GET_SIZE(tmpp);
                unlink_list(tmpp,GET_SIZE(tmpp));
            }
            //printf("%lx\n",nfsize);
            assert(tmpp==freep+nfsize);
            init_free(freep,nfsize);
            assert(GET_SIZE(freep)==nfsize);
            freep=tmpp;
        }
        freep=tmpp;
        while ((GET_ALLOC(freep)==1)&&(GET_SIZE(freep)!=0))
            freep=NEXT_BLKP(freep);
    }
    
}

static int bit_len(unsigned int n)
{
    int powof2[32] = 
    { 
                 1,           2,           4,           8,         16,          32, 
                64,         128,         256,         512,       1024,        2048, 
              4096,        8192,       16384,       32768,      65536,      131072, 
            262144,      524288,     1048576,     2097152,    4194304,     8388608, 
          16777216,    33554432,    67108864,   134217728,  268435456,   536870912, 
        1073741824,  2147483648 
    } ; 

    int left = 0 ; 
    int right = 31 ; 

    while (left <= right) 
    { 
        int mid = (left + right) / 2 ; 

        if (powof2[mid] <= n) 
        { 
            if (powof2[mid + 1] > n) 
                return mid + 1; // got it! 
            else // powof2[mid] < n, search right part
                left = mid + 1 ; 
        } 

        else // powof2[mid] > n, search left part 
            right = mid - 1 ; 
    } 

    // not found  
    return -1 ; 
}
/*void p(int size)
{
    long* xxx=startp;
    int* xx=startp;
    for(int i=0;i<size;i++)printf("%lx %lx\n",&xxx[i],xxx[i]);
    printf("%x\n",xx[1073]);
}*/