/** @brief  Test the performance and distribution of various lookup utilities.
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

int
main(void)
{
    struct BoostHashTable bht = {0};
    BoostHashTable__init(&bht);
    time_hash_table(
        "Boost Hash Table",
        &bht,
        (enum PutUniqueStatus(*)(void *const, uint64_t const, uint64_t const))
            BoostHashTable__put,
        (struct LookupReturn(*)(void const *const,
                                uint64_t const))BoostHashTable__lookup,
        (void (*)(void *const))BoostHashTable__destroy);

    struct HashTable ght = {0};
    HashTable__init(&ght);
    time_hash_table("GLib Hash Table",
                    &ght,
                    (enum PutUniqueStatus(*)(void *const,
                                             uint64_t const,
                                             uint64_t const))HashTable__put,
                    (struct LookupReturn(*)(void const *const,
                                            uint64_t const))HashTable__lookup,
                    (void (*)(void *const))HashTable__destroy);

    struct KHashTable kht = {0};
    KHashTable__init(&kht);
    time_hash_table("KLib Hash Table",
                    &kht,
                    (enum PutUniqueStatus(*)(void *const,
                                             uint64_t const,
                                             uint64_t const))KHashTable__put,
                    (struct LookupReturn(*)(void const *const,
                                            uint64_t const))KHashTable__lookup,
                    (void (*)(void *const))KHashTable__destroy);

    return 0;
}
