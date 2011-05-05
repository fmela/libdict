/*
 * libdict -- skiplist implementation.
 * cf. [Pugh 1990], [Sedgewick 1998]
 *
 * Copyright (c) 2001-2011, Farooq Mela
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Farooq Mela.
 * 4. Neither the name of the Farooq Mela nor the
 *    names of contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY FAROOQ MELA ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL FAROOQ MELA BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>

#include "skiplist.h"
#include "dict_private.h"

typedef struct skip_node skip_node;

struct skip_node {
	void*				key;
	void*				datum;
/*	skip_node*			prev; */
	unsigned			link_count;
	skip_node*			link[1];
};

#define MAX_LINK		32

struct skiplist {
	skip_node*			head;
	unsigned			max_link;
	unsigned			top_link;
	dict_compare_func	cmp_func;
	dict_delete_func	del_func;
	unsigned			count;
	unsigned			randgen;
};

#define RGEN_A		1664525U
#define RGEN_M		1013904223U

struct skiplist_itor {
	skiplist*			list;
	skip_node*			node;
};

static dict_vtable skiplist_vtable = {
	(dict_inew_func)		skiplist_dict_itor_new,
	(dict_dfree_func)		skiplist_free,
	(dict_insert_func)		skiplist_insert,
	(dict_probe_func)		skiplist_probe,
	(dict_search_func)		skiplist_search,
	(dict_csearch_func)		skiplist_csearch,
	(dict_remove_func)		skiplist_remove,
	(dict_clear_func)		skiplist_clear,
	(dict_traverse_func)	skiplist_traverse,
	(dict_count_func)		skiplist_count
};

static itor_vtable skiplist_itor_vtable = {
	(dict_ifree_func)		skiplist_itor_free,
	(dict_valid_func)		skiplist_itor_valid,
	(dict_invalidate_func)	skiplist_itor_invalidate,
	(dict_next_func)		skiplist_itor_next,
	(dict_prev_func)		skiplist_itor_prev,
	(dict_nextn_func)		skiplist_itor_nextn,
	(dict_prevn_func)		skiplist_itor_prevn,
	(dict_first_func)		skiplist_itor_first,
	(dict_last_func)		skiplist_itor_last,
	(dict_key_func)			skiplist_itor_key,
	(dict_data_func)		skiplist_itor_data,
	(dict_cdata_func)		skiplist_itor_cdata,
	(dict_dataset_func)		skiplist_itor_set_data,
	(dict_iremove_func)		NULL,	/* skiplist_itor_remove not implemented yet */
	(dict_icompare_func)	NULL	/* skiplist_itor_compare not implemented yet */
};

static skip_node*		node_new(void *key, void *datum, unsigned link_count);
static int				node_insert(skiplist *list, void *key, void *datum,
									skip_node **update);
static unsigned			rand_link_count(skiplist *list);

skiplist *
skiplist_new(dict_compare_func cmp_func, dict_delete_func del_func, unsigned max_link)
{
	skiplist *list;

	ASSERT(max_link > 0);

	if (max_link > MAX_LINK)
		max_link = MAX_LINK;

	if ((list = MALLOC(sizeof(*list))) == NULL)
		return NULL;

	if ((list->head = node_new(NULL, NULL, max_link)) == NULL) {
		FREE(list);
		return NULL;
	}

	list->max_link = max_link;
	list->top_link = 0;
	list->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	list->del_func = del_func;
	list->count = 0;
	list->randgen = rand();

	return list;
}

dict *
skiplist_dict_new(dict_compare_func cmp_func, dict_delete_func del_func, unsigned max_link)
{
	dict *dct;

	if ((dct = MALLOC(sizeof(*dct))) == NULL)
		return NULL;

	if ((dct->_object = skiplist_new(cmp_func, del_func, max_link)) == NULL) {
		FREE(dct);
		return NULL;
	}
	dct->_vtable = &skiplist_vtable;

	return dct;
}

unsigned
skiplist_free(skiplist *list)
{
	unsigned count;

	ASSERT(list != NULL);

	count = skiplist_clear(list);

	FREE(list->head);
	FREE(list);

	return count;
}

int
node_insert(skiplist *list, void *key, void *datum, skip_node **update)
{
	skip_node *x;
	unsigned k, nlinks = rand_link_count(list);

	ASSERT(nlinks < list->max_link);

	x = node_new(key, datum, nlinks);
	if (x == NULL)
		return -1;

	while (list->top_link < nlinks) {
		for (k = list->top_link+1; k<=nlinks; k++) {
			ASSERT(update[k] == NULL);
			update[k] = list->head;
		}
		list->top_link = nlinks;
	}

	for (k = 0; k < nlinks; k++) {
		ASSERT(update[k]->link_count > k);
		x->link[k] = update[k]->link[k];
		update[k]->link[k] = x;
	}
	list->count++;
	return 0;
}

int
skiplist_insert(skiplist *list, void *key, void *datum, int overwrite)
{
	skip_node *x, *update[MAX_LINK] = { 0 };
	unsigned k;

	ASSERT(list != NULL);

	x = list->head;
	for (k = list->top_link+1; k-->0; ) {
		ASSERT(x->link_count > k);
		while (x->link[k]) {
			int cmp = list->cmp_func(key, x->link[k]->key);
			if (cmp < 0)
				break;
			x = x->link[k];
			ASSERT(x->link_count > k);
			if (cmp == 0) {
				if (!overwrite)
					return 1;
				if (list->del_func)
					list->del_func(x->key, x->datum);
				x->key = key;
				x->datum = datum;
				return 0;
			}
		}
		update[k] = x;
	}

	return node_insert(list, key, datum, update);
}

int
skiplist_probe(skiplist *list, void *key, void **datum)
{
	skip_node *x, *update[MAX_LINK] = { 0 };
	unsigned k;

	ASSERT(list != NULL);

	x = list->head;
	for (k = list->top_link+1; k-->0; ) {
		ASSERT(x->link_count > k);
		while (x->link[k]) {
			int cmp = list->cmp_func(key, x->link[k]->key);
			if (cmp < 0)
				break;
			x = x->link[k];
			ASSERT(x->link_count > k);
			if (cmp == 0) {
				*datum = x->datum;
				return 0;
			}
		}
		update[k] = x;
	}
	return node_insert(list, key, *datum, update);
}

void *
skiplist_search(skiplist *list, const void *key)
{
	skip_node *x;
	unsigned k;

	ASSERT(list != NULL);

	x = list->head;
	for (k = list->top_link+1; k-->0;) {
		while (x->link[k]) {
			int cmp = list->cmp_func(key, x->link[k]->key);
			if (cmp < 0)
				break;
			x = x->link[k];
			if (cmp == 0)
				return x->datum;
		}
	}
	return NULL;
}

const void *
skiplist_csearch(const skiplist *list, const void *key)
{
	ASSERT(list != NULL);

	return skiplist_search((skiplist *)list, key); /* Cast OK. */
}

int
skiplist_remove(skiplist *list, const void *key)
{
	skip_node *x, *update[MAX_LINK] = { 0 };
	unsigned k;

	ASSERT(list != NULL);

	x = list->head;
	for (k = list->top_link+1; k-->0;) {
		ASSERT(x->link_count > k);
		while (x->link[k] && list->cmp_func(key, x->link[k]->key) > 0)
			x = x->link[k];
		update[k] = x;
	}
	x = x->link[0];
	if (x == NULL || list->cmp_func(key, x->key) != 0)
		return -1;
	for (k = 0; k <= list->top_link; k++) {
		ASSERT(update[k] != NULL);
		ASSERT(update[k]->link_count > k);
		if (update[k]->link[k] != x)
			break;
		update[k]->link[k] = x->link[k];
	}
	if (list->del_func)
		list->del_func(x->key, x->datum);
	FREE(x);
	while (list->top_link > 0 && list->head->link[list->top_link] == NULL)
		list->top_link--;
	list->count--;
	return 0;
}

unsigned
skiplist_clear(skiplist *list)
{
	unsigned count;
	skip_node *node;

	ASSERT(list != NULL);

	node = list->head->link[0];
	while (node) {
		skip_node *next = node->link[0];
		if (list->del_func)
			list->del_func(node->key, node->datum);
		FREE(node);
		node = next;
	}

	count = list->count;
	list->count = 0;
	list->head->link[list->top_link] = NULL;
	while (list->top_link)
		list->head->link[--list->top_link] = NULL;

	return count;
}

unsigned
skiplist_traverse(skiplist *list, dict_visit_func visit)
{
	skip_node *node;
	unsigned count = 0;

	ASSERT(list != NULL);
	ASSERT(visit != NULL);

	for (node = list->head->link[0]; node; node = node->link[0]) {
		++count;
		if (!visit(node->key, node->datum))
			break;
	}
	return count;
}

unsigned
skiplist_count(const skiplist *list)
{
	ASSERT(list != NULL);

	return list->count;
}

#define RETVALID(itor)	return itor->node != NULL

skiplist_itor *
skiplist_itor_new(skiplist *list)
{
	skiplist_itor *itor;

	ASSERT(list != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;

	itor->list = list;
	itor->node = NULL;

	skiplist_itor_first(itor);
	return itor;
}

dict_itor *
skiplist_dict_itor_new(skiplist *list)
{
	dict_itor *itor;

	ASSERT(list != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;
	if ((itor->_itor = skiplist_itor_new(list)) == NULL) {
		FREE(itor);
		return NULL;
	}
	itor->_vtable = &skiplist_itor_vtable;

	return itor;
}

void
skiplist_itor_free(skiplist_itor *itor)
{
	ASSERT(itor != NULL);

	FREE(itor);
}

int
skiplist_itor_valid(const skiplist_itor *itor)
{
	ASSERT(itor != NULL);

	RETVALID(itor);
}

void
skiplist_itor_invalidate(skiplist_itor *itor)
{
	ASSERT(itor != NULL);

	itor->node = NULL;
}

int
skiplist_itor_next(skiplist_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		return skiplist_itor_first(itor);

	itor->node = itor->node->link[0];
	RETVALID(itor);
}

int
skiplist_itor_prev(skiplist_itor *itor)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		return skiplist_itor_last(itor);

/*	itor->node = itor->node->prev; */
	itor->node = NULL;
	RETVALID(itor);
}

int
skiplist_itor_nextn(skiplist_itor *itor, unsigned count)
{
	ASSERT(itor != NULL);

	if (!count)
		RETVALID(itor);

	while (count--) {
		if (!skiplist_itor_next(itor))
			return FALSE;
	}
	return TRUE;
}

int
skiplist_itor_prevn(skiplist_itor *itor, unsigned count)
{
	ASSERT(itor != NULL);

	if (!count)
		RETVALID(itor);

	while (count--) {
		if (!skiplist_itor_prev(itor))
			return FALSE;
	}
	return TRUE;
}

int
skiplist_itor_first(skiplist_itor *itor)
{
	ASSERT(itor != NULL);

	itor->node = itor->list->head->link[0];
	RETVALID(itor);
}

int
skiplist_itor_last(skiplist_itor *itor)
{
	unsigned k;
	skip_node *x;

	ASSERT(itor != NULL);

	x = itor->list->head;
	for (k = itor->list->top_link; k-->0;) {
		while (x->link[k])
			x = x->link[k];
	}
	if (x == itor->list->head) {
		itor->node = NULL;
		return FALSE;
	} else {
		itor->node = x;
		return TRUE;
	}
}

int
skiplist_itor_search(skiplist_itor *itor, const void *key)
{
	skiplist *list;
	skip_node *x;
	unsigned k;

	ASSERT(itor != NULL);

	list = itor->list;
	x = list->head;
	for (k = list->top_link+1; k-->0;) {
		while (x->link[k]) {
			int cmp = list->cmp_func(key, x->link[k]->key);
			if (cmp < 0)
				break;
			x = x->link[k];
			if (cmp == 0) {
				itor->node = x;
				return TRUE;
			}
		}
	}
	itor->node = NULL;
	return FALSE;
}

const void *
skiplist_itor_key(const skiplist_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->key : NULL;
}

void *
skiplist_itor_data(skiplist_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->datum : NULL;
}

const void *
skiplist_itor_cdata(const skiplist_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->datum : NULL;
}

int
skiplist_itor_set_data(skiplist_itor *itor, void *datum, void **prev_datum)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		return -1;

	if (prev_datum)
		*prev_datum = itor->node->datum;
	itor->node->datum = datum;
	return 0;
}

skip_node*
node_new(void *key, void *datum, unsigned link_count)
{
	skip_node *node;

	ASSERT(link_count >= 1);

	node = MALLOC(sizeof(*node) + sizeof(node->link[0]) * (link_count - 1));
	if (node == NULL)
		return NULL;

	node->key = key;
	node->datum = datum;
/*	node->prev = NULL; */
	node->link_count = link_count;
	memset(node->link, 0, sizeof(node->link[0]) * link_count);
	return node;
}

static unsigned
rand_link_count(skiplist *list)
{
	unsigned r, i;

	r = list->randgen = list->randgen * RGEN_A + RGEN_M;

	for (i=1; i+1<list->max_link; i++)
		if (r > (1U<<(32-i)))
/*		if (r & (1U<<(32-i))) */
			break;
	return i;
}

void
skiplist_verify(const skiplist *list)
{
	const skip_node *node, *next;
	unsigned k;

	ASSERT(list != NULL);
	ASSERT(list->top_link <= list->max_link);

	node = list->head->link[0];
	while (node != NULL) {
		ASSERT(node->link_count >= 1);
		if (node->link_count > list->top_link) {
			printf("Node has link count %u but list top link is %u.\n",
				   node->link_count, list->top_link);
		}
	/*	ASSERT(node->link_count <= list->top_link); */
		for (k = 0; k < node->link_count; k++) {
			next = node->link[k];
			if (next != NULL)
				ASSERT(next->link_count >= k);
		}

		node = node->link[0];
	}
}
