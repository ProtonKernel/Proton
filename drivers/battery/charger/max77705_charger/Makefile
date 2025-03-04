obj-$(CONFIG_CHARGER_MAX77705)	+= max77705-charger.o
max77705-charger-$(CONFIG_CHARGER_MAX77705)	+= max77705_charger.o

ifeq ($(CONFIG_SEC_KUNIT), y)
 GCOV_PROFILE_max77705_charger.o := $(CONFIG_SEC_KUNIT)
endif
ccflags-y := -Wformat
