#include <assert.h>
#include <endian.h> /* This is Linux specific */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <glib.h>

#include "arrays/array_size.h"
#include "arrays/is_last.h"
#include "io/io.h"
#include "logger/logger.h"
#include "trace/reader.h"
#include "trace/trace.h"

size_t
get_bytes_per_trace_item(enum TraceFormat format)
{
    switch (format) {
    case TRACE_FORMAT_KIA:
        return 25;
    case TRACE_FORMAT_SARI:
        return 20;
    default:
        LOGGER_ERROR("unrecognized format");
        return 0;
    }
}

/// @brief  Read a trace item in Kia's format.
///
/// @note   Kia's binary format is as follows:
///
///         Field Name  | Size (bytes)  | Offset (bytes)
///         ------------|---------------|---------------
///         Timestamp   | u64 (8 bytes) | 0
///         Command     | u64 (1 byte)  | 8
///         Key         | u64 (8 bytes) | 9
///         Object size | u32 (4 bytes) | 17
///         Time-to-live| u32 (4 bytes) | 21
///
///         N.B. Everything is little-endian.
static struct FullTraceItem
read_kia_trace_item(uint8_t const *const restrict bytes)
{
    uint64_t timestamp_ms = 0, key = 0;
    uint32_t size = 0, ttl_s = 0;
    uint8_t command = 0;
    memcpy(&timestamp_ms, &bytes[0], sizeof(timestamp_ms));
    command = bytes[8];
    memcpy(&key, &bytes[9], sizeof(key));
    memcpy(&size, &bytes[17], sizeof(size));
    memcpy(&ttl_s, &bytes[21], sizeof(ttl_s));
    return (struct FullTraceItem){.timestamp_ms = le64toh(timestamp_ms),
                                  .command = command,
                                  .key = le64toh(key),
                                  .size = le32toh(size),
                                  .ttl_s = le32toh(ttl_s)};
}

/// @brief  Read a trace item in Sari's format.
///
/// @note   Sari's binary format on the disks uses TTL rather than
///         eviction time (as in the TTLs Matter paper).
/// @note   Sari's binary format is as follows:
///
///         Field Name  | Size (bytes)  | Offset (bytes)
///         ------------|---------------|---------------
///         Timestamp   | u32 (4 bytes) | 0
///         Key         | u64 (8 bytes) | 4
///         Object size | u32 (4 bytes) | 12
///         Time-to-live| u32 (4 bytes) | 16
///
///         N.B. Everything is little-endian as far as I can tell.
/* Timestamp at byte 0, Key at byte 4, Size at byte 12, Eviction time at
 * byte 16 */
static struct FullTraceItem
read_sari_trace_item(uint8_t const *const restrict bytes)
{
    // NOTE Sari's format uses uint32 timestamps. This means that we
    //      need to read 4 bytes and interpret this as a little-
    //      endian uint32 before sticking this in the uint64 in the
    //      data structure.
    uint32_t timestamp_s = 0, size = 0, ttl_s = 0;
    uint64_t key = 0;
    memcpy(&timestamp_s, &bytes[0], sizeof(timestamp_s));
    memcpy(&key, &bytes[4], sizeof(key));
    memcpy(&size, &bytes[12], sizeof(size));
    memcpy(&ttl_s, &bytes[16], sizeof(ttl_s));

    return (struct FullTraceItem){
        // We need to stick the little-endian uint32 into the
        // host's uint64.
        .timestamp_ms = 1000 * (uint64_t)le32toh(timestamp_s),
        // Sari's format only contains 'get' requests as far as I know.
        .command = 0,
        .key = le64toh(key),
        .size = le32toh(size),
        // NOTE According to Sari's TTLs Matter paper, the format is:
        //      `le32toh(eviction_time_s) - le32toh(timestamp_s)`,
        //      but that's not what I empirically observe.
        .ttl_s = le32toh(ttl_s)};
}

/// @note   Hehe... bit twiddly hacks.
/// @note   I really hope the compiler inlines this for performance.
/// Source: https://man7.org/linux/man-pages/man3/endian.3.html
struct TraceItemResult
construct_trace_item(uint8_t const *const restrict bytes,
                     enum TraceFormat format)
{
    struct TraceItemResult const invalid_result = {.valid = false, .item = {0}};
    if (bytes == NULL) {
        LOGGER_ERROR("got NULL");
        return invalid_result;
    }

    // We perform memcpy because the bytes may not be aligned,
    // so we cannot do simple assignment.
    switch (format) {
    case TRACE_FORMAT_KIA: {
        struct FullTraceItem item = read_kia_trace_item(bytes);
        return (struct TraceItemResult){
            // We want to filter for gets, which have the value 0.
            .valid = !item.command,
            .item = (struct TraceItem){.key = item.key}};
    }
    case TRACE_FORMAT_SARI: {
        struct FullTraceItem item = read_sari_trace_item(bytes);
        return (struct TraceItemResult){
            // Sari's format only contains get entries as far as I know
            .valid = true,
            .item = (struct TraceItem){.key = item.key}};
    }
    default:
        LOGGER_ERROR("unrecognized format %d", format);
        return invalid_result;
    }
}

struct FullTraceItemResult
construct_full_trace_item(uint8_t const *const restrict bytes,
                          enum TraceFormat format)
{
    if (bytes == NULL) {
        LOGGER_ERROR("got NULL");
        goto error_cleanup;
    }

    // We perform memcpy because the bytes may not be aligned,
    // so we cannot do simple assignment.
    switch (format) {
    case TRACE_FORMAT_KIA: {
        return (struct FullTraceItemResult){.valid = true,
                                            .item = read_kia_trace_item(bytes)};
    }
    case TRACE_FORMAT_SARI: {
        return (struct FullTraceItemResult){.valid = true,
                                            .item =
                                                read_sari_trace_item(bytes)};
    }
    default:
        LOGGER_ERROR("unrecognized format %d", format);
        goto error_cleanup;
    }
    assert(0 && "impossible");
error_cleanup:
    return (struct FullTraceItemResult){.valid = false,
                                        .item = (struct FullTraceItem){0}};
}

void
print_available_trace_formats(FILE *stream)
{
    fprintf(stream, "{");
    // NOTE We want to skip the "INVALID" algorithm name (i.e. 0).
    for (size_t i = 1; i < ARRAY_SIZE(TRACE_FORMAT_STRINGS); ++i) {
        fprintf(stream, "%s", TRACE_FORMAT_STRINGS[i]);
        if (!is_last(i, ARRAY_SIZE(TRACE_FORMAT_STRINGS))) {
            fprintf(stream, ",");
        }
    }
    fprintf(stream, "}");
}

enum TraceFormat
parse_trace_format_string(char const *const format_str)
{
    if (format_str == NULL)
        return TRACE_FORMAT_INVALID;
    for (size_t i = 1; i < ARRAY_SIZE(TRACE_FORMAT_STRINGS); ++i) {
        if (strcmp(TRACE_FORMAT_STRINGS[i], format_str) == 0)
            return (enum TraceFormat)i;
    }
    LOGGER_ERROR("unparsable format string: '%s'", format_str);
    fprintf(LOGGER_STREAM, "   expected: ");
    print_available_trace_formats(LOGGER_STREAM);
    fprintf(LOGGER_STREAM, "\n");
    return TRACE_FORMAT_INVALID;
}

struct Trace
read_trace(char const *const restrict file_name, enum TraceFormat format)
{
    struct TraceItem *trace = NULL;
    size_t nobj_expected = 0;

    size_t bytes_per_obj = get_bytes_per_trace_item(format);
    if (bytes_per_obj == 0) {
        LOGGER_ERROR("unrecognized format %d", format);
        goto cleanup;
    }

    struct MemoryMap mm = {0};
    if (!MemoryMap__init(&mm, file_name, "rb")) {
        LOGGER_ERROR("could not open '%s'", file_name);
        goto cleanup;
    }

    nobj_expected = mm.num_bytes / bytes_per_obj;
    trace = calloc(nobj_expected, sizeof(*trace));
    if (trace == NULL) {
        LOGGER_ERROR("could not allocate return value for %zu * %zu bytes",
                     nobj_expected,
                     sizeof(*trace));
        goto cleanup;
    }

    // Rearrange the bytes correctly
    size_t idx = 0;
    for (size_t i = 0; i < nobj_expected; ++i) {
        struct TraceItemResult result =
            construct_trace_item(&((uint8_t *)mm.buffer)[bytes_per_obj * i],
                                 format);
        if (result.valid) {
            trace[idx] = result.item;
            ++idx;
        }
    }

    if (!MemoryMap__destroy(&mm)) {
        LOGGER_ERROR("could not close file %s", file_name);
        goto cleanup;
    }

    return (struct Trace){.trace = trace, .length = idx};

cleanup:
    MemoryMap__destroy(&mm);
    free(trace);
    // Yes, I know I could just `return (struct Trace){0}`, but I want
    // to be VERY explicit.
    return (struct Trace){.trace = NULL, .length = 0};
}
