// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-interface-vra-v2.h"
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "is-vra-net-array-v3_2.h"
#include "is-hw-vra-v2.h"

struct is_lib_vra *g_lib_vra;

static void is_lib_vra_callback_final_output_ready(u32 num_all_faces,
	const struct api_vpl_out_face *faces_ptr,
	const struct api_vpl_out_list_info *out_list_info_ptr)
{
	int i, j;
	struct is_lib_vra *lib_vra = g_lib_vra;
	u32 face_rect[CAMERA2_MAX_FACES][4];
	u32 face_center[CAMERA2_MAX_FACES][2];
	bool debug_flag = false;
	unsigned long flags = 0;
	u32 instance = out_list_info_ptr->stream_idx;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return;
	}

#if defined(VRA_DMA_TEST_BY_IMAGE)
	info_lib("------ num_all_faces(%d) -------\n", num_all_faces);
#endif

	/*
	 * A spinlock is because reading and writing is performed in different thread.
	 * The read is performed in 3AA for FDAE. And the write is performed in VRA.
	 */
	spin_lock_irqsave(&lib_vra->ae_fd_slock[instance], flags);

	memcpy(&lib_vra->out_list_info[instance], (void *)out_list_info_ptr,
		sizeof(lib_vra->out_list_info[instance]));

	lib_vra->all_face_num[instance] = num_all_faces;

	for (i = 0; i < min_t(u32, lib_vra->all_face_num[instance], MAX_FACE_COUNT); i++) {
		lib_vra->out_faces[instance][i] = faces_ptr[i];
#if defined(VRA_DMA_TEST_BY_IMAGE)
		info_lib("lib_vra: id[%d]; x,y,w,h,score; %d,%d,%d,%d,%d\n",
				i, faces_ptr[i].rect.left,
				faces_ptr[i].rect.top,
				faces_ptr[i].rect.width,
				faces_ptr[i].rect.height,
				faces_ptr[i].score);
#endif
	}

	spin_unlock_irqrestore(&lib_vra->ae_fd_slock[instance], flags);

	for (i = 0; i < min_t(u32, lib_vra->all_face_num[instance], MAX_FACE_COUNT); i++) {
		face_rect[i][0] = faces_ptr[i].rect.topleft.x;
		face_rect[i][1] = faces_ptr[i].rect.topleft.y;
		face_rect[i][2] = faces_ptr[i].rect.topleft.x + faces_ptr[i].rect.width;
		face_rect[i][3] = faces_ptr[i].rect.topleft.y  + faces_ptr[i].rect.height;
		face_center[i][0] = (face_rect[i][0] + face_rect[i][2]) >> 1;
		face_center[i][1] = (face_rect[i][1] + face_rect[i][3]) >> 1;
	}

	for (i = 0; i < min_t(u32, lib_vra->all_face_num[instance], MAX_FACE_COUNT); i++) {
		for (j = 0; j < min_t(u32, lib_vra->all_face_num[instance], MAX_FACE_COUNT); j++) {
			if (i == j)
				continue;
			if (((face_rect[j][0] <= face_center[i][0]) && (face_center[i][0] <= face_rect[j][2]))
				&& ((face_rect[j][1] <= face_center[i][1]) && (face_center[i][1] <= face_rect[j][3]))) {
				info_lib("lib_vra_callback_output_ready: debug_flag on(%d/%d)\n", i, j);
				debug_flag = true;
				break;
			}
		}
	}

	if ((num_all_faces > lib_vra->max_face_num) || (debug_flag)) {
		dbg_lib(3, "lib_vra_callback_output_ready: num_all_faces(%d) > MAX(%d)\n",
			num_all_faces, lib_vra->max_face_num);

		for (i = 0; i < min_t(u32, lib_vra->all_face_num[instance], MAX_FACE_COUNT); i++) {
			dbg_lib(3, "lib_vra: (%d), id[%d]; x,y,w,h,score; %d,%d,%d,%d,%d\n",
				lib_vra->all_face_num[instance],
				i, faces_ptr[i].rect.topleft.x,
				faces_ptr[i].rect.topleft.y,
				faces_ptr[i].rect.width,
				faces_ptr[i].rect.height,
				faces_ptr[i].score);
		}
	}

	/* for VRA tracker & 10 fps operation : Frame done */
	is_hw_vra_frame_end_final_out_ready_done(lib_vra);

}

void is_lib_vra_task_trigger(struct is_lib_vra *lib_vra,
	void *func)
{
	u32 work_index = 0;
	struct is_lib_task *task_vra;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return;
	}

	task_vra = &lib_vra->task_vra;

	spin_lock(&task_vra->work_lock);

	task_vra->work[task_vra->work_index % LIB_MAX_TASK].func = func;
	task_vra->work[task_vra->work_index % LIB_MAX_TASK].params = lib_vra;
	task_vra->work_index++;
	work_index = (task_vra->work_index - 1) % LIB_MAX_TASK;

	spin_unlock(&task_vra->work_lock);

	kthread_queue_work(&task_vra->worker, &task_vra->work[work_index].work);
}

int __nocfi is_lib_vra_invoke_fwalgs_event(struct is_lib_vra *lib_vra)
{
	enum api_vpl_type status = VPL_OK;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	dbg_lib(3, "lib_vra_invoke_fwalgs_event\n");

	if (in_irq()) {
		spin_lock(&lib_vra->algs_lock);
		status = CALL_VRAOP(lib_vra, post_frame_process, lib_vra->post_frame_info);
		if (status) {
			err_lib("post_frame_process is fail (%#x)", status);
			spin_unlock(&lib_vra->algs_lock);
			return -EINVAL;
		}
		spin_unlock(&lib_vra->algs_lock);
	} else {
		spin_lock_irqsave(&lib_vra->algs_lock, lib_vra->algs_irq_flag);
		status = CALL_VRAOP(lib_vra, post_frame_process, lib_vra->post_frame_info);
		if (status) {
			err_lib("post_frame_process is fail (%#x)", status);
			spin_unlock_irqrestore(&lib_vra->algs_lock, lib_vra->algs_irq_flag);
			return -EINVAL;
		}
		spin_unlock_irqrestore(&lib_vra->algs_lock, lib_vra->algs_irq_flag);
	}

	return 0;
}

void __nocfi is_lib_vra_task_work(struct kthread_work *work)
{
	struct is_task_work *cur_work;
	struct is_lib_vra *lib_vra;

	cur_work = container_of(work, struct is_task_work, work);
	lib_vra = (struct is_lib_vra *)cur_work->params;

	cur_work->func((void *)lib_vra);
}

int is_lib_vra_init_task(struct is_lib_vra *lib_vra)
{
	s32 ret = 0;
	u32 j, cpu;
	struct sched_param param = { .sched_priority = IS_MAX_PRIO - 3 };

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	spin_lock_init(&lib_vra->task_vra.work_lock);
	kthread_init_worker(&lib_vra->task_vra.worker);

	lib_vra->task_vra.task = kthread_run(kthread_worker_fn,
		&lib_vra->task_vra.worker, "is_lib_vra");
	if (IS_ERR(lib_vra->task_vra.task)) {
		err_lib("failed to create thread for VRA, err(%ld)",
			PTR_ERR(lib_vra->task_vra.task));
		return PTR_ERR(lib_vra->task_vra.task);
	}
	is_fpsimd_set_task_using(lib_vra->task_vra.task, NULL);
	param.sched_priority = TASK_LIB_VRA_PRIO;
	ret = sched_setscheduler_nocheck(lib_vra->task_vra.task,
		SCHED_FIFO, &param);
	if (ret) {
		err("sched_setscheduler_nocheck is fail(%d)", ret);
		return ret;
	}

	lib_vra->task_vra.work_index = 0;
	for (j = 0; j < LIB_MAX_TASK; j++) {
		lib_vra->task_vra.work[j].func = NULL;
		lib_vra->task_vra.work[j].params = NULL;
		kthread_init_work(&lib_vra->task_vra.work[j].work,
			is_lib_vra_task_work);
	}

	cpu = TASK_VRA_AFFINITY;
	set_cpus_allowed_ptr(lib_vra->task_vra.task, cpumask_of(cpu));
	dbg_lib(3, "is_lib_vra: affinity %d\n", cpu);

	return 0;
}

void is_lib_vra_set_event_control(u32 event_type)
{
	warn_lib("Invalid event_type (%d)", event_type);

	return;
}

void is_lib_vra_frame_end_process(void *post_frame_info)
{
	struct is_lib_vra *lib_vra = g_lib_vra;

	lib_vra->post_frame_info = post_frame_info;
	dbg_lib(3, "lib_vra_set_event_fw_algs\n");
	is_lib_vra_task_trigger(lib_vra,
		is_lib_vra_invoke_fwalgs_event);

	return;
}

int __nocfi is_lib_vra_alloc_memory(struct is_lib_vra *lib_vra, ulong dma_addr)
{
	u32 size;
	enum api_vpl_type status = VPL_OK;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	size = ARRAY_SIZE(vra_network_array);
	lib_vra->dma_net_array_size = size;
	lib_vra->dma_net_array_heap = is_alloc_vra_net_array(size);
	memcpy(lib_vra->dma_net_array_heap, vra_network_array, size);

	lib_vra->dma_net_array.total_size = lib_vra->dma_net_array_size;
	lib_vra->dma_net_array.base_adr   = lib_vra->dma_net_array_heap;

	status = CALL_VRAOP(lib_vra, ex_get_memory_sizes,
				&lib_vra->fr_work_size,
				&lib_vra->dma_out_size,
				&lib_vra->dma_net_array);
	if (status) {
		err_lib("ex_get_memory_sizes is fail (%d)", status);
		return -ENOMEM;
	}

	if (SIZE_VRA_INTERNEL_BUF < lib_vra->dma_out_size) {
		err_lib("SIZE_VRA_INTERNEL_BUF(%d) < Request dma size(%d)",
			SIZE_VRA_INTERNEL_BUF, lib_vra->dma_out_size);
		return -ENOMEM;
	}

	dbg_lib(3, "lib_vra_alloc_memory: dma_out_size(%d), fr_work_size(%d)\n",
		lib_vra->dma_out_size, lib_vra->fr_work_size);

	size = lib_vra->dma_out_size;
	lib_vra->dma_out_heap = is_alloc_vra(size);
	memset(lib_vra->dma_out_heap, 0, size);

	size = lib_vra->fr_work_size;
	lib_vra->fr_work_heap = is_alloc_vra(size);
	memset(lib_vra->fr_work_heap, 0, size);

	dbg_lib(3, "lib_vra_alloc_memory: dma_out_heap(0x%p), fr_work_heap(0x%p), net_array(0x%p)\n",
		lib_vra->dma_out_heap, lib_vra->fr_work_heap, lib_vra->dma_net_array);

	return 0;
}

int is_lib_vra_free_memory(struct is_lib_vra *lib_vra)
{
	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	is_free_vra(lib_vra->dma_out_heap);
	is_free_vra(lib_vra->fr_work_heap);

	is_free_vra_net_array(lib_vra->dma_net_array_heap);

	g_lib_vra = NULL;

	return 0;
}

int __nocfi is_lib_vra_init_frame_work(struct is_lib_vra *lib_vra,
	void __iomem *base_addr)
{
	int ret;
	int ret_err;
	enum api_vpl_type status = VPL_OK;
	struct vra_call_backs_str *callbacks;
	int i;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	/* Connected vra library to global value */
	g_lib_vra = lib_vra;

	lib_vra->fr_index = 0;
#if defined(VRA_DMA_TEST_BY_IMAGE)
	lib_vra->image_load = false;
#endif

	spin_lock_init(&lib_vra->algs_lock);
	spin_lock_init(&lib_vra->intr_lock);

	for (i = 0; i < VRA_TOTAL_SENSORS; i++) {
		spin_lock_init(&lib_vra->ae_fd_slock[i]);
	}

	lib_vra->fr_work_in.total_size = lib_vra->fr_work_size;
	lib_vra->fr_work_in.base_adr   = lib_vra->fr_work_heap;

	lib_vra->dma_out.total_size = lib_vra->dma_out_size;
	lib_vra->dma_out.base_adr   = lib_vra->dma_out_heap;

	dbg_lib(3, "lib_vra_init_frame_work: hw_regs_adr(%#lx), swmem(%p : %d), hwmem(%p : %d)\n",
		(uintptr_t)base_addr,
		lib_vra->fr_work_in.base_adr, lib_vra->fr_work_in.total_size,
		lib_vra->dma_out.base_adr, lib_vra->dma_out.total_size);

	callbacks = &lib_vra->call_backs;
	callbacks->sen_final_output_ready_ptr = is_lib_vra_callback_final_output_ready;

	status = CALL_VRAOP(lib_vra, vra_frame_work_init,
				&lib_vra->fr_work_in,
				&lib_vra->dma_out,
				(unsigned char *)base_addr,
				&callbacks->sen_final_output_ready_ptr);
	if (status) {
		err_lib("vra_frame_work_init is fail(0x%x)", status);
		ret = -EINVAL;
		goto free;
	}

	for (i = 0; i < VRA_TOTAL_SENSORS; i++)
		clear_bit(VRA_INST_FRAME_DESC_INIT, &lib_vra->inst_state[i]);

	return 0;
free:
	ret_err = is_lib_vra_free_memory(lib_vra);
	if (ret_err)
		err_lib("lib_vra_free_memory is fail");

	return ret;
}

int is_lib_vra_frame_work_init(struct is_lib_vra *lib_vra,
	void __iomem *base_addr)
{
	int ret;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	if (!test_bit(VRA_LIB_FRAME_WORK_INIT, &lib_vra->state)) {
		ret = is_lib_vra_init_frame_work(lib_vra, base_addr);
		if (ret) {
			err_lib("lib_vra_init_frame_work is fail (%d)", ret);
			return ret;
		}
		set_bit(VRA_LIB_FRAME_WORK_INIT, &lib_vra->state);
	}

	set_bit(VRA_LIB_FWALGS_ABORT, &lib_vra->state);

	return 0;
}

int __nocfi is_lib_vra_new_frame(struct is_lib_vra *lib_vra,
	unsigned char *buffer_kva, unsigned char *buffer_dva, u32 instance)
{
	enum api_vpl_type status = VPL_OK;
	unsigned char *input_dma_buf_kva = NULL;
	dma_addr_t input_dma_buf_dva;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

#if defined(VRA_DMA_TEST_BY_IMAGE)
	input_dma_buf_kva = lib_vra->test_input_buffer;
	is_dva_vra((ulong)lib_vra->test_input_buffer, (u32 *)&input_dma_buf_dva);
#else
	input_dma_buf_kva = buffer_kva;
	input_dma_buf_dva = (dma_addr_t)buffer_dva;
#endif

	status = CALL_VRAOP(lib_vra, on_new_frame,
				instance,
				lib_vra->fr_index,
				lib_vra->in_sizes[instance],
				input_dma_buf_kva,
				(unsigned char *)input_dma_buf_dva);
	if (status == VPL_ERROR) {
		err_lib("[%d]on_new_frame is fail(%#x)", instance, status);
		return -EINVAL;
	}

	if (status == VPL_SKIP_HW)
		lib_vra->skip_hw = true;
	else
		lib_vra->skip_hw = false;

	clear_bit(VRA_LIB_FWALGS_ABORT, &lib_vra->state);

	return 0;
}

int __nocfi is_lib_vra_handle_interrupt(struct is_lib_vra *lib_vra, u32 id)
{
	enum api_vpl_type result;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	spin_lock(&lib_vra->intr_lock);
	result = CALL_VRAOP(lib_vra, on_interrupt);
	if (result) {
		err_lib("on_interrupt is fail (%#x)", result);
		spin_unlock(&lib_vra->intr_lock);
		return -EINVAL;
	}
	spin_unlock(&lib_vra->intr_lock);

	return 0;
}

static int is_lib_vra_fwalgs_stop(struct is_lib_vra *lib_vra)
{
	int ret = 0;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	dbg_lib(3, "lib_vra_fwalgs_stop\n");

	set_bit(VRA_LIB_FWALGS_ABORT, &lib_vra->state);

	return ret;
}

int is_lib_vra_stop_instance(struct is_lib_vra *lib_vra, u32 instance)
{
	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	if (instance >= VRA_TOTAL_SENSORS) {
		err_lib("invalid instance");
		return -EINVAL;
	}

	lib_vra->all_face_num[instance] = 0;

	return 0;
}

int __nocfi is_lib_vra_stop(struct is_lib_vra *lib_vra)
{
	int ret;
	int i;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	ret = is_lib_vra_fwalgs_stop(lib_vra);
	if (ret) {
		err_lib("lib_vra_fwalgs_stop is fail(%d)", ret);
		return ret;
	}

	for (i = 0; i < VRA_TOTAL_SENSORS; i++) {
		lib_vra->all_face_num[i] = 0;
	}

	return 0;
}

int __nocfi is_lib_vra_frame_work_final(struct is_lib_vra *lib_vra)
{
	enum api_vpl_type result;
	int ret;
	int i;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	result = CALL_VRAOP(lib_vra, frame_work_terminate);
	if (result) {
		err_lib("frame_work_terminate is fail (%#x)", result);
		return -EINVAL;
	}

	ret = is_lib_vra_fwalgs_stop(lib_vra);
	if (ret) {
		err_lib("lib_vra_fwalgs_stop is fail(%d)", ret);
		return ret;
	}

	if (!IS_ERR_OR_NULL(lib_vra->task_vra.task)) {
		ret = kthread_stop(lib_vra->task_vra.task);
		if (ret)
			err_lib("kthread_stop fail (%d)", ret);

		lib_vra->task_vra.task = NULL;
	}

	clear_bit(VRA_LIB_FRAME_WORK_INIT, &lib_vra->state);
	clear_bit(VRA_LIB_FWALGS_ABORT, &lib_vra->state);
	clear_bit(VRA_LIB_BYPASS_REQUESTED, &lib_vra->state);

	for (i = 0; i < VRA_TOTAL_SENSORS; i++) {
		clear_bit(VRA_INST_FRAME_DESC_INIT, &lib_vra->inst_state[i]);
	}

	return 0;
}

int __nocfi is_lib_vra_sw_reset(struct is_lib_vra *lib_vra)
{
	enum api_vpl_type result;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	result = CALL_VRAOP(lib_vra, sw_reset);
	if (result) {
		err_lib("vra sw_reset is fail (%#x)", result);
		return -EINVAL;
	}

	return 0;
}

int __nocfi is_lib_vra_run_tracker(struct is_lib_vra *lib_vra, u32 instance,
	u32 fcount)
{
	enum api_vpl_type result;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	result = CALL_VRAOP(lib_vra, run_tracker, instance, fcount);
	if (result) {
		err_lib("vra run_tracker is fail (%#x)", result);
		return -EINVAL;
	}

	return 0;
}

static int is_lib_vra_update_dm(struct is_lib_vra *lib_vra, u32 instance,
	struct camera2_stats_dm *dm)
{
	int face_num;
	struct api_vpl_out_face *faceout;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	if (unlikely(!dm)) {
		err_lib("camera2_stats_dm is NULL");
		return -EINVAL;
	}

	if (!lib_vra->all_face_num[instance]) {
		memset(&dm->faceIds, 0, sizeof(dm->faceIds));
		memset(&dm->faceRectangles, 0, sizeof(dm->faceRectangles));
		memset(&dm->faceScores, 0, sizeof(dm->faceScores));
		memset(&dm->faceLandmarks, 0, sizeof(dm->faceIds));
	}

	dm->faceSrcImageSize[0] = lib_vra->out_list_info[instance].in_sizes.width;
	dm->faceSrcImageSize[1] = lib_vra->out_list_info[instance].in_sizes.height;
	dbg_lib(3, "lib_vra_update_dm: face size(%d,%d)\n", dm->faceSrcImageSize[0], dm->faceSrcImageSize[1]);

	for (face_num = 0; face_num < min_t(u32, lib_vra->all_face_num[instance], MAX_FACE_COUNT); face_num++) {
		faceout = &lib_vra->out_faces[instance][face_num];
		/* X min */
		dm->faceRectangles[face_num][0] = faceout->rect.topleft.x;
		/* Y min */
		dm->faceRectangles[face_num][1] = faceout->rect.topleft.y;
		/* X max */
		dm->faceRectangles[face_num][2] = faceout->rect.topleft.x + faceout->rect.width;
		/* Y max */
		dm->faceRectangles[face_num][3] = faceout->rect.topleft.y + faceout->rect.height;
		/* Score (0 ~ 255) */
		dm->faceScores[face_num] = faceout->score;
		/* ID */
		dm->faceIds[face_num] = faceout->unique_id;

		dbg_lib(3, "lib_vra_update_dm[%d]: face position(%d,%d),size(%dx%d),scores(%d),id(%d))\n",
			instance,
			dm->faceRectangles[face_num][0],
			dm->faceRectangles[face_num][1],
			dm->faceRectangles[face_num][2],
			dm->faceRectangles[face_num][3],
			dm->faceScores[face_num],
			dm->faceIds[face_num]);
	}

	/* ToDo: Add error handler for detected face range */

	return 0;
}

int is_lib_vra_update_sm(struct is_lib_vra *lib_vra)
{
	/* ToDo */
	return 0;
}

int is_lib_vra_get_meta(struct is_lib_vra *lib_vra,
	struct is_frame *frame)
{
	int ret = 0;
	struct camera2_stats_ctl *stats_ctl;
	struct camera2_stats_dm *stats_dm;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	if (unlikely(!frame)) {
		err_lib("frame is NULL");
		return -EINVAL;
	}

	stats_ctl = &frame->shot->ctl.stats;
	stats_dm = &frame->shot->dm.stats;

	if (stats_ctl->faceDetectMode == FACEDETECT_MODE_OFF) {
		stats_dm->faceDetectMode = FACEDETECT_MODE_OFF;
		return 0;
	} else {
		/* TODO: FACEDETECT_MODE_FULL*/
		stats_dm->faceDetectMode = FACEDETECT_MODE_SIMPLE;
	}

	ret = is_lib_vra_update_dm(lib_vra, frame->instance,
		stats_dm);
	if (ret) {
		err_lib("lib_vra_update_dm is fail (%#x)", ret);
		return -EINVAL;
	}

	/* ToDo : is_lib_vra_update_sm */

	return 0;
}

#if defined(VRA_DMA_TEST_BY_IMAGE)
int is_lib_vra_test_image_load(struct is_lib_vra *lib_vra)
{
	int ret = 0;
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *vra_dma_image = NULL;
	long fsize, nread;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	if (lib_vra->image_load)
		return 0;

	vra_dma_image = filp_open(VRA_DMA_TEST_IMAGE_PATH, O_RDONLY, 0);
	if (unlikely(!vra_dma_image)) {
		err("filp_open(%s) fail!!\n", VRA_DMA_TEST_IMAGE_PATH);
		return -EEXIST;
	}

	fsize = vra_dma_image->f_path.dentry->d_inode->i_size;
	fsize -= 1;

	info_lib("lib_vra_test_image_load: size(%ld)Bytes\n", fsize);
	lib_vra->test_input_buffer = is_alloc_vra(fsize);
	nread = kernel_read(vra_dma_image,
			lib_vra->test_input_buffer,
			fsize, &vra_dma_image->f_pos);
	if (nread != fsize) {
		err_lib("failed to read firmware file (%ld)Bytes", nread);
		ret = -EINVAL;
		goto buf_free;
	}

	lib_vra->image_load = true;

	return 0;

buf_free:
	is_free_vra(lib_vra->test_input_buffer);
#else
	err("not support %s due to kernel_read!", __func__);
	ret = -EINVAL;
#endif
	return ret;
}
#endif

void is_lib_vra_assert(void)
{
	BUG_ON(1);
}

void __nocfi is_lib_vra_os_funcs(void)
{
	struct is_lib_vra_os_system_funcs funcs;

	funcs.frame_end_process = is_lib_vra_frame_end_process;
	funcs.set_dram_adr_from_core_to_vdma = is_dva_vra;
	funcs.set_net_dram_adr_from_core_to_vdma = is_dva_vra_net_array;

	funcs.clean_cache_region             = is_inv_vra;
	funcs.invalidate_cache_region        = is_inv_vra;
	funcs.data_write_back_cache_region   = is_clean_vra;
	funcs.log_write_console              = is_log_write_console;
	funcs.log_write                      = is_log_write;

	funcs.spin_lock_init 	     = is_spin_lock_init;
	funcs.spin_lock_finish 	     = is_spin_lock_finish;
	funcs.spin_lock              = is_spin_lock;
	funcs.spin_unlock            = is_spin_unlock;
	funcs.spin_lock_irq          = is_spin_lock_irq;
	funcs.spin_unlock_irq        = is_spin_unlock_irq;
	funcs.spin_lock_irqsave      = is_spin_lock_irqsave;
	funcs.spin_unlock_irqrestore = is_spin_unlock_irqrestore;
	funcs.lib_assert       = is_lib_vra_assert;
	funcs.lib_in_irq       = is_lib_in_irq;

	fpsimd_get_func();
	((vra_set_os_funcs_t)VRA_LIB_ADDR)((void *)&funcs);
	fpsimd_put_func();
}

int __nocfi is_lib_vra_test_input(struct is_lib_vra *lib_vra, u32 instance)
{
	int ret = 0;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	lib_vra->in_sizes[instance].width = 320;
	lib_vra->in_sizes[instance].height = 240;

	return ret;
}

int __nocfi is_lib_vra_dma_input(struct is_lib_vra *lib_vra,
	struct vra_param *param, u32 instance, u32 fcount)
{
	int ret = 0;

	if (unlikely(!lib_vra)) {
		err_lib("lib_vra is NULL");
		return -EINVAL;
	}

	if (unlikely(!param)) {
		err_lib("vra_param is NULL");
		return -EINVAL;
	}

	lib_vra->in_sizes[instance].width = param->dma_input.width;
	lib_vra->in_sizes[instance].height = param->dma_input.height;

	return ret;
}
