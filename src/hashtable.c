/*
 * hashtable.c
 *
 * Implementation for chained hash table.
 * Copyright (C) 2001-2010 Farooq Mela.
 *
 * $Id$
 *
 * cf. [Gonnet 1984], [Knuth 1998]
 */

#include <stdlib.h>

#include "hashtable.h"
#include "dict_private.h"

/*
 * We store the untruncated hash value in the "hash" field. This speeds up
 * searching and allows us to resize without recomputing the original hash
 * value.
 *
 * If it weren't for iterators, there would be no reason have a "prev" field.
 */

typedef struct hash_node hash_node;

struct hash_node {
	void*				key;
	void*				datum;
	unsigned			hash;
	hash_node*			next;
	hash_node*			prev;
};

struct hashtable {
	hash_node**			table;
	unsigned			size;
	dict_compare_func	key_cmp;
	dict_hash_func		key_hash;
	dict_delete_func	del_func;
	unsigned			count;
};

struct hashtable_itor {
	hashtable*			table;
	hash_node*			node;
	unsigned			slot;
};

static dict_vtable hashtable_vtable = {
	(inew_func)			hashtable_dict_itor_new,
	(destroy_func)		hashtable_destroy,
	(insert_func)		hashtable_insert,
	(probe_func)		hashtable_probe,
	(search_func)		hashtable_search,
	(csearch_func)		hashtable_csearch,
	(remove_func)		hashtable_remove,
	(empty_func)		hashtable_empty,
	(walk_func)			hashtable_walk,
	(count_func)		hashtable_count
};

static itor_vtable hashtable_itor_vtable = {
	(idestroy_func)		hashtable_itor_destroy,
	(valid_func)		hashtable_itor_valid,
	(invalidate_func)	hashtable_itor_invalidate,
	(next_func)			hashtable_itor_next,
	(prev_func)			hashtable_itor_prev,
	(nextn_func)		hashtable_itor_nextn,
	(prevn_func)		hashtable_itor_prevn,
	(first_func)		hashtable_itor_first,
	(last_func)			hashtable_itor_last,
	(key_func)			hashtable_itor_key,
	(data_func)			hashtable_itor_data,
	(cdata_func)		hashtable_itor_cdata,
	(dataset_func)		hashtable_itor_set_data,
	(iremove_func)		NULL, /* hashtable_itor_remove not implemented yet */
	(compare_func)		NULL  /* hashtable_itor_compare not implemented yet  */
};

hashtable *
hashtable_new(dict_compare_func key_cmp, dict_hash_func key_hash, dict_delete_func del_func, unsigned size)
{
	hashtable *table;
	unsigned i;

	ASSERT(key_hash != NULL);
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
	table->key_cmp = key_cmp ? key_cmp : dict_ptr_cmp;
	table->key_hash = key_hash;
	table->del_func = del_func;
	table->count = 0;

	return table;
}

dict *
hashtable_dict_new(dict_compare_func key_cmp, dict_hash_func key_hash, dict_delete_func del_func, unsigned size)
{
	dict *dct;

	ASSERT(key_hash != NULL);
	ASSERT(size != 0);

	if ((dct = MALLOC(sizeof(*dct))) == NULL)
		return NULL;

	if ((dct->_object = hashtable_new(key_cmp, key_hash, del_func, size)) == NULL) {
		FREE(dct);
		return NULL;
	}
	dct->_vtable = &hashtable_vtable;

	return dct;
}

unsigned
hashtable_destroy(hashtable *table)
{
	unsigned count;

	ASSERT(table != NULL);

	count = hashtable_empty(table);

	FREE(table->table);
	FREE(table);

	return count;
}

int
hashtable_insert(hashtable *table, void *key, void *datum, int overwrite)
{
	unsigned hash, mhash;
	hash_node *node, *add;

	ASSERT(table != NULL);

	hash = table->key_hash(key);

	mhash = hash % table->size;
	for (node = table->table[mhash]; node; node = node->next)
		if (hash == node->hash && table->key_cmp(key, node->key) == 0) {
			if (overwrite == 0)
				return 1;
			if (table->del_func)
				table->del_func(node->key, node->datum);
			node->key = key;
			node->datum = datum;
			return 0;
		}

	if ((add = MALLOC(sizeof(*add))) == NULL)
		return -1;
	add->key = key;
	add->datum = datum;
	add->hash = hash;
	add->prev = NULL;

	add->next = table->table[mhash];
	if (table->table[mhash])
		table->table[mhash]->prev = add;
	table->table[mhash] = add;
	table->count++;

	return 0;
}

int
hashtable_probe(hashtable *table, void *key, void **datum)
{
	unsigned hash, mhash;
	hash_node *node, *prev, *add;
	void *tmp;

	ASSERT(table != NULL);
	ASSERT(datum != NULL);

	hash = table->key_hash(key);
	mhash = hash % table->size;

	prev = NULL;
	node = table->table[mhash];
	for (; node; prev = node, node = node->next)
		if (hash == node->hash && table->key_cmp(key, node->key) == 0)
			break;
	if (node) {
		if (prev) { /* Tranpose. */
			SWAP(prev->key, node->key, tmp);
			SWAP(prev->datum, node->datum, tmp);
			SWAP(prev->hash, node->hash, hash);
			node = prev;
		}
		*datum = node->datum;
		return 0;
	}

	if ((add = MALLOC(sizeof(*add))) == NULL)
		return -1;
	add->key = key;
	add->datum = *datum;
	add->hash = hash;
	add->prev = NULL;

	add->next = table->table[mhash];
	if (table->table[mhash])
		table->table[mhash]->prev = add;
	table->table[mhash] = add;
	table->count++;
	return 1;
}

void *
hashtable_search(hashtable *table, const void *key)
{
	unsigned hash;
	hash_node *node, *prev;
	void *tmp;

	ASSERT(table != NULL);

	hash = table->key_hash(key);
	prev = NULL;
	node = table->table[hash % table->size];
	for (; node; prev = node, node = node->next)
		if (hash == node->hash && table->key_cmp(key, node->key) == 0)
			break;
	if (node) {
		if (prev) {
			/*
			 * Tranpose. This typically offers better performance than move-to-
			 * front, but requires a fairly large number of accesses to
			 * take a randomly ordered chain and re-arrange it to nearly
			 * optimal. According to [Gonnet 1984] it may take Big-Omega(n^2)
			 * to come within 1+epsilon of the final state.
			 */
			SWAP(prev->key, node->key, tmp);
			SWAP(prev->datum, node->datum, tmp);
			SWAP(prev->hash, node->hash, hash);
			node = prev;
		}
		/* Node was already at front of list. */
		return node->datum;
	}
	return NULL;
}

const void *
hashtable_csearch(const hashtable *table, const void *key)
{
	ASSERT(table != NULL);

	/*
	 * Cast OK, we want to be able to tranpose, which doesnt modify the
	 * contents of table, only the ordering of items on the chain.
	 */
	return hashtable_search((hashtable *)table, key);
}

int
hashtable_remove(hashtable *table, const void *key)
{
	hash_node *node, *prev;
	unsigned hash, mhash;

	ASSERT(table != NULL);

	hash = table->key_hash(key);
	mhash = hash % table->size;

	prev = NULL;
	node = table->table[mhash];
	for (; node; prev = node, node = node->next)
		if (hash == node->hash && table->key_cmp(key, node->key) == 0)
			break;
	if (node == NULL)
		return -1;

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

unsigned
hashtable_empty(hashtable *table)
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
hashtable_walk(hashtable *table, dict_visit_func visit)
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

	ASSERT(table != NULL);
	ASSERT(size != 0);

	if (table->size == size)
		return 0;

	if ((ntable = MALLOC(size * sizeof(hash_node *))) == NULL)
		return -1;

	for (i = 0; i < size; i++)
		ntable[i] = NULL;

	/*
	 * This way of resizing completely reverses(!) the effects of the trans-
	 * positions that we have been doing in probes and lookups. Hopefully
	 * resizing the hashtable is something that is done rarely or not at all,
	 * so this won't make too much difference.
	 */
	for (i = 0; i < table->size; i++) {
		for (node = table->table[i]; node; node = next) {
			next = node->next;
			hash = node->hash % size;
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
hashtable_itor_destroy(hashtable_itor *itor)
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
	unsigned hash;

	hash = itor->table->key_hash(key);
	node = itor->table->table[hash % itor->table->size];
	for (; node; node = node->next)
		if (hash == node->hash && itor->table->key_cmp(key, node->key) == 0)
			break;
	itor->node = node;
	itor->slot = node ? (hash % itor->table->size) : 0;
	return node != NULL;
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
