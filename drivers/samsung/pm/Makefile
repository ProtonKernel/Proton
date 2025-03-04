# drivers/samsung/pm/Makefile
#
# Copyright (c) 2010-2020 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

# Samsung Mobile Power Management Debugging Support
obj-$(CONFIG_SEC_PM_DEBUG)		+= sec_pm_debug.o
obj-$(CONFIG_SEC_PM_LOG)		+= sec_pm_log.o
obj-$(CONFIG_SEC_PM_CPUFREQ)		+= sec_pm_cpufreq.o
obj-$(CONFIG_SEC_PM_THERMISTOR)		+= sec_thermistor.o
obj-$(CONFIG_SEC_WAKEUP_CPU_ALLOCATOR)		+= sec_wakeup_cpu_allocator.o

ccflags-y := -Wformat
