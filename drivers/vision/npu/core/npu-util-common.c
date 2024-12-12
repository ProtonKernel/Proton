/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/ktime.h>

#include "npu-util-common.h"
#include "npu-log.h"

/*
 * Time calculation
 */
inline s64 npu_get_time_ns(void)
{
	return ktime_to_ns(ktime_get_boottime());
}

inline s64 npu_get_time_us(void)
{
	return ktime_to_us(ktime_get_boottime());
}

/*
 * Validate memory range of offsets in NCP. Should not extend beyond NCP size.
 * Return error if check fails.
 */
static inline int validate_ncp_offset_range(u32 offset, u32 size, u32 cnt, size_t ncp_size)
{
	size_t start = offset, end = start + size * cnt;

	if (!start || !cnt || !size) /* Redundant entry. Ignore. */
		return 0;

	if (end > ncp_size)
		return -EFAULT;

	return 0;
}

static int validate_ncp_header(struct ncp_header *ncp_header, size_t ncp_size)
{
	/* Validate NCP version. */
	if (ncp_header->hdr_version != NCP_VERSION) {
		npu_err("NCP version mismatch. user=0x%x, expected=0x%x\n",
			ncp_header->hdr_version, NCP_VERSION);
		return -EFAULT;
	}

	/* Validate vector memory range. */

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->address_vector_offset,
		sizeof(struct address_vector), ncp_header->address_vector_cnt, ncp_size),
		"address vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->address_vector_offset, ncp_header->address_vector_cnt, ncp_size);

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->memory_vector_offset,
		sizeof(struct memory_vector), ncp_header->memory_vector_cnt, ncp_size),
		"memory vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->memory_vector_offset, ncp_header->memory_vector_cnt, ncp_size);

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->group_vector_offset,
		sizeof(struct group_vector), ncp_header->group_vector_cnt, ncp_size),
		"group vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->group_vector_offset, ncp_header->group_vector_cnt, ncp_size);

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->thread_vector_offset,
		sizeof(struct thread_vector), ncp_header->thread_vector_cnt, ncp_size),
		"thread vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->thread_vector_offset, ncp_header->thread_vector_cnt, ncp_size);

	/* Validate body memory range. */
	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->body_offset,
		ncp_header->body_size, 1, ncp_size),
		"body out of bounds. start=0x%x, size=0x%x, ncp_size=0x%lx\n",
		ncp_header->body_offset, ncp_header->body_size, ncp_size);

	return 0;
}

static int validate_ncp_memory_vector(struct ncp_header *ncp_header, size_t ncp_size)
{
	u32 mv_cnt, mv_offset, av_index, av_offset;
	struct memory_vector *mv;
	struct address_vector *av;
	int i;

	mv_cnt = ncp_header->memory_vector_cnt;
	mv_offset = ncp_header->memory_vector_offset;
	av_offset = ncp_header->address_vector_offset;

	mv = (struct memory_vector *)((void *)ncp_header + mv_offset);
	av = (struct address_vector *)((void *)ncp_header + av_offset);

	for (i = 0; i < mv_cnt; i++) {

		/* Validate address vector index */
		av_index = mv[i].address_vector_index;
		if (av_index >= ncp_header->address_vector_cnt) {
			npu_err("memory vector %d has invalid address vector index %d\n",
				i, av_index);
			return -EFAULT;
		}

		/* Validate address vector m_addr */
		switch (mv[i].type) {
		case MEMORY_TYPE_CUCODE:
			fallthrough;
		case MEMORY_TYPE_WEIGHT:
			fallthrough;
		case MEMORY_TYPE_WMASK:
			NPU_ERR_RET(validate_ncp_offset_range(av[av_index].m_addr,
				av[av_index].size, 1, ncp_size),
				"m_addr out of bounds. start=0x%x, size=0x%x, ncp_size=0x%lx\n",
				av[av_index].m_addr, av[av_index].size, ncp_size);
			break;
		default:
			break;
		}
	}

	return 0;
}

static int validate_ncp_group_vector(struct ncp_header *ncp_header, size_t ncp_size)
{
	u32 gv_cnt, gv_offset;
	struct group_vector *gv;
	int i;

	gv_cnt = ncp_header->group_vector_cnt;
	gv_offset = ncp_header->group_vector_offset;
	gv = (struct group_vector *)((void *)ncp_header + gv_offset);

	for (i = 0; i < gv_cnt; i++) {

		/* Validate isa memory out of bounds. */
		NPU_ERR_RET(validate_ncp_offset_range(
			ncp_header->body_offset + gv[i].isa_offset, gv[i].isa_size, 1, ncp_size),
			"group vector isa out of bounds. start=0x%x, size=0x%x, ncp_size=0x%lx\n",
			ncp_header->body_offset + gv[i].isa_offset, gv[i].isa_size, ncp_size);

		/* Validate intrinsic memory out of bounds. */
		NPU_ERR_RET(validate_ncp_offset_range(
			ncp_header->body_offset + gv[i].intrinsic_offset,
			gv[i].intrinsic_size, 1, ncp_size),
			"group vector intrinsic out of bounds. start=0x%x, size=0x%x, ncp_size=0x%lx\n",
			ncp_header->body_offset + gv[i].intrinsic_offset,
			gv[i].intrinsic_size, ncp_size);
	}

	return 0;
}

static int validate_ncp_thread_vector(struct ncp_header *ncp_header, size_t ncp_size)
{
	u32 tv_cnt, tv_offset;
	struct thread_vector *tv;
	int i;

	tv_cnt = ncp_header->thread_vector_cnt;
	tv_offset = ncp_header->thread_vector_offset;
	tv = (struct thread_vector *)((void *)ncp_header + tv_offset);

	for (i = 0; i < tv_cnt; i++) {

		/* Validate group vector index out of bounds. */
		if (ncp_header->group_vector_cnt) {
			if (tv[i].group_str_idx >= ncp_header->group_vector_cnt ||
				tv[i].group_end_idx >= ncp_header->group_vector_cnt) {
				npu_err("thread vector group index out of bounds\n");
				return -EFAULT;
			}
		}

	}

	return 0;
}

int npu_util_validate_user_ncp(struct npu_session *session, struct ncp_header *ncp_header,
				size_t ncp_size)
{
	int ret;

	ret = validate_ncp_header(ncp_header, ncp_size);
	if (ret)
		goto fail;

	ret = validate_ncp_memory_vector(ncp_header, ncp_size);
	if (ret)
		goto fail;

	ret = validate_ncp_group_vector(ncp_header, ncp_size);
	if (ret)
		goto fail;

	ret = validate_ncp_thread_vector(ncp_header, ncp_size);
	if (ret)
		goto fail;

	return 0;
fail:
	return ret;
}
