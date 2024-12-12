obj-$(CONFIG_SEC_DEBUG_TSP_LOG) += sec_tsp_log.o
obj-$(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)	+= sec_tclm_v2.o
obj-$(CONFIG_INPUT_SEC_SECURE_TOUCH)	+= sec_secure_touch.o
obj-$(CONFIG_TOUCHSCREEN_DUMP_MODE)	+= sec_tsp_dumpkey.o
obj-$(CONFIG_INPUT_SEC_NOTIFIER) += sec_input_notifier.o
obj-$(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)	+= sec_virtual_tsp.o

ifeq ($(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE), m)
obj-$(CONFIG_INPUT_SEC_INPUT) += sec_common_fn.o sec_cmd.o
else
ifeq ($(CONFIG_SEC_KUNIT), y)
obj-$(CONFIG_INPUT_SEC_INPUT) += sec_common.o
sec_common-$(CONFIG_INPUT_SEC_INPUT) += sec_common_fn.o sec_cmd.o
else
obj-$(CONFIG_INPUT_SEC_INPUT) += sec_common_fn.o sec_cmd.o
endif
endif

ccflags-y += -Wformat

GCOV_PROFILE_sec_cmd.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_tsp_log.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_tclm_v2.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_secure_touch.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_tsp_dumpkey.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_input_notifier.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_virtual_tsp.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_common_fn.o := $(CONFIG_SEC_KUNIT)

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-Wno-format-zero-length
