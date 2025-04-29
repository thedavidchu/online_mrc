#pragma once

#include <cstddef>
#include <cstdint>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

/// @note   For eviction algorithms like Clock and SIEVE that mark
///         whether the hand has touched them, there may need to be two
///         types of iterators; a read-only one that gives the objects'
///         total eviction order and another one that changes the state
///         of objects accordingly.
template <typename T> class TotalEvictionOrderCacheIterator {
public:
    T
    operator*() const;

    void
    operator++();

    bool
    operator==(TotalEvictionOrderCacheIterator const &rhs) const;

    bool
    operator!=(TotalEvictionOrderCacheIterator const &rhs) const;
};

template <typename T> class TotalEvictionOrderCache {
public:
    virtual void
    access(uint64_t const key);

    virtual void
    remove(uint64_t const key);

    virtual size_t
    size() const;

    virtual TotalEvictionOrderCacheIterator<T>
    begin() const;

    virtual TotalEvictionOrderCacheIterator<T>
    end() const;
};
