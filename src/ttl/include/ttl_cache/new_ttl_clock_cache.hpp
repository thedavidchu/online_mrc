#include <cstddef>
#include <iostream>

#include "cache/base_cache.hpp"
#include "cpp_lib/cache_metadata.hpp"
#include "ttl_cache/base_ttl_cache.hpp"
#include "unused/mark_unused.h"

class NewTTLClockCache : public BaseTTLCache {
    /// @brief  This method should be called in three cases:
    ///         1. Upon insertion of a new element
    ///         2. Upon deletion of the soonest expiring object
    ///         3. Upon promotion of the soonest expiring object
    ///         N.B. Cases 2 and 3 are basically saying if the soonest
    ///         expiration time changes.
    void
    update_insertion_position_ms()
    {
        if (size() < capacity_) {
            // NOTE I chose this forumlation over a simple increment
            //      because it is idempotent.
            // NOTE If we have objects with short TTLs, then basing our
            //      insertion point on size may be erroneous here, since
            //      some objects may be evicted. This affects the size
            //      but not the insertion position (AFAIK).
            //      Thus, maybe we should check for the lowest next
            //      value from the current insertion point that does not
            //      have a TTL in the promoted or non-promoted position.
            insertion_position_ms_ = DEFAULT_EPOCH_TIME_MS + size();
        } else {
            std::uint64_t min_exp_tm = expiration_queue_.begin()->first;
            insertion_position_ms_ = min_exp_tm + capacity_;
        }
    }

    void
    hit(std::uint64_t const timestamp_ms, std::uint64_t const key)
    {
        auto obj = map_.find(key);
        assert(obj != map_.end());
        CacheMetadata &metadata = obj->second;
        auto old_exp_tm_ms = metadata.expiration_time_ms_;
        if (old_exp_tm_ms < insertion_position_ms_) {
            std::uint64_t const new_exp_tm_ms = old_exp_tm_ms + capacity_;
            metadata.visit(timestamp_ms, new_exp_tm_ms);
            update_expiration_time(old_exp_tm_ms, key, new_exp_tm_ms);
            update_insertion_position_ms();
        } else {
            metadata.visit(timestamp_ms, {});
        }
        statistics_.deprecated_hit();
    }

    void
    miss(std::uint64_t const timestamp_ms, std::uint64_t const key)
    {
        if (map_.size() == capacity_) {
            auto r = evict_soonest_expiring();
            assert(r.has_value());
            assert(map_.size() + 1 == capacity_);
        }
        std::uint64_t exp_tm_ms = insertion_position_ms_;
        map_.emplace(key, CacheMetadata(timestamp_ms, exp_tm_ms));
        expiration_queue_.emplace(exp_tm_ms, key);
        statistics_.deprecated_miss();
        update_insertion_position_ms();
    }

public:
    /// @note   The insertion position is initialized to 2^30 seconds.
    ///         However, if we're using real-world time, this value is
    ///         in the past (since 2^30 seconds after 1970 January 1 is
    ///         circa 2002).
    NewTTLClockCache(std::size_t const capacity)
        : BaseTTLCache(capacity),
          insertion_position_ms_(DEFAULT_EPOCH_TIME_MS)
    {
    }

    /// @note   This is for verbose debugging purposes and isn't
    ///         guaranteed to be a stable interface.
    void
    debug_print() const
    {
        std::cout << name << "(insertion_position_ms=" << insertion_position_ms_
                  << "): ";
        std::cout << "{";
        for (auto [k, _] : map_) {
            std::cout << k << ",";
        }
        std::cout << "} ";
        for (auto [tm, k] : expiration_queue_) {
            std::cout << k << "@" << tm << ",";
        }
        std::cout << std::endl;
    }

    bool
    validate(int const verbose = 0) const
    {
        return BaseTTLCache::validate(verbose);
    }

    int
    access_item(CacheAccess const &access)
    {
        // NOTE We don't support user-set TTLs yet.
        assert(map_.size() == expiration_queue_.size());
        assert(map_.size() <= capacity_);
        if (capacity_ == 0) {
            statistics_.deprecated_miss();
            return 0;
        }
        if (map_.count(access.key)) {
            hit(access.timestamp_ms, access.key);
        } else {
            miss(access.timestamp_ms, access.key);
        }
        return 0;
    }

private:
    std::uint64_t insertion_position_ms_ = 0;

public:
    static constexpr char name[] = "NewTTLClockCache";
};
