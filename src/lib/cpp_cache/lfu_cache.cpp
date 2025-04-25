#include "cpp_cache/lfu_cache.hpp"

LFUIterator::LFUIterator(LFUCache const &cache)
    : cache_(cache)
{
}

ListNode const *
LFUIterator::operator*() const
{
    return node_;
}

void
LFUIterator::operator++()
{
    if (node_ == nullptr) {
        auto iter = cache_.list_.upper_bound(frq);
        if (iter == cache_.list_.end()) {
            node_ = nullptr;
            return;
        }
        frq = iter->first;
        node_ = *iter->second.begin();
    } else {
        node_ = node_->r;
    }
}

bool
LFUIterator::operator==(LFUIterator const &rhs) const
{
    return node_ == rhs.node_;
}

bool
LFUIterator::operator!=(LFUIterator const &rhs) const
{
    return node_ != rhs.node_;
}

LFUCache::LFUCache()
    : TotalEvictionOrderCache<ListNode const *>()
{
}

void
LFUCache::access(uint64_t const key)
{
    if (map_.contains(key)) {
        auto old_frq = map_[key];
        auto &lru_cache = list_[old_frq];
        lru_cache.remove(key);
        auto &new_lru_cache = list_[old_frq + 1];
        new_lru_cache.access(key);
        ++map_[key];
    } else {
        map_[key] = 1;
        list_[1].access(key);
    }
}

void
LFUCache::remove(uint64_t const key)
{
    if (map_.contains(key)) {
        list_[map_[key]].remove(key);
        map_.erase(key);
    }
}

size_t
LFUCache::size() const
{
    return list_.size();
}

TotalEvictionOrderCacheIterator<ListNode const *>
LFUCache::begin() const
{
    return LFUIterator{*this};
}

TotalEvictionOrderCacheIterator<ListNode const *>
LFUCache::end() const
{
    return LFUIterator{*this};
}
