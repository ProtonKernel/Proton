/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo video node functions
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
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

#define MAX_OUT_LOGICAL_NODE	(1)
#define MAX_CAP_LOGICAL_NODE	(4)

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
	u32	h_pix_num[MAX_STRIPE_REGION_NUM];
};

struct fimc_is_stripe_info {
	/* Region index. */
	u32				region_id;
	/* Total region num. */
	u32				region_num;
	/* Frame base address of Y, UV plane */
	u32				region_base_addr[2];
	/* stripe width for sub-frame */
	u32 				stripe_w;
	/* Stripe size for incrop/otcrop */
	struct fimc_is_stripe_size	in;
	struct fimc_is_stripe_size	out;
	/* For image dump */
	ulong                           kva[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
	size_t                          size[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
};

struct is_sub_frame {
	/* Target address for all input node
	 * 0: invalid address, DMA should be disabled
	 * any value: valid address
	 */
	u32			id;
	u32			num_planes;
	u32			hw_pixeltype;
	ulong			kva[IS_MAX_PLANES];
	dma_addr_t		dva[IS_MAX_PLANES];
	dma_addr_t		backup_dva[IS_MAX_PLANES];

	struct fimc_is_stripe_info	stripe_info;
};

struct is_sub_node {
	u32			hw_id;
	struct is_sub_frame	sframe[CAPTURE_NODE_MAX];
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
	struct is_priv_buf	*pb_output;
#ifdef ENABLE_LOGICAL_VIDEO_NODE
	struct is_priv_buf	*pb_capture[CAPTURE_NODE_MAX];
#endif
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
	u32 txcTargetAddress[IS_MAX_PLANES];		/* 3AA capture WDMA */
	u32 txpTargetAddress[IS_MAX_PLANES];		/* 3AA preview WDMA */
	u32 mrgTargetAddress[IS_MAX_PLANES];		/* 3AA merge WDMA */
	u32 efdTargetAddress[IS_MAX_PLANES];		/* 3AA FDPIG WDMA */
	u32 txdgrTargetAddress[IS_MAX_PLANES];		/* 3AA DRC GRID WDMA */
	u32 txodsTargetAddress[IS_MAX_PLANES];		/* 3AA ORB DS WDMA */
	u32 txldsTargetAddress[IS_MAX_PLANES];		/* 3AA LME DS WDMA */
	u32 txhfTargetAddress[IS_MAX_PLANES];		/* 3AA HF WDMA */
	u32 lmesTargetAddress[IS_MAX_PLANES];       	/* LME prev IN DMA */
	u32 lmecTargetAddress[IS_MAX_PLANES];       	/* LME OUT DMA */
	u64 lmecKTargetAddress[IS_MAX_PLANES];       	/* LME OUT KVA */
	u64 lmemKTargetAddress[IS_MAX_PLANES];       	/* LME SW OUT KVA */
	u32 ixcTargetAddress[IS_MAX_PLANES];		/* DNS YUV out WDMA */
	u32 ixpTargetAddress[IS_MAX_PLANES];
	u32 ixdgrTargetAddress[IS_MAX_PLANES];		/* DNS DRC GRID RDMA */
	u32 ixrrgbTargetAddress[IS_MAX_PLANES];		/* DNS REP RGB RDMA */
	u32 ixnoirTargetAddress[IS_MAX_PLANES];		/* DNS REP NOISE RDMA */
	u32 ixscmapTargetAddress[IS_MAX_PLANES];	/* DNS SC MAP RDMA */
	u32 ixnrdsTargetAddress[IS_MAX_PLANES];		/* DNS NR DS WDMA */
	u32 ixnoiTargetAddress[IS_MAX_PLANES];		/* DNS NOISE WDMA */
	u32 ixdgaTargetAddress[IS_MAX_PLANES];		/* DNS DRC GAIN WDMA */
	u32 ixsvhistTargetAddress[IS_MAX_PLANES];	/* DNS SV HIST WDMA */
	u32 ixhfTargetAddress[IS_MAX_PLANES];		/* DNS HF WDMA */
	u32 ixwrgbTargetAddress[IS_MAX_PLANES];		/* DNS REP RGB WDMA */
	u32 ixnoirwTargetAddress[IS_MAX_PLANES];	/* DNS REP NOISE WDMA */
	u32 ixbnrTargetAddress[IS_MAX_PLANES];		/* DNS BNR WDMA */
	u32 ixvTargetAddress[IS_MAX_PLANES];		/* TNR PREV WDMA */
	u32 ixwTargetAddress[IS_MAX_PLANES];		/* TNR PREV weight WDMA */
	u32 ixtTargetAddress[IS_MAX_PLANES];		/* TNR PREV RDMA */
	u32 ixgTargetAddress[IS_MAX_PLANES];		/* TNR PREV weight RDMA */
	u32 ixmTargetAddress[IS_MAX_PLANES];		/* TNR MOTION RDMA */
	u64 ixmKTargetAddress[IS_MAX_PLANES];		/* TNR MOTION KVA */
	u64 mexcTargetAddress[IS_MAX_PLANES];		/* ME or MCH out DMA */
	u64 orbxcTargetAddress[IS_MAX_PLANES];		/* ME or MCH out DMA */
	u32 ypnrdsTargetAddress[IS_MAX_PLANES];		/* YPP Y/UV L2 RDMA */
	u32 ypnoiTargetAddress[IS_MAX_PLANES];		/* YPP NOISE RDMA */
	u32 ypdgaTargetAddress[IS_MAX_PLANES];		/* YPP DRC GAIN RDMA */
	u32 ypsvhistTargetAddress[IS_MAX_PLANES];	/* YPP SV HIST RDMA */
	u32 sc0TargetAddress[IS_MAX_PLANES];
	u32 sc1TargetAddress[IS_MAX_PLANES];
	u32 sc2TargetAddress[IS_MAX_PLANES];
	u32 sc3TargetAddress[IS_MAX_PLANES];
	u32 sc4TargetAddress[IS_MAX_PLANES];
	u32 sc5TargetAddress[IS_MAX_PLANES];
	u32 clxsTargetAddress[IS_MAX_PLANES];		/* CLAHE IN DMA */
	u32 clxcTargetAddress[IS_MAX_PLANES];		/* CLAHE OUT DMA */

	/* Logical node information */
	struct is_sub_node	out_node;
	struct is_sub_node	cap_node;

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
	struct list_head	preview_list;
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

