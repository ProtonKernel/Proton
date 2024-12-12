// SPDX-License-Identifier: GPL-2.0
/*
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

//#ifndef CONFIG_MODEM_IF_NET_GRO
//#define CONFIG_MODEM_IF_NET_GRO

#include <linux/kernel.h>

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/cpu.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/indirect_call_wrapper.h>

#if defined(CONFIG_MCPS_V2)
#include <linux/etherdevice.h>
#include <net/busy_poll.h>
#include <net/xfrm.h>
#include <net/dst_metadata.h>
#endif // #if defined(CONFIG_MCPS_V2)

#include "migration/mcps_migration.h"
#include "utils/mcps_logger.h"
#include "mcps_sauron.h"
#include "mcps_device.h"
#include "mcps_buffer.h"
#include "mcps_debug.h"
#include "mcps_fl.h"

#define PRINT_GRO_WORKED(C, W, Q) MCPS_VERBOSE("WORKED[%d] : WORK[%d] / QUOTA[%d] GRO\n", C, W, Q)
#define PRINT_GRO_IPIQ(FROM, TO) MCPS_VERBOSE("IPIQ : TO[%d] FROM[%d] GRO\n", TO, FROM)

DEFINE_PER_CPU_ALIGNED(struct mcps_pantry, mcps_gro_pantries);
EXPORT_PER_CPU_SYMBOL(mcps_gro_pantries);

int mcps_gro_pantry_max_capability __read_mostly = 30000;
module_param(mcps_gro_pantry_max_capability, int, 0640);
#define mcps_gro_pantry_quota 128

#if defined(CONFIG_MCPS_V2)
#if defined(CONFIG_MCPS_CHUNK_GRO)
// MUST EQUAL TO MODEM POLLING NAPI WEIGHT
#define MODEM_NAPI_QUOTA 64
#endif

void mcps_migration_handler(unsigned int from, unsigned int to, unsigned int option)
{
	if (option == MCPS_MIGRATION_FLOWID)
		_move_flow(from, to);
	else
		migrate_flow_on_cpu(from, to, option);
}

mcps_visible_for_testing
void mcps_gro_normal_list(struct napi_struct *napi)
{
	struct sk_buff *skb, *next;

	list_for_each_entry_safe(skb, next, &napi->rx_list, list) {
		skb_list_del_init(skb);
		if (unlikely(mcps_try_skb_internal(skb)))
			netif_receive_skb(skb);
	}
	INIT_LIST_HEAD(&napi->rx_list);
	napi->rx_count = 0;
}

int mcps_gro_max_rx_list __read_mostly = 8;
module_param(mcps_gro_max_rx_list, int, 0640);

mcps_visible_for_testing
int mcps_gro_normal_one(struct napi_struct *napi, struct sk_buff *skb, int segs)
{
	list_add_tail(&skb->list, &napi->rx_list);
	napi->rx_count += segs;
	if (napi->rx_count >= mcps_gro_max_rx_list)
		mcps_gro_normal_list(napi);

	return NET_RX_SUCCESS;
}

INDIRECT_CALLABLE_DECLARE(int inet_gro_complete(struct sk_buff *, int));
INDIRECT_CALLABLE_DECLARE(int ipv6_gro_complete(struct sk_buff *, int));
static int mcps_napi_gro_complete(struct napi_struct *napi, struct sk_buff *skb)
{
	struct packet_offload *ptype;
	__be16 type = skb->protocol;
	int err = -ENOENT;

	BUILD_BUG_ON(sizeof(struct napi_gro_cb) > sizeof(skb->cb));

	if (NAPI_GRO_CB(skb)->count == 1) {
		skb_shinfo(skb)->gso_size = 0;
		goto out;
	}

	rcu_read_lock();
	ptype = gro_find_complete_by_type(type);
	if (ptype) {
		err = INDIRECT_CALL_INET(ptype->callbacks.gro_complete,
								 ipv6_gro_complete, inet_gro_complete,
								 skb, 0);
	}
	rcu_read_unlock();

	if (err) {
		kfree_skb(skb);
		return NET_RX_SUCCESS;
	}

out:
	return mcps_gro_normal_one(napi, skb, NAPI_GRO_CB(skb)->count);
}

#if defined(CONFIG_MCPS_GRO_PER_SESSION)
static void __mcps_napi_gro_single_flush_chain(struct napi_struct *napi, u32 index, unsigned int hash)
{
	struct list_head *head = &napi->gro_hash[index].list;
	struct sk_buff *skb, *p;

	list_for_each_entry_safe_reverse(skb, p, head, list) {
		if (skb->hash == hash) {
			skb_list_del_init(skb);
			mcps_napi_gro_complete(napi, skb);
			napi->gro_hash[index].count--;
		}
	}

	if (!napi->gro_hash[index].count)
		__clear_bit(index, &napi->gro_bitmask);
}

void mcps_napi_gro_single_flush(struct napi_struct *napi, u32 hash)
{
	u32 index = (hash & (GRO_HASH_BUCKETS - 1));

	if (test_bit(index, &napi->gro_bitmask))
		__mcps_napi_gro_single_flush_chain(napi, index, hash);
}

static int __mcps_napi_gro_light_flush_chain(struct napi_struct *napi, u32 index, u32 count)
{
	struct list_head *head = &napi->gro_hash[index].list;
	struct sk_buff *skb, *p;
	int nflush = 0;

	list_for_each_entry_safe_reverse(skb, p, head, list) {
		if (NAPI_GRO_CB(skb)->count < count) {
			skb_list_del_init(skb);
			mcps_napi_gro_complete(napi, skb);
			napi->gro_hash[index].count--;
			nflush++;
		}
	}

	if (!napi->gro_hash[index].count)
		__clear_bit(index, &napi->gro_bitmask);

	return nflush;
}

int mcps_napi_gro_light_flush(struct napi_struct *napi, u32 count)
{
	unsigned long bitmask = napi->gro_bitmask;
	unsigned int i, base = ~0U;
	int nflush = 0;

	while ((i = ffs(bitmask)) != 0) {
		bitmask >>= i;
		base += i;
		nflush += __mcps_napi_gro_light_flush_chain(napi, base, count);
	}

	return nflush;
}
#endif // #if defined(CONFIG_MCPS_GRO_PER_SESSION)

static void __mcps_napi_gro_flush_chain(struct napi_struct *napi, u32 index,
				   bool flush_old)
{
	struct list_head *head = &napi->gro_hash[index].list;
	struct sk_buff *skb, *p;

	list_for_each_entry_safe_reverse(skb, p, head, list) {
		if (flush_old && NAPI_GRO_CB(skb)->age == jiffies)
			return;
		skb_list_del_init(skb);
		mcps_napi_gro_complete(napi, skb);
		napi->gro_hash[index].count--;
	}

	if (!napi->gro_hash[index].count)
		__clear_bit(index, &napi->gro_bitmask);
}

void mcps_napi_gro_flush(struct napi_struct *napi, bool flush_old)
{
	unsigned long bitmask = napi->gro_bitmask;
	unsigned int i, base = ~0U;

	while ((i = ffs(bitmask)) != 0) {
		bitmask >>= i;
		base += i;
		__mcps_napi_gro_flush_chain(napi, base, flush_old);
	}
}

static void mcps_gro_flush_oldest(struct napi_struct *napi, struct list_head *head)
{
	struct sk_buff *oldest;

	oldest = list_last_entry(head, struct sk_buff, list);

	/* We are called with head length >= MAX_GRO_SKBS, so this is
	 * impossible.
	 */
	if (WARN_ON_ONCE(!oldest))
		return;

	/* Do not adjust napi->gro_hash[].count, caller is adding a new
	 * SKB to the chain.
	 */
	skb_list_del_init(oldest);
	mcps_napi_gro_complete(napi, oldest);
}

static struct list_head *mcps_gro_list_prepare(struct napi_struct *napi, struct sk_buff *skb)
{
	struct sk_buff *p;
	unsigned int maclen = skb->dev->hard_header_len;
	u32 hash = skb_get_hash_raw(skb);

	struct list_head *head;

	head = &napi->gro_hash[hash & (GRO_HASH_BUCKETS - 1)].list;
	list_for_each_entry(p, head, list) {
		unsigned long diffs;

		NAPI_GRO_CB(p)->flush = 0;

		if (hash != skb_get_hash_raw(p)) {
			NAPI_GRO_CB(p)->same_flow = 0;
			continue;
		}

		diffs = (unsigned long)p->dev ^ (unsigned long)skb->dev;
		diffs |= skb_vlan_tag_present(p) ^ skb_vlan_tag_present(skb);
		if (skb_vlan_tag_present(p))
			diffs |= p->vlan_tci ^ skb->vlan_tci;
		diffs |= skb_metadata_dst_cmp(p, skb);
		diffs |= skb_metadata_differs(p, skb);
		if (maclen == ETH_HLEN)
			diffs |= compare_ether_header(skb_mac_header(p),
										  skb_mac_header(skb));
		else if (!diffs)
			diffs = memcmp(skb_mac_header(p),
						   skb_mac_header(skb),
						   maclen);
		NAPI_GRO_CB(p)->same_flow = !diffs;
	}

	return head;
}

static inline void mcps_skb_gro_reset_offset(struct sk_buff *skb, u32 nhoff)
{
	const struct skb_shared_info *pinfo = skb_shinfo(skb);
	const skb_frag_t *frag0 = &pinfo->frags[0];

	NAPI_GRO_CB(skb)->data_offset = 0;
	NAPI_GRO_CB(skb)->frag0 = NULL;
	NAPI_GRO_CB(skb)->frag0_len = 0;

	if (skb_mac_header(skb) == skb_tail_pointer(skb) &&
		pinfo->nr_frags &&
		!PageHighMem(skb_frag_page(frag0)) &&
		(!NET_IP_ALIGN || !((skb_frag_off(frag0) + nhoff) & 3))) {
		NAPI_GRO_CB(skb)->frag0 = skb_frag_address(frag0);
		NAPI_GRO_CB(skb)->frag0_len = min_t(unsigned int,
		skb_frag_size(frag0),
				skb->end - skb->tail);
	}
}

static void mcps_gro_pull_from_frag0(struct sk_buff *skb, int grow)
{
	struct skb_shared_info *pinfo = skb_shinfo(skb);

	BUG_ON(skb->end - skb->tail < grow);

	memcpy(skb_tail_pointer(skb), NAPI_GRO_CB(skb)->frag0, grow);

	skb->data_len -= grow;
	skb->tail += grow;

	skb_frag_off_add(&pinfo->frags[0], grow);
	skb_frag_size_sub(&pinfo->frags[0], grow);

	if (unlikely(!skb_frag_size(&pinfo->frags[0]))) {
		skb_frag_unref(skb, 0);
		memmove(pinfo->frags, pinfo->frags + 1,
				--pinfo->nr_frags * sizeof(pinfo->frags[0]));
	}
}

#define MAX_GRO_SKBS 8

INDIRECT_CALLABLE_DECLARE(struct sk_buff *inet_gro_receive(struct list_head *,
							   struct sk_buff *));
INDIRECT_CALLABLE_DECLARE(struct sk_buff *ipv6_gro_receive(struct list_head *,
							   struct sk_buff *));
static enum gro_result mcps_dev_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	u32 hash = skb_get_hash_raw(skb) & (GRO_HASH_BUCKETS - 1);
	struct packet_offload *ptype;
	__be16 type = skb->protocol;
	struct list_head *gro_head;
	struct sk_buff *pp = NULL;
	enum gro_result ret;
	int same_flow;
	int grow;

	if (netif_elide_gro(skb->dev))
		goto normal;

	gro_head = mcps_gro_list_prepare(napi, skb);

	rcu_read_lock();
	ptype = gro_find_receive_by_type(type);

	//substitution
	//if (&ptype->list == head)
	//		goto normal;
	if (!ptype) {
		rcu_read_unlock();
		goto normal;
	}

	skb_set_network_header(skb, skb_gro_offset(skb));
	skb_reset_mac_len(skb);
	NAPI_GRO_CB(skb)->same_flow = 0;
	NAPI_GRO_CB(skb)->flush = skb_is_gso(skb) || skb_has_frag_list(skb);

	NAPI_GRO_CB(skb)->free = 0;
	NAPI_GRO_CB(skb)->encap_mark = 0;
	NAPI_GRO_CB(skb)->recursion_counter = 0;
	NAPI_GRO_CB(skb)->is_fou = 0;
#if defined(CONFIG_MCPS_ON_EXYNOS)
	NAPI_GRO_CB(skb)->is_atomic = 1;
#endif
	NAPI_GRO_CB(skb)->gro_remcsum_start = 0;

	/* Setup for GRO checksum validation */
	switch (skb->ip_summed) {
	case CHECKSUM_COMPLETE:
		NAPI_GRO_CB(skb)->csum = skb->csum;
		NAPI_GRO_CB(skb)->csum_valid = 1;
		NAPI_GRO_CB(skb)->csum_cnt = 0;
		break;
	case CHECKSUM_UNNECESSARY:
		NAPI_GRO_CB(skb)->csum_cnt = skb->csum_level + 1;
		NAPI_GRO_CB(skb)->csum_valid = 0;
		break;
	default:
		NAPI_GRO_CB(skb)->csum_cnt = 0;
		NAPI_GRO_CB(skb)->csum_valid = 0;
	}

	pp = INDIRECT_CALL_INET(ptype->callbacks.gro_receive,
				ipv6_gro_receive, inet_gro_receive,
				gro_head, skb);
	rcu_read_unlock();

	if (IS_ERR(pp) && PTR_ERR(pp) == -EINPROGRESS) {
		ret = GRO_CONSUMED;
		goto ok;
	}

	same_flow = NAPI_GRO_CB(skb)->same_flow;
	ret = NAPI_GRO_CB(skb)->free ? GRO_MERGED_FREE : GRO_MERGED;

	if (pp) {
		skb_list_del_init(pp);
		mcps_napi_gro_complete(napi, pp);
		napi->gro_hash[hash].count--;
	}

	if (same_flow)
		goto ok;

	if (NAPI_GRO_CB(skb)->flush)
		goto normal;

	if (unlikely(napi->gro_hash[hash].count >= MAX_GRO_SKBS))
		mcps_gro_flush_oldest(napi, gro_head);
	else
		napi->gro_hash[hash].count++;

	NAPI_GRO_CB(skb)->count = 1;
	NAPI_GRO_CB(skb)->age = jiffies;
	NAPI_GRO_CB(skb)->last = skb;
	skb_shinfo(skb)->gso_size = skb_gro_len(skb);
	list_add(&skb->list, gro_head);
	ret = GRO_HELD;

pull:
	grow = skb_gro_offset(skb) - skb_headlen(skb);
	if (grow > 0)
		mcps_gro_pull_from_frag0(skb, grow);
ok:
	if (napi->gro_hash[hash].count) {
		if (!test_bit(hash, &napi->gro_bitmask))
			__set_bit(hash, &napi->gro_bitmask);
	} else if (test_bit(hash, &napi->gro_bitmask)) {
		__clear_bit(hash, &napi->gro_bitmask);
	}

	return ret;

normal:
	ret = GRO_NORMAL;
	goto pull;
}

static void mcps_napi_skb_free_stolen_head(struct sk_buff *skb)
{
	WARN(skb->destructor, "MCPSW : There is destructor");
	skb->destructor = NULL;

#if IS_ENABLED(CONFIG_NF_CONNTRACK)
	WARN(skb_get_nfct(skb), "MCPSW : There is nfct");
	skb_set_nfct(skb, 0UL);
#endif

	kfree_skb_partial(skb, 1);
}

static gro_result_t mcps_napi_skb_finish(gro_result_t ret, struct sk_buff *skb, struct napi_struct *napi)
{
	switch (ret) {
	case GRO_NORMAL:
		mcps_gro_normal_one(napi, skb, 1);
		break;

	case GRO_DROP:
		kfree_skb(skb);
		break;

	case GRO_MERGED_FREE:
		if (NAPI_GRO_CB(skb)->free == NAPI_GRO_FREE_STOLEN_HEAD)
			mcps_napi_skb_free_stolen_head(skb);
		else
			__kfree_skb(skb);
		break;

	case GRO_HELD:
	case GRO_MERGED:
	case GRO_CONSUMED:
		break;
	}

	return ret;
}

gro_result_t mcps_napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	gro_result_t ret;

	skb_mark_napi_id(skb, napi);
	mcps_skb_gro_reset_offset(skb, 0);

	ret = mcps_napi_skb_finish(mcps_dev_gro_receive(napi, skb), skb, napi);

	return ret;
}

static long mcps_gro_flush_time;
module_param(mcps_gro_flush_time, long, 0640);

static int mcps_gro_flush_timer(struct mcps_pantry *pantry)
{
	struct timespec64 curr, diff;

	if (!mcps_gro_flush_time)
		return 0;

	if (unlikely(pantry->gro_flush_time.tv_sec == 0)) {
		getnstimespec64ofday(&pantry->gro_flush_time);
	} else {
		getnstimespec64ofday(&(curr));
		diff = timespec64_sub(curr, pantry->gro_flush_time);
		if ((diff.tv_sec > 0) || (diff.tv_nsec > mcps_gro_flush_time)) {
			getnstimespec64ofday(&pantry->gro_flush_time);
			return 1;
		}
	}
	return 0;
}

static unsigned int mcps_gro_light_flush_limit __read_mostly;
module_param(mcps_gro_light_flush_limit, uint, 0640);

static unsigned int mcps_max_gro_batch_count __read_mostly = 300;
module_param(mcps_max_gro_batch_count, uint, 0640);

static int flush_gro_napi_struct(struct napi_struct *napi, int quota)
{
	struct mcps_pantry *pantry = container_of(napi, struct mcps_pantry, gro_napi_struct);
#if defined(CONFIG_MCPS_CHUNK_GRO)
	if (pantry->modem_napi_work >= MODEM_NAPI_QUOTA) {
		pantry->modem_napi_work_batched += pantry->modem_napi_work;
		pantry->modem_napi_work = 0;

		if (mcps_napi_gro_light_flush(&pantry->gro_napi_struct, mcps_gro_light_flush_limit)
		|| pantry->modem_napi_work_batched >= mcps_max_gro_batch_count) {
			pantry->modem_napi_work_batched = 0;
			mcps_gro_normal_list(&pantry->gro_napi_struct);
		}

		local_irq_disable();
		__napi_schedule_irqoff(&pantry->gro_napi_struct);
		local_irq_enable();
		return 0;
	}

	pantry->modem_napi_work = pantry->modem_napi_work_batched = 0;
	mcps_napi_gro_flush(&pantry->gro_napi_struct, false);
	getnstimespec64ofday(&pantry->gro_flush_time);
	mcps_gro_normal_list(&pantry->gro_napi_struct);
#else // #if defined(CONFIG_MCPS_CHUNK_GRO)
	mcps_napi_gro_flush(&pantry->gro_napi_struct, false);
	getnstimespec64ofday(&pantry->gro_flush_time);
	mcps_gro_normal_list(&pantry->gro_napi_struct);
#endif // #if defined(CONFIG_MCPS_CHUNK_GRO)

	local_irq_disable();
	mcps_napi_complete(&pantry->gro_napi_struct);
	local_irq_enable();
	return 0;
}

static int process_gro_skb(struct sk_buff *skb)
{
	struct mcps_pantry *pantry = this_cpu_ptr(&mcps_gro_pantries);
	unsigned long flags;
	gro_result_t ret;
#if defined(CONFIG_MCPS_GRO_PER_SESSION)
	bool flush = false;
	u32 hash = 0;
#endif

	tracing_mark_writev('B', 1111, "process_gro_skb", 0);

#if defined(CONFIG_MCPS_GRO_PER_SESSION)
	if (mcps_eye_gro_skip(skb)) {
		__this_cpu_inc(mcps_gro_pantries.processed);
		if (unlikely(mcps_try_skb_internal(skb)))
			netif_receive_skb(skb);
	} else {
		hash = skb_get_hash_raw(skb);
		flush = mcps_eye_gro_flush(skb);
		ret = mcps_napi_gro_receive(&pantry->gro_napi_struct, skb);
		if (ret == GRO_NORMAL)
			__this_cpu_inc(mcps_gro_pantries.processed);
		else
			__this_cpu_inc(mcps_gro_pantries.gro_processed);

		if (flush)
			mcps_napi_gro_single_flush(&pantry->gro_napi_struct, hash);
	}
#else
	ret = mcps_napi_gro_receive(&pantry->gro_napi_struct, skb);
	if (ret == GRO_NORMAL) {
		__this_cpu_inc(mcps_gro_pantries.processed);
		getnstimespec64ofday(&pantry->gro_flush_time);
	} else {
		__this_cpu_inc(mcps_gro_pantries.gro_processed);
	}
#endif // #if defined(CONFIG_MCPS_GRO_PER_SESSION)

	if (mcps_gro_flush_timer(pantry)) {
		mcps_napi_gro_flush(&pantry->gro_napi_struct, false);
		mcps_gro_normal_list(&pantry->gro_napi_struct);
	}

	local_irq_save(flags);
	if (!__test_and_set_bit(NAPI_STATE_SCHED, &pantry->gro_napi_struct.state))
		__napi_schedule_irqoff(&pantry->gro_napi_struct);

	local_irq_restore(flags);

#if defined(CONFIG_MCPS_CHUNK_GRO)
	pantry->modem_napi_work++;
#endif // #if defined(CONFIG_MCPS_CHUNK_GRO)
	tracing_mark_writev('E', 1111, "process_gro_skb", 0);
	return NET_RX_SUCCESS;
}

static struct timespec64 last_complete_poll_time;

void mcps_complete_handler(int work, int stop)
{
	ktime_get_real_ts64(&last_complete_poll_time);
}
EXPORT_SYMBOL_GPL(mcps_complete_handler);

void mcps_prepare_handler(void)
{
	if (!mcps_is_migration_request_empty()) {
		struct timespec64 curr, diff;

		ktime_get_real_ts64(&curr);
		diff = timespec64_sub(curr, last_complete_poll_time);

		if (mcps_is_qualified_interval(&diff))
			mcps_run_migration();
	}
}
EXPORT_SYMBOL_GPL(mcps_prepare_handler);

#else // #if defined(CONFIG_MCPS_V2)
#define mcps_napi_gro_receive(napi, skb) napi_gro_receive(napi, skb)
#define mcps_napi_gro_flush(napi, flush_old) napi_gro_flush(napi, flush_old)
#endif // #if defined(CONFIG_MCPS_V2)

static long mcps_agro_flush_time = 5000L;
module_param(mcps_agro_flush_time, long, 0640);

static void mcps_agro_flush_timer(struct mcps_pantry *pantry)
{
	struct timespec64 curr, diff;

	if (!mcps_agro_flush_time) {
		mcps_napi_gro_flush(&pantry->rx_napi_struct, false);
		mcps_gro_normal_list(&pantry->rx_napi_struct);
		return;
	}

	if (unlikely(pantry->agro_flush_time.tv_sec == 0)) {
		getnstimespec64ofday(&pantry->agro_flush_time);
	} else {
		getnstimespec64ofday(&(curr));
		diff = timespec64_sub(curr, pantry->agro_flush_time);
		if ((diff.tv_sec > 0) || (diff.tv_nsec > mcps_agro_flush_time)) {
			mcps_napi_gro_flush(&pantry->rx_napi_struct, false);
			getnstimespec64ofday(&pantry->agro_flush_time);
			mcps_gro_normal_list(&pantry->rx_napi_struct);
		}
	}
}

void mcps_current_processor_gro_flush(void)
{
	struct mcps_pantry *pantry = this_cpu_ptr(&mcps_gro_pantries);

	mcps_napi_gro_flush(&pantry->rx_napi_struct, false);
	getnstimespec64ofday(&pantry->agro_flush_time);
	mcps_gro_normal_list(&pantry->rx_napi_struct);
#if defined(CONFIG_MCPS_V2)
	mcps_napi_gro_flush(&pantry->gro_napi_struct, false);
	getnstimespec64ofday(&pantry->gro_flush_time);
	mcps_gro_normal_list(&pantry->gro_napi_struct);
#endif // #if defined(CONFIG_MCPS_V2)
}

static int process_gro_pantry(struct napi_struct *napi, int quota)
{
	struct mcps_pantry *pantry = container_of(napi, struct mcps_pantry, rx_napi_struct);
	int work = 0;
	bool again = true;
	gro_result_t ret;
#if defined(CONFIG_MCPS_GRO_PER_SESSION)
	bool flush = false;
	u32 hash = 0;
#endif

	tracing_mark_writev('B', 1111, "process_gro_pantry", 0);
	if (mcps_on_ipi_waiting(pantry)) {
		local_irq_disable();
		mcps_do_ipi_and_irq_enable(pantry);
	}

	getnstimespec64ofday(&pantry->agro_flush_time);

	while (again) {
		struct sk_buff *skb;

		while ((skb = __skb_dequeue(&pantry->process_queue))) {
#if defined(CONFIG_MCPS_GRO_PER_SESSION)
			if (mcps_eye_gro_skip(skb)) {
				__this_cpu_inc(mcps_gro_pantries.processed);
				if (unlikely(mcps_try_skb_internal(skb)))
					netif_receive_skb(skb);
			} else {
				hash = skb_get_hash_raw(skb);
				flush = mcps_eye_gro_flush(skb);
				ret = mcps_napi_gro_receive(napi, skb);

				if (ret == GRO_NORMAL) {
					__this_cpu_inc(mcps_gro_pantries.processed);
					getnstimespec64ofday(&pantry->agro_flush_time);
				} else {
					__this_cpu_inc(mcps_gro_pantries.gro_processed);
				}

				if (flush)
					mcps_napi_gro_single_flush(napi, hash);
			}
#else // #if defined(CONFIG_MCPS_GRO_PER_SESSION)
			ret = mcps_napi_gro_receive(napi, skb);
			if (ret == GRO_NORMAL) {
				__this_cpu_inc(mcps_gro_pantries.processed);
				getnstimespec64ofday(&pantry->agro_flush_time);
			} else {
				__this_cpu_inc(mcps_gro_pantries.gro_processed);
			}
#endif // #if defined(CONFIG_MCPS_GRO_PER_SESSION)
			mcps_agro_flush_timer(pantry);

			if (++work >= quota)
				goto end;
		}

		local_irq_disable();
		pantry_lock(pantry);
		if (skb_queue_empty(&pantry->input_pkt_queue)) {
			mcps_napi_complete(&pantry->rx_napi_struct);
			again = false;
		} else {
			skb_queue_splice_tail_init(&pantry->input_pkt_queue,
									   &pantry->process_queue);
		}
		pantry_unlock(pantry);
		local_irq_enable();

	}
	mcps_napi_gro_flush(&pantry->rx_napi_struct, false);
	mcps_gro_normal_list(&pantry->rx_napi_struct);
end:
	PRINT_GRO_WORKED(pantry->cpu, work, quota);
	tracing_mark_writev('E', 1111, "process_gro_pantry", work);
	return work;
}

int mcps_gro_ipi_queued(struct mcps_pantry *pantry)
{
	struct mcps_pantry *this_pantry = this_cpu_ptr(&mcps_gro_pantries);

	if (pantry != this_pantry) {
		pantry->ipi_next = this_pantry->ipi_list;
		this_pantry->ipi_list = pantry;

		pantry_ipi_lock(this_pantry);
		if (!__test_and_set_bit(NAPI_STATE_SCHED, &this_pantry->ipi_napi_struct.state))
			__napi_schedule_irqoff(&this_pantry->ipi_napi_struct);
		pantry_ipi_unlock(this_pantry);

		PRINT_GRO_IPIQ(this_pantry->cpu, pantry->cpu);
		return 1;
	}
	return 0;
}

int splice_to_gro_pantry(struct sk_buff_head *pendings, int len, int cpu)
{
	struct mcps_pantry *pantry;
	unsigned long flags;
	unsigned int qlen;

	tracing_mark_writev('B', 1111, "splice_to_gro_pantry", cpu);
	pantry = &per_cpu(mcps_gro_pantries, cpu);

	local_irq_save(flags); //save register information into flags and disable irq (local_irq_disabled)

	pantry_lock(pantry);

	qlen = skb_queue_len(&pantry->input_pkt_queue);
	if (qlen <= mcps_gro_pantry_max_capability) {
		if (qlen) {
enqueue:
			skb_queue_splice_tail_init(pendings, &pantry->input_pkt_queue);

			pantry->enqueued += len;

			pantry_unlock(pantry);
			local_irq_restore(flags);
			tracing_mark_writev('E', 1111, "splice_to_gro_pantry", cpu);
			return NET_RX_SUCCESS;
		}

		/* Schedule NAPI for backlog device
		 * We can use non atomic operation since we own the queue lock
		 */
		if (!__test_and_set_bit(NAPI_STATE_SCHED, &pantry->rx_napi_struct.state)) {
			if (!mcps_gro_ipi_queued(pantry))
				__napi_schedule_irqoff(&pantry->rx_napi_struct);
		}
		goto enqueue;
	}

	MCPS_DEBUG("[%d] dropped\n", cpu);

	pantry->dropped += len;
	pantry_unlock(pantry);

	local_irq_restore(flags);
	tracing_mark_writev('E', 1111, "splice_to_gro_pantry", 99);
	return NET_RX_DROP;
}

static int enqueue_to_gro_pantry(struct sk_buff *skb, int cpu)
{
	struct mcps_pantry *pantry;
	unsigned long flags;
	unsigned int qlen;

	tracing_mark_writev('B', 1111, "enqueue_to_gro_pantry", cpu);
	pantry = &per_cpu(mcps_gro_pantries, cpu);

	local_irq_save(flags); //save register information into flags and disable irq (local_irq_disabled)

	pantry_lock(pantry);

	// hp off.
	if (pantry->offline) {
		int hdr_cpu = 0;

		pantry_unlock(pantry);
		local_irq_restore(flags);

		hdr_cpu = try_to_hqueue(skb->hash, cpu, skb, MCPS_AGRO_LAYER);
		cpu = hdr_cpu;
		if (hdr_cpu < 0) {
			tracing_mark_writev('E', 1111, "enqueue_to_gro_pantry", 88);
			return NET_RX_SUCCESS;
		}

		pantry = &per_cpu(mcps_gro_pantries, hdr_cpu);

		local_irq_save(flags);
		pantry_lock(pantry);
	}

	qlen = skb_queue_len(&pantry->input_pkt_queue);
	if (qlen <= mcps_gro_pantry_max_capability) {
		if (qlen) {
enqueue:
			__skb_queue_tail(&pantry->input_pkt_queue, skb);

			pantry->enqueued++;

			pantry_unlock(pantry);
			local_irq_restore(flags);
			tracing_mark_writev('E', 1111, "enqueue_to_gro_pantry", cpu);
			return NET_RX_SUCCESS;
		}

		/* Schedule NAPI for backlog device
		 * We can use non atomic operation since we own the queue lock
		 */
		if (!__test_and_set_bit(NAPI_STATE_SCHED, &pantry->rx_napi_struct.state)) {
			if (!mcps_gro_ipi_queued(pantry))
				__napi_schedule_irqoff(&pantry->rx_napi_struct);
		}
		goto enqueue;
	}

	MCPS_DEBUG("[%d] dropped\n", cpu);

	pantry->dropped++;
	pantry_unlock(pantry);

	local_irq_restore(flags);
	tracing_mark_writev('E', 1111, "enqueue_to_gro_pantry", 99);
	return NET_RX_DROP;
}

static int
mcps_try_gro_internal(struct sk_buff *skb)
{
	int cpu = -1;
	int ret = -1;
	u32 hash;

	rcu_read_lock();
	flush_flows(0);
	skb_reset_network_header(skb);
	hash = skb_get_hash(skb);

	if (!hash) {
		rcu_read_unlock();
		goto error;
	}

	cpu = get_agro_cpu(&mcps->sauron, skb);
	if (cpu < 0 || cpu >= MCPS_CPU_ERROR) {
		rcu_read_unlock();
		goto error;
	}

	switch (cpu) {
#if defined(CONFIG_MCPS_V2)
	case MCPS_CPU_DIRECT_GRO:
		ret = process_gro_skb(skb);
		break;
#endif // #if defined(CONFIG_MCPS_V2)
	case MCPS_CPU_GRO_BYPASS:
		cpu = get_arps_cpu(&mcps->sauron, skb);
		if (!cpu_possible(cpu)) {
			rcu_read_unlock();
			goto error;
		}
		if (cpu == MCPS_CPU_ON_PENDING)
			goto pending;

		ret = enqueue_to_pantry(skb, cpu);
		break;
	case MCPS_CPU_ON_PENDING:
pending:
		rcu_read_unlock();
		return NET_RX_SUCCESS;
	default:
		if (likely(cpu_possible(cpu))) {
			ret = enqueue_to_gro_pantry(skb, cpu);
		} else {
			rcu_read_unlock();
			goto error;
		}
		break;
	}
	rcu_read_unlock();

	if (ret == NET_RX_DROP) {
		mcps_drop_packet(hash);
		goto error;
	}

	return ret;
error:
	PRINT_TRY_FAIL(hash, cpu);
	return NET_RX_DROP;
}

/* mcps_try_gro - Main entry point of mcps gro
 * return NET_RX_DROP(1) if mcps fail or disabled, otherwise NET_RX_SUCCESS(0)
 */
int mcps_try_gro(struct sk_buff *skb)
{
	if (!mcps_enable)
		return NET_RX_DROP;

	return mcps_try_gro_internal(skb);
}
EXPORT_SYMBOL(mcps_try_gro);

int mcps_gro_cpu_startup_callback(unsigned int ocpu)
{
	struct mcps_pantry *pantry = &per_cpu(mcps_gro_pantries, ocpu);

	tracing_mark_writev('B', 1111, "mcps_gro_online", ocpu);

	local_irq_disable();
	pantry_lock(pantry);
	pantry->offline = 0;
	pantry_unlock(pantry);
	local_irq_enable();

	hotplug_on(ocpu, 0, MCPS_AGRO_LAYER);

	tracing_mark_writev('E', 1111, "mcps_gro_online", ocpu);
	return 0;
}

int mcps_gro_cpu_teardown_callback(unsigned int ocpu)
{
	unsigned int cpu, oldcpu = ocpu;
	struct mcps_pantry *pantry, *oldpantry;
	unsigned int qlen = 0;

	struct sk_buff_head queue;
	bool needsched = false;
	struct list_head pinfo_queue;

	tracing_mark_writev('B', 1111, "mcps_gro_offline", ocpu);
	skb_queue_head_init(&queue);
	INIT_LIST_HEAD(&pinfo_queue);

	cpu = light_cpu();
	if (cpu == ocpu)
		cpu = 0;

	pantry = &per_cpu(mcps_gro_pantries, cpu);
	oldpantry = &per_cpu(mcps_gro_pantries, oldcpu);

	MCPS_DEBUG("[%u] : CPU[%u -> %u] - HOTPLUGGED OUT\n"
			, smp_processor_id_safe(), oldcpu, cpu);

	mcps_napi_gro_flush(&oldpantry->rx_napi_struct, false);
	getnstimespec64ofday(&oldpantry->agro_flush_time);
	mcps_gro_normal_list(&oldpantry->rx_napi_struct);

#if defined(CONFIG_MCPS_V2)
	mcps_napi_gro_flush(&oldpantry->gro_napi_struct, false);
	getnstimespec64ofday(&oldpantry->gro_flush_time);
	mcps_gro_normal_list(&oldpantry->gro_napi_struct);
#endif // #if defined(CONFIG_MCPS_V2)

	//detach first.
	local_irq_disable();
	pantry_lock(oldpantry);
	oldpantry->offline = 1;

	if (test_bit(NAPI_STATE_SCHED, &oldpantry->rx_napi_struct.state))
		mcps_napi_complete(&oldpantry->rx_napi_struct);

	if (test_bit(NAPI_STATE_SCHED, &oldpantry->ipi_napi_struct.state))
		mcps_napi_complete(&oldpantry->ipi_napi_struct);

#if defined(CONFIG_MCPS_V2)
	if (test_bit(NAPI_STATE_SCHED, &oldpantry->gro_napi_struct.state))
		mcps_napi_complete(&oldpantry->gro_napi_struct);
#endif // #if defined(CONFIG_MCPS_V2)

	if (!skb_queue_empty(&oldpantry->process_queue)) {
		qlen += skb_queue_len(&oldpantry->process_queue);
		skb_queue_splice_tail_init(&oldpantry->process_queue, &queue);
	}

	if (!skb_queue_empty(&oldpantry->input_pkt_queue)) {
		qlen += skb_queue_len(&oldpantry->input_pkt_queue);
		skb_queue_splice_tail_init(&oldpantry->input_pkt_queue, &queue);
	}

	oldpantry->ignored += qlen;
	oldpantry->processed += qlen;

	splice_pending_info_2q(oldcpu, &pinfo_queue);

	pantry_unlock(oldpantry);
	local_irq_enable();

	// attach second.
	local_irq_disable();
	pantry_lock(pantry);

	if (!skb_queue_empty(&queue)) {
		skb_queue_splice_tail_init(&queue, &pantry->input_pkt_queue);
		pantry->enqueued += qlen;
		needsched = true;
	}

	qlen = pop_hqueue(cpu, oldcpu, &pantry->input_pkt_queue, 1);
	if (qlen) {
		pantry->enqueued += qlen;
		needsched = true;
	}

	if (!list_empty(&pinfo_queue)) {
		needsched = true;
		update_pending_info(&pinfo_queue, cpu, pantry->enqueued);
		splice_pending_info_2cpu(&pinfo_queue, cpu);
	}

	needsched = (needsched && !__test_and_set_bit(NAPI_STATE_SCHED, &pantry->rx_napi_struct.state));

	pantry_unlock(pantry);
	local_irq_enable();

	if (needsched && mcps_cpu_online(cpu))
		smp_call_function_single_async(pantry->cpu, &pantry->csd);

	tracing_mark_writev('E', 1111, "mcps_gro_offline", cpu);
	return 0;
}

void mcps_gro_init(struct net_device *mcps_device)
{
	int i = 0;

	for_each_possible_cpu(i) {
		if (VALID_CPU(i)) {
			struct mcps_pantry *pantry = &per_cpu(mcps_gro_pantries, i);

			skb_queue_head_init(&pantry->input_pkt_queue);
			skb_queue_head_init(&pantry->process_queue);

			pantry->received_arps = 0;
			pantry->processed = 0;
			pantry->gro_processed = 0;
			pantry->enqueued = 0;
			pantry->dropped = 0;
			pantry->ignored = 0;

			pantry->csd.func = mcps_napi_schedule;
			pantry->csd.info = pantry;
			pantry->cpu = i;
			pantry->offline = 0;

			memset(&pantry->rx_napi_struct, 0, sizeof(struct napi_struct));
			netif_napi_add(mcps_device, &pantry->rx_napi_struct,
					process_gro_pantry, mcps_gro_pantry_quota);
			napi_enable(&pantry->rx_napi_struct);

			memset(&pantry->ipi_napi_struct, 0, sizeof(struct napi_struct));
			netif_napi_add(mcps_device, &pantry->ipi_napi_struct,
						   schedule_ipi, 1);
			napi_enable(&pantry->ipi_napi_struct);

#if defined(CONFIG_MCPS_V2)
			memset(&pantry->gro_napi_struct, 0, sizeof(struct napi_struct));
			netif_napi_add(mcps_device, &pantry->gro_napi_struct,
						   flush_gro_napi_struct, 1);
			napi_enable(&pantry->gro_napi_struct);

			getnstimespec64ofday(&pantry->gro_flush_time);

			INIT_LIST_HEAD(&pantry->gro_napi_struct.rx_list);

#endif // #if defined(CONFIG_MCPS_V2)
			getnstimespec64ofday(&pantry->agro_flush_time);
		}
	}

	ktime_get_real_ts64(&last_complete_poll_time);
	init_migration_manager(mcps_migration_handler);
	MCPS_DEBUG("COMPLETED\n");
}

void mcps_gro_exit(void)
{
	int i = 0;

	for_each_possible_cpu(i) {
		if (VALID_CPU(i)) {
			struct mcps_pantry *pantry = &per_cpu(mcps_gro_pantries, i);

			napi_disable(&pantry->rx_napi_struct);
			netif_napi_del(&pantry->rx_napi_struct);

			napi_disable(&pantry->ipi_napi_struct);
			netif_napi_del(&pantry->ipi_napi_struct);

#if defined(CONFIG_MCPS_V2)
			napi_disable(&pantry->gro_napi_struct);
			netif_napi_del(&pantry->gro_napi_struct);
#endif // #if defined(CONFIG_MCPS_V2)
		}
	}

	release_migration_manager();
}
//#endif
