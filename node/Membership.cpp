/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <algorithm>

#include "Membership.hpp"
#include "RuntimeEnvironment.hpp"
#include "Peer.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "Node.hpp"

namespace ZeroTier {

Membership::Membership() :
	_lastUpdatedMulticast(0),
	_comRevocationThreshold(0),
	_lastPushedCredentials(0),
	_revocations(4),
	_remoteTags(4),
	_remoteCaps(4),
	_remoteCoos(4)
{
}

void Membership::pushCredentials(const RuntimeEnvironment *RR,void *tPtr,const int64_t now,const Address &peerAddress,const NetworkConfig &nconf)
{
	const Capability *sendCaps[ZT_MAX_NETWORK_CAPABILITIES];
	unsigned int sendCapCount = 0;
	for(unsigned int c=0;c<nconf.capabilityCount;++c)
		sendCaps[sendCapCount++] = &(nconf.capabilities[c]);

	const Tag *sendTags[ZT_MAX_NETWORK_TAGS];
	unsigned int sendTagCount = 0;
	for(unsigned int t=0;t<nconf.tagCount;++t)
		sendTags[sendTagCount++] = &(nconf.tags[t]);

	const CertificateOfOwnership *sendCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP];
	unsigned int sendCooCount = 0;
	for(unsigned int c=0;c<nconf.certificateOfOwnershipCount;++c)
		sendCoos[sendCooCount++] = &(nconf.certificatesOfOwnership[c]);

	unsigned int capPtr = 0;
	unsigned int tagPtr = 0;
	unsigned int cooPtr = 0;
	bool sendCom = (bool)(nconf.com);
	while ((capPtr < sendCapCount)||(tagPtr < sendTagCount)||(cooPtr < sendCooCount)||(sendCom)) {
		Packet outp(peerAddress,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);

		if (sendCom) {
			sendCom = false;
			nconf.com.serialize(outp);
		}
		outp.append((uint8_t)0x00);

		const unsigned int capCountAt = outp.size();
		outp.addSize(2);
		unsigned int thisPacketCapCount = 0;
		while ((capPtr < sendCapCount)&&((outp.size() + sizeof(Capability) + 16) < ZT_PROTO_MAX_PACKET_LENGTH)) {
			sendCaps[capPtr++]->serialize(outp);
			++thisPacketCapCount;
		}
		outp.setAt(capCountAt,(uint16_t)thisPacketCapCount);

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

	_lastPushedCredentials = now;
}

void Membership::clean(const int64_t now,const NetworkConfig &nconf)
{
	_cleanCredImpl<Tag>(nconf,_remoteTags);
	_cleanCredImpl<Capability>(nconf,_remoteCaps);
	_cleanCredImpl<CertificateOfOwnership>(nconf,_remoteCoos);
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const CertificateOfMembership &com)
{
	const int64_t newts = com.timestamp();
	if (newts <= _comRevocationThreshold) {
		RR->t->credentialRejected(tPtr,com.networkId(),sourcePeerIdentity.address(),com.id(),com.timestamp(),ZT_CREDENTIAL_TYPE_COM,ZT_TRACE_CREDENTIAL_REJECTION_REASON_REVOKED);
		return ADD_REJECTED;
	}

	const int64_t oldts = _com.timestamp();
	if (newts < oldts) {
		RR->t->credentialRejected(tPtr,com.networkId(),sourcePeerIdentity.address(),com.id(),com.timestamp(),ZT_CREDENTIAL_TYPE_COM,ZT_TRACE_CREDENTIAL_REJECTION_REASON_OLDER_THAN_LATEST);
		return ADD_REJECTED;
	}
	if ((newts == oldts)&&(_com == com))
		return ADD_ACCEPTED_REDUNDANT;

	switch(com.verify(RR,tPtr)) {
		default:
			RR->t->credentialRejected(tPtr,com.networkId(),sourcePeerIdentity.address(),com.id(),com.timestamp(),ZT_CREDENTIAL_TYPE_COM,ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID);
			return Membership::ADD_REJECTED;
		case Credential::VERIFY_OK:
			_com = com;
			return ADD_ACCEPTED_NEW;
		case Credential::VERIFY_BAD_SIGNATURE:
			RR->t->credentialRejected(tPtr,com.networkId(),sourcePeerIdentity.address(),com.id(),com.timestamp(),ZT_CREDENTIAL_TYPE_COM,ZT_TRACE_CREDENTIAL_REJECTION_REASON_SIGNATURE_VERIFICATION_FAILED);
			return ADD_REJECTED;
		case Credential::VERIFY_NEED_IDENTITY:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

// 3/5 of the credential types have identical addCredential() code
template<typename C>
static ZT_ALWAYS_INLINE Membership::AddCredentialResult _addCredImpl(
	Hashtable<uint32_t,C> &remoteCreds,
	const Hashtable<uint64_t,int64_t> &revocations,
	const RuntimeEnvironment *const RR,
	void *const tPtr,
	const Identity &sourcePeerIdentity,
	const NetworkConfig &nconf,
	const C &cred)
{
	C *rc = remoteCreds.get(cred.id());
	if (rc) {
		if (rc->timestamp() > cred.timestamp()) {
			RR->t->credentialRejected(tPtr,nconf.networkId,sourcePeerIdentity.address(),cred.id(),cred.timestamp(),C::credentialType(),ZT_TRACE_CREDENTIAL_REJECTION_REASON_OLDER_THAN_LATEST);
			return Membership::ADD_REJECTED;
		}
		if (*rc == cred)
			return Membership::ADD_ACCEPTED_REDUNDANT;
	}

	const int64_t *const rt = revocations.get(Membership::credentialKey(C::credentialType(),cred.id()));
	if ((rt)&&(*rt >= cred.timestamp())) {
		RR->t->credentialRejected(tPtr,nconf.networkId,sourcePeerIdentity.address(),cred.id(),cred.timestamp(),C::credentialType(),ZT_TRACE_CREDENTIAL_REJECTION_REASON_REVOKED);
		return Membership::ADD_REJECTED;
	}

	switch(cred.verify(RR,tPtr)) {
		default:
			RR->t->credentialRejected(tPtr,nconf.networkId,sourcePeerIdentity.address(),cred.id(),cred.timestamp(),C::credentialType(),ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID);
			return Membership::ADD_REJECTED;
		case 0:
			if (!rc)
				rc = &(remoteCreds[cred.id()]);
			*rc = cred;
			return Membership::ADD_ACCEPTED_NEW;
		case 1:
			return Membership::ADD_DEFERRED_FOR_WHOIS;
	}
}
Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const Tag &tag) { return _addCredImpl<Tag>(_remoteTags,_revocations,RR,tPtr,sourcePeerIdentity,nconf,tag); }
Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const Capability &cap) { return _addCredImpl<Capability>(_remoteCaps,_revocations,RR,tPtr,sourcePeerIdentity,nconf,cap); }
Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const CertificateOfOwnership &coo) { return _addCredImpl<CertificateOfOwnership>(_remoteCoos,_revocations,RR,tPtr,sourcePeerIdentity,nconf,coo); }

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const Revocation &rev)
{
	int64_t *rt;
	switch(rev.verify(RR,tPtr)) {
		default:
			RR->t->credentialRejected(tPtr,nconf.networkId,sourcePeerIdentity.address(),rev.id(),0,ZT_CREDENTIAL_TYPE_REVOCATION,ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID);
			return ADD_REJECTED;
		case 0: {
			const ZT_CredentialType ct = rev.typeBeingRevoked();
			switch(ct) {
				case ZT_CREDENTIAL_TYPE_COM:
					if (rev.threshold() > _comRevocationThreshold) {
						_comRevocationThreshold = rev.threshold();
						return ADD_ACCEPTED_NEW;
					}
					return ADD_ACCEPTED_REDUNDANT;
				case ZT_CREDENTIAL_TYPE_CAPABILITY:
				case ZT_CREDENTIAL_TYPE_TAG:
				case ZT_CREDENTIAL_TYPE_COO:
					rt = &(_revocations[credentialKey(ct,rev.credentialId())]);
					if (*rt < rev.threshold()) {
						*rt = rev.threshold();
						_comRevocationThreshold = rev.threshold();
						return ADD_ACCEPTED_NEW;
					}
					return ADD_ACCEPTED_REDUNDANT;
				default:
					RR->t->credentialRejected(tPtr,nconf.networkId,sourcePeerIdentity.address(),rev.id(),0,ZT_CREDENTIAL_TYPE_REVOCATION,ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID);
					return ADD_REJECTED;
			}
		}
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

bool Membership::_isUnspoofableAddress(const NetworkConfig &nconf,const InetAddress &ip) const
{
	if ((ip.isV6())&&(nconf.ndpEmulation())) {
		const InetAddress sixpl(InetAddress::makeIpv66plane(nconf.networkId,nconf.issuedTo.toInt()));
		for(unsigned int i=0;i<nconf.staticIpCount;++i) {
			if (nconf.staticIps[i].ipsEqual(sixpl)) {
				bool prefixMatches = true;
				for(unsigned int j=0;j<5;++j) { // check for match on /40
					if ((((const struct sockaddr_in6 *)&ip)->sin6_addr.s6_addr)[j] != (((const struct sockaddr_in6 *)&sixpl)->sin6_addr.s6_addr)[j]) {
						prefixMatches = false;
						break;
					}
				}
				if (prefixMatches)
					return true;
				break;
			}
		}

		const InetAddress rfc4193(InetAddress::makeIpv6rfc4193(nconf.networkId,nconf.issuedTo.toInt()));
		for(unsigned int i=0;i<nconf.staticIpCount;++i) {
			if (nconf.staticIps[i].ipsEqual(rfc4193)) {
				bool prefixMatches = true;
				for(unsigned int j=0;j<11;++j) { // check for match on /88
					if ((((const struct sockaddr_in6 *)&ip)->sin6_addr.s6_addr)[j] != (((const struct sockaddr_in6 *)&rfc4193)->sin6_addr.s6_addr)[j]) {
						prefixMatches = false;
						break;
					}
				}
				if (prefixMatches)
					return true;
				break;
			}
		}
	}
	return false;
}

} // namespace ZeroTier
