/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/abox/vts_proc.h
 *
 * ALSA SoC Audio Layer - Samsung vts Proc FS driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __SND_SOC_VTS_PROC_H
#define __SND_SOC_VTS_PROC_H

#include <linux/proc_fs.h>

struct vts_proc_bin {
	void *data;
	size_t size;
};

/**
 * Get given data of the file
 * @param[in]	file	pointer to file entry
 * @return	data which is given at registration
 */
extern void *vts_proc_data(const struct file *file);

/**
 * Make a directory
 * @param[in]	name	name of the directory
 * @param[in]	parent	parent of the directory
 * @return	entry to the directory
 */
extern struct proc_dir_entry *vts_proc_mkdir(const char *name,
		struct proc_dir_entry *parent);

/**
 * Remove a file or directory
 * @param[in]	pde	entry to the directory which will be deleted
 */
extern void vts_proc_remove_file(struct proc_dir_entry *pde);

/**
 * Create a file
 * @param[in]	name	name of a file
 * @param[in]	mode	access mode
 * @param[in]	parent	parent of the file
 * @param[in]	fops	proc ops
 * @param[in]	data	private data
 * @param[in]	size	size of the file. set it 0 if unsure
 * @return	entry to the file
 */
extern struct proc_dir_entry *vts_proc_create_file(const char *name,
		umode_t mode, struct proc_dir_entry *parent,
		const struct file_operations *fops, void *data, size_t size);

/**
 * Initialize vts_proc
 */
extern int vts_proc_probe(void);

/**
 * Destroy vts_proc
 */
extern void vts_proc_remove(void);

#endif /* __SND_SOC_VTS_PROC_H */
