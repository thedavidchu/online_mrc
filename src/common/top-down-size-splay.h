/// Created by David Chu for the data structure and forward declarations
#pragma once

typedef struct tree_node Tree;
struct tree_node {
    Tree * left, * right;
    int key;
    int size;   /* maintained to be the number of nodes rooted here */
};

Tree * splay (int i, Tree *t);
Tree * insert(int i, Tree * t);
Tree * delete(int i, Tree *t);
Tree *find_rank(int r, Tree *t);
void printtree(Tree * t, int d);
