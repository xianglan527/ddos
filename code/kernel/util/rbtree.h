#ifndef __UTIL_RBTREE_H__
#define __UTIL_RBTREE_H__

#include "types.h"

typedef struct Rb_node Rb_node;
struct Rb_node{
    bool red;
    Rb_node *parent;
    Rb_node *left, *right;
};

typedef struct Rb_tree Rb_tree;
struct Rb_tree{
    int (*compare)(Rb_node *node1, Rb_node *node2);
    Rb_node *nil, *root;
};

Rb_tree *rb_tree_create(int (*compare)(Rb_node *node1, Rb_node *node2));
void rb_tree_destroy(Rb_tree *tree);
void rb_insert(Rb_tree *tree, Rb_node *node);
void rb_delete(Rb_tree *tree, Rb_node *node);
Rb_node *rb_search(Rb_tree *tree, int (*compare)(Rb_node *node, void *key), void *key);
Rb_node *rb_node_prev(Rb_tree *tree, Rb_node *node);
Rb_node *rb_node_next(Rb_tree *tree, Rb_node *node);
Rb_node *rb_node_root(Rb_tree *tree);
Rb_node *rb_node_left(Rb_tree *tree, Rb_node *node);
Rb_node *rb_node_right(Rb_tree *tree, Rb_node *node);

void check_rb_tree(void);

#endif