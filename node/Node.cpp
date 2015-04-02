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

#include "../version.h"

#include "Constants.hpp"
#include "Node.hpp"
#include "RuntimeEnvironment.hpp"
#include "NetworkConfigMaster.hpp"
#include "CMWC4096.hpp"
#include "Switch.hpp"
#include "Multicaster.hpp"
#include "AntiRecursion.hpp"
#include "Topology.hpp"
#include "Buffer.hpp"
#include "Packet.hpp"
#include "Logger.hpp"
#include "Address.hpp"
#include "Identity.hpp"

namespace ZeroTier {

Node::Node(
	uint64_t now,
	ZT1_DataStoreGetFunction *dataStoreGetFunction,
	ZT1_DataStorePutFunction *dataStorePutFunction,
	ZT1_WirePacketSendFunction *wirePacketSendFunction,
	ZT1_VirtualNetworkFrameFunction *virtualNetworkFrameFunction,
	ZT1_VirtualNetworkConfigCallback *virtualNetworkConfigCallback,
	ZT1_StatusCallback *statusCallback) :
	RR(new RuntimeEnvironment(this)),
	_dataStoreGetFunction(dataStoreGetFunction),
	_dataStorePutFunction(dataStorePutFunction),
	_wirePacketSendFunction(wirePacketSendFunction),
	_virtualNetworkFrameFunction(virtualNetworkFrameFunction),
	_virtualNetworkConfigCallback(virtualNetworkConfigCallback),
	_statusCallback(statusCallback),
	_networks(),
	_networks_m(),
	_now(now),
	_timeOfLastPacketReceived(0),
	_timeOfLastPrivilegedPacket(0),
	_spamCounter(0)
{
	try {
		RR->prng = new CMWC4096();
		RR->sw = new Switch(RR);
		RR->mc = new Multicaster(RR);
		RR->antiRec = new AntiRecursion(RR);
		RR->topology = new Topology(RR);
	} catch ( ... ) {
		delete RR->topology;
		delete RR->antiRec;
		delete RR->mc;
		delete RR->sw;
		delete RR->prng;
		delete RR->log;
		delete RR;
		throw;
	}
}

Node::~Node()
{
	delete RR->topology;
	delete RR->antiRec;
	delete RR->mc;
	delete RR->sw;
	delete RR->prng;
	delete RR->log;
	delete RR;
}

ZT1_ResultCode Node::processWirePacket(
	uint64_t now,
	const struct sockaddr_storage *remoteAddress,
	int linkDesperation,
	const void *packetData,
	unsigned int packetLength,
	uint64_t *nextCallDeadline)
{
	_now = now;
}

ZT1_ResultCode Node::processVirtualNetworkFrame(
	uint64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	uint64_t *nextCallDeadline)
{
	_now = now;
}

ZT1_Resultcode Node::processNothing(uint64_t now,uint64_t *nextCallDeadline)
{
	_now = now;
}

ZT1_ResultCode Node::join(uint64_t nwid)
{
	Mutex::Lock _l(_networks_m);
	SharedPtr<Network> &nw = _networks[nwid];
	if (!nw)
		nw = SharedPtr<Network>(new Network(RR,nwid));
	return ZT1_RESULT_OK;
}

ZT1_ResultCode Node::leave(uint64_t nwid)
{
	Mutex::Lock _l(_networks_m);
	std::map< uint64_t,SharedPtr<Network> >::iterator nw(_networks.find(nwid));
	if (nw != _networks.end()) {
		nw->second->destroy();
		_networks.erase(nw);
	}
}

ZT1_ResultCode Node::multicastSubscribe(ZT1_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi)
{
}

ZT1_ResultCode Node::multicastUnsubscribe(ZT1_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi)
{
}

void Node::status(ZT1_NodeStatus *status)
{
}

ZT1_PeerList *Node::peers()
{
}

ZT1_VirtualNetworkConfig *Node::networkConfig(uint64_t nwid)
{
}

ZT1_VirtualNetworkList *Node::listNetworks()
{
}

void Node::freeQueryResult(void *qr)
{
	if (qr)
		::free(qr);
}

void Node::setNetconfMaster(void *networkConfigMasterInstance)
{
	RR->netconfMaster = reinterpret_cast<NetworkConfigMaster *>(networkConfigMasterInstance);
}

} // namespace ZeroTier

extern "C" {

enum ZT1_ResultCode ZT1_Node_new(
	ZT1_Node **node,
	uint64_t now,
	ZT1_DataStoreGetFunction *dataStoreGetFunction,
	ZT1_DataStorePutFunction *dataStorePutFunction,
	ZT1_WirePacketSendFunction *wirePacketSendFunction,
	ZT1_VirtualNetworkFrameFunction *virtualNetworkFrameFunction,
	ZT1_VirtualNetworkConfigCallback *virtualNetworkConfigCallback,
	ZT1_StatusCallback *statusCallback)
{
	*node = (ZT1_Node *)0;
	try {
		*node = reinterpret_cast<ZT1_Node *>(new ZeroTier::Node(now,dataStoreGetFunction,dataStorePutFunction,wirePacketSendFunction,virtualNetworkFrameFunction,virtualNetworkConfigCallback,statusCallback));
		return ZT1_RESULT_OK;
	} catch (std::bad_alloc &exc) {
		return ZT1_RESULT_ERROR_OUT_OF_MEMORY;
	} catch (std::runtime_error &exc) {
		return ZT1_RESULT_ERROR_DATA_STORE_FAILED;
	} catch ( ... ) {
		return ZT1_RESULT_ERROR_INTERNAL;
	}
}

enum ZT1_ResultCode ZT1_Node_processWirePacket(
	ZT1_Node *node,
	uint64_t now,
	const struct sockaddr_storage *remoteAddress,
	int linkDesperation,
	const void *packetData,
	unsigned int packetLength,
	uint64_t *nextCallDeadline)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->processWirePacket(now,remoteAddress,linkDesperation,packetData,packetLength,nextCallDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT1_RESULT_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT1_RESULT_PACKET_INVALID;
	}
}

enum ZT1_ResultCode ZT1_Node_processVirtualNetworkFrame(
	ZT1_Node *node,
	uint64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	uint64_t *nextCallDeadline)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->processVirtualNetworkFrame(now,nwid,sourceMac,destMac,etherType,vlanId,frameData,frameLength,nextCallDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT1_RESULT_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT1_RESULT_ERROR_INTERNAL;
	}
}

enum ZT1_Resultcode ZT1_Node_processNothing(ZT1_Node *node,uint64_t now,uint64_t *nextCallDeadline)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->processNothing(now,nextCallDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT1_RESULT_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT1_RESULT_ERROR_INTERNAL;
	}
}

enum ZT1_ResultCode ZT1_Node_join(ZT1_Node *node,uint64_t nwid)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->join(nwid);
	} catch (std::bad_alloc &exc) {
		return ZT1_RESULT_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT1_RESULT_ERROR_INTERNAL;
	}
}

enum ZT1_ResultCode ZT1_Node_leave(ZT1_Node *node,uint64_t nwid)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->leave(nwid);
	} catch (std::bad_alloc &exc) {
		return ZT1_RESULT_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT1_RESULT_ERROR_INTERNAL;
	}
}

enum ZT1_ResultCode ZT1_Node_multicastSubscribe(ZT1_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->multicastSubscribe(nwid,multicastGroup,multicastAdi);
	} catch (std::bad_alloc &exc) {
		return ZT1_RESULT_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT1_RESULT_ERROR_INTERNAL;
	}
}

enum ZT1_ResultCode ZT1_Node_multicastUnsubscribe(ZT1_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->multicastUnsubscribe(nwid,multicastGroup,multicastAdi);
	} catch (std::bad_alloc &exc) {
		return ZT1_RESULT_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT1_RESULT_ERROR_INTERNAL;
	}
}

void ZT1_Node_status(ZT1_Node *node,ZT1_NodeStatus *status)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->status(status);
	} catch ( ... ) {}
}

ZT1_PeerList *ZT1_Node_peers(ZT1_Node *node)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->peers();
	} catch ( ... ) {
		return (ZT1_PeerList *)0;
	}
}

ZT1_VirtualNetworkConfig *ZT1_Node_networkConfig(ZT1_Node *node,uint64_t nwid)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->networkConfig(nwid);
	} catch ( ... ) {
		return (ZT1_VirtualNetworkConfig *)0;
	}
}

ZT1_VirtualNetworkList *ZT1_Node_listNetworks(ZT1_Node *node)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->listNetworks();
	} catch ( ... ) {
		return (ZT1_VirtualNetworkList *)0;
	}
}

void ZT1_Node_freeQueryResult(ZT1_Node *node,void *qr)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->freeQueryResult(qr);
	} catch ( ... ) {}
}

void ZT1_Node_setNetconfMaster(ZT1_Node *node,void *networkConfigMasterInstance)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->setNetconfMaster(networkConfigMasterInstance);
	} catch ( ... ) {}
}

void ZT1_version(int *major,int *minor,int *revision,unsigned long *featureFlags)
{
	if (major) *major = ZEROTIER_ONE_VERSION_MAJOR;
	if (minor) *minor = ZEROTIER_ONE_VERSION_MINOR;
	if (revision) *revision = ZEROTIER_ONE_VERSION_REVISION;
	if (featureFlags) {
		*featureFlags =
			ZT1_FEATURE_FLAG_THREAD_SAFE |
#ifdef ZT_OFFICIAL_BUILD
			ZT1_FEATURE_FLAG_OFFICIAL
#endif
		;
	}
}

} // extern "C"
