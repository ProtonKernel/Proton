# SPDX-License-Identifier: GPL-2.0

ccflags-y := -Wformat

obj-$(CONFIG_SEC_MISC)		+= misc/
obj-$(CONFIG_SEC_PM)		+= pm/

obj-$(CONFIG_SEC_REBOOT)        += sec_reboot.o
obj-$(CONFIG_SEC_KEY_NOTIFIER)	+= sec_crash_key.o
obj-$(CONFIG_SEC_KEY_NOTIFIER)	+= sec_crash_key_user.o
obj-$(CONFIG_SEC_KEY_NOTIFIER)	+= sec_hard_reset_hook.o
obj-$(CONFIG_SEC_KEY_NOTIFIER)	+= sec_key_notifier.o
obj-$(CONFIG_SEC_PARAM)         += sec_mparam.o
obj-$(CONFIG_DRV_SAMSUNG)	+= sec_class.o
obj-$(CONFIG_SEC_BOOTSTAT)	+= sec_bootstat.o
obj-$(CONFIG_ARGOS)		+= sec_argos.o
