/*
 * Copyright (C) 2012 Farooq Mela.
 * All rights reserved.
 *
 * Common definitions for binary search trees.
 */

#include "tree_common.h"

#include "dict_private.h"

typedef struct tree_node tree_node;
struct tree_node {
    TREE_NODE_FIELDS(tree_node);
};

typedef struct tree tree;
struct tree {
    TREE_FIELDS(tree_node);
};

typedef struct tree_iterator tree_iterator;
struct tree_iterator {
    TREE_ITERATOR_FIELDS(tree, tree_node);
};

void
tree_node_rot_left(void *Tree, void *Node)
{
    tree *tree = Tree;
    tree_node *node = Node;

    ASSERT(tree != NULL);
    ASSERT(node != NULL);
    ASSERT(node->rlink != NULL);

    tree_node *rlink = node->rlink;
    node->rlink = rlink->llink;
    if (rlink->llink)
	rlink->llink->parent = node;
    tree_node *parent = node->parent;
    rlink->parent = parent;
    if (parent) {
	if (parent->llink == node)
	    parent->llink = rlink;
	else
	    parent->rlink = rlink;
    } else {
	tree->root = rlink;
    }
    rlink->llink = node;
    node->parent = rlink;
}

void
tree_node_rot_right(void *Tree, void *Node)
{
    tree *tree = Tree;
    tree_node *node = Node;

    ASSERT(tree != NULL);
    ASSERT(node != NULL);
    ASSERT(node->llink != NULL);

    tree_node *llink = node->llink;
    node->llink = llink->rlink;
    if (llink->rlink)
	llink->rlink->parent = node;
    tree_node *parent = node->parent;
    llink->parent = parent;
    if (parent) {
	if (parent->llink == node)
	    parent->llink = llink;
	else
	    parent->rlink = llink;
    } else {
	tree->root = llink;
    }
    llink->rlink = node;
    node->parent = llink;
}

void*
tree_node_prev(void *Node)
{
    tree_node *node = Node;
    ASSERT(node != NULL);
    if (node->llink) {
	return tree_node_max(node->llink);
    }
    tree_node *parent = node->parent;
    while (parent && parent->llink == node) {
	node = parent;
	parent = parent->parent;
    }
    return parent;
}

void*
tree_node_next(void *Node)
{
    tree_node *node = Node;
    ASSERT(node != NULL);
    if (node->rlink) {
	return tree_node_min(node->rlink);
    }
    tree_node *parent = node->parent;
    while (parent && parent->rlink == node) {
	node = parent;
	parent = parent->parent;
    }
    return parent;
}

void*
tree_node_min(void *Node)
{
    tree_node *node = Node;
    ASSERT(node != NULL);
    while (node->llink)
	node = node->llink;
    return node;
}

void*
tree_node_max(void *Node)
{
    tree_node *node = Node;
    ASSERT(node != NULL);
    while (node->rlink)
	node = node->rlink;
    return node;
}

void*
tree_search(void *Tree, const void *key)
{
    tree *tree = Tree;
    ASSERT(tree != NULL);
    tree_node *node = tree->root;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = node->rlink;
	else
	    return node->datum;
    }
    return NULL;
}

const void*
tree_min(const void *Tree)
{
    const tree *tree = Tree;
    ASSERT(tree != NULL);
    if (!tree->root)
	return NULL;
    const tree_node *node = tree->root;
    while (node->llink)
	node = node->llink;
    return node->key;
}

const void*
tree_max(const void *Tree)
{
    const tree *tree = Tree;
    ASSERT(tree != NULL);
    if (!tree->root)
	return NULL;
    const tree_node *node = tree->root;
    while (node->rlink)
	node = node->rlink;
    return node->key;
}

size_t
tree_traverse(void *Tree, dict_visit_func visit)
{
    tree *tree = Tree;
    ASSERT(tree != NULL);
    ASSERT(visit != NULL);

    size_t count = 0;
    if (tree->root) {
	tree_node *node = tree_node_min(tree);
	do {
	    ++count;
	    if (!visit(node->key, node->datum))
		break;
	    node = tree_node_next(node);
	} while (node);
    }
    return count;
}

static void
tree_node_free(tree *tree, tree_node *node)
{
    ASSERT(node != NULL);

    tree_node *llink = node->llink;
    tree_node *rlink = node->rlink;
    if (tree->del_func)
	tree->del_func(node->key, node->datum);
    FREE(node);
    if (llink)
	tree_node_free(tree, llink);
    if (rlink)
	tree_node_free(tree, rlink);
}

size_t
tree_clear(void *Tree)
{
    tree *tree = Tree;
    ASSERT(tree != NULL);

    const size_t count = tree->count;
    if (tree->root) {
	tree_node_free(tree, tree->root);
	tree->root = NULL;
	tree->count = 0;
    }
    return count;
}

size_t
tree_free(void *Tree)
{
    tree *tree = Tree;
    ASSERT(tree != NULL);

    const size_t count = tree_clear(tree);
    FREE(tree);
    return count;
}

static size_t
node_min_leaf_depth(const tree_node* node, size_t depth)
{
    ASSERT(node != NULL);
    if (node->llink && node->rlink) {
	size_t l_height = node_min_leaf_depth(node->llink, depth + 1);
	size_t r_height = node_min_leaf_depth(node->rlink, depth + 1);
	return MIN(l_height, r_height);
    } else if (node->llink) {
	return node_min_leaf_depth(node->llink, depth + 1);
    } else if (node->rlink) {
	return node_min_leaf_depth(node->rlink, depth + 1);
    } else {
	return depth;
    }
}

size_t
tree_min_leaf_depth(const void *Tree)
{
    const tree *tree = Tree;
    ASSERT(tree != NULL);
    return tree->root ? node_min_leaf_depth(tree->root, 1) : 0;
}

static size_t
node_max_leaf_depth(const tree_node* node, size_t depth)
{
    ASSERT(node != NULL);
    if (node->llink && node->rlink) {
	size_t l_height = node_max_leaf_depth(node->llink, depth + 1);
	size_t r_height = node_max_leaf_depth(node->rlink, depth + 1);
	return MAX(l_height, r_height);
    } else if (node->llink) {
	return node_max_leaf_depth(node->llink, depth + 1);
    } else if (node->rlink) {
	return node_max_leaf_depth(node->rlink, depth + 1);
    } else {
	return depth;
    }
}

size_t
tree_max_leaf_depth(const void *Tree)
{
    const tree *tree = Tree;
    ASSERT(tree != NULL);
    return tree->root ? node_max_leaf_depth(tree->root, 1) : 0;
}

bool
tree_iterator_valid(const void *Iterator)
{
    const tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return iterator->node != NULL;
}

void
tree_iterator_invalidate(void *Iterator)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    iterator->node = NULL;
}

void
tree_iterator_free(void *Iterator)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    tree_iterator_invalidate(iterator);
    FREE(iterator);
}

bool
tree_iterator_next(void *Iterator)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    if (iterator->node)
	return (iterator->node = tree_node_next(iterator->node)) != NULL;
    return false;
}

bool
tree_iterator_prev(void *Iterator)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    if (iterator->node)
	return (iterator->node = tree_node_prev(iterator->node)) != NULL;
    return false;
}

bool
tree_iterator_next_n(void *Iterator, size_t count)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    while (iterator->node && count--)
	iterator->node = tree_node_next(iterator->node);
    return iterator->node != NULL;
}

bool
tree_iterator_prev_n(void *Iterator, size_t count)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    while (iterator->node && count--)
	iterator->node = tree_node_prev(iterator->node);
    return iterator->node != NULL;
}

bool
tree_iterator_first(void *Iterator)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    if (iterator->tree->root) {
	iterator->node = tree_node_min(iterator->tree->root);
	return true;
    }
    return false;
}

bool
tree_iterator_last(void *Iterator)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    if (iterator->tree->root) {
	iterator->node = tree_node_max(iterator->tree->root);
	return true;
    }
    return false;
}

bool
tree_iterator_search(void *Iterator, const void *key)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return (iterator->node = tree_search(iterator->tree, key)) != NULL;
}

const void*
tree_iterator_key(const void *Iterator)
{
    const tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return iterator->node ? iterator->node->key : NULL;
}

void*
tree_iterator_data(void *Iterator)
{
    tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return iterator->node ? iterator->node->datum : NULL;
}

bool
tree_iterator_set_data(void *Iterator, void *datum, void **old_datum)
{
    const tree_iterator *iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    if (iterator->node) {
	if (old_datum)
	    *old_datum = iterator->node->datum;
	iterator->node->datum = datum;
	return true;
    }
    return false;
}
