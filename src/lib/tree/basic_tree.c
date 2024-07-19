#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tree/basic_tree.h"
#include "tree/types.h"

struct RemoveStatus {
    struct Subtree *removed;
    struct Subtree *new_child;
};

struct Subtree *
subtree__new(KeyType key)
{
    struct Subtree *subtree = (struct Subtree *)malloc(sizeof(struct Subtree));
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

bool
tree__init(struct Tree *me)
{
    if (me == NULL) {
        return false;
    }
    me->cardinality = 0;
    me->root = NULL;
    return true;
}

struct Tree *
tree__new(void)
{
    struct Tree *me = (struct Tree *)malloc(sizeof(struct Tree));
    if (me == NULL) {
        assert(0 && "OOM error!");
        return NULL;
    }
    bool success = tree__init(me);
    if (!success) {
        free(me);
        return NULL;
    }
    return me;
}

uint64_t
tree__cardinality(struct Tree *me)
{
    return me->cardinality;
}

bool
subtree__insert(struct Subtree *me, KeyType key)
{
    if (me == NULL) {
        // N.B. This should only be called if the first invocation passes a NULL
        return false;
    } else if (key < me->key) {
        if (me->left_subtree == NULL) {
            me->left_subtree = subtree__new(key);
            if (me->left_subtree == NULL) {
                return false; // OOM error!
            }
            ++me->cardinality;
            return true;
        } else {
            bool r = subtree__insert(me->left_subtree, key);
            if (r) {
                ++me->cardinality;
            }
            return r;
        }
    } else if (me->key < key) {
        if (me->right_subtree == NULL) {
            me->right_subtree = subtree__new(key);
            if (me->right_subtree == NULL) {
                return false; // OOM error!
            }
            ++me->cardinality;
            return true;
        } else {
            bool r = subtree__insert(me->right_subtree, key);
            if (r) {
                ++me->cardinality;
            }
            return r;
        }
    } else {
        return false;
    }
}

bool
tree__insert(struct Tree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    if (me->root == NULL) {
        me->root = subtree__new(key);
        if (me->root == NULL) {
            return false;
        }
        ++me->cardinality;
        return true;
    }
    bool r = subtree__insert(me->root, key);
    if (r) {
        ++me->cardinality;
    }
    return r;
}

bool
tree__search(struct Tree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    struct Subtree *current_node = me->root;
    // NOTE We use a while-loop because we do not need to update the tree as we
    //      traverse up the stack.
    while (true) {
        if (current_node == NULL) {
            return false;
        } else if (key < current_node->key) {
            current_node = current_node->left_subtree;
        } else if (current_node->key < key) {
            current_node = current_node->right_subtree;
        } else {
            return true;
        }
    }
    assert(0 && "impossible!");
}

static uint64_t
subtree__right_cardinality(struct Subtree *me)
{
    if (me == NULL) {
        return 0;
    } else if (me->right_subtree == NULL) {
        return 0;
    } else {
        return me->right_subtree->cardinality;
    }
}

uint64_t
tree__reverse_rank(struct Tree *me, KeyType key)
{
    if (me == NULL) {
        return SIZE_MAX;
    } else if (me->root == NULL) {
        return SIZE_MAX;
    }

    uint64_t rank = 0;
    struct Subtree *subtree = me->root;
    while (true) {
        if (key < subtree->key) {
            if (subtree->left_subtree == NULL) {
                return SIZE_MAX;
            }
            uint64_t right_cardinality = subtree__right_cardinality(subtree);
            rank += right_cardinality + 1;
            subtree = subtree->left_subtree;
        } else if (subtree->key < key) {
            if (subtree->right_subtree == NULL) {
                return SIZE_MAX;
            }
            // Rank remains the same when traversing right
            subtree = subtree->right_subtree;
        } else {
            uint64_t right_cardinality = subtree__right_cardinality(subtree);
            rank += right_cardinality;
            return rank;
        }
    }
}

static struct RemoveStatus
subtree__pop_minimum(struct Subtree *me)
{
    if (me == NULL) {
        // NOTE This should only be called if this function is originally called
        //      with a NULL argument. No recursive call should pass NULL.
        return (struct RemoveStatus){.removed = NULL, .new_child = NULL};
    } else if (me->left_subtree == NULL) {
        return (struct RemoveStatus){.removed = me,
                                     .new_child = me->right_subtree};
    } else {
        struct RemoveStatus r = subtree__pop_minimum(me->left_subtree);
        assert(r.removed != NULL &&
               "should have popped an item from the non-empty subtree");
        --me->cardinality;
        me->left_subtree = r.new_child;
        r.new_child = me;
        return r;
    }
}

static struct RemoveStatus
subtree__remove(struct Subtree *me, KeyType key)
{
    if (me == NULL) {
        return (struct RemoveStatus){.removed = false, .new_child = NULL};
    } else if (key < me->key) {
        struct RemoveStatus r = subtree__remove(me->left_subtree, key);
        if (r.removed) {
            --me->cardinality;
        }
        // NOTE Automatically overwriting the rather than checking may cause the
        //      cache line to be needlessly dirtied.
        me->left_subtree = r.new_child;
        r.new_child = me;
        return r;
    } else if (me->key < key) {
        struct RemoveStatus r = subtree__remove(me->right_subtree, key);
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
            return (struct RemoveStatus){.removed = me, .new_child = NULL};
        } else if (me->left_subtree == NULL) {
            // Single child
            struct Subtree *single_child = me->right_subtree;
            return (struct RemoveStatus){.removed = me,
                                         .new_child = single_child};
        } else if (me->right_subtree == NULL) {
            // Single child
            struct Subtree *single_child = me->left_subtree;
            return (struct RemoveStatus){.removed = me,
                                         .new_child = single_child};
        } else {
            // Double child => recursively replace with successor (arbitrarily
            // chose the successor rather than the predecessor).
            struct RemoveStatus r = subtree__pop_minimum(me->right_subtree);
            struct Subtree *replacement_node = r.removed;
            replacement_node->cardinality = me->cardinality - 1;
            replacement_node->left_subtree = me->left_subtree;
            replacement_node->right_subtree = r.new_child;
            return (struct RemoveStatus){.removed = me,
                                         .new_child = replacement_node};
        }
    }
}

bool
tree__remove(struct Tree *me, KeyType key)
{
    if (me == NULL) {
        return false;
    }
    struct RemoveStatus r = subtree__remove(me->root, key);
    if (r.removed == NULL) {
        return false;
    }
    me->root = r.new_child;
    --me->cardinality;
    free(r.removed);
    return true;
}

static void
subtree__print(struct Subtree *me)
{
    if (me == NULL) {
        printf("null");
        return;
    }
    printf("{\"key\": %" PRIu64 ", \"cardinality\": %" PRIu64 ", \"left\": ",
           me->key,
           me->cardinality);
    subtree__print(me->left_subtree);
    printf(", \"right\": ");
    subtree__print(me->right_subtree);
    printf("}");
}

void
tree__print(struct Tree *me)
{
    if (me == NULL) {
        printf("{}\n");
        return;
    }
    printf("{\"cardinality\": %" PRIu64 ", \"root\": ", me->cardinality);
    subtree__print(me->root);
    printf("}\n");
}

static void
subtree__prettyprint(struct Subtree *me, uint64_t level)
{
    if (me == NULL) {
        return;
    }
    subtree__prettyprint(me->right_subtree, level + 1);
    for (uint64_t i = 0; i < level; ++i) {
        printf("  ");
    }
    printf("Key: %" PRIu64 ", Size: %" PRIu64 "\n", me->key, me->cardinality);
    subtree__prettyprint(me->left_subtree, level + 1);
}

void
tree__prettyprint(struct Tree *me)
{
    if (me == NULL) {
        return;
    }
    subtree__prettyprint(me->root, 0);
}

static bool
subtree__validate(struct Subtree *me,
                  const uint64_t cardinality,
                  const bool valid_lowerbound,
                  const KeyType lowerbound,
                  const bool valid_upperbound,
                  const KeyType upperbound)
{
    if (me == NULL) {
        bool r = (cardinality == 0);
        return r;
    } else if (me->cardinality != cardinality) {
        printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
        return false;
    }

    // Validate that the tree is sorted
    if (valid_lowerbound && me->key < lowerbound) {
        printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
        return false;
    }
    if (valid_upperbound && me->key > upperbound) {
        printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
        return false;
    }

    // Get size of subtrees
    const uint64_t left_cardinality =
        (me->left_subtree ? me->left_subtree->cardinality : 0);
    const uint64_t right_cardinality =
        (me->right_subtree ? me->right_subtree->cardinality : 0);
    if (me->left_subtree != NULL) {
        bool r = subtree__validate(me->left_subtree,
                                   cardinality - right_cardinality - 1,
                                   valid_lowerbound,
                                   lowerbound,
                                   true,
                                   me->key);
        if (!r) {
            printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
            return false;
        }
    }
    if (me->right_subtree != NULL) {
        bool r = subtree__validate(me->right_subtree,
                                   cardinality - left_cardinality - 1,
                                   true,
                                   me->key,
                                   valid_upperbound,
                                   upperbound);
        if (!r) {
            printf("[ERROR] %s:%d\n", __FILE__, __LINE__);
            return false;
        }
    }

    // Assert correct ordering
    return true;
}

bool
tree__validate(struct Tree *me)
{
    if (me == NULL) {
        return true;
    } else if (me->root == NULL) {
        bool r = (me->cardinality == 0);
        return r;
    } else {
        bool r =
            subtree__validate(me->root, me->cardinality, false, 0, false, 0);
        return r;
    }
}

/// @brief  Free a subtree iteratively (though destructively).
/// @note   This converts the 'left' branch to point to the parent,
///         thus allowing us to walk back up the tree. I really like
///         this solution's performance, but I am less than thrilled
///         that it does some eyebrow raising repurposing.
/// @note   I stole this as-is (i.e. without understanding fully) from
///         https://codegolf.stackexchange.com/questions/478/free-a-binary-tree/489
/// @todo   UNDERSTAND THE GOSH-DARN THING!
/// @note   Previously, in my quest for efficient resource deallocation,
///         I iterated over chains of single children. This seemed to
///         work OK but didn't have any good theoretical guarantees and
///         failed on Twitter's cluster15.bin.
static void
free_subtree(struct Subtree *me)
{
    struct Subtree *node = me;
    struct Subtree *up = NULL;
    while (node != NULL) {
        if (node->left_subtree != NULL) {
            struct Subtree *left = node->left_subtree;
            node->left_subtree = up;
            up = node;
            node = left;
        } else if (node->right_subtree != NULL) {
            struct Subtree *right = node->right_subtree;
            node->left_subtree = up;
            node->right_subtree = NULL;
            up = node;
            node = right;
        } else {
            if (up == NULL) {
                free(node);
                node = NULL;
            }
            while (up != NULL) {
                free(node);
                if (up->right_subtree != NULL) {
                    node = up->right_subtree;
                    up->right_subtree = NULL;
                    break;
                } else {
                    node = up;
                    up = up->left_subtree;
                }
            }
        }
    }
}

void
tree__destroy(struct Tree *me)
{
    if (me == NULL) {
        return;
    }
    free_subtree(me->root);
    *me = (struct Tree){0};
}

void
tree__free(struct Tree *me)
{
    tree__destroy(me);
    free(me);
}
