#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "cache_metadata/cache_metadata.hpp"
#include "cache_statistics/cache_statistics.hpp"

class BaseCache {
public:
    BaseCache(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    bool
    size() const
    {
        return map_.size();
    }

    /// @note   I cannot do std::optional<T const&>, so I chose this.
    CacheMetadata const *
    get_metadata(std::uint64_t const key) const
    {
        auto x = map_.find(key);
        if (x == map_.end()) {
            return nullptr;
        }
        return &x->second;
    }

    /// @note   I cannot do std::optional<T const&>, so I chose this.
    bool
    visit_metadata(
        std::uint64_t const key,
        std::uint64_t const current_time_ms,
        std::optional<std::uint64_t> const new_expiration_time_ms = {})
    {
        auto x = map_.find(key);
        if (x == map_.end()) {
            return false;
        }
        x->second.visit(current_time_ms, new_expiration_time_ms);
        return true;
    }

    bool
    unvisit_metadata(std::uint64_t const key)
    {
        auto x = map_.find(key);
        if (x == map_.end()) {
            return false;
        }
        x->second.unvisit();
        return true;
    }

    bool
    contains(std::uint64_t const key) const
    {
        return map_.find(key) != map_.end();
    }

    /// @brief  Get keys from Key-Value map.
    std::vector<std::uint64_t>
    get_keys() const
    {
        std::vector<std::uint64_t> keys;
        keys.reserve(size());
        for (auto [k, metadata] : map_) {
            keys.push_back(k);
        }
        return keys;
    }

    int
    access_item(std::uint64_t const timestamp_ms,
                std::uint64_t const key,
                std::uint64_t const ttl_s);

    // template <class Stream>
    // void
    // to_stream(Stream &s) const;

    // bool
    // validate(int const verbose = 0) const;

protected:
    std::unordered_map<std::uint64_t, CacheMetadata> map_;
    std::size_t const capacity_;

public:
    static constexpr char name[] = "BaseCache";
    CacheStatistics statistics_;
};
