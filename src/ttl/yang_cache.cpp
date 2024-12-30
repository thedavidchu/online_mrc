#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cache_metadata/cache_access.hpp"
#include "yang_cache/yang_cache.hpp"
// NOTE It shouldn't be this painful to include a library file, but it is...
#include "libCacheSim/include/libCacheSim.h"

static common_cache_params_t
set_cc_params(std::size_t const capacity)
{
    common_cache_params_t cc_param = default_common_cache_params();
    cc_param.cache_size = capacity;
    return cc_param;
}

static cache_t *
init_cache(std::size_t const capacity, YangCacheType const type)
{
    switch (type) {
    case YangCacheType::CLOCK:
        return Clock_init(set_cc_params(capacity), NULL);
    case YangCacheType::SIEVE:
        return Sieve_init(set_cc_params(capacity), NULL);
    default:
        assert(0);
    }
}

template <YangCacheType T>
YangCache<T>::YangCache(std::size_t const capacity)
    : capacity_(capacity),
      type_(T),
      cache_(init_cache(capacity, T)),
      req_(new_request())
{
}

template <YangCacheType T> YangCache<T>::~YangCache()
{
    cache_t *c = (cache_t *)cache_;
    c->cache_free(c);
    free_request((request_t *)req_);
}

template <YangCacheType T>
std::size_t
YangCache<T>::size() const
{
    cache_t *c = (cache_t *)cache_;
    return c->n_obj;
}

template <YangCacheType T>
bool
YangCache<T>::contains(std::uint64_t const key) const
{
    cache_t *c = (cache_t *)cache_;
    request_t *r = (request_t *)req_;
    r->obj_id = key;
    return c->find(c, r, false) != NULL;
}

template <YangCacheType T>
int
YangCache<T>::access_item(CacheAccess const &access)
{
    cache_t *c = (cache_t *)cache_;
    request_t *r = (request_t *)req_;
    r->obj_id = access.key;
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

template <YangCacheType T>
std::vector<std::uint64_t>
YangCache<T>::get_keys() const
{
    cache_t *c = (cache_t *)cache_;
    std::vector<std::uint64_t> keys;
    switch (T) {
    case YangCacheType::CLOCK: {
        Clock_params_t *p = (Clock_params_t *)c->eviction_params;
        for (cache_obj_t *o = p->q_head; o != NULL; o = o->queue.next) {
            keys.push_back(o->obj_id);
        }
        break;
    }
    case YangCacheType::SIEVE: {
        Sieve_params_t *p = (Sieve_params_t *)c->eviction_params;
        for (cache_obj_t *o = p->q_head; o != NULL; o = o->queue.next) {
            keys.push_back(o->obj_id);
        }
        break;
    }
    default:
        assert(0);
    }
    return keys;
}

template <YangCacheType T>
void
YangCache<T>::print() const
{
    cache_t *c = (cache_t *)cache_;
    assert(c);
    switch (T) {
    case YangCacheType::CLOCK: {
        Clock_params_t *p = (Clock_params_t *)c->eviction_params;
        printf("SieveCache (size=%zu): ", c->n_obj);
        for (cache_obj_t *o = p->q_head; o != NULL; o = o->queue.next) {
            if (o->sieve.freq) {
                printf("v");
            }
            printf("%zu ", o->obj_id);
        }
        printf("\n");
        break;
    }
    case YangCacheType::SIEVE: {
        Sieve_params_t *p = (Sieve_params_t *)c->eviction_params;
        printf("SieveCache (size=%zu): ", c->n_obj);
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
        break;
    }
    default:
        assert(0);
    }
}

template <YangCacheType T>
bool
YangCache<T>::validate(int const verbose) const
{
    // I trust Juncheng Yang's code, so I don't actually do any major
    // testing. Is this a mistake? Well, I'm doing it for expediency.
    if (verbose) {
        std::cout << "Validate(type=YangCache, type=" << (int)T << ")"
                  << std::endl;
    }
    switch (T) {
    case YangCacheType::CLOCK:
        return true;
    case YangCacheType::SIEVE:
        return true;
    default:
        return true;
    }
}
