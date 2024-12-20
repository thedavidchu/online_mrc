#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cache_metadata/cache_metadata.hpp"
#include "cache_statistics/cache_statistics.hpp"
#include "math/saturation_arithmetic.h"

/// @todo   Handle overflows more gracefully.
///         For example, too large of an epoch should trigger a mass
///         shift of epochs downward. Too large of a access time should
///         trigger a mass shift of access times downward.
static inline std::uint64_t
get_ttl_cache_expiration_time(std::uint64_t const epoch,
                              std::uint64_t const epoch_time_ms,
                              std::uint64_t const access_time_ms)
{
    // TODO Handle an overflow more gracefully.
    assert(access_time_ms < epoch_time_ms);
    // TODO Handle an overflow more gracefully. IDEK if this works...
    assert(std::numeric_limits<std::uint64_t>::max() / epoch > 1);
    return saturation_add(saturation_multiply(epoch, epoch_time_ms),
                          access_time_ms);
}

class BaseTTLCache {
public:
    BaseTTLCache(std::size_t const capacity)
        : capacity_(capacity)
    {
    }

    std::uint64_t
    size() const
    {
        return map_.size();
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

    /// @brief  Change the expiration time of an object.
    bool
    update_expiration_time(std::uint64_t const old_expiration_time_ms,
                           std::uint64_t const key,
                           std::uint64_t const new_expiration_time_ms)
    {
        auto [begin, end] =
            expiration_queue_.equal_range(old_expiration_time_ms);
        for (auto x = begin; x != end; ++x) {
            if (x->second == key) {
                auto nh = expiration_queue_.extract(x);
                nh.key() = new_expiration_time_ms;
                expiration_queue_.insert(std::move(nh));
                return true;
            }
        }
        return true;
    }

    template <class Stream>
    void
    to_stream(Stream &s) const
    {
        s << name << "(capacity=" << capacity_ << ",size=" << size() << ")"
          << std::endl;
        s << "> Key-Metadata Map:" << std::endl;
        for (auto [k, metadata] : map_) {
            // This is inefficient, but looks easier on the eyes.
            std::stringstream ss;
            metadata.to_stream(ss);
            s << ">> key: " << k << ", metadata: " << ss.str() << std::endl;
        }
        s << "> Expiration Queue:" << std::endl;
        for (auto [exp_tm, k] : expiration_queue_) {
            s << ">> expiration time[ms]: " << exp_tm << ", key: " << k
              << std::endl;
        }
    }

    bool
    validate(int const verbose = 0) const
    {
        if (verbose) {
            std::cout << "validate(name=" << name << ",verbose=" << verbose
                      << ")" << std::endl;
        }
        assert(map_.size() == expiration_queue_.size());
        assert(size() <= capacity_);
        if (verbose) {
            std::cout << "> size: " << size() << std::endl;
        }
        if (verbose >= 2) {
            to_stream(std::cout);
        }
        for (auto [k, metadata] : map_) {
            if (verbose >= 2) {
                std::stringstream ss;
                metadata.to_stream(ss);
                std::cout << "> Validating: key=" << k
                          << ", metadata=" << ss.str() << std::endl;
            }
            assert(expiration_queue_.count(metadata.expiration_time_ms_));
            auto obj = expiration_queue_.find(metadata.expiration_time_ms_);
            assert(obj != expiration_queue_.end());
            assert(obj->first == metadata.expiration_time_ms_);
            assert(obj->second == k);
        }
        return true;
    }

    int
    access_item(std::uint64_t const timestamp_ms,
                std::uint64_t const key,
                std::uint64_t const ttl_s);

protected:
    std::uint64_t const ttl_s_ = 1 << 30;
    std::size_t const capacity_;

    /// @brief  Map the keys to the metadata.
    std::unordered_map<std::uint64_t, CacheMetadata> map_;
    std::multimap<std::uint64_t, std::uint64_t> expiration_queue_;

public:
    static constexpr char name[] = "BaseTTLCache";
    CacheStatistics statistics_;
};
