#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "arrays/array_size.h"
#include "interval/interval_olken.h"
#include "invariants/implies.h"
#include "io/io.h"
#include "logger/logger.h"
#include "trace/reader.h"
#include "trace/trace.h"
#include "unused/mark_unused.h"

/// @brief  Create record of reuse distances and times.
bool
generate_reuse_stats(struct Trace *trace, char const *const fname)
{
    if (trace == NULL || !implies(trace->length != 0, trace->trace != NULL)) {
        LOGGER_ERROR("invalid trace");
        return false;
    }

    struct IntervalOlken me = {0};
    bool r = IntervalOlken__init(&me, trace->length);
    if (!r) {
        LOGGER_ERROR("bad init");
        return false;
    }

    for (size_t i = 0; i < trace->length; ++i) {
        IntervalOlken__access_item(&me, trace->trace[i].key);
    }

    write_buffer(fname, me.stats, me.length, sizeof(*me.stats));
    IntervalOlken__destroy(&me);
    return true;
}

#if 1
static gchar *input_path = NULL;
static gchar *output_path = NULL;
static gchar *input_format = "Kia";

static GOptionEntry entries[] = {
    {"input",
     'i',
     0,
     G_OPTION_ARG_FILENAME,
     &input_path,
     "input path to the trace",
     "<input-path>"},
    {"output",
     'o',
     0,
     G_OPTION_ARG_FILENAME,
     &output_path,
     "output path to the interval-based histogram",
     "<output-path>"},
    {"format",
     'f',
     0,
     G_OPTION_ARG_STRING,
     &input_format,
     "format of the input file, either {Kia,Sari}. Default: Kia",
     "<input-format>"},
    G_OPTION_ENTRY_NULL,
};

static enum TraceFormat
parse_input_format_string(gchar *format_str)
{
    for (size_t i = 1; i < ARRAY_SIZE(TRACE_FORMAT_STRINGS); ++i) {
        if (strcmp(TRACE_FORMAT_STRINGS[i], format_str) == 0)
            return (enum TraceFormat)i;
    }
    LOGGER_ERROR("unparsable format string: '%s'", format_str);
    exit(-1);
}

static bool
verify_path(gchar const *const path)
{
    return path != NULL;
}

int
main(int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;
    context = g_option_context_new("- analyze MRC in intervals");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        exit(1);
    }

    bool is_error = false;
    if (!verify_path(input_path)) {
        LOGGER_ERROR("invalid input path");
        is_error = true;
    }
    if (!verify_path(output_path)) {
        LOGGER_ERROR("invalid output path");
        is_error = true;
    }
    if (is_error) {
        gchar *help_msg = g_option_context_get_help(context, FALSE, NULL);
        g_print("%s", help_msg);
        free(help_msg);
        exit(-1);
    }

    enum TraceFormat format = parse_input_format_string(input_format);
    struct Trace trace = read_trace(input_path, format);
    generate_reuse_stats(&trace, output_path);

    return 0;
}
#endif
