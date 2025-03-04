/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_H
#define __SND_SOC_VTS_H

#include <sound/memalloc.h>
#include <linux/pm_wakeup.h>
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/imgloader.h>
#endif
#include <soc/samsung/memlogger.h>
#include <soc/samsung/sysevent.h>

#include "vts_shared_info.h"
#define TEST_WAKEUP

/* SYSREG_VTS */
#define VTS_DEBUG			(0x0404)
#define VTS_DMIC_CLK_CTRL		(0x0408)
#define VTS_HWACG_CM4_CLKREQ	(0x0428)
#define VTS_DMIC_CLK_CON		(0x0434)
#define VTS_SYSPOWER_CTRL		(0x0500)
#define VTS_SYSPOWER_STATUS		(0x0504)
#define VTS_MIF_REQ_OUT			(0x0200)
#define VTS_MIF_REQ_ACK_IN		(0x0204)

/* VTS_DEBUG */
#define VTS_NMI_EN_BY_WDT_OFFSET	(0)
#define VTS_NMI_EN_BY_WDT_SIZE		(1)
/* VTS_DMIC_CLK_CTRL */
#define VTS_CG_STATUS_OFFSET            (5)
#define VTS_CG_STATUS_SIZE		(1)
#define VTS_CLK_ENABLE_OFFSET		(4)
#define VTS_CLK_ENABLE_SIZE		(1)
#define VTS_CLK_SEL_OFFSET		(0)
#define VTS_CLK_SEL_SIZE		(1)
/* VTS_HWACG_CM4_CLKREQ */
#define VTS_MASK_OFFSET			(0)
#define VTS_MASK_SIZE			(32)
/* VTS_DMIC_CLK_CON */
#define VTS_ENABLE_CLK_GEN_OFFSET       (0)
#define VTS_ENABLE_CLK_GEN_SIZE         (1)
#define VTS_SEL_EXT_DMIC_CLK_OFFSET     (1)
#define VTS_SEL_EXT_DMIC_CLK_SIZE       (1)
#define VTS_ENABLE_CLK_CLK_GEN_OFFSET   (14)
#define VTS_ENABLE_CLK_CLK_GEN_SIZE     (1)

/* VTS_SYSPOWER_CTRL */
#define VTS_SYSPOWER_CTRL_OFFSET	(0)
#define VTS_SYSPOWER_CTRL_SIZE		(1)
/* VTS_SYSPOWER_STATUS */
#define VTS_SYSPOWER_STATUS_OFFSET	(0)
#define VTS_SYSPOWER_STATUS_SIZE	(1)


#define VTS_DMIC_ENABLE_DMIC_IF		(0x0000)
#define VTS_DMIC_CONTROL_DMIC_IF	(0x0004)
/* VTS_DMIC_ENABLE_DMIC_IF */
#define VTS_DMIC_ENABLE_DMIC_IF_OFFSET	(31)
#define VTS_DMIC_ENABLE_DMIC_IF_SIZE	(1)
#define VTS_DMIC_PERIOD_DATA2REQ_OFFSET	(16)
#define VTS_DMIC_PERIOD_DATA2REQ_SIZE	(2)
/* VTS_DMIC_CONTROL_DMIC_IF */
#define VTS_DMIC_HPF_EN_OFFSET		(31)
#define VTS_DMIC_HPF_EN_SIZE		(1)
#define VTS_DMIC_HPF_SEL_OFFSET		(28)
#define VTS_DMIC_HPF_SEL_SIZE		(1)
#define VTS_DMIC_CPS_SEL_OFFSET		(27)
#define VTS_DMIC_CPS_SEL_SIZE		(1)
#define VTS_DMIC_GAIN_OFFSET		(24)
#define VTS_DMIC_1DB_GAIN_OFFSET	(21)
#define VTS_DMIC_GAIN_SIZE		(3)
#define VTS_DMIC_DMIC_SEL_OFFSET	(18)
#define VTS_DMIC_DMIC_SEL_SIZE		(1)
#define VTS_DMIC_RCH_EN_OFFSET		(17)
#define VTS_DMIC_RCH_EN_SIZE		(1)
#define VTS_DMIC_LCH_EN_OFFSET		(16)
#define VTS_DMIC_LCH_EN_SIZE		(1)
#define VTS_DMIC_SYS_SEL_OFFSET		(12)
#define VTS_DMIC_SYS_SEL_SIZE		(2)
#define VTS_DMIC_POLARITY_CLK_OFFSET	(10)
#define VTS_DMIC_POLARITY_CLK_SIZE	(1)
#define VTS_DMIC_POLARITY_OUTPUT_OFFSET	(9)
#define VTS_DMIC_POLARITY_OUTPUT_SIZE	(1)
#define VTS_DMIC_POLARITY_INPUT_OFFSET	(8)
#define VTS_DMIC_POLARITY_INPUT_SIZE	(1)
#define VTS_DMIC_OVFW_CTRL_OFFSET	(4)
#define VTS_DMIC_OVFW_CTRL_SIZE		(1)
#define VTS_DMIC_CIC_SEL_OFFSET		(0)
#define VTS_DMIC_CIC_SEL_SIZE		(1)

/* CM4 */
#define VTS_CM4_R(x)			(0x0010 + (x * 0x4))
#define VTS_CM4_PC			(0x0004)

#if IS_ENABLED(CONFIG_SOC_EXYNOS8895) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS9810)
#define VTS_IRQ_VTS_ERROR               (16)
#define VTS_IRQ_VTS_BOOT_COMPLETED      (17)
#define VTS_IRQ_VTS_IPC_RECEIVED        (18)
#define VTS_IRQ_VTS_VOICE_TRIGGERED     (19)
#define VTS_IRQ_VTS_PERIOD_ELAPSED      (20)
#define VTS_IRQ_VTS_REC_PERIOD_ELAPSED  (21)
#define VTS_IRQ_VTS_DBGLOG_BUFZERO      (22)
#define VTS_IRQ_VTS_DBGLOG_BUFONE       (23)
#define VTS_IRQ_VTS_AUDIO_DUMP          (24)
#define VTS_IRQ_VTS_LOG_DUMP            (25)
#define VTS_IRQ_COUNT                   (26)
#define VTS_IRQ_AP_IPC_RECEIVED		(0)
#define VTS_IRQ_AP_SET_DRAM_BUFFER	(1)
#define VTS_IRQ_AP_START_RECOGNITION	(2)
#define VTS_IRQ_AP_STOP_RECOGNITION	(3)
#define VTS_IRQ_AP_START_COPY		(4)
#define VTS_IRQ_AP_STOP_COPY		(5)
#define VTS_IRQ_AP_SET_MODE		(6)
#define VTS_IRQ_AP_POWER_DOWN		(7)
#define VTS_IRQ_AP_TARGET_SIZE		(8)
#define VTS_IRQ_AP_SET_REC_BUFFER	(9)
#define VTS_IRQ_AP_START_REC		(10)
#define VTS_IRQ_AP_STOP_REC		(11)
#define VTS_IRQ_AP_RESTART_RECOGNITION	(13)
#define VTS_IRQ_AP_TEST_COMMAND		(15)
#else
#define VTS_IRQ_VTS_ERROR               (0)
#define VTS_IRQ_VTS_BOOT_COMPLETED      (1)
#define VTS_IRQ_VTS_IPC_RECEIVED        (2)
#define VTS_IRQ_VTS_VOICE_TRIGGERED     (3)
#define VTS_IRQ_VTS_PERIOD_ELAPSED      (4)
#define VTS_IRQ_VTS_REC_PERIOD_ELAPSED  (5)
#define VTS_IRQ_VTS_DBGLOG_BUFZERO      (6)
#define VTS_IRQ_VTS_DBGLOG_BUFONE       (7)
#define VTS_IRQ_VTS_AUDIO_DUMP          (8)
#define VTS_IRQ_VTS_LOG_DUMP            (9)
#define VTS_IRQ_COUNT                   (10)
#define VTS_IRQ_VTS_SLIF_DUMP           (11)
#define VTS_IRQ_VTS_CP_WAKEUP           (15)
#define VTS_IRQ_AP_IPC_RECEIVED         (16)
#define VTS_IRQ_AP_SET_DRAM_BUFFER      (17)
#define VTS_IRQ_AP_START_RECOGNITION    (18)
#define VTS_IRQ_AP_STOP_RECOGNITION     (19)
#define VTS_IRQ_AP_START_COPY           (20)
#define VTS_IRQ_AP_STOP_COPY            (21)
#define VTS_IRQ_AP_SET_MODE             (22)
#define VTS_IRQ_AP_POWER_DOWN           (23)
#define VTS_IRQ_AP_TARGET_SIZE          (24)
#define VTS_IRQ_AP_SET_REC_BUFFER       (25)
#define VTS_IRQ_AP_START_REC            (26)
#define VTS_IRQ_AP_STOP_REC             (27)
#define VTS_IRQ_AP_GET_VERSION			(28)
#define VTS_IRQ_AP_RESTART_RECOGNITION  (29)
#define VTS_IRQ_AP_TEST_COMMAND         (31)
#endif

#if (IS_ENABLED(CONFIG_SOC_EXYNOS9830) || IS_ENABLED(CONFIG_SOC_EXYNOS2100))
#define VTS_DMIC_IF_ENABLE_DMIC_IF		(0x1000)
#define VTS_DMIC_IF_ENABLE_DMIC_AUD0		(0x6)
#define VTS_DMIC_IF_ENABLE_DMIC_AUD1		(0x7)
#define VTS_DMIC_IF_ENABLE_DMIC_AUD2		(0x8)
#endif

#define VTS_IRQ_LIMIT			(32)

#define VTS_BAAW_BASE			(0x60000000)
#define VTS_BAAW_SRC_START_ADDRESS	(0x10000)
#define VTS_BAAW_SRC_END_ADDRESS	(0x10004)
#define VTS_BAAW_REMAPPED_ADDRESS	(0x10008)
#define VTS_BAAW_INIT_DONE		(0x1000C)

#define SICD_SOC_DOWN_OFFSET	(0x18C)
#define SICD_MIF_DOWN_OFFSET	(0x19C)

#define BUFFER_BYTES_MAX (0xa0000)
#define PERIOD_BYTES_MIN (SZ_4)
#define PERIOD_BYTES_MAX (BUFFER_BYTES_MAX / 2)

#define SOUND_MODEL_SIZE_MAX (SZ_32K)
#define SOUND_MODEL_COUNT (3)

#if (IS_ENABLED(CONFIG_SOC_EXYNOS9820) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS9830) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS2100))
#define SOUND_MODEL_SVOICE_OFFSET	(0xAA800)
#define SOUND_MODEL_GOOGLE_OFFSET	(0xB2B00)
#else
#define SOUND_MODEL_SVOICE_OFFSET	(0x2A800)
#define SOUND_MODEL_SVOICE_OFFSET	(0x32B00)
#endif

/* DRAM for copying VTS firmware logs */
#define LOG_BUFFER_BYTES_MAX	(0x2000)
#define VTS_SRAMLOG_MSGS_OFFSET (0x59000)
#define VTS_SRAMLOG_SIZE_MAX	(SZ_2K)		/* SZ_2K : 0x800 */

/* VTS firmware version information offset */
#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
#define VTSFW_VERSION_OFFSET	(0xac)
#define DETLIB_VERSION_OFFSET	(0xa8)
#elif IS_ENABLED(CONFIG_SOC_EXYNOS9830)
#define VTSFW_VERSION_OFFSET	(0x9c)
#define DETLIB_VERSION_OFFSET	(0x98)
#else
#define VTSFW_VERSION_OFFSET	(0x7c)
#define DETLIB_VERSION_OFFSET	(0x78)
#endif

/* svoice net(0x8000) & grammar(0x300) binary sizes defined in firmware */
#define SOUND_MODEL_SVOICE_SIZE_MAX (0x8000 + 0x300)

/* google binary size defined in firmware
 * (It is same with VTSDRV_MISC_MODEL_BIN_MAXSZ)
 */
#define SOUND_MODEL_GOOGLE_SIZE_MAX (0x10800)

/* VTS Model Binary Max buffer sizes */
#define VTS_MODEL_SVOICE_BIN_MAXSZ     (SOUND_MODEL_SVOICE_SIZE_MAX)
#define VTS_MODEL_GOOGLE_BIN_MAXSZ     (SOUND_MODEL_GOOGLE_SIZE_MAX)

#define VTS_ERR_HARD_FAULT	(0x1)
#define VTS_ERR_BUS_FAULT	(0x3)

enum ipc_state {
	IDLE,
	SEND_MSG,
	SEND_MSG_OK,
	SEND_MSG_FAIL,
};

enum trigger {
	TRIGGER_NONE	= -1,
	TRIGGER_SVOICE	= 0,
	TRIGGER_GOOGLE	= 1,
	TRIGGER_SENSORY	= 2,
	TRIGGER_COUNT,
};

enum vts_platform_type {
	PLATFORM_VTS_NORMAL_RECORD,
	PLATFORM_VTS_TRIGGER_RECORD,
};

enum vts_dmic_sel {
	DPDM,
	APDM,
};

enum executionmode {
	//default is off
	VTS_OFF_MODE			= 0,
	//voice-trig-mode:Both LPSD & Trigger are enabled
	VTS_VOICE_TRIGGER_MODE		= 1,
	//sound-detect-mode: Low Power sound Detect
	VTS_SOUND_DETECT_MODE		= 2,
	//vt-always-mode: key phrase Detection only(Trigger)
	VTS_VT_ALWAYS_ON_MODE		= 3,
	//google-trigger: key phrase Detection only(Trigger)
	VTS_GOOGLE_TRIGGER_MODE		= 4,
	//sensory-trigger: key phrase Detection only(Trigger)
	VTS_SENSORY_TRIGGER_MODE	= 5,
	//off:voice-trig-mode:Both LPSD & Trigger are enabled
	VTS_VOICE_TRIGGER_MODE_OFF	= 6,
	//off:sound-detect-mode: Low Power sound Detect
	VTS_SOUND_DETECT_MODE_OFF	= 7,
	//off:vt-always-mode: key phrase Detection only(Trigger)
	VTS_VT_ALWAYS_ON_MODE_OFF	= 8,
	//off:google-trigger: key phrase Detection only(Trigger)
	VTS_GOOGLE_TRIGGER_MODE_OFF	= 9,
	//off:sensory-trigger: key phrase Detection only(Trigger)
	VTS_SENSORY_TRIGGER_MODE_OFF	= 10,
	VTS_MODE_COUNT,
};

enum vts_dump_type {
	KERNEL_PANIC_DUMP,
	VTS_FW_NOT_READY,
	VTS_IPC_TRANS_FAIL,
	VTS_FW_ERROR,
	VTS_ITMON_ERROR,
	RUNTIME_SUSPEND_DUMP,
	VTS_DUMP_LAST,
};

enum vts_test_command {
	VTS_START_SLIFDUMP	= 0x00000010,
	VTS_STOP_SLIFDUMP	= 0x00000020,
	VTS_KERNEL_TIME		= 0x00000040,
	VTS_DISABLE_LOGDUMP	= 0x01000000,
	VTS_ENABLE_LOGDUMP	= 0x02000000,
	VTS_DISABLE_AUDIODUMP	= 0x04000000,
	VTS_ENABLE_AUDIODUMP	= 0x08000000,
	VTS_DISABLE_DEBUGLOG	= 0x10000000,
	VTS_ENABLE_DEBUGLOG	= 0x20000000,
	VTS_ENABLE_SRAM_LOG	= 0x80000000,
};

struct vts_ipc_msg {
	int msg;
	u32 values[3];
};

enum vts_micconf_type {
	VTS_MICCONF_FOR_RECORD	= 0,
	VTS_MICCONF_FOR_TRIGGER	= 1,
	VTS_MICCONF_FOR_GOOGLE	= 2,
};

enum vts_state_machine {
	VTS_STATE_NONE			= 0,	/* runtime_suspended state */
	VTS_STATE_VOICECALL		= 1,	/* sram L2Cache call state */
	VTS_STATE_RUNTIME_RESUMING	= 2,	/* runtime_resume started */
	VTS_STATE_RUNTIME_RESUMED	= 3,	/* runtime_resume done */
	VTS_STATE_RECOG_STARTED		= 4,	/* Recognization started */
	VTS_STATE_RECOG_TRIGGERED	= 5,	/* Recognize triggered */
	VTS_STATE_SEAMLESS_REC_STARTED	= 6,	/* seamless record started */
	VTS_STATE_SEAMLESS_REC_STOPPED	= 7,	/* seamless record stopped */
	VTS_STATE_RECOG_STOPPED		= 8,	/* Recognization stopped */
	VTS_STATE_RUNTIME_SUSPENDING	= 9,	/* runtime_suspend started */
	VTS_STATE_RUNTIME_SUSPENDED	= 10,	/* runtime_suspend done */
};

enum vts_rec_state_machine {
	VTS_REC_STATE_STOP  = 0,
	VTS_REC_STATE_START = 1,
};

enum vts_tri_state_machine {
	VTS_TRI_STATE_COPY_STOP  = 0,
	VTS_TRI_STATE_COPY_START = 1,
};

enum vts_clk_src {
	VTS_CLK_SRC_RCO			= 0,
	VTS_CLK_SRC_AUD0		= 1,
	VTS_CLK_SRC_AUD1		= 2,
};

struct vts_model_bin_info {
	unsigned char *data;
	size_t	actual_sz;
	size_t	max_sz;
	bool loaded;
};

struct vts_dbg_dump {
	long long time;
	enum vts_dump_type dbg_type;
	unsigned int gpr[17];
	char *sram_log;
	char *sram_fw;
};

struct vts_data {
	struct platform_device *pdev;
	struct snd_soc_component *cmpnt;
	void __iomem *sfr_base;
	void __iomem *baaw_base;
	void __iomem *sram_base;
	void __iomem *dmic_if0_base;
	void __iomem *dmic_ahb0_base;
	void __iomem *timer0_base;
#if IS_ENABLED(CONFIG_SOC_EXYNOS9820) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS9830) || \
	IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	void __iomem *dmic_if1_base;
#endif
	void __iomem *gpr_base;

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	void __iomem *intmem_code;
	void __iomem *intmem_data;
	void __iomem *intmem_pcm;
#endif
	void __iomem *sicd_base;

	size_t sram_size;
	phys_addr_t sram_phys;
	struct regmap *regmap_dmic;
	struct clk *clk_rco;
	struct clk *clk_dmic;
	struct clk *clk_dmic_if;
	struct clk *clk_dmic_sync;
#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
	struct clk *mux_clk_dmic_if;
#endif
	struct clk *clk_sys;
	struct pinctrl *pinctrl;
	struct mutex mutex_pin;
	unsigned int vtsfw_version;
	unsigned int vtsdetectlib_version;
	const struct firmware *firmware;
	unsigned int vtsdma_count;
	unsigned long syssel_rate;
	unsigned int target_sysclk;
	unsigned long sysclk_rate;
	struct platform_device *pdev_mailbox;
	struct platform_device *pdev_vtsdma[2];
	int irq[VTS_IRQ_LIMIT];
	enum ipc_state ipc_state_ap;
	wait_queue_head_t ipc_wait_queue;
	spinlock_t ipc_spinlock;
	struct mutex ipc_mutex;
	u32 dma_area_vts;
	struct snd_dma_buffer dmab;
	struct snd_dma_buffer dmab_rec;
	struct snd_dma_buffer dmab_log;
	struct snd_dma_buffer dmab_model;
	u32 target_size;
	enum trigger active_trigger;
	u32 voicerecog_start;
	enum executionmode exec_mode;
	bool vts_ready;
	unsigned long sram_acquired;
	bool enabled;
	bool running;
	bool voicecall_enabled;
	struct snd_soc_card *card;
	int micclk_init_cnt;
	unsigned int mic_ready;
	enum vts_dmic_sel dmic_if;
	bool irq_state;
	u32 lpsdgain;
	u32 dmicgain;
	u32 amicgain;
	char *sramlog_baseaddr;
	u32 running_ipc;
	struct wakeup_source *wake_lock;
	unsigned int vts_state;
	unsigned int vts_tri_state;
	unsigned int vts_rec_state;
	u32 fw_logfile_enabled;
	u32 fw_logger_enabled;
	bool audiodump_enabled;
	bool logdump_enabled;
	struct vts_model_bin_info svoice_info;
	struct vts_model_bin_info google_info;
	spinlock_t state_spinlock;
	struct notifier_block pm_nb;
	struct notifier_block itmon_nb;
	struct vts_dbg_dump p_dump[VTS_DUMP_LAST];
	bool slif_dump_enabled;
	enum vts_clk_src clk_path;
	bool imgloader;
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	struct imgloader_desc	vts_imgloader_desc;
#endif
	struct memlog *log_desc;
	struct memlog_obj *kernel_log_file_obj;
	struct memlog_obj *kernel_log_obj;
	struct memlog_obj *dump_file_obj;
	struct memlog_obj *dump_obj;
	struct memlog_obj *fw_log_file_obj;
	struct memlog_obj *fw_log_obj;
	struct sysevent_desc sysevent_desc;
	struct sysevent_device *sysevent_dev;
	int google_version;
	struct vts_shared_info *shared_info;
};

struct vts_platform_data {
	unsigned int id;
	struct platform_device *pdev_vts;
	struct device *dev;
	struct vts_data *vts_data;
	struct snd_pcm_substream *substream;
	enum vts_platform_type type;
	unsigned int pointer;
};

extern int vts_start_ipc_transaction(struct device *dev,
	struct vts_data *data, int msg, u32 (*values)[3], int atomic, int sync);
extern int vts_send_ipc_ack(struct vts_data *data, u32 result);
extern void vts_register_dma(struct platform_device *pdev_vts,
		struct platform_device *pdev_vts_dma, unsigned int id);
extern int vts_set_dmicctrl(struct platform_device *pdev,
	int micconf_type, bool enable);
extern int vts_sound_machine_drv_register(void);
extern void vts_pad_retention(bool retention);
extern int vts_chk_dmic_clk_mode(struct device *dev);
extern bool is_vts(struct device *dev);
extern int vts_start_runtime_resume(struct device *dev, int skip_log);
extern struct vts_data *vts_get_data(struct device *dev);
extern int cmu_vts_rco_400_control(bool on);

#endif /* __SND_SOC_VTS_H */
