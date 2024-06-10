/** This is based on my EasyLib logger. */
#pragma once

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// NOTE These are globally configurable parameters. Having these as macros
//      means there is no performance overhead for unused logging (assuming
//      effective dead code elimination).
#define LOGGER_STREAM stdout
#define LOGGER_LEVEL  LOGGER_LEVEL_TRACE

// NOTE The relationship between these levels is subject to change. But
//      if you do go ahead and change them, you need to change the
//      EASY_LOGGER_LEVEL_STRINGS too.
enum LoggerLevels {
    LOGGER_LEVEL_VERBOSE = 0,
    LOGGER_LEVEL_TRACE = 1,
    LOGGER_LEVEL_DEBUG = 2,
    LOGGER_LEVEL_INFO = 3,
    LOGGER_LEVEL_WARN = 4,
    LOGGER_LEVEL_ERROR = 5,
    LOGGER_LEVEL_FATAL = 6,
};

static char const *const LOGGER_LEVEL_STRINGS[] =
    {"VERBOSE", "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

// NOTE I intentionally make all of the parameters verbose instead of
//      packing some of them into a structure. This is because then we
//      are not reliant on the structure if I decide I no longer like it.
/// @note   I capture errno outside of this function because any standard
///         library call (e.g. printf) can change the value of errno.
static inline void
_logger(FILE *const stream,
        enum LoggerLevels const threshold_level,
        enum LoggerLevels const log_level,
        int const errno_,
        char const *const file,
        int const line,
        char const *const format,
        ...)
{
    va_list ap;
    time_t t = {0};
    if (log_level < threshold_level) {
        return;
    }
    t = time(NULL);
    struct tm tm = *localtime(&t);
    fprintf(stream, "[%s] ", LOGGER_LEVEL_STRINGS[log_level]);
    fprintf(stream,
            "[%d-%02d-%02d %02d:%02d:%02d] ",
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec);
    fprintf(stream, "[ %s:%d ] ", file, line);
    // NOTE I print this even where there is no error to make the log
    //      easier to parse.
    fprintf(stream, "[errno %d: %s] ", errno_, strerror(errno_));
    va_start(ap, format);
    vfprintf(stream, format, ap);
    va_end(ap);
    fprintf(stream, "\n");
    fflush(stream);
}

#define LOGGER_VERBOSE(...)                                                    \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_VERBOSE,                                              \
            errno,                                                             \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_TRACE(...)                                                      \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_TRACE,                                                \
            errno,                                                             \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_DEBUG(...)                                                      \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_DEBUG,                                                \
            errno,                                                             \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_INFO(...)                                                       \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_INFO,                                                 \
            errno,                                                             \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_WARN(...)                                                       \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_WARN,                                                 \
            errno,                                                             \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_ERROR(...)                                                      \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_ERROR,                                                \
            errno,                                                             \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_FATAL(...)                                                      \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_FATAL,                                                \
            errno,                                                             \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)
