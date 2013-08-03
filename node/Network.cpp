/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdlib.h>
#include <math.h>

#include <openssl/sha.h>

#include "RuntimeEnvironment.hpp"
#include "NodeConfig.hpp"
#include "Network.hpp"
#include "Switch.hpp"
#include "Packet.hpp"

namespace ZeroTier {

void Network::Certificate::_shaForSignature(unsigned char *dig) const
{
	SHA256_CTX sha;
	SHA256_Init(&sha);
	unsigned char zero = 0;
	for(const_iterator i(begin());i!=end();++i) {
		SHA256_Update(&sha,&zero,1);
		SHA256_Update(&sha,(const unsigned char *)i->first.data(),i->first.length());
		SHA256_Update(&sha,&zero,1);
		SHA256_Update(&sha,(const unsigned char *)i->second.data(),i->second.length());
		SHA256_Update(&sha,&zero,1);
	}
	SHA256_Final(dig,&sha);
}

static const std::string _DELTA_PREFIX("~");
bool Network::Certificate::qualifyMembership(const Network::Certificate &mc) const
{
	// Note: optimization probably needed here, probably via some kind of
	// memoization / dynamic programming.

	for(const_iterator myField(begin());myField!=end();++myField) {
		if (!((myField->first.length() > 1)&&(myField->first[0] == '~'))) { // ~fields are max delta range specs
			// If they lack the same field, comparison fails.
			const_iterator theirField(mc.find(myField->first));
			if (theirField == mc.end())
				return false;

			const_iterator deltaField(find(_DELTA_PREFIX + myField->first));
			if (deltaField == end()) {
				// If there is no delta, compare on simple equality
				if (myField->second != theirField->second)
					return false;
			} else {
				// Otherwise compare range with max delta. Presence of a dot in delta
				// indicates a floating point comparison. Otherwise an integer
				// comparison occurs.
				if (deltaField->second.find('.') != std::string::npos) {
					double my = strtod(myField->second.c_str(),(char **)0);
					double their = strtod(theirField->second.c_str(),(char **)0);
					double delta = strtod(deltaField->second.c_str(),(char **)0);
					if (fabs(my - their) > delta)
						return false;
				} else {
					int64_t my = strtoll(myField->second.c_str(),(char **)0,10);
					int64_t their = strtoll(theirField->second.c_str(),(char **)0,10);
					int64_t delta = strtoll(deltaField->second.c_str(),(char **)0,10);
					if (my > their) {
						if ((my - their) > delta)
							return false;
					} else {
						if ((their - my) > delta)
							return false;
					}
				}
			}
		}
	}

	return true;
}

Network::Network(const RuntimeEnvironment *renv,uint64_t id)
	throw(std::runtime_error) :
	_r(renv),
	_tap(renv,renv->identity.address().toMAC(),ZT_IF_MTU,&_CBhandleTapData,this),
	_lastConfigUpdate(0),
	_id(id)
{
}

Network::~Network()
{
}

void Network::setConfiguration(const Network::Config &conf)
{
	Mutex::Lock _l(_lock);
	if ((conf.networkId() == _id)&&(conf.peerAddress() == _r->identity.address())) { // sanity check
		_configuration = conf;
		_myCertificate = conf.certificateOfMembership();
		_lastConfigUpdate = Utils::now();
	}
}

void Network::requestConfiguration()
{
	Packet outp(controller(),_r->identity.address(),Packet::VERB_NETWORK_CONFIG_REQUEST);
	outp.append((uint64_t)_id);
	_r->sw->send(outp,true);
}

bool Network::isAllowed(const Address &peer) const
{
	// Exceptions can occur if we do not yet have *our* configuration.
	try {
		Mutex::Lock _l(_lock);
		if (_configuration.isOpen())
			return true;
		std::map<Address,Certificate>::const_iterator pc(_membershipCertificates.find(peer));
		if (pc == _membershipCertificates.end())
			return false;
		return _myCertificate.qualifyMembership(pc->second);
	} catch (std::exception &exc) {
		TRACE("isAllowed() check failed for peer %s: unexpected exception: %s",peer.toString().c_str(),exc.what());
		return false;
	} catch ( ... ) {
		TRACE("isAllowed() check failed for peer %s: unexpected exception: unknown exception",peer.toString().c_str());
		return false;
	}
}

void Network::clean()
{
	Mutex::Lock _l(_lock);
	for(std::map<Address,Certificate>::iterator i=(_membershipCertificates.begin());i!=_membershipCertificates.end();) {
		if (_myCertificate.qualifyMembership(i->second))
			++i;
		else _membershipCertificates.erase(i++);
	}
}

void Network::_CBhandleTapData(void *arg,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data)
{
	const RuntimeEnvironment *_r = ((Network *)arg)->_r;
	try {
		_r->sw->onLocalEthernet(SharedPtr<Network>((Network *)arg),from,to,etherType,data);
	} catch (std::exception &exc) {
		TRACE("unexpected exception handling local packet: %s",exc.what());
	} catch ( ... ) {
		TRACE("unexpected exception handling local packet");
	}
}

} // namespace ZeroTier
