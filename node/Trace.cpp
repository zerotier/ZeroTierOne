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

// Defining ZT_TRACE causes debug tracing messages to be dumped to stderr
#ifdef ZT_TRACE

static const char *packetVerbString(Packet::Verb v)
{
	switch(v) {
		case Packet::VERB_NOP: return "NOP";
		case Packet::VERB_HELLO: return "HELLO";
		case Packet::VERB_ERROR: return "ERROR";
		case Packet::VERB_OK: return "OK";
		case Packet::VERB_WHOIS: return "WHOIS";
		case Packet::VERB_RENDEZVOUS: return "RENDEZVOUS";
		case Packet::VERB_FRAME: return "FRAME";
		case Packet::VERB_EXT_FRAME: return "EXT_FRAME";
		case Packet::VERB_ECHO: return "ECHO";
		case Packet::VERB_MULTICAST_LIKE: return "MULTICAST_LIKE";
		case Packet::VERB_NETWORK_CREDENTIALS: return "NETWORK_CREDENTIALS";
		case Packet::VERB_NETWORK_CONFIG_REQUEST: return "NETWORK_CONFIG_REQUEST";
		case Packet::VERB_NETWORK_CONFIG: return "NETWORK_CONFIG";
		case Packet::VERB_MULTICAST_GATHER: return "MULTICAST_GATHER";
		case Packet::VERB_MULTICAST_FRAME: return "MULTICAST_FRAME";
		case Packet::VERB_PUSH_DIRECT_PATHS: return "PUSH_DIRECT_PATHS";
		case Packet::VERB_USER_MESSAGE: return "USER_MESSAGE";
		case Packet::VERB_REMOTE_TRACE: return "REMOTE_TRACE";
	}
	return "(unknown)";
}

static const char *packetErrorString(Packet::ErrorCode e)
{
	switch(e) {
		case Packet::ERROR_NONE: return "NONE";
		case Packet::ERROR_INVALID_REQUEST: return "INVALID_REQUEST";
		case Packet::ERROR_BAD_PROTOCOL_VERSION: return "BAD_PROTOCOL_VERSION";
		case Packet::ERROR_OBJ_NOT_FOUND: return "OBJECT_NOT_FOUND";
		case Packet::ERROR_IDENTITY_COLLISION: return "IDENTITY_COLLISION";
		case Packet::ERROR_UNSUPPORTED_OPERATION: return "UNSUPPORTED_OPERATION";
		case Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE: return "NEED_MEMBERSHIP_CERTIFICATE";
		case Packet::ERROR_NETWORK_ACCESS_DENIED_: return "NETWORK_ACCESS_DENIED";
		case Packet::ERROR_UNWANTED_MULTICAST: return "UNWANTED_MULTICAST";
	}
	return "(unknown)";
}

#define TRprintf(f,...) { fprintf(stderr,(f),__VA_ARGS__); fflush(stderr); }

#else

#define TRprintf(f,...) 

#endif // ZT_TRACE

#define ZT_REMOTE_TRACE_FIELD__EVENT "E"
#define ZT_REMOTE_TRACE_FIELD__PACKET_ID "pid"
#define ZT_REMOTE_TRACE_FIELD__PACKET_VERB "pv"
#define ZT_REMOTE_TRACE_FIELD__PACKET_TRUSTED_PATH_ID "ptpid"
#define ZT_REMOTE_TRACE_FIELD__PACKET_TRUSTED_PATH_APPROVED "ptpok"
#define ZT_REMOTE_TRACE_FIELD__PACKET_HOPS "phops"
#define ZT_REMOTE_TRACE_FIELD__OLD_REMOTE_PHYADDR "oldrphy"
#define ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR "rzt"
#define ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR "rphy"
#define ZT_REMOTE_TRACE_FIELD__LOCAL_ZTADDR "lzt"
#define ZT_REMOTE_TRACE_FIELD__LOCAL_PHYADDR "lphy"
#define ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET "ls"
#define ZT_REMOTE_TRACE_FIELD__IP_SCOPE "ipsc"
#define ZT_REMOTE_TRACE_FIELD__NETWORK_ID "nwid"
#define ZT_REMOTE_TRACE_FIELD__SOURCE_MAC "seth"
#define ZT_REMOTE_TRACE_FIELD__DEST_MAC "deth"
#define ZT_REMOTE_TRACE_FIELD__ETHERTYPE "et"
#define ZT_REMOTE_TRACE_FIELD__VLAN_ID "vlan"
#define ZT_REMOTE_TRACE_FIELD__FRAME_LENGTH "fl"
#define ZT_REMOTE_TRACE_FIELD__FRAME_DATA "fd"
#define ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE "credtype"
#define ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID "credid"
#define ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP "credts"
#define ZT_REMOTE_TRACE_FIELD__CREDENTIAL_INFO "credinfo"
#define ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ISSUED_TO "crediss"
#define ZT_REMOTE_TRACE_FIELD__CREDENTIAL_REVOCATION_TARGET "credRt"
#define ZT_REMOTE_TRACE_FIELD__REASON "reason"

#define ZT_REMOTE_TRACE_EVENT__RESETTING_PATHS_IN_SCOPE_S "1000"
#define ZT_REMOTE_TRACE_EVENT__TX_TIMED_OUT_S "1001"
#define ZT_REMOTE_TRACE_EVENT__PEER_CONFIRMING_UNKNOWN_PATH_S "1002"
#define ZT_REMOTE_TRACE_EVENT__PEER_LEARNED_NEW_PATH_S "1003"
#define ZT_REMOTE_TRACE_EVENT__PEER_REDIRECTED_S "1004"
#define ZT_REMOTE_TRACE_EVENT__PACKET_MAC_FAILURE_S "1005"
#define ZT_REMOTE_TRACE_EVENT__PACKET_INVALID_S "1006"
#define ZT_REMOTE_TRACE_EVENT__DROPPED_HELLO_S "1006"

#define ZT_REMOTE_TRACE_EVENT__OUTGOING_NETWORK_FRAME_DROPPED_S "2000"
#define ZT_REMOTE_TRACE_EVENT__INCOMING_NETWORK_ACCESS_DENIED_S "2001"
#define ZT_REMOTE_TRACE_EVENT__INCOMING_NETWORK_FRAME_DROPPED_S "2002"
#define ZT_REMOTE_TRACE_EVENT__CREDENTIAL_REJECTED_S "2003"
#define ZT_REMOTE_TRACE_EVENT__CREDENTIAL_ACCEPTED_S "2004"

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
	if (reason)
		d.add(ZT_REMOTE_TRACE_FIELD__REASON,reason);
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
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,network->id());
	d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_ZTADDR,source);
	if (path) {
		d.add(ZT_REMOTE_TRACE_FIELD__REMOTE_PHYADDR,path->address().toString(tmp));
		d.add(ZT_REMOTE_TRACE_FIELD__LOCAL_SOCKET,path->localSocket());
	}
}

void Trace::incomingNetworkFrameDropped(void *const tPtr,const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,const MAC &sourceMac,const MAC &destMac)
{
	//Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	//d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__INCOMING_NETWORK_FRAME_DROPPED_S);
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
	//char tmp[128];
	//Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	//_send(tPtr,d,network.id());
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
}

void Trace::credentialAccepted(void *const tPtr,const CertificateOfRepresentation &c)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_ACCEPTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TIMESTAMP,c.timestamp());
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
}

void Trace::credentialAccepted(void *const tPtr,const Revocation &c)
{
	Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> d;
	d.add(ZT_REMOTE_TRACE_FIELD__EVENT,ZT_REMOTE_TRACE_EVENT__CREDENTIAL_ACCEPTED_S);
	d.add(ZT_REMOTE_TRACE_FIELD__NETWORK_ID,c.networkId());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_TYPE,(uint64_t)c.credentialType());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_ID,(uint64_t)c.id());
	d.add(ZT_REMOTE_TRACE_FIELD__CREDENTIAL_REVOCATION_TARGET,c.target());
}

void Trace::_send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d)
{
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

void Trace::_send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d,const SharedPtr<Network> &network)
{
	_send(tPtr,d);
	if ((network)&&(network->config().remoteTraceTarget)) {
		Packet outp(network->config().remoteTraceTarget,RR->identity.address(),Packet::VERB_REMOTE_TRACE);
		outp.appendCString(d.data());
		outp.compress();
		RR->sw->send(tPtr,outp,true);
	}
}

} // namespace ZeroTier
