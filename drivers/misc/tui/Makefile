#
# Makefile for the stui Module
#

ccflags-y += -Idrivers/video/fbdev/exynos/dpu30
ccflags-y += -Idrivers/video/fbdev/exynos/panel

ccflags-y += -I$(srctree)/drivers/misc/tui/
ccflags-y += -I$(srctree)/drivers/misc/tui/platforms/exynos2100
ccflags-y += -Idrivers/misc/tzdev/include/tzdev

ccflags-y += -DUSE_TEE_CLIENT_API
tuihw-inf-objs := stui_inf.o

tuihw-objs := main.o
tuihw-objs += stui_core.o
tuihw-objs += platforms/exynos2100/stui_hal_display.o
tuihw-objs += platforms/exynos2100/stui_hal_touch.o

ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
tuihw-objs += iwd_agent.o
ccflags-y += -DCONFIG_SAMSUNG_TUI_LOWLEVEL
endif

obj-$(CONFIG_SAMSUNG_TUI) += tuihw.o tuihw-inf.o
