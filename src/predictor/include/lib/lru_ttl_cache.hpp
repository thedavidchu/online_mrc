#pragma once

#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_metadata.hpp"
#include "cpp_cache/cache_statistics.hpp"
#include "list/list.hpp"
#include "trace/reader.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdlib.h>
#include <sys/types.h>

using uint64_t = std::uint64_t;
using size_t = std::size_t;

class LRUTTLCache {
private:
    /// @brief  Insert an object into the cache.
    void
    insert(CacheAccess const &access);

    /// @brief  Process an access to an item in the cache.
    bool
    update(CacheAccess const &access);

    /// @brief  Evict an object in the cache (either due to the eviction
    ///         policy or TTL expiration).
    void
    evict(uint64_t const victim_key);

    void
    evict_expired_objects(uint64_t const current_time_ms);

    bool
    ensure_enough_room(size_t const nbytes, List &lru_cache);

    void
    hit(CacheAccess const &access);

    int
    miss(CacheAccess const &access);

public:
    /// @param  capacity: size_t - The capacity of the cache in bytes.
    LRUTTLCache(size_t const capacity)
        : capacity_(capacity)
    {
    }

    int
    access(CacheAccess const &access);

    size_t
    size() const;

    size_t
    capacity() const;

    CacheMetadata const *
    get(uint64_t const key);

private:
    // Maximum number of bytes in the cache.
    size_t const capacity_;

    // Number of bytes in the current cache.
    size_t size_ = 0;

    // Statistics related to cache performance.
    CacheStatistics statistics_;

    uint64_t current_time_ms_ = 0;

    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CacheMetadata> map_;
    // Maps last access time to keys.
    List lru_cache_;

    // Maps expiration time to keys.
    std::multimap<uint64_t, uint64_t> ttl_cache_;
};
