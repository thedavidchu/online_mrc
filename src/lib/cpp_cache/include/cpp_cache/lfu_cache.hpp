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
    LFUIterator(LFUCache const &cache)
        : cache_(cache)
    {
    }

    ListNode const *
    operator*() const
    {
        return node_;
    }

    void
    operator++()
    {

        auto &r = cache_.list_.upper_bound(frq);
    }

    bool
    operator==(LFUIterator const &rhs) const
    {
        return idx == rhs.idx;
    }

    bool
    operator!=(LFUIterator const &rhs) const
    {
        return idx != rhs.idx;
    }

private:
    LFUCache const &cache_;
    uint64_t frq = 0;
    uint64_t idx = 0;
};

class LFUCache : TotalEvictionOrderCache<ListNode const *> {
public:
    LFUCache()
        : TotalEvictionOrderCache<ListNode const *>()
    {
    }

    void
    access(uint64_t const key) override final
    {
        list_.access(key);
    }

    void
    remove(uint64_t const key) override final
    {
        list_.remove(key);
    }

    size_t
    size() const override final
    {
        return list_.size();
    }

    TotalEvictionOrderCacheIterator<ListNode const *>
    begin() const override final
    {
        return LFUIterator{*list_.begin()};
    }

    TotalEvictionOrderCacheIterator<ListNode const *>
    end() const override final
    {
        return LFUIterator{*list_.end()};
    }

protected:
    friend LFUIterator;
    // Map keys to frequency.
    std::unordered_map<uint64_t, uint64_t> map_;
    // Map frequency to LRU Lists.
    std::map<uint64_t, LRUCache> list_;
};
