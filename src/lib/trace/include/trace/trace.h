#pragma once

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct TraceItem {
    uint64_t key;
};

struct FullTraceItem {
    // The timestamp when the access occurred. This is expressed in
    // milliseconds.
    uint64_t timestamp_ms;
    // Only Kia stores whether it is a get (=0) or set (=1) request.
    // I assume Sari filters out the set requests.
    uint8_t command;
    uint64_t key;
    uint32_t size;
    // This expresses the time-to-live (TTL) in seconds.
    // Kia stores TTL rather than expiry time;  Sari stores expiry time
    // rather than the TTL.
    uint32_t ttl_s;
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

#ifdef __cplusplus
}
#endif /* __cplusplus */
