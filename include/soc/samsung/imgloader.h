// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __EXYNOS_IMGLOADER_H
#define __EXYNOS_IMGLOADER_H

struct device;
struct module;
struct imgloader_priv;

/**
 * struct imgloader_desc - imgloader descriptor
 * @name: string used for imgloader_get()
 * @fw_name: firmware name
 * @dev: parent device
 * @ops: callback functions
 * @owner: module the descriptor belongs to
 * @flags: bitfield for image flags
 * @priv: DON'T USE - internal only
 * @data: driver's priv data only
 * @shutdown_fail: Set if imgloader op for shutting down subsystem fails.
 * @notify_signal: Set if notify signal is needed
 * @verify_fw: Set if supporting verify firmware
 * @need_blk_pwron: Set if block power on is needed
 * @reqeust_firmware_skip; Skip request_firmware on is needed
 */
struct imgloader_desc {
	const char *name;
	const char *fw_name;
	struct device *dev;
	const struct imgloader_ops *ops;
	struct module *owner;
	unsigned long flags;
	struct imgloader_priv *priv;
	void *data;
	unsigned int fw_id;
	bool shutdown_fail;
	bool notify_signal;
	bool s2mpu_support;
	bool skip_request_firmware;
};

/**
 * struct imgloader_reset_ops - imgloader operations
 */
struct imgloader_ops {
	int (*mem_setup)(struct imgloader_desc *imgloader,
			 const u8 *metadata, size_t size,
			 phys_addr_t *fw_phys_base,
			 size_t *fw_bin_size,
			 size_t *fw_mem_size);
	int (*verify_fw)(struct imgloader_desc *imgloader,
			 phys_addr_t fw_phys_base,
			 size_t fw_bin_size,
			 size_t fw_mem_size);
	int (*blk_pwron)(struct imgloader_desc *imgloader);
	int (*init_image)(struct imgloader_desc *imgloader);
	int (*deinit_image)(struct imgloader_desc *imgloader);
	int (*shutdown)(struct imgloader_desc *imgloader);
};

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
extern int imgloader_desc_init(struct imgloader_desc *desc);
extern int imgloader_boot(struct imgloader_desc *desc);
extern void imgloader_shutdown(struct imgloader_desc *desc);
extern void imgloader_desc_release(struct imgloader_desc *desc);
extern phys_addr_t imgloader_get_phys_base(struct imgloader_desc *desc);
#else
static inline int imgloader_desc_init(struct imgloader_desc *desc) { return 0; }
static inline int imgloader_boot(struct imgloader_desc *desc) { return 0; }
static inline void imgloader_shutdown(struct imgloader_desc *desc) { }
static inline void imgloader_desc_release(struct imgloader_desc *desc) { }
extern phys_addr_t imgloader_get_phys_base(struct imgloader_desc *desc) { return 0; }
#endif
#endif
