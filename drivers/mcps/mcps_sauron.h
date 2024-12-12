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
#ifndef __MCPS_SAURON_H__
#define __MCPS_SAURON_H__

#include <linux/rculist.h>
#if defined(CONFIG_MCPS_GRO_PER_SESSION)
#include <linux/skbuff.h>
#endif // #if defined(CONFIG_MCPS_GRO_PER_SESSION)

#ifdef CONFIG_MCPS_DEBUG
#include <uapi/linux/in6.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#endif // #ifdef CONFIG_MCPS_DEBUG

#include "mcps_buffer.h"

#define sauron_eyes_bits 9
#define sauron_eyes_num (1<<sauron_eyes_bits)

#if defined(CONFIG_MCPS_ICGB)
enum {
	EYE_POLICY_FAST = 0,
	EYE_POLICY_SLOW,
};
#endif // #if defined(CONFIG_MCPS_ICGB)

/* struct eye - information structure of packet flow (session)
 * Notice : eye must be allocated by zero with kzalloc
 */
struct eye {
	u32 hash;
	u32 value;
	u32 capture;
	u32 pps;

	u32 cpu;
	u32 state;
#if defined(CONFIG_MCPS_GRO_PER_SESSION)
	char option;
	u32 gro_nskb;
	u32 gro_tog;

	unsigned long t_interval;
	unsigned long t_created;
#endif // #if defined(CONFIG_MCPS_GRO_PER_SESSION)

#if defined(CONFIG_MCPS_ICGB)
	u32 policy;
#endif // #if defined(CONFIG_MCPS_ICGB)
	unsigned int	dst_ipv4;
	struct in6_addr dst_ipv6;
	unsigned int	dst_port;

	unsigned long	t_stamp;
	unsigned long	t_capture;

	int is_on_monitoring;

	struct rcu_head rcu;
	struct hlist_node		eye_hash_node;

#ifdef CONFIG_MCPS_DEBUG
	struct timespec64 timestamp;
	atomic_t		input_num;
	atomic_t		output_num;
	atomic_t		drop_num;
	atomic_t		ofo_num;
	atomic_t		mig_count;
	atomic_t		l2l_count;
	atomic_t		l2b_count;
	atomic_t		b2l_count;
	atomic_t		b2b_count;
	struct timespec64 mig_1st_time;
	struct timespec64 mig_last_time;
#endif
	struct pending_queue pendings;
};

#define FLOW_STATE_NO_GRO 0x02
#define FLOW_STATE_FIXED_GRO 0x01
#define FLOW_STATE_FLEX_GRO 0x00

struct sauron {
	spinlock_t sauron_eyes_lock;

	struct hlist_head sauron_eyes[sauron_eyes_num];

	spinlock_t cached_eyes_lock[NR_CPUS];

	struct eye *heavy_eyes[NR_CPUS];				// cached_eyes_lock, sauron_eyes_lock(?)
	struct eye *light_eyes[NR_CPUS];				// cached_eyes_lock, sauron_eyes_lock(?)
	unsigned int flow_cnt_by_cpus[NR_CPUS];			  // sauron_eyes_lock
	unsigned int target_flow_cnt_by_cpus[NR_CPUS];	   // sauron_eyes_lock
};

#define DEFAULT_MONITOR_PPS_THRESHOLD 100 // 1mbps = about 100 pps (mtu 1500)
#define mcps_is_satisfy_monitoring_speed(pps) ((pps) >= DEFAULT_MONITOR_PPS_THRESHOLD)
#define mcps_is_on_monitoring(eye) ((eye)->is_on_monitoring)

static inline void sauron_lock(struct sauron *s)
{
	spin_lock(&s->sauron_eyes_lock);
}

static inline void sauron_unlock(struct sauron *s)
{
	spin_unlock(&s->sauron_eyes_lock);
}

struct eye *pick_heavy(struct sauron  *sauron, int cpu);
struct eye *pick_light(struct sauron  *sauron, int cpu);

struct sauron_statistics {
	unsigned int cpu_distribute;
};

DECLARE_PER_CPU(struct sauron_statistics, sauron_stats);

#if defined(CONFIG_MCPS_GRO_PER_SESSION)
struct eye_skb {
	unsigned int count;
	unsigned int limit;
};

#define EYESKB(skb) ((struct eye_skb *)((skb)->cb))
static inline void mcps_eye_gro_stamp(struct eye *e, struct sk_buff *skb)
{
	if (++e->gro_tog >= e->gro_nskb)
		e->gro_tog = 0;
	EYESKB(skb)->limit = e->gro_nskb;
	EYESKB(skb)->count = e->gro_tog;
}

static inline int mcps_eye_gro_skip(struct sk_buff *skb)
{
	return (EYESKB(skb)->limit == 0);
}

static inline int mcps_eye_gro_flush(struct sk_buff *skb)
{
	return (EYESKB(skb)->count == 0);
}
#else // #if defined(CONFIG_MCPS_GRO_PER_SESSION)
#define mcps_eye_gro_stamp(e, skb) do { } while (0)
#define mcps_eye_gro_skip(skb)	 do { } while (0)
#define mcps_eye_gro_flush(skb)	do { } while (0)
#endif // #if defined(CONFIG_MCPS_GRO_PER_SESSION)

/* Declare functions */
void init_mcps_core(void);
void release_mcps_core(void);

int get_arps_cpu(struct sauron *sauron, struct sk_buff *skb);
int get_agro_cpu(struct sauron *sauron, struct sk_buff *skb);
int flush_flows(int force);

struct eye *search_flow(struct sauron *sauron, u32 hash);
int _move_flow(unsigned int hash, unsigned int to);
int migrate_flow_on_cpu(unsigned int from, unsigned int to, unsigned int option);
int migrate_flow(unsigned int from, unsigned int to, unsigned int option);

unsigned int light_cpu(void);
#endif //__MCPS_SAURON_H__
