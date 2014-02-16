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

#include <string>
#include <map>
#include <set>
#include <algorithm>

#include "Constants.hpp"
#include "EthernetTap.hpp"
#include "Logger.hpp"
#include "RuntimeEnvironment.hpp"
#include "Utils.hpp"
#include "Mutex.hpp"
#include "Utils.hpp"

// ff:ff:ff:ff:ff:ff with no ADI
static const ZeroTier::MulticastGroup _blindWildcardMulticastGroup(ZeroTier::MAC(0xff),0);

//
// TAP implementation for *nix OSes, with some specialization for different flavors
//

#ifdef __UNIX_LIKE__ /////////////////////////////////////////////////////////

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

// Command identifiers used with command finder static (on various *nixes)
#define ZT_UNIX_IP_COMMAND 1
#define ZT_UNIX_IFCONFIG_COMMAND 2
#define ZT_MAC_KEXTLOAD_COMMAND 3
#define ZT_MAC_KEXTUNLOAD_COMMAND 4

// Finds external commands on startup
class _CommandFinder
{
public:
	_CommandFinder()
	{
		_findCmd(ZT_UNIX_IFCONFIG_COMMAND,"ifconfig");
#ifdef __LINUX__
		_findCmd(ZT_UNIX_IP_COMMAND,"ip");
#endif
#ifdef __APPLE__
		_findCmd(ZT_MAC_KEXTLOAD_COMMAND,"kextload");
		_findCmd(ZT_MAC_KEXTUNLOAD_COMMAND,"kextunload");
#endif
	}

	// returns NULL if command was not found
	inline const char *operator[](int id) const
		throw()
	{
		std::map<int,std::string>::const_iterator c(_paths.find(id));
		if (c == _paths.end())
			return (const char *)0;
		return c->second.c_str();
	}

private:
	inline void _findCmd(int id,const char *name)
	{
		char tmp[4096];
		ZeroTier::Utils::snprintf(tmp,sizeof(tmp),"/sbin/%s",name);
		if (ZeroTier::Utils::fileExists(tmp)) {
			_paths[id] = tmp;
			return;
		}
		ZeroTier::Utils::snprintf(tmp,sizeof(tmp),"/usr/sbin/%s",name);
		if (ZeroTier::Utils::fileExists(tmp)) {
			_paths[id] = tmp;
			return;
		}
		ZeroTier::Utils::snprintf(tmp,sizeof(tmp),"/bin/%s",name);
		if (ZeroTier::Utils::fileExists(tmp)) {
			_paths[id] = tmp;
			return;
		}
		ZeroTier::Utils::snprintf(tmp,sizeof(tmp),"/usr/bin/%s",name);
		if (ZeroTier::Utils::fileExists(tmp)) {
			_paths[id] = tmp;
			return;
		}
	}
	std::map<int,std::string> _paths;
};
static const _CommandFinder UNIX_COMMANDS;

#ifdef __LINUX__
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_addr.h>
#include <linux/if_ether.h>
#include <ifaddrs.h>
#endif // __LINUX__

#ifdef __APPLE__

#include <sys/cdefs.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
struct prf_ra { // stupid OSX compile fix... in6_var defines this in a struct which namespaces it for C++
	u_char onlink : 1;
	u_char autonomous : 1;
	u_char reserved : 6;
} prf_ra;
#include <netinet6/in6_var.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <ifaddrs.h>

// These are KERNEL_PRIVATE... why?
#ifndef SIOCAUTOCONF_START
#define SIOCAUTOCONF_START _IOWR('i', 132, struct in6_ifreq)    /* accept rtadvd on this interface */
#endif
#ifndef SIOCAUTOCONF_STOP
#define SIOCAUTOCONF_STOP _IOWR('i', 133, struct in6_ifreq)    /* stop accepting rtadv for this interface */
#endif

static volatile int EthernetTap_instances = 0;
static ZeroTier::Mutex EthernetTap_instances_m;

static inline bool _setIpv6Stuff(const char *ifname,bool performNUD,bool acceptRouterAdverts)
{
	struct in6_ndireq nd;
	struct in6_ifreq ifr;

	int s = socket(AF_INET6,SOCK_DGRAM,0);
	if (s <= 0)
		return false;

	memset(&nd,0,sizeof(nd));
	strncpy(nd.ifname,ifname,sizeof(nd.ifname));

	if (ioctl(s,SIOCGIFINFO_IN6,&nd)) {
		close(s);
		return false;
	}

	unsigned long oldFlags = (unsigned long)nd.ndi.flags;

	if (performNUD)
		nd.ndi.flags |= ND6_IFF_PERFORMNUD;
	else nd.ndi.flags &= ~ND6_IFF_PERFORMNUD;

	if (oldFlags != (unsigned long)nd.ndi.flags) {
		if (ioctl(s,SIOCSIFINFO_FLAGS,&nd)) {
			close(s);
			return false;
		}
	}

	memset(&ifr,0,sizeof(ifr));
	strncpy(ifr.ifr_name,ifname,sizeof(ifr.ifr_name));
	if (ioctl(s,acceptRouterAdverts ? SIOCAUTOCONF_START : SIOCAUTOCONF_STOP,&ifr)) {
		close(s);
		return false;
	}

	close(s);
	return true;
}

#endif // __APPLE__

namespace ZeroTier {

// Only permit one tap to be opened concurrently across the entire process
static Mutex __tapCreateLock;

#ifdef __LINUX__
EthernetTap::EthernetTap(
	const RuntimeEnvironment *renv,
	const char *tag,
	const MAC &mac,
	unsigned int mtu,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
	throw(std::runtime_error) :
	_mac(mac),
	_mtu(mtu),
	_r(renv),
	_handler(handler),
	_arg(arg),
	_fd(0)
{
	char procpath[128];
	struct stat sbuf;
	Mutex::Lock _l(__tapCreateLock); // create only one tap at a time, globally

	if (mtu > 4096)
		throw std::runtime_error("max tap MTU is 4096");

	_fd = ::open("/dev/net/tun",O_RDWR);
	if (_fd <= 0)
		throw std::runtime_error(std::string("could not open TUN/TAP device: ") + strerror(errno));

	struct ifreq ifr;
	memset(&ifr,0,sizeof(ifr));

	// Try to recall our last device name, or pick an unused one if that fails.
	bool recalledDevice = false;
	if ((tag)&&(tag[0])) {
		Utils::scopy(ifr.ifr_name,sizeof(ifr.ifr_name),tag);
		Utils::snprintf(procpath,sizeof(procpath),"/proc/sys/net/ipv4/conf/%s",ifr.ifr_name);
		recalledDevice = (stat(procpath,&sbuf) != 0);
	}
	if (!recalledDevice) {
		int devno = 0;
		do {
			Utils::snprintf(ifr.ifr_name,sizeof(ifr.ifr_name),"zt%d",devno++);
			Utils::snprintf(procpath,sizeof(procpath),"/proc/sys/net/ipv4/conf/%s",ifr.ifr_name);
		} while (stat(procpath,&sbuf) == 0); // try zt#++ until we find one that does not exist
	}

	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	if (ioctl(_fd,TUNSETIFF,(void *)&ifr) < 0) {
		::close(_fd);
		throw std::runtime_error("unable to configure TUN/TAP device for TAP operation");
	}

	strcpy(_dev,ifr.ifr_name);

	ioctl(_fd,TUNSETPERSIST,0); // valgrind may generate a false alarm here

	// Open an arbitrary socket to talk to netlink
	int sock = socket(AF_INET,SOCK_DGRAM,0);
	if (sock <= 0) {
		::close(_fd);
		throw std::runtime_error("unable to open netlink socket");
	}

	// Set MAC address
	ifr.ifr_ifru.ifru_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(ifr.ifr_ifru.ifru_hwaddr.sa_data,mac.data,6);
	if (ioctl(sock,SIOCSIFHWADDR,(void *)&ifr) < 0) {
		::close(_fd);
		::close(sock);
		throw std::runtime_error("unable to configure TAP hardware (MAC) address");
		return;
	}

	// Set MTU
	ifr.ifr_ifru.ifru_mtu = (int)mtu;
	if (ioctl(sock,SIOCSIFMTU,(void *)&ifr) < 0) {
		::close(_fd);
		::close(sock);
		throw std::runtime_error("unable to configure TAP MTU");
	}

	if (fcntl(_fd,F_SETFL,fcntl(_fd,F_GETFL) & ~O_NONBLOCK) == -1) {
		::close(_fd);
		throw std::runtime_error("unable to set flags on file descriptor for TAP device");
	}

	/* Bring interface up */
	if (ioctl(sock,SIOCGIFFLAGS,(void *)&ifr) < 0) {
		::close(_fd);
		::close(sock);
		throw std::runtime_error("unable to get TAP interface flags");
	}
	ifr.ifr_flags |= IFF_UP;
	if (ioctl(sock,SIOCSIFFLAGS,(void *)&ifr) < 0) {
		::close(_fd);
		::close(sock);
		throw std::runtime_error("unable to set TAP interface flags");
	}

	::close(sock);

	::pipe(_shutdownSignalPipe);

	TRACE("tap %s created",_dev);

	_thread = Thread::start(this);
}
#endif // __LINUX__

#ifdef __APPLE__
EthernetTap::EthernetTap(
	const RuntimeEnvironment *renv,
	const char *tag,
	const MAC &mac,
	unsigned int mtu,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
	throw(std::runtime_error) :
	_mac(mac),
	_mtu(mtu),
	_r(renv),
	_handler(handler),
	_arg(arg),
	_fd(0)
{
	char devpath[64],ethaddr[64],mtustr[16],tmp[4096];
	struct stat stattmp;
	Mutex::Lock _l(__tapCreateLock); // create only one tap at a time, globally

	if (mtu > 4096)
		throw std::runtime_error("max tap MTU is 4096");

	// Check for existence of ZT tap devices, try to load module if not there
	const char *kextload = UNIX_COMMANDS[ZT_MAC_KEXTLOAD_COMMAND];
	if ((stat("/dev/zt0",&stattmp))&&(kextload)) {
		strcpy(tmp,_r->homePath.c_str());
		long kextpid = (long)vfork();
		if (kextpid == 0) {
			chdir(tmp);
			execl(kextload,kextload,"-q","-repository",tmp,"tap.kext",(const char *)0);
			_exit(-1);
		} else if (kextpid > 0) {
			int exitcode = -1;
			waitpid(kextpid,&exitcode,0);
			usleep(500);
		} else throw std::runtime_error("unable to create subprocess with fork()");
	}
	if (stat("/dev/zt0",&stattmp))
		throw std::runtime_error("/dev/zt# tap devices do not exist and unable to load kernel extension");

	// Try to reopen the last device we had, if we had one and it's still unused.
	bool recalledDevice = false;
	if ((tag)&&(tag[0])) {
		Utils::snprintf(devpath,sizeof(devpath),"/dev/%s",tag);
		if (stat(devpath,&stattmp) == 0) {
			_fd = ::open(devpath,O_RDWR);
			if (_fd > 0) {
				Utils::scopy(_dev,sizeof(_dev),tag);
				recalledDevice = true;
			}
		}
	}

	// Open the first unused tap device if we didn't recall a previous one.
	if (!recalledDevice) {
		for(int i=0;i<256;++i) {
			Utils::snprintf(devpath,sizeof(devpath),"/dev/zt%d",i);
			if (stat(devpath,&stattmp))
				throw std::runtime_error("no more TAP devices available");
			_fd = ::open(devpath,O_RDWR);
			if (_fd > 0) {
				Utils::snprintf(_dev,sizeof(_dev),"zt%d",i);
				break;
			}
		}
	}

	if (_fd <= 0)
		throw std::runtime_error("unable to open TAP device or no more devices available");

	if (fcntl(_fd,F_SETFL,fcntl(_fd,F_GETFL) & ~O_NONBLOCK) == -1) {
		::close(_fd);
		throw std::runtime_error("unable to set flags on file descriptor for TAP device");
	}

	const char *ifconfig = UNIX_COMMANDS[ZT_UNIX_IFCONFIG_COMMAND];
	if (!ifconfig) {
		::close(_fd);
		throw std::runtime_error("unable to find 'ifconfig' command on system");
	}

	// Configure MAC address and MTU, bring interface up
	Utils::snprintf(ethaddr,sizeof(ethaddr),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(int)mac[0],(int)mac[1],(int)mac[2],(int)mac[3],(int)mac[4],(int)mac[5]);
	Utils::snprintf(mtustr,sizeof(mtustr),"%u",mtu);
	long cpid;
	if ((cpid = (long)vfork()) == 0) {
		execl(ifconfig,ifconfig,_dev,"lladdr",ethaddr,"mtu",mtustr,"up",(const char *)0);
		_exit(-1);
	} else {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		if (exitcode) {
			::close(_fd);
			throw std::runtime_error("ifconfig failure setting link-layer address and activating tap interface");
		}
	}

	_setIpv6Stuff(_dev,true,false);

	::pipe(_shutdownSignalPipe);

	_thread = Thread::start(this);

	EthernetTap_instances_m.lock();
	++EthernetTap_instances;
	EthernetTap_instances_m.unlock();
}
#endif // __APPLE__

EthernetTap::~EthernetTap()
{
	::write(_shutdownSignalPipe[1],"\0",1); // causes thread to exit
	Thread::join(_thread);
	::close(_fd);

#ifdef __APPLE__
	EthernetTap_instances_m.lock();
	int instances = --EthernetTap_instances;
	EthernetTap_instances_m.unlock();
	if (instances <= 0) {
		// Unload OSX kernel extension on the deletion of the last EthernetTap
		// instance.
		const char *kextunload = UNIX_COMMANDS[ZT_MAC_KEXTUNLOAD_COMMAND];
		if (kextunload) {
			char tmp[4096];
			sprintf(tmp,"%s/tap.kext",_r->homePath.c_str());
			long kextpid = (long)vfork();
			if (kextpid == 0) {
				execl(kextunload,kextunload,tmp,(const char *)0);
				_exit(-1);
			} else if (kextpid > 0) {
				int exitcode = -1;
				waitpid(kextpid,&exitcode,0);
			}
		}
	}
#endif // __APPLE__
}

void EthernetTap::setDisplayName(const char *dn)
{
}

#ifdef __LINUX__
static bool ___removeIp(const char *_dev,const InetAddress &ip)
{
	const char *ipcmd = UNIX_COMMANDS[ZT_UNIX_IP_COMMAND];
	if (!ipcmd)
		return false;
	long cpid = (long)vfork();
	if (cpid == 0) {
		execl(ipcmd,ipcmd,"addr","del",ip.toString().c_str(),"dev",_dev,(const char *)0);
		_exit(-1);
	} else {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		return (exitcode == 0);
	}
}

bool EthernetTap::addIP(const InetAddress &ip)
{
	const char *ipcmd = UNIX_COMMANDS[ZT_UNIX_IP_COMMAND];
	if (!ipcmd) {
		LOG("ERROR: could not configure IP address for %s: unable to find 'ip' command on system (checked /sbin, /bin, /usr/sbin, /usr/bin)",_dev);
		return false;
	}

	if (!ip)
		return false;

	std::set<InetAddress> allIps(ips());
	if (allIps.count(ip) > 0)
		return true;

	// Remove and reconfigure if address is the same but netmask is different
	for(std::set<InetAddress>::iterator i(allIps.begin());i!=allIps.end();++i) {
		if (i->ipsEqual(ip)) {
			if (___removeIp(_dev,*i)) {
				break;
			} else {
				LOG("WARNING: failed to remove old IP/netmask %s to replace with %s",i->toString().c_str(),ip.toString().c_str());
			}
		}
	}

	long cpid;
	if ((cpid = (long)vfork()) == 0) {
		execl(ipcmd,ipcmd,"addr","add",ip.toString().c_str(),"dev",_dev,(const char *)0);
		_exit(-1);
	} else if (cpid > 0) {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		return (exitcode == 0);
	}

	return false;
}
#endif // __LINUX__

#ifdef __APPLE__
static bool ___removeIp(const char *_dev,const InetAddress &ip)
{
	const char *ifconfig = UNIX_COMMANDS[ZT_UNIX_IFCONFIG_COMMAND];
	if (!ifconfig)
		return false;
	long cpid;
	if ((cpid = (long)vfork()) == 0) {
		execl(ifconfig,ifconfig,_dev,"inet",ip.toIpString().c_str(),"-alias",(const char *)0);
		_exit(-1);
	} else {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		return (exitcode == 0);
	}
	return false; // never reached, make compiler shut up about return value
}

bool EthernetTap::addIP(const InetAddress &ip)
{
	const char *ifconfig = UNIX_COMMANDS[ZT_UNIX_IFCONFIG_COMMAND];
	if (!ifconfig) {
		LOG("ERROR: could not configure IP address for %s: unable to find 'ifconfig' command on system (checked /sbin, /bin, /usr/sbin, /usr/bin)",_dev);
		return false;
	}

	if (!ip)
		return false;

	std::set<InetAddress> allIps(ips());
	if (allIps.count(ip) > 0)
		return true; // IP/netmask already assigned

	// Remove and reconfigure if address is the same but netmask is different
	for(std::set<InetAddress>::iterator i(allIps.begin());i!=allIps.end();++i) {
		if ((i->ipsEqual(ip))&&(i->netmaskBits() != ip.netmaskBits())) {
			if (___removeIp(_dev,*i)) {
				break;
			} else {
				LOG("WARNING: failed to remove old IP/netmask %s to replace with %s",i->toString().c_str(),ip.toString().c_str());
			}
		}
	}

	long cpid;
	if ((cpid = (long)vfork()) == 0) {
		execl(ifconfig,ifconfig,_dev,ip.isV4() ? "inet" : "inet6",ip.toString().c_str(),"alias",(const char *)0);
		_exit(-1);
	} else {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		return (exitcode == 0);
	}

	return false;
}
#endif // __APPLE__

bool EthernetTap::removeIP(const InetAddress &ip)
{
	if (ips().count(ip) > 0) {
		if (___removeIp(_dev,ip))
			return true;
	}
	return false;
}

std::set<InetAddress> EthernetTap::ips() const
{
	struct ifaddrs *ifa = (struct ifaddrs *)0;
	if (getifaddrs(&ifa))
		return std::set<InetAddress>();

	std::set<InetAddress> r;

	struct ifaddrs *p = ifa;
	while (p) {
		if ((!strcmp(p->ifa_name,_dev))&&(p->ifa_addr)&&(p->ifa_netmask)&&(p->ifa_addr->sa_family == p->ifa_netmask->sa_family)) {
			switch(p->ifa_addr->sa_family) {
				case AF_INET: {
					struct sockaddr_in *sin = (struct sockaddr_in *)p->ifa_addr;
					struct sockaddr_in *nm = (struct sockaddr_in *)p->ifa_netmask;
					r.insert(InetAddress(&(sin->sin_addr.s_addr),4,Utils::countBits((uint32_t)nm->sin_addr.s_addr)));
				}	break;
				case AF_INET6: {
					struct sockaddr_in6 *sin = (struct sockaddr_in6 *)p->ifa_addr;
					struct sockaddr_in6 *nm = (struct sockaddr_in6 *)p->ifa_netmask;
					uint32_t b[4];
					memcpy(b,nm->sin6_addr.s6_addr,sizeof(b));
					r.insert(InetAddress(sin->sin6_addr.s6_addr,16,Utils::countBits(b[0]) + Utils::countBits(b[1]) + Utils::countBits(b[2]) + Utils::countBits(b[3])));
				}	break;
			}
		}
		p = p->ifa_next;
	}

	if (ifa)
		freeifaddrs(ifa);

	return r;
}

void EthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	char putBuf[4096 + 14];
	if ((_fd > 0)&&(len <= _mtu)) {
		for(int i=0;i<6;++i)
			putBuf[i] = to.data[i];
		for(int i=0;i<6;++i)
			putBuf[i+6] = from.data[i];
		*((uint16_t *)(putBuf + 12)) = htons((uint16_t)etherType);
		memcpy(putBuf + 14,data,len);
		len += 14;

		int n = ::write(_fd,putBuf,len);
		if (n <= 0) {
			LOG("error writing packet to Ethernet tap device: %s",strerror(errno));
		} else if (n != (int)len) {
			// Saw this gremlin once, so log it if we see it again... OSX tap
			// or something seems to have goofy issues with certain MTUs.
			LOG("ERROR: write underrun: %s tap write() wrote %d of %u bytes of frame",_dev,n,len);
		}
	}
}

std::string EthernetTap::deviceName() const
{
	return std::string(_dev);
}

std::string EthernetTap::persistentId() const
{
	return std::string();
}

#ifdef __LINUX__
bool EthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
{
	char *ptr,*ptr2;
	unsigned char mac[6];
	std::set<MulticastGroup> newGroups;

	int fd = ::open("/proc/net/dev_mcast",O_RDONLY);
	if (fd > 0) {
		char buf[131072];
		int n = (int)::read(fd,buf,sizeof(buf));
		if ((n > 0)&&(n < (int)sizeof(buf))) {
			buf[n] = (char)0;
			for(char *l=strtok_r(buf,"\r\n",&ptr);(l);l=strtok_r((char *)0,"\r\n",&ptr)) {
				int fno = 0;
				char *devname = (char *)0;
				char *mcastmac = (char *)0;
				for(char *f=strtok_r(l," \t",&ptr2);(f);f=strtok_r((char *)0," \t",&ptr2)) {
					if (fno == 1)
						devname = f;
					else if (fno == 4)
						mcastmac = f;
					++fno;
				}
				if ((devname)&&(!strcmp(devname,_dev))&&(mcastmac)&&(Utils::unhex(mcastmac,mac,6) == 6))
					newGroups.insert(MulticastGroup(MAC(mac),0));
			}
		}
		::close(fd);
	}

	{
		std::set<InetAddress> allIps(ips());
		for(std::set<InetAddress>::const_iterator i(allIps.begin());i!=allIps.end();++i)
			newGroups.insert(MulticastGroup::deriveMulticastGroupForAddressResolution(*i));
	}

	bool changed = false;

	newGroups.insert(_blindWildcardMulticastGroup); // always join this

	for(std::set<MulticastGroup>::iterator mg(newGroups.begin());mg!=newGroups.end();++mg) {
		if (!groups.count(*mg)) {
			groups.insert(*mg);
			changed = true;
		}
	}
	for(std::set<MulticastGroup>::iterator mg(groups.begin());mg!=groups.end();) {
		if (!newGroups.count(*mg)) {
			groups.erase(mg++);
			changed = true;
		} else ++mg;
	}

	return changed;
}
#endif // __LINUX__

#ifdef __APPLE__
// --------------------------------------------------------------------------
// This source is from:
// http://www.opensource.apple.com/source/Libinfo/Libinfo-406.17/gen.subproj/getifmaddrs.c?txt
// It's here because OSX 10.6 does not have this convenience function.

#define	SALIGN	(sizeof(uint32_t) - 1)
#define	SA_RLEN(sa)	((sa)->sa_len ? (((sa)->sa_len + SALIGN) & ~SALIGN) : \
(SALIGN + 1))
#define	MAX_SYSCTL_TRY	5
#define	RTA_MASKS	(RTA_GATEWAY | RTA_IFP | RTA_IFA)

/* FreeBSD uses NET_RT_IFMALIST and RTM_NEWMADDR from <sys/socket.h> */
/* We can use NET_RT_IFLIST2 and RTM_NEWMADDR2 on Darwin */
//#define DARWIN_COMPAT

//#ifdef DARWIN_COMPAT
#define GIM_SYSCTL_MIB NET_RT_IFLIST2
#define GIM_RTM_ADDR RTM_NEWMADDR2
//#else
//#define GIM_SYSCTL_MIB NET_RT_IFMALIST
//#define GIM_RTM_ADDR RTM_NEWMADDR
//#endif

// Not in 10.6 includes so use our own
struct _intl_ifmaddrs {
	struct _intl_ifmaddrs *ifma_next;
	struct sockaddr *ifma_name;
	struct sockaddr *ifma_addr;
	struct sockaddr *ifma_lladdr;
};

static inline int _intl_getifmaddrs(struct _intl_ifmaddrs **pif)
{
	int icnt = 1;
	int dcnt = 0;
	int ntry = 0;
	size_t len;
	size_t needed;
	int mib[6];
	int i;
	char *buf;
	char *data;
	char *next;
	char *p;
	struct ifma_msghdr2 *ifmam;
	struct _intl_ifmaddrs *ifa, *ift;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;             /* protocol */
	mib[3] = 0;             /* wildcard address family */
	mib[4] = GIM_SYSCTL_MIB;
	mib[5] = 0;             /* no flags */
	do {
		if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
			return (-1);
		if ((buf = (char *)malloc(needed)) == NULL)
			return (-1);
		if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
			if (errno != ENOMEM || ++ntry >= MAX_SYSCTL_TRY) {
				free(buf);
				return (-1);
			}
			free(buf);
			buf = NULL;
		} 
	} while (buf == NULL);

	for (next = buf; next < buf + needed; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		switch (rtm->rtm_type) {
			case GIM_RTM_ADDR:
				ifmam = (struct ifma_msghdr2 *)(void *)rtm;
				if ((ifmam->ifmam_addrs & RTA_IFA) == 0)
					break;
				icnt++;
				p = (char *)(ifmam + 1);
				for (i = 0; i < RTAX_MAX; i++) {
					if ((RTA_MASKS & ifmam->ifmam_addrs &
						 (1 << i)) == 0)
						continue;
					sa = (struct sockaddr *)(void *)p;
					len = SA_RLEN(sa);
					dcnt += len;
					p += len;
				}
				break;
		}
	}

	data = (char *)malloc(sizeof(struct _intl_ifmaddrs) * icnt + dcnt);
	if (data == NULL) {
		free(buf);
		return (-1);
	}

	ifa = (struct _intl_ifmaddrs *)(void *)data;
	data += sizeof(struct _intl_ifmaddrs) * icnt;

	memset(ifa, 0, sizeof(struct _intl_ifmaddrs) * icnt);
	ift = ifa;

	for (next = buf; next < buf + needed; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		switch (rtm->rtm_type) {
			case GIM_RTM_ADDR:
				ifmam = (struct ifma_msghdr2 *)(void *)rtm;
				if ((ifmam->ifmam_addrs & RTA_IFA) == 0)
					break;

				p = (char *)(ifmam + 1);
				for (i = 0; i < RTAX_MAX; i++) {
					if ((RTA_MASKS & ifmam->ifmam_addrs &
						 (1 << i)) == 0)
						continue;
					sa = (struct sockaddr *)(void *)p;
					len = SA_RLEN(sa);
					switch (i) {
						case RTAX_GATEWAY:
							ift->ifma_lladdr =
							(struct sockaddr *)(void *)data;
							memcpy(data, p, len);
							data += len;
							break;

						case RTAX_IFP:
							ift->ifma_name =
							(struct sockaddr *)(void *)data;
							memcpy(data, p, len);
							data += len;
							break;

						case RTAX_IFA:
							ift->ifma_addr =
							(struct sockaddr *)(void *)data;
							memcpy(data, p, len);
							data += len;
							break;

						default:
							data += len;
							break;
					}
					p += len;
				}
				ift->ifma_next = ift + 1;
				ift = ift->ifma_next;
				break;
		}
	}

	free(buf);

	if (ift > ifa) {
		ift--;
		ift->ifma_next = NULL;
		*pif = ifa;
	} else {
		*pif = NULL;
		free(ifa);
	}
	return (0);
}

static inline void _intl_freeifmaddrs(struct _intl_ifmaddrs *ifmp)
{
	free(ifmp);
}

// --------------------------------------------------------------------------

bool EthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
{
	std::set<MulticastGroup> newGroups;
	struct _intl_ifmaddrs *ifmap = (struct _intl_ifmaddrs *)0;
	if (!_intl_getifmaddrs(&ifmap)) {
		struct _intl_ifmaddrs *p = ifmap;
		while (p) {
			if (p->ifma_addr->sa_family == AF_LINK) {
				struct sockaddr_dl *in = (struct sockaddr_dl *)p->ifma_name;
				struct sockaddr_dl *la = (struct sockaddr_dl *)p->ifma_addr;
				if ((la->sdl_alen == 6)&&(in->sdl_nlen <= sizeof(_dev))&&(!memcmp(_dev,in->sdl_data,in->sdl_nlen)))
					newGroups.insert(MulticastGroup(MAC(la->sdl_data + la->sdl_nlen),0));
			}
			p = p->ifma_next;
		}
		_intl_freeifmaddrs(ifmap);
	}

	{
		std::set<InetAddress> allIps(ips());
		for(std::set<InetAddress>::const_iterator i(allIps.begin());i!=allIps.end();++i)
			newGroups.insert(MulticastGroup::deriveMulticastGroupForAddressResolution(*i));
	}

	bool changed = false;

	newGroups.insert(_blindWildcardMulticastGroup); // always join this

	for(std::set<MulticastGroup>::iterator mg(newGroups.begin());mg!=newGroups.end();++mg) {
		if (!groups.count(*mg)) {
			groups.insert(*mg);
			changed = true;
		}
	}
	for(std::set<MulticastGroup>::iterator mg(groups.begin());mg!=groups.end();) {
		if (!newGroups.count(*mg)) {
			groups.erase(mg++);
			changed = true;
		} else ++mg;
	}

	return changed;
}
#endif // __APPLE__

void EthernetTap::threadMain()
	throw()
{
	fd_set readfds,nullfds;
	MAC to,from;
	int n,nfds,r;
	char getBuf[8194];
	Buffer<4096> data;

	// Wait for a moment after startup -- wait for Network to finish
	// constructing itself.
	Thread::sleep(500);

	FD_ZERO(&readfds);
	FD_ZERO(&nullfds);
	nfds = (int)std::max(_shutdownSignalPipe[0],_fd) + 1;

	r = 0;
	for(;;) {
		FD_SET(_shutdownSignalPipe[0],&readfds);
		FD_SET(_fd,&readfds);
		select(nfds,&readfds,&nullfds,&nullfds,(struct timeval *)0);

		if (FD_ISSET(_shutdownSignalPipe[0],&readfds)) // writes to shutdown pipe terminate thread
			break;

		if (FD_ISSET(_fd,&readfds)) {
			n = (int)::read(_fd,getBuf + r,sizeof(getBuf) - r);
			if (n < 0) {
				if ((errno != EINTR)&&(errno != ETIMEDOUT)) {
					TRACE("unexpected error reading from tap: %s",strerror(errno));
					break;
				}
			} else {
				// Some tap drivers like to send the ethernet frame and the
				// payload in two chunks, so handle that by accumulating
				// data until we have at least a frame.
				r += n;
				if (r > 14) {
					if (r > ((int)_mtu + 14)) // sanity check for weird TAP behavior on some platforms
						r = _mtu + 14;
					for(int i=0;i<6;++i)
						to.data[i] = (unsigned char)getBuf[i];
					for(int i=0;i<6;++i)
						from.data[i] = (unsigned char)getBuf[i + 6];
					unsigned int etherType = ntohs(((const uint16_t *)getBuf)[6]);
					if (etherType != 0x8100) { // VLAN tagged frames are not supported!
						data.copyFrom(getBuf + 14,(unsigned int)r - 14);
						_handler(_arg,from,to,etherType,data);
					}
					r = 0;
				}
			}
		}
	}
}

bool EthernetTap::deletePersistentTapDevice(const RuntimeEnvironment *_r,const char *pid)
{
	return false;
}

} // namespace ZeroTier

#endif // __UNIX_LIKE__ //////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

#ifdef __WINDOWS__ ///////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <WinSock2.h>
#include <Windows.h>
#include <iphlpapi.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>
#include <tchar.h>
#include <winreg.h>
#include <wchar.h>
#include <nldef.h>
#include <netioapi.h>

#include "..\windows\TapDriver\tap-windows.h"

namespace ZeroTier {

// Helper function to get an adapter's LUID and index from its GUID. The LUID is
// constant but the index can change, so go ahead and just look them both up by
// the GUID which is constant. (The GUID is the instance ID in the registry.)
static inline std::pair<NET_LUID,NET_IFINDEX> _findAdapterByGuid(const GUID &guid)
	throw(std::runtime_error)
{
	MIB_IF_TABLE2 *ift = (MIB_IF_TABLE2 *)0;

	if (GetIfTable2Ex(MibIfTableRaw,&ift) != NO_ERROR)
		throw std::runtime_error("GetIfTable2Ex() failed");

	for(ULONG i=0;i<ift->NumEntries;++i) {
		if (ift->Table[i].InterfaceGuid == guid) {
			std::pair<NET_LUID,NET_IFINDEX> tmp(ift->Table[i].InterfaceLuid,ift->Table[i].InterfaceIndex);
			FreeMibTable(ift);
			return tmp;
		}
	}

	FreeMibTable(&ift);

	throw std::runtime_error("interface not found");
}

static Mutex _systemTapInitLock;

EthernetTap::EthernetTap(
	const RuntimeEnvironment *renv,
	const char *tag,
	const MAC &mac,
	unsigned int mtu,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
	throw(std::runtime_error) :
	_mac(mac),
	_mtu(mtu),
	_r(renv),
	_handler(handler),
	_arg(arg),
	_tap(INVALID_HANDLE_VALUE),
	_injectSemaphore(INVALID_HANDLE_VALUE),
	_run(true),
	_initialized(false)
{
	char subkeyName[4096];
	char subkeyClass[4096];
	char data[4096];

	if (mtu > ZT_IF_MTU)
		throw std::runtime_error("MTU too large for Windows tap");

#ifdef _WIN64
	BOOL is64Bit = TRUE;
	const char *devcon = "\\devcon_x64.exe";
	const char *tapDriver = "\\tap-windows\\x64\\zttap200.inf";
#else
	BOOL is64Bit = FALSE;
	IsWow64Process(GetCurrentProcess(),&is64Bit);
	const char *devcon = ((is64Bit == TRUE) ? "\\devcon_x64.exe" : "\\devcon_x86.exe");
	const char *tapDriver = ((is64Bit == TRUE) ? "\\tap-windows\\x64\\zttap200.inf" : "\\tap-windows\\x86\\zttap200.inf");
#endif

	Mutex::Lock _l(_systemTapInitLock); // only init one tap at a time, process-wide

	HKEY nwAdapters;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",0,KEY_READ|KEY_WRITE,&nwAdapters) != ERROR_SUCCESS)
		throw std::runtime_error("unable to open registry key for network adapter enumeration");

	std::set<std::string> existingDeviceInstances;
	std::string mySubkeyName;

	// Enumerate tap instances and look for one tagged with this tag
	for(DWORD subkeyIndex=0;subkeyIndex!=-1;) {
		DWORD type;
		DWORD dataLen;
		DWORD subkeyNameLen = sizeof(subkeyName);
		DWORD subkeyClassLen = sizeof(subkeyClass);
		FILETIME lastWriteTime;
		switch (RegEnumKeyExA(nwAdapters,subkeyIndex++,subkeyName,&subkeyNameLen,(DWORD *)0,subkeyClass,&subkeyClassLen,&lastWriteTime)) {
			case ERROR_NO_MORE_ITEMS: subkeyIndex = -1; break;
			case ERROR_SUCCESS:
				type = 0;
				dataLen = sizeof(data);
				if (RegGetValueA(nwAdapters,subkeyName,"ComponentId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
					data[dataLen] = '\0';
					if (!strnicmp(data,"zttap",5)) {
						std::string instanceId;
						type = 0;
						dataLen = sizeof(data);
						if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
							instanceId.assign(data,dataLen);
							existingDeviceInstances.insert(instanceId);
						}

						std::string instanceIdPath;
						type = 0;
						dataLen = sizeof(data);
						if (RegGetValueA(nwAdapters,subkeyName,"DeviceInstanceID",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS)
							instanceIdPath.assign(data,dataLen);

						if ((_myDeviceInstanceId.length() == 0)&&(instanceId.length() != 0)&&(instanceIdPath.length() != 0)) {
							type = 0;
							dataLen = sizeof(data);
							if (RegGetValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
								data[dataLen] = '\0';
								if (!strcmp(data,tag)) {
									_myDeviceInstanceId = instanceId;
									_myDeviceInstanceIdPath = instanceIdPath;
									mySubkeyName = subkeyName;
									subkeyIndex = -1; // break outer loop
								}
							}
						}
					}
				}
				break;
		}
	}

	// If there is no device, try to create one
	if (_myDeviceInstanceId.length() == 0) {
		// Execute devcon to install an instance of the Microsoft Loopback Adapter
		STARTUPINFOA startupInfo;
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo;
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + devcon + "\" install \"" + _r->homePath + tapDriver + "\" zttap200").c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			RegCloseKey(nwAdapters);
			throw std::runtime_error(std::string("unable to find or execute devcon at ")+devcon);
		}
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
 
		// Scan for the new instance by simply looking for taps that weren't
		// there originally.
		for(DWORD subkeyIndex=0;subkeyIndex!=-1;) {
			DWORD type;
			DWORD dataLen;
			DWORD subkeyNameLen = sizeof(subkeyName);
			DWORD subkeyClassLen = sizeof(subkeyClass);
			FILETIME lastWriteTime;
			switch (RegEnumKeyExA(nwAdapters,subkeyIndex++,subkeyName,&subkeyNameLen,(DWORD *)0,subkeyClass,&subkeyClassLen,&lastWriteTime)) {
				case ERROR_NO_MORE_ITEMS: subkeyIndex = -1; break;
				case ERROR_SUCCESS:
					type = 0;
					dataLen = sizeof(data);
					if (RegGetValueA(nwAdapters,subkeyName,"ComponentId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
						data[dataLen] = '\0';
						if (!strnicmp(data,"zttap",5)) {
							type = 0;
							dataLen = sizeof(data);
							if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
								if (existingDeviceInstances.count(std::string(data,dataLen)) == 0) {
									RegSetKeyValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",REG_SZ,tag,(DWORD)(strlen(tag)+1));
									_myDeviceInstanceId.assign(data,dataLen);
									type = 0;
									dataLen = sizeof(data);
									if (RegGetValueA(nwAdapters,subkeyName,"DeviceInstanceID",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS)
										_myDeviceInstanceIdPath.assign(data,dataLen);
									mySubkeyName = subkeyName;

									// Disable DHCP by default on newly created devices
									HKEY tcpIpInterfaces;
									if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
										DWORD enable = 0;
										RegSetKeyValueA(tcpIpInterfaces,_myDeviceInstanceId.c_str(),"EnableDHCP",REG_DWORD,&enable,sizeof(enable));
										RegCloseKey(tcpIpInterfaces);
									}

									subkeyIndex = -1; // break outer loop
								}
							}
						}
					}
					break;
			}
		}
	}

	// If we have a device, configure it
	if (_myDeviceInstanceId.length() > 0) {
		char tmps[4096];
		unsigned int tmpsl = Utils::snprintf(tmps,sizeof(tmps),"%.2X-%.2X-%.2X-%.2X-%.2X-%.2X",(unsigned int)mac.data[0],(unsigned int)mac.data[1],(unsigned int)mac.data[2],(unsigned int)mac.data[3],(unsigned int)mac.data[4],(unsigned int)mac.data[5]) + 1;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"NetworkAddress",REG_SZ,tmps,tmpsl);
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"MAC",REG_SZ,tmps,tmpsl);
		DWORD tmp = mtu;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"MTU",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
		tmp = 0;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"EnableDHCP",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
	}

	// Done with registry
	RegCloseKey(nwAdapters);	

	// If we didn't get a device, we can't start
	if (_myDeviceInstanceId.length() == 0)
		throw std::runtime_error("unable to create new tap adapter");

	// Convert device GUID junk... blech
	{
		char nobraces[128];
		const char *nbtmp1 = _myDeviceInstanceId.c_str();
		char *nbtmp2 = nobraces;
		while (*nbtmp1) {
			if ((*nbtmp1 != '{')&&(*nbtmp1 != '}'))
				*nbtmp2++ = *nbtmp1;
			++nbtmp1;
		}
		*nbtmp2 = (char)0;
		if (UuidFromStringA((RPC_CSTR)nobraces,&_deviceGuid) != RPC_S_OK)
			throw std::runtime_error("unable to convert instance ID GUID to native GUID (invalid NetCfgInstanceId in registry?)");
	}

	// Disable and enable interface to ensure registry settings take effect
	{
		STARTUPINFOA startupInfo;
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo;
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + devcon + "\" disable @" + _myDeviceInstanceIdPath).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			RegCloseKey(nwAdapters);
			throw std::runtime_error(std::string("unable to find or execute devcon at ")+devcon);
		}
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}
	{
		STARTUPINFOA startupInfo;
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo;
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + devcon + "\" enable @" + _myDeviceInstanceIdPath).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			RegCloseKey(nwAdapters);
			throw std::runtime_error(std::string("unable to find or execute devcon at ")+devcon);
		}
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}

	// Open the tap, which is in this weird Windows analog of /dev
	char tapPath[4096];
	Utils::snprintf(tapPath,sizeof(tapPath),"\\\\.\\Global\\%s.tap",_myDeviceInstanceId.c_str());
	_tap = CreateFileA(tapPath,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED,NULL);
	if (_tap == INVALID_HANDLE_VALUE)
		throw std::runtime_error(std::string("unable to open tap device ")+tapPath);

	// Set media status to enabled
	uint32_t tmpi = 1;
	DWORD bytesReturned = 0;
	DeviceIoControl(_tap,TAP_WIN_IOCTL_SET_MEDIA_STATUS,&tmpi,sizeof(tmpi),&tmpi,sizeof(tmpi),&bytesReturned,NULL);

	// Initialized overlapped I/O structures and related events
	memset(&_tapOvlRead,0,sizeof(_tapOvlRead));
	_tapOvlRead.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	memset(&_tapOvlWrite,0,sizeof(_tapOvlWrite));
	_tapOvlWrite.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	// Start background thread that actually performs I/O
	_injectSemaphore = CreateSemaphore(NULL,0,1,NULL);
	_thread = Thread::start(this);

	// Certain functions can now work (e.g. ips())
	_initialized = true;
}

EthernetTap::~EthernetTap()
{
	_run = false;

	ReleaseSemaphore(_injectSemaphore,1,NULL);
	Thread::join(_thread);
	CloseHandle(_tap);
	CloseHandle(_tapOvlRead.hEvent);
	CloseHandle(_tapOvlWrite.hEvent);
	CloseHandle(_injectSemaphore);

#ifdef _WIN64
	BOOL is64Bit = TRUE;
	const char *devcon = "\\devcon_x64.exe";
#else
	BOOL is64Bit = FALSE;
	IsWow64Process(GetCurrentProcess(),&is64Bit);
	const char *devcon = ((is64Bit == TRUE) ? "\\devcon_x64.exe" : "\\devcon_x86.exe");
#endif

	// Disable network device on shutdown
	STARTUPINFOA startupInfo;
	startupInfo.cb = sizeof(startupInfo);
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo,0,sizeof(STARTUPINFOA));
	memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
	if (CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + devcon + "\" disable @" + _myDeviceInstanceIdPath).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}
}

void EthernetTap::setDisplayName(const char *dn)
{
	if (!_initialized)
		return;
	HKEY ifp;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,(std::string("SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\") + _myDeviceInstanceId).c_str(),0,KEY_READ|KEY_WRITE,&ifp) == ERROR_SUCCESS) {
		RegSetKeyValueA(ifp,"Connection","Name",REG_SZ,(LPCVOID)dn,(DWORD)(strlen(dn)+1));
		RegCloseKey(ifp);
	}
}

bool EthernetTap::addIP(const InetAddress &ip)
{
	if (!_initialized)
		return false;
	if (!ip.netmaskBits()) // sanity check... netmask of 0.0.0.0 is WUT?
		return false;

	std::set<InetAddress> haveIps(ips());

	try {
		// Add IP to interface at the netlink level if not already assigned.
		if (!haveIps.count(ip)) {
			std::pair<NET_LUID,NET_IFINDEX> ifidx = _findAdapterByGuid(_deviceGuid);
			MIB_UNICASTIPADDRESS_ROW ipr;

			InitializeUnicastIpAddressEntry(&ipr);
			if (ip.isV4()) {
				ipr.Address.Ipv4.sin_family = AF_INET;
				ipr.Address.Ipv4.sin_addr.S_un.S_addr = *((const uint32_t *)ip.rawIpData());
				ipr.OnLinkPrefixLength = ip.port();
			} else if (ip.isV6()) {
				// TODO
			} else return false;

			ipr.PrefixOrigin = IpPrefixOriginManual;
			ipr.SuffixOrigin = IpSuffixOriginManual;
			ipr.ValidLifetime = 0xffffffff;
			ipr.PreferredLifetime = 0xffffffff;

			ipr.InterfaceLuid = ifidx.first;
			ipr.InterfaceIndex = ifidx.second;

			if (CreateUnicastIpAddressEntry(&ipr) == NO_ERROR) {
				haveIps.insert(ip);
			} else {
				LOG("unable to add IP address %s to interface %s: %d",ip.toString().c_str(),deviceName().c_str(),(int)GetLastError());
				return false;
			}
		}

		_syncIpsWithRegistry(haveIps);
	} catch (std::exception &exc) {
		LOG("unexpected exception adding IP address %s to %s: %s",ip.toString().c_str(),deviceName().c_str(),exc.what());
	} catch ( ... ) {
		LOG("unexpected exception adding IP address %s to %s: unknown exception",ip.toString().c_str(),deviceName().c_str());
	}
	return false;
}

bool EthernetTap::removeIP(const InetAddress &ip)
{
	if (!_initialized)
		return false;
	try {
		MIB_UNICASTIPADDRESS_TABLE *ipt = (MIB_UNICASTIPADDRESS_TABLE *)0;
		std::pair<NET_LUID,NET_IFINDEX> ifidx = _findAdapterByGuid(_deviceGuid);
		if (GetUnicastIpAddressTable(AF_UNSPEC,&ipt) == NO_ERROR) {
			for(DWORD i=0;i<ipt->NumEntries;++i) {
				if ((ipt->Table[i].InterfaceLuid.Value == ifidx.first.Value)&&(ipt->Table[i].InterfaceIndex == ifidx.second)) {
					InetAddress addr;
					switch(ipt->Table[i].Address.si_family) {
						case AF_INET:
							addr.set(&(ipt->Table[i].Address.Ipv4.sin_addr.S_un.S_addr),4,ipt->Table[i].OnLinkPrefixLength);
							break;
						case AF_INET6:
							addr.set(ipt->Table[i].Address.Ipv6.sin6_addr.u.Byte,16,ipt->Table[i].OnLinkPrefixLength);
							if (addr.isLinkLocal())
								continue; // can't remove link-local IPv6 addresses
							break;
					}
					if (addr == ip) {
						DeleteUnicastIpAddressEntry(&(ipt->Table[i]));
						FreeMibTable(ipt);
						_syncIpsWithRegistry(ips());
						return true;
					}
				}
			}
			FreeMibTable((PVOID)ipt);
		}
	} catch (std::exception &exc) {
		LOG("unexpected exception removing IP address %s from %s: %s",ip.toString().c_str(),deviceName().c_str(),exc.what());
	} catch ( ... ) {
		LOG("unexpected exception removing IP address %s from %s: unknown exception",ip.toString().c_str(),deviceName().c_str());
	}
	return false;
}

std::set<InetAddress> EthernetTap::ips() const
{
	static const InetAddress linkLocalLoopback("fe80::1",64); // what is this and why does Windows assign it?
	std::set<InetAddress> addrs;

	if (!_initialized)
		return addrs;

	try {
		MIB_UNICASTIPADDRESS_TABLE *ipt = (MIB_UNICASTIPADDRESS_TABLE *)0;
		std::pair<NET_LUID,NET_IFINDEX> ifidx = _findAdapterByGuid(_deviceGuid);

		if (GetUnicastIpAddressTable(AF_UNSPEC,&ipt) == NO_ERROR) {
			for(DWORD i=0;i<ipt->NumEntries;++i) {
				if ((ipt->Table[i].InterfaceLuid.Value == ifidx.first.Value)&&(ipt->Table[i].InterfaceIndex == ifidx.second)) {
					switch(ipt->Table[i].Address.si_family) {
						case AF_INET: {
							InetAddress ip(&(ipt->Table[i].Address.Ipv4.sin_addr.S_un.S_addr),4,ipt->Table[i].OnLinkPrefixLength);
							if (ip != InetAddress::LO4)
								addrs.insert(ip);
						}	break;
						case AF_INET6: {
							InetAddress ip(ipt->Table[i].Address.Ipv6.sin6_addr.u.Byte,16,ipt->Table[i].OnLinkPrefixLength);
							if ((ip != linkLocalLoopback)&&(ip != InetAddress::LO6))
								addrs.insert(ip);
						}	break;
					}
				}
			}
			FreeMibTable(ipt);
		}
	} catch ( ... ) {}

	return addrs;
}

void EthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	if (!_initialized)
		return;
	if (len > (ZT_IF_MTU))
		return;

	{
		Mutex::Lock _l(_injectPending_m);
		_injectPending.push( std::pair<Array<char,ZT_IF_MTU + 32>,unsigned int>(Array<char,ZT_IF_MTU + 32>(),len + 14) );
		char *d = _injectPending.back().first.data;
		memcpy(d,to.data,6);
		memcpy(d + 6,from.data,6);
		d[12] = (char)((etherType >> 8) & 0xff);
		d[13] = (char)(etherType & 0xff);
		memcpy(d + 14,data,len);
	}

	ReleaseSemaphore(_injectSemaphore,1,NULL);
}

std::string EthernetTap::deviceName() const
{
	return _myDeviceInstanceId;
}

std::string EthernetTap::persistentId() const
{
	return _myDeviceInstanceIdPath;
}

bool EthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
{
	if (!_initialized)
		return false;

	std::set<MulticastGroup> newGroups;

	// Ensure that groups are added for each IP... this handles the MAC:ADI
	// groups that are created from IPv4 addresses. Some of these may end
	// up being duplicates of what the IOCTL returns but that's okay since
	// the set<> will filter that.
	std::set<InetAddress> ipaddrs(ips());
	for(std::set<InetAddress>::const_iterator i(ipaddrs.begin());i!=ipaddrs.end();++i)
		newGroups.insert(MulticastGroup::deriveMulticastGroupForAddressResolution(*i));

	// The ZT1 tap driver supports an IOCTL to get multicast memberships at the L2
	// level... something Windows does not seem to expose ordinarily. This lets
	// pretty much anything work... IPv4, IPv6, IPX, oldskool Netbios, who knows...
	unsigned char mcastbuf[TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS_OUTPUT_BUF_SIZE];
	DWORD bytesReturned = 0;
	if (DeviceIoControl(_tap,TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS,(LPVOID)0,0,(LPVOID)mcastbuf,sizeof(mcastbuf),&bytesReturned,NULL)) {
		MAC mac;
		DWORD i = 0;
		while ((i + 6) <= bytesReturned) {
			mac.data[0] = mcastbuf[i++];
			mac.data[1] = mcastbuf[i++];
			mac.data[2] = mcastbuf[i++];
			mac.data[3] = mcastbuf[i++];
			mac.data[4] = mcastbuf[i++];
			mac.data[5] = mcastbuf[i++];
			if (mac.isMulticast()) {
				// exclude the nulls that may be returned or any other junk Windows puts in there
				newGroups.insert(MulticastGroup(mac,0));
			}
		}
	}

	newGroups.insert(_blindWildcardMulticastGroup); // always join this

	bool changed = false;

	for(std::set<MulticastGroup>::iterator mg(newGroups.begin());mg!=newGroups.end();++mg) {
		if (!groups.count(*mg)) {
			groups.insert(*mg);
			changed = true;
		}
	}
	for(std::set<MulticastGroup>::iterator mg(groups.begin());mg!=groups.end();) {
		if (!newGroups.count(*mg)) {
			groups.erase(mg++);
			changed = true;
		} else ++mg;
	}

	return changed;
}

void EthernetTap::threadMain()
	throw()
{
	HANDLE wait4[3];
	wait4[0] = _injectSemaphore;
	wait4[1] = _tapOvlRead.hEvent;
	wait4[2] = _tapOvlWrite.hEvent; // only included if writeInProgress is true

	ReadFile(_tap,_tapReadBuf,sizeof(_tapReadBuf),NULL,&_tapOvlRead);
	bool writeInProgress = false;

	for(;;) {
		if (!_run) break;
		DWORD r = WaitForMultipleObjectsEx(writeInProgress ? 3 : 2,wait4,FALSE,INFINITE,TRUE);
		if (!_run) break;

		if (HasOverlappedIoCompleted(&_tapOvlRead)) {
			DWORD bytesRead = 0;
			if (GetOverlappedResult(_tap,&_tapOvlRead,&bytesRead,FALSE)) {
				if (bytesRead > 14) {
					MAC to(_tapReadBuf);
					MAC from(_tapReadBuf + 6);
					unsigned int etherType = Utils::ntoh(*((const uint16_t *)(_tapReadBuf + 12)));
					Buffer<4096> tmp(_tapReadBuf + 14,bytesRead - 14);
					_handler(_arg,from,to,etherType,tmp);
				}
			}
			ReadFile(_tap,_tapReadBuf,sizeof(_tapReadBuf),NULL,&_tapOvlRead);
		}

		if (writeInProgress) {
			if (HasOverlappedIoCompleted(&_tapOvlWrite)) {
				writeInProgress = false;
				_injectPending_m.lock();
				_injectPending.pop();
			} else continue; // still writing, so skip code below and wait
		} else _injectPending_m.lock();

		if (!_injectPending.empty()) {
			WriteFile(_tap,_injectPending.front().first.data,_injectPending.front().second,NULL,&_tapOvlWrite);
			writeInProgress = true;
		}

		_injectPending_m.unlock();
	}

	CancelIo(_tap);
}

bool EthernetTap::deletePersistentTapDevice(const RuntimeEnvironment *_r,const char *pid)
{
#ifdef _WIN64
	BOOL is64Bit = TRUE;
	const char *devcon = "\\devcon_x64.exe";
#else
	BOOL is64Bit = FALSE;
	IsWow64Process(GetCurrentProcess(),&is64Bit);
	const char *devcon = ((is64Bit == TRUE) ? "\\devcon_x64.exe" : "\\devcon_x86.exe");
#endif

	STARTUPINFOA startupInfo;
	startupInfo.cb = sizeof(startupInfo);
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo,0,sizeof(STARTUPINFOA));
	memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
	if (CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + devcon + "\" remove @" + pid).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
		return true;
	}

	return false;
}

void EthernetTap::_syncIpsWithRegistry(const std::set<InetAddress> &haveIps)
{
	// Update registry to contain all non-link-local IPs for this interface
	std::string regMultiIps,regMultiNetmasks;
	for(std::set<InetAddress>::const_iterator i(haveIps.begin());i!=haveIps.end();++i) {
		if (!i->isLinkLocal()) {
			regMultiIps.append(i->toIpString());
			regMultiIps.push_back((char)0);
			regMultiNetmasks.append(i->netmask().toIpString());
			regMultiNetmasks.push_back((char)0);
		}
	}
	HKEY tcpIpInterfaces;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
		if (regMultiIps.length()) {
			regMultiIps.push_back((char)0);
			regMultiNetmasks.push_back((char)0);
			RegSetKeyValueA(tcpIpInterfaces,_myDeviceInstanceId.c_str(),"IPAddress",REG_MULTI_SZ,regMultiIps.data(),(DWORD)regMultiIps.length());
			RegSetKeyValueA(tcpIpInterfaces,_myDeviceInstanceId.c_str(),"SubnetMask",REG_MULTI_SZ,regMultiNetmasks.data(),(DWORD)regMultiNetmasks.length());
		} else {
			RegDeleteKeyValueA(tcpIpInterfaces,_myDeviceInstanceId.c_str(),"IPAddress");
			RegDeleteKeyValueA(tcpIpInterfaces,_myDeviceInstanceId.c_str(),"SubnetMask");
		}
	}
	RegCloseKey(tcpIpInterfaces);
}

} // namespace ZeroTier

#endif // __WINDOWS__ ////////////////////////////////////////////////////////
