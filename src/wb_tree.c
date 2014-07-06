/*
 * libdict -- weight-balanced tree implementation.
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
 * cf. [Gonnet 1984], [Nievergelt and Reingold 1973]
 */

#include "wb_tree.h"

#include <limits.h>
#include "dict_private.h"
#include "tree_common.h"

/* A tree BB[alpha] is said to be of weighted balance alpha if every node in
 * the tree has a balance p(n) such that alpha <= p(n) <= 1 - alpha. The
 * balance of a node is defined as the number of nodes in its left subtree
 * divided by the number of nodes in either subtree. The weight of a node is
 * defined as the number of external nodes in its subtrees.
 *
 * Legal values for alpha are 0 <= alpha <= 1/2. BB[0] is a normal, unbalanced
 * binary tree, and BB[1/2] includes only completely balanced binary search
 * trees of 2^height - 1 nodes. A higher value of alpha specifies a more
 * stringent balance requirement. Values for alpha in the range 2/11 <= alpha
 * <= 1 - sqrt(2)/2 are interesting because a tree can be brought back into
 * weighted balance after an insertion or deletion using at most one rotation
 * per level (thus the number of rotations after insertion or deletion is
 * O(lg N)).
 *
 * These are the parameters for alpha = 1 - sqrt(2)/2 == .292893, as
 * recommended in [Gonnet 1984]. */

/* These constants are approximated by integer fractions in the code to
 * eliminate floating point arithmetic. */
#if 0
# define ALPHA_0	    .292893f	/* 1 - sqrt(2)/2    */
# define ALPHA_1	    .707106f	/* sqrt(2)/2	    */
# define ALPHA_2	    .414213f	/* sqrt(2) - 1	    */
# define ALPHA_3	    .585786f	/* 2 - sqrt(2)	    */
#endif

typedef struct wb_node wb_node;
struct wb_node {
    TREE_NODE_FIELDS(wb_node);
    uint32_t		    weight;
};

#define WEIGHT(n)	((n) ? (n)->weight : 1U)

struct wb_tree {
    TREE_FIELDS(wb_node);
};

struct wb_itor {
    TREE_ITERATOR_FIELDS(wb_tree, wb_node);
};

static dict_vtable wb_tree_vtable = {
    (dict_inew_func)	    wb_dict_itor_new,
    (dict_dfree_func)	    tree_free,
    (dict_insert_func)	    wb_tree_insert,
    (dict_search_func)	    tree_search,
    (dict_search_func)	    tree_search_le,
    (dict_search_func)	    tree_search_lt,
    (dict_search_func)	    tree_search_ge,
    (dict_search_func)	    tree_search_gt,
    (dict_remove_func)	    wb_tree_remove,
    (dict_clear_func)	    tree_clear,
    (dict_traverse_func)    tree_traverse,
    (dict_count_func)	    tree_count,
    (dict_verify_func)	    wb_tree_verify,
    (dict_clone_func)	    wb_tree_clone,
};

static itor_vtable wb_tree_itor_vtable = {
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
    (dict_iremove_func)	    NULL,/* wb_itor_remove not implemented yet */
    (dict_icompare_func)    NULL /* wb_itor_compare not implemented yet */
};

static size_t	node_height(const wb_node* node);
static size_t	node_mheight(const wb_node* node);
static size_t	node_pathlen(const wb_node* node, size_t level);
static wb_node*	node_new(void* key);

wb_tree*
wb_tree_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    wb_tree* tree = MALLOC(sizeof(*tree));
    if (tree) {
	tree->root = NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	tree->del_func = del_func;
	tree->rotation_count = 0;
    }
    return tree;
}

dict*
wb_dict_new(dict_compare_func cmp_func, dict_delete_func del_func)
{
    dict* dct = MALLOC(sizeof(*dct));
    if (dct) {
	if (!(dct->_object = wb_tree_new(cmp_func, del_func))) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &wb_tree_vtable;
    }
    return dct;
}

size_t
wb_tree_free(wb_tree* tree)
{
    ASSERT(tree != NULL);

    size_t count = tree_clear(tree);
    FREE(tree);
    return count;
}

wb_tree*
wb_tree_clone(wb_tree* tree, dict_key_datum_clone_func clone_func)
{
    ASSERT(tree != NULL);

    return tree_clone(tree, sizeof(wb_tree), sizeof(wb_node), clone_func);
}

void*
wb_tree_search(wb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    return tree_search(tree, key);
}

static inline unsigned
fixup(wb_tree* tree, wb_node* n) {
    unsigned rotations = 0;
    unsigned weight = WEIGHT(n->llink);
    if (weight * 1000U < n->weight * 293U) {
	wb_node* nr = n->rlink;
	ASSERT(nr != NULL);
	wb_node* nrl = nr->llink;
	if (WEIGHT(nrl) * 1000U < nr->weight * 586U) {	/* LL */
	    /* Rotate |n| left. */
	    tree_node_rot_left(tree, n);
	    nr->weight = (n->weight = WEIGHT(n->llink) + WEIGHT(n->rlink)) +
			 WEIGHT(nr->rlink);
	    rotations += 1;
	} else {					    /* RL */
	    /* Rotate |nr| right, then |n| left. */
	    ASSERT(nrl != NULL);
	    wb_node* p = n->parent;
	    if ((nrl->parent = p) != NULL) {
		if (p->llink == n)
		    p->llink = nrl;
		else
		    p->rlink = nrl;
	    } else {
		tree->root = nrl;
	    }

	    wb_node* a = nrl->llink;
	    nrl->llink = n;
	    n->parent = nrl;
	    if ((n->rlink = a) != NULL)
		a->parent = n;

	    wb_node* b = nrl->rlink;
	    nrl->rlink = nr;
	    nr->parent = nrl;
	    if ((nr->llink = b) != NULL)
		b->parent = nr;

	    nrl->weight = (n->weight = WEIGHT(n->llink) + WEIGHT(a)) +
			  (nr->weight = WEIGHT(b) + WEIGHT(nr->rlink));
	    rotations += 2;
	}
    } else if (weight * 1000U > n->weight * 707U) {
	wb_node* nl = n->llink;
	ASSERT(nl != NULL);
	weight = WEIGHT(nl->llink);
	if (weight * 1000U > nl->weight * 414U) {	/* RR */
	    tree_node_rot_right(tree, n);

	    n->weight = WEIGHT(n->llink) + WEIGHT(n->rlink);
	    nl->weight = weight + n->weight;
	    rotations += 1;
	} else {						/* LR */
	    /* Rotate |nl| left, then |n| right. */
	    wb_node* nlr = nl->rlink;
	    ASSERT(nlr != NULL);
	    wb_node* p = n->parent;
	    if ((nlr->parent = p) != NULL) {
		if (p->llink == n)
		    p->llink = nlr;
		else
		    p->rlink = nlr;
	    } else {
		tree->root = nlr;
	    }

	    wb_node* a = nlr->llink;
	    nlr->llink = nl;
	    nl->parent = nlr;
	    if ((nl->rlink = a) != NULL)
		a->parent = nl;

	    wb_node* b = nlr->rlink;
	    nlr->rlink = n;
	    n->parent = nlr;
	    if ((n->llink = b) != NULL)
		b->parent = n;

	    nlr->weight = (n->weight = WEIGHT(b) + WEIGHT(n->rlink)) +
			  (nl->weight = WEIGHT(nl->llink) + WEIGHT(a));
	    rotations += 2;
	}
    }
    return rotations;
}

void**
wb_tree_insert(wb_tree* tree, void* key, bool* inserted)
{
    int cmp = 0;

    ASSERT(tree != NULL);

    wb_node* node = tree->root;
    wb_node* parent = NULL;
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

    wb_node* add = node = node_new(key);
    if (!add)
	return NULL;
    if (inserted)
	*inserted = true;
    if (!(node->parent = parent)) {
	ASSERT(tree->count == 0);
	ASSERT(tree->root == NULL);
	tree->root = node;
    } else {
	if (cmp < 0)
	    parent->llink = node;
	else
	    parent->rlink = node;

	unsigned rotations = 0;
	while ((node = parent) != NULL) {
	    parent = node->parent;
	    ++node->weight;
	    rotations += fixup(tree, node);
	}
	tree->rotation_count += rotations;
    }
    ++tree->count;
    return &add->datum;
}

bool
wb_tree_remove(wb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);
    ASSERT(key != NULL);

    wb_node* node = tree->root;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0) {
	    node = node->llink;
	} else if (cmp) {
	    node = node->rlink;
	} else {
	    if (node->llink && node->rlink) {
		wb_node* out;
		if (node->llink->weight > node->rlink->weight) {
		    out = node->llink;
		    while (out->rlink)
			out = out->rlink;
		} else {
		    out = node->rlink;
		    while (out->llink)
			out = out->llink;
		}
		void* tmp;
		SWAP(node->key, out->key, tmp);
		SWAP(node->datum, out->datum, tmp);
		node = out;
	    }
	    ASSERT(!node->llink || !node->rlink);
	    /* Splice in the successor, if any. */
	    wb_node* child = node->llink ? node->llink : node->rlink;
	    wb_node* parent = node->parent;
	    if (child)
		child->parent = parent;
	    if (parent) {
		if (parent->llink == node)
		    parent->llink = child;
		else
		    parent->rlink = child;
	    } else {
		ASSERT(tree->root == node);
		tree->root = child;
	    }
	    if (tree->del_func)
		tree->del_func(node->key, node->datum);
	    FREE(node);
	    --tree->count;
	    /* Now move up the tree, decrementing weights. */
	    unsigned rotations = 0;
	    while (parent) {
		--parent->weight;
		wb_node* up = parent->parent;
		rotations += fixup(tree, parent);
		parent = up;
	    }
	    tree->rotation_count += rotations;
	    return true;
	}
    }
    return false;
}

size_t
wb_tree_clear(wb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree_clear(tree);
}

const void*
wb_tree_min(const wb_tree* tree)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return NULL;

    const wb_node* node = tree->root;
    while (node->llink)
	node = node->llink;
    return node->key;
}

const void*
wb_tree_max(const wb_tree* tree)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return NULL;

    const wb_node* node = tree->root;
    while (node->rlink)
	node = node->rlink;
    return node->key;
}

size_t
wb_tree_traverse(wb_tree* tree, dict_visit_func visit)
{
    ASSERT(tree != NULL);

    if (!tree->root)
	return 0;
    size_t count = 0;
    wb_node* node = tree_node_min(tree->root);
    while (node) {
	++count;
	if (!visit(node->key, node->datum))
	    break;
	node = tree_node_next(node);
    }
    return count;
}

size_t
wb_tree_count(const wb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree_count(tree);
}

size_t
wb_tree_height(const wb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_height(tree->root) : 0;
}

size_t
wb_tree_mheight(const wb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_mheight(tree->root) : 0;
}

size_t
wb_tree_pathlen(const wb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_pathlen(tree->root, 1) : 0;
}

static wb_node*
node_new(void* key)
{
    wb_node* node = MALLOC(sizeof(*node));
    if (node) {
	node->key = key;
	node->datum = NULL;
	node->parent = NULL;
	node->llink = NULL;
	node->rlink = NULL;
	node->weight = 2;
    }
    return node;
}

static size_t
node_height(const wb_node* node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_height(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_height(node->rlink) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const wb_node* node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_mheight(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_mheight(node->rlink) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const wb_node* node, size_t level)
{
    size_t n = 0;

    ASSERT(node != NULL);

    if (node->llink)
	n += level + node_pathlen(node->llink, level + 1);
    if (node->rlink)
	n += level + node_pathlen(node->rlink, level + 1);
    return n;
}

wb_itor*
wb_itor_new(wb_tree* tree)
{
    ASSERT(tree != NULL);

    wb_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->tree = tree;
	itor->node = NULL;
    }
    return itor;
}

dict_itor*
wb_dict_itor_new(wb_tree* tree)
{
    ASSERT(tree != NULL);
    dict_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = wb_itor_new(tree))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &wb_tree_itor_vtable;
    }
    return itor;
}

static bool
node_verify(const wb_tree* tree, const wb_node* parent, const wb_node* node)
{
    ASSERT(tree != NULL);

    if (!parent) {
	VERIFY(tree->root == node);
    } else {
	VERIFY(parent->llink == node || parent->rlink == node);
    }
    if (node) {
	VERIFY(node->parent == parent);
	if (!node_verify(tree, node, node->llink) ||
	    !node_verify(tree, node, node->rlink))
	    return false;
	const unsigned lweight = WEIGHT(node->llink);
	VERIFY(node->weight == lweight + WEIGHT(node->rlink));
	VERIFY(lweight * 1000U >= node->weight * 292U);
	VERIFY(lweight * 1000U <= node->weight * 708U);
    }
    return true;
}

bool
wb_tree_verify(const wb_tree* tree)
{
    ASSERT(tree != NULL);

    if (tree->root) {
	VERIFY(tree->count > 0);
    } else {
	VERIFY(tree->count == 0);
    }
    return node_verify(tree, NULL, tree->root);
}

void
wb_itor_free(wb_itor* itor)
{
	ASSERT(itor != NULL);

	FREE(itor);
}

bool
wb_itor_valid(const wb_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node != NULL;
}

void
wb_itor_invalidate(wb_itor* itor)
{
    ASSERT(itor != NULL);

    itor->node = NULL;
}

bool
wb_itor_next(wb_itor* itor)
{
    ASSERT(itor != NULL);

    if (!itor->node) {
	return wb_itor_first(itor);
    } else {
	itor->node = tree_node_next(itor->node);
	return itor->node != NULL;
    }
}

bool
wb_itor_prev(wb_itor* itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	return wb_itor_last(itor);
    else {
	itor->node = tree_node_prev(itor->node);
	return itor->node != NULL;
    }
}

bool
wb_itor_nextn(wb_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!wb_itor_next(itor))
	    return false;
    return itor->node != NULL;
}

bool
wb_itor_prevn(wb_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!wb_itor_prev(itor))
	    return false;
    return itor->node != NULL;
}

bool
wb_itor_first(wb_itor* itor)
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
wb_itor_last(wb_itor* itor)
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
wb_itor_key(const wb_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->key : NULL;
}

void**
wb_itor_data(wb_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node ? &itor->node->datum : NULL;
}
