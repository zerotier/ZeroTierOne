/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#include <algorithm>

#include "Membership.hpp"
#include "RuntimeEnvironment.hpp"
#include "Peer.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "Node.hpp"

#define ZT_CREDENTIAL_PUSH_EVERY (ZT_NETWORK_AUTOCONF_DELAY / 3)

namespace ZeroTier {

Membership::Membership() :
	_lastUpdatedMulticast(0),
	_lastPushAttempt(0),
	_lastPushedCom(0),
	_comRevocationThreshold(0)
{
	for(unsigned int i=0;i<ZT_MAX_NETWORK_TAGS;++i) _remoteTags[i] = &(_tagMem[i]);
	for(unsigned int i=0;i<ZT_MAX_NETWORK_CAPABILITIES;++i) _remoteCaps[i] = &(_capMem[i]);
}

void Membership::pushCredentials(const RuntimeEnvironment *RR,const uint64_t now,const Address &peerAddress,const NetworkConfig &nconf,int localCapabilityIndex,const bool force)
{
	// This limits how often we go through this logic, which prevents us from
	// doing all this for every single packet or other event.
	if ( ((now - _lastPushAttempt) < 1000ULL) && (!force) )
		return;
	_lastPushAttempt = now;

	try {
		unsigned int localTagPtr = 0;
		bool needCom = ( (nconf.com) && ( ((now - _lastPushedCom) >= ZT_CREDENTIAL_PUSH_EVERY) || (force) ) );
		do {
			Buffer<ZT_PROTO_MAX_PACKET_LENGTH> capsAndTags;

			unsigned int appendedCaps = 0;
			if (localCapabilityIndex >= 0) {
				capsAndTags.addSize(2);

				if ( (_localCaps[localCapabilityIndex].id != nconf.capabilities[localCapabilityIndex].id()) || ((now - _localCaps[localCapabilityIndex].lastPushed) >= ZT_CREDENTIAL_PUSH_EVERY) || (force) ) {
					_localCaps[localCapabilityIndex].lastPushed = now;
					_localCaps[localCapabilityIndex].id = nconf.capabilities[localCapabilityIndex].id();
					nconf.capabilities[localCapabilityIndex].serialize(capsAndTags);
					++appendedCaps;
				}

				capsAndTags.setAt<uint16_t>(0,(uint16_t)appendedCaps);
				localCapabilityIndex = -1; // don't send this cap again on subsequent loops if force is true
			} else {
				capsAndTags.append((uint16_t)0);
			}

			unsigned int appendedTags = 0;
			const unsigned int tagCountPos = capsAndTags.size();
			capsAndTags.addSize(2);
			for(;localTagPtr<nconf.tagCount;++localTagPtr) {
				if ( (_localTags[localTagPtr].id != nconf.tags[localTagPtr].id()) || ((now - _localTags[localTagPtr].lastPushed) >= ZT_CREDENTIAL_PUSH_EVERY) || (force) ) {
					if ((capsAndTags.size() + sizeof(Tag)) >= (ZT_PROTO_MAX_PACKET_LENGTH - sizeof(CertificateOfMembership)))
						break;
					nconf.tags[localTagPtr].serialize(capsAndTags);
					++appendedTags;
				}
			}
			capsAndTags.setAt<uint16_t>(tagCountPos,(uint16_t)appendedTags);

			if (needCom||appendedCaps||appendedTags) {
				Packet outp(peerAddress,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
				if (needCom) {
					nconf.com.serialize(outp);
					_lastPushedCom = now;
				}
				outp.append((uint8_t)0x00);
				outp.append(capsAndTags.data(),capsAndTags.size());
				outp.append((uint16_t)0); // no revocations, these propagate differently
				outp.compress();
				RR->sw->send(outp,true);
				needCom = false; // don't send COM again on subsequent loops if force is true
			}
		} while (localTagPtr < nconf.tagCount);
	} catch ( ... ) {
		TRACE("unable to send credentials due to unexpected exception");
	}
}

const Capability *Membership::getCapability(const NetworkConfig &nconf,const uint32_t id) const
{
	const _RemoteCapability *const *c = std::lower_bound(&(_remoteCaps[0]),&(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]),(uint64_t)id,_RemoteCredentialSorter<_RemoteCapability>());
	return ( ((c != &(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]))&&((*c)->id == (uint64_t)id)) ? ((((*c)->lastReceived)&&(_isCredentialTimestampValid(nconf,(*c)->cap,**c))) ? &((*c)->cap) : (const Capability *)0) : (const Capability *)0);
}

const Tag *Membership::getTag(const NetworkConfig &nconf,const uint32_t id) const
{
	const _RemoteTag *const *t = std::lower_bound(&(_remoteTags[0]),&(_remoteTags[ZT_MAX_NETWORK_TAGS]),(uint64_t)id,_RemoteCredentialSorter<_RemoteTag>());
	return ( ((t != &(_remoteTags[ZT_MAX_NETWORK_CAPABILITIES]))&&((*t)->id == (uint64_t)id)) ? ((((*t)->lastReceived)&&(_isCredentialTimestampValid(nconf,(*t)->tag,**t))) ? &((*t)->tag) : (const Tag *)0) : (const Tag *)0);
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,const NetworkConfig &nconf,const CertificateOfMembership &com)
{
	const uint64_t newts = com.timestamp().first;
	if (newts <= _comRevocationThreshold) {
		TRACE("addCredential(CertificateOfMembership) for %s on %.16llx REJECTED (revoked)",com.issuedTo().toString().c_str(),com.networkId());
		return ADD_REJECTED;
	}

	const uint64_t oldts = _com.timestamp().first;
	if (newts < oldts) {
		TRACE("addCredential(CertificateOfMembership) for %s on %.16llx REJECTED (older than current)",com.issuedTo().toString().c_str(),com.networkId());
		return ADD_REJECTED;
	}
	if ((newts == oldts)&&(_com == com)) {
		TRACE("addCredential(CertificateOfMembership) for %s on %.16llx ACCEPTED (redundant)",com.issuedTo().toString().c_str(),com.networkId());
		return ADD_ACCEPTED_REDUNDANT;
	}

	switch(com.verify(RR)) {
		default:
			TRACE("addCredential(CertificateOfMembership) for %s on %.16llx REJECTED (invalid signature or object)",com.issuedTo().toString().c_str(),com.networkId());
			return ADD_REJECTED;
		case 0:
			TRACE("addCredential(CertificateOfMembership) for %s on %.16llx ACCEPTED (new)",com.issuedTo().toString().c_str(),com.networkId());
			_com = com;
			return ADD_ACCEPTED_NEW;
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,const NetworkConfig &nconf,const Tag &tag)
{
	_RemoteTag *const *htmp = std::lower_bound(&(_remoteTags[0]),&(_remoteTags[ZT_MAX_NETWORK_TAGS]),(uint64_t)tag.id(),_RemoteCredentialSorter<_RemoteTag>());
	_RemoteTag *have = ((htmp != &(_remoteTags[ZT_MAX_NETWORK_TAGS]))&&((*htmp)->id == (uint64_t)tag.id())) ? *htmp : (_RemoteTag *)0;
	if (have) {
		if ( (!_isCredentialTimestampValid(nconf,tag,*have)) || (have->tag.timestamp() > tag.timestamp()) ) {
			TRACE("addCredential(Tag) for %s on %.16llx REJECTED (revoked or too old)",tag.issuedTo().toString().c_str(),tag.networkId());
			return ADD_REJECTED;
		}
		if (have->tag == tag) {
			TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (redundant)",tag.issuedTo().toString().c_str(),tag.networkId());
			return ADD_ACCEPTED_REDUNDANT;
		}
	}

	switch(tag.verify(RR)) {
		default:
			TRACE("addCredential(Tag) for %s on %.16llx REJECTED (invalid)",tag.issuedTo().toString().c_str(),tag.networkId());
			return ADD_REJECTED;
		case 0:
			TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (new)",tag.issuedTo().toString().c_str(),tag.networkId());
			if (have) {
				have->lastReceived = RR->node->now();
				have->tag = tag;
			} else {
				uint64_t minlr = 0xffffffffffffffffULL;
				for(unsigned int i=0;i<ZT_MAX_NETWORK_TAGS;++i) {
					if (_remoteTags[i]->id == 0xffffffffffffffffULL) {
						have = _remoteTags[i];
						break;
					} else if (_remoteTags[i]->lastReceived <= minlr) {
						have = _remoteTags[i];
						minlr = _remoteTags[i]->lastReceived;
					}
				}
				have->lastReceived = RR->node->now();
				have->tag = tag;
				std::sort(&(_remoteTags[0]),&(_remoteTags[ZT_MAX_NETWORK_TAGS]),_RemoteCredentialSorter<_RemoteTag>());
			}
			return ADD_ACCEPTED_NEW;
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,const NetworkConfig &nconf,const Capability &cap)
{
	_RemoteCapability *const *htmp = std::lower_bound(&(_remoteCaps[0]),&(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]),(uint64_t)cap.id(),_RemoteCredentialSorter<_RemoteCapability>());
	_RemoteCapability *have = ((htmp != &(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]))&&((*htmp)->id == (uint64_t)cap.id())) ? *htmp : (_RemoteCapability *)0;
	if (have) {
		if ( (!_isCredentialTimestampValid(nconf,cap,*have)) || (have->cap.timestamp() > cap.timestamp()) ) {
			TRACE("addCredential(Tag) for %s on %.16llx REJECTED (revoked or too old)",tag.issuedTo().toString().c_str(),tag.networkId());
			return ADD_REJECTED;
		}
		if (have->cap == cap) {
			TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (redundant)",tag.issuedTo().toString().c_str(),tag.networkId());
			return ADD_ACCEPTED_REDUNDANT;
		}
	}

	switch(cap.verify(RR)) {
		default:
			TRACE("addCredential(Tag) for %s on %.16llx REJECTED (invalid)",tag.issuedTo().toString().c_str(),tag.networkId());
			return ADD_REJECTED;
		case 0:
			TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (new)",tag.issuedTo().toString().c_str(),tag.networkId());
			if (have) {
				have->lastReceived = RR->node->now();
				have->cap = cap;
			} else {
				uint64_t minlr = 0xffffffffffffffffULL;
				for(unsigned int i=0;i<ZT_MAX_NETWORK_CAPABILITIES;++i) {
					if (_remoteCaps[i]->id == 0xffffffffffffffffULL) {
						have = _remoteCaps[i];
						break;
					} else if (_remoteCaps[i]->lastReceived <= minlr) {
						have = _remoteCaps[i];
						minlr = _remoteCaps[i]->lastReceived;
					}
				}
				have->lastReceived = RR->node->now();
				have->cap = cap;
				std::sort(&(_remoteCaps[0]),&(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]),_RemoteCredentialSorter<_RemoteCapability>());
			}
			return ADD_ACCEPTED_NEW;
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

} // namespace ZeroTier
