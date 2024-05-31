#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

/// @note   I am a bit sketched out about adding a dependency on my logger.
#include "logger/logger.h"

static inline void
write_double(FILE *stream, void const *const element)
{
    fprintf(stream, "%g", *(double *)element);
}

static inline void
write_uint64(FILE *stream, void const *const element)
{
    fprintf(stream, "%" PRIu64, *(uint64_t *)element);
}

static inline void
write_usize(FILE *stream, void const *const element)
{
    fprintf(stream, "%zu", *(size_t *)element);
}

static inline size_t
get_datum_size(void (*write_datum)(FILE *, void const *const))
{
    if (write_datum == write_double) {
        return sizeof(double);
    } else if (write_datum == write_uint64) {
        return sizeof(uint64_t);
    } else if (write_datum == write_usize) {
        return sizeof(size_t);
    } else {
        return 0;
    }
}

/// @note   Specifying the datum_size and write_datum functions are
///         redundant.
static inline void
array__write_as_json(FILE *stream,
                     void const *const data,
                     size_t const length,
                     void (*write_datum)(FILE *, void const *const))
{
    if (stream == NULL) {
        LOGGER_WARN("stream == NULL");
        return;
    }
    if (data == NULL) {
        fprintf(stream, "{\"type\": null}\n");
        return;
    }
    fprintf(stream,
            "{\"type\": \"Array\", \".length\": %zu, \".data\": [",
            length);
    uint8_t const *const bytes = data;
    size_t const datum_size = get_datum_size(write_datum);

    if (datum_size == 0) {
        LOGGER_WARN("unrecognized printing function %p", write_datum);
        return;
    }

    for (size_t i = 0; i < length; ++i) {
        write_datum(stream, &bytes[i * datum_size]);
        // NOTE I have a function to determine whether i is the last index, but
        //      I don't want this file to have any dependencies!
        if (i != length - 1) {
            fprintf(stream, ", ");
        }
    }
    fprintf(stream, "]}\n");
    return;
}
