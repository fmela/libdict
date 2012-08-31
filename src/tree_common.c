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
	node = node->llink;
	while (node->rlink)
	    node = node->rlink;
	return node;
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
	node = node->rlink;
	while (node->llink)
	    node = node->llink;
	return node;
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
    for (; node->llink; node = node->llink)
	/* void */;
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
    for (; node->rlink; node = node->rlink)
	/* void */;
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

size_t
tree_clear(void *Tree)
{
    tree *tree = Tree;
    ASSERT(tree != NULL);

    const size_t count = tree->count;
    tree_node *node = tree->root;
    while (node) {
	if (node->llink || node->rlink) {
	    node = node->llink ? node->llink : node->rlink;
	    continue;
	}

	if (tree->del_func)
	    tree->del_func(node->key, node->datum);

	tree_node *parent = node->parent;
	FREE(node);
	if (parent) {
	    if (parent->llink == node)
		parent->llink = NULL;
	    else
		parent->rlink = NULL;
	}
	node = parent;
    }

    tree->root = NULL;
    tree->count = 0;
    return count;
}

void
tree_node_height(void *Node, size_t *minheight, size_t *maxheight)
{
    ASSERT(minheight != NULL);
    ASSERT(maxheight != NULL);
    tree_node *node = Node;
    if (node) {
	size_t lmin, lmax, rmin, rmax;
	tree_node_height(node->llink, &lmin, &lmax);
	tree_node_height(node->rlink, &rmin, &rmax);
	*minheight = MIN(lmin, rmin) + 1;
	*maxheight = MAX(lmax, rmax) + 1;
    } else {
	*minheight = *maxheight = 0;
    }
}

void
tree_height(void *Tree, size_t *minheight, size_t *maxheight)
{
    tree *tree = Tree;
    ASSERT(tree != NULL);
    ASSERT(minheight != NULL);
    ASSERT(maxheight != NULL);
    tree_node_height(tree->root, minheight, maxheight);
}
