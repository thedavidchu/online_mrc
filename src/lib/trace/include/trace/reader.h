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
construct_trace_item(uint8_t const *const restrict bytes)
{
    struct TraceItem trace = {0};

    // We perform memcpy because the bytes may not be aligned, so we cannot do
    // simple assignment.
    memcpy(&trace.timestamp, &bytes[0], sizeof(trace.timestamp));
    memcpy(&trace.command, &bytes[8], sizeof(trace.command));
    memcpy(&trace.key, &bytes[9], sizeof(trace.key));
    memcpy(&trace.object_size, &bytes[17], sizeof(trace.object_size));
    memcpy(&trace.time_to_live, &bytes[21], sizeof(trace.time_to_live));

    return (struct TraceItem){
        .timestamp = le64toh(trace.timestamp),
        .command = trace.command,
        .key = le64toh(trace.key),
        .object_size = le32toh(trace.object_size),
        .time_to_live = le32toh(trace.time_to_live),
    };
}

/// @brief  Read the traces formatted by Kia and Sari.
/// So the data format is basically each access occupies 25 bytes in this order:
/// 8 bytes -> u64 -> timestamp (unix timestamp)
/// 1 byte -> u8 -> command (0 is get, 1 is set)
/// 8 bytes -> u64 -> key
/// 4 bytes -> u32 -> object size
/// 4 bytes -> u32 -> ttl (zero means no ttl)
/// Everything is formatted in little endian
static inline struct Trace
read_trace(char const *const restrict file_name)
{
    FILE *fp = NULL;
    struct TraceItem *trace = NULL;
    uint8_t *bytes = NULL;
    size_t file_size = 0, nobj_read = 0;

    fp = fopen(file_name, "rb");
    if (fp == NULL) {
        LOGGER_ERROR("could not open '%s' with error %d '%s'",
                     file_name,
                     errno,
                     strerror(errno));
        goto cleanup;
    }

    file_size = get_file_size_in_bytes(fp);
    if (file_size % 25 != 0) {
        LOGGER_ERROR("file size is not divisible by 25 bytes (i.e. the size of "
                     "each entry)");
        goto cleanup;
    }

    bytes = malloc(file_size);
    if (bytes == NULL) {
        LOGGER_ERROR("could not allocate temporary buffer of size %zu",
                     file_size);
        goto cleanup;
    }

    trace = calloc(file_size / 25, sizeof(*trace));
    if (trace == NULL) {
        LOGGER_ERROR("could not allocate return value");
        goto cleanup;
    }

    nobj_read = fread(trace, 25, file_size / 25, fp);
    if (file_size != 25 * nobj_read) {
        LOGGER_ERROR("expected %zu bytes, got %zu bytes", file_size, 25 * nobj_read);
        goto cleanup;
    }

    // Rearrange the bytes correctly
    for (size_t i = 0; i < nobj_read; ++i) {
        trace[i] = construct_trace_item(&bytes[25 * i]);
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
