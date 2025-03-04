/* sound/soc/samsung/abox/abox_shm.h
 *
 * ALSA SoC Audio Layer - Samsung Abox Shared Memory module
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_SHM_H
#define __SND_SOC_ABOX_SHM_H

#include <linux/types.h>

#define ABOX_SHM_VDMA_BASE 0x0
#define ABOX_SHM_VDMA_SIZE 0x10

#define ABOX_SHM_VDMA_APPL_PTR(i) \
	(ABOX_SHM_VDMA_BASE + (ABOX_SHM_VDMA_SIZE * (i)) + 0x0)
#define ABOX_SHM_VDMA_HW_PTR(i) \
	(ABOX_SHM_VDMA_BASE + (ABOX_SHM_VDMA_SIZE * (i)) + 0x4)
#define ABOX_SHM_VDMA_RESERVED0(i) \
	(ABOX_SHM_VDMA_BASE + (ABOX_SHM_VDMA_SIZE * (i)) + 0x8)
#define ABOX_SHM_VDMA_RESERVED1(i) \
	(ABOX_SHM_VDMA_BASE + (ABOX_SHM_VDMA_SIZE * (i)) + 0xc)

extern void abox_shm_init(void *base, size_t size);

extern u8 abox_shm_readb(unsigned int offset);

extern u16 abox_shm_readw(unsigned int offset);

extern u32 abox_shm_readl(unsigned int offset);

extern u64 abox_shm_readq(unsigned int offset);

extern void abox_shm_writeb(u8 value, unsigned int offset);

extern void abox_shm_writew(u16 value, unsigned int offset);

extern void abox_shm_writel(u32 value, unsigned int offset);

extern void abox_shm_writeq(u64 value, unsigned int offset);

static inline u32 abox_shm_read_vdma_hw_ptr(int id)
{
	return abox_shm_readl(ABOX_SHM_VDMA_HW_PTR(id));
}

static inline void abox_shm_write_vdma_appl_ptr(int id, u32 ptr)
{
	abox_shm_writel(ptr, ABOX_SHM_VDMA_APPL_PTR(id));
}

#endif /* __SND_SOC_ABOX_SHM_H */
