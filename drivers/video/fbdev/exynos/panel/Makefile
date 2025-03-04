#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

mcd-panel-y += timenval.o \
				panel.o \
				panel_bl.o \
				dimming.o \
				panel_drv.o \
				panel_irc.o \
				panel_vrr.o

mcd-panel-$(CONFIG_PANEL_DISPLAY_MODE) += panel_modes.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_SYSFS) += sysfs.o
mcd-panel-$(CONFIG_EXYNOS_DECON_MDNIE_LITE) += mdnie.o
mcd-panel-$(CONFIG_PANEL_DEBUG) += panel_debug.o
mcd-panel-$(CONFIG_EXTEND_LIVE_CLOCK) += aod/aod_drv.o
mcd-panel-$(CONFIG_DYNAMIC_MIPI) += dynamic_mipi/dynamic_mipi.o
mcd-panel-$(CONFIG_SUPPORT_MAFPC) += mafpc/mafpc_drv.o
mcd-panel-$(CONFIG_PANEL_NOTIFY) += panel_notify.o
mcd-panel-$(CONFIG_DISPLAY_USE_INFO) += dpui.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_COPR) += copr.o
mcd-panel-$(CONFIG_SUPPORT_POC_SPI) += panel_poc.o panel_spi.o
mcd-panel-$(CONFIG_SUPPORT_DISPLAY_PROFILER) += \
	display_profiler/display_profiler.o \
	display_profiler/maskgen.o

obj-y += s6e3fab/
obj-y += s6e3hab/
obj-y += s6e3had/
obj-y += ea8082/
obj-y += s6e3fc3/
obj-$(CONFIG_MCD_PANEL) += mcd-panel.o

