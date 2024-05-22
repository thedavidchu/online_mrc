/** This is based on my EasyLib logger. */
#pragma once

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

// NOTE These are globally configurable parameters. Having these as macros
//      means there is no performance overhead for unused logging (assuming
//      effective dead code elimination).
#define LOGGER_STREAM stdout
#define LOGGER_LEVEL  LOGGER_LEVEL_INFO

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
static inline void
_logger(FILE *const stream,
        enum LoggerLevels const threshold_level,
        enum LoggerLevels const log_level,
        char const *const file,
        int const line,
        char const *const format,
        ...)
{
    va_list ap;
    if (log_level < threshold_level) {
        return;
    }
    fprintf(stream,
            "[%s] %s:%d : ",
            LOGGER_LEVEL_STRINGS[log_level],
            file,
            line);
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
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_TRACE(...)                                                      \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_TRACE,                                                \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_DEBUG(...)                                                      \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_DEBUG,                                                \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_INFO(...)                                                       \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_INFO,                                                 \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_WARN(...)                                                       \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_WARN,                                                 \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_ERROR(...)                                                      \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_ERROR,                                                \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)

#define LOGGER_FATAL(...)                                                      \
    _logger(LOGGER_STREAM,                                                     \
            LOGGER_LEVEL,                                                      \
            LOGGER_LEVEL_FATAL,                                                \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __VA_ARGS__)
