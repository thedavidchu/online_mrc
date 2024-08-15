#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>

#include "lookup/lookup.h"
#include "types/key_type.h"
#include "types/value_type.h"

struct BoostHashTablePrivate;

struct BoostHashTable {
    // NOTE I am uninspired and named the private member 'private'. This
    //      is a keyword in C++, so I need to add the trailing underscore.
    struct BoostHashTablePrivate *private_;
};

bool
BoostHashTable__init(struct BoostHashTable *const me);

void
BoostHashTable__destroy(struct BoostHashTable *const me);

struct LookupReturn
BoostHashTable__lookup(struct BoostHashTable const *const me,
                       KeyType const key);

enum PutUniqueStatus
BoostHashTable__put(struct BoostHashTable *const me,
                    KeyType const key,
                    ValueType const value);

struct LookupReturn
BoostHashTable__remove(struct BoostHashTable *const me, KeyType const key);

bool
BoostHashTable__write(struct BoostHashTable const *const me,
                      FILE *const stream,
                      bool const newline);

size_t
BoostHashTable__get_size(struct BoostHashTable const *const me);

#ifdef __cplusplus
}
#endif
