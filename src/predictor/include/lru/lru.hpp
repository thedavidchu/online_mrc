#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>

#include "logger/logger.h"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

struct ListNode {
    uint64_t key;
    struct ListNode *l;
    struct ListNode *r;
};

class LRU {
private:
    void
    append(struct ListNode *node)
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

public:
    LRU() {}

    ~LRU()
    {
        struct ListNode *next = nullptr;
        for (auto p = head_; p != nullptr; p = next) {
            next = p->r;
            free(p);
        }
    }

    bool
    validate()
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
    print()
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
    extract(uint64_t const key)
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
    access(uint64_t const key)
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
    remove_head()
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

    std::unordered_map<uint64_t, struct ListNode *const> map_;
    struct ListNode *head_ = nullptr;
    struct ListNode *tail_ = nullptr;
};

int
main()
{
    LRU l{};

    l.access(0);
    l.access(1);
    l.access(2);
    l.access(0);
    std::free(l.extract(0));
    std::free(l.extract(0));
    std::free(l.extract(0));
    std::free(l.extract(0));
    std::free(l.remove_head());
    std::free(l.remove_head());
    std::free(l.remove_head());
    std::free(l.remove_head());

    return 0;
}
