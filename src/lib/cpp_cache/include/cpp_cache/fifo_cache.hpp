#pragma once

#include "cpp_cache/total_eviction_ordered_cache.hpp"
#include "cpp_struct/hash_list.hpp"

#include <cstddef>
#include <cstdint>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class FIFOIterator : public TotalEvictionOrderCacheIterator<ListNode const *> {
public:
    FIFOIterator(ListNode const *const node)
        : node_(node)
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
        node_ = node_->r;
    }

    bool
    operator==(FIFOIterator const &rhs) const
    {
        return node_ == rhs.node_;
    }

    bool
    operator!=(FIFOIterator const &rhs) const
    {
        return node_ != rhs.node_;
    }

private:
    ListNode const *node_;
};

class FIFOCache : TotalEvictionOrderCache<ListNode const *> {
public:
    FIFOCache()
        : TotalEvictionOrderCache<ListNode const *>()
    {
    }

    void
    access(uint64_t const key) override final
    {
        if (!list_.contains(key)) {
            list_.access(key);
        }
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
        return FIFOIterator{*list_.begin()};
    }

    TotalEvictionOrderCacheIterator<ListNode const *>
    end() const override final
    {
        return FIFOIterator{*list_.end()};
    }

private:
    HashList list_;
};
