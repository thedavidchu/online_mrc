/** @brief  A C++ utility libary to supplement the STL.
 *  @note   Files that import this file should be build with the Meson
 *          'boost_dep'.
 */

#pragma once

#include "arrays/is_last.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

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

bool
atob_or_panic(char const *const a);

/// @brief  Split a string at any point where a delimiter is found.
std::vector<std::string>
string_split(std::string const &src, std::string const &delim);

static inline std::string
bool2str(bool const b)
{
    return b ? "true" : "false";
}

static inline double
calculate_error(double const x, double const y)
{
    return std::abs(x - y) / std::max(x, y);
}

template <typename T, typename S = T>
static inline std::string
vec2str(std::vector<T> const &vec,
        std::string const open = "[",
        std::string const close = "]",
        std::string const sep = ", ",
        bool const quote_value = false)
{
    std::stringstream ss;
    ss << open;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (quote_value) {
            ss << "\"";
        }
        ss << static_cast<S>(vec[i]);
        if (quote_value) {
            ss << "\"";
        }
        if (!is_last(i, vec.size())) {
            ss << sep;
        }
    }
    ss << close;
    return ss.str();
}

template <typename M>
static inline std::string
map2str(M const &map, bool const quote_value = false)
{
    std::stringstream ss;
    size_t i = 0;
    ss << "{";
    for (auto [k, v] : map) {
        ss << "\"" << k << "\": ";
        if (quote_value) {
            ss << "\"";
        }
        ss << v;
        if (quote_value) {
            ss << "\"";
        }
        if (!is_last(i++, map.size())) {
            ss << ", ";
        }
    }
    ss << "}";
    return ss.str();
}

template <typename A, typename B>
static inline std::string
pair2str(std::pair<A, B> const &pair,
         bool const quote_first = false,
         bool const quote_second = false)
{
    std::stringstream ss;
    std::string const first_quote = quote_first ? "\"" : "";
    std::string const second_quote = quote_second ? "\"" : "";

    ss << "[" << first_quote << pair.first << first_quote << ", ";
    ss << second_quote << pair.second << second_quote << "]";
    return ss.str();
}
