#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "tree/naive_tree.h"

struct NaiveSubtree {
    KeyType key;
    size_t cardinality;
    struct NaiveSubtree *left_subtree;
    struct NaiveSubtree *right_subtree;
};

struct NaiveTree {
    struct NaiveSubtree *root;
    size_t cardinality;
};

struct RemoveStatus {
    struct NaiveSubtree *removed;
    struct NaiveSubtree *new_child;
};

struct NaiveSubtree *
subtree_new(KeyType key)
{
    struct NaiveSubtree *subtree = (struct NaiveSubtree *)malloc(sizeof(struct NaiveSubtree));
    if (subtree == NULL) {
        assert(0 && "OOM error!");
        return NULL;
    }
    subtree->key = key;
    subtree->cardinality = 1;
    subtree->left_subtree = NULL;
    subtree->right_subtree = NULL;
    return subtree;
}

struct NaiveTree *
tree_new()
{
    struct NaiveTree *r = (struct NaiveTree *)malloc(sizeof(struct NaiveTree));
    if (r == NULL) {
        assert(0 && "OOM error!");
        return NULL;
    }
    r->cardinality = 0;
    r->root = NULL;
    return r;
}

size_t
tree_cardinality(struct NaiveTree *me)
{
    return me->cardinality;
}

static bool
subtree_insert(struct NaiveSubtree *me, KeyType key)
{
    struct NaiveSubtree **next_subtree;
    if (me == NULL) {
        // N.B. This should only be called if the first invocation passes a NULL
        return false;
    } else if (key < me->key) {
        next_subtree = &me->left_subtree;
    } else if (me->key < key) {
        next_subtree = &me->right_subtree;
    } else {
        return false;
    }

    if (*next_subtree == NULL) {
        *next_subtree = subtree_new(key);
        if (*next_subtree == NULL) {
            return false;   // OOM error!
        }
        ++me->cardinality;
        return true;
    } else {
        bool r = subtree_insert(*next_subtree, key);
        if (r) {
            ++me->cardinality;
        }
        return r;
    }
}

bool
tree_insert(struct NaiveTree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    if (me->root == NULL) {
        me->root = subtree_new(key);
        if (me->root == NULL) {
            return false;
        }
        ++me->cardinality;
        return true;
    }
    bool r = subtree_insert(me->root,key);
    if (r) {
        ++me->cardinality;
    }
    return r;
}

static bool
subtree_search(struct NaiveSubtree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    } else if (key < me->key) {
        return subtree_search(me->left_subtree, key);
    } else if (me->key < key) {
        return subtree_search(me->right_subtree, key);
    } else {
        return true;
    }
}

bool
tree_search(struct NaiveTree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    return subtree_search(me->root, key);
}

static struct RemoveStatus
subtree_pop_minimum(struct NaiveSubtree *me)
{
    if (me == NULL) {
        // NOTE This should only be called if this function is originally called
        //      with a NULL argument. No recursive call should pass NULL.
        return (struct RemoveStatus) {.removed = NULL, .new_child = NULL};
    } else if (me->left_subtree == NULL) {
        return (struct RemoveStatus) {.removed = me, .new_child = me->right_subtree};
    } else {
        struct RemoveStatus r = subtree_pop_minimum(me->left_subtree);
        assert(r.removed != NULL &&
                "should have popped an item from the non-empty subtree");
        --me->cardinality;
        me->left_subtree = r.new_child;
        r.new_child = me;
        return r;
    }
}

static struct RemoveStatus
subtree_remove(struct NaiveSubtree *me, KeyType key)
{
    if (me == NULL) {
        return (struct RemoveStatus) {.removed = false, .new_child = NULL};
    } else if (key < me->key) {
        struct RemoveStatus r = subtree_remove(me->left_subtree, key);
        if (r.removed) {
            --me->cardinality;
        }
        // NOTE Automatically overwriting the rather than checking may cause the
        //      cache line to be needlessly dirtied.
        me->left_subtree = r.new_child;
        r.new_child = me;
        return r;
    } else if (me->key < key) {
        struct RemoveStatus r = subtree_remove(me->right_subtree, key);
        if (r.removed) {
            --me->cardinality;
        }
        // NOTE Automatically overwriting the rather than checking may cause the
        //      cache line to be needlessly dirtied.
        me->right_subtree = r.new_child;
        r.new_child = me;
        return r;
    } else {
        if (me->left_subtree == NULL && me->right_subtree == NULL) {
            // Is a leaf
            return (struct RemoveStatus) {.removed = me, .new_child = NULL};
        } else if (me->left_subtree != NULL) {
            // Single child
            struct NaiveSubtree *single_child = me->left_subtree;
            return (struct RemoveStatus) {.removed = me, .new_child = single_child};
        } else if (me->right_subtree != NULL) {
            // Single child
            struct NaiveSubtree *single_child = me->right_subtree;
            return (struct RemoveStatus) {.removed = me, .new_child = single_child};
        } else {
            // Double child => recursively replace with successor (arbitrarily
            // chose the successor rather than the predecessor).
            struct RemoveStatus r = subtree_pop_minimum(me->right_subtree);
            struct NaiveSubtree *replacement_node = r.removed;
            replacement_node->cardinality = me->cardinality - 1;
            replacement_node->left_subtree = me->left_subtree;
            replacement_node->right_subtree = r.new_child;
            return (struct RemoveStatus) {.removed = me, .new_child = replacement_node};
        }
    }
}

bool
tree_remove(struct NaiveTree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    struct RemoveStatus r = subtree_remove(me->root, key);
    if (r.removed == NULL) {
        return false;
    }
    me->root = r.new_child;
    --me->cardinality;
    free(r.removed);
    return true;
}

static void
subtree_print(struct NaiveSubtree *me)
{
    if (me == NULL) {
        printf("null");
        return;
    }
    printf("{\"key\": %zu, \"cardinality\": %zu, \"left\": ", me->key, me->cardinality);
    subtree_print(me->left_subtree);
    printf(", \"right\": ");
    subtree_print(me->right_subtree);
    printf("}");
}

void
tree_print(struct NaiveTree *me)
{
    if (me == NULL) {
        printf("{}\n");
        return;
    }
    printf("{\"cardinality\": %zu, \"root\": ", me->cardinality);
    subtree_print(me->root);
    printf("}\n");
}

static bool
subtree_validate(struct NaiveSubtree *me, const size_t cardinality,
        const bool valid_lowerbound, const KeyType lowerbound,
        const bool valid_upperbound, const KeyType upperbound)
{
    if (me == NULL) {
        bool r = (cardinality == 0);
        return r;
    } else if (me->cardinality != cardinality) {
        subtree_print(me);
        printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
        return false;
    }

    // Validate that the tree is sorted
    if (valid_lowerbound && me->key < lowerbound) {
        subtree_print(me);
        printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
        return false;
    }
    if (valid_upperbound && me->key > upperbound) {
        subtree_print(me);
        printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
        return false;
    }

    // Get size of subtrees
    const size_t left_cardinality = (me->left_subtree ? me->left_subtree->cardinality : 0);
    const size_t right_cardinality = (me->right_subtree ? me->right_subtree->cardinality : 0);
    if (me->left_subtree != NULL) {
        bool r = subtree_validate(me->left_subtree, cardinality - right_cardinality - 1,
                valid_lowerbound, lowerbound, true, me->key);
        if (!r) {
            subtree_print(me);
            printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
            return false;
        }
    }
    if (me->right_subtree != NULL) {
        bool r = subtree_validate(me->right_subtree, cardinality - left_cardinality - 1,
                true, me->key, valid_upperbound, upperbound);
        if (!r) {
            subtree_print(me);
            printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
            return false;
        }
    }

    // Assert correct ordering
    return true;
}

bool
tree_validate(struct NaiveTree *me)
{
    if (me == NULL) {
        printf("[TRACE] %s:%d\n", __FILE__, __LINE__);
        return true;
    } else if (me->root == NULL) {
        bool r = (me->cardinality == 0); 
        printf("[TRACE] %s:%d\n", __FILE__, __LINE__);
        return r;
    } else {
        bool r = subtree_validate(me->root, me->cardinality, false, 0, false, 0);
        return r;
    }
}