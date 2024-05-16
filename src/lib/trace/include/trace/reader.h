#pragma once

#include <assert.h>
#include <endian.h> /* This is Linux specific */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "logger/logger.h"
#include "trace/trace.h"

enum TraceFormat {
    /// So the data format is basically each access occupies 25 bytes in this
    /// order:
    /// 8 bytes -> u64 -> timestamp (unix timestamp)
    /// 1 byte -> u8 -> command (0 is get, 1 is set)
    /// 8 bytes -> u64 -> key
    /// 4 bytes -> u32 -> object size
    /// 4 bytes -> u32 -> ttl (zero means no ttl)
    /// Everything is formatted in little endian
    TRACE_FORMAT_KIA,
    /// Our access traces are binary formatted using the following format, and
    /// sorted by timestamp.
    /// | Property  | Type                           |
    /// | --------- | ------------------------------ |
    /// | Timestamp | Time in Seconds (uint32)       |
    /// | Key       | uint64                         |
    /// | Size      | uint32                         |
    /// | Eviction Time       | uint32 (TTL + Timestamp) |
    /// Each access thus requires 20 bytes.
    TRACE_FORMAT_SARI,
};

char const *const TRACE_FORMAT_STRINGS[] = {"Kia", "Sari"};

/// @return Get the number of bytes per trace item.
static inline size_t
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

/// Source: stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
static inline size_t
get_file_size_in_bytes(FILE *fp)
{
    size_t nbytes = 0;

    assert(fp != NULL);

    fseek(fp, 0L, SEEK_END);
    nbytes = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    return nbytes;
}

/// @note   Hehe... bit twiddly hacks.
/// Source: https://man7.org/linux/man-pages/man3/endian.3.html
static inline struct TraceItem
construct_trace_item(uint8_t const *const restrict bytes,
                     enum TraceFormat format)
{
    struct TraceItem trace = {0};
    if (bytes == NULL) {
        LOGGER_ERROR("got NULL");
        return trace;
    }

    // We perform memcpy because the bytes may not be aligned, so we cannot do
    // simple assignment.
    switch (format) {
    case TRACE_FORMAT_KIA: {
        /* Timestamp at byte 0, Command at byte 8, Key at byte 9, Object size at
         * byte 17, Time-to-live at byte 21 */
        memcpy(&trace.key, &bytes[9], sizeof(trace.key));
        return (struct TraceItem){
            .key = le64toh(trace.key),
        };
    }
    case TRACE_FORMAT_SARI: {
        /* Timestamp at byte 0, Key at byte 4, Size at byte 12, Eviction time at
         * byte 16 */
        memcpy(&trace.key, &bytes[4], sizeof(trace.key));
        return (struct TraceItem){
            .key = le64toh(trace.key),
        };
    }
    default:
        LOGGER_ERROR("unrecognized format %d", format);
        return trace;
    }
}

/// @brief  Read the traces formatted by Kia and Sari.
static inline struct Trace
read_trace(char const *const restrict file_name, enum TraceFormat format)
{
    FILE *fp = NULL;
    struct TraceItem *trace = NULL;
    uint8_t *bytes = NULL;
    size_t file_size = 0, nobj_expected = 0, nobj_read = 0;

    size_t bytes_per_obj = get_bytes_per_trace_item(format);
    if (bytes_per_obj == 0) {
        LOGGER_ERROR("unrecognized format %d", format);
        goto cleanup;
    }

    fp = fopen(file_name, "rb");
    if (fp == NULL) {
        LOGGER_ERROR("could not open '%s' with error %d '%s'",
                     file_name,
                     errno,
                     strerror(errno));
        goto cleanup;
    }

    file_size = get_file_size_in_bytes(fp);
    if (file_size % bytes_per_obj != 0) {
        LOGGER_ERROR(
            "file size %zu is not divisible by %zu bytes (i.e. the size of "
            "each entry)",
            file_size,
            bytes_per_obj);
        goto cleanup;
    }

    bytes = malloc(file_size);
    if (bytes == NULL) {
        LOGGER_ERROR("could not allocate temporary buffer of size %zu",
                     file_size);
        goto cleanup;
    }

    nobj_expected = file_size / bytes_per_obj;
    trace = calloc(nobj_expected, sizeof(*trace));
    if (trace == NULL) {
        LOGGER_ERROR("could not allocate return value for %zu * %zu bytes",
                     nobj_expected,
                     sizeof(*trace));
        goto cleanup;
    }

    nobj_read = fread(bytes, bytes_per_obj, nobj_expected, fp);
    if (nobj_read != nobj_expected) {
        LOGGER_ERROR("expected %zu trace items, got %zu",
                     nobj_expected,
                     nobj_read);
        goto cleanup;
    }

    // Rearrange the bytes correctly
    for (size_t i = 0; i < nobj_read; ++i) {
        trace[i] = construct_trace_item(&bytes[bytes_per_obj * i], format);
    }

    int r = fclose(fp);
    if (r != 0) {
        LOGGER_ERROR("could not close file %s", file_name);
        goto cleanup;
    }

    free(bytes);
    return (struct Trace){.trace = trace, .length = nobj_read};

cleanup:
    free(bytes);
    free(trace);
    // Yes, I know I could just `return (struct Trace){0}`, but I want
    // to be VERY explicit.
    return (struct Trace){.trace = NULL, .length = 0};
}
