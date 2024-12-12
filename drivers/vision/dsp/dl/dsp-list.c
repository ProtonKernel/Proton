// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-list.h"

void dsp_list_head_init(struct dsp_list_head *head)
{
	head->num = 0;
	head->next = NULL;
	head->prev = NULL;
}

void dsp_list_node_init(struct dsp_list_node *node)
{
	node->next = NULL;
	node->prev = NULL;
}

void dsp_list_node_push_back(struct dsp_list_head *head,
	struct dsp_list_node *node)
{
	head->num++;

	if (head->prev == NULL)
		head->next = node;
	else {
		head->prev->next = node;
		node->prev = head->prev;
	}

	head->prev = node;
}

void dsp_list_node_insert_back(struct dsp_list_head *head,
	struct dsp_list_node *loc, struct dsp_list_node *node)
{
	head->num++;

	if (loc->next == NULL)
		head->prev = node;
	else
		loc->next->prev = node;

	node->prev = loc;
	node->next = loc->next;
	loc->next = node;
}

void dsp_list_node_remove(struct dsp_list_head *head,
	struct dsp_list_node *node)
{
	head->num--;

	if (node->prev == NULL)
		head->next = node->next;
	else
		node->prev->next = node->next;

	if (node->next == NULL)
		head->prev = node->prev;
	else
		node->next->prev = node->prev;
}

void dsp_list_node_swap(struct dsp_list_head *head,
	struct dsp_list_node *node1,
	struct dsp_list_node *node2)
{
	struct dsp_list_node *swap_tmp;

	if (node1->next == node2) {
		swap_tmp = node1->prev;
		node1->prev = node2;
		node2->prev = swap_tmp;

		swap_tmp = node2->next;
		node2->next = node1;
		node1->next = swap_tmp;
	} else if (node2->next == node1) {
		swap_tmp = node1->next;
		node1->next = node2;
		node2->next = swap_tmp;

		swap_tmp = node2->prev;
		node2->prev = node1;
		node1->prev = swap_tmp;
	} else {
		swap_tmp = node1->next;
		node1->next = node2->next;
		node2->next = swap_tmp;

		swap_tmp = node1->prev;
		node1->prev = node2->prev;
		node2->prev = swap_tmp;
	}

	if (!node1->prev)
		head->next = node1;
	else
		node1->prev->next = node1;

	if (!node1->next)
		head->prev = node1;
	else
		node1->next->prev = node1;

	if (!node2->prev)
		head->next = node2;
	else
		node2->prev->next = node2;

	if (!node2->next)
		head->prev = node2;
	else
		node2->next->prev = node2;
}

void dsp_list_merge(struct dsp_list_head *head,
	struct dsp_list_head *head1,
	struct dsp_list_head *head2)
{
	head1->prev->next = head2->next;
	head2->next->prev = head1->prev;
	head->prev = head2->prev;
	head->next = head1->next;
	head->num = head1->num + head2->num;
}

int dsp_list_is_empty(struct dsp_list_head *head)
{
	if (head->next == NULL && head->prev == NULL)
		return 1;
	else
		return 0;
}

void dsp_list_sort(struct dsp_list_head *head,
	dsp_list_compare_func func)
{
	int swap_ck = 1;
	struct dsp_list_node *node;

	while (swap_ck) {
		swap_ck = 0;
		for (node = head->next; node != NULL && node->next != NULL;) {
			if (func(node, node->next) < 0) {
				dsp_list_node_swap(head, node, node->next);
				swap_ck = 1;
			} else
				node = node->next;
		}
	}
}

void dsp_list_unique(struct dsp_list_head *head,
	struct dsp_list_head *remove,
	dsp_list_compare_func func)
{
	struct dsp_list_node *node, *next;

	dsp_list_for_each(node, head) {
		dsp_list_for_each(next, node) {
			struct dsp_list_node *prev = next->prev;

			if (func(node, next) == 0) {
				dsp_list_node_remove(head, next);
				dsp_list_node_init(next);
				dsp_list_node_push_back(remove, next);
				next = prev;
			}
		}
	}
}
