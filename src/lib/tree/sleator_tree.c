/*
           An implementation of top-down splaying with sizes
             D. Sleator <sleator@cs.cmu.edu>, January 1994.
Modified a little by Qingpeng Niu for tracing the global chunck library memory use. Just add a
compute sum of size from search node to the right most node.
*/
/*
           An implementation of top-down splaying with sizes
             D. Sleator <sleator@cs.cmu.edu>, January 1994.

  This extends top-down-splay.c to maintain a size field in each node.
  This is the number of nodes in the subtree rooted there.  This makes
  it possible to efficiently compute the rank of a key.  (The rank is
  the number of nodes to the left of the given key.)  It it also
  possible to quickly find the node of a given rank.  Both of these
  operations are illustrated in the code below.  The remainder of this
  introduction is taken from top-down-splay.c.

  "Splay trees", or "self-adjusting search trees" are a simple and
  efficient data structure for storing an ordered set.  The data
  structure consists of a binary tree, with no additional fields.  It
  allows searching, insertion, deletion, deletemin, deletemax,
  splitting, joining, and many other operations, all with amortized
  logarithmic performance.  Since the trees adapt to the sequence of
  requests, their performance on real access patterns is typically even
  better.  Splay trees are described in a number of texts and papers
  [1,2,3,4].

  The code here is adapted from simple top-down splay, at the bottom of
  page 669 of [2].  It can be obtained via anonymous ftp from
  spade.pc.cs.cmu.edu in directory /usr/sleator/public.

  The chief modification here is that the splay operation works even if the
  item being splayed is not in the tree, and even if the tree root of the
  tree is NULL.  So the line:

                              t = splay(i, t);

  causes it to search for item with key i in the tree rooted at t.  If it's
  there, it is splayed to the root.  If it isn't there, then the node put
  at the root is the last one before NULL that would have been reached in a
  normal binary search for i.  (It's a neighbor of i in the tree.)  This
  allows many other operations to be easily implemented, as shown below.

  [1] "Data Structures and Their Algorithms", Lewis and Denenberg,
       Harper Collins, 1991, pp 243-251.
  [2] "Self-adjusting Binary Search Trees" Sleator and Tarjan,
       JACM Volume 32, No 3, July 1985, pp 652-686.
  [3] "Data Structure and Algorithm Analysis", Mark Weiss,
       Benjamin Cummins, 1992, pp 119-130.
  [4] "Data Structures, Algorithms, and Performance", Derick Wood,
       Addison-Wesley, 1993, pp 367-375
*/
#include <stdio.h>
#include <stdlib.h>

#include "tree/sleator_tree.h"

#define compare(i, j) ((int64_t)(i) - (int64_t)(j))
/* This is the comparison.                                       */
/* Returns <0 if i<j, =0 if i=j, and >0 if i>j                   */

#define node_size(x) (((x) == NULL) ? 0 : ((x)->cardinality))
/* This macro returns the size of a node.  Unlike "x->cardinality",     */
/* it works even if x=NULL.  The test could be avoided by using  */
/* a special version of NULL which was a real node with cardinality 0.  */

static struct Subtree *
sleator_splay(struct Subtree *t, KeyType key);

static struct Subtree *
sleator_splay(struct Subtree *t, KeyType i)
/* Splay using the key i (which may or may not be in the tree.) */
/* The starting root is t, and the tree used is defined by rat  */
/* size fields are maintained */
{
    struct Subtree N, *l, *r, *y;
    int64_t comp;
    uint64_t l_size, r_size;
    if (t == NULL)
        return t;
    N.left_subtree = N.right_subtree = NULL;
    l = r = &N;
    l_size = r_size = 0;

    for (;;) {
        comp = compare(i, t->key);
        if (comp < 0) {
            if (t->left_subtree == NULL)
                break;
            if (compare(i, t->left_subtree->key) < 0) {
                y = t->left_subtree; /* rotate right */
                t->left_subtree = y->right_subtree;
                y->right_subtree = t;
                t->cardinality = node_size(t->left_subtree) + node_size(t->right_subtree) + 1;
                t = y;
                if (t->left_subtree == NULL)
                    break;
            }
            r->left_subtree = t; /* link right */
            r = t;
            t = t->left_subtree;
            r_size += 1 + node_size(r->right_subtree);
        } else if (comp > 0) {
            if (t->right_subtree == NULL)
                break;
            if (compare(i, t->right_subtree->key) > 0) {
                y = t->right_subtree; /* rotate left */
                t->right_subtree = y->left_subtree;
                y->left_subtree = t;
                t->cardinality = node_size(t->left_subtree) + node_size(t->right_subtree) + 1;
                t = y;
                if (t->right_subtree == NULL)
                    break;
            }
            l->right_subtree = t; /* link left */
            l = t;
            t = t->right_subtree;
            l_size += 1 + node_size(l->left_subtree);
        } else {
            break;
        }
    }
    l_size += node_size(t->left_subtree);  /* Now l_size and r_size are the sizes of */
    r_size += node_size(t->right_subtree); /* the left and right trees we just built.*/
    t->cardinality = l_size + r_size + 1;

    l->right_subtree = r->left_subtree = NULL;

    /* The following two loops correct the size fields of the right path  */
    /* from the left child of the root and the right path from the left   */
    /* child of the root.                                                 */
    for (y = N.right_subtree; y != NULL; y = y->right_subtree) {
        y->cardinality = l_size;
        l_size -= 1 + node_size(y->left_subtree);
    }
    for (y = N.left_subtree; y != NULL; y = y->left_subtree) {
        y->cardinality = r_size;
        r_size -= 1 + node_size(y->right_subtree);
    }

    l->right_subtree = t->left_subtree; /* assemble */
    r->left_subtree = t->right_subtree;
    t->left_subtree = N.right_subtree;
    t->right_subtree = N.left_subtree;

    return t;
}

bool
sleator_insert(struct Tree *tree, KeyType i)
{
    /* Insert key i into the tree t, if it is not already there. */
    /* Return a pointer to the resulting tree.                   */
    struct Subtree *t;
    struct Subtree *new;
    if (tree == NULL) {
        return false;
    }
    t = tree->root;

    if (t != NULL) {
        t = sleator_splay(t, i);
        if (compare(i, t->key) == 0) {
            return false; /* it's already there */
        }
    }
    new = (struct Subtree *)malloc(sizeof(struct Subtree));
    if (new == NULL) {
        printf("Ran out of space\n");
        exit(1);
    }
    if (t == NULL) {
        new->left_subtree = new->right_subtree = NULL;
    } else if (compare(i, t->key) < 0) {
        new->left_subtree = t->left_subtree;
        new->right_subtree = t;
        t->left_subtree = NULL;
        t->cardinality = 1 + node_size(t->right_subtree);
    } else {
        new->right_subtree = t->right_subtree;
        new->left_subtree = t;
        t->right_subtree = NULL;
        t->cardinality = 1 + node_size(t->left_subtree);
    }
    new->key = i;
    new->cardinality = 1 + node_size(new->left_subtree) + node_size(new->right_subtree);
    // Update tree data structure
    tree->root = new;
    tree->cardinality = new->cardinality;
    return true;
}

bool
sleator_remove(struct Tree *tree, KeyType i)
{
    /* Deletes i from the tree if it's there.               */
    /* Return a pointer to the resulting tree.              */
    struct Subtree *t;
    struct Subtree *x;
    uint64_t tsize;

    if (tree == NULL) {
        return false;
    }
    t = tree->root;

    if (t == NULL)
        return false;
    tsize = t->cardinality;
    t = sleator_splay(t, i);
    if (compare(i, t->key) == 0) { /* found it */
        if (t->left_subtree == NULL) {
            x = t->right_subtree;
        } else {
            x = sleator_splay(t->left_subtree, i);
            x->right_subtree = t->right_subtree;
        }
        free(t);
        if (x == NULL) {
            tree->cardinality = 0;
        } else {
            x->cardinality = tsize - 1;
            tree->cardinality = x->cardinality;
        }
        tree->root = x;
        return true;
    } else {
        if (t == NULL) {
            tree->cardinality = 0;
        } else {
            tree->cardinality = t->cardinality;
        }
        tree->root = t;
        return false; /* It wasn't there */
    }
}

struct Subtree *
sleator_find_rank(struct Subtree *t, unsigned r)
{
    /* Returns a pointer to the node in the tree with the given rank.  */
    /* Returns NULL if there is no such node.                          */
    /* Does not change the tree.  To guarantee logarithmic behavior,   */
    /* the node found here should be splayed to the root.              */
    uint64_t lsize;
    if (r >= node_size(t))
        return NULL;
    for (;;) {
        lsize = node_size(t->left_subtree);
        if (r < lsize) {
            t = t->left_subtree;
        } else if (r > lsize) {
            r = r - lsize - 1;
            t = t->right_subtree;
        } else {
            return t;
        }
    }
}
