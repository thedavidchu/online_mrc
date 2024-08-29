#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

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
    // NOTE This case is (empirically) handled by the 'access' function,
    //      however, I do not want to rely on standard library functions
    //      handling NULL correctly.
    if (file_name == NULL) {
        return false;
    }

    int const prev_errno = errno;
    errno = 0;
    // Source:
    // https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c/230068#230068
    // NOTE Good if we don't want to create the file.
    bool const r = access(file_name, F_OK) == 0;
    if (errno) {
        LOGGER_TRACE("file '%s' DNE", file_name);
    }
    errno = prev_errno;
    return r;
}

char *
get_absolute_path(char const *const path)
{
    if (!path || strcmp(path, "") == 0) {
        return NULL;
    }

    switch (path[0]) {
    // NOTE We promise that the caller must free the return value, so we
    //      duplicate the string so that it can be freed by the caller.
    case '/': {
        // NOTE The 'strdup' function is not necessarily in standard C.
        char *abs_path = strdup(path);
        if (!abs_path) {
            LOGGER_ERROR("error duplicating '%s'", path);
        }
        return abs_path;
    }
    case '~': {
        // NOTE I was going to use 'wordexp()' to evaluate the relative
        //      path, but I decided that it is vulnerable to code
        //      injection and so I didn't want to risk someone else
        //      (especially not future-David) from using this vulnerable
        //      code!!!
        gchar const *home = g_get_home_dir();
        char const *non_tilde_path = strlen(path) == 1 ? "" : &path[2];
        gchar *abs_path = g_canonicalize_filename(non_tilde_path, home);
        return abs_path;
    }
    // NOTE This is the case for both './...' and '../...' paths. I will
    //      not support paths that don't start with './', '../', '~/',
    //      or '/' for now.
    case '.': {
        gchar *cwd = g_get_current_dir();
        gchar *abs_path = g_canonicalize_filename(path, cwd);
        free(cwd);
        return abs_path;
    }
    default:
        LOGGER_ERROR(
            "paths must start with './', '../', '~/', or '/'. "
            "This option is deprecated! Your path '%s' starts with '%c'",
            path,
            path[0]);
        return NULL;
    }
}
