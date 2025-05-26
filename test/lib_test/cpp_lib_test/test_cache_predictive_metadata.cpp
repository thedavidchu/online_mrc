
#include "cpp_lib/cache_predictive_metadata.hpp"
#include "cpp_lib/format_measurement.hpp"
#include <cassert>
#include <cstdlib>

#define real_assert(r)                                                         \
    do {                                                                       \
        if (!(r)) {                                                            \
            assert((r));                                                       \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

static bool
test_which_eviction_queue()
{
    WhichEvictionQueue q;
    // Normal test
    real_assert(q.str() == format_binary((uint64_t)0));
    q.set_ttl();
    real_assert(q.str() == format_binary((uint64_t)1));
    q.set_lru();
    real_assert(q.str() == format_binary((uint64_t)3));
    q.unset_ttl();
    real_assert(q.str() == format_binary((uint64_t)2));
    q.unset_lru();
    real_assert(q.str() == format_binary((uint64_t)0));
    return true;
}

int
main()
{
    real_assert(test_which_eviction_queue());
    return 0;
}
