#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/cdefs.h>
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
/* These complex macros don't translate well with Rust bindgen, so compute them with the C compiler and export them. */
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
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const char *platformDefaultHomePath();
extern int64_t msSinceEpoch();

#ifdef __cplusplus
}
#endif
