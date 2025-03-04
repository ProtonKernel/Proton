#
#  Makefile for Exynos-Debug
#

obj-$(CONFIG_DEBUG_SNAPSHOT) 		:= dss.o
dss-y 					+= debug-snapshot.o \
						debug-snapshot-log.o\
						debug-snapshot-utils.o \
						debug-snapshot-dpm.o cache.o
obj-$(CONFIG_DEBUG_SNAPSHOT_BUILT_IN)	+= debug-snapshot-built.o
obj-$(CONFIG_DEBUG_SNAPSHOT_SFRDUMP)	+= debug-snapshot-sfrdump.o
obj-$(CONFIG_DEBUG_SNAPSHOT_QUICKDUMP)	+= debug-snapshot-qd.o

obj-$(CONFIG_EXYNOS_DEBUG_TEST)         += exynos-debug-test.o

obj-$(CONFIG_EXYNOS_ADV_TRACER) 	+= eat.o
eat-y					:= exynos-adv-tracer.o

obj-$(CONFIG_EXYNOS_ADV_TRACER_S2D)	+= exynos-adv-tracer-s2d.o

obj-$(CONFIG_EXYNOS_CORESIGHT)		+= exynos-coresight.o
obj-$(CONFIG_EXYNOS_ECC_HANDLER)	+= exynos-ecc-handler.o

obj-$(CONFIG_EXYNOS_EHLD)		+= ehld.o
ehld-y					:= exynos-ehld.o \
					   exynos-ehld-ipc.o

ifeq ($(CONFIG_SOC_EXYNOS2100),y)
obj-$(CONFIG_EXYNOS_ITMON)		+= exynos2100-itmon.o
endif

obj-$(CONFIG_EXYNOS_HARDLOCKUP_WATCHDOG) += hardlockup-watchdog.o
obj-$(CONFIG_EXYNOS_HARDLOCKUP_DEBUG)	+= hardlockup-debug.o
obj-$(CONFIG_EXYNOS_SSLD)		+= exynos-ssld.o
