// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-hash.h"

void dsp_hash_tab_init(struct dsp_hash_tab *tab)
{
	int idx;

	for (idx = 0; idx < DSP_HASH_MAX; idx++)
		dsp_list_head_init(&tab->list[idx]);
}

unsigned int dsp_hash_get_key(const char *data)
{
	int len;
	unsigned int hash, tmp;
	int rem;


	if (data == NULL)
		return 0;

	len = strlen(data);
	if (len <= 0)
		return 0;

	hash = len;

	rem = len & 3;
	len >>= 2;

	for (; len > 0; len--) {
		hash += get16bits(data);
		tmp = (get16bits(data + 2) << 11) ^ hash;
		hash = (hash << 16) ^ tmp;
		data += 2 * sizeof(unsigned short);
		hash += hash >> 11;
	}

	switch (rem) {
	case 3:
		hash += get16bits(data);
		hash ^= hash << 16;
		hash ^= ((signed char)data[sizeof(unsigned short)]) << 18;
		hash += hash >> 11;
		break;
	case 2:
		hash += get16bits(data);
		hash ^= hash << 11;
		hash += hash >> 17;
		break;
	case 1:
		hash += (signed char)(*data);
		hash ^= hash << 10;
		hash += hash >> 1;
	}

	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;

	return hash % DSP_HASH_MAX;
}

void dsp_hash_push(struct dsp_hash_tab *tab, const char *k, void *value)
{
	unsigned int key = dsp_hash_get_key(k);
	struct dsp_list_head *hash_head = &tab->list[key];
	struct dsp_hash_node *hash_node =
		(struct dsp_hash_node *)dsp_dl_malloc(sizeof(*hash_node),
			"Hash node");

	hash_node->key = key;
	hash_node->str = (char *)dsp_dl_malloc(strlen(k) + 1, "Hash node str");
	strcpy(hash_node->str, k);

	hash_node->value = value;
	dsp_list_node_init(&hash_node->node);
	dsp_list_node_push_back(hash_head, &hash_node->node);
}

int dsp_hash_get(struct dsp_hash_tab *tab, const char *k, void **value)
{
	unsigned int key = dsp_hash_get_key(k);
	struct dsp_list_node *node;

	*value = NULL;
	dsp_list_for_each(node, &tab->list[key]) {
		struct dsp_hash_node *hash_node =
			container_of(node, struct dsp_hash_node, node);

		if (hash_node->key == key && strcmp(hash_node->str, k) == 0) {
			*value = hash_node->value;
			return 0;
		}
	}
	return -1;
}

void dsp_hash_pop(struct dsp_hash_tab *tab, const char *k, int free_data)
{
	unsigned int key = dsp_hash_get_key(k);
	struct dsp_list_node *node = tab->list[key].next;

	while (node != NULL) {
		struct dsp_hash_node *hash_node =
			container_of(node, struct dsp_hash_node, node);

		node = node->next;

		if (hash_node->key == key && strcmp(hash_node->str, k) == 0) {
			dsp_list_node_remove(&tab->list[key], &hash_node->node);

			if (free_data)
				dsp_dl_free(hash_node->value);

			dsp_dl_free(hash_node->str);
			dsp_dl_free(hash_node);
		}
	}
}

void dsp_hash_free(struct dsp_hash_tab *tab, int free_data)
{
	int idx;

	DL_DEBUG("Hash free(%d)\n", free_data);

	for (idx = 0; idx < DSP_HASH_MAX; idx++) {
		struct dsp_list_node *cur, *next;

		cur = (&tab->list[idx])->next;

		while (cur != NULL) {
			struct dsp_hash_node *hash_node =
				container_of(cur, struct dsp_hash_node, node);

			next = cur->next;

			if (free_data)
				dsp_dl_free(hash_node->value);

			dsp_dl_free(hash_node->str);
			dsp_dl_free(hash_node);
			cur = next;
		}
	}
}
