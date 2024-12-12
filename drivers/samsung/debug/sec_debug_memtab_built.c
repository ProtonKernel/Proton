// SPDX-License-Identifier: GPL-2.0
/*
 * sec_debug_memtab.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/rtmutex.h>
/* for module symbol */
#include <linux/module.h>
#include <linux/elf.h>
/* sec debug default */
#include <linux/sec_debug.h>
#include "sec_debug_internal.h"

#define DEFINE_MEMBER_TYPE	SECDBG_DEFINE_MEMBER_TYPE

void secdbg_base_built_set_memtab_info(struct sec_debug_memtab *mtab)
{
	mtab->table_start_pa = __pa(__start__secdbg_member_table);
	mtab->table_end_pa = __pa(__stop__secdbg_member_table);
}

DEFINE_MEMBER_TYPE(task_struct_pid, task_struct, pid);
DEFINE_MEMBER_TYPE(task_struct_comm, task_struct, comm);
DEFINE_MEMBER_TYPE(thread_info_flags, thread_info, flags);
DEFINE_MEMBER_TYPE(task_struct_state, task_struct, state);
DEFINE_MEMBER_TYPE(task_struct_exit_state, task_struct, exit_state);
DEFINE_MEMBER_TYPE(task_struct_stack, task_struct, stack);
DEFINE_MEMBER_TYPE(task_struct_flags, task_struct, flags);
DEFINE_MEMBER_TYPE(task_struct_on_cpu, task_struct, on_cpu);
DEFINE_MEMBER_TYPE(task_struct_on_rq, task_struct, on_rq);
DEFINE_MEMBER_TYPE(task_struct_sched_class, task_struct, sched_class);
DEFINE_MEMBER_TYPE(task_struct_cpu, task_struct, cpu);
#ifdef CONFIG_SEC_DEBUG_DTASK
DEFINE_MEMBER_TYPE(task_struct_ssdbg_wait__type, task_struct, android_vendor_data1[0]);
DEFINE_MEMBER_TYPE(task_struct_ssdbg_wait__data, task_struct, android_vendor_data1[1]);
#endif
#ifdef CONFIG_DEBUG_SPINLOCK
DEFINE_MEMBER_TYPE(raw_spinlock_owner_cpu, raw_spinlock, owner_cpu);
DEFINE_MEMBER_TYPE(raw_spinlock_owner, raw_spinlock, owner);
#endif
DEFINE_MEMBER_TYPE(rw_semaphore_count, rw_semaphore, count);
DEFINE_MEMBER_TYPE(rw_semaphore_owner, rw_semaphore, owner);
#ifdef CONFIG_RT_MUTEXES
DEFINE_MEMBER_TYPE(rt_mutex_owner, rt_mutex, owner);
#endif
DEFINE_MEMBER_TYPE(task_struct_normal_prio, task_struct, normal_prio);
DEFINE_MEMBER_TYPE(task_struct_rt_priority, task_struct, rt_priority);
DEFINE_MEMBER_TYPE(task_struct_cpus_ptr, task_struct, cpus_ptr);
#ifdef CONFIG_FAST_TRACK
DEFINE_MEMBER_TYPE(task_struct_se__ftt_mark, task_struct, se.ftt_mark);
#endif
DEFINE_MEMBER_TYPE(task_struct_se__vruntime, task_struct, se.vruntime);
DEFINE_MEMBER_TYPE(task_struct_se__avg__load_avg, task_struct, se.avg.load_avg);
DEFINE_MEMBER_TYPE(task_struct_se__avg__util_avg, task_struct, se.avg.util_avg);

/* for module symbol */
DEFINE_MEMBER_TYPE(module_list, module, list);
DEFINE_MEMBER_TYPE(module_name, module, name);
DEFINE_MEMBER_TYPE(module_core_layout__base, module, core_layout.base);
DEFINE_MEMBER_TYPE(module_core_layout__size, module, core_layout.size);
DEFINE_MEMBER_TYPE(module_init_layout__base, module, init_layout.base);
DEFINE_MEMBER_TYPE(module_init_layout__size, module, init_layout.size);
#ifdef CONFIG_KALLSYMS
DEFINE_MEMBER_TYPE(module_kallsyms, module, kallsyms);
#endif
DEFINE_MEMBER_TYPE(mod_kallsyms_symtab, mod_kallsyms, symtab);
DEFINE_MEMBER_TYPE(mod_kallsyms_num_symtab, mod_kallsyms, num_symtab);
DEFINE_MEMBER_TYPE(mod_kallsyms_strtab, mod_kallsyms, strtab);
DEFINE_MEMBER_TYPE(mod_kallsyms_typetab, mod_kallsyms, typetab);
/* elf64_sym from include/uapi/linux/elf.h */
DEFINE_MEMBER_TYPE(elf64_sym_st_name, elf64_sym, st_name);
DEFINE_MEMBER_TYPE(elf64_sym_st_info, elf64_sym, st_info);
DEFINE_MEMBER_TYPE(elf64_sym_st_shndx, elf64_sym, st_shndx);
DEFINE_MEMBER_TYPE(elf64_sym_st_value, elf64_sym, st_value);
DEFINE_MEMBER_TYPE(elf64_sym_st_size, elf64_sym, st_size);
