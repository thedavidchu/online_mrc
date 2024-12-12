#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "arrays/array_size.h"
#include "cache/sieve_cache.hpp"
#include "logger/logger.h"
#include "test/mytester.h"

// NOTE This is the trace shown on the SIEVE website. Or at least, it is
//      one of the possible traces that causes the behaviour seen on the
//      SIEVE website.
//      Source: https://cachemon.github.io/SIEVE-website/
std::uint64_t trace[] = {
    'A',
    'A',
    'B',
    'B',
    'C',
    'D',
    'E',
    'F',
    'G',
    'G',
    'H',
    'A',
    'D',
    'I',
    'B',
    'J',
};
std::string soln[] = {
    "||",
    "|A|",
    "|A|",
    "|B|A|",
    "|B|A|",
    "|C|B|A|",
    "|D|C|B|A|",
    "|E|D|C|B|A|",
    "|F|E|D|C|B|A|",
    "|G|F|E|D|C|B|A|",
    "|G|F|E|D|C|B|A|",
    "|H|G|F|E|D|C|B|A|",
    "|H|G|F|E|D|C|B|A|",
    "|H|G|F|E|D|C|B|A|",
    "|I|H|G|F|E|D|B|A|",
    "|I|H|G|F|E|D|B|A|",
    "|J|I|H|G|F|E|B|A|",
};

std::string
sieve_print(SieveCache &cache)
{
    std::string s;
    for (auto key : cache.get_keys_in_eviction_order()) {
        // NOTE We know that the key is a character.
        s = std::string(1, (char)key) + "|" + s;
    }
    if (s == "") {
        return "||";
    }
    return "|" + s;
}

bool
simple_test()
{
    std::vector<std::string> my_soln;
    SieveCache cache(7);

    std::string s = sieve_print(cache);
    my_soln.push_back(s);
    for (auto i : trace) {
        cache.access_item(i);
        s = sieve_print(cache);
        my_soln.push_back(s);
    }

    // Print my solution
    for (auto s : my_soln) {
        std::cout << s << std::endl;
    }

    assert(my_soln.size() == ARRAY_SIZE(soln));
    for (std::size_t i = 0; i < my_soln.size(); ++i) {
        if (my_soln[i] != soln[i]) {
            LOGGER_ERROR("mismatching strings at %zu: got '%s', expecting '%s'",
                         i,
                         my_soln[i].c_str(),
                         soln[i].c_str());
        }
    }

    return 0;
}

int
main()
{
    ASSERT_FUNCTION_RETURNS_TRUE(simple_test());
    return 0;
}
