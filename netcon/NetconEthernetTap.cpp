/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#include <algorithm>
#include <utility>
#include <dlfcn.h>
#include <sys/poll.h>
#include <stdint.h>
#include <utility>
#include <string>
#include <sys/resource.h>

#include "NetconEthernetTap.hpp"

#include "../node/Utils.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/Phy.hpp"

#include "Intercept.h"
#include "LWIPStack.hpp"

#include "lwip/tcp_impl.h"
#include "netif/etharp.h"
#include "lwip/api.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/ip_frag.h"
#include "lwip/tcp.h"

#include "common.inc.c"
#include "RPC.h"

#define APPLICATION_POLL_FREQ           50
#define ZT_LWIP_TCP_TIMER_INTERVAL      5
#define STATUS_TMR_INTERVAL             1000 // How often we check connection statuses (in ms)
#define DEFAULT_BUF_SZ                  1024 * 1024 * 2


namespace ZeroTier {

// ---------------------------------------------------------------------------

static err_t tapif_init(struct netif *netif)
{
  // Actual init functionality is in addIp() of tap
  return ERR_OK;
}

/*
 * Outputs data from the pbuf queue to the interface
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  struct pbuf *q;
  char buf[ZT_MAX_MTU+32];
  char *bufptr;
  int totalLength = 0;

  ZeroTier::NetconEthernetTap *tap = (ZeroTier::NetconEthernetTap*)netif->state;
  bufptr = buf;
  // Copy data from each pbuf, one at a time
  for(q = p; q != NULL; q = q->next) {
    memcpy(bufptr, q->payload, q->len);
    bufptr += q->len;
    totalLength += q->len;
  }

  // [Send packet to network]
  // Split ethernet header and feed into handler
  struct eth_hdr *ethhdr;
  ethhdr = (struct eth_hdr *)buf;

  ZeroTier::MAC src_mac;
  ZeroTier::MAC dest_mac;
  src_mac.setTo(ethhdr->src.addr, 6);
  dest_mac.setTo(ethhdr->dest.addr, 6);

  tap->_handler(tap->_arg,tap->_nwid,src_mac,dest_mac,
    Utils::ntoh((uint16_t)ethhdr->type),0,buf + sizeof(struct eth_hdr),totalLength - sizeof(struct eth_hdr));
  return ERR_OK;
}

/*
 * TCP connection administered by service
 */
class TcpConnection
{
public:
  bool listening;
  int pid, txsz, rxsz;
  PhySocket *rpcSock, *sock;
  struct tcp_pcb *pcb;
  struct sockaddr_storage *addr;
  unsigned char txbuf[DEFAULT_BUF_SZ];
  unsigned char rxbuf[DEFAULT_BUF_SZ];
};

/*
 * A helper class for passing a reference to _phy to LWIP callbacks as a "state"
 */
class Larg
{
public:
  NetconEthernetTap *tap;
  TcpConnection *conn;
  Larg(NetconEthernetTap *_tap, TcpConnection *conn) : tap(_tap), conn(conn) {}
};

// ---------------------------------------------------------------------------

NetconEthernetTap::NetconEthernetTap(
	const char *homePath,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *friendlyName,
	void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
	void *arg) :
  _nwid(nwid),
	_handler(handler),
	_arg(arg),
  _phy(this,false,true),
  _unixListenSocket((PhySocket *)0),
	_mac(mac),
	_homePath(homePath),
	_mtu(mtu),
	_enabled(true),
	_run(true)
{
	char sockPath[4096],lwipPath[4096];
	rpcCounter = -1;
	Utils::snprintf(sockPath,sizeof(sockPath),"%s%snc_%.16llx",homePath,ZT_PATH_SEPARATOR_S,_nwid,ZT_PATH_SEPARATOR_S,(unsigned long long)nwid);
    _dev = sockPath; // in netcon mode, set device to be just the network ID

	Utils::snprintf(lwipPath,sizeof(lwipPath),"%s%sliblwip.so",homePath,ZT_PATH_SEPARATOR_S);
	lwipstack = new LWIPStack(lwipPath);
	if(!lwipstack)
		throw std::runtime_error("unable to dynamically load a new instance of liblwip.so (searched ZeroTier home path)");
	lwipstack->lwip_init();

	_unixListenSocket = _phy.unixListen(sockPath,(void *)this);
	dwr(MSG_INFO," NetconEthernetTap initialized!\n", _phy.getDescriptor(_unixListenSocket));
	if (!_unixListenSocket)
		throw std::runtime_error(std::string("unable to bind to ")+sockPath);
	_thread = Thread::start(this);
}

NetconEthernetTap::~NetconEthernetTap()
{
	_run = false;
	_phy.whack();
	_phy.whack(); // TODO: Rationale?
	Thread::join(_thread);
	_phy.close(_unixListenSocket,false);
	delete lwipstack;
}

void NetconEthernetTap::setEnabled(bool en)
{
	_enabled = en;
}

bool NetconEthernetTap::enabled() const
{
	return _enabled;
}

bool NetconEthernetTap::addIp(const InetAddress &ip)
{
	Mutex::Lock _l(_ips_m);
	if (std::find(_ips.begin(),_ips.end(),ip) == _ips.end()) {
		_ips.push_back(ip);
		std::sort(_ips.begin(),_ips.end());

		if (ip.isV4()) {
			// Set IP
			static ip_addr_t ipaddr, netmask, gw;
			IP4_ADDR(&gw,192,168,0,1);
			ipaddr.addr = *((u32_t *)ip.rawIpData());
			netmask.addr = *((u32_t *)ip.netmask().rawIpData());

			// Set up the lwip-netif for LWIP's sake
			lwipstack->netif_add(&interface,&ipaddr, &netmask, &gw, NULL, tapif_init, lwipstack->_ethernet_input);
			interface.state = this;
			interface.output = lwipstack->_etharp_output;
			_mac.copyTo(interface.hwaddr, 6);
			interface.mtu = _mtu;
			interface.name[0] = 't';
			interface.name[1] = 'p';
			interface.linkoutput = low_level_output;
			interface.hwaddr_len = 6;
			interface.flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
			lwipstack->netif_set_default(&interface);
			lwipstack->netif_set_up(&interface);
		}
	}
	return true;
}

bool NetconEthernetTap::removeIp(const InetAddress &ip)
{
	Mutex::Lock _l(_ips_m);
	std::vector<InetAddress>::iterator i(std::find(_ips.begin(),_ips.end(),ip));
	if (i == _ips.end())
		return false;
	_ips.erase(i);
	if (ip.isV4()) {
		// TODO: dealloc from LWIP
	}
	return true;
}

std::vector<InetAddress> NetconEthernetTap::ips() const
{
	Mutex::Lock _l(_ips_m);
	return _ips;
}

void NetconEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	struct pbuf *p,*q;
	if (!_enabled)
		return;

	struct eth_hdr ethhdr;
	from.copyTo(ethhdr.src.addr, 6);
	to.copyTo(ethhdr.dest.addr, 6);
	ethhdr.type = Utils::hton((uint16_t)etherType);

	// We allocate a pbuf chain of pbufs from the pool.
	p = lwipstack->pbuf_alloc(PBUF_RAW, len+sizeof(struct eth_hdr), PBUF_POOL);

	if (p != NULL) {
		const char *dataptr = reinterpret_cast<const char *>(data);

		// First pbuf gets ethernet header at start
		q = p;
		if (q->len < sizeof(ethhdr)) {
			dwr(MSG_ERROR,"_put(): Dropped packet: first pbuf smaller than ethernet header\n");
			return;
		}
		memcpy(q->payload,&ethhdr,sizeof(ethhdr));
		memcpy((char*)q->payload + sizeof(ethhdr),dataptr,q->len - sizeof(ethhdr));
		dataptr += q->len - sizeof(ethhdr);

		// Remaining pbufs (if any) get rest of data
		while ((q = q->next)) {
			memcpy(q->payload,dataptr,q->len);
			dataptr += q->len;
		}
	} else {
		dwr(MSG_ERROR,"put(): Dropped packet: no pbufs available\n");
		return;
	}

	{
		Mutex::Lock _l2(lwipstack->_lock);
		if(interface.input(p, &interface) != ERR_OK) {
			dwr(MSG_ERROR,"put(): Error while RXing packet (netif->input)\n");
		}
	}
}

std::string NetconEthernetTap::deviceName() const
{
	return _dev;
}

void NetconEthernetTap::setFriendlyName(const char *friendlyName) {
}

void NetconEthernetTap::scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed)
{
	std::vector<MulticastGroup> newGroups;
	Mutex::Lock _l(_multicastGroups_m);

	// TODO: get multicast subscriptions from LWIP

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

void NetconEthernetTap::threadMain()
	throw()
{
	uint64_t prev_tcp_time = 0, prev_status_time = 0, prev_etharp_time = 0;

	// Main timer loop
	while (_run) {
		uint64_t now = OSUtils::now();
		uint64_t since_tcp = now - prev_tcp_time;
		uint64_t since_etharp = now - prev_etharp_time;
		uint64_t since_status = now - prev_status_time;
		uint64_t tcp_remaining = ZT_LWIP_TCP_TIMER_INTERVAL;
		uint64_t etharp_remaining = ARP_TMR_INTERVAL;
		uint64_t status_remaining = STATUS_TMR_INTERVAL;

		// Connection prunning
		if (since_status >= STATUS_TMR_INTERVAL) {
			prev_status_time = now;
			status_remaining = STATUS_TMR_INTERVAL - since_status;


			for(size_t i=0;i<_TcpConnections.size();++i) {
				if(!_TcpConnections[i]->sock)
					continue; // Skip, this is a pending connection
				int fd = _phy.getDescriptor(_TcpConnections[i]->sock);
				dwr(MSG_DEBUG," tap_thread(): tcp\\jobs = {%d, %d}\n", _TcpConnections.size(), jobmap.size());

				fcntl(fd, F_SETFL, O_NONBLOCK);
				unsigned char tmpbuf[BUF_SZ];
				
				int n = read(fd,&tmpbuf,BUF_SZ);
				if(_TcpConnections[i]->pcb->state == SYN_SENT) {
					dwr(MSG_DEBUG_EXTRA,"  tap_thread(): <%x> state = SYN_SENT, should finish or be removed soon\n", _TcpConnections[i]->sock);
				}
				if((n < 0 && errno != EAGAIN) || (n == 0 && errno == EAGAIN)) {
					dwr(MSG_DEBUG," tap_thread(): closing sock (%x)\n", _TcpConnections[i]->sock);
					closeConnection(_TcpConnections[i]->sock);
				} else if (n > 0) {
					dwr(MSG_DEBUG," tap_thread(): data read during connection check (%d bytes)\n", n);
					phyOnUnixData(_TcpConnections[i]->sock,_phy.getuptr(_TcpConnections[i]->sock),&tmpbuf,BUF_SZ);
				}				
			}
		}
		// Main TCP/ETHARP timer section
		if (since_tcp >= ZT_LWIP_TCP_TIMER_INTERVAL) {
			prev_tcp_time = now;
			lwipstack->tcp_tmr();
			// Makeshift poll
			for(size_t i=0;i<_TcpConnections.size();++i) {
				if(_TcpConnections[i]->txsz > 0){
					lwipstack->_lock.lock();
					handleWrite(_TcpConnections[i]);
					lwipstack->_lock.unlock();
				}
			}
		} else {
			tcp_remaining = ZT_LWIP_TCP_TIMER_INTERVAL - since_tcp;
		}
		if (since_etharp >= ARP_TMR_INTERVAL) {
			prev_etharp_time = now;
			lwipstack->etharp_tmr();
		} else {
			etharp_remaining = ARP_TMR_INTERVAL - since_etharp;
		}
		_phy.poll((unsigned long)std::min(tcp_remaining,etharp_remaining));
	}
	dlclose(lwipstack->_libref);
}

// Unused -- no UDP or TCP from this thread/Phy<>
void NetconEthernetTap::phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len) {}
void NetconEthernetTap::phyOnTcpConnect(PhySocket *sock,void **uptr,bool success) {}
void NetconEthernetTap::phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from) {}
void NetconEthernetTap::phyOnTcpClose(PhySocket *sock,void **uptr) {}
void NetconEthernetTap::phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len) {}
void NetconEthernetTap::phyOnTcpWritable(PhySocket *sock,void **uptr) {}


TcpConnection *NetconEthernetTap::getConnection(PhySocket *sock)
{
	Mutex::Lock _l(_tcpconns_m);
	for(size_t i=0;i<_TcpConnections.size();++i) {
		if(_TcpConnections[i]->sock == sock)
			return _TcpConnections[i];
	}
	return NULL;
}

TcpConnection *NetconEthernetTap::addConnection(TcpConnection *conn)
{
	Mutex::Lock _l(_tcpconns_m);
	_TcpConnections.push_back(conn);
	return conn;
}

void NetconEthernetTap::removeConnection(TcpConnection *conn)
{
	Mutex::Lock _l(_tcpconns_m);
	for(size_t i=0;i<_TcpConnections.size();++i) {
		if(_TcpConnections[i] == conn){
			_TcpConnections.erase(_TcpConnections.begin() + i);
			return;
		}
	}
}

void NetconEthernetTap::closeConnection(PhySocket *sock)
{
	dwr(MSG_DEBUG,"closeConnection(%x)\n",sock);
	if(!sock) {
		dwr(MSG_DEBUG," closeConnection(): invalid PhySocket\n");
		return;
	}
	TcpConnection *conn = getConnection(sock);
	if(!conn)
		return;
	else
		removeConnection(conn);
	if(!conn->pcb)
		return;
	if(conn->pcb->state == SYN_SENT || conn->pcb->state == CLOSE_WAIT) {
		dwr(MSG_DEBUG," closeConnection(): invalid PCB state for this operation. ignoring.\n");
		return;
	}	
	dwr(MSG_DEBUG," closeConnection(): PCB->state = %d\n", conn->pcb->state);
	if(lwipstack->_tcp_close(conn->pcb) != ERR_OK) {
		dwr(MSG_ERROR," closeConnection(): error while calling tcp_close()\n");
	}
	if(!sock)
		return;
	close(_phy.getDescriptor(sock));
	_phy.close(sock, false);
}

void NetconEthernetTap::phyOnUnixClose(PhySocket *sock,void **uptr) {
	closeConnection(sock);
}

void NetconEthernetTap::phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable) {
	// Currently unused since phyOnUnixData() handles everything now
}

void NetconEthernetTap::phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN) {
	dwr(MSG_DEBUG,"\nphyOnUnixAccept(): new connection = %x\n", sockN);
}

void NetconEthernetTap::phyOnUnixWritable(PhySocket *sock,void **uptr)
{
	TcpConnection *conn = getConnection(sock);
	Mutex::Lock _l(_rx_buf_m);
	int len = conn->rxsz;
	int n = _phy.streamSend(conn->sock, conn->rxbuf, len);
	if(n > 0) {
		if(n < len) {
		    dwr(MSG_ERROR,"\n phyOnUnixWritable(): unable to write entire \"block\" to stream\n");
		}
		if(len-n)
			memcpy(conn->rxbuf, conn->rxbuf+n, len-n);
	  	conn->rxsz -= n;
	  	float max = (float)DEFAULT_BUF_SZ;
		dwr(MSG_TRANSFER,"    <--- RX :: { TX: %.3f%%  |  RX: %.3f%% }  :: %d bytes\n", 
			(float)conn->txsz / max, (float)conn->rxsz / max, n);
	  	lwipstack->_tcp_recved(conn->pcb, n);
	  	if(conn->rxsz == 0){
	  		_phy.setNotifyWritable(conn->sock, false); // Nothing more to be notified about
	  	}
	} else {
		perror("\n");
		dwr(MSG_ERROR," phyOnUnixWritable(): errno = %d\n", errno);
	}
}

void NetconEthernetTap::phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len)
{		
	uint64_t CANARY_num;
	pid_t pid, tid;
	int rpcCount, wlen = len;
	char cmd, timestamp[20], CANARY[CANARY_SZ], padding[] = {PADDING};
	void *payload;
	unsigned char *buf = (unsigned char*)data;
	std::pair<PhySocket*, void*> sockdata;
	PhySocket *rpcSock;
	bool foundJob = false, detected_rpc = false;
	TcpConnection *conn;

	// RPC
	char phrase[RPC_PHRASE_SZ];
	memset(phrase, 0, RPC_PHRASE_SZ);
	if(len == BUF_SZ) {
		memcpy(phrase, buf, RPC_PHRASE_SZ);
		if(strcmp(phrase, RPC_PHRASE) == 0)
			detected_rpc = true;
	}
	if(detected_rpc) {
		unloadRPC(data, pid, tid, rpcCount, timestamp, CANARY, cmd, payload);
		memcpy(&CANARY_num, CANARY, CANARY_SZ);
		dwr(MSG_DEBUG," <%x> RPC: (pid=%d, tid=%d, rpcCount=%d, timestamp=%s, cmd=%d)\n", 
			sock, pid, tid, rpcCount, timestamp, cmd);

		if(cmd == RPC_SOCKET) {				
			dwr(MSG_DEBUG,"  <%x> RPC_SOCKET\n", sock);
			// Create new lwip socket and associate it with this sock
			struct socket_st socket_rpc;
			memcpy(&socket_rpc, &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct socket_st));
			TcpConnection * new_conn;
			if((new_conn = handleSocket(sock, uptr, &socket_rpc))) {
				new_conn->pid = pid; // Merely kept to look up application path/names later, not strictly necessary
			}
		} else {
			jobmap[CANARY_num] = std::make_pair<PhySocket*, void*>(sock, data);
		}
		write(_phy.getDescriptor(sock), "z", 1); // RPC ACK byte to maintain order
	}
	// STREAM
	else {
		int data_start = -1, data_end = -1, canary_pos = -1, padding_pos = -1;
		// Look for padding
		std::string padding_pattern(padding, padding+PADDING_SZ);
		std::string buffer(buf, buf + len);
		padding_pos = buffer.find(padding_pattern);
		canary_pos = padding_pos-CANARY_SZ;
		// Grab token, next we'll use it to look up an RPC job
		if(canary_pos > -1) {
			memcpy(&CANARY_num, buf+canary_pos, CANARY_SZ);
			if(CANARY_num != 0) {
				// Find job
				sockdata = jobmap[CANARY_num];
				if(!sockdata.first) {
					dwr(MSG_DEBUG," <%x> unable to locate job entry for %llu\n", sock, CANARY_num);
					return;
				}  else
					foundJob = true;
			}
		}

		conn = getConnection(sock);
		if(!conn)
			return;

		if(padding_pos == -1) { // [DATA]
			memcpy(&conn->txbuf[conn->txsz], buf, wlen);
		} else { // Padding found, implies a canary is present
			// [CANARY]
			if(len == CANARY_SZ+PADDING_SZ && canary_pos == 0) {
				wlen = 0; // Nothing to write
			} else {
				// [CANARY] + [DATA]
				if(len > CANARY_SZ+PADDING_SZ && canary_pos == 0) {
					wlen = len - CANARY_SZ+PADDING_SZ;
					data_start = padding_pos+PADDING_SZ;
					memcpy((&conn->txbuf)+conn->txsz, buf+data_start, wlen);
				}
				// [CANARY] + [TOKEN]
				if(len > CANARY_SZ+PADDING_SZ && canary_pos > 0 && canary_pos == len - CANARY_SZ+PADDING_SZ) {
					wlen = len - CANARY_SZ+PADDING_SZ;
					data_start = 0;
					memcpy((&conn->txbuf)+conn->txsz, buf+data_start, wlen);												
				}
				// [CANARY] + [TOKEN] + [DATA]
				if(len > CANARY_SZ+PADDING_SZ && canary_pos > 0 && len > (canary_pos + CANARY_SZ+PADDING_SZ)) {
					wlen = len - CANARY_SZ+PADDING_SZ;
					data_start = 0;
					data_end = padding_pos-CANARY_SZ;
					memcpy((&conn->txbuf)+conn->txsz, buf+data_start, (data_end-data_start)+1);
					memcpy((&conn->txbuf)+conn->txsz, buf+(padding_pos+PADDING_SZ), len-(canary_pos+CANARY_SZ+PADDING_SZ));
				}
			}
		}
		// Write data from stream
		if(conn->txsz > (DEFAULT_BUF_SZ / 2)) {
			_phy.setNotifyReadable(sock, false);
		}
		lwipstack->_lock.lock();
		conn->txsz += wlen;
		handleWrite(conn);
		lwipstack->_lock.unlock();
	}
	if(foundJob) {
		rpcSock = sockdata.first;
		buf = (unsigned char*)sockdata.second;
	}

	// Process RPC if we have a corresponding jobmap entry
	if(foundJob) {
		unloadRPC(buf, pid, tid, rpcCount, timestamp, CANARY, cmd, payload);
		dwr(MSG_DEBUG," <%x> RPC: (pid=%d, tid=%d, rpcCount=%d, timestamp=%s, cmd=%d)\n", 
			sock, pid, tid, rpcCount, timestamp, cmd);

		switch(cmd) {
			case RPC_BIND:
				dwr(MSG_DEBUG,"  <%x> RPC_BIND\n", sock);
			    struct bind_st bind_rpc;
			    memcpy(&bind_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct bind_st));
			    handleBind(sock, rpcSock, uptr, &bind_rpc);
				break;
		  	case RPC_LISTEN:
				dwr(MSG_DEBUG,"  <%x> RPC_LISTEN\n", sock);
			    struct listen_st listen_rpc;
			    memcpy(&listen_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct listen_st));
			    handleListen(sock, rpcSock, uptr, &listen_rpc);
				break;
		  	case RPC_GETSOCKNAME:
		  		dwr(MSG_DEBUG,"  <%x> RPC_GETSOCKNAME\n", sock);
		  		struct getsockname_st getsockname_rpc;
		    	memcpy(&getsockname_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct getsockname_st));
		  		handleGetsockname(sock, rpcSock, uptr, &getsockname_rpc);
		  		break;
			case RPC_CONNECT:
				dwr(MSG_DEBUG,"  <%x> RPC_CONNECT\n", sock);
			    struct connect_st connect_rpc;
			    memcpy(&connect_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct connect_st));
			    handleConnect(sock, rpcSock, conn, &connect_rpc);
			    jobmap.erase(CANARY_num);
				return; // Keep open RPC, we'll use it once in nc_connected to send retval
		  	default:
				break;
		}
		closeConnection(sockdata.first); // close RPC after sending retval, no longer needed
		jobmap.erase(CANARY_num);
		return;
	}
}

int NetconEthernetTap::sendReturnValue(PhySocket *sock, int retval, int _errno = 0){
	return sendReturnValue(_phy.getDescriptor(sock), retval, _errno);
}
int NetconEthernetTap::sendReturnValue(int fd, int retval, int _errno = 0)
{
	dwr(MSG_DEBUG," sendReturnValue(): fd = %d, retval = %d, errno = %d\n", fd, retval, _errno);
	int sz = sizeof(char) + sizeof(retval) + sizeof(errno);
	char retmsg[sz];
	memset(&retmsg, 0, sizeof(retmsg));
	retmsg[0]=RPC_RETVAL;
	memcpy(&retmsg[1], &retval, sizeof(retval));
	memcpy(&retmsg[1]+sizeof(retval), &_errno, sizeof(_errno));
	return write(fd, &retmsg, sz);
}

void NetconEthernetTap::unloadRPC(void *data, pid_t &pid, pid_t &tid, 
	int &rpcCount, char (timestamp[RPC_TIMESTAMP_SZ]), char (CANARY[sizeof(uint64_t)]), char &cmd, void* &payload)
{
	unsigned char *buf = (unsigned char*)data;
	memcpy(&pid, &buf[IDX_PID], sizeof(pid_t));
	memcpy(&tid, &buf[IDX_TID], sizeof(pid_t));
	memcpy(&rpcCount, &buf[IDX_COUNT], sizeof(int));
	memcpy(timestamp, &buf[IDX_TIME], RPC_TIMESTAMP_SZ);
	memcpy(&cmd, &buf[IDX_PAYLOAD], sizeof(char));
	memcpy(CANARY, &buf[IDX_PAYLOAD+1], CANARY_SZ);
}

/*------------------------------------------------------------------------------
--------------------------------- LWIP callbacks -------------------------------
------------------------------------------------------------------------------*/

err_t NetconEthernetTap::nc_accept(void *arg, struct tcp_pcb *newPCB, err_t err)
{
	Larg *l = (Larg*)arg;
	TcpConnection *conn = l->conn;
	NetconEthernetTap *tap = l->tap;

	if(!conn->sock)
		return -1;
	int fd = tap->_phy.getDescriptor(conn->sock);

  	if(conn) {
	  	// create new socketpair
	  	ZT_PHY_SOCKFD_TYPE fds[2];
		if(socketpair(PF_LOCAL, SOCK_STREAM, 0, fds) < 0) {
			if(errno < 0) {
				l->tap->sendReturnValue(conn, -1, errno);
				dwr(MSG_ERROR," nc_accept(): unable to create socketpair\n");
				return ERR_MEM;
			}
		}
		// create and populate new TcpConnection
		TcpConnection *newTcpConn = new TcpConnection();
		tap->addConnection(newTcpConn);
		newTcpConn->pcb = newPCB;
		newTcpConn->sock = tap->_phy.wrapSocket(fds[0], newTcpConn);

		if(sock_fd_write(fd, fds[1]) < 0)
	  		return -1;
		else {
			//close(fds[1]); // close other end of socketpair
		}
	    tap->lwipstack->_tcp_arg(newPCB, new Larg(tap, newTcpConn));
	    tap->lwipstack->_tcp_recv(newPCB, nc_recved);
	    tap->lwipstack->_tcp_err(newPCB, nc_err);
	    tap->lwipstack->_tcp_sent(newPCB, nc_sent);
	    tap->lwipstack->_tcp_poll(newPCB, nc_poll, 1);
	    if(conn->pcb->state == LISTEN) {
	    	dwr(MSG_DEBUG," nc_accept(): can't call tcp_accept() on LISTEN socket (pcb = %x)\n", conn->pcb);
	    	return ERR_OK;
	    }
	    tcp_accepted(conn->pcb); // Let lwIP know that it can queue additional incoming connections
		return ERR_OK;
  	} else
  		dwr(MSG_ERROR," nc_accept(): can't locate Connection object for PCB.\n");
  	return -1;
}

err_t NetconEthernetTap::nc_recved(void *arg, struct tcp_pcb *PCB, struct pbuf *p, err_t err)
{
	Larg *l = (Larg*)arg;
	int tot = 0;
  	struct pbuf* q = p;

	if(!l->conn) {
		dwr(MSG_ERROR," nc_recved(): no connection\n");
		return ERR_OK; 
	}
	if(p == NULL) {
		if(l->conn && !l->conn->listening) {
			dwr(MSG_INFO," nc_recved(): closing connection\n");
			//if(l->tap->lwipstack->_tcp_close(l->conn->pcb) != ERR_OK)
			//	dwr(MSG_ERROR," nc_recved(): error while calling tcp_close()\n");
			l->tap->closeConnection(l->conn->sock);
			return ERR_ABRT;
		}
		return err;
	}
	Mutex::Lock _l(l->tap->_rx_buf_m);
	// Cycle through pbufs and write them to the RX buffer
	// The RX buffer will be emptied via phyOnUnixWritable()
	while(p != NULL) {
		if(p->len <= 0)
			break;
		int avail = DEFAULT_BUF_SZ - l->conn->rxsz;
		int len = p->len;
		if(avail < len)
			dwr(MSG_ERROR," nc_recved(): not enough room (%d bytes) on RX buffer\n", avail);
		memcpy(l->conn->rxbuf + (l->conn->rxsz), p->payload, len);
		l->conn->rxsz += len;
		p = p->next;
		tot += len;
	}
	if(tot)
		l->tap->_phy.setNotifyWritable(l->conn->sock, true);
	l->tap->lwipstack->_pbuf_free(q);
	return ERR_OK;
}

void NetconEthernetTap::nc_err(void *arg, err_t err)
{
	dwr(MSG_DEBUG,"nc_err() = %d\n", err);
	Larg *l = (Larg*)arg;
	if(!l->conn)
		dwr(MSG_ERROR,"nc_err(): connection is NULL!\n");
	int fd = l->tap->_phy.getDescriptor(l->conn->sock);

	switch(err)
	{
		case ERR_MEM:
		  dwr(MSG_ERROR,"nc_err(): ERR_MEM->ENOMEM\n");
			l->tap->sendReturnValue(fd, -1, ENOMEM);
			break;
		case ERR_BUF:
			dwr(MSG_ERROR,"nc_err(): ERR_BUF->ENOBUFS\n");
			l->tap->sendReturnValue(fd, -1, ENOBUFS);
			break;
		case ERR_TIMEOUT:
			dwr(MSG_ERROR,"nc_err(): ERR_TIMEOUT->ETIMEDOUT\n");
			l->tap->sendReturnValue(fd, -1, ETIMEDOUT);
			break;
		case ERR_RTE:
			dwr(MSG_ERROR,"nc_err(): ERR_RTE->ENETUNREACH\n");
			l->tap->sendReturnValue(fd, -1, ENETUNREACH);
			break;
		case ERR_INPROGRESS:
			dwr(MSG_ERROR,"nc_err(): ERR_INPROGRESS->EINPROGRESS\n");
			l->tap->sendReturnValue(fd, -1, EINPROGRESS);
			break;
		case ERR_VAL:
			dwr(MSG_ERROR,"nc_err(): ERR_VAL->EINVAL\n");
			l->tap->sendReturnValue(fd, -1, EINVAL);
			break;
		case ERR_WOULDBLOCK:
			dwr(MSG_ERROR,"nc_err(): ERR_WOULDBLOCK->EWOULDBLOCK\n");
			l->tap->sendReturnValue(fd, -1, EWOULDBLOCK);
			break;
		case ERR_USE:
			dwr(MSG_ERROR,"nc_err(): ERR_USE->EADDRINUSE\n");
			l->tap->sendReturnValue(fd, -1, EADDRINUSE);
			break;
		case ERR_ISCONN:
			dwr(MSG_ERROR,"nc_err(): ERR_ISCONN->EISCONN\n");
			l->tap->sendReturnValue(fd, -1, EISCONN);
			break;
		case ERR_ABRT:
			dwr(MSG_ERROR,"nc_err(): ERR_ABRT->ECONNREFUSED\n");
			l->tap->sendReturnValue(fd, -1, ECONNREFUSED);
			break;

			// FIXME: Below are errors which don't have a standard errno correlate

		case ERR_RST:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		case ERR_CLSD:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		case ERR_CONN:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		case ERR_ARG:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		case ERR_IF:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		default:
			break;
	}
	dwr(MSG_ERROR,"nc_err(): closing connection\n");
	l->tap->closeConnection(l->conn);
}

err_t NetconEthernetTap::nc_poll(void* arg, struct tcp_pcb *PCB)
{
	return ERR_OK;
}

err_t NetconEthernetTap::nc_sent(void* arg, struct tcp_pcb *PCB, u16_t len)
{
	Larg *l = (Larg*)arg;
	if(len) {
		float max = (float)DEFAULT_BUF_SZ;
		if(l->conn->txsz < max / 2) {
			l->tap->_phy.setNotifyReadable(l->conn->sock, true);
			l->tap->_phy.whack();
		}
	}
	return ERR_OK;
}

err_t NetconEthernetTap::nc_connected(void *arg, struct tcp_pcb *PCB, err_t err)
{
	Larg *l = (Larg*)arg;
	l->tap->sendReturnValue(l->tap->_phy.getDescriptor(l->conn->rpcSock), ERR_OK);
	return ERR_OK;
}

/*------------------------------------------------------------------------------
----------------------------- RPC Handler functions ----------------------------
------------------------------------------------------------------------------*/

void NetconEthernetTap::handleGetsockname(PhySocket *sock, PhySocket *rpcSock, void **uptr, struct getsockname_st *getsockname_rpc)
{
	TcpConnection *conn = getConnection(sock);
	char retmsg[sizeof(struct sockaddr_storage)];
	memset(&retmsg, 0, sizeof(retmsg));
	if ((conn)&&(conn->addr))
    	memcpy(&retmsg, conn->addr, sizeof(struct sockaddr_storage));
	write(_phy.getDescriptor(rpcSock), &retmsg, sizeof(struct sockaddr_storage));
}

void NetconEthernetTap::handleBind(PhySocket *sock, PhySocket *rpcSock, void **uptr, struct bind_st *bind_rpc)
{
	struct sockaddr_in *rawAddr = (struct sockaddr_in *) &bind_rpc->addr;
	int port = lwipstack->ntohs(rawAddr->sin_port);
	ip_addr_t connAddr;
	connAddr.addr = *((u32_t *)_ips[0].rawIpData());
	TcpConnection *conn = getConnection(sock);
	dwr(MSG_DEBUG," handleBind(%d)\n", bind_rpc->sockfd);
	if(conn) {
		if(conn->pcb->state == CLOSED){
	  		int err = lwipstack->tcp_bind(conn->pcb, &connAddr, port);
			int ip = rawAddr->sin_addr.s_addr;
			unsigned char d[4];
			d[0] = ip & 0xFF;
			d[1] = (ip >>  8) & 0xFF;
			d[2] = (ip >> 16) & 0xFF;
			d[3] = (ip >> 24) & 0xFF;
			dwr(MSG_DEBUG," handleBind(): %d.%d.%d.%d : %d\n", d[0],d[1],d[2],d[3], port);

			if(err != ERR_OK) {
				dwr(MSG_ERROR," handleBind(): err = %d\n", err);
				if(err == ERR_USE)
					sendReturnValue(rpcSock, -1, EADDRINUSE);
				if(err == ERR_MEM)
					sendReturnValue(rpcSock, -1, ENOMEM);
				if(err == ERR_BUF)
					sendReturnValue(rpcSock, -1, ENOMEM);
			} else {
				conn->addr = (struct sockaddr_storage *) &bind_rpc->addr;
				sendReturnValue(rpcSock, ERR_OK, ERR_OK); // Success
			}
		} else {
			dwr(MSG_ERROR," handleBind(): PCB (%x) not in CLOSED state. Ignoring BIND request.\n", conn->pcb);
			sendReturnValue(rpcSock, -1, EINVAL);
		}
	} else {
		dwr(MSG_ERROR," handleBind(): unable to locate TcpConnection.\n");
		sendReturnValue(rpcSock, -1, EBADF);
	}
}

void NetconEthernetTap::handleListen(PhySocket *sock, PhySocket *rpcSock, void **uptr, struct listen_st *listen_rpc)
{
	TcpConnection *conn = getConnection(sock);
	if(!conn){
		dwr(MSG_ERROR," handleListen(): unable to locate TcpConnection.\n");
		sendReturnValue(rpcSock, -1, EBADF);
		return;
	}
	if(conn->pcb->state == LISTEN) {
		dwr(MSG_ERROR," handleListen(): PCB is already in listening state.\n");
		sendReturnValue(rpcSock, ERR_OK, ERR_OK);
		return;
	}
	struct tcp_pcb* listeningPCB;

#ifdef TCP_LISTEN_BACKLOG
		listeningPCB = lwipstack->tcp_listen_with_backlog(conn->pcb, listen_rpc->backlog);
#else
		listeningPCB = lwipstack->tcp_listen(conn->pcb);
#endif

	if(listeningPCB != NULL) {
    	conn->pcb = listeningPCB;
    	lwipstack->tcp_accept(listeningPCB, nc_accept);
		lwipstack->tcp_arg(listeningPCB, new Larg(this, conn));
		/* we need to wait for the client to send us the fd allocated on their end
		for this listening socket */
		fcntl(_phy.getDescriptor(conn->sock), F_SETFL, O_NONBLOCK);
		conn->listening = true;
		sendReturnValue(rpcSock, ERR_OK, ERR_OK);
		return;
  	}
  sendReturnValue(rpcSock, -1, -1);
}

TcpConnection * NetconEthernetTap::handleSocket(PhySocket *sock, void **uptr, struct socket_st* socket_rpc)
{
	struct tcp_pcb *newPCB = lwipstack->tcp_new();
  	if(newPCB != NULL) {
  		TcpConnection *newConn = new TcpConnection();
  		*uptr = newConn;
  		newConn->sock = sock;
  		newConn->pcb = newPCB;
		return addConnection(newConn);;
	}
	dwr(MSG_ERROR," handleSocket(): Memory not available for new PCB\n");
	sendReturnValue(_phy.getDescriptor(sock), -1, ENOMEM);
	return NULL;
}

void NetconEthernetTap::handleConnect(PhySocket *sock, PhySocket *rpcSock, TcpConnection *conn, struct connect_st* connect_rpc)
{
	struct sockaddr_in *rawAddr = (struct sockaddr_in *) &connect_rpc->__addr;
	int port = lwipstack->ntohs(rawAddr->sin_port);
	ip_addr_t connAddr = convert_ip((struct sockaddr_in *)&connect_rpc->__addr);

	if(conn != NULL) {
		lwipstack->tcp_sent(conn->pcb, nc_sent);
		lwipstack->tcp_recv(conn->pcb, nc_recved);
		lwipstack->tcp_err(conn->pcb, nc_err);
		lwipstack->tcp_poll(conn->pcb, nc_poll, APPLICATION_POLL_FREQ);
		lwipstack->tcp_arg(conn->pcb, new Larg(this, conn));

		int err = 0, ip = rawAddr->sin_addr.s_addr;
		unsigned char d[4];
		d[0] = ip & 0xFF;
		d[1] = (ip >>  8) & 0xFF;
		d[2] = (ip >> 16) & 0xFF;
		d[3] = (ip >> 24) & 0xFF;
		dwr(MSG_DEBUG," handleConnect(): %d.%d.%d.%d: %d\n", d[0],d[1],d[2],d[3], port);	
		dwr(MSG_DEBUG," handleConnect(): pcb->state = %x\n", conn->pcb->state);
		if(conn->pcb->state != CLOSED) {
			dwr(MSG_DEBUG," handleConnect(): PCB != CLOSED, cannot connect using this PCB\n");
			sendReturnValue(rpcSock, -1, EAGAIN);
			return;
		}
		if((err = lwipstack->tcp_connect(conn->pcb,&connAddr,port,nc_connected)) < 0)
		{
			if(err == ERR_ISCONN) {
				sendReturnValue(rpcSock, -1, EISCONN); // Already in connected state
				return;
			} if(err == ERR_USE) {
				sendReturnValue(rpcSock, -1, EADDRINUSE); // Already in use
				return;
			} if(err == ERR_VAL) {
				sendReturnValue(rpcSock, -1, EINVAL); // Invalid ipaddress parameter
				return;
			} if(err == ERR_RTE) {
				sendReturnValue(rpcSock, -1, ENETUNREACH); // No route to host
				return;
			} if(err == ERR_BUF) {
				sendReturnValue(rpcSock, -1, EAGAIN); // No more ports available
				return;
			}
			if(err == ERR_MEM) {
				/* Can occur for the following reasons: tcp_enqueue_flags()

				1) tcp_enqueue_flags is always called with either SYN or FIN in flags.
				  We need one available snd_buf byte to do that.
				  This means we can't send FIN while snd_buf==0. A better fix would be to
				  not include SYN and FIN sequence numbers in the snd_buf count.

				2) Cannot allocate new pbuf
				3) Cannot allocate new TCP segment

				*/
				sendReturnValue(rpcSock, -1, EAGAIN); // FIXME: Doesn't describe the problem well, but closest match
				return;
			}

			// We should only return a value if failure happens immediately
			// Otherwise, we still need to wait for a callback from lwIP.
			// - This is because an ERR_OK from tcp_connect() only verifies
			//   that the SYN packet was enqueued onto the stack properly,
			//   that's it!
			// - Most instances of a retval for a connect() should happen
			//   in the nc_connect() and nc_err() callbacks!
			dwr(MSG_ERROR," handleConnect(): unable to connect\n");
			sendReturnValue(rpcSock, -1, EAGAIN);
		}
		// Everything seems to be ok, but we don't have enough info to retval
		conn->listening=true;
		conn->rpcSock=rpcSock; // used for return value from lwip CB
	} else {
		dwr(MSG_ERROR," handleConnect(): could not locate PCB based on their fd\n");
		sendReturnValue(rpcSock, -1, EBADF);
	}
}

void NetconEthernetTap::handleWrite(TcpConnection *conn)
{
	if(!conn || !conn->pcb) {
		dwr(MSG_ERROR," handleWrite(): invalid connection/PCB\n");
		return;
	}
	// How much we are currently allowed to write to the connection
	int err, sz, r, sndbuf = conn->pcb->snd_buf;
	if(!sndbuf) {
		/* PCB send buffer is full, turn off readability notifications for the
		corresponding PhySocket until nc_sent() is called and confirms that there is
		now space on the buffer */
		dwr(MSG_DEBUG," handleWrite(): sndbuf == 0, LWIP stack is full\n");
		_phy.setNotifyReadable(conn->sock, false);
		return;
	}
	if(conn->txsz <= 0)
		return; // Nothing to write
	if(!conn->listening)
		lwipstack->_tcp_output(conn->pcb);

	if(conn->sock) {
		r = conn->txsz < sndbuf ? conn->txsz : sndbuf;
		/* Writes data pulled from the client's socket buffer to LWIP. This merely sends the
		 * data to LWIP to be enqueued and eventually sent to the network. */
		if(r > 0) {
			err = lwipstack->_tcp_write(conn->pcb, &conn->txbuf, r, TCP_WRITE_FLAG_COPY);
			lwipstack->_tcp_output(conn->pcb);
			if(err != ERR_OK) {
				dwr(MSG_ERROR," handleWrite(): error while writing to PCB, (err = %d)\n", err);
				if(err == -1) 
					dwr(MSG_DEBUG," handleWrite(): out of memory\n");
				return;
			} else {
				sz = (conn->txsz)-r;
				if(sz)
					memmove(&conn->txbuf, (conn->txbuf+r), sz);
				conn->txsz -= r;

				float max = (float)DEFAULT_BUF_SZ;
				dwr(MSG_TRANSFER," TX --->    :: { TX: %.3f%%  |  RX: %.3f%% }  :: %d bytes\n", 
					(float)conn->txsz / max, (float)conn->rxsz / max, r);
				return;
			}
		}
	}
}

} // namespace ZeroTier
