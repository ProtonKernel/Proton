#ifndef IS_DEVICE_CSI_H
#define IS_DEVICE_CSI_H

#include <linux/interrupt.h>
#include <media/v4l2-device.h>
#include "is-hw.h"
#include "is-type.h"
#include "is-framemgr.h"
#include "is-subdev-ctrl.h"
#include "is-work.h"
#include "is-device-camif-dma.h"

#define CSI_IRQ_NAME_LENGTH	16

#define CSI_NOTIFY_VSYNC	10
#define CSI_NOTIFY_VBLANK	11

#define CSI_LINE_RATIO		14	/* 70% */
#define CSI_ERR_COUNT		10  	/* 10frame */

#define CSI_WAIT_ABORT_TIMEOUT	(HZ / 4)

#define CSI_VALID_ENTRY_TO_CH(id) ((id) >= ENTRY_SSVC0 && (id) <= ENTRY_SSVC3)
#define CSI_ENTRY_TO_CH(id) ({BUG_ON(!CSI_VALID_ENTRY_TO_CH(id));id - ENTRY_SSVC0;}) /* range : vc0(0) ~ vc3(3) */
#define CSI_CH_TO_ENTRY(id) (id + ENTRY_SSVC0) /* range : ENTRY_SSVC0 ~ ENTRY_SSVC3 */

/* for use multi buffering */
#define CSI_GET_PREV_FRAMEPTR(frameptr, num_frames, offset) \
	((frameptr) == 0 ? (num_frames) - offset : (frameptr) - offset)
#define CSI_GET_NEXT_FRAMEPTR(frameptr, num_frames) \
	(((frameptr) + 1) % num_frames)

/* For frame id decoder */
#define F_ID_SIZE		4	/* for frame id decoder: 4 bit */
#define BUF_SWAP_CNT		2	/* for frame_id decoder & FRO mode: double buffering */
#define CHECK_ID_60FPS(id)		((((id) >> 0) & 0xF) == 1 ||	\
				(((id) >>  4) & 0xF) == 1 ||	\
				(((id) >>  8) & 0xF) == 1 ||	\
				(((id) >> 12) & 0xF) == 1 ||	\
				(((id) >> 16) & 0xF) == 1 ||	\
				(((id) >> 20) & 0xF) == 1 ||	\
				(((id) >> 24) & 0xF) == 1 ||	\
				(((id) >> 28) & 0xF) == 1)

/* for ebuf */
#define EBUF_MANUAL_MODE	0
#define EBUF_AUTO_MODE		1

#define MAX_NUM_EBUF_CH		4

extern int debug_csi;
extern struct is_sysfs_debug sysfs_debug;

enum is_csi_state {
	/* csis join ischain */
	CSIS_JOIN_ISCHAIN,
	/* one the fly output */
	CSIS_OTF_WITH_3AA,
	/* If it's dummy, H/W setting can't be applied */
	CSIS_DUMMY,
	/* WDMA flag */
	CSIS_DMA_ENABLE,
	CSIS_DMA_FLUSH_WAIT,
	CSIS_START_STREAM,
	/* runtime buffer done state for error */
	CSIS_BUF_ERR_VC0,
	CSIS_BUF_ERR_VC1,
	CSIS_BUF_ERR_VC2,
	CSIS_BUF_ERR_VC3,
	/* csi vc multibuffer setting state */
	CSIS_SET_MULTIBUF_VC0,
	CSIS_SET_MULTIBUF_VC1,
	CSIS_SET_MULTIBUF_VC2,
	CSIS_SET_MULTIBUF_VC3,
	/* use csi_dma disable on vvalid */
	CSIS_DMA_DISABLING,
};

/* MIPI-CSI interface */
enum itf_vc_buf_data_type {
	VC_BUF_DATA_TYPE_INVALID = -1,
	VC_BUF_DATA_TYPE_SENSOR_STAT1 = 0,
	VC_BUF_DATA_TYPE_GENERAL_STAT1,
	VC_BUF_DATA_TYPE_SENSOR_STAT2,
	VC_BUF_DATA_TYPE_GENERAL_STAT2,
	VC_BUF_DATA_TYPE_MAX
};

struct is_device_csi {
	u32				instance;	/* logical stream id */
	u32				ch;		/* physical CSI channel */
	u32				device_id;	/* pyysical sensor node id */
	u32				sensor_id;	/* physical module enum */

	u32 __iomem			*base_reg;
	u32 __iomem			*phy_reg;
	u32 __iomem			*mcb_reg;
	u32 __iomem			*ebuf_reg;
	resource_size_t			regs_start;
	resource_size_t			regs_end;
	int				irq;
	int				ebuf_irq;
	char				irq_name[CSI_IRQ_NAME_LENGTH];

	struct is_camif_wdma		*wdma;
	int				wdma_ch;
	unsigned int			wdma_ch_hint; /* use designated ch, if it is in DT. */

	/* debug */
	struct hw_debug_info		debug_info[DEBUG_FRAME_COUNT];

	/* for settle time */
	struct is_sensor_cfg	*sensor_cfg;

	/* image configuration */
	struct is_image		image;

	unsigned long			state;

	/* for DMA feature */
	struct is_framemgr		*framemgr;
	u32				overflow_cnt;
	u32				sw_checker;
	atomic_t			fcount;
	u32				hw_fcount;
	struct tasklet_struct		tasklet_csis_str;
	struct tasklet_struct		tasklet_csis_end;
	struct tasklet_struct		tasklet_csis_line;

	struct workqueue_struct		*workqueue;
	struct work_struct		wq_csis_dma[CSI_VIRTUAL_CH_MAX];
	struct is_work_list		work_list[CSI_VIRTUAL_CH_MAX];

	int				pre_dma_enable[CSI_VIRTUAL_CH_MAX];
	int				cur_dma_enable[CSI_VIRTUAL_CH_MAX];

	/* subdev slots for dma */
	struct is_subdev		*dma_subdev[CSI_VIRTUAL_CH_MAX];

	/* pointer address from device sensor */
	struct v4l2_subdev		**subdev;
	struct phy			*phy;

	ulong error_id[CSI_VIRTUAL_CH_MAX];
	u32 error_id_last[CSI_VIRTUAL_CH_MAX];
	u32 error_count;
	u32 error_count_vc_overlap;
	u32 tasklet_csis_end_count;

	atomic_t                        vblank_count; /* Increase at CSI frame end */
	atomic_t			vvalid; /* set 1 while vvalid period */

	char				name[IS_STR_LEN];
	u32				use_cphy;
	bool				potf;
	bool				f_id_dec; /* For frame id decoder in FRO mode */
	atomic_t			bufring_cnt; /* For double buffering in FRO mode */
	u32				dma_batch_num;
	u32				otf_batch_num;
	spinlock_t			dma_seq_slock;
	struct mutex			ebuf_lock;

	wait_queue_head_t		dma_flush_wait_q;
	bool				crc_flag;
	atomic_t			ebuf_rcount[MAX_NUM_EBUF_CH];
};

void csi_frame_start_inline(struct is_device_csi *csi);
void csi_hw_cdump_all(struct is_device_csi *csi);
void csi_hw_dump_all(struct is_device_csi *csi);

int __must_check is_csi_probe(void *parent, u32 instance, u32 ch, int wdma_ch_hint);
int __must_check is_csi_open(struct v4l2_subdev *subdev, struct is_framemgr *framemgr);
int __must_check is_csi_close(struct v4l2_subdev *subdev);
/* for DMA feature */
extern u32 __iomem *notify_fcount_sen0;
extern u32 __iomem *notify_fcount_sen1;
extern u32 __iomem *notify_fcount_sen2;
extern u32 __iomem *last_fcount0;
extern u32 __iomem *last_fcount1;
#endif
