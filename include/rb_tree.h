/*
 * rb_tree.h
 *
 * Definitions for red-black binary search tree.
 * Copyright (C) 2001-2010 Farooq Mela.
 *
 * $Id$
 */

#ifndef _RB_TREE_H_
#define _RB_TREE_H_

#include "dict.h"

BEGIN_DECL

struct rb_tree;
typedef struct rb_tree rb_tree;

rb_tree	*rb_tree_new(dict_cmp_func key_cmp, dict_del_func key_del,
					 dict_del_func dat_del);
dict	*rb_dict_new(dict_cmp_func key_cmp, dict_del_func key_del,
					 dict_del_func dat_del);
void rb_tree_destroy(rb_tree *tree, int del);

int rb_tree_insert(rb_tree *tree, void *key, void *dat, int overwrite);
int rb_tree_probe(rb_tree *tree, void *key, void **dat);
void *rb_tree_search(rb_tree *tree, const void *key);
const void *rb_tree_csearch(const rb_tree *tree, const void *key);
int rb_tree_remove(rb_tree *tree, const void *key, int del);
void rb_tree_empty(rb_tree *tree, int del);
void rb_tree_walk(rb_tree *tree, dict_vis_func visit);
unsigned rb_tree_count(const rb_tree *tree);
unsigned rb_tree_height(const rb_tree *tree);
unsigned rb_tree_mheight(const rb_tree *tree);
unsigned rb_tree_pathlen(const rb_tree *tree);
const void *rb_tree_min(const rb_tree *tree);
const void *rb_tree_max(const rb_tree *tree);

struct rb_itor;
typedef struct rb_itor rb_itor;

rb_itor *rb_itor_new(rb_tree *tree);
dict_itor *rb_dict_itor_new(rb_tree *tree);
void rb_itor_destroy(rb_itor *tree);

int rb_itor_valid(const rb_itor *itor);
void rb_itor_invalidate(rb_itor *itor);
int rb_itor_next(rb_itor *itor);
int rb_itor_prev(rb_itor *itor);
int rb_itor_nextn(rb_itor *itor, unsigned count);
int rb_itor_prevn(rb_itor *itor, unsigned count);
int rb_itor_first(rb_itor *itor);
int rb_itor_last(rb_itor *itor);
int rb_itor_search(rb_itor *itor, const void *key);
const void *rb_itor_key(const rb_itor *itor);
void *rb_itor_data(rb_itor *itor);
const void *rb_itor_cdata(const rb_itor *itor);
int rb_itor_set_data(rb_itor *itor, void *dat, int del);
int rb_itor_remove(rb_itor *itor, int del);

END_DECL

#endif /* !_RB_TREE_H_ */
