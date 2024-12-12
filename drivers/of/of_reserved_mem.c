// SPDX-License-Identifier: GPL-2.0+
/*
 * Device tree based initialization code for reserved memory.
 *
 * Copyright (c) 2013, 2015 The Linux Foundation. All Rights Reserved.
 * Copyright (c) 2013,2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 * Author: Marek Szyprowski <m.szyprowski@samsung.com>
 * Author: Josh Cartwright <joshc@codeaurora.org>
 */

#define pr_fmt(fmt)	"OF: reserved mem: " fmt

#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/mm.h>
#include <linux/sizes.h>
#include <linux/of_reserved_mem.h>
#include <linux/sort.h>
#include <linux/slab.h>
#include <linux/memblock.h>

#define MAX_RESERVED_REGIONS	64
static struct reserved_mem reserved_mem[MAX_RESERVED_REGIONS];
static int reserved_mem_count;

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX_CHANGE_RMEM_SIZE)
#define MAGIC_ABOX_RMEM_SIZE	0xab0cab0c
#define DEBUG_LEVEL_LOW		0x4f4c
#define FORCE_UPLOAD_DISABLED	0x0

enum cmdline_check_mode {
	DEBUG_LEVEL,
	FORCE_UPLOAD,
	CHK_MODE_MAX,
};

static const char *cmdline_mode_str[CHK_MODE_MAX] = {
	[DEBUG_LEVEL] = "androidboot.debug_level=",
	[FORCE_UPLOAD] = "androidboot.force_upload=",
};

int debug_level;
int force_upload;

static int check_cmdline(enum cmdline_check_mode mode, int *val)
{
	int len, i;
	const char *bootargs;
	char *p_val;
	char buff_val[16];
	int ret = 0;
	unsigned long root, chosen;

	root = of_get_flat_dt_root();
	chosen = of_get_flat_dt_subnode_by_name(root, "chosen");
	if (!chosen) {
		pr_err("%s: no chosen\n", __func__);
		return -EINVAL;
	}

	bootargs = of_get_flat_dt_prop(chosen, "bootargs", &len);
	if (!bootargs) {
		pr_err("%s: no bootargs\n", __func__);
		return -EINVAL;
	}

	p_val = strstr(bootargs, cmdline_mode_str[mode]);
	if (!p_val) {
		pr_err("%s: invalid cmdline_mode %d\n", __func__, mode);
		return -EINVAL;
	}

	p_val += strlen(cmdline_mode_str[mode]);

	for (i = 0; i < (int) sizeof(buff_val) - 1; i++) {
		if (p_val[i] == ' ' || p_val[i] == 0) {
			buff_val[i] = 0;
			break;
		}
		buff_val[i] = p_val[i];
	}
	buff_val[i] = 0;

	ret = kstrtoint(buff_val, 0, val);
	if (ret < 0)
		pr_err("%s: str2int err (%s) %d\n", __func__, buff_val, ret);

	pr_debug("%s: %s = 0x%x\n", __func__, cmdline_mode_str[mode], *val);

	return ret;
}
#endif

static int __init early_init_dt_alloc_reserved_memory_arch(phys_addr_t size,
	phys_addr_t align, phys_addr_t start, phys_addr_t end, bool nomap,
	phys_addr_t *res_base)
{
	phys_addr_t base;

	end = !end ? MEMBLOCK_ALLOC_ACCESSIBLE : end;
	align = !align ? SMP_CACHE_BYTES : align;
	base = memblock_find_in_range(start, end, size, align);
	if (!base)
		return -ENOMEM;

	*res_base = base;
	if (nomap)
		return memblock_remove(base, size);

	return memblock_reserve(base, size);
}

/**
 * res_mem_save_node() - save fdt node for second pass initialization
 */
void __init fdt_reserved_mem_save_node(unsigned long node, const char *uname,
				      phys_addr_t base, phys_addr_t size)
{
	struct reserved_mem *rmem = &reserved_mem[reserved_mem_count];

	if (reserved_mem_count == ARRAY_SIZE(reserved_mem)) {
		pr_err("not enough space all defined regions.\n");
		return;
	}

	rmem->fdt_node = node;
	rmem->name = uname;
	rmem->base = base;
	rmem->size = size;

	reserved_mem_count++;
	return;
}

/**
 * res_mem_alloc_size() - allocate reserved memory described by 'size', 'align'
 *			  and 'alloc-ranges' properties
 */
static int __init __reserved_mem_alloc_size(unsigned long node,
	const char *uname, phys_addr_t *res_base, phys_addr_t *res_size)
{
	int t_len = (dt_root_addr_cells + dt_root_size_cells) * sizeof(__be32);
	phys_addr_t start = 0, end = 0;
	phys_addr_t base = 0, align = 0, size;
	int len;
	const __be32 *prop;
	int nomap;
	int ret;
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX_CHANGE_RMEM_SIZE)
	int change_size = 0;
	phys_addr_t size_min = MAGIC_ABOX_RMEM_SIZE;
#endif

	prop = of_get_flat_dt_prop(node, "size", &len);
	if (!prop)
		return -EINVAL;

	if (len != dt_root_size_cells * sizeof(__be32)) {
		pr_err("invalid size property in '%s' node.\n", uname);
		return -EINVAL;
	}
	size = dt_mem_next_cell(dt_root_size_cells, &prop);

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX_CHANGE_RMEM_SIZE)
	if (!strcmp(uname, "abox-dbg")) {
		check_cmdline(FORCE_UPLOAD, &force_upload);
		if (force_upload == FORCE_UPLOAD_DISABLED)
			change_size = 1;
	}

	if (!strcmp(uname, "abox-slog")) {
		check_cmdline(DEBUG_LEVEL, &debug_level);
		pr_err("%s: debug_level 0x%x\n", __func__, debug_level);
		if (debug_level == DEBUG_LEVEL_LOW)
			change_size = 1;
	}

	if (change_size) {
		pr_info("change size for %s\n", uname);
		prop = of_get_flat_dt_prop(node, "size_min", &len);
		if (!prop)
			pr_err("no size_min prop in '%s' node.\n", uname);
		else if (len != dt_root_size_cells * sizeof(__be32))
			pr_err("invalid size_min prop in '%s' node.\n", uname);
		else
			size_min = dt_mem_next_cell(dt_root_size_cells, &prop);

		pr_info("%s: %s size 0x%x, size_min 0x%x\n", __func__, uname, size, size_min);

		if (size_min != MAGIC_ABOX_RMEM_SIZE && (size > size_min))
			size = size_min;
	}
#endif

	nomap = of_get_flat_dt_prop(node, "no-map", NULL) != NULL;

	prop = of_get_flat_dt_prop(node, "alignment", &len);
	if (prop) {
		if (len != dt_root_addr_cells * sizeof(__be32)) {
			pr_err("invalid alignment property in '%s' node.\n",
				uname);
			return -EINVAL;
		}
		align = dt_mem_next_cell(dt_root_addr_cells, &prop);
	}

	/* Need adjust the alignment to satisfy the CMA requirement */
	if (IS_ENABLED(CONFIG_CMA)
	    && of_flat_dt_is_compatible(node, "shared-dma-pool")
	    && of_get_flat_dt_prop(node, "reusable", NULL)
	    && !of_get_flat_dt_prop(node, "no-map", NULL)) {
		unsigned long order =
			max_t(unsigned long, MAX_ORDER - 1, pageblock_order);

		align = max(align, (phys_addr_t)PAGE_SIZE << order);
	}

	prop = of_get_flat_dt_prop(node, "alloc-ranges", &len);
	if (prop) {

		if (len % t_len != 0) {
			pr_err("invalid alloc-ranges property in '%s', skipping node.\n",
			       uname);
			return -EINVAL;
		}

		base = 0;

		while (len > 0) {
			start = dt_mem_next_cell(dt_root_addr_cells, &prop);
			end = start + dt_mem_next_cell(dt_root_size_cells,
						       &prop);

			ret = early_init_dt_alloc_reserved_memory_arch(size,
					align, start, end, nomap, &base);
			if (ret == 0) {
				pr_debug("allocated memory for '%s' node: base %pa, size %lu MiB\n",
					uname, &base,
					(unsigned long)(size / SZ_1M));
				break;
			}
			len -= t_len;
		}

	} else {
		ret = early_init_dt_alloc_reserved_memory_arch(size, align,
							0, 0, nomap, &base);
		if (ret == 0)
			pr_debug("allocated memory for '%s' node: base %pa, size %lu MiB\n",
				uname, &base, (unsigned long)(size / SZ_1M));
	}

	if (base == 0) {
		pr_info("failed to allocate memory for node '%s'\n", uname);
		return -ENOMEM;
	}

	*res_base = base;
	*res_size = size;

	return 0;
}

static const struct of_device_id __rmem_of_table_sentinel
	__used __section(__reservedmem_of_table_end);

/**
 * res_mem_init_node() - call region specific reserved memory init code
 */
static int __init __reserved_mem_init_node(struct reserved_mem *rmem)
{
	extern const struct of_device_id __reservedmem_of_table[];
	const struct of_device_id *i;
	int ret = -ENOENT;

	for (i = __reservedmem_of_table; i < &__rmem_of_table_sentinel; i++) {
		reservedmem_of_init_fn initfn = i->data;
		const char *compat = i->compatible;

		if (!of_flat_dt_is_compatible(rmem->fdt_node, compat))
			continue;

		ret = initfn(rmem);
		if (ret == 0) {
			pr_info("initialized node %s, compatible id %s\n",
				rmem->name, compat);
			break;
		}
	}
	return ret;
}

static int __init __rmem_cmp(const void *a, const void *b)
{
	const struct reserved_mem *ra = a, *rb = b;

	if (ra->base < rb->base)
		return -1;

	if (ra->base > rb->base)
		return 1;

	/*
	 * Put the dynamic allocations (address == 0, size == 0) before static
	 * allocations at address 0x0 so that overlap detection works
	 * correctly.
	 */
	if (ra->size < rb->size)
		return -1;
	if (ra->size > rb->size)
		return 1;

	return 0;
}

static void __init __rmem_check_for_overlap(void)
{
	int i;

	if (reserved_mem_count < 2)
		return;

	sort(reserved_mem, reserved_mem_count, sizeof(reserved_mem[0]),
	     __rmem_cmp, NULL);
	for (i = 0; i < reserved_mem_count - 1; i++) {
		struct reserved_mem *this, *next;

		this = &reserved_mem[i];
		next = &reserved_mem[i + 1];

		if (this->base + this->size > next->base) {
			phys_addr_t this_end, next_end;

			this_end = this->base + this->size;
			next_end = next->base + next->size;
			pr_err("OVERLAP DETECTED!\n%s (%pa--%pa) overlaps with %s (%pa--%pa)\n",
			       this->name, &this->base, &this_end,
			       next->name, &next->base, &next_end);
		}
	}
}

/**
 * fdt_init_reserved_mem - allocate and init all saved reserved memory regions
 */
void __init fdt_init_reserved_mem(void)
{
	int i;

	/* check for overlapping reserved regions */
	__rmem_check_for_overlap();

	for (i = 0; i < reserved_mem_count; i++) {
		struct reserved_mem *rmem = &reserved_mem[i];
		unsigned long node = rmem->fdt_node;
		int len;
		const __be32 *prop;
		int err = 0;
		int nomap, reusable;

		nomap = of_get_flat_dt_prop(node, "no-map", NULL) != NULL;
		reusable = of_get_flat_dt_prop(node, "reusable", NULL) != NULL;
		prop = of_get_flat_dt_prop(node, "phandle", &len);
		if (!prop)
			prop = of_get_flat_dt_prop(node, "linux,phandle", &len);
		if (prop)
			rmem->phandle = of_read_number(prop, len/4);

		if (rmem->size == 0)
			err = __reserved_mem_alloc_size(node, rmem->name,
						 &rmem->base, &rmem->size);
		if (err == 0) {
			err = __reserved_mem_init_node(rmem);
			if (err != 0 && err != -ENOENT) {
				pr_info("node %s compatible matching fail\n",
					rmem->name);
				memblock_free(rmem->base, rmem->size);
				if (nomap)
					memblock_add(rmem->base, rmem->size);
			} else {
#ifdef CONFIG_ION_RBIN_HEAP
				if (of_get_flat_dt_prop(node, "ion,recyclable", NULL))
					reusable = true;
#endif
				memblock_memsize_record(rmem->name, rmem->base,
							rmem->size, nomap,
							reusable);
			}
		}
	}
}

static inline struct reserved_mem *__find_rmem(struct device_node *node)
{
	unsigned int i;

	if (!node->phandle)
		return NULL;

	for (i = 0; i < reserved_mem_count; i++)
		if (reserved_mem[i].phandle == node->phandle)
			return &reserved_mem[i];
	return NULL;
}

struct rmem_assigned_device {
	struct device *dev;
	struct reserved_mem *rmem;
	struct list_head list;
};

static LIST_HEAD(of_rmem_assigned_device_list);
static DEFINE_MUTEX(of_rmem_assigned_device_mutex);

/**
 * of_reserved_mem_device_init_by_idx() - assign reserved memory region to
 *					  given device
 * @dev:	Pointer to the device to configure
 * @np:		Pointer to the device_node with 'reserved-memory' property
 * @idx:	Index of selected region
 *
 * This function assigns respective DMA-mapping operations based on reserved
 * memory region specified by 'memory-region' property in @np node to the @dev
 * device. When driver needs to use more than one reserved memory region, it
 * should allocate child devices and initialize regions by name for each of
 * child device.
 *
 * Returns error code or zero on success.
 */
int of_reserved_mem_device_init_by_idx(struct device *dev,
				       struct device_node *np, int idx)
{
	struct rmem_assigned_device *rd;
	struct device_node *target;
	struct reserved_mem *rmem;
	int ret;

	if (!np || !dev)
		return -EINVAL;

	target = of_parse_phandle(np, "memory-region", idx);
	if (!target)
		return -ENODEV;

	if (!of_device_is_available(target)) {
		of_node_put(target);
		return 0;
	}

	rmem = __find_rmem(target);
	of_node_put(target);

	if (!rmem || !rmem->ops || !rmem->ops->device_init)
		return -EINVAL;

	rd = kmalloc(sizeof(struct rmem_assigned_device), GFP_KERNEL);
	if (!rd)
		return -ENOMEM;

	ret = rmem->ops->device_init(rmem, dev);
	if (ret == 0) {
		rd->dev = dev;
		rd->rmem = rmem;

		mutex_lock(&of_rmem_assigned_device_mutex);
		list_add(&rd->list, &of_rmem_assigned_device_list);
		mutex_unlock(&of_rmem_assigned_device_mutex);

		dev_info(dev, "assigned reserved memory node %s\n", rmem->name);
	} else {
		kfree(rd);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(of_reserved_mem_device_init_by_idx);

/**
 * of_reserved_mem_device_release() - release reserved memory device structures
 * @dev:	Pointer to the device to deconfigure
 *
 * This function releases structures allocated for memory region handling for
 * the given device.
 */
void of_reserved_mem_device_release(struct device *dev)
{
	struct rmem_assigned_device *rd;
	struct reserved_mem *rmem = NULL;

	mutex_lock(&of_rmem_assigned_device_mutex);
	list_for_each_entry(rd, &of_rmem_assigned_device_list, list) {
		if (rd->dev == dev) {
			rmem = rd->rmem;
			list_del(&rd->list);
			kfree(rd);
			break;
		}
	}
	mutex_unlock(&of_rmem_assigned_device_mutex);

	if (!rmem || !rmem->ops || !rmem->ops->device_release)
		return;

	rmem->ops->device_release(rmem, dev);
}
EXPORT_SYMBOL_GPL(of_reserved_mem_device_release);

/**
 * of_reserved_mem_lookup() - acquire reserved_mem from a device node
 * @np:		node pointer of the desired reserved-memory region
 *
 * This function allows drivers to acquire a reference to the reserved_mem
 * struct based on a device node handle.
 *
 * Returns a reserved_mem reference, or NULL on error.
 */
struct reserved_mem *of_reserved_mem_lookup(struct device_node *np)
{
	const char *name;
	int i;

	if (!np->full_name)
		return NULL;

	name = kbasename(np->full_name);
	for (i = 0; i < reserved_mem_count; i++)
		if (!strcmp(reserved_mem[i].name, name))
			return &reserved_mem[i];

	return NULL;
}
EXPORT_SYMBOL_GPL(of_reserved_mem_lookup);
