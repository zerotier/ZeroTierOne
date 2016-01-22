/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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

#include <algorithm>
#include <utility>

#include "../node/Constants.hpp"
#include "../node/Utils.hpp"
#include "../node/Mutex.hpp"
#include "../node/Dictionary.hpp"
#include "OSUtils.hpp"
#include "LinuxEthernetTap.hpp"

// ff:ff:ff:ff:ff:ff with no ADI
static const ZeroTier::MulticastGroup _blindWildcardMulticastGroup(ZeroTier::MAC(0xff),0);

namespace ZeroTier {

static Mutex __tapCreateLock;

LinuxEthernetTap::LinuxEthernetTap(
	const char *homePath,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *friendlyName,
	void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
	void *arg) :
	_handler(handler),
	_arg(arg),
	_nwid(nwid),
	_homePath(homePath),
	_mtu(mtu),
	_fd(0),
	_enabled(true)
{
	char procpath[128],nwids[32];
	struct stat sbuf;

	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",nwid);

	Mutex::Lock _l(__tapCreateLock); // create only one tap at a time, globally

	if (mtu > 2800)
		throw std::runtime_error("max tap MTU is 2800");

	_fd = ::open("/dev/net/tun",O_RDWR);
	if (_fd <= 0) {
		_fd = ::open("/dev/tun",O_RDWR);
		if (_fd <= 0)
			throw std::runtime_error(std::string("could not open TUN/TAP device: ") + strerror(errno));
	}

	struct ifreq ifr;
	memset(&ifr,0,sizeof(ifr));

	// Try to recall our last device name, or pick an unused one if that fails.
	bool recalledDevice = false;
	std::string devmapbuf;
	Dictionary devmap;
	if (OSUtils::readFile((_homePath + ZT_PATH_SEPARATOR_S + "devicemap").c_str(),devmapbuf)) {
		devmap.fromString(devmapbuf);
		std::string desiredDevice(devmap.get(nwids,""));
		if (desiredDevice.length() > 2) {
			Utils::scopy(ifr.ifr_name,sizeof(ifr.ifr_name),desiredDevice.c_str());
			Utils::snprintf(procpath,sizeof(procpath),"/proc/sys/net/ipv4/conf/%s",ifr.ifr_name);
			recalledDevice = (stat(procpath,&sbuf) != 0);
		}
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

	::ioctl(_fd,TUNSETPERSIST,0); // valgrind may generate a false alarm here

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
	::fcntl(_fd,F_SETFD,fcntl(_fd,F_GETFD) | FD_CLOEXEC);

	::pipe(_shutdownSignalPipe);

	devmap[nwids] = _dev;
	OSUtils::writeFile((_homePath + ZT_PATH_SEPARATOR_S + "devicemap").c_str(),devmap.toString());

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
}

bool LinuxEthernetTap::enabled() const
{
	return _enabled;
}

static bool ___removeIp(const std::string &_dev,const InetAddress &ip)
{
	long cpid = (long)vfork();
	if (cpid == 0) {
		OSUtils::redirectUnixOutputs("/dev/null",(const char *)0);
		setenv("PATH", "/sbin:/bin:/usr/sbin:/usr/bin", 1);
		::execlp("ip","ip","addr","del",ip.toString().c_str(),"dev",_dev.c_str(),(const char *)0);
		::_exit(-1);
	} else {
		int exitcode = -1;
		::waitpid(cpid,&exitcode,0);
		return (exitcode == 0);
	}
}

bool LinuxEthernetTap::addIp(const InetAddress &ip)
{
	if (!ip)
		return false;

	std::vector<InetAddress> allIps(ips());
	if (std::binary_search(allIps.begin(),allIps.end(),ip))
		return true;

	// Remove and reconfigure if address is the same but netmask is different
	for(std::vector<InetAddress>::iterator i(allIps.begin());i!=allIps.end();++i) {
		if (i->ipsEqual(ip))
			___removeIp(_dev,*i);
	}

	long cpid = (long)vfork();
	if (cpid == 0) {
		OSUtils::redirectUnixOutputs("/dev/null",(const char *)0);
		setenv("PATH", "/sbin:/bin:/usr/sbin:/usr/bin", 1);
		if (ip.isV4()) {
			::execlp("ip","ip","addr","add",ip.toString().c_str(),"broadcast",ip.broadcast().toIpString().c_str(),"dev",_dev.c_str(),(const char *)0);
		} else {
			::execlp("ip","ip","addr","add",ip.toString().c_str(),"dev",_dev.c_str(),(const char *)0);
		}
		::_exit(-1);
	} else if (cpid > 0) {
		int exitcode = -1;
		::waitpid(cpid,&exitcode,0);
		return (exitcode == 0);
	}

	return false;
}

bool LinuxEthernetTap::removeIp(const InetAddress &ip)
{
	if (!ip)
		return true;
	std::vector<InetAddress> allIps(ips());
	if (!std::binary_search(allIps.begin(),allIps.end(),ip)) {
		if (___removeIp(_dev,ip))
			return true;
	}
	return false;
}

std::vector<InetAddress> LinuxEthernetTap::ips() const
{
	struct ifaddrs *ifa = (struct ifaddrs *)0;
	if (getifaddrs(&ifa))
		return std::vector<InetAddress>();

	std::vector<InetAddress> r;

	struct ifaddrs *p = ifa;
	while (p) {
		if ((!strcmp(p->ifa_name,_dev.c_str()))&&(p->ifa_addr)&&(p->ifa_netmask)&&(p->ifa_addr->sa_family == p->ifa_netmask->sa_family)) {
			switch(p->ifa_addr->sa_family) {
				case AF_INET: {
					struct sockaddr_in *sin = (struct sockaddr_in *)p->ifa_addr;
					struct sockaddr_in *nm = (struct sockaddr_in *)p->ifa_netmask;
					r.push_back(InetAddress(&(sin->sin_addr.s_addr),4,Utils::countBits((uint32_t)nm->sin_addr.s_addr)));
				}	break;
				case AF_INET6: {
					struct sockaddr_in6 *sin = (struct sockaddr_in6 *)p->ifa_addr;
					struct sockaddr_in6 *nm = (struct sockaddr_in6 *)p->ifa_netmask;
					uint32_t b[4];
					memcpy(b,nm->sin6_addr.s6_addr,sizeof(b));
					r.push_back(InetAddress(sin->sin6_addr.s6_addr,16,Utils::countBits(b[0]) + Utils::countBits(b[1]) + Utils::countBits(b[2]) + Utils::countBits(b[3])));
				}	break;
			}
		}
		p = p->ifa_next;
	}

	if (ifa)
		freeifaddrs(ifa);

	std::sort(r.begin(),r.end());
	std::unique(r.begin(),r.end());

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

void LinuxEthernetTap::setFriendlyName(const char *friendlyName)
{
}

void LinuxEthernetTap::scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed)
{
	char *ptr,*ptr2;
	unsigned char mac[6];
	std::vector<MulticastGroup> newGroups;

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
					newGroups.push_back(MulticastGroup(MAC(mac,6),0));
			}
		}
		::close(fd);
	}

	std::vector<InetAddress> allIps(ips());
	for(std::vector<InetAddress>::iterator ip(allIps.begin());ip!=allIps.end();++ip)
		newGroups.push_back(MulticastGroup::deriveMulticastGroupForAddressResolution(*ip));

	std::sort(newGroups.begin(),newGroups.end());
	std::unique(newGroups.begin(),newGroups.end());

	for(std::vector<MulticastGroup>::iterator m(newGroups.begin());m!=newGroups.end();++m) {
		if (!std::binary_search(_multicastGroups.begin(),_multicastGroups.end(),*m))
			added.push_back(*m);
	}
	for(std::vector<MulticastGroup>::iterator m(_multicastGroups.begin());m!=_multicastGroups.end();++m) {
		if (!std::binary_search(newGroups.begin(),newGroups.end(),*m))
			removed.push_back(*m);
	}

	_multicastGroups.swap(newGroups);
}

void LinuxEthernetTap::threadMain()
	throw()
{
	fd_set readfds,nullfds;
	MAC to,from;
	int n,nfds,r;
	char getBuf[8194];

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
				if ((errno != EINTR)&&(errno != ETIMEDOUT))
					break;
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
						// TODO: VLAN support
						_handler(_arg,_nwid,from,to,etherType,0,(const void *)(getBuf + 14),r - 14);
					}

					r = 0;
				}
			}
		}
	}
}

} // namespace ZeroTier
