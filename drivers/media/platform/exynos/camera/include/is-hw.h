/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_H
#define IS_HW_H

#include <linux/phy/phy.h>

#include "is-type.h"
#include "is-hw-chain.h"
#include "is-framemgr.h"
#include "is-device-ischain.h"
#include "../hardware/is-hw-control.h"

#define CSI_VIRTUAL_CH_0	0
#define CSI_VIRTUAL_CH_1	1
#define CSI_VIRTUAL_CH_2	2
#define CSI_VIRTUAL_CH_3	3
#define CSI_VIRTUAL_CH_MAX	4

#define IS_CSIS_VERSION(a, b, c, d) \
	(((a) << 24) + ((b) << 16) + ((c) << 8) + (d))

#define SENSOR_DUMMY_ELEMS	2

#define SHARED_META_INIT	0
#define SHARED_META_SHOT	1
#define SHARED_META_SHOT_DONE	2

/*
 * Get each lane speed (Mbps)
 * w : width, h : height, fps : framerate
 * bit : bit width per pixel, lanes : total lane number (1 ~ 4)
 * margin : S/W margin (15 means 15%)
 */
#define CSI_GET_LANE_SPEED(w, h, fps, bit, lanes, margin) \
	({u64 tmp; tmp = ((u64)w) * ((u64)h) * ((u64)fps) * ((u64)bit) / (lanes); \
	  tmp *= (100 + margin); tmp /= 100; tmp /= 1000000; (u32)tmp;})

/*
 * Get binning ratio.
 * The ratio is expressed by 0.5 step(500)
 * The improtant thing is that it was round off the ratio to the closet 500 unit.
 */
#define BINNING(x, y) (rounddown((x) * 1000 / (y), 500))

/*
 * Get size ratio.
 * w : width, h : height
 * The return value is defined of the enum ratio_size.
 */
#define SIZE_RATIO(w, h) ((w) * 10 / (h))

/*
 * This enum will be used in order to know the size ratio based upon RATIO mecro return value.
 */
enum ratio_size {
	RATIO_1_1		= 1,
	RATIO_11_9		= 12,
	RATIO_4_3		= 13,
	RATIO_3_2		= 15,
	RATIO_5_3		= 16,
	RATIO_16_9		= 17,
};

enum csis_hw_type {
	CSIS_LINK			= 0,
	CSIS_WDMA			= 1,
};

/*
 * This enum will be used for current error status by reading interrupt source.
 */
enum csis_hw_err_id {
	CSIS_ERR_ID,
	CSIS_ERR_CRC,
	CSIS_ERR_ECC,
	CSIS_ERR_WRONG_CFG,
	CSIS_ERR_OVERFLOW_VC,
	CSIS_ERR_LOST_FE_VC,
	CSIS_ERR_LOST_FS_VC,
	CSIS_ERR_SOT_VC,
	CSIS_ERR_INVALID_CODE_HS,
	CSIS_ERR_SOT_SYNC_HS,
	CSIS_ERR_MAL_CRC,
	CSIS_ERR_VRESOL_MISMATCH,
	CSIS_ERR_HRESOL_MISMATCH,
	CSIS_ERR_CRC_CPHY,
	CSIS_ERR_DMA_OTF_OVERLAP_VC,
	CSIS_ERR_DMA_DMAFIFO_FULL,
	CSIS_ERR_DMA_ABORT_DONE,
	CSIS_ERR_DMA_FRAME_DROP_VC,
	CSIS_ERR_DMA_LASTDATA_ERROR,
	CSIS_ERR_DMA_LASTADDR_ERROR,
	CSIS_ERR_DMA_FSTART_IN_FLUSH_VC,
	CSIS_ERR_DMA_C2COM_LOST_FLUSH_VC,
	CSIS_ERR_END
};

/*
 * This enum will be used in csi_hw_s_control api to set specific functions.
 */
enum csis_hw_control_id {
	CSIS_CTRL_INTERLEAVE_MODE,
	CSIS_CTRL_LINE_RATIO,
	CSIS_CTRL_BUS_WIDTH,
	CSIS_CTRL_DMA_ABORT_REQ,
	CSIS_CTRL_ENABLE_LINE_IRQ,
	CSIS_CTRL_PIXEL_ALIGN_MODE,
	CSIS_CTRL_LRTE,
	CSIS_CTRL_DESCRAMBLE,
};

/*
 * This struct will be used in csi_hw_g_irq_src api.
 * In csi_hw_g_irq_src, all interrupt source status should be
 * saved to this structure. otf_start, otf_end, dma_start, dma_end,
 * line_end fields has virtual channel bit set info.
 * Ex. If otf end interrupt occured in virtual channel 0 and 2 and
 *        dma end interrupt occured in virtual channel 0 only,
 *   .otf_end = 0b0101
 *   .dma_end = 0b0001
 */
struct csis_irq_src {
	u32			otf_start;
	u32			otf_end;
	u32			dma_start;
	u32			dma_end;
	u32			line_end;
	u32			dma_abort;
	bool			err_flag;
	ulong			err_id[CSI_VIRTUAL_CH_MAX];
	u32			ebuf_status;
};

struct is_vci_config {
	u32			map;
	u32			hwformat;
	u32			extformat;
	u32			type;
	u32			width;
	u32			height;
};

/*
 * This enum will be used for masking each interrupt masking.
 * The irq_ids params which masked by shifting this bit(id)
 * was sended to flite_hw_s_irq_msk.
 */
enum flite_hw_irq_id {
	FLITE_MASK_IRQ_START		= 0,
	FLITE_MASK_IRQ_END		= 1,
	FLITE_MASK_IRQ_OVERFLOW		= 2,
	FLITE_MASK_IRQ_LAST_CAPTURE	= 3,
	FLITE_MASK_IRQ_LINE		= 4,
	FLITE_MASK_IRQ_ALL,
};

/*
 * This enum will be used for current status by reading interrupt source.
 */
enum flite_hw_status_id {
	FLITE_STATUS_IRQ_SRC_START		= 0,
	FLITE_STATUS_IRQ_SRC_END		= 1,
	FLITE_STATUS_IRQ_SRC_OVERFLOW		= 2,
	FLITE_STATUS_IRQ_SRC_LAST_CAPTURE	= 3,
	FLITE_STATUS_OFY			= 4,
	FLITE_STATUS_OFCR			= 5,
	FLITE_STATUS_OFCB			= 6,
	FLITE_STATUS_MIPI_VALID			= 7,
	FLITE_STATUS_IRQ_SRC_LINE		= 8,
	FLITE_STATUS_ALL,
};

/*
 * This enum will be used in flite_hw_s_control api to set specific functions.
 */
enum flite_hw_control_id {
	FLITE_CTRL_TEST_PATTERN,
	FLITE_CTRL_LINE_RATIO,
};

struct is_hardware;
/*
 * ******************
 * MIPI-CSIS H/W APIS
 * ******************
 */
u32 csi_hw_get_version(u32 __iomem *base_reg);
void csi_hw_phy_otp_config(u32 __iomem *base_reg, u32 instance);
int csi_hw_get_ppc_mode(u32 __iomem *base_reg);
u32 csi_hw_s_fcount(u32 __iomem *base_reg, u32 vc, u32 count);
u32 csi_hw_g_fcount(u32 __iomem *base_reg, u32 vc);
int csi_hw_reset(u32 __iomem *base_reg);
int csi_hw_s_settle(u32 __iomem *base_reg, u32 settle);
int csi_hw_s_phy_sctrl_n(u32 __iomem *base_reg, u32 ctrl, u32 n);
int csi_hw_s_phy_bctrl_n(u32 __iomem *base_reg, u32 ctrl, u32 n);
int csi_hw_s_lane(u32 __iomem *base_reg, struct is_image *img, u32 lanes, u32 mipi_speed, u32 use_cphy);
int csi_hw_s_control(u32 __iomem *base_reg, u32 id, u32 value);
int csi_hw_s_config(u32 __iomem *base_reg, u32 channel, struct is_vci_config *config,
	u32 width, u32 height, bool potf);
int csi_hw_s_irq_msk(u32 __iomem *base_reg, bool on, bool f_id_dec);
int csi_hw_g_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, bool clear);
int csi_hw_enable(u32 __iomem *base_reg, u32 use_cphy);
int csi_hw_disable(u32 __iomem *base_reg);
int csi_hw_dump(u32 __iomem *base_reg);
int csi_hw_vcdma_dump(u32 __iomem *base_reg);
int csi_hw_vcdma_cmn_dump(u32 __iomem *base_reg);
int csi_hw_phy_dump(u32 __iomem *base_reg, u32 instance);
int csi_hw_common_dma_dump(u32 __iomem *base_reg);
#if defined(ENABLE_CLOG_RESERVED_MEM)
int csi_hw_cdump(u32 __iomem *base_reg);
int csi_hw_vcdma_cdump(u32 __iomem *base_reg);
int csi_hw_vcdma_cmn_cdump(u32 __iomem *base_reg);
int csi_hw_phy_cdump(u32 __iomem *base_reg, u32 instance);
int csi_hw_common_dma_cdump(u32 __iomem *base_reg);
#endif
void csi_hw_dma_reset(u32 __iomem *base_reg);
void csi_hw_s_frameptr(u32 __iomem *base_reg, u32 vc, u32 number, bool clear);
u32 csi_hw_g_frameptr(u32 __iomem *base_reg, u32 vc);
void csi_hw_s_dma_addr(u32 __iomem *base_reg, u32 vc, u32 number, u32 addr);
void csi_hw_s_multibuf_dma_addr(u32 __iomem *base_reg, u32 vc, u32 number, u32 addr);
void csi_hw_s_output_dma(u32 __iomem *base_reg, u32 vc, bool enable);
bool csi_hw_g_output_dma_enable(u32 __iomem *base_reg, u32 vc);
bool csi_hw_g_output_cur_dma_enable(u32 __iomem *base_reg, u32 vc);
#ifdef CONFIG_USE_SENSOR_GROUP
int csi_hw_s_config_dma(u32 __iomem *base_reg, u32 channel, struct is_frame_cfg *cfg, u32 hwformat, u32 dummy_pixel);
#else
int csi_hw_s_config_dma(u32 __iomem *base_reg, u32 channel, struct is_image *image, u32 hwformat, u32 dummy_pixel);
#endif
int csi_hw_dma_common_reset(u32 __iomem *base_reg, bool on);
int csi_hw_s_dma_common_dynamic(u32 __iomem *base_reg, size_t size, u32 dma_ch);
int csi_hw_s_dma_common(u32 __iomem *base_reg);
int csi_hw_s_dma_common_pattern_enable(u32 __iomem *base_reg, u32 width, u32 height, u32 fps, u32 clk);
void csi_hw_s_dma_common_pattern_disable(u32 __iomem *base_reg);
int csi_hw_s_dma_common_votf_cfg(u32 __iomem *base_reg, u32 width, u32 dma_ch, u32 vc, bool on);
int csi_hw_s_dma_common_frame_id_decoder(u32 __iomem *base_reg, u32 dma_ch,
		u32 enable);
int csi_hw_g_dma_common_frame_id(u32 __iomem *base_reg, u32 batch_num, u32 *frame_id);
int csi_hw_clear_fro_count(u32 __iomem *dma_top_reg, u32 __iomem *vc_reg);
int csi_hw_s_fro_count(u32 __iomem *vc_cmn_reg, u32 batch_num, u32 vc);

int csi_hw_s_dma_irq_msk(u32 __iomem *base_reg, bool on);
int csi_hw_g_dma_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, bool clear);
int csi_hw_g_dma_irq_src_vc(u32 __iomem *base_reg, struct csis_irq_src *src, u32 vc_abs, bool clear);
int csi_hw_s_config_dma_cmn(u32 __iomem *base_reg, u32 vc, u32 actual_vc, u32 hwformat, bool potf);

int csi_hw_s_phy_default_value(u32 __iomem *base_reg, u32 instance);
int csi_hw_s_phy_config(u32 __iomem *base_reg,
	u32 lanes, u32 mipi_speed, u32 settle, u32 instance);
int csi_hw_s_phy_set(struct phy *phy, u32 lanes, u32 mipi_speed,
		u32 settle, u32 instance, u32 use_cphy);

void csi_hw_s_mcb_qch(u32 __iomem *base_reg, bool on);
void csi_hw_s_ebuf_enable(u32 __iomem *base_reg, bool on, u32 ebuf_ch, int mode);
void csi_hw_s_cfg_ebuf(u32 __iomem *base_reg, u32 ebuf_ch, u32 vc, u32 width,
		u32 height);
void csi_hw_g_ebuf_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src,
		bool clear);
void csi_hw_s_ebuf_fake_sign(u32 __iomem *base_reg, u32 ebuf_ch);
/*
 * ************************************
 * ISCHAIN AND CAMIF CONFIGURE H/W APIS
 * ************************************
 */
/*
 * It's for hw version
 */
#define HW_SET_VERSION(first, second, third, fourth) \
	(((first) << 24) | ((second) << 16) | ((third) << 8) | ((fourth) << 0))

/*
 * This enum will be used in is_hw_s_ctrl api.
 */
enum hw_s_ctrl_id {
	HW_S_CTRL_FULL_BYPASS,
	HW_S_CTRL_CHAIN_IRQ,
	HW_S_CTRL_HWFC_IDX_RESET,
	HW_S_CTRL_MCSC_SET_INPUT,
};

/*
 * This enum will be used in is_hw_g_ctrl api.
 */
enum hw_g_ctrl_id {
	HW_G_CTRL_FRM_DONE_WITH_DMA,
	HW_G_CTRL_HAS_MCSC,
	HW_G_CTRL_HAS_VRA_CH1_ONLY,
};

int is_hw_group_cfg(void *group_data);
int is_hw_group_open(void *group_data);
void is_hw_camif_init(void);
int is_hw_camif_cfg(void *sensor_data);
int is_hw_camif_open(void *sensor_data);
#ifdef USE_CAMIF_FIX_UP
int is_hw_camif_fix_up(struct is_device_sensor *sensor);
#else
#define is_hw_camif_fix_up(a) ({ int __retval = 0; do {} while (0); __retval; })
#endif
void is_hw_ischain_qe_cfg(void);
int is_hw_ischain_cfg(void *ischain_data);
int is_hw_ischain_enable(struct is_device_ischain *device);
int is_hw_ischain_disable(struct is_device_ischain *device);
int is_hw_lme_hw_bug_wa(u32 hw_id, bool enable);
int is_hw_get_address(void *itfc_data, void *pdev_data, int hw_id);
int is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id);
int is_hw_request_irq(void *itfc_data, int hw_id);
int is_hw_slot_id(int hw_id);
int is_get_hw_list(int group_id, int *hw_list);
int is_hw_s_ctrl(void *itfc_data, int hw_id, enum hw_s_ctrl_id id, void *val);
int is_hw_g_ctrl(void *itfc_data, int hw_id, enum hw_g_ctrl_id id, void *val);
int is_hw_query_cap(void *cap_data, int hw_id);
int is_hw_check_gframe_skip(void *group_data);
int is_hw_shared_meta_update(struct is_device_ischain *device,
		struct is_group *group, struct is_frame *frame, int shot_done_flag);
void __iomem *is_hw_get_sysreg(ulong core_regs);
u32 is_hw_find_settle(u32 mipi_speed, u32 use_cphy);
#ifdef ENABLE_FULLCHAIN_OVERFLOW_RECOVERY
int is_hw_overflow_recovery(void);
#endif
unsigned int get_dma(struct is_device_sensor *device, u32 *dma_ch);
void is_hw_interrupt_relay(struct is_group *group, void *hw_ip);
void is_hw_configure_llc(bool on, struct is_device_ischain *device, ulong *llc_state);
void is_hw_configure_bts_scen(struct is_resourcemgr *resourcemgr, int scenario_id);
#ifdef ENABLE_LOGICAL_VIDEO_NODE
int is_hw_get_output_slot(u32 vid);
int is_hw_get_capture_slot(struct is_frame *frame, u32 **taddr, u64 **taddr_k, u32 vid);
#endif
void * is_get_dma_blk(int type);
int is_get_dma_id(u32 vid);
int is_hw_check_changed_chain_id(struct is_hardware *hardware, u32 hw_id, u32 instance);
void is_hw_fill_target_address(u32 hw_id, struct is_frame *dst,	struct is_frame *src,
	bool reset);
#define TADDR_COPY(dst, src, taddr, reset)				\
	do {								\
		memcpy(dst->taddr, src->taddr, sizeof(src->taddr));	\
		if (reset)						\
			memset(src->taddr, 0x0, sizeof(src->taddr));	\
	} while (0);

/*
 * ********************
 * RUNTIME-PM FUNCTIONS
 * ********************
 */
int is_sensor_runtime_suspend_pre(struct device *dev);
int is_sensor_runtime_resume_pre(struct device *dev);

int is_ischain_runtime_suspend_post(struct device *dev);
int is_ischain_runtime_resume_pre(struct device *dev);
int is_ischain_runtime_resume_post(struct device *dev);

int is_ischain_runtime_suspend(struct device *dev);
int is_ischain_runtime_resume(struct device *dev);

int is_runtime_suspend_pre(struct device *dev);
int is_runtime_suspend_post(struct device *dev);
int is_runtime_resume_post(struct device *dev);

void is_hw_djag_get_input(struct is_device_ischain *ischain, u32 *djag_in);
void is_hw_djag_adjust_out_size(struct is_device_ischain *ischain,
					u32 in_width, u32 in_height,
					u32 *out_width, u32 *out_height);

/*
 * ************
 * FP/SIMD APIS
 * ************
 */
#if defined(ENABLE_FPSIMD_FOR_USER)
#if defined(VH_FPSIMD_API) /* Using android vendor hook to save and restore fpsimd regs */
extern void is_fpsimd_save_state(struct is_fpsimd_state *state);
extern void is_fpsimd_load_state(struct is_fpsimd_state *state);
void is_fpsimd_get_isr(void);
void is_fpsimd_put_isr(void);
void is_fpsimd_get_func(void);
void is_fpsimd_put_func(void);
void is_fpsimd_get_task(void);
void is_fpsimd_put_task(void);
void is_fpsimd_set_task_using(struct task_struct *t, struct is_kernel_fpsimd_state *kst);
int is_fpsimd_probe(void);
#else
#define is_fpsimd_get_isr()			kernel_neon_begin()
#define is_fpsimd_put_isr()			kernel_neon_end()
#define is_fpsimd_get_func()			fpsimd_get()
#define is_fpsimd_put_func()			fpsimd_put()
#define is_fpsimd_get_task()			do { } while(0)
#define is_fpsimd_put_task()			do { } while(0)
#define is_fpsimd_set_task_using(t, kst)	fpsimd_set_task_using(t)
#define is_fpsimd_probe()			({0;})
#endif
#else
#define is_fpsimd_get_isr()			do { } while(0)
#define is_fpsimd_put_isr()			do { } while(0)
#define is_fpsimd_get_func()			do { } while(0)
#define is_fpsimd_put_func()			do { } while(0)
#define is_fpsimd_get_task()			do { } while(0)
#define is_fpsimd_put_task()			do { } while(0)
#define is_fpsimd_set_task_using(t, kst)	do { } while(0)
#define is_fpsimd_probe()			({0;})
#endif

extern void is_clean_dcache_area(void *kaddr, u32 size);
#endif
