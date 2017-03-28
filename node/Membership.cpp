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
	_lastPushedCom(0),
	_comRevocationThreshold(0)
{
	for(unsigned int i=0;i<ZT_MAX_NETWORK_TAGS;++i) _remoteTags[i] = &(_tagMem[i]);
	for(unsigned int i=0;i<ZT_MAX_NETWORK_CAPABILITIES;++i) _remoteCaps[i] = &(_capMem[i]);
	for(unsigned int i=0;i<ZT_MAX_CERTIFICATES_OF_OWNERSHIP;++i) _remoteCoos[i] = &(_cooMem[i]);
}

void Membership::pushCredentials(const RuntimeEnvironment *RR,void *tPtr,const uint64_t now,const Address &peerAddress,const NetworkConfig &nconf,int localCapabilityIndex,const bool force)
{
	bool sendCom = ( (nconf.com) && ( ((now - _lastPushedCom) >= ZT_CREDENTIAL_PUSH_EVERY) || (force) ) );

	const Capability *sendCap;
	if (localCapabilityIndex >= 0) {
		sendCap = &(nconf.capabilities[localCapabilityIndex]);
		if ( (_localCaps[localCapabilityIndex].id != sendCap->id()) || ((now - _localCaps[localCapabilityIndex].lastPushed) >= ZT_CREDENTIAL_PUSH_EVERY) || (force) ) {
			_localCaps[localCapabilityIndex].lastPushed = now;
			_localCaps[localCapabilityIndex].id = sendCap->id();
		} else sendCap = (const Capability *)0;
	} else sendCap = (const Capability *)0;

	const Tag *sendTags[ZT_MAX_NETWORK_TAGS];
	unsigned int sendTagCount = 0;
	for(unsigned int t=0;t<nconf.tagCount;++t) {
		if ( (_localTags[t].id != nconf.tags[t].id()) || ((now - _localTags[t].lastPushed) >= ZT_CREDENTIAL_PUSH_EVERY) || (force) ) {
			_localTags[t].lastPushed = now;
			_localTags[t].id = nconf.tags[t].id();
			sendTags[sendTagCount++] = &(nconf.tags[t]);
		}
	}

	const CertificateOfOwnership *sendCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP];
	unsigned int sendCooCount = 0;
	for(unsigned int c=0;c<nconf.certificateOfOwnershipCount;++c) {
		if ( (_localCoos[c].id != nconf.certificatesOfOwnership[c].id()) || ((now - _localCoos[c].lastPushed) >= ZT_CREDENTIAL_PUSH_EVERY) || (force) ) {
			_localCoos[c].lastPushed = now;
			_localCoos[c].id = nconf.certificatesOfOwnership[c].id();
			sendCoos[sendCooCount++] = &(nconf.certificatesOfOwnership[c]);
		}
	}

	unsigned int tagPtr = 0;
	unsigned int cooPtr = 0;
	while ((tagPtr < sendTagCount)||(cooPtr < sendCooCount)||(sendCom)||(sendCap)) {
		Packet outp(peerAddress,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);

		if (sendCom) {
			sendCom = false;
			nconf.com.serialize(outp);
			_lastPushedCom = now;
		}
		outp.append((uint8_t)0x00);

		if (sendCap) {
			outp.append((uint16_t)1);
			sendCap->serialize(outp);
			sendCap = (const Capability *)0;
		} else outp.append((uint16_t)0);

		const unsigned int tagCountAt = outp.size();
		outp.addSize(2);
		unsigned int thisPacketTagCount = 0;
		while ((tagPtr < sendTagCount)&&((outp.size() + sizeof(Tag) + 16) < ZT_PROTO_MAX_PACKET_LENGTH)) {
			sendTags[tagPtr++]->serialize(outp);
			++thisPacketTagCount;
		}
		outp.setAt(tagCountAt,(uint16_t)thisPacketTagCount);

		// No revocations, these propagate differently
		outp.append((uint16_t)0);

		const unsigned int cooCountAt = outp.size();
		outp.addSize(2);
		unsigned int thisPacketCooCount = 0;
		while ((cooPtr < sendCooCount)&&((outp.size() + sizeof(CertificateOfOwnership) + 16) < ZT_PROTO_MAX_PACKET_LENGTH)) {
			sendCoos[cooPtr++]->serialize(outp);
			++thisPacketCooCount;
		}
		outp.setAt(cooCountAt,(uint16_t)thisPacketCooCount);

		outp.compress();
		RR->sw->send(tPtr,outp,true);
	}
}

const Tag *Membership::getTag(const NetworkConfig &nconf,const uint32_t id) const
{
	const _RemoteCredential<Tag> *const *t = std::lower_bound(&(_remoteTags[0]),&(_remoteTags[ZT_MAX_NETWORK_TAGS]),(uint64_t)id,_RemoteCredentialComp<Tag>());
	return ( ((t != &(_remoteTags[ZT_MAX_NETWORK_CAPABILITIES]))&&((*t)->id == (uint64_t)id)) ? ((((*t)->lastReceived)&&(_isCredentialTimestampValid(nconf,**t))) ? &((*t)->credential) : (const Tag *)0) : (const Tag *)0);
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const CertificateOfMembership &com)
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

	switch(com.verify(RR,tPtr)) {
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

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Tag &tag)
{
	_RemoteCredential<Tag> *const *htmp = std::lower_bound(&(_remoteTags[0]),&(_remoteTags[ZT_MAX_NETWORK_TAGS]),(uint64_t)tag.id(),_RemoteCredentialComp<Tag>());
	_RemoteCredential<Tag> *have = ((htmp != &(_remoteTags[ZT_MAX_NETWORK_TAGS]))&&((*htmp)->id == (uint64_t)tag.id())) ? *htmp : (_RemoteCredential<Tag> *)0;
	if (have) {
		if ( (!_isCredentialTimestampValid(nconf,*have)) || (have->credential.timestamp() > tag.timestamp()) ) {
			TRACE("addCredential(Tag) for %s on %.16llx REJECTED (revoked or too old)",tag.issuedTo().toString().c_str(),tag.networkId());
			return ADD_REJECTED;
		}
		if (have->credential == tag) {
			TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (redundant)",tag.issuedTo().toString().c_str(),tag.networkId());
			return ADD_ACCEPTED_REDUNDANT;
		}
	}

	switch(tag.verify(RR,tPtr)) {
		default:
			TRACE("addCredential(Tag) for %s on %.16llx REJECTED (invalid)",tag.issuedTo().toString().c_str(),tag.networkId());
			return ADD_REJECTED;
		case 0:
			TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (new)",tag.issuedTo().toString().c_str(),tag.networkId());
			if (!have) have = _newTag(tag.id());
			have->lastReceived = RR->node->now();
			have->credential = tag;
			return ADD_ACCEPTED_NEW;
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Capability &cap)
{
	_RemoteCredential<Capability> *const *htmp = std::lower_bound(&(_remoteCaps[0]),&(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]),(uint64_t)cap.id(),_RemoteCredentialComp<Capability>());
	_RemoteCredential<Capability> *have = ((htmp != &(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]))&&((*htmp)->id == (uint64_t)cap.id())) ? *htmp : (_RemoteCredential<Capability> *)0;
	if (have) {
		if ( (!_isCredentialTimestampValid(nconf,*have)) || (have->credential.timestamp() > cap.timestamp()) ) {
			TRACE("addCredential(Capability) for %s on %.16llx REJECTED (revoked or too old)",cap.issuedTo().toString().c_str(),cap.networkId());
			return ADD_REJECTED;
		}
		if (have->credential == cap) {
			TRACE("addCredential(Capability) for %s on %.16llx ACCEPTED (redundant)",cap.issuedTo().toString().c_str(),cap.networkId());
			return ADD_ACCEPTED_REDUNDANT;
		}
	}

	switch(cap.verify(RR,tPtr)) {
		default:
			TRACE("addCredential(Capability) for %s on %.16llx REJECTED (invalid)",cap.issuedTo().toString().c_str(),cap.networkId());
			return ADD_REJECTED;
		case 0:
			TRACE("addCredential(Capability) for %s on %.16llx ACCEPTED (new)",cap.issuedTo().toString().c_str(),cap.networkId());
			if (!have) have = _newCapability(cap.id());
			have->lastReceived = RR->node->now();
			have->credential = cap;
			return ADD_ACCEPTED_NEW;
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Revocation &rev)
{
	switch(rev.verify(RR,tPtr)) {
		default:
			return ADD_REJECTED;
		case 0: {
			const uint64_t now = RR->node->now();
			switch(rev.type()) {
				default:
					return ADD_REJECTED;
				case Revocation::CREDENTIAL_TYPE_COM:
					return (_revokeCom(rev) ? ADD_ACCEPTED_NEW : ADD_ACCEPTED_REDUNDANT);
				case Revocation::CREDENTIAL_TYPE_CAPABILITY:
					return (_revokeCap(rev,now) ? ADD_ACCEPTED_NEW : ADD_ACCEPTED_REDUNDANT);
				case Revocation::CREDENTIAL_TYPE_TAG:
					return (_revokeTag(rev,now) ? ADD_ACCEPTED_NEW : ADD_ACCEPTED_REDUNDANT);
				case Revocation::CREDENTIAL_TYPE_COO:
					return (_revokeCoo(rev,now) ? ADD_ACCEPTED_NEW : ADD_ACCEPTED_REDUNDANT);
			}
		}
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const CertificateOfOwnership &coo)
{
	_RemoteCredential<CertificateOfOwnership> *const *htmp = std::lower_bound(&(_remoteCoos[0]),&(_remoteCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP]),(uint64_t)coo.id(),_RemoteCredentialComp<CertificateOfOwnership>());
	_RemoteCredential<CertificateOfOwnership> *have = ((htmp != &(_remoteCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP]))&&((*htmp)->id == (uint64_t)coo.id())) ? *htmp : (_RemoteCredential<CertificateOfOwnership> *)0;
	if (have) {
		if ( (!_isCredentialTimestampValid(nconf,*have)) || (have->credential.timestamp() > coo.timestamp()) ) {
			TRACE("addCredential(CertificateOfOwnership) for %s on %.16llx REJECTED (revoked or too old)",coo.issuedTo().toString().c_str(),coo.networkId());
			return ADD_REJECTED;
		}
		if (have->credential == coo) {
			TRACE("addCredential(CertificateOfOwnership) for %s on %.16llx ACCEPTED (redundant)",coo.issuedTo().toString().c_str(),coo.networkId());
			return ADD_ACCEPTED_REDUNDANT;
		}
	}

	switch(coo.verify(RR,tPtr)) {
		default:
			TRACE("addCredential(CertificateOfOwnership) for %s on %.16llx REJECTED (invalid)",coo.issuedTo().toString().c_str(),coo.networkId());
			return ADD_REJECTED;
		case 0:
			TRACE("addCredential(CertificateOfOwnership) for %s on %.16llx ACCEPTED (new)",coo.issuedTo().toString().c_str(),coo.networkId());
			if (!have) have = _newCoo(coo.id());
			have->lastReceived = RR->node->now();
			have->credential = coo;
			return ADD_ACCEPTED_NEW;
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

Membership::_RemoteCredential<Tag> *Membership::_newTag(const uint64_t id)
{
	_RemoteCredential<Tag> *t = NULL;
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

	if (t) {
		t->id = id;
		t->lastReceived = 0;
		t->revocationThreshold = 0;
		t->credential = Tag();
	}

	std::sort(&(_remoteTags[0]),&(_remoteTags[ZT_MAX_NETWORK_TAGS]),_RemoteCredentialComp<Tag>());
	return t;
}

Membership::_RemoteCredential<Capability> *Membership::_newCapability(const uint64_t id)
{
	_RemoteCredential<Capability> *c = NULL;
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

	if (c) {
		c->id = id;
		c->lastReceived = 0;
		c->revocationThreshold = 0;
		c->credential = Capability();
	}

	std::sort(&(_remoteCaps[0]),&(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]),_RemoteCredentialComp<Capability>());
	return c;
}

Membership::_RemoteCredential<CertificateOfOwnership> *Membership::_newCoo(const uint64_t id)
{
	_RemoteCredential<CertificateOfOwnership> *c = NULL;
	uint64_t minlr = 0xffffffffffffffffULL;
	for(unsigned int i=0;i<ZT_MAX_CERTIFICATES_OF_OWNERSHIP;++i) {
		if (_remoteCoos[i]->id == ZT_MEMBERSHIP_CRED_ID_UNUSED) {
			c = _remoteCoos[i];
			break;
		} else if (_remoteCoos[i]->lastReceived <= minlr) {
			c = _remoteCoos[i];
			minlr = _remoteCoos[i]->lastReceived;
		}
	}

	if (c) {
		c->id = id;
		c->lastReceived = 0;
		c->revocationThreshold = 0;
		c->credential = CertificateOfOwnership();
	}

	std::sort(&(_remoteCoos[0]),&(_remoteCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP]),_RemoteCredentialComp<CertificateOfOwnership>());
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
	_RemoteCredential<Capability> *const *htmp = std::lower_bound(&(_remoteCaps[0]),&(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]),(uint64_t)rev.credentialId(),_RemoteCredentialComp<Capability>());
	_RemoteCredential<Capability> *have = ((htmp != &(_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]))&&((*htmp)->id == (uint64_t)rev.credentialId())) ? *htmp : (_RemoteCredential<Capability> *)0;
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
	_RemoteCredential<Tag> *const *htmp = std::lower_bound(&(_remoteTags[0]),&(_remoteTags[ZT_MAX_NETWORK_TAGS]),(uint64_t)rev.credentialId(),_RemoteCredentialComp<Tag>());
	_RemoteCredential<Tag> *have = ((htmp != &(_remoteTags[ZT_MAX_NETWORK_TAGS]))&&((*htmp)->id == (uint64_t)rev.credentialId())) ? *htmp : (_RemoteCredential<Tag> *)0;
	if (!have) have = _newTag(rev.credentialId());
	if (rev.threshold() > have->revocationThreshold) {
		have->lastReceived = now;
		have->revocationThreshold = rev.threshold();
		return true;
	}
	return false;
}

bool Membership::_revokeCoo(const Revocation &rev,const uint64_t now)
{
	_RemoteCredential<CertificateOfOwnership> *const *htmp = std::lower_bound(&(_remoteCoos[0]),&(_remoteCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP]),(uint64_t)rev.credentialId(),_RemoteCredentialComp<CertificateOfOwnership>());
	_RemoteCredential<CertificateOfOwnership> *have = ((htmp != &(_remoteCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP]))&&((*htmp)->id == (uint64_t)rev.credentialId())) ? *htmp : (_RemoteCredential<CertificateOfOwnership> *)0;
	if (!have) have = _newCoo(rev.credentialId());
	if (rev.threshold() > have->revocationThreshold) {
		have->lastReceived = now;
		have->revocationThreshold = rev.threshold();
		return true;
	}
	return false;
}

} // namespace ZeroTier
