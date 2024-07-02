/** @brief  Test the cardinalities of the hyperloglogs.
 *  @note   Yes, I know the way I clean up the files in the end is a
 *          complete hack.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "logger/logger.h"
#include "lookup/evicting_hash_table.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "random/uniform_random.h"
#include "random/zipfian_random.h"
#include "shards/fixed_size_shards_sampler.h"
#include "test/mytester.h"
#include "trace/reader.h"

static uint64_t rng_seed = 42;
static size_t artificial_trace_length = 1 << 20;
static double init_sampling_rate = 1e0;
static size_t max_size = 1 << 13;

static bool run_zipfian = false;
static bool run_uniform = false;
static char *trace_path = NULL;
// When cleanup is enable, we delete all the files we generate from this
// test.
static bool cleanup = false;

static char const *const TRACE_OUTPUT_PATH =
    "./trace_hyperloglog_cardinalities.bin";
static char const *const UNIFORM_OUTPUT_PATH =
    "./uniform_hyperloglog_cardinalities.bin";
static char const *const ZIPFIAN_OUTPUT_PATH =
    "./zipfian_hyperloglog_cardinalities.bin";

static GOptionEntry entries[] = {
    {"zipfian",
     'z',
     0,
     G_OPTION_ARG_NONE,
     &run_zipfian,
     "Run the test case with a Zipfian random trace",
     NULL},
    {"uniform",
     'u',
     0,
     G_OPTION_ARG_NONE,
     &run_uniform,
     "Run the test case with a Uniform random trace",
     NULL},
    {"trace",
     't',
     0,
     G_OPTION_ARG_FILENAME,
     &trace_path,
     "Path to the input trace",
     NULL},
    {"cleanup",
     0,
     0,
     G_OPTION_ARG_NONE,
     &cleanup,
     "Cleanup the output files",
     NULL},
    G_OPTION_ENTRY_NULL,
};

/// @brief  Test the cardinality estimation of various techniques.
static bool
test_hyperloglog_accuracy(char const *const fpath,
                          uint64_t (*f_next)(void *),
                          void *data)
{
    struct HashTable ht = {0};
    struct EvictingHashTable eht = {0};
    struct FixedSizeShardsSampler fs = {0};

    g_assert_true(HashTable__init(&ht));
    g_assert_true(EvictingHashTable__init(&eht, max_size, init_sampling_rate));
    g_assert_true(
        FixedSizeShardsSampler__init(&fs, init_sampling_rate, max_size, false));

    size_t *estimates = calloc(artificial_trace_length, 3 * sizeof(*estimates));
    assert(estimates);

    for (size_t i = 0; i < artificial_trace_length; ++i) {
        enum PutUniqueStatus s = LOOKUP_PUTUNIQUE_ERROR;
        uint64_t const x = f_next(data);
        s = HashTable__put_unique(&ht, x, 0);
        EvictingHashTable__try_put(&eht, x, 0);
        if (s == LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE &&
            FixedSizeShardsSampler__sample(&fs, x)) {
            FixedSizeShardsSampler__insert(&fs, x, NULL, NULL);
        }

        size_t ht_size = g_hash_table_size(ht.hash_table);
        // NOTE It's just math that this is the cardinality estimate.
        size_t eht_size =
            EvictingHashTable__estimate_scale_factor(&eht) * eht.num_inserted;
        size_t fs_size = FixedSizeShardsSampler__estimate_cardinality(&fs);
        estimates[3 * i + 0] = ht_size;
        estimates[3 * i + 1] = eht_size;
        estimates[3 * i + 2] = fs_size;
    }

    if (fpath != NULL) {
        FILE *fp = fopen(fpath, "wb");
        assert(fp);
        if (fwrite(estimates,
                   3 * sizeof(*estimates),
                   artificial_trace_length,
                   fp) != artificial_trace_length) {
            LOGGER_ERROR("incorrect number of elements written");
            return false;
        }
        if (fclose(fp) != 0) {
            LOGGER_ERROR("couldn't close");
            return false;
        }
    }
    free(estimates);

    HashTable__destroy(&ht);
    EvictingHashTable__destroy(&eht);
    FixedSizeShardsSampler__destroy(&fs);
    return true;
}

static uint64_t
next_trace_item(struct Trace *trace)
{
    static size_t i;

    if (i >= trace->length) {
        i = 0;
    }

    return trace->trace[i++].key;
}

static bool
test_hyperloglog_accuracy_on_trace(char const *const trace_path,
                                   enum TraceFormat trace_format)
{
    struct Trace trace = read_trace(trace_path, trace_format);
    assert(trace.trace != NULL && trace.length != 0);

    test_hyperloglog_accuracy(TRACE_OUTPUT_PATH,
                              (uint64_t(*)(void *))next_trace_item,
                              &trace);

    Trace__destroy(&trace);
    return true;
}

static bool
test_hyperloglog_accuracy_on_uniform(void)
{
    struct UniformRandom urng = {0};

    g_assert_true(UniformRandom__init(&urng, rng_seed));

    test_hyperloglog_accuracy(UNIFORM_OUTPUT_PATH,
                              (uint64_t(*)(void *))UniformRandom__next_uint64,
                              &urng);

    UniformRandom__destroy(&urng);
    return true;
}

static bool
test_hyperloglog_accuracy_on_zipfian(void)
{
    struct ZipfianRandom zrng = {0};

    g_assert_true(ZipfianRandom__init(&zrng, 1 << 20, 0.99, rng_seed));

    test_hyperloglog_accuracy(ZIPFIAN_OUTPUT_PATH,
                              (uint64_t(*)(void *))ZipfianRandom__next,
                              &zrng);

    ZipfianRandom__destroy(&zrng);
    return true;
}

int
main(int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;
    context = g_option_context_new("- test cardinality estimates");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        exit(1);
    }

    if (trace_path != NULL) {
        ASSERT_FUNCTION_RETURNS_TRUE(
            test_hyperloglog_accuracy_on_trace(trace_path, TRACE_FORMAT_KIA));
        if (cleanup && remove(TRACE_OUTPUT_PATH) != 0) {
            LOGGER_ERROR("failed to remove '%s'", TRACE_OUTPUT_PATH);
        }
    }
    if (run_uniform) {
        ASSERT_FUNCTION_RETURNS_TRUE(test_hyperloglog_accuracy_on_uniform());
        if (cleanup && remove(UNIFORM_OUTPUT_PATH) != 0) {
            LOGGER_ERROR("failed to remove '%s'", UNIFORM_OUTPUT_PATH);
        }
    }
    if (run_zipfian) {
        ASSERT_FUNCTION_RETURNS_TRUE(test_hyperloglog_accuracy_on_zipfian());
        if (cleanup && remove(ZIPFIAN_OUTPUT_PATH) != 0) {
            LOGGER_ERROR("failed to remove '%s'", ZIPFIAN_OUTPUT_PATH);
        }
    }

    return 0;
}
