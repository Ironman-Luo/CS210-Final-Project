
#ifdef VHEAP
  #define MALLOC_STR            "VMalloc"
  #define FREE_STR              "VFree"
  #define PART                  2
#else
  #define MALLOC_STR            "Malloc"
  #define FREE_STR              "Free"
  #define PART                  1
#endif

typedef struct state_s {
    size_t  alloc_blocks;
    size_t  free_blocks;
    size_t  raw_alloc_bytes;
    size_t  padded_alloc_bytes;
    size_t  raw_free_bytes;
    size_t  padded_free_bytes;
    size_t  malloc_requests;
    size_t  free_requests;
    size_t  failure_requests;
    size_t  total_malloc_clock_cycles;
    size_t  total_free_clock_cycles;
} state_t;

state_t g_stat;

void print_state(){
  printf(
    "<<Part %d for Region M%d>>\n"
    "Number of allocated blocks : [%s%ld%s]\n"
    "Number of free blocks  : [%s%ld%s]\n"
    "Raw total number of bytes allocated : [%s%ld%s]\n"
    "Padded total number of bytes allocated : [%s%ld%s]\n"
    "Raw total number of bytes free : [%s%ld%s]\n"
    "Aligned total number of bytes free : [%s%ld%s]\n"
    "Total number of %s requests : [%s%ld%s]\n"
    "Total number of %s requests: [%s%ld%s]\n"
    "Total number of request failures: [%s%ld%s]\n"
    "Average clock cycles for a %s request: [%s%ld%s]\n"
    "Average clock cycles for a %s request: [%s%ld%s]\n"
    "Total clock cycles for all requests: [%s%ld%s]\n",
    PART, PART, 
    KBLU, g_stat.alloc_blocks, KRESET,
    KBLU, g_stat.free_blocks, KRESET,
    KBLU, g_stat.raw_alloc_bytes, KRESET,
    KBLU, g_stat.padded_alloc_bytes, KRESET,
    KBLU, g_stat.raw_free_bytes, KRESET,
    KBLU, g_stat.padded_free_bytes >> 3 << 3, KRESET,
    MALLOC_STR, KBLU, g_stat.malloc_requests, KRESET,
    FREE_STR, KBLU, g_stat.free_requests, KRESET,
    KBLU, g_stat.failure_requests, KRESET,
    MALLOC_STR, KBLU, g_stat.malloc_requests ? g_stat.total_malloc_clock_cycles / g_stat.malloc_requests : 0, KRESET,
    FREE_STR, KBLU, g_stat.free_requests ? g_stat.total_free_clock_cycles / g_stat.free_requests : 0, KRESET,
    KBLU, g_stat.total_malloc_clock_cycles + g_stat.total_free_clock_cycles, KRESET
  );
}



