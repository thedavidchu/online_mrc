#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "lookup/lookup.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

#include <glib.h>

struct HashTable {
    GHashTable *hash_table;
};

bool
HashTable__init(struct HashTable *me);

size_t
HashTable__get_size(struct HashTable const *const me);

struct LookupReturn
HashTable__lookup(struct HashTable *me, EntryType key);

enum PutUniqueStatus
HashTable__put_unique(struct HashTable *me, EntryType key, TimeStampType value);

struct LookupReturn
HashTable__remove(struct HashTable *const me, EntryType const key);

void
HashTable__write_as_json(FILE *stream, struct HashTable const *const me);

void
HashTable__destroy(struct HashTable *me);
