/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
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
#if defined(CONFIG_PABLO_V8_1_0) || defined(CONFIG_PABLO_V8_10_0) || defined(CONFIG_PABLO_V8_20_0) || defined(CONFIG_PABLO_V9_1_0)
#include "is-lib-vra_v1_4.h"
#else
#include "is-lib-vra_v1_10.h"
#endif

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

enum is_lib_vra_dir {
	VRA_REAR_ORIENTATION,
	VRA_FRONT_ORIENTATION
};

struct lib_vra_tune_set {
	u32 index;
	ulong addr;
	u32 size;
	int decrypt_flag;
};

struct lib_vra_fr_work_info {
	struct api_vra_fr_work_init		fr_work_init;
	void					*fr_work_heap;
	u32					fr_work_size;
};

struct is_lib_vra_os_system_funcs {
	void (*control_task_set_event)(u32 event_type);
	void (*fw_algs_task_set_event)(u32 event_type);
	int  (*set_dram_adr_from_core_to_vdma)(ulong src_adr, u32 *target_adr);
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
	enum api_vra_type (*ex_get_memory_sizes)(const struct api_vra_alloc_info *alloc_ptr,
			vra_uint32 *fr_work_ram_size,
			vra_uint32 *sensor_ram_size,
			vra_uint32 *dma_ram_size);
	enum api_vra_type (*vra_frame_work_init)(struct lib_vra_fr_work_info *fr_work_info,
			const struct api_vra_alloc_info *alloc_ptr,
			const struct api_vra_dma_ram *dma_ram_info_ptr,
			unsigned int api_version);
	enum api_vra_type (*vra_sensor_init)(void *sen_obj_ptr,
			vra_uint32 callbacks_handle,
			const struct api_vra_input_desc *def_input_ptr,
			enum api_vra_track_mode def_track_mode);
	unsigned int (*set_parameter)(void *fr_obj_ptr, void *sen_obj_ptr,
			const struct api_vra_tune_data *tune_data);
	enum api_vra_type (*set_orientation)(void *sen_obj_ptr,
			enum api_vra_orientation orientation);
	enum api_vra_type (*set_input)(void *sen_obj_ptr,
			struct api_vra_input_desc *input_ptr,
			enum api_vra_update_tr_data_base update_tr_data_base);
	unsigned int (*get_parameter)(void *fr_obj_ptr,
			void *sen_obj_ptr,
			struct api_vra_tune_data *tune_data,
			enum api_vra_orientation *orientation);
	enum api_vra_type (*on_new_frame)(void *sen_obj_ptr,
			vra_uint32 unique_index,
			unsigned char *base_adrress_kva,
			unsigned char *base_adrress_dva);
	enum api_vra_type (*on_control_task_event)(void *fr_obj_ptr);
	enum api_vra_type (*on_fw_algs_task_event)(void *fr_obj_ptr);
	enum api_vra_type (*on_interrupt)(void *fr_obj_ptr, unsigned int index);
	enum api_vra_type (*frame_work_abort)(void *fr_obj_ptr,
			vra_boolean reset_tr_data_base);
	enum api_vra_type (*frame_work_terminate)(void *fr_obj_ptr);
	/* The set_post_detect_output is added with the ENABLE_HYBRID_FD.
	 * This function was deprecated after including CNN hardware in VRA block.
	 * But, we keep this interface function for backward compatibility
	 */
	enum api_vra_type (*set_post_detect_output)(void *sen_obj_ptr,
			vra_boolean post_detect_output);
#ifdef ENABLE_VRA_CHANGE_SETFILE_PARSING
	int (*copy_tune_set)(void *sen_obj_ptr, vra_uint32 instance_id, struct lib_vra_tune_set *set);
	int (*apply_tune_set)(void *sen_obj_ptr, vra_uint32 instance_id, vra_uint32 index);
#endif
#ifdef ENABLE_VRA_OVERFLOW_RECOVERY
	int (*recovery)(void *sen_obj_ptr);
#endif
};

struct is_lib_vra_debug {
	u32					lost_frame_cnt[VRA_PROCESS_FR + 1];
	u32					err_cnt;
	enum api_vra_sen_err			last_err_type;
	u32					last_err_info;
};

struct is_lib_vra_frame_lock {
	u32					lock_frame_num;
	u32					init_frames_per_lock; /* DMA */
	u32					normal_frames_per_lock; /* OTF */
};

struct is_lib_vra_tune_data {
	struct api_vra_tune_data		api_tune;
	enum is_lib_vra_dir		dir;
	struct is_lib_vra_frame_lock	frame_lock;
};

struct is_lib_vra {
	ulong					state;
	ulong					inst_state[VRA_TOTAL_SENSORS];

	u32					fr_index;
	struct api_vra_alloc_info		alloc_info;

	/* VRA frame work */
	struct api_vra_fr_work_init		fr_work_init;
	void					*fr_work_heap;
	u32					fr_work_size;

	/* VRA frame descript */
	struct api_vra_input_desc		frame_desc[VRA_TOTAL_SENSORS];
	void					*frame_desc_heap[VRA_TOTAL_SENSORS];
	u32					frame_desc_size;

	/* VRA out dma */
	struct api_vra_dma_ram			dma_out;
	void					*dma_out_heap;
	u32					dma_out_size;

	/* VRA Task */
	struct is_lib_task			task_vra;
	enum api_vra_ctrl_task_set_event	ctl_task_type;
	enum api_vra_fw_algs_task_set_event	algs_task_type;
	spinlock_t				ctl_lock;
	ulong					ctl_irq_flag;
	spinlock_t				algs_lock;
	ulong					algs_irq_flag;

	/* VRA interrupt lock */
	spinlock_t				intr_lock;

	/* VRA library functions */
	struct is_lib_vra_interface_funcs	itf_func;

	/* VRA output data */
	u32					max_face_num;	/* defined in setfile */
	u32					all_face_num[VRA_TOTAL_SENSORS];
	struct api_vra_out_list_info		out_list_info[VRA_TOTAL_SENSORS];
	struct api_vra_out_face			out_faces[VRA_TOTAL_SENSORS][MAX_FACE_COUNT];

	/* VRA debug data */
	struct is_lib_vra_debug		debug;

	/* VRA orientation data */
	enum is_lib_vra_dir		orientation[VRA_TOTAL_SENSORS];
	spinlock_t				slock;

#ifdef VRA_DMA_TEST_BY_IMAGE
	void					*test_input_buffer;
	bool					image_load;
#endif
	/* REPROCESSING_FD */
	struct is_hw_ip		*hw_ip;
	unsigned long			done_vra_callback_out_ready;
	unsigned long			done_vra_hw_intr;
	spinlock_t			reprocess_fd_lock;
	ulong				reprocess_fd_flag;

#ifdef ENABLE_VRA_CHANGE_SETFILE_PARSING
	ulong				tune_count;
#endif
	/* HYBRID_FD */
	unsigned int			post_detection_enable[VRA_TOTAL_SENSORS];
	u32				pdt_all_face_num[VRA_TOTAL_SENSORS];
	struct api_vra_out_list_info	pdt_out_list_info[VRA_TOTAL_SENSORS];
	struct api_vra_pdt_out_face	pdt_out_faces[VRA_TOTAL_SENSORS][MAX_FACE_COUNT];

	/* Fast FDAE & FDAF */
	u32				af_all_face_num[VRA_TOTAL_SENSORS];
	struct api_vra_out_list_info	af_out_list_info[VRA_TOTAL_SENSORS];
	struct api_vra_face_base_str	af_face_base[VRA_TOTAL_SENSORS][MAX_FACE_COUNT];
	spinlock_t			af_fd_slock[VRA_TOTAL_SENSORS];
	spinlock_t			ae_fd_slock[VRA_TOTAL_SENSORS];

};

void is_lib_vra_os_funcs(void);
int is_lib_vra_init_task(struct is_lib_vra *lib_vra);
int is_lib_vra_alloc_memory(struct is_lib_vra *lib_vra, ulong dma_addr);
int is_lib_vra_free_memory(struct is_lib_vra *lib_vra);
int is_lib_vra_frame_work_init(struct is_lib_vra *lib_vra,
	void __iomem *base_addr, enum is_lib_vra_input_type input_type);
int is_lib_vra_frame_desc_init(struct is_lib_vra *lib_vra,
	u32 instance);
int is_lib_vra_stop_instance(struct is_lib_vra *lib_vra, u32 instance);
int is_lib_vra_stop(struct is_lib_vra *lib_vra);
int is_lib_vra_frame_work_final(struct is_lib_vra *lib_vra);
int is_lib_vra_otf_input(struct is_lib_vra *lib_vra,
	struct vra_param *param, u32 instance, u32 fcount);
int is_lib_vra_dma_input(struct is_lib_vra *lib_vra,
	struct vra_param *param, u32 instance, u32 fcount);
int is_lib_vra_test_input(struct is_lib_vra *lib_vra, u32 instance);
int is_lib_vra_apply_tune(struct is_lib_vra *lib_vra,
	struct is_lib_vra_tune_data *vra_tune, u32 instance);
int is_lib_vra_set_orientation(struct is_lib_vra *lib_vra,
	u32 orientation, u32 instance);
int is_lib_vra_new_frame(struct is_lib_vra *lib_vra,
	unsigned char *buffer_kva, unsigned char *buffer_dva, u32 instance);
int is_lib_vra_handle_interrupt(struct is_lib_vra *lib_vra, u32 id);
int is_lib_vra_get_meta(struct is_lib_vra *lib_vra,
	struct is_frame *frame);
int is_lib_vra_test_image_load(struct is_lib_vra *lib_vra);
int is_lib_vra_copy_tune_set(struct is_lib_vra *this,
	ulong addr, u32 size, u32 index, int flag, u32 instance_id);
int is_lib_vra_apply_tune_set(struct is_lib_vra *this,
	u32 index, u32 instance_id);
#ifdef ENABLE_HYBRID_FD
int is_lib_vra_set_post_detect_output(struct is_lib_vra *lib_vra,
	unsigned int hfd_enable, u32 instance);
#endif
#ifdef ENABLE_VRA_OVERFLOW_RECOVERY
int is_lib_vra_reset_recovery(struct is_lib_vra *lib_vra, u32 instance_id);
#endif

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
		is_fpsimd_get_func();					\
		ret_call_libop = ((lib)->itf_func.op ?			\
				(lib)->itf_func.op(args) : -EINVAL);	\
		is_fpsimd_put_func();					\
									\
	ret_call_libop;})
#endif
