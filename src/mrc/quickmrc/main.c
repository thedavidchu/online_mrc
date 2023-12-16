#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

struct QuickMrc {
    void *cache;
    void *ghost_cache;
    GHashTable *key_lookup;

    uint64_t cache_capacity;
    uint64_t ghost_cache_capacity;
}

int
main(void)
{
    struct QuickMrc a = {0, 0, 0};

    return 0;
}
