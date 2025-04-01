#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

struct ListNode {
    uint64_t key;
    struct ListNode *l;
    struct ListNode *r;
};

class List {
private:
    void
    append(struct ListNode *node);

    struct ListNode *
    extract_list_only(uint64_t const key);

    void
    append_list_only(struct ListNode *node);

public:
    List();

    ~List();

    struct ListNode const *
    begin() const;

    struct ListNode const *
    end() const;

    bool
    validate();

    void
    debug_print();

    struct ListNode *
    extract(uint64_t const key);

    bool
    remove(uint64_t const key);

    void
    access(uint64_t const key);

    struct ListNode const *
    get(uint64_t const key);

    size_t
    size() const {
        return map_.size();
    }

    struct ListNode *
    remove_head();

    std::unordered_map<uint64_t, struct ListNode *const> map_;
    struct ListNode *head_ = nullptr;
    struct ListNode *tail_ = nullptr;
};
