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
	ZT1_DataStoreGetFunction *dataStoreGetFunction,
	ZT1_DataStorePutFunction *dataStorePutFunction,
	ZT1_WirePacketSendFunction *wirePacketSendFunction,
	ZT1_VirtualNetworkFrameFunction *virtualNetworkFrameFunction,
	ZT1_VirtualNetworkConfigCallback *networkConfigCallback,
	ZT1_StatusCallback *statusCallback) :
	RR(new RuntimeEnvironment(this)),
	_dataStoreGetFunction(dataStoreGetFunction),
	_dataStorePutFunction(dataStorePutFunction),
	_wirePacketSendFunction(wirePacketSendFunction),
	_virtualNetworkFrameFunction(virtualNetworkFrameFunction),
	_networkConfigCallback(networkConfigCallback),
	_statusCallback(statusCallback),
	_networks(),
	_networks_m(),
	_now(0),
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

ZT1_ResultCode Node::join(uint64_t nwid)
{
}

ZT1_ResultCode Node::leave(uint64_t nwid)
{
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
