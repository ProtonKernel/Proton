# SPDX-License-Identifier: GPL-2.0-only
#
# Samsung thermal specific Makefile
#
obj-$(CONFIG_EXYNOS_THERMAL_V2)		+= exynos_thermal.o
exynos_thermal-y			+= exynos_tmu_v2.o
exynos_thermal-y			+= exynos_acpm_tmu.o
exynos_thermal-$(CONFIG_EXYNOS_CPU_THERMAL)	+= cpu_cooling.o
exynos_thermal-$(CONFIG_ISP_THERMAL)	+= isp_cooling.o
exynos_thermal-$(CONFIG_DEV_THERMAL)	+= dev_cooling.o

obj-$(CONFIG_GPU_THERMAL)		+= gpu_cooling.o
