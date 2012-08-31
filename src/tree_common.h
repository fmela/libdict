/*
 * Copyright (C) 2012 Farooq Mela.
 * All rights reserved.
 *
 * Common definitions for binary search trees.
 */

#ifndef _TREE_COMMON_H_
#define _TREE_COMMON_H_

#include "dict.h"

#define TREE_NODE_FIELDS(node_type) \
    void*		key; \
    void*		datum; \
    node_type*		parent; \
    node_type*		llink; \
    node_type*		rlink; \

#define TREE_FIELDS(node_type) \
    node_type*		root; \
    size_t		count; \
    dict_compare_func	cmp_func; \
    dict_delete_func	del_func; \

void	    tree_node_rot_left(void *tree, void *node);
void	    tree_node_rot_right(void *tree, void *node);
void*	    tree_node_prev(void *node);
void*	    tree_node_next(void *node);
void*	    tree_node_min(void *node);
void*	    tree_node_max(void *node);
void*	    tree_search(void *tree, const void *key);
const void* tree_min(const void *tree);
const void* tree_max(const void *tree);
size_t	    tree_traverse(void *tree, dict_visit_func visit);
size_t	    tree_clear(void *tree);
void	    tree_node_height(void *tree, size_t *minheight, size_t *maxheight);
void	    tree_height(void *tree, size_t *minheight, size_t *maxheight);

#endif /* !defined(_TREE_COMMON_H_) */
