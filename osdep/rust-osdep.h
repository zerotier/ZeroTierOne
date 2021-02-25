/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/********************************************************************************************************************/

#ifdef __APPLE__
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/cdefs.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/ndrv.h>
#include <netinet/in_var.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <ifaddrs.h>
#ifdef __cplusplus
extern "C" {
#endif
/* These complex macros don't translate well with Rust bindgen, so compute
 * them with the C compiler and export them. */
extern const unsigned long c_BIOCSBLEN;
extern const unsigned long c_BIOCIMMEDIATE;
extern const unsigned long c_BIOCSSEESENT;
extern const unsigned long c_BIOCSETIF;
extern const unsigned long c_BIOCSHDRCMPLT;
extern const unsigned long c_BIOCPROMISC;
extern const unsigned long c_SIOCGIFINFO_IN6;
extern const unsigned long c_SIOCSIFINFO_FLAGS;
extern const unsigned long c_SIOCAUTOCONF_START;
extern const unsigned long c_SIOCAUTOCONF_STOP;
#ifdef __cplusplus
}
#endif
#endif /* __APPLE__ */

/********************************************************************************************************************/

#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_addr.h>
#include <linux/if_ether.h>
#include <ifaddrs.h>
#endif /* __linux__ */

/********************************************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

extern const char *platformDefaultHomePath();
extern int64_t msSinceEpoch();
extern void lockDownFile(const char *path, int isDir);
extern void getSecureRandom(void *buf, unsigned int len);
extern void sha384(const void *in, unsigned int len, void *out);
extern void sha512(const void *in, unsigned int len, void *out);

#ifdef __cplusplus
}
#endif
