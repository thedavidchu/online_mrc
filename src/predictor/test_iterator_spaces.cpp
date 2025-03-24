
#include "lib/iterator_spaces.hpp"

#include <glib.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

static bool
test_semilogspace(bool const verbose = false)
{
    uint64_t const mem_16_gib = 16 * (uint64_t)1 << 30;
    std::vector<size_t> r;
    r = semilogspace(mem_16_gib, 10);
    if (verbose) {
        print_vector(r);
    }
    g_assert_cmpuint(r.front(), ==, ((mem_16_gib >> 4) / std::sqrt(2)));
    g_assert_cmpuint(r.back(), ==, mem_16_gib);

    r = semilogspace(mem_16_gib, 11);
    if (verbose) {
        print_vector(r);
    }
    g_assert_cmpuint(r.front(), ==, (mem_16_gib >> 5));
    g_assert_cmpuint(r.back(), ==, mem_16_gib);
    return true;
}

int
main()
{
    test_semilogspace();
    return 0;
}
