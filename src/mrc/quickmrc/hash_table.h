#pragma once

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "tree/types.h"
#include "types/time_stamp_type.h"

struct HashNode {
    TimeStampType timestamp;
    Hash64BitType hash;
    KeyType key;
};

struct HashTable {
    struct HashNode **data; /* Array of pointers */
    size_t length;
};

bool
HashTable__init(struct HashTable *me, size_t max_members);

enum FlakyReturn {
    FLAKY_ERROR,
    INSERT_NEW,
    MODIFY_OLD,
    REPLACE_OLD,
    BLOCKED_BY_NEW
};

/**
 *  @brief  This overwrites upon a collision.
 *  @return See definition of FlakyReturn
 */
enum FlakyReturn
HashTable__flaky_insert(struct HashTable *me,
                        KeyType key,
                        TimeStampType timestamp);

void
HashTable__println(struct HashTable *me);

void
HashTable__destroy(struct HashTable *me);
