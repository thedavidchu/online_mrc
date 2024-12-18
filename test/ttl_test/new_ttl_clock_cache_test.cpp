#include <cstdint>
#include <iostream>
#include <vector>

#include "ttl_cache/new_ttl_clock_cache.hpp"

void
print_keys(std::vector<std::uint64_t> &keys)
{
    for (auto k : keys) {
        std::cout << k << ",";
    }
    std::cout << std::endl;
}

int
main()
{
    std::uint64_t trace[] = {0, 1, 2, 3, 0, 1, 2, 3, 4};
    NewTTLClockCache cache(4);

    std::uint64_t time = 0;
    for (auto x : trace) {
        cache.access_item(time++, x, 0);
        cache.validate(0);
        // std::vector<std::uint64_t> keys = cache.get_keys();
        // print_keys(keys);
    }

    return 0;
}
