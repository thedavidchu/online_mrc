#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "khash.h"
#include "lookup/lookup.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

KHASH_MAP_INIT_INT64(64, uint64_t)

/// @brief  This implemenents a hash table with key and values of type
///         uint64_t. It uses the klib library backend.
struct KHashTable {
    khash_t(64) * hash_table;
};

bool
KHashTable__init(struct KHashTable *me);

size_t
KHashTable__get_size(struct KHashTable const *const me);

struct LookupReturn
KHashTable__lookup(struct KHashTable *me, EntryType key);

/// @return Returns whether we inserted, replaced, or errored.
enum PutUniqueStatus
KHashTable__put_unique(struct KHashTable *me,
                       EntryType key,
                       TimeStampType value);

struct LookupReturn
KHashTable__remove(struct KHashTable *const me, EntryType const key);

bool
KHashTable__write(struct KHashTable const *const me,
                  FILE *const stream,
                  bool const newline);

void
KHashTable__destroy(struct KHashTable *me);
