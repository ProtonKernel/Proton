/* linux/arm/arm/mach-exynos/include/mach/regs-clock.h
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5 - Header file for exynos pm support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PM_H
#define __EXYNOS_PM_H

#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/gpio.h>
#include <soc/samsung/cal-if.h>
#define EXYNOS_PM_PREFIX	"EXYNOS-PM:"

#if defined(CONFIG_EXYNOS_FLEXPMU_DBG) || defined(CONFIG_EXYNOS_FLEXPMU_DBG_MODULE)
extern u32 acpm_get_mifdn_count(void);
extern u32 acpm_get_apsocdn_count(void);
extern u32 acpm_get_early_wakeup_count(void);
extern int acpm_get_mif_request(void);
extern u32 acpm_noti_dsu_cpd(bool is_dsu_cpd);
extern u32 acpm_get_dsu_cpd(void);
#else
static inline int acpm_get_mif_request(void) { return 0; }
static inline u32 acpm_get_mifdn_count(void) { return 0; }
static inline u32 acpm_get_apsocdn_count(void) { return 0; }
static inline u32 acpm_get_early_wakeup_count(void ){ return 0; }
static inline u32 acpm_noti_dsu_cpd(bool is_dsu_cpd){ return 0; }
static inline u32 acpm_get_dsu_cpd(void){ return 0; }
#endif

#ifdef CONFIG_PINCTRL_EXYNOS
extern u64 exynos_get_eint_wake_mask(void);
#else
static inline u64 exynos_get_eint_wake_mask(void) { return ULLONG_MAX; }
#endif

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
struct wakeup_stat_name {
	const char *name[32];
};
#endif /* CONFIG_SEC_PM_DEBUG */

struct exynos_pm_info {
	void __iomem *eint_base;		/* GPIO_ALIVE base to check wkup reason */
	void __iomem *gic_base;			/* GICD_ISPENDRn base to check wkup reason */
	unsigned int num_eint;			/* Total number of EINT sources */
	unsigned int num_gic;			/* Total number of GIC sources */

	bool is_early_wakeup;
	unsigned int suspend_mode_idx;		/* power mode to be used in suspend scenario */
	unsigned int apdn_cnt_prev;		/* sleep apsoc down sequence prev count */
	unsigned int apdn_cnt;			/* sleep apsoc down sequence count */

	unsigned int num_wakeup_stat;			/* Total number of wakeup_stat */
	unsigned int	*wakeup_stat_offset;

	unsigned int	num_wakeup_int_en;
	unsigned int	wakeup_stat_eint;
	unsigned int	wakeup_stat_rtc;
	unsigned int	*wakeup_int_en_offset;
	unsigned int	*wakeup_int_en;
	unsigned int	*usbl2_wakeup_int_en;

	unsigned int	num_eint_wakeup_mask;
	unsigned int	*eint_wakeup_mask_offset;

	void __iomem *vgpio2pmu_base;			/* SYSREG_VGPIO2PMU base */
	unsigned int	vgpio_inten_offset;
	unsigned int	vgpio_wakeup_inten;

	struct wakeup_source *ws;
	bool is_stay_awake;
#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	struct wakeup_stat_name *ws_names;	/* Names of each bits of wakeup_stat */
#endif /* CONFIG_SEC_PM_DEBUG */
};

struct exynos_pm_dbg {
	u32 test_early_wakeup;

	unsigned int mifdn_early_wakeup_prev;
	unsigned int mifdn_early_wakeup_cnt;
	unsigned int mifdn_cnt_prev;
	unsigned int mifdn_cnt;
	unsigned int mif_req;
};

#if IS_ENABLED(CONFIG_SEC_DEBUG)
enum ids_info {
	tg,
	lg,
	mg,
	bg,
	g3dg,
	mifg,
	lids,
	mids,
	bids,
	gids,
	gfx,
};

extern int asv_ids_information(enum ids_info id);
#endif
#endif /* __EXYNOS_PM_H */
