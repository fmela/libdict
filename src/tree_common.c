/*
 * Copyright (C) 2012 Farooq Mela.
 * All rights reserved.
 *
 * Common definitions for binary search trees.
 */

#include "tree_common.h"

#include <string.h>
#include "dict_private.h"

typedef struct tree_node {
    TREE_NODE_FIELDS(struct tree_node);
} tree_node;

typedef struct {
    TREE_FIELDS(tree_node);
} tree;

typedef struct {
    TREE_ITERATOR_FIELDS(tree, tree_node);
} tree_iterator;

void
tree_node_rot_left(void* Tree, void* Node)
{
    tree* t = Tree;
    tree_node* n = Node;

    ASSERT(t != NULL);
    ASSERT(n != NULL);
    tree_node* nr = n->rlink;
    ASSERT(nr != NULL);
    if ((n->rlink = nr->llink) != NULL)
	n->rlink->parent = n;
    nr->llink = n;

    tree_node* p = n->parent;
    n->parent = nr;
    if ((nr->parent = p) != NULL) {
	if (p->llink == n)
	    p->llink = nr;
	else
	    p->rlink = nr;
    } else {
	t->root = nr;
    }
}

void
tree_node_rot_right(void* Tree, void* Node)
{
    tree* t = Tree;
    tree_node* n = Node;

    ASSERT(t != NULL);
    ASSERT(n != NULL);
    tree_node* nl = n->llink;
    ASSERT(nl != NULL);
    if ((n->llink = nl->rlink) != NULL)
	n->llink->parent = n;
    nl->rlink = n;

    tree_node* p = n->parent;
    n->parent = nl;
    if ((nl->parent = p) != NULL) {
	if (p->llink == n)
	    p->llink = nl;
	else
	    p->rlink = nl;
    } else {
	t->root = nl;
    }
}

void*
tree_node_prev(void* Node)
{
    tree_node* node = Node;
    ASSERT(node != NULL);
    if (node->llink) {
	return tree_node_max(node->llink);
    }
    tree_node* parent = node->parent;
    while (parent && parent->llink == node) {
	node = parent;
	parent = parent->parent;
    }
    return parent;
}

void*
tree_node_next(void* Node)
{
    tree_node* node = Node;
    ASSERT(node != NULL);
    if (node->rlink) {
	return tree_node_min(node->rlink);
    }
    tree_node* parent = node->parent;
    while (parent && parent->rlink == node) {
	node = parent;
	parent = parent->parent;
    }
    return parent;
}

void*
tree_node_min(void* Node)
{
    tree_node* node = Node;
    ASSERT(node != NULL);
    while (node->llink)
	node = node->llink;
    return node;
}

void*
tree_node_max(void* Node)
{
    tree_node* node = Node;
    ASSERT(node != NULL);
    while (node->rlink)
	node = node->rlink;
    return node;
}

void*
tree_search(void* Tree, const void* key)
{
    tree* tree = Tree;
    ASSERT(tree != NULL);
    tree_node* node = tree->root;
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
tree_min(const void* Tree)
{
    const tree* tree = Tree;
    ASSERT(tree != NULL);
    if (!tree->root)
	return NULL;
    const tree_node* node = tree->root;
    while (node->llink)
	node = node->llink;
    return node->key;
}

const void*
tree_max(const void* Tree)
{
    const tree* tree = Tree;
    ASSERT(tree != NULL);
    if (!tree->root)
	return NULL;
    const tree_node* node = tree->root;
    while (node->rlink)
	node = node->rlink;
    return node->key;
}

size_t
tree_traverse(void* Tree, dict_visit_func visit)
{
    tree* tree = Tree;
    ASSERT(tree != NULL);
    ASSERT(visit != NULL);

    size_t count = 0;
    if (tree->root) {
	tree_node* node = tree_node_min(tree->root);
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
tree_node_free(tree* tree, tree_node* node)
{
    ASSERT(node != NULL);

    tree_node* llink = node->llink;
    tree_node* rlink = node->rlink;
    if (tree->del_func)
	tree->del_func(node->key, node->datum);
    FREE(node);
    if (llink)
	tree_node_free(tree, llink);
    if (rlink)
	tree_node_free(tree, rlink);
}

size_t
tree_count(const void* Tree)
{
    const tree* tree = Tree;
    ASSERT(tree != NULL);
    return tree->count;
}

size_t
tree_clear(void* Tree)
{
    tree* tree = Tree;
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
tree_free(void* Tree)
{
    tree* tree = Tree;
    ASSERT(tree != NULL);

    const size_t count = tree_clear(tree);
    FREE(tree);
    return count;
}

static tree_node_base*
node_clone(tree_node_base* node, tree_node_base* parent, size_t node_size,
	   dict_key_datum_clone_func clone_func)
{
    if (!node)
	return NULL;
    tree_node_base* clone = MALLOC(node_size);
    if (!clone)
	return NULL;
    memcpy(clone, node, node_size);
    if (clone_func)
	clone_func(&clone->key, &clone->datum);
    clone->parent = parent;
    clone->llink = node_clone(node->llink, clone, node_size, clone_func);
    clone->rlink = node_clone(node->rlink, clone, node_size, clone_func);
    return clone;
}

void*
tree_clone(void* tree, size_t tree_size, size_t node_size,
	   dict_key_datum_clone_func clone_func)
{
    ASSERT(tree_size >= sizeof(tree_base));
    ASSERT(node_size >= sizeof(tree_node_base));

    tree_base* clone = MALLOC(tree_size);
    if (clone) {
	memcpy(clone, tree, tree_size);
	clone->root = node_clone(((tree_base*)tree)->root, NULL, node_size,
				 clone_func);
    }
    return clone;
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
tree_min_leaf_depth(const void* Tree)
{
    const tree* tree = Tree;
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
tree_max_leaf_depth(const void* Tree)
{
    const tree* tree = Tree;
    ASSERT(tree != NULL);
    return tree->root ? node_max_leaf_depth(tree->root, 1) : 0;
}

bool
tree_iterator_valid(const void* Iterator)
{
    const tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return iterator->node != NULL;
}

void
tree_iterator_invalidate(void* Iterator)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    iterator->node = NULL;
}

void
tree_iterator_free(void* Iterator)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    tree_iterator_invalidate(iterator);
    FREE(iterator);
}

bool
tree_iterator_next(void* Iterator)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    if (iterator->node)
	return (iterator->node = tree_node_next(iterator->node)) != NULL;
    return false;
}

bool
tree_iterator_prev(void* Iterator)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    if (iterator->node)
	return (iterator->node = tree_node_prev(iterator->node)) != NULL;
    return false;
}

bool
tree_iterator_next_n(void* Iterator, size_t count)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    while (iterator->node && count--)
	iterator->node = tree_node_next(iterator->node);
    return iterator->node != NULL;
}

bool
tree_iterator_prev_n(void* Iterator, size_t count)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    while (iterator->node && count--)
	iterator->node = tree_node_prev(iterator->node);
    return iterator->node != NULL;
}

bool
tree_iterator_first(void* Iterator)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    if (iterator->tree->root) {
	iterator->node = tree_node_min(iterator->tree->root);
	return true;
    }
    return false;
}

bool
tree_iterator_last(void* Iterator)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    if (iterator->tree->root) {
	iterator->node = tree_node_max(iterator->tree->root);
	return true;
    }
    return false;
}

bool
tree_iterator_search(void* Iterator, const void* key)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return (iterator->node = tree_search(iterator->tree, key)) != NULL;
}

const void*
tree_iterator_key(const void* Iterator)
{
    const tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return iterator->node ? iterator->node->key : NULL;
}

void**
tree_iterator_data(void* Iterator)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return iterator->node ? &iterator->node->datum : NULL;
}
