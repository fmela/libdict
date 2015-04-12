/*
 * libdict - common definitions for binary search trees.
 *
 * Copyright (c) 2001-2014, Farooq Mela
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
    ASSERT(Tree != NULL);
    ASSERT(Node != NULL);

    tree_node* n = Node;
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
	((tree*)Tree)->root = nr;
    }
}

void
tree_node_rot_right(void* Tree, void* Node)
{
    ASSERT(Tree != NULL);
    ASSERT(Node != NULL);

    tree_node* n = Node;
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
	((tree*)Tree)->root = nl;
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
tree_search_node(void* Tree, const void* key)
{
    ASSERT(Tree != NULL);

    tree* t = Tree;
    for (tree_node* node = t->root; node;) {
	int cmp = t->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = node->rlink;
	else
	    return node;
    }
    return NULL;
}

void*
tree_search(void* Tree, const void* key)
{
    tree_node* node = tree_search_node(Tree, key);

    return node ? node->datum : NULL;
}

void*
tree_search_le_node(void* Tree, const void* key)
{
    tree* t = Tree;
    ASSERT(t != NULL);
    tree_node* node = t->root, *ret = NULL;
    while (node) {
	int cmp = t->cmp_func(key, node->key);
	if (cmp == 0)
	    return node;
	if (cmp < 0) {
	    node = node->llink;
	} else {
	    ret = node;
	    node = node->rlink;
	}
    }
    return ret;
}

void*
tree_search_le(void* Tree, const void* key)
{
    tree_node* node = tree_search_le_node(Tree, key);

    return node ? node->datum : NULL;
}

void*
tree_search_lt_node(void* Tree, const void* key)
{
    tree* t = Tree;
    ASSERT(t != NULL);
    tree_node* node = t->root, *ret = NULL;
    while (node) {
	int cmp = t->cmp_func(key, node->key);
	if (cmp <= 0) {
	    node = node->llink;
	} else {
	    ret = node;
	    node = node->rlink;
	}
    }
    return ret;
}

void*
tree_search_lt(void* Tree, const void* key)
{
    tree_node* node = tree_search_lt_node(Tree, key);

    return node ? node->datum : NULL;
}

void*
tree_search_ge_node(void* Tree, const void* key)
{
    tree* t = Tree;
    ASSERT(t != NULL);
    tree_node* node = t->root, *ret = NULL;
    while (node) {
	int cmp = t->cmp_func(key, node->key);
	if (cmp == 0) {
	    return node;
	}
	if (cmp < 0) {
	    ret = node;
	    node = node->llink;
	} else {
	    node = node->rlink;
	}
    }
    return ret;
}

void*
tree_search_ge(void* Tree, const void* key)
{
    tree_node* node = tree_search_ge_node(Tree, key);

    return node ? node->datum : NULL;
}

void*
tree_search_gt_node(void* Tree, const void* key)
{
    tree* t = Tree;
    ASSERT(t != NULL);
    tree_node* node = t->root, *ret = NULL;
    while (node) {
	int cmp = t->cmp_func(key, node->key);
	if (cmp < 0) {
	    ret = node;
	    node = node->llink;
	} else {
	    node = node->rlink;
	}
    }
    return ret;
}

void*
tree_search_gt(void* Tree, const void* key)
{
    tree_node* node = tree_search_gt_node(Tree, key);

    return node ? node->datum : NULL;
}

const void*
tree_min(const void* Tree)
{
    ASSERT(Tree != NULL);

    const tree* t = Tree;
    if (!t->root)
	return NULL;
    const tree_node* node = t->root;
    while (node->llink)
	node = node->llink;
    return node->key;
}

const void*
tree_max(const void* Tree)
{
    ASSERT(Tree != NULL);

    const tree* t = Tree;
    if (!t->root)
	return NULL;
    const tree_node* node = t->root;
    while (node->rlink)
	node = node->rlink;
    return node->key;
}

size_t
tree_traverse(void* Tree, dict_visit_func visit)
{
    ASSERT(Tree != NULL);
    ASSERT(visit != NULL);

    tree* t = Tree;
    size_t count = 0;
    if (t->root) {
	tree_node* node = tree_node_min(t->root);
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
    ASSERT(Tree != NULL);
    return ((tree*)Tree)->count;
}

size_t
tree_clear(void* Tree)
{
    ASSERT(Tree != NULL);

    tree* t = Tree;
    const size_t count = t->count;
    if (t->root) {
	tree_node_free(t, t->root);
	t->root = NULL;
	t->count = 0;
    }
    return count;
}

size_t
tree_free(void* Tree)
{
    ASSERT(Tree != NULL);

    const size_t count = tree_clear(Tree);
    FREE(Tree);
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
    const tree* t = Tree;
    ASSERT(t != NULL);
    return t->root ? node_min_leaf_depth(t->root, 1) : 0;
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
    const tree* t = Tree;
    ASSERT(t != NULL);
    return t->root ? node_max_leaf_depth(t->root, 1) : 0;
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
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
    ASSERT(iterator->tree != NULL);
    iterator->node = NULL;
}

void
tree_iterator_free(void* Iterator)
{
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
    ASSERT(iterator->tree != NULL);
    tree_iterator_invalidate(iterator);
    FREE(iterator);
}

bool
tree_iterator_next(void* Iterator)
{
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
    ASSERT(iterator->tree != NULL);
    if (iterator->node)
	return (iterator->node = tree_node_next(iterator->node)) != NULL;
    return false;
}

bool
tree_iterator_prev(void* Iterator)
{
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
    ASSERT(iterator->tree != NULL);
    if (iterator->node)
	return (iterator->node = tree_node_prev(iterator->node)) != NULL;
    return false;
}

bool
tree_iterator_next_n(void* Iterator, size_t count)
{
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
    ASSERT(iterator->tree != NULL);
    while (iterator->node && count--)
	iterator->node = tree_node_next(iterator->node);
    return iterator->node != NULL;
}

bool
tree_iterator_prev_n(void* Iterator, size_t count)
{
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
    ASSERT(iterator->tree != NULL);
    while (iterator->node && count--)
	iterator->node = tree_node_prev(iterator->node);
    return iterator->node != NULL;
}

bool
tree_iterator_first(void* Iterator)
{
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
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
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
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
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
    ASSERT(iterator->tree != NULL);
    return (iterator->node = tree_search_node(iterator->tree, key)) != NULL;
}

bool
tree_iterator_search_le(void* Iterator, const void* key)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return (iterator->node = tree_search_le_node(iterator->tree, key)) != NULL;
}

bool
tree_iterator_search_lt(void* Iterator, const void* key)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return (iterator->node = tree_search_lt_node(iterator->tree, key)) != NULL;
}

bool
tree_iterator_search_ge(void* Iterator, const void* key)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return (iterator->node = tree_search_ge_node(iterator->tree, key)) != NULL;
}

bool
tree_iterator_search_gt(void* Iterator, const void* key)
{
    tree_iterator* iterator = Iterator;
    ASSERT(iterator != NULL);
    ASSERT(iterator->tree != NULL);
    return (iterator->node = tree_search_gt_node(iterator->tree, key)) != NULL;
}

const void*
tree_iterator_key(const void* Iterator)
{
    ASSERT(Iterator != NULL);
    const tree_iterator* iterator = Iterator;
    ASSERT(iterator->tree != NULL);
    return iterator->node ? iterator->node->key : NULL;
}

void**
tree_iterator_data(void* Iterator)
{
    ASSERT(Iterator != NULL);
    tree_iterator* iterator = Iterator;
    ASSERT(iterator->tree != NULL);
    return iterator->node ? &iterator->node->datum : NULL;
}
