#ifdef BENCHMARK_INTERNAL_H_

#ifdef EXTSTORE
#error extstore is not supported
#endif

#ifdef TLS
#error disable enabled
#endif

#ifdef _OPENMP
#include <omp.h>
#endif



////////////////////////////////////////////////////////////////////////////////////////////////////
// Benchmark Configuration Settings
////////////////////////////////////////////////////////////////////////////////////////////////////
#define LOW_MEMORY 1
// #define MED_MEMORY 2
// #define LARGE_MEMORY 3


#ifdef LOW_MEMORY
////////////////////////////////////////////////////////////////////////////////////////////////////
// SMALL MEMORY (8G)
////////////////////////////////////////////////////////////////////////////////////////////////////

// size of the hash table
#define HASHPOWER_DEFAULT 16    // the required configuration settings
#define HASHPOWER_MAX 32        // the required configuration settings

#define BENCHMARK_USED_SLAB_PAGE_SIZE (1UL << 21)
#define BENCHMARK_SLAB_PREALLOC_SIZE (8UL << 30)

/// THIS IS THE AMOUNT FOR THE ELEMENT ARRAY
#define BENCHMARK_ELEMENT_SIZE (2 * sizeof(void *))
/// THIS IS THE AMOUNT FOR THE HASH TABLE ITEM
#define BENCHMARK_ITEM_VALUE_SIZE (128 - 65)

#elif defined(MED_MEMORY)
////////////////////////////////////////////////////////////////////////////////////////////////////
// MEd MEMORY (150G)
////////////////////////////////////////////////////////////////////////////////////////////////////

#define HASHPOWER_DEFAULT 30
#define HASHPOWER_MAX 32

#define BENCHMARK_USED_SLAB_PAGE_SIZE (1UL << 30)
#define BENCHMARK_SLAB_PREALLOC_SIZE (64UL << 30)
/// THIS IS THE AMOUNT FOR THE ELEMENT ARRAY
#define BENCHMARK_ELEMENT_SIZE 64
/// THIS IS THE AMOUNT FOR THE HASH TABLE ITEM
#define BENCHMARK_ITEM_VALUE_SIZE 32

#elif defined(LARGE_MEMORY)

////////////////////////////////////////////////////////////////////////////////////////////////////
// LARGE MEMORY (1.2 T)
////////////////////////////////////////////////////////////////////////////////////////////////////

// aim for 1.2T
#define HASHPOWER_DEFAULT 35
#define HASHPOWER_MAX 35

#define BENCHMARK_USED_SLAB_PAGE_SIZE (1UL << 32)
#define BENCHMARK_SLAB_PREALLOC_SIZE (512UL << 30)
/// THIS IS THE AMOUNT FOR THE ELEMENT ARRAY
#define BENCHMARK_ELEMENT_SIZE 128
/// THIS IS THE AMOUNT FOR THE HASH TABLE ITEM
#define BENCHMARK_ITEM_VALUE_SIZE 64

#else
////////////////////////////////////////////////////////////////////////////////////////////////////
// ERROR CASE
////////////////////////////////////////////////////////////////////////////////////////////////////
#error unknown memory configuration
#endif


// the number of keys
#define BENCHMARK_MAX_KEYS (1UL << (HASHPOWER_MAX - 3))
#define BENCHMARK_QUERIES_PER_THREAD (1024UL * 1000UL * 1000UL)

void internal_benchmark_run(struct settings *settings);
void internal_benchmark_config(struct settings *settings);