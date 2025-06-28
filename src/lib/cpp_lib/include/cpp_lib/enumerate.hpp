/** @brief  Python-like enumerate() in C++17.
 *  @note   Don't quiz me on the details of the template stuff.
 * Source: https://www.reedbeta.com/blog/python-like-enumerate-in-cpp17/
 */
#pragma once

#include <cstddef>
#include <iterator>
#include <tuple>

using size_t = std::size_t;

template <typename T,
          typename TIter = decltype(std::begin(std::declval<T>())),
          typename = decltype(std::end(std::declval<T>())),
          typename TIdx = decltype(std::size(std::declval<T>()))>
constexpr auto
enumerate(T &&iterable)
{
    struct iterator {
        TIdx i;
        TIter iter;
        bool
        operator!=(const iterator &other) const
        {
            return iter != other.iter;
        }
        void
        operator++()
        {
            ++i;
            ++iter;
        }
        auto
        operator*() const
        {
            return std::tie(i, *iter);
        }
    };
    struct iterable_wrapper {
        T iterable;
        auto
        begin()
        {
            return iterator{static_cast<TIdx>(0), std::begin(iterable)};
        }
        auto
        end()
        {
            // We compare the iterables, not the index. That's why this
            // code originally put a 0 into the index. I changed it
            // (without understanding the template code) to make it more
            // intuitive.
            return iterator{std::size(iterable), std::end(iterable)};
        }
    };
    return iterable_wrapper{std::forward<T>(iterable)};
}
