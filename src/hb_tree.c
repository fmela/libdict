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
    union {
	intptr_t    bal;
	hb_node*    pptr;
    };
    hb_node*	    llink;
    hb_node*	    rlink;
};

#define BAL_MASK	    ((intptr_t)3)
#define PARENT(node)	    ((hb_node*) ((node)->bal & ~BAL_MASK))
#define BAL_POS(node)	    ((node)->bal & 1)
#define BAL_NEG(node)	    ((node)->bal & 2)

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
    (dict_search_func)	    tree_search,
    (dict_search_func)	    tree_search_le,
    (dict_search_func)	    tree_search_lt,
    (dict_search_func)	    tree_search_ge,
    (dict_search_func)	    tree_search_gt,
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
	if (node->llink) {
	    node = node->llink;
	    continue;
	}
	if (node->rlink) {
	    node = node->rlink;
	    continue;
	}

	if (delete_func)
	    delete_func(node->key, node->datum);

	hb_node* parent = PARENT(node);
	FREE(node);
	tree->count--;

	if (parent) {
	    if (parent->llink == node)
		parent->llink = NULL;
	    else
		parent->rlink = NULL;
	}
	node = parent;
    }

    tree->root = NULL;
    ASSERT(tree->count == 0);

    return count;
}

/* L: rotate |q| left */
static bool
rotate_l(hb_tree* restrict const tree, hb_node* restrict const q)
{
    hb_node* restrict const qp = PARENT(q);
    hb_node* restrict const qr = q->rlink;

    ASSERT(BAL_POS(q));

    hb_node *restrict const qrl = qr->llink;
    const int qr_bal = qr->bal & BAL_MASK;

    /* q->parent <- qr; q->bal <- (qr_bal == 0); */
    q->bal = (intptr_t)qr | (qr_bal == 0);
    if ((q->rlink = qrl) != NULL)
	qrl->bal = (intptr_t)q | (qrl->bal & BAL_MASK); /* qrl->parent <- q; */

    /* qr->parent <- qp; qr->bal <- -(qr_bal == 0); */
    qr->bal = (intptr_t)qp | ((qr_bal == 0) << 1);
    qr->llink = q;
    *(qp == NULL ? &tree->root : qp->llink == q ? &qp->llink : &qp->rlink) = qr;

    return (qr_bal == 0);
}

/* R: rotate |q| right. */
static bool
rotate_r(hb_tree* restrict const tree, hb_node* restrict const q)
{
    hb_node* restrict const qp = PARENT(q);
    hb_node* restrict const ql = q->llink;

    ASSERT(BAL_NEG(q));

    hb_node* restrict const qlr = ql->rlink;
    const int ql_bal = ql->bal & BAL_MASK;

    /* q->parent <- ql; q->bal <- -(ql_bal == 0); */
    q->bal = (intptr_t)ql | ((ql_bal == 0) << 1);
    if ((q->llink = qlr) != NULL)
	qlr->bal = (intptr_t)q | (qlr->bal & BAL_MASK); /* qlr->parent <- q; */

    /* ql->parent <- qp; ql->bal <- (ql_bal == 0); */
    ql->bal = (intptr_t)qp | (ql_bal == 0);
    ql->rlink = q;
    *(qp == NULL ? &tree->root : qp->llink == q ? &qp->llink : &qp->rlink) = ql;

    return (ql_bal == 0);
}

/* RL: Rotate |q->rlink| right, then |q| left. */
static void
rotate_rl(hb_tree* restrict const tree, hb_node* restrict const q)
{
    hb_node* restrict const qp = PARENT(q);
    hb_node* restrict const qr = q->rlink;
    hb_node* restrict const qrl = qr->llink;

    ASSERT(BAL_POS(q));
    ASSERT(BAL_NEG(qr));

    const int qrl_bal = qrl->bal & BAL_MASK;
    hb_node* restrict const qrll = qrl->llink;
    hb_node* restrict const qrlr = qrl->rlink;

    /* qrl->parent <- qp; qrl->bal <- 0; */
    qrl->bal = (intptr_t)qp;
    *(qp == NULL ? &tree->root : qp->llink == q ? &qp->llink : &qp->rlink) = qrl;
    qrl->llink = q;
    qrl->rlink = qr;

    /* q->parent <- qrl; q->bal <- -(qrl_bal == 1); */
    q->bal = (intptr_t)qrl | ((qrl_bal == 1) << 1);
    if ((q->rlink = qrll) != NULL)
	qrll->bal = (intptr_t)q | (qrll->bal & BAL_MASK);

    /* qr->parent <- qrl; qr->bal <- (qrl_bal == -1); */
    qr->bal = (intptr_t)qrl | (qrl_bal == 2);
    if ((qr->llink = qrlr) != NULL)
	qrlr->bal = (intptr_t)qr | (qrlr->bal & BAL_MASK);
}

/* LR: rotate |q->llink| left, then rotate |q| right. */
static void
rotate_lr(hb_tree* restrict const tree, hb_node* restrict const q)
{
    hb_node* restrict const qp = PARENT(q);
    hb_node* restrict const ql = q->llink;
    hb_node* restrict const qlr = ql->rlink;

    ASSERT(BAL_NEG(q));
    ASSERT(BAL_POS(ql));

    const int qlr_bal = qlr->bal & BAL_MASK;
    hb_node* restrict const qlrl = qlr->llink;
    hb_node* restrict const qlrr = qlr->rlink;

    /* qlr->parent <- qp; qlr->bal <- 0; */
    qlr->bal = (intptr_t)qp;
    *(qp == NULL ? &tree->root : qp->llink == q ? &qp->llink : &qp->rlink) = qlr;
    qlr->llink = ql;
    qlr->rlink = q;

    /* q->parent <- qlr; q->bal = (qlr_bal == -1); */
    q->bal = (intptr_t)qlr | (qlr_bal == 2);
    if ((q->llink = qlrr) != NULL)
	qlrr->bal = (intptr_t)q | (qlrr->bal & BAL_MASK);

    /* ql->parent <- qlr; ql->bal <- -(qlr_bal == 1); */
    ql->bal = (intptr_t)qlr | ((qlr_bal == 1) << 1);
    if ((ql->rlink = qlrl) != NULL)
	qlrl->bal = (intptr_t)ql | (qlrl->bal & BAL_MASK);
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
	if (cmp < 0) {
	    parent = node; node = node->llink;
	} else if (cmp > 0) {
	    parent = node; node = node->rlink;
	} else
	    return (dict_insert_result) { &node->datum, false };
	if (parent->bal & BAL_MASK)
	    q = parent;
    }

    hb_node* const add = node = node_new(key);
    if (!node)
	return (dict_insert_result) { NULL, false };

    if (!(node->pptr = parent)) {
	ASSERT(tree->count == 0);
	ASSERT(tree->root == NULL);
	tree->root = node;
    } else {
	if (cmp < 0)
	    parent->llink = node;
	else
	    parent->rlink = node;

	while (parent != q) {
	    ASSERT((parent->bal & BAL_MASK) == 0);
	    if (parent->llink == node)
		parent->bal |= 2;
	    else
		parent->bal |= 1;
	    node = parent;
	    parent = PARENT(parent);
	}
	if (q) {
	    ASSERT(q->bal & BAL_MASK);
	    if (q->llink == node) {
		if (BAL_NEG(q)) { /* if q->balance == -1 */
		    if (BAL_POS(q->llink)) { /* if q->llink->balance == +1 */
			tree->rotation_count += 2;
			rotate_lr(tree, q);
		    } else {
			tree->rotation_count += 1;
			ASSERT(!rotate_r(tree, q));
		    }
		} else { /* else, q->balance == +1 */
		    ASSERT(BAL_POS(q));
		    q->bal &= ~BAL_MASK; /* q->balance <- 0 */
		}
	    } else {
		ASSERT(q->rlink == node);
		if (BAL_POS(q)) { /* if q->balance == +1 */
		    if (BAL_NEG(q->rlink)) { /* if q->rlink->balance == -1 */
			tree->rotation_count += 2;
			rotate_rl(tree, q);
		    } else {
			tree->rotation_count += 1;
			ASSERT(!rotate_l(tree, q));
		    }
		} else { /* else, q->balance == -1 */
		    ASSERT(BAL_NEG(q));
		    q->bal &= ~BAL_MASK; /* q->balance <- 0 */
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
    for (;;) {
	if (!node)
	    return (dict_remove_result) { NULL, NULL, false };
	int cmp = tree->cmp_func(key, node->key);
	if (cmp < 0)
	    node = node->llink;
	else if (cmp > 0)
	    node = node->rlink;
	else
	    break;
    }

    if (node->llink && node->rlink) {
	hb_node* restrict out =
	    BAL_POS(node) ? tree_node_min(node->rlink) : tree_node_max(node->llink);
	void* tmp;
	SWAP(node->key, out->key, tmp);
	SWAP(node->datum, out->datum, tmp);
	node = out;
    }

    const dict_remove_result result = { node->key, node->datum, true };
    hb_node* p = PARENT(node);
    hb_node* child = node->llink ? node->llink : node->rlink;
    FREE(node);
    tree->count--;
    if (child)
	child->bal = (intptr_t)p | (child->bal & BAL_MASK);
    if (!p) {
	tree->root = child;
	return result;
    }

    bool left = (p->llink == node);
    if (left) {
	p->llink = child;
    } else {
	ASSERT(p->rlink == node);
	p->rlink = child;
    }
    node = child;

    unsigned rotations = 0;
    for (;;) {
	if (left) {
	    ASSERT(p->llink == node);
	    if (BAL_POS(p)) { /* if p->balance == +1 */
		if (BAL_NEG(p->rlink)) { /* if p->rlink->balance == -1 */
		    rotations += 2;
		    rotate_rl(tree, p);
		} else {
		    rotations += 1;
		    if (rotate_l(tree, p))
			break;
		}
		node = PARENT(p);
	    } else if (BAL_NEG(p)) { /* else if p->balance == -1 */
		p->bal &= ~BAL_MASK; /* p->balance <- 0 */
		node = p;
	    } else { /* else, p->balance == 0 */
		ASSERT((p->bal & BAL_MASK) == 0);
		p->bal |= 1; /* p->balance <- +1 */
		break;
	    }
	} else {
	    ASSERT(p->rlink == node);
	    if (BAL_NEG(p)) { /* if p->balance == -1 */
		if (BAL_POS(p->llink)) { /* if p->llink->balance == +1 */
		    rotations += 2;
		    rotate_lr(tree, p);
		} else {
		    rotations += 1;
		    if (rotate_r(tree, p))
			break;
		}
		node = PARENT(p);
	    } else if (BAL_POS(p)) { /* else if p->balance == +1 */
		p->bal &= ~BAL_MASK; /* p->balance <- 0 */
		node = p;
	    } else { /* else, p->balance == 0 */
		ASSERT((p->bal & BAL_MASK) == 0);
		p->bal |= 2; /* p->balance <- -1 */
		break;
	    }
	}

	if (!(p = PARENT(node)))
	    break;
	if (p->llink == node) {
	    left = true;
	} else {
	    ASSERT(p->rlink == node);
	    left = false;
	}
    }
    tree->rotation_count += rotations;
    return result;
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
	ASSERT((((intptr_t)node) & 3) == 0); /* Ensure malloc returns aligned result. */
	node->key = key;
	node->datum = NULL;
	node->pptr = NULL; /* also sets balance to zero */
	node->llink = NULL;
	node->rlink = NULL;
    }
    return node;
}

static size_t
node_height(const hb_node* node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_height(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_height(node->rlink) + 1 : 0;
    return MAX(l, r);
}

static size_t
node_mheight(const hb_node* node)
{
    ASSERT(node != NULL);

    size_t l = node->llink ? node_mheight(node->llink) + 1 : 0;
    size_t r = node->rlink ? node_mheight(node->rlink) + 1 : 0;
    return MIN(l, r);
}

static size_t
node_pathlen(const hb_node* node, size_t level)
{
    ASSERT(node != NULL);

    size_t n = 0;
    if (node->llink)
	n += level + node_pathlen(node->llink, level + 1);
    if (node->rlink)
	n += level + node_pathlen(node->rlink, level + 1);
    return n;
}

static hb_node*
node_prev(hb_node* node)
{
    ASSERT(node != NULL);
    if (node->llink)
	return tree_node_max(node->llink);
    hb_node* parent = PARENT(node);
    while (parent && parent->llink == node) {
	node = parent;
	parent = PARENT(parent);
    }
    return parent;
}

static hb_node*
node_next(hb_node* node)
{
    ASSERT(node != NULL);
    if (node->rlink)
	return tree_node_min(node->rlink);
    hb_node* parent = PARENT(node);
    while (parent && parent->rlink == node) {
	node = parent;
	parent = PARENT(parent);
    }
    return parent;
}

static bool
node_verify(const hb_tree* tree, const hb_node* parent, const hb_node* node,
	    unsigned* height, size_t *count)
{
    ASSERT(tree != NULL);

    if (!parent) {
	VERIFY(tree->root == node);
    } else {
	if (parent->llink == node) {
	    if (node)
		VERIFY(tree->cmp_func(parent->key, node->key) > 0);
	} else {
	    ASSERT(parent->rlink == node);
	    if (node)
		VERIFY(tree->cmp_func(parent->key, node->key) < 0);
	}
    }
    if (node) {
	int bal = node->bal & BAL_MASK;
	VERIFY(bal >= 0);
	VERIFY(bal <= 2);
	if (bal == 2) {
	    VERIFY(node->llink != NULL);
	    bal = -1;
	} else if (bal == 1) {
	    VERIFY(node->rlink != NULL);
	}
	VERIFY(PARENT(node) == parent);
	unsigned lheight, rheight;
	if (!node_verify(tree, node, node->llink, &lheight, count) ||
	    !node_verify(tree, node, node->rlink, &rheight, count))
	    return false;
	VERIFY(bal == (int)rheight - (int)lheight);
	if (height)
	    *height = MAX(lheight, rheight) + 1;
	*count += 1;
    } else {
	if (height)
	    *height = 0;
    }
    return true;
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

    itor->node = itor->tree->root ? tree_node_min(itor->tree->root) : NULL;
    return itor->node != NULL;
}

bool
hb_itor_last(hb_itor* itor)
{
    ASSERT(itor != NULL);

    itor->node = itor->tree->root ? tree_node_max(itor->tree->root) : NULL;
    return itor->node != NULL;
}

bool
hb_itor_search(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = tree_search_node(itor->tree, key)) != NULL;
}

bool
hb_itor_search_le(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = tree_search_le_node(itor->tree, key)) != NULL;
}

bool
hb_itor_search_lt(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = tree_search_lt_node(itor->tree, key)) != NULL;
}

bool
hb_itor_search_ge(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = tree_search_ge_node(itor->tree, key)) != NULL;
}

bool
hb_itor_search_gt(hb_itor* itor, const void* key)
{
    ASSERT(itor != NULL);
    return (itor->node = tree_search_gt_node(itor->tree, key)) != NULL;
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
