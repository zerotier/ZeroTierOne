/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "../core/Constants.hpp"

#ifdef __APPLE__

#include "../core/Utils.hpp"
#include "../core/Mutex.hpp"
#include "../core/Dictionary.hpp"
#include "OSUtils.hpp"
#include "MacEthernetTap.hpp"
#include "MacEthernetTapAgent.h"

#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <signal.h>

#include <fcntl.h>
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <sys/sysctl.h>
#include <ifaddrs.h>

#include <string>
#include <map>
#include <set>
#include <algorithm>

static const ZeroTier::MulticastGroup _blindWildcardMulticastGroup(ZeroTier::MAC(0xff),0);

namespace ZeroTier {

static Mutex globalTapCreateLock;
static bool globalTapInitialized = false;

MacEthernetTap::MacEthernetTap(
	const char *homePath,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *friendlyName,
	void (*handler)(void *,void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *data,unsigned int len),
	void *arg) :
	_handler(handler),
	_arg(arg),
	_nwid(nwid),
	_homePath(homePath),
	_mtu(mtu),
	_agentStdin(-1),
	_agentStdout(-1),
	_agentStderr(-1),
	_agentStdin2(-1),
	_agentStdout2(-1),
	_agentStderr2(-1),
	_agentPid(-1),
	_enabled(true)
{
	char ethaddr[64],mtustr[16],devnostr[16],devstr[16],metricstr[16];
	OSUtils::ztsnprintf(ethaddr,sizeof(ethaddr),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(int)mac[0],(int)mac[1],(int)mac[2],(int)mac[3],(int)mac[4],(int)mac[5]);
	OSUtils::ztsnprintf(mtustr,sizeof(mtustr),"%u",mtu);
	OSUtils::ztsnprintf(metricstr,sizeof(metricstr),"%u",metric);

	std::string agentPath(homePath);
	agentPath.push_back(ZT_PATH_SEPARATOR);
	agentPath.append("MacEthernetTapAgent");
	if (!OSUtils::fileExists(agentPath.c_str()))
		throw std::runtime_error("MacEthernetTapAgent not installed in ZeroTier home");

	Mutex::Lock _gl(globalTapCreateLock); // only make one at a time

	// Destroy all feth devices on first tap start in case ZeroTier did not exit cleanly last time.
	// We leave interfaces less than feth100 alone in case something else is messing with feth devices.
	if (!globalTapInitialized) {
		globalTapInitialized = true;
		struct ifaddrs *ifa = (struct ifaddrs *)0;
		std::set<std::string> deleted;
		if (!getifaddrs(&ifa)) {
			struct ifaddrs *p = ifa;
			while (p) {
				if ((!strncmp(p->ifa_name,"feth",4))&&(strlen(p->ifa_name) >= 7)&&(deleted.count(std::string(p->ifa_name)) == 0)) {
					deleted.insert(std::string(p->ifa_name));
					const char *args[4];
					args[0] = "/sbin/ifconfig";
					args[1] = p->ifa_name;
					args[2] = "destroy";
					args[3] = (char *)0;
					const pid_t pid = vfork();
					if (pid == 0) {
						execv(args[0],const_cast<char **>(args));
						_exit(-1);
					} else if (pid > 0) {
						int rv = 0;
						waitpid(pid,&rv,0);
					}
				}
				p = p->ifa_next;
			}
			freeifaddrs(ifa);
		}
	}

	unsigned int devNo = 100 + ((nwid ^ (nwid >> 32) ^ (nwid >> 48)) % 4900);
	for(;;) {
		OSUtils::ztsnprintf(devnostr,sizeof(devnostr),"%u",devNo);
		OSUtils::ztsnprintf(devstr,sizeof(devstr),"feth%u",devNo);
		bool duplicate = false;
		struct ifaddrs *ifa = (struct ifaddrs *)0;
		if (!getifaddrs(&ifa)) {
			struct ifaddrs *p = ifa;
			while (p) {
				if (!strcmp(p->ifa_name,devstr)) {
					duplicate = true;
					break;
				}
				p = p->ifa_next;
			}
			freeifaddrs(ifa);
		}
		if (duplicate) {
			devNo = (devNo + 1) % 5000;
			if (devNo < 100)
				devNo = 100;
		} else {
			break;
		}
	}
	_dev = devstr;

	if (::pipe(_shutdownSignalPipe))
		throw std::runtime_error("pipe creation failed");

	int agentStdin[2];
	int agentStdout[2];
	int agentStderr[2];
	if (::pipe(agentStdin))
		throw std::runtime_error("pipe creation failed");
	if (::pipe(agentStdout))
		throw std::runtime_error("pipe creation failed");
	if (::pipe(agentStderr))
		throw std::runtime_error("pipe creation failed");
	_agentStdin = agentStdin[1];
	_agentStdout = agentStdout[0];
	_agentStderr = agentStderr[0];
	_agentStdin2 = agentStdin[0];
	_agentStdout2 = agentStdout[1];
	_agentStderr2 = agentStderr[1];
	long apid = (long)fork();
	if (apid < 0) {
		throw std::runtime_error("fork failed");
	} else if (apid == 0) {
		::dup2(agentStdin[0],STDIN_FILENO);
		::dup2(agentStdout[1],STDOUT_FILENO);
		::dup2(agentStderr[1],STDERR_FILENO);
		::close(agentStdin[0]);
		::close(agentStdin[1]);
		::close(agentStdout[0]);
		::close(agentStdout[1]);
		::close(agentStderr[0]);
		::close(agentStderr[1]);
		::execl(agentPath.c_str(),agentPath.c_str(),devnostr,ethaddr,mtustr,metricstr,(char *)0);
		::_exit(-1);
	} else {
		_agentPid = apid;
	}
	Thread::sleep(100); // this causes them to come up in a more user-friendly order on launch

	_thread = Thread::start(this);
}

MacEthernetTap::~MacEthernetTap()
{
	Mutex::Lock _gl(globalTapCreateLock);
	::write(_shutdownSignalPipe[1],"\0",1); // causes thread to exit
	Thread::join(_thread);
	::close(_shutdownSignalPipe[0]);
	::close(_shutdownSignalPipe[1]);
	int ec = 0;
	::kill(_agentPid,SIGTERM);
	::waitpid(_agentPid,&ec,0);
	::close(_agentStdin);
	::close(_agentStdout);
	::close(_agentStderr);
	::close(_agentStdin2);
	::close(_agentStdout2);
	::close(_agentStderr2);
}

void MacEthernetTap::setEnabled(bool en) { _enabled = en; }
bool MacEthernetTap::enabled() const { return _enabled; }

bool MacEthernetTap::addIp(const InetAddress &ip)
{
	char tmp[128];

	if (!ip)
		return false;

	std::string cmd;
	cmd.push_back((char)ZT_MACETHERNETTAPAGENT_STDIN_CMD_IFCONFIG);
	cmd.append((ip.family() == AF_INET6) ? "inet6" : "inet");
	cmd.push_back(0);
	cmd.append(ip.toString(tmp));
	cmd.push_back(0);
	cmd.append("alias");
	cmd.push_back(0);

	uint16_t l = (uint16_t)cmd.length();
	_putLock.lock();
	write(_agentStdin,&l,2);
	write(_agentStdin,cmd.data(),cmd.length());
	_putLock.unlock();

	return true;
}

bool MacEthernetTap::removeIp(const InetAddress &ip)
{
	char tmp[128];

	if (!ip)
		return false;

	std::string cmd;
	cmd.push_back((char)ZT_MACETHERNETTAPAGENT_STDIN_CMD_IFCONFIG);
	cmd.append((ip.family() == AF_INET6) ? "inet6" : "inet");
	cmd.push_back(0);
	cmd.append(ip.toString(tmp));
	cmd.push_back(0);
	cmd.append("-alias");
	cmd.push_back(0);

	uint16_t l = (uint16_t)cmd.length();
	_putLock.lock();
	write(_agentStdin,&l,2);
	write(_agentStdin,cmd.data(),cmd.length());
	_putLock.unlock();

	return true;
}

std::vector<InetAddress> MacEthernetTap::ips() const
{
	struct ifaddrs *ifa = (struct ifaddrs *)0;
	std::vector<InetAddress> r;
	if (!getifaddrs(&ifa)) {
		struct ifaddrs *p = ifa;
		while (p) {
			if ((p->ifa_name)&&(!strcmp(p->ifa_name,_dev.c_str()))&&(p->ifa_addr)) {
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
		freeifaddrs(ifa);
	}
	std::sort(r.begin(),r.end());
	r.erase(std::unique(r.begin(),r.end()),r.end());
	return r;
}

void MacEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	struct iovec iov[3];
	unsigned char hdr[15];
	uint16_t l;
	if ((_agentStdin > 0)&&(len <= _mtu)&&(_enabled)) {
		hdr[0] = ZT_MACETHERNETTAPAGENT_STDIN_CMD_PACKET;
		to.copyTo(hdr + 1);
		from.copyTo(hdr + 7);
		hdr[13] = (unsigned char)((etherType >> 8) & 0xff);
		hdr[14] = (unsigned char)(etherType & 0xff);
		l = (uint16_t)(len + 15);
		iov[0].iov_base = &l;
		iov[0].iov_len = 2;
		iov[1].iov_base = hdr;
		iov[1].iov_len = 15;
		iov[2].iov_base = const_cast<void *>(data);
		iov[2].iov_len = len;
		_putLock.lock();
		writev(_agentStdin,iov,3);
		_putLock.unlock();
	}
}

std::string MacEthernetTap::deviceName() const { return _dev; }
void MacEthernetTap::setFriendlyName(const char *friendlyName) {}

void MacEthernetTap::scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed)
{
	std::vector<MulticastGroup> newGroups;

	struct ifmaddrs *ifmap = (struct ifmaddrs *)0;
	if (!getifmaddrs(&ifmap)) {
		struct ifmaddrs *p = ifmap;
		while (p) {
			if (p->ifma_addr->sa_family == AF_LINK) {
				struct sockaddr_dl *in = (struct sockaddr_dl *)p->ifma_name;
				struct sockaddr_dl *la = (struct sockaddr_dl *)p->ifma_addr;
				if ((la->sdl_alen == 6)&&(in->sdl_nlen <= _dev.length())&&(!memcmp(_dev.data(),in->sdl_data,in->sdl_nlen)))
					newGroups.push_back(MulticastGroup(MAC((uint8_t *)(la->sdl_data + la->sdl_nlen)),0));
			}
			p = p->ifma_next;
		}
		freeifmaddrs(ifmap);
	}

	std::vector<InetAddress> allIps(ips());
	for(std::vector<InetAddress>::iterator ip(allIps.begin());ip!=allIps.end();++ip)
		newGroups.push_back(MulticastGroup::deriveMulticastGroupForAddressResolution(*ip));

	std::sort(newGroups.begin(),newGroups.end());
	newGroups.erase(std::unique(newGroups.begin(),newGroups.end()),newGroups.end());

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

void MacEthernetTap::setMtu(unsigned int mtu)
{
	if (_mtu != mtu) {
		char tmp[16];
		std::string cmd;
		cmd.push_back((char)ZT_MACETHERNETTAPAGENT_STDIN_CMD_IFCONFIG);
		cmd.append("mtu");
		cmd.push_back(0);
		OSUtils::ztsnprintf(tmp,sizeof(tmp),"%u",mtu);
		cmd.append(tmp);
		cmd.push_back(0);
		uint16_t l = (uint16_t)cmd.length();
		_putLock.lock();
		write(_agentStdin,&l,2);
		write(_agentStdin,cmd.data(),cmd.length());
		_putLock.unlock();
		_mtu = mtu;
	}
}

#define ZT_MACETHERNETTAP_AGENT_READ_BUF_SIZE 131072

void MacEthernetTap::threadMain()
	throw()
{
	char agentReadBuf[ZT_MACETHERNETTAP_AGENT_READ_BUF_SIZE];
	char agentStderrBuf[256];
	fd_set readfds,nullfds;
	MAC to,from;

	Thread::sleep(250);

	const int nfds = std::max(std::max(_shutdownSignalPipe[0],_agentStdout),_agentStderr) + 1;
	long agentReadPtr = 0;
	fcntl(_agentStdout,F_SETFL,fcntl(_agentStdout,F_GETFL)|O_NONBLOCK);
	fcntl(_agentStderr,F_SETFL,fcntl(_agentStderr,F_GETFL)|O_NONBLOCK);

	FD_ZERO(&readfds);
	FD_ZERO(&nullfds);
	for(;;) {
		FD_SET(_shutdownSignalPipe[0],&readfds);
		FD_SET(_agentStdout,&readfds);
		FD_SET(_agentStderr,&readfds);
		select(nfds,&readfds,&nullfds,&nullfds,(struct timeval *)0);

		if (FD_ISSET(_shutdownSignalPipe[0],&readfds))
			break;

		if (FD_ISSET(_agentStdout,&readfds)) {
			long n = (long)read(_agentStdout,agentReadBuf + agentReadPtr,ZT_MACETHERNETTAP_AGENT_READ_BUF_SIZE - agentReadPtr);
			if (n > 0) {
				agentReadPtr += n;
				while (agentReadPtr >= 2) {
					long len = *((uint16_t *)agentReadBuf);
					if (agentReadPtr >= (len + 2)) {
						char *msg = agentReadBuf + 2;

						if ((len > 14)&&(_enabled)) {
							to.setTo((uint8_t *)msg);
							from.setTo((uint8_t *)(msg + 6));
							_handler(_arg,(void *)0,_nwid,from,to,ntohs(((const uint16_t *)msg)[6]),0,(const void *)(msg + 14),(unsigned int)len - 14);
						}

						if (agentReadPtr > (len + 2)) {
							memmove(agentReadBuf,agentReadBuf + len + 2,agentReadPtr -= (len + 2));
						} else {
							agentReadPtr = 0;
						}
					} else {
						break;
					}
				}
			}
		}

		if (FD_ISSET(_agentStderr,&readfds)) {
			read(_agentStderr,agentStderrBuf,sizeof(agentStderrBuf));
			/*
			const ssize_t n = read(_agentStderr,agentStderrBuf,sizeof(agentStderrBuf));
			if (n > 0)
				write(STDERR_FILENO,agentStderrBuf,(size_t)n);
			*/
		}
	}
}

} // namespace ZeroTier

#endif // __APPLE__
