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

/// @return Get the number of bytes per trace item.
static size_t
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

struct TraceItemResult {
    bool valid;
    struct TraceItem item;
};

/// @note   Hehe... bit twiddly hacks.
/// Source: https://man7.org/linux/man-pages/man3/endian.3.html
static inline struct TraceItemResult
construct_trace_item(uint8_t const *const restrict bytes,
                     enum TraceFormat format)
{
    struct TraceItemResult const invalid_result = {.valid = false, .item = {0}};
    if (bytes == NULL) {
        LOGGER_ERROR("got NULL");
        return invalid_result;
    }

    // We perform memcpy because the bytes may not be aligned, so we cannot do
    // simple assignment.
    switch (format) {
    case TRACE_FORMAT_KIA: {
        /* Timestamp at byte 0, Command at byte 8, Key at byte 9, Object size at
         * byte 17, Time-to-live at byte 21 */
        uint8_t const cmd = bytes[8];
        struct TraceItem trace = {0};
        memcpy(&trace.key, &bytes[9], sizeof(trace.key));
        return (struct TraceItemResult){
            // We want to filter for gets, which have the value 0.
            .valid = !cmd,
            .item = (struct TraceItem){.key = le64toh(trace.key)}};
    }
    case TRACE_FORMAT_SARI: {
        /* Timestamp at byte 0, Key at byte 4, Size at byte 12, Eviction time at
         * byte 16 */
        struct TraceItem trace = {0};
        memcpy(&trace.key, &bytes[4], sizeof(trace.key));
        return (struct TraceItemResult){
            // Sari's format only contains get entries as far as I know
            .valid = true,
            .item = (struct TraceItem){.key = le64toh(trace.key)}};
    }
    default:
        LOGGER_ERROR("unrecognized format %d", format);
        return invalid_result;
    }
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
