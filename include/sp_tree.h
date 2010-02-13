/*
 * sp_tree.h
 *
 * Definitions for splay binary search tree.
 * Copyright (C) 2001-2010 Farooq Mela.
 *
 * $Id$
 */

#ifndef _SP_TREE_H_
#define _SP_TREE_H_

#include "dict.h"

BEGIN_DECL

struct sp_tree;
typedef struct sp_tree sp_tree;

sp_tree	*sp_tree_new(dict_cmp_func key_cmp, dict_del_func key_del,
					 dict_del_func dat_del);
dict	*sp_dict_new(dict_cmp_func key_cmp, dict_del_func key_del,
					 dict_del_func dat_del);
void sp_tree_destroy(sp_tree *tree, int del);

int sp_tree_insert(sp_tree *tree, void *key, void *dat, int overwrite);
int sp_tree_probe(sp_tree *tree, void *key, void **dat);
void *sp_tree_search(sp_tree *tree, const void *key);
const void *sp_tree_csearch(const sp_tree *tree, const void *key);
int sp_tree_remove(sp_tree *tree, const void *key, int del);
void sp_tree_empty(sp_tree *tree, int del);
void sp_tree_walk(sp_tree *tree, dict_vis_func visit);
unsigned sp_tree_count(const sp_tree *tree);
unsigned sp_tree_height(const sp_tree *tree);
unsigned sp_tree_mheight(const sp_tree *tree);
unsigned sp_tree_pathlen(const sp_tree *tree);
const void *sp_tree_min(const sp_tree *tree);
const void *sp_tree_max(const sp_tree *tree);

struct sp_itor;
typedef struct sp_itor sp_itor;

sp_itor *sp_itor_new(sp_tree *tree);
dict_itor *sp_dict_itor_new(sp_tree *tree);
void sp_itor_destroy(sp_itor *tree);

int sp_itor_valid(const sp_itor *itor);
void sp_itor_invalidate(sp_itor *itor);
int sp_itor_next(sp_itor *itor);
int sp_itor_prev(sp_itor *itor);
int sp_itor_nextn(sp_itor *itor, unsigned count);
int sp_itor_prevn(sp_itor *itor, unsigned count);
int sp_itor_first(sp_itor *itor);
int sp_itor_last(sp_itor *itor);
int sp_itor_search(sp_itor *itor, const void *key);
const void *sp_itor_key(const sp_itor *itor);
void *sp_itor_data(sp_itor *itor);
const void *sp_itor_cdata(const sp_itor *itor);
int sp_itor_set_data(sp_itor *itor, void *dat, int del);
int sp_itor_remove(sp_itor *itor, int del);

END_DECL

#endif /* !_SP_TREE_H_ */
