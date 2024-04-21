#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "hash_table.h"

bool
HashTable__init(struct HashTable *me, size_t max_members)
{
    *me = (struct HashTable){
        .data = (struct HashNode **)calloc(max_members, sizeof(void *)),
        .length = max_members,
    };

    if (me->data == NULL) {
        return false;
    }
    return true;
}

static Hash64BitType
hash_function(KeyType key)
{
#ifdef QUICK_TEST
    return key;
#else
    return splitmix64_hash(key);
#endif /* RELEASE */
}

enum FlakyReturn
HashTable__flaky_insert(struct HashTable *me,
                        KeyType key,
                        TimeStampType timestamp)
{
    if (me == NULL || me->data == NULL || me->length == 0) {
        return FLAKY_ERROR;
    }

    Hash64BitType hash = hash_function(key);

    struct HashNode *new_node = (struct HashNode *)malloc(sizeof(*new_node));
    if (new_node == NULL) {
        return FLAKY_ERROR;
    }
    *new_node =
        (struct HashNode){.hash = hash, .timestamp = timestamp, .key = key};

    struct HashNode **const node_ptr = &me->data[hash % me->length];
    struct HashNode *old_node = *node_ptr;

    /* Try to insert a new node (likely to fail!) */
    // NOTE This additional check is an attempt at optimization! (c.f. TTS lock)
    if (old_node == NULL) {
        if (__sync_bool_compare_and_swap(node_ptr, NULL, new_node)) {
            return INSERT_NEW;
        }
    }

    /** Replaces the incumbent if:
     *      1. The keys match and the incumbent timestamp is greater
     *      2. The incumbent hash is greater.
     *  Therefore, exit if:
     *      1. The keys match but the incumbent timestamp is lesser (being the
     *          same is erroneous)
     *      2. The incumbent hash is lesser or equal.
     */
    do {
        // Check exit conditions
        old_node = *node_ptr;
        assert(old_node->timestamp != timestamp &&
               "timestamps should be unique!");
        if (old_node->key == key && old_node->timestamp < timestamp) {
            free(new_node);
            return BLOCKED_BY_NEW;
        }
        if (old_node->hash <= hash) {
            free(new_node);
            return BLOCKED_BY_NEW;
        }
    } while (!__sync_bool_compare_and_swap(node_ptr, old_node, new_node));

    // Return results based on the old_node's values
    if (old_node->key == key && old_node->timestamp < timestamp) {
        free(old_node);
        return MODIFY_OLD;
    } else if (old_node->hash > hash) {
        free(old_node);
        return REPLACE_OLD;
    } else {
        return FLAKY_ERROR;
    }
}

void
HashTable__println(struct HashTable *me)
{
    if (me == NULL || me->data == NULL || me->length == 0) {
        return;
    }

    printf("[%zu]{", me->length);
    for (size_t i = 0; i < me->length; ++i) {
        if (me->data[i] == NULL) {
            printf("%p, ", NULL);
        } else {
            printf("%zu/%zu: %zu, ",
                   me->data[i]->key,
                   me->data[i]->hash,
                   me->data[i]->timestamp);
        }
    }
    printf("}\n");
}

void
HashTable__destroy(struct HashTable *me)
{
    if (me == NULL) {
        return;
    }
    for (size_t i = 0; i < me->length; ++i) {
        free(me->data[i]);
    }
    free(me->data);
    *me = (struct HashTable){0};
}

#ifdef QUICK_TEST
int
main(void)
{
    struct HashTable a = {0};
    HashTable__init(&a, 8);
    HashTable__println(&a);

    for (KeyType i = 17; i != 0; --i) {
        HashTable__flaky_insert(&a, i, i);
        HashTable__println(&a);
    }

    HashTable__destroy(&a);
    return 0;
}
#endif /* QUICK_TEST */
