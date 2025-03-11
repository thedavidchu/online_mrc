#pragma once

#include <cstdint>
#include <string>

using uint64_t = std::uint64_t;

struct CacheStatistics {
private:
    void
    hit(uint64_t const size_bytes);
    void
    miss(uint64_t const size_bytes);

public:
    void
    skip(uint64_t const size_bytes);
    void
    insert(uint64_t const size_bytes);
    void
    update(uint64_t const old_size_bytes, uint64_t const new_size_bytes);
    void
    evict(uint64_t const size_bytes);
    void
    expire(uint64_t const size_bytes);

    // === Aggregate access methods ===

    uint64_t
    total_ops() const;
    uint64_t
    total_bytes() const;
    double
    miss_rate() const;

    std::string
    json() const;

public:
    uint64_t skip_ops_ = 0;
    uint64_t skip_bytes_ = 0;

    uint64_t insert_ops_ = 0;
    uint64_t insert_bytes_ = 0;

    uint64_t update_ops_ = 0;
    uint64_t update_bytes_ = 0;

    uint64_t evict_ops_ = 0;
    uint64_t evict_bytes_ = 0;

    uint64_t expire_ops_ = 0;
    uint64_t expire_bytes_ = 0;

    // MRC statistics
    uint64_t hit_ops_ = 0;
    uint64_t hit_bytes_ = 0;
    uint64_t miss_ops_ = 0;
    uint64_t miss_bytes_ = 0;

    // Aggregate statistics
    uint64_t size_ = 0;
    uint64_t max_size_ = 0;

    uint64_t resident_objs_ = 0;
    uint64_t max_resident_objs_ = 0;

    // Working-Set-Size Statistics
    // The Working Set Size (WSS) is the largest a cache would need to
    // be such that there are no evictions. My original method for
    // measuring this would have been to count the bytes inserted and
    // take the maximum of updates, while ignoring evictions; however,
    // this is flawed. The WSS cannot be measured if there are
    // evictions, because when an object is evicted and reinserted, we
    // cannot differentiate between that and two unrelated objects.
    uint64_t upperbound_wss_ = 0;
    // Similarly, the TTL WSS is the largest a cache needs to be to
    // ensure no evictions while taking TTLs into account (therefore, it
    // is equal or smaller than the WSS).
    // To accurate measure this, you may need to run the simulation such
    // that no evictions occur; when an object is evicted, it will not
    // (necessarily) expire, because it is not in the cache. I'm not
    // sure if this is truly the upperbound because LRU+TTLs is a non-
    // stack algorithm.
    uint64_t upperbound_ttl_wss_ = 0;
};
