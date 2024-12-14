#include <cstddef>
#include <cstdint>
#include <vector>

#include "cache/yang_sieve_cache.hpp"
// NOTE It shouldn't be this painful to include a library file, but it is...
#include "libCacheSim/include/libCacheSim.h"

static common_cache_params_t
set_cc_params(std::size_t const capacity)
{
    common_cache_params_t cc_param = default_common_cache_params();
    cc_param.cache_size = capacity;
    return cc_param;
}

YangSieveCache::YangSieveCache(std::size_t const capacity)
    : capacity_(capacity),
      cache_(Sieve_init(set_cc_params(capacity), NULL)),
      req_(new_request())
{
}

YangSieveCache::~YangSieveCache()
{
    cache_t *c = (cache_t *)cache_;
    c->cache_free(c);
    free_request((request_t *)req_);
}

std::size_t
YangSieveCache::size() const
{
    cache_t *c = (cache_t *)cache_;
    return c->n_obj;
}

bool
YangSieveCache::contains(std::uint64_t const key) const
{
    cache_t *c = (cache_t *)cache_;
    request_t *r = (request_t *)req_;
    return c->find(c, r, false) != NULL;
}

int
YangSieveCache::access_item(std::uint64_t const key)
{
    cache_t *c = (cache_t *)cache_;
    request_t *r = (request_t *)req_;
    r->obj_id = key;
    bool is_hit = c->get(c, r);
    if (is_hit) {
        statistics_.hit();
    } else {
        statistics_.miss();
    }
    return 0;
}

// HACK This is a complete hack in order to access an invisible data
//      structure. Originally, this data structure is private, but by
//      copying it exactly here, I am able to make it "public".
//      Admittedly, this is a horrendous and brittle idea.
typedef struct {
    cache_obj_t *q_head;
    cache_obj_t *q_tail;

    cache_obj_t *pointer;
} Sieve_params_t;

std::vector<std::uint64_t>
YangSieveCache::get_keys() const
{
    cache_t *c = (cache_t *)cache_;
    Sieve_params_t *p = (Sieve_params_t *)c->eviction_params;

    std::vector<std::uint64_t> keys;
    for (cache_obj_t *o = p->q_head; o != NULL; o = o->queue.next) {
        keys.push_back(o->obj_id);
    }
    return keys;
}

void
YangSieveCache::print() const
{
    cache_t *c = (cache_t *)cache_;
    assert(c);
    Sieve_params_t *p = (Sieve_params_t *)c->eviction_params;
    printf("Cache (size=%zu): ", c->n_obj);
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
