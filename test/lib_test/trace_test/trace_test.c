#include <glib.h>

#include "trace/read_trace.h"

int
main(void)
{
    struct Trace trace = read_trace("/home/david/Downloads/src2.bin");
    g_assert_nonnull(trace.trace);
    return 0;
}
