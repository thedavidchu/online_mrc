#include <assert.h>

#include <glib.h>
#include <stdint.h>

#include "file/file.h"
#include "histogram/histogram.h"
#include "io/io.h"
#include "logger/logger.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "trace/reader.h"

struct CommandLineArguments {
    char *executable;
    gchar *trace_path;
    gchar *hist_path;
    gchar *rd_hist_path;
    gchar *wr_hist_path;
    enum TraceFormat trace_format;
};

/// @note   Copired from '//src/run/generate_mrc.c'. Adapted for this use case.
static struct CommandLineArguments
parse_command_line_arguments(int argc, char *argv[])
{
    gchar *help_msg = NULL;

    // Set defaults.
    struct CommandLineArguments args = {.executable = argv[0],
                                        .trace_path = NULL,
                                        .trace_format = TRACE_FORMAT_KIA,
                                        .hist_path = NULL,
                                        .rd_hist_path = NULL,
                                        .wr_hist_path = NULL};
    gchar *trace_format = NULL;

    // Command line options.
    GOptionEntry entries[] = {
        // Arguments related to the input generation
        {"trace",
         't',
         0,
         G_OPTION_ARG_FILENAME,
         &args.trace_path,
         "path to the input trace",
         NULL},
        {"full-histogram",
         'g',
         0,
         G_OPTION_ARG_FILENAME,
         &args.hist_path,
         "path to the output read/write histogram",
         NULL},
        {"read-histogram",
         'r',
         0,
         G_OPTION_ARG_FILENAME,
         &args.rd_hist_path,
         "path to the output read histogram",
         NULL},
        {"write-histogram",
         'w',
         0,
         G_OPTION_ARG_FILENAME,
         &args.wr_hist_path,
         "path to the output write histogram",
         NULL},
        {"format",
         'f',
         0,
         G_OPTION_ARG_STRING,
         &trace_format,
         "format of the input trace. Options: {Kia,Sari}. Default: Kia.",
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
    if (args.trace_path == NULL || !file_exists(args.trace_path)) {
        LOGGER_ERROR("input trace path '%s' DNE",
                     args.trace_path == NULL ? "(null)" : args.trace_path);
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
    if (args.hist_path == NULL || args.rd_hist_path == NULL ||
        args.wr_hist_path == NULL) {
        LOGGER_ERROR("require histogram paths!");
        goto cleanup;
    }
    if (file_exists(args.hist_path) || file_exists(args.rd_hist_path) ||
        file_exists(args.wr_hist_path)) {
        LOGGER_ERROR("histogram file(s) exist(s) already!");
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
run(struct CommandLineArguments const *const args)
{
    struct MemoryMap mm = {0};
    struct HashTable ht = {0};
    struct HashTable rd_ht = {0};
    struct HashTable wr_ht = {0};
    struct Histogram hg = {0};
    struct Histogram rd_hg = {0};
    struct Histogram wr_hg = {0};
    size_t bytes_per_trace_item = 0;
    size_t num_entries = 0;

    if (args == NULL) {
        LOGGER_ERROR("args is NULL");
        goto cleanup_error;
    }

    LOGGER_INFO("CommandLineArguments(executable='%s', trace_path='%s', "
                "trace_format='%s', histogram_path='%s', "
                "read_histogram_path='%s', write_histogram_path='%s')",
                args->executable,
                args->trace_path,
                TRACE_FORMAT_STRINGS[args->trace_format],
                args->hist_path,
                args->rd_hist_path,
                args->wr_hist_path);
    bytes_per_trace_item = get_bytes_per_trace_item(args->trace_format);
    if (bytes_per_trace_item == 0) {
        LOGGER_ERROR("invalid input", args->trace_format);
        goto cleanup_error;
    }

    if (!HashTable__init(&ht) || !HashTable__init(&rd_ht) ||
        !HashTable__init(&wr_ht)) {
        LOGGER_ERROR("failed to init hash table");
        goto cleanup_error;
    }
    if (!Histogram__init(&hg, 1 << 20, 1, HistogramOutOfBoundsMode__realloc) ||
        !Histogram__init(&rd_hg,
                         1 << 20,
                         1,
                         HistogramOutOfBoundsMode__realloc) ||
        !Histogram__init(&wr_hg,
                         1 << 20,
                         1,
                         HistogramOutOfBoundsMode__realloc)) {
        LOGGER_ERROR("failed to init histogram");
        goto cleanup_error;
    }

    // Memory map the input trace file
    if (!MemoryMap__init(&mm, args->trace_path, "rb")) {
        LOGGER_ERROR("failed to mmap '%s'", args->trace_path);
        goto cleanup_error;
    }
    num_entries = mm.num_bytes / bytes_per_trace_item;

    LOGGER_INFO("length [entries]: %zu", num_entries);
    for (size_t i = 0; i < num_entries; ++i) {
        struct FullTraceItemResult r = construct_full_trace_item(
            &((uint8_t *)mm.buffer)[i * bytes_per_trace_item],
            args->trace_format);
        assert(r.valid);

        // Normal lookup
        struct LookupReturn s = HashTable__lookup(&ht, r.item.key);
        if (s.success) {
            Histogram__insert_finite(&hg, r.item.timestamp_ms - s.timestamp);
        } else {
            Histogram__insert_infinite(&hg);
        }
        enum PutUniqueStatus t =
            HashTable__put(&ht, r.item.key, r.item.timestamp_ms);
        assert(t == LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE ||
               t == LOOKUP_PUTUNIQUE_REPLACE_VALUE);
        if (r.item.command == 0) {
            // Normal lookup
            struct LookupReturn s = HashTable__lookup(&rd_ht, r.item.key);
            if (s.success) {
                Histogram__insert_finite(&rd_hg,
                                         r.item.timestamp_ms - s.timestamp);
            } else {
                Histogram__insert_infinite(&rd_hg);
            }
            enum PutUniqueStatus t =
                HashTable__put(&rd_ht, r.item.key, r.item.timestamp_ms);
            assert(t == LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE ||
                   t == LOOKUP_PUTUNIQUE_REPLACE_VALUE);
        } else {
            // Normal lookup
            struct LookupReturn s = HashTable__lookup(&wr_ht, r.item.key);
            if (s.success) {
                Histogram__insert_finite(&wr_hg,
                                         r.item.timestamp_ms - s.timestamp);
            } else {
                Histogram__insert_infinite(&wr_hg);
            }
            enum PutUniqueStatus t =
                HashTable__put(&wr_ht, r.item.key, r.item.timestamp_ms);
            assert(t == LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE ||
                   t == LOOKUP_PUTUNIQUE_REPLACE_VALUE);
        }
    }

    Histogram__save(&hg, args->hist_path);
    Histogram__save(&rd_hg, args->rd_hist_path);
    Histogram__save(&wr_hg, args->wr_hist_path);

    MemoryMap__destroy(&mm);
    HashTable__destroy(&ht);
    HashTable__destroy(&rd_ht);
    HashTable__destroy(&wr_ht);
    Histogram__destroy(&hg);
    Histogram__destroy(&rd_hg);
    Histogram__destroy(&wr_hg);
    return true;
cleanup_error:
    MemoryMap__destroy(&mm);
    HashTable__destroy(&ht);
    HashTable__destroy(&rd_ht);
    HashTable__destroy(&wr_ht);
    Histogram__destroy(&hg);
    Histogram__destroy(&rd_hg);
    Histogram__destroy(&wr_hg);
    return false;
}

int
main(int argc, char *argv[])
{
    struct CommandLineArguments args = parse_command_line_arguments(argc, argv);
    if (!run(&args)) {
        LOGGER_ERROR("runner failed");
        return EXIT_FAILURE;
    }
    LOGGER_INFO("=== SUCCESS ===");
    return EXIT_SUCCESS;
}
