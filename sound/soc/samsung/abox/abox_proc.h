/* sound/soc/samsung/abox/abox_proc.h
 *
 * ALSA SoC Audio Layer - Samsung Abox Proc FS driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __SND_SOC_ABOX_PROC_H
#define __SND_SOC_ABOX_PROC_H

#include <linux/proc_fs.h>

struct abox_proc_bin {
	void *data;
	size_t size;
};

/**
 * Get given data of the file
 * @param[in]	file	pointer to file entry
 * @return	data which is given at registration
 */
extern void *abox_proc_data(const struct file *file);

/**
 * Make a directory
 * @param[in]	name	name of the directory
 * @param[in]	parent	parent of the directory
 * @return	entry to the directory
 */
extern struct proc_dir_entry *abox_proc_mkdir(const char *name,
		struct proc_dir_entry *parent);

/**
 * Remove a file or directory
 * @param[in]	pde	entry to the directory which will be deleted
 */
extern void abox_proc_remove_file(struct proc_dir_entry *pde);

/**
 * Create a file
 * @param[in]	name	name of a file
 * @param[in]	mode	access mode
 * @param[in]	parent	parent of the file
 * @param[in]	fops	file operations
 * @param[in]	data	private data
 * @param[in]	size	size of the file. set it 0 if unsure
 * @return	entry to the file
 */
extern struct proc_dir_entry *abox_proc_create_file(const char *name,
		umode_t mode, struct proc_dir_entry *parent,
		const struct file_operations *fops, void *data, size_t size);

/**
 * Create a binary file
 * @param[in]	name	name of a file
 * @param[in]	mode	access mode
 * @param[in]	parent	parent of the file
 * @param[in]	bin	definition of the binary file
 * @return	entry to the file
 */
extern struct proc_dir_entry *abox_proc_create_bin(const char *name,
		umode_t mode, struct proc_dir_entry *parent,
		struct abox_proc_bin *bin);

/**
 * Create a unsigned 64bit attribute file
 * @param[in]	name	name of a file
 * @param[in]	mode	access mode
 * @param[in]	parent	parent of the file
 * @param[in]	data	pointer to the unsigned 64bit value
 * @return	entry to the file
 */
extern struct proc_dir_entry *abox_proc_create_u64(const char *name,
		umode_t mode, struct proc_dir_entry *parent, u64 *data);

/**
 * Create a unsigned 32bit attribute file
 * @param[in]	name	name of a file
 * @param[in]	mode	access mode
 * @param[in]	parent	parent of the file
 * @param[in]	data	pointer to the unsigned 64bit value
 * @return	entry to the file
 */
extern struct proc_dir_entry *abox_proc_create_u32(const char *name,
		umode_t mode, struct proc_dir_entry *parent, u32 *data);

/**
 * Create a boolean attribute file
 * @param[in]	name	name of a file
 * @param[in]	mode	access mode
 * @param[in]	parent	parent of the file
 * @param[in]	data	pointer to the boolean value
 * @return	entry to the file
 */
extern struct proc_dir_entry *abox_proc_create_bool(const char *name,
		umode_t mode, struct proc_dir_entry *parent, bool *data);

/**
 * Create a symbolic link to a device attribute
 * @param[in]	dev	pointer to the device
 * @param[in]	name	name of the attribute
 * @param[in]	parent	parent of the file
 * @return	error code or 0
 */
extern int abox_proc_symlink_attr(struct device *dev, const char *name,
		struct proc_dir_entry *parent);

/**
 * Create a symbolic link to a sysfs
 * @param[in]	dev	pointer to the device
 * @param[in]	name	name of the attribute
 * @return	error code or 0
 */
extern int abox_proc_symlink_dev(struct device *dev, const char *name);

/**
 * Initialize abox_proc
 */
extern int abox_proc_probe(void);

/**
 * Destroy abox_proc
 */
extern void abox_proc_remove(void);

#endif /* __SND_SOC_ABOX_PROC_H */
