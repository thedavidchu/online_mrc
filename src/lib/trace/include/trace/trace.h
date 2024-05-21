#pragma once

#include <stddef.h>
#include <stdint.h>

struct TraceItem {
    uint64_t key;
};

struct FullTraceItem {
    uint64_t timestamp;
    uint8_t command;
    uint64_t key;
    uint32_t object_size;
    uint32_t time_to_live; /* Kia stores TTL rather than expiry time */
    uint32_t expiry_time;  /* Sari stores expiry time rather than the TTL */
};

struct Trace {
    // NOTE I want to mark this as `struct TraceItem const *const restrict` but
    //      what if we have multiple readers? From the examples I see online,
    //      `restrict` means that other aliases to the pointer won't _modify_
    //      this value; but the written contract says aliases won't even exist.
    //      I defer to the written contract (until I'm convinced that it
    //      supports my interpretation of the examples).
    struct TraceItem const *trace;
    size_t length;
};

void
Trace__destroy(struct Trace *const me);
