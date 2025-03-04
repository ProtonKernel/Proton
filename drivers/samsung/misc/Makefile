# drivers/samsung/misc/Makefile
#
# Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

# Samsung Debug Feature
obj-$(CONFIG_SEC_LOG)		+= sec_log.o

obj-$(CONFIG_SEC_MEM)		+= sec_memory.o
# Samsung sec NAD Feature
obj-$(CONFIG_SEC_NAD)		+= sec_nad.o
# Samsung TN NAD Balancer Feature
obj-$(CONFIG_SEC_NAD_BALANCER)		+= sec_nad_balancer.o
# Samsung sec BPS Feature
obj-$(CONFIG_SEC_BPS_INFO)		+= sec_bps.o

# Samsung HQM device feature
obj-$(CONFIG_SEC_HQM_DEVICE)		+= sec_hqm_device.o

obj-$(CONFIG_VBUS_NOTIFIER)	+= vbus_notifier.o

# obj-$(CONFIG_PCI_EXYNOS)	+= sec_pcie.o

