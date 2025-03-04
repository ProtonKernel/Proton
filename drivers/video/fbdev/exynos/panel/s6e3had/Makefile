# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

mcd-panel-s6e3had-y := s6e3had.o
mcd-panel-s6e3had-$(CONFIG_EXTEND_LIVE_CLOCK) += s6e3had_aod.o
mcd-panel-s6e3had-$(CONFIG_SUPPORT_DISPLAY_PROFILER) += s6e3had_profiler.o

obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HAD) += mcd-panel-s6e3had.o
