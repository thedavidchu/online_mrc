#include "cpp_lib/cache_statistics.hpp"
#include "arrays/is_last.h"
#include "cpp_lib/format_measurement.hpp"
#include "logger/logger.h"
#include "math/saturation_arithmetic.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/time.h>

constexpr bool DEBUG = false;

static uint64_t
time_diff(uint64_t const start, uint64_t const end)
{
    if (start > end) {
        LOGGER_WARN("end time is before start time!");
        return 0;
    }
    return end - start;
}

static uint64_t
get_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec / 1000 + 1000 * tv.tv_sec;
}

bool
CacheStatistics::should_sample()
{
    return temporal_sampler_.should_sample(current_time_ms_.value_or(0));
}

void
CacheStatistics::sample()
{
    temporal_times_ms_.update(current_time_ms_.value_or(NAN));
    temporal_sizes_.update(size_);
    temporal_max_sizes_.update(max_size_);
    temporal_resident_objects_.update(resident_objs_);
}

void
CacheStatistics::register_cache_action()
{
    if (should_sample()) {
        sample();
    }
}

void
CacheStatistics::hit(uint64_t const size_bytes)
{
    // These are slightly different than the update statistics.
    hit_ops_ += 1;
    hit_bytes_ += size_bytes;
}

void
CacheStatistics::miss(uint64_t const size_bytes)
{
    // These statistics are the sum of the skip and inserts.
    miss_ops_ += 1;
    miss_bytes_ += size_bytes;
}

void
CacheStatistics::start_simulation()
{
    if (sim_start_time_ms_.has_value()) {
        LOGGER_WARN("overwriting existing simulation start time!");
    }
    sim_start_time_ms_ = get_ms();
}

void
CacheStatistics::end_simulation()
{
    if (sim_end_time_ms_.has_value()) {
        LOGGER_WARN("overwriting existing simulation end time!");
    }
    sim_end_time_ms_ = get_ms();
}

void
CacheStatistics::time(uint64_t const tm_ms)
{
    if (!start_time_ms_.has_value()) {
        start_time_ms_ = tm_ms;
    }
    // Unfortunately, Sari's cluster50 Twitter traces doesn't have
    // non-decreasing time stamps, so this triggers more than I'd like.
    if (DEBUG && current_time_ms_.value_or(0) > tm_ms) {
        LOGGER_WARN("old time (%" PRIu64
                    ") is larger than input in time (%" PRIu64 ")",
                    current_time_ms_.value_or(0),
                    tm_ms);
    }
    current_time_ms_ = std::max(current_time_ms_.value_or(0), tm_ms);
}

void
CacheStatistics::skip(uint64_t const size_bytes)
{
    skip_ops_ += 1;
    skip_bytes_ += size_bytes;

    upperbound_wss_ += size_bytes;
    upperbound_ttl_wss_ += size_bytes;

    miss(size_bytes);
    register_cache_action();
}

void
CacheStatistics::insert(uint64_t const size_bytes)
{
    insert_ops_ += 1;
    insert_bytes_ += size_bytes;

    size_ += size_bytes;
    max_size_ = std::max(max_size_, size_);

    resident_objs_ += 1;
    max_resident_objs_ = std::max(max_resident_objs_, resident_objs_);
    upperbound_unique_objs_ += 1;

    upperbound_wss_ += size_bytes;
    upperbound_ttl_wss_ += size_bytes;

    miss(size_bytes);
    register_cache_action();
}

void
CacheStatistics::update(uint64_t const old_size_bytes,
                        uint64_t const new_size_bytes)
{
    update_ops_ += 1;
    update_bytes_ += new_size_bytes;

    size_ -= old_size_bytes;
    size_ += new_size_bytes;
    max_size_ = std::max(max_size_, size_);

    upperbound_wss_ += new_size_bytes;
    upperbound_ttl_wss_ += new_size_bytes;

    // We successfully accessed the old number of bytes.
    // This changes the old semantics, where I would update the cache
    // hit based on the new size.
    hit(old_size_bytes);
    register_cache_action();
}

void
CacheStatistics::lru_evict(uint64_t const size_bytes, double const ttl_ms)
{
    lru_evict_.evict(size_bytes, ttl_ms);

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    register_cache_action();
}

void
CacheStatistics::no_room_evict(uint64_t const size_bytes, double const ttl_ms)
{
    no_room_evict_.evict(size_bytes, ttl_ms);

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    register_cache_action();
}

void
CacheStatistics::ttl_evict(uint64_t const size_bytes, double const ttl_ms)
{
    ttl_evict_.evict(size_bytes, ttl_ms);

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    register_cache_action();
}

void
CacheStatistics::ttl_expire(uint64_t const size_bytes)
{
    ttl_expire_.evict(size_bytes, 0.0);

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    upperbound_ttl_wss_ -= size_bytes;

    register_cache_action();
}

void
CacheStatistics::lazy_expire(uint64_t const size_bytes, double const ttl_ms)
{
    ttl_lazy_expire_.evict(size_bytes, ttl_ms);

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    upperbound_ttl_wss_ -= size_bytes;

    register_cache_action();
}

void
CacheStatistics::sampling_remove(uint64_t const size_bytes)
{
    sampling_remove_ops_ += 1;
    sampling_remove_bytes_ += size_bytes;

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    upperbound_ttl_wss_ -= size_bytes;

    register_cache_action();
}

void
CacheStatistics::deprecated_hit()
{
    // NOTE The register_cache_action() is called in 'update(...)'.
    update(1, 1);
}

void
CacheStatistics::deprecated_miss()
{
    // NOTE The register_cache_action() is called in 'insert(...)'.
    insert(1);
}

uint64_t
CacheStatistics::total_ops() const
{
    return insert_ops_ + update_ops_ + lru_evict_.ops() + ttl_evict_.ops() +
           ttl_expire_.ops() + ttl_lazy_expire_.ops();
}

uint64_t
CacheStatistics::total_bytes() const
{
    return insert_bytes_ + update_bytes_ + lru_evict_.bytes() +
           ttl_evict_.bytes() + ttl_expire_.bytes() + ttl_lazy_expire_.bytes();
}

double
CacheStatistics::miss_ratio() const
{
    uint64_t total_bytes_ = hit_bytes_ + miss_bytes_;
    if (total_bytes_ == 0) {
        return NAN;
    }
    return (double)miss_bytes_ / total_bytes_;
}

uint64_t
CacheStatistics::uptime_ms() const
{
    uint64_t begin = 0, end = 0;
    if (start_time_ms_.has_value() && current_time_ms_.has_value()) {
        begin = start_time_ms_.value();
        end = current_time_ms_.value();
        return time_diff(begin, end);
    }
    return 0;
}

uint64_t
CacheStatistics::sim_uptime_ms() const
{
    uint64_t begin = 0, end = 0;
    if (sim_start_time_ms_.has_value() && sim_end_time_ms_.has_value()) {
        begin = sim_start_time_ms_.value();
        end = sim_end_time_ms_.value();
        return time_diff(begin, end);
    }
    return 0;
}

std::string
CacheStatistics::json() const
{
    std::stringstream ss;
    // NOTE Times end in '[ms]' simply to denote that it's a time value
    //      for downstream processing. The actual unit is included.
    ss << "{"
       << "\"Start Time [ms]\": " << format_time(start_time_ms_.value_or(0))
       << ", \"Current Time [ms]\": "
       << format_time(current_time_ms_.value_or(0))
       << ", \"Uptime [ms]\": " << format_time(uptime_ms())
       << ", \"Skips [#]\": " << format_engineering(skip_ops_)
       << ", \"Skips [B]\": " << format_memory_size(skip_bytes_)
       << ", \"Inserts [#]\": " << format_engineering(insert_ops_)
       << ", \"Inserts [B]\": " << format_memory_size(insert_bytes_)
       << ", \"Updates [#]\": " << format_engineering(update_ops_)
       << ", \"Updates [B]\": "
       << format_memory_size(update_bytes_)
       // Eviction and expiration statistics.
       << ", \"lru_evict\": " << lru_evict_.json()
       << ", \"no_room_evict\": " << no_room_evict_.json()
       << ", \"ttl_evict\": " << ttl_evict_.json()
       << ", \"ttl_expire\": " << ttl_expire_.json()
       << ", \"ttl_lazy_expire\": "
       << ttl_lazy_expire_.json()
       // Total statistics
       << ", \"Total Evicts [#]\": "
       << format_engineering(lru_evict_.ops() + no_room_evict_.ops() +
                             ttl_evict_.ops())
       << ", \"Total Evicts [B]\": "
       << format_memory_size(lru_evict_.bytes() + no_room_evict_.bytes() +
                             ttl_evict_.bytes())
       << ", \"Total Expires [#]\": "
       << format_engineering(ttl_expire_.ops() + ttl_lazy_expire_.ops())
       << ", \"Total Expires [B]\": "
       << format_memory_size(ttl_expire_.bytes() + ttl_lazy_expire_.bytes())
       // Other reasons for removal.
       << ", \"Sampling Removes [#]\": "
       << format_engineering(sampling_remove_ops_)
       << ", \"Sampling Removes [B]\": "
       << format_memory_size(sampling_remove_bytes_)
       // General cache performance statistics.
       << ", \"Hits [#]\": " << format_engineering(hit_ops_)
       << ", \"Hits [B]\": " << format_memory_size(hit_bytes_)
       << ", \"Misses [#]\": " << format_engineering(miss_ops_)
       << ", \"Misses [B]\": " << format_memory_size(miss_bytes_)
       << ", \"Current Size [B]\": " << format_memory_size(size_)
       << ", \"Max Size [B]\": " << format_memory_size(max_size_)
       << ", \"Current Resident Objects [#]\": "
       << format_engineering(resident_objs_)
       << ", \"Max Resident Objects [#]\": "
       << format_engineering(max_resident_objs_)
       << ", \"Upperbound Unique Objects [#]\": "
       << format_engineering(upperbound_unique_objs_)
       << ", \"Upperbound WSS [B]\": " << format_memory_size(upperbound_wss_)
       << ", \"Upperbound TTL WSS [B]\": "
       << format_memory_size(upperbound_ttl_wss_)
       // Aggregate measurements
       << ", \"Simulation Start Time [ms]\": "
       << format_time(sim_start_time_ms_.value_or(0))
       << ", \"Simulation End Time [ms]\": "
       << format_time(sim_end_time_ms_.value_or(0))
       << ", \"Simulation Uptime [ms]\": " << format_time(sim_uptime_ms())
       << ", \"Miss Ratio\": " << miss_ratio()
       << ", \"Temporal Sampler\": " << temporal_sampler_.json()
       << ", \"Mean Size [B]\": " << temporal_sizes_.finite_mean_or(0.0)
       << ", \"Temporal Times [ms]\": " << temporal_times_ms_.str()
       << ", \"Temporal Sizes [B]\": " << temporal_sizes_.str()
       << ", \"Temporal Max Sizes [B]\": " << temporal_max_sizes_.str()
       << ", \"Temporal Resident Objects [#]\": "
       << temporal_resident_objects_.str();
    ss << "}";
    return ss.str();
}

void
CacheStatistics::print(std::string const &name, uint64_t const capacity) const
{
    std::cout << name << "(capacity=" << capacity << "): " << json()
              << std::endl;
}
