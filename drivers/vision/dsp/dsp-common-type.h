/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Module for controlling SCore driver connected with APCPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_COMMON_TYPE_H__
#define __DSP_COMMON_TYPE_H__

#define GET_COMMON_GRAPH_ID(X)						\
({									\
	union dsp_common_global_id temp_id;				\
	temp_id.num = (X);						\
	temp_id.graph_id;						\
})

#define SET_COMMON_CONTEXT_ID(VAR, X)					\
do {									\
	union dsp_common_global_id *temp_id;				\
	temp_id = (union dsp_common_global_id *)(VAR);			\
	temp_id->context_id = (X);					\
} while (0)

#define GET_COMMON_CONTEXT_ID(X)					\
({									\
	union dsp_common_global_id temp_id;				\
	temp_id.num = (X);						\
	temp_id.context_id;						\
})

#define DSP_MD5_SIZE			(32)

enum dsp_bin_type {
	BIN_TYPE_DSP_BIN,
	BIN_TYPE_DSP_MASTER_BIN,
	BIN_TYPE_DSP_IAC_DM_BIN,
	BIN_TYPE_DSP_IAC_PM_BIN,
	BIN_TYPE_DSP_IVP_DM_BIN,
	BIN_TYPE_DSP_IVP_PM_BIN,
	BIN_TYPE_DSP_RELOC_RULES_BIN,
	BIN_TYPE_DSP_GKT_XML,
	BIN_TYPE_LIB_IVP_ELF,
	BIN_TYPE_LIB_LOG_ELF,
	BIN_TYPE_MAX,
};

struct dsp_bin_file {
	unsigned int			name;
	unsigned int			size;
	char				md5[DSP_MD5_SIZE];
	unsigned long long		vaddr;
};

struct dsp_bin_file_list {
	unsigned int			count;
	unsigned int			reserved;
	struct dsp_bin_file		bins[BIN_TYPE_MAX];
};

enum dsp_common_message_id {
	DSP_COMMON_LOAD_GRAPH,
	DSP_COMMON_EXECUTE_MSG,
	DSP_COMMON_UNLOAD_GRAPH,
	DSP_COMMON_IVP_CACHE_INVALIDATE,
	DSP_COMMON_MESSAGE_NUM
};

enum dsp_common_addr_type {
	DSP_COMMON_V_ADDR,
	DSP_COMMON_DV_ADDR,
	DSP_COMMON_FD,
};

enum dsp_common_mem_attr {
	DSP_COMMON_CACHEABLE,
	DSP_COMMON_NON_CACHEABLE,
	DSP_COMMON_UNKNOWN_CACHEABLE,
};

enum dsp_common_mem_type {
	DSP_COMMON_MEM_ION,
	DSP_COMMON_MEM_MALLOC,
	DSP_COMMON_MEM_ASHMEM
};

enum dsp_common_param_type {
	DSP_COMMON_PARAM_TSGD,
	DSP_COMMON_PARAM,
	DSP_COMMON_PARAM_UPDATE,
	DSP_COMMON_PARAM_KERNEL,
	DSP_COMMON_PARAM_EMPTY,
};

struct dsp_common_mem_v1 {
	unsigned char			addr_type;
	unsigned char			mem_attr;
	unsigned char			mem_type;
	unsigned char			is_mandatory;
	unsigned int			size;
	union {
		struct {
			int		fd;
			unsigned int	iova;
		};
		void			*not_use;
	};
};

struct dsp_common_mem_v2 {
	unsigned char			addr_type;
	unsigned char			mem_attr;
	unsigned char			mem_type;
	unsigned char			is_mandatory;
	unsigned int			size;
	union {
		unsigned int		reserved1[4];
		struct {
			unsigned int	width;
			unsigned int	height;
			unsigned int	channel;
			unsigned int	reserved2;
		};
	};
	union {
		struct {
			int		fd;
			unsigned int	iova;
		};
		void			*not_use;
	};
};

struct dsp_common_mem_v3 {
	unsigned char			addr_type;
	unsigned char			mem_attr;
	unsigned char			mem_type;
	unsigned char			is_mandatory;
	unsigned int			size;
	unsigned int			offset;
	unsigned int			reserved;
	unsigned int			param[6];
	union {
		struct {
			int		fd;
			unsigned int	iova;
		};
		void			*not_use;
	};
};

struct dsp_common_param_v1 {
	unsigned int			param_type;
	union {
		unsigned int		param_index;
		unsigned int		kernel_id;
	};
	struct dsp_common_mem_v1	param_mem;
};

struct dsp_common_param_v2 {
	unsigned int			param_type;
	union {
		unsigned int		param_index;
		unsigned int		kernel_id;
	};
	struct dsp_common_mem_v2	param_mem;
};

struct dsp_common_param_v3 {
	unsigned int			param_type;
	union {
		unsigned int		param_index;
		unsigned int		kernel_id;
	};
	struct dsp_common_mem_v3	param_mem;
};

union dsp_common_global_id {
	struct {
		unsigned int		head :2;
		unsigned int		target :2;
		unsigned int		graph_id :8;
		unsigned int		context_id :4;
		unsigned int		msg_id :16;
	};
	unsigned int			num;
};

struct dsp_common_graph_info_v1 {
	unsigned int			global_id;
	unsigned int			n_tsgd;
	unsigned int			n_param;
	unsigned int			n_kernel;
	struct dsp_common_param_v1	param_list[0];
};

struct dsp_common_graph_info_v2 {
	unsigned int			global_id;
	unsigned int			n_tsgd;
	unsigned int			n_param;
	unsigned int			n_kernel;
	struct dsp_common_param_v2	param_list[0];
};

struct dsp_common_graph_info_v3 {
	unsigned int			global_id;
	unsigned int			n_tsgd;
	unsigned int			n_param;
	unsigned int			n_kernel;
	struct dsp_common_param_v3	param_list[0];
};

struct dsp_common_execute_info_v1 {
	unsigned int			global_id;
	unsigned int			n_update_param;
	struct dsp_common_param_v1	param_list[0];
};

struct dsp_common_execute_info_v2 {
	unsigned int			global_id;
	unsigned int			n_update_param;
	struct dsp_common_param_v2	param_list[0];
};

struct dsp_common_execute_info_v3 {
	unsigned int			global_id;
	unsigned int			n_update_param;
	struct dsp_common_param_v3	param_list[0];
};

#endif
