#pragma once

#include "cpp_cache/lru_cache.hpp"
#include "cpp_cache/total_eviction_ordered_cache.hpp"
#include "cpp_struct/hash_list.hpp"

#include <cstddef>
#include <cstdint>
#include <map>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class LFUCache;

class LFUIterator : public TotalEvictionOrderCacheIterator<ListNode const *> {
public:
    LFUIterator(LFUCache const &cache);

    ListNode const *
    operator*() const;

    void
    operator++();

    bool
    operator==(LFUIterator const &rhs) const;

    bool
    operator!=(LFUIterator const &rhs) const;

private:
    LFUCache const &cache_;
    ListNode const *node_ = nullptr;
    // This should be 0 (not 1) because I take the upper_bound with this
    // as the argument. For a frequency of 1, I need to pass in 0 to
    // "upper_bound" it. See the implementation of the operator++.
    uint64_t frq = 0;
};

class LFUCache : TotalEvictionOrderCache<ListNode const *> {
public:
    LFUCache();

    void
    access(uint64_t const key) override final;

    void
    remove(uint64_t const key) override final;

    size_t
    size() const override final;

    TotalEvictionOrderCacheIterator<ListNode const *>
    begin() const override final;

    TotalEvictionOrderCacheIterator<ListNode const *>
    end() const override final;

protected:
    friend LFUIterator;
    // Map keys to frequency.
    std::unordered_map<uint64_t, uint64_t> map_;
    // Map frequency to LRU Lists.
    std::map<uint64_t, LRUCache> list_;
};
