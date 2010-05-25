/*
 * wb_tree.h
 *
 * Interface for weight balanced tree.
 * Copyright (C) 2001-2010 Farooq Mela.
 */

#ifndef _WB_TREE_H_
#define _WB_TREE_H_

#include "dict.h"

BEGIN_DECL

typedef struct wb_tree wb_tree;

wb_tree*	wb_tree_new(dict_compare_func key_cmp, dict_delete_func del_func);
dict*		wb_dict_new(dict_compare_func key_cmp, dict_delete_func del_func);
unsigned	wb_tree_destroy(wb_tree *tree);

int			wb_tree_insert(wb_tree *tree, void *key, void *datum, int overwrite);
int			wb_tree_probe(wb_tree *tree, void *key, void **datum);
void*		wb_tree_search(wb_tree *tree, const void *key);
const void*	wb_tree_csearch(const wb_tree *tree, const void *key);
int			wb_tree_remove(wb_tree *tree, const void *key);
unsigned	wb_tree_empty(wb_tree *tree);
unsigned	wb_tree_walk(wb_tree *tree, dict_visit_func visit);
unsigned	wb_tree_count(const wb_tree *tree);
unsigned	wb_tree_height(const wb_tree *tree);
unsigned	wb_tree_mheight(const wb_tree *tree);
unsigned	wb_tree_pathlen(const wb_tree *tree);
const void*	wb_tree_min(const wb_tree *tree);
const void*	wb_tree_max(const wb_tree *tree);
void		wb_tree_verify(const wb_tree *tree);

typedef struct wb_itor wb_itor;

wb_itor*	wb_itor_new(wb_tree *tree);
dict_itor*	wb_dict_itor_new(wb_tree *tree);
void		wb_itor_destroy(wb_itor *tree);

int			wb_itor_valid(const wb_itor *itor);
void		wb_itor_invalidate(wb_itor *itor);
int			wb_itor_next(wb_itor *itor);
int			wb_itor_prev(wb_itor *itor);
int			wb_itor_nextn(wb_itor *itor, unsigned count);
int			wb_itor_prevn(wb_itor *itor, unsigned count);
int			wb_itor_first(wb_itor *itor);
int			wb_itor_last(wb_itor *itor);
int			wb_itor_search(wb_itor *itor, const void *key);
const void*	wb_itor_key(const wb_itor *itor);
void*		wb_itor_data(wb_itor *itor);
const void*	wb_itor_cdata(const wb_itor *itor);
int			wb_itor_set_data(wb_itor *itor, void *datum, void **old_datum);
int			wb_itor_remove(wb_itor *itor, int del);

END_DECL

#endif /* !_WB_TREE_H_ */
