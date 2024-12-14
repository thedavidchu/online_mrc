#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>

#include "cache_statistics/cache_statistics.hpp"
#include "math/saturation_arithmetic.h"

struct myTTLForLRU {
    uint64_t last_access_time_ms;
    uint64_t ttl_s;
};

class TTLLRUCache {
    std::uint64_t const ttl_s_ = 1 << 30;
    std::size_t const capacity_;

    std::unordered_map<std::uint64_t, struct myTTLForLRU> map_;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue_;
    std::uint64_t logical_time_ = 0;

public:
    static constexpr char name[] = "TTLLRUCache";
    CacheStatistics statistics_;

    TTLLRUCache(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    static std::uint64_t
    get_expiry_time_ms(std::uint64_t const access_time_ms,
                       std::uint64_t const ttl_s)
    {
        return saturation_add(access_time_ms, saturation_multiply(1000, ttl_s));
    }

    int
    evict_soonest_expiring()
    {
        auto const x = expiration_queue_.begin();
        std::uint64_t victim_key = x->second;
        expiration_queue_.erase(x);
        std::size_t i = map_.erase(victim_key);
        assert(i == 1);
        assert(map_.size() + 1 == capacity_);
        return 0;
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
            struct myTTLForLRU &s = map_[key];

            // Use old eviction time
            std::uint64_t old_evict_time =
                TTLLRUCache::get_expiry_time_ms(s.last_access_time_ms, s.ttl_s);
            auto const [start, end] =
                expiration_queue_.equal_range(old_evict_time);
            for (auto i = start; i != end; ++i) {
                if (i->second == key) {
                    auto node_handle = expiration_queue_.extract(i);
                    node_handle.key() =
                        TTLLRUCache::get_expiry_time_ms(logical_time_, ttl_s_);
                    expiration_queue_.insert(std::move(node_handle));
                    break;
                }
            }

            // Update new eviction time
            s.last_access_time_ms = logical_time_;
            s.ttl_s = ttl_s_;

            statistics_.hit();
        } else {
            if (map_.size() >= capacity_) {
                this->evict_soonest_expiring();
            }
            map_[key] = {
                logical_time_,
                ttl_s_,
            };
            uint64_t eviction_time_ms =
                TTLLRUCache::get_expiry_time_ms(logical_time_, ttl_s_);
            expiration_queue_.emplace(eviction_time_ms, key);

            statistics_.miss();
        }
        ++logical_time_;
        return 0;
    }
};
