#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct TraceItem {
    uint64_t key;
};

struct FullTraceItem {
    uint64_t timestamp;
    // Only Kia stores whether it is a get/set request. I assume Sari filters
    // out the set requests.
    uint8_t command;
    uint64_t key;
    uint32_t size;
    // Kia stores TTL rather than expiry time; Sari stores expiry time rather
    // than the TTL.
    uint32_t time_to_live;
};

struct Trace {
    struct TraceItem *trace;
    size_t length;
};

void
Trace__write_as_json(FILE *stream, struct Trace const *const me);

/// @brief  Allocate the required memory in the trace.
bool
Trace__init(struct Trace *const me, size_t const length);

void
Trace__destroy(struct Trace *const me);
