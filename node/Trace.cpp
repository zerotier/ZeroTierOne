/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#include "Trace.hpp"
#include "RuntimeEnvironment.hpp"
#include "Switch.hpp"
#include "Node.hpp"
#include "Utils.hpp"
#include "Dictionary.hpp"
#include "CertificateOfMembership.hpp"
#include "CertificateOfOwnership.hpp"
#include "CertificateOfRepresentation.hpp"
#include "Tag.hpp"
#include "Capability.hpp"
#include "Revocation.hpp"

namespace ZeroTier {

void Trace::resettingPathsInScope(void *const tPtr,const Address &reporter,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,const InetAddress::IpScope scope)
{
	char tmp[128];
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__RESETTING_PATHS_IN_SCOPE_S);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,reporter);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,reporterPhysicalAddress.toString(tmp));
	d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_PHYADDR,myPhysicalAddress.toString(tmp));
	d.add(ZT_REMOTE_TRACE_FIELD__IP_SCOPE,(uint64_t)scope);
	_send(tPtr,d,0);
}

void Trace::txTimedOut(void *const tPtr,const Address &destination)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__TX_TIMED_OUT_S);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,destination);
	_send(tPtr,d,0);
}

void Trace::peerConfirmingUnknownPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &path,const uint64_t packetId,const Packet::Verb verb)
{
	char tmp[128];
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PEER_CONFIRMING_UNKNOWN_PATH_S);
	d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
	d.add(ZT_REMOTE_TRACE_FIELD__PACKET_VERB,(uint64_t)verb);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,networkId);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,peer.address());
	if (path) {
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
		d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
	}
	_send(tPtr,d,networkId);
}

void Trace::peerLearnedNewPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &oldPath,const SharedPtr<Path> &newPath,const uint64_t packetId)
{
	char tmp[128];
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PEER_LEARNED_NEW_PATH_S);
	d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,networkId);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,peer.address());
	if (oldPath) {
		d.add(ZT_REMOTE_TRACE_FIELD__OLD_REMOTE_PHYADDR,oldPath->address().toString(tmp));
	}
	if (newPath) {
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,newPath->address().toString(tmp));
		d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,newPath->localSocket());
	}
	_send(tPtr,d,networkId);
}

void Trace::peerRedirected(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &oldPath,const SharedPtr<Path> &newPath)
{
	char tmp[128];
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PEER_REDIRECTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,networkId);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,peer.address());
	if (oldPath) {
		d.add(ZT_REMOTE_TRACE_FIELD__OLD_REMOTE_PHYADDR,oldPath->address().toString(tmp));
	}
	if (newPath) {
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,newPath->address().toString(tmp));
		d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,newPath->localSocket());
	}
	_send(tPtr,d,networkId);
}

void Trace::outgoingNetworkFrameDropped(void *const tPtr,const SharedPtr<Network> &network,const MAC &sourceMac,const MAC &destMac,const unsigned int etherType,const unsigned int vlanId,const unsigned int frameLen,const char *reason)
{
	if (!network) return; // sanity check
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__OUTGOING_NETWORK_FRAME_DROPPED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__SOURCE_MAC,sourceMac.toInt());
	d.add(ZT_REMOTE_TRACE_FIELD__DEST_MAC,destMac.toInt());
	d.add(ZT_REMOTE_TRACE_FIELD__ETHERTYPE,(uint64_t)etherType);
	d.add(ZT_REMOTE_TRACE_FIELD__VLAN_ID,(uint64_t)vlanId);
	d.add(ZT_REMOTE_TRACE_FIELD__FRAME_LENGTH,(uint64_t)frameLen);
	if (reason) {
		d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);
	}
	_send(tPtr,d,network);
}

void Trace::incomingNetworkAccessDenied(void *const tPtr,const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,bool credentialsRequested)
{
	if (!network) return; // sanity check
	char tmp[128];
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
	_send(tPtr,d,*network);
}

void Trace::incomingNetworkFrameDropped(void *const tPtr,const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,const MAC &sourceMac,const MAC &destMac,const char *reason)
{
	if (!network) return; // sanity check
	char tmp[128];
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
	_send(tPtr,d,*network);
}

void Trace::incomingPacketTrustedPath(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const uint64_t trustedPathId,bool approved)
{
	// TODO
}

void Trace::incomingPacketMessageAuthenticationFailure(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const unsigned int hops)
{
	char tmp[128];
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PACKET_MAC_FAILURE_S);
	d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
	d.add(ZT_REMOTE_TRACE_FIELD__PACKET_HOPS,(uint64_t)hops);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,source);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
	d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
	_send(tPtr,d,0);
}

void Trace::incomingPacketInvalid(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const unsigned int hops,const Packet::Verb verb,const char *reason)
{
	char tmp[128];
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PACKET_INVALID_S);
	d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
	d.add(ZT_REMOTE_TRACE_FIELD__PACKET_VERB,(uint64_t)verb);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,source);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
	d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
	d.add(ZT_REMOTE_TRACE_FIELD__PACKET_HOPS,(uint64_t)hops);
	if (reason)
		d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);
	_send(tPtr,d,0);
}

void Trace::incomingPacketDroppedHELLO(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const char *reason)
{
	char tmp[128];
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__PACKET_INVALID_S);
	d.add(ZT_REMOTE_TRACE_FIELD__PACKET_ID,packetId);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,source);
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
	d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
	if (reason)
		d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);
	_send(tPtr,d,0);
}

void Trace::networkConfigRequestSent(void *const tPtr,const Network &network,const Address &controller)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__NETWORK_CONFIG_REQUEST_SENT_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,network.id());
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_CONTROLLER_ID,controller);
	_send(tPtr,d,network);
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
	_send(tPtr,d,network);
}

void Trace::credentialRejected(void *const tPtr,const CertificateOfMembership &c,const char *reason)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
	if (reason)
		d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);
	_send(tPtr,d,c.networkId());
}

void Trace::credentialRejected(void *const tPtr,const CertificateOfOwnership &c,const char *reason)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
	if (reason)
		d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);
	_send(tPtr,d,c.networkId());
}

void Trace::credentialRejected(void *const tPtr,const CertificateOfRepresentation &c,const char *reason)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
	if (reason)
		d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);
	_send(tPtr,d,0);
}

void Trace::credentialRejected(void *const tPtr,const Capability &c,const char *reason)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
	if (reason)
		d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);
	_send(tPtr,d,c.networkId());
}

void Trace::credentialRejected(void *const tPtr,const Tag &c,const char *reason)
{
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
	_send(tPtr,d,c.networkId());
}

void Trace::credentialRejected(void *const tPtr,const Revocation &c,const char *reason)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_REVOCATION_TARGET,c.target());
	if (reason)
		d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);
	_send(tPtr,d,c.networkId());
}

void Trace::credentialAccepted(void *const tPtr,const CertificateOfMembership &c)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_ACCEPTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
	_send(tPtr,d,c.networkId());
}

void Trace::credentialAccepted(void *const tPtr,const CertificateOfOwnership &c)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_ACCEPTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
	_send(tPtr,d,c.networkId());
}

void Trace::credentialAccepted(void *const tPtr,const CertificateOfRepresentation &c)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_ACCEPTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
	_send(tPtr,d,0);
}

void Trace::credentialAccepted(void *const tPtr,const Capability &c)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_ACCEPTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
	_send(tPtr,d,c.networkId());
}

void Trace::credentialAccepted(void *const tPtr,const Tag &c)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_ACCEPTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO,c.issuedTo());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_INFO,(uint64_t)c.value());
	_send(tPtr,d,c.networkId());
}

void Trace::credentialAccepted(void *const tPtr,const Revocation &c)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_ACCEPTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_REVOCATION_TARGET,c.target());
	_send(tPtr,d,c.networkId());
}

void Trace::_send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d)
{
#ifdef ZT_TRACE
	unsigned int i = 0;
	while (i < (unsigned int)(sizeof(_traceMsgBuf) - 1)) {
		const char c = d.data()[i];
		if (c == 0) {
			break;
		} else if (c == '\n') {
			_traceMsgBuf[i++] = ' ';
		} else if ((c >= 32)&&(c <= 126)) {
			_traceMsgBuf[i++] = c;
		} else {
			if ((i + 3) < (unsigned int)(sizeof(_traceMsgBuf) - 1)) {
				_traceMsgBuf[i++] = '\\';
				Utils::hex((uint8_t)c,_traceMsgBuf + i);
			}
		}
	}
	_traceMsgBuf[i] = (char)0;
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,_traceMsgBuf);
#endif

	const Address rtt(RR->node->remoteTraceTarget());
	if (rtt) {
		Packet outp(rtt,RR->identity.address(),Packet::VERB_REMOTE_TRACE);
		outp.appendCString(d.data());
		outp.compress();
		RR->sw->send(tPtr,outp,true);
	}
}

void Trace::_send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d,const uint64_t networkId)
{
	_send(tPtr,d);
	if (networkId) {
		const SharedPtr<Network> network(RR->node->network(networkId));
		if ((network)&&(network->config().remoteTraceTarget)) {
			Packet outp(network->config().remoteTraceTarget,RR->identity.address(),Packet::VERB_REMOTE_TRACE);
			outp.appendCString(d.data());
			outp.compress();
			RR->sw->send(tPtr,outp,true);
		}
	}
}

void Trace::_send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d,const Network &network)
{
	_send(tPtr,d);
	if (network.config().remoteTraceTarget) {
		Packet outp(network.config().remoteTraceTarget,RR->identity.address(),Packet::VERB_REMOTE_TRACE);
		outp.appendCString(d.data());
		outp.compress();
		RR->sw->send(tPtr,outp,true);
	}
}

} // namespace ZeroTier
