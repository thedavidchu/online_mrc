#include "lib/lifetime_thresholds.hpp"

#include <glib.h>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>

// An hour represented in milliseconds.
static constexpr uint64_t HOUR = 1000 * 3600;

template <typename A, typename B>
static void
print_pair(std::pair<A, B> const &p)
{
    std::cout << "{" << p.first << "," << p.second << "}" << std::endl;
}

static bool
test_empty()
{
    LifeTimeThresholds t(0.25, 0.75);
    std::pair<uint64_t, uint64_t> r;

    r = t.thresholds();
    g_assert_cmpuint(r.first, ==, 0);
    g_assert_cmpuint(r.second, ==, UINT64_MAX);
    r = t.thresholds();
    g_assert_cmpuint(r.first, ==, 0);
    g_assert_cmpuint(r.second, ==, UINT64_MAX);
    r = t.thresholds();
    g_assert_cmpuint(r.first, ==, 0);
    g_assert_cmpuint(r.second, ==, UINT64_MAX);
    r = t.thresholds();
    g_assert_cmpuint(r.first, ==, 0);
    g_assert_cmpuint(r.second, ==, UINT64_MAX);

    return true;
}

static bool
test_thresholds(bool const debug = false)
{
    LifeTimeThresholds t(0.25, 0.75);
    std::pair<uint64_t, uint64_t> r;

    for (uint64_t i = 1; i < 100; ++i) {
        t.register_cache_eviction(i, 1, 0);
        t.refresh_thresholds();
        r = t.thresholds();
        if (debug) {
            std::cout << "> " << i << ": " << r.first << ", " << r.second
                      << std::endl;
        }
    }

    return true;
}

static bool
test_refresh(bool const debug = false)
{
    LifeTimeThresholds t(0.25, 0.75);
    std::pair<uint64_t, uint64_t> r;

    // Sample from 1..=100 and make sure the thresholds change at the
    // expected times.
    for (size_t i = 0; i < 1000 - 1; ++i) {
        t.register_cache_eviction(i % 100 + 1, 1, 0);
        r = t.thresholds();
        g_assert_cmpuint(r.first, ==, 0);
        g_assert_cmpuint(r.second, ==, UINT64_MAX);
    }
    // We may only change the threshold once per hour.
    t.register_cache_eviction(100, 1, HOUR);
    r = t.thresholds();
    if (debug) {
        print_pair(r);
    }
    g_assert_cmpuint(r.first, ==, 25);
    g_assert_cmpuint(r.second, ==, 75);

    // Sample from a different population; the thresholds won't change
    // until expected.
    for (size_t i = 0; i < 1000 - 1; ++i) {
        t.register_cache_eviction(i % 100 + 101, 1, HOUR);
        r = t.thresholds();
        g_assert_cmpuint(r.first, ==, 25);
        g_assert_cmpuint(r.second, ==, 75);
    }

    // It is only now that the thresholds change.
    t.register_cache_eviction(200, 1, 2 * HOUR);
    r = t.thresholds();
    if (debug) {
        print_pair(r);
    }
    g_assert_cmpuint(r.first, ==, 50);
    g_assert_cmpuint(r.second, ==, 150);

    return true;
}

int
main()
{
    bool r = 0;
    r = test_empty();
    assert(r);
    r = test_thresholds(false);
    assert(r);
    r = test_refresh(false);
    assert(r);
    return 0;
}
