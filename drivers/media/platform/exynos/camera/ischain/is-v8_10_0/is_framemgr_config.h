/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo video node functions
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define IS_MAX_BUFS	VIDEO_MAX_FRAME
#define IS_MAX_PLANES	17
#define IS_EXT_MAX_PLANES	4

#define MAX_FRAME_INFO		(4)
#define MAX_STRIPE_REGION_NUM	(5)

enum is_frame_info_index {
	INFO_FRAME_START,
	INFO_CONFIG_LOCK,
	INFO_FRAME_END_PROC
};

struct is_frame_info {
	int			cpu;
	int			pid;
	unsigned long long	when;
};

struct fimc_is_stripe_size {
	/* Horizontal pixel ratio how much stripe processing is done. */
	u32	h_pix_ratio;
	/* Horizontal pixel count which stripe processing is done for. */
	u32	h_pix_num;
	/* Horizontal pixel count which stripe processing is done for before h_pix_num is processed. */
	u32	prev_h_pix_num;
};

struct fimc_is_stripe_info {
	/* Region index. */
	u32				region_id;
	/* Total region num. */
	u32				region_num;
	/* Frame base address of Y, UV plane */
	u32				region_base_addr[2];
	/* Stripe size for incrop/otcrop */
	struct fimc_is_stripe_size	in;
	struct fimc_is_stripe_size	out;
	/* For image dump */
	ulong                           kva[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
	size_t                          size[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
};

struct is_frame {
	struct list_head	list;
	struct kthread_work	work;
	struct kthread_delayed_work dwork;
	void			*groupmgr;
	void			*group;
	void			*subdev; /* is_subdev */

	/* group leader use */
	struct camera2_shot_ext	*shot_ext;
	struct camera2_shot	*shot;
	size_t			shot_size;

	/* stream use */
	struct camera2_stream	*stream;

	/* common use */
	u32			planes; /* total planes include multi-buffers */
	dma_addr_t		dvaddr_buffer[IS_MAX_PLANES];
	ulong			kvaddr_buffer[IS_MAX_PLANES];
	u32			size[IS_MAX_PLANES];

	/* external plane use */
	u32			ext_planes;
	ulong			kvaddr_ext[IS_EXT_MAX_PLANES];

	/*
	 * target address for capture node
	 * [0] invalid address, stop
	 * [others] valid address
	 */
	u32 txcTargetAddress[IS_MAX_PLANES];	/* 3AA capture DMA */
	u32 txpTargetAddress[IS_MAX_PLANES];	/* 3AA preview DMA */
	u32 mrgTargetAddress[IS_MAX_PLANES];	/* 3AA merge DMA */
	u32 efdTargetAddress[IS_MAX_PLANES];	/* 3AA FDPIG DMA */
	u32 ixcTargetAddress[IS_MAX_PLANES];	/* DNS YUV out DMA */
	u32 ixpTargetAddress[IS_MAX_PLANES];
	u32 ixvTargetAddress[IS_MAX_PLANES];	/* TNR PREV out DMA */
	u32 ixwTargetAddress[IS_MAX_PLANES];	/* TNR PREV weight out DMA */
	u32 ixtTargetAddress[IS_MAX_PLANES];	/* TNR PREV in DMA */
	u32 ixgTargetAddress[IS_MAX_PLANES];	/* TNR PREV weight in DMA */
	u64 mexcTargetAddress[IS_MAX_PLANES];	/* ME or MCH out DMA */
	u64 orbxcTargetAddress[IS_MAX_PLANES];	/* ME or MCH out DMA */
	u32 sc0TargetAddress[IS_MAX_PLANES];
	u32 sc1TargetAddress[IS_MAX_PLANES];
	u32 sc2TargetAddress[IS_MAX_PLANES];
	u32 sc3TargetAddress[IS_MAX_PLANES];
	u32 sc4TargetAddress[IS_MAX_PLANES];
	u32 sc5TargetAddress[IS_MAX_PLANES];
	u32 clxsTargetAddress[IS_MAX_PLANES];	/* CLAHE IN DMA */
	u32 clxcTargetAddress[IS_MAX_PLANES];	/* CLAHE OUT DMA */

	/* multi-buffer use */
	/* total number of buffers per frame */
	u32			num_buffers;
	/* current processed buffer index */
	u32			cur_buf_index;
	/* total number of shots per frame */
	u32			num_shots;
	/* current processed buffer index */
	u32			cur_shot_idx;

	/* internal use */
	unsigned long		mem_state;
	u32			state;
	u32			fcount;
	u32			rcount;
	u32			index;
	u32			lindex;
	u32			hindex;
	u32			result;
	unsigned long		out_flag;
	unsigned long		bak_flag;

	struct is_frame_info frame_info[MAX_FRAME_INFO];
	u32			instance; /* device instance */
	u32			type;
	unsigned long		core_flag;
	atomic_t		shot_done_flag;

#ifdef ENABLE_SYNC_REPROCESSING
	struct list_head	sync_list;
#endif
	struct list_head	votf_list;

	/* for NI(noise index from DDK) use */
	u32			noise_idx; /* Noise index */

#ifdef MEASURE_TIME
	/* time measure externally */
	struct timeval	*tzone;
	/* time measure internally */
	struct is_monitor	mpoint[TMS_END];
#endif

	/* for draw digit */
	u32			width;
	u32			height;

	struct fimc_is_stripe_info	stripe_info;
};

