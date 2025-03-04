obj-$(CONFIG_DIRECT_CHARGING)		+= sec_direct_charger.o

ccflags-y := -Wformat

obj-$(CONFIG_SEC_PD)				+= sec_pd.o

obj-$(CONFIG_BATTERY_SAMSUNG)	+= sec-battery.o
sec-battery-$(CONFIG_BATTERY_SAMSUNG)		+= sec_battery.o sec_battery_vote.o sec_battery_thermal.o sec_battery_sysfs.o sec_battery_dt.o sec_battery_ttf.o sec_adc.o sb_full_soc.o
sec-battery-$(CONFIG_BATTERY_CISD)	+= sec_cisd.o
sec-battery-$(CONFIG_STEP_CHARGING)		+= sec_step_charging.o
sec-battery-$(CONFIG_WIRELESS_AUTH)		+= sec_battery_misc.o
sec-battery-$(CONFIG_WIRELESS_CHARGING)	+= sec_battery_wc.o

obj-$(CONFIG_UPDATE_BATTERY_DATA)	+= sec_battery_data.o

ifeq ($(CONFIG_SEC_KUNIT), y)
 GCOV_PROFILE_sec_battery.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_battery_wc.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_battery_thermal.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_battery_vote.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_cisd.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_adc.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_battery_sysfs.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_battery_ttf.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_step_charging.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_battery_dt.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_battery_misc.o := $(CONFIG_SEC_KUNIT)
 GCOV_PROFILE_sec_pd.o := $(CONFIG_SEC_KUNIT)
ifeq ($(CONFIG_UML), y)
endif
endif
ccflags-y := -Wformat
