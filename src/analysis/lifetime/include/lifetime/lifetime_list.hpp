#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <unordered_map>

#include "cache_metadata/cache_access.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

/// @brief  Compress two 32-bit numbers into a 64-bit.
/// @todo   Convert float32 maybe?
uint64_t
compress(uint32_t time_ms, uint32_t size_bytes)
{
    return (uint64_t)time_ms << 32 | size_bytes;
}

struct LifetimeListNode {
    uint64_t key;
    uint64_t last_access_time;
    uint64_t size;
    struct LifetimeListNode *l;
    struct LifetimeListNode *r;
};

class LifetimeList {
private:
    void
    append(struct LifetimeListNode *node);

    struct LifetimeListNode *
    extract_list_only(uint64_t const key);

    void
    append_list_only(struct LifetimeListNode *node);

public:
    LifetimeList();

    ~LifetimeList();

    struct LifetimeListNode const *
    begin() const;

    struct LifetimeListNode const *
    end() const;

    bool
    validate();

    void
    debug_print();

    struct LifetimeListNode *
    extract(uint64_t const key);

    bool
    remove(uint64_t const key);

    /// @brief  Do the business of accessing an object in the cache.
    void
    access(CacheAccess const &access);

    struct LifetimeListNode const *
    get(uint64_t const key);

    struct LifetimeListNode *
    remove_head();

    /// Save the histogram in the format {lifetime: u32, cache-size: u32, count:
    /// }
    void
    save_histogram(std::string const &path)
    {
        std::ofstream fs(path,
                         std::ios::out | std::ios::binary | std::ios::app);
        for (auto [stats, cnt] : histogram_) {
            fs.write((char *)&stats.lifetime_ms, sizeof(stats.lifetime_ms));
            fs.write((char *)&stats.size_bytes, sizeof(stats.size_bytes));
            fs.write((char *)&cnt, sizeof(cnt));
        }
        fs.close();
    }

    std::unordered_map<uint64_t, struct LifetimeListNode *const> map_;
    std::unordered_map<uint64_t, uint64_t> histogram_;
    struct LifetimeListNode *head_ = nullptr;
    struct LifetimeListNode *tail_ = nullptr;
};
