#
# Makefile for the CMUCAL drivers.
#

# Object files in subdirectories

obj-$(CONFIG_CAL_IF)			+= cmupmucal.o
cmupmucal-y				+= cal-if.o cmucal.o ra.o vclk.o pll_spec.o cmucal-debug.o
cmupmucal-$(CONFIG_PMUCAL)		+= pmucal_system.o pmucal_local.o pmucal_cpu.o pmucal_rae.o pmucal_powermode.o
cmupmucal-$(CONFIG_PMUCAL_DBG)		+= pmucal_dbg.o
cmupmucal-$(CONFIG_SOC_EXYNOS9820)	+= exynos9820/cal_data.o
ifndef CONFIG_SOC_EXYNOS2100_EVT0
cmupmucal-$(CONFIG_SOC_EXYNOS2100)	+= exynos2100/cal_data.o
else
cmupmucal-$(CONFIG_SOC_EXYNOS2100_EVT0)	+= exynos2100_evt0/cal_data.o
endif
cmupmucal-$(CONFIG_ACPM_DVFS)		+= acpm_dvfs.o fvmap.o
cmupmucal-$(CONFIG_EXYNOS_DEBUG_FREQ)        += exynos_debug_freq.o

cmupmucal-$(CONFIG_CP_PMUCAL) += pmucal_cp.o
obj-$(CONFIG_CMU_EWF)			+= cmu_ewf.o

