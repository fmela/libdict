/*
 * pr_tree.h
 *
 * Interface for path reduction tree.
 * Copyright (C) 2001-2008 Farooq Mela.
 *
 * $Id$
 */

#ifndef _PR_TREE_H_
#define _PR_TREE_H_

#include "dict.h"

BEGIN_DECL

struct pr_tree;
typedef struct pr_tree pr_tree;

pr_tree *pr_tree_new(dict_cmp_func key_cmp, dict_del_func key_del,
						  dict_del_func dat_del);
dict	*pr_dict_new(dict_cmp_func key_cmp, dict_del_func key_del,
						  dict_del_func dat_del);
void	 pr_tree_destroy(pr_tree *tree, int del);

int pr_tree_insert(pr_tree *tree, void *key, void *dat, int overwrite);
int pr_tree_probe(pr_tree *tree, void *key, void **dat);
void *pr_tree_search(pr_tree *tree, const void *key);
const void *pr_tree_csearch(const pr_tree *tree, const void *key);
int pr_tree_remove(pr_tree *tree, const void *key, int del);
void pr_tree_empty(pr_tree *tree, int del);
void pr_tree_walk(pr_tree *tree, dict_vis_func visit);
unsigned pr_tree_count(const pr_tree *tree);
unsigned pr_tree_height(const pr_tree *tree);
unsigned pr_tree_mheight(const pr_tree *tree);
unsigned pr_tree_pathlen(const pr_tree *tree);
const void *pr_tree_min(const pr_tree *tree);
const void *pr_tree_max(const pr_tree *tree);

struct pr_itor;
typedef struct pr_itor pr_itor;

pr_itor *pr_itor_new(pr_tree *tree);
dict_itor *pr_dict_itor_new(pr_tree *tree);
void pr_itor_destroy(pr_itor *tree);

int pr_itor_valid(const pr_itor *itor);
void pr_itor_invalidate(pr_itor *itor);
int pr_itor_next(pr_itor *itor);
int pr_itor_prev(pr_itor *itor);
int pr_itor_nextn(pr_itor *itor, unsigned count);
int pr_itor_prevn(pr_itor *itor, unsigned count);
int pr_itor_first(pr_itor *itor);
int pr_itor_last(pr_itor *itor);
int pr_itor_search(pr_itor *itor, const void *key);
const void *pr_itor_key(const pr_itor *itor);
void *pr_itor_data(pr_itor *itor);
const void *pr_itor_cdata(const pr_itor *itor);
int pr_itor_set_data(pr_itor *itor, void *dat, int del);
int pr_itor_remove(pr_itor *itor, int del);

END_DECL

#endif /* !_PR_TREE_H_ */
