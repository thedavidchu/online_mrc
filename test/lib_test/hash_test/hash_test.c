#include <bits/stdint-uintn.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <glib.h>

#include "hash/MurmurHash3.h"
#include "hash/MyMurmurHash3.h"
#include "logger/logger.h"
#include "test/mytester.h"

static bool
test_string_hash(void)
{
    char const *const str = "Hello, World!";
    uint32_t hash = 0;
    MurmurHash3_x86_32(str, strlen(str), 0, &hash);
    LOGGER_INFO("Hash %s = %" PRIu32, str, hash);
    g_assert_cmpuint(hash, ==, 592631239UL);
    return true;
}

static bool
test_uint64_hash(void)
{
    uint64_t input = 0;
    uint64_t hash[2] = {0, 0};

    MurmurHash3_x64_128(&input, sizeof(input), 0, &hash);
    LOGGER_INFO("Hash %" PRIu64 " = %" PRIu64 ".%" PRIu64,
                input,
                hash[0],
                hash[1]);
    g_assert_cmpuint(hash[0], ==, 2945182322382062539ULL);
    g_assert_cmpuint(hash[1], ==, 17462001654787800658ULL);

    // Test MyMurmurHash3() wrapper
    g_assert_cmpuint(hash[0], ==, MyMurmurHash3(0));
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_string_hash());
    ASSERT_FUNCTION_RETURNS_TRUE(test_uint64_hash());
    return 0;
}
