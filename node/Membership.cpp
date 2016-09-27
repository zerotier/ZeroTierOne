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
			if (!have) have = _newTag(tag.id());
			have->lastReceived = RR->node->now();
			have->tag = tag;
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
			TRACE("addCredential(Tag) for %s on %.16llx REJECTED (revoked or too old)",cap.issuedTo().toString().c_str(),cap.networkId());
			return ADD_REJECTED;
		}
		if (have->cap == cap) {
			TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (redundant)",cap.issuedTo().toString().c_str(),cap.networkId());
			return ADD_ACCEPTED_REDUNDANT;
		}
	}

	switch(cap.verify(RR)) {
		default:
			TRACE("addCredential(Tag) for %s on %.16llx REJECTED (invalid)",cap.issuedTo().toString().c_str(),cap.networkId());
			return ADD_REJECTED;
		case 0:
			TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (new)",cap.issuedTo().toString().c_str(),cap.networkId());
			if (!have) have = _newCapability(cap.id());
			have->lastReceived = RR->node->now();
			have->cap = cap;
			return ADD_ACCEPTED_NEW;
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,const NetworkConfig &nconf,const Revocation &rev)
{
	switch(rev.verify(RR)) {
		default:
			return ADD_REJECTED;
		case 0: {
			const uint64_t now = RR->node->now();
			switch(rev.type()) {
				default:
				//case Revocation::CREDENTIAL_TYPE_ALL:
					return ( (_revokeCom(rev)||_revokeCap(rev,now)||_revokeTag(rev,now)) ? ADD_ACCEPTED_NEW : ADD_ACCEPTED_REDUNDANT );
				case Revocation::CREDENTIAL_TYPE_COM:
					return (_revokeCom(rev) ? ADD_ACCEPTED_NEW : ADD_ACCEPTED_REDUNDANT);
				case Revocation::CREDENTIAL_TYPE_CAPABILITY:
					return (_revokeCap(rev,now) ? ADD_ACCEPTED_NEW : ADD_ACCEPTED_REDUNDANT);
				case Revocation::CREDENTIAL_TYPE_TAG:
					return (_revokeTag(rev,now) ? ADD_ACCEPTED_NEW : ADD_ACCEPTED_REDUNDANT);
			}
		}
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

Membership::_RemoteTag *Membership::_newTag(const uint64_t id)
{
	_RemoteTag *t;
	uint64_t minlr = 0xffffffffffffffffULL;
	for(unsigned int i=0;i<ZT_MAX_NETWORK_TAGS;++i) {
		if (_remoteTags[i]->id == ZT_MEMBERSHIP_CRED_ID_UNUSED) {
			t = _remoteTags[i];
			break;
		} else if (_remoteTags[i]->lastReceived <= minlr) {
			t = _remoteTags[i];
			minlr = _remoteTags[i]->lastReceived;
		}
	}
	t->id = id;
	t->lastReceived = 0;
	t->revocationThreshold = 0;
	t->tag = Tag();
	std::sort(&(_remoteTags[0]),&(_remoteTags[ZT_MAX_NETWORK_TAGS]),_RemoteCredentialSorter<_RemoteTag>());
	return t;
}

Membership::_RemoteCapability *Membership::_newCapability(const uint64_t id)
{
	_RemoteCapability *c;
	uint64_t minlr = 0xffffffffffffffffULL;
	for(unsigned int i=0;i<ZT_MAX_NETWORK_CAPABILITIES;++i) {
		if (_remoteCaps[i]->id == ZT_MEMBERSHIP_CRED_ID_UNUSED) {
			c = _remoteCaps[i];
			break;
		} else if (_remoteCaps[i]->lastReceived <= minlr) {
			c = _remoteCaps[i];
			minlr = _remoteCaps[i]->lastReceived;
		}
	}
	c->id = id;
	c->lastReceived = 0;
	c->revocationThreshold = 0;
	c->cap = Capability();
	std::sort(&(_remoteCaps[0]),&(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]),_RemoteCredentialSorter<_RemoteCapability>());
	return c;
}

bool Membership::_revokeCom(const Revocation &rev)
{
	if (rev.threshold() > _comRevocationThreshold) {
		_comRevocationThreshold = rev.threshold();
		return true;
	}
	return false;
}

bool Membership::_revokeCap(const Revocation &rev,const uint64_t now)
{
	_RemoteCapability *const *htmp = std::lower_bound(&(_remoteCaps[0]),&(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]),(uint64_t)rev.credentialId(),_RemoteCredentialSorter<_RemoteCapability>());
	_RemoteCapability *have = ((htmp != &(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]))&&((*htmp)->id == (uint64_t)rev.credentialId())) ? *htmp : (_RemoteCapability *)0;
	if (!have) have = _newCapability(rev.credentialId());
	if (rev.threshold() > have->revocationThreshold) {
		have->lastReceived = now;
		have->revocationThreshold = rev.threshold();
		return true;
	}
	return false;
}

bool Membership::_revokeTag(const Revocation &rev,const uint64_t now)
{
	_RemoteTag *const *htmp = std::lower_bound(&(_remoteTags[0]),&(_remoteTags[ZT_MAX_NETWORK_TAGS]),(uint64_t)rev.credentialId(),_RemoteCredentialSorter<_RemoteTag>());
	_RemoteTag *have = ((htmp != &(_remoteTags[ZT_MAX_NETWORK_TAGS]))&&((*htmp)->id == (uint64_t)rev.credentialId())) ? *htmp : (_RemoteTag *)0;
	if (!have) have = _newTag(rev.credentialId());
	if (rev.threshold() > have->revocationThreshold) {
		have->lastReceived = now;
		have->revocationThreshold = rev.threshold();
		return true;
	}
	return false;
}

} // namespace ZeroTier
