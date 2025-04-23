#pragma once

#include <cstdint>
#include <optional>
#include <string>

using uint64_t = std::uint64_t;

struct CacheStatistics {
private:
    void
    hit(uint64_t const size_bytes);
    void
    miss(uint64_t const size_bytes);

public:
    // Track the simulation runtime of the cache.
    void
    start_simulation();
    void
    end_simulation();

    void
    time(uint64_t const tm_ms);

    void
    skip(uint64_t const size_bytes);
    void
    insert(uint64_t const size_bytes);
    void
    update(uint64_t const old_size_bytes, uint64_t const new_size_bytes);
    void
    lru_evict(uint64_t const size_bytes);
    void
    no_room_evict(uint64_t const size_bytes);
    void
    ttl_evict(uint64_t const size_bytes);
    void
    ttl_expire(uint64_t const size_bytes);
    void
    lazy_expire(uint64_t const size_bytes);
    void
    sampling_evict(uint64_t const size_bytes);

    // Deprecated, but useful for legacy TTL code.
    void
    deprecated_hit();
    void
    deprecated_miss();

    // === Aggregate access methods ===

    uint64_t
    total_ops() const;
    uint64_t
    total_bytes() const;
    double
    miss_rate() const;
    uint64_t
    uptime_ms() const;
    uint64_t
    sim_uptime_ms() const;

    std::string
    json() const;

    // Deprecated
    void
    print(std::string const &name, uint64_t const capacity) const;

public:
    std::optional<uint64_t> start_time_ms_ = std::nullopt;
    std::optional<uint64_t> current_time_ms_ = std::nullopt;

    std::optional<uint64_t> sim_start_time_ms_ = std::nullopt;
    std::optional<uint64_t> sim_end_time_ms_ = std::nullopt;

    uint64_t skip_ops_ = 0;
    uint64_t skip_bytes_ = 0;

    uint64_t insert_ops_ = 0;
    uint64_t insert_bytes_ = 0;

    uint64_t update_ops_ = 0;
    uint64_t update_bytes_ = 0;

    uint64_t lru_evict_ops_ = 0;
    uint64_t lru_evict_bytes_ = 0;

    uint64_t no_room_ops_ = 0;
    uint64_t no_room_bytes_ = 0;

    // Evictions by the secondary eviction policy, volatile TTLs.
    uint64_t ttl_evict_ops_ = 0;
    uint64_t ttl_evict_bytes_ = 0;

    uint64_t ttl_expire_ops_ = 0;
    uint64_t ttl_expire_bytes_ = 0;

    // Expirations done non-actively.
    uint64_t lazy_expire_ops_ = 0;
    uint64_t lazy_expire_bytes_ = 0;

    uint64_t sampling_evict_ops_ = 0;
    uint64_t sampling_evict_bytes_ = 0;

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
    uint64_t upperbound_unique_objs_ = 0;

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
