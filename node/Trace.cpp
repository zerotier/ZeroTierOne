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

namespace ZeroTier {

#ifdef ZT_TRACE
static const char *packetVerbString(Packet::Verb v)
{
	switch(v) {
		case Packet::VERB_NOP: return "NOP";
		case Packet::VERB_HELLO: return "HELLO";
		case Packet::Packet::VERB_ERROR: return "ERROR";
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
#endif

void Trace::resettingPathsInScope(const Address &reporter,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,const InetAddress::IpScope scope)
{
}

void Trace::txTimedOut(const Address &destination)
{
}

void Trace::peerConfirmingUnknownPath(Peer &peer,const SharedPtr<Path> &path,const uint64_t packetId,const Packet::Verb verb)
{
}

void Trace::peerLearnedNewPath(Peer &peer,const SharedPtr<Path> &oldPath,const SharedPtr<Path> &newPath,const uint64_t packetId)
{
}

void Trace::peerRedirected(Peer &peer,const SharedPtr<Path> &oldPath,const SharedPtr<Path> &newPath)
{
}

void Trace::outgoingFrameDropped(const SharedPtr<Network> &network,const MAC &sourceMac,const MAC &destMac,const unsigned int etherType,const unsigned int vlanId,const unsigned int frameLen,const char *reason)
{
}

void Trace::incomingPacketTrustedPath(const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const uint64_t trustedPathId,bool approved)
{
}

void Trace::incomingPacketMessageAuthenticationFailure(const SharedPtr<Path> &path,const uint64_t packetId,const Address &source)
{
}

void Trace::incomingPacketInvalid(const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const Packet::Verb verb,const char *reason)
{
}

void Trace::incomingPacketDroppedHELLO(const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const char *reason)
{
}

void Trace::networkAccessDenied(const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,bool credentialsRequested)
{
}

void Trace::networkFrameDropped(const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,const MAC &sourceMac,const MAC &destMac)
{
}

void Trace::networkConfigRequestSent(const Network &network,const Address &controller)
{
}

void Trace::networkFilter(
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
}

void Trace::credentialRejected(const CertificateOfMembership &c,const char *reason)
{
}

void Trace::credentialRejected(const CertificateOfOwnership &c,const char *reason)
{
}

void Trace::credentialRejected(const CertificateOfRepresentation &c,const char *reason)
{
}

void Trace::credentialRejected(const Capability &c,const char *reason)
{
}

void Trace::credentialRejected(const Tag &c,const char *reason)
{
}

void Trace::credentialRejected(const Revocation &c,const char *reason)
{
}

void Trace::credentialAccepted(const CertificateOfMembership &c)
{
}

void Trace::credentialAccepted(const CertificateOfOwnership &c)
{
}

void Trace::credentialAccepted(const CertificateOfRepresentation &c)
{
}

void Trace::credentialAccepted(const Capability &c)
{
}

void Trace::credentialAccepted(const Tag &c)
{
}

void Trace::credentialAccepted(const Revocation &c)
{
}

} // namespace ZeroTier
