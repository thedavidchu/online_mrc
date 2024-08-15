#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>

#include "lookup/lookup.h"
#include "types/key_type.h"
#include "types/value_type.h"

struct BoostHashTable *
BoostHashTable__new(void);

void
BoostHashTable__free(struct BoostHashTable *const me);

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

#ifdef __cplusplus
}
#endif
