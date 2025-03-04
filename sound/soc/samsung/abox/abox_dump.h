/* sound/soc/samsung/abox/abox_dump.h
 *
 * ALSA SoC Audio Layer - Samsung Abox Internal Buffer Dumping driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_DUMP_H
#define __SND_SOC_ABOX_DUMP_H

#include <linux/device.h>
#include <sound/samsung/abox.h>

/**
 * Report dump data written
 * @param[in]	id		unique buffer id
 * @param[in]	pointer		byte index of the written data
 */
extern void abox_dump_period_elapsed(int id, size_t pointer);

/**
 * Transfer dump data
 * @param[in]	id		unique buffer id
 * @param[in]	buf		start of the trasferring buffer
 * @param[in]	bytes		number of bytes
 */
extern void abox_dump_transfer(int id, const char *buf, size_t bytes);

/**
 * Register abox file only
 * @param[in]	name		unique buffer name
 * @param[in]	data		private data
 * @param[in]	fops		file operation callbacks
 * @return	file entry pointer
 */
extern struct proc_dir_entry *abox_dump_register_file(const char *name,
		void *data, const struct file_operations *fops);

/**
 * Destroy registered dump file
 * @param[in]	file		pointer to file entry
 */
extern void abox_dump_unregister_file(struct proc_dir_entry *file);

/**
 * Get given data of the dump file
 * @param[in]	file		pointer to file entry
 * @return	data which is given at registration
 */
extern void *abox_dump_get_data(const struct file *file);

/**
 * Register abox dump
 * @param[in]	data		abox data
 * @param[in]	gid		unique buffer gid
 * @param[in]	id		unique buffer id
 * @param[in]	name		unique buffer name
 * @param[in]	area		virtual address of the buffer
 * @param[in]	addr		pysical address of the buffer
 * @param[in]	bytes		buffer size in bytes
 * @return	error code if any
 */
extern int abox_dump_register(struct abox_data *data, int gid, int id,
		const char *name, void *area, phys_addr_t addr, size_t bytes);

/**
 * Register abox dump (legacy version)
 * @param[in]	data		abox data
 * @param[in]	id		unique buffer id
 * @param[in]	name		unique buffer name
 * @param[in]	area		virtual address of the buffer
 * @param[in]	addr		pysical address of the buffer
 * @param[in]	bytes		buffer size in bytes
 * @return	error code if any
 */
static inline int abox_dump_register_legacy(struct abox_data *data, int id,
		const char *name, void *area, phys_addr_t addr, size_t bytes)
{
	return abox_dump_register(data, 0, id, name, area, addr, bytes);
}

/**
 * Initialize abox dump module
 * @param[in]	dev		pointer to abox device
 */
extern void abox_dump_init(struct device *dev_abox);
#endif /* __SND_SOC_ABOX_DUMP_H */
