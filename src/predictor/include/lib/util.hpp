#pragma once

#include <cassert>
#include <cstring>
#include <map>

template <typename K, typename V>
static inline auto
find_multimap_kv(std::multimap<K, V> &me, K const k, V const v)
{
    for (auto it = me.lower_bound(k); it != me.upper_bound(k); ++it) {
        if (it->second == v) {
            return it;
        }
    }
    return me.end();
}

/// @brief  Remove a specific <key, value> pair from a multimap,
///         specifically the first instance of that pair.
template <typename K, typename V>
static inline bool
remove_multimap_kv(std::multimap<K, V> &me, K const k, V const v)
{
    auto it = find_multimap_kv(me, k, v);
    if (it != me.end()) {
        me.erase(it);
        return true;
    }
    return false;
}

static inline bool
atob_or_panic(char const *const a)
{
    if (strcmp(a, "true") == 0) {
        return true;
    }
    if (strcmp(a, "false") == 0) {
        return false;
    }
    assert(0 && "unexpected 'true' or 'false'");
}
