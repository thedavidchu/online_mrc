#include <bits/stdint-uintn.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <glib.h>

#include "hash/MurmurHash3.h"
#include "hash/hash.h"
#include "hash/types.h"
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

    return true;
}

/// @brief  Check my hash wrappers to make sure they are consistent.
/// @note   I do not make any guarantees about which hash functions I
///         use. Moreover, I do not guarantee any specific relationship
///         between hash functions (e.g. I used to have it that the 64
///         bit hash function would be the first half of the 128 bit).
static bool
test_hash(void)
{
    uint64_t const zero = 0;
    g_assert_cmpuint(Hash32Bit(zero), ==, Hash32Bit(zero));
    g_assert_cmpuint(Hash64Bit(zero), ==, Hash64Bit(zero));
    g_assert_cmpuint(Hash128Bit(zero).hash[0], ==, Hash128Bit(zero).hash[0]);
    g_assert_cmpuint(Hash128Bit(zero).hash[1], ==, Hash128Bit(zero).hash[1]);

    // NOTE I am making sure that the hash functions are suitably good.
    //      This is probabilistically true; I just picked these numbers
    //      randomly, so I may need to change them in future.
    uint64_t const one = 1;
    uint64_t const two = 2;
    g_assert_cmpuint(Hash32Bit(one), !=, Hash32Bit(two));
    g_assert_cmpuint(Hash64Bit(one), !=, Hash64Bit(two));
    g_assert_cmpuint(Hash128Bit(one).hash[0], !=, Hash128Bit(two).hash[0]);
    g_assert_cmpuint(Hash128Bit(one).hash[1], !=, Hash128Bit(two).hash[1]);

    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_string_hash_to_uint32());
    ASSERT_FUNCTION_RETURNS_TRUE(test_uint64_hash_to_uint128());
    ASSERT_FUNCTION_RETURNS_TRUE(test_hash());
    return 0;
}
