/** @brief  Print some lines from the trace file.
 *
 *  @example
 *  ```bash
 *  # Print the first 100 lines.
 *  ./build/src/analysis/text/print_trace_exe \
 *      -i ./data/src2.bin -f Kia -s 0 -l 100
 *  ```
 *
 *  @todo
 *  1. Allow negative indexing (c.f. Python). A negative '--start' would
 *      count from the back; a negative '--length' would count backwards.
 *      But what to do if we get `--start -1 --length 2`? Would this
 *      produce an error, wrap around, or truncate? Or wrap around with
 *      a warning?
 */
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "file/file.h"
#include "io/io.h"
#include "logger/logger.h"
#include "trace/reader.h"
#include "trace/trace.h"

struct CommandLineArguments {
    char *executable;
    gchar *input_path;
    enum TraceFormat trace_format;
    gint command;
    gboolean count_printed_only;
    gint64 start;
    gint64 length;
};

static bool
validate_start_and_length(struct CommandLineArguments const *const args)
{
    if (args == NULL) {
        LOGGER_ERROR("args is NULL");
        return false;
    }
    if (args->start < 0) {
        // TODO(dchu):  Maybe we can support wraparound, similarly to
        //              Python's indexing.
        LOGGER_ERROR("cannot start at a negative index!");
        return false;
    }
    if (args->length <= 0) {
        LOGGER_ERROR("must have positive length!");
        return false;
    }
    // Check that this doesn't exceed the bounds of the array. We'd want
    // to do it now so that we fail early, rather than waiting 10
    // minutes to fail.
    size_t const file_size = get_file_size(args->input_path);
    size_t const item_size = get_bytes_per_trace_item(args->trace_format);
    if (item_size == 0) {
        LOGGER_ERROR("unrecognized trace format '%d'", args->trace_format);
        return false;
    }
    size_t const num_entries = file_size / item_size;
    size_t const start = args->start;
    size_t const length = args->length;
    size_t const end = args->start + args->length;
    // NOTE We will always report an error when trying to read an empty
    //      file, even with `--start 0 --length 0`, since
    //      `(start=0) >= (num_entries=0)`.
    if (start >= num_entries || end > num_entries) {
        LOGGER_ERROR(
            "invalid start (%zu) or length (%zu) for number of entries (%zu)",
            start,
            length,
            num_entries);
        return false;
    }
    return true;
}

/// @note   Copired from '//src/run/generate_mrc.c'. Adapted for this use case.
static struct CommandLineArguments
parse_command_line_arguments(int argc, char *argv[])
{
    gchar *help_msg = NULL;

    // Set defaults.
    struct CommandLineArguments args = {.executable = argv[0],
                                        .input_path = NULL,
                                        .trace_format = TRACE_FORMAT_KIA,
                                        .command = -1,
                                        .count_printed_only = FALSE,
                                        .start = 0,
                                        .length = 10};
    gchar *trace_format = NULL;

    // Command line options.
    GOptionEntry entries[] = {
        // Arguments related to the input generation
        {"input",
         'i',
         0,
         G_OPTION_ARG_FILENAME,
         &args.input_path,
         "path to the input trace",
         NULL},
        {"format",
         'f',
         0,
         G_OPTION_ARG_STRING,
         &trace_format,
         "format of the input trace. Options: {Kia,Sari}. Default: Kia.",
         NULL},
        {"start",
         's',
         0,
         G_OPTION_ARG_INT64,
         &args.start,
         "index to begin. Default 0.",
         NULL},
        {"command",
         'c',
         0,
         G_OPTION_ARG_INT,
         &args.command,
         "filter for a specific command. Default: -1 (i.e. print any "
         "commands)",
         NULL},
        {"printed",
         'p',
         0,
         G_OPTION_ARG_NONE,
         &args.count_printed_only,
         "only count the number of entries printed, rather than the number of "
         "entries passed over. Default: false. N.B. Obviously, we won't exceed "
         "the file size.",
         NULL},
        {"length",
         'l',
         0,
         G_OPTION_ARG_INT64,
         &args.length,
         "length to print. Default 10.",
         NULL},
        G_OPTION_ENTRY_NULL,
    };

    GError *error = NULL;
    GOptionContext *context;
    context = g_option_context_new("- print rows of a trace");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        goto cleanup;
    }
    // Come on, GLib! The 'g_option_context_parse' changes the errno to
    // 2 and leaves it for me to clean up. Or maybe I'm using it wrong.
    errno = 0;

    // Check the arguments for correctness.
    if (args.input_path == NULL || !file_exists(args.input_path)) {
        LOGGER_ERROR("input trace path '%s' DNE",
                     args.input_path == NULL ? "(null)" : args.input_path);
        goto cleanup;
    }
    if (trace_format != NULL) {
        args.trace_format = parse_trace_format_string(trace_format);
        if (args.trace_format == TRACE_FORMAT_INVALID) {
            LOGGER_ERROR("invalid trace format '%s'", trace_format);
            goto cleanup;
        }
    } else {
        // NOTE If 'trace_format' is NULL, the we remain with the default.
        LOGGER_TRACE("using default trace format");
    }
    if (!validate_start_and_length(&args)) {
        LOGGER_ERROR("error in start and/or length");
        goto cleanup;
    }

    g_option_context_free(context);
    return args;
cleanup:
    help_msg = g_option_context_get_help(context, FALSE, NULL);
    g_print("%s", help_msg);
    free(help_msg);
    g_option_context_free(context);
    exit(-1);
}

static bool
print_header(FILE *const stream, size_t const num_entries)
{
    if (stream == NULL) {
        return false;
    }
    fprintf(stdout, "total trace length [entries]: %zu\n", num_entries);
    fprintf(stdout,
            "%10s | %20s | %1s | %20s | %10s | %10s\n",
            "ID",
            "Timestamp [ms]",
            "Command",
            "Key",
            "Size [B]",
            "TTL [s]");
    fprintf(stdout,
            "-----------|----------------------|---------|---------------------"
            "-|-----------"
            "-|-----------\n");
    return true;
}

static bool
maybe_print_entry(size_t const id,
                  void const *const buffer,
                  int const command,
                  enum TraceFormat const trace_format)
{
    size_t const bytes_per_trace_item = get_bytes_per_trace_item(trace_format);
    struct FullTraceItemResult r = construct_full_trace_item(
        &((uint8_t *)buffer)[id * bytes_per_trace_item],
        trace_format);
    if (!r.valid) {
        return false;
    }
    struct FullTraceItem item = r.item;
    if (command == -1 || command == r.item.command) {
        fprintf(stdout,
                "%10zu | %20" PRIu64 " | %7" PRIu8 " | %20" PRIu64
                " | %10" PRIu32 " | %10" PRIu32 "\n",
                id,
                item.timestamp_ms,
                item.command,
                item.key,
                item.size,
                item.ttl_s);
        return true;
    }
    return false;
}

static bool
print_entries(struct CommandLineArguments const *const args,
              struct MemoryMap const *const mm)
{
    if (args == NULL || mm == NULL || mm->buffer == NULL) {
        return false;
    }
    size_t const start = args->start;
    size_t const length = args->length;
    // This is the earliest the ending could possibly be according to
    // the user's inputs (N.B. it may still be pasted the end).
    // That is, if all of the entries are printed, we cannot end before
    // this point (unless we run out of entries in the trace).
    size_t const min_requested_end = start + length;
    size_t const bytes_per_trace_item =
        get_bytes_per_trace_item(args->trace_format);
    if (bytes_per_trace_item == 0) {
        return false;
    }
    size_t const num_entries = mm->num_bytes / bytes_per_trace_item;
    if (!print_header(stdout, num_entries)) {
        LOGGER_ERROR("failed to print header");
        return false;
    }
    if (start >= num_entries) {
        LOGGER_ERROR("invalid start (%zu) given only %zu entries",
                     start,
                     num_entries);
        return false;
    } else if (min_requested_end > num_entries) {
        LOGGER_WARN("start + length (%zu + %zu) exceeds the "
                    "number of entries (%zu)",
                    start,
                    length,
                    num_entries);
    }
    if (!args->count_printed_only) {
        size_t const end = MIN(min_requested_end, num_entries);
        for (size_t i = start; i < end; ++i) {
            maybe_print_entry(i, mm->buffer, args->command, args->trace_format);
        }
    } else {
        size_t count = 0;
        size_t i = start;
        while (count < length && i < num_entries) {
            bool r = maybe_print_entry(i,
                                       mm->buffer,
                                       args->command,
                                       args->trace_format);
            if (r) {
                ++count;
            }
            ++i;
        }
        // We got to the end of the file before reaching our desired
        //  number of entries to print.
        if (i == num_entries && count < length) {
            LOGGER_WARN("we only printed %zu of the requested %zu entries",
                        count,
                        length);
            return false;
        }
    }
    return true;
}

static bool
run(struct CommandLineArguments const *const args)
{
    struct MemoryMap mm = {0};
    if (args == NULL) {
        LOGGER_ERROR("args is NULL");
        goto cleanup_error;
    }
    LOGGER_INFO("CommandLineArguments(executable='%s', input='%s', "
                "trace_format='%s', start=%" PRId64 ", length=%" PRId64 ")",
                args->executable,
                args->input_path,
                TRACE_FORMAT_STRINGS[args->trace_format],
                args->start,
                args->length);
    // Memory map the input trace file
    if (!MemoryMap__init(&mm, args->input_path, "rb")) {
        LOGGER_ERROR("failed to mmap '%s'", args->input_path);
        goto cleanup_error;
    }
    if (!print_entries(args, &mm)) {
        LOGGER_ERROR("failed to print entries");
        goto cleanup_error;
    }
    MemoryMap__destroy(&mm);
    return true;
cleanup_error:
    MemoryMap__destroy(&mm);
    return false;
}

int
main(int argc, char **argv)
{
    struct CommandLineArguments args = parse_command_line_arguments(argc, argv);
    if (!run(&args)) {
        LOGGER_ERROR("runner failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
