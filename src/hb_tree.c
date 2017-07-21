/*
 * libdict -- height-balanced (AVL) tree implementation.
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

#include "hb_tree.h"

#include "dict_private.h"
#include "tree_common.h"

typedef struct hb_node hb_node;

struct hb_node {
    void*	    key;
    void*	    datum;
    hb_node*	    parent;
    union {
	intptr_t    lbal;
	hb_node*    lptr;
    };
    union {
	intptr_t    rbal;
	hb_node*    rptr;
    };
};

#define LBAL(node)	    ((node)->lbal & (intptr_t)1)
#define RBAL(node)	    ((node)->rbal & (intptr_t)1)
#define BAL(node)	    ((int) (RBAL(node) - LBAL(node)))
#define LLINK(node)	    ((hb_node*) ((node)->lbal & ~(intptr_t)1))
#define RLINK(node)	    ((hb_node*) ((node)->rbal & ~(intptr_t)1))
#define SET_BAL(node,bal) do { \
    const int __bal = (bal); \
    switch (__bal) { \
	case -1: node->lbal |=  (intptr_t)1; node->rbal &= ~(intptr_t)1; break; \
	case  0: node->lbal &= ~(intptr_t)1; node->rbal &= ~(intptr_t)1; break; \
	case +1: node->lbal &= ~(intptr_t)1; node->rbal |=  (intptr_t)1; break; \
	default: ASSERT("This should never happen." && false); \
    } } while (0)
#define SET_LLINK(node,l) (node)->lbal = LBAL(node) | (intptr_t)(l)
#define SET_RLINK(node,r) (node)->rbal = RBAL(node) | (intptr_t)(r)

struct hb_tree {
    TREE_FIELDS(hb_node);
};

struct hb_itor {
    TREE_ITERATOR_FIELDS(hb_tree, hb_node);
};

static dict_vtable hb_tree_vtable = {
    (dict_inew_func)	    hb_dict_itor_new,
    (dict_dfree_func)	    hb_tree_free,
    (dict_insert_func)	    hb_tree_insert,
    (dict_search_func)	    hb_tree_search,
    (dict_search_func)	    hb_tree_search_le,
    (dict_search_func)	    hb_tree_search_lt,
    (dict_search_func)	    hb_tree_search_ge,
    (dict_search_func)	    hb_tree_search_gt,
    (dict_remove_func)	    hb_tree_remove,
    (dict_clear_func)	    hb_tree_clear,
    (dict_traverse_func)    hb_tree_traverse,
    (dict_count_func)	    hb_tree_count,
    (dict_verify_func)	    hb_tree_verify,
};

static itor_vtable hb_tree_itor_vtable = {
    (dict_ifree_func)	    hb_itor_free,
    (dict_valid_func)	    hb_itor_valid,
    (dict_invalidate_func)  hb_itor_invalidate,
    (dict_next_func)	    hb_itor_next,
    (dict_prev_func)	    hb_itor_prev,
    (dict_nextn_func)	    hb_itor_nextn,
    (dict_prevn_func)	    hb_itor_prevn,
    (dict_first_func)	    hb_itor_first,
    (dict_last_func)	    hb_itor_last,
    (dict_key_func)	    hb_itor_key,
    (dict_datum_func)	    hb_itor_datum,
    (dict_isearch_func)	    hb_itor_search,
    (dict_isearch_func)	    hb_itor_search_le,
    (dict_isearch_func)	    hb_itor_search_lt,
    (dict_isearch_func)	    hb_itor_search_ge,
    (dict_isearch_func)	    hb_itor_search_gt,
    (dict_iremove_func)	    NULL,/* hb_itor_remove not implemented yet */
    (dict_icompare_func)    NULL,/* hb_itor_compare not implemented yet */
};

static hb_node* node_min(hb_node* node);
static hb_node* node_max(hb_node* node);
static hb_node* node_prev(hb_node* node);
static hb_node* node_next(hb_node* node);
static size_t	node_height(const hb_node* node);
static size_t	node_mheight(const hb_node* node);
static size_t	node_pathlen(const hb_node* node, size_t level);
static hb_node*	node_new(void* key);
static bool	node_verify(const hb_tree* tree, const hb_node* parent, const hb_node* node,
			    unsigned* height, size_t *count);

hb_tree*
hb_tree_new(dict_compare_func cmp_func)
{
    ASSERT(cmp_func != NULL);

    hb_tree* tree = MALLOC(sizeof(*tree));
    if (tree) {
	tree->root = NULL;
	tree->count = 0;
	tree->cmp_func = cmp_func;
	tree->rotation_count = 0;
    }
    return tree;
}

dict*
hb_dict_new(dict_compare_func cmp_func)
{
    dict* dct = MALLOC(sizeof(*dct));
    if (dct) {
	if (!(dct->_object = hb_tree_new(cmp_func))) {
	    FREE(dct);
	    return NULL;
	}
	dct->_vtable = &hb_tree_vtable;
    }
    return dct;
}

size_t
hb_tree_free(hb_tree* tree, dict_delete_func delete_func)
{
    ASSERT(tree != NULL);

    const size_t count = hb_tree_clear(tree, delete_func);
    FREE(tree);
    return count;
}

size_t
hb_tree_clear(hb_tree* tree, dict_delete_func delete_func)
{
    ASSERT(tree != NULL);

    hb_node* node = tree->root;
    const size_t count = tree->count;
    while (node) {
	if (LLINK(node)) {
	    node = LLINK(node);
	    continue;
	}
	if (RLINK(node)) {
	    node = RLINK(node);
	    continue;
	}

	if (delete_func)
	    delete_func(node->key, node->datum);

	hb_node* parent = node->parent;
	FREE(node);
	tree->count--;

	if (parent) {
	    if (LLINK(parent) == node)
		SET_LLINK(parent, NULL);
	    else
		SET_RLINK(parent, NULL);
	}
	node = parent;
    }

    tree->root = NULL;
    ASSERT(tree->count == 0);

    return count;
}

static hb_node*
search_node(hb_tree* tree, const void* key)
{
    hb_node* node = tree->root;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = LLINK(node);
	else if (cmp > 0)
	    node = RLINK(node);
	else
	    return node;
    }
    return NULL;
}

void**
hb_tree_search(hb_tree* tree, const void* key)
{
    hb_node* node = search_node(tree, key);
    return node ? &node->datum : NULL;
}

static hb_node*
search_le_node(hb_tree* tree, const void* key)
{
    hb_node* node = tree->root, *ret = NULL;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0) {
	    node = LLINK(node);
	} else {
	    ret = node;
	    if (cmp == 0)
		break;
	    node = RLINK(node);
	}
    }
    return ret;
}

void**
hb_tree_search_le(hb_tree* tree, const void *key)
{
    hb_node* ret = search_le_node(tree, key);
    return ret ? &ret->datum : NULL;
}

static hb_node*
search_lt_node(hb_tree* tree, const void* key)
{
    hb_node* node = tree->root, *ret = NULL;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp > 0) {
	    ret = node;
	    node = RLINK(node);
	} else {
	    node = LLINK(node);
	}
    }
    return ret;
}

void**
hb_tree_search_lt(hb_tree* tree, const void *key)
{
    hb_node* ret = search_lt_node(tree, key);
    return ret ? &ret->datum : NULL;
}

static hb_node*
search_ge_node(hb_tree* tree, const void* key)
{
    hb_node* node = tree->root, *ret = NULL;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp > 0) {
	    node = RLINK(node);
	} else {
	    ret = node;
	    if (cmp == 0)
		break;
	    node = LLINK(node);
	}
    }
    return ret;
}

void**
hb_tree_search_ge(hb_tree* tree, const void *key)
{
    hb_node* ret = search_ge_node(tree, key);
    return ret ? &ret->datum : NULL;
}

static hb_node*
search_gt_node(hb_tree* tree, const void* key)
{
    hb_node* node = tree->root, *ret = NULL;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0) {
	    ret = node;
	    node = LLINK(node);
	} else {
	    node = RLINK(node);
	}
    }
    return ret;
}

void**
hb_tree_search_gt(hb_tree* tree, const void *key)
{
    hb_node* ret = search_gt_node(tree, key);
    return ret ? &ret->datum : NULL;
}

dict_insert_result
hb_tree_insert(hb_tree* tree, void* key)
{
    ASSERT(tree != NULL);

    int cmp = 0;
    hb_node* node = tree->root;
    hb_node* parent = NULL;
    hb_node* q = NULL;
    while (node) {
	cmp = tree->cmp_func(key, node->key);
	if (cmp == 0)
	    return (dict_insert_result) { &node->datum, false };

	parent = node;
	if (BAL(parent))
	    q = parent;
	node = (cmp < 0) ? LLINK(node) : RLINK(node);
    }

    hb_node* const add = node = node_new(key);
    if (!node)
	return (dict_insert_result) { NULL, false };

    if (!(node->parent = parent)) {
	ASSERT(tree->count == 0);
	ASSERT(tree->root == NULL);
	tree->root = node;
    } else {
	if (cmp < 0)
	    SET_LLINK(parent, node);
	else
	    SET_RLINK(parent, node);

	while (parent != q) {
	    ASSERT(BAL(parent) == 0);
	    SET_BAL(parent, (RLINK(parent) == node) ? +1 : -1);
	    node = parent;
	    parent = parent->parent;
	}
	if (q) {
	    const int qbal = BAL(q);
	    if (LLINK(q) == node) {
		if (qbal == -1) {
		    hb_node* const qp = q->parent;
		    hb_node* const ql = LLINK(q);
		    if (BAL(ql) > 0) {
			/* LR: rotate q->llink left, then rotate q right. */
			hb_node* const qlr = RLINK(ql);

			if (qp) {
			    if (LLINK(qp) == q)
				SET_LLINK(qp, qlr);
			    else {
				ASSERT(RLINK(qp) == q);
				SET_RLINK(qp, qlr);
			    }
			} else {
			    tree->root = qlr;
			}
			qlr->parent = qp;

			SET_LLINK(q, RLINK(qlr));
			if (LLINK(q))
			    LLINK(q)->parent = q;
			SET_RLINK(ql, LLINK(qlr));
			if (RLINK(ql))
			    RLINK(ql)->parent = ql;
			SET_LLINK(qlr, ql);
			SET_RLINK(qlr, q);
			ql->parent = q->parent = qlr;
			const int qlr_bal = BAL(qlr);
			SET_BAL(q, (qlr_bal == -1));
			SET_BAL(ql, -(qlr_bal == 1));
			SET_BAL(qlr, 0);

			tree->rotation_count += 2;
		    } else {
			/* R: rotate q right. */
			if (qp) {
			    if (LLINK(qp) == q)
				SET_LLINK(qp, ql);
			    else {
				ASSERT(RLINK(qp) == q);
				SET_RLINK(qp, ql);
			    }
			} else {
			    tree->root = ql;
			}
			ql->parent = qp;

			SET_LLINK(q, RLINK(ql));
			if (LLINK(q))
			    LLINK(q)->parent = q;
			SET_RLINK(ql, q);
			q->parent = ql;
			SET_BAL(q, 0);
			SET_BAL(ql, 0);

			tree->rotation_count += 1;
		    }
		} else {
		    SET_BAL(q, qbal - 1);
		}
	    } else {
		ASSERT(RLINK(q) == node);
		if (qbal == 1) {
		    hb_node* const qp = q->parent;
		    hb_node* const qr = RLINK(q);
		    if (BAL(qr) < 0) {
			/* RL: Rotate q->rlink right, then q left. */
			hb_node* const qrl = LLINK(qr);

			if (qp) {
			    if (LLINK(qp) == q)
				SET_LLINK(qp, qrl);
			    else {
				ASSERT(RLINK(qp) == q);
				SET_RLINK(qp, qrl);
			    }
			} else {
			    tree->root = qrl;
			}
			qrl->parent = qp;

			SET_RLINK(q, LLINK(qrl));
			if (RLINK(q))
			    RLINK(q)->parent = q;
			SET_LLINK(qr, RLINK(qrl));
			if (LLINK(qr))
			    LLINK(qr)->parent = qr;
			SET_LLINK(qrl, q);
			SET_RLINK(qrl, qr);
			qr->parent = q->parent = qrl;
			const int qrl_bal = BAL(qrl);
			SET_BAL(q, -(qrl_bal == 1));
			SET_BAL(qr, (qrl_bal == -1));
			SET_BAL(qrl, 0);

			tree->rotation_count += 2;
		    } else {
			/* L: rotate q left */
			if (qp) {
			    if (LLINK(qp) == q)
				SET_LLINK(qp, qr);
			    else {
				ASSERT(RLINK(qp) == q);
				SET_RLINK(qp, qr);
			    }
			} else {
			    tree->root = qr;
			}
			qr->parent = qp;

			SET_RLINK(q, LLINK(qr));
			if (RLINK(q))
			    RLINK(q)->parent = q;
			SET_LLINK(qr, q);
			q->parent = qr;
			SET_BAL(q, 0);
			SET_BAL(qr, 0);

			tree->rotation_count += 1;
		    }
		} else {
		    SET_BAL(q, qbal + 1);
		}
	    }
	}
    }
    tree->count++;
    return (dict_insert_result) { &add->datum, true };
}

dict_remove_result
hb_tree_remove(hb_tree* tree, const void* key)
{
    ASSERT(tree != NULL);

    hb_node* node = tree->root;
    while (node) {
	int cmp = tree->cmp_func(key, node->key);
	if (cmp == 0)
	    break;
	node = (cmp < 0) ? LLINK(node) : RLINK(node);
    }
    if (!node)
	return (dict_remove_result) { NULL, NULL, false };

    if (LLINK(node) && RLINK(node)) {
	hb_node* out = (BAL(node) > 0) ? node_min(RLINK(node)) : node_max(LLINK(node));
	void* tmp;
	SWAP(node->key, out->key, tmp);
	SWAP(node->datum, out->datum, tmp);
	node = out;
    }

    const dict_remove_result result = { node->key, node->datum, true };
    hb_node* p = node->parent;
    hb_node* child = LLINK(node) ? LLINK(node) : RLINK(node);
    FREE(node);
    tree->count--;
    if (child)
	child->parent = p;
    if (!p) {
	tree->root = child;
	return result;
    }

    bool left = (LLINK(p) == node);
    if (left)
	SET_LLINK(p, child);
    else
	SET_RLINK(p, child);
    node = child;

    unsigned rotations = 0;
    for (;;) {
	if (left) {
	    ASSERT(LLINK(p) == node);
	    if (BAL(p) == +1) {
		hb_node* const pp = p->parent;
		hb_node* const pr = RLINK(p);
		if (BAL(pr) < 0) {
		    /* RL: rotate |pr| right, then |p| left. */
		    rotations += 2;

		    hb_node* const prl = LLINK(pr);
		    if (pp) {
			if (LLINK(pp) == p)
			    SET_LLINK(pp, prl);
			else {
			    ASSERT(RLINK(pp) == p);
			    SET_RLINK(pp, prl);
			}
		    } else {
			tree->root = prl;
		    }
		    prl->parent = pp;

		    SET_RLINK(p, LLINK(prl));
		    if (RLINK(p))
			RLINK(p)->parent = p;
		    SET_LLINK(pr, RLINK(prl));
		    if (LLINK(pr))
			LLINK(pr)->parent = pr;
		    SET_LLINK(prl, p);
		    SET_RLINK(prl, pr);
		    pr->parent = p->parent = prl;
		    const int prl_bal = BAL(prl);
		    SET_BAL(p, -(prl_bal == 1));
		    SET_BAL(pr, (prl_bal == -1));
		    SET_BAL(prl, 0);
		} else {
		    /* L: rotate |p| left. */
		    rotations += 1;

		    if (pp) {
			if (LLINK(pp) == p)
			    SET_LLINK(pp, pr);
			else {
			    ASSERT(RLINK(pp) == p);
			    SET_RLINK(pp, pr);
			}
		    } else {
			tree->root = pr;
		    }
		    pr->parent = pp;

		    SET_RLINK(p, LLINK(pr));
		    if (RLINK(p))
			RLINK(p)->parent = p;
		    SET_LLINK(pr, p);
		    p->parent = pr;
		    const int pr_bal = BAL(pr);
		    SET_BAL(p, (pr_bal == 0));
		    SET_BAL(pr, -(pr_bal == 0));
		    if (pr_bal == 0)
			break;
		}
		node = p->parent;
	    } else if (BAL(p) == 0) {
		SET_BAL(p, +1);
		break;
	    } else {
		ASSERT(BAL(p) == -1);
		SET_BAL(p, 0);
		node = p;
	    }
	} else {
	    ASSERT(RLINK(p) == node);
	    if (BAL(p) == -1) {
		hb_node* const pp = p->parent;
		hb_node* const pl = LLINK(p);
		if (BAL(pl) > 0) {
		    /* LR: rotate |pl| left, then |p| right. */
		    rotations += 2;

		    hb_node* const plr = RLINK(pl);
		    if (pp) {
			if (LLINK(pp) == p)
			    SET_LLINK(pp, plr);
			else {
			    ASSERT(RLINK(pp) == p);
			    SET_RLINK(pp, plr);
			}
		    } else {
			tree->root = plr;
		    }
		    plr->parent = pp;

		    SET_LLINK(p, RLINK(plr));
		    if (LLINK(p))
			LLINK(p)->parent = p;
		    SET_RLINK(pl, LLINK(plr));
		    if (RLINK(pl))
			RLINK(pl)->parent = pl;
		    SET_LLINK(plr, pl);
		    SET_RLINK(plr, p);
		    pl->parent = p->parent = plr;
		    const int plr_bal = BAL(plr);
		    SET_BAL(p, (plr_bal == -1));
		    SET_BAL(pl, -(plr_bal == 1));
		    SET_BAL(plr, 0);
		} else {
		    /* R: rotate |p| right. */
		    rotations += 1;

		    if (pp) {
			if (LLINK(pp) == p)
			    SET_LLINK(pp, pl);
			else {
			    ASSERT(RLINK(pp) == p);
			    SET_RLINK(pp, pl);
			}
		    } else {
			tree->root = pl;
		    }
		    pl->parent = pp;

		    SET_LLINK(p, RLINK(pl));
		    if (LLINK(p))
			LLINK(p)->parent = p;
		    SET_RLINK(pl, p);
		    p->parent = pl;
		    const int pl_bal = BAL(pl);
		    SET_BAL(p, -(pl_bal == 0));
		    SET_BAL(pl, (pl_bal == 0));
		    if (pl_bal == 0)
			break;
		}
		node = p->parent;
	    } else if (BAL(p) == 0) {
		SET_BAL(p, -1);
		break;
	    } else {
		ASSERT(BAL(p) == +1);
		SET_BAL(p, 0);
		node = p;
	    }
	}

	if (!(p = node->parent))
	    break;
	if (LLINK(p) == node) {
	    left = true;
	} else {
	    ASSERT(RLINK(p) == node);
	    left = false;
	}
    }
    tree->rotation_count += rotations;
    return result;
}

const void*
hb_tree_min(const hb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_min(tree->root)->key : NULL;
}

const void*
hb_tree_max(const hb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_max(tree->root)->key : NULL;
}

size_t
hb_tree_traverse(hb_tree* tree, dict_visit_func visit)
{
    ASSERT(tree != NULL);

    return tree_traverse(tree, visit);
}

size_t
hb_tree_count(const hb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree_count(tree);
}

size_t
hb_tree_height(const hb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_height(tree->root) : 0;
}

size_t
hb_tree_mheight(const hb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_mheight(tree->root) : 0;
}

size_t
hb_tree_pathlen(const hb_tree* tree)
{
    ASSERT(tree != NULL);

    return tree->root ? node_pathlen(tree->root, 1) : 0;
}

static hb_node*
node_new(void* key)
{
    hb_node* node = MALLOC(sizeof(*node));
    if (node) {
	ASSERT((((intptr_t)node) & 1) == 0); /* Ensure malloc returns aligned result. */
	node->key = key;
	node->datum = NULL;
	node->parent = NULL;
	node->lbal = node->rbal = 0;
    }
    return node;
}

static size_t
node_height(const hb_node* node)
{
    ASSERT(node != NULL);

    size_t l = LLINK(node) ? node_height(LLINK(node)) + 1 : 0;
    size_t r = RLINK(node) ? node_height(RLINK(node)) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const hb_node* node)
{
    ASSERT(node != NULL);

    size_t l = LLINK(node) ? node_mheight(LLINK(node)) + 1 : 0;
    size_t r = RLINK(node) ? node_mheight(RLINK(node)) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const hb_node* node, size_t level)
{
    ASSERT(node != NULL);

    size_t n = 0;
    if (LLINK(node))
	n += level + node_pathlen(LLINK(node), level + 1);
    if (RLINK(node))
	n += level + node_pathlen(RLINK(node), level + 1);
    return n;
}

static hb_node*
node_min(hb_node* node)
{
    ASSERT(node != NULL);
    while (LLINK(node))
	node = LLINK(node);
    return node;
}

static hb_node*
node_max(hb_node* node)
{
    ASSERT(node != NULL);
    while (RLINK(node))
	node = RLINK(node);
    return node;
}

static hb_node*
node_prev(hb_node* node)
{
    ASSERT(node != NULL);
    if (LLINK(node))
	return node_max(LLINK(node));
    hb_node* parent = node->parent;
    while (parent && LLINK(parent) == node) {
	node = parent;
	parent = parent->parent;
    }
    return parent;
}

static hb_node*
node_next(hb_node* node)
{
    ASSERT(node != NULL);
    if (RLINK(node))
	return node_min(RLINK(node));
    hb_node* parent = node->parent;
    while (parent && RLINK(parent) == node) {
	node = parent;
	parent = parent->parent;
    }
    return parent;
}

static bool
node_verify(const hb_tree* tree, const hb_node* parent, const hb_node* node,
	    unsigned* height, size_t *count)
{
    ASSERT(tree != NULL);

    if (!parent) {
	ASSERT(tree->root == node);
    } else {
	if (LLINK(parent) == node) {
	    if (node)
		ASSERT(tree->cmp_func(parent->key, node->key) > 0);
	} else {
	    ASSERT(RLINK(parent) == node);
	    if (node)
		ASSERT(tree->cmp_func(parent->key, node->key) < 0);
	}
    }
    bool verified = true;
    if (node) {
	const int bal = BAL(node);
	ASSERT(bal >= -1);
	ASSERT(bal <= +1);
	if (RBAL(node))
	    ASSERT(LBAL(node) == 0);
	if (LBAL(node))
	    ASSERT(RBAL(node) == 0);
	ASSERT(node->parent == parent);
	unsigned lheight, rheight;
	verified &= node_verify(tree, node, LLINK(node), &lheight, count);
	verified &= node_verify(tree, node, RLINK(node), &rheight, count);
	ASSERT(bal == (int)rheight - (int)lheight);
	if (height)
	    *height = MAX(lheight, rheight) + 1;
	*count += 1;
    } else {
	if (height)
	    *height = 0;
    }
    return verified;
}

bool
hb_tree_verify(const hb_tree* tree)
{
    ASSERT(tree != NULL);

    size_t count = 0;
    bool verified = node_verify(tree, NULL, tree->root, NULL, &count);
    VERIFY(tree->count == count);
    return verified;
}

hb_itor*
hb_itor_new(hb_tree* tree)
{
    ASSERT(tree != NULL);

    hb_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	itor->tree = tree;
	itor->node = NULL;
    }
    return itor;
}

dict_itor*
hb_dict_itor_new(hb_tree* tree)
{
    ASSERT(tree != NULL);

    dict_itor* itor = MALLOC(sizeof(*itor));
    if (itor) {
	if (!(itor->_itor = hb_itor_new(tree))) {
	    FREE(itor);
	    return NULL;
	}
	itor->_vtable = &hb_tree_itor_vtable;
    }
    return itor;
}

void
hb_itor_free(hb_itor* itor)
{
    ASSERT(itor != NULL);

    FREE(itor);
}

bool
hb_itor_valid(const hb_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node != NULL;
}

void
hb_itor_invalidate(hb_itor* itor)
{
    ASSERT(itor != NULL);

    itor->node = NULL;
}

bool
hb_itor_next(hb_itor* itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	hb_itor_first(itor);
    else
	itor->node = node_next(itor->node);
    return itor->node != NULL;
}

bool
hb_itor_prev(hb_itor* itor)
{
    ASSERT(itor != NULL);

    if (!itor->node)
	hb_itor_last(itor);
    else
	itor->node = node_prev(itor->node);
    return itor->node != NULL;
}

bool
hb_itor_nextn(hb_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!hb_itor_prev(itor))
	    return false;
    return itor->node != NULL;
}

bool
hb_itor_prevn(hb_itor* itor, size_t count)
{
    ASSERT(itor != NULL);

    while (count--)
	if (!hb_itor_prev(itor))
	    return false;
    return itor->node != NULL;
}

bool
hb_itor_first(hb_itor* itor)
{
    ASSERT(itor != NULL);

    itor->node = itor->tree->root ? node_min(itor->tree->root) : NULL;
    return itor->node != NULL;
}

bool
hb_itor_last(hb_itor* itor)
{
    ASSERT(itor != NULL);

    itor->node = itor->tree->root ? node_max(itor->tree->root) : NULL;
    return itor->node != NULL;
}

bool
hb_itor_search(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = search_node(itor->tree, key)) != NULL;
}

bool
hb_itor_search_le(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = search_le_node(itor->tree, key)) != NULL;
}

bool
hb_itor_search_lt(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = search_lt_node(itor->tree, key)) != NULL;
}

bool
hb_itor_search_ge(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = search_ge_node(itor->tree, key)) != NULL;
}

bool
hb_itor_search_gt(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = search_gt_node(itor->tree, key)) != NULL;
}

const void*
hb_itor_key(const hb_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node ? itor->node->key : NULL;
}

void**
hb_itor_datum(hb_itor* itor)
{
    ASSERT(itor != NULL);

    return itor->node ? &itor->node->datum : NULL;
}
