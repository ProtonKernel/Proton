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

#ifndef IS_LIB_VRA_H
#define IS_LIB_VRA_H

#define VRA_DICO_API_VERSION			320

typedef unsigned char vra_boolean;

typedef unsigned int vra_uint32;

typedef signed int vra_int32;
typedef unsigned char *vpl_dram_adr_type;

enum api_vpl_type {
	VPL_OK				= 0,
	VPL_ERROR				= 1,
	VPL_SKIP_HW				= 2,
};

/* brief Whether Interrupts are set as "pulse" or "level" */
enum api_vpl_int_type {
	VPL_INT_PULSE = 0,
	VPL_INT_LEVEL = 1
};

/* memory pointer, size */
struct api_vpl_memory {
	vpl_dram_adr_type		addr;		/* The memory pointer */
	vra_uint32				size;		/* The memory size in bytes */
};

/* pixel position */
struct api_vpl_point {
	vra_int32	x;		/* The point's x position. */
	vra_int32	y;		/* The point's y position. */
};

/* rectangle size, position */
struct api_vpl_rectangle {
	struct api_vpl_point		topleft;	/* The rectangle's top left point.*/
	vra_uint32					width;		/* The rectangle's width.*/
	vra_uint32					height;		/* The rectangle's height.*/
};

/* face structure for VPL
 * includes needed information about face from face detection
 */
struct api_vpl_out_face {
	vra_uint32					unique_id;				/* The face's identification number.*/
	struct api_vpl_rectangle	rect;		/* The face's area on the image.*/
	vra_uint32					score;			/* The face's score, given by the FD engine.*/
};

struct api_vpl_org_crop_info {
	uint16_t                crop_offset_x;  /* The frame's origin_crop_x */
	uint16_t                crop_offset_y;  /* The frame's origin_crop_y */
	uint16_t                crop_width;     /* The frame's origin_crop_w */
	uint16_t                crop_height;    /* The frame's origin_crop_h */
	uint16_t                full_width;     /* The frame's origin_dma_in_w */
	uint16_t                full_height;    /* The frame's origin_dma_in_h */
};

/* Holds the width and the height of a frame (in pixels) */
struct api_vpl_frame_size {
	vra_uint32			width;		/* The frame's width */
	vra_uint32			height;		/* The frame's height */
	struct api_vpl_org_crop_info    org_crop_info;  /* The frame's org_input_dma_crop_info */
};

/* hold pointers to input buffers (R, G, and B) */
struct api_vpl_frame {
	vpl_dram_adr_type				in_image_addr;		/* The frame's image buffer */
	unsigned int					image_buff_dva;
	struct api_vpl_frame_size		frame_size;	/* The frame size (in pixel) */
};

/* brief Header of output list */
struct api_vpl_out_list_info {
	vra_uint32				stream_idx;

	/* The unique frame index of the results that are reported */
	vra_uint32				fr_ind;

	/*
	 * The input sizes of this frame.
	 * The Rects are described related to these coordiantes
	 */
	struct api_vpl_frame_size		in_sizes;
};

#define VPL_MAX_OUT_SIZES		3
struct api_vpl_post_frame_info {
	vra_uint32			stream_idx;
	vra_uint32			frame_idx;
	vra_uint32			net_idx;
	struct api_vpl_memory	input_desc;
	struct api_vpl_memory	out_desc[VPL_MAX_OUT_SIZES];
	vra_uint32			in_buf_dva;
};

/*
 * FrameWork callbacks.
 * 2 of them are generic and the rest includes Sensor Handle
 * that is set on initialization and describes the relevant sensor
 */

/*
 * The function that is called when output results of current frame are ready.
 * Notice that for ROI tracking this function is called after the ROI process
 * and no callback is called on "full frame" process.
 *
 * param NumAllFaces - Total number of faces found
 * param FacesPtr - Pointer to list of faces information
 *		(its length is NumAllFaces)
 * param OutListInfoPtr - Pointer to output list header
 */
typedef void (*on_sen_final_output_ready_ptr)(unsigned int num_all_faces,
		const struct api_vpl_out_face *faces_ptr,
		const struct api_vpl_out_list_info *out_list_info_ptr);

/* Framework callbacks */
struct vra_call_backs_str {
	on_sen_final_output_ready_ptr	sen_final_output_ready_ptr;
};

/* Description of the RAM section that will be used for HW DMAs output */
struct api_vpl_dma_ram {
	/* The base address of the RAM */
	vpl_dram_adr_type	base_adr;
	/* The total size of the RAM - for all DMAs */
	vra_uint32		total_size;
	/* Whether the RAM that is allocated for DMAs usage is cached */
};

#endif
