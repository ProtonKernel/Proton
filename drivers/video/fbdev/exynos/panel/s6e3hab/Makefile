# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

mcd-panel-s6e3hab-y := s6e3hab.o
mcd-panel-s6e3hab-$(CONFIG_EXTEND_LIVE_CLOCK) += s6e3hab_aod.o
mcd-panel-s6e3hab-$(CONFIG_SUPPORT_DISPLAY_PROFILER) += s6e3hab_profiler.o

obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HAB) += mcd-panel-s6e3hab.o
