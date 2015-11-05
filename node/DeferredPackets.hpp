/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#ifndef ZT_DEFERREDPACKETS_HPP
#define ZT_DEFERREDPACKETS_HPP

#include "Constants.hpp"
#include "SharedPtr.hpp"
#include "Mutex.hpp"
#include "DeferredPackets.hpp"
#include "BinarySemaphore.hpp"

/**
 * Maximum number of deferred packets
 */
#define ZT_DEFFEREDPACKETS_MAX 1024

namespace ZeroTier {

class IncomingPacket;
class RuntimeEnvironment;

/**
 * Deferred packets
 *
 * IncomingPacket can defer its decoding this way by enqueueing itself here.
 * When this is done, deferredDecode() is called later. This is done for
 * operations that may be expensive to allow them to potentially be handled
 * in the background or rate limited to maintain quality of service for more
 * routine operations.
 */
class DeferredPackets
{
public:
	DeferredPackets(const RuntimeEnvironment *renv);
	~DeferredPackets();

	/**
	 * Enqueue a packet
	 *
	 * Since packets enqueue themselves, they call it with 'this' and we wrap
	 * them in a SharedPtr<>. This is safe as SharedPtr<> is introspective and
	 * supports this. This should not be called from any other code outside
	 * IncomingPacket.
	 *
	 * @param pkt Packet to process later (possibly in the background)
	 * @return False if queue is full
	 */
	bool enqueue(IncomingPacket *pkt);

	/**
	 * Wait for and then process a deferred packet
	 *
	 * If we are shutting down (in destructor), this returns -1 and should
	 * not be called again. Otherwise it returns the number of packets
	 * processed.
	 *
	 * @return Number processed or -1 if shutting down
	 */
	int process();

private:
	SharedPtr<IncomingPacket> _q[ZT_DEFFEREDPACKETS_MAX];
	const RuntimeEnvironment *const RR;
	unsigned long _readPtr;
	unsigned long _writePtr;
	bool _die;
	Mutex _q_m;
	BinarySemaphore _q_s;
};

} // namespace ZeroTier

#endif
