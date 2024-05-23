#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
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
        LOGGER_ERROR("failed to open file '%s', error %d: %s",
                     file_name,
                     errno,
                     strerror(errno));
        return false;
    }

    fd = fileno(fp);
    if (fd == -1) {
        LOGGER_ERROR("failed get file descriptor for '%s', error %d: %s",
                     file_name,
                     errno,
                     strerror(errno));
        fclose(fp);
        return false;
    }

    if (fstat(fd, &sb) == -1) {
        LOGGER_ERROR("failed to get size of file '%s', error %d: %s",
                     file_name,
                     errno,
                     strerror(errno));
        // We close the file with close since we've transferred ownership to fd.
        if (close(fd) == -1) {
            LOGGER_ERROR("failed to close '%s' too, error %d: %s",
                         file_name,
                         errno,
                         strerror(errno));
        }
        return false;
    }

    buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    *me =
        (struct MemoryMap){.buffer = buffer, .num_bytes = sb.st_size, .fd = fd};
    return true;
}

bool
MemoryMap__destroy(struct MemoryMap *me)
{
    if (me == NULL || me->fd == -1) {
        return false;
    }
    if (close(me->fd) == -1) {
        LOGGER_ERROR("failed to close file, error %d: %s",
                     errno,
                     strerror(errno));
        return false;
    }
    *me = (struct MemoryMap){0};
    return true;
}
