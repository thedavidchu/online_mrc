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

class ListNodeIterator {
public:
    ListNodeIterator(ListNode const *node)
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
    operator==(ListNodeIterator const &rhs) const
    {
        return node_ == rhs.node_;
    }

    bool
    operator!=(ListNodeIterator const &rhs) const
    {
        return node_ != rhs.node_;
    }

private:
    struct ListNode const *node_;
};

/// @brief  A hash table indexed doubly linked list.
/// @example    Here is an example of a linked list.
///         |--------|    |--------|    |--------|
///         | node_0 |    | node_1 |    | node_2 |
///         \ key: 0 |    | key: 1 |    | key: 2 |
///         \ l: nil |<---| l: n_0 |<---| l: n_1 |
///         \ r: n_1 |--->| r: n_2 |--->| r: nil |
///         |--------|    |--------|    |--------|
///              ^                          ^
///              |                          |
///             HEAD                      TAIL (append)
class HashList {
private:
    /// @brief  Attach node to the tail.
    void
    append(struct ListNode *node);

    struct ListNode *
    extract_list_only(uint64_t const key);

    void
    append_list_only(struct ListNode *node);

public:
    HashList();

    ~HashList();

    ListNodeIterator
    begin() const;

    ListNodeIterator
    end() const;

    bool
    validate();

    void
    debug_print();

    /// @brief  Extract and return an owning pointer to a node.
    struct ListNode *
    extract(uint64_t const key);

    /// @brief  Extract and free a node.
    bool
    remove(uint64_t const key);

    /// @brief  Add or move a node to the tail of the queue.
    void
    access(uint64_t const key);

    /// @brief  Get an immutable view of a node.
    struct ListNode const *
    get(uint64_t const key);

    bool
    contains(uint64_t const key) const;

    size_t
    size() const;

    struct ListNode *
    extract_head();

    std::unordered_map<uint64_t, struct ListNode *const> map_;
    struct ListNode *head_ = nullptr;
    struct ListNode *tail_ = nullptr;
};
