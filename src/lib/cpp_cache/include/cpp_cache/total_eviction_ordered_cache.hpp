#pragma once

#include <cstddef>
#include <cstdint>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class TotalEvictionOrderCacheIterator {};

class TotalEvictionOrderCache {
public:
    void
    access(uint64_t const key);

    void
    remove(uint64_t const key);

    size_t
    size() const;

    TotalEvictionOrderCacheIterator
    begin();

    TotalEvictionOrderCacheIterator
    end();
};
