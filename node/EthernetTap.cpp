/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
#define ZT_MAC_IPCONFIG_COMMAND 4

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
		_findCmd(ZT_MAC_IPCONFIG_COMMAND,"ipconfig");
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
		sprintf(tmp,"/sbin/%s",name);
		if (ZeroTier::Utils::fileExists(tmp)) {
			_paths[id] = tmp;
			return;
		}
		sprintf(tmp,"/usr/sbin/%s",name);
		if (ZeroTier::Utils::fileExists(tmp)) {
			_paths[id] = tmp;
			return;
		}
		sprintf(tmp,"/bin/%s",name);
		if (ZeroTier::Utils::fileExists(tmp)) {
			_paths[id] = tmp;
			return;
		}
		sprintf(tmp,"/usr/bin/%s",name);
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
#endif // __LINUX__

#ifdef __APPLE__
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
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
	Mutex::Lock _l(__tapCreateLock); // create only one tap at a time, globally

	if (mtu > 4096)
		throw std::runtime_error("max tap MTU is 4096");

	_fd = ::open("/dev/net/tun",O_RDWR);
	if (_fd <= 0)
		throw std::runtime_error(std::string("could not open TUN/TAP device: ") + strerror(errno));

	struct ifreq ifr;
	memset(&ifr,0,sizeof(ifr));

	{ // pick an unused device name
		int devno = 0;
		struct stat sbuf;
		do {
			sprintf(ifr.ifr_name,"zt%d",devno++);
			sprintf(procpath,"/proc/sys/net/ipv4/conf/%s",ifr.ifr_name);
		} while (stat(procpath,&sbuf) == 0);
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
	char devpath[64],ethaddr[64],mtustr[16];
	struct stat tmp;
	Mutex::Lock _l(__tapCreateLock); // create only one tap at a time, globally

	if (mtu > 4096)
		throw std::runtime_error("max tap MTU is 4096");

	// Check for existence of ZT tap devices, try to load module if not there
	const char *kextload = UNIX_COMMANDS[ZT_MAC_KEXTLOAD_COMMAND];
	if ((stat("/dev/zt0",&tmp))&&(kextload)) {
		long kextpid;
		char tmp[4096];
		strcpy(tmp,_r->homePath.c_str());
		if ((kextpid = (long)vfork()) == 0) {
			chdir(tmp);
			execl(kextload,kextload,"-q","-repository",tmp,"tap.kext",(const char *)0);
			_exit(-1);
		} else {
			int exitcode = -1;
			waitpid(kextpid,&exitcode,0);
			usleep(500);
		}
	}
	if (stat("/dev/zt0",&tmp))
		throw std::runtime_error("/dev/zt# tap devices do not exist and unable to load kernel extension");

	// Open the first available device (ones in use will fail with resource busy)
	for(int i=0;i<256;++i) {
		sprintf(devpath,"/dev/zt%d",i);
		if (stat(devpath,&tmp))
			throw std::runtime_error("no more TAP devices available");
		_fd = ::open(devpath,O_RDWR);
		if (_fd > 0) {
			sprintf(_dev,"zt%d",i);
			break;
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
	sprintf(ethaddr,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(int)mac[0],(int)mac[1],(int)mac[2],(int)mac[3],(int)mac[4],(int)mac[5]);
	sprintf(mtustr,"%u",mtu);
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

	whack(); // turns on IPv6 on OSX

	::pipe(_shutdownSignalPipe);

	_thread = Thread::start(this);
}
#endif // __APPLE__

EthernetTap::~EthernetTap()
{
	::write(_shutdownSignalPipe[1],"\0",1); // causes thread to exit
	Thread::join(_thread);
	::close(_fd);
}

#ifdef __APPLE__
void EthernetTap::whack()
{
	const char *ipconfig = UNIX_COMMANDS[ZT_MAC_IPCONFIG_COMMAND];
	if (ipconfig) {
		long cpid = (long)vfork();
		if (cpid == 0) {
			execl(ipconfig,ipconfig,"set",_dev,"AUTOMATIC-V6",(const char *)0);
			_exit(-1);
		} else {
			int exitcode = -1;
			waitpid(cpid,&exitcode,0);
		}
	}
}
#else
void EthernetTap::whack() {}
#endif // __APPLE__ / !__APPLE__

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

	Mutex::Lock _l(_ips_m);

	if (!ip)
		return false;
	if (_ips.count(ip) > 0)
		return true;

	// Remove and reconfigure if address is the same but netmask is different
	for(std::set<InetAddress>::iterator i(_ips.begin());i!=_ips.end();++i) {
		if (i->ipsEqual(ip)) {
			if (___removeIp(_dev,*i)) {
				_ips.erase(i);
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
	} else {
		int exitcode = -1;
		waitpid(cpid,&exitcode,0);
		if (exitcode == 0) {
			_ips.insert(ip);
			return true;
		} else return false;
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

	Mutex::Lock _l(_ips_m);

	if (!ip)
		return false;
	if (_ips.count(ip) > 0)
		return true; // IP/netmask already assigned

	// Remove and reconfigure if address is the same but netmask is different
	for(std::set<InetAddress>::iterator i(_ips.begin());i!=_ips.end();++i) {
		if ((i->ipsEqual(ip))&&(i->netmaskBits() != ip.netmaskBits())) {
			if (___removeIp(_dev,*i)) {
				_ips.erase(i);
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
		if (exitcode == 0) {
			_ips.insert(ip);
			return true;
		}
	}

	return false;
}
#endif // __APPLE__

bool EthernetTap::removeIP(const InetAddress &ip)
{
	Mutex::Lock _l(_ips_m);
	if (_ips.count(ip) > 0) {
		if (___removeIp(_dev,ip)) {
			_ips.erase(ip);
			return true;
		}
	}
	return false;
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
		Mutex::Lock _l(_ips_m);
		for(std::set<InetAddress>::const_iterator i(_ips.begin());i!=_ips.end();++i)
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
bool EthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
{
	std::set<MulticastGroup> newGroups;
	struct ifmaddrs *ifmap = (struct ifmaddrs *)0;
	if (!getifmaddrs(&ifmap)) {
		struct ifmaddrs *p = ifmap;
		while (p) {
			if (p->ifma_addr->sa_family == AF_LINK) {
				struct sockaddr_dl *in = (struct sockaddr_dl *)p->ifma_name;
				struct sockaddr_dl *la = (struct sockaddr_dl *)p->ifma_addr;
				if ((la->sdl_alen == 6)&&(in->sdl_nlen <= sizeof(_dev))&&(!memcmp(_dev,in->sdl_data,in->sdl_nlen)))
					newGroups.insert(MulticastGroup(MAC(la->sdl_data + la->sdl_nlen),0));
			}
			p = p->ifma_next;
		}
		freeifmaddrs(ifmap);
	}

	{
		Mutex::Lock _l(_ips_m);
		for(std::set<InetAddress>::const_iterator i(_ips.begin());i!=_ips.end();++i)
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
	char getBuf[4096 + 14];
	Buffer<4096> data;

	// Wait for a moment after startup -- wait for Network to finish
	// constructing itself.
	Thread::sleep(500);

	FD_ZERO(&readfds);
	FD_ZERO(&nullfds);
	int nfds = (int)std::max(_shutdownSignalPipe[0],_fd) + 1;

	for(;;) {
		FD_SET(_shutdownSignalPipe[0],&readfds);
		FD_SET(_fd,&readfds);
		select(nfds,&readfds,&nullfds,&nullfds,(struct timeval *)0);

		if (FD_ISSET(_shutdownSignalPipe[0],&readfds)) // writes to shutdown pipe terminate thread
			break;

		if (FD_ISSET(_fd,&readfds)) {
			int n = (int)::read(_fd,getBuf,_mtu + 14);

			if (n > 14) {
				for(int i=0;i<6;++i)
					to.data[i] = (unsigned char)getBuf[i];
				for(int i=0;i<6;++i)
					from.data[i] = (unsigned char)getBuf[i + 6];
				data.copyFrom(getBuf + 14,(unsigned int)n - 14);
				_handler(_arg,from,to,ntohs(((const uint16_t *)getBuf)[6]),data);
			} else if (n < 0) {
				if ((errno != EINTR)&&(errno != ETIMEDOUT)) {
					TRACE("unexpected error reading from tap: %s",strerror(errno));
					break;
				}
			}
		}
	}
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

namespace ZeroTier {

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
	_arg(arg)
{
	char subkeyName[4096];
	char subkeyClass[4096];
	char data[4096];

	Mutex::Lock _l(_systemTapInitLock); // only init one tap at a time, process-wide

	HKEY nwAdapters;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",0,KEY_READ|KEY_WRITE,&nwAdapters) != ERROR_SUCCESS)
		throw std::runtime_error("unable to open registry key for network adapter enumeration");

	std::set<std::string> existingDeviceInstances;
	std::string mySubkeyName;

	// Enumerate all Microsoft Loopback Adapter instances and look for one
	// that matches our tag.
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
					if (!strcmpi(data,"*msloop")) {
						std::string instanceId;
						type = 0;
						dataLen = sizeof(data);
						if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
							instanceId.assign(data,dataLen);
							existingDeviceInstances.insert(instanceId);
						}

						if ((_myDeviceInstanceId.length() == 0)&&(instanceId.length() != 0)) {
							type = 0;
							dataLen = sizeof(data);
							if (RegGetValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
								data[dataLen] = '\0';
								if (!strcmp(data,tag)) {
									type = 0;
									dataLen = sizeof(data);
									if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
										_myDeviceInstanceId = instanceId;
										mySubkeyName = subkeyName;
										subkeyIndex = -1; // break outer loop
									}
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
#ifdef _WIN64
		const char *devcon = "\\devcon64.exe";
#else
		BOOL f64 = FALSE;
		const char *devcon = ((IsWow64Process(GetCurrentProcess(),&f64) == TRUE) ? "\\devcon64.exe" : "\\devcon32.exe");
#endif
		char windir[4096];
		windir[0] = '\0';
		GetWindowsDirectoryA(windir,sizeof(windir));
		STARTUPINFOA startupInfo;
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo;
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + devcon + "\" install " + windir + "\\inf\\netloop.inf *msloop").c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			RegCloseKey(nwAdapters);
			throw std::runtime_error(std::string("unable to find or execute devcon at ")+devcon);
		}
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);

		// Scan for that new instance by looking for adapters of type
		// *msloop that we did not already see on the first scan.
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
						if (!strcmpi(data,"*msloop")) {
							type = 0;
							dataLen = sizeof(data);
							if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
								if (existingDeviceInstances.count(std::string(data,dataLen)) == 0) {
									RegSetKeyValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",REG_SZ,tag,strlen(tag)+1);
									_myDeviceInstanceId.assign(data,dataLen);
									mySubkeyName = subkeyName;
									subkeyIndex = -1; // break outer loop
								}
							}
						}
					}
					break;
			}
		}
	}

	if (_myDeviceInstanceId.length() > 0) {
		char tmps[4096];
		sprintf_s(tmps,"%.2X-%.2X-%.2X-%.2X-%.2X-%.2X",(unsigned int)mac.data[0],(unsigned int)mac.data[1],(unsigned int)mac.data[2],(unsigned int)mac.data[3],(unsigned int)mac.data[4],(unsigned int)mac.data[5]);
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"NetworkAddress",REG_SZ,tmps,strlen(tmps)+1);
		DWORD tmp = mtu;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"MTU",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
		tmp = 0;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"EnableDHCP",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"DriverDesc",REG_SZ,"ZeroTier One Virtual LAN",25);
	}

	RegCloseKey(nwAdapters);	

	if (_myDeviceInstanceId.length() == 0)
		throw std::runtime_error("unable to create new loopback adapter for tap");

	//Thread::start(this);
}

EthernetTap::~EthernetTap()
{
}

void EthernetTap::whack()
{
}

bool EthernetTap::addIP(const InetAddress &ip)
{
	return false;
}

bool EthernetTap::removeIP(const InetAddress &ip)
{
	return false;
}

void EthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	if (len > (ZT_IF_MTU))
		return;

	{
		Mutex::Lock _l(_injectPending_m);
		_injectPending.push( std::pair<Array<char,ZT_IF_MTU + 32>,unsigned int>(Array<char,ZT_IF_MTU + 32>(),len + 14) );
		char *d = _injectPending.back().first.data;
		memcpy(d,to.data,6);
		memcpy(d + 6,from.data,6);
		*((uint16_t *)(d + 12)) = Utils::hton(etherType);
		memcpy(d + 14,data,len);
	}

	//ReleaseSemaphore(_pcapIoThread.updateSem,1,NULL);
}

std::string EthernetTap::deviceName() const
{
	return _myDeviceInstanceId;
}

bool EthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
{
	// TODO
	return false;
}

void EthernetTap::threadMain()
	throw()
{
}

} // namespace ZeroTier

#endif // __WINDOWS__ ////////////////////////////////////////////////////////
