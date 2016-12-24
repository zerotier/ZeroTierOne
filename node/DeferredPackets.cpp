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

#include "Constants.hpp"
#include "DeferredPackets.hpp"
#include "IncomingPacket.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"

namespace ZeroTier {

DeferredPackets::DeferredPackets(const RuntimeEnvironment *renv) :
	RR(renv),
	_waiting(0),
	_die(false)
{
}

DeferredPackets::~DeferredPackets()
{
	_q_m.lock();
	_die = true;
	_q_m.unlock();

	for(;;) {
		_q_s.post();

		_q_m.lock();
		if (_waiting <= 0) {
			_q_m.unlock();
			break;
		} else {
			_q_m.unlock();
		}
	}
}

bool DeferredPackets::enqueue(IncomingPacket *pkt)
{
	{
		Mutex::Lock _l(_q_m);
		if (_q.size() >= ZT_DEFFEREDPACKETS_MAX)
			return false;
		_q.push_back(*pkt);
	}
	_q_s.post();
	return true;
}

int DeferredPackets::process()
{
	std::list<IncomingPacket> pkt;

	_q_m.lock();

	if (_die) {
		_q_m.unlock();
		return -1;
	}

	while (_q.empty()) {
		++_waiting;
		_q_m.unlock();
		_q_s.wait();
		_q_m.lock();
		--_waiting;
		if (_die) {
			_q_m.unlock();
			return -1;
		}
	}

	// Move item from _q list to a dummy list here to avoid copying packet
	pkt.splice(pkt.end(),_q,_q.begin());

	_q_m.unlock();

	try {
		pkt.front().tryDecode(RR,true);
	} catch ( ... ) {} // drop invalids

	return 1;
}

} // namespace ZeroTier
