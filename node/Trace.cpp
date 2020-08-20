/*
 * Copyright (c)2019 ZeroTier, Inc.
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

//#define ZT_TRACE

#include <stdio.h>
#include <stdarg.h>

#include "Trace.hpp"
#include "RuntimeEnvironment.hpp"
#include "Switch.hpp"
#include "Node.hpp"
#include "Utils.hpp"
#include "Dictionary.hpp"
#include "CertificateOfMembership.hpp"
#include "CertificateOfOwnership.hpp"
#include "Tag.hpp"
#include "Capability.hpp"
#include "Revocation.hpp"
#include "../include/ZeroTierDebug.h"

namespace ZeroTier {

#ifdef ZT_TRACE
static void ZT_LOCAL_TRACE(void *const tPtr,const RuntimeEnvironment *const RR,const char *const fmt,...)
{
	char traceMsgBuf[1024];
	va_list ap;
	va_start(ap,fmt);
	vsnprintf(traceMsgBuf,sizeof(traceMsgBuf),fmt,ap);
	va_end(ap);
	traceMsgBuf[sizeof(traceMsgBuf) - 1] = (char)0;
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,traceMsgBuf);
}
#else
#define ZT_LOCAL_TRACE(...)
#endif

void Trace::resettingPathsInScope(void *const tPtr,const Address &reporter,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,const InetAddress::IpScope scope)
{
	char tmp[128];

	ZT_LOCAL_TRACE(tPtr,RR,"RESET and revalidate paths in scope %d; new phy address %s reported by trusted peer %.10llx",(int)scope,myPhysicalAddress.toIpString(tmp),reporter.toInt());

	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__RESETTING_PATHS_IN_SCOPE_S);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,reporter);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,reporterPhysicalAddress.toString(tmp));
	d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_PHYADDR,myPhysicalAddress.toString(tmp));
	d.add(ZT_REMOTE_TRACE_FIELD__IP_SCOPE,(uint64_t)scope);

	if (_globalTarget)
		_send(tPtr,d,_globalTarget);
	_spamToAllNetworks(tPtr,d,Trace::LEVEL_NORMAL);
}

void Trace::peerConfirmingUnknownPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &path,const uint64_t packetId,const Packet::Verb verb)
{
	char tmp[128];
	if (!path) return; // sanity check

	ZT_LOCAL_TRACE(tPtr,RR,"trying unknown path %s to %.10llx (packet %.16llx verb %d local socket %lld network %.16llx)",path->address().toString(tmp),peer.address().toInt(),packetId,(double)verb,path->localSocket(),networkId);

	std::pair<Address,Trace::Level> byn;
	if (networkId) { Mutex::Lock l(_byNet_m); _byNet.get(networkId,byn); }

	if ((_globalTarget)||(byn.first)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PEER_CONFIRMING_UNKNOWN_PATH_S);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_VERB,(uint64_t)verb);
		if (networkId)
			d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,networkId);
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,peer.address());
		if (path) {
			d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
			d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
		}

		if (_globalTarget)
			_send(tPtr,d,_globalTarget);
		if (byn.first)
			_send(tPtr,d,byn.first);
	}
}

void Trace::bondStateMessage(void *const tPtr,char *msg)
{
	ZT_LOCAL_TRACE(tPtr,RR,"%s",msg);
}

void Trace::peerLearnedNewPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &newPath,const uint64_t packetId)
{
	char tmp[128];
	if (!newPath) return; // sanity check

	ZT_LOCAL_TRACE(tPtr,RR,"learned new path %s to %.10llx (packet %.16llx local socket %lld network %.16llx)",newPath->address().toString(tmp),peer.address().toInt(),packetId,newPath->localSocket(),networkId);

	std::pair<Address,Trace::Level> byn;
	if (networkId) { Mutex::Lock l(_byNet_m); _byNet.get(networkId,byn); }

	if ((_globalTarget)||(byn.first)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PEER_LEARNED_NEW_PATH_S);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
		if (networkId)
			d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,networkId);
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,peer.address());
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,newPath->address().toString(tmp));
		d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,newPath->localSocket());

		if (_globalTarget)
			_send(tPtr,d,_globalTarget);
		if (byn.first)
			_send(tPtr,d,byn.first);
	}
}

void Trace::peerRedirected(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &newPath)
{
	char tmp[128];
	if (!newPath) return; // sanity check

	ZT_LOCAL_TRACE(tPtr,RR,"explicit redirect from %.10llx to path %s",peer.address().toInt(),newPath->address().toString(tmp));

	std::pair<Address,Trace::Level> byn;
	if (networkId) { Mutex::Lock l(_byNet_m); _byNet.get(networkId,byn); }

	if ((_globalTarget)||(byn.first)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PEER_REDIRECTED_S);
		if (networkId)
			d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,networkId);
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,peer.address());
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,newPath->address().toString(tmp));
		d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,newPath->localSocket());

		if (_globalTarget)
			_send(tPtr,d,_globalTarget);
		if (byn.first)
			_send(tPtr,d,byn.first);
	}
}

void Trace::outgoingNetworkFrameDropped(void *const tPtr,const SharedPtr<Network> &network,const MAC &sourceMac,const MAC &destMac,const unsigned int etherType,const unsigned int vlanId,const unsigned int frameLen,const char *reason)
{
#ifdef ZT_TRACE
	char tmp[128],tmp2[128];
#endif
	if (!network) return; // sanity check

	ZT_LOCAL_TRACE(tPtr,RR,"%.16llx DROP frame %s -> %s etherType %.4x size %u (%s)",network->id(),sourceMac.toString(tmp),destMac.toString(tmp2),etherType,frameLen,(reason) ? reason : "unknown reason");

	std::pair<Address,Trace::Level> byn;
	{ Mutex::Lock l(_byNet_m); _byNet.get(network->id(),byn); }

	if ( ((_globalTarget)&&((int)_globalLevel >= (int)Trace::LEVEL_VERBOSE)) || ((byn.first)&&((int)byn.second >= (int)Trace::LEVEL_VERBOSE)) ) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__OUTGOING_NETWORK_FRAME_DROPPED_S);
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,network->id());
		d.add(ZT_REMOTE_TRACE_FIELD__SOURCE_MAC,sourceMac.toInt());
		d.add(ZT_REMOTE_TRACE_FIELD__DEST_MAC,destMac.toInt());
		d.add(ZT_REMOTE_TRACE_FIELD__ETHERTYPE,(uint64_t)etherType);
		d.add(ZT_REMOTE_TRACE_FIELD__VLAN_ID,(uint64_t)vlanId);
		d.add(ZT_REMOTE_TRACE_FIELD__FRAME_LENGTH,(uint64_t)frameLen);
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		if ((_globalTarget)&&((int)_globalLevel >= (int)Trace::LEVEL_VERBOSE))
			_send(tPtr,d,_globalTarget);
		if ((byn.first)&&((int)byn.second >= (int)Trace::LEVEL_VERBOSE))
			_send(tPtr,d,byn.first);
	}
}

void Trace::incomingNetworkAccessDenied(void *const tPtr,const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,bool credentialsRequested)
{
	char tmp[128];
	if (!network) return; // sanity check

	ZT_LOCAL_TRACE(tPtr,RR,"%.16llx DENIED packet from %.10llx(%s) verb %d size %u%s",network->id(),source.toInt(),(path) ? (path->address().toString(tmp)) : "???",(int)verb,packetLength,credentialsRequested ? " (credentials requested)" : " (credentials not requested)");

	std::pair<Address,Trace::Level> byn;
	{ Mutex::Lock l(_byNet_m); _byNet.get(network->id(),byn); }

	if ( ((_globalTarget)&&((int)_globalLevel >= (int)Trace::LEVEL_VERBOSE)) || ((byn.first)&&((int)byn.second >= (int)Trace::LEVEL_VERBOSE)) ) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__INCOMING_NETWORK_ACCESS_DENIED_S);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_VERB,(uint64_t)verb);
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,source);
		if (path) {
			d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
			d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
		}
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,network->id());

		if ((_globalTarget)&&((int)_globalLevel >= (int)Trace::LEVEL_VERBOSE))
			_send(tPtr,d,_globalTarget);
		if ((byn.first)&&((int)byn.second >= (int)Trace::LEVEL_VERBOSE))
			_send(tPtr,d,byn.first);
	}
}

void Trace::incomingNetworkFrameDropped(void *const tPtr,const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,const MAC &sourceMac,const MAC &destMac,const char *reason)
{
	char tmp[128];
	if (!network) return; // sanity check

	ZT_LOCAL_TRACE(tPtr,RR,"%.16llx DROPPED frame from %.10llx(%s) verb %d size %u",network->id(),source.toInt(),(path) ? (path->address().toString(tmp)) : "???",(int)verb,packetLength);

	std::pair<Address,Trace::Level> byn;
	{ Mutex::Lock l(_byNet_m); _byNet.get(network->id(),byn); }

	if ( ((_globalTarget)&&((int)_globalLevel >= (int)Trace::LEVEL_VERBOSE)) || ((byn.first)&&((int)byn.second >= (int)Trace::LEVEL_VERBOSE)) ) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__INCOMING_NETWORK_FRAME_DROPPED_S);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_VERB,(uint64_t)verb);
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,source);
		if (path) {
			d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
			d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
		}
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,network->id());
		d.add(ZT_REMOTE_TRACE_FIELD__SOURCE_MAC,sourceMac.toInt());
		d.add(ZT_REMOTE_TRACE_FIELD__DEST_MAC,destMac.toInt());
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		if ((_globalTarget)&&((int)_globalLevel >= (int)Trace::LEVEL_VERBOSE))
			_send(tPtr,d,_globalTarget);
		if ((byn.first)&&((int)byn.second >= (int)Trace::LEVEL_VERBOSE))
			_send(tPtr,d,byn.first);
	}
}

void Trace::incomingPacketMessageAuthenticationFailure(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const unsigned int hops,const char *reason)
{
	char tmp[128];

	ZT_LOCAL_TRACE(tPtr,RR,"MAC failed for packet %.16llx from %.10llx(%s)",packetId,source.toInt(),(path) ? path->address().toString(tmp) : "???");

	if ((_globalTarget)&&((int)_globalLevel >= Trace::LEVEL_DEBUG)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PACKET_MAC_FAILURE_S);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_HOPS,(uint64_t)hops);
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,source);
		if (path) {
			d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
			d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
		}
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		_send(tPtr,d,_globalTarget);
	}
}

void Trace::incomingPacketInvalid(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const unsigned int hops,const Packet::Verb verb,const char *reason)
{
	char tmp[128];

	ZT_LOCAL_TRACE(tPtr,RR,"INVALID packet %.16llx from %.10llx(%s) (%s)",packetId,source.toInt(),(path) ? path->address().toString(tmp) : "???",(reason) ? reason : "unknown reason");

	if ((_globalTarget)&&((int)_globalLevel >= Trace::LEVEL_DEBUG)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PACKET_INVALID_S);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_VERB,(uint64_t)verb);
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,source);
		if (path) {
			d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
			d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
		}
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_HOPS,(uint64_t)hops);
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		_send(tPtr,d,_globalTarget);
	}
}

void Trace::incomingPacketDroppedHELLO(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const char *reason)
{
	char tmp[128];

	ZT_LOCAL_TRACE(tPtr,RR,"DROPPED HELLO from %.10llx(%s) (%s)",source.toInt(),(path) ? path->address().toString(tmp) : "???",(reason) ? reason : "???");

	if ((_globalTarget)&&((int)_globalLevel >= Trace::LEVEL_DEBUG)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PACKET_INVALID_S);
		d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,source);
		if (path) {
			d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
			d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
		}
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		_send(tPtr,d,_globalTarget);
	}
}

void Trace::networkConfigRequestSent(void *const tPtr,const Network &network,const Address &controller)
{
	ZT_LOCAL_TRACE(tPtr,RR,"requesting configuration for network %.16llx",network.id());
	if ((_globalTarget)&&((int)_globalLevel >= Trace::LEVEL_DEBUG)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__NETWORK_CONFIG_REQUEST_SENT_S);
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,network.id());
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_CONTROLLER_ID,controller);
		_send(tPtr,d,_globalTarget);
	}
}

void Trace::networkFilter(
	void *const tPtr,
	const Network &network,
	const RuleResultLog &primaryRuleSetLog,
	const RuleResultLog *const matchingCapabilityRuleSetLog,
	const Capability *const matchingCapability,
	const Address &ztSource,
	const Address &ztDest,
	const MAC &macSource,
	const MAC &macDest,
	const uint8_t *const frameData,
	const unsigned int frameLen,
	const unsigned int etherType,
	const unsigned int vlanId,
	const bool noTee,
	const bool inbound,
	const int accept)
{
	std::pair<Address,Trace::Level> byn;
	{ Mutex::Lock l(_byNet_m); _byNet.get(network.id(),byn); }

	if ( ((_globalTarget)&&((int)_globalLevel >= (int)Trace::LEVEL_RULES)) || ((byn.first)&&((int)byn.second >= (int)Trace::LEVEL_RULES)) ) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__NETWORK_FILTER_TRACE_S);
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,network.id());
		d.add(ZT_REMOTE_TRACE_FIELD__SOURCE_ZTADDR,ztSource);
		d.add(ZT_REMOTE_TRACE_FIELD__DEST_ZTADDR,ztDest);
		d.add(ZT_REMOTE_TRACE_FIELD__SOURCE_MAC,macSource.toInt());
		d.add(ZT_REMOTE_TRACE_FIELD__DEST_MAC,macDest.toInt());
		d.add(ZT_REMOTE_TRACE_FIELD__ETHERTYPE,(uint64_t)etherType);
		d.add(ZT_REMOTE_TRACE_FIELD__VLAN_ID,(uint64_t)vlanId);
		d.add(ZT_REMOTE_TRACE_FIELD__FILTER_FLAG_NOTEE,noTee ? "1" : "0");
		d.add(ZT_REMOTE_TRACE_FIELD__FILTER_FLAG_INBOUND,inbound ? "1" : "0");
		d.add(ZT_REMOTE_TRACE_FIELD__FILTER_RESULT,(int64_t)accept);
		d.add(ZT_REMOTE_TRACE_FIELD__FILTER_BASE_RULE_LOG,(const char *)primaryRuleSetLog.data(),(int)primaryRuleSetLog.sizeBytes());
		if (matchingCapabilityRuleSetLog)
			d.add(ZT_REMOTE_TRACE_FIELD__FILTER_CAP_RULE_LOG,(const char *)matchingCapabilityRuleSetLog->data(),(int)matchingCapabilityRuleSetLog->sizeBytes());
		if (matchingCapability)
			d.add(ZT_REMOTE_TRACE_FIELD__FILTER_CAP_ID,(uint64_t)matchingCapability->id());
		d.add(ZT_REMOTE_TRACE_FIELD__FRAME_LENGTH,(uint64_t)frameLen);
		if (frameLen > 0)
			d.add(ZT_REMOTE_TRACE_FIELD__FRAME_DATA,(const char *)frameData,(frameLen > 256) ? (int)256 : (int)frameLen);

		if ((_globalTarget)&&((int)_globalLevel >= (int)Trace::LEVEL_RULES))
			_send(tPtr,d,_globalTarget);
		if ((byn.first)&&((int)byn.second >= (int)Trace::LEVEL_RULES))
			_send(tPtr,d,byn.first);
	}
}

void Trace::credentialRejected(void *const tPtr,const CertificateOfMembership &c,const char *reason)
{
	std::pair<Address,Trace::Level> byn;
	if (c.networkId()) { Mutex::Lock l(_byNet_m); _byNet.get(c.networkId(),byn); }

	if ((_globalTarget)||(byn.first)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		if (_globalTarget)
			_send(tPtr,d,_globalTarget);
		if (byn.first)
			_send(tPtr,d,byn.first);
	}
}

void Trace::credentialRejected(void *const tPtr,const CertificateOfOwnership &c,const char *reason)
{
	std::pair<Address,Trace::Level> byn;
	if (c.networkId()) { Mutex::Lock l(_byNet_m); _byNet.get(c.networkId(),byn); }

	if ((_globalTarget)||(byn.first)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		if (_globalTarget)
			_send(tPtr,d,_globalTarget);
		if (byn.first)
			_send(tPtr,d,byn.first);
	}
}

void Trace::credentialRejected(void *const tPtr,const Capability &c,const char *reason)
{
	std::pair<Address,Trace::Level> byn;
	if (c.networkId()) { Mutex::Lock l(_byNet_m); _byNet.get(c.networkId(),byn); }

	if ((_globalTarget)||(byn.first)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		if (_globalTarget)
			_send(tPtr,d,_globalTarget);
		if (byn.first)
			_send(tPtr,d,byn.first);
	}
}

void Trace::credentialRejected(void *const tPtr,const Tag &c,const char *reason)
{
	std::pair<Address,Trace::Level> byn;
	if (c.networkId()) { Mutex::Lock l(_byNet_m); _byNet.get(c.networkId(),byn); }

	if ((_globalTarget)||(byn.first)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_INFO,(uint64_t)c.value());
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		if (_globalTarget)
			_send(tPtr,d,_globalTarget);
		if (byn.first)
			_send(tPtr,d,byn.first);
	}
}

void Trace::credentialRejected(void *const tPtr,const Revocation &c,const char *reason)
{
	std::pair<Address,Trace::Level> byn;
	if (c.networkId()) { Mutex::Lock l(_byNet_m); _byNet.get(c.networkId(),byn); }

	if ((_globalTarget)||(byn.first)) {
		Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
		d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
		d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
		d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_REVOCATION_TARGET,c.target());
		if (reason)
			d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);

		if (_globalTarget)
			_send(tPtr,d,_globalTarget);
		if (byn.first)
			_send(tPtr,d,byn.first);
	}
}

void Trace::updateMemoizedSettings()
{
	_globalTarget = RR->node->remoteTraceTarget();
	_globalLevel = RR->node->remoteTraceLevel();
	const std::vector< SharedPtr<Network> > nws(RR->node->allNetworks());
	{
		Mutex::Lock l(_byNet_m);
		_byNet.clear();
		for(std::vector< SharedPtr<Network> >::const_iterator n(nws.begin());n!=nws.end();++n) {
			const Address dest((*n)->config().remoteTraceTarget);
			if (dest) {
				std::pair<Address,Trace::Level> &m = _byNet[(*n)->id()];
				m.first = dest;
				m.second = (*n)->config().remoteTraceLevel;
			}
		}
	}
}

void Trace::_send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d,const Address &dest)
{
	Packet outp(dest,RR->identity.address(),Packet::VERB_REMOTE_TRACE);
	outp.appendCString(d.data());
	outp.compress();
	RR->sw->send(tPtr,outp,true);
}

void Trace::_spamToAllNetworks(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d,const Level level)
{
	Mutex::Lock l(_byNet_m);
	Hashtable< uint64_t,std::pair< Address,Trace::Level > >::Iterator i(_byNet);
	uint64_t *k = (uint64_t *)0;
	std::pair<Address,Trace::Level> *v = (std::pair<Address,Trace::Level> *)0;
	while (i.next(k,v)) {
		if ((v)&&(v->first)&&((int)v->second >= (int)level))
			_send(tPtr,d,v->first);
	}
}

} // namespace ZeroTier
