#
# Integrity TEE Driver
#

ccflags-$(CONFIG_TZDEV) += -I$(srctree)/drivers/misc/tzdev/include
ccflags-y += -I$(srctree)/security/samsung/five
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

obj-$(CONFIG_ICD) += icd_driver.o

icd_driver-y += icd.o oemflag.o


EXTRA_CFLAGS += -I$(src)
icd_driver-$(CONFIG_ICD_USE_QSEECOM) += qsee_oemflag.o
icd_driver-$(CONFIG_ICD_USE_TZDEV) += tzdev_oemflag.o
ifneq ($(CONFIG_SOC_EXYNOS7885),)
  ifeq ($(CONFIG_TEEGRIS_VERSION), 3)
    ccflags-$(CONFIG_ICD_USE_TZDEV) += -I$(srctree)/drivers/misc/tzdev/3.0
  else
    ccflags-$(CONFIG_ICD_USE_TZDEV) += -I$(srctree)/drivers/misc/tzdev
  endif
else
ccflags-$(CONFIG_ICD_USE_TZDEV) += -I$(srctree)/drivers/misc/tzdev
endif

ifeq ($(MTK_PLATFORM), mt6768)
  icd_driver-$(CONFIG_ICD_USE_TRUSTONIC) += kinibi_atf_oemflag.o
else
  icd_driver-$(CONFIG_ICD_USE_TRUSTONIC) += kinibi_oemflag.o
endif

ccflags-y += -Wformat

ifneq ($(CONFIG_SEC_KUNIT), y)
  EXTRA_CFLAGS += -D__visible_for_testing=static
endif