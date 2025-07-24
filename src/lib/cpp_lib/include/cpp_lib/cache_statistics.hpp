#pragma once

#include "cpp_lib/eviction_counter.hpp"
#include "cpp_lib/temporal_data.hpp"
#include "cpp_lib/temporal_sampler.hpp"
#include <cstdint>
#include <optional>
#include <string>

using uint64_t = std::uint64_t;

struct CacheStatistics {
private:
    bool
    should_sample();

    void
    sample();

    /// @brief  This hook should be called on every public cache action.
    ///         This should be called after the action has occurred.
    void
    register_cache_action();

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

    uint64_t
    current_time_ms() const
    {
        return current_time_ms_.value_or(0);
    }

    /// @brief  Mark a time.
    void
    time(uint64_t const tm_ms);

    void
    skip(uint64_t const size_bytes);
    void
    insert(uint64_t const size_bytes);
    void
    update(uint64_t const old_size_bytes, uint64_t const new_size_bytes);
    void
    lru_evict(uint64_t const size_bytes, double const remaining_lifespan_ms);
    void
    no_room_evict(uint64_t const size_bytes,
                  double const remaining_lifespan_ms);
    void
    ttl_evict(uint64_t const size_bytes, double const remaining_lifespan_ms);
    void
    ttl_expire(uint64_t const size_bytes);
    /// @note   The TTL should be negative (because it expired in the past).
    void
    lazy_expire(uint64_t const size_bytes, double const remaining_lifespan_ms);
    /// @todo   Add remaining lifespan parameter [ms] to this.
    ///         I didn't because this function isn't even used AFAIK so
    ///         it would be a waste of time.
    void
    sampling_remove(uint64_t const size_bytes);

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
    miss_ratio() const;
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

    EvictionCounter lru_evict_;
    // Evictions because the object is too big on update.
    EvictionCounter no_room_evict_;
    // Evictions by the secondary eviction policy, volatile TTLs.
    EvictionCounter ttl_evict_;
    // Expirations done proactively.
    EvictionCounter ttl_expire_;
    // Expirations done non-actively.
    EvictionCounter ttl_lazy_expire_;

    uint64_t sampling_remove_ops_ = 0;
    uint64_t sampling_remove_bytes_ = 0;

    // MRC statistics
    uint64_t hit_ops_ = 0;
    uint64_t hit_bytes_ = 0;
    uint64_t miss_ops_ = 0;
    uint64_t miss_bytes_ = 0;

    // --- Aggregate statistics ---
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

    // --- Averaged Statistics ---
    TemporalSampler temporal_sampler_{Duration::HOUR, false, true};
    TemporalData temporal_times_ms_;
    TemporalData temporal_max_sizes_;
    // This is the maximum cache size within the current interval. It is
    // probably unimportant to print the interval_max_size_ at the end, because
    // this only represents the maximum size for the final interval.
    uint64_t interval_max_size_ = 0;
    TemporalData temporal_interval_max_sizes_;
    TemporalData temporal_sizes_;
    TemporalData temporal_resident_objects_;
    TemporalData temporal_miss_bytes_;
    TemporalData temporal_hit_bytes_;
};
