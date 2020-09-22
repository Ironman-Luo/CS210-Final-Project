// Your code for part 2 goes here.
// Do not include your main function
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "heap_checker.c"
#include <assert.h>

#define ALIGNED_8(size)       ((size + 7) >> 3 << 3)
#define GET_SIZE(s)           ((s) & ~0x7)
#define GET_PADDING(s)        ((s) & 0x7)
#define SET_SIZE(s, padding)  ((s) | (padding))

typedef char* addrs_t;
typedef void* any_t;

/* the head structure for every alloced memory */
typedef struct block_header_s {
    uint32_t length;   /* size of block & with padding*/
    uint32_t rt_index; /* index in RT */
} block_header_t;

/* the vmalloc header for M2 */
typedef struct vmalloc_chunck_header_s {
    uint32_t length;          /* M2 size */
    uint32_t free_length;     /* M2 free memory */
} vmalloc_chunck_header_t;

#define CHUNCK_HEADER_SIZE  ALIGNED_8(sizeof(vmalloc_chunck_header_t))

static addrs_t s_baseptr = NULL; /* points to base memory area, M2 */
static addrs_t s_rt_table[ 1 << 20 ]; 

void VInit(size_t size){
    s_baseptr = (addrs_t)malloc(size);
    memset(s_baseptr, 0x0, size);
    vmalloc_chunck_header_t* chunckptr = (vmalloc_chunck_header_t*)s_baseptr;
    chunckptr->length = size;
    chunckptr->free_length = size - CHUNCK_HEADER_SIZE;
    memset(s_rt_table, 0x0, sizeof(s_rt_table));

    memset(&g_stat, 0x0, sizeof(state_t));
    g_stat.free_blocks = 1;
    g_stat.padded_alloc_bytes = CHUNCK_HEADER_SIZE;
    g_stat.raw_free_bytes = chunckptr->free_length;
    g_stat.padded_free_bytes = ALIGNED_8(chunckptr->free_length);

}

addrs_t* VMalloc (size_t size){


    unsigned long start, finish;
    RDTSC(start);
    g_stat.malloc_requests += 1;


    if(size == 0){
        RDTSC(finish);
        g_stat.failure_requests += 1;
        g_stat.total_malloc_clock_cycles += finish - start;
        return NULL;
    }

    size_t alloc_size = ALIGNED_8(size) + sizeof(block_header_t);
    vmalloc_chunck_header_t* chunckptr = (vmalloc_chunck_header_t*)s_baseptr;
    if(chunckptr->free_length < alloc_size)
    {
        errno = ENOMEM;
        RDTSC(finish);
        g_stat.failure_requests += 1;
        g_stat.total_malloc_clock_cycles += finish - start;
        return NULL;
    }

    /* find a free index in rt */
    uint8_t padding = ALIGNED_8(size) - size;
    block_header_t new_header = {SET_SIZE(alloc_size, padding), -1};
    size_t i;
    for(i = 0; i < sizeof(s_rt_table); ++i){
        if(s_rt_table[i] == NULL){
            new_header.rt_index = i;
            break;
        }
    }

    /* rt is full */
    if(new_header.rt_index == -1){
        errno = ENOMEM;
        RDTSC(finish);
        g_stat.failure_requests += 1;
        g_stat.total_malloc_clock_cycles += finish - start;
        return NULL;
    }

    addrs_t ptr = s_baseptr + chunckptr->length - chunckptr->free_length; /* base address of free area */
    block_header_t* bptr = (block_header_t*)ptr;
    *bptr = new_header;
    s_rt_table[new_header.rt_index] = (addrs_t)(bptr + 1);
    chunckptr->free_length -= alloc_size;

    g_stat.alloc_blocks += 1;
    g_stat.raw_alloc_bytes += size;
    g_stat.padded_alloc_bytes += alloc_size;
    g_stat.raw_free_bytes -= size;
    g_stat.padded_free_bytes -= alloc_size;
    if(chunckptr->free_length == 0){
        g_stat.free_blocks = 0;
    }
    RDTSC(finish);
    g_stat.total_malloc_clock_cycles += finish - start;

    return &s_rt_table[new_header.rt_index];
}

void VFree (addrs_t* addr){
    unsigned long start, finish;
    RDTSC(start);
    g_stat.free_requests += 1;
    if(addr == NULL || *addr == NULL){
        RDTSC(finish);
        g_stat.failure_requests += 1;
        g_stat.total_free_clock_cycles += finish - start;

        return;
    }

    block_header_t* bptr = (block_header_t*)(*addr) - 1;
    s_rt_table[bptr->rt_index] = NULL;

    g_stat.alloc_blocks -= 1;
    g_stat.free_blocks = 1;
    g_stat.raw_alloc_bytes -= GET_SIZE(bptr->length) - sizeof(block_header_t) - GET_PADDING(bptr->length) ;
    g_stat.padded_alloc_bytes -= GET_SIZE(bptr->length);
    g_stat.raw_free_bytes += GET_SIZE(bptr->length) - sizeof(block_header_t) - GET_PADDING(bptr->length);
    g_stat.padded_free_bytes += GET_SIZE(bptr->length);

    vmalloc_chunck_header_t* chunckptr = (vmalloc_chunck_header_t*)s_baseptr;
    addrs_t block_end_ptr = s_baseptr + chunckptr->length - chunckptr->free_length;
    chunckptr->free_length += GET_SIZE(bptr->length);

    addrs_t ptr = (addrs_t)bptr;
    block_header_t* next_bptr = (block_header_t*)(ptr + GET_SIZE(bptr->length));
    //compaction
    while((addrs_t)next_bptr < block_end_ptr){
        uint32_t size = GET_SIZE(next_bptr->length);
        block_header_t* next_next_bptr = (block_header_t*)((addrs_t)next_bptr + size);
        s_rt_table[next_bptr->rt_index] = ptr + sizeof(block_header_t);
        assert(next_bptr->length != 0);
        memmove(ptr, next_bptr, size);
        ptr = ptr + size;
        next_bptr = next_next_bptr;
    }

    RDTSC(finish);
    g_stat.total_free_clock_cycles += finish - start;
}

addrs_t* VPut (any_t data, size_t size){
    addrs_t* ptr = VMalloc(size);
    if(ptr == NULL)
        return NULL;
    memcpy(*ptr, data, size);
    return ptr;
}

void VGet (any_t return_data, addrs_t* addr, size_t size){
    memcpy(return_data, *addr, size);
    VFree(addr);
}
