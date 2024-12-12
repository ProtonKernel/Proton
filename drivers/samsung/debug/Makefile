# SPDX-License-Identifier: GPL-2.0

ccflags-y := -Wformat

# Samsung Debugging Features
obj-$(CONFIG_SEC_DEBUG_BASE)		+= sec_debug.o
sec_debug-y				+= sec_debug_base.o sec_debug_ksym.o sec_dump_sink.o
obj-$(CONFIG_SEC_DEBUG_BASE)		+= sec_debug_test.o
obj-$(CONFIG_SEC_DEBUG_BASE)		+= sec_debug_dprm.o

obj-$(CONFIG_SEC_DEBUG_BASE_BUILT_IN)		+= sec_debug_base_built.o
obj-$(CONFIG_SEC_DEBUG_MODE)			+= sec_debug_mode.o
obj-$(CONFIG_SEC_DEBUG_AUTO_COMMENT)		+= sec_debug_auto_comment_built.o
obj-$(CONFIG_SEC_DEBUG_MEMTAB)			+= sec_debug_memtab_built.o
obj-$(CONFIG_SEC_DEBUG_EXTRA_INFO)		+= sec_debug_extra_info.o
obj-$(CONFIG_SEC_DEBUG_EXTRA_INFO_BUILT_IN)	+= sec_debug_extra_info_built.o
obj-$(CONFIG_SEC_DEBUG_HW_PARAM)		+= sec_debug_hw_param.o
obj-$(CONFIG_SEC_DEBUG_RESET_REASON)		+= sec_debug_reset_reason.o
obj-$(CONFIG_SEC_DEBUG_WATCHDOGD_FOOTPRINT)	+= sec_debug_wdd_info.o
obj-$(CONFIG_SEC_DEBUG_DTASK)			+= sec_debug_dtask_built.o
obj-$(CONFIG_SEC_DEBUG_HIST_LOG) 		+= sec_debug_hist.o
obj-$(CONFIG_SEC_DEBUG_SUMMARY_LOG)		+= sec_debug_summary.o
obj-$(CONFIG_SEC_DEBUG_SOFTDOG)   		+= sec_debug_softdog.o
obj-$(CONFIG_SEC_DEBUG_LOGBUF_IF)		+= sec_debug_logbuf_interface.o
obj-$(CONFIG_SEC_DEBUG_SCHED_INFO)		+= sec_debug_sched_info.o
obj-$(CONFIG_SEC_DEBUG_STACKTRACE)		+= sec_debug_stacktrace.o
obj-$(CONFIG_SEC_DEBUG_HARDLOCKUP_INFO)		+= sec_debug_hardlockup_info.o
obj-$(CONFIG_SEC_DEBUG_SCHED_REPORT)		+= sec_debug_sched_report.o
