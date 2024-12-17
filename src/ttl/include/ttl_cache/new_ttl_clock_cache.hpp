#include "ttl_cache/base_ttl_cache.hpp"
#include "unused/mark_unused.h"
#include <cstddef>

class NewTTLClockCache : public BaseTTLCache {
    void
    hit(std::uint64_t const timestamp_ms, std::uint64_t const key)
    {
        auto obj = map_.find(key);
        assert(obj != map_.end());
        auto old_exp_tm_ms = obj->second.expiration_time_ms_;
        if (old_exp_tm_ms < insertion_position_ms_) {
            std::uint64_t const new_exp_tm_ms = old_exp_tm_ms + capacity_;
            obj->second.visit(timestamp_ms, new_exp_tm_ms);
        } else {
            obj->second.visit(timestamp_ms, {});
        }
        statistics_.hit();
    }

    void
    miss(std::uint64_t const timestamp_ms, std::uint64_t const key)
    {
        if (map_.size() == capacity_) {
            auto exp = get_soonest_expiring();
            assert(exp.has_value());
            insertion_position_ms_ = exp->first + capacity_;
            auto r = evict_soonest_expiring();
            assert(r.has_value());
            assert(map_.size() + 1 == capacity_);
        } else {
            // NOTE This won't work if we support user-set TTLs, because
            //      we may end up with overlapping objects at a single
            //      timestamp. Or not, because the user-set TTLs will
            //      be very far in the past. I'm not sure.
            //      I'll need to think about this mroe.
            ++insertion_position_ms_;
        }
        std::uint64_t exp_tm_ms = insertion_position_ms_;
        map_.emplace(key, TTLMetadata(timestamp_ms, exp_tm_ms));
        expiration_queue_.emplace(exp_tm_ms, key);
        statistics_.miss();
    }

public:
    /// @note   The insertion position is initialized to 2^30 seconds.
    ///         However, if we're using real-world time, this value is
    ///         in the past (since 2^30 seconds after 1970 January 1 is
    ///         circa 2002).
    NewTTLClockCache(std::size_t const capacity)
        : BaseTTLCache(capacity),
          insertion_position_ms_(1000 * ttl_s_)
    {
    }

    int
    access_item(std::uint64_t const timestamp_ms,
                std::uint64_t const key,
                std::uint64_t const ttl_s)
    {
        // NOTE We don't support user-set TTLs yet.
        UNUSED(ttl_s);
        assert(map_.size() == expiration_queue_.size());
        assert(map_.size() <= capacity_);
        if (capacity_ == 0) {
            statistics_.miss();
            return 0;
        }
        if (map_.count(key)) {
            hit(timestamp_ms, key);
        } else {
            miss(timestamp_ms, key);
        }
        return 0;
    }

private:
    std::uint64_t insertion_position_ms_ = 0;

public:
    static constexpr char name[] = "NewTTLClockCache";
};
