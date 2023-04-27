#include "benchmark_internal.h"
#include "memcached.h"
#include <stdio.h>

void internal_benchmark_config(struct settings* settings)
{
    // we use our own threads
    settings->num_threads = 1;
    settings->maxbytes = BENCHMARK_SLAB_PREALLOC_SIZE;

    //
    settings->use_cas = true;
    settings->lru_maintainer_thread = false;
    settings->hashpower_init = HASHPOWER_DEFAULT;

    settings->slab_reassign = false;
    settings->idle_timeout = false;
    settings->item_size_max = 1024 * 1024;
    settings->slab_page_size = USED_SLAB_PAGE_SIZE;
}

struct element {
    uint64_t key;
    char data[BENCHMARK_ELEMENT_SIZE - sizeof(uint64_t)];
};

void internal_benchmark_run(struct settings* settings)
{
    fprintf(stderr, "=====================================\n");
    fprintf(stderr, "INTERNAL BENCHMARK STARTING\n");
    fprintf(stderr, "=====================================\n");

#define KEY_STRING "my-key-0x%016lx"
#define KEY_LENGTH 26

    size_t num_threads = omp_get_num_threads();
    conn* my_conns = calloc(num_threads, sizeof(*my_conn));

    for (size_t i = 0; i < num_threads; i++) {
        conns[i] = conn_new(i, 0, 0, 0, 0, NULL);
    }

    struct timeval start, end;

    fprintf(stderr, "element size: %zu bytes\n", sizeof(struct element));
    fprintf(stderr, "number of keys: %zu\n", BENCHMARK_MAX_KEYS);
    fprintf(stderr, "allocating %zu bytes (%zu GB) for the element array\n",
        BENCHMARK_MAX_KEYS * sizeof(struct element),
        (BENCHMARK_MAX_KEYS * sizeof(struct element)) >> 30);

    struct element* elms = calloc(BENCHMARK_MAX_KEYS, sizeof(struct element));

    size_t counter = 0;

    fprintf(stderr, "Populate the database\n");

    fprintf(stderr, "Populating %zu / %zu key-value pairs:\n", counter, BENCHMARK_MAX_KEYS);
/* prepopulate the thing */
#pragma omp parallel reduction(+ \
                               : counter)
    {
        /* pin threads */
        int thread_id = omp_get_thread_num();

        cpu_set_t my_set;
        CPU_ZERO(&my_set);
        CPU_SET(thread_id, &my_set);
        sched_setaffinity(0, sizeof(cpu_set_t), &my_set);

        conn* myconn = my_conns[thread_id];

#pragma omp for
        for (size_t i = 0; i < BENCHMARK_MAX_KEYS; i++) {
            // printf("SET[%zu]: key=%s, len=%zu\n", i, mkey, strlen(mkey));
            item* it = item_alloc((char*)&i, sizeof(i), 0, 0, BENCHMARK_ITEM_VALUE_SIZE);
            if (!it) {
                // printf("Item was NULL! %zu\n", i);
                continue;
            }

            memset(ITEM_data(it), 0, BENCHMARK_ITEM_VALUE_SIZE);

            elms[i].key = i;
            uintptr_t ptr = (uintptr_t)&elms[i];
            memcpy(ITEM_data(it), &ptr, sizeof(uintptr_t));
            store_item(it, NREAD_SET, myconn);
            counter++;
        }
    }
    fprintf(stderr, "Populated %zu / %zu key-value pairs:\n", counter, BENCHMARK_MAX_KEYS);
    fprintf(stderr, "=====================================\n");

    gettimeofday(&start, NULL);

    size_t num_threads = omp_get_num_threads();
    size_t num_queries = 0;
// for (size_t i = 0; i < 2; i++) {
#pragma omp parallel reduction(+ \
                               : num_queries)
    {

        /* pin threads */
        int thread_id = omp_get_thread_num();
        size_t unknown = 0;
        size_t found = 0;

        conn* myconn = my_conns[thread_id];

        size_t g_seed = (214013UL * thread_id + 2531011UL);

#pragma omp for
        for (size_t i = 0; i < (num_threads * QUERIES_PER_THREAD); i++) {
            size_t idx = (i + (g_seed >> 16)) % (BENCHMARK_MAX_KEYS);
            g_seed = (214013UL * g_seed + 2531011UL);

            // char mkey[64];
            // snprintf(mkey, 64, KEY_STRING,  idx);
            //  printf("GET: key=%s, len=%zu\n", mkey, strlen(mkey));
            item* it = item_get((char*)&idx, sizeof(idx), myconn, DONT_UPDATE);
            if (!it) {
                unknown++;
            } else {
                struct element* e;
                memcpy(&e, ITEM_data(it), sizeof(void*));
                if (e->key == idx) {
                    found++;
                } else {
                    unknown++;
                }
            }

            // printf("got item: %p\n", (void *)it);
            // printf("got value[%zu]: %s\n",i, (char *)ITEM_data(it));
        }

        num_queries += unknown + found;
        // printf("thread:%i of %i found %zu, missed %zu\n", thread_id,
        //       omp_get_num_threads(), found, unknown);
    }
    gettimeofday(&end, NULL);



    double sec = (end.tv_sec - start.tv_sec) + (end.tv_usec - end.tv_usec) * 1000000;

    fprintf(stderr, "benchmark took %lu seconds\n", end.tv_sec - start.tv_sec);
    fprintf(stderr, "benchmark took %lu queries / second\n", num_queries / (end.tv_sec - start.tv_sec));
    fprintf(stderr, "benchmark executed %lu / %lu queries\n", num_queries, (num_threads * QUERIES_PER_THREAD));

    exit(0);

    //}
    /*
    printf("do the benchmark...\n");
    char *mkey = "hello";
    size_t nmkey = 5;
    item *it = item_alloc(mkey, nmkey, 0, 0, 7);
    printf("copy payload...\n");
    memcpy(ITEM_data(it), "foobar", 6);

    conns[0] = conn_new(0, 0, 0, 0, 0, NULL);

    store_item(it, NREAD_SET, conns[0]);

    printf("get the key... %p\n",  (void *)conns[0]);
    //ITEM_set_cas(it, req_cas_id);

    mkey = "hello";
    it = item_get(mkey, 5, conns[0], DO_UPDATE);
    printf("got item: %p\n", (void *)it);
    printf("got value: %s\n", (char *)ITEM_data(it));

    //it = item_touch(key, nkey, realtime(exptime_int), conn[0]);
    //void  item_remove(item *it);
    //int   item_replace(item *it, item *new_it, const uint64_t hv);
    //it = item_touch(key, nkey, realtime(exptime), conn[0])
  */

    fprintf(stderr, "terminating.\n");
    fprintf(stderr, "===============================================================================\n");
    fprintf(stderr, "===============================================================================\n");

    exit(0);
}