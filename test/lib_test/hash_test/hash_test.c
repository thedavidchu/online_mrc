#include <bits/stdint-uintn.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <glib.h>

#include "hash/MurmurHash3.h"
#include "hash/hash.h"
#include "logger/logger.h"
#include "test/mytester.h"

static bool
test_string_hash_to_uint32(void)
{
    char const *const str = "Hello, World!";
    uint32_t hash = 0;
    MurmurHash3_x86_32(str, strlen(str), 0, &hash);
    LOGGER_INFO("Hash %s = %" PRIu32, str, hash);
    g_assert_cmpuint(hash, ==, 592631239UL);
    return true;
}

static bool
test_uint64_hash_to_uint128(void)
{
    uint64_t input = 0;
    uint64_t hash[2] = {0, 0};

    MurmurHash3_x64_128(&input, sizeof(input), 0, &hash);
    LOGGER_INFO("Hash %" PRIu64 " = {%" PRIu64 ", %" PRIu64 "}",
                input,
                hash[0],
                hash[1]);
    g_assert_cmpuint(hash[0], ==, 2945182322382062539ULL);
    g_assert_cmpuint(hash[1], ==, 17462001654787800658ULL);

    // Test hash() wrapper
    g_assert_cmpuint(hash[0], ==, Hash64Bit(0));
    return true;
}

static bool
test_my_murmur_hash_wrappers(void)
{
    // NOTE I'm mostly checking to avoid segmentation faults.
    uint64_t input = 0;

    g_assert_cmpuint(Hash32Bit(input), ==, 1669671676UL);
    g_assert_cmpuint(Hash64Bit(input), ==, 2945182322382062539ULL);
    g_assert_cmpuint(Hash128Bit(input).hash[0], ==, 2945182322382062539ULL);
    g_assert_cmpuint(Hash128Bit(input).hash[1], ==, 17462001654787800658ULL);

    // NOTE The 64 bit hash is just the first 8 bytes of the 128 bit hash!
    g_assert_cmpuint(Hash64Bit(input), ==, Hash128Bit(input).hash[0]);

    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_string_hash_to_uint32());
    ASSERT_FUNCTION_RETURNS_TRUE(test_uint64_hash_to_uint128());
    ASSERT_FUNCTION_RETURNS_TRUE(test_my_murmur_hash_wrappers());
    return 0;
}
