/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos is mem functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/dma-buf.h>
#include <linux/dma-buf-container.h>
#include <linux/ion.h>
#include <linux/kmemleak.h>
#include <asm/cacheflush.h>
#include <media/videobuf2-memops.h>
#include <media/videobuf2-dma-sg.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include <linux/iommu.h>

struct is_vfree_deferred {
	struct llist_head list;
	struct work_struct wq;
};
static struct is_vfree_deferred __percpu *is_vfree_deferred;

#if IS_ENABLED(CONFIG_VIDEOBUF2_DMA_SG)
#define IS_IOMMU_PROP	(IOMMU_READ | IOMMU_WRITE)

struct vb2_dma_sg_buf {
	struct device			*dev;
	void				*vaddr;
	struct page			**pages;
	struct frame_vector		*vec;
	int				offset;
	enum dma_data_direction		dma_dir;
	struct sg_table			sg_table;
	/*
	 * This will point to sg_table when used with the MMAP or USERPTR
	 * memory model, and to the dma_buf sglist when used with the
	 * DMABUF memory model.
	 */
	struct sg_table			*dma_sgt;
	size_t				size;
	unsigned int			num_pages;
	refcount_t			refcount;
	struct vb2_vmarea_handler	handler;

	struct dma_buf_attachment	*db_attach;
	/*
	 * Our IO address space is not managed by dma-mapping. Therefore
	 * scatterlist.dma_address should not be corrupted by the IO address
	 * returned by iovmm_map() because it is used by cache maintenance.
	 */
	dma_addr_t			iova;
};

/* IS memory statistics */
static struct is_mem_stats stats;
static atomic_t mem_stats_sz_active = ATOMIC_INIT(0);

static int param_set_mem_stats_active(const char *val, const struct kernel_param *kp)
{
	int res;

	if (!kstrtoint(val, 0, &res))
		atomic_set(&mem_stats_sz_active, res);

	return 0;
}

static int param_get_mem_stats_active(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%d\n", atomic_read(&mem_stats_sz_active));
}

static const struct kernel_param_ops param_ops_mem_stats_active = {
	.set = param_set_mem_stats_active,
	.get = param_get_mem_stats_active,
};

module_param_cb(mem_stats_active, &param_ops_mem_stats_active, NULL, 0644);

static void is_free_work(struct work_struct *w)
{
	struct is_vfree_deferred *p = container_of(w, struct is_vfree_deferred, wq);
	struct llist_node *t, *llnode;

	llist_for_each_safe(llnode, t, llist_del_all(&p->list))
		vfree((void *)llnode);
}

/* is vb2 buffer operations */
static inline ulong is_vb2_dma_sg_plane_kvaddr(
		struct is_vb2_buf *vbuf, u32 plane)

{
	return (ulong)vb2_plane_vaddr(&vbuf->vb.vb2_buf, plane);
}

static inline dma_addr_t is_vb2_dma_sg_plane_dvaddr(
		struct is_vb2_buf *vbuf, u32 plane)

{
	struct sg_table *sgt = vb2_dma_sg_plane_desc(&vbuf->vb.vb2_buf, plane);
	return sg_dma_address(sgt->sgl);
}

static inline ulong is_vb2_dma_sg_plane_kmap(
		struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf = vb->planes[plane].mem_priv;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbuf;
	u32 adj_plane = plane;

	if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER) && vbuf->num_merged_dbufs) {
		/* Is a non-image plane? */
		if (plane >= num_i_planes)
			adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
				+ (plane - num_i_planes);
		else
			adj_plane = (buf_idx * num_i_planes) + plane;
	}

	if (likely(vbuf->kva[adj_plane]))
		return vbuf->kva[adj_plane];

	/* Use a dma_buf extracted from the DMA buffer container or original one. */
	if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER)
		&& vbuf->num_merged_dbufs && (plane < num_i_planes))
		dbuf = vbuf->dbuf[adj_plane];
	else
		dbuf = dma_buf_get(vb->planes[plane].m.fd);

	if (IS_ERR_OR_NULL(dbuf)) {
		pr_err("failed to get dmabuf of fd: %d, plane: %d\n",
				vb->planes[plane].m.fd, plane);
		return 0;
	}

	if (dma_buf_begin_cpu_access(dbuf, buf->dma_dir)) {
		dma_buf_put(dbuf);
		pr_err("failed to access dmabuf of fd: %d, plane: %d\n",
				vb->planes[plane].m.fd, plane);
		return 0;
	}

	vbuf->kva[adj_plane] = (ulong)dma_buf_vmap(dbuf) + buf->offset;
	if (!vbuf->kva[adj_plane]) {
		dma_buf_end_cpu_access(dbuf, buf->dma_dir);
		dma_buf_put(dbuf);
		pr_err("failed to kmapping dmabuf of fd: %d, plane: %d\n",
				vb->planes[plane].m.fd, plane);
		return 0;
	}

	/* Save the dma_buf if there is no saved one. */
	if (!vbuf->dbuf[adj_plane])
		vbuf->dbuf[adj_plane] = dbuf;
	vbuf->kva[adj_plane] += buf->offset & ~PAGE_MASK;

	atomic_inc(&stats.cnt_plane_kmap);

	return vbuf->kva[adj_plane];
}

static inline void is_vb2_dma_sg_plane_kunmap(
		struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbuf;
	u32 adj_plane = plane;

	if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER) && vbuf->num_merged_dbufs) {
		if ((plane - num_i_planes) >= 0)
			adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
				+ (plane - num_i_planes);
		else
			adj_plane = (buf_idx * num_i_planes) + plane;
	}

	if (vbuf->kva[adj_plane]) {
		dbuf = vbuf->dbuf[adj_plane];

		dma_buf_vunmap(dbuf, (void *)vbuf->kva[adj_plane]);
		dma_buf_end_cpu_access(dbuf, 0);
		dma_buf_put(dbuf);

		vbuf->dbuf[adj_plane] = NULL;
		vbuf->kva[adj_plane] = 0;

		atomic_inc(&stats.cnt_plane_kunmap);
	}
}

static long is_vb2_dma_sg_remap_attr(struct is_vb2_buf *vbuf, int attr)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf;
	unsigned int plane;
	long ret;
	int ioprot = IS_IOMMU_PROP;

	for (plane = 0; plane < vb->num_planes; ++plane) {
		buf = vb->planes[plane].mem_priv;
		vbuf->sgt[plane] = dma_buf_map_attachment(buf->db_attach, buf->dma_dir);

		if (IS_ERR(vbuf->sgt[plane])) {
			ret = PTR_ERR(vbuf->sgt[plane]);
			pr_err("Error getting dmabuf scatterlist\n");
			goto err_get_sgt;
		}

		if ((vbuf->dva[plane] == 0) || IS_ERR_VALUE(vbuf->dva[plane])) {
			if (device_get_dma_attr(buf->dev) == DEV_DMA_COHERENT)
				ioprot |= IOMMU_CACHE;

			vbuf->dva[plane] = sg_dma_address(vbuf->sgt[plane]->sgl);
			if (IS_ERR_VALUE(vbuf->dva[plane])) {
				ret = vbuf->dva[plane];
				pr_err("Error from sg_dma_address(): %ld\n", ret);
				goto err_map_remap;
			}
		}
	}

	atomic_inc(&stats.cnt_buf_remap);

	return 0;

err_map_remap:
	dma_buf_unmap_attachment(buf->db_attach, vbuf->sgt[plane], buf->dma_dir);
	vbuf->dva[plane] = 0;

err_get_sgt:
	while (plane-- > 0) {
		buf = vb->planes[plane].mem_priv;

		dma_buf_unmap_attachment(buf->db_attach, vbuf->sgt[plane], buf->dma_dir);
		vbuf->dva[plane] = 0;
	}

	return ret;
}

void is_vb2_dma_sg_unremap_attr(struct is_vb2_buf *vbuf, int attr)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf;
	unsigned int plane;

	for (plane = 0; plane < vb->num_planes; ++plane) {
		buf = vb->planes[plane].mem_priv;

		dma_buf_unmap_attachment(buf->db_attach, vbuf->sgt[plane], buf->dma_dir);
		vbuf->dva[plane] = 0;
	}

	atomic_inc(&stats.cnt_buf_unremap);
}

#if IS_ENABLED(CONFIG_DMA_BUF_CONTAINER)
static long is_dbufcon_prepare(struct is_vb2_buf *vbuf, struct device *dev)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbufcon;
	int count;
	int p, b, i;
	long ret;

	for (p = 0; p < num_i_planes; p++) {
		dbufcon = dma_buf_get(vb->planes[p].m.fd);
		if (IS_ERR_OR_NULL(dbufcon)) {
			err("failed to get dmabuf of fd: %d, plane: %d",
				vb->planes[p].m.fd, p);
			ret = -EINVAL;
			goto err_get_dbufcon;
		}

		count = dmabuf_container_get_count(dbufcon);
		if (count < 0) {
			ret = 0;
			goto err_get_count;
		} else if (count == 0) {
			err("empty dmabuf-container of fd: %d", vb->planes[p].m.fd);
			ret = -EINVAL;
			goto err_empty_dbufcon;
		}

		/* IS_MAX_PLANES includes meta-planes */
		if (((count * num_i_planes) + NUM_OF_META_PLANE)
			> IS_MAX_PLANES) {
			err("out of range plane. require: %d > supported: %d",
				(count * num_i_planes) + NUM_OF_META_PLANE,
				IS_MAX_PLANES);
			ret = -EINVAL;
			goto err_too_many_planes;

		}

		/* change the order of each plane by the order of buffer */
		for (b = 0, i = p; b < count; b++, i = (b * num_i_planes) + p) {
			vbuf->dbuf[i] = dmabuf_container_get_buffer(dbufcon, b);
			if (IS_ERR_OR_NULL(vbuf->dbuf[i])) {
				err("failed to get dmabuf-container's dmabuf: %d", b);
				ret = -EINVAL;
				goto err_get_dbuf;
			}

			vbuf->atch[i] = dma_buf_attach(vbuf->dbuf[i], dev);
			if (IS_ERR(vbuf->atch[i])) {
				err("failed to attach dmabuf: %d", b);
				ret = PTR_ERR(vbuf->atch[i]);
				goto err_attach_dbuf;
			}

			vbuf->sgt[i] = dma_buf_map_attachment(vbuf->atch[i],
							DMA_BIDIRECTIONAL);
			if (IS_ERR(vbuf->sgt[i])) {
				err("failed to get dmabuf's sgt: %d", b);
				ret = PTR_ERR(vbuf->sgt[i]);
				goto err_get_sgt;
			}
		}

		dma_buf_put(dbufcon);
	}

	vbuf->num_merged_dbufs = count;

	atomic_inc(&stats.cnt_dbuf_prepare);

	return 0;

err_get_sgt:
	dma_buf_detach(vbuf->dbuf[i], vbuf->atch[i]);
	vbuf->sgt[i] = NULL;

err_attach_dbuf:
	dma_buf_put(vbuf->dbuf[i]);
	vbuf->atch[i] = NULL;
	vbuf->dbuf[i] = NULL;

err_get_dbuf:
	while (b-- > 0) {
		i = (b * num_i_planes) + p;
		dma_buf_unmap_attachment(vbuf->atch[i], vbuf->sgt[i],
				DMA_BIDIRECTIONAL);
		dma_buf_detach(vbuf->dbuf[i], vbuf->atch[i]);
		dma_buf_put(vbuf->dbuf[i]);
		vbuf->sgt[i] = NULL;
		vbuf->atch[i] = NULL;
		vbuf->dbuf[i] = NULL;
	}

err_too_many_planes:
err_empty_dbufcon:
err_get_count:
	dma_buf_put(dbufcon);

err_get_dbufcon:
	vbuf->num_merged_dbufs = 0;

	while (p-- > 0) {
		dbufcon = dma_buf_get(vb->planes[p].m.fd);
		count = dmabuf_container_get_count(dbufcon);
		dma_buf_put(dbufcon);

		for (b = 0, i = p; b < count; b++, i = (b * num_i_planes) + p) {
			dma_buf_unmap_attachment(vbuf->atch[i], vbuf->sgt[i],
					DMA_BIDIRECTIONAL);
			dma_buf_detach(vbuf->dbuf[i], vbuf->atch[i]);
			dma_buf_put(vbuf->dbuf[i]);
			vbuf->sgt[i] = NULL;
			vbuf->atch[i] = NULL;
			vbuf->dbuf[i] = NULL;
		}
	}

	return ret;
}

static void is_dbufcon_finish(struct is_vb2_buf *vbuf)
{
	int i;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vbuf->num_merged_dbufs
		* (vb->num_planes - NUM_OF_META_PLANE);

	for (i = 0; i < num_i_planes; i++) {
		dma_buf_unmap_attachment(vbuf->atch[i], vbuf->sgt[i],
				DMA_BIDIRECTIONAL);
		dma_buf_detach(vbuf->dbuf[i], vbuf->atch[i]);
		dma_buf_put(vbuf->dbuf[i]);
		vbuf->sgt[i] = NULL;
		vbuf->atch[i] = NULL;
		vbuf->dbuf[i] = NULL;

	}

	atomic_inc(&stats.cnt_dbuf_finish);
}

static long is_dbufcon_map(struct is_vb2_buf *vbuf)
{
	int i;
	long ret;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vbuf->num_merged_dbufs
		* (vb->num_planes - NUM_OF_META_PLANE);

	for (i = 0; i < num_i_planes; i++) {
		vbuf->sgt[i] = dma_buf_map_attachment(vbuf->atch[i], DMA_BIDIRECTIONAL);
		if (IS_ERR(vbuf->sgt[i])) {
			ret = PTR_ERR(vbuf->atch[i]);
			pr_err("Error getting dmabuf scatterlist\n");
			goto err_map_dbuf;
		}
		vbuf->dva[i] = sg_dma_address(vbuf->sgt[i]->sgl);
		if (IS_ERR_VALUE(vbuf->dva[i])) {
			err("failed to map dmabuf: %d", i);
			ret = vbuf->dva[i];
			goto err_map_dbuf;
		}
	}

	atomic_inc(&stats.cnt_dbuf_map);

	return 0;

err_map_dbuf:
	vbuf->dva[i] = 0;

	while (i-- > 0) {
		dma_buf_unmap_attachment(vbuf->atch[i], vbuf->sgt[i],
				DMA_BIDIRECTIONAL);
		vbuf->dva[i] = 0;
	}

	return ret;
}

static void is_dbufcon_unmap(struct is_vb2_buf *vbuf)
{
	int i;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vbuf->num_merged_dbufs
		* (vb->num_planes - NUM_OF_META_PLANE);

	for (i = 0; i < num_i_planes; i++) {
		dma_buf_unmap_attachment(vbuf->atch[i], vbuf->sgt[i],
				DMA_BIDIRECTIONAL);
		vbuf->dva[i] = 0;
	}

	atomic_inc(&stats.cnt_dbuf_unmap);
}

static int is_dbufcon_kmap(struct is_vb2_buf *vbuf, u32 plane)
{
	int ret;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	u32 num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbufcon = NULL;
	u32 adj_plane = plane;
	u32 num_planes, b, p;
	int count = 0, total_planes;
	bool is_dbuf = (plane < num_i_planes);

	if (is_dbuf) {
		/* DMA buffers */
		num_planes = num_i_planes;
		count = vbuf->num_merged_dbufs;
	} else {
		/* Shot buffers */
		adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
			+ (plane - num_i_planes);
		num_planes = 1;
		count = vbuf->num_merged_sbufs;
	}

	dbufcon = dma_buf_get(vb->planes[plane].m.fd);
	if (IS_ERR_OR_NULL(dbufcon)) {
		err("failed to get dmabuf of fd: %d, plane: %d",
			vb->planes[plane].m.fd, plane);
		ret = -EINVAL;
		goto err_get_dbufcon;
	}

	if (count) {
		if (is_dbuf)
			goto skip_dbufcon_release;
		else
			return 0;
	}

	count = dmabuf_container_get_count(dbufcon);
	if (count < 0) {
		ret = 0;
		goto err_get_count;
	} else if (count == 0) {
		err("empty dmabuf-container of fd: %d",
			vb->planes[plane].m.fd);
		ret = -EINVAL;
		goto err_empty_dbufcon;
	}

	/* Check the plane count limitation. */
	if (!is_dbuf) {
		total_planes = (num_i_planes * vbuf->num_merged_dbufs) + count;
		if (total_planes > IS_MAX_PLANES) {
			err("out of range plane. require: %d > supported: %d",
					total_planes, IS_MAX_PLANES);
			ret = -EINVAL;
			goto err_too_many_planes;

		}
	}

skip_dbufcon_release:
	for (b = 0, p = adj_plane; b < count; b++, p += num_planes) {
		/* Increase dma_buf reference count */
		if (vbuf->dbuf[p])
			get_dma_buf(vbuf->dbuf[p]);
		else
			vbuf->dbuf[p] = dmabuf_container_get_buffer(dbufcon, b);
		if (IS_ERR_OR_NULL(vbuf->dbuf[p])) {
			err("failed to get dmabuf-container's dmabuf: %d", b);
			ret = -EINVAL;
			goto err_get_dbuf;
		}


		vbuf->kva[p] = (ulong) dma_buf_vmap(vbuf->dbuf[p]);
		if (IS_ERR_OR_NULL((void *) vbuf->kva[p])) {
			err("[P%d]Failed to get kva %ld",
				p, vbuf->kva[p]);
			ret = -ENOMEM;
			goto err_kmap;
		}
	}

	if (dbufcon)
		dma_buf_put(dbufcon);

	if (!is_dbuf)
		vbuf->num_merged_sbufs = count;

	atomic_inc(&stats.cnt_dbuf_kmap);

	return 0;

err_kmap:
err_get_dbuf:
	for (b = 0, p = adj_plane; b < count; b++, p += num_planes) {
		if (!IS_ERR_OR_NULL((void *) vbuf->kva[p])) {
			dma_buf_vunmap(vbuf->dbuf[p], (void *)vbuf->kva[p]);
			vbuf->kva[p] = 0;
		}

		if (!IS_ERR_OR_NULL(vbuf->dbuf[p]))
			dma_buf_put(vbuf->dbuf[p]);
	}

err_too_many_planes:
err_empty_dbufcon:
err_get_count:
	if (dbufcon)
		dma_buf_put(dbufcon);

err_get_dbufcon:
	if (!is_dbuf)
		vbuf->num_merged_sbufs = 0;

	return ret;
}

static void is_dbufcon_kunmap(struct is_vb2_buf *vbuf, u32 plane)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	u32 adj_plane = plane;
	u32 num_planes, num_merged, b, p;
	bool is_dbuf = (plane < num_i_planes);

	if (is_dbuf) {
		/* DMA buffers */
		num_planes = num_i_planes;
		num_merged = vbuf->num_merged_dbufs;
	} else {
		/* Shot buffers */
		adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
			+ (plane - num_i_planes);
		num_planes = 1;
		num_merged = vbuf->num_merged_sbufs;
	}

	for (b = 0, p = adj_plane; b < num_merged; b++, p += num_planes) {
		if (!IS_ERR_OR_NULL((void *) vbuf->kva[p])) {
			dma_buf_vunmap(vbuf->dbuf[p], (void *)vbuf->kva[p]);
			vbuf->kva[p] = 0;
		}

		if (!IS_ERR_OR_NULL(vbuf->dbuf[p]))
			dma_buf_put(vbuf->dbuf[p]);
	}

	if (!is_dbuf)
		vbuf->num_merged_sbufs = 0;

	atomic_inc(&stats.cnt_dbuf_kunmap);
}

static int _is_dbufcon_get_count(struct dma_buf *dbufcon)
{
	return dmabuf_container_get_count(dbufcon);
}
#else
static long is_dbufcon_prepare(struct is_vb2_buf *vbuf, truct device *dev)
{
	return 0;
}
static void is_dbufcon_finish(struct is_vb2_buf *vbuf)
{
}

static long is_dbufcon_map(struct is_vb2_buf *vbuf)
{
	return 0;
}

static void is_dbufcon_unmap(struct is_vb2_buf *vbuf)
{
}

static int is_dbufcon_kmap(struct is_vb2_buf *vbuf, u32 plane)
{
	return 0;
}

static void is_dbufcon_kunmap(struct is_vb2_buf *vbuf, u32 plane)
{
}

static int _is_dbufcon_get_count(struct dma_buf *dbufcon)
{
	/* It's not dma_buf_containter FD */
	return -1
}
#endif

#ifdef ENABLE_LOGICAL_VIDEO_NODE
void is_subbuf_prepare(struct is_sub_dma_buf *buf, struct v4l2_plane *plane,
		struct device *dev)
{
	struct dma_buf *dbuf;
	int cnt = 0;
	u32 num_p = buf->num_plane;
	u32 p, b, i;
	bool is_dbufcon = true;
	u32 retry = 0, retry_max = 3;

	for (p = 0; p < num_p; p++) {
		dbuf = dma_buf_get(plane[p].m.fd);
		if (IS_ERR_OR_NULL(dbuf)) {
			err("[V%d][P%d]Failed to get dmabuf. fd %d ret %ld",
				buf->vid, p, plane[p].m.fd, PTR_ERR(dbuf));
			goto err_get_dbufcon;
		}

		cnt = _is_dbufcon_get_count(dbuf);
		if (cnt < 0) {
			is_dbufcon = false;
			cnt = 1;
		} else if (cnt == 0) {
			err("[V%d][P%d]Empty dma_buf_container. fd %d",
				buf->vid, p, plane[p].m.fd);
			goto err_get_dbuf;
		}

		if ((cnt * num_p) > IS_MAX_PLANES) {
			err("[V%d][P%d]Out of range plane. required %d > supported %d",
				buf->vid, p,
				(cnt * num_p), IS_MAX_PLANES);
			goto err_get_dbuf;
		}

		for (b = 0, i = p; b < cnt; b++, i = (num_p * b) + p) {
			buf->dbuf[i] = is_dbufcon ?
				dmabuf_container_get_buffer(dbuf, b) :
				dma_buf_get(plane[i].m.fd);
			if (IS_ERR_OR_NULL(buf->dbuf[i])) {
				err("[V%d][P%d]Failed to get dmabuf. fd %d ret %ld",
					buf->vid, i,
					plane[i].m.fd, PTR_ERR(buf->dbuf[i]));
				goto err_get_dbuf;
			}

			buf->atch[i] = dma_buf_attach(buf->dbuf[i], dev);
			if (IS_ERR(buf->atch[i])) {
				err("[V%d][P%d]Failed to attach dmabuf. ret %ld",
					buf->vid, i, PTR_ERR(buf->atch[i]));
				goto err_get_dbuf;
			}

			do {
				buf->sgt[i] = dma_buf_map_attachment(buf->atch[i],
					DMA_BIDIRECTIONAL);
				if (IS_ERR(buf->sgt[i])) {
					err("[V%d][P%d]Failed to get sgt. ret %d (retry:%ld)",
						buf->vid, i,  PTR_ERR(buf->sgt[i]), retry);
					retry++;
					if (retry >= retry_max)
						goto err_get_dbuf;

					usleep_range(2000, 2001);
				}

			} while (IS_ERR(buf->sgt[i]));

			buf->size[i] = plane[p].length;
			buf->dva[i] = 0;
			buf->kva[i] = 0;
		}

		dma_buf_put(dbuf);
	}

	buf->num_buffer = cnt;

	atomic_inc(&stats.cnt_subbuf_prepare);

	return;

err_get_dbuf:
	dma_buf_put(dbuf);

err_get_dbufcon:
	buf->num_buffer = 0;

	while (p-- > 0) {
		for (b = 0, i = p; b < cnt; b++, i = (b * num_p) + p) {
			dma_buf_unmap_attachment(buf->atch[i], buf->sgt[i],
					DMA_BIDIRECTIONAL);
			dma_buf_detach(buf->dbuf[i], buf->atch[i]);
			dma_buf_put(buf->dbuf[i]);
			buf->sgt[i] = NULL;
			buf->atch[i] = NULL;
			buf->dbuf[i] = NULL;
		}
	}
}

long is_subbuf_dvmap(struct is_sub_dma_buf *buf)
{
	long ret;
	u32 num_planes = buf->num_plane * buf->num_buffer;
	u32 p;

	for (p = 0; p < num_planes; p++) {
		if (!buf->sgt[p]) {
			err("[N%d][P%d]Invalid sgt 0x%llx fd %d",
				buf->vid, p,
				buf->sgt[p],
				buf->buf_fd[p]);
			/* critical error!! will be die */
			return -ENOMEM;
		}

		buf->dva[p] = sg_dma_address(buf->sgt[p]->sgl);
		if (IS_ERR_VALUE(buf->dva[p])) {
			err("[N%d][P%d]Failed to get dva",
				buf->vid, p);
			ret = buf->dva[p];
			goto err_map_dbuf;
		}
	}

	return 0;

err_map_dbuf:
	while (p-- > 0) {
		dma_buf_unmap_attachment(buf->atch[p], buf->sgt[p],
				DMA_BIDIRECTIONAL);
		buf->dva[p] = 0;
	}

	return ret;
}

ulong is_subbuf_kvmap(struct is_sub_dma_buf *buf)
{
	long ret;
	u32 num_planes = buf->num_plane * buf->num_buffer;
	u32 p;

	for (p = 0; p < num_planes; p++) {
		buf->kva[p] = (ulong)dma_buf_vmap(buf->dbuf[p]);
		if (IS_ERR_OR_NULL((void *)buf->kva[p])) {
			err("[N%d][P%d]Failed to get kva",
				buf->vid, p);
			ret = buf->kva[p];
			goto err_vmap;
		}
	}

	atomic_inc(&stats.cnt_subbuf_kvmap);

	return 0;

err_vmap:
	while (p-- > 0) {
		dma_buf_vunmap(buf->dbuf[p], (void *)buf->kva[p]);
		buf->kva[p] = 0;
	}

	return ret;
}

void is_subbuf_kunmap(struct is_sub_dma_buf *buf)
{
	u32 num_planes = buf->num_plane * buf->num_buffer;
	u32 p;

	for (p = 0; p < num_planes; p++) {
		if (buf->kva[p]) {
			dma_buf_vunmap(buf->dbuf[p], (void *)buf->kva[p]);
			buf->kva[p] = 0;
		}
	}

	atomic_inc(&stats.cnt_subbuf_kunmap);
}

void is_subbuf_finish(struct is_sub_dma_buf *buf)
{
	u32 num_planes = buf->num_plane * buf->num_buffer;
	u32 p;

	for (p = 0; p < num_planes; p++) {
		dma_buf_unmap_attachment(buf->atch[p], buf->sgt[p],
				DMA_BIDIRECTIONAL);
		dma_buf_detach(buf->dbuf[p], buf->atch[p]);
		dma_buf_put(buf->dbuf[p]);

		/* clear */
		buf->sgt[p] = NULL;
		buf->atch[p] = NULL;
		buf->dbuf[p] = NULL;
		buf->dva[p] = 0;
		buf->buf_fd[p] = 0;
	}

	buf->num_plane = 0;
	buf->num_buffer = 0;
	buf->vid = 0;

	atomic_inc(&stats.cnt_subbuf_finish);
}
#endif

const struct is_vb2_buf_ops is_vb2_buf_ops_dma_sg = {
	.plane_kvaddr		= is_vb2_dma_sg_plane_kvaddr,
	.plane_dvaddr		= is_vb2_dma_sg_plane_dvaddr,
	.plane_kmap		= is_vb2_dma_sg_plane_kmap,
	.plane_kunmap		= is_vb2_dma_sg_plane_kunmap,
	.remap_attr		= is_vb2_dma_sg_remap_attr,
	.unremap_attr		= is_vb2_dma_sg_unremap_attr,
	.dbufcon_prepare	= is_dbufcon_prepare,
	.dbufcon_finish		= is_dbufcon_finish,
	.dbufcon_map		= is_dbufcon_map,
	.dbufcon_unmap		= is_dbufcon_unmap,
	.dbufcon_kmap		= is_dbufcon_kmap,
	.dbufcon_kunmap		= is_dbufcon_kunmap,
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	.subbuf_prepare		= is_subbuf_prepare,
	.subbuf_dvmap		= is_subbuf_dvmap,
	.subbuf_kvmap		= is_subbuf_kvmap,
	.subbuf_kunmap		= is_subbuf_kunmap,
	.subbuf_finish		= is_subbuf_finish
#endif
};

/* is private buffer operations */
static void is_ion_free(struct is_priv_buf *pbuf)
{
	struct is_ion_ctx *alloc_ctx;

	alloc_ctx = pbuf->ctx;

	if (pbuf->kva)
		dma_buf_vunmap(pbuf->dma_buf, pbuf->kva);

	dma_buf_unmap_attachment(pbuf->attachment, pbuf->sgt,
				DMA_BIDIRECTIONAL);
	dma_buf_detach(pbuf->dma_buf, pbuf->attachment);
	dma_buf_put(pbuf->dma_buf);

	dbg_mem(1, "%s: size(%zu)\n", __func__, pbuf->size);
	atomic_sub(pbuf->size, &mem_stats_sz_active);

	vfree(pbuf);
}

static ulong is_ion_kvaddr(struct is_priv_buf *pbuf)
{
	if (!pbuf)
		return 0;

	if (!pbuf->kva)
		pbuf->kva = dma_buf_vmap(pbuf->dma_buf);

	return (ulong)pbuf->kva;
}

static dma_addr_t is_ion_dvaddr(struct is_priv_buf *pbuf)
{
	dma_addr_t dva = 0;

	if (!pbuf)
		return -EINVAL;

	if (pbuf->iova == 0)
		return -EINVAL;

	dva = pbuf->iova;

	return (ulong)dva;
}

/* The physical address can only be supported when using reserved memory. */
static phys_addr_t is_ion_phaddr(struct is_priv_buf *pbuf)
{
	dma_addr_t phya = 0;

	phya = sg_phys(pbuf->sgt->sgl);

	return (phys_addr_t)phya;
}

static void _is_ion_sync_for_xxx(struct is_priv_buf *pbuf,
		off_t offset, size_t size, enum dma_data_direction dir, enum is_cache_ops ops)
{
	int i;
	struct scatterlist *sg;
	ulong size_offset = 0;
	size_t remained_size = size;

	if (pbuf->kva) {
		FIMC_BUG_VOID((offset < 0) || (offset > pbuf->size));
		FIMC_BUG_VOID((offset + size) < size);
		FIMC_BUG_VOID((size > pbuf->size) || ((offset + size) > pbuf->size));

		dbg_mem(1, "sync_for_%s start (iova:%x, offset:%d, size:%d, total_len:%d)",
				ops == IS_SYNC_FOR_DEVICE ? "device" : "cpu", pbuf->iova,
				offset, size, sg_dma_len(pbuf->sgt->sgl));
		for_each_sg(pbuf->sgt->sgl, sg, pbuf->sgt->orig_nents, i) {
			dbg_mem(2, "[I:%d] size_offset: %d, sg->length:%d", i, size_offset, sg->length);
			if (size_offset + sg->length >= offset) {
				ulong actual_offset;
				size_t actual_size;

				/* If offset starts inside the sg region,
				 * start region of cache process is offset.
				 * Else, start region of cache process is sg start point
				 */
				actual_offset = (size_offset <= offset) ? offset : size_offset;

				/* Case 1: cache region is in sg region
				 * Case 2: cache region start in sg region, but size is out of region
				 * Case 3: cache region is larger than sg region or same
				 */
				if (offset + size < size_offset + sg->length)
					actual_size = remained_size;
				else if (offset + size >= size_offset + sg->length && actual_offset == offset)
					actual_size = sg->length - (offset - size_offset);
				else
					actual_size = sg->length;

				remained_size -= actual_size;

				if (actual_size > sg->length) {
					warn("%s, actual_size %d > sg->length %d", __func__, actual_size, sg->length);
					actual_offset = size_offset;
					actual_size = sg->length;
				}

				dbg_mem(2, "[I:%d] do cache ops(actual_offset:%d, actual_size:%d)",
						i, actual_offset, actual_size);
				if (ops == IS_SYNC_FOR_DEVICE) {
					dma_sync_single_for_device(is_dev,
							pbuf->iova + actual_offset, actual_size, dir);
				} else {
					dma_sync_single_for_cpu(is_dev,
							pbuf->iova + actual_offset, actual_size, dir);
				}
			}
			size_offset += sg->length;
			if (size_offset >= offset + size)
				break;
		}
		dbg_mem(1, "sync_for_%s end", ops == IS_SYNC_FOR_DEVICE ? "device" : "cpu");
	}
}

static void is_ion_sync_for_device(struct is_priv_buf *pbuf,
		off_t offset, size_t size, enum dma_data_direction dir)
{
	FIMC_BUG_VOID(!pbuf);
	_is_ion_sync_for_xxx(pbuf, offset, size, dir, IS_SYNC_FOR_DEVICE);
}

static void is_ion_sync_for_cpu(struct is_priv_buf *pbuf,
		off_t offset, size_t size, enum dma_data_direction dir)
{
	FIMC_BUG_VOID(!pbuf);
	_is_ion_sync_for_xxx(pbuf, offset, size, dir, IS_SYNC_FOR_CPU);
}

const struct is_priv_buf_ops is_priv_buf_ops_ion = {
	.free			= is_ion_free,
	.kvaddr			= is_ion_kvaddr,
	.dvaddr			= is_ion_dvaddr,
	.phaddr			= is_ion_phaddr,
	.sync_for_device	= is_ion_sync_for_device,
	.sync_for_cpu		= is_ion_sync_for_cpu,
};

/* is memory operations */
unsigned int is_ion_query_heapmask(const char *heap_name)
{
	struct ion_heap_data data[ION_NUM_MAX_HEAPS];
	int i;
	int cnt = ion_query_heaps_kernel(NULL, 0);

	ion_query_heaps_kernel((struct ion_heap_data *)data, cnt);

	for (i = 0; i < cnt; i++) {
		 if (!strncmp(data[i].name, heap_name, MAX_HEAP_NAME))
			 break;
	}

	if (i == cnt)
		return 0;

	return 1 << data[i].heap_id;
}

static void *is_ion_init(struct platform_device *pdev)
{
	struct is_ion_ctx *ctx;
	const char *heapname =
		IS_ENABLED(CONFIG_ION_EXYNOS) ? "vendor_system_heap" : "ion_system_heap";
	ctx = vzalloc(sizeof(*ctx));
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->dev = &pdev->dev;
	ctx->alignment = SZ_4K;
	ctx->flags = ION_FLAG_CACHED;
	mutex_init(&ctx->lock);
	ctx->heapmask = is_ion_query_heapmask(heapname);
	if (!ctx->heapmask)
		ctx->heapmask = is_ion_query_heapmask("ion_system_heap");

	return ctx;
}

static void is_ion_deinit(void *ctx)
{
	struct is_ion_ctx *alloc_ctx = ctx;

	mutex_destroy(&alloc_ctx->lock);
	vfree(alloc_ctx);
}

static struct is_priv_buf *is_ion_alloc(void *ctx,
		size_t size, const char *heapname, unsigned int flags)
{
	struct is_ion_ctx *alloc_ctx = ctx;
	struct is_priv_buf *buf;
	unsigned int heapmask;
#if defined(CONFIG_CAMERA_VENDER_MCD)
	unsigned int heapmask_r;
#endif
	int ret = 0;

	buf = vzalloc(sizeof(*buf));
	if (!buf)
		return ERR_PTR(-ENOMEM);

	size = PAGE_ALIGN(size);

	heapmask = (flags & ION_EXYNOS_FLAG_PROTECTED) ? alloc_ctx->heapmask_s : alloc_ctx->heapmask;

#if defined(CONFIG_CAMERA_VENDER_MCD)
	if (heapname != NULL
		&& !strncmp(heapname, "camera_heap", MAX_HEAP_NAME)) {
		heapmask_r = is_ion_query_heapmask(heapname);
		if (heapmask_r) {
			heapmask = heapmask_r;
		}
	}
#endif

	buf->dma_buf = ion_alloc(size, heapmask, flags ? flags : alloc_ctx->flags);
	if (IS_ERR(buf->dma_buf)) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	buf->attachment = dma_buf_attach(buf->dma_buf, alloc_ctx->dev);
	if (IS_ERR(buf->attachment)) {
		ret = PTR_ERR(buf->attachment);
		goto err_attach;
	}

	buf->sgt = dma_buf_map_attachment(
				buf->attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->sgt)) {
		ret = PTR_ERR(buf->sgt);
		goto err_map_dmabuf;
	}

	buf->ctx = alloc_ctx;
	buf->size = size;
	buf->direction = DMA_BIDIRECTIONAL;
	buf->ops = &is_priv_buf_ops_ion;

	mutex_lock(&alloc_ctx->lock);
	buf->iova = sg_dma_address(buf->sgt->sgl);
	if (IS_ERR_VALUE(buf->iova)) {
		ret = (int)buf->iova;
		mutex_unlock(&alloc_ctx->lock);
		goto err_ion_map_io;
	}
	mutex_unlock(&alloc_ctx->lock);

	dbg_mem(1, "%s: size(%zu), flag(%x)\n", __func__, size, flags);
	atomic_add(size, &mem_stats_sz_active);

	return buf;

err_ion_map_io:
	dma_buf_unmap_attachment(buf->attachment, buf->sgt,
				DMA_BIDIRECTIONAL);
err_map_dmabuf:
	dma_buf_detach(buf->dma_buf, buf->attachment);
err_attach:
	dma_buf_put(buf->dma_buf);
err_alloc:
	vfree(buf);

	pr_err("%s: Error occured while allocating\n", __func__);
	return ERR_PTR(ret);
}

static int is_ion_resume(void *ctx)
{
	return 0;
}

static void is_ion_suspend(void *ctx)
{
}

const struct is_mem_ops is_mem_ops_ion = {
	.init			= is_ion_init,
	.cleanup		= is_ion_deinit,
	.resume			= is_ion_resume,
	.suspend		= is_ion_suspend,
	.alloc			= is_ion_alloc,
};
#endif

/* is private buffer operations */
static void is_km_free(struct is_priv_buf *pbuf)
{
	kfree(pbuf->kvaddr);
	kfree(pbuf);
}

static ulong is_km_kvaddr(struct is_priv_buf *pbuf)
{
	if (!pbuf)
		return 0;

	return (ulong)pbuf->kvaddr;
}

static phys_addr_t is_km_phaddr(struct is_priv_buf *pbuf)
{
	phys_addr_t pa = 0;

	if (!pbuf)
		return 0;

	pa = virt_to_phys(pbuf->kvaddr);

	return pa;
}

const struct is_priv_buf_ops is_priv_buf_ops_km = {
	.free			= is_km_free,
	.kvaddr			= is_km_kvaddr,
	.phaddr			= is_km_phaddr,
};

static struct is_priv_buf *is_kmalloc(size_t size, size_t align)
{
	struct is_priv_buf *buf = NULL;
	int ret = 0;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->kvaddr = kzalloc(size, GFP_KERNEL);
	if (!buf->kvaddr) {
		ret = -ENOMEM;
		goto err_priv_alloc;
	}

	buf->size = size;
	buf->align = align;
	buf->ops = &is_priv_buf_ops_km;

	return buf;

err_priv_alloc:
	kfree(buf);

	return ERR_PTR(ret);
}

void is_vfree_atomic(const void *addr)
{
	/*
	 * Use raw_cpu_ptr() because this can be called from preemptible
	 * context. Preemption is absolutely fine here, because the llist_add()
	 * implementation is lockless, so it works even if we are adding to
	 * nother cpu's list.  schedule_work() should be fine with this too.
	 */
	struct is_vfree_deferred *p = raw_cpu_ptr(is_vfree_deferred);

	kmemleak_free(addr);

	if (llist_add((struct llist_node *)addr, &p->list))
		schedule_work(&p->wq);
}

static void is_mem_init_stats(void)
{
	atomic_set(&stats.cnt_plane_kmap, 0);
	atomic_set(&stats.cnt_plane_kunmap, 0);
	atomic_set(&stats.cnt_buf_remap, 0);
	atomic_set(&stats.cnt_buf_unremap, 0);
	atomic_set(&stats.cnt_dbuf_prepare, 0);
	atomic_set(&stats.cnt_dbuf_finish, 0);
	atomic_set(&stats.cnt_dbuf_map, 0);
	atomic_set(&stats.cnt_dbuf_unmap, 0);
	atomic_set(&stats.cnt_dbuf_kmap, 0);
	atomic_set(&stats.cnt_dbuf_kunmap, 0);
	atomic_set(&stats.cnt_subbuf_prepare, 0);
	atomic_set(&stats.cnt_subbuf_finish, 0);
	atomic_set(&stats.cnt_subbuf_kvmap, 0);
	atomic_set(&stats.cnt_subbuf_kunmap, 0);
}

void is_mem_check_stats(struct is_mem *mem)
{
	bool unbalanced = atomic_read(&mem->stats->cnt_plane_kmap)
		!= atomic_read(&mem->stats->cnt_plane_kunmap)	||
		atomic_read(&mem->stats->cnt_buf_remap)
		!= atomic_read(&mem->stats->cnt_buf_unremap)	||
		atomic_read(&mem->stats->cnt_dbuf_prepare)
		!= atomic_read(&mem->stats->cnt_dbuf_finish)	||
		atomic_read(&mem->stats->cnt_dbuf_map)
		!= atomic_read(&mem->stats->cnt_dbuf_unmap)	||
		atomic_read(&mem->stats->cnt_dbuf_kmap)
		!= atomic_read(&mem->stats->cnt_dbuf_kunmap)	||
		atomic_read(&mem->stats->cnt_subbuf_prepare)
		!= atomic_read(&mem->stats->cnt_subbuf_finish)	||
		atomic_read(&mem->stats->cnt_subbuf_kvmap)
		!= atomic_read(&mem->stats->cnt_subbuf_kunmap);


	if (unbalanced) {
		dev_err(mem->default_ctx->dev,
			"counters for memory OPs unbalanced!\n");
		dev_err(mem->default_ctx->dev,
			"\tplane_kmap: %ld, plane_kunmap: %ld\n",
			atomic_read(&mem->stats->cnt_plane_kmap),
			atomic_read(&mem->stats->cnt_plane_kunmap));
		dev_err(mem->default_ctx->dev,
			"\tbuf_remap: %ld, buf_unremap: %ld\n",
			atomic_read(&mem->stats->cnt_buf_remap),
			atomic_read(&mem->stats->cnt_buf_unremap));
		dev_err(mem->default_ctx->dev,
			"\tdbuf_prepare: %ld, dbuf_finish: %ld\n",
			atomic_read(&mem->stats->cnt_dbuf_prepare),
			atomic_read(&mem->stats->cnt_dbuf_finish));
		dev_err(mem->default_ctx->dev,
			"\tdbuf_map: %ld, dbuf_unmap: %ld\n",
			atomic_read(&mem->stats->cnt_dbuf_map),
			atomic_read(&mem->stats->cnt_dbuf_unmap));
		dev_err(mem->default_ctx->dev,
			"\tdbuf_kmap: %ld, dbuf_kunmap: %ld\n",
			atomic_read(&mem->stats->cnt_dbuf_kmap),
			atomic_read(&mem->stats->cnt_dbuf_kunmap));
		dev_err(mem->default_ctx->dev,
			"\tsubbuf_prepare: %ld, subbuf_finish: %ld\n",
			atomic_read(&mem->stats->cnt_subbuf_prepare),
			atomic_read(&mem->stats->cnt_subbuf_finish));
		dev_err(mem->default_ctx->dev,
			"\tsubbuf_kvmap: %ld, subbuf_kunmap: %ld\n",
			atomic_read(&mem->stats->cnt_subbuf_kvmap),
			atomic_read(&mem->stats->cnt_subbuf_kunmap));
	}

	is_mem_init_stats();
}

int is_mem_init(struct is_mem *mem, struct platform_device *pdev)
{
	int i;
#if IS_ENABLED(CONFIG_VIDEOBUF2_DMA_SG)
	mem->is_mem_ops = &is_mem_ops_ion;
	mem->vb2_mem_ops = &vb2_dma_sg_memops;
	mem->is_vb2_buf_ops = &is_vb2_buf_ops_dma_sg;
	mem->kmalloc = &is_kmalloc;
#endif

	mem->default_ctx = CALL_PTR_MEMOP(mem, init, pdev);
	if (IS_ERR_OR_NULL(mem->default_ctx)) {
		if (IS_ERR(mem->default_ctx))
			return PTR_ERR(mem->default_ctx);
		else
			return -EINVAL;
	}

	is_vfree_deferred = alloc_percpu(struct is_vfree_deferred);
	for_each_possible_cpu(i) {
		struct is_vfree_deferred *p;

		p = per_cpu_ptr(is_vfree_deferred, i);
		init_llist_head(&p->list);
		INIT_WORK(&p->wq, is_free_work);
	}

	mem->stats = &stats;

	is_mem_init_stats();

	return 0;
}
