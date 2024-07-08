#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "io/io.h"
#include "logger/logger.h"

static bool
file_to_mmap(struct MemoryMap *const me,
             FILE *const fp,
             char const *const fpath)
{
    int fd = 0;
    struct stat sb = {0};
    void *buffer = NULL;

    // NOTE This does NOT transfer ownership of the file from fp to fd!
    //      The C standard library still has allocated data in the FILE
    //      pointer that we need to clean up.
    fd = fileno(fp);
    if (fd == -1) {
        LOGGER_ERROR("failed get file descriptor for '%s'", fpath);
        fclose(fp);
        return false;
    }

    // We want to get the file size so we know how large our memory
    // mapped region is.
    if (fstat(fd, &sb) == -1) {
        LOGGER_ERROR("failed to get size of file '%s'", fpath);
        if (fclose(fp) == EOF) {
            LOGGER_ERROR("failed to close '%s' too", fpath);
        }
        return false;
    }

    buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    *me = (struct MemoryMap){.buffer = buffer, .num_bytes = sb.st_size};
    return true;
}

/// @note   I use the fopen modes because then we don't need to think
///         too hard when using this function.
bool
MemoryMap__init(struct MemoryMap *me,
                char const *const file_name,
                char const *const modes)
{
    FILE *fp = NULL; // NOTE We transfer the ownership to the fd
    if (me == NULL || file_name == NULL) {
        return false;
    }
    fp = fopen(file_name, modes);
    if (fp == NULL) {
        LOGGER_ERROR("failed to open file '%s'", file_name);
        return false;
    }
    if (!file_to_mmap(me, fp, file_name)) {
        LOGGER_ERROR("failed to memory map '%s'", file_name);
        return false;
    }
    // NOTE One may close the file without invalidating the memory map.
    // NOTE I close the fp versus the fd because otherwise there is a
    //      memory leak.
    if (fclose(fp) == EOF) {
        LOGGER_ERROR("failed to close file");
        return false;
    }
    return true;
}

void
MemoryMap__write_as_json(FILE *stream, struct MemoryMap *me)
{
    if (stream == NULL) {
        LOGGER_ERROR("stream is NULL");
        return;
    }
    if (me == NULL) {
        fprintf(stream, "{\"type\": null}\n");
        return;
    }
    fprintf(stream,
            "{\"type\": \"MemoryMap\", \".buffer\": %p, \".num_bytes\": %zu}\n",
            me->buffer,
            me->num_bytes);
    return;
}

bool
MemoryMap__destroy(struct MemoryMap *me)
{
    if (me == NULL || me->buffer == NULL) {
        return false;
    }
    if (munmap(me->buffer, me->num_bytes) != 0) {
        LOGGER_ERROR("failed to unmap region");
        return false;
    }
    *me = (struct MemoryMap){0};
    return true;
}
