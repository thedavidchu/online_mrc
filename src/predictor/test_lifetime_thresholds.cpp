#include "cpp_lib/duration.hpp"
#include "lib/lifetime_thresholds.hpp"

#include <cmath>
#include <glib.h>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>

template <typename A, typename B, typename C>
static void
print_triple(std::tuple<A, B, C> const &p)
{
    std::cout << "{" << std::get<0>(p) << "," << std::get<1>(p) << ","
              << std::get<2>(p) << "}" << std::endl;
}

static bool
test_empty()
{
    LifeTimeThresholds t(0.25, 0.75);

    auto r = t.thresholds();
    g_assert_cmpfloat(r.first, ==, 0);
    g_assert_cmpfloat(r.second, ==, INFINITY);
    r = t.thresholds();
    g_assert_cmpfloat(r.first, ==, 0);
    g_assert_cmpfloat(r.second, ==, INFINITY);
    r = t.thresholds();
    g_assert_cmpfloat(r.first, ==, 0);
    g_assert_cmpfloat(r.second, ==, INFINITY);
    r = t.thresholds();
    g_assert_cmpfloat(r.first, ==, 0);
    g_assert_cmpfloat(r.second, ==, INFINITY);

    return true;
}

static bool
test_thresholds(bool const debug = false)
{
    LifeTimeThresholds t(0.25, 0.75);

    for (uint64_t i = 1; i < 100; ++i) {
        t.register_cache_eviction(i, 1, 0);
        t.refresh_thresholds();
        auto r = t.thresholds();
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
    LifeTimeThresholds t{0.25,
                         0.75,
                         /*refresh_period_ms=*/Duration::HOUR,
                         /*decay=*/1.0};
    std::tuple<double, double, bool> r;

    // Sample from 1..=100 and make sure the thresholds change at the
    // expected times.
    for (size_t i = 0; i < 1000 - 1; ++i) {
        t.register_cache_eviction(i % 100 + 1, 1, 0);
        auto [lo_t, hi_t, updated] = t.get_updated_thresholds(0);
        g_assert_cmpfloat(lo_t, ==, 0);
        g_assert_cmpfloat(hi_t, ==, INFINITY);
    }
    // We may only change the threshold once per hour.
    t.register_cache_eviction(100, 1, Duration::HOUR);
    r = t.get_updated_thresholds(Duration::HOUR);
    if (debug) {
        print_triple(r);
    }
    g_assert_cmpfloat(std::get<0>(r), ==, 25);
    g_assert_cmpfloat(std::get<1>(r), ==, 75);

    // Sample from a different population; the thresholds won't change
    // until expected.
    for (size_t i = 0; i < 1000 - 1; ++i) {
        t.register_cache_eviction(i % 100 + 101, 1, Duration::HOUR);
        auto [lo_t, hi_t, updated] = t.get_updated_thresholds(Duration::HOUR);
        g_assert_cmpfloat(lo_t, ==, 25);
        g_assert_cmpfloat(hi_t, ==, 75);
    }

    // It is only now that the thresholds change.
    t.register_cache_eviction(200, 1, 2 * Duration::HOUR);
    r = t.get_updated_thresholds(2 * Duration::HOUR);
    if (debug) {
        print_triple(r);
    }
    g_assert_cmpfloat(std::get<0>(r), ==, 50);
    g_assert_cmpfloat(std::get<1>(r), ==, 150);

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
