/*
 * hashtable.h
 *
 * Interface for chained hash table.
 * Copyright (C) 2001-2010 Farooq Mela.
 *
 * $Id$
 */

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include "dict.h"

BEGIN_DECL

typedef struct hashtable hashtable;

hashtable*	hashtable_new(dict_compare_func key_cmp, dict_hash_func key_hash,
				dict_delete_func del_func, unsigned size);
dict*		hashtable_dict_new(dict_compare_func key_cmp, dict_hash_func key_hash,
				dict_delete_func del_func, unsigned size);
unsigned	hashtable_destroy(hashtable *table);

int			hashtable_insert(hashtable *table, void *key, void *datum, int overwrite);
int			hashtable_probe(hashtable *table, void *key, void **datum);
void*		hashtable_search(hashtable *table, const void *key);
const void*	hashtable_csearch(const hashtable *table, const void *key);
int			hashtable_remove(hashtable *table, const void *key);
unsigned	hashtable_empty(hashtable *table);
unsigned	hashtable_walk(hashtable *table, dict_visit_func visit);
unsigned	hashtable_count(const hashtable *table);
unsigned	hashtable_size(const hashtable *table);
unsigned	hashtable_slots_used(const hashtable *table);
int			hashtable_resize(hashtable *table, unsigned new_size);

typedef struct hashtable_itor hashtable_itor;

hashtable_itor*
			hashtable_itor_new(hashtable *table);
dict_itor*	hashtable_dict_itor_new(hashtable *table);
void		hashtable_itor_destroy(hashtable_itor *);

int			hashtable_itor_valid(const hashtable_itor *itor);
void		hashtable_itor_invalidate(hashtable_itor *itor);
int			hashtable_itor_next(hashtable_itor *itor);
int			hashtable_itor_prev(hashtable_itor *itor);
int			hashtable_itor_nextn(hashtable_itor *itor, unsigned count);
int			hashtable_itor_prevn(hashtable_itor *itor, unsigned count);
int			hashtable_itor_first(hashtable_itor *itor);
int			hashtable_itor_last(hashtable_itor *itor);
int			hashtable_itor_search(hashtable_itor *itor, const void *key);
const void*	hashtable_itor_key(const hashtable_itor *itor);
void*		hashtable_itor_data(hashtable_itor *itor);
const void*	hashtable_itor_cdata(const hashtable_itor *itor);
int			hashtable_itor_set_data(hashtable_itor *itor, void *datum, void **prev_datum);
int			hashtable_itor_remove(hashtable_itor *itor, int del);

END_DECL

#endif /* !_HASHTABLE_H_ */
