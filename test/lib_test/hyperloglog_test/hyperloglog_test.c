#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "logger/logger.h"
#include "lookup/evicting_hash_table.h"
#include "lookup/hash_table.h"
#include "random/uniform_random.h"
#include "random/zipfian_random.h"
#include "shards/fixed_size_shards_sampler.h"
#include "test/mytester.h"
#include "trace/reader.h"

uint64_t const rng_seed = 42;
size_t const artificial_trace_length = 1 << 20;

static bool
test_hyperloglog_accuracy(char const *const fpath,
                          uint64_t (*f_next)(void *),
                          void *data)
{
    struct HashTable ht = {0};
    struct EvictingHashTable eht = {0};

    g_assert_true(HashTable__init(&ht));
    g_assert_true(EvictingHashTable__init(&eht, 1 << 13, 1.0));

    size_t *estimates = calloc(artificial_trace_length, 2 * sizeof(*estimates));
    assert(estimates);

    for (size_t i = 0; i < artificial_trace_length; ++i) {
        uint64_t const x = f_next(data);
        HashTable__put_unique(&ht, x, 0);
        EvictingHashTable__try_put(&eht, x, 0);

        size_t ht_size = g_hash_table_size(ht.hash_table);
        size_t eht_size =
            EvictingHashTable__estimate_scale_factor(&eht) * eht.length;
        estimates[2 * i + 0] = ht_size;
        estimates[2 * i + 1] = eht_size;
    }

    FILE *fp = fopen(fpath, "wb");
    assert(fp);
    if (fwrite(estimates,
               2 * sizeof(*estimates),
               artificial_trace_length,
               fp) != artificial_trace_length) {
        LOGGER_ERROR("incorrect number of elements written");
        return false;
    }
    if (fclose(fp) != 0) {
        LOGGER_ERROR("couldn't close");
        return false;
    }
    free(estimates);

    HashTable__destroy(&ht);
    EvictingHashTable__destroy(&eht);
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

    test_hyperloglog_accuracy("trace_hyperloglog_cardinalities.bin",
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

    test_hyperloglog_accuracy("uniform_hyperloglog_cardinalities.bin",
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

    test_hyperloglog_accuracy("zipfian_hyperloglog_cardinalities.bin",
                              (uint64_t(*)(void *))ZipfianRandom__next,
                              &zrng);

    ZipfianRandom__destroy(&zrng);
    return true;
}

int
main(int argc, char *argv[])
{
    g_assert_true(argc == 1 || argc == 2);
    if (argc == 2) {
        char const *const trace_path = argv[1];
        ASSERT_FUNCTION_RETURNS_TRUE(
            test_hyperloglog_accuracy_on_trace(trace_path, TRACE_FORMAT_KIA));
    }
    ASSERT_FUNCTION_RETURNS_TRUE(test_hyperloglog_accuracy_on_uniform());
    ASSERT_FUNCTION_RETURNS_TRUE(test_hyperloglog_accuracy_on_zipfian());
    return 0;
}
