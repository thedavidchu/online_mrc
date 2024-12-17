#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cache_statistics/cache_statistics.hpp"
#include "math/saturation_arithmetic.h"

static inline std::uint64_t
get_expiration_time(std::uint64_t const current_time_ms,
                    std::uint64_t const ttl_s)
{
    return saturation_add(current_time_ms, saturation_multiply(1000, ttl_s));
}

struct TTLMetadata {
    /// @note   We don't consider the first access in the frequency
    ///         counter. There's no real reason, I just think it's nice
    ///         to start at 0 rather than 1.
    std::size_t frequency_ = 0;
    std::uint64_t insertion_time_ms_ = 0;
    std::uint64_t last_access_time_ms_ = 0;
    bool visited_ = false;

    std::uint64_t expiration_time_ms_ = 0;

    TTLMetadata(std::uint64_t const insertion_time_ms,
                std::uint64_t const expiration_time_ms)
        : insertion_time_ms_(insertion_time_ms),
          last_access_time_ms_(insertion_time_ms),
          expiration_time_ms_(expiration_time_ms)
    {
    }

    void
    visit(std::uint64_t const access_time_ms,
          std::optional<std::uint64_t> const new_expiration_time_ms)
    {
        ++frequency_;
        last_access_time_ms_ = access_time_ms;
        visited_ = true;
        if (new_expiration_time_ms) {
            expiration_time_ms_ = new_expiration_time_ms.value();
        }
    }

    void
    unvisit()
    {
        visited_ = false;
    }
};

class BaseTTLCache {
public:
    BaseTTLCache(std::size_t const capacity)
        : capacity_(capacity)
    {
    }

    /// @brief  Get keys in current eviction order (from soonest to
    ///         furthest eviction time).
    std::vector<std::uint64_t>
    get_keys() const
    {
        std::vector<std::uint64_t> keys;
        keys.reserve(capacity_);
        for (auto [exp_tm, key] : expiration_queue_) {
            keys.push_back(key);
        }
        return keys;
    }

    /// @brief  Get {expiration time, key} pair of the soonest expiring
    ///         data.
    std::optional<std::pair<std::uint64_t, std::uint64_t>>
    get_soonest_expiring()
    {
        auto begin = expiration_queue_.begin();
        if (begin == expiration_queue_.end()) {
            return {};
        }
        return {{begin->first, begin->second}};
    }

    std::optional<std::uint64_t>
    evict_soonest_expiring()
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
    access_item(std::uint64_t const timestamp_ms,
                std::uint64_t const key,
                std::uint64_t const ttl_s);

protected:
    std::uint64_t const ttl_s_ = 1 << 30;
    std::size_t const capacity_;

    /// @brief  Map the keys to the metadata.
    std::unordered_map<std::uint64_t, TTLMetadata> map_;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue_;

public:
    static constexpr char name[] = "BaseTTLCache";
    CacheStatistics statistics_;
};
