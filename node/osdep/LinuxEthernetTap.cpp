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
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_addr.h>
#include <linux/if_ether.h>
#include <ifaddrs.h>

#include <string>
#include <map>
#include <set>
#include <algorithm>

#include "../Constants.hpp"
#include "../Utils.hpp"
#include "../Mutex.hpp"
#include "LinuxEthernetTap.hpp"

// ff:ff:ff:ff:ff:ff with no ADI
static const ZeroTier::MulticastGroup _blindWildcardMulticastGroup(ZeroTier::MAC(0xff),0);

// On startup, searches the path for required external utilities (which might vary by Linux distro)
#define ZT_UNIX_IP_COMMAND 1
class _CommandFinder
{
public:
	_CommandFinder()
	{
		_findCmd(ZT_UNIX_IP_COMMAND,"ip");
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

namespace ZeroTier {

// Only permit one tap to be opened concurrently across the entire process
static Mutex __tapCreateLock;

LinuxEthernetTap::LinuxEthernetTap(
	const RuntimeEnvironment *renv,
	const char *tryToGetDevice,
	const MAC &mac,
	unsigned int mtu,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
	throw(std::runtime_error) :
	EthernetTap("LinuxEthernetTap",mac,mtu),
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
	mac.copyTo(ifr.ifr_ifru.ifru_hwaddr.sa_data,6);
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

	_thread = Thread::start(this);
}

LinuxEthernetTap::~LinuxEthernetTap()
{
	::write(_shutdownSignalPipe[1],"\0",1); // causes thread to exit
	Thread::join(_thread);
	::close(_fd);
	::close(_shutdownSignalPipe[0]);
	::close(_shutdownSignalPipe[1]);
}

void LinuxEthernetTap::setEnabled(bool en)
{
	_enabled = en;
	// TODO: interface status change
}

bool LinuxEthernetTap::enabled() const
{
	return _enabled;
}

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

bool LinuxEthernetTap::addIP(const InetAddress &ip)
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

bool LinuxEthernetTap::removeIP(const InetAddress &ip)
{
	if (ips().count(ip) > 0) {
		if (___removeIp(_dev,ip))
			return true;
	}
	return false;
}

std::set<InetAddress> LinuxEthernetTap::ips() const
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

void LinuxEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	char putBuf[8194];
	if ((_fd > 0)&&(len <= _mtu)&&(_enabled)) {
		to.copyTo(putBuf,6);
		from.copyTo(putBuf + 6,6);
		*((uint16_t *)(putBuf + 12)) = htons((uint16_t)etherType);
		memcpy(putBuf + 14,data,len);
		len += 14;
		::write(_fd,putBuf,len);
	}
}

std::string LinuxEthernetTap::deviceName() const
{
	return _dev;
}

std::string LinuxEthernetTap::persistentId() const
{
	return std::string();
}

bool LinuxEthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
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
					newGroups.insert(MulticastGroup(MAC(mac,6),0));
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

	for(std::set<MulticastGroup>::iterator mg(newGroups.begin());mg!=newGroups.end();++mg) {
		if (!groups.count(*mg)) {
			groups.insert(*mg);
			changed = true;
		}
	}
	for(std::set<MulticastGroup>::iterator mg(groups.begin());mg!=groups.end();) {
		if ((!newGroups.count(*mg))&&(*mg != _blindWildcardMulticastGroup)) {
			groups.erase(mg++);
			changed = true;
		} else ++mg;
	}

	return changed;
}

void LinuxEthernetTap::threadMain()
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

					if (_enabled) {
						to.setTo(getBuf,6);
						from.setTo(getBuf + 6,6);
						unsigned int etherType = ntohs(((const uint16_t *)getBuf)[6]);
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
