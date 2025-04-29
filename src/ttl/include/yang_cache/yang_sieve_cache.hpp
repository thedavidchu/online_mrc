#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cache/base_cache.hpp"
#include "cpp_lib/cache_statistics.hpp"

class YangSieveCache {
public:
    YangSieveCache(std::size_t capacity);
    ~YangSieveCache();

    std::size_t
    size() const;

    bool
    contains(std::uint64_t const key) const;

    int
    access_item(CacheAccess const &access);

    std::vector<std::uint64_t>
    get_keys() const;

    void
    print() const;

    std::size_t const capacity_;
    CacheStatistics statistics_;

private:
    // NOTE This is an opaque pointer to Yang's cache and request.
    void *const cache_;
    void *const req_;
};
