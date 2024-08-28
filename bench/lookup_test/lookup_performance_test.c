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

static inline void
time_lookup(char const *const name,
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
    for (size_t i = 0; i < NUM_VALUES_FOR_PERF; ++i) {
        lookup(object, i);
    }
    double const t1 = get_wall_time_sec();
    destroy(object);
    LOGGER_INFO("%s time: %f", name, t1 - t0);
}

int
main(void)
{
    struct BoostHashTable bht = {0};
    BoostHashTable__init(&bht);
    time_lookup("Boost Hash Table",
                &bht,
                (enum PutUniqueStatus(*)(void *const,
                                         uint64_t const,
                                         uint64_t const))BoostHashTable__put,
                (struct LookupReturn(*)(void const *const,
                                        uint64_t const))BoostHashTable__lookup,
                (void (*)(void *const))BoostHashTable__destroy);

    struct HashTable ght = {0};
    HashTable__init(&ght);
    time_lookup("GLib Hash Table",
                &ght,
                (enum PutUniqueStatus(*)(void *const,
                                         uint64_t const,
                                         uint64_t const))HashTable__put,
                (struct LookupReturn(*)(void const *const,
                                        uint64_t const))HashTable__lookup,
                (void (*)(void *const))HashTable__destroy);

    struct KHashTable kht = {0};
    KHashTable__init(&kht);
    time_lookup("KLib Hash Table",
                &kht,
                (enum PutUniqueStatus(*)(void *const,
                                         uint64_t const,
                                         uint64_t const))KHashTable__put,
                (struct LookupReturn(*)(void const *const,
                                        uint64_t const))KHashTable__lookup,
                (void (*)(void *const))KHashTable__destroy);

    return 0;
}
