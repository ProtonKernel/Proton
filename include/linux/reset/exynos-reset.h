/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_RESET_EXYNOS_RESET_H__
#define __LINUX_RESET_EXYNOS_RESET_H__

#define	EXYNOS_PMU_TOP_OUT_OFFSET		(0x3b20)
#define	EXYNOS_PMU_TOP_OUT_PWRRGTON_NPU		(1<<13)

#define	EXYNOS_PMU_VGPIO_TX_MONITOR_OFFSET	(0x0a10)
#define	EXYNOS_PMU_VGPIO_TX_MONITOR_TXDONE	(1<<22)

#if IS_ENABLED(CONFIG_POWER_RESET_EXYNOS)
extern void exynos_reboot_register_acpm_ops(void *acpm_reboot_func);
extern void exynos_reboot_register_pmic_ops(void *main_wa,
					    void *sub_wa,
					    void *seq_wa,
					    void *pwrkey);
#else
#define exynos_reboot_register_acpm_ops(a)	do { } while (0)
#define exynos_reboot_register_pmic_ops(a,b,c,d)	do { } while (0)
#endif

struct exynos_reboot_helper_ops {
	int (*pmic_off_main_wa)(void);
	int (*pmic_off_sub_wa)(void);
	int (*pmic_off_seq_wa)(void);
	int (*pmic_pwrkey_status)(void);
	void (*acpm_reboot)(void);
};

#endif /* __LINUX_RESET_EXYNOS_RESET_H__ */
