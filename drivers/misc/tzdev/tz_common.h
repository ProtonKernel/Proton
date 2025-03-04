/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TZ_COMMON_H__
#define __TZ_COMMON_H__

#ifndef __USED_BY_TZSL__
#include <crypto/sha.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/limits.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

#define TZ_IOC_MAGIC			'c'

#define TZIO_MEM_REGISTER		_IOW(TZ_IOC_MAGIC, 120, struct tzio_mem_register)
#define TZIO_CRYPTO_CLOCK_CONTROL	_IOW(TZ_IOC_MAGIC, 123, int)
#define TZIO_GET_SYSCONF		_IOW(TZ_IOC_MAGIC, 124, struct tzio_sysconf)
#define TZIO_BOOST_ON			_IOW(TZ_IOC_MAGIC, 125, int)
#define TZIO_BOOST_OFF			_IOW(TZ_IOC_MAGIC, 126, int)
#define TZIO_PMF_INIT			_IOW(TZ_IOC_MAGIC, 127, struct tzio_pmf_init)
#define TZIO_PMF_FINI			_IOW(TZ_IOC_MAGIC, 128, int)
#define TZIO_PMF_GET_TICKS		_IOW(TZ_IOC_MAGIC, 129, int)

#define TZIO_UIWSOCK_CONNECT		_IOW(TZ_IOC_MAGIC, 130, int)
#define TZIO_UIWSOCK_WAIT_CONNECTION	_IOW(TZ_IOC_MAGIC, 131, int)
#define TZIO_UIWSOCK_SEND		_IOW(TZ_IOC_MAGIC, 132, int)
#define TZIO_UIWSOCK_RECV_MSG		_IOWR(TZ_IOC_MAGIC, 133, int)
#define TZIO_UIWSOCK_LISTEN		_IOWR(TZ_IOC_MAGIC, 134, int)
#define TZIO_UIWSOCK_ACCEPT		_IOWR(TZ_IOC_MAGIC, 135, int)
#define TZIO_UIWSOCK_GETSOCKOPT		_IOWR(TZ_IOC_MAGIC, 136, int)
#define TZIO_UIWSOCK_SETSOCKOPT		_IOWR(TZ_IOC_MAGIC, 137, int)

/* NB: Sysconf related definitions should match with those in SWd */
#define SYSCONF_SWD_VERSION_LEN			256
#define SYSCONF_SWD_EARLY_SWD_INIT		(1 << 0)

#define SYSCONF_NWD_CRYPTO_CLOCK_MANAGEMENT	(1 << 0)
#define SYSCONF_NWD_CPU_HOTPLUG			(1 << 1)
#define SYSCONF_NWD_TZDEV_DEPLOY_TZAR		(1 << 2)

#ifndef SHA256_DIGEST_SIZE
#define SHA256_DIGEST_SIZE			32
#endif

#ifndef CRED_HASH_SIZE
#define CRED_HASH_SIZE				SHA256_DIGEST_SIZE
#endif

enum cluster_type {
	CPU_CLUSTER_BIG,
	CPU_CLUSTER_MIDDLE,
	CPU_CLUSTER_LITTLE,
	CPU_CLUSTER_MAX,
};

struct sysconf_cluster_info {
	uint32_t type;
	uint32_t nr_cpus;
	uint32_t mask;
} __attribute__((__packed__));

struct tzio_swd_sysconf {
	uint32_t os_version;			/* SWd OS version */
	uint32_t cpu_num;			/* SWd number of cpu supported */
	uint32_t flags;				/* SWd flags */
	uint32_t nr_clusters;					/* Number of CPU clusters */
	struct sysconf_cluster_info cluster_info[CPU_CLUSTER_MAX];	/* Cluster layout */
	char version[SYSCONF_SWD_VERSION_LEN];	/* SWd OS version string */
} __attribute__((__packed__));

struct tzio_nwd_sysconf {
	uint32_t iwi_event;			/* NWd interrupt to notify about SWd event */
	uint32_t iwi_panic;			/* NWd interrupt to notify about SWd panic */
	uint32_t flags;				/* NWd flags */
} __attribute__((__packed__));

struct tzio_sysconf {
	struct tzio_nwd_sysconf nwd_sysconf;
	struct tzio_swd_sysconf swd_sysconf;
} __attribute__((__packed__));

struct tzio_mem_register {
	uint64_t size;			/* Memory region size (in) */
	uint32_t write;			/* 1 - rw, 0 - ro */
} __attribute__((__packed__));

struct tzio_pmf_init {
	const uint64_t ptr;				/* Memory region with pmf_sample */
	uint32_t id;					/* shmem_id for sample data */
} __attribute__((__packed__));

/* Used for switching crypto clocks ON/OFF */
enum {
	TZIO_CRYPTO_CLOCK_OFF,
	TZIO_CRYPTO_CLOCK_ON,
};

#ifndef __USED_BY_TZSL__
struct tz_uuid {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint8_t clock_seq_and_node[8];
} __attribute__((__packed__));

enum {
	TZ_CRED_UUID,
	TZ_CRED_HASH,
	TZ_CRED_KERNEL
};
#define TZ_CRED_HASH_SIZE 32

struct tz_cred {
	uint32_t pid;
	uint32_t uid;
	uint32_t gid;
	union {
		struct tz_uuid uuid;
		uint8_t hash[TZ_CRED_HASH_SIZE];
	};
	uint32_t type; /* TZ_CRED_UUID/TZ_CRED_HASH */
} __attribute__((__packed__));

#else /* __USED_BY_TZSL__ */
#  include <tz_cred.h>
#  include <tz_uuid.h>
#endif /* __USED_BY_TZSL__ */

struct tz_msghdr {
	char		*msgbuf;
	unsigned long	msglen;
	char		*msg_control;
	unsigned long	msg_controllen;
	int		msg_flags;
};

#ifdef CONFIG_COMPAT
struct compat_tz_msghdr {
	uint32_t	msgbuf;
	uint32_t	msglen;
	uint32_t	msg_control;
	uint32_t	msg_controllen;
	int32_t		msg_flags;
};
#endif

struct tz_cmsghdr {
	uint32_t cmsg_len;	/* data byte count, including header */
	uint32_t cmsg_level;	/* originating protocol */
	uint32_t cmsg_type;	/* protocol-specific type */
	/* followed by the actual control message data */
};

#define TZ_CMSG_ALIGN(LEN)		(((LEN) + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1))
#define TZ_CMSG_DATA(CMSG)		((void *)((char *)(CMSG) + TZ_CMSG_ALIGN(sizeof(struct tz_cmsghdr))))
#define TZ_CMSG_SPACE(LEN)		(TZ_CMSG_ALIGN(sizeof(struct tz_cmsghdr)) + TZ_CMSG_ALIGN(LEN))
#define TZ_CMSG_LEN(LEN)		(TZ_CMSG_ALIGN(sizeof(struct tz_cmsghdr)) + (LEN))

static inline
struct tz_cmsghdr *TZ_CMSG_FIRSTHDR(const struct tz_msghdr *msg)
{
	return ((msg->msg_controllen >= sizeof(struct tz_cmsghdr)) ? (struct tz_cmsghdr *)msg->msg_control : NULL);
}

static inline
struct tz_cmsghdr *TZ_CMSG_NXTHDR(const struct tz_msghdr *msg, const struct tz_cmsghdr *cmsg)
{
	struct tz_cmsghdr *ret;
	const uint32_t cmsg_len = cmsg->cmsg_len;
	const void *ctl = msg->msg_control;
	const size_t size = msg->msg_controllen;

	if (cmsg_len < sizeof(struct tz_cmsghdr))
		return NULL;

	ret = (struct tz_cmsghdr *)(((unsigned long)cmsg) + TZ_CMSG_ALIGN(cmsg_len));
	if ((size_t)((unsigned long)(ret + 1) - (unsigned long)ctl) > size)
		return NULL;

	return ret;
}

struct tz_cmsghdr_cred {
	struct tz_cmsghdr cmsghdr;
	struct tz_cred cred;
};

#define TZ_UIWSOCK_MAX_NAME_LENGTH	256

struct tz_uiwsock_connection {
	char name[TZ_UIWSOCK_MAX_NAME_LENGTH];
};

struct tz_uiwsock_data {
	uint64_t buffer;
	uint64_t size;
	uint32_t flags;
} __attribute__((__packed__));

struct tz_uiwsock_sockopt {
	int32_t level;
	int32_t optname;
	uint64_t optval;
	uint64_t optlen;
} __attribute__((__packed__));

#endif /*__TZ_COMMON_H__*/
