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

#ifndef IS_API_VRA_H
#define IS_API_VRA_H

#include "is-interface-library.h"
#include "is-metadata.h"
#include "is-param.h"
#include "is-config.h"
#include "is-lib-vra_v3_2.h"

#define VRA_TOTAL_SENSORS		IS_STREAM_COUNT
#define LIB_VRA_API_ADDR		(VRA_LIB_ADDR + LIB_VRA_OFFSET)

/* #define VRA_DMA_TEST_BY_IMAGE */
#define VRA_DMA_TEST_IMAGE_PATH		"/data/vra_test.yuv"

typedef int (*vra_set_os_funcs_t)(void *func);
typedef u32 (*vra_load_itf_funcs_t)(void *func);
static const vra_load_itf_funcs_t get_lib_vra_func = (vra_load_itf_funcs_t)LIB_VRA_API_ADDR;

enum is_lib_vra_state_list {
	VRA_LIB_FRAME_WORK_INIT,
	VRA_LIB_FWALGS_ABORT,
	VRA_LIB_BYPASS_REQUESTED
};

enum vra_inst_state {
	VRA_INST_FRAME_DESC_INIT,
	VRA_INST_APPLY_TUNE_SET,
};

enum is_lib_vra_input_type {
	VRA_INPUT_OTF,
	VRA_INPUT_MEMORY
};

struct is_lib_vra_os_system_funcs {
	void (*frame_end_process)(void *post_frame_info);
	int  (*set_dram_adr_from_core_to_vdma)(ulong src_adr, u32 *target_adr);
	int  (*set_net_dram_adr_from_core_to_vdma)(ulong src_adr, u32 *target_adr);
	void (*clean_cache_region)(ulong start_addr, u32 size);
	void (*invalidate_cache_region)(ulong start_addr, u32 size);
	void (*data_write_back_cache_region)(ulong start_adr, u32 size);
	int  (*log_write_console)(char *str);
	int  (*log_write)(const char *str, ...);
	int  (*spin_lock_init)(void **slock);
	int  (*spin_lock_finish)(void *slock_lib);
	int  (*spin_lock)(void *slock_lib);
	int  (*spin_unlock)(void *slock_lib);
	int  (*spin_lock_irq)(void *slock_lib);
	int  (*spin_unlock_irq)(void *slock_lib);
	ulong (*spin_lock_irqsave)(void *slock_lib);
	int  (*spin_unlock_irqrestore)(void *slock_lib, ulong flag);
	void (*lib_assert)(void);
	bool (*lib_in_irq)(void);
};

struct is_lib_vra_interface_funcs {
	enum api_vpl_type (*ex_get_memory_sizes)(vra_uint32 *fr_work_ram_size,
			vra_uint32 *dma_ram_size,
			const struct api_vpl_dma_ram *dma_net_array_ptr);
	enum api_vpl_type (*vra_frame_work_init)(const struct api_vpl_dma_ram *fr_work_in_prt,
			const struct api_vpl_dma_ram *dma_ram_info_ptr,
			unsigned char *hw_base_addr,
			const on_sen_final_output_ready_ptr *out_ready_callbacks);
	/* on_new_frame = HandleFrame */
	enum api_vpl_type (*on_new_frame)(vra_uint32 vra_stream_id,
			vra_uint32 unique_index,
			const struct api_vpl_frame_size in_size,
			unsigned char *base_adrress_kva,
			unsigned char *base_adrress_dva);
	enum api_vpl_type (*post_frame_process)(void *post_frame_info);
	enum api_vpl_type (*on_interrupt)(void);
	enum api_vpl_type (*frame_work_terminate)(void);
	enum api_vpl_type (*sw_reset)(void);
	enum api_vpl_type (*run_tracker)(vra_uint32 vra_stream_id, vra_uint32 fcount);
};

struct is_lib_vra {
	ulong					state;
	ulong					inst_state[VRA_TOTAL_SENSORS];

	u32					fr_index;

	/* VRA frame work */
	struct vra_call_backs_str	call_backs;
	struct api_vpl_frame_size	in_sizes[VRA_TOTAL_SENSORS];
	struct api_vpl_dma_ram			fr_work_in;
	void					*fr_work_heap;
	u32					fr_work_size;

	/* VRA out dma */
	struct api_vpl_dma_ram			dma_out;
	void					*dma_out_heap;
	u32					dma_out_size;

	/* VRA Task */
	struct is_lib_task			task_vra;
	spinlock_t				algs_lock;
	ulong					algs_irq_flag;

	/* VRA interrupt lock */
	spinlock_t				intr_lock;

	/* VRA library functions */
	struct is_lib_vra_interface_funcs	itf_func;

	/* VRA output data */
	u32					max_face_num;
	u32					all_face_num[VRA_TOTAL_SENSORS];
	struct api_vpl_out_list_info		out_list_info[VRA_TOTAL_SENSORS];
	struct api_vpl_out_face			out_faces[VRA_TOTAL_SENSORS][MAX_FACE_COUNT];

#ifdef VRA_DMA_TEST_BY_IMAGE
	void					*test_input_buffer;
	bool					image_load;
#endif
	/* for VRA tracker & 10 fps operation */
	struct is_hw_ip		*hw_ip;

	/* Fast FDAE */
	spinlock_t			ae_fd_slock[VRA_TOTAL_SENSORS];

	struct api_vpl_dma_ram			dma_net_array;
	void					*dma_net_array_heap;
	u32					dma_net_array_size;

	void				*post_frame_info;
	bool				skip_hw;
};

void is_lib_vra_os_funcs(void);
int is_lib_vra_init_task(struct is_lib_vra *lib_vra);
int is_lib_vra_alloc_memory(struct is_lib_vra *lib_vra, ulong dma_addr);
int is_lib_vra_free_memory(struct is_lib_vra *lib_vra);
int is_lib_vra_frame_work_init(struct is_lib_vra *lib_vra,
	void __iomem *base_addr);
int is_lib_vra_stop_instance(struct is_lib_vra *lib_vra, u32 instance);
int is_lib_vra_stop(struct is_lib_vra *lib_vra);
int is_lib_vra_frame_work_final(struct is_lib_vra *lib_vra);
int is_lib_vra_sw_reset(struct is_lib_vra *lib_vra);
int is_lib_vra_run_tracker(struct is_lib_vra *lib_vra, u32 instance, u32 fcount);
int is_lib_vra_dma_input(struct is_lib_vra *lib_vra,
	struct vra_param *param, u32 instance, u32 fcount);
int is_lib_vra_test_input(struct is_lib_vra *lib_vra, u32 instance);
int is_lib_vra_new_frame(struct is_lib_vra *lib_vra,
	unsigned char *buffer_kva, unsigned char *buffer_dva, u32 instance);
int is_lib_vra_handle_interrupt(struct is_lib_vra *lib_vra, u32 id);
int is_lib_vra_get_meta(struct is_lib_vra *lib_vra,
	struct is_frame *frame);
int is_lib_vra_test_image_load(struct is_lib_vra *lib_vra);

#ifdef DEBUG_HW_SIZE
#define lib_vra_check_size(desc, param, fcount) \
	is_lib_vra_check_size(desc, param, fcount)
#else
#define lib_vra_check_size(desc, param, fcount)
#endif

#define CALL_VRAOP(lib, op, args...)					\
	({								\
		int ret_call_libop;					\
									\
		is_fpsimd_get_func();						\
		ret_call_libop = ((lib)->itf_func.op ?			\
				(lib)->itf_func.op(args) : -EINVAL);	\
		is_fpsimd_put_func();						\
									\
	ret_call_libop;})
#endif
