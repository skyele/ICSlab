/*
 * mm.c
 *
 * <guest377>
 * ����ƽ
 * mail:1100012779@pku.edu.cn
 *
 * overviw:�������е�ע����Ӣ�Ļ����ˣ�����£�
 *
 * 0����ʹ�õ��Ƿ��������״����䡢ֱ�Ӻϲ�
 * 
 * 1������ÿ����С��(16�ֽڣ��ҷ�Ϊ(ÿ�������������ĸ��ֽڣ���
 *   �ѷ���飺[head][content][content][foot]   ���п飺[head][pred][succ][foot]
 *           ͷ��Ϣ       ����        ����Ϣ       ͷ��Ϣ|ǰ������|��̿���|����Ϣ
 * ����pred��succ�����ط���û�б���ǰ���ͺ�̵�ָ�룬��Ϊÿ��ʵ�������Ѳ�����ڵ���2^32��
 * ������pred��succ�����ط�ֻ��Ҫ����˿���ǰ���������п��ƫ��λ�ã�4�ֽڣ������ˣ�
 * ��������С���ֻ��16�ֽڣ���ʡ�˿��С�
 * 
 * 2�����ڷ����������ö����ݴν����Ϊ20�ࡾ[16,32)��[32,64)��[2^4,2^5) ����[2^n,2^(n+1))����[2^23,infinite)��
 * ��Ѱ������п�ʱ�����ж����ݴεĴ��ڣ�������λ������١� 
 *
 * 3��ÿ������������ֵ��������ͷfree_list_p[i], ����βfree_list_t[i]���ֱ�ָ�������ͷ��β)��
 * ����ͷ����Ϊfree_list_p,����β����Ϊfree_list_t,��ʼ��ʱ�ڶѵ��ʼ������40 * DSIZE����������ָ�롣
 * �������Ϊ����ôfree_list_p = free_list_t = NULL��
 * 
 * 4������Ĵ洢˳���յ�ַ˳��
 * �������Ҫ����Ϊinsert��remove���ֱ�����������в���/ɾ��һ�����п�
 * 
 * 5���ϲ��ķ�ʽΪֱ�Ӻϲ�
 * �������һ�����п飬��ô�ͼ������ǰ��飬����п��е���ô��ֱ�Ӻϲ�
 * �ϲ��ֳ������������ǰ����Ƿ�Ϊ���п�������
 * 
 * 6�����䷽ʽΪ�״����䣨����ÿ����������ʱ��ֻҪ�������ʵľ�ֱ�����䣩
 *
 * 7�����������ռ��extend_heap�У�����ѵ����һ��Ϊ���У���ô�����ʵ�������Ҫ���ŵ�sizeֵ
 * 
 * 8���ú��inline��װ�Ƚϳ��õĺ��������㣬��߷�װ�ԣ��������
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */


/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Doubleword size (bytes) */
#define TSIZE       12
#define QSIZE       16
#define MINBLOCK    16
#define INFOSIZE    8
#define CHUNKSIZE   (1 << 8)  /* Extend heap by this amount (bytes) */ 

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            
#define PUT(p, val)  (*(unsigned int *)(p) = (val))  

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   
#define GET_ALLOC(p) (GET(p) & 0x1)                  

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr bp, compute size/pred/succ */
#define SIZE(bp)      (GET_SIZE(HDRP(bp)))
#define PREV_SIZE(bp) (GET_SIZE((char *)(bp) - DSIZE))
#define NEXT_SIZE(bp) (GET_SIZE((char *)(bp) + SIZE(bp) - WSIZE))
#define ALLOC(bp)     (GET_ALLOC(HDRP(bp)))
#define PREV_ALLOC(bp) (GET_ALLOC((char *)(bp) - DSIZE))
#define NEXT_ALLOC(bp) (GET_ALLOC((char *)(bp) + SIZE(bp) - WSIZE))
#define PRED(bp)      ((char *)(bp) - GET(bp))
#define SUCC(bp)      ((char *)(bp) + GET((char *)(bp) + WSIZE))

/* Given block ptr bp, change pred/succ */
#define PUT_PRED(bp, pre)  PUT(bp, (unsigned int)((char *)(bp) - (char *)(pre)))
#define PUT_SUCC(bp, suc)  PUT((char *)(bp) + WSIZE, (unsigned int)((char *)(suc) - (char *)(bp)))


/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
static size_t *free_list_p;  /* ����ͷ����,��i������ͷΪfree_list_p[i] */
static size_t *free_list_t;  /* ����β����,��i������βΪfree_list_t[i] */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t asize);    //�������Ŀռ�
static void *place(void *bp, size_t asize);//��ֿ�
static void *find_fit(size_t asize);       //Ѱ������Ŀ��п�
static void *coalesce(void *bp);           //�ϲ����п�
static void printblock(void *bp);          //�������Ϣ
static void checkblock(void *bp);          //����
static void *insertblock(void *bp);        //����һ���µĿ��п鵽���������� 
static void removeblock(void *bp);         //ɾ�����������е�ĳ�����п�
static void checkfreelist(void);           //���ÿ����������
static int blockindex(unsigned int size);  //��������ڵ����������

/*
 * mm_init - Called when a new trace starts.
 * �Ѻ�һЩ�����ĳ�ʼ��
 */
int mm_init(void) 
{
	/* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(40 * DSIZE + QSIZE)) == (void *)-1) //line:vm:mm:begininit
		return -1;
    
	/* ���������ʼ�� */
	free_list_p = (size_t *)heap_listp;
	free_list_t = (size_t *)(heap_listp + 20 * DSIZE);
	int i;
	for (i = 0; i < 20; ++i){
		free_list_p[i] = (size_t)NULL;
		free_list_t[i] = (size_t)NULL;
	}
	heap_listp += 40 * DSIZE;   

	/* heap��ͷβ��װ */
	PUT(heap_listp, 0);                      /* Alignment padding */
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + DSIZE, PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + TSIZE, PACK(0, 1));     /* Epilogue header */  
	heap_listp += DSIZE;
	

	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE) == NULL) 
		return -1;
	return 0;
}

/*
 * malloc - Allocate a block.
 * �ȴӿ����������ҵ��������Ŀ��п飬���û�о��������Ŀռ�
 *
 */
void *mm_malloc(size_t size) 
{
	size_t asize;      /* Adjusted block size */
    char *bp;      
    
	/* Adjust block size to include overhead and alignment reqs. */
    if (size + INFOSIZE <= MINBLOCK)                    
		asize = MINBLOCK;                                     
    else
		asize = DSIZE * ((size + INFOSIZE + (DSIZE - 1)) / DSIZE); 
	
	/* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
		bp = place(bp, asize);
		return bp;
    }

    /* No fit found. Get more memory and place the block */
	if (asize > CHUNKSIZE){
		/* big size - require asize directly */ 
		if ((bp = extend_heap(asize)) == NULL)  
			return NULL;
		/* simplified place */
		removeblock(bp);
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		return bp;
	}
	else{
		/* require CHUNKSIZE */
		if ((bp = extend_heap(CHUNKSIZE)) == NULL)  
				return NULL;
		place(bp, asize);
		return bp;
	}
}

/*
 * free - free a allocted block.
 * ���ѷ���Ŀ����°�װ�ɿ��п飬���Ҳ��뵽����������
 */
void mm_free(void *bp)
{
	if(bp == NULL) 
		return;

    size_t size = SIZE(bp);

	/* repack the block into freestate */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

	/* insert bp into free_list and coalesce*/
	coalesce(insertblock(bp));
}


/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.
 * 1�����ptrΪNULL,�൱��malloc
 * 2�����sizeΪ0���൱��free
 * 3�����ԭ���Ŀ��Ѿ��㹻����ô����ʣ��Ĳ����Ƿ��ܲ�ֳɿ��п�
 * 4���������Ŀ��ԭ���Ŀ���ܺ��Ѿ��㹻����ô����ʣ��Ĳ����Ƿ��ܲ�ֳɿ��п�
 * 5��mallocһ���飬Ȼ�����ݸ��ƹ�ȥ
 *
 */
void *mm_realloc(void *ptr, size_t size)
{
	size_t oldsize, asize, freesize;
	char *free_bp, *next;
    void *newptr;

	/* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL)
		return mm_malloc(size);
    
	/* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
		mm_free(ptr);
		return NULL;
	}

    oldsize = SIZE(ptr);
	if (size + INFOSIZE <= MINBLOCK)                    
		asize = MINBLOCK;                                     
    else
		asize = DSIZE * ((size + INFOSIZE + (DSIZE - 1)) / DSIZE);

    if(oldsize >= asize){
		/* oldsize is enough */
		if (oldsize - asize >= MINBLOCK){
			/* turn remainder size into free block */
			if (!NEXT_ALLOC(ptr)){
				/* if next is free, can coalesce */
				oldsize += NEXT_SIZE(ptr);
				removeblock(NEXT_BLKP(ptr));
		    }
			/* remainder is free */
			PUT(HDRP(ptr), PACK(asize, 1));
			PUT(FTRP(ptr), PACK(asize, 1));
			/* repack free-block */
			free_bp = NEXT_BLKP(ptr);
			PUT(HDRP(free_bp), PACK(oldsize - asize, 0));
			PUT(FTRP(free_bp), PACK(oldsize - asize, 0));
			/* relink free-block */
			insertblock(free_bp);
		}
		return ptr;
	}
	else if (!NEXT_ALLOC(ptr) && NEXT_SIZE(ptr) + oldsize >= asize){
		/* next's a free-block and can fill the gap */
		next = NEXT_BLKP(ptr);
		removeblock(next);
		if (SIZE(next) + oldsize - asize >= MINBLOCK){
			/* remainder is big than MINBLOKP, can turn into a free-block */
			freesize = SIZE(next) + oldsize - asize;
			/* repack the alloc-block */
			PUT(HDRP(ptr), PACK(asize, 1));
			PUT(FTRP(ptr), PACK(asize, 1));
			/* repack the free-block */
			free_bp = NEXT_BLKP(ptr);
			PUT(HDRP(free_bp), PACK(freesize, 0));
			PUT(FTRP(free_bp), PACK(freesize, 0));
			/* relink the free-block */
			insertblock(free_bp);
		}
		else{
			/* the all next-block and oldblock turn to a alloc-block */
			PUT(HDRP(ptr), PACK(SIZE(next) + oldsize, 1));
			PUT(FTRP(ptr), PACK(SIZE(next) + oldsize, 1));
		}
		return ptr;
	}
	else{
		/* just malloc one */
		newptr = mm_malloc(size);
		memcpy(newptr, ptr, size);//move block's data
		mm_free(ptr);
		return newptr;
	}
}

/* 
 * The remaining routines are internal helper routines 
 */

/*
 * calloc - Allocate the block and set it to zero.
 * ����һ��size�飬���ҳ�ʼ��Ϊ��
 */
void *calloc (size_t nmemb, size_t size)
{
   size_t bytes = nmemb * size;
   void *newptr;
   /* malloc */
   newptr = mm_malloc(bytes);
   /* initial */
   memset(newptr, 0, bytes);

   return newptr;
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 * �ϲ���ǰ�᣺bpΪһ�����п飬��������
 * �ϲ��ֳ��������1��ǰ�����ѷ���飬����Ҫ�ϲ�
 *                 2�������ǿ��п飬ɾ������Ŀ飬��bp�ϲ���һ����Ŀ��ٲ���
 *                 3��ǰ���ǿ��п飬ɾ��ǰ��Ŀ飬��bp�ϲ���һ����Ŀ����
 *                 4��ǰ���ǿ��п飬ɾ�����飬��bp�ϲ���һ����Ŀ����
 */
static void *coalesce(void *bp) 
{	
	size_t prev_alloc = PREV_ALLOC(bp);
    size_t next_alloc = NEXT_ALLOC(bp);
    size_t size = SIZE(bp);

    if (prev_alloc && next_alloc)              /* Case 1 */
		return bp;

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
		char *next = NEXT_BLKP(bp);
		size += SIZE(next);
		/* delete link */
		removeblock(next);
		removeblock(bp);
		/* repack bp */
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size,0));
		/* relink */
		insertblock(bp);
		return bp;
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
		char *prev = PREV_BLKP(bp);
		size += SIZE(prev);
		/* delete link */
		removeblock(prev);
		removeblock(bp);
		/* repack bp */
		PUT(HDRP(prev), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
		/* relink */
		insertblock(prev);
		return prev;
    }

    else {                                     /* Case 4 */
		char *prev = PREV_BLKP(bp);
		char *next = NEXT_BLKP(bp);
		size += SIZE(prev) + SIZE(next);
		/* delete link */
		removeblock(prev);
		removeblock(next);
		removeblock(bp);
		/* repack bp */ 
		PUT(HDRP(prev), PACK(size, 0));
		PUT(FTRP(next), PACK(size, 0));
		/* relink */
		insertblock(prev);
		return prev;
    }
}

/* 
 * insertblock - insert bp into free-list 
 * �����ǰ�᣺bp��һ�����п飬���ǲ��ڿ���������
 * ����Ĺ���ʽ�ǰ���ַ˳��
 */
static inline void *insertblock(void *bp){
	
	int i = blockindex(SIZE(bp));

	if (free_list_p[i] == (size_t)NULL)
		/* the free one is the only free-block */
		free_list_p[i] = free_list_t[i] = (size_t)bp;
	else{
		if ((size_t)(bp) < free_list_p[i]){
			/* bp has the min-adrr, trun to free_list_p */
			PUT_SUCC(bp, free_list_p[i]);
			PUT_PRED(free_list_p[i], bp);
			free_list_p[i] = (size_t)bp;
		}
		else if ((size_t)(bp) > free_list_t[i]){
			PUT_PRED(bp, free_list_t[i]);
			PUT_SUCC(free_list_t[i], bp);
			free_list_t[i] = (size_t)bp;
		}
		else{
			char *temp = (char *)free_list_p[i];
			/* find pred of bp */
			while (SUCC(temp) < (char *)bp)
				temp = SUCC(temp);
			/* relink bp */
			PUT_PRED(bp, temp);
			PUT_SUCC(bp, SUCC(temp));
			PUT_PRED(SUCC(temp), bp);
			PUT_SUCC(temp, bp);
		}
	}
	return bp;
}

/* 
 * removeblock - remove bp out of free-list 
 * ɾ��������ķ�ʽ��
 * 1���������������ͷ/β��ֱ�����±�������ͷβ
 * 2�������㲻��ͷβ����ͷβ��ǰ���ͺ��������һ��
 */
static inline void removeblock(void *bp){
	
	int i = blockindex(SIZE(bp));

	if (free_list_p[i] == free_list_t[i]){
		/* the free one is the only free-block */
		free_list_p[i] = free_list_t[i] = (size_t)NULL;
	}
	else if ((size_t)(bp) == free_list_p[i])
		/* bp is free_list's head */
		free_list_p[i] = (size_t)SUCC(bp);
	else if ((size_t)(bp) == free_list_t[i])
		/* bp is free_list's tail*/
		free_list_t[i] = (size_t)PRED(bp);
	else{
		/* else relink bp's pred/succ */
		PUT_PRED(SUCC(bp), PRED(bp));
		PUT_SUCC(PRED(bp), SUCC(bp));
	}
}

/*
 * blockindex - ͨ����Ĵ�С�����ؿ����ڵķ���������±�
 * ����Ϊ20�顾[16,32)��[32,64)��[2^4,2^5) ����[2^n,2^(n+1))����[2^23,infinite)��
 * ��Ѱ������λ�������
 */
static inline int blockindex(unsigned int size){
	int i = 0;
	unsigned int mul = 32;
	while (i < 19 && mul < size){
		++i;
		mul <<= 1;
	}
	return i;
}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 * �¿���words�ֽڵĿ飨���룩�����ǰ���п��п������ã����Ҽ����¿��ٿ�Ĵ�С
 */
static void *extend_heap(size_t asize) 
{
	char *bp, *prev = (char *)mem_heap_hi() - 7;/* prev:���һ����Ч��ĽŲ� */

	/* save space by use last-block if last-block is free */
	if (!GET_ALLOC(prev)){
		int realneed = (int)(asize - GET_SIZE(prev));
		prev = prev - GET_SIZE(prev) + DSIZE;//move prev from foot to bp(real)
		removeblock(prev);
		if ((bp = mem_sbrk(realneed)) == (void *)-1)  
			return NULL;
		bp = prev;
	}
	else{
		if ((bp = mem_sbrk(asize)) == (void *)-1)  
			return NULL;
	}	

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(asize, 0));         /* Free block header */  
    PUT(FTRP(bp), PACK(asize, 0));         /* Free block footer */  
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 

	return insertblock(bp);
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 * ����ǰ�벿��������ʣ��������ûʣ���ɾ��
 * ��ʵ���ԸĽ��ɷ����벿������ǰ�벿�����ǰ�벿��С����ԭ���ķ������������У�
 * ��ô�͵���
 */
static void *place(void *bp, size_t asize)
{
	size_t csize = SIZE(bp);   

	removeblock(bp);
    
	if ((csize - asize) >= MINBLOCK) { 
		size_t freesize = csize - asize;
		
		/* repack the alloc-block */
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));

		/* repack the free-block */
		char *free_block = NEXT_BLKP(bp);;
		PUT(HDRP(free_block), PACK(freesize, 0));
		PUT(FTRP(free_block), PACK(freesize, 0));

		/* relink the free-block */
		insertblock(free_block);
    }
    else {
		/* repack the block */
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
    }

	return bp;
}

/* 
 * find_fit - �ڿ���������Ѱ������asize��С�Ŀ��п� 
 * �״�����
 */
static void *find_fit(size_t asize)
{
	void *bp;
	int i;

	for (i = blockindex(asize); i <= 19; ++i){
		if (free_list_p[i] == (size_t)NULL)
			continue;
		/* find first fit */
		for (bp = (char *)free_list_p[i];; bp = SUCC(bp)) {
			if (SIZE(bp) >= asize){
				return bp;
			}
			if ((size_t)bp == free_list_t[i])
				break;
		}
	}
	return NULL;
}

/*
 * ��麯����for debug and show the heap
 */

/*
 * mm_checkheap - ���ѵ����п飬���ҵ��ü�����������checkfreelist
 *    
 */
void mm_checkheap(int verbose) {

	verbose = verbose;
	printf("\n[check_heap] :\n");
	printf("heap_listp: %p free_list_p: %p free_list_t: %p\n", heap_listp, free_list_p, free_list_t);
	
	/* ���ÿ���������ͷβָ�� */
	printf("     free_list_p                  free_list_t\n");
	int i;
	for (i = 0; i < 20; ++i){
		printf("[%02d] %11p %11p ", i, free_list_p + i, (void *)free_list_p[i]);
		printf("[%02d] %11p %11p\n", i, free_list_t + i, (void *)free_list_t[i]);
	}
	printf("\n");

	/* ���������ͷָ�� */
	char *bp = heap_listp;
    if ((SIZE(heap_listp) != DSIZE) || !ALLOC(heap_listp))
		fprintf(stderr, "Error: Bad prologue header\n");
    checkblock(heap_listp);

	/* �����е����п�(��������) */
    for (bp = heap_listp; SIZE(bp) > 0; bp = NEXT_BLKP(bp)) {
		if (verbose) 
			printblock(bp);
		checkblock(bp);
    }

	/* ����β */
    if (verbose)
		printblock(bp);
	if ((SIZE(bp) != 0) || !(GET_ALLOC(HDRP(bp))))
		fprintf(stderr, "Error: Bad epilogue header\n");

	/* ������������� */
	checkfreelist();
	printf("check over\n");
}
/*
 * printblock - ��ӡ�����Ϣ
 */
static void printblock(void *bp) 
{
    size_t size, alloc;
	size = SIZE(bp);
    alloc = ALLOC(bp);  

    if (size == 0) {
		/* ��β��ӡ���� */
		printf("%p: EOL\n", bp);
		return;
    }

    printf("(%p:%p) alloc/free: [%c] size: [%lu]\n", bp, (char *)FTRP(bp) - WSIZE, (alloc ? 'a' : 'f'), size);
	printf("                          prev: %p next: %p\n", PREV_BLKP(bp), NEXT_BLKP(bp));
	
	/* ���п����Ϣ */
	if (!alloc)
		printf("                          pred: %p succ: %p\n", PRED(bp), SUCC(bp));
}
/*
 * checkblock - ���һ����
 */
static void checkblock(void *bp) 
{
    /* ���ָ����� */
	if ((size_t)bp % 8)
		fprintf(stderr, "Error: %p is not doubleword aligned\n", bp);
	/* ���ͷβ��� */
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
		fprintf(stderr, "Error: header does not match footer\n");
}
/*
 * checkfreelist - �����������е�ÿ�����
 */
static void checkfreelist(void)
{
	printf("\n[check_list] :\n");
	char *temp;
	int i;
	int count = 0;
	/* ���ÿ���������� */
	for (i = 0; i < 20; ++i){
		temp = (char *)free_list_p[i];
		if (temp == NULL)
			continue;
		printf("list: %d free_list_p: %p free_list_t: %p\n", i, (char *)free_list_p[i], (char *)free_list_t[i]);
		/* ����ͷβָ����� */
		if (free_list_p[i] > free_list_t[i] || (void *)free_list_p[i] < mem_heap_lo() || (void *)free_list_t[i] > mem_heap_hi())
			fprintf(stderr, "Error: free_list addr error!\n");
		else{
			while (1){
				/* ��ӡ�����е�ÿһ���� */
				printblock(temp);
				checkblock(heap_listp);
				++count;
				/* ������ */
				if (ALLOC(temp))
					fprintf(stderr, "Error: alloc one in free list!\n");
				/* ���ǰ������ */
				if (temp != (char *)free_list_t[i] && PRED(SUCC(temp)) != temp)
					fprintf(stderr, "Error: block succ can't link it!\n");
				if (temp == (char *)free_list_t[i])
					break;
				else
					temp = SUCC(temp);
			}
		}
	}
	printf("there are %d free-block\n", count);
}
