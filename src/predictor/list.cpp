#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>

#include "list/list.hpp"
#include "logger/logger.h"

void
List::append(struct ListNode *node)
{
    LOGGER_INFO("append(%zu)", node->key);
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

List::List() {}

List::~List()
{
    struct ListNode *next = nullptr;
    for (auto p = head_; p != nullptr; p = next) {
        next = p->r;
        free(p);
    }
}

bool
List::validate()
{
    bool const expensive = true;
    // Sanity checks
    bool r = false;
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

    if (expensive) {
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
    }

    return true;
}

void
List::print()
{
    std::cout << "Map: ";
    for (auto [k, p] : map_) {
        std::cout << k << ": " << p << ", ";
    }
    std::cout << std::endl;

    std::cout << "Head: " << head_ << ", Tail: " << tail_ << std::endl;
    std::cout << "List: ";
    for (auto p = head_; p != nullptr; p = p->r) {
        std::cout << p << ": " << p->key << ", ";
    }
    std::cout << std::endl;
}

struct ListNode *
List::extract(uint64_t const key)
{
    LOGGER_INFO("extract(%zu)", key);
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
    map_.erase(key);
    validate();
    // Reset internal pointers so we don't dangle invalid pointers.
    n->l = n->r = nullptr;
    return n;
}

void
List::access(uint64_t const key)
{
    LOGGER_INFO("access(%zu)", key);
    validate();
    struct ListNode *r = extract(key);
    if (r == nullptr) {
        validate();
        r = (struct ListNode *)std::malloc(sizeof(struct ListNode));
        if (r == nullptr) {
            std::perror(strerror(errno));
            std::exit(1);
        }
        *r = ListNode{key, nullptr, nullptr};
    }
    append(r);
    validate();
}

struct ListNode *
List::remove_head()
{
    LOGGER_INFO("remove_head() -> %p(%s)",
                head_,
                head_ ? std::to_string(head_->key).c_str() : "?");
    validate();
    if (head_ == nullptr) {
        validate();
        return nullptr;
    }
    return extract(head_->key);
}
