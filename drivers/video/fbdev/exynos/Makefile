obj-$(CONFIG_MCD_PANEL)		+= panel/
obj-$(CONFIG_EXYNOS_DPU30)	+= dpu30/
obj-$(CONFIG_SEC_DISPLAYPORT_EXT_FUNCTION)	+= dp_logger/
subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-Wframe-larger-than=8192 \
	-Wno-unused-variable \
	-Wno-unused-function \
	-Wno-missing-braces \
	-Wno-format \
	-Wno-unused-result

subdir-ccflags-y += -I$(srctree)/drivers/video/fbdev/exynos/panel