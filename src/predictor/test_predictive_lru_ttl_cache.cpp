#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdlib.h>
#include <sys/types.h>

#include "cpp_cache/cache_access.hpp"
#include "lib/lifetime_cache.hpp"
#include "lib/predictive_lru_ttl_cache.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

bool
test_lru()
{
    PredictiveCache p(2, 0.0, 1.0, LifeTimeCacheMode::EvictionTime);
    CacheAccess accesses[] = {CacheAccess{0, 0, 1, 10},
                              CacheAccess{1, 1, 1, 10},
                              CacheAccess{2, 2, 1, 10}};
    // Test initial state.
    assert(p.size() == 0);
    assert(p.get(0) == nullptr);
    assert(p.get(1) == nullptr);
    assert(p.get(2) == nullptr);
    // Test first state.
    p.access(accesses[0]);
    assert(p.size() == 1);
    assert(p.get(0) != nullptr);
    assert(p.get(1) == nullptr);
    assert(p.get(2) == nullptr);

    // Test second state.
    p.access(accesses[1]);
    assert(p.size() == 2);
    assert(p.get(0) != nullptr);
    assert(p.get(1) != nullptr);
    assert(p.get(2) == nullptr);

    // Test third state.
    p.access(accesses[2]);
    assert(p.size() == 2);
    assert(p.get(0) == nullptr);
    assert(p.get(1) != nullptr);
    assert(p.get(2) != nullptr);

    return true;
}

bool
test_ttl()
{
    PredictiveCache p(2, 0.0, 1.0, LifeTimeCacheMode::EvictionTime);
    CacheAccess accesses[] = {CacheAccess{0, 0, 1, 1},
                              CacheAccess{1001, 1, 1, 10}};
    // Test initial state.
    assert(p.size() == 0);
    assert(p.get(0) == nullptr);
    assert(p.get(1) == nullptr);
    p.print();
    // Test first state.
    p.access(accesses[0]);
    assert(p.size() == 1);
    assert(p.get(0) != nullptr);
    assert(p.get(1) == nullptr);
    p.print();
    // Test second state.
    p.access(accesses[1]);
    assert(p.size() == 1);
    assert(p.get(0) == nullptr);
    assert(p.get(1) != nullptr);
    p.print();
    return true;
}

int
main()
{
    assert(test_lru());
    std::cout << "---\n";
    assert(test_ttl());
    std::cout << "OK!" << std::endl;
    return 0;
}
