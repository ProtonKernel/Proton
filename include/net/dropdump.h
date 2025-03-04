#ifndef __NET_DROPDUMP_H
#define __NET_DROPDUMP_H

#include <linux/netdevice.h>

/* vendor driver couldn't be used by builtin, with GKI.
   when using dropdump on GKI, check about that /trace/hoooks/net.h
   otherwise, by builtin driver, include /net/dropdump.h at /net/dst.h */

#if IS_ENABLED(CONFIG_SUPPORT_DROPDUMP)
extern void trace_android_vh_ptype_head
            (const struct packet_type *pt, struct list_head *vendor_pt);
extern void trace_android_vh_kfree_skb(struct sk_buff *skb);
#else
#define trace_android_vh_ptype_head(pt, vendor_pt)
#define trace_android_vh_kfree_skb(skb)
#endif
#endif //__NET_DROPDUMP_H
