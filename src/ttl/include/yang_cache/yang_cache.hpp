#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cache_metadata/cache_access.hpp"
#include "cache_statistics/cache_statistics.hpp"

enum class YangCacheType {
    CLOCK,
    SIEVE,
};

class YangCache {
public:
    YangCache(std::size_t const capacity, YangCacheType const type);
    ~YangCache();

    std::size_t
    size() const;

    bool
    contains(std::uint64_t const key) const;

    virtual int
    access_item(CacheAccess const &access);

    std::vector<std::uint64_t>
    get_keys() const;

    void
    print() const;

    bool
    validate(int const verbose = 0) const;

    std::size_t const capacity_;
    YangCacheType const type_;
    CacheStatistics statistics_;

private:
    // NOTE This is an opaque pointer to Yang's cache and request.
    void *const cache_;
    void *const req_;
};
