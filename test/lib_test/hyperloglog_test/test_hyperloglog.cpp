#include "cpp_lib/util.hpp"
#include "hash/hash.h"
#include "hyperloglog/hyperloglog.hpp"
#include "logger/logger.h"
#include "math/doubles_are_equal.h"
#include "random/uniform_random.h"
#include "random/zipfian_random.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <unordered_set>

using uint64_t = std::uint64_t;

static inline bool
analyze(uint64_t const expected, uint64_t const got)
{
    LOGGER_INFO("expected: %zu, got: %zu => ratio: %f",
                expected,
                got,
                calculate_error(expected, got));
    // I chose 3% error, since 'test_zipfian(0.1)' has over 2% error.
    return doubles_are_close(expected, got, 0.03 * expected);
}

static bool
test_linear_growth()
{
    uint64_t const N = 1 << 20;
    HyperLogLog hll{1 << 13};
    for (uint64_t i = 0; i < N; ++i) {
        uint64_t h = Hash64Bit(i);
        hll.add(h);
    }
    return analyze(N, hll.count());
}

static bool
test_zipfian(double const skew)
{
    uint64_t const N = 1 << 20;
    HyperLogLog hll{1 << 13};
    std::unordered_set<uint64_t> set;
    ZipfianRandom zrng = {};
    ZipfianRandom__init(&zrng, N, skew, 0);
    for (uint64_t i = 0; i < N; ++i) {
        // Track the real number, regardless of hash collisions!
        uint64_t x = ZipfianRandom__next(&zrng);
        set.insert(x);
        uint64_t h = Hash64Bit(x);
        hll.add(h);
    }
    return analyze(set.size(), hll.count());
}

static bool
test_uniform()
{
    uint64_t const N = 1 << 20;
    HyperLogLog hll{1 << 13};
    std::unordered_set<uint64_t> set;
    UniformRandom urng = {};
    UniformRandom__init(&urng, 0);
    for (uint64_t i = 0; i < N; ++i) {
        // Track the real number, regardless of hash collisions!
        uint64_t x = UniformRandom__next_uint64(&urng);
        set.insert(x);
        uint64_t h = Hash64Bit(x);
        hll.add(h);
    }
    return analyze(set.size(), hll.count());
}

int
main()
{
    LOGGER_LEVEL = LOGGER_LEVEL_DEBUG;
    assert(test_linear_growth());
    assert(test_zipfian(0.0));
    assert(test_zipfian(0.1));
    assert(test_zipfian(0.5));
    assert(test_zipfian(0.9));
    assert(test_zipfian(0.99));
    assert(test_uniform());
    return 0;
}
