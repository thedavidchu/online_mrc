#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "cache/base_cache.hpp"
#include "cpp_lib/cache_statistics.hpp"

class SieveCache {
    struct SieveBucket {
        bool visited;
        std::uint64_t idx;
    };

    std::unordered_map<std::uint64_t, SieveBucket> map_;
    std::map<std::uint64_t, std::uint64_t> queue_;
    std::size_t const capacity_;
    std::uint64_t hand_ = 0;
    std::uint64_t logical_time_ = 0;

public:
    static constexpr char name[] = "SieveCache";
    CacheStatistics statistics_;

    SieveCache(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    std::size_t
    size()
    {
        return map_.size();
    }

    std::optional<std::uint64_t>
    delete_sieve()
    {
        if (queue_.size() == 0) {
            return {};
        }
        // NOTE This starts on the element greater-than-or-equal-to the
        //      hand_. This hand_ may be in the cache because hand_ = 0
        //      is in the cache; it also may not be in the cache since
        //      it points to the last element that we removed.
        for (auto victim_it = queue_.lower_bound(hand_);
             victim_it != queue_.end();
             ++victim_it) {
            std::uint64_t victim_key = victim_it->second;
            assert(map_.count(victim_key));
            if (map_[victim_key].visited) {
                map_[victim_key].visited = false;
            } else {
                queue_.erase(victim_it);
                map_.erase(victim_key);
                hand_ = victim_it->first;
                return victim_key;
            }
        }
        // NOTE We have completed a full cycle, so let's begin again!
        hand_ = 0;
        return this->delete_sieve();
    }

    int
    access_item(CacheAccess const &access)
    {
        assert(map_.size() == queue_.size());
        if (capacity_ == 0) {
            statistics_.deprecated_miss();
            return 0;
        }
        if (map_.count(access.key)) {
            map_[access.key].visited = true;
            statistics_.deprecated_hit();
        } else {
            if (queue_.size() >= capacity_) {
                this->delete_sieve();
                assert(map_.size() + 1 == capacity_);
            }
            assert(map_.size() + 1 <= capacity_);
            struct SieveBucket new_bucket = {false, logical_time_};
            auto [it, inserted] = map_.emplace(access.key, new_bucket);
            assert(inserted && it->first == access.key &&
                   it->second.visited == false &&
                   it->second.idx == logical_time_);
            queue_.emplace(logical_time_, access.key);
            assert(map_.size() <= capacity_);
            statistics_.deprecated_miss();
        }
        ++logical_time_;
        return 0;
    }

    /// @note   This is simply for debugging purposes.
    std::vector<std::uint64_t>
    get_keys_in_eviction_order()
    {
        std::vector<std::uint64_t> r;
        // Just a small optimization to avoid resizing.
        r.reserve(queue_.size());
        for (auto [expiry_tm, key] : queue_) {
            r.push_back(key);
        }
        return r;
    }
};
