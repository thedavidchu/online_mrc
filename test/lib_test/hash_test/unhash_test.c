/** @brief  This file tests unhashing values. */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "hash/splitmix64.h"
#include "logger/logger.h"
#include "test/mytester.h"

bool
test_unhash(uint64_t (*const hash)(uint64_t),
            uint64_t (*const unhash)(uint64_t))
{
    uint64_t x = 0;
    for (uint64_t i = 0; i < 100; ++i) {
        // NOTE We can rely on these functions being pure functions
        //      (i.e. given an input, it will deterministically provide
        //      a repeatable output). However, we cannot assume these
        //      are one-to-one mappings between key and hash. Therefore,
        //      we cannot know whether the unhash function with map the
        //      hash back to the original key or a new value.
        //
        //      Example:
        //              hash()      unhash()        hash()      unhash()
        //          key -----> hash ~~~~~~~> unhash -----> hash ~~~~~~~> unhash
        uint64_t const x_hashed = hash(x);
        uint64_t const x_unhashed = unhash(x_hashed);
        // NOTE Let us assume that the 'hash' and 'unhash' functions are
        //      deterministic. Therefore, we only need a single test to
        //      check if our 'unhash' function provides a value that,
        //      when hashed, will yield the hash again. Therefore, we
        //      only need to check that the hash of the key matches the
        //      hash of the unhashed.
        //      For example, let hash(x) : x -> 0 and let unhash(x) : x -> 1.
        //      Then, we will have the following chain:
        //      1. key: 3.14 -- hash(3.14) => 0
        //      2. hash: 0 -- unhash(0) => 1
        //      3. unhash: 1 -- hash(1) => 0
        //      4. hash: 0 -- unhash(0) => 1
        LOGGER_DEBUG("key: %" PRIu64 " | hash: %" PRIu64 " | unhash: %" PRIu64
                     " | hash(unhash): %" PRIu64,
                     x,
                     x_hashed,
                     x_unhashed,
                     hash(x_unhashed));
        g_assert_cmpuint(x_hashed, ==, hash(x_unhashed));
        x = 3 * x + 1;
    }
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(
        test_unhash(splitmix64_hash, reverse_splitmix64_hash));
    return 0;
}
