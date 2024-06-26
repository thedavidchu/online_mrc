#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "io/io.h"
#include "logger/logger.h"

/// @note   I use the fopen modes because then we don't need to think
///         too hard when using this function.
bool
MemoryMap__init(struct MemoryMap *me,
                char const *const file_name,
                char const *const modes)
{
    FILE *fp = NULL; // NOTE We transfer the ownership to the fd
    int fd = 0;
    void *buffer = NULL;
    struct stat sb = {0};

    if (me == NULL || file_name == NULL) {
        return false;
    }

    fp = fopen(file_name, modes);
    if (fp == NULL) {
        LOGGER_ERROR("failed to open file '%s'", file_name);
        return false;
    }

    fd = fileno(fp);
    if (fd == -1) {
        LOGGER_ERROR("failed get file descriptor for '%s'", file_name);
        fclose(fp);
        return false;
    }

    if (fstat(fd, &sb) == -1) {
        LOGGER_ERROR("failed to get size of file '%s'", file_name);
        // We close the file with close since we've transferred ownership to fd.
        if (close(fd) == -1) {
            LOGGER_ERROR("failed to close '%s' too", file_name);
        }
        return false;
    }

    buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    *me = (struct MemoryMap){.buffer = buffer,
                             .num_bytes = sb.st_size,
                             .fd = fd,
                             .fp = fp};
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
            "{\"type\": \"MemoryMap\", \".buffer\": %p, \".num_bytes\": %zu, "
            "\".fd\": %d, \".fp\": %p}\n",
            me->buffer,
            me->num_bytes,
            me->fd,
            (void *)me->fp);
    return;
}

bool
MemoryMap__destroy(struct MemoryMap *me)
{
    if (me == NULL || me->fd == -1) {
        return false;
    }
    // NOTE I close the fp versus the fd because otherwise there is a
    //      memory leak.
    if (fclose(me->fp) == EOF) {
        LOGGER_ERROR("failed to close file");
        return false;
    }
    *me = (struct MemoryMap){0};
    return true;
}
