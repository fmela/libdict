/*
 * skiplist.h
 *
 * Interface for skiplist.
 * Copyright (C) 2001-2010 Farooq Mela.
 *
 * $Id$
 */

#ifndef _SKIPLIST_H_
#define _SKIPLIST_H_

#include "dict.h"

BEGIN_DECL

typedef struct skiplist skiplist;

skiplist*	skiplist_new(dict_compare_func key_cmp, dict_delete_func del_func);
dict*		skiplist_dict_new(dict_compare_func key_cmp, dict_delete_func del_func);
unsigned	skiplist_destroy(skiplist *table);

int			skiplist_insert(skiplist *table, void *key, void *datum, int overwrite);
int			skiplist_probe(skiplist *table, void *key, void **datum);
void*		skiplist_search(skiplist *table, const void *key);
const void*	skiplist_csearch(const skiplist *table, const void *key);
int			skiplist_remove(skiplist *table, const void *key);
unsigned	skiplist_empty(skiplist *table);
unsigned	skiplist_walk(skiplist *table, dict_visit_func visit);
unsigned	skiplist_count(const skiplist *table);

typedef struct skiplist_itor skiplist_itor;

skiplist_itor*
			skiplist_itor_new(skiplist *table);
dict_itor*	skiplist_dict_itor_new(skiplist *table);
void		skiplist_itor_destroy(skiplist_itor *);

int			skiplist_itor_valid(const skiplist_itor *itor);
void		skiplist_itor_invalidate(skiplist_itor *itor);
int			skiplist_itor_next(skiplist_itor *itor);
int			skiplist_itor_prev(skiplist_itor *itor);
int			skiplist_itor_nextn(skiplist_itor *itor, unsigned count);
int			skiplist_itor_prevn(skiplist_itor *itor, unsigned count);
int			skiplist_itor_first(skiplist_itor *itor);
int			skiplist_itor_last(skiplist_itor *itor);
int			skiplist_itor_search(skiplist_itor *itor, const void *key);
const void*	skiplist_itor_key(const skiplist_itor *itor);
void*		skiplist_itor_data(skiplist_itor *itor);
const void*	skiplist_itor_cdata(const skiplist_itor *itor);
int			skiplist_itor_set_data(skiplist_itor *itor, void *datum, void **prev_datum);
int			skiplist_itor_remove(skiplist_itor *itor, int del);

END_DECL

#endif /* !_SKIPLIST_H_ */
