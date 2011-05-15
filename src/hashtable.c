/*
 * libdict -- chained hash-table, with chains sorted by hash, implementation.
 * cf. [Gonnet 1984], [Knuth 1998]
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
#include <string.h> /* For memset() */

#include "hashtable.h"
#include "dict_private.h"

typedef struct hash_node hash_node;

struct hash_node {
	void*				key;
	void*				datum;
	unsigned			hash;	/* Untruncated hash value. */
	hash_node*			next;
	hash_node*			prev;	/* Only because iterators are bidirectional. */
};

struct hashtable {
	hash_node**			table;
	unsigned			size;
	dict_compare_func	cmp_func;
	dict_hash_func		hash_func;
	dict_delete_func	del_func;
	unsigned			count;
};

struct hashtable_itor {
	hashtable*			table;
	hash_node*			node;
	unsigned			slot;
};

static dict_vtable hashtable_vtable = {
	(dict_inew_func)		hashtable_dict_itor_new,
	(dict_dfree_func)		hashtable_free,
	(dict_insert_func)		hashtable_insert,
	(dict_probe_func)		hashtable_probe,
	(dict_search_func)		hashtable_search,
	(dict_csearch_func)		hashtable_search,
	(dict_remove_func)		hashtable_remove,
	(dict_clear_func)		hashtable_clear,
	(dict_traverse_func)	hashtable_traverse,
	(dict_count_func)		hashtable_count
};

static itor_vtable hashtable_itor_vtable = {
	(dict_ifree_func)		hashtable_itor_free,
	(dict_valid_func)		hashtable_itor_valid,
	(dict_invalidate_func)	hashtable_itor_invalidate,
	(dict_next_func)		hashtable_itor_next,
	(dict_prev_func)		hashtable_itor_prev,
	(dict_nextn_func)		hashtable_itor_nextn,
	(dict_prevn_func)		hashtable_itor_prevn,
	(dict_first_func)		hashtable_itor_first,
	(dict_last_func)		hashtable_itor_last,
	(dict_key_func)			hashtable_itor_key,
	(dict_data_func)		hashtable_itor_data,
	(dict_cdata_func)		hashtable_itor_cdata,
	(dict_dataset_func)		hashtable_itor_set_data,
	(dict_iremove_func)		NULL,/* hashtable_itor_remove not implemented yet */
	(dict_icompare_func)	NULL/* hashtable_itor_compare not implemented yet */
};

hashtable *
hashtable_new(dict_compare_func cmp_func, dict_hash_func hash_func,
			  dict_delete_func del_func, unsigned size)
{
	hashtable *table;
	unsigned i;

	ASSERT(hash_func != NULL);
	ASSERT(size != 0);

	if ((table = MALLOC(sizeof(*table))) == NULL)
		return NULL;
	if ((table->table = MALLOC(size * sizeof(hash_node *))) == NULL) {
		FREE(table);
		return NULL;
	}
	for (i = 0; i < size; i++)
		table->table[i] = NULL;

	table->size = size;
	table->cmp_func = cmp_func ? cmp_func : dict_ptr_cmp;
	table->hash_func = hash_func;
	table->del_func = del_func;
	table->count = 0;

	return table;
}

dict *
hashtable_dict_new(dict_compare_func cmp_func, dict_hash_func hash_func,
				   dict_delete_func del_func, unsigned size)
{
	dict *dct;

	ASSERT(hash_func != NULL);
	ASSERT(size != 0);

	if ((dct = MALLOC(sizeof(*dct))) == NULL)
		return NULL;

	dct->_object = hashtable_new(cmp_func, hash_func, del_func, size);
	if (dct->_object == NULL) {
		FREE(dct);
		return NULL;
	}
	dct->_vtable = &hashtable_vtable;

	return dct;
}

unsigned
hashtable_free(hashtable *table)
{
	unsigned count;

	ASSERT(table != NULL);

	count = hashtable_clear(table);

	FREE(table->table);
	FREE(table);

	return count;
}

int
hashtable_insert(hashtable *table, void *key, void *datum, int overwrite)
{
	unsigned hash, mhash;
	hash_node *node, *prev, *add;

	ASSERT(table != NULL);

	hash = table->hash_func(key);
	mhash = hash % table->size;
	prev = NULL;
	for (node = table->table[mhash]; node; prev = node, node = node->next) {
		if (hash < node->hash)
			break;

		if (hash == node->hash && table->cmp_func(key, node->key) == 0) {
			if (!overwrite)
				return 1;
			if (table->del_func)
				table->del_func(node->key, node->datum);
			node->key = key;
			node->datum = datum;
			return 0;
		}
	}

	if ((add = MALLOC(sizeof(*add))) == NULL)
		return -1;

	add->key = key;
	add->datum = datum;
	add->hash = hash;

	add->prev = prev;
	if (prev)
		prev->next = add;
	else
		table->table[mhash] = add;

	add->next = node;
	if (node)
		node->prev = add;

	table->count++;
	return 0;
}

int
hashtable_probe(hashtable *table, void *key, void **datum)
{
	unsigned hash, mhash;
	hash_node *node, *prev, *add;

	ASSERT(table != NULL);
	ASSERT(datum != NULL);

	mhash = (hash = table->hash_func(key)) % table->size;

	prev = NULL;
	for (node = table->table[mhash]; node; prev = node, node = node->next) {
		if (hash < node->hash)
			break;

		if (hash == node->hash && table->cmp_func(key, node->key) == 0) {
			*datum = node->datum;
			return 0;
		}
	}

	if ((add = MALLOC(sizeof(*add))) == NULL)
		return -1;

	add->key = key;
	add->datum = *datum;
	add->hash = hash;

	add->prev = prev;
	if (prev)
		prev->next = add;
	else
		table->table[mhash] = add;

	add->next = node;
	if (node)
		node->prev = add;

	table->count++;
	return 1;
}

void *
hashtable_search(hashtable *table, const void *key)
{
	unsigned hash;
	hash_node *node;

	ASSERT(table != NULL);

	hash = table->hash_func(key);
	for (node = table->table[hash % table->size]; node; node = node->next) {
		if (hash < node->hash)
			break;
		if (hash == node->hash && table->cmp_func(key, node->key) == 0)
			return node->datum;
	}
	return NULL;
}

int
hashtable_remove(hashtable *table, const void *key)
{
	hash_node *node, *prev;
	unsigned hash, mhash;

	ASSERT(table != NULL);

	mhash = (hash = table->hash_func(key)) % table->size;

	prev = NULL;
	for (node = table->table[mhash]; node; prev = node, node = node->next) {
		if (hash < node->hash)
			break;

		if (hash == node->hash && table->cmp_func(key, node->key) == 0) {
			if (prev)
				prev->next = node->next;
			else
				table->table[mhash] = node->next;

			if (node->next)
				node->next->prev = prev;

			if (table->del_func)
				table->del_func(node->key, node->datum);

			FREE(node);
			table->count--;
			return 0;
		}
	}
	return -1;
}

unsigned
hashtable_clear(hashtable *table)
{
	unsigned slot, count;

	ASSERT(table != NULL);

	count = table->count;

	for (slot = 0; slot < table->size; slot++) {
		hash_node *node = table->table[slot];
		while (node != NULL) {
			hash_node *next = node->next;
			if (table->del_func)
				table->del_func(node->key, node->datum);
			FREE(node);
			node = next;
		}
		table->table[slot] = NULL;
	}

	table->count = 0;
	return count;
}

unsigned
hashtable_traverse(hashtable *table, dict_visit_func visit)
{
	hash_node *node;
	unsigned i, count = 0;

	ASSERT(table != NULL);
	ASSERT(visit != NULL);

	for (i = 0; i < table->size; i++)
		for (node = table->table[i]; node; node = node->next) {
			++count;
			if (!visit(node->key, node->datum))
				goto out;
		}

out:
	return count;
}

unsigned
hashtable_count(const hashtable *table)
{
	ASSERT(table != NULL);

	return table->count;
}

unsigned
hashtable_size(const hashtable *table)
{
	ASSERT(table != NULL);

	return table->size;
}

unsigned
hashtable_slots_used(const hashtable *table)
{
	unsigned i, count = 0;

	ASSERT(table != NULL);

	for (i = 0; i < table->size; i++)
		if (table->table[i])
			count++;
	return count;
}

int
hashtable_resize(hashtable *table, unsigned size)
{
	hash_node **ntable;
	hash_node *node, *next;
	unsigned i, hash;
	size_t table_memory;

	ASSERT(table != NULL);
	ASSERT(size != 0);

	if (table->size == size)
		return 0;

	/* TODO: use REALLOC instead of MALLOC. */
	table_memory = size * sizeof(hash_node *);
	if ((ntable = MALLOC(table_memory)) == NULL)
		return -1;
	memset(ntable, 0, table_memory);

	/* FIXME: insert data in sorted order in the new list. */
	for (i = 0; i < table->size; i++) {
		for (node = table->table[i]; node; node = next) {
			next = node->next;
			hash = table->hash_func(node->key) % size;
			node->next = ntable[hash];
			node->prev = NULL;
			if (ntable[hash])
				ntable[hash]->prev = node;
			ntable[hash] = node;
		}
	}

	FREE(table->table);
	table->table = ntable;
	table->size = size;

	return 0;
}

#define RETVALID(itor)	return itor->node != NULL

hashtable_itor *
hashtable_itor_new(hashtable *table)
{
	hashtable_itor *itor;

	ASSERT(table != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;

	itor->table = table;
	itor->node = NULL;
	itor->slot = 0;

	hashtable_itor_first(itor);
	return itor;
}

dict_itor *
hashtable_dict_itor_new(hashtable *table)
{
	dict_itor *itor;

	ASSERT(table != NULL);

	if ((itor = MALLOC(sizeof(*itor))) == NULL)
		return NULL;
	if ((itor->_itor = hashtable_itor_new(table)) == NULL) {
		FREE(itor);
		return NULL;
	}
	itor->_vtable = &hashtable_itor_vtable;

	return itor;
}

void
hashtable_itor_free(hashtable_itor *itor)
{
	ASSERT(itor != NULL);

	FREE(itor);
}

int
hashtable_itor_valid(const hashtable_itor *itor)
{
	ASSERT(itor != NULL);

	RETVALID(itor);
}

void
hashtable_itor_invalidate(hashtable_itor *itor)
{
	ASSERT(itor != NULL);

	itor->node = NULL;
	itor->slot = 0;
}

int
hashtable_itor_next(hashtable_itor *itor)
{
	unsigned slot;
	hash_node *node;

	ASSERT(itor != NULL);

	if ((node = itor->node) == NULL)
		return hashtable_itor_first(itor);

	slot = itor->slot;
	node = node->next;
	if (node) {
		itor->node = node;
		return 1;
	}

	while (++slot < itor->table->size)
		if ((node = itor->table->table[slot]) != NULL)
			break;
	itor->node = node;
	itor->slot = node ? slot : 0;
	RETVALID(itor);
}

int
hashtable_itor_prev(hashtable_itor *itor)
{
	unsigned slot;
	hash_node *node;

	ASSERT(itor != NULL);

	if ((node = itor->node) == NULL)
		return hashtable_itor_last(itor);

	slot = itor->slot;
	node = node->prev;
	if (node) {
		itor->node = node;
		return 1;
	}

	while (slot > 0)
		if ((node = itor->table->table[--slot]) != NULL) {
			for (; node->next; node = node->next)
				/* void */;
			break;
		}
	itor->node = node;
	itor->slot = slot;

	RETVALID(itor);
}

int
hashtable_itor_nextn(hashtable_itor *itor, unsigned count)
{
	ASSERT(itor != NULL);

	if (!count)
		RETVALID(itor);

	while (count) {
		if (!hashtable_itor_next(itor))
			break;
		count--;
	}
	return count == 0;
}

int
hashtable_itor_prevn(hashtable_itor *itor, unsigned count)
{
	ASSERT(itor != NULL);

	if (!count)
		RETVALID(itor);

	while (count) {
		if (!hashtable_itor_prev(itor))
			break;
		count--;
	}
	return count == 0;
}

int
hashtable_itor_first(hashtable_itor *itor)
{
	unsigned slot;

	ASSERT(itor != NULL);

	for (slot = 0; slot < itor->table->size; slot++)
		if (itor->table->table[slot])
			break;
	if (slot == itor->table->size) {
		itor->node = NULL;
		itor->slot = 0;
	} else {
		itor->node = itor->table->table[slot];
		itor->slot = (int)slot;
	}
	RETVALID(itor);
}

int
hashtable_itor_last(hashtable_itor *itor)
{
	unsigned slot;

	ASSERT(itor != NULL);

	for (slot = itor->table->size; slot;)
		if (itor->table->table[--slot])
			break;
	if ((int)slot < 0) {
		itor->node = NULL;
		itor->slot = 0;
	} else {
		hash_node *node;

		for (node = itor->table->table[slot]; node->next; node = node->next)
			/* void */;
		itor->node = node;
		itor->slot = slot;
	}
	RETVALID(itor);
}

int
hashtable_itor_search(hashtable_itor *itor, const void *key)
{
	hash_node *node;
	unsigned hash, mhash;

	mhash = (hash = itor->table->hash_func(key)) % itor->table->size;
	for (node = itor->table->table[mhash]; node; node = node->next) {
		if (hash < node->hash)
			break;

		if (hash == node->hash && itor->table->cmp_func(key, node->key) == 0) {
			itor->node = node;
			itor->slot = mhash;
			return TRUE;
		}
	}
	itor->node = NULL;
	itor->slot = 0;
	return FALSE;
}

const void *
hashtable_itor_key(const hashtable_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->key : NULL;
}

void *
hashtable_itor_data(hashtable_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->datum : NULL;
}

const void *
hashtable_itor_cdata(const hashtable_itor *itor)
{
	ASSERT(itor != NULL);

	return itor->node ? itor->node->datum : NULL;
}

int
hashtable_itor_set_data(hashtable_itor *itor, void *datum, void **prev_datum)
{
	ASSERT(itor != NULL);

	if (itor->node == NULL)
		return -1;

	if (prev_datum)
		*prev_datum = itor->node->datum;
	itor->node->datum = datum;
	return 0;
}
