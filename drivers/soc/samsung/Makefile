# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_EXYNOS_SEH)	+= exynos-seh.o

obj-$(CONFIG_EXYNOS_PD_EL3)	+= exynos-pd_el3.o
obj-$(CONFIG_EXYNOS_SECURE_LOG) += exynos-seclog.o

obj-$(CONFIG_EXYNOS_TZASC_FAIL_DETECTOR) += exynos-tzasc.o
obj-$(CONFIG_EXYNOS_PPMPU_FAIL_DETECTOR) += exynos-ppmpu.o

obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o
obj-$(CONFIG_EXYNOS_CHIPID_V2)	+= exynos-chipid_v2.o

obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o
obj-$(CONFIG_EXYNOS_PMU_IF)	+= exynos-pmu-if.o
obj-$(CONFIG_EXYNOS_FLEXPMU_DBG)            += exynos-flexpmu-dbg.o

obj-$(CONFIG_EXYNOS_PD)		+= exynos-pd.o
obj-$(CONFIG_EXYNOS_PD_DBG)	+=exynos-pd-dbg.o

# Exynos Memlogger
obj-$(CONFIG_EXYNOS_MEMORY_LOGGER)      += memlogger.o

# Exynos Kernel Protection
obj-$(CONFIG_EXYNOS_KERNEL_INFO)	+= exynos-kernel_info.o

# ECT
obj-$(CONFIG_ECT)               += ect_parser.o
obj-$(CONFIG_EXYNOS_PM) 	+= exynos-pm.o
obj-$(CONFIG_EXYNOS_PMU_ARM_DRIVERS)	+= exynos3250-pmu.o exynos4-pmu.o \
					exynos5250-pmu.o exynos5420-pmu.o
obj-$(CONFIG_EXYNOS_PM_DOMAINS) += pm_domains.o


# CAL
obj-$(CONFIG_CAL_IF)            += cal-if/
#obj-$(CONFIG_CAL_IF)	+= exynos-powermode.o

# USI_V2
obj-$(CONFIG_USI_V2) += usi_v2.o

# DEBUG
obj-$(CONFIG_EXYNOS_DEBUG)      += debug/
# ACPM
obj-$(CONFIG_EXYNOS_ACPM)       += acpm/

obj-$(CONFIG_EXYNOS_RGT)       += exynos-rgt.o

# Exynos Smart Secure Platform
obj-$(CONFIG_EXYNOS_SSP)	+= exynos-ssp.o

# Exynos EL2
obj-$(CONFIG_EXYNOS_EL2)	+= exynos-el2.o

# Exynos S2MPU
obj-$(CONFIG_EXYNOS_S2MPU)	+= exynos-s2mpu.o

# CPIF
obj-$(CONFIG_EXYNOS_MODEM_IF) += cpif/
# HDCP
obj-$(CONFIG_EXYNOS_HDCP2)  += exynos-hdcp/

# CPU Hotplug
obj-$(CONFIG_EXYNOS_CPUHP)	+= exynos-cpuhp.o

# CPU Power Management
obj-$(CONFIG_EXYNOS_CPUPM)	+= exynos-cpupm.o

# Exynos User Frequency & Cstate Control
obj-$(CONFIG_EXYNOS_UFCC)	+= exynos-ufcc.o

# Exynos CPU Performance
obj-$(CONFIG_EXYNOS_PERF)	+= xperf/

# SDM
obj-$(CONFIG_EXYNOS_SDM)	+= exynos-sdm.o

# SCI
obj-$(CONFIG_EXYNOS_SCI)		+= exynos_sci.o
exynos_sci-$(CONFIG_EXYNOS_SCI)		+= exynos-sci.o
exynos_sci-$(CONFIG_EXYNOS_SCI_DBG)	+= exynos-sci_dbg.o

obj-$(CONFIG_EXYNOS_BCM_DBG)    += exynos-bcm_dbg.o exynos-bcm_dbg-dt.o
obj-$(CONFIG_EXYNOS_BCM_DBG_DUMP)       += exynos-bcm_dbg-dump.o

# EXYNOS_PM_QOS
obj-$(CONFIG_EXYNOS_PM_QOS)	+= exynos_pm_qos.o

# DVFS MANAGER
obj-$(CONFIG_EXYNOS_DVFS_MANAGER)       += exynos-dm.o
obj-$(CONFIG_EXYNOS_MCINFO)		+= exynos-mcinfo.o

# Exynos Mobile Scheduler Notifier
obj-$(CONFIG_EXYNOS_MSN)	+= exynos-msn.o

# AFM
obj-$(CONFIG_EXYNOS_AFM)        += exynos-afm.o

# secmem
obj-$(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)       += secmem.o

# Multi IP Governor
obj-$(CONFIG_EXYNOS_MIGOV)	+= exynos-migov.o
obj-$(CONFIG_EXYNOS_CPU_PROFILER)	+= exynos-cpu-profiler.o
obj-$(CONFIG_EXYNOS_GPU_PROFILER)	+= exynos-gpu-profiler.o
obj-$(CONFIG_EXYNOS_MIF_PROFILER)	+= exynos-mif-profiler.o

# Exynos Image Loader
obj-$(CONFIG_EXYNOS_IMGLOADER)       += imgloader.o

# Exynos Subsystem
obj-$(CONFIG_EXYNOS_SYSTEM_EVENT)       += sysevent.o sysevent_notif.o

# Exynos Idle profiler
obj-$(CONFIG_EXYNOS_IDLE_PROFILER)	+= exynos-idle-profiler.o

# Exynos PM DVS
obj-$(CONFIG_EXYNOS_PM_DVS)        += exynos-pm-dvs.o
