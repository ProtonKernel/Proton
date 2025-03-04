/*
 * UFS Host Controller driver for Exynos specific extensions
 *
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/smc.h>
#include "ufshcd.h"
#include "ufshci.h"
#include "unipro.h"
#include "ufshcd-pltfrm.h"
#include "ufs_quirks.h"
#include "ufs-exynos.h"
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos-cpupm.h>
#include <linux/sec_class.h>
#include <linux/sec_debug.h>

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
#include <asm/unaligned.h>
#define CREATE_TRACE_POINTS
#include <trace/events/marker.h>
#endif

/* Performance */
#include "ufs-exynos-perf.h"

#define IS_C_STATE_ON(h) ((h)->c_state == C_ON)
#define PRINT_STATES(h)							\
	dev_err((h)->dev, "%s: prev h_state %d, cur c_state %d\n",	\
				__func__, (h)->h_state, (h)->c_state);

static struct exynos_ufs *ufs_host_backup[1];
static int ufs_host_index = 0;
static const char *res_token[2] = {
	"passes",
	"fails",
};

enum {
	UFS_S_TOKEN_FAIL,
	UFS_S_TOKEN_NUM,
};

/* UFSHCD states */
enum {
	UFSHCD_STATE_RESET,
	UFSHCD_STATE_ERROR,
	UFSHCD_STATE_OPERATIONAL,
	UFSHCD_STATE_EH_SCHEDULED_FATAL,
	UFSHCD_STATE_EH_SCHEDULED_NON_FATAL,
};

static const char *ufs_s_str_token[UFS_S_TOKEN_NUM] = {
	"fail to",
};

static const char *ufs_pmu_token = "ufs-phy-iso";
static const char *ufs_tcxo_token = "phy-tcxo-con";

static const char *ufs_ext_blks[EXT_BLK_MAX][2] = {
	{"samsung,sysreg-phandle", "ufs-iocc"},	/* sysreg */
};
static const int ufs_ext_ignore[EXT_BLK_MAX] = {0};
static inline void exynos_ufs_ctrl_phy_tcxo(struct exynos_ufs *ufs, bool en);
struct exynos_ufs *g_phy = NULL;

int hsi_tcxo_far_control(int owner, int on)
{
	int ret = 0;
	u32 owner_bit;

	if (g_phy == NULL) {
		pr_info("g_phy is NULL!\n");
		ret = -ENODEV;
		goto err;
	}

	pr_debug("%s, TCXO_FAR current status : %d, owner= %d, on = %d\n",
			__func__, g_phy->tcxo_far_owner, owner, on);

	spin_lock(&g_phy->tcxo_far_lock);

	owner_bit = (1 << owner);

	if (on) {
		if (g_phy->tcxo_far_owner) {
			g_phy->tcxo_far_owner |= owner_bit;
			goto out;
		} else {
			exynos_ufs_ctrl_phy_tcxo(g_phy, 1);
			g_phy->tcxo_far_owner |= owner_bit;
		}
	} else {
		g_phy->tcxo_far_owner &= ~(owner_bit);

		if (g_phy->tcxo_far_owner == 0)
			exynos_ufs_ctrl_phy_tcxo(g_phy, 0);
	}
out:
	spin_unlock(&g_phy->tcxo_far_lock);
err:
	return 0;
}
EXPORT_SYMBOL_GPL(hsi_tcxo_far_control);

/*
 * This type makes 1st DW and another DW be logged.
 * The second one is the head of CDB for COMMAND UPIU and
 * the head of data for DATA UPIU.
 */
static const int __cport_log_type = 0x22;

/* Functions to map registers or to something by other modules */
static void ufs_udelay(u32 n)
{
	udelay(n);
}

static inline void ufs_map_vs_regions(struct exynos_ufs *ufs)
{
	ufs->handle.hci = ufs->reg_hci;
	ufs->handle.ufsp = ufs->reg_ufsp;
	ufs->handle.unipro = ufs->reg_unipro;
	ufs->handle.pma = ufs->reg_phy;
	ufs->handle.cport = ufs->reg_cport;
	ufs->handle.udelay = ufs_udelay;
}

/* Helper for UFS CAL interface */
static int ufs_call_cal(struct exynos_ufs *ufs, int init, void *func)
{
	struct ufs_cal_param *p = &ufs->cal_param;
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret;
	u32 reg;
	cal_if_func_init fn_init;
	cal_if_func fn;

	/* Enable MPHY APB */
	reg = hci_readl(handle, HCI_CLKSTOP_CTRL);
	hci_writel(handle, reg & ~MPHY_APBCLK_STOP, HCI_CLKSTOP_CTRL);
	if (init) {
		fn_init = (cal_if_func_init)func;
		ret = fn_init(p, ufs_host_index);
	} else {
		fn = (cal_if_func)func;
		ret = fn(p);
	}
	if (ret != UFS_CAL_NO_ERROR) {
		dev_err(ufs->dev, "%s: %d\n", __func__, ret);
		ret = -EPERM;
	}
	/* Disable MPHY APB */
	hci_writel(handle, reg | MPHY_APBCLK_STOP, HCI_CLKSTOP_CTRL);
	return ret;

}

static void exynos_ufs_update_active_lanes(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_cal_param *p = &ufs->cal_param;
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 active_tx_lane = 0;
	u32 active_rx_lane = 0;

	active_tx_lane = unipro_readl(handle, UNIP_PA_ACTIVETXDATALENS);
	active_rx_lane = unipro_readl(handle, UNIP_PA_ACTIVERXDATALENS);

	/*
	 * Exynos driver doesn't consider asymmetric lanes, e.g. rx=2, tx=1
	 * so, for the cases, panic occurs to detect when you face new hardware
	 */
	if (!active_rx_lane || !active_tx_lane || active_rx_lane != active_tx_lane) {
		dev_err(hba->dev, "%s: invalid active lanes. rx=%d, tx=%d\n",
			__func__, active_rx_lane, active_tx_lane);
		WARN_ON(1);
	}
	p->active_rx_lane = (u8)active_rx_lane;
	p->active_tx_lane = (u8)active_tx_lane;

	dev_info(ufs->dev, "PA_ActiveTxDataLanes(%d), PA_ActiveRxDataLanes(%d)\n",
		 active_tx_lane, active_rx_lane);
}

static void exynos_ufs_update_max_gear(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *pmd = &ufs->req_pmd_parm;
	struct ufs_cal_param *p = &ufs->cal_param;
	u32 max_rx_hs_gear = 0;

	max_rx_hs_gear = unipro_readl(&ufs->handle, UNIP_PA_MAXRXHSGEAR);

	p->max_gear = min_t(u8, max_rx_hs_gear, pmd->gear);

	dev_info(ufs->dev, "max_gear(%d), PA_MaxRxHSGear(%d)\n", p->max_gear, max_rx_hs_gear);

	/* set for sysfs */
	ufs->params[UFS_S_PARAM_EOM_SZ] =
			EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX *
			ufs_s_eom_repeat[ufs->cal_param.max_gear];
}

static inline void exynos_ufs_ctrl_phy_pwr(struct exynos_ufs *ufs, bool en)
{
	struct ext_cxt *cxt = &ufs->cxt_phy_iso;

	exynos_pmu_update(cxt->offset, cxt->mask, (en ? 1 : 0) ? cxt->val : 0);
}

static inline void exynos_ufs_ctrl_phy_tcxo(struct exynos_ufs *ufs, bool en)
{
	struct ext_cxt *cxt = &ufs->cxt_tcxo_con;

	exynos_pmu_update(cxt->offset, cxt->mask, ((en ? 1 : 0) ? cxt->val : 0) << 3);
}

static inline void __thaw_cport_logger(struct ufs_vs_handle *handle)
{
	hci_writel(handle, __cport_log_type, 0x114);
	hci_writel(handle, 1, 0x110);
}

static inline void __freeze_cport_logger(struct ufs_vs_handle *handle)
{
	hci_writel(handle, 0, 0x110);
}

/*
 * Exynos debugging main function
 */
static void exynos_ufs_dump_debug_info(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	unsigned long flags;

	spin_lock_irqsave(&ufs->dbg_lock, flags);
	if (ufs->under_dump == 0)
		ufs->under_dump = 1;
	else {
		spin_unlock_irqrestore(&ufs->dbg_lock, flags);
		goto out;
	}
	spin_unlock_irqrestore(&ufs->dbg_lock, flags);

	/* freeze cport logger */
	__freeze_cport_logger(handle);

	exynos_ufs_dump_info(handle, ufs->dev);

	/* thaw cport logger */
	__thaw_cport_logger(handle);

	spin_lock_irqsave(&ufs->dbg_lock, flags);
	ufs->under_dump = 0;
	spin_unlock_irqrestore(&ufs->dbg_lock, flags);

out:
#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
	/* do not recover system if test mode is enabled */
	BUG();
#endif
	return;
}

/*
static void exynos_ufs_dbg_command_log(struct ufs_hba *hba,
				struct scsi_cmnd *cmd, const char *str, int tag)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;

	if (!strcmp(str, "send")) {
		exynos_ufs_cmd_log_start(handle, hba, cmd);
	} else if (!strcmp(str, "complete"))
		exynos_ufs_cmd_log_end(handle, hba, tag);
}
*/

inline void exynos_ufs_ctrl_auto_hci_clk(struct exynos_ufs *ufs, bool en)
{
	u32 reg = hci_readl(&ufs->handle, HCI_FORCE_HCS);

	if (en)
		hci_writel(&ufs->handle, reg | HCI_CORECLK_STOP_EN, HCI_FORCE_HCS);
	else
		hci_writel(&ufs->handle, reg & ~HCI_CORECLK_STOP_EN, HCI_FORCE_HCS);
}

static inline void exynos_ufs_ctrl_clk(struct exynos_ufs *ufs, bool en)
{
	u32 reg = hci_readl(&ufs->handle, HCI_FORCE_HCS);

	if (en)
		hci_writel(&ufs->handle, reg | CLK_STOP_CTRL_EN_ALL, HCI_FORCE_HCS);
	else
		hci_writel(&ufs->handle, reg & ~CLK_STOP_CTRL_EN_ALL, HCI_FORCE_HCS);
}

static inline void exynos_ufs_gate_clk(struct exynos_ufs *ufs, bool en)
{

	u32 reg = hci_readl(&ufs->handle, HCI_CLKSTOP_CTRL);

	if (en)
		hci_writel(&ufs->handle, reg | CLK_STOP_ALL, HCI_CLKSTOP_CTRL);
	else
		hci_writel(&ufs->handle, reg & ~CLK_STOP_ALL, HCI_CLKSTOP_CTRL);
}

static void exynos_ufs_set_unipro_mclk(struct exynos_ufs *ufs)
{
	ufs->mclk_rate = (u32)clk_get_rate(ufs->clk_unipro);
	dev_info(ufs->dev, "mclk: %u\n", ufs->mclk_rate);
}

static void exynos_ufs_fit_aggr_timeout(struct exynos_ufs *ufs)
{
	u32 cnt_val;
	unsigned long nVal;

	/* IA_TICK_SEL : 1(1us_TO_CNT_VAL) */
	nVal = hci_readl(&ufs->handle, HCI_UFSHCI_V2P1_CTRL);
	nVal |= IA_TICK_SEL;
	hci_writel(&ufs->handle, nVal, HCI_UFSHCI_V2P1_CTRL);

	cnt_val = ufs->mclk_rate / 1000000 ;
	hci_writel(&ufs->handle, cnt_val & CNT_VAL_1US_MASK, HCI_1US_TO_CNT_VAL);
}

static void exynos_ufs_init_pmc_req(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *pwr_max,
		struct ufs_pa_layer_attr *pwr_req)
{

	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *req_pmd = &ufs->req_pmd_parm;
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;

	/*
	 * Exynos driver doesn't consider asymmetric lanes, e.g. rx=2, tx=1
	 * so, for the cases, panic occurs to detect when you face new hardware.
	 * pwr_max comes from core driver, i.e. ufshcd. That keeps the number
	 * of connected lanes.
	 */
	if (pwr_max->lane_rx != pwr_max->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes. rx=%d, tx=%d\n",
			__func__, pwr_max->lane_rx, pwr_max->lane_tx);
		WARN_ON(1);
	}

	/* gear change parameters to core driver */
	pwr_req->gear_rx
		= act_pmd->gear= min_t(u8, pwr_max->gear_rx, req_pmd->gear);
	pwr_req->gear_tx
		= act_pmd->gear = min_t(u8, pwr_max->gear_tx, req_pmd->gear);
	pwr_req->lane_rx
		= act_pmd->lane = min_t(u8, pwr_max->lane_rx, req_pmd->lane);
	pwr_req->lane_tx
		= act_pmd->lane = min_t(u8, pwr_max->lane_tx, req_pmd->lane);
	pwr_req->pwr_rx = act_pmd->mode = req_pmd->mode;
	pwr_req->pwr_tx = act_pmd->mode = req_pmd->mode;
	pwr_req->hs_rate = act_pmd->hs_series = req_pmd->hs_series;
}

static void exynos_ufs_dev_hw_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* bit[1] for resetn */
	hci_writel(&ufs->handle, 0 << 0, HCI_GPIO_OUT);
	udelay(5);
	hci_writel(&ufs->handle, 1 << 0, HCI_GPIO_OUT);
}

static void exynos_ufs_config_host(struct exynos_ufs *ufs)
{
	u32 reg;

	/* internal clock control */
	exynos_ufs_ctrl_auto_hci_clk(ufs, false);
	exynos_ufs_set_unipro_mclk(ufs);

	/* period for interrupt aggregation */
	exynos_ufs_fit_aggr_timeout(ufs);

	/* misc HCI configurations */
	hci_writel(&ufs->handle, 0xA, HCI_DATA_REORDER);
	hci_writel(&ufs->handle, PRDT_PREFECT_EN | PRDT_SET_SIZE(12),
			HCI_TXPRDT_ENTRY_SIZE);
	hci_writel(&ufs->handle, PRDT_SET_SIZE(12), HCI_RXPRDT_ENTRY_SIZE);
	hci_writel(&ufs->handle, 0xFFFFFFFF, HCI_UTRL_NEXUS_TYPE);
	hci_writel(&ufs->handle, 0xFFFFFFFF, HCI_UTMRL_NEXUS_TYPE);

	reg = hci_readl(&ufs->handle, HCI_AXIDMA_RWDATA_BURST_LEN) &
					~BURST_LEN(0);
	hci_writel(&ufs->handle, WLU_EN | BURST_LEN(3),
					HCI_AXIDMA_RWDATA_BURST_LEN);

	/*
	 * enable HWACG control by IOP
	 *
	 * default value 1->0 at KC.
	 * always "0"(controlled by UFS_ACG_DISABLE)
	 */
	reg = hci_readl(&ufs->handle, HCI_IOP_ACG_DISABLE);
	hci_writel(&ufs->handle, reg & (~HCI_IOP_ACG_DISABLE_EN), HCI_IOP_ACG_DISABLE);

	unipro_writel(&ufs->handle, DBG_SUITE1_ENABLE,
			UNIP_PA_DBG_OPTION_SUITE_1);
	unipro_writel(&ufs->handle, DBG_SUITE2_ENABLE,
			UNIP_PA_DBG_OPTION_SUITE_2);
}

static int exynos_ufs_config_externals(struct exynos_ufs *ufs)
{
	int ret = 0;
	int i;
	struct regmap **p = NULL;
	struct ext_cxt *q = NULL;

	/* PHY isolation bypass */
	exynos_ufs_ctrl_phy_pwr(ufs, true);

	/* Set for UFS iocc */
	for (i = EXT_SYSREG, p = &ufs->regmap_sys, q = &ufs->cxt_iocc;
			i < EXT_BLK_MAX; i++, p++, q++) {
		if (IS_ERR_OR_NULL(*p)) {
			if (!ufs_ext_ignore[i])
				ret = -EINVAL;
			dev_err(ufs->dev, "Unable to control %s\n",
					ufs_ext_blks[i][1]);
			goto out;
		}
		regmap_update_bits(*p, q->offset, q->mask, q->val);
	}

	/* performance */
	ufs_perf_reset(ufs->perf, ufs->hba, true);
out:
	return ret;
}

static int exynos_ufs_get_clks(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct list_head *head = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	int i = 0;

	if (!head || list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		/*
		 * get clock with an order listed in device tree
		 *
		 * hci, unipro
		 */
		if (i == 0)
			ufs->clk_hci = clki->clk;
		else if (i == 1)
			ufs->clk_unipro = clki->clk;

		i++;
	}
out:
	if (!ufs->clk_hci || !ufs->clk_unipro)
		return -EINVAL;

	return 0;
}

static void exynos_ufs_set_features(struct ufs_hba *hba)
{
	/* caps */
	hba->caps = UFSHCD_CAP_CLK_GATING |
			UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;

	/* quirks of common driver */
	hba->quirks = UFSHCD_QUIRK_PRDT_BYTE_GRAN |
			UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR |
			UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR |
			UFSHCD_QUIRK_BROKEN_CRYPTO |
			UFSHCD_QUIRK_BROKEN_OCS_FATAL_ERROR |
			UFSHCI_QUIRK_SKIP_MANUAL_WB_FLUSH_CTRL;
}

static void exynos_ufs_device_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* guarantee device internal cache flush */
	if (hba->eh_flags && (hba->uic_link_state == UIC_LINK_ACTIVE_STATE)
			&& !ufs->skip_flush) {
		dev_info(hba->dev, "%s: Waiting device internal cache flush\n",
			__func__);
		ssleep(2);
		ufs->skip_flush = true;
#if IS_ENABLED(CONFIG_SEC_ABC)
		ufs->reset_cnt++;
		if ((ufs->reset_cnt % 10) == 0)
			sec_abc_send_event("MODULE=storage@ERROR=ufs_hwreset_err");
#endif
	}
}

/* sec special features */
static struct ufs_vendor_dev_info ufs_vdi;

static void ufs_set_sec_unique_number(struct ufs_hba *hba, u8 *desc_buf)
{
	u8 manid;
	u8 serial_num_index;
	u8 snum_buf[UFS_UN_MAX_DIGITS];
	int buff_len;
	u8 *str_desc_buf = NULL;
	int err;

	/* read string desc */
	buff_len = QUERY_DESC_MAX_SIZE;
	str_desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!str_desc_buf)
		goto out;

	serial_num_index = desc_buf[DEVICE_DESC_PARAM_SN];

	/* spec is unicode but sec uses hex data */
	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					QUERY_DESC_IDN_STRING, serial_num_index, 0,
					str_desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading string descriptor. err %d",
			__func__, err);
		goto out;
	}

	/* setup unique_number */
	manid = desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];
	memset(snum_buf, 0, sizeof(snum_buf));
	memcpy(snum_buf, str_desc_buf + QUERY_DESC_HDR_SIZE, SERIAL_NUM_SIZE);
	memset(ufs_vdi.unique_number, 0, sizeof(ufs_vdi.unique_number));

	sprintf(ufs_vdi.unique_number, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		manid,
		desc_buf[DEVICE_DESC_PARAM_MANF_DATE], desc_buf[DEVICE_DESC_PARAM_MANF_DATE+1],
		snum_buf[0], snum_buf[1], snum_buf[2], snum_buf[3], snum_buf[4], snum_buf[5], snum_buf[6]);

	/* Null terminate the unique number string */
	ufs_vdi.unique_number[UFS_UN_20_DIGITS] = '\0';

	dev_dbg(hba->dev, "%s: ufs un : %s\n", __func__, ufs_vdi.unique_number);
out:
	if (str_desc_buf)
		kfree(str_desc_buf);
}
static void ufs_get_health_desc(struct ufs_hba *hba)
{
	int buff_len;
	u8 *desc_buf = NULL;
	int err;

	buff_len = hba->desc_size.hlth_desc;
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return;
	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_HEALTH, 0, 0,
			desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health descriptor. err %d",
				__func__, err);
		goto out;
	}
	/* getting Life Time at Device Health DESC*/
	ufs_vdi.lifetime = desc_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A];
	dev_info(hba->dev, "LT: 0x%02x\n", (desc_buf[3] << 4) | desc_buf[4]);
out:
	kfree(desc_buf);
}

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)

static void ufs_sec_wb_init_sysfs(struct ufs_hba *hba);

#define set_wb_state(wb, s) \
	({(wb)->state = (s); (wb)->state_ts = jiffies; })

#define SEC_WB_may_on(wb, write_blocks, write_rqs) \
	((write_blocks) > ((wb)->wb_off ? (wb)->lp_up_threshold_block : (wb)->up_threshold_block) || \
	 (write_rqs) > ((wb)->wb_off ? (wb)->lp_up_threshold_rqs : (wb)->up_threshold_rqs))
#define SEC_WB_may_off(wb, write_blocks, write_rqs) \
	((write_blocks) < ((wb)->wb_off ? (wb)->lp_down_threshold_block : (wb)->down_threshold_block) && \
	 (write_rqs) < ((wb)->wb_off ? (wb)->lp_down_threshold_rqs : (wb)->down_threshold_rqs))
#define SEC_WB_check_on_delay(wb)	\
	(time_after_eq(jiffies,		\
	 (wb)->state_ts + ((wb)->wb_off ? (wb)->lp_on_delay : (wb)->on_delay)))
#define SEC_WB_check_off_delay(wb)	\
	(time_after_eq(jiffies,		\
	 (wb)->state_ts + ((wb)->wb_off ? (wb)->lp_off_delay : (wb)->off_delay)))

static void ufs_sec_wb_update_summary_stats(struct SEC_WB_info *wb_info)
{
	int idx;

	if (unlikely(!wb_info->wb_curr_issued_block))
		return;

	if (wb_info->wb_curr_issued_max_block < wb_info->wb_curr_issued_block)
		wb_info->wb_curr_issued_max_block = wb_info->wb_curr_issued_block;
	if (wb_info->wb_curr_issued_min_block > wb_info->wb_curr_issued_block)
		wb_info->wb_curr_issued_min_block = wb_info->wb_curr_issued_block;

	/*
	 * count up index
	 * 0 : wb_curr_issued_block < 4GB
	 * 1 : 4GB <= wb_curr_issued_block < 8GB
	 * 2 : 8GB <= wb_curr_issued_block < 16GB
	 * 3 : 16GB <= wb_curr_issued_block
	 */
	idx = fls(wb_info->wb_curr_issued_block >> 20);
	idx = (idx < 4) ? idx : 3;
	wb_info->wb_issued_size_cnt[idx]++;

	/*
	 * wb_curr_issued_block : per 4KB
	 * wb_total_issued_mb : MB
	 */
	wb_info->wb_total_issued_mb += (wb_info->wb_curr_issued_block >> 8);

	wb_info->wb_curr_issued_block = 0;
}

static void ufs_sec_wb_update_state(struct exynos_ufs *ufs)
{
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
	struct ufs_hba *hba = ufs->hba;

	if (hba->pm_op_in_progress) {
		pr_err("%s: pm_op_in_progress.\n", __func__);
		return;
	}

	if (!ufs_sec_is_wb_allowed(ufs))
		return;

	trace_mark_count('C', "UFS-WB", "sectors", wb_info->wb_current_block, 0, 0, 0, 1);
	trace_mark_count('C', "UFS-WB", "rqs", wb_info->wb_current_rqs, 0, 0, 0, 1);

	switch (wb_info->state) {
	case WB_OFF:
		if (SEC_WB_may_on(wb_info, wb_info->wb_current_block, wb_info->wb_current_rqs)) {
			set_wb_state(wb_info, WB_ON_READY);
			trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);
		}
		break;
	case WB_ON_READY:
		if (SEC_WB_may_off(wb_info, wb_info->wb_current_block, wb_info->wb_current_rqs)) {
			set_wb_state(wb_info, WB_OFF);
			trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);
		} else if (SEC_WB_check_on_delay(wb_info)) {
			set_wb_state(wb_info, WB_ON);
			trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);
			// queue work to WB on
			queue_work(ufs->SEC_WB_workq, &ufs->SEC_WB_on_work);
		}
		break;
	case WB_OFF_READY:
		if (SEC_WB_may_on(wb_info, wb_info->wb_current_block, wb_info->wb_current_rqs)) {
			set_wb_state(wb_info, WB_ON);
			trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);
		} else if (SEC_WB_check_off_delay(wb_info)) {
			set_wb_state(wb_info, WB_OFF);
			trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);
			// queue work to WB off
			queue_work(ufs->SEC_WB_workq, &ufs->SEC_WB_off_work);
			ufs_sec_wb_update_summary_stats(wb_info);
		}
		break;
	case WB_ON:
		if (SEC_WB_may_off(wb_info, wb_info->wb_current_block, wb_info->wb_current_rqs)) {
			set_wb_state(wb_info, WB_OFF_READY);
			trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);
		}
		break;
	default:
		WARN_ON(1);
		break;
	}
}

static int ufs_sec_wb_ctrl(struct ufs_hba *hba, bool enable, bool force)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
	int ret = 0;
	u8 index;
	enum query_opcode opcode;
	int retry = 3;
	bool need_runtime_pm = false;

	/*
	 * Do not issue query, return immediately and set prev. state
	 * when workqueue run in suspend/resume
	 */
	if (hba->pm_op_in_progress) {
		pr_err("%s: pm_op_in_progress.\n", __func__);
		return -EBUSY;
	}

	/*
	 * Return error when workqueue run in WB disabled state
	 */
	if (!ufs_sec_is_wb_allowed(ufs) && !force) {
		pr_err("%s: not allowed.\n", __func__);
		return 0;
	}

	if (!(enable ^ hba->wb_enabled)) {
		pr_info("%s: write booster is already %s\n",
				__func__, enable ? "enabled" : "disabled");
		return 0;
	}

	if (enable)
		opcode = UPIU_QUERY_OPCODE_SET_FLAG;
	else
		opcode = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	index = ufshcd_wb_get_query_index(hba);

	if (pm_runtime_status_suspended(hba->dev)) {
		pm_runtime_get_sync(hba->dev);
		need_runtime_pm = true;
	}

	do {
		ret = ufshcd_query_flag(hba, opcode,
				QUERY_FLAG_IDN_WB_EN, index, NULL);
		retry--;
	} while (ret && retry);

	if (need_runtime_pm)
		pm_runtime_put(hba->dev);

	if (!ret) {
		hba->wb_enabled = enable;
		pr_info("%s: SEC write booster %s, current WB state is %d.\n",
				__func__, enable ? "enable" : "disable",
				wb_info->state);
	}

	return ret;
}


static void ufs_sec_wb_on_work_func(struct work_struct *work)
{
	struct exynos_ufs *ufs = container_of(work, struct exynos_ufs,
			SEC_WB_on_work);
	struct ufs_hba *hba = ufs->hba;
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
	int ret = 0;

	ret = ufs_sec_wb_ctrl(hba, true, false);

	/* error case, except pm_op_in_progress and no supports */
	if (ret) {
		unsigned long flags;

		spin_lock_irqsave(hba->host->host_lock, flags);

		dev_err(hba->dev, "%s: write booster on failed %d, now WB is %s, state is %d.\n", __func__,
				ret, hba->wb_enabled ? "on" : "off",
				wb_info->state);

		/*
		 * check only WB_ON state
		 *   WB_OFF_READY : WB may off after this condition
		 *   WB_OFF or WB_ON_READY : in WB_OFF, off_work should be queued
		 */
		if (wb_info->state == WB_ON) {
			/* set WB state to WB_ON_READY to trigger WB ON again */
			set_wb_state(wb_info, WB_ON_READY);
			trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}

	dev_dbg(hba->dev, "%s: WB %s, count %d, ret %d.\n", __func__,
			hba->wb_enabled ? "on" : "off",
			wb_info->wb_current_rqs, ret);
}


static void ufs_sec_wb_off_work_func(struct work_struct *work)
{
	struct exynos_ufs *ufs = container_of(work, struct exynos_ufs,
			SEC_WB_off_work);
	struct ufs_hba *hba = ufs->hba;
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
	int ret = 0;

	ret = ufs_sec_wb_ctrl(hba, false, false);

	/* error case, except pm_op_in_progress and no supports */
	if (ret) {
		unsigned long flags;

		spin_lock_irqsave(hba->host->host_lock, flags);

		dev_err(hba->dev, "%s: write booster off failed %d, now WB is %s, state is %d.\n", __func__,
				ret, hba->wb_enabled ? "on" : "off",
				wb_info->state);

		/*
		 * check only WB_OFF state
		 *   WB_ON_READY : WB may on after this condition
		 *   WB_ON or WB_OFF_READY : in WB_ON, on_work should be queued
		 */
		if (wb_info->state == WB_OFF) {
			/* set WB state to WB_OFF_READY to trigger WB OFF again */
			set_wb_state(wb_info, WB_OFF_READY);
			trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}

	dev_dbg(hba->dev, "%s: WB %s, count %d, ret %d.\n", __func__,
			hba->wb_enabled ? "on" : "off",
			wb_info->wb_current_rqs, ret);
}

static bool ufs_sec_parse_wb_info(struct exynos_ufs *ufs, struct device_node *np)
{

	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
	int temp_delay_ms_value;

	wb_info->wb_support = of_property_read_bool(np, "sec,wb-enable");
	if (!wb_info->wb_support)
		return false;

	if (of_property_read_u32(np, "sec,wb-up-threshold-block", &wb_info->up_threshold_block))
		wb_info->up_threshold_block = 3072;

	if (of_property_read_u32(np, "sec,wb-up-threshold-rqs", &wb_info->up_threshold_rqs))
		wb_info->up_threshold_rqs = 30;

	if (of_property_read_u32(np, "sec,wb-down-threshold-block", &wb_info->down_threshold_block))
		wb_info->down_threshold_block = 1536;

	if (of_property_read_u32(np, "sec,wb-down-threshold-rqs", &wb_info->down_threshold_rqs))
		wb_info->down_threshold_rqs = 25;

	if (of_property_read_u32(np, "sec,wb-disable-threshold-lt", &wb_info->wb_disable_threshold_lt))
		wb_info->wb_disable_threshold_lt = 9;

	if (of_property_read_u32(np, "sec,wb-on-delay-ms", &temp_delay_ms_value))
		wb_info->on_delay = msecs_to_jiffies(92);
	else
		wb_info->on_delay = msecs_to_jiffies(temp_delay_ms_value);

	if (of_property_read_u32(np, "sec,wb-off-delay-ms", &temp_delay_ms_value))
		wb_info->off_delay = msecs_to_jiffies(4500);
	else
		wb_info->off_delay = msecs_to_jiffies(temp_delay_ms_value);

	wb_info->wb_curr_issued_block = 0;
	wb_info->wb_total_issued_mb = 0;
	wb_info->wb_issued_size_cnt[0] = 0;
	wb_info->wb_issued_size_cnt[1] = 0;
	wb_info->wb_issued_size_cnt[2] = 0;
	wb_info->wb_issued_size_cnt[3] = 0;

	/* default values will be used when (wb_off == true) */
	wb_info->lp_up_threshold_block = 3072;		/* 12MB */
	wb_info->lp_up_threshold_rqs = 30;
	wb_info->lp_down_threshold_block = 3072;
	wb_info->lp_down_threshold_rqs = 30;
	wb_info->lp_on_delay = msecs_to_jiffies(200);
	wb_info->lp_off_delay = msecs_to_jiffies(0);

	return true;
}

static void ufs_sec_wb_toggle_flush_during_h8(struct ufs_hba *hba, bool set)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
	int val;
	u8 index;
	int err = 0;
	int retry = 3;

	if (!(wb_info->wb_setup_done && wb_info->wb_support))
		return;

	if (set)
		val =  UPIU_QUERY_OPCODE_SET_FLAG;
	else
		val = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	/* ufshcd_wb_toggle_flush_during_h8 */
	index = ufshcd_wb_get_query_index(hba);

	do {
		err = ufshcd_query_flag(hba, val,
				QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8,
				index, NULL);
		retry--;
	} while (err && retry);

	dev_err(hba->dev, "%s: %s WB flush during H8 is %s.\n", __func__,
			set ? "set" : "clear",
			err ? "failed" : "done");
}

static void ufs_sec_wb_config(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;

	if (!ufs_sec_is_wb_allowed(ufs))
		return;

	set_wb_state(wb_info, WB_OFF);
	trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);

	if (ufs_vdi.lifetime >= (u8)wb_info->wb_disable_threshold_lt)
		goto wb_disabled;

	/* reset wb disable count and enable wb */
	atomic_set(&wb_info->wb_off_cnt, 0);
	wb_info->wb_off = false;

	ufs_sec_wb_toggle_flush_during_h8(hba, true);

	return;
wb_disabled:
	wb_info->wb_support = false;
}

static void ufs_sec_wb_probe(struct ufs_hba *hba, u8 *desc_buf)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
	u8 lun;
	u32 d_lu_wb_buf_alloc = 0;
	int err = 0;

	if (!ufs_sec_is_wb_allowed(ufs))
		return;

	if (wb_info->wb_setup_done)
		return;

	if (hba->desc_size.dev_desc < DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 4)
		goto wb_disabled;

	dev_info->d_ext_ufs_feature_sup =
		get_unaligned_be32(desc_buf +
				   DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP);

	if (!(dev_info->d_ext_ufs_feature_sup & UFS_DEV_WRITE_BOOSTER_SUP))
		goto wb_disabled;

	/*
	 * WB may be supported but not configured while provisioning.
	 * The spec says, in dedicated wb buffer mode,
	 * a max of 1 lun would have wb buffer configured.
	 * Now only shared buffer mode is supported.
	 */
	dev_info->b_wb_buffer_type =
		desc_buf[DEVICE_DESC_PARAM_WB_TYPE];

	dev_info->b_presrv_uspc_en =
		desc_buf[DEVICE_DESC_PARAM_WB_PRESRV_USRSPC_EN];

	if (dev_info->b_wb_buffer_type == WB_BUF_MODE_SHARED) {
		dev_info->d_wb_alloc_units =
		get_unaligned_be32(desc_buf +
				   DEVICE_DESC_PARAM_WB_SHARED_ALLOC_UNITS);
		if (!dev_info->d_wb_alloc_units)
			goto wb_disabled;
	} else {
		int buf_len;
		u8 *desc_buf_temp;

		buf_len = hba->desc_size.unit_desc;

		desc_buf_temp = kmalloc(buf_len, GFP_KERNEL);
		if (!desc_buf_temp)
			goto wb_disabled;

		for (lun = 0; lun < UFS_UPIU_MAX_WB_LUN_ID; lun++) {
			d_lu_wb_buf_alloc = 0;
			err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					QUERY_DESC_IDN_UNIT, lun, 0,
					desc_buf_temp, &buf_len);
			if (err) {
				kfree(desc_buf_temp);
				goto wb_disabled;
			}

			memcpy((u8 *)&d_lu_wb_buf_alloc,
					&desc_buf_temp[UNIT_DESC_PARAM_WB_BUF_ALLOC_UNITS],
					sizeof(d_lu_wb_buf_alloc));
			if (d_lu_wb_buf_alloc) {
				dev_info->wb_dedicated_lu = lun;
				break;
			}
		}
		kfree(desc_buf_temp);

		if (!d_lu_wb_buf_alloc)
			goto wb_disabled;
	}

	dev_info(hba->dev, "%s: SEC WB is enabled. type=%x, size=%u.\n", __func__,
			dev_info->b_wb_buffer_type, d_lu_wb_buf_alloc);

	wb_info->wb_setup_done = true;

	/* create WB sysfs-nodes */
	ufs_sec_wb_init_sysfs(hba);

	return;
wb_disabled:
	wb_info->wb_support = false;
}
#endif	// IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)

static ssize_t ufs_unique_number_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", ufs_vdi.unique_number);
}
static DEVICE_ATTR(un, 0440, ufs_unique_number_show, NULL);

static ssize_t ufs_lt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = ufs_vdi.hba;
	if (!hba) {
		dev_info(dev, "skipping ufs lt read\n");
		ufs_vdi.lifetime = 0;
	} else if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
		pm_runtime_get_sync(hba->dev);
		ufs_get_health_desc(hba);
		pm_runtime_put(hba->dev);
	} else {
		/* return previous LT value if not operational */
		dev_info(hba->dev, "ufshcd_state : %d, old LT: %01x\n",
					hba->ufshcd_state, ufs_vdi.lifetime);
	}

	return snprintf(buf, PAGE_SIZE, "%01x\n", ufs_vdi.lifetime);
}
static DEVICE_ATTR(lt, 0444, ufs_lt_show, NULL);

static ssize_t ufs_lc_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", ufs_vdi.lc_info);
}

static ssize_t ufs_lc_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	ufs_vdi.lc_info = value;

	return count;
}
static DEVICE_ATTR(lc, 0664, ufs_lc_info_show, ufs_lc_info_store);

static ssize_t ufs_man_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = ufs_vdi.hba;
	if (!hba) {
		dev_err(dev, "skipping ufs manid read\n");
		return -EINVAL;
	} else {
		return snprintf(buf, PAGE_SIZE, "%04x\n", hba->dev_info.wmanufacturerid);
	}
}
static DEVICE_ATTR(man_id, 0444, ufs_man_id_show, NULL);

static ssize_t ufs_transferred_cnt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_ufs *ufs = to_exynos_ufs(dev_get_drvdata(dev));

	return sprintf(buf, "%llu\n", ufs->transferred_bytes);
}
static DEVICE_ATTR(transferred_cnt, 0444, ufs_transferred_cnt_show, NULL);

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
static ssize_t ufs_sec_wb_support_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 value;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
	unsigned long flags;
	int ret = 0;

	if (!wb_info->wb_setup_done) {
		dev_err(hba->dev, "SEC WB is not ready yet.\n");
		goto out;
	}

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	value = !!value;

	if (value == wb_info->wb_support) {
		dev_err(hba->dev, "SEC WB is already %s.\n", value ? "supported" : "not supported");
		goto out;
	}

	// change support first
	wb_info->wb_support = value;

	if (!value) {
		cancel_work_sync(&ufs->SEC_WB_on_work);
		cancel_work_sync(&ufs->SEC_WB_off_work);

		pr_err("no support SEC WB : set state %d -> %d.\n", wb_info->state, WB_OFF);

		spin_lock_irqsave(hba->host->host_lock, flags);
		set_wb_state(wb_info, WB_OFF);
		spin_unlock_irqrestore(hba->host->host_lock, flags);

		trace_mark_count('C', "UFS-WB", "state", wb_info->state, 0, 0, 0, 1);

		if (hba->wb_enabled) {	// need to WB off
			ret = ufs_sec_wb_ctrl(hba, false, true);
			ufs_sec_wb_update_summary_stats(wb_info);
			if (ret)
				pr_err("disable SEC WB is failed ret=%d.\n", ret);
		}
	} else
		pr_err("support SEC WB.\n");

out:
	return count;
}

static ssize_t ufs_sec_wb_support_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;

	return sprintf(buf, "%s:%s\n", wb_info->wb_support ? "Support" : "No support",
			hba->wb_enabled ? "on" : "off");
}
static DEVICE_ATTR(sec_wb_support, 0664, ufs_sec_wb_support_show, ufs_sec_wb_support_store);

static ssize_t ufs_sec_wb_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 value;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
	unsigned long flags;

	if (!wb_info->wb_setup_done) {
		dev_err(hba->dev, "SEC WB is not ready yet.\n");
		return -ENODEV;
	}

	if (!ufs_sec_is_wb_allowed(ufs)) {
		pr_err("%s: not allowed.\n", __func__);
		return -EPERM;
	}

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, flags);
	value = !!value;

	if (!value) {
		if (atomic_inc_return(&wb_info->wb_off_cnt) == 1) {
			wb_info->wb_off = true;
			pr_err("disable SEC WB : state %d.\n", wb_info->state);
		}
	} else {
		if (atomic_dec_and_test(&wb_info->wb_off_cnt)) {
			wb_info->wb_off = false;
			pr_err("enable SEC WB.\n");
		}
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return count;
}

static ssize_t ufs_sec_wb_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct SEC_WB_info *wb_info = &ufs->sec_wb_info;

	return sprintf(buf, "%s\n", wb_info->wb_off ? "off" : "Enabled");
}
static DEVICE_ATTR(sec_wb_enable, 0664, ufs_sec_wb_enable_show, ufs_sec_wb_enable_store);

SEC_UFS_WB_DATA_ATTR(wb_up_threshold_block, "%d\n", up_threshold_block);
SEC_UFS_WB_DATA_ATTR(wb_up_threshold_rqs, "%d\n", up_threshold_rqs);
SEC_UFS_WB_DATA_ATTR(wb_down_threshold_block, "%d\n", down_threshold_block);
SEC_UFS_WB_DATA_ATTR(wb_down_threshold_rqs, "%d\n", down_threshold_rqs);
SEC_UFS_WB_DATA_ATTR(lp_wb_up_threshold_block, "%d\n", lp_up_threshold_block);
SEC_UFS_WB_DATA_ATTR(lp_wb_up_threshold_rqs, "%d\n", lp_up_threshold_rqs);
SEC_UFS_WB_DATA_ATTR(lp_wb_down_threshold_block, "%d\n", lp_down_threshold_block);
SEC_UFS_WB_DATA_ATTR(lp_wb_down_threshold_rqs, "%d\n", lp_down_threshold_rqs);

SEC_UFS_WB_TIME_ATTR(wb_on_delay_ms, "%d\n", on_delay);
SEC_UFS_WB_TIME_ATTR(wb_off_delay_ms, "%d\n", off_delay);
SEC_UFS_WB_TIME_ATTR(lp_wb_on_delay_ms, "%d\n", lp_on_delay);
SEC_UFS_WB_TIME_ATTR(lp_wb_off_delay_ms, "%d\n", lp_off_delay);

SEC_UFS_WB_DATA_RO_ATTR(wb_state, "%d,%u\n", wb_info->state, jiffies_to_msecs(jiffies - wb_info->state_ts));
SEC_UFS_WB_DATA_RO_ATTR(wb_current_stat, "current : block %d, rqs %d, issued blocks %d\n",
		wb_info->wb_current_block, wb_info->wb_current_rqs, wb_info->wb_curr_issued_block);
SEC_UFS_WB_DATA_RO_ATTR(wb_current_min_max_stat, "current issued blocks : min %d, max %d.\n",
		wb_info->wb_curr_issued_min_block, wb_info->wb_curr_issued_max_block);
SEC_UFS_WB_DATA_RO_ATTR(wb_total_stat, "total : %dMB\n\t<  4GB:%d\n\t<  8GB:%d\n\t< 16GB:%d\n\t>=16GB:%d\n",
		wb_info->wb_total_issued_mb,
		wb_info->wb_issued_size_cnt[0],
		wb_info->wb_issued_size_cnt[1],
		wb_info->wb_issued_size_cnt[2],
		wb_info->wb_issued_size_cnt[3]);
#endif	// IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)

static struct device *sec_ufs_cmd_dev;

/*
 * Exynos-specific callback functions
 */
static inline int create_ufs_sys_file(struct device *dev, struct exynos_ufs *ufs)
{
	/* sec specific vendor sysfs nodes */
	sec_ufs_cmd_dev = sec_device_create(ufs->hba, "ufs");

	if (IS_ERR(sec_ufs_cmd_dev))
		pr_err("Fail to create sysfs dev\n");
	else {
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_un) < 0)
			pr_err("Fail to create status sysfs file\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lt) < 0)
			pr_err("Fail to create status sysfs file\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lc) < 0)
			pr_err("Fail to create status sysfs file\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_man_id) < 0)
			pr_err("Fail to create status sysfs file\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_transferred_cnt) < 0)
			pr_err("Fail to create status sysfs file\n");
	}

	return 0;
}

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
static void ufs_sec_wb_init_sysfs(struct ufs_hba *hba)
{
	/* The sec_ufs_cmd_dev must be created
	 * in create_ufs_sys_file() before this.
	 * If sec_ufs_cmd_dev is NULL, try to create again.*/
	if (!sec_ufs_cmd_dev)
		sec_ufs_cmd_dev = sec_device_create(hba, "ufs");

	if (IS_ERR(sec_ufs_cmd_dev))
		pr_err("Fail to create sysfs dev\n");
	else {
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_sec_wb_support) < 0)
			pr_err("Fail to create status sysfs file : sec_wb_support\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_sec_wb_enable) < 0)
			pr_err("Fail to create status sysfs file : sec_wb_enable\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_up_threshold_block) < 0)
			pr_err("Fail to create status sysfs file : wb_up_threshold_block\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_up_threshold_rqs) < 0)
			pr_err("Fail to create status sysfs file : wb_up_threshold_rqs\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_down_threshold_block) < 0)
			pr_err("Fail to create status sysfs file : wb_down_threshold_block\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_down_threshold_rqs) < 0)
			pr_err("Fail to create status sysfs file : wb_down_threshold_rqs\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_up_threshold_block) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_up_threshold_block\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_up_threshold_rqs) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_up_threshold_rqs\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_down_threshold_block) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_down_threshold_block\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_down_threshold_rqs) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_down_threshold_rqs\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_on_delay_ms) < 0)
			pr_err("Fail to create status sysfs file : on_delay_ms\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_off_delay_ms) < 0)
			pr_err("Fail to create status sysfs file : off_delay_ms\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_on_delay_ms) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_on_delay_ms\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_off_delay_ms) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_off_delay_ms\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_state) < 0)
			pr_err("Fail to create status sysfs file : wb_state\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_current_stat) < 0)
			pr_err("Fail to create status sysfs file : wb_current_stat\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_current_min_max_stat) < 0)
			pr_err("Fail to create status sysfs file : wb_current_min_max_stat\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_total_stat) < 0)
			pr_err("Fail to create status sysfs file : wb_total_stat\n");
	}
}
#endif

static int exynos_ufs_init(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret;

	/* refer to hba */
	ufs->hba = hba;

	/* configure externals */
	ret = exynos_ufs_config_externals(ufs);
	if (ret)
		return ret;

	/* idle ip nofification for SICD, disable by default */
#if defined(CONFIG_ARM64_EXYNOS_CPUIDLE)
	ufs->idle_ip_index = exynos_get_idle_ip_index(dev_name(hba->dev), 1);
	exynos_update_ip_idle_status(ufs->idle_ip_index, 0);
#endif

	/* to read standard hci registers */
	ufs->handle.std = hba->mmio_base;

	/* get some clock sources and debug infomation structures */
	ret = exynos_ufs_get_clks(hba);
	if (ret)
		return ret;

	/* set features, such as caps or quirks */
	exynos_ufs_set_features(hba);

	create_ufs_sys_file(ufs->dev, ufs);
	exynos_ufs_fmp_config(hba, 1);
	exynos_ufs_srpmb_set_wlun_uac(true);

	return 0;
}

static void exynos_ufs_init_host(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	unsigned long timeout = jiffies + msecs_to_jiffies(1);

	/* host reset */
	hci_writel(&ufs->handle, UFS_SW_RST_MASK, HCI_SW_RST);

	do {
		if (!(hci_readl(&ufs->handle, HCI_SW_RST) & UFS_SW_RST_MASK))
			goto success;
	} while (time_before(jiffies, timeout));

	dev_err(ufs->dev, "timeout host sw-reset\n");

	exynos_ufs_dump_info(&ufs->handle, ufs->dev);

	goto out;

success:
	/* configure host */
	exynos_ufs_config_host(ufs);
out:
	return;
}

static int exynos_ufs_setup_clocks(struct ufs_hba *hba, bool on,
					enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	if (on) {
		if (notify == PRE_CHANGE) {
			/* Clear for SICD */
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
			exynos_update_ip_idle_status(ufs->idle_ip_index, 0);
#endif
			hsi_tcxo_far_control(0, 1);
		} else {
			/* PM Qos hold for stability */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
			exynos_pm_qos_update_request(&ufs->pm_qos_int, ufs->pm_qos_int_value);
#endif
			ufs->c_state = C_ON;
		}
	} else {
		if (notify == PRE_CHANGE) {
			ufs->c_state = C_OFF;

			hsi_tcxo_far_control(0, 0);
			/* reset perf context to start again */
			ufs_perf_reset(ufs->perf, NULL, false);
			/* PM Qos Release for stability */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
			exynos_pm_qos_update_request(&ufs->pm_qos_int, 0);
#endif
		} else {
			/* Set for SICD */
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
			exynos_update_ip_idle_status(ufs->idle_ip_index, 1);
#endif
		}
	}

	return ret;
}

static int exynos_ufs_get_available_lane(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret = -EINVAL;

	/* Get the available lane count */
	ufs->available_lane_tx = unipro_readl(handle, UNIP_PA_AVAILTXDATALENS);
	ufs->available_lane_rx = unipro_readl(handle, UNIP_PA_AVAILRXDATALENS);

	/*
	 * Exynos driver doesn't consider asymmetric lanes, e.g. rx=2, tx=1
	 * so, for the cases, panic occurs to detect when you face new hardware
	 */
	if (!ufs->available_lane_rx || !ufs->available_lane_tx ||
			(ufs->available_lane_rx != ufs->available_lane_tx)) {
		dev_err(hba->dev, "%s: invalid host available lanes. rx=%d, tx=%d\n",
				__func__, ufs->available_lane_rx, ufs->available_lane_tx);
		BUG_ON(1);
		goto out;
	}
	ret = exynos_ufs_dbg_set_lanes(handle, ufs->dev, ufs->available_lane_rx);
	if (ret)
		goto out;

	ufs->num_rx_lanes = ufs->available_lane_rx;
	ufs->num_tx_lanes = ufs->available_lane_tx;
	ret = 0;
out:
	return ret;
}

static void exynos_ufs_override_hba_params(struct ufs_hba *hba)
{
	hba->spm_lvl = UFS_PM_LVL_5;
}

static int exynos_ufs_hce_enable_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	PRINT_STATES(ufs);

	switch (notify) {
	case PRE_CHANGE:
		/*
		 * This function is called in ufshcd_hba_enable,
		 * maybe boot, wake-up or link start-up failure cases.
		 * To start safely, reset of entire logics of host
		 * is required
		 */
		exynos_ufs_init_host(hba);

		/* device reset */
		exynos_ufs_dev_hw_reset(hba);
		break;
	case POST_CHANGE:
		exynos_ufs_ctrl_clk(ufs, true);
		exynos_ufs_gate_clk(ufs, false);

		ret = exynos_ufs_get_available_lane(hba);

		/* freeze cport logger */
		__thaw_cport_logger(&ufs->handle);

		ufs->h_state = H_RESET;
		break;
	default:
		break;
	}

	exynos_ufs_srpmb_set_wlun_uac(true);

	return ret;
}

static int exynos_ufs_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_cal_param *p = &ufs->cal_param;
	int ret = 0;

	switch (notify) {
	case PRE_CHANGE:
		/* override some parameters from core driver */
		exynos_ufs_override_hba_params(hba);

		if (!IS_C_STATE_ON(ufs) ||
				ufs->h_state != H_RESET)
			PRINT_STATES(ufs);

		/* hci */
		hci_writel(&ufs->handle, DFES_ERR_EN | DFES_DEF_DL_ERRS, HCI_ERROR_EN_DL_LAYER);
		hci_writel(&ufs->handle, DFES_ERR_EN | DFES_DEF_N_ERRS, HCI_ERROR_EN_N_LAYER);
		hci_writel(&ufs->handle, DFES_ERR_EN | DFES_DEF_T_ERRS, HCI_ERROR_EN_T_LAYER);

		ufs->mclk_rate = clk_get_rate(ufs->clk_unipro);

		/* cal */
		p->mclk_rate = ufs->mclk_rate;
		p->available_lane = ufs->num_rx_lanes;
		ret = ufs_call_cal(ufs, 0, ufs_cal_pre_link);
		break;
	case POST_CHANGE:
		/* update max gear after link*/
		exynos_ufs_update_max_gear(ufs->hba);

		p->connected_tx_lane = unipro_readl(&ufs->handle, UNIP_PA_CONNECTEDTXDATALENS);
		p->connected_rx_lane = unipro_readl(&ufs->handle, UNIP_PA_CONNECTEDRXDATALENS);
		/* cal */
		ret = ufs_call_cal(ufs, 0, ufs_cal_post_link);

		/* print link start-up result */
		dev_info(ufs->dev, "UFS link start-up %s\n",
					(!ret) ? res_token[0] : res_token[1]);

		ufs->h_state = H_LINK_UP;
		break;
	default:
		break;
	}

	return ret;
}

static int exynos_ufs_pwr_change_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status notify,
					struct ufs_pa_layer_attr *pwr_max,
					struct ufs_pa_layer_attr *pwr_req)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;
	int ret = 0;

	switch (notify) {
	case PRE_CHANGE:
		/*
		 * we're here, that means the sequence up to fDeviceinit
		 * is doen successfully.
		 */
		dev_info(ufs->dev, "UFS device initialized\n");

		if (!IS_C_STATE_ON(ufs) ||
				ufs->h_state != H_REQ_BUSY)
			PRINT_STATES(ufs);

		/* Set PMC parameters to be requested */
		exynos_ufs_init_pmc_req(hba, pwr_max, pwr_req);

		/* cal */
		ufs->cal_param.pmd = act_pmd;
		ret = ufs_call_cal(ufs, 0, ufs_cal_pre_pmc);

		break;
	case POST_CHANGE:
		/* update active lanes after pmc */
		exynos_ufs_update_active_lanes(hba);

		/* cal */
		ret = ufs_call_cal(ufs, 0, ufs_cal_post_pmc);

		dev_info(ufs->dev,
				"Power mode change(%d): M(%d)G(%d)L(%d)HS-series(%d)\n",
				ret, act_pmd->mode, act_pmd->gear,
				act_pmd->lane, act_pmd->hs_series);
		/*
		 * print gear change result.
		 * Exynos driver always considers gear change to
		 * HS-B and fast mode.
		 */
		if (ufs->req_pmd_parm.mode == FAST_MODE &&
				ufs->req_pmd_parm.hs_series == PA_HS_MODE_B)
			dev_info(ufs->dev, "HS mode config %s\n",
					(!ret) ? res_token[0] : res_token[1]);

		ufs->h_state = H_LINK_BOOST;
		ufs->skip_flush = false;
		break;
	default:
		break;
	}

	return ret;
}

static void exynos_ufs_set_nexus_t_xfer_req(struct ufs_hba *hba,
				int tag, bool cmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	enum ufs_perf_op op = UFS_PERF_OP_NONE;
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *scmd;
	u32 type;
	struct ufs_vs_handle *handle = &ufs->handle;

	if (!IS_C_STATE_ON(ufs) ||
			(ufs->h_state != H_LINK_UP &&
			ufs->h_state != H_LINK_BOOST &&
			ufs->h_state != H_REQ_BUSY))
		PRINT_STATES(ufs);

	type =  hci_readl(&ufs->handle, HCI_UTRL_NEXUS_TYPE);

	if (cmd)
		type |= (1 << tag);
	else
		type &= ~(1 << tag);

	hci_writel(&ufs->handle, type, HCI_UTRL_NEXUS_TYPE);

	ufs->h_state = H_REQ_BUSY;
	lrbp = &hba->lrb[tag];

	if (lrbp && lrbp->cmd) {
		scmd = lrbp->cmd;

		/* performance, only for SCSI */
		if (scmd->cmnd[0] == 0x28)
			op = UFS_PERF_OP_R;
		else if (scmd->cmnd[0] == 0x2A)
			op = UFS_PERF_OP_W;
		ufs_perf_update_stat(ufs->perf, scmd->request->__data_len, op);

		/* cmnd[7] & cmnd[8] is the length of the data, 4KB unit*/
		if (op == UFS_PERF_OP_R || op == UFS_PERF_OP_W)
			ufs->transferred_bytes += (u64)((scmd->cmnd[7] << 8) | scmd->cmnd[8]) * 4096;
		/* cmd_logging */
		exynos_ufs_cmd_log_start(handle, hba, scmd);

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
		/* sec write booster */
		if (ufs_sec_is_wb_allowed(ufs)) {
			if (scmd->cmnd[0] == 0x2A) {
				struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
				int curr_block = 0;

				curr_block = (scmd->cmnd[7] << 8) | (scmd->cmnd[8] << 0); // count WRITE block
				wb_info->wb_current_block += curr_block;
				wb_info->wb_current_rqs++;

				ufs_sec_wb_update_state(ufs);
			}
		}
#endif
	}
}

static void exynos_ufs_compl_nexus_t_xfer_req(struct ufs_hba *hba,
				int tag, bool is_scsi)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *scmd;
	u8 sense_key;
	u8 asc = 0;
	u8 ascq = 0;
	u8 opcode = 0;
	u32 lba = 0;
	int transfer_len = 0;

	/* cmd_logging */
	if (is_scsi) {
		exynos_ufs_cmd_log_end(handle, hba, tag);

		lrbp = &hba->lrb[tag];
		if (lrbp && lrbp->cmd)
			scmd = lrbp->cmd;
		else
			return;

		/* sense err check */
		sense_key = lrbp->ucd_rsp_ptr->sr.sense_data[2] & 0x0F;
		if ((sense_key == 0x3) || (sense_key == 0x4)) {
			asc = lrbp->ucd_rsp_ptr->sr.sense_data[12];
			ascq = lrbp->ucd_rsp_ptr->sr.sense_data[13];
			opcode = scmd->cmnd[0];
			lba = (scmd->cmnd[2] << 24) | (scmd->cmnd[3] << 16) |
						(scmd->cmnd[4] << 8) | scmd->cmnd[5];
			transfer_len = (scmd->cmnd[7] << 8) | scmd->cmnd[8];

			pr_err("UFS : sense key 0x%x(asc 0x%x, ascq 0x%x), opcode 0x%x, lba 0x%x, len 0x%x.\n",
						sense_key, asc, ascq, opcode, lba, transfer_len);

#if IS_ENABLED(CONFIG_SEC_ABC)
			if (sense_key == 0x3)
				sec_abc_send_event("MODULE=storage@ERROR=ufs_medium_err");
#endif 

#if IS_ENABLED(CONFIG_SEC_DEBUG)
			if (secdbg_mode_enter_upload())
				(sense_key == 0x3)? panic("ufs medium error\n") : panic("ufs hardware error\n");
#endif
		}

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
		/* sec write booster */
		if (ufs_sec_is_wb_allowed(ufs)) {
			if (scmd->cmnd[0] == 0x2A) {
				struct SEC_WB_info *wb_info = &ufs->sec_wb_info;
				int curr_block = 0;

				curr_block = (scmd->cmnd[7] << 8) | (scmd->cmnd[8] << 0); // count WRITE block
				wb_info->wb_current_block -= curr_block;
				wb_info->wb_current_rqs--;

				if (hba->wb_enabled)
					wb_info->wb_curr_issued_block += (unsigned int)curr_block;

				ufs_sec_wb_update_state(ufs);
			}
		}
#endif
	}
}

static void exynos_ufs_set_nexus_t_task_mgmt(struct ufs_hba *hba, int tag, u8 tm_func)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	u32 type;

	if (!IS_C_STATE_ON(ufs) ||
			(ufs->h_state != H_LINK_BOOST &&
			ufs->h_state != H_TM_BUSY &&
			ufs->h_state != H_REQ_BUSY))
		PRINT_STATES(ufs);

	type =  hci_readl(&ufs->handle, HCI_UTMRL_NEXUS_TYPE);

	switch (tm_func) {
	case UFS_ABORT_TASK:
	case UFS_QUERY_TASK:
		type |= (1 << tag);
		break;
	case UFS_ABORT_TASK_SET:
	case UFS_CLEAR_TASK_SET:
	case UFS_LOGICAL_RESET:
	case UFS_QUERY_TASK_SET:
		type &= ~(1 << tag);
		break;
	}

	hci_writel(&ufs->handle, type, HCI_UTMRL_NEXUS_TYPE);

	ufs->h_state = H_TM_BUSY;
}

static void exynos_ufs_hibern8_notify(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	if (cmd == UIC_CMD_DME_HIBER_ENTER) {
		if (!IS_C_STATE_ON(ufs) ||
				(ufs->h_state != H_LINK_UP &&
				 ufs->h_state != H_REQ_BUSY &&
				 ufs->h_state != H_LINK_BOOST))
			PRINT_STATES(ufs);

		if (notify == PRE_CHANGE)
			;
		else {
			/* cal */
			ufs_call_cal(ufs, 0, ufs_cal_post_h8_enter);
			/* Internal clock off */
			exynos_ufs_gate_clk(ufs, true);

			ufs->h_state_prev = ufs->h_state;
			ufs->h_state = H_HIBERN8;
		}
	} else {
		if (notify == PRE_CHANGE) {
			ufs->h_state = ufs->h_state_prev;

			/* Internal clock on */
			exynos_ufs_gate_clk(ufs, false);
			/* cal */
			ufs_call_cal(ufs, 0, ufs_cal_pre_h8_exit);
		} else {
			int h8_delay_ms_ovly =
				ufs->params[UFS_S_PARAM_H8_D_MS];

			/* override h8 enter delay */
			if (h8_delay_ms_ovly)
				hba->clk_gating.delay_ms =
					(unsigned long)h8_delay_ms_ovly;
		}
	}
}

static int __exynos_ufs_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	exynos_ufs_srpmb_set_wlun_uac(true);

	if (!IS_C_STATE_ON(ufs) ||
			ufs->h_state != H_HIBERN8)
		PRINT_STATES(ufs);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_update_request(&ufs->pm_qos_int, 0);
#endif

	hci_writel(&ufs->handle, 0 << 0, HCI_GPIO_OUT);

	exynos_ufs_ctrl_phy_pwr(ufs, false);

	ufs->h_state = H_SUSPEND;
	return 0;
}

static int __exynos_ufs_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
	/*
	 * Change WB state to WB_OFF to default in link off state
	 * because UFS device is needed to setup link.
	 */
	if (ufshcd_is_link_off(hba)) {
		struct SEC_WB_info *wb_info = &ufs->sec_wb_info;

		set_wb_state(wb_info, WB_OFF);
	}
#endif
	if (!IS_C_STATE_ON(ufs) ||
			ufs->h_state != H_SUSPEND)
		PRINT_STATES(ufs);

	if ((!hba->lrb_in_use) && (hba->clk_gating.active_reqs != 1)) {
		dev_err(hba->dev, "%s: hba->clk_gating.active_reqs = %d\n",
				__func__, hba->clk_gating.active_reqs);
#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
		BUG();
#endif
	}

	/* system init */
	ret = exynos_ufs_config_externals(ufs);
	if (ret)
		return ret;

	exynos_ufs_fmp_config(hba, 0);
	exynos_ufs_srpmb_set_wlun_uac(true);

	return 0;
}

#if 0
static void exynos_ufs_perf_mode(struct ufs_hba *hba, struct scsi_cmnd *cmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	enum ufs_perf_op op = UFS_PERF_OP_NONE;

	/* performance, only for SCSI */
	if (cmd->cmnd[0] == 0x28)
		op = UFS_PERF_OP_R;
	else if (cmd->cmnd[0] == 0x2A)
		op = UFS_PERF_OP_W;
	ufs_perf_update_stat(ufs->perf, cmd->request->__data_len, op);
}
#endif

static void ufs_set_sec_features(struct ufs_hba *hba)
{
	int buff_len;
	u8 *desc_buf = NULL;
	int err;

	ufs_vdi.hba = hba;

	/* read device desc */
	buff_len = hba->desc_size.dev_desc;
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return;

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_DEVICE, 0, 0,
			desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading device descriptor. err %d",
				__func__, err);
		goto out;
	}

	ufs_set_sec_unique_number(hba, desc_buf);
	ufs_get_health_desc(hba);

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
	ufs_sec_wb_probe(hba, desc_buf);
#endif
out:
	kfree(desc_buf);
}

static int __apply_dev_quirks(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	const u32 pa_h8_time_offset = 0x329C;
	u32 peer_hibern8time;

	/*
	 * As for tActivate, device value is bigger than host value,
	 * while, as for tHibern8time, vice versa.
	 * For tActiavate, it's enabled by setting
	 * UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE to one.
	 * In here, it's handled only for tActivate.
	 */
	if (hba->dev_quirks & UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE) {
		peer_hibern8time = unipro_readl(handle, pa_h8_time_offset);
		unipro_writel(handle, peer_hibern8time + 1, pa_h8_time_offset);
	}

	/* check only at the first init */
	if (!(hba->eh_flags || hba->pm_op_in_progress)) {
		/* sec special features */
		ufs_set_sec_features(hba);

#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
		dev_info(hba->dev, "UFS test mode enabled\n");
#endif
	}

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
	ufs_sec_wb_config(hba);
#endif

	return 0;
}

static void __fixup_dev_quirks(struct ufs_hba *hba)
{
	hba->dev_quirks &= ~(UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS);
}

static struct ufs_hba_variant_ops exynos_ufs_ops = {
	.init = exynos_ufs_init,
	.setup_clocks = exynos_ufs_setup_clocks,
	.hce_enable_notify = exynos_ufs_hce_enable_notify,
	.link_startup_notify = exynos_ufs_link_startup_notify,
	.pwr_change_notify = exynos_ufs_pwr_change_notify,
	.setup_xfer_req = exynos_ufs_set_nexus_t_xfer_req,
	.compl_xfer_req = exynos_ufs_compl_nexus_t_xfer_req,
	.setup_task_mgmt = exynos_ufs_set_nexus_t_task_mgmt,
	.hibern8_notify = exynos_ufs_hibern8_notify,
	.device_reset = exynos_ufs_device_reset,
	.dbg_register_dump = exynos_ufs_dump_debug_info,
	//.dbg_command_log = exynos_ufs_dbg_command_log,
	.suspend = __exynos_ufs_suspend,
	.resume = __exynos_ufs_resume,
	//.perf_mode = exynos_ufs_perf_mode,
	.apply_dev_quirks = __apply_dev_quirks,
	.fixup_dev_quirks = __fixup_dev_quirks,
};

/*
 * This function is to define offset, mask and shift to access somewhere.
 */
static int exynos_ufs_set_context_for_access(struct device *dev,
				const char *name, struct ext_cxt *cxt)
{
	struct device_node *np;
	int ret = -EINVAL;

	np = of_get_child_by_name(dev->of_node, name);
	if (!np) {
		dev_info(dev, "get node(%s) doesn't exist\n", name);
		goto out;
	}

	ret = of_property_read_u32(np, "offset", &cxt->offset);
	if (ret == 0) {
		ret = of_property_read_u32(np, "mask", &cxt->mask);
		if (ret == 0)
			ret = of_property_read_u32(np, "val", &cxt->val);
	}
	if (ret != 0) {
		dev_err(dev, "%s set cxt(%s) val\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], name);
		goto out;
	}

	ret = 0;
out:
	return ret;
}

static int exynos_ufs_populate_dt_extern(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *np = dev->of_node;
	struct regmap **reg = NULL;
	struct ext_cxt *cxt;

	bool is_dma_coherent = !!of_find_property(dev->of_node,
						"dma-coherent", NULL);

	int i;
	int ret = -EINPROGRESS;

	/*
	 * pmu for phy isolation. for the pmu, we use api from outside, not regmap
	 */
	cxt = &ufs->cxt_phy_iso;
	ret = exynos_ufs_set_context_for_access(dev, ufs_pmu_token, cxt);
	if (ret) {
		dev_err(dev, "%s: %u: %s get %s\n", __func__, __LINE__,
			ufs_s_str_token[UFS_S_TOKEN_FAIL], ufs_pmu_token);
		goto out;
	}

	/* others */
	for (i = 0, reg = &ufs->regmap_sys, cxt = &ufs->cxt_iocc;
			i < EXT_BLK_MAX; i++, reg++, cxt++) {
		/* look up phandle for external regions */
		*reg = syscon_regmap_lookup_by_phandle(np, ufs_ext_blks[i][0]);
		if (IS_ERR(*reg)) {
			dev_err(dev, "%s: %u: %s find %s\n",
				__func__, __LINE__,
				ufs_s_str_token[UFS_S_TOKEN_FAIL],
				ufs_ext_blks[i][0]);
			if (ufs_ext_ignore[i])
				continue;
			else
				ret = PTR_ERR(*reg);
			goto out;
		}

		/* get and pars echild nodes for external regions in ufs node */
		ret = exynos_ufs_set_context_for_access(dev,
				ufs_ext_blks[i][1], cxt);
		if (ret) {
			dev_err(dev, "%s: %u: %s get %s\n",
				__func__, __LINE__,
				ufs_s_str_token[UFS_S_TOKEN_FAIL],
				ufs_ext_blks[i][1]);
			if (ufs_ext_ignore[i]) {
				ret = 0;
				continue;
			}
			goto out;
		}

		dev_info(dev, "%s: offset 0x%x, mask 0x%x, value 0x%x\n",
				ufs_ext_blks[i][1], cxt->offset, cxt->mask, cxt->val);
	}

	/* TCXO control */
	cxt = &ufs->cxt_tcxo_con;
	ret = exynos_ufs_set_context_for_access(dev, ufs_tcxo_token, cxt);
	if (ret) {
		dev_err(dev, "%s: %u: fail to get %s\n",
					__func__, __LINE__, ufs_tcxo_token);
		goto out;
	}

	/*
	 * w/o 'dma-coherent' means the descriptors would be non-cacheable.
	 * so, iocc should be disabled.
	 */
	if (!is_dma_coherent) {
		ufs->cxt_iocc.val = 0;
		dev_info(dev, "no 'dma-coherent', ufs iocc disabled\n");
	}
out:
	return ret;
}

static int exynos_ufs_get_pwr_mode(struct device_node *np,
				struct exynos_ufs *ufs)
{
	struct uic_pwr_mode *pmd = &ufs->req_pmd_parm;

	pmd->mode = FAST_MODE;

	if (of_property_read_u8(np, "ufs,pmd-attr-lane", &pmd->lane))
		pmd->lane = 1;

	if (of_property_read_u8(np, "ufs,pmd-attr-gear", &pmd->gear))
		pmd->gear = 1;

	pmd->hs_series = PA_HS_MODE_B;

	return 0;
}

static int exynos_ufs_populate_dt(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	int ret;
	int id;

	/* Regmap for external regions */
	ret = exynos_ufs_populate_dt_extern(dev, ufs);
	if (ret) {
		dev_err(dev, "%s populate dt-pmu\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL]);
		goto out;
	}

	/* Get exynos-evt version for featuring */
	if (of_property_read_u8(np, "evt-ver", &ufs->cal_param.evt_ver))
		ufs->cal_param.evt_ver = 0;

	dev_info(dev, "exynos ufs evt version : %d\n",
			ufs->cal_param.evt_ver);


	/* PM QoS */
	child_np = of_get_child_by_name(np, "ufs-pm-qos");
	ufs->pm_qos_int_value = 0;
	if (!child_np)
		dev_info(dev, "No ufs-pm-qos node, not guarantee pm qos\n");
	else {
		of_property_read_u32(child_np, "freq-int", &ufs->pm_qos_int_value);
	}

	/* UIC specifics */
	exynos_ufs_get_pwr_mode(np, ufs);

	ufs->cal_param.board = 0;
	of_property_read_u8(np, "brd-for-cal", &ufs->cal_param.board);

	/* get fmp & smu id */
	ret = of_property_read_u32(np, "fmp-id", &id);
	if (ret)
		ufs->fmp = SMU_ID_MAX;
	else
		ufs->fmp = id;

	ret = of_property_read_u32(np, "smu-id", &id);
	if (ret)
		ufs->smu = SMU_ID_MAX;
	else
		ufs->smu = id;

	ufs_perf_populate_dt(ufs->perf, np);

#if IS_ENABLED(CONFIG_SEC_UFS_WB_FEATURE)
	if (ufs_sec_parse_wb_info(ufs, np)) {
		char wq_name[sizeof("SEC_WB_00")];

		INIT_WORK(&ufs->SEC_WB_on_work, ufs_sec_wb_on_work_func);
		INIT_WORK(&ufs->SEC_WB_off_work, ufs_sec_wb_off_work_func);
		snprintf(wq_name, sizeof(wq_name), "SEC_WB_%d", 0);
		ufs->SEC_WB_workq = create_freezable_workqueue(wq_name);
	}
#endif

out:
	return ret;
}

static int exynos_ufs_ioremap(struct exynos_ufs *ufs, struct platform_device *pdev)
{
	/* Indicators for logs */
	static const char *ufs_region_names[NUM_OF_UFS_MMIO_REGIONS + 1] = {
		"",			/* standard hci */
		"reg_hci",		/* exynos-specific hci */
		"reg_unipro",		/* unipro */
		"reg_ufsp",		/* ufs protector */
		"reg_phy",		/* phy */
		"reg_cport",		/* cport */
	};
	struct device *dev = &pdev->dev;
	struct resource *res;
	void **p = NULL;
	int i = 0;
	int ret = 0;

	for (i = 1, p = &ufs->reg_hci;
			i < NUM_OF_UFS_MMIO_REGIONS + 1; i++, p++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			ret = -ENOMEM;
			break;
		}
		*p = devm_ioremap_resource(dev, res);
		if (!*p) {
			ret = -ENOMEM;
			break;
		}
		dev_info(dev, "%-10s 0x%llx\n", ufs_region_names[i], *p);
	}

	if (ret)
		dev_err(dev, "%s ioremap for %s, 0x%llx\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], ufs_region_names[i]);
	dev_info(dev, "\n");
	return ret;
}

/* sysfs to support utc, eom or whatever */
struct exynos_ufs_sysfs_attr {
	struct attribute attr;
	ssize_t (*show)(struct exynos_ufs *ufs, char *buf, enum exynos_ufs_param_id id);
	int (*store)(struct exynos_ufs *ufs, u32 value, enum exynos_ufs_param_id id);
	int id;
};

static ssize_t exynos_ufs_sysfs_default_show(struct exynos_ufs *ufs, char *buf,
					     enum exynos_ufs_param_id id)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", ufs->params[id]);
}

#define UFS_S_RO(_name, _id)						\
	static struct exynos_ufs_sysfs_attr ufs_s_##_name = {		\
		.attr = { .name = #_name, .mode = 0444 },		\
		.id = _id,						\
		.show = exynos_ufs_sysfs_default_show,			\
	}

#define UFS_S_RW(_name, _id)						\
	static struct exynos_ufs_sysfs_attr ufs_s_##_name = {		\
		.attr = { .name = #_name, .mode = 0666 },		\
		.id = _id,						\
		.show = exynos_ufs_sysfs_default_show,			\
	}

UFS_S_RO(eom_version, UFS_S_PARAM_EOM_VER);
UFS_S_RO(eom_size, UFS_S_PARAM_EOM_SZ);

static int exynos_ufs_sysfs_lane_store(struct exynos_ufs *ufs, u32 value,
				       enum exynos_ufs_param_id id)
{
	if (value >= ufs->num_rx_lanes) {
		dev_err(ufs->dev, "%s set lane to %u. Its max is %u\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], value, ufs->num_rx_lanes);
		return -EINVAL;
	}

	ufs->params[id] = value;

	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_lane = {
	.attr = { .name = "lane", .mode = 0666 },
	.id = UFS_S_PARAM_LANE,
	.show = exynos_ufs_sysfs_default_show,
	.store = exynos_ufs_sysfs_lane_store,
};

static int exynos_ufs_sysfs_mon_store(struct exynos_ufs *ufs, u32 value,
				      enum exynos_ufs_param_id id)
{
	struct ufs_vs_handle *handle = &ufs->handle;

	if (value & UFS_S_MON_LV1) {
		/* Trigger HCI error */
		dev_info(ufs->dev, "Interface error test\n");
		unipro_writel(handle, ((0x1F < 24) | (0x14 << 18)),
			      UNIP_DBG_RX_INFO_CONTROL_DIRECT);
	} else if (value & UFS_S_MON_LV2) {
		/* Block all the interrupts */
		dev_info(ufs->dev, "Device error test\n");
		std_writel(handle, 0, REG_INTERRUPT_ENABLE);
	} else {
		dev_err(ufs->dev, "Undefined level\n");
		return -EINVAL;
	}

	ufs->params[id] = value;
	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_monitor = {
	.attr = { .name = "monitor", .mode = 0666 },
	.id = UFS_S_PARAM_MON,
	.show = exynos_ufs_sysfs_default_show,
	.store = exynos_ufs_sysfs_mon_store,
};

static int exynos_ufs_sysfs_eom_offset_store(struct exynos_ufs *ufs, u32 offset,
					     enum exynos_ufs_param_id id)
{
	u32 num_per_lane = ufs->params[UFS_S_PARAM_EOM_SZ];

	if (offset >= num_per_lane) {
		dev_err(ufs->dev,
			"%s set ofs to %u. The available offset is up to %u\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL],
			offset, num_per_lane - 1);
		return -EINVAL;
	}

	ufs->params[id] = offset;

	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_eom_offset = {
	.attr = { .name = "eom_offset", .mode = 0666 },
	.id = UFS_S_PARAM_EOM_OFS,
	.show = exynos_ufs_sysfs_default_show,
	.store = exynos_ufs_sysfs_eom_offset_store,
};

static ssize_t exynos_ufs_sysfs_show_h8_delay(struct exynos_ufs *ufs,
					      char *buf,
					      enum exynos_ufs_param_id id)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", ufs->hba->clk_gating.delay_ms);
}

static struct exynos_ufs_sysfs_attr ufs_s_h8_delay_ms = {
	.attr = { .name = "h8_delay_ms", .mode = 0666 },
	.id = UFS_S_PARAM_H8_D_MS,
	.show = exynos_ufs_sysfs_show_h8_delay,
};

static int exynos_ufs_sysfs_eom_store(struct exynos_ufs *ufs, u32 value,
				      enum exynos_ufs_param_id id)
{
	struct ufs_cal_param *p = &ufs->cal_param;
	ssize_t ret;

	ret = ufs_cal_eom(p);
	if (ret)
		dev_err(ufs->dev, "%s store eom data\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL]);

	return ret;
}

static struct exynos_ufs_sysfs_attr ufs_s_eom = {
	.attr = { .name = "eom", .mode = 0222 },
	.store = exynos_ufs_sysfs_eom_store,
};

#define UFS_S_EOM_RO(_name)							\
static ssize_t exynos_ufs_sysfs_eom_##_name##_show(struct exynos_ufs *ufs,	\
						   char *buf,			\
						   enum exynos_ufs_param_id id)	\
{										\
	struct ufs_eom_result_s *p =						\
			ufs->cal_param.eom[ufs->params[UFS_S_PARAM_LANE]] +	\
			ufs->params[UFS_S_PARAM_EOM_OFS];			\
	return snprintf(buf, PAGE_SIZE, "%u", p->v_##_name);			\
};										\
static struct exynos_ufs_sysfs_attr ufs_s_eom_##_name = {			\
	.attr = { .name = "eom_"#_name, .mode = 0444 },				\
	.id = -1,								\
	.show = exynos_ufs_sysfs_eom_##_name##_show,				\
}

UFS_S_EOM_RO(phase);
UFS_S_EOM_RO(vref);
UFS_S_EOM_RO(err);

const static struct attribute *ufs_s_sysfs_attrs[] = {
	&ufs_s_eom_version.attr,
	&ufs_s_eom_size.attr,
	&ufs_s_eom_offset.attr,
	&ufs_s_eom.attr,
	&ufs_s_eom_phase.attr,
	&ufs_s_eom_vref.attr,
	&ufs_s_eom_err.attr,
	&ufs_s_lane.attr,
	&ufs_s_h8_delay_ms.attr,
	&ufs_s_monitor.attr,
	NULL,
};

static ssize_t exynos_ufs_sysfs_show(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	struct exynos_ufs *ufs = container_of(kobj,
			struct exynos_ufs, sysfs_kobj);
	struct exynos_ufs_sysfs_attr *param = container_of(attr,
			struct exynos_ufs_sysfs_attr, attr);

	if (!param->show) {
		dev_err(ufs->dev, "%s show thanks to no existence of show\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL]);
		return 0;
	}

	return param->show(ufs, buf, param->id);
}

static ssize_t exynos_ufs_sysfs_store(struct kobject *kobj,
				      struct attribute *attr,
				      const char *buf, size_t length)
{
	struct exynos_ufs *ufs = container_of(kobj,
			struct exynos_ufs, sysfs_kobj);
	struct exynos_ufs_sysfs_attr *param = container_of(attr,
			struct exynos_ufs_sysfs_attr, attr);
	u32 val;
	int ret = 0;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (!param->store) {
		ufs->params[param->id] = val;
		return length;
	}

	ret = param->store(ufs, val, param->id);
	return (ret == 0) ? length : (ssize_t)ret;
}

static const struct sysfs_ops exynos_ufs_sysfs_ops = {
	.show	= exynos_ufs_sysfs_show,
	.store	= exynos_ufs_sysfs_store,
};

static struct kobj_type ufs_s_ktype = {
	.sysfs_ops	= &exynos_ufs_sysfs_ops,
	.release	= NULL,
};

static int exynos_ufs_sysfs_init(struct exynos_ufs *ufs)
{
	int error = -ENOMEM;
	int i;
	struct ufs_eom_result_s *p;

	/* allocate memory for eom per lane */
	for (i = 0; i < MAX_LANE; i++) {
		ufs->cal_param.eom[i] =
			devm_kcalloc(ufs->dev, EOM_MAX_SIZE,
				     sizeof(struct ufs_eom_result_s), GFP_KERNEL);
		p = ufs->cal_param.eom[i];
		if (!p) {
			dev_err(ufs->dev, "%s allocate eom data\n",
				ufs_s_str_token[UFS_S_TOKEN_FAIL]);
			goto fail_mem;
		}
	}

	/* create a path of /sys/kernel/ufs_x */
	kobject_init(&ufs->sysfs_kobj, &ufs_s_ktype);
	error = kobject_add(&ufs->sysfs_kobj, kernel_kobj, "ufs_%c", (char)(ufs->id + '0'));
	if (error) {
		dev_err(ufs->dev, "%s register sysfs directory: %d\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], error);
		goto fail_kobj;
	}

	/* create attributes */
	error = sysfs_create_files(&ufs->sysfs_kobj, ufs_s_sysfs_attrs);
	if (error) {
		dev_err(ufs->dev, "%s create sysfs files: %d\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], error);
		goto fail_kobj;
	}

	/*
	 * Set sysfs params by default. The values could change or
	 * initial configuration could be done elsewhere in the future.
	 *
	 * As for eom_version, you have to move it to store a value
	 * from device tree when eom code is revised, even though I expect
	 * it's not gonna to happen.
	 */
	ufs->params[UFS_S_PARAM_EOM_VER] = 0;
	ufs->params[UFS_S_PARAM_MON] = 0;
	ufs->params[UFS_S_PARAM_H8_D_MS] = 4;

	return 0;

fail_kobj:
	kobject_put(&ufs->sysfs_kobj);
fail_mem:
	for (i = 0; i < MAX_LANE; i++) {
		if (ufs->cal_param.eom[i])
			devm_kfree(ufs->dev, ufs->cal_param.eom[i]);
		ufs->cal_param.eom[i] = NULL;
	}
	return error;
}

static inline void exynos_ufs_sysfs_exit(struct exynos_ufs *ufs)
{
	kobject_put(&ufs->sysfs_kobj);
}

static u64 exynos_ufs_dma_mask = DMA_BIT_MASK(32);

static void __ufs_resume_async(struct work_struct *work)
{
	struct exynos_ufs *ufs =
		container_of(work, struct exynos_ufs, resume_work);
	struct ufs_hba *hba = ufs->hba;
	int ret;

	/* to block incoming commands prior to completion of resuming */
	hba->ufshcd_state = UFSHCD_STATE_RESET;

	/* adding delay to guarantee vcc discharge for abnormal wakeup case */
	if (!ufs->deep_suspended)
		msleep(8);

	ret = ufshcd_system_resume(hba);
	if (ret)
		hba->ufshcd_state = UFSHCD_STATE_ERROR;
}

static int exynos_ufs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_ufs *ufs;
	int ret;

	dev_info(dev, "%s: start\n", __func__);
	dev_info(dev, "===============================\n");

	/* allocate memory */
	ufs = devm_kzalloc(dev, sizeof(*ufs), GFP_KERNEL);
	if (!ufs) {
		ret = -ENOMEM;
		goto out;
	}
	ufs->dev = dev;
	dev->platform_data = ufs;
	dev->dma_mask = &exynos_ufs_dma_mask;

	/* remap regions */
	ret = exynos_ufs_ioremap(ufs, pdev);
	if (ret)
		goto out;
	ufs_map_vs_regions(ufs);

	/* init io perf stat, need an identifier later */
	if (!ufs_perf_init(&ufs->perf, dev))
		dev_err(dev, "Not enable UFS performance mode\n");

	/* populate device tree nodes */
	ret = exynos_ufs_populate_dt(dev, ufs);
	if (ret) {
		dev_err(dev, "%s get dt info.\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL]);
		return ret;
	}

	/* TCXO FAR init */
	g_phy = ufs;
	spin_lock_init(&ufs->tcxo_far_lock);
	ufs->tcxo_far_owner = 0x7;

	/* init cal */
	ufs->cal_param.handle = &ufs->handle;
	ret = ufs_call_cal(ufs, 1, ufs_cal_init);
	if (ret)
		return ret;
	dev_info(dev, "===============================\n");

	/* idle ip nofification for SICD, disable by default */
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	ufs->idle_ip_index = exynos_get_idle_ip_index(dev_name(ufs->dev), 1);
	exynos_update_ip_idle_status(ufs->idle_ip_index, 0);
#endif
	/* register pm qos knobs */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_add_request(&ufs->pm_qos_int, PM_QOS_DEVICE_THROUGHPUT, 0);
#endif

	/* init dbg */
	ret = exynos_ufs_init_dbg(&ufs->handle);
	if (ret)
		return ret;
	spin_lock_init(&ufs->dbg_lock);

	/* store ufs host symbols to analyse later */
	ufs->id = ufs_host_index++;
	ufs_host_backup[ufs->id] = ufs;

	/* init sysfs */
	exynos_ufs_sysfs_init(ufs);

	/* init specific states */
	ufs->h_state = H_DISABLED;
	ufs->c_state = C_OFF;

	/* async resume */
	INIT_WORK(&ufs->resume_work, __ufs_resume_async);

	/* go to core driver through the glue driver */
	ret = ufshcd_pltfrm_init(pdev, &exynos_ufs_ops);
out:
	return ret;
}

static int exynos_ufs_remove(struct platform_device *pdev)
{
	struct exynos_ufs *ufs = dev_get_platdata(&pdev->dev);
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	ufs_host_index--;

	exynos_ufs_sysfs_exit(ufs);

	disable_irq(hba->irq);
	ufshcd_remove(hba);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_remove_request(&ufs->pm_qos_int);
#endif

	/* performance */
	ufs_perf_exit(ufs->perf);

	exynos_ufs_ctrl_phy_pwr(ufs, false);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_ufs_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	ufs->deep_suspended = false;

	/* mainly for early wake-up cases */
	if (work_busy(&ufs->resume_work) && work_pending(&ufs->resume_work))
		flush_work(&ufs->resume_work);

	ret = ufshcd_system_suspend(hba);
	if (!ret)
		hba->ufshcd_state = UFSHCD_STATE_RESET;

	return ret;
}

static int exynos_ufs_suspend_noirq(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	ufs->deep_suspended = true;

	return 0;
}

static int exynos_ufs_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	schedule_work(&ufs->resume_work);

	return 0;
}
#else
#define exynos_ufs_suspend	NULL
#define exynos_ufs_resume	NULL
#define exynos_ufs_suspend_noirq	NULL
#endif /* CONFIG_PM_SLEEP */

static void exynos_ufs_shutdown(struct platform_device *pdev)
{
	struct exynos_ufs *ufs = dev_get_platdata(&pdev->dev);
	struct ufs_hba *hba;

	hba = (struct ufs_hba *)platform_get_drvdata(pdev);
	ufshcd_shutdown(hba);
	hba->ufshcd_state = UFSHCD_STATE_ERROR;
	ufs_perf_exit(ufs->perf);
}

static const struct dev_pm_ops exynos_ufs_dev_pm_ops = {
	.suspend		= exynos_ufs_suspend,
	.resume			= exynos_ufs_resume,
	.suspend_noirq		= exynos_ufs_suspend_noirq,
};

static const struct of_device_id exynos_ufs_match[] = {
	{ .compatible = "samsung,exynos-ufs", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ufs_match);

static struct platform_driver exynos_ufs_driver = {
	.driver = {
		.name = "exynos-ufs",
		.owner = THIS_MODULE,
		.pm = &exynos_ufs_dev_pm_ops,
		.of_match_table = exynos_ufs_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_ufs_probe,
	.remove = exynos_ufs_remove,
	.shutdown = exynos_ufs_shutdown,
};

module_platform_driver(exynos_ufs_driver);
MODULE_DESCRIPTION("Exynos Specific UFSHCI driver");
MODULE_AUTHOR("Seungwon Jeon <tgih.jun@samsung.com>");
MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_LICENSE("GPL");
