#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

struct MemoryMap {
    void *buffer;
    size_t num_bytes;
    int fd;
};

bool
MemoryMap__init(struct MemoryMap *me,
                char const *const file_name,
                char const *const modes);

bool
MemoryMap__destroy(struct MemoryMap *me);
