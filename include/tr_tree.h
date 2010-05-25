/*
 * tr_tree.h
 *
 * Interface for treap.
 * Copyright (C) 2001-2010 Farooq Mela.
 */

#ifndef _TR_TREE_H_
#define _TR_TREE_H_

#include "dict.h"

BEGIN_DECL

typedef struct tr_tree tr_tree;

tr_tree*	tr_tree_new(dict_compare_func key_cmp, dict_delete_func del_func);
dict*		tr_dict_new(dict_compare_func key_cmp, dict_delete_func del_func);
unsigned	tr_tree_destroy(tr_tree *tree);

int			tr_tree_insert(tr_tree *tree, void *key, void *datum, int overwrite);
int			tr_tree_probe(tr_tree *tree, void *key, void **datum);
void*		tr_tree_search(tr_tree *tree, const void *key);
const void*	tr_tree_csearch(const tr_tree *tree, const void *key);
int			tr_tree_remove(tr_tree *tree, const void *key);
unsigned	tr_tree_empty(tr_tree *tree);
unsigned	tr_tree_walk(tr_tree *tree, dict_visit_func visit);
unsigned	tr_tree_count(const tr_tree *tree);
unsigned	tr_tree_height(const tr_tree *tree);
unsigned	tr_tree_mheight(const tr_tree *tree);
unsigned	tr_tree_pathlen(const tr_tree *tree);
const void*	tr_tree_min(const tr_tree *tree);
const void*	tr_tree_max(const tr_tree *tree);

typedef struct tr_itor tr_itor;

tr_itor*	tr_itor_new(tr_tree *tree);
dict_itor*	tr_dict_itor_new(tr_tree *tree);
void		tr_itor_destroy(tr_itor *tree);

int			tr_itor_valid(const tr_itor *itor);
void		tr_itor_invalidate(tr_itor *itor);
int			tr_itor_next(tr_itor *itor);
int			tr_itor_prev(tr_itor *itor);
int			tr_itor_nextn(tr_itor *itor, unsigned count);
int			tr_itor_prevn(tr_itor *itor, unsigned count);
int			tr_itor_first(tr_itor *itor);
int			tr_itor_last(tr_itor *itor);
int			tr_itor_search(tr_itor *itor, const void *key);
const void*	tr_itor_key(const tr_itor *itor);
void*		tr_itor_data(tr_itor *itor);
const void*	tr_itor_cdata(const tr_itor *itor);
int			tr_itor_set_data(tr_itor *itor, void *datum, void **old_datum);
int			tr_itor_remove(tr_itor *itor, int del);

END_DECL

#endif /* !_TR_TREE_H_ */
