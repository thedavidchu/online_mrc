#include <stdbool.h>
#include <stdio.h>

#include <glib.h>

#include "io/io.h"
#include "logger/logger.h"
#include "test/mytester.h"

static bool
test_mmap(char const *const fpath)
{
    struct MemoryMap me = {0};
    if (!MemoryMap__init(&me, fpath, "rb")) {
        LOGGER_ERROR("bad initialization");
        return false;
    }
    MemoryMap__write_as_json(stdout, &me);
    // We expect the number of bytes to exactly match our expectations!
    g_assert_cmpuint(me.num_bytes, ==, 84311825UL);

    // Simply check that all parts of the memory map are reachable.
    size_t checksum = 0;
    for (size_t i = 0; i < me.num_bytes; ++i) {
        checksum += ((char *)me.buffer)[i];
    }
    // NOTE I just simply calculated this checksum by running this test
    //      and using the number it produced.
    g_assert_cmpuint(checksum, ==, 141284780);

    MemoryMap__destroy(&me);
    return true;
}

int
main(int argc, char *argv[])
{
    assert(argc == 2);
    ASSERT_FUNCTION_RETURNS_TRUE(test_mmap(argv[1]));
    return 0;
}
