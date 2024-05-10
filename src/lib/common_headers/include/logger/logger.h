#pragma once

#include <stdio.h>

// NOTE These are globally configurable parameters. Having these as macros
//      means there is no performance overhead for unused logging (assuming
//      effective dead code elimination).
#define LOGGER_STREAM stderr
#define LOGGER_LEVEL  LOGGER_LEVEL_TRACE

// NOTE The relationship between these levels is subject to change.
enum LoggerLevels {
    LOGGER_LEVEL_TRACE = 0,
    LOGGER_LEVEL_DEBUG = 1,
    LOGGER_LEVEL_INFO = 2,
    LOGGER_LEVEL_WARN = 3,
    LOGGER_LEVEL_ERROR = 4,
    LOGGER_LEVEL_FATAL = 5,
};

#define LOGGER_TRACE(...)                                                      \
    do {                                                                       \
        if (LOGGER_LEVEL >= LOGGER_LEVEL_TRACE) {                              \
            fprintf(LOGGER_STREAM, "[TRACE] %s:%d : ", __FILE__, __LINE__);    \
            fprintf(LOGGER_STREAM, __VA_ARGS__);                               \
            fprintf(LOGGER_STREAM, "\n");                                      \
        }                                                                      \
    } while (0)

#define LOGGER_DEBUG(...)                                                      \
    do {                                                                       \
        if (LOGGER_LEVEL >= LOGGER_LEVEL_TRACE) {                              \
            fprintf(LOGGER_STREAM, "[DEBUG] %s:%d : ", __FILE__, __LINE__);    \
            fprintf(LOGGER_STREAM, __VA_ARGS__);                               \
            fprintf(LOGGER_STREAM, "\n");                                      \
        }                                                                      \
    } while (0)

#define LOGGER_INFO(...)                                                       \
    do {                                                                       \
        if (LOGGER_LEVEL >= LOGGER_LEVEL_TRACE) {                              \
            fprintf(LOGGER_STREAM, "[INFO] %s:%d : ", __FILE__, __LINE__);     \
            fprintf(LOGGER_STREAM, __VA_ARGS__);                               \
            fprintf(LOGGER_STREAM, "\n");                                      \
        }                                                                      \
    } while (0)

#define LOGGER_WARN(...)                                                       \
    do {                                                                       \
        if (LOGGER_LEVEL >= LOGGER_LEVEL_TRACE) {                              \
            fprintf(LOGGER_STREAM, "[WARN] %s:%d : ", __FILE__, __LINE__);     \
            fprintf(LOGGER_STREAM, __VA_ARGS__);                               \
            fprintf(LOGGER_STREAM, "\n");                                      \
        }                                                                      \
    } while (0)

#define LOGGER_ERROR(...)                                                      \
    do {                                                                       \
        if (LOGGER_LEVEL >= LOGGER_LEVEL_TRACE) {                              \
            fprintf(LOGGER_STREAM, "[ERROR] %s:%d : ", __FILE__, __LINE__);    \
            fprintf(LOGGER_STREAM, __VA_ARGS__);                               \
            fprintf(LOGGER_STREAM, "\n");                                      \
        }                                                                      \
    } while (0)

#define LOGGER_FATAL(...)                                                      \
    do {                                                                       \
        if (LOGGER_LEVEL >= LOGGER_LEVEL_TRACE) {                              \
            fprintf(LOGGER_STREAM, "[FATAL] %s:%d : ", __FILE__, __LINE__);    \
            fprintf(LOGGER_STREAM, __VA_ARGS__);                               \
            fprintf(LOGGER_STREAM, "\n");                                      \
        }                                                                      \
    } while (0)
