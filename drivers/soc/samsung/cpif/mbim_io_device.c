// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Samsung Electronics.
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

#include <linux/ip.h>
#include <linux/ipv6.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_dump.h"


int mbim_xmit(struct sk_buff *skb)
{
	struct modem_ctl *mc = get_mc();
	struct io_device *iod = NULL;
	struct link_device *ld = get_current_link(mc->iod);
	struct sk_buff *skb_new = skb;
	int ret;
	struct iphdr *iph = (struct iphdr *)skb->data;
	struct ipv6hdr *ip6h;
	static int current_index = 0;
	int count = 0;
	int i = current_index;
	bool find_flag = false;
#ifdef DEBUG_MODEM_IF
	struct timespec ts;
#endif

#ifdef DEBUG_MODEM_IF
	/* Record the timestamp */
	getnstimeofday(&ts);
#endif

#if IS_ENABLED(CONFIG_NET_SUPPORT_DROPDUMP)
	skb->dropmask = PACKET_OUT;
#endif

	if (unlikely(!cp_online(mc)))
		goto drop;

	while (count < RMNET_COUNT) {
		if (ld->pdn_table.pdn[i].is_activated == true) {
			if (iph->version == 4) {
				skb->protocol = htons(ETH_P_IP);
				if (ld->pdn_table.pdn[i].ipv4_src_addr == iph->saddr)
					find_flag = true;
			} else {
				skb->protocol = htons(ETH_P_IPV6);
				ip6h = (struct ipv6hdr *)skb->data;
				if (!memcmp(ld->pdn_table.pdn[i].ipv6_src_addr, ip6h->saddr.s6_addr, IPV6_ADDR_SIZE))
					find_flag = true;
			}
		}
		if (find_flag == true) {
			iod = link_get_iod_with_channel(ld, ld->get_ch_from_cid(ld->pdn_table.pdn[i].cid));
			current_index = i;
			break;
		}

		if (++i >= RMNET_COUNT)
			i = 0;

		count++;
	}

	/* packet capture for MBIM device */
	skb_reset_transport_header(skb);
	skb_reset_network_header(skb);
	skb_reset_mac_header(skb);

	if (!iod) {
		if (ld->internet_pdn_cid) {
			iod = link_get_iod_with_channel(ld, ld->get_ch_from_cid(ld->internet_pdn_cid));
		} else {
			if (iph->version == 4) {
				mif_err_limited("ERR! packet not in pdn table: from %pI4\n", &iph->saddr);
			} else {
				mif_err_limited("ERR! packet not in pdn table: from %pI6c\n", &ip6h->saddr.s6_addr);
			}

			goto drop;
		}
	}

	mif_queue_skb(skb_new, TX);

	skbpriv(skb_new)->sipc_ch = iod->ch;

#ifdef DEBUG_MODEM_IF
	/* Copy the timestamp to the skb */
	memcpy(&skbpriv(skb_new)->ts, &ts, sizeof(struct timespec));
#endif
#if defined(DEBUG_MODEM_IF_IODEV_TX) && defined(DEBUG_MODEM_IF_PS_DATA)
	mif_pkt(iod->ch, "IOD-TX", skb_new);
#endif

	ret = ld->send(ld, iod, skb_new);
	if (unlikely(ret < 0)) {
		if (ret != -EBUSY) {
			mif_err_limited("ERR! send fail:%d\n", ret);
		}
		goto drop;
	}

	/*
	 * If @skb has been expanded to $skb_new, @skb must be freed here.
	 * ($skb_new will be freed by the link device.)
	 */
	if (skb_new != skb)
		dev_consume_skb_any(skb);

	return 0;

drop:
	//DROPDUMP_QUEUE_SKB(skb, NET_DROPDUMP_OPT_MIF_TXFAIL);
	dev_kfree_skb_any(skb);

	/*
	If @skb has been expanded to $skb_new, $skb_new must also be freed here.
	*/
	if (skb_new != skb)
		dev_consume_skb_any(skb_new);

	return -1;
}
EXPORT_SYMBOL(mbim_xmit);

