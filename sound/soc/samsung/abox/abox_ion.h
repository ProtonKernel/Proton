/* sound/soc/samsung/abox/abox_ion.h
 *
 * ALSA SoC - Samsung Abox ION buffer module
 *
 * Copyright (c) 2018 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_ION_H
#define __SND_SOC_ABOX_ION_H

#include <sound/hwdep.h>
#include <sound/sounddev_abox.h>

struct abox_ion_buf {
	struct device *dev;
	void *kva;
	dma_addr_t iova;
	size_t size;
	struct sg_table *sgt;

	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	enum dma_data_direction direction;
	int fd;

	void *priv;
};

/**
 * get information about given ion memory
 * @param[in]	dev	pointer to calling device
 * @param[in]	buf	information of allocated memory
 * @param[in]	mmap_fd	information about given ion memory
 * @return		0 or error code
 */
extern int abox_ion_get_mmap_fd(struct device *dev,
		struct abox_ion_buf *buf,
		struct snd_pcm_mmap_fd *mmap_fd);

/**
 * add hardware dependent layer for the ion buffer
 * @param[in]	runtime	ASoC PCM runtime
 * @param[in]	buf	ion buffer
 * @param[out]	hwdep	pointer to new snd_hwdep
 * @return		0 or error code
 */
extern int abox_ion_new_hwdep(struct snd_soc_pcm_runtime *runtime,
		struct abox_ion_buf *buf, struct snd_hwdep **hwdep);

/**
 * allocate ion memory and map to given io virtual address
 * @param[in]	dev		pointer to calling device
 * @param[in]	data		abox data
 * @param[in]	iova		io virtual address
 * @param[in]	size		size of the requesting memory
 * @param[in]	playback	true if playback, otherwise false
 * @return		0 or error code
 */
extern struct abox_ion_buf *abox_ion_alloc(struct device *dev,
		struct abox_data *data,
		unsigned long iova,
		size_t size,
		bool playback);

/**
 * free the given ion memory
 * @param[in]	dev	pointer to calling device
 * @param[in]	data	abox data
 * @param[in]	buf	information of allocated memory
 * @return		0 or error code
 */
extern int abox_ion_free(struct device *dev,
		struct abox_data *data,
		struct abox_ion_buf *buf);
#endif /*__SND_SOC_ABOX_ION_H */
