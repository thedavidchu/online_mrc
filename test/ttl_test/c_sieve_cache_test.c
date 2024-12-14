/** @note   This file exists for the sole reason that debugging C++
 *          errors sucks, while debugging C compile-time errors is less
 *          bad. Hence, I'm testing libCacheSim in C. */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

// NOTE It shouldn't be this painful to include a library file, but it is...
#include "libCacheSim/include/libCacheSim.h"
#include "libCacheSim/include/libCacheSim/cacheObj.h"

// HACK This is a complete hack in order to access an invisible data
//      structure. Originally, this data structure is private, but by
//      copying it exactly here, I am able to make it "public".
//      Admittedly, this is a horrendous and brittle idea.
typedef struct {
    cache_obj_t *q_head;
    cache_obj_t *q_tail;

    cache_obj_t *pointer;
} Sieve_params_t;

static void
print_sieve(cache_t *cache)
{
    assert(cache);
    Sieve_params_t *p = cache->eviction_params;
    printf("Cache (size=%zu): ", cache->n_obj);
    for (cache_obj_t *o = p->q_head; o != NULL; o = o->queue.next) {
        // Indicate where the hand is pointing.
        if (p->pointer == o) {
            printf("*");
        }
        if (o->sieve.freq) {
            printf("v");
        }
        printf("%zu ", o->obj_id);
    }
    printf("\n");
}

int
main(void)
{
    size_t n_miss = 0, n_req = 0;
    common_cache_params_t cc_params = default_common_cache_params();
    cc_params.cache_size = 7;
    cache_t *cache = Sieve_init(cc_params, NULL);
    if (cache == NULL) {
        return EXIT_FAILURE;
    }

    request_t *req = new_request();
    for (size_t i = 0; i < 100; ++i) {
        req->obj_id = i;
        if (!cache->get(cache, req)) {
            ++n_miss;
        }
        ++n_req;
        print_sieve(cache);
    }
    free_request(req);

    printf("Miss Ratio: %zu/%zu = %f\n", n_miss, n_req, (double)n_miss / n_req);

    cache->cache_free(cache);
    return 0;
}
