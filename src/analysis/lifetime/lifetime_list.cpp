#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>

#include "cache_metadata/cache_access.hpp"
#include "lifetime/lifetime_list.hpp"
#include "logger/logger.h"

constexpr bool DEBUG = false;

void
LifetimeList::append(struct LifetimeListNode *node)
{
    LOGGER_TRACE("append(%zu)", node->key);
    validate();
    map_.emplace(node->key, node);
    if (tail_ == nullptr) {
        head_ = tail_ = node;
        node->l = node->r = nullptr;
        validate();
        return;
    }
    assert(head_ != nullptr && tail_->r == nullptr && map_.size() != 0);
    tail_->r = node;
    node->l = tail_;
    tail_ = node;
}

struct LifetimeListNode *
LifetimeList::extract_list_only(uint64_t const key)
{
    LOGGER_TRACE("extract(%zu)", key);
    validate();
    auto it = map_.find(key);
    if (it == map_.end()) {
        validate();
        return nullptr;
    }
    struct LifetimeListNode *const n = it->second;
    if (n->l != nullptr) {
        n->l->r = n->r;
    } else {
        head_ = n->r;
    }
    if (n->r != nullptr) {
        n->r->l = n->l;
    } else {
        tail_ = n->l;
    }
    validate();
    // Reset internal pointers so we don't dangle invalid pointers.
    n->l = n->r = nullptr;
    return n;
}

void
LifetimeList::append_list_only(struct LifetimeListNode *node)
{
    LOGGER_TRACE("append(%zu)", node->key);
    validate();
    if (tail_ == nullptr) {
        head_ = tail_ = node;
        node->l = node->r = nullptr;
        validate();
        return;
    }
    assert(head_ != nullptr && tail_->r == nullptr && map_.size() != 0);
    tail_->r = node;
    node->l = tail_;
    tail_ = node;
}

LifetimeList::LifetimeList() {}

LifetimeList::~LifetimeList()
{
    struct LifetimeListNode *next = nullptr;
    for (auto p = head_; p != nullptr; p = next) {
        next = p->r;
        free(p);
    }
}

bool
LifetimeList::validate()
{
    bool r = false;
    if (!DEBUG) {
        return true;
    }
    // Sanity checks
    switch (map_.size()) {
    case 0:
        r = (head_ == nullptr && tail_ == nullptr);
        assert(r);
        if (!r) {
            return false;
        }
        break;
    case 1:
        r = (head_ == tail_);
        assert(r);
        if (!r) {
            return false;
        }
        break;
    default:
        r = (head_ != tail_ && head_ != nullptr && tail_ != nullptr);
        assert(r);
        if (!r) {
            return false;
        }
        break;
    }

    // Check internal consistency of the data structure.
    size_t cnt = 0;
    for (auto p = head_; p != nullptr; p = p->r) {
        assert(map_.count(p->key));
        assert(map_.at(p->key) == p);
        ++cnt;
        if (p->l != nullptr) {
            assert(p->l->r == p);
        } else {
            assert(p == head_);
        }
        if (p->r != nullptr) {
            assert(p->r->l == p);
        } else {
            assert(tail_ == p);
        }
    }
    assert(cnt == map_.size());

    return true;
}

void
LifetimeList::debug_print()
{
    std::cout << "Map: ";
    for (auto [k, p] : map_) {
        std::cout << k << ": " << p << ", ";
    }
    std::cout << std::endl;

    std::cout << "Head: " << head_ << ", Tail: " << tail_ << std::endl;
    std::cout << "LifetimeList: ";
    for (auto p = head_; p != nullptr; p = p->r) {
        std::cout << p << ": " << p->key << ", ";
    }
    std::cout << std::endl;
}

struct LifetimeListNode const *
LifetimeList::begin() const
{
    return head_;
}

struct LifetimeListNode const *
LifetimeList::end() const
{
    return nullptr;
}

struct LifetimeListNode *
LifetimeList::extract(uint64_t const key)
{
    LOGGER_TRACE("extract(%zu)", key);
    validate();
    auto it = map_.find(key);
    if (it == map_.end()) {
        validate();
        return nullptr;
    }
    struct LifetimeListNode *const n = it->second;
    if (n->l != nullptr) {
        n->l->r = n->r;
    } else {
        head_ = n->r;
    }
    if (n->r != nullptr) {
        n->r->l = n->l;
    } else {
        tail_ = n->l;
    }
    map_.erase(it);
    validate();
    // Reset internal pointers so we don't dangle invalid pointers.
    n->l = n->r = nullptr;
    return n;
}

bool
LifetimeList::remove(uint64_t const key)
{
    auto n = extract(key);
    std::free(n);
    return n != nullptr;
}

void
LifetimeList::access(CacheAccess const &access)
{
    LOGGER_TRACE("access(%zu)", access.key);
    validate();
    struct LifetimeListNode *r = extract_list_only(access.key);
    if (r == nullptr) {
        validate();
        r = (struct LifetimeListNode *)std::malloc(
            sizeof(struct LifetimeListNode));
        if (r == nullptr) {
            std::perror(strerror(errno));
            std::exit(1);
        }
        *r = LifetimeListNode{access.key,
                              access.timestamp_ms,
                              access.size_bytes,
                              nullptr,
                              nullptr};
        append(r);
    } else {
        append_list_only(r);
    }
    validate();
}

struct LifetimeListNode const *
LifetimeList::get(uint64_t const key)
{
    LOGGER_TRACE("get(%zu)", key);
    auto r = map_.find(key);
    if (r == map_.end()) {
        return nullptr;
    }
    return r->second;
}

struct LifetimeListNode *
LifetimeList::remove_head()
{
    LOGGER_TRACE("remove_head() -> %p(%s)",
                 head_,
                 head_ ? std::to_string(head_->key).c_str() : "?");
    validate();
    if (head_ == nullptr) {
        validate();
        return nullptr;
    }
    return extract(head_->key);
}
