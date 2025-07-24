#pragma once
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/util.hpp"
#include "lib/eviction_cause.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

/// @note   Every function has its definition so the linker is made happy.
///         These are overwritten by the children classes. I tried doing
///         this with non-virtual methods but it didn't link correctly.
///         Source:
///         https://stackoverflow.com/questions/3065154/undefined-reference-to-vtable
class Accurate {
protected:
    /// @brief  Insert an object into the cache.
    virtual void
    insert(CacheAccess const &)
    {
        assert(0 && "unimplemented");
    }

    /// @brief  Update an existing object in the cache.
    virtual void
    update(CacheAccess const &, CacheMetadata &)
    {
        assert(0 && "unimplemented");
    }

    /// @brief  Remove an object from the cache.
    virtual void
    remove(uint64_t const, EvictionCause const, CacheAccess const &)
    {
        assert(0 && "unimplemented");
    }

    /// @brief  Remove expired objects from the cache by calling remove().
    /// @note   Run on every access. If you want this to be sampled (e.g.
    ///         run once every 10 seconds), then the sampling logic
    ///         should be within this function.
    virtual void
    remove_expired(CacheAccess const &)
    {
        assert(0 && "unimplemented");
    }

    bool
    accessed_is_expired(CacheAccess const &access) const
    {
        return map_.contains(access.key) &&
               map_.at(access.key).ttl_ms(access.timestamp_ms) < 0.0;
    }

    void
    remove_accessed_if_expired(CacheAccess const &access)
    {
        if (accessed_is_expired(access)) {
            remove(access.key, EvictionCause::AccessExpired, access);
        }
    }

public:
    Accurate(uint64_t const capacity_bytes)
        : capacity_bytes_{capacity_bytes}
    {
    }

    void
    start_simulation()
    {
        statistics_.start_simulation();
    }

    void
    end_simulation()
    {
        statistics_.end_simulation();
    }

    /// @brief  Process a cache access (specifically a 'get-set').
    void
    access(CacheAccess const &access)
    {
        assert(size_bytes_ == statistics_.size_);
        statistics_.time(access.timestamp_ms);
        remove_accessed_if_expired(access);
        remove_expired(access);
        if (map_.contains(access.key)) {
            update(access, map_.at(access.key));
        } else {
            insert(access);
        }
    }

    std::string
    json(std::unordered_map<std::string, std::string> extras = {}) const
    {
        return map2str(std::vector<std::pair<std::string, std::string>>{
            {"Capacity [B]", val2str(format_memory_size(capacity_bytes_))},
            {"Statistics", statistics_.json()},
            {"Extras", map2str(extras)},
            {"Expiration Work [#]", std::to_string(expiration_work_)},
        });
    }

protected:
    // Maximum number of bytes in the cache.
    size_t const capacity_bytes_;
    // Number of bytes in the current cache.
    size_t size_bytes_ = 0;
    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CacheMetadata> map_;
    // Statistics related to cache performance.
    CacheStatistics statistics_;
    // HACK This is a hacky way to make it easier to print this.
    uint64_t expiration_work_ = 0;
};
