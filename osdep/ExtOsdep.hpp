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

#ifndef ZT_EXTOSDEP_HPP
#define ZT_EXTOSDEP_HPP

#ifdef ZT_EXTOSDEP

#include <stdint.h>

#define ZT_EOD_MAXMSGSIZE (64 * 1024)

#define ZT_EOD_MSG_STARTED			1	// no data
#define ZT_EOD_MSG_ADDTAP			2
#define ZT_EOD_MSG_ADDTAPRESP		3
#define ZT_EOD_MSG_DELTAP			4
#define ZT_EOD_MSG_DELTAPRESP		5	// no data
#define ZT_EOD_MSG_SETMTU			6
#define ZT_EOD_MSG_SETMTURESP		7
#define ZT_EOD_MSG_ADDIP			8
#define ZT_EOD_MSG_ADDIPRESP		9
#define ZT_EOD_MSG_DELIP			10
#define ZT_EOD_MSG_DELIPRESP		11
#define ZT_EOD_MSG_GETIPS			12
#define ZT_EOD_MSG_GETIPSRESP		13
#define ZT_EOD_MSG_GETBINDADDRS		14
#define ZT_EOD_MSG_GETBINDADDRSRESP 15
#define ZT_EOD_MSG_ADDROUTE			16
#define ZT_EOD_MSG_ADDROUTERESP		17
#define ZT_EOD_MSG_DELROUTE			18
#define ZT_EOD_MSG_DELROUTERESP		19

struct zt_eod_msg_addtap {
	unsigned char cmd;
	uint64_t nwid;
	uint32_t mtu;
	uint32_t metric;
	char fname[128];
	unsigned char mac[6];
} __attribute__((packed));

struct zt_eod_msg_addtapresp {
	unsigned char cmd;
	char name[16];
} __attribute__((packed));

struct zt_eod_msg_deltap {
	unsigned char cmd;
	char name[16];
} __attribute__((packed));

struct zt_eod_msg_setmtu {
	unsigned char cmd;
	char name[16];
	unsigned mtu;
} __attribute__((packed));

struct zt_eod_msg_ip {
	unsigned char cmd;
	char name[16];
	unsigned char afi;	 // 1 ip, 2 ip6
	unsigned char len;	 // bits in mask
	unsigned char data[16];
} __attribute__((packed));

struct zt_eod_msg_getips {
	unsigned char cmd;
	char name[16];
} __attribute__((packed));

struct zt_eod_msg_getipsresp {
	unsigned char cmd;
	struct addr {
		unsigned char afi;
		unsigned char len;
		unsigned char data[16];
	} __attribute__((packed)) addrs[0];
} __attribute__((packed));
#define ZT_EOD_GETIPSRESP_MAXADDRS ((ZT_EOD_MAXMSGSIZE - sizeof(zt_eod_msg_getipsresp)) / sizeof(zt_eod_msg_getipsresp::addr))

struct zt_eod_msg_getbindaddrsresp {
	unsigned char cmd;
	unsigned char result;
	struct addr {
		unsigned char afi;
		unsigned char len;
		unsigned char data[16];
		char ifname[16];
	} __attribute__((packed)) addrs[0];
} __attribute__((packed));
#define ZT_EOD_GETBINDADDRSRESP_MAXADDRS ((ZT_EOD_MAXMSGSIZE - sizeof(zt_eod_msg_getbindaddrsresp)) / sizeof(zt_eod_msg_getbindaddrsresp::addr))

struct zt_eod_msg_route {
	unsigned char cmd;
	unsigned char afi;	 // 1 ip, 2 ip6
	unsigned char dstlen;
	unsigned char dst[16];
	unsigned char gw[16];
	char dev[16];
	unsigned char src[16];
} __attribute__((packed));

struct zt_eod_mgmt_req {
	uint32_t method;
	uint32_t pathlen;
	uint32_t datalen;
	unsigned char data[0];
} __attribute__((packed));

struct zt_eod_mgmt_reply {
	uint32_t scode;
	uint32_t datalen;
	unsigned char data[0];
} __attribute__((packed));

#ifndef ZT_EXTOSDEP_IFACEONLY

#include "../node/AtomicCounter.hpp"
#include "../node/Hashtable.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"
#include "../node/Mutex.hpp"
#include "BlockingQueue.hpp"
#include "EthernetTap.hpp"
#include "Thread.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ZeroTier {

class ExtOsdep {
  public:
	static void init(int, int);
	static void started(int*, void**);

	static void routeAddDel(bool, const InetAddress& target, const InetAddress& via, const InetAddress& src, const char* ifaceName);
	static bool getBindAddrs(std::map<InetAddress, std::string>&);

	static bool mgmtRecv(void* cookie, void* data, unsigned long len, std::function<unsigned(unsigned, const std::string&, const std::string&, std::string&)>);
	static bool mgmtWritable(void*);
};

class ExtOsdepTap : public EthernetTap {
  public:
	ExtOsdepTap(
		const char* homePath,
		const MAC& mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char* friendlyName,
		void (*handler)(void*, void*, uint64_t, const MAC&, const MAC&, unsigned int, unsigned int, const void*, unsigned int),
		void* arg);

	virtual ~ExtOsdepTap();

	virtual void setEnabled(bool en);
	virtual bool enabled() const;
	virtual bool addIp(const InetAddress& ip);
	virtual bool addIps(std::vector<InetAddress> ips);
	virtual bool removeIp(const InetAddress& ip);
	virtual std::vector<InetAddress> ips() const;
	virtual void put(const MAC& from, const MAC& to, unsigned int etherType, const void* data, unsigned int len);
	virtual std::string deviceName() const;
	virtual void setFriendlyName(const char* friendlyName);
	virtual void scanMulticastGroups(std::vector<MulticastGroup>& added, std::vector<MulticastGroup>& removed);
	virtual void setMtu(unsigned int mtu);
	virtual void setDns(const char* domain, const std::vector<InetAddress>& servers)
	{
	}

  private:
	void (*_handler)(void*, void*, uint64_t, const MAC&, const MAC&, unsigned int, unsigned int, const void*, unsigned int);
	void* _arg;
	uint64_t _nwid;
	MAC _mac;
	std::string _homePath;
	std::string _dev;
	std::vector<MulticastGroup> _multicastGroups;
	unsigned int _mtu;
	int _fd;
	int _shutdownSignalPipe[2];
	std::atomic_bool _enabled;
	std::atomic_bool _run;
	std::thread _tapReaderThread[2];
	std::thread _tapProcessorThread;
	std::mutex _buffers_l;
	std::vector<void*> _buffers;
	BlockingQueue<std::pair<void*, int> > _tapq;
	AtomicCounter _tapqsize;

	std::vector<InetAddress> allIps;
	void doRemoveIp(const InetAddress&);
};

}	// namespace ZeroTier

#endif	 // ZT_EXTOSDEP_IFACEONLY
#endif	 // ZT_EXTOSDEP

#endif
