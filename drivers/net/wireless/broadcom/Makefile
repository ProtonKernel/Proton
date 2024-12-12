# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_B43)		+= b43/
obj-$(CONFIG_B43LEGACY)		+= b43legacy/

obj-$(CONFIG_BRCMFMAC)	+= brcm80211/
obj-$(CONFIG_BRCMSMAC)	+= brcm80211/

$(warning wireless broadcom MAKEFILES)
$(warning DHD // WLAN_VENDOR: $(CONFIG_WLAN_VENDOR_BROADCOM))
ifeq ($(SEC_BUILD_OPTION_ALT_WIFI), true)
export CONFIG_BCM4389=m
export CONFIG_BCM4375=n
endif

ifneq ($(filter y m,$(CONFIG_BCM4389)),)
export ANDROID_VERSION = 11.0.0
obj-m += bcmdhd_101_18/
endif

ifneq ($(filter y m,$(CONFIG_BCM4375)),)
export ANDROID_VERSION = 11.0.0
obj-m += bcmdhd_101_16/
endif