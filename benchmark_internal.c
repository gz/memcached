#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event.h>
#include <pthread.h>



#include "benchmark_internal.h"
#include "memcached.h"

#define ITEM_SIZE (sizeof(item) + sizeof(size_t) + BENCHMARK_ITEM_VALUE_SIZE + BENCHMARK_ITEM_KEY_SIZE)

// ./configure --disable-extstore --enable-static

void internal_benchmark_config(struct settings* settings)
{
    fprintf(stderr, "=====================================\n");
    fprintf(stderr, "INTERNAL BENCHMARK CONFIGURE\n");
    fprintf(stderr, "=====================================\n");

    // calculate the number of items
    size_t num_items = settings->x_benchmark_mem / ITEM_SIZE;
    if (num_items < 100) {
        fprintf(stderr, "WARNING: too little elements\n");
        num_items = 100;
    }

    // calculate the maximum number of bytes
    settings->maxbytes = settings->x_benchmark_mem;

    settings->use_cas = true;
    settings->lru_maintainer_thread = false;

    size_t hash_power = HASHPOWER_DEFAULT;
    while((num_items >> (hash_power - 1)) != 0 && hash_power < HASHPOWER_MAX) {
        hash_power++;
    }

    settings->hashpower_init = hash_power;

    settings->slab_reassign = false;
    settings->idle_timeout = false;
    settings->item_size_max = 1024;
    settings->slab_page_size = BENCHMARK_USED_SLAB_PAGE_SIZE;
    fprintf(stderr,"------------------------------------------\n");
    fprintf(stderr, " - x_benchmark_mem = %zu MB\n", settings->x_benchmark_mem >> 20);
    fprintf(stderr, " - x_benchmark_queries = %zu\n", settings->x_benchmark_queries);
    fprintf(stderr, " - num_threads = %u\n", omp_get_num_procs());
    fprintf(stderr, " - maxbytes = %zu MB\n", settings->maxbytes >> 20);
    fprintf(stderr, " - slab_page_size = %u kB\n", settings->slab_page_size >> 10);
    fprintf(stderr, " - hashpower_init = %u\n", settings->hashpower_init);
    fprintf(stderr,"------------------------------------------\n");
}



void internal_benchmark_run(struct settings* settings, struct event_base *main_base)
{
    if (settings->x_benchmark_no_run) {
        fprintf(stderr, "=====================================\n");
        fprintf(stderr, "INTERNAL BENCHMARK SKIPPING\n");
        fprintf(stderr, "=====================================\n");
        return;
    }

    printf("------------------------------------------");

    fprintf(stderr, "=====================================\n");
    fprintf(stderr, "INTERNAL BENCHMARK STARTING\n");
    fprintf(stderr, "=====================================\n");


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Establish the connections
    ////////////////////////////////////////////////////////////////////////////////////////////////
    size_t num_threads = omp_get_num_procs();
    conn** my_conns = calloc(num_threads, sizeof(*my_conns));
    for (size_t i = 0; i < num_threads; i++) {
        my_conns[i] = conn_new(i, conn_listening, 0, 0, local_transport, main_base, NULL, 0, ascii_prot);
        my_conns[i]->thread = malloc(sizeof(LIBEVENT_THREAD));
    }

    // set the number of threads to the amount of processors we have
    omp_set_num_threads(num_threads);


    // calculate the amount of items to fit within memory.
    size_t num_items = settings->x_benchmark_mem / (ITEM_SIZE);
    if (num_items < 100) {
        fprintf(stderr, "WARNING: too little elements\n");
        num_items = 100;
    }

    struct timeval start, end, elapsed;
    uint64_t elapsed_us;

    fprintf(stderr, "number of threads: %zu\n", num_threads);
    fprintf(stderr, "item size: %zu bytes\n", ITEM_SIZE);
    fprintf(stderr, "number of keys: %zu\n", num_items);

    size_t counter = 0;

    // fprintf(stderr, "Prefilling slabs\n");
    // gettimeofday(&start, NULL);
    // slabs_prefill_global();

    // gettimeofday(&end, NULL);
    // struct timeval elapsed;
    // timersub(&end, &start, &elapsed);
    // uint64_t elapsed_us = (elapsed.tv_sec * 1000000) + elapsed.tv_usec;
    // fprintf(stderr, "Prefilling slabs took %lu ms\n", elapsed_us / 1000);

    fprintf(stderr, "Populating %zu / %zu key-value pairs.\n", counter, num_items);

    gettimeofday(&start, NULL);

/* prepopulate the thing */
#pragma omp parallel reduction(+ \
                               : counter)
    {
        /* pin threads */
        int thread_id = omp_get_thread_num();

#ifdef __linux__
        // cpu_set_t my_set;
        // CPU_ZERO(&my_set);
        // CPU_SET(thread_id, &my_set);
        // pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &my_set);
#else
        /* BSD/RUMP kernel doesn't do this! */
        // cpuset_t *my_set = cpuset_create();
        // cpuset_zero(my_set);
        // cpuset_set(thread_id, my_set);
        // pthread_setaffinity_np(pthread_self(), cpuset_size(my_set), my_set);
#endif
        conn* myconn = my_conns[thread_id];

#pragma omp for schedule(static, 1024)
        for (size_t i = 0; i < num_items; i++) {

            char key[BENCHMARK_ITEM_KEY_SIZE + 1];
            snprintf(key, BENCHMARK_ITEM_KEY_SIZE + 1, "%08x", (unsigned int)i);

            char value[BENCHMARK_ITEM_VALUE_SIZE + 1];
            snprintf(value, BENCHMARK_ITEM_VALUE_SIZE, "value-%016lx", i);

            item* it = item_alloc(key, BENCHMARK_ITEM_KEY_SIZE, 0, 0, BENCHMARK_ITEM_VALUE_SIZE);
            if (!it) {
                printf("Item was NULL! %zu\n", i);
                continue;
            }

            memcpy(ITEM_data(it), value, BENCHMARK_ITEM_VALUE_SIZE);

            uint64_t cas = 0;

            switch (store_item(it, NREAD_SET, myconn->thread, NULL, &cas, CAS_NO_STALE)) {
                case STORED:
                    myconn->cas = cas;
                    break;
                case EXISTS:
                    break;
                case NOT_FOUND:
                    break;
                case NOT_STORED:
                    break;
                default:
                    break;
            }

            counter++;
            if (counter % 25000000 == 0) {
                fprintf(stderr, "populate: thread %d added %zu elements. \n", thread_id, counter);
            }
        }
        fprintf(stderr, "populate: thread %d done. \n", thread_id);
    }

    gettimeofday(&end, NULL);
    timersub(&end, &start, &elapsed);
    elapsed_us = (elapsed.tv_sec * 1000000) + elapsed.tv_usec;

    fprintf(stderr, "Populated %zu / %zu key-value pairs in %lu ms:\n", counter, num_items, elapsed_us / 1000);
    fprintf(stderr, "=====================================\n");

    fprintf(stderr, "Executing %zu queries with %zu threads.\n", num_threads * settings->x_benchmark_queries, num_threads);

    gettimeofday(&start, NULL);

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

#pragma omp for schedule(static)
        for (size_t i = 0; i < (num_threads * settings->x_benchmark_queries); i++) {
            size_t idx = (i + (g_seed >> 16)) % (num_items);
            g_seed = (214013UL * g_seed + 2531011UL);

            char key[BENCHMARK_ITEM_KEY_SIZE + 1];
            snprintf(key, BENCHMARK_ITEM_KEY_SIZE + 1, "%08x", (unsigned int)idx);

            // char mkey[64];
            // snprintf(mkey, 64, KEY_STRING,  idx);
            //  printf("GET: key=%s, len=%zu\n", mkey, strlen(mkey));
            item* it = item_get(key, BENCHMARK_ITEM_KEY_SIZE, myconn->thread, DONT_UPDATE);
            if (!it) {
                unknown++;
            } else {
                found++;
            }

            // printf("got item: %p\n", (void *)it);
            // printf("got value[%zu]: %s\n",i, (char *)ITEM_data(it));
        }

        num_queries += unknown + found;
        // printf("thread:%i of %i found %zu, missed %zu\n", thread_id,
        //       omp_get_num_threads(), found, unknown);
    }
    gettimeofday(&end, NULL);

    timersub(&end, &start, &elapsed);
    elapsed_us = (elapsed.tv_sec * 1000000) + elapsed.tv_usec;

    fprintf(stderr, "benchmark took %lu ms\n", elapsed_us / 1000);
    fprintf(stderr, "benchmark took %lu queries / second\n", (num_queries / elapsed_us) * 1000000);
    fprintf(stderr, "benchmark executed %lu / %lu queries\n", num_queries, (num_threads * settings->x_benchmark_queries));

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