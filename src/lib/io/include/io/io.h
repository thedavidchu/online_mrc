#pragma once

#ifdef __cplusplus
extern "C" {
#endif /* !__cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

struct MemoryMap {
    void *buffer;
    size_t num_bytes;
};

bool
MemoryMap__init(struct MemoryMap *me,
                char const *const file_name,
                char const *const modes);

void
MemoryMap__write_as_json(FILE *stream, struct MemoryMap *me);

bool
MemoryMap__destroy(struct MemoryMap *me);

bool
write_buffer(char const *const file_name,
             void const *const buffer,
             size_t const nmemb,
             size_t size);

#ifdef __cplusplus
}
#endif /* !__cplusplus */
