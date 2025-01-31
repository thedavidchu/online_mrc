#pragma once
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

class List {
private:
    void
    append(struct ListNode *node);

public:
    List();

    ~List();

    bool
    validate();

    void
    print();

    struct ListNode *
    extract(uint64_t const key);

    void
    access(uint64_t const key);

    struct ListNode *
    remove_head();

    std::unordered_map<uint64_t, struct ListNode *const> map_;
    struct ListNode *head_ = nullptr;
    struct ListNode *tail_ = nullptr;
};
