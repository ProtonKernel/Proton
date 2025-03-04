/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019-2021 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MCPS_DEVICE_H__
#define __MCPS_DEVICE_H__
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include <kunit/mock.h>

#include "utils/mcps_cpu.h"
#include "mcps_sauron.h"

enum {
	MCPS_MODE_NONE = 0,
	MCPS_MODE_APP_CLUSTER = 1,
	MCPS_MODE_TP_THRESHOLD = 2,
	MCPS_MODE_SINGLE_MIGRATE = 4,
	MCPS_MODES = 4
};

enum {
	// ~ NR_CPUS - 1
	MCPS_CPU_ON_PENDING = mcps_nr_cpus,
#if defined(CONFIG_MCPS_V2)
	MCPS_CPU_DIRECT_GRO,
#endif // #if defined(CONFIG_MCPS_V2)
	MCPS_CPU_GRO_BYPASS,
	MCPS_CPU_ERROR,
};

enum {
	MCPS_ARPS_LAYER = 0,
#if defined(CONFIG_MCPS_GRO_ENABLE)
	MCPS_AGRO_LAYER,
#endif
	MCPS_TYPE_LAYER,
};

#define VALID_UCPU(c) cpu_possible(c)
#define VALID_CPU(c) cpu_possible(c)

#define NR_CLUSTER 4
#define MID_CLUSTER 3
#define BIG_CLUSTER 2
#define LIT_CLUSTER 1
#define ALL_CLUSTER 0

#if defined(CONFIG_SOC_EXYNOS2100)
#define CLUSTER_MAP {LIT_CLUSTER, LIT_CLUSTER, LIT_CLUSTER, LIT_CLUSTER, MID_CLUSTER, MID_CLUSTER, MID_CLUSTER, BIG_CLUSTER}
#elif defined(CONFIG_SOC_EXYNOS9830) || defined(CONFIG_SOC_EXYNOS9820)
#define CLUSTER_MAP {LIT_CLUSTER, LIT_CLUSTER, LIT_CLUSTER, LIT_CLUSTER, MID_CLUSTER, MID_CLUSTER, BIG_CLUSTER, BIG_CLUSTER}
#else
#define CLUSTER_MAP { [0 ... (mcps_nr_cpus - 1)] = LIT_CLUSTER }
#endif

static const int __mcps_cpu_cluster_map[mcps_nr_cpus] = CLUSTER_MAP;

#define CLUSTER(c) __mcps_cpu_cluster_map[(c)]

/*Declare mcps_enable .. Sync...*/
extern int mcps_enable;

extern cpumask_var_t mcps_cpu_online_mask;

#define mcps_cpu_online(cpu)		cpumask_test_cpu((cpu), mcps_cpu_online_mask)

struct mcps_modes {
	unsigned int mode;
	struct rcu_head rcu;
};

extern struct mcps_modes __rcu *mcps_mode;

struct mcps_pantry {
	struct sk_buff_head process_queue;

	unsigned int		received_arps;
	unsigned int		processed;
	unsigned int		enqueued;
	unsigned int		ignored;

	unsigned int		cpu;

	struct mcps_pantry  *ipi_list;
	struct mcps_pantry  *ipi_next;

	call_single_data_t  csd ____cacheline_aligned_in_smp;

	unsigned int		dropped;
	struct sk_buff_head input_pkt_queue;
	struct napi_struct  rx_napi_struct;

	unsigned int		gro_processed;
#if defined(CONFIG_MCPS_V2)
	struct timespec64	 gro_flush_time;
#endif // #if defined(CONFIG_MCPS_V2)
	struct timespec64	 agro_flush_time;

	struct napi_struct  ipi_napi_struct;
#if defined(CONFIG_MCPS_V2)
	struct napi_struct  gro_napi_struct; // never be touched by other core.
#endif // #if defined(CONFIG_MCPS_V2)
#if defined(CONFIG_MCPS_CHUNK_GRO)
	unsigned int modem_napi_work;
	unsigned int modem_napi_work_batched;
#endif // #if defined(CONFIG_MCPS_CHUNK_GRO)
	int				 offline;
};

DECLARE_PER_CPU_ALIGNED(struct mcps_pantry, mcps_pantries);

DECLARE_PER_CPU_ALIGNED(struct mcps_pantry, mcps_gro_pantries);

static inline void pantry_lock(struct mcps_pantry *pantry)
{
	spin_lock(&pantry->input_pkt_queue.lock);
}

static inline void pantry_unlock(struct mcps_pantry *pantry)
{
	spin_unlock(&pantry->input_pkt_queue.lock);
}

static inline void pantry_ipi_lock(struct mcps_pantry *pantry)
{
	spin_lock(&pantry->process_queue.lock);
}

static inline void pantry_ipi_unlock(struct mcps_pantry *pantry)
{
	spin_unlock(&pantry->process_queue.lock);
}

static inline unsigned int pantry_read_enqueued(struct mcps_pantry *p)
{
	unsigned int ret = 0;

	pantry_lock(p);
	ret = p->enqueued;
	pantry_unlock(p);
	return ret;
}

struct arps_meta {
	struct rcu_head rcu;
	struct rps_map *maps[NR_CLUSTER];
	struct rps_map *maps_filtered[NR_CLUSTER];

	cpumask_var_t mask[NR_CLUSTER];
	cpumask_var_t mask_filtered[NR_CLUSTER];
};

#define ARPS_MAP(m, t) (m->maps[t])
#define ARPS_MAP_FILTERED(m, t) (m->maps_filtered[t])

extern struct arps_meta __rcu *static_arps;
extern struct arps_meta __rcu *dynamic_arps;
extern struct arps_meta __rcu *newflow_arps;
extern struct arps_meta *get_arps_rcu(void);
extern struct arps_meta *get_newflow_rcu(void);
extern struct arps_meta *get_static_arps_rcu(void);

#define NUM_FACTORS 5
#define FACTOR_QLEN 0
#define FACTOR_PROC 1
#define FACTOR_NFLO 2
#define FACTOR_DROP 3
#define FACTOR_DIST 4
struct arps_config {
	struct rcu_head rcu;

	unsigned int weights[NUM_FACTORS];
};

static inline void init_arps_config(struct arps_config *config)
{
	int i;

	for (i = 0; i < NUM_FACTORS; i++)
		config->weights[i] = 5;

	config->weights[FACTOR_NFLO] = 2;
}

struct mcps_config {
	struct sauron			   sauron;

	struct arps_config __rcu	*arps_config;

	struct rcu_head rcu;
} ____cacheline_aligned_in_smp;

struct mcps_attribute {
	struct attribute attr;
	ssize_t (*show)(struct mcps_config *config,
		struct mcps_attribute *attr, char *buf);
	ssize_t (*store)(struct mcps_config *config,
		struct mcps_attribute *attr, const char *buf, size_t len);
};

extern struct mcps_config *mcps;
void mcps_napi_complete(struct napi_struct *n);
int mcps_gro_cpu_startup_callback(unsigned int ocpu);
int mcps_gro_cpu_teardown_callback(unsigned int ocpu);
int del_mcps(struct mcps_config *mcps);
int init_mcps(struct mcps_config *mcps);

static inline int smp_processor_id_safe(void)
{
	int cpu = get_cpu();

	put_cpu();
	return cpu;
}

#if defined(CONFIG_MCPS_ICB)
void mcps_boost_clock(int cluster);
void init_mcps_icb(void);
#else
static inline void mcps_boost_clock(int cluster) { }
static inline void init_mcps_icb(void) { }
#endif // #if defined(CONFIG_MCPS_ICB)

#if defined(CONFIG_MCPS_ICGB) && \
	!defined(CONFIG_KLAT) && \
	defined(CONFIG_MCPS_ON_EXYNOS)
int check_mcps_in_addr(unsigned int addr);
int check_mcps_in6_addr(struct in6_addr *addr);
#else
static inline int check_mcps_in_addr(unsigned int addr)
{
	return 0;
}
static inline int check_mcps_in6_addr(struct in6_addr *addr)
{
	return 0;
}
#endif // #if defined(CONFIG_MCPS_ICGB) && !defined(CONFIG_KLAT) && (defined(CONFIG_SOC_EXYNOS9820) || defined(CONFIG_SOC_EXYNOS9630) || defined(CONFIG_SOC_EXYNOS9830))
#endif //__MCPS_DEVICE_H__
