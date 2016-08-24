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

#define ZT_CREDENTIAL_PUSH_EVERY (ZT_NETWORK_AUTOCONF_DELAY / 4)

namespace ZeroTier {

bool Membership::sendCredentialsIfNeeded(const RuntimeEnvironment *RR,const uint64_t now,const Address &peerAddress,const CertificateOfMembership &com,const Capability *cap,const Tag **tags,const unsigned int tagCount)
{
	try {
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH> capsAndTags;

		unsigned int appendedCaps = 0;
		if (cap) {
			capsAndTags.addSize(2);
			std::map<uint32_t,CState>::iterator cs(_caps.find(cap->id()));
			if ((cs != _caps.end())&&((now - cs->second.lastPushed) >= ZT_CREDENTIAL_PUSH_EVERY)) {
				cap->serialize(capsAndTags);
				cs->second.lastPushed = now;
				++appendedCaps;
			}
			capsAndTags.setAt<uint16_t>(0,(uint16_t)appendedCaps);
		} else {
			capsAndTags.append((uint16_t)0);
		}

		unsigned int appendedTags = 0;
		const unsigned int tagCountPos = capsAndTags.size();
		capsAndTags.addSize(2);
		for(unsigned int i=0;i<tagCount;++i) {
			TState *const ts = _tags.get(tags[i]->id());
			if ((now - ts->lastPushed) >= ZT_CREDENTIAL_PUSH_EVERY) {
				if ((capsAndTags.size() + sizeof(Tag)) > (ZT_PROTO_MAX_PACKET_LENGTH - sizeof(CertificateOfMembership)))
					break;
				tags[i]->serialize(capsAndTags);
				ts->lastPushed = now;
				++appendedTags;
			}
		}
		capsAndTags.setAt<uint16_t>(tagCountPos,(uint16_t)appendedTags);

		if ( ((com)&&((now - _lastPushedCom) >= ZT_CREDENTIAL_PUSH_EVERY)) || (appendedCaps) || (appendedTags) ) {
			Packet outp(peerAddress,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
			if (com)
				com.serialize(outp);
			outp.append((uint8_t)0x00);
			outp.append(capsAndTags.data(),capsAndTags.size());
			outp.compress();
			RR->sw->send(outp,true);
			_lastPushedCom = now;
			return true;
		}
	} catch ( ... ) {
		TRACE("unable to send credentials due to unexpected exception");
	}
	return false;
}

int Membership::addCredential(const RuntimeEnvironment *RR,const CertificateOfMembership &com)
{
	if (_com == com) {
		TRACE("addCredential(CertificateOfMembership) for %s on %.16llx ACCEPTED (redundant)",com.issuedTo().toString().c_str(),com.networkId());
		return 0;
	}
	const int vr = com.verify(RR);
	if (vr == 0) {
		TRACE("addCredential(CertificateOfMembership) for %s on %.16llx ACCEPTED (new)",com.issuedTo().toString().c_str(),com.networkId());
		if (com.timestamp().first > _com.timestamp().first)
			_com = com;
	} else {
		TRACE("addCredential(CertificateOfMembership) for %s on %.16llx REJECTED (%d)",com.issuedTo().toString().c_str(),com.networkId(),vr);
	}
	return vr;
}

int Membership::addCredential(const RuntimeEnvironment *RR,const Tag &tag)
{
	TState *t = _tags.get(tag.id());
	if ((t)&&(t->lastReceived != 0)&&(t->tag == tag)) {
		TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (redundant)",tag.issuedTo().toString().c_str(),tag.networkId());
		return 0;
	}
	const int vr = tag.verify(RR);
	if (vr == 0) {
		TRACE("addCredential(Tag) for %s on %.16llx ACCEPTED (new)",tag.issuedTo().toString().c_str(),tag.networkId());
		if (!t)
			t = &(_tags[tag.id()]);
		if (t->tag.timestamp() <= tag.timestamp()) {
			t->lastReceived = RR->node->now();
			t->tag = tag;
		}
	} else {
		TRACE("addCredential(Tag) for %s on %.16llx REJECTED (%d)",tag.issuedTo().toString().c_str(),tag.networkId(),vr);
	}
	return vr;
}

int Membership::addCredential(const RuntimeEnvironment *RR,const Capability &cap)
{
	std::map<uint32_t,CState>::iterator c(_caps.find(cap.id()));
	if ((c != _caps.end())&&(c->second.lastReceived != 0)&&(c->second.cap == cap)) {
		TRACE("addCredential(Capability) for %s on %.16llx ACCEPTED (redundant)",cap.issuedTo().toString().c_str(),cap.networkId());
		return 0;
	}
	const int vr = cap.verify(RR);
	if (vr == 0) {
		TRACE("addCredential(Capability) for %s on %.16llx ACCEPTED (new)",cap.issuedTo().toString().c_str(),cap.networkId());
		if (c == _caps.end()) {
			CState &c2 = _caps[cap.id()];
			c2.lastReceived = RR->node->now();
			c2.cap = cap;
		} else if (c->second.cap.timestamp() <= cap.timestamp()) {
			c->second.lastReceived = RR->node->now();
			c->second.cap = cap;
		}
	} else {
		TRACE("addCredential(Capability) for %s on %.16llx REJECTED (%d)",cap.issuedTo().toString().c_str(),cap.networkId(),vr);
	}
	return vr;
}

} // namespace ZeroTier
