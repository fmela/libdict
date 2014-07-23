/*
 * libdict -- treap implementation.
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

/*
 * cf. [Aragon and Seidel, 1996], [Knuth 1998]
 *
 * A treap is a randomized data structure in which each node of tree has an
 * associated key and priority. The priority is chosen at random when the node
 * is inserted into the tree. Each node is inserted so that the lexicographic
 * order of the keys is preserved, and the priority of any node is less than
 * the priority of either of its child nodes; in this way the treap is a
 * combination of a tree and a min-heap. In this implementation, this is
 * accomplished by first inserting the node according to lexigraphical order of
 * keys as in a normal binary tree, and then, if needed, sifting the node
 * upwards using a series of rotations until the heap property of the tree is
 * restored.
 */

#include "tr_tree.h"

#include <limits.h>
#include "dict_private.h"
#include "tree_common.h"

typedef struct tr_node tr_node;
struct tr_node {
    TREE_NODE_FIELDS(tr_node);
    uint32_t		    prio;
};

struct tr_tree {
    TREE_FIELDS(tr_node);
    dict_prio_func	    prio_func;
};

struct tr_itor {
    TREE_ITERATOR_FIELDS(tr_tree, tr_node);
};

static dict_vtable tr_tree_vtable = {
    (dict_inew_func)	    tr_dict_itor_new,
    (dict_dfree_func)	    tree_free,
    (dict_insert_func)	    tr_tree_insert,
    (dict_search_func)	    tree_search,
    (dict_search_func)	    tree_search_le,
    (dict_search_func)	    tree_search_lt,
    (dict_search_func)	    tree_search_ge,
    (dict_search_func)	    tree_search_gt,
    (dict_remove_func)	    tr_tree_remove,
    (dict_clear_func)	    tree_clear,
    (dict_traverse_func)    tree_traverse,
    (dict_count_func)	    tree_count,
    (dict_verify_func)	    tr_tree_verify,
    (dict_clone_func)	    tr_tree_clone,
};

static itor_vtable tr_tree_itor_vtable = {
    (dict_ifree_func)	    tree_iterator_free,
    (dict_valid_func)	    tree_iterator_valid,
    (dict_invalidate_func)  tree_iterator_invalidate,
    (dict_next_func)	    tree_iterator_next,
    (dict_prev_func)	    tree_iterator_prev,
    (dict_nextn_func)	    tree_iterator_next_n,
    (dict_prevn_func)	    tree_iterator_prev_n,
    (dict_first_func)	    tree_iterator_first,
    (dict_last_func)	    tree_iterator_last,
    (dict_key_func)	    tree_iterator_key,
    (dict_data_func)	    tree_iterator_data,
    (dict_isearch_func)	    tree_iterator_search,
    (dict_isearch_func)	    tree_iterator_search_le,
    (dict_isearch_func)	    tree_iterator_search_lt,
    (dict_isearch_func)	    tree_iterator_search_ge,
    (dict_isearch_func)	    tree_iterator_search_gt,
    (dict_iremove_func)	    NULL,/* tr_itor_remove not implemented yet */
    (dict_icompare_func)    NULL /* tr_itor_compare not implemented yet */
};

static size_t	node_height(const tr_node* node);
static size_t	node_mheight(const tr_node* node);
static size_t	node_pathlen(const tr_node* node, size_t level);
static tr_node*	node_new(void* key);

tr_tree*
tr_tree_new(dict_compare_func cmp_func, dict_prio_func prio_func,
	    dict_delete_func del_func)
{
    tr_tree* tree = MALLOC(sizeof(*tree));
    if (tree) {
	tree->root = NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;
	tree->rotation_count = 0;
	tree->prio_func = prio_func;
    }
    return tree;
}

dict*
tr_dict_new(dict_compare_func cmp_func, dict_prio_func prio_func,
	    dict_delete_func del_func)
{
    dict* dct = MALLOC(sizeof(*dct));
    if (dct) {
	if (!(dct->_object = tr_tree_new(cmp_func, prio_func, del_func))) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &tr_tree_vtable;
    }
    return dct;
}

size_t
tr_tree_free(tr_tree* tree)
{
    ASSERT(tree != NULL);

    size_t count = tree_clear(tree);
    FREE(tree);
    return count;
}

tr_tree*
tr_tree_clone(tr_tree* tree, dict_key_datum_clone_func clone_func)
{
    ASSERT(tree != NULL);

    return tree_clone(tree, sizeof(tr_tree), sizeof(tr_node), clone_func);
}

size_t
tr_tree_clear(tr_tree* tree)
{
    ASSERT(tree != NULL);

    return tree_clear(tree);
}

void**
tr_tree_insert(tr_tree* tree, void* key, bool* inserted)
{
    ASSERT(tree != NULL);

    int cmp = 0;
    tr_node* node = tree->root;
    tr_node* parent = NULL;
    while (node) {
	cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    parent = node, node = node->llink;
	else if (cmp)
	    parent = node, node = node->rlink;
	else {
	    if (inserted)
		*inserted = false;
	    return &node->datum;
	}
    }

    if (!(node = node_new(key)))
	return NULL;
    if (inserted)
	*inserted = true;
    node->prio = tree->prio_func ? tree->prio_func(key) : dict_rand();

    if (!(node->parent = parent)) {
	ASSERT(tree->count == 0);
	tree->root = node;
    } else {
	if (cmp < 0)
	    parent->llink = node;
	else
	    parent->rlink = node;

	unsigned rotations = 0;
	while (parent->prio < node->prio) {
	    ++rotations;
	    if (parent->llink == node)
		tree_node_rot_right(tree, parent);
	    else
		tree_node_rot_left(tree, parent);
	    if (!(parent = node->parent))
		break;
	}
	tree->rotation_count += rotations;
    }
    ++tree->count;
    return &node->datum;
}

bool
tr_tree_remove(tr_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    tr_node* node = tree->root;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp)
	    node = node->rlink;
	else
	    break;
    }
    if (!node)
	return false;

    unsigned rotations = 0;
    while (node->llink && node->rlink) {
	++rotations;
	if (node->llink->prio > node->rlink->prio)
	    tree_node_rot_right(tree, node);
	else
	    tree_node_rot_left(tree, node);
    }
    tree->rotation_count += rotations;

    tr_node* out = node->llink ? node->llink : node->rlink;
    if (out)
	out->parent = node->parent;
    if (node->parent) {
	if (node->parent->llink == node)
	    node->parent->llink = out;
	else
	    node->parent->rlink = out;
    } else {
	tree->root = out;
    }

    if (tree->del_func)
	tree->del_func(node->key, node->datum);
    FREE(node);

    --tree->count;
    return true;
}

void*
tr_tree_search(tr_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    return tree_search(tree, key);
}

size_t
tr_tree_traverse(tr_tree* tree, dict_visit_func visit)
{
    ASSERT(tree != NULL);
    ASSERT(visit != NULL);

    return tree_traverse(tree, visit);
}

size_t
tr_tree_count(const tr_tree* tree)
{
    ASSERT(tree != NULL);

    return tree_count(tree);
}

size_t
tr_tree_height(const tr_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_height(tree->root) : 0;
}

size_t
tr_tree_mheight(const tr_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_mheight(tree->root) : 0;
}

size_t
tr_tree_pathlen(const tr_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_pathlen(tree->root, 1) : 0;
}

const void*
tr_tree_min(const tr_tree* tree)
{
    ASSERT(tree != NULL);

    return tree_min(tree);
}

const void*
tr_tree_max(const tr_tree* tree)
{
    ASSERT(tree != NULL);

    return tree_max(tree);
}

static tr_node*
node_new(void* key)
{
    tr_node* node = MALLOC(sizeof(*node));
    if (node) {
	node->key = key;
	node->datum = NULL;
	node->parent = NULL;
	node->llink = NULL;
	node->rlink = NULL;
    }
    return node;
}

static size_t
node_height(const tr_node* node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_height(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_height(node->rlink) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const tr_node* node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_mheight(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_mheight(node->rlink) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const tr_node* node, size_t level)
{
    size_t n = 0;

    ASSERT(node != NULL);

    if (node->llink)
	n += level + node_pathlen(node->llink, level + 1);
    if (node->rlink)
	n += level + node_pathlen(node->rlink, level + 1);
    return n;
}

static bool
node_verify(const tr_tree* tree, const tr_node* parent, const tr_node* node)
{
    ASSERT(tree != NULL);

    if (!parent) {
	VERIFY(tree->root == node);
    } else {
	VERIFY(parent->llink == node || parent->rlink == node);
    }
    if (node) {
	VERIFY(node->parent == parent);
	if (parent) {
	    VERIFY(node->prio <= parent->prio);
	}
	if (!node_verify(tree, node, node->llink) ||
	    !node_verify(tree, node, node->rlink))
	    return false;
    }
    return true;
}

bool
tr_tree_verify(const tr_tree* tree)
{
    ASSERT(tree != NULL);

    if (tree->root) {
	VERIFY(tree->count > 0);
    } else {
	VERIFY(tree->count == 0);
    }
    return node_verify(tree, NULL, tree->root);
}

tr_itor*
tr_itor_new(tr_tree* tree)
{
    ASSERT(tree != NULL);

    tr_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->tree = tree;
	itor->node = NULL;
    }
    return itor;
}

dict_itor*
tr_dict_itor_new(tr_tree* tree)
{
    ASSERT(tree != NULL);

    dict_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = tr_itor_new(tree))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &tr_tree_itor_vtable;
    }
    return itor;
}

void
tr_itor_free(tr_itor* itor)
{
    ASSERT(itor != NULL);

    FREE(itor);
}

bool
tr_itor_valid(const tr_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node != NULL;
}

void
tr_itor_invalidate(tr_itor* itor)
{
    ASSERT(itor != NULL);

    itor->node = NULL;
}

bool
tr_itor_next(tr_itor* itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	tr_itor_first(itor);
    else
	itor->node = tree_node_next(itor->node);
    return itor->node != NULL;
}

bool
tr_itor_prev(tr_itor* itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	tr_itor_last(itor);
    else
	itor->node = tree_node_prev(itor->node);
    return itor->node != NULL;
}

bool
tr_itor_nextn(tr_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!tr_itor_next(itor))
	    return false;
    return itor->node != NULL;
}

bool
tr_itor_prevn(tr_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!tr_itor_prev(itor))
	    return false;
    return itor->node != NULL;
}

bool
tr_itor_first(tr_itor* itor)
{
    ASSERT(itor != NULL);

    if (itor->tree->root) {
	itor->node = tree_node_min(itor->tree->root);
	return true;
    } else {
	itor->node = NULL;
	return false;
    }
}

bool
tr_itor_last(tr_itor* itor)
{
    ASSERT(itor != NULL);

    if (itor->tree->root) {
	itor->node = tree_node_max(itor->tree->root);
	return true;
    } else {
	itor->node = NULL;
	return false;
    }
}

const void*
tr_itor_key(const tr_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->key : NULL;
}

void**
tr_itor_data(tr_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node ? &itor->node->datum : NULL;
}
