#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

#include "cache/base_cache.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "math/saturation_arithmetic.h"

class TTLFIFOCache {
    std::uint64_t const ttl_s_ = 1 << 30;
    std::size_t const capacity_;

    std::unordered_map<std::uint64_t, bool> map_;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue_;
    std::uint64_t logical_time_ = 0;

public:
    static constexpr char name[] = "TTLFIFOCache";
    CacheStatistics statistics_;

    TTLFIFOCache(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    std::optional<std::uint64_t>
    evict_ttl_fifo()
    {
        auto const it = expiration_queue_.begin();
        if (it == expiration_queue_.end()) {
            return std::nullopt;
        }
        std::uint64_t victim_key = it->second;
        expiration_queue_.erase(it);
        std::size_t i = map_.erase(victim_key);
        assert(i == 1);
        return victim_key;
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
            map_[access.key] = true;
            statistics_.deprecated_hit();
        } else {
            if (map_.size() >= capacity_) {
                auto r = this->evict_ttl_fifo();
                assert(r);
                assert(map_.size() + 1 == capacity_);
            }
            map_[access.key] = false;
            uint64_t eviction_time_ms =
                saturation_add(logical_time_,
                               saturation_multiply(1000, ttl_s_));
            expiration_queue_.emplace(eviction_time_ms, access.key);
            statistics_.deprecated_miss();
        }
        ++logical_time_;
        return 0;
    }
};
