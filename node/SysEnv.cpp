/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

#include <set>
#include <string>

#include "Constants.hpp"
#include "SysEnv.hpp"
#include "Utils.hpp"
#include "RuntimeEnvironment.hpp"
#include "NodeConfig.hpp"

#ifdef __UNIX_LIKE__
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <net/route.h>
#endif

#ifdef __WINDOWS__
#include <Windows.h>
#include <WinSock2.h>
#endif

namespace ZeroTier {

SysEnv::SysEnv()
{
}

SysEnv::~SysEnv()
{
}

#ifdef __APPLE__

uint64_t SysEnv::getNetworkConfigurationFingerprint(const std::set<std::string> &ignoreDevices)
{
	int mib[6];
	size_t needed;
	uint64_t fingerprint = 5381; // djb2 hash algorithm is used below

	// Right now this just scans for changes in default routes. This is not
	// totally robust -- it will miss cases where we switch from one 10.0.0.0/24
	// network with gateway .1 to another -- but most of the time it'll pick
	// up shifts in connectivity. Combined with sleep/wake detection this seems
	// pretty solid so far on Mac for detecting when you change locations.

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_UNSPEC;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	if (!sysctl(mib,6,NULL,&needed,NULL,0)) {
		char *buf = (char *)malloc(needed);
		if (buf) {
			if (!sysctl(mib,6,buf,&needed,NULL,0)) {
		    struct rt_msghdr *rtm;
				for(char *next=buf,*end=buf+needed;next<end;) {
					rtm = (struct rt_msghdr *)next;
					char *saptr = (char *)(rtm + 1);
					char *saend = next + rtm->rtm_msglen;
					if (((rtm->rtm_addrs & RTA_DST))&&((rtm->rtm_addrs & RTA_GATEWAY))) {
						int sano = 0;
						struct sockaddr *dst = (struct sockaddr *)0;
						struct sockaddr *gateway = (struct sockaddr *)0;
						while (saptr < saend) {
							struct sockaddr *sa = (struct sockaddr *)saptr;
							if (!sa->sa_len)
								break;
							if (sano == 0)
								dst = sa;
							else if (sano == 1)
								gateway = sa;
							else if (sano > 1)
								break;
							++sano;
							saptr += sa->sa_len;
						}
						if ((dst)&&(gateway)) {
							if ((dst->sa_family == AF_INET)&&(gateway->sa_family == AF_INET)&&(!((struct sockaddr_in *)dst)->sin_addr.s_addr)) {
								fingerprint = ((fingerprint << 5) + fingerprint) + (uint64_t)((struct sockaddr_in *)gateway)->sin_addr.s_addr;
							} else if ((dst->sa_family == AF_INET6)&&(gateway->sa_family == AF_INET6)&&(Utils::isZero(((struct sockaddr_in6 *)dst)->sin6_addr.s6_addr,16))) {
								for(unsigned int i=0;i<16;++i)
									fingerprint = ((fingerprint << 5) + fingerprint) + (uint64_t)((struct sockaddr_in6 *)gateway)->sin6_addr.s6_addr[i];
							}
						}
					}
					next = saend;
				}
			}
			free(buf);
		}
	}

	return fingerprint;
}

#endif // __APPLE__

#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)

uint64_t SysEnv::getNetworkConfigurationFingerprint(const std::set<std::string> &ignoreDevices)
{
	char buf[16384];
	uint64_t fingerprint = 5381; // djb2 hash algorithm is used below
	char *t1,*t2;

	try {
		// Include default IPv4 route if available
		int fd = open("/proc/net/route",O_RDONLY);
		if (fd > 0) {
			long n = read(fd,buf,sizeof(buf) - 1);
			::close(fd);
			if (n > 0) {
				buf[n] = 0;
				for(char *line=strtok_r(buf,"\r\n",&t1);(line);line=strtok_r((char *)0,"\r\n",&t1)) {
					int fno = 0;
					for(char *field=strtok_r(line," \t",&t2);(field);field=strtok_r((char *)0," \t",&t2)) {
						if (fno == 0) { // device name
							if ((ignoreDevices.count(std::string(field)))||(!strcmp(field,"lo")))
								break;
						} else if ((fno == 1)||(fno == 2)) { // destination, gateway
							if (strlen(field) == 8) { // ignore header junk, use only hex route info
								while (*field)
									fingerprint = ((fingerprint << 5) + fingerprint) + (uint64_t)*(field++);
							}
						} else if (fno > 2)
							break;
						++fno;
					}
				}
			}
		}

		// Include IPs of IPv6 enabled interfaces if available
		fd = open("/proc/net/if_inet6",O_RDONLY);
		if (fd > 0) {
			long n = read(fd,buf,sizeof(buf) - 1);
			::close(fd);
			if (n > 0) {
				buf[n] = 0;
				for(char *line=strtok_r(buf,"\r\n",&t1);(line);line=strtok_r((char *)0,"\r\n",&t1)) {
					int fno = 0;
					const char *v6ip = (const char *)0;
					const char *devname = (const char *)0;
					for(char *field=strtok_r(line," \t",&t2);(field);field=strtok_r((char *)0," \t",&t2)) {
						switch(fno) {
							case 0:
								v6ip = field;
								break;
							case 5:
								devname = field;
								break;
						}
						++fno;
					}

					if ((v6ip)&&(devname)) {
						if ((!(ignoreDevices.count(std::string(devname))))&&(strcmp(devname,"lo"))) {
							while (*v6ip)
								fingerprint = ((fingerprint << 5) + fingerprint) + (uint64_t)*(v6ip++);
						}
					}
				}
			}
		}
	} catch ( ... ) {}

	return fingerprint;
}

#endif // __linux__

#ifdef __WINDOWS__

uint64_t SysEnv::getNetworkConfigurationFingerprint(const std::set<std::string> &ignoreDevices)
{
	// TODO: windows version
	return 1;
}

#endif // __WINDOWS__

} // namespace ZeroTier
