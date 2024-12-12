// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts_proc.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Proc FS driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/slab.h>

#include "vts.h"
#include "vts_proc.h"

#define ROOT_DIR_NAME "vts"

static struct proc_dir_entry *root;

/* for backward compatibility */
static struct dentry *debugfs_link;

void *vts_proc_data(const struct file *file)
{
	return PDE_DATA(file_inode(file));
}

struct proc_dir_entry *vts_proc_mkdir(const char *name,
		struct proc_dir_entry *parent)
{
	if (!parent)
		parent = root;

	return proc_mkdir(name, parent);
}

void vts_proc_remove_file(struct proc_dir_entry *pde)
{
	proc_remove(pde);
}

struct proc_dir_entry *vts_proc_create_file(const char *name, umode_t mode,
		struct proc_dir_entry *parent,
		const struct file_operations *fops, void *data, size_t size)
{
	struct proc_dir_entry *pde;

	if (!parent)
		parent = root;
	pde = proc_create_data(name, mode, parent, fops, data);
	if (!IS_ERR(pde))
		proc_set_size(pde, size);

	return pde;
}

int vts_proc_probe(void)
{
	root = proc_mkdir(ROOT_DIR_NAME, NULL);

	/* for backward compatibility */
	debugfs_link = debugfs_create_symlink(ROOT_DIR_NAME, NULL,
			"/proc/"ROOT_DIR_NAME);

	return PTR_ERR_OR_ZERO(root);
}

void vts_proc_remove(void)
{
	/* for backward compatibility */
	debugfs_remove(debugfs_link);

	proc_remove(root);
}
