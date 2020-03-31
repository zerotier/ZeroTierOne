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
#include "Node.hpp"

namespace ZeroTier {

Membership::Membership() :
	_comRevocationThreshold(0),
	_lastPushedCredentials(0),
	_comAgreementLocalTimestamp(0),
	_comAgreementRemoteTimestamp(0)
{
}

Membership::~Membership()
{
}

void Membership::pushCredentials(const RuntimeEnvironment *RR,void *tPtr,const int64_t now,const SharedPtr<Peer> &to,const NetworkConfig &nconf)
{
	if (!nconf.com) // sanity check
		return;

	SharedPtr<Buf> outp(new Buf());
	Protocol::Header &ph = outp->as<Protocol::Header>();

	unsigned int capPtr = 0,tagPtr = 0,cooPtr = 0;
	bool sendCom = true;
	bool complete = false;
	while (!complete) {
		ph.packetId = Protocol::getPacketId();
		to->address().copyTo(ph.destination);
		RR->identity.address().copyTo(ph.source);
		ph.flags = 0;
		ph.verb = Protocol::VERB_NETWORK_CREDENTIALS;

		int outl = sizeof(Protocol::Header);

		if (sendCom) {
			sendCom = false;
			outp->wO(outl,nconf.com);
		}
		outp->wI8(outl,0);

		if ((outl + ZT_CAPABILITY_MARSHAL_SIZE_MAX + 2) < ZT_PROTO_MAX_PACKET_LENGTH) {
			void *const capCountAt = outp->unsafeData + outl;
			outl += 2;
			unsigned int capCount = 0;
			while (capPtr < nconf.capabilityCount) {
				outp->wO(outl,nconf.capabilities[capPtr++]);
				++capCount;
				if ((outl + ZT_CAPABILITY_MARSHAL_SIZE_MAX) >= ZT_PROTO_MAX_PACKET_LENGTH)
					break;
			}
			Utils::storeBigEndian(capCountAt,(uint16_t)capCount);

			if ((outl + ZT_TAG_MARSHAL_SIZE_MAX + 4) < ZT_PROTO_MAX_PACKET_LENGTH) {
				void *const tagCountAt = outp->unsafeData + outl;
				outl += 2;
				unsigned int tagCount = 0;
				while (tagPtr < nconf.tagCount) {
					outp->wO(outl,nconf.tags[tagPtr++]);
					++tagCount;
					if ((outl + ZT_TAG_MARSHAL_SIZE_MAX) >= ZT_PROTO_MAX_PACKET_LENGTH)
						break;
				}
				Utils::storeBigEndian(tagCountAt,(uint16_t)tagCount);

				outp->wI16(outl,0); // no revocations sent here as these propagate differently

				if ((outl + ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX + 2) < ZT_PROTO_MAX_PACKET_LENGTH) {
					void *const cooCountAt = outp->unsafeData + outl;
					outl += 2;
					unsigned int cooCount = 0;
					while (cooPtr < nconf.certificateOfOwnershipCount) {
						outp->wO(outl,nconf.certificatesOfOwnership[cooPtr++]);
						++cooCount;
						if ((outl + ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX) >= ZT_PROTO_MAX_PACKET_LENGTH)
							break;
					}
					Utils::storeBigEndian(cooCountAt,(uint16_t)cooCount);

					complete = true;
				} else {
					outp->wI16(outl,0);
				}
			} else {
				outp->wI32(outl,0);
				outp->wI16(outl,0); // three zero 16-bit integers
			}
		} else {
			outp->wI64(outl,0); // four zero 16-bit integers
		}

		if (outl > (int)sizeof(Protocol::Header)) {
			outl = Protocol::compress(outp,outl);
			// TODO
		}
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
		RR->t->credentialRejected(tPtr,0xd9992121,com.networkId(),sourcePeerIdentity.address(),sourcePeerIdentity,com.id(),com.timestamp(),ZT_CREDENTIAL_TYPE_COM,ZT_TRACE_CREDENTIAL_REJECTION_REASON_REVOKED);
		return ADD_REJECTED;
	}

	const int64_t oldts = _com.timestamp();
	if (newts < oldts) {
		RR->t->credentialRejected(tPtr,0xd9928192,com.networkId(),sourcePeerIdentity.address(),sourcePeerIdentity,com.id(),com.timestamp(),ZT_CREDENTIAL_TYPE_COM,ZT_TRACE_CREDENTIAL_REJECTION_REASON_OLDER_THAN_LATEST);
		return ADD_REJECTED;
	}
	if ((newts == oldts)&&(_com == com))
		return ADD_ACCEPTED_REDUNDANT;

	switch(com.verify(RR,tPtr)) {
		default:
			RR->t->credentialRejected(tPtr,0x0f198241,com.networkId(),sourcePeerIdentity.address(),sourcePeerIdentity,com.id(),com.timestamp(),ZT_CREDENTIAL_TYPE_COM,ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID);
			return Membership::ADD_REJECTED;
		case Credential::VERIFY_OK:
			_com = com;
			return ADD_ACCEPTED_NEW;
		case Credential::VERIFY_BAD_SIGNATURE:
			RR->t->credentialRejected(tPtr,0xbaf0aaaa,com.networkId(),sourcePeerIdentity.address(),sourcePeerIdentity,com.id(),com.timestamp(),ZT_CREDENTIAL_TYPE_COM,ZT_TRACE_CREDENTIAL_REJECTION_REASON_SIGNATURE_VERIFICATION_FAILED);
			return ADD_REJECTED;
		case Credential::VERIFY_NEED_IDENTITY:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

// 3/5 of the credential types have identical addCredential() code
template<typename C>
static ZT_INLINE Membership::AddCredentialResult _addCredImpl(
	Map<uint32_t,C> &remoteCreds,
	const Map<uint64_t,int64_t> &revocations,
	const RuntimeEnvironment *const RR,
	void *const tPtr,
	const Identity &sourcePeerIdentity,
	const NetworkConfig &nconf,
	const C &cred)
{
	C *rc = remoteCreds.get(cred.id());
	if (rc) {
		if (rc->timestamp() > cred.timestamp()) {
			RR->t->credentialRejected(tPtr,0x40000001,nconf.networkId,sourcePeerIdentity.address(),sourcePeerIdentity,cred.id(),cred.timestamp(),C::credentialType(),ZT_TRACE_CREDENTIAL_REJECTION_REASON_OLDER_THAN_LATEST);
			return Membership::ADD_REJECTED;
		}
		if (*rc == cred)
			return Membership::ADD_ACCEPTED_REDUNDANT;
	}

	const int64_t *const rt = revocations.get(Membership::credentialKey(C::credentialType(),cred.id()));
	if ((rt)&&(*rt >= cred.timestamp())) {
		RR->t->credentialRejected(tPtr,0x24248124,nconf.networkId,sourcePeerIdentity.address(),sourcePeerIdentity,cred.id(),cred.timestamp(),C::credentialType(),ZT_TRACE_CREDENTIAL_REJECTION_REASON_REVOKED);
		return Membership::ADD_REJECTED;
	}

	switch(cred.verify(RR,tPtr)) {
		default:
			RR->t->credentialRejected(tPtr,0x01feba012,nconf.networkId,sourcePeerIdentity.address(),sourcePeerIdentity,cred.id(),cred.timestamp(),C::credentialType(),ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID);
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
			RR->t->credentialRejected(tPtr,0x938fffff,nconf.networkId,sourcePeerIdentity.address(),sourcePeerIdentity,rev.id(),0,ZT_CREDENTIAL_TYPE_REVOCATION,ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID);
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
					RR->t->credentialRejected(tPtr,0x0bbbb1a4,nconf.networkId,sourcePeerIdentity.address(),sourcePeerIdentity,rev.id(),0,ZT_CREDENTIAL_TYPE_REVOCATION,ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID);
					return ADD_REJECTED;
			}
		}
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

bool Membership::_isUnspoofableAddress(const NetworkConfig &nconf,const InetAddress &ip) const noexcept
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
