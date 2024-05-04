#pragma once

#include <assert.h>
#include <bits/stdint-uintn.h>
#include <endian.h> /* This is Linux specific */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct TraceItem {
    uint64_t timestamp;
    uint8_t command;
    uint64_t key;
    uint32_t object_size;
    uint32_t time_to_live;
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
    return (struct TraceItem){
        .timestamp = le64toh(*(uint64_t *)&bytes[0]),
        .command = bytes[8],
        .key = le64toh(*(uint64_t *)&bytes[9]),
        .object_size = le32toh(*(uint32_t *)&bytes[17]),
        .time_to_live = le32toh(*(uint32_t *)&bytes[21]),
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
static inline struct TraceItem *
read_trace(char const *const restrict file_name)
{
    FILE *fp = NULL;
    struct TraceItem *trace = NULL;
    size_t file_size = 0, read_size = 0;

    trace = calloc(1 << 20, sizeof(*trace));
    if (trace == NULL)
        goto cleanup;

    fp = fopen(file_name, "rb");
    if (fp == NULL)
        goto cleanup;

    file_size = get_file_size_in_bytes(fp);
    read_size = fread(trace, sizeof(*trace), file_size / 25, fp);
    assert(file_size == read_size);

    // Rearrange the bytes correctly
    for (size_t i = 0; i < file_size / 25; ++i) {
        uint8_t bytes[25] = {0};
        memcpy(&trace[25 * i], bytes, 25);

        trace[i] = construct_trace_item(bytes);
    }

    int r = fclose(fp);
    if (r != 0)
        goto cleanup;

    return trace;

cleanup:
    free(trace);
    return NULL;
}
