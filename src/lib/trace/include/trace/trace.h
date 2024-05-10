#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct TraceItem {
    uint64_t timestamp;
    uint8_t command;
    uint64_t key;
    uint32_t object_size;
    uint32_t time_to_live;
};

struct Trace {
    // NOTE I want to mark this as `struct TraceItem const *const restrict` but
    //      what if we have multiple readers? From the examples I see online,
    //      `restrict` means that other aliases to the pointer won't _modify_
    //      this value; but the written contract says aliases won't even exist.
    //      I defer to the written contract (until I'm convinced that it
    //      supports my interpretation of the examples).
    struct TraceItem const *const trace;
    size_t const length;
};

void
Trace__destroy(struct Trace *me)
{
    if (me == NULL)
        return;

    // HACK This is an utter hack to get around the const-qualifiers.
    free((void *)me->trace);
    memset(me, 0, sizeof(*me));
}
