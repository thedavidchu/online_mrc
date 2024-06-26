#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "file/file.h"
#include "logger/logger.h"

bool
write_buffer(char const *const file_name,
             void const *const buffer,
             size_t const nmemb,
             size_t size)
{
    FILE *fp = fopen(file_name, "wb");
    if (fp == NULL) {
        LOGGER_ERROR("failed to open file '%s'", file_name);
        return false;
    }

    size_t nwritten = fwrite(buffer, size, nmemb, fp);
    if (nwritten != nmemb) {
        LOGGER_WARN("expected to write %zu * %zu bytes, wrote %zu * %zu bytes ",
                    nmemb,
                    size,
                    nwritten,
                    size);
    }

    if (fclose(fp) != 0) {
        LOGGER_ERROR("failed to close file '%s'", file_name);
        return false;
    }
    return true;
}

/// @brief  Check whether a file exists.
/// @note   I add a lot of complicated machinery to save and restore the
///         old errno because having the file not exist is almost
///         expected (sometimes).
bool
file_exists(char const *const file_name)
{
    int const prev_errno = errno;
    errno = 0;
    // Source:
    // https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c/230068#230068
    // NOTE Good if we don't want to create the file.
    bool const r = access(file_name, F_OK) == 0;
    if (errno) {
        LOGGER_TRACE("access(\"%s\", F_OK) raised error", file_name);
    }
    errno = prev_errno;
    return r;
}
