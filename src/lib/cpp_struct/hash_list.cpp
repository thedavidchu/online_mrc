#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>

#include "cpp_struct/hash_list.hpp"
#include "logger/logger.h"

constexpr bool DEBUG = false;

void
HashList::append(struct ListNode *node)
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

struct ListNode *
HashList::extract_list_only(uint64_t const key)
{
    LOGGER_TRACE("extract(%zu)", key);
    validate();
    auto it = map_.find(key);
    if (it == map_.end()) {
        validate();
        return nullptr;
    }
    struct ListNode *const n = it->second;
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
HashList::append_list_only(struct ListNode *node)
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

HashList::HashList() {}

HashList::~HashList()
{
    struct ListNode *next = nullptr;
    for (auto p = head_; p != nullptr; p = next) {
        next = p->r;
        free(p);
    }
}

bool
HashList::validate()
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
HashList::debug_print()
{
    std::cout << "Map: ";
    for (auto [k, p] : map_) {
        std::cout << k << ": " << p << ", ";
    }
    std::cout << std::endl;

    std::cout << "Head: " << head_ << ", Tail: " << tail_ << std::endl;
    std::cout << "HashList: ";
    for (auto p = head_; p != nullptr; p = p->r) {
        std::cout << p << ": " << p->key << ", ";
    }
    std::cout << std::endl;
}

ListNodeIterator
HashList::begin() const
{
    return ListNodeIterator{head_};
}

ListNodeIterator
HashList::end() const
{
    return ListNodeIterator{nullptr};
}

struct ListNode *
HashList::extract(uint64_t const key)
{
    LOGGER_TRACE("extract(%zu)", key);
    validate();
    auto it = map_.find(key);
    if (it == map_.end()) {
        validate();
        return nullptr;
    }
    struct ListNode *const n = it->second;
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
HashList::remove(uint64_t const key)
{
    auto n = extract(key);
    std::free(n);
    return n != nullptr;
}

void
HashList::access(uint64_t const key)
{
    LOGGER_TRACE("access(%zu)", key);
    validate();
    struct ListNode *r = extract_list_only(key);
    if (r == nullptr) {
        validate();
        r = (struct ListNode *)std::malloc(sizeof(struct ListNode));
        if (r == nullptr) {
            std::perror(strerror(errno));
            std::exit(1);
        }
        *r = ListNode{key, nullptr, nullptr};
        append(r);
    } else {
        append_list_only(r);
    }
    validate();
}

struct ListNode const *
HashList::get(uint64_t const key)
{
    LOGGER_TRACE("get(%zu)", key);
    auto r = map_.find(key);
    if (r == map_.end()) {
        return nullptr;
    }
    return r->second;
}

bool
HashList::contains(uint64_t const key) const
{
    return map_.contains(key);
}

struct ListNode *
HashList::extract_head()
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

size_t
HashList::size() const
{
    return map_.size();
}
