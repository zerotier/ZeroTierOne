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

#include <string>
#include <map>
#include <set>
#include <algorithm>

#include "Constants.hpp"
#include "UnixEthernetTap.hpp"
#include "Logger.hpp"
#include "RuntimeEnvironment.hpp"
#include "Utils.hpp"
#include "Mutex.hpp"

// ff:ff:ff:ff:ff:ff with no ADI
static const ZeroTier::MulticastGroup _blindWildcardMulticastGroup(ZeroTier::MAC(0xff),0);

// Command identifiers used with command finder static (on various *nixes)
#define ZT_UNIX_IP_COMMAND 1
#define ZT_UNIX_IFCONFIG_COMMAND 2
#define ZT_MAC_KEXTLOAD_COMMAND 3
#define ZT_MAC_KEXTUNLOAD_COMMAND 4

// Finds external commands on startup ----------------------------------------
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
// ---------------------------------------------------------------------------

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
UnixEthernetTap::UnixEthernetTap(
	const RuntimeEnvironment *renv,
	const char *tryToGetDevice,
	const MAC &mac,
	unsigned int mtu,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
	throw(std::runtime_error) :
	EthernetTap("UnixEthernetTap",mac,mtu),
	_r(renv),
	_handler(handler),
	_arg(arg),
	_fd(0),
	_enabled(true)
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
	if ((tryToGetDevice)&&(tryToGetDevice[0])) {
		Utils::scopy(ifr.ifr_name,sizeof(ifr.ifr_name),tryToGetDevice);
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

	_dev = ifr.ifr_name;

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

	// Set close-on-exec so that devices cannot persist if we fork/exec for update
	fcntl(_fd,F_SETFD,fcntl(_fd,F_GETFD) | FD_CLOEXEC);

	::pipe(_shutdownSignalPipe);

	TRACE("tap %s created",_dev.c_str());

	_thread = Thread::start(this);
}
#endif // __LINUX__

#ifdef __APPLE__
UnixEthernetTap::UnixEthernetTap(
	const RuntimeEnvironment *renv,
	const char *tryToGetDevice,
	const MAC &mac,
	unsigned int mtu,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
	throw(std::runtime_error) :
	EthernetTap("UnixEthernetTap",mac,mtu),
	_r(renv),
	_handler(handler),
	_arg(arg),
	_fd(0),
	_enabled(true)
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
	if ((tryToGetDevice)&&(tryToGetDevice[0])) {
		Utils::snprintf(devpath,sizeof(devpath),"/dev/%s",tryToGetDevice);
		if (stat(devpath,&stattmp) == 0) {
			_fd = ::open(devpath,O_RDWR);
			if (_fd > 0) {
				_dev = tryToGetDevice;
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
				char foo[16];
				Utils::snprintf(foo,sizeof(foo),"zt%d",i);
				_dev = foo;
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
		execl(ifconfig,ifconfig,_dev.c_str(),"lladdr",ethaddr,"mtu",mtustr,"up",(const char *)0);
		_exit(-1);
	} else {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		if (exitcode) {
			::close(_fd);
			throw std::runtime_error("ifconfig failure setting link-layer address and activating tap interface");
		}
	}

	_setIpv6Stuff(_dev.c_str(),true,false);

	// Set close-on-exec so that devices cannot persist if we fork/exec for update
	fcntl(_fd,F_SETFD,fcntl(_fd,F_GETFD) | FD_CLOEXEC);

	::pipe(_shutdownSignalPipe);

	_thread = Thread::start(this);

	EthernetTap_instances_m.lock();
	++EthernetTap_instances;
	EthernetTap_instances_m.unlock();
}
#endif // __APPLE__

UnixEthernetTap::~UnixEthernetTap()
{
	::write(_shutdownSignalPipe[1],"\0",1); // causes thread to exit
	Thread::join(_thread);
	::close(_fd);
	::close(_shutdownSignalPipe[0]);
	::close(_shutdownSignalPipe[1]);

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

void UnixEthernetTap::setEnabled(bool en)
{
	_enabled = en;
	// TODO: interface status change
}

bool UnixEthernetTap::enabled() const
{
	return _enabled;
}

void UnixEthernetTap::setDisplayName(const char *dn)
{
}

#ifdef __LINUX__
static bool ___removeIp(const std::string &_dev,const InetAddress &ip)
{
	const char *ipcmd = UNIX_COMMANDS[ZT_UNIX_IP_COMMAND];
	if (!ipcmd)
		return false;
	long cpid = (long)vfork();
	if (cpid == 0) {
		execl(ipcmd,ipcmd,"addr","del",ip.toString().c_str(),"dev",_dev.c_str(),(const char *)0);
		_exit(-1);
	} else {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		return (exitcode == 0);
	}
}

bool UnixEthernetTap::addIP(const InetAddress &ip)
{
	const char *ipcmd = UNIX_COMMANDS[ZT_UNIX_IP_COMMAND];
	if (!ipcmd) {
		LOG("ERROR: could not configure IP address for %s: unable to find 'ip' command on system (checked /sbin, /bin, /usr/sbin, /usr/bin)",_dev.c_str());
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
		execl(ipcmd,ipcmd,"addr","add",ip.toString().c_str(),"dev",_dev.c_str(),(const char *)0);
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
static bool ___removeIp(const std::string &_dev,const InetAddress &ip)
{
	const char *ifconfig = UNIX_COMMANDS[ZT_UNIX_IFCONFIG_COMMAND];
	if (!ifconfig)
		return false;
	long cpid;
	if ((cpid = (long)vfork()) == 0) {
		execl(ifconfig,ifconfig,_dev.c_str(),"inet",ip.toIpString().c_str(),"-alias",(const char *)0);
		_exit(-1);
	} else {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		return (exitcode == 0);
	}
	return false; // never reached, make compiler shut up about return value
}

bool UnixEthernetTap::addIP(const InetAddress &ip)
{
	const char *ifconfig = UNIX_COMMANDS[ZT_UNIX_IFCONFIG_COMMAND];
	if (!ifconfig) {
		LOG("ERROR: could not configure IP address for %s: unable to find 'ifconfig' command on system (checked /sbin, /bin, /usr/sbin, /usr/bin)",_dev.c_str());
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
		execl(ifconfig,ifconfig,_dev.c_str(),ip.isV4() ? "inet" : "inet6",ip.toString().c_str(),"alias",(const char *)0);
		_exit(-1);
	} else {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		return (exitcode == 0);
	}

	return false;
}
#endif // __APPLE__

bool UnixEthernetTap::removeIP(const InetAddress &ip)
{
	if (ips().count(ip) > 0) {
		if (___removeIp(_dev,ip))
			return true;
	}
	return false;
}

std::set<InetAddress> UnixEthernetTap::ips() const
{
	struct ifaddrs *ifa = (struct ifaddrs *)0;
	if (getifaddrs(&ifa))
		return std::set<InetAddress>();

	std::set<InetAddress> r;

	struct ifaddrs *p = ifa;
	while (p) {
		if ((!strcmp(p->ifa_name,_dev.c_str()))&&(p->ifa_addr)&&(p->ifa_netmask)&&(p->ifa_addr->sa_family == p->ifa_netmask->sa_family)) {
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

void UnixEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
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
			LOG("ERROR: write underrun: %s tap write() wrote %d of %u bytes of frame",_dev.c_str(),n,len);
		}
	}
}

std::string UnixEthernetTap::deviceName() const
{
	return _dev;
}

std::string UnixEthernetTap::persistentId() const
{
	return std::string();
}

#ifdef __LINUX__
bool UnixEthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
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
				if ((devname)&&(!strcmp(devname,_dev.c_str()))&&(mcastmac)&&(Utils::unhex(mcastmac,mac,6) == 6))
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

bool UnixEthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
{
	std::set<MulticastGroup> newGroups;
	struct _intl_ifmaddrs *ifmap = (struct _intl_ifmaddrs *)0;
	if (!_intl_getifmaddrs(&ifmap)) {
		struct _intl_ifmaddrs *p = ifmap;
		while (p) {
			if (p->ifma_addr->sa_family == AF_LINK) {
				struct sockaddr_dl *in = (struct sockaddr_dl *)p->ifma_name;
				struct sockaddr_dl *la = (struct sockaddr_dl *)p->ifma_addr;
				if ((la->sdl_alen == 6)&&(in->sdl_nlen <= _dev.length())&&(!memcmp(_dev.data(),in->sdl_data,in->sdl_nlen)))
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

void UnixEthernetTap::threadMain()
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

} // namespace ZeroTier
