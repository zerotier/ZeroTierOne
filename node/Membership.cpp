/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
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
#include "Trace.hpp"

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
	for(unsigned int c=0;c<nconf.capabilityCount;++c) {
		sendCaps[sendCapCount++] = &(nconf.capabilities[c]);
	}

	const Tag *sendTags[ZT_MAX_NETWORK_TAGS];
	unsigned int sendTagCount = 0;
	for(unsigned int t=0;t<nconf.tagCount;++t) {
		sendTags[sendTagCount++] = &(nconf.tags[t]);
	}

	const CertificateOfOwnership *sendCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP];
	unsigned int sendCooCount = 0;
	for(unsigned int c=0;c<nconf.certificateOfOwnershipCount;++c) {
		sendCoos[sendCooCount++] = &(nconf.certificatesOfOwnership[c]);
	}

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
		Metrics::pkt_network_credentials_out++;
	}

	_lastPushedCredentials = now;
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const CertificateOfMembership &com)
{
	const int64_t newts = com.timestamp();
	if (newts <= _comRevocationThreshold) {
		RR->t->credentialRejected(tPtr,com,"revoked");
		return ADD_REJECTED;
	}

	const int64_t oldts = _com.timestamp();
	if (newts < oldts) {
		RR->t->credentialRejected(tPtr,com,"old");
		return ADD_REJECTED;
	}
	if (_com == com) {
		return ADD_ACCEPTED_REDUNDANT;
	}

	switch(com.verify(RR,tPtr)) {
		default:
			RR->t->credentialRejected(tPtr,com,"invalid");
			return ADD_REJECTED;
		case 0:
			//printf("%.16llx %.10llx replacing COM %lld with %lld\n", com.networkId(), com.issuedTo().toInt(), _com.timestamp(), com.timestamp()); fflush(stdout);
			_com = com;
			return ADD_ACCEPTED_NEW;
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

// Template out addCredential() for many cred types to avoid copypasta
template<typename C>
static Membership::AddCredentialResult _addCredImpl(Hashtable<uint32_t,C> &remoteCreds,const Hashtable<uint64_t,int64_t> &revocations,const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const C &cred)
{
	C *rc = remoteCreds.get(cred.id());
	if (rc) {
		if (rc->timestamp() > cred.timestamp()) {
			RR->t->credentialRejected(tPtr,cred,"old");
			return Membership::ADD_REJECTED;
		}
		if (*rc == cred) {
			return Membership::ADD_ACCEPTED_REDUNDANT;
		}
	}

	const int64_t *const rt = revocations.get(Membership::credentialKey(C::credentialType(),cred.id()));
	if ((rt)&&(*rt >= cred.timestamp())) {
		RR->t->credentialRejected(tPtr,cred,"revoked");
		return Membership::ADD_REJECTED;
	}

	switch(cred.verify(RR,tPtr)) {
		default:
			RR->t->credentialRejected(tPtr,cred,"invalid");
			return Membership::ADD_REJECTED;
		case 0:
			if (!rc) {
				rc = &(remoteCreds[cred.id()]);
			}
			*rc = cred;
			return Membership::ADD_ACCEPTED_NEW;
		case 1:
			return Membership::ADD_DEFERRED_FOR_WHOIS;
	}
}

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Tag &tag) { return _addCredImpl<Tag>(_remoteTags,_revocations,RR,tPtr,nconf,tag); }
Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Capability &cap) { return _addCredImpl<Capability>(_remoteCaps,_revocations,RR,tPtr,nconf,cap); }
Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const CertificateOfOwnership &coo) { return _addCredImpl<CertificateOfOwnership>(_remoteCoos,_revocations,RR,tPtr,nconf,coo); }

Membership::AddCredentialResult Membership::addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Revocation &rev)
{
	int64_t *rt;
	switch(rev.verify(RR,tPtr)) {
		default:
			RR->t->credentialRejected(tPtr,rev,"invalid");
			return ADD_REJECTED;
		case 0: {
			const Credential::Type ct = rev.type();
			switch(ct) {
				case Credential::CREDENTIAL_TYPE_COM:
					if (rev.threshold() > _comRevocationThreshold) {
						_comRevocationThreshold = rev.threshold();
						return ADD_ACCEPTED_NEW;
					}
					return ADD_ACCEPTED_REDUNDANT;
				case Credential::CREDENTIAL_TYPE_CAPABILITY:
				case Credential::CREDENTIAL_TYPE_TAG:
				case Credential::CREDENTIAL_TYPE_COO:
					rt = &(_revocations[credentialKey(ct,rev.credentialId())]);
					if (*rt < rev.threshold()) {
						*rt = rev.threshold();
						_comRevocationThreshold = rev.threshold();
						return ADD_ACCEPTED_NEW;
					}
					return ADD_ACCEPTED_REDUNDANT;
				default:
					RR->t->credentialRejected(tPtr,rev,"invalid");
					return ADD_REJECTED;
			}
		}
		case 1:
			return ADD_DEFERRED_FOR_WHOIS;
	}
}

void Membership::clean(const int64_t now,const NetworkConfig &nconf)
{
	_cleanCredImpl<Tag>(nconf,_remoteTags);
	_cleanCredImpl<Capability>(nconf,_remoteCaps);
	_cleanCredImpl<CertificateOfOwnership>(nconf,_remoteCoos);
}

} // namespace ZeroTier
