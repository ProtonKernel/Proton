# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y := -Wformat

obj-$(CONFIG_ION_SYSTEM_HEAP) += ion_sys_heap.o
ion_sys_heap-y := ion_exynos_system_heap.o ion_page_pool.o

obj-$(CONFIG_ION_CMA_HEAP) += ion_cma_heap.o
obj-$(CONFIG_ION_RBIN_HEAP) += ion_rbin_heap.o
