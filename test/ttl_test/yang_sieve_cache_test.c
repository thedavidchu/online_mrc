/** @note   This file exists for the sole reason that debugging C++
 *          errors sucks, while debugging C compile-time errors is less
 *          bad. Hence, I'm testing libCacheSim in C. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "test/mytester.h"

// NOTE It shouldn't be this painful to include a library file, but it is...
#include "libCacheSim/include/libCacheSim.h"

// NOTE This is an anonymized version of the first few accesses in the
//      MSR src2 trace.
//      i.e. the keys are changed to their unique position in the trace.
static uint64_t const msr_src2_trace[] =
    {1, 2, 3, 4, 5, 5, 3, 4, 5, 6, 7, 8, 9, 10, 11};

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

static bool
test_sieve_capacity_2_on_msr_src2(void)
{
    // NOTE This hit pattern is specific to Sieve with a capacity of 2.
    bool hit_pattern[] = {false,
                          false,
                          false,
                          false,
                          false,
                          true,
                          false,
                          false,
                          true,
                          false,
                          false,
                          false,
                          false,
                          false,
                          false};
    size_t n_miss = 0, n_req = 0;
    common_cache_params_t cc_params = default_common_cache_params();
    cc_params.cache_size = 2;
    cache_t *cache = Sieve_init(cc_params, NULL);
    if (cache == NULL) {
        return false;
    }

    request_t *req = new_request();
    for (size_t i = 0; i < ARRAY_LENGTH(msr_src2_trace); ++i) {
        req->obj_id = msr_src2_trace[i];
        bool is_hit = cache->get(cache, req);
        if (!is_hit) {
            ++n_miss;
        }
        assert(is_hit == hit_pattern[i]);
        ++n_req;
        print_sieve(cache);
    }
    free_request(req);

    printf("Miss Ratio: %zu/%zu = %f\n", n_miss, n_req, (double)n_miss / n_req);

    cache->cache_free(cache);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_sieve_capacity_2_on_msr_src2());
    return 0;
}
