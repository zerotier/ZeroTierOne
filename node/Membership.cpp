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

#include "Membership.hpp"
#include "RuntimeEnvironment.hpp"
#include "Peer.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "Node.hpp"

#define ZT_CREDENTIAL_PUSH_EVERY (ZT_NETWORK_AUTOCONF_DELAY / 2)

namespace ZeroTier {

bool Membership::sendCredentialsIfNeeded(const RuntimeEnvironment *RR,const uint64_t now,const Peer &peer,const NetworkConfig &nconf,const uint32_t *capIds,const unsigned int capCount,const uint32_t *tagIds,const unsigned int tagCount)
{
	try {
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH> capsAndTags;

		capsAndTags.addSize(2);
		unsigned int appendedCaps = 0;
		for(unsigned int i=0;i<capCount;++i) {
			CState *cs = _caps.get(capIds[i]);
			if ((now - cs->lastPushed) >= ZT_CREDENTIAL_PUSH_EVERY) {
				if ((capsAndTags.size() + sizeof(Capability)) > (ZT_PROTO_MAX_PACKET_LENGTH - sizeof(CertificateOfMembership)))
					break;
				const Capability *c = nconf.capability(capIds[i]);
				if (c) {
					c->serialize(capsAndTags);
					++appendedCaps;
					cs->lastPushed = now;
				}
			}
		}
		capsAndTags.setAt<uint16_t>(0,(uint16_t)appendedCaps);

		const unsigned int tagCountPos = capsAndTags.size();
		capsAndTags.addSize(2);
		unsigned int appendedTags = 0;
		for(unsigned int i=0;i<tagCount;++i) {
			TState *ts = _tags.get(tagIds[i]);
			if ((now - ts->lastPushed) >= ZT_CREDENTIAL_PUSH_EVERY) {
				if ((capsAndTags.size() + sizeof(Tag)) > (ZT_PROTO_MAX_PACKET_LENGTH - sizeof(CertificateOfMembership)))
					break;
				const Tag *t = nconf.tag(tagIds[i]);
				if (t) {
					t->serialize(capsAndTags);
					++appendedTags;
					ts->lastPushed = now;
				}
			}
		}
		capsAndTags.setAt<uint16_t>(tagCountPos,(uint16_t)appendedTags);

		if (((now - _lastPushedCom) >= ZT_CREDENTIAL_PUSH_EVERY)||(appendedCaps)||(appendedTags)) {
			Packet outp(peer.address(),RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
			nconf.com.serialize(outp);
			outp.append((uint8_t)0x00);
			outp.append(capsAndTags.data(),capsAndTags.size());
			outp.compress();
			RR->sw->send(outp,true,0);
			_lastPushedCom = now;
			return true;
		}
	} catch ( ... ) {
		TRACE("unable to send credentials due to unexpected exception");
		return false;
	}
}

int Membership::addCredential(const RuntimeEnvironment *RR,const uint64_t now,const CertificateOfMembership &com)
{
	if (com.issuedTo() != RR->identity.address())
		return -1;
	if (_com == com)
		return 0;
	const int vr = com.verify(RR);
	if (vr == 0)
		_com = com;
	return vr;
}

int Membership::addCredential(const RuntimeEnvironment *RR,const uint64_t now,const Tag &tag)
{
	if (tag.issuedTo() != RR->identity.address())
		return -1;
	TState *t = _tags.get(tag.networkId());
	if ((t)&&(t->lastReceived != 0)&&(t->tag == tag))
		return 0;
	const int vr = tag.verify(RR);
	if (vr == 0) {
		if (!t)
			t = &(_tags[tag.networkId()]);
		t->lastReceived = now;
		t->tag = tag;
	}
	return vr;
}

int Membership::addCredential(const RuntimeEnvironment *RR,const uint64_t now,const Capability &cap)
{
	if (!cap.wasIssuedTo(RR->identity.address()))
		return -1;
	CState *c = _caps.get(cap.networkId());
	if ((c)&&(c->lastReceived != 0)&&(c->cap == cap))
		return 0;
	const int vr = cap.verify(RR);
	if (vr == 0) {
		if (!c)
			c = &(_caps[cap.networkId()]);
		c->lastReceived = now;
		c->cap = cap;
	}
	return vr;
}

} // namespace ZeroTier
