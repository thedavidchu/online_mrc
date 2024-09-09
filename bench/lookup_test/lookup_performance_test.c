/** @brief  Test the performance and distribution of various lookup utilities.
 *
 *  @note   Perform memory tests on the various algorithms by running
 *          the following:
 *          `/usr/bin/time -v <exe> {boost,k,g}
 *          Look for the 'Maximum resident set size (kbytes)'.
 *          It is important to type the full path '/usr/bin/time' since
 *          you do not want to confuse it with Bash's built-in 'time'.
 *          Source:
 *          https://stackoverflow.com/questions/774556/peak-memory-usage-of-a-linux-unix-process
 */

#include <stddef.h>
#include <stdint.h>

#include "logger/logger.h"
#include "lookup/boost_hash_table.h"
#include "lookup/hash_table.h"
#include "lookup/k_hash_table.h"
#include "lookup/lookup.h"
#include "timer/timer.h"

size_t const NUM_VALUES_FOR_PERF = 1 << 20;

/// @brief  Test the performance of various domains of the hash table.
/// @note   I do not test remove operations, because these are not
///         important to me at the moment. I do not use remove
///         operations frequently.
static inline void
time_hash_table(char const *const name,
                void *const object,
                enum PutUniqueStatus (*put)(void *const object,
                                            uint64_t const key,
                                            uint64_t const value),
                struct LookupReturn (*lookup)(void const *const object,
                                              uint64_t const key),
                void (*destroy)(void *const object))
{
    double const t0 = get_wall_time_sec();
    for (size_t i = 0; i < NUM_VALUES_FOR_PERF; ++i) {
        put(object, i, i);
    }
    double const t1 = get_wall_time_sec();
    for (size_t i = 0; i < NUM_VALUES_FOR_PERF; ++i) {
        put(object, i, 2 * i);
    }
    double const t2 = get_wall_time_sec();
    for (size_t i = 0; i < NUM_VALUES_FOR_PERF; ++i) {
        lookup(object, i);
    }
    double const t3 = get_wall_time_sec();
    for (size_t i = 0; i < NUM_VALUES_FOR_PERF; ++i) {
        lookup(object, i + NUM_VALUES_FOR_PERF);
    }
    double const t4 = get_wall_time_sec();
    destroy(object);
    double const t5 = get_wall_time_sec();
    LOGGER_INFO("%s -- insert time: %f | replace time: %f | lookup time: %f | "
                "lookup miss time: %f | destroy time: %f | total time: %f",
                name,
                t1 - t0,
                t2 - t1,
                t3 - t2,
                t4 - t3,
                t5 - t4,
                t5 - t0);
}

enum LookupType {
    BOOST_HASH_TABLE,
    K_HASH_TABLE,
    G_HASH_TABLE,
};

void
run(enum LookupType const lookup_type)
{
    switch (lookup_type) {
    case BOOST_HASH_TABLE: {
        struct BoostHashTable bht = {0};
        BoostHashTable__init(&bht);
        time_hash_table(
            "Boost Hash Table",
            &bht,
            (enum PutUniqueStatus(*)(void *const,
                                     uint64_t const,
                                     uint64_t const))BoostHashTable__put,
            (struct LookupReturn(*)(void const *const,
                                    uint64_t const))BoostHashTable__lookup,
            (void (*)(void *const))BoostHashTable__destroy);
        break;
    }
    case K_HASH_TABLE: {
        struct KHashTable kht = {0};
        KHashTable__init(&kht);
        time_hash_table(
            "KLib Hash Table",
            &kht,
            (enum PutUniqueStatus(*)(void *const,
                                     uint64_t const,
                                     uint64_t const))KHashTable__put,
            (struct LookupReturn(*)(void const *const,
                                    uint64_t const))KHashTable__lookup,
            (void (*)(void *const))KHashTable__destroy);
        break;
    }
    case G_HASH_TABLE: {
        struct HashTable ght = {0};
        HashTable__init(&ght);
        time_hash_table(
            "GLib Hash Table",
            &ght,
            (enum PutUniqueStatus(*)(void *const,
                                     uint64_t const,
                                     uint64_t const))HashTable__put,
            (struct LookupReturn(*)(void const *const,
                                    uint64_t const))HashTable__lookup,
            (void (*)(void *const))HashTable__destroy);
        break;
    }
    default:
        assert(0);
    }
}

int
main(int argc, char **argv)
{
    if (argc == 1) {
        run(BOOST_HASH_TABLE);
        run(G_HASH_TABLE);
        run(K_HASH_TABLE);
        return 0;
    }
    for (int i = 1; i < argc; ++i) {
        assert(argv[i] != NULL);
        if (strcmp(argv[i], "boost") == 0) {
            run(BOOST_HASH_TABLE);
        } else if (strcmp(argv[i], "k") == 0) {
            run(K_HASH_TABLE);
        } else if (strcmp(argv[i], "g") == 0) {
            run(G_HASH_TABLE);
        } else {
            LOGGER_WARN("skipping unrecognized argument '%s'. Try any "
                        "combination of 'boost', 'k', or 'g' (e.g. '%s boost k "
                        "g'); or enter no arguments to run everything.",
                        argv[i],
                        argv[0]);
        }
    }
    return 0;
}
