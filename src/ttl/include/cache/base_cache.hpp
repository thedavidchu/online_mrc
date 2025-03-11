#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_metadata.hpp"
#include "cpp_cache/cache_statistics.hpp"

class BaseCache {
public:
    BaseCache(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    bool
    size() const noexcept
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
    contains(std::uint64_t const key) const noexcept
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
    access_item(CacheAccess const &access);

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
    }

    bool
    validate(int const verbose = 0) const
    {
        if (verbose) {
            to_stream(std::cout);
        }
        assert(size() == map_.size());
        return true;
    }

protected:
    std::unordered_map<std::uint64_t, CacheMetadata> map_;
    std::size_t const capacity_;

public:
    static constexpr char name[] = "BaseCache";
    CacheStatistics statistics_;
};
