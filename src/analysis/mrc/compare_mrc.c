/**
 *  @brief  This file compares the accuracy of MRC files.
 */
#include <stdlib.h>

#include <glib.h>

#include "file/file.h"
#include "logger/logger.h"
#include "miss_rate_curve/miss_rate_curve.h"

struct CommandLineArguments {
    char *executable;

    // Inputs
    char *oracle_path;
    char *test_path;
};

static struct CommandLineArguments
parse_command_line_arguments(int argc, char **argv)
{
    gchar *help_msg = NULL;

    // Set defaults.
    struct CommandLineArguments args = {.executable = argv[0],
                                        .oracle_path = NULL,
                                        .test_path = NULL};

    // Command line options.
    GOptionEntry entries[] = {
        {"oracle",
         0,
         0,
         G_OPTION_ARG_FILENAME,
         &args.oracle_path,
         "path to the oracle MRC",
         NULL},
        {"test",
         0,
         0,
         G_OPTION_ARG_STRING,
         &args.test_path,
         "path to the MRC to test",
         NULL},
        G_OPTION_ENTRY_NULL,
    };

    GError *error = NULL;
    GOptionContext *context;
    context = g_option_context_new("- analyze MRC accuracy");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        goto cleanup;
    }
    // Come on, GLib! The 'g_option_context_parse' changes the errno to
    // 2 and leaves it for me to clean up. Or maybe I'm using it wrong.
    errno = 0;
    g_option_context_free(context);

    // Check the arguments for correctness.
    if (args.oracle_path == NULL || args.test_path == NULL) {
        LOGGER_ERROR("invalid MRC oracle path '%s' or test path '%s'",
                     args.oracle_path == NULL ? "(null)" : args.oracle_path,
                     args.test_path == NULL ? "(null)" : args.test_path);
        goto cleanup;
    }
    if (!file_exists(args.oracle_path)) {
        LOGGER_ERROR("input MRC path '%s' DNE", args.oracle_path);
        goto cleanup;
    }
    if (!file_exists(args.test_path)) {
        LOGGER_ERROR("input MRC path '%s' DNE", args.test_path);
        goto cleanup;
    }
    return args;
cleanup:
    help_msg = g_option_context_get_help(context, FALSE, NULL);
    g_print("%s", help_msg);
    free(help_msg);
    exit(-1);
}

int
main(int argc, char **argv)
{
    struct CommandLineArguments args = {0};
    args = parse_command_line_arguments(argc, argv);

    struct MissRateCurve oracle = {0};
    struct MissRateCurve test = {0};

    if (!MissRateCurve__load(&oracle, args.oracle_path)) {
        LOGGER_ERROR("failed to load oracle from '%s'", args.oracle_path);
        return EXIT_FAILURE;
    }
    if (!MissRateCurve__load(&test, args.test_path)) {
        LOGGER_ERROR("failed to load test from '%s'", args.test_path);
        return EXIT_FAILURE;
    }

    double const mae = MissRateCurve__mean_absolute_error(&oracle, &test);
    double const mse = MissRateCurve__mean_squared_error(&oracle, &test);
    LOGGER_INFO("MAE: %g | MSE: %g", mae, mse);

    return 0;
}
