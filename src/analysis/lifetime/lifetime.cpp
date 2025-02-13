#include <cassert>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
// Ugh, the amount of time you spend in C/C++ twiddling with the build
// system is quite incredible.
#include "cache_metadata/cache_access.hpp"
#include "lifetime/lifetime_list.hpp"

int
main(int argc, char *argv[])
{
    assert(argc == 1 && argv != NULL);
    CacheAccessTrace trace("../../data/src2.bin", TRACE_FORMAT_KIA);
    LifetimeList cache{};
    for (size_t i = 0; i < trace.size(); ++i) {
        auto item = trace.get(i);
        cache.access(item);
    }
    cache.save_histogram("lifetime-histogram.bin");
    return 0;
}
