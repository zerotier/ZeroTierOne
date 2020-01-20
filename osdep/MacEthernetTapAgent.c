/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

/*
 * This creates a pair of feth devices with the lower numbered device
 * being the ZeroTier virtual interface and the other being the device
 * used to actually read and write packets. The latter gets no IP config
 * and is only used for I/O. The behavior of feth is similar to the
 * veth pairs that exist on Linux.
 *
 * The feth device has only existed since MacOS Sierra, but that's fairly
 * long ago in Mac terms.
 *
 * I/O with feth must be done using two different sockets. The BPF socket
 * is used to receive packets, while an AF_NDRV (low-level network driver
 * access) socket must be used to inject. AF_NDRV can't read IP frames
 * since BSD doesn't forward packets out the NDRV tap if they've already
 * been handled, and while BPF can inject its MTU for injected packets
 * is limited to 2048. AF_NDRV packet injection is required to inject
 * ZeroTier's large MTU frames.
 *
 * Benchmarks show that this performs similarly to the old tap.kext driver,
 * and a kext is no longer required. Splitting it off into an agent will
 * also make it easier to have zerotier-one itself drop permissions.
 *
 * All this stuff is basically undocumented. A lot of tracing through
 * the Darwin/XNU kernel source was required to figure out how to make
 * this actually work.
 *
 * See also:
 *
 * https://apple.stackexchange.com/questions/337715/fake-ethernet-interfaces-feth-if-fake-anyone-ever-seen-this
 * https://opensource.apple.com/source/xnu/xnu-4570.41.2/bsd/net/if_fake.c.auto.html
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
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

#include "version.h"
#include "MacEthernetTapAgent.h"

#ifndef SIOCAUTOCONF_START
#define SIOCAUTOCONF_START _IOWR('i', 132, struct in6_ifreq)    /* accept rtadvd on this interface */
#endif
#ifndef SIOCAUTOCONF_STOP
#define SIOCAUTOCONF_STOP _IOWR('i', 133, struct in6_ifreq)    /* stop accepting rtadv for this interface */
#endif

#define P_IFCONFIG "/sbin/ifconfig"

static unsigned char s_pktReadBuf[131072] __attribute__ ((__aligned__(16)));
static unsigned char s_stdinReadBuf[131072] __attribute__ ((__aligned__(16)));
static char s_deviceName[IFNAMSIZ];
static char s_peerDeviceName[IFNAMSIZ];
static int s_bpffd = -1;
static int s_ndrvfd = -1;
static pid_t s_parentPid;

static void configureIpv6Parameters(const char *ifname,int performNUD,int acceptRouterAdverts)
{
	struct in6_ndireq nd;
	struct in6_ifreq ifr;

	int s = socket(AF_INET6,SOCK_DGRAM,0);
	if (s <= 0)
		return;

	memset(&nd,0,sizeof(nd));
	strncpy(nd.ifname,ifname,sizeof(nd.ifname));

	if (ioctl(s,SIOCGIFINFO_IN6,&nd)) {
		close(s);
		return;
	}

	unsigned long oldFlags = (unsigned long)nd.ndi.flags;

	if (performNUD)
		nd.ndi.flags |= ND6_IFF_PERFORMNUD;
	else nd.ndi.flags &= ~ND6_IFF_PERFORMNUD;

	if (oldFlags != (unsigned long)nd.ndi.flags) {
		if (ioctl(s,SIOCSIFINFO_FLAGS,&nd)) {
			close(s);
			return;
		}
	}

	memset(&ifr,0,sizeof(ifr));
	strncpy(ifr.ifr_name,ifname,sizeof(ifr.ifr_name));
	if (ioctl(s,acceptRouterAdverts ? SIOCAUTOCONF_START : SIOCAUTOCONF_STOP,&ifr)) {
		close(s);
		return;
	}

	close(s);
}

static int run(const char *path,...)
{
	va_list ap;
	char *args[16];
	int argNo = 1;

	va_start(ap,path);
	args[0] = (char *)path;
	for(;argNo<15;++argNo) {
		args[argNo] = va_arg(ap,char *);
		if (!args[argNo]) {
			break;
		}
	}
	args[argNo++] = (char *)0;
	va_end(ap);

	pid_t pid = vfork();
	if (pid < 0) {
		return -1;
	} else if (pid == 0) {
		dup2(STDERR_FILENO,STDOUT_FILENO);
		execv(args[0],args);
		_exit(-1);
	}
	int rv = 0;
	waitpid(pid,&rv,0);
	return rv;
}

static void die()
{
	if (s_ndrvfd >= 0)
		close(s_ndrvfd);
	if (s_bpffd >= 0)
		close(s_bpffd);
	if (s_deviceName[0])
		run("/sbin/ifconfig",s_deviceName,"destroy",(char *)0);
	if (s_peerDeviceName[0])
		run("/sbin/ifconfig",s_peerDeviceName,"destroy",(char *)0);
}

int main(int argc,char **argv)
{
	struct ifreq ifr;
	u_int fl;
	fd_set rfds,wfds,efds;
	struct iovec iov[2];

	s_deviceName[0] = 0;
	s_peerDeviceName[0] = 0;
	s_parentPid = getppid();

	atexit(&die);
	signal(SIGIO,SIG_IGN);
	signal(SIGCHLD,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGALRM,SIG_IGN);
	signal(SIGQUIT,&exit);
	signal(SIGTERM,&exit);
	signal(SIGKILL,&exit);
	signal(SIGINT,&exit);
	signal(SIGPIPE,&exit);

	if (getuid() != 0) {
		if (setuid(0) != 0) {
			fprintf(stderr,"E must be run as root or with root setuid bit on executable\n");
			return ZT_MACETHERNETTAPAGENT_EXIT_CODE_INVALID_REQUEST;
		}
	}

	if (argc < 5) {
		fprintf(stderr,"E invalid or missing argument(s) (usage: MacEthernetTapAgent <0-4999> <mac> <mtu> <metric>)\n");
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_INVALID_REQUEST;
	}
	const int deviceNo = atoi(argv[1]);
	if ((deviceNo < 0)||(deviceNo > 4999)) {
		fprintf(stderr,"E invalid or missing argument(s) (usage: MacEthernetTapAgent <0-4999> <mac> <mtu> <metric>)\n");
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_INVALID_REQUEST;
	}
	const char *mac = argv[2];
	const char *mtu = argv[3];
	const char *metric = argv[4];

	s_ndrvfd = socket(AF_NDRV,SOCK_RAW,0);
	if (s_ndrvfd < 0) {
		fprintf(stderr,"E unable to open AF_NDRV socket\n");
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}

	snprintf(s_deviceName,sizeof(s_deviceName),"feth%d",deviceNo);
	snprintf(s_peerDeviceName,sizeof(s_peerDeviceName),"feth%d",deviceNo+5000);
	if (run(P_IFCONFIG,s_peerDeviceName,"create",(char *)0) != 0) {
		fprintf(stderr,"E unable to create %s\n",s_deviceName);
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}
	usleep(10);
	if (run(P_IFCONFIG,s_deviceName,"create",(char *)0) != 0) {
		fprintf(stderr,"E unable to create %s\n",s_deviceName);
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}
	run(P_IFCONFIG,s_deviceName,"lladdr",mac,(char *)0);
	usleep(10);
	run(P_IFCONFIG,s_peerDeviceName,"peer",s_deviceName,(char *)0);
	usleep(10);
	run(P_IFCONFIG,s_peerDeviceName,"mtu","16370","up",(char *)0); /* 16370 is the largest MTU MacOS/Darwin seems to allow */
	usleep(10);
	run(P_IFCONFIG,s_deviceName,"mtu",mtu,"metric",metric,"up",(char *)0);
	usleep(10);
	configureIpv6Parameters(s_deviceName,1,0);
	usleep(10);

	struct sockaddr_ndrv nd;
	nd.snd_len = sizeof(struct sockaddr_ndrv);
	nd.snd_family = AF_NDRV;
	memcpy(nd.snd_name,s_peerDeviceName,sizeof(nd.snd_name));
	if (bind(s_ndrvfd,(struct sockaddr *)&nd,sizeof(nd)) != 0) {
		fprintf(stderr,"E unable to bind AF_NDRV socket\n");
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}
	if (connect(s_ndrvfd,(struct sockaddr *)&nd,sizeof(nd)) != 0) {
		fprintf(stderr,"E unable to connect AF_NDRV socket\n");
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}

	/* Start at /dev/bpf1 since some simple bpf-using net utilities hard-code /dev/bpf0.
	 * Things like libpcap are smart enough to search. */
	for(int bpfno=1;bpfno<5000;++bpfno) {
		char tmp[32];
		snprintf(tmp,sizeof(tmp),"/dev/bpf%d",bpfno);
		s_bpffd = open(tmp,O_RDWR);
		if (s_bpffd >= 0) {
			break;
		}
	}
	if (s_bpffd < 0) {
		fprintf(stderr,"E unable to open bpf device\n");
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}

	fl = sizeof(s_pktReadBuf);
	if (ioctl(s_bpffd,BIOCSBLEN,&fl) != 0) {
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}
	const size_t readPktSize = (size_t)fl;
	fl = 1;
	if (ioctl(s_bpffd,BIOCIMMEDIATE,&fl) != 0) {
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}
	fl = 0;
	if (ioctl(s_bpffd,BIOCSSEESENT,&fl) != 0) {
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}
	memset(&ifr,0,sizeof(ifr));
	memcpy(ifr.ifr_name,s_peerDeviceName,IFNAMSIZ);
	if (ioctl(s_bpffd,BIOCSETIF,&ifr) != 0) {
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}
	fl = 1;
	if (ioctl(s_bpffd,BIOCSHDRCMPLT,&fl) != 0) {
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}
	fl = 1;
	if (ioctl(s_bpffd,BIOCPROMISC,&fl) != 0) {
		return ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE;
	}

	fprintf(stderr,"I %s %s %d.%d.%d.%d\n",s_deviceName,s_peerDeviceName,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION,ZEROTIER_ONE_VERSION_BUILD);

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	long stdinReadPtr = 0;
	for(;;) {
		FD_SET(STDIN_FILENO,&rfds);
		FD_SET(s_bpffd,&rfds);
		if (select(s_bpffd+1,&rfds,&wfds,&efds,(struct timeval *)0) < 0) {
			if ((errno == EAGAIN)||(errno == EINTR)) {
				usleep(10);
				continue;
			}
			return ZT_MACETHERNETTAPAGENT_EXIT_CODE_READ_ERROR;
		}

		if (FD_ISSET(s_bpffd,&rfds)) {
			long n = (long)read(s_bpffd,s_pktReadBuf,readPktSize);
			if (n > 0) {
				for(unsigned char *p=s_pktReadBuf,*eof=p+n;p<eof;) {
					struct bpf_hdr *h = (struct bpf_hdr *)p;
					if ((h->bh_caplen > 0)&&((p + h->bh_hdrlen + h->bh_caplen) <= eof)) {
						uint16_t len = (uint16_t)h->bh_caplen;
						iov[0].iov_base = &len;
						iov[0].iov_len = 2;
						iov[1].iov_base = p + h->bh_hdrlen;
						iov[1].iov_len = h->bh_caplen;
						writev(STDOUT_FILENO,iov,2);
					}
					p += BPF_WORDALIGN(h->bh_hdrlen + h->bh_caplen);
				}
			}
		}

		if (FD_ISSET(STDIN_FILENO,&rfds)) {
			long n = (long)read(STDIN_FILENO,s_stdinReadBuf + stdinReadPtr,sizeof(s_stdinReadBuf) - stdinReadPtr);
			if (n > 0) {
				stdinReadPtr += n;
				while (stdinReadPtr >= 2) {
					long len = *((uint16_t *)s_stdinReadBuf);
					if (stdinReadPtr >= (len + 2)) {
						if (len > 0) {
							unsigned char *msg = s_stdinReadBuf + 2;

							switch(msg[0]) {
								case ZT_MACETHERNETTAPAGENT_STDIN_CMD_PACKET:
									if (len > 1) {
										if (write(s_ndrvfd,msg+1,len-1) < 0) {
											fprintf(stderr,"E inject failed size==%ld errno==%d\n",len-1,errno);
										}
									}
									break;

								case ZT_MACETHERNETTAPAGENT_STDIN_CMD_IFCONFIG: {
									char *args[16];
									args[0] = P_IFCONFIG;
									args[1] = s_deviceName;
									int argNo = 2;
									for(int argPtr=0,k=1,l=(int)len;k<l;++k) {
										if (!msg[k]) {
											if (argPtr > 0) {
												argPtr = 0;
												++argNo;
												if (argNo >= 15) {
													break;
												}
											}
										} else {
											if (argPtr == 0) {
												args[argNo] = (char *)(msg + k);
											}
											argPtr++;
										}
									}
									args[argNo] = (char *)0;
									if (argNo > 2) {
										pid_t pid = fork();
										if (pid < 0) {
											return -1;
										} else if (pid == 0) {
											dup2(STDERR_FILENO,STDOUT_FILENO);
											execv(args[0],args);
											_exit(-1);
										}
										int rv = 0;
										waitpid(pid,&rv,0);
									}
								}	break;

								case ZT_MACETHERNETTAPAGENT_STDIN_CMD_EXIT:
									return ZT_MACETHERNETTAPAGENT_EXIT_CODE_SUCCESS;

								default:
									fprintf(stderr,"E unrecognized message type over pipe from host process: %d (length: %d)\n",(int)msg[0],(int)len);
									break;
							}
						}

						if (stdinReadPtr > (len + 2)) {
							memmove(s_stdinReadBuf,s_stdinReadBuf + len + 2,stdinReadPtr -= (len + 2));
						} else {
							stdinReadPtr = 0;
						}
					} else {
						break;
					}
				}
			}
		}
	}

	return ZT_MACETHERNETTAPAGENT_EXIT_CODE_SUCCESS;
}
