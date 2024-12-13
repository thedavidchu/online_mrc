#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

#include "cache_statistics.hpp"
#include "math/saturation_arithmetic.h"

class TTLClockCache {
    std::uint64_t const ttl_s_ = 1 << 30;
    std::size_t const capacity_;

    std::unordered_map<std::uint64_t, bool> map_;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue_;
    std::uint64_t logical_time_ = 0;

public:
    static constexpr char name[] = "TTLClockCache";
    CacheStatistics statistics_;

    TTLClockCache(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    static std::uint64_t
    get_expiry_time_ms(std::uint64_t const access_time_ms,
                       std::uint64_t const ttl_s)
    {
        return saturation_add(access_time_ms, saturation_multiply(1000, ttl_s));
    }

    std::optional<std::uint64_t>
    evict_ttl_clock()
    {
        if (map_.size() == 0) {
            return {};
        }
        for (auto it = expiration_queue_.begin(); it != expiration_queue_.end();
             ++it) {
            auto const victim_key = it->second;
            if (map_[victim_key]) {
                map_[victim_key] = false;
                auto nh = expiration_queue_.extract(it);
                nh.key() =
                    TTLClockCache::get_expiry_time_ms(logical_time_, ttl_s_);
                // NOTE We don't seem to access the object we just
                //      inserted into the multimap data structure...
                //      how come?
                expiration_queue_.insert(std::move(nh));
            } else {
                expiration_queue_.erase(it);
                std::size_t i = map_.erase(victim_key);
                assert(i == 1);
                return victim_key;
            }
        }
        // We've traversed the whole clock, so retry! I don't understand
        // why this is necessary if we've inserted a refreshed object
        // into the eviction_queue_.
        return this->evict_ttl_clock();
    }

    int
    access_item(std::uint64_t const key)
    {
        assert(map_.size() == expiration_queue_.size());
        if (capacity_ == 0) {
            statistics_.miss();
            return 0;
        }
        if (map_.count(key)) {
            map_[key] = true;
            statistics_.hit();
        } else {
            if (map_.size() >= capacity_) {
                auto r = this->evict_ttl_clock();
                assert(r.has_value());
                assert(map_.size() + 1 == capacity_);
            }
            map_[key] = false;
            uint64_t eviction_time_ms =
                TTLClockCache::get_expiry_time_ms(logical_time_, ttl_s_);
            expiration_queue_.emplace(eviction_time_ms, key);
            statistics_.miss();
        }
        ++logical_time_;
        return 0;
    }
};
