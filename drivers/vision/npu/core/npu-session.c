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

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
//#include "lib/vpul-ds.h"
#include "npu-log.h"
#include "npu-device.h"
#include "npu-common.h"
#include "npu-session.h"
#include "npu-scheduler.h"
#include "npu-util-common.h"

#include <asm/cacheflush.h>

#ifdef NPU_LOG_TAG
#undef NPU_LOG_TAG
#endif
#define NPU_LOG_TAG             "npu-sess"

//#define SYSMMU_FAULT_TEST

const struct npu_session_ops npu_session_ops;

struct addr_info *find_addr_info(struct npu_session *session, u32 av_index, mem_opt_e mem_type, u32 cl_index)
{
	struct addr_info *ret = NULL;
	u32 i;
	u32 FM_cnt;
	struct addr_info *addr_info = NULL;

	switch (mem_type) {
	case OFM_TYPE:
		FM_cnt = session->OFM_cnt;
		/* Only 0th buffer given for comparison */
		addr_info = session->OFM_info[cl_index][0].addr_info;
		break;
	case IMB_TYPE:
		FM_cnt = session->IMB_cnt;
		addr_info = session->IMB_info;
		break;
	default:
		npu_uerr("Invalid mem_type : %d\n", session, mem_type);
		ret = NULL;
		goto err_exit;
	}
	for (i = 0; i < FM_cnt; i++) {
		if (unlikely((addr_info + i)->av_index == av_index)) {
			ret = addr_info + i;
			break;
		}
	}
err_exit:
	return ret;
}

npu_errno_t chk_nw_result_no_error(struct npu_session *session)
{
	return session->nw_result.result_code;
}

int npu_session_save_result(struct npu_session *session, struct nw_result nw_result)
{
	session->nw_result = nw_result;
	wake_up(&session->wq);
	return 0;
}

void npu_session_queue_done(struct npu_queue *queue, struct vb_container_list *inclist, struct vb_container_list *otclist, unsigned long flag)
{
	u32 buff_cnt;
	buff_cnt = otclist->containers[0].count;
	atomic_inc(&otclist->containers[0].completed_count);
	if (likely(atomic_read(&otclist->containers[0].completed_count) >= buff_cnt))
		npu_queue_done(queue, inclist, otclist, flag);
	return;
}

static save_result_func get_notify_func(const nw_cmd_e nw_cmd)
{
	if (unlikely(NPU_NW_CMD_UNLOAD == nw_cmd) || unlikely(NPU_NW_CMD_CLEAR_CB == nw_cmd))
		return NULL;
	else
		return npu_session_save_result;
}

static int npu_session_put_nw_req(struct npu_session *session, nw_cmd_e nw_cmd)
{
	int ret = 0;
	struct npu_nw req = {
		.uid = session->uid,
		.bound_id = session->sched_param.bound_id,
		.npu_req_id = 0,
		.result_code = 0,
		.result_value = 0,
		.session = session,
		.cmd = nw_cmd,
		.ncp_addr = session->ncp_info.ncp_addr,
		.magic_tail = NPU_NW_MAGIC_TAIL,
	};

	BUG_ON(!session);

	req.notify_func = get_notify_func(nw_cmd);

	ret = npu_ncp_mgmt_put(&req);
	if (unlikely(!ret)) {
		npu_uerr("npu_ncp_mgmt_put failed", session);
		return ret;
	}
	return ret;
}

int npu_session_put_frame_req(
	struct npu_session *session, struct npu_queue *queue,
	struct vb_container_list *incl, struct vb_container_list *otcl,
	frame_cmd_e frame_cmd, struct av_info *IFM_info, struct av_info *OFM_info, u32 j)
{
	int ret = 0;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_vertex_ctx *vctx;
	struct npu_frame frame = {
		.uid = session->uid,
		.frame_id = j+1,
		.npu_req_id = 0,	/* Determined in manager */
		.result_code = 0,
		.session = session,
		.cmd = frame_cmd,
		.priority = session->sched_param.priority,
		.priority = 0,		/* Not used */
		.src_queue = queue,
		.input = incl,
		.output = otcl,
		.magic_tail = NPU_FRAME_MAGIC_TAIL,
		.mbox_process_dat = session->mbox_process_dat,
		.IFM_info = IFM_info,
		.OFM_info = OFM_info,
	};

	ret = npu_buffer_q_put(&frame);
	if (unlikely(!ret)) {
		npu_uerr("fail(%d) in npu_buffer_q_put\n",
				session, ret);
		ret = -EFAULT;
		goto p_err;
	}
	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	npu_scheduler_set_init_freq(device, session->uid);
	npu_udbg("success in %s\n", session, __func__);
	return 0;
p_err:
	return ret;
}

int add_ion_mem(struct npu_session *session, struct npu_memory_buffer *mem_buf, mem_opt_e MEM_OPT)
{
	switch (MEM_OPT) {
	case NCP_TYPE:
		session->ncp_mem_buf = mem_buf;
		break;
	case NCP_HDR_TYPE:
		session->ncp_hdr_buf = mem_buf;
		break;
	case IOFM_TYPE:
		session->IOFM_mem_buf = mem_buf;
		break;
	case IMB_TYPE:
		session->IMB_mem_buf = mem_buf;
		break;
	default:
		break;
	}
	return 0;
}

void __ion_release(struct npu_memory *memory, struct npu_memory_buffer *ion_mem_buf, u32 idx)
{
	u32 i;

	for (i = 0; i < idx; i++)
		npu_memory_free(memory, ion_mem_buf + i);
}

void __release_graph_ion(struct npu_session *session)
{
	struct npu_memory *memory;
	struct npu_memory_buffer *ion_mem_buf;

	memory = session->memory;
	ion_mem_buf = session->ncp_mem_buf;
	session->ncp_mem_buf = NULL;
	if (unlikely(ion_mem_buf)) {
		npu_memory_unmap(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->ncp_hdr_buf;
	session->ncp_hdr_buf = NULL;
	if (unlikely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->IOFM_mem_buf;
	session->IOFM_mem_buf = NULL;
	if (unlikely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->IMB_mem_buf;
	session->IMB_mem_buf = NULL;
	if (unlikely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}
}

int npu_session_NW_CMD_UNLOAD(struct npu_session *session)
{
	/* check npu_device emergency error */
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	nw_cmd_e nw_cmd = NPU_NW_CMD_UNLOAD;

	if (unlikely(!session)) {
		npu_err("invalid session\n");
		return -EINVAL;
	}

	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	if (unlikely(npu_device_is_emergency_err(device)))
		npu_udbg("NPU DEVICE IS EMERGENCY ERROR\n", session);

	/* Post unload command */
	npu_udbg("sending UNLOAD command.\n", session);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);

	if (session->ncp_payload) {
		npu_memory_free(session->memory, session->ncp_payload);
		kfree(session->ncp_payload);
		session->ncp_payload = NULL;
	}

	return 0;
}

int npu_session_close(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	session->ss_state |= BIT(NPU_SESSION_STATE_CLOSE);

	ret = npu_session_undo_close(session);
	if (ret)
		goto p_err;

	ret = npu_session_undo_open(session);
	if (ret)
		goto p_err;

	return ret;
p_err:
	npu_err("fail(%d) in npu_session_close\n", ret);
	return ret;
}

int npu_session_open(struct npu_session **session, void *cookie, void *memory)
{
	int ret = 0;
	int i = 0;
	int k = 0;

	*session = kzalloc(sizeof(struct npu_session), GFP_KERNEL);
	if (unlikely(*session == NULL)) {
		npu_err("fail in npu_session_open kzalloc\n");
		ret = -ENOMEM;
		return ret;
	}

	(*session)->ss_state |= BIT(NPU_SESSION_STATE_OPEN);

	ret = npu_sessionmgr_regID(cookie, *session);
	if (unlikely(ret)) {
		npu_err("fail(%d) in npu_sessionmgr_regID\n", ret);
		npu_session_register_undo_cb(*session, npu_session_undo_open);
		goto p_err;
	}

	(*session)->ss_state |= BIT(NPU_SESSION_STATE_REGISTER);

	init_waitqueue_head(&((*session)->wq));

	(*session)->cookie = cookie;
	(*session)->memory = memory;
	(*session)->gops = &npu_session_ops;
	(*session)->control.owner = *session;

	(*session)->IMB_info = NULL;
	for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
		for (i = 0; i < VISION_MAX_BUFFER; i++) {
			(*session)->IFM_info[k][i].addr_info = NULL;
			(*session)->IFM_info[k][i].state = SS_BUF_STATE_UNPREPARE;
			(*session)->OFM_info[k][i].addr_info = NULL;
			(*session)->OFM_info[k][i].state = SS_BUF_STATE_UNPREPARE;
		}
	}
	(*session)->ncp_mem_buf = NULL;
	(*session)->IOFM_mem_buf = NULL;
	(*session)->IMB_mem_buf = NULL;

	(*session)->qbuf_IOFM_idx = -1;
	(*session)->dqbuf_IOFM_idx = -1;

	/* Set default scheduling parameters */
	(*session)->sched_param.bound_id = NPU_BOUND_UNBOUND;
	(*session)->sched_param.priority = NPU_PRIORITY_MAX_VAL; /* lower priority */

	(*session)->pid	= task_pid_nr(current);

	return ret;
p_err:
	npu_session_execute_undo_cb(*session);
	return ret;
}

int _undo_s_graph_each_state(struct npu_session *session)
{
	u32 i = 0;
	u32 k = 0;
	struct npu_memory *memory;
	struct npu_memory_buffer *ion_mem_buf;
	struct addr_info *addr_info;

	BUG_ON(!session);

	memory = session->memory;

	if (session->ss_state <= BIT(NPU_SESSION_STATE_GRAPH_ION_MAP))
		goto graph_ion_unmap;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_WGT_KALLOC))
		goto wgt_kfree;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_IOFM_KALLOC))
		goto iofm_kfree;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_IOFM_ION_ALLOC))
		goto iofm_ion_unmap;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC))
		goto imb_ion_unmap;

imb_ion_unmap:
	addr_info = session->IMB_info;
	session->IMB_info = NULL;
	if (likely(addr_info))
		kfree(addr_info);

	ion_mem_buf = session->IMB_mem_buf;
	session->IMB_mem_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

iofm_ion_unmap:
	ion_mem_buf = session->IOFM_mem_buf;
	session->IOFM_mem_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

iofm_kfree:
	for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
		for (i = 0; i < VISION_MAX_BUFFER; i++) {
			addr_info = session->IFM_info[k][i].addr_info;
			session->IFM_info[k][i].addr_info = NULL;
			if (likely(addr_info))
				kfree(addr_info);

			addr_info = session->OFM_info[k][i].addr_info;
			session->OFM_info[k][i].addr_info = NULL;
			if (likely(addr_info))
				kfree(addr_info);
		}
	}

wgt_kfree:
	addr_info = session->WGT_info;
	session->WGT_info = NULL;
	if (likely(addr_info))
		kfree(addr_info);

graph_ion_unmap:
	ion_mem_buf = session->ncp_mem_buf;
	session->ncp_mem_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_unmap(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->ncp_hdr_buf;
	session->ncp_hdr_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	return 0;
}

int _undo_s_format_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_IMB_ION_ALLOC))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_FORMAT_OT)) {
		goto init_format;
	}

init_format:

p_err:
	return ret;
}

int _undo_streamon_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_FORMAT_OT))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_START))
		goto release_streamon;

release_streamon:

p_err:
	return ret;
}

int _undo_streamoff_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_START))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_STOP))
		goto release_streamoff;

release_streamoff:

p_err:
	return ret;
}

int _undo_close_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_STOP))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_CLOSE))
		goto session_close;

session_close:

p_err:
	return ret;
}

bool EVER_FIND_FM(u32 *FM_cnt, struct addr_info *FM_av, u32 address_vector_index)
{
	bool ret = false;
	u32 i;

	for (i = 0; i < (*FM_cnt) ; i++) {
		if ((FM_av + i)->av_index == address_vector_index) {
			ret = true;
			break;
		}
	}
	return ret;
}

void __set_unique_id(struct npu_session *session, struct drv_usr_share *usr_data)
{
	u32 uid = session->uid;
	usr_data->id = uid;
}

int __get_ncp_bin_size(struct npu_binary *binary, char *ncp_path, char *ncp_name, size_t *ncp_size)
{
	int ret = 0;
	struct device dev_obj = {};
	struct device *dev = &dev_obj;

	ret = npu_binary_init(binary, dev, ncp_path, NCP_BIN_PATH, ncp_name);
	if (ret)
		goto p_err;

	ret = npu_binary_g_size(binary, ncp_size);
	if (ret)
		goto p_err;

	return ret;
p_err:
	npu_err("fail in __get_ncp_bin_size %d\n", ret);
	return ret;
}

int __update_ncp_info(struct npu_session *session, struct npu_memory_buffer *ncp_mem_buf)
{
	if (!session) {
		npu_err("invalid session or ncp_mem_buf\n");
		return -EINVAL;
	}

	session->ncp_info.ncp_addr.size = ncp_mem_buf->size;
	session->ncp_info.ncp_addr.vaddr = ncp_mem_buf->vaddr;
	session->ncp_info.ncp_addr.daddr = ncp_mem_buf->daddr;
	return 0;
}

static struct npu_memory_buffer *
copy_ncp_header(struct npu_session *session, struct npu_memory_buffer *ncp_mem_buf)
{
	struct npu_memory_buffer *buffer;
	struct ncp_header *src_hdr = ncp_mem_buf->vaddr;

	buffer = npu_memory_copy(session->memory, ncp_mem_buf, 0, src_hdr->hdr_size);
	if (IS_ERR_OR_NULL(buffer))
		return buffer;

	add_ion_mem(session, buffer, NCP_HDR_TYPE);

	return buffer;
}

int __ncp_ion_map(struct npu_session *session, struct drv_usr_share *usr_data)
{
	int ret = 0;

	struct npu_memory_buffer *ncp_mem_buf = NULL, *ncp_hdr_buf = NULL;
	mem_opt_e opt = NCP_TYPE;

	ncp_mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (ncp_mem_buf == NULL) {
		npu_err("fail in npu_ion_map kzalloc\n");
		return -ENOMEM;
	}
	ncp_mem_buf->fd = usr_data->ncp_fd;
	ncp_mem_buf->size = usr_data->ncp_size;

	ret = npu_memory_map(session->memory, ncp_mem_buf, configs[NPU_PBHA_HINT_11]);
	if (unlikely(ret)) {
		npu_err("npu_memory_map is fail(%d).\n", ret);
		goto err_free;
	}
	npu_info("ncp_ion_map(0x%pad), vaddr(0x%pK)\n", &ncp_mem_buf->daddr, ncp_mem_buf->vaddr);
	ret = __update_ncp_info(session, ncp_mem_buf);
	if (ret) {
		npu_err("__ncp_ion_map is fail(%d).\n", ret);
		goto err_unmap;
	}
	ret = add_ion_mem(session, ncp_mem_buf, opt);
	if (ret) {
		npu_err("__ncp_ion_map is fail(%d).\n", ret);
		goto err_unmap;
	}

	ncp_hdr_buf = copy_ncp_header(session, ncp_mem_buf);
	if (IS_ERR_OR_NULL(ncp_hdr_buf))
		return PTR_ERR(ncp_hdr_buf); /* free buffer from __release_graph_ion */

	ret = npu_util_validate_user_ncp(session, ncp_hdr_buf->vaddr, ncp_mem_buf->size);
	if (ret)
		return ret; /* free buffer from __release_graph_ion */

	return ret;
err_unmap:
	npu_memory_unmap(session->memory, ncp_mem_buf);
err_free:
	kfree(ncp_mem_buf);
	return ret;
}

int __get_session_info(struct npu_session *session, struct vs4l_graph *info)
{
	int ret = 0;
	struct drv_usr_share *usr_data = NULL;
	usr_data = kzalloc(sizeof(struct drv_usr_share), GFP_KERNEL);
	if (unlikely(!usr_data)) {
		npu_err("Not enough Memory\n");
		ret = -ENOMEM;
		return ret;
	}
	ret = copy_from_user((void *)usr_data, (void *)info->addr, sizeof(struct drv_usr_share));
	if (ret) {
		npu_err("copy_from_user failed(%d)\n", ret);
		goto p_err;
	}
	__set_unique_id(session, usr_data);
	npu_utrace("usr_data(0x%pK), ncp_size(%u)\n", session, usr_data, usr_data->ncp_size);
	ret = copy_to_user((void *)info->addr, (void *)usr_data, sizeof(struct drv_usr_share));
	if (ret) {
		npu_err("copy_to_user failed(%d)\n", ret);
		goto p_err;
	}
	ret = __ncp_ion_map(session, usr_data);
	if (unlikely(ret)) {
		npu_uerr("__ncp_ion_map is fail(%d)\n", session, ret);
		goto p_err;
	}
	session->ss_state |= BIT(NPU_SESSION_STATE_GRAPH_ION_MAP);
p_err:
	if (usr_data)
		kfree(usr_data);
	return ret;
}

int __pilot_parsing_ncp(struct npu_session *session, u32 *IFM_cnt, u32 *OFM_cnt, u32 *IMB_cnt, u32 *WGT_cnt)
{
	int ret = 0;
	u32 i = 0;
	char *ncp_vaddr;
	u32 memory_vector_cnt;
	u32 memory_vector_offset;
	struct memory_vector *mv;
	struct ncp_header *ncp;

	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	ncp = (struct ncp_header *)ncp_vaddr;
	memory_vector_offset = ncp->memory_vector_offset;
	if (unlikely(memory_vector_offset > session->ncp_mem_buf->size)) {
		npu_err("memory vector offset(0x%x) > max size(0x%x) ;out of bounds\n",
				(u32)memory_vector_offset, (u32)session->ncp_mem_buf->size);
		return -EFAULT;
	}

	memory_vector_cnt = ncp->memory_vector_cnt;
	if (unlikely(((memory_vector_cnt * sizeof(struct memory_vector)) + memory_vector_offset) >  session->ncp_mem_buf->size)) {
		npu_err("memory vector count(0x%x) seems abnormal ;out of bounds\n", memory_vector_cnt);
		return -EFAULT;
	}
	session->memory_vector_offset = memory_vector_offset;
	session->memory_vector_cnt = memory_vector_cnt;

	mv = (struct memory_vector *)(ncp_vaddr + memory_vector_offset);

	for (i = 0; i < memory_vector_cnt; i++) {
		u32 memory_type = (mv + i)->type;
		switch (memory_type) {
		case MEMORY_TYPE_IN_FMAP:
			{
				(*IFM_cnt)++;
				break;
			}
		case MEMORY_TYPE_OT_FMAP:
			{
				(*OFM_cnt)++;
				break;
			}
		case MEMORY_TYPE_IM_FMAP:
			{
				(*IMB_cnt)++;
				break;
			}
		case MEMORY_TYPE_CUCODE:
		case MEMORY_TYPE_WEIGHT:
		case MEMORY_TYPE_WMASK:
			{
				(*WGT_cnt)++;
				break;
			}
		}
	}
	return ret;
}

int __second_parsing_ncp(
	struct npu_session *session,
	struct addr_info **IFM_av, struct addr_info **OFM_av,
	struct addr_info **IMB_av, struct addr_info **WGT_av)
{
	int ret = 0;
	u32 i = 0;
	u32 weight_size;

	u32 IFM_cnt = 0;
	u32 OFM_cnt = 0;
	u32 IMB_cnt = 0;
	u32 WGT_cnt = 0;

	u32 address_vector_offset;
	u32 address_vector_cnt;
	u32 memory_vector_offset;
	u32 memory_vector_cnt;

	struct ncp_header *ncp;
	struct address_vector *av;
	struct memory_vector *mv;

	char *ncp_vaddr;
	dma_addr_t ncp_daddr;

	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	ncp = (struct ncp_header *)ncp_vaddr;

	address_vector_offset = ncp->address_vector_offset;
	if (unlikely(address_vector_offset > session->ncp_mem_buf->size)) {
		npu_err("address vector offset(0x%x) > max size(0x%x) ;out of bounds\n",
				address_vector_offset, (u32)session->ncp_mem_buf->size);
		return -EFAULT;
	}

	address_vector_cnt = ncp->address_vector_cnt;
	if (unlikely(((address_vector_cnt * sizeof(struct address_vector)) + address_vector_offset) >
									session->ncp_mem_buf->size)) {
		npu_err("address vector count(0x%x) seems abnormal ;out of bounds\n", address_vector_cnt);
		return -EFAULT;
	}

	session->address_vector_offset = address_vector_offset;
	session->address_vector_cnt = address_vector_cnt;

	session->ncp_info.address_vector_cnt = address_vector_cnt;

	memory_vector_offset = session->memory_vector_offset;
	memory_vector_cnt = session->memory_vector_cnt;

	if (address_vector_cnt > memory_vector_cnt) {
		npu_err("address_vector_cnt(%d) should not exceed memory_vector_cnt(%d)\n",
				address_vector_cnt, memory_vector_cnt);
		return -EFAULT;
	}

	mv = (struct memory_vector *)(ncp_vaddr + memory_vector_offset);
	av = (struct address_vector *)(ncp_vaddr + address_vector_offset);

	for (i = 0; i < memory_vector_cnt; i++) {
		u32 memory_type = (mv + i)->type;
		u32 address_vector_index;
		u32 weight_offset;

		switch (memory_type) {
		case MEMORY_TYPE_IN_FMAP:
			{
				if (IFM_cnt >= session->IFM_cnt) {
					npu_err("IFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							IFM_cnt, session->IFM_cnt);
					return -EFAULT;
				}
				address_vector_index = (mv + i)->address_vector_index;
				if (likely(!EVER_FIND_FM(&IFM_cnt, *IFM_av, address_vector_index))) {
					(*IFM_av + IFM_cnt)->av_index = address_vector_index;
					if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
						npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
								address_vector_index, address_vector_cnt);
						return -EFAULT;
					}
					(*IFM_av + IFM_cnt)->size = (av + address_vector_index)->size;
					(*IFM_av + IFM_cnt)->pixel_format = (mv + i)->pixel_format;
					(*IFM_av + IFM_cnt)->width = (mv + i)->width;
					(*IFM_av + IFM_cnt)->height = (mv + i)->height;
					(*IFM_av + IFM_cnt)->channels = (mv + i)->channels;
					(mv + i)->wstride = 0;
					(*IFM_av + IFM_cnt)->stride = (mv + i)->wstride;
					npu_uinfo("(IFM_av + %u)->index = %u\n", session,
						IFM_cnt, (*IFM_av + IFM_cnt)->av_index);
					npu_utrace("[session] IFM, index(%u)\n", session, (*IFM_av + IFM_cnt)->av_index);
					npu_utrace("[session] IFM, size(%zu)\n", session, (*IFM_av + IFM_cnt)->size);
					npu_utrace("[session] IFM, pixel_format(%u)\n", session, (*IFM_av + IFM_cnt)->pixel_format);
					npu_utrace("[session] IFM, width(%u)\n", session, (*IFM_av + IFM_cnt)->width);
					npu_utrace("[session] IFM, height(%u)\n", session, (*IFM_av + IFM_cnt)->height);
					npu_utrace("[session] IFM, channels(%u)\n", session, (*IFM_av + IFM_cnt)->channels);
					npu_utrace("[session] IFM, wstride(%u)\n", session, (*IFM_av + IFM_cnt)->stride);

					IFM_cnt++;
				}
				break;
			}
		case MEMORY_TYPE_OT_FMAP:
			{
				if (OFM_cnt >= session->OFM_cnt) {
					npu_err("OFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							OFM_cnt, session->OFM_cnt);
					return -EFAULT;
				}
				address_vector_index = (mv + i)->address_vector_index;
				if (likely(!EVER_FIND_FM(&OFM_cnt, *OFM_av, address_vector_index))) {
					(*OFM_av + OFM_cnt)->av_index = address_vector_index;
					if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
						npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
								address_vector_index, address_vector_cnt);
						return -EFAULT;
					}
					(*OFM_av + OFM_cnt)->size = (av + address_vector_index)->size;
					(*OFM_av + OFM_cnt)->pixel_format = (mv + i)->pixel_format;
					(*OFM_av + OFM_cnt)->width = (mv + i)->width;
					(*OFM_av + OFM_cnt)->height = (mv + i)->height;
					(*OFM_av + OFM_cnt)->channels = (mv + i)->channels;
					(mv + i)->wstride = 0;
					(*OFM_av + OFM_cnt)->stride = (mv + i)->wstride;
					npu_uinfo("(OFM_av + %u)->index = %u\n", session, OFM_cnt, (*OFM_av + OFM_cnt)->av_index);
					npu_utrace("OFM, index(%d)\n", session, (*OFM_av + OFM_cnt)->av_index);
					npu_utrace("[session] OFM, size(%zu)\n", session, (*OFM_av + OFM_cnt)->size);
					npu_utrace("[session] OFM, pixel_format(%u)\n", session, (*OFM_av + OFM_cnt)->pixel_format);
					npu_utrace("[session] OFM, width(%u)\n", session, (*OFM_av + OFM_cnt)->width);
					npu_utrace("[session] OFM, height(%u)\n", session, (*OFM_av + OFM_cnt)->height);
					npu_utrace("[session] OFM, channels(%u)\n", session, (*OFM_av + OFM_cnt)->channels);
					npu_utrace("[session] OFM, wstride(%u)\n", session, (*OFM_av + OFM_cnt)->stride);
					OFM_cnt++;
				}
				break;
			}
		case MEMORY_TYPE_IM_FMAP:
			{
				if (IMB_cnt >= session->IMB_cnt) {
					npu_err("IMB_cnt(%d) should not exceed size of allocated array(%d)\n",
							IMB_cnt, session->IMB_cnt);
					return -EFAULT;
				}
				address_vector_index = (mv + i)->address_vector_index;
				if (likely(!EVER_FIND_FM(&IMB_cnt, *IMB_av, address_vector_index))) {
					(*IMB_av + IMB_cnt)->av_index = address_vector_index;
					if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
						npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
								address_vector_index, address_vector_cnt);
						return -EFAULT;
					}
					(*IMB_av + IMB_cnt)->size = (av + address_vector_index)->size;
					(*IMB_av + IMB_cnt)->pixel_format = (mv + i)->pixel_format;
					(*IMB_av + IMB_cnt)->width = (mv + i)->width;
					(*IMB_av + IMB_cnt)->height = (mv + i)->height;
					(*IMB_av + IMB_cnt)->channels = (mv + i)->channels;
					(mv + i)->wstride = 0;
					(*IMB_av + IMB_cnt)->stride = (mv + i)->wstride;
					//npu_info("(*temp_IMB_av + %ld)->index = 0x%x\n",
					//		IMB_cnt, (*temp_IMB_av + IMB_cnt)->index);
					//npu_info("(*temp_IMB_av + %ld)->size = 0x%x\n",
					//		IMB_cnt, (*temp_IMB_av + IMB_cnt)->size);
					IMB_cnt++;
				}
				break;
			}
		case MEMORY_TYPE_CUCODE:
		case MEMORY_TYPE_WEIGHT:
		case MEMORY_TYPE_WMASK:
			{
				ncp_daddr = session->ncp_mem_buf->daddr;
				ncp_vaddr = (char *)session->ncp_mem_buf->vaddr;

				if (WGT_cnt >= session->WGT_cnt) {
					npu_err("WGT_cnt(%d) should not exceed size of allocated array(%d)\n",
							WGT_cnt, session->WGT_cnt);
					return -EFAULT;
				}
				// update address vector, m_addr with ncp_alloc_daddr + offset
				address_vector_index = (mv + i)->address_vector_index;
				if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
					npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
							address_vector_index, address_vector_cnt);
					return -EFAULT;
				}
				weight_offset = (av + address_vector_index)->m_addr;
				if (unlikely(weight_offset > (u32)session->ncp_mem_buf->size)) {
					ret = -EINVAL;
					npu_uerr("weight_offset is invalid, offset(0x%x), ncp_daddr(0x%x)\n",
						session, (u32)weight_offset, (u32)session->ncp_mem_buf->size);
					goto p_err;
				}
				(av + address_vector_index)->m_addr = weight_offset + ncp_daddr;

				(*WGT_av + WGT_cnt)->av_index = address_vector_index;
				weight_size = (av + address_vector_index)->size;
				if (unlikely((weight_offset + weight_size) < weight_size)) {
					npu_err("weight_offset(0x%x) + weight size (0x%x) seems to be overflow.\n",
						weight_offset, weight_size);
					return -ERANGE;
				}
				if (unlikely((weight_offset + weight_size) > (u32)session->ncp_mem_buf->size)) {
					npu_err("weight_offset(0x%x) + weight size (0x%x) seems to go beyond ncp size(0x%x)\n",
							weight_offset, weight_size, (u32)session->ncp_mem_buf->size);
					return -EFAULT;
				}

				(*WGT_av + WGT_cnt)->size = weight_size;
				(*WGT_av + WGT_cnt)->daddr = weight_offset + ncp_daddr;
				(*WGT_av + WGT_cnt)->vaddr = weight_offset + ncp_vaddr;
				(*WGT_av + WGT_cnt)->memory_type = memory_type;
				npu_utrace("(*WGT_av + %u)->av_index = %u\n", session,	WGT_cnt, (*WGT_av + WGT_cnt)->av_index);
				npu_utrace("(*WGT_av + %u)->size = %zu\n", session, WGT_cnt, (*WGT_av + WGT_cnt)->size);
				npu_utrace("(*WGT_av + %u)->daddr = 0x%pad\n", session, WGT_cnt, &((*WGT_av + WGT_cnt)->daddr));
				npu_utrace("(*WGT_av + %u)->vaddr = 0x%pK\n", session, WGT_cnt, (*WGT_av + WGT_cnt)->vaddr);
				WGT_cnt++;
				break;
			}
		default:
			break;
		}
	}
	session->IOFM_cnt = IFM_cnt + OFM_cnt;
	return ret;
p_err:
	return ret;
}


int __make_IFM_info(struct npu_session *session, struct addr_info **IFM_av)
{
	int ret = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	u32 IFM_cnt = session->IFM_cnt;
	struct addr_info *IFM_addr;

	for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
		for (i = 0; i < VISION_MAX_BUFFER; i++) {
			IFM_addr = kcalloc(IFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
			if (unlikely(!IFM_addr)) {
				npu_err("failed in __make_IFM_info(ENOMEM)\n");
				ret = -ENOMEM;
				goto p_err;
			}
			session->IFM_info[k][i].addr_info = IFM_addr;
			for (j = 0; j < IFM_cnt; j++) {
			(IFM_addr + j)->av_index = (*IFM_av + j)->av_index;
			(IFM_addr + j)->size = (*IFM_av + j)->size;
			(IFM_addr + j)->pixel_format = (*IFM_av + j)->pixel_format;
			(IFM_addr + j)->width = (*IFM_av + j)->width;
			(IFM_addr + j)->height = (*IFM_av + j)->height;
			(IFM_addr + j)->channels = (*IFM_av + j)->channels;
			(IFM_addr + j)->stride = (*IFM_av + j)->stride;
			}
		}
	}

p_err:
	return ret;
}

int __make_OFM_info(struct npu_session *session, struct addr_info **OFM_av)
{
	int ret = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	u32 OFM_cnt = session->OFM_cnt;
	struct addr_info *OFM_addr;

	for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
		for (i = 0; i < VISION_MAX_BUFFER; i++) {
			OFM_addr = kcalloc(OFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
			if (unlikely(!OFM_addr)) {
				npu_err("failed in __make_OFM_info(ENOMEM)\n");
				ret = -ENOMEM;
				goto p_err;
			}
			session->OFM_info[k][i].addr_info = OFM_addr;
			for (j = 0; j < OFM_cnt; j++) {
			(OFM_addr + j)->av_index = (*OFM_av + j)->av_index;
			(OFM_addr + j)->size = (*OFM_av + j)->size;
			(OFM_addr + j)->pixel_format = (*OFM_av + j)->pixel_format;
			(OFM_addr + j)->width = (*OFM_av + j)->width;
			(OFM_addr + j)->height = (*OFM_av + j)->height;
			(OFM_addr + j)->channels = (*OFM_av + j)->channels;
			(OFM_addr + j)->stride = (*OFM_av + j)->stride;
			}
		}
	}

p_err:
	return ret;
}

int __ion_alloc_IOFM(struct npu_session *session, struct addr_info **IFM_av, struct addr_info **OFM_av)
{
	int ret = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct npu_memory_buffer *IOFM_mem_buf;
	mem_opt_e opt = IOFM_TYPE;
	struct address_vector *vaddr;

	u32 IFM_cnt = session->IFM_cnt;
	u32 IOFM_cnt = session->IOFM_cnt;

	IOFM_mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (unlikely(!IOFM_mem_buf)) {
		npu_err("failed in __ion_alloc_IOFM(ENOMEM)\n");
		ret = -ENOMEM;
		return ret;
	}
	npu_udbg("ion alloc IOFM(0x%pK)\n", session, IOFM_mem_buf);

	IOFM_mem_buf->size = VISION_MAX_CONTAINERLIST * VISION_MAX_BUFFER * IOFM_cnt * sizeof(struct address_vector);
	ret = npu_memory_alloc(session->memory, IOFM_mem_buf, configs[NPU_PBHA_HINT_00]);
	if (unlikely(ret)) {
		npu_uerr("npu_memory_alloc is fail(%d).\n", session, ret);
		goto p_err;
	}
	npu_udbg(" (IOFM_mem_buf + %d)->vaddr(0x%pK), daddr(0x%pad), size(%zu)\n",
		session, 0, (IOFM_mem_buf + 0)->vaddr, &(IOFM_mem_buf + 0)->daddr, (IOFM_mem_buf + 0)->size);

	vaddr = IOFM_mem_buf->vaddr;
	for (i = 0; i < IOFM_cnt; i++) {
		for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
			for (j = 0; j < VISION_MAX_BUFFER; j++) {
				if (i < IFM_cnt) {
					(vaddr + (k*VISION_MAX_BUFFER + j)*IOFM_cnt + i)->index = (*IFM_av + i)->av_index;
					(vaddr + (k*VISION_MAX_BUFFER + j)*IOFM_cnt + i)->size = (*IFM_av + i)->size;
				} else {
					(vaddr + (k*VISION_MAX_BUFFER + j)*IOFM_cnt + i)->index = (*OFM_av + (i - IFM_cnt))->av_index;
					(vaddr + (k*VISION_MAX_BUFFER + j)*IOFM_cnt + i)->size = (*OFM_av + (i - IFM_cnt))->size;
				}
			}
		}
	}
	ret = add_ion_mem(session, IOFM_mem_buf, opt);
	return ret;
p_err:
	npu_memory_free(session->memory, IOFM_mem_buf);
	if (IOFM_mem_buf)
		kfree(IOFM_mem_buf);
	return ret;
}

int __make_IMB_info(struct npu_session *session, struct npu_memory_buffer *IMB_mem_buf, struct addr_info **temp_IMB_av)
{
	int ret = 0;
	u32 i = 0;
	u32 IMB_cnt = session->IMB_cnt;
	struct addr_info *IMB_info;

	IMB_info = kcalloc(IMB_cnt, sizeof(struct addr_info), GFP_KERNEL);
	if (unlikely(!IMB_info)) {
		npu_err("failed in __make_IMB_info(ENOMEM)\n");
		ret = -ENOMEM;
		return ret;
	}

	session->IMB_info = IMB_info;

	for (i = 0; i < IMB_cnt; i++) {
		(IMB_info + i)->av_index = (*temp_IMB_av + i)->av_index;
		(IMB_info + i)->vaddr = (session->imb_ion_info + i)->vaddr;
		(IMB_info + i)->daddr = (session->imb_ion_info + i)->daddr;
		(IMB_info + i)->size = (session->imb_ion_info + i)->size;
		(IMB_info + i)->pixel_format = (*temp_IMB_av + i)->pixel_format;
		(IMB_info + i)->width = (*temp_IMB_av + i)->width;
		(IMB_info + i)->height = (*temp_IMB_av + i)->height;
		(IMB_info + i)->channels = (*temp_IMB_av + i)->channels;
		(IMB_info + i)->stride = (*temp_IMB_av + i)->stride;
	}
	return ret;
}


int __ion_alloc_IMB(struct npu_session *session, struct addr_info **IMB_av)
{
	int ret = 0;
	u32 i = 0;
	u32 address_vector_offset;
	struct address_vector *av;
	char *ncp_vaddr;
	u32 IMB_cnt = session->IMB_cnt;
	struct npu_memory_buffer *IMB_mem_buf;
	mem_opt_e opt = IMB_TYPE;
	u32 addr_offset = 0;

	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	address_vector_offset = session->address_vector_offset;

	av = (struct address_vector *)(ncp_vaddr + address_vector_offset);

	IMB_mem_buf = kcalloc(1, sizeof(struct npu_memory_buffer), GFP_KERNEL);

	if (unlikely(!IMB_mem_buf)) {
		npu_err("failed in __make_IMB_info(ENOMEM)\n");
		ret = -ENOMEM;
		return ret;
	}

	if (likely(session->imb_ion_info == NULL)) {
		session->imb_ion_info = kcalloc(IMB_cnt, sizeof(struct ion_info), GFP_KERNEL);
		if (unlikely(!session->imb_ion_info)) {
			npu_err("failed in __make_IMB_info(ENOMEM)\n");
			ret = -ENOMEM;
			if (IMB_mem_buf)
				kfree(IMB_mem_buf);
			return ret;
		}
	}

	for (i = 0; i < IMB_cnt; i++) {
		IMB_mem_buf->size += ALIGN((*IMB_av + i)->size, NPU_IMB_ALIGN);
	}

	ret = npu_memory_alloc(session->memory, IMB_mem_buf, configs[NPU_PBHA_HINT_00]);
	if (unlikely(ret)) {
		npu_uerr("npu_memory_alloc is fail(%d).\n", session, ret);
		goto p_err;
	}

	for (i = 0; i < IMB_cnt; i++) {
		(av + (*IMB_av + i)->av_index)->m_addr = IMB_mem_buf->daddr + addr_offset;
		(session->imb_ion_info + i)->daddr = IMB_mem_buf->daddr + addr_offset;
		(session->imb_ion_info + i)->vaddr = ((void *)((char *)(IMB_mem_buf->vaddr)) + addr_offset);
		(session->imb_ion_info + i)->size = (*IMB_av + i)->size;
		addr_offset += ALIGN((u32)(*IMB_av + i)->size, (u32)NPU_IMB_ALIGN);

		npu_udbg("(IMB_mem_buf + %d)->vaddr(0x%pK), daddr(0x%pad), size(%zu)\n",
			session, i, (session->imb_ion_info + i)->vaddr, &(session->imb_ion_info + i)->daddr, (session->imb_ion_info + i)->size);
	}

	ret = add_ion_mem(session, IMB_mem_buf, opt);
	ret = __make_IMB_info(session, IMB_mem_buf, IMB_av);

	if (likely(session->imb_ion_info != NULL)) {
		kfree(session->imb_ion_info);
		session->imb_ion_info = NULL;
	}

	return ret;
p_err:
	npu_memory_free(session->memory, IMB_mem_buf);
	if (IMB_mem_buf)
		kfree(IMB_mem_buf);
	if (likely(session->imb_ion_info != NULL)) {
		kfree(session->imb_ion_info);
		session->imb_ion_info = NULL;
	}
	return ret;
}

void npu_session_ion_sync_for_device(struct npu_memory_buffer *pbuf, off_t offset, size_t size, enum dma_data_direction dir)
{
	if (likely(pbuf->vaddr)) {
		BUG_ON((offset < 0) || (offset > pbuf->size));
		BUG_ON((offset + size) < size);
		BUG_ON((size > pbuf->size) || ((offset + size) > pbuf->size));

		//__dma_map_area(pbuf->vaddr + offset, size, dir);
		if (dir == DMA_TO_DEVICE)
			dma_sync_sg_for_device(pbuf->attachment->dev,
				pbuf->sgt->sgl, pbuf->sgt->orig_nents, DMA_TO_DEVICE);
		else // dir == DMA_FROM_DEVICE
			dma_sync_sg_for_cpu(pbuf->attachment->dev,
				pbuf->sgt->sgl, pbuf->sgt->orig_nents, DMA_FROM_DEVICE);
	}
}

int __config_session_info(struct npu_session *session)
{
	int ret = 0;

	struct addr_info *IFM_av = NULL;
	struct addr_info *OFM_av = NULL;
	struct addr_info *IMB_av = NULL;
	struct addr_info *WGT_av = NULL;

	struct npu_memory_buffer *IMB_mem_buf;

	ret = __pilot_parsing_ncp(session, &session->IFM_cnt, &session->OFM_cnt, &session->IMB_cnt, &session->WGT_cnt);
	if (ret) {
		npu_err("failed in __pilot_parsing_ncp\n");
		goto p_err;
	}

	IFM_av = kcalloc(session->IFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	OFM_av = kcalloc(session->OFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	IMB_av = kcalloc(session->IMB_cnt, sizeof(struct addr_info), GFP_KERNEL);
	WGT_av = kcalloc(session->WGT_cnt, sizeof(struct addr_info), GFP_KERNEL);

	session->WGT_info = WGT_av;
	session->ss_state |= BIT(NPU_SESSION_STATE_WGT_KALLOC);

	if (unlikely(!IFM_av || !OFM_av || !IMB_av || !WGT_av)) {
		npu_err("failed in __config_session_info(ENOMEM)\n", session);
		ret = -ENOMEM;
		goto p_err;
	}

	ret = __second_parsing_ncp(session, &IFM_av, &OFM_av, &IMB_av, &WGT_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in second_parsing_ncp\n", session, ret);
		goto p_err;
	}

	ret = __make_IFM_info(session, &IFM_av);
	ret = __make_OFM_info(session, &OFM_av);
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_KALLOC);

	ret = __ion_alloc_IOFM(session, &IFM_av, &OFM_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in __ion_alloc_IOFM\n", session, ret);
		goto p_err;
	}
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_ION_ALLOC);

	ret = __ion_alloc_IMB(session, &IMB_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in __ion_alloc_IMB\n", session, ret);
		goto p_err;
	}
	session->ss_state |= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC);

	npu_session_ion_sync_for_device(session->ncp_mem_buf, 0, session->ncp_mem_buf->size, DMA_TO_DEVICE);

	IMB_mem_buf = session->IMB_mem_buf;
	npu_session_ion_sync_for_device(IMB_mem_buf, 0, IMB_mem_buf->size, DMA_TO_DEVICE);

p_err:
	if (likely(IFM_av))
		kfree(IFM_av);
	if (likely(OFM_av))
		kfree(OFM_av);
	if (likely(IMB_av))
		kfree(IMB_av);

	return ret;
}

#ifdef CONFIG_NPU_ARBITRATION
int __arbitration_session_info(struct npu_session *session)
{
	int ret = 0;
	struct npu_sessionmgr *sessionmgr;
	struct ncp_header *ncp;
	struct group_vector *gcurr;
	u32 isa_size;
	int count;

	sessionmgr = session->cookie;
	ncp = (struct ncp_header *)session->ncp_mem_buf->vaddr;
	mutex_lock(&sessionmgr->mlock);
	session->total_flc_transfer_size = ncp->total_flc_transfer_size;
	session->total_sdma_transfer_size = ncp->total_sdma_transfer_size;
	sessionmgr->cumulative_flc_size += session->total_flc_transfer_size;
	sessionmgr->cumulative_sdma_size += session->total_sdma_transfer_size;

	if (ncp->group_vector_offset < session->ncp_mem_buf->size) {
		gcurr = (struct group_vector *)((u64)ncp + ncp->group_vector_offset);
	} else {
		npu_err("memory vector offset(0x%x) > max size(0x%x), out of bounds\n",
						(u32)ncp->group_vector_offset, (u32)session->ncp_mem_buf->size);
		return -EFAULT;
	}

	isa_size = 0;
	for (count = 0; count < ncp->group_vector_cnt; ++count, ++gcurr)
		isa_size += gcurr->isa_size;

	session->cmdq_isa_size = isa_size;

	if (ncp->thread_vector_cnt > (NPU_MAX_CORES_ALLOWED-1)) {
		npu_err("please change the macro NPU_MAX_CORES_ALLOWED to (%d + 1)\n", ncp->thread_vector_cnt);
		return -EINVAL;
	}
	sessionmgr->count_thread_ncp[ncp->thread_vector_cnt]++;
	session->inferencefreq_index = 0;
	session->last_q_time_stamp = npu_get_time_us();
	for (count = 0; count < NPU_Q_TIMEDIFF_WIN_MAX; count++)
		session->inferencefreq_window[count] = 0;
	session->inferencefreq = 0;
	npu_trace("npu_session_start: %d %d %d %d %d %d", session->inferencefreq_window[0],
						session->inferencefreq_window[1],
						session->inferencefreq_window[2],
						session->inferencefreq_window[3],
						session->inferencefreq_window[4],
						session->inferencefreq
					);
	mutex_unlock(&sessionmgr->mlock);

	return ret;
}

void npu_arbitration_session_stop(struct npu_session *session)
{
	struct npu_sessionmgr *sessionmgr;
	struct ncp_header *ncp;
	int count;

	sessionmgr = session->cookie;
	ncp = (struct ncp_header *)session->ncp_mem_buf->vaddr;
	mutex_lock(&sessionmgr->mlock);
	sessionmgr->cumulative_flc_size -= session->total_flc_transfer_size;
	sessionmgr->cumulative_sdma_size -= session->total_sdma_transfer_size;
	sessionmgr->count_thread_ncp[ncp->thread_vector_cnt]--;
	session->inferencefreq_index = 0;
	for (count = 0; count < NPU_Q_TIMEDIFF_WIN_MAX; count++)
		session->inferencefreq_window[count] = 0;
	session->inferencefreq = 0;
	npu_trace("npu_session_stop: %d %d %d %d %d %d", session->inferencefreq_window[0],
						session->inferencefreq_window[1],
						session->inferencefreq_window[2],
						session->inferencefreq_window[3],
						session->inferencefreq_window[4],
						session->inferencefreq
					);
	mutex_unlock(&sessionmgr->mlock);
}

void npu_arbitration_info_update(struct npu_session *session)
{
	u64 now, maxvalue = -1;
	u32 temp;

	now = npu_get_time_us();

	temp = session->inferencefreq * NPU_Q_TIMEDIFF_WIN_MAX - session->inferencefreq_window[session->inferencefreq_index];
	if (now > session->last_q_time_stamp)
		session->inferencefreq_window[session->inferencefreq_index] = (u32)(1000000/(now - session->last_q_time_stamp));
	else
		session->inferencefreq_window[session->inferencefreq_index] = (u32)(1000000/( (maxvalue - session->last_q_time_stamp) + now));

	session->inferencefreq = (temp + session->inferencefreq_window[session->inferencefreq_index])/NPU_Q_TIMEDIFF_WIN_MAX;
	session->last_q_time_stamp = now;

	session->inferencefreq_index = (session->inferencefreq_index + 1) % NPU_Q_TIMEDIFF_WIN_MAX;
	npu_dbg("infFreq=%d addrofInfFreq = 0x%llx window = 0x%x 0x%x 0x%x 0x%x 0x%x %llx",
			session->inferencefreq, &session->inferencefreq,
			session->inferencefreq_window[0],
			session->inferencefreq_window[1],
			session->inferencefreq_window[2],
			session->inferencefreq_window[3],
			session->inferencefreq_window[4],
			session->last_q_time_stamp);
}

#else /* !CONFIG_NPU_ARBITRATION */

static inline int __arbitration_session_info(struct npu_session *session) { return 0; }
static inline void npu_arbitration_session_stop(struct npu_session *session) { }
static inline void npu_arbitration_info_update(struct npu_session *session) { }

#endif /* CONFIG_NPU_ARBITRATION */


int npu_session_s_graph(struct npu_session *session, struct vs4l_graph *info)
{
	int ret = 0;
	BUG_ON(!session);
	BUG_ON(!info);
	ret = __get_session_info(session, info);
	if (unlikely(ret)) {
		npu_uerr("invalid in __get_session_info\n", session);
		goto p_err;
	}
	ret = __config_session_info(session);
	if (unlikely(ret)) {
		npu_uerr("invalid in __config_session_info\n", session);
		goto p_err;
	}
	ret = __arbitration_session_info(session);
	if (unlikely(ret)) {
		npu_uerr("invalid in __arbitration_session_info\n", session);
		goto p_err;
	}
	return ret;
p_err:
	npu_uerr("Clean-up buffers for graph\n", session);
	__release_graph_ion(session);
	return ret;
}

int npu_session_start(struct npu_queue *queue)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	BUG_ON(!queue);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	BUG_ON(!vctx);
	BUG_ON(!session);

	session->ss_state |= BIT(NPU_SESSION_STATE_START);
	return ret;
}

int npu_session_NW_CMD_STREAMON(struct npu_session *session)
{
	nw_cmd_e nw_cmd = NPU_NW_CMD_STREAMON;

	BUG_ON(!session);

	profile_point1(PROBE_ID_DD_NW_RECEIVED, session->uid, 0, nw_cmd);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);

	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
	profile_point1(PROBE_ID_DD_NW_NOTIFIED, session->uid, 0, nw_cmd);

	return 0;
}

int npu_session_streamoff(struct npu_queue *queue)
{
	BUG_ON(!queue);
	return 0;
}

int npu_session_NW_CMD_STREAMOFF(struct npu_session *session)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	nw_cmd_e nw_cmd = NPU_NW_CMD_STREAMOFF;

	BUG_ON(!session);

	/* check npu_device emergency error */
	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	npu_udbg("sending STREAM OFF command.\n", session);
	profile_point1(PROBE_ID_DD_NW_RECEIVED, session->uid, 0, nw_cmd);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
	profile_point1(PROBE_ID_DD_NW_NOTIFIED, session->uid, 0, nw_cmd);

	if (npu_device_is_emergency_err(device)) {
		npu_udbg("NPU DEVICE IS EMERGENCY ERROR !\n", session);
		npu_udbg("sending CLEAR_CB command.\n", session);
		npu_session_put_nw_req(session, NPU_NW_CMD_CLEAR_CB);
		/* Clear CB has no notify function */
	}

	return 0;
}

int npu_session_stop(struct npu_queue *queue)
{
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;

	BUG_ON(!queue);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	npu_arbitration_session_stop(session);

	session->ss_state |= BIT(NPU_SESSION_STATE_STOP);

	return 0;
}

int npu_session_format(struct npu_queue *queue, struct vs4l_format_list *flist)
{
	int ret = 0;
	u32 i = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;

	struct address_vector *av;
	struct memory_vector *mv;

	char *ncp_vaddr;
	struct vs4l_format *formats;
	struct addr_info *FM_av;
	u32 address_vector_offset;
	u32 address_vector_cnt;

	u32 memory_vector_offset;
	u32 memory_vector_cnt;

	u32 cal_size;
	u32 bpp;
	u32 channels;
	u32 width;
	u32 height;

	u32 FM_cnt;

	BUG_ON(!queue);
	BUG_ON(!flist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	BUG_ON(!vctx);
	BUG_ON(!session);

	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;

	address_vector_offset = session->address_vector_offset;
	address_vector_cnt = session->address_vector_cnt;

	memory_vector_offset = session->memory_vector_offset;
	memory_vector_cnt = session->memory_vector_cnt;

	mv = (struct memory_vector *)(ncp_vaddr + memory_vector_offset);
	av = (struct address_vector *)(ncp_vaddr + address_vector_offset);

	formats = flist->formats;

	if (flist->direction == VS4L_DIRECTION_IN) {
		FM_av = session->IFM_info[0][0].addr_info;
		FM_cnt = session->IFM_cnt;
	} else {
		FM_av = session->OFM_info[0][0].addr_info;
		FM_cnt = session->OFM_cnt;
	}

	npu_udbg("flist %d, FM_cnt %d\n", session, flist->count, FM_cnt);
	if (FM_cnt > flist->count || FM_cnt + FM_SHARED_NUM < flist->count) {
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < FM_cnt; i++) {
		(mv + (FM_av + i)->av_index)->wstride = (formats + i)->stride;
		(mv + (FM_av + i)->av_index)->cstride = (formats + i)->cstride;

		bpp = (formats + i)->pixel_format;
		channels = (formats + i)->channels;
		width = (formats + i)->width;
		height = (formats + i)->height;
		cal_size = (bpp / 8) * channels * width * height;

		npu_udbg("dir(%d), av_index(%d)\n", session, flist->direction, (FM_av + i)->av_index);
		npu_udbg("dir(%d), av_size(%zu), cal_size(%u)\n", session, flist->direction, (FM_av + i)->size, cal_size);
#ifndef SYSMMU_FAULT_TEST
		if ((FM_av + i)->size > cal_size) {
			npu_uinfo("in_size(%zu), cal_size(%u) invalid\n", session, (FM_av + i)->size, cal_size);
			ret = NPU_ERR_DRIVER(NPU_ERR_SIZE_NOT_MATCH);
			goto p_err;
		}
#endif
	}

	if (flist->direction == VS4L_DIRECTION_IN) {
		session->ss_state |= BIT(NPU_SESSION_STATE_FORMAT_IN);
	} else {
		session->ss_state |= BIT(NPU_SESSION_STATE_FORMAT_OT);
	}
p_err:
	return ret;
}

static struct npu_memory_buffer *alloc_npu_load_payload(struct npu_session *session)
{
	struct npu_memory_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	/* 2 payloads.
	 * - User NCP
	 * - Kernel copy and updated NCP header.
	 */
	buffer->size = sizeof(struct cmd_load_payload) * 2;

	ret = npu_memory_alloc(session->memory, buffer, configs[NPU_PBHA_HINT_00]);
	if (ret) {
		npu_err("failed to allocate payload buffer(%d)\n", ret);
		goto err_free_buffer;
	}

	return buffer;
err_free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

int npu_session_NW_CMD_LOAD(struct npu_session *session)
{
	int ret = 0;
	nw_cmd_e nw_cmd = NPU_NW_CMD_LOAD;

	if (unlikely(!session)) {
		npu_err("invalid session\n");
		ret = -EINVAL;
		return ret;
	}

	profile_point1(PROBE_ID_DD_NW_RECEIVED, session->uid, 0, nw_cmd);

	session->ncp_payload = alloc_npu_load_payload(session);
	if (IS_ERR_OR_NULL(session->ncp_payload)) {
		ret = PTR_ERR(session->ncp_payload);
		session->ncp_payload = NULL;
		return ret;
	}

	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
	profile_point1(PROBE_ID_DD_NW_NOTIFIED, session->uid, 0, nw_cmd);
	return ret;
}

int __update_qbuf_IOFM_daddr(struct npu_session *session, struct addr_info *IFM_addr, struct addr_info *OFM_addr, u32 j)
{
	int ret = 0;
	u32 i = 0;
	struct address_vector *IOFM_av;
	struct npu_memory_buffer *IOFM_mem_buf;
	u32 IFM_cnt = session->IFM_cnt;
	u32 IOFM_cnt = session->IOFM_cnt;

	int cur_IOFM_idx = session->qbuf_IOFM_idx;

	if (cur_IOFM_idx == -1 || cur_IOFM_idx >= VISION_MAX_CONTAINERLIST) {
		ret = -EINVAL;
		goto p_err;
	}

	if (j >= VISION_MAX_BUFFER) {
		ret = -EINVAL;
		goto p_err;
	}


	IOFM_mem_buf = session->IOFM_mem_buf;
	IOFM_av = (struct address_vector *)(IOFM_mem_buf->vaddr);
	IOFM_av = IOFM_av + (cur_IOFM_idx*VISION_MAX_BUFFER + j)*IOFM_cnt;

	for (i = 0; i < IOFM_cnt; i++) {
		if (i < IFM_cnt) {
			(IOFM_av + i)->m_addr = (IFM_addr + i)->daddr;
			npu_unotice("update_IFM_ion, m_addr(%d/0x%x)\n", session, i, (IOFM_av + i)->m_addr);
		} else {
			(IOFM_av + i)->m_addr = (OFM_addr + (i - IFM_cnt))->daddr;
			npu_unotice("update_OFM_ion, m_addr(%d/0x%x)\n", session, i, (IOFM_av + i)->m_addr);
		}
	}
p_err:
	return ret;
}

static int npu_session_queue(struct npu_queue *queue, struct vb_container_list *incl, struct vb_container_list *otcl)
{
	int ret = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;

	struct mbox_process_dat *mbox_process_dat;

	struct av_info *IFM_info;
	struct av_info *OFM_info;

	struct addr_info *IFM_addr;
	struct addr_info *OFM_addr;
	u32 j;
	u32 buff_cnt;

	frame_cmd_e frame_cmd = NPU_FRAME_CMD_Q;

	BUG_ON(!queue);
	BUG_ON(!incl);
	/*BUG_ON(!incl->index >= configs[NPU_MAX_FRAME]);*/
	BUG_ON(!otcl);
	/*BUG_ON(!otcl->index >= configs[NPU_MAX_FRAME]);*/

	if (otcl->timestamp[5].tv_sec) {
		frame_cmd = NPU_FRAME_CMD_PROFILER;
	} else {
		frame_cmd = NPU_FRAME_CMD_Q;
	}

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	session->qbuf_IOFM_idx = incl->index;

	atomic_set(&otcl->containers[0].completed_count, 0);
	buff_cnt = otcl->containers[0].count;

	for (j = 0; j < buff_cnt; j++)	{
		IFM_info = &session->IFM_info[incl->index][j];
		IFM_addr = IFM_info->addr_info;
		IFM_info->IOFM_buf_idx = incl->index;

		OFM_info = &session->OFM_info[otcl->index][j];
		OFM_addr = OFM_info->addr_info;
		OFM_info->IOFM_buf_idx = otcl->index;

		/* Save profile data */
		profile_point1(PROBE_ID_DD_FRAME_EQ, session->uid, incl->id, incl->direction);
		profile_point1(PROBE_ID_DD_FRAME_EQ, session->uid, otcl->id, otcl->direction);

		ret = __update_qbuf_IOFM_daddr(session, IFM_addr, OFM_addr, j);
		if (unlikely(ret)) {
			goto p_err;
		}

		mbox_process_dat = &session->mbox_process_dat;
		mbox_process_dat->address_vector_cnt = session->IOFM_cnt;
		mbox_process_dat->address_vector_start_daddr = (session->IOFM_mem_buf->daddr)
			+ ((session->qbuf_IOFM_idx*VISION_MAX_BUFFER + j)*session->IOFM_cnt) * (sizeof(struct address_vector));

		ret = npu_session_put_frame_req(session, queue, incl, otcl, frame_cmd, IFM_info, OFM_info,
											incl->id*VISION_MAX_BUFFER + j);
		if (unlikely(ret))
			goto p_err;

		IFM_info->state = SS_BUF_STATE_QUEUE;
		OFM_info->state = SS_BUF_STATE_QUEUE;
	}

	npu_arbitration_info_update(session);

	return 0;
p_err:
	return ret;
}

static int npu_session_deque(struct npu_queue *queue, struct vb_container_list *clist)
{
	int ret = 0;

	struct npu_session *session;
	struct npu_vertex_ctx *vctx;
	int j;
	u32 buff_cnt;
	u32 containerListIndex = clist->index;

	struct av_info *FM_info;

	BUG_ON(!queue);
	BUG_ON(!clist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	if (unlikely(clist->index >= VISION_MAX_CONTAINERLIST)) {
		ret = -EINVAL;
		goto p_err;
	}

	buff_cnt = clist->containers[0].count;
	for (j = 0; j < buff_cnt; j++) {
		if (clist->direction == VS4L_DIRECTION_IN) {
			FM_info = &session->IFM_info[containerListIndex][j];
		} else {
			FM_info = &session->OFM_info[containerListIndex][j];
		}

		FM_info->state = SS_BUF_STATE_DEQUEUE;
		session->dqbuf_IOFM_idx = clist->index;
		/* Save profile data */
		profile_point1(PROBE_ID_DD_FRAME_DQ, session->uid, clist->id, clist->direction);

		npu_udbg("success in %s()\n", session, __func__);
	}
p_err:
	return ret;
}

const struct npu_queue_ops npu_queue_ops = {
	.start		= npu_session_start,
	.stop		= npu_session_stop,
	.format	= npu_session_format,
	.queue		= npu_session_queue,
	.deque		= npu_session_deque,
	.streamoff	= npu_session_streamoff,
};

static int npu_session_control(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_request(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_process(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_cancel(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_done(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_get_resource(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_put_resource(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_update_param(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

const struct npu_session_ops npu_session_ops = {
	.control	= npu_session_control,
	.request	= npu_session_request,
	.process	= npu_session_process,
	.cancel	= npu_session_cancel,
	.done		= npu_session_done,
	.get_resource	= npu_session_get_resource,
	.put_resource	= npu_session_put_resource,
	.update_param	= npu_session_update_param
};

static int npu_session_prepare(struct vb_queue *q, struct vb_container_list *clist)
{
	int ret = 0;
	struct npu_queue *queue = q->private_data;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	struct vb_buffer *buffer;
	struct av_info *FM_info;
	struct addr_info *FM_addr;
	struct vb_container *container;

	u32 FM_cnt, count;
	u32 i, j;
	u32 buf_cnt;
	u32 contailerListIndex;

	BUG_ON(!queue);
	BUG_ON(!clist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	contailerListIndex = clist->index;

	if (clist->index >= VISION_MAX_CONTAINERLIST) {
		ret = -EINVAL;
		goto p_err;
	}

	count = clist->count;

	for (i = 0; i < count ; i++) {
		container = &clist->containers[i];
		buf_cnt = clist->containers[i].count;

		for (j = 0; j < buf_cnt; j++) {
			if (clist->direction == VS4L_DIRECTION_IN) {
				FM_info = &session->IFM_info[contailerListIndex][j];
				FM_cnt = session->IFM_cnt;
			} else {
				FM_info = &session->OFM_info[contailerListIndex][j];
				FM_cnt = session->OFM_cnt;
			}

			npu_udbg("clist %d, FM_cnt %d\n", session, count, FM_cnt);
			if (FM_cnt > count || FM_cnt + FM_SHARED_NUM < count) {
				npu_uerr("dir(%d), FM_cnt(%u) != count(%u)\n", session, clist->direction, FM_cnt, count);
				ret = -EINVAL;
				goto p_err;
			}

			FM_addr = FM_info->addr_info;
			buffer = &container->buffers[j];
			(FM_addr + i)->daddr = buffer->daddr;
			(FM_addr + i)->vaddr = buffer->vaddr;
		}
		FM_info->address_vector_cnt = count;
		FM_info->state = SS_BUF_STATE_PREPARE;
	}

p_err:
	return ret;
}

static int npu_session_unprepare(struct vb_queue *q, struct vb_container_list *clist)
{
	int ret = 0;
	struct npu_queue *queue = q->private_data;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	struct av_info *FM_info;

	u32 k;

	BUG_ON(!queue);
	BUG_ON(!clist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	if (clist->index >= VISION_MAX_CONTAINERLIST) {
		ret = -EINVAL;
		goto p_err;
	}

	for (k = 0; k < VISION_MAX_BUFFER; k++) {
		if (clist->direction == VS4L_DIRECTION_IN) {
			FM_info = &session->IFM_info[clist->index][k];
		} else {
			FM_info = &session->OFM_info[clist->index][k];
		}

		FM_info->state = SS_BUF_STATE_UNPREPARE;
	}
p_err:
	return ret;
}

/* S_PARAM handler list definition - Chain of responsibility pattern */
typedef npu_s_param_ret (*fn_s_param_handler)(struct npu_session *, struct vs4l_param *, int *);
fn_s_param_handler s_param_handlers[] = {
	NULL,	/* NULL termination is required to denote EOL */
	npu_scheduler_param_handler,
	npu_qos_param_handler,
	fw_test_s_param_handler
};

int npu_session_param(struct npu_session *session, struct vs4l_param_list *plist)
{
	npu_s_param_ret		ret;
	int			retval = 0;
	struct vs4l_param	*param;
	u32			i, target;
	fn_s_param_handler	*p_fn;

	/* Search over each set param request over handlers */
	for (i = 0; i < plist->count; i++) {
		param = &plist->params[i];
		target = param->target;
		npu_udbg("Try set param [%u/%u] - target: [0x%x]\n",
			session, i+1, plist->count, target);

		p_fn = s_param_handlers;
		if ((target >= NPU_S_PARAM_PERF_MODE) &&
				(target <= NPU_S_PARAM_TPF)) {
			p_fn += 1;
		} else if ((target >= NPU_S_PARAM_QOS_NPU) &&
				(target <= NPU_S_PARAM_QOS_RST)) {
			p_fn += 2;
		} else if ((target >= NPU_S_PARAM_FW_UTC_LOAD) &&
				(target <= NPU_S_PARAM_FW_UTC_EXECUTE)) {
			p_fn += 3;
		} else {
			npu_uwarn("No handler defined for target [%u]."
				"Skipping.\n", session, param->target);
			/* Continue process others but retuen -EINVAL */
			retval = -EINVAL;
			continue;
		}

		ret = (*p_fn)(session, param, &retval);
		if (ret != S_PARAM_NOMB)
			npu_udbg("Handled by handler at [%pK](%d)\n",
						session, *p_fn, ret);
	}

	return retval;
}

const struct vb_ops vb_ops = {
	.buf_prepare = npu_session_prepare,
	.buf_unprepare = npu_session_unprepare
};

int npu_session_nw_sched_param(struct npu_session *session, struct vs4l_sched_param *param)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;

	int		retval = 0;
	u32		priority;
	u32		bound_id;
	u32		max_npu_core;

	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	priority = param->priority;
	bound_id = param->bound_id;
	max_npu_core = session->sched_param.max_npu_core;
	/* Check priority range */
	if (priority <= NPU_PRIORITY_MAX_VAL) {
		session->sched_param.priority = priority;
	} else {
		retval = -EINVAL;
	}
	/* Check boundness to the core */
	if ((bound_id < max_npu_core) || (bound_id == NPU_BOUND_UNBOUND)) {
		session->sched_param.bound_id = bound_id;
	} else {
		retval = -EINVAL;
	}
	/* Set the default value if anything is wrong */
	if (retval != 0) {
		session->sched_param.priority = NPU_PRIORITY_MAX_VAL;
		session->sched_param.bound_id = NPU_BOUND_UNBOUND;
	}

	npu_dbg("r[%d], p[%u], b[%u], c[%u]\n",
			retval, priority, bound_id, max_npu_core);
	npu_scheduler_update_sched_param(device, session);

	return retval;
}

int npu_session_register_undo_cb(struct npu_session *session, session_cb cb)
{
	BUG_ON(!session);

	session->undo_cb = cb;

	return 0;
}

int npu_session_execute_undo_cb(struct npu_session *session)
{
	BUG_ON(!session);

	if (session->undo_cb)
		session->undo_cb(session);

	return 0;
}

int npu_session_undo_open(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (session->ss_state < BIT(NPU_SESSION_STATE_OPEN)) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_OPEN))
		goto session_free;

	npu_sessionmgr_unregID(session->cookie, session);

session_free:
	kfree(session);
	session = NULL;
p_err:
	return ret;
}

int npu_session_undo_s_graph(struct npu_session *session)
{
	int ret = 0;

	if (!_undo_s_graph_each_state(session)) {
		npu_session_register_undo_cb(session, NULL);
		goto next_cb;
	}
	ret = -EINVAL;
	return ret;
next_cb:
	npu_session_execute_undo_cb(session);
	return ret;
}

int npu_session_undo_close(struct npu_session *session)
{
	int ret = 0;

	if (!_undo_close_each_state(session)) {
		npu_session_register_undo_cb(session, npu_session_undo_s_graph);
		goto next_cb;
	}
	ret = -EINVAL;
	return ret;
next_cb:
	npu_session_execute_undo_cb(session);
	return ret;
}

int npu_session_flush(struct npu_session *session)
{
	int ret = 0;

	if (!session)
		ret = -EINVAL;
	else
		_undo_s_graph_each_state(session);

	return ret;
}
