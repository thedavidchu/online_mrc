#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lookup/lookup.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

// HACK This is the post-macro name of the khash_t(64). This is simply
//      to avoid dumping the namespace of 'khash.h' into all downstream
//      headers. In theory, this is a pointer anyways, so maybe I can
//      just have a 'void *'. Who knows?
struct kh_64_s;

/// @brief  This implemenents a hash table with key and values of type
///         uint64_t. It uses the klib library backend.
struct KHashTable {
    struct kh_64_s *hash_table;
};

bool
KHashTable__init(struct KHashTable *const me);

size_t
KHashTable__get_size(struct KHashTable const *const me);

struct LookupReturn
KHashTable__lookup(struct KHashTable const *const me, EntryType key);

/// @return Returns whether we inserted, replaced, or errored.
enum PutUniqueStatus
KHashTable__put_unique(struct KHashTable *const me,
                       EntryType const key,
                       TimeStampType const value);

struct LookupReturn
KHashTable__remove(struct KHashTable *const me, EntryType const key);

bool
KHashTable__write(struct KHashTable const *const me,
                  FILE *const stream,
                  bool const newline);

void
KHashTable__destroy(struct KHashTable *const me);
