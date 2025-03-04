/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_LIST_H__
#define __DL_DSP_LIST_H__

#include "dl/dsp-common.h"

#define dsp_list_for_each(node, head)					\
	for (node = (head)->next; node != NULL; node = node->next)

#define dsp_list_free(head, type, member)				\
	do {								\
		struct dsp_list_node *cur, *next;			\
		\
		cur = (head)->next;					\
		while (cur != NULL) {					\
			next = cur->next;				\
			dsp_dl_free(container_of(cur, type, member));	\
			cur = next;					\
		}							\
	} while (0)

struct dsp_list_node {
	struct dsp_list_node *next;
	struct dsp_list_node *prev;
};

typedef int (*dsp_list_compare_func)(
	struct dsp_list_node *a,
	struct dsp_list_node *b);

struct dsp_list_head {
	int num;
	struct dsp_list_node *next;
	struct dsp_list_node *prev;
};

void dsp_list_head_init(struct dsp_list_head *head);
void dsp_list_node_init(struct dsp_list_node *node);

void dsp_list_node_push_back(struct dsp_list_head *head,
	struct dsp_list_node *node);
void dsp_list_node_insert_back(struct dsp_list_head *head,
	struct dsp_list_node *loc,
	struct dsp_list_node *node);

void dsp_list_node_remove(struct dsp_list_head *head,
	struct dsp_list_node *node);

void dsp_list_node_swap(struct dsp_list_head *head,
	struct dsp_list_node *node1,
	struct dsp_list_node *node2);

void dsp_list_merge(struct dsp_list_head *head,
	struct dsp_list_head *head1,
	struct dsp_list_head *head2);

int dsp_list_is_empty(struct dsp_list_head *head);

void dsp_list_sort(struct dsp_list_head *head,
	dsp_list_compare_func func);
void dsp_list_unique(struct dsp_list_head *head,
	struct dsp_list_head *remove,
	dsp_list_compare_func func);

#endif
