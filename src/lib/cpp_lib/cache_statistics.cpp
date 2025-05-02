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
    bool r = is_last(sampling_counter_ % sampling_period_, sampling_period_);
    // I start counting from 0, so I ensure that the counter is incremented
    // after the check.
    ++sampling_counter_;
    return r;
}

void
CacheStatistics::sample()
{
    temporal_sizes_.update(size_);
    saturation_multiply(2, sampling_period_);
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
CacheStatistics::lru_evict(uint64_t const size_bytes)
{
    lru_evict_ops_ += 1;
    lru_evict_bytes_ += size_bytes;

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    register_cache_action();
}

void
CacheStatistics::no_room_evict(uint64_t const size_bytes)
{
    no_room_ops_ += 1;
    no_room_bytes_ += size_bytes;

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    register_cache_action();
}

void
CacheStatistics::ttl_evict(uint64_t const size_bytes)
{
    ttl_evict_ops_ += 1;
    ttl_evict_bytes_ += size_bytes;

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    register_cache_action();
}

void
CacheStatistics::ttl_expire(uint64_t const size_bytes)
{
    ttl_expire_ops_ += 1;
    ttl_expire_bytes_ += size_bytes;

    size_ -= size_bytes;
    // Cannot set a new maximum size.

    resident_objs_ -= 1;
    // Cannot set a new maximum number of resident objects.

    upperbound_ttl_wss_ -= size_bytes;

    register_cache_action();
}

void
CacheStatistics::lazy_expire(uint64_t const size_bytes)
{
    lazy_expire_ops_ += 1;
    lazy_expire_bytes_ += size_bytes;

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
    return insert_ops_ + update_ops_ + lru_evict_ops_ + ttl_evict_ops_ +
           ttl_expire_ops_ + lazy_expire_ops_;
}

uint64_t
CacheStatistics::total_bytes() const
{
    return insert_bytes_ + update_bytes_ + lru_evict_bytes_ + ttl_evict_bytes_ +
           ttl_expire_bytes_ + lazy_expire_bytes_;
}

double
CacheStatistics::miss_rate() const
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
    ss << "{"
       << "\"start_time_ms\": " << format_time(start_time_ms_.value_or(0))
       << ", \"current_time_ms\": " << format_time(current_time_ms_.value_or(0))
       << ", \"uptime_ms\": " << format_time(uptime_ms())
       << ", \"skip_ops\": " << format_engineering(skip_ops_)
       << ", \"skip_bytes\": " << format_memory_size(skip_bytes_)
       << ", \"insert_ops\": " << format_engineering(insert_ops_)
       << ", \"insert_bytes\": " << format_memory_size(insert_bytes_)
       << ", \"update_ops\": " << format_engineering(update_ops_)
       << ", \"update_bytes\": "
       << format_memory_size(update_bytes_)
       // Eviction and expiration statistics.
       << ", \"lru_evict_ops\": " << format_engineering(lru_evict_ops_)
       << ", \"lru_evict_bytes\": " << format_memory_size(lru_evict_bytes_)
       << ", \"no_room_evict_ops\": " << format_engineering(no_room_ops_)
       << ", \"no_room_evict_bytes\": " << format_memory_size(no_room_bytes_)
       << ", \"ttl_evict_ops\": " << format_engineering(ttl_evict_ops_)
       << ", \"ttl_evict_bytes\": " << format_memory_size(ttl_evict_bytes_)
       << ", \"ttl_expire_ops\": " << format_engineering(ttl_expire_ops_)
       << ", \"ttl_expire_bytes\": " << format_memory_size(ttl_expire_bytes_)
       << ", \"lazy_expire_ops\": " << format_engineering(lazy_expire_ops_)
       << ", \"lazy_expire_bytes\": "
       << format_memory_size(lazy_expire_bytes_)
       // Total statistics
       << ", \"total_evict_ops\": "
       << format_engineering(lru_evict_ops_ + no_room_ops_ + ttl_evict_ops_)
       << ", \"total_evict_bytes\": "
       << format_memory_size(lru_evict_bytes_ + no_room_bytes_ +
                             ttl_evict_bytes_)
       << ", \"total_expire_ops\": "
       << format_engineering(ttl_expire_ops_ + lazy_expire_ops_)
       << ", \"total_expire_bytes\": "
       << format_memory_size(ttl_expire_bytes_ + lazy_expire_bytes_)
       // Other reasons for removal.
       << ", \"sampling_remove_ops\": "
       << format_engineering(sampling_remove_ops_)
       << ", \"sampling_remove_bytes\": "
       << format_memory_size(sampling_remove_bytes_)
       // General cache performance statistics.
       << ", \"hit_ops\": " << format_engineering(hit_ops_)
       << ", \"hit_bytes\": " << format_memory_size(hit_bytes_)
       << ", \"miss_ops\": " << format_engineering(miss_ops_)
       << ", \"miss_bytes\": " << format_memory_size(miss_bytes_)
       << ", \"size\": " << format_memory_size(size_)
       << ", \"max_size\": " << format_memory_size(max_size_)
       << ", \"resident_objs\": " << format_engineering(resident_objs_)
       << ", \"max_resident_objs\": " << format_engineering(max_resident_objs_)
       << ", \"upperbound_unique_objs\": "
       << format_engineering(upperbound_unique_objs_)
       << ", \"upperbound_wss\": " << format_memory_size(upperbound_wss_)
       << ", \"upperbound_ttl_wss\": "
       << format_memory_size(upperbound_ttl_wss_)
       // Aggregate measurements
       << ", \"sim_start_time_ms\": "
       << format_time(sim_start_time_ms_.value_or(0))
       << ", \"sim_end_time_ms\": " << format_time(sim_end_time_ms_.value_or(0))
       << ", \"sim_uptime_ms\": " << format_time(sim_uptime_ms())
       << ", \"miss rate\": " << miss_rate()
       << ", \"sampling_counter\": " << sampling_counter_
       << ", \"sampling_period\": " << sampling_period_
       << ", \"mean_size\": " << temporal_sizes_.mean() << "}";
    return ss.str();
}

void
CacheStatistics::print(std::string const &name, uint64_t const capacity) const
{
    std::cout << name << "(capacity=" << capacity << "): " << json()
              << std::endl;
}
