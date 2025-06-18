#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>

#include "cache/base_cache.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "math/saturation_arithmetic.h"

struct myTTLForLFU {
    static constexpr std::uint64_t BASE_FRQ = 1;

    std::uint64_t frequency;
    std::uint64_t last_access_time_ms;
    std::uint64_t ttl_s;
};

class TTLLFUCache {
    std::uint64_t const ttl_s_ = 1 << 30;
    std::size_t const capacity_;

    std::unordered_map<std::uint64_t, struct myTTLForLFU> map_;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue_;
    std::uint64_t logical_time_ = 0;

public:
    static constexpr char name[] = "TTLLFUCache";
    CacheStatistics statistics_;

    TTLLFUCache(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    static std::uint64_t
    get_expiry_time_ms(std::uint64_t const access_time_ms,
                       std::uint64_t const ttl_s,
                       std::uint64_t const frequency)
    {
        return saturation_add(
            access_time_ms,
            saturation_multiply(1 + frequency,
                                saturation_multiply(1000, ttl_s)));
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
    access_item(CacheAccess const &access)
    {
        assert(map_.size() == expiration_queue_.size());
        if (capacity_ == 0) {
            statistics_.deprecated_miss();
            return 0;
        }
        if (map_.count(access.key)) {
            struct myTTLForLFU &s = map_[access.key];

            // Use old eviction time
            std::uint64_t old_evict_time =
                TTLLFUCache::get_expiry_time_ms(s.last_access_time_ms,
                                                s.ttl_s,
                                                s.frequency);
            auto const [start, end] =
                expiration_queue_.equal_range(old_evict_time);
            for (auto i = start; i != end; ++i) {
                if (i->second == access.key) {
                    auto node_handle = expiration_queue_.extract(i);
                    node_handle.key() =
                        TTLLFUCache::get_expiry_time_ms(logical_time_,
                                                        ttl_s_,
                                                        s.frequency + 1);
                    expiration_queue_.insert(std::move(node_handle));
                    break;
                }
            }

            // Update new eviction time
            s.frequency += 1;
            s.last_access_time_ms = logical_time_;
            s.ttl_s = ttl_s_;

            statistics_.deprecated_hit();
        } else {
            if (map_.size() >= capacity_) {
                this->evict_soonest_expiring();
            }
            map_.emplace(access.key,
                         myTTLForLFU::BASE_FRQ,
                         logical_time_,
                         ttl_s_);
            uint64_t eviction_time_ms =
                TTLLFUCache::get_expiry_time_ms(logical_time_, ttl_s_, 0);
            expiration_queue_.emplace(eviction_time_ms, access.key);

            statistics_.deprecated_miss();
        }
        ++logical_time_;
        return 0;
    }
};
