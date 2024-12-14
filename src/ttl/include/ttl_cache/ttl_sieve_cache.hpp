#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "cache_statistics/cache_statistics.hpp"
#include "math/saturation_arithmetic.h"

class TTLSieveCache {
    std::uint64_t const ttl_s_ = 1 << 30;
    std::size_t const capacity_;

    std::unordered_map<std::uint64_t, std::uint64_t> map_;
    std::multimap<std::uint64_t, std::uint64_t> ttl_queue_;
    std::uint64_t logical_time_ = 0;
    // NOTE The epoch starts at 1 because we multiply it by the TTL
    //      interval.
    std::uint64_t epoch_ = 1;

public:
    static constexpr char name[] = "TTLSieveCache";
    CacheStatistics statistics_;

    TTLSieveCache(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    std::size_t
    size() const
    {
        return map_.size();
    }

    std::vector<std::uint64_t>
    get_keys_in_eviction_order() const
    {
        std::vector<std::uint64_t> keys;
        keys.reserve(ttl_queue_.size());
        for (auto [exp_tm, key] : ttl_queue_) {
            keys.push_back(key);
        }
        return keys;
    }

    static std::uint64_t
    get_expiry_time_ms(std::uint64_t const access_time_ms,
                       std::uint64_t const epoch,
                       std::uint64_t const ttl_s)
    {
        return saturation_add(
            access_time_ms,
            saturation_multiply(epoch, saturation_multiply(1000, ttl_s)));
    }

    std::optional<std::uint64_t>
    evict_soonest_expiring()
    {
        auto const it = ttl_queue_.begin();
        if (it == ttl_queue_.end()) {
            return std::nullopt;
        }
        std::uint64_t victim_key = it->second;
        ttl_queue_.erase(it);
        std::size_t i = map_.erase(victim_key);
        assert(i == 1);
        return victim_key;
    }

    int
    access_item(std::uint64_t const key)
    {
        assert(map_.size() == ttl_queue_.size());
        if (capacity_ == 0) {
            statistics_.miss();
            return 0;
        }
        if (map_.count(key)) {
            // Only increment the eviction time if it hasn't been
            // incremented already in this epoch.
            if (map_[key] <=
                TTLSieveCache::get_expiry_time_ms(0, epoch_ + 1, ttl_s_)) {
                auto nh = ttl_queue_.extract(map_[key]);
                nh.key() += ttl_s_;
                ttl_queue_.insert(std::move(nh));
                map_[key] += ttl_s_;
            }
            statistics_.hit();
        } else {
            if (map_.size() >= capacity_) {
                auto r = this->evict_soonest_expiring();
                assert(r.has_value());
                assert(map_.size() + 1 == capacity_);
            }
            uint64_t eviction_time_ms =
                TTLSieveCache::get_expiry_time_ms(logical_time_,
                                                  epoch_,
                                                  ttl_s_);
            map_[key] = eviction_time_ms;
            ttl_queue_.emplace(eviction_time_ms, key);
            statistics_.miss();
        }
        ++logical_time_;
        return 0;
    }
};
