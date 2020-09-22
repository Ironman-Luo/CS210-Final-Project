// Your code for part 1 goes here.
// Do not include your main function
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <memory.h>
#include <assert.h>
#include "heap_checker.c"

#define ALIGNED_8(size)     ((size + 7) >> 3 << 3)
#define GET_SIZE(s)           ((s) & ~0x7)
#define GET_PADDING(s)        ((s) & 0x7)
#define SET_SIZE(s, padding)  ((s) | (padding))
#define BLOCK_IN_USE        0x1  /* block is alloced and it is unfree */
#define BLOCK_UN_USE        0x0 /* block is alloced and it is free */

typedef char* addrs_t;
typedef void* any_t;

/* the head structure for every alloced memory */
typedef struct block_header_s {
    uint32_t length;
    uint32_t in_use;
} block_header_t;

/* the vmalloc header for M2 */
typedef struct malloc_chunck_header_s {
    uint32_t length;                /* M1 size */
    uint32_t free_length;           /* M1 free memory */
} malloc_chunck_header_t;

#define CHUNCK_HEADER_SIZE  ALIGNED_8(sizeof(malloc_chunck_header_t))

static addrs_t s_baseptr = NULL;  /* points to base memory area, M1 */

/* 
 * Coalesce - merged together with other contiguous free memory 
 * (either side of the block being freed)
 * in block chain
 */
static void Coalesce(){
    malloc_chunck_header_t* chunckptr = (malloc_chunck_header_t*)s_baseptr;
    addrs_t maxptr = s_baseptr + chunckptr->length;
    block_header_t* bptr = (block_header_t*)(s_baseptr + CHUNCK_HEADER_SIZE);
    uint32_t block_size = GET_SIZE(bptr->length);
    block_header_t* next_bptr = (block_header_t*)((addrs_t)bptr + block_size);
    while((addrs_t)next_bptr < maxptr){
        //printf("Coalesce block %p, len %u, use %u, next block %p, len %u, use %u\n", 
        //    bptr, bptr->length, bptr->in_use, next_bptr, next_bptr->length, next_bptr->in_use);

        if(bptr->in_use == BLOCK_UN_USE && next_bptr->in_use == BLOCK_UN_USE){
            bptr->length += next_bptr->length;
            g_stat.free_blocks -= 1;
        }
        else
            bptr = next_bptr;
        next_bptr = (block_header_t*)((addrs_t)next_bptr + GET_SIZE(next_bptr->length));
    }
}

void Init(size_t size){
    s_baseptr = (addrs_t)malloc(size);
    memset(s_baseptr, 0x0, size);
    malloc_chunck_header_t* chunckptr = (malloc_chunck_header_t*)s_baseptr;
    chunckptr->length = size;
    chunckptr->free_length = size - CHUNCK_HEADER_SIZE;

    /* 将整个M1空间排除头部信息后剩余的部分设置成一整个大的空闲块 */
    block_header_t* block_start = (block_header_t*)(s_baseptr + CHUNCK_HEADER_SIZE);
    block_start->length = chunckptr->free_length;
    block_start->in_use = BLOCK_UN_USE;
    memset(&g_stat, 0x0, sizeof(state_t));
    g_stat.free_blocks = 1;
    g_stat.padded_alloc_bytes = CHUNCK_HEADER_SIZE;
    g_stat.raw_free_bytes = chunckptr->free_length;
    g_stat.padded_free_bytes = ALIGNED_8(chunckptr->free_length);
}

addrs_t Malloc (size_t size){
    //printf("====Malloc size %lu\n", size);
    unsigned long start, finish;
    RDTSC(start);
    g_stat.malloc_requests += 1;
    if(size == 0) {
        RDTSC(finish);
        g_stat.failure_requests += 1;
        g_stat.total_malloc_clock_cycles += finish - start;
        return NULL;
    }
    /* 实际需要的内存空间，是8字节对齐后再加上块头信息 */
    size_t alloc_size = ALIGNED_8(size);
    uint8_t padding = ALIGNED_8(size) - size;
    alloc_size += sizeof(block_header_t);
    addrs_t alloc_ptr = NULL;
    malloc_chunck_header_t* chunckptr = (malloc_chunck_header_t*)s_baseptr;
    addrs_t ptr = (addrs_t)(s_baseptr + CHUNCK_HEADER_SIZE);  /* 寻找空闲空间遍历用的 */
    addrs_t maxptr = s_baseptr + chunckptr->length; /* 整个M1空闲空间的尾边界 */
    while(ptr < maxptr){
        block_header_t* bptr = (block_header_t*)ptr;
        //printf("find block %p, len %u, use %u\n", bptr, bptr->length, bptr->in_use);
        uint32_t block_size = GET_SIZE(bptr->length);
        if(bptr->in_use == BLOCK_UN_USE && block_size >= alloc_size){
            g_stat.alloc_blocks += 1;
            g_stat.raw_alloc_bytes += size;
            g_stat.padded_alloc_bytes += alloc_size;
            g_stat.raw_free_bytes -= size;
            g_stat.padded_free_bytes -= alloc_size;
            if(block_size == alloc_size){ /* find a suitable block, then set in use and return */
                bptr->in_use = BLOCK_IN_USE;
                bptr->length = SET_SIZE(alloc_size, padding);
                g_stat.free_blocks -= 1;
            }
            else{ /* find a bigger block, split into a suitable block and a free block, then return the suitable */
                /* 寻找到一个比实际需要的大的空闲块，那么将这个块切分成两块，第一块为需要大小的块，第二块为剩余部分的空闲块 */
                size_t ori_size = bptr->length;
                bptr->in_use = BLOCK_IN_USE;
                bptr->length = SET_SIZE(alloc_size, padding);
                block_header_t* next_bptr = (block_header_t*)(ptr + alloc_size);
                next_bptr->length = ori_size - alloc_size;
                next_bptr->in_use = BLOCK_UN_USE;
                //assert(next_bptr->length != 0);
            }
            alloc_ptr = (addrs_t)(bptr + 1); /* 这里等价于 (addrs_t)bptr + sizeof(block_header_t) */
            break;
        }
        else{
            ptr = ptr + block_size;
        }
    }
    if(!alloc_ptr){
        g_stat.failure_requests += 1;
    }
    RDTSC(finish);
    g_stat.total_malloc_clock_cycles += finish - start;
    return alloc_ptr;
}

void Free (addrs_t addr){
    //printf("====Free addr %p\n", addr);
    unsigned long start, finish;
    RDTSC(start);
    g_stat.free_requests += 1;
    if(addr == NULL){
        RDTSC(finish);
        g_stat.failure_requests += 1;
        g_stat.total_free_clock_cycles += finish - start;
        return;
    }

    block_header_t* bptr = ((block_header_t*)addr) - 1; /* 这里等价于 addr - sizeof(block_header_t) */
    if(bptr->in_use != BLOCK_IN_USE){
        errno = EOVERFLOW;
        RDTSC(finish);
        g_stat.failure_requests += 1;
        g_stat.total_free_clock_cycles += finish - start;
        return;
    }

    if(bptr->in_use == BLOCK_IN_USE){
        bptr->in_use = BLOCK_UN_USE;
        g_stat.alloc_blocks -= 1;
        g_stat.raw_alloc_bytes -= GET_SIZE(bptr->length) - sizeof(block_header_t) - GET_PADDING(bptr->length);
        g_stat.padded_alloc_bytes -= GET_SIZE(bptr->length);
        g_stat.raw_free_bytes += GET_SIZE(bptr->length) - sizeof(block_header_t) - GET_PADDING(bptr->length);
        g_stat.padded_free_bytes += GET_SIZE(bptr->length);
        bptr->length = GET_SIZE(bptr->length);
        g_stat.free_blocks += 1;
        Coalesce();
    }
    malloc_chunck_header_t* chunckptr = (malloc_chunck_header_t*)s_baseptr;
    addrs_t ptr = (addrs_t)(s_baseptr + CHUNCK_HEADER_SIZE);  /* 寻找空闲空间遍历用的 */
    addrs_t maxptr = s_baseptr + chunckptr->length; /* 整个M1空闲空间的尾边界 */
    RDTSC(finish);
    g_stat.total_free_clock_cycles += finish - start;
}

addrs_t Put (any_t data, size_t size){
    addrs_t ptr = Malloc(size);
    if(ptr == NULL)
        return NULL;
    memcpy(ptr, data, size);
    return ptr;
}

void Get (any_t return_data, addrs_t addr, size_t size){
    if(addr == NULL) return;
    block_header_t* bptr = ((block_header_t*)addr) - 1;
    memcpy(return_data, addr, size);
    Free(addr);
}
