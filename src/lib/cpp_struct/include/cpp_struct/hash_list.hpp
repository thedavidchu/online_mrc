#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

struct ListNode {
    uint64_t key;
    ListNode *l;
    ListNode *r;

    void
    reset()
    {
        key = 0;
        l = r = nullptr;
    }

    /// @brief  Remove dangling pointers.
    void
    sanitize()
    {
        l = r = nullptr;
    }
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
    ListNode const *node_;
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
    void
    validate() const;

    void
    debug_print() const;

    /// @brief  Attach node to the tail.
    void
    append(ListNode *node, bool const add_to_map);

    /// @brief  Extract and return an owning pointer to a node.
    std::pair<std::unordered_map<uint64_t, ListNode *const>::iterator,
              ListNode *>
    extract(uint64_t const key);

public:
    HashList();
    ~HashList();

    // Delete the copy constructor. This is because my linked list has
    // no implicit copy constructor.
    HashList(HashList const &) = delete;
    // Delete the copy assignment operator. This is also because my
    // linked list has no implicit copy constructor.
    HashList &
    operator=(HashList const &) = delete;

    // This is required to be able to have a HashList as a value within
    // std::map. It is called a 'move constructor".
    HashList(HashList const &&src)
        : map_(src.map_),
          head_(src.head_),
          tail_(src.tail_)
    {
    }

    ListNodeIterator
    begin() const;

    ListNodeIterator
    end() const;

    /// @brief  Extract and free a node.
    bool
    remove(uint64_t const key);

    /// @brief  Add or move a node to the tail of the queue.
    void
    access(uint64_t const key);

    /// @brief  Get an immutable view of a node.
    ListNode const *
    get(uint64_t const key) const;

    bool
    contains(uint64_t const key) const;

    size_t
    size() const;

    ListNode *
    extract_head();

private:
    std::unordered_map<uint64_t, ListNode *const> map_;
    ListNode *head_ = nullptr;
    ListNode *tail_ = nullptr;
};
