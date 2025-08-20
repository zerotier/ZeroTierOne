/*
 * Copyright (c)2019 ZeroTier, Inc.
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

#include "ExtOsdep.hpp"

#include "../node/AtomicCounter.hpp"

#include <fcntl.h>
#include <iostream>
#include <list>
#include <sys/times.h>
#include <unistd.h>

#define ZT_TAP_BUF_SIZE 16384

namespace ZeroTier {

static int eodFd = -1;
static Mutex eodMutex;
static int eodMgmtFd = -1;

struct EodRoute {
	InetAddress target;
	InetAddress via;
	InetAddress src;
	std::string ifname;
};
static std::list<EodRoute> allRoutes;

template <typename T> static void __eodSend(const T& t)
{
	write(eodFd, &t, sizeof(t));
}

static void strncpyx(char* dest, const char* src, size_t n)
{
	strncpy(dest, src, n);
	if (n > 1) {
		dest[n - 1] = 0;
	}
}

static int __eodWait(unsigned char msg, unsigned char* d, unsigned l, unsigned maxl = 0, int* recvfd = nullptr)
{
	if (! maxl) {
		maxl = l;
	}
	auto start = times(NULL);
	while (1) {
		msghdr mh;
		iovec iov;
		struct {
			size_t cmsg_len;
			int cmsg_level;
			int cmsg_type;
			int fd;
		} __attribute__((packed)) cmsg;
		memset(&mh, 0, sizeof(mh));
		mh.msg_iov = &iov;
		mh.msg_iovlen = 1;
		if (recvfd) {
			mh.msg_control = &cmsg;
			mh.msg_controllen = sizeof(cmsg);
		}
		iov.iov_base = d;
		iov.iov_len = maxl;

		int r = recvmsg(eodFd, &mh, MSG_TRUNC | MSG_CMSG_CLOEXEC);
		if (r > 0) {
			if (recvfd && mh.msg_controllen >= sizeof(cmsg) && cmsg.cmsg_len == sizeof(cmsg) && cmsg.cmsg_level == SOL_SOCKET && cmsg.cmsg_type == SCM_RIGHTS) {
				*recvfd = cmsg.fd;
				fprintf(stderr, "eodWait: received fd %d\n", *recvfd);
			}

			if (d[0] != msg) {
				fprintf(stderr, "eodWait: wrong msg, expected %u got %u\n", msg, d[0]);
				return -1;
			}
			if ((unsigned)r < l || (unsigned)r > maxl) {
				fprintf(stderr, "eodWait: wrong len, expected %u got %d\n", l, r);
				return -1;
			}
			return r;
		}
		if (times(NULL) - start > 500) {
			fprintf(stderr, "eodWait: timeout\n");
			return -1;
		}
		usleep(100000);
	}
}

template <typename T> static bool __eodWait(unsigned msg, T& t)
{
	return __eodWait(msg, (unsigned char*)&t, sizeof(T)) == (int)sizeof(T);
}

template <typename M, typename R> static bool __eodXchg(const M& m, unsigned rm, R& r)
{
	__eodSend(m);
	return __eodWait(rm, r);
}

template <typename M, typename R> static bool eodXchg(const M& m, unsigned rm, R& r)
{
	Mutex::Lock l(eodMutex);
	return __eodXchg(m, rm, r);
}

void ExtOsdep::init(int fd1, int fd2)
{
	eodFd = fd1;
	eodMgmtFd = fd2;
	fcntl(eodMgmtFd, F_SETFL, O_NONBLOCK);
}

void ExtOsdep::started(int* f, void** cp)
{
	*f = eodMgmtFd;
	*cp = (void*)eodMgmtFd;

	unsigned char msg = ZT_EOD_MSG_STARTED;
	Mutex::Lock l(eodMutex);
	__eodSend(msg);
}

static std::string mgmtrd;
static std::string mgmtwr;

bool ExtOsdep::mgmtWritable(void* cookie)
{
	if (cookie != (void*)eodMgmtFd) {
		return false;
	}
	if (mgmtwr.size() == 0) {
		return true;
	}
	auto sz = write(eodMgmtFd, mgmtwr.data(), mgmtwr.size());
	if (sz <= 0) {
		return false;
	}
	mgmtwr.erase(mgmtwr.begin(), mgmtwr.begin() + sz);
	return mgmtwr.empty();
}

bool ExtOsdep::mgmtRecv(void* cookie, void* data, unsigned long len, std::function<unsigned(unsigned, const std::string&, const std::string&, std::string&)> cb)
{
	if (cookie != (void*)eodMgmtFd) {
		return false;
	}
	mgmtrd.append((char*)data, len);
	while (1) {
		auto req = (zt_eod_mgmt_req*)mgmtrd.data();
		if (mgmtrd.size() < sizeof(*req)) {
			break;
		}
		unsigned reqsz = sizeof(*req) + req->pathlen + req->datalen;
		if (mgmtrd.size() < reqsz) {
			break;
		}

		std::string resp;
		char* p = (char*)req->data;
		zt_eod_mgmt_reply rep;
		rep.scode = cb(req->method, std::string(p, p + req->pathlen), std::string(p + req->pathlen, p + req->pathlen + req->datalen), resp);
		rep.datalen = resp.size();

		mgmtrd.erase(mgmtrd.begin(), mgmtrd.begin() + reqsz);

		mgmtwr.append((char*)&rep, sizeof(rep));
		mgmtwr.append(resp);

		auto sz = write(eodMgmtFd, mgmtwr.data(), mgmtwr.size());
		if (sz > 0) {
			mgmtwr.erase(mgmtwr.begin(), mgmtwr.begin() + sz);
		}
	}
	return ! mgmtwr.empty();
}

void ExtOsdep::routeAddDel(bool add, const InetAddress& target, const InetAddress& via, const InetAddress& src, const char* ifname)
{
	Mutex::Lock l(eodMutex);

	std::string ifn;
	if (ifname) {
		ifn = ifname;
	}
	if (add) {
		for (auto x = allRoutes.begin(); x != allRoutes.end(); ++x) {
			if (x->target == target && x->via == via && x->src == src && x->ifname == ifn) {
				return;
			}
		}
		allRoutes.push_back({ target, via, src, ifn });
	}
	else {
		bool found = false;
		for (auto x = allRoutes.begin(); x != allRoutes.end(); ++x) {
			if (x->target == target && x->via == via && x->src == src && x->ifname == ifn) {
				allRoutes.erase(x);
				found = true;
				break;
			}
		}
		if (! found) {
			return;
		}
	}

	zt_eod_msg_route req;
	memset(&req, 0, sizeof(req));
	req.cmd = add ? ZT_EOD_MSG_ADDROUTE : ZT_EOD_MSG_DELROUTE;
	req.afi = target.isV4() ? 1 : 2;
	req.dstlen = target.netmaskBits();
	memcpy(req.dst, target.rawIpData(), target.isV4() ? 4 : 16);
	if (ifname) {
		strncpyx(req.dev, ifname, sizeof(req.dev));
	}
	if (via) {
		memcpy(req.gw, via.rawIpData(), target.isV4() ? 4 : 16);
	}
	if (src) {
		memcpy(req.src, src.rawIpData(), target.isV4() ? 4 : 16);
	}

	unsigned char resp;
	__eodXchg(req, add ? ZT_EOD_MSG_ADDROUTERESP : ZT_EOD_MSG_DELROUTERESP, resp);
}

bool ExtOsdep::getBindAddrs(std::map<InetAddress, std::string>& ret)
{
	Mutex::Lock l(eodMutex);

	unsigned char req = ZT_EOD_MSG_GETBINDADDRS;
	__eodSend(req);

	zt_eod_msg_getbindaddrsresp* resp;
	unsigned char buf[ZT_EOD_MAXMSGSIZE];
	int r = __eodWait(ZT_EOD_MSG_GETBINDADDRSRESP, (unsigned char*)buf, sizeof(*resp), sizeof(buf));
	if (r < (int)sizeof(*resp)) {
		return false;
	}

	int c = (r - (int)sizeof(*resp)) / sizeof(resp->addrs[0]);
	resp = (zt_eod_msg_getbindaddrsresp*)buf;
	for (int i = 0; i < c; ++i) {
		ret[InetAddress(resp->addrs[i].data, resp->addrs[i].afi == 1 ? 4 : 16, resp->addrs[i].len)] = resp->addrs[i].ifname;
	}

	return resp->result;
}

ExtOsdepTap::ExtOsdepTap(
	const char* homePath,
	const MAC& mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char* friendlyName,
	void (*handler)(void*, void*, uint64_t, const MAC&, const MAC&, unsigned int, unsigned int, const void*, unsigned int),
	void* arg)
	: _handler(handler)
	, _arg(arg)
	, _nwid(nwid)
	, _mac(mac)
	, _homePath(homePath)
	, _mtu(mtu)
	, _fd(0)
	, _enabled(true)
	, _run(true)
{
	zt_eod_msg_addtap req;
	req.cmd = ZT_EOD_MSG_ADDTAP;
	req.nwid = nwid;
	req.mtu = mtu;
	req.metric = metric;
	strncpyx(req.fname, friendlyName, sizeof(req.fname));
	mac.copyTo(req.mac, 6);

	zt_eod_msg_addtapresp resp;

	Mutex::Lock l(eodMutex);
	__eodSend(req);
	_fd = -1;
	if (__eodWait(ZT_EOD_MSG_ADDTAPRESP, (unsigned char*)&resp, sizeof(resp), sizeof(resp), &_fd) != sizeof(resp)) {
		throw std::runtime_error(std::string("could not create TAP"));
	}

	_dev = resp.name;
	if (_dev.empty() || _fd < 0) {
		throw std::runtime_error(std::string("could not create TAP"));
	}

	fcntl(_fd, F_SETFL, O_NONBLOCK);

	(void)::pipe(_shutdownSignalPipe);
	for (unsigned int t = 0; t < 2; ++t) {
		_tapReaderThread[t] = std::thread([this, t] {
			fd_set readfds, nullfds;
			int n, nfds, r;
			void* buf = nullptr;
			std::vector<void*> buffers;

			if (! _run) {
				return;
			}

			FD_ZERO(&readfds);
			FD_ZERO(&nullfds);
			nfds = (int)std::max(_shutdownSignalPipe[0], _fd) + 1;

			r = 0;
			for (;;) {
				FD_SET(_shutdownSignalPipe[0], &readfds);
				FD_SET(_fd, &readfds);
				select(nfds, &readfds, &nullfds, &nullfds, (struct timeval*)0);

				if (FD_ISSET(_shutdownSignalPipe[0], &readfds)) {	// writes to shutdown pipe terminate thread
					break;
				}

				if (FD_ISSET(_fd, &readfds)) {
					for (;;) {	 // read until there are no more packets, then return to outer select() loop
						if (! buf) {
							// To reduce use of the mutex, we keep a local buffer vector and
							// swap (which is a pointer swap) with the global one when it's
							// empty. This retrieves a batch of buffers to use.
							if (buffers.empty()) {
								std::lock_guard<std::mutex> l(_buffers_l);
								buffers.swap(_buffers);
							}
							if (buffers.empty()) {
								buf = malloc(ZT_TAP_BUF_SIZE);
								if (! buf) {
									break;
								}
							}
							else {
								buf = buffers.back();
								buffers.pop_back();
							}
						}

						n = (int)::read(_fd, reinterpret_cast<uint8_t*>(buf) + r, ZT_TAP_BUF_SIZE - r);
						if (n > 0) {
							// Some tap drivers like to send the ethernet frame and the
							// payload in two chunks, so handle that by accumulating
							// data until we have at least a frame.
							r += n;
							if (r > 14) {
								if (r > ((int)_mtu + 14)) {	  // sanity check for weird TAP behavior on some platforms
									r = _mtu + 14;
								}

								if (_enabled && _tapqsize.load() < 1000) {
									++_tapqsize;
									_tapq.post(std::pair<void*, int>(buf, r));
									buf = nullptr;
								}

								r = 0;
							}
						}
						else {
							r = 0;
							break;
						}
					}
				}
			}
		});
	}

	_tapProcessorThread = std::thread([this] {
		MAC to, from;
		std::pair<void*, int> qi;
		while (_tapq.get(qi)) {
			--_tapqsize;
			uint8_t* const b = reinterpret_cast<uint8_t*>(qi.first);
			if (b) {
				to.setTo(b, 6);
				from.setTo(b + 6, 6);
				unsigned int etherType = Utils::ntoh(((const uint16_t*)b)[6]);
				_handler(_arg, nullptr, _nwid, from, to, etherType, 0, (const void*)(b + 14), (unsigned int)(qi.second - 14));
				{
					std::lock_guard<std::mutex> l(_buffers_l);
					if (_buffers.size() < 128) {
						_buffers.push_back(qi.first);
					}
					else {
						free(qi.first);
					}
				}
			}
			else {
				break;
			}
		}
	});
}

ExtOsdepTap::~ExtOsdepTap()
{
	_run = false;

	(void)::write(_shutdownSignalPipe[1], "\0", 1);	  // causes reader thread(s) to exit
	_tapq.post(std::pair<void*, int>(nullptr, 0));	  // causes processor thread to exit

	_tapReaderThread[0].join();
	_tapReaderThread[1].join();
	_tapProcessorThread.join();

	::close(_fd);
	::close(_shutdownSignalPipe[0]);
	::close(_shutdownSignalPipe[1]);

	for (std::vector<void*>::iterator i(_buffers.begin()); i != _buffers.end(); ++i) {
		free(*i);
	}
	std::vector<std::pair<void*, int> > dv(_tapq.drain());
	for (std::vector<std::pair<void*, int> >::iterator i(dv.begin()); i != dv.end(); ++i) {
		if (i->first) {
			free(i->first);
		}
	}

	zt_eod_msg_deltap req;
	req.cmd = ZT_EOD_MSG_DELTAP;
	strcpy(req.name, _dev.c_str());

	unsigned char resp;
	eodXchg(req, ZT_EOD_MSG_DELTAPRESP, resp);
}

void ExtOsdepTap::setEnabled(bool en)
{
	_enabled = en;
}

bool ExtOsdepTap::enabled() const
{
	return _enabled;
}

void ExtOsdepTap::doRemoveIp(const InetAddress& ip)
{
	zt_eod_msg_ip req;
	req.cmd = ZT_EOD_MSG_DELIP;
	strcpy(req.name, _dev.c_str());
	req.afi = ip.isV4() ? 1 : 2;
	req.len = ip.netmaskBits();
	memcpy(req.data, ip.rawIpData(), ip.isV4() ? 4 : 16);

	unsigned char resp;
	__eodXchg(req, ZT_EOD_MSG_DELIPRESP, resp);
}

bool ExtOsdepTap::addIp(const InetAddress& ip)
{
	Mutex::Lock l(eodMutex);

	for (auto i = allIps.begin(); i != allIps.end(); ++i) {
		if (*i == ip) {
			return true;
		}
		if (i->ipsEqual(ip)) {
			doRemoveIp(*i);
		}
	}

	zt_eod_msg_ip req;
	req.cmd = ZT_EOD_MSG_ADDIP;
	strcpy(req.name, _dev.c_str());
	req.afi = ip.isV4() ? 1 : 2;
	req.len = ip.netmaskBits();
	memcpy(req.data, ip.rawIpData(), ip.isV4() ? 4 : 16);

	unsigned char resp;
	__eodXchg(req, ZT_EOD_MSG_ADDIPRESP, resp);

	allIps.push_back(ip);

	return true;
}
bool ExtOsdepTap::addIps(std::vector<InetAddress> ips)
{
	return false;
}

bool ExtOsdepTap::removeIp(const InetAddress& ip)
{
	Mutex::Lock l(eodMutex);
	for (auto i = allIps.begin(); i != allIps.end(); ++i) {
		if (*i == ip) {
			doRemoveIp(*i);
			return true;
		}
	}
	return false;
}
std::vector<InetAddress> ExtOsdepTap::ips() const
{
	std::vector<InetAddress> ret;

	Mutex::Lock l(eodMutex);

	zt_eod_msg_getips req;
	req.cmd = ZT_EOD_MSG_GETIPS;
	strcpy(req.name, _dev.c_str());
	__eodSend(req);

	zt_eod_msg_getipsresp* resp;
	unsigned char buf[ZT_EOD_MAXMSGSIZE];
	int r = __eodWait(ZT_EOD_MSG_GETIPSRESP, (unsigned char*)buf, sizeof(*resp), sizeof(buf));
	if (r < (int)sizeof(*resp)) {
		return ret;
	}

	int c = (r - (int)sizeof(*resp)) / sizeof(resp->addrs[0]);
	resp = (zt_eod_msg_getipsresp*)buf;
	for (int i = 0; i < c; ++i) {
		ret.push_back(InetAddress(resp->addrs[i].data, resp->addrs[i].afi == 1 ? 4 : 16, resp->addrs[i].len));
	}

	return ret;
}
void ExtOsdepTap::put(const MAC& from, const MAC& to, unsigned int etherType, const void* data, unsigned int len)
{
	char putBuf[ZT_MAX_MTU + 64];
	if ((_fd > 0) && (len <= _mtu) && (_enabled)) {
		to.copyTo(putBuf, 6);
		from.copyTo(putBuf + 6, 6);
		*((uint16_t*)(putBuf + 12)) = htons((uint16_t)etherType);
		memcpy(putBuf + 14, data, len);
		len += 14;
		(void)::write(_fd, putBuf, len);
	}
}
std::string ExtOsdepTap::deviceName() const
{
	return _dev;
}
void ExtOsdepTap::setFriendlyName(const char* friendlyName)
{
}

void ExtOsdepTap::scanMulticastGroups(std::vector<MulticastGroup>& added, std::vector<MulticastGroup>& removed)
{
	char *ptr, *ptr2;
	unsigned char mac[6];
	std::vector<MulticastGroup> newGroups;

	int fd = ::open("/proc/net/dev_mcast", O_RDONLY);
	if (fd > 0) {
		char buf[131072];
		int n = (int)::read(fd, buf, sizeof(buf));
		if ((n > 0) && (n < (int)sizeof(buf))) {
			buf[n] = (char)0;
			for (char* l = strtok_r(buf, "\r\n", &ptr); (l); l = strtok_r((char*)0, "\r\n", &ptr)) {
				int fno = 0;
				char* devname = (char*)0;
				char* mcastmac = (char*)0;
				for (char* f = strtok_r(l, " \t", &ptr2); (f); f = strtok_r((char*)0, " \t", &ptr2)) {
					if (fno == 1) {
						devname = f;
					}
					else if (fno == 4) {
						mcastmac = f;
					}
					++fno;
				}
				if ((devname) && (! strcmp(devname, _dev.c_str())) && (mcastmac) && (Utils::unhex(mcastmac, mac, 6) == 6)) {
					newGroups.push_back(MulticastGroup(MAC(mac, 6), 0));
				}
			}
		}
		::close(fd);
	}

	std::vector<InetAddress> allIps(ips());
	for (std::vector<InetAddress>::iterator ip(allIps.begin()); ip != allIps.end(); ++ip) {
		newGroups.push_back(MulticastGroup::deriveMulticastGroupForAddressResolution(*ip));
	}

	std::sort(newGroups.begin(), newGroups.end());
	newGroups.erase(std::unique(newGroups.begin(), newGroups.end()), newGroups.end());

	for (std::vector<MulticastGroup>::iterator m(newGroups.begin()); m != newGroups.end(); ++m) {
		if (! std::binary_search(_multicastGroups.begin(), _multicastGroups.end(), *m)) {
			added.push_back(*m);
		}
	}
	for (std::vector<MulticastGroup>::iterator m(_multicastGroups.begin()); m != _multicastGroups.end(); ++m) {
		if (! std::binary_search(newGroups.begin(), newGroups.end(), *m)) {
			removed.push_back(*m);
		}
	}

	_multicastGroups.swap(newGroups);
}
void ExtOsdepTap::setMtu(unsigned int mtu)
{
	if (mtu == _mtu) {
		return;
	}
	_mtu = mtu;

	zt_eod_msg_setmtu req;
	req.cmd = ZT_EOD_MSG_SETMTU;
	strcpy(req.name, _dev.c_str());
	req.mtu = mtu;

	unsigned char resp;
	eodXchg(req, ZT_EOD_MSG_SETMTURESP, resp);
}

}	// namespace ZeroTier
